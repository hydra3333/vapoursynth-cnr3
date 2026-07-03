# CNR3 PATCH SCOPE — DIAG.2a: hot-zone (D-SUM-11) writer + prune/eviction (D-SUM-10) with re-churn counter

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 proven-code; R-PROCESS-19 observe-only proof is the exit gate).
Implement exactly this; propose back for review before commit. **DIAG.2a ALONE** (D-SUM-11 + D-SUM-10;
D-SUM-04/05/08 are DIAG.2b).
**Target:** `cnr3_cache_diagnostics.{h,cpp}`, `cnr3_cache_core.{h,cpp}` (prune path only), and the summary
emission point (cache-core selftest runner + filter free, mirroring DIAG.1).
**Governing docs:** Condensed Plan v1.5 (DIAG.2 churn-field priority), diagnostics spec v1.5, DIAG.1 (the
framework + [DSUM-SUMMARY] pattern this reuses), Document A R-PROCESS-19 + R-PROCESS-21.
**Builds on committed:** DIAG.1 (framework + D-SUM-01 + observe-only proof). Marshalling arc complete (~-80%).

---

## 1. Goal + why this split

DIAG.2a delivers the CHURN telemetry — the counters that answer the open "is the ~50% fmUnordered-no--r-1
recovery inherent or tunable" question (FI-10 multi-thread sub-finding). Source inventory shows the two
families are at very different completion, which is why they are grouped and split from 04/05/08:
```text
D-SUM-11 (hot-zone): ~80% DONE. Cnr3CacheHotZoneDiagnosticStats fully defined; all 8 observe hooks are
   ALREADY LIVE and CORRECTLY GATED behind CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE inside the _locked cache-core
   methods (observe_hot_zone_create/slide/merge/decay/expiry/state_sample/prune_rejections_locked). The
   observe-only discipline is already applied (e.g. #else (void)rejected_frame_count;). OWED: only the
   SUMMARY WRITER (emit the [DSUM-SUMMARY] D-SUM-11 block) + confirm the print-gate path.
D-SUM-10 (prune/eviction): mostly OWED. Gate CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION exists; the prune sink
   (store_owned_frame_and_prune_impl + Cnr3CombinedStoreAndPruneSummary) exists; but there is NO D-SUM-10
   struct, no eviction counters, and no RE-CHURN field. This is the new work.
```

## 2. D-SUM-11 (hot-zone) — add the writer only, touch nothing else

```text
- Add a summary writer cnr3_cache_hot_zone_diagnostic_write_summary(...) in cnr3_cache_diagnostics.cpp that:
    * takes a SNAPSHOT of Cnr3CacheHotZoneDiagnosticStats under the cache lock (or is handed a by-value copy
      taken under lock by the caller), then formats + writes OUTSIDE the lock via cnr3_diag_write_line
      (the DIAG.1 snapshot-then-release pattern EXACTLY; no stderr under any lock).
    * emits a [DSUM-SUMMARY] D-SUM-11 block: hot_zone_updates, zones_created/slid/merged/decayed/expired,
      zone_count min/mean(sum/samples)/max, protected_range min/max, frames_rejected_from_prune_due_to_hot_zone.
    * includes the header-comment interpretation note for D-SUM-11 (per build_config.h field-map).
- Gate the writer body behind CNR3_DIAG_PRINT_DSUM11_HOT_ZONE (print gate), consistent with the two-gate
  pattern. Emit at the same lifecycle points DIAG.1 uses (selftest runner synthetic summary + filter free).
- DO NOT modify the existing observe hooks or the _locked methods — they are correct and gated. Wiring the
  writer is the whole D-SUM-11 job.
```

## 3. D-SUM-10 (prune/eviction) — new struct + counters + the RE-CHURN field

```text
3.1 STRUCT (new, in cnr3_cache_diagnostics.h): Cnr3CachePruneDiagnosticStats
    - prune_invocations, prune_events_triggered (prune_is_required true), frames_evicted (total),
      bytes_evicted (if available from the summary), checkpoint_prunes, hot_zone_rejected (may mirror /
      cross-check the hot-zone field).
    - RE-CHURN (coordinator-priority): frames_evicted_then_re_requested; optional
      frames_re_requested_repeatedly (repeat re-requests of the same evicted frame = strong thrash signal).
    - RING + REPORTING fields (see 3.3): the fixed-capacity ring of recently-evicted frame numbers, a write
      index/head, live-entry count, ring_capacity (the resolved derived-or-overridden size), the derivation
      inputs used (checkpoint_search_bound_B, active_ceiling, k), ring_wrap_count, and ring_saturated flag.
      All observe-only; all compile out with the macro off.
    - TIER-1/2 state: gap_histogram[7] (bucketed re-request gaps), a bounded top-N map (cap 16) of
      frame_number -> re_churn_count, and the dump firing counters (window_dumps_emitted,
      full_dumps_emitted) used to enforce the Xa/Y caps. All observe-only, all compile out with the macro off.

3.2 EVICTION OBSERVATION (observe_prune_locked): at the prune sink
    (store_owned_frame_and_prune_impl / the point where frames are actually evicted, using
    Cnr3CombinedStoreAndPruneSummary which already reports the eviction outcome), record per prune:
    ++prune_invocations; if prune happened, add frames_evicted, and record the EVICTED FRAME NUMBERS into
    the bounded recently-evicted set (3.3). Gate behind CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION.

3.3 RE-CHURN DETECTION (the load-bearing new counter). Definition: a frame that was EVICTED and then
    subsequently RE-REQUESTED (its absence forces a recovery/rebuild — the avoidable-churn signal). This is
    the counter we most care about because we SUSPECT a real over-eviction pathology (the multi-thread
    fmUnordered behaviour). Bias the whole design toward NOT under-counting and toward being INSPECTABLE.

    RING (recently-evicted frame numbers), observe-only, ordered for replay:
    - Structure: a fixed-capacity ring buffer of int frame numbers + a write index (head) + a wrap/overflow
      counter + a "count of live entries" (up to capacity). Ordered so it can be REPLAY-PRINTED oldest->
      youngest using the write index. This ordering is the point: a count alone cannot distinguish "the same
      few frames thrashing" (tunable — pin/hot-zone bug) from "a broad spread of one-time re-requests"
      (inherent arrival disorder). The ordered ring lets the writer dump the actual eviction sequence so we
      can SEE which pattern it is.

    - CAPACITY — DERIVED, LARGE, and OVERRIDE-ABLE (coordinator requirement):
        * Derive at cache construction from the cache's own bounds so it auto-scales:
              derived_capacity = k * max(checkpoint_search_bound_B, cache_active_ceiling)
          with a GENEROUS k (k = 16, bumped from 8 at review — err large, suspected pathology) and a high
          FLOOR (>= 1024; consider 8192 on the normal profile). Resolved sizes: normal ~16000, tiny 1024.
          Rationale: a frame evicted more than
          ~max(B, request-ahead-depth) frames before the current request is unlikely to be re-requested, so
          k=16x that bound (with a >=1024 floor, consider 8192 on normal) is deliberately over-large. It is
          gated-off in production, so the
          only cost is during diagnostic runs — cheap insurance against under-counting.
        * PRINT/LABEL the resolved capacity (and the derivation inputs B, active_ceiling, k) in the D-SUM-10
          summary, so the run self-documents what size it used.
        * OVERRIDE SEAM: implement the capacity as a single clearly-commented line, e.g.
              const std::size_t ring_capacity = /*DERIVED*/ k * std::max(B, active_ceiling); // or floor
          with a commented-out override line immediately adjacent:
              // const std::size_t ring_capacity = 65536; // MANUAL OVERRIDE: uncomment to force a fixed size
          so we can comment out the derived line and drop in a fixed large literal without hunting.

    - OVERFLOW/WRAP REPORTING (coordinator requirement): every time the ring WRAPS (head passes a still-live
      oldest entry, i.e. an evicted frame number is overwritten before being matched), increment a
      ring_wrap_count and set a ring_saturated flag. The writer reports both. If ring_saturated is true, the
      re-churn count is a LOWER BOUND (some evict->re-request pairs may have aged out) and the summary must
      SAY SO explicitly ("re-churn >= N; ring saturated, count is a lower bound; consider larger capacity").
      This makes the counter self-labelling: honest under saturation rather than silently wrong.

    - MATCH MECHANISM: at the LOOKUP/PRESENCE-MISS point (requested frame found NOT present —
      cnr3_cache_slot_has_frame / has_frame() false on a request), scan the ring for that frame number; if
      present, ++frames_evicted_then_re_requested. Do NOT remove it (leave the eviction history intact for
      replay-print); instead track distinct-vs-repeat if cheap (a repeat re-request of the same evicted frame
      is itself a strong thrash signal — optional second counter frames_re_requested_repeatedly).

    - PURE OBSERVATION (R-PROCESS-19): the ring, the wrap counter, the scans, and all counters must have ZERO
      effect on cache behaviour, eviction policy, pinning, or lookup RESULTS. The lookup returns exactly what
      it returned before; the diagnostic only WATCHES the miss. With CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION
      undefined, the ring, indices, counters, and scans all compile OUT; lookup/prune behave identically.

    - REPLAY-PRINT: the D-SUM-10 writer (gated on the print macro) can optionally dump the ring contents
      oldest->youngest (walk from head+1 around to head over the live entries) as a compact line, so the
      eviction sequence is inspectable when diagnosing a suspected over-eviction pattern. Keep it a single
      bounded line (or capped to the last M entries if the ring is very large) — stderr, outside locks.

    FLAG FOR REVIEW: the ring capacity/derivation and the "is it large enough" question are a SPECIFIC
    REVIEW ITEM when this patch returns — confirm the derived size against the actual cache bounds and the
    observed multi-thread arrival spread before committing.

3.5 RE-CHURN READOUT DESIGN — distilled signal first, bounded raw dumps second (coordinator-designed).
    A large ring dumped raw is unreadable; the DIAGNOSIS lives in the PATTERN, not the raw frame numbers. So
    the primary output is DISTILLED (always on), and raw ring dumps are BOUNDED and independently gated.
    ALL of the following are observe-only and compile out with CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION off.
    Each artifact carries a distinct grep tag so it is independently searchable/analyzable.

    TIER 1 — RE-REQUEST GAP HISTOGRAM (always on; the money signal). For each detected re-churn, compute the
       GAP = (current_requested_frame_index_or_eviction_age) i.e. how many evictions/frames elapsed between
       the frame's eviction and its re-request. Bucket into a small fixed histogram, e.g.
       [1-10, 11-50, 51-100, 101-500, 501-1000, 1001-5000, 5000+]. Emit as [DSUM10-GAP-HISTO].
       INTERPRETATION (the inherent-vs-tunable answer): gaps clustered SMALL => frames re-requested almost
       immediately after eviction => OVER-EVICTION / evicting-too-eagerly = TUNABLE. Gaps broad/large =>
       legitimately-later re-requests = INHERENT to the arrival pattern.

    TIER 2 — TOP-N THRASHERS (always on). A small bounded map (cap N = 16) of frame_number -> re-churn count;
       keep the highest-count entries. Emit as [DSUM10-TOP-THRASH] e.g. "frame 47: 12x, frame 82: 9x, ...".
       INTERPRETATION: a FEW frames with high counts => localized thrash (a specific pin/hot-zone bug =
       TUNABLE, and it NAMES the culprits). MANY frames each x1 => broad churn = INHERENT.
       (Optional companion counter frames_re_requested_repeatedly from 3.3.)

    TIER 3 — BOUNDED RAW RING DUMPS. UNIFORM RULE: every dump type fires a BOUNDED number of times, never
       unbounded, so nothing can flood stderr. Three independently-gated behaviours:
       3a WINDOW (periodic, capped): every INTERVAL (100) evictions, dump the most-recent SIZE (100) ring
          entries, but only for the FIRST MAX_DUMPS (Xa = 12) firings, then quiet. INTERVAL == SIZE == 100 so
          consecutive firings tile the eviction stream with NO gaps and NO overlap. Captures early-run
          eviction behaviour (where a developing thrash first shows). Emit [DSUM10-RING-WINDOW] chunked
          100/line with index ranges. DEFAULT ON.
       3b FULL-AT-OVERFLOW (capped): on ring overflow (head overwrites a still-live oldest entry), dump the
          WHOLE ring, but only the FIRST MAX_DUMPS (Y = 4) overflows, then quiet. The "I need everything, but
          bounded" escape hatch. Emit [DSUM10-RING-FULL] chunked 100/line. DEFAULT OFF.
       3c FINAL (end-of-run tail, once): at teardown, dump the last COUNT (Z = 100) entries (or fewer if the
          run evicted < Z). Guarantees a raw view for SMALL CLIPS where 3a may never reach its interval and
          3b never overflows. Emit [DSUM10-RING-FINAL] with a saturation/coverage label:
          "[DSUM10-RING-FINAL] entries=N saturated=yes/no (showing last Z of total_evicted=M)" so it is
          honest about whether you are seeing EVERYTHING (small clip) or just the TAIL (large clip).
          DEFAULT ON.
       All three dumps: snapshot the needed entries UNDER the cache lock, then format+write OUTSIDE the lock
          (DIAG.1 discipline); walk oldest->youngest via the ring head; observe-only; no effect on eviction.

    3.5.1 BUILD-CONFIG GATE STRUCTURE (nested #ifdef; numbers live INSIDE their feature so a commented-out
       feature won't compile stale). ADD these NEW sub-feature flags to build_config.h INSIDE the EXISTING
       CMS07-B.1 CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION block (do NOT alter the existing compute/print/#error
       lines). Master gate off => the whole nested block (ring, tiers, dumps) compiles out (R-PROCESS-19).
       Home: build_config.h (consistent with all other diagnostic gates living there; the dump CODE lives in
       the diagnostics TU, only the SWITCHES live in build_config.h).

         #if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)   // existing master gate (unchanged)

         #   define CNR3_DIAG_DSUM10_RING_WINDOW_DUMP 1          // 3a DEFAULT ON  (comment out to disable)
         #   if defined(CNR3_DIAG_DSUM10_RING_WINDOW_DUMP)
         #       define CNR3_DIAG_DSUM10_RING_WINDOW_INTERVAL  100
         #       define CNR3_DIAG_DSUM10_RING_WINDOW_SIZE      100   // == interval: tiled, no gaps
         #       define CNR3_DIAG_DSUM10_RING_WINDOW_MAX_DUMPS 12    // Xa: first 12 firings only
         #   endif

         //  #define CNR3_DIAG_DSUM10_RING_FULL_DUMP 1           // 3b DEFAULT OFF (uncomment to enable)
         #   if defined(CNR3_DIAG_DSUM10_RING_FULL_DUMP)
         #       define CNR3_DIAG_DSUM10_RING_FULL_MAX_DUMPS   4     // Y: first 4 overflows only
         #   endif

         #   define CNR3_DIAG_DSUM10_RING_FINAL_DUMP 1           // 3c DEFAULT ON  (comment out to disable)
         #   if defined(CNR3_DIAG_DSUM10_RING_FINAL_DUMP)
         #       define CNR3_DIAG_DSUM10_RING_FINAL_COUNT      100   // Z: last 100 (or fewer) at teardown
         #   endif

         #endif  // CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION

       NOTE: adding these sub-flags to build_config.h is the ONE sanctioned build_config.h change for DIAG.2a
       (they are new gate definitions, the header's stated purpose). Do NOT touch the existing DSUM01-14
       compute/print/#error lines or any other gate. Each new sub-flag carries a one-line comment stating its
       default and that it is observe-only. The nesting IS the guard (a dump flag cannot be set with the
       master off), so no extra #error is needed.

3.4 WRITER: cnr3_cache_prune_diagnostic_write_summary(...) — snapshot-under-lock, format+write OUTSIDE lock,
    [DSUM-SUMMARY] D-SUM-10 block (prune_invocations, prune_events, frames_evicted, bytes_evicted,
    checkpoint_prunes, and prominently frames_evicted_then_re_requested = the re-churn signal). Gate behind
    the print gate. Same lifecycle emission points.
```

## 4. THE CRUX — observe-only + no behaviour change (R-PROCESS-19 / R-PROCESS-21)

```text
- The re-churn ring + counter is the ONLY genuinely new observation logic. It MUST be observe-only: it may
  READ frame numbers at evict and at lookup-miss, but must not alter which frames are evicted, stored, pinned,
  or returned. Do not let the ring influence prune candidate selection or lookup results in any way.
- Placement is ADDITIVE gated calls at the existing prune sink and the existing lookup-miss point — do NOT
  restructure store_owned_frame_and_prune_impl or the lookup path to host the observation (R-PROCESS-21).
  If hosting the observation cleanly requires any change to a proven method's control flow, STOP and propose
  that specific change first.
- Lock discipline (DIAG.1 pattern): observation may run under the cache lock (the counters are per-core
  members like hot_zone_diag_stats_); all FORMATTING/stderr WRITING happens on a snapshot OUTSIDE the lock.
- Bounded ring is a DIAGNOSTIC structure: fixed cap, no unbounded growth, no allocation in the hot path
  beyond the fixed buffer. With the macro off it does not exist.
```

## 5. Hard constraints (do / do not)

```text
DO:
  - D-SUM-11: add ONLY the summary writer + print-gate wiring; leave the live observe hooks untouched.
  - D-SUM-10: add the struct, the evict + lookup-miss observations (additive, gated), the bounded
    recently-evicted ring, the re-churn counter, and the writer.
  - Reuse cnr3_diag_write_line / the DIAG.1 snapshot-outside-lock pattern; [DSUM-SUMMARY] tag.
  - Consume the existing build_config.h gates (DSUM10_PRUNE_EVICTION, DSUM11_HOT_ZONE) UNCHANGED.
  - Deliver the R-PROCESS-19 observe-only proof for BOTH families (macro-off = identical behaviour).
DO NOT:
  - Modify the EXISTING build_config.h gates (DSUM01-14 compute/print/#error) or the hot-zone observe hooks.
    (EXCEPTION: DIAG.2a ADDS the new D-SUM-10 ring-dump sub-flags INSIDE the existing DSUM10 block per 3.5.1
    — that is the one sanctioned build_config.h addition; the existing gate lines stay untouched.)
  - Let the re-churn ring/counter influence eviction, lookup, pinning, or return decisions (observe-only).
  - Restructure the prune sink or lookup path to host observations (propose first if needed; R-PROCESS-21).
  - Emit stderr inside any cache lock / CMS atomic scope.
  - Implement D-SUM-04/05/08 (DIAG.2b) or any getFrame/recovery family (DIAG.3).
  - Use the [KDT-SUMMARY] tag (that is Keystone dev-trace; D-SUM uses [DSUM-SUMMARY]).
```

## 6. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2, DEFAULT build_config (KDT off), with
   CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION and CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE DEFINED.
   Four-way: 56/56 / 56/56 / 55/56 exit 1 / 56/56. Confirm the D-SUM-10 and D-SUM-11 [DSUM-SUMMARY] blocks
   emit at end-of-run with populated fields (esp. frames_evicted_then_re_requested), AND the readout
   artifacts emit with their grep tags: [DSUM10-GAP-HISTO], [DSUM10-TOP-THRASH], [DSUM10-RING-WINDOW]
   (default on), [DSUM10-RING-FINAL] (default on); [DSUM10-RING-FULL] only when its flag is enabled.
2. R-PROCESS-19 OBSERVE-ONLY PROOF (exit gate) — for BOTH families: rebuild with BOTH macros UNDEFINED ->
   compiles/links cleanly (the prune diag struct, ring, counter, and both writers compile OUT), four-way
   IDENTICAL to step 1, and (recommended) a .vpy run byte-identical macro-on vs macro-off. Also test each
   macro independently off (10 off / 11 on, and 11 off / 10 on) to prove the gates are independent. ANY
   behavioural divergence under any macro-off = defect in the gate; fix before commit.
3. R-PROCESS-21: confirm the prune sink and lookup path proven behaviour is unchanged (only additive gated
   observation added). The cache-core selftests (ref-balance, prune, hot-zone) must pass unchanged.
4. CHURN VALIDATION (the point of the exercise): run the recovery .vpy harness S-series under -r 1 and
   confirm the D-SUM-10 re-churn counter and D-SUM-11 hot-zone eviction fields populate sensibly (e.g. S1
   in-order -> ~0 re-churn; S3/S7/S8 disorder -> non-zero, bounded). This is a SANITY read of the counters,
   not a value-identity gate.
5. RING-SIZE REVIEW (SPECIFIC coordinator review item): confirm the resolved ring_capacity (printed in the
   summary) is comfortably larger than the observed evict->re-request gap for the tested arrival patterns,
   and that ring_saturated is FALSE on the diagnostic runs (if it ever saturates, the re-churn count is a
   lower bound -> increase k or use the manual override). Report the resolved capacity, its derivation
   inputs (B, active_ceiling, k), and the wrap count for each scenario. Because we SUSPECT a real
   over-eviction pathology, err large: a saturating ring on these tests means the size is inadequate for the
   diagnosis and must be raised before drawing any inherent-vs-tunable conclusion.
```

## 7. Expected result + what it buys

D-SUM-11 writer is small (hooks already live). D-SUM-10 is the real work (struct + two observation points +
ring + counter + writer). Once committed, running the S-series -r 1 sweep with these summaries on gives the
DETERMINISTIC prune/hot-zone/re-churn baseline per arrival pattern — the first half of answering
inherent-vs-tunable. D-SUM-12 (recovery-rate, DIAG.3) is the second half. No performance claim here; this is
observe-only telemetry.

## 8. Out of scope

D-SUM-04/05/08 (DIAG.2b), all getFrame/recovery families incl. D-SUM-12 (DIAG.3), D-SUM-02 memory (DIAG.4),
any behavioural/policy change to prune/eviction/hot-zone (this observes the CURRENT policy; TUNING it is a
later, separate decision informed by these counters), CMS design.

BUILD_CONFIG.H — PRECISE BOUNDARY (resolves the earlier ambiguity; §3.5.1 is controlling): DIAG.2a MAY add
the NEW nested D-SUM-10 ring-dump sub-flags INSIDE the existing CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION
block (the eight new defines: RING_WINDOW_DUMP/INTERVAL/SIZE/MAX_DUMPS, RING_FULL_DUMP/MAX_DUMPS,
RING_FINAL_DUMP/FINAL_COUNT). DIAG.2a MUST NOT alter any existing DSUM01-14 compute/print/#error gate line,
nor any other build_config.h content. So build_config.h is NOT wholesale out-of-scope — only the existing
gate lines and unrelated content are; the new D-SUM-10 sub-flags are the one sanctioned addition.
