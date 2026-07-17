# CNR3 — RING & PLAN-TRACE: Design Rationale and Intent — v1

**Purpose:** this document consolidates, in one durable place, the FULL design rationale, exploration
reasoning, findings, and intent for two related diagnostics: (A) the D-SUM-10 recently-evicted RING and
its FI-11 correlation story (committed, DIAG.2a), and (B) the PLAN/RESULT PLAN-TRACE family (DIAG.3c,
spec drafted, not yet built). A great deal of designer/coordinator exploration produced these designs;
this document exists so that context survives chat turnover and coordinator memory. **REQUIRED READING
for any new designer chat. PRIMARY INPUT (with the spec v1 + the coder cross-check) to the plan-trace
spec v2 and the DIAG.3c scope.**

---

## PART A — THE RING (D-SUM-10 recently-evicted ring) — COMMITTED (DIAG.2a)

### A.1 What it is
A fixed-capacity circular buffer inside the D-SUM-10 prune/eviction telemetry recording the FRAME
NUMBERS of recently-evicted frames, oldest -> youngest. On eviction the frame number enters the ring;
when full, the oldest entry is overwritten.

### A.2 The sizing discipline (reused by later families — a house pattern)
- DERIVED capacity, self-documenting: k * max(checkpoint_search_bound_B, active_ceiling), k=16,
  floor 1024 => 16000 normal profile / 1024 tiny. The derivation INPUTS are printed in the summary
  (B, ceiling, k) so a reader can verify the sizing without the source.
- SATURATION-HONEST: ring_wrap_count + ring_saturated flag; when saturated the summary says counts are
  lower bounds. NEVER silently lossy. (This capacity+saturation discipline was reused for the D-SUM-13
  open-addressed recalculation table in DIAG.3a — treat it as the standing pattern for any bounded
  diagnostic container.)
- Dumps: [DSUM10-RING-WINDOW] (first ~100, fires once at a threshold), [DSUM10-RING-FINAL] (last ~100 in
  50-entry chunks at teardown), [DSUM10-RING-FULL] (full dump at overflow, DEFAULT OFF). Nested
  sub-flags live INSIDE the DSUM10 gate in build_config.h (numbers inside their feature #if).

### A.3 What the ring was FOR, and what we learned (the FI-11 story — do not lose this)
The original churn question (coordinator priority): under fmUnordered without -r 1, ~50% of frames were
being recovered — is that INHERENT to the workload or TUNABLE (over-eviction)? D-SUM-10 added an
evict-then-re-requested re-churn counter hooking the two cache-lookup not_found sites
(lookup_frame_and_add_ref_locked, pin_frame_locked).

THE FINDING (S7/S8, -r 1): the ring PROVED real churn was happening — 168 evictions into regions
(950-1200, 1950-2000) that the run then JUMPED BACK INTO — yet the re-churn counter read ZERO. Source
trace explained it: arInitial looks up the PREDECESSOR (N-1, usually resident), not the requested frame
N; when the predecessor is absent it builds a RECOVERY plan via
plan_bounded_recovery_search_and_record_anchor_pin, which searches for an ANCHOR and rebuilds from
SOURCE frames WITHOUT any cache-lookup-miss on the evicted frame numbers. So the COSTLY churn
(evict-then-REBUILD) flows entirely through the recovery path, which the lookup-side counter never sees.
The ring showed the churn was real; the counter watched the wrong door.

CONSEQUENCE: D-SUM-12 (recovery-plan/rate, DIAG.3a) was built as the correct measurement — recovery
plans, exact/floor split, recovery-rate %, spans. FIRST BASELINE (-r 1): S1 0% / S3 27.5% / S7 0.375% /
S8 21.4%. The S8-vs-S7 contrast is the headline insight: **recovery churn is driven by ARRIVAL DISORDER
within a working set (shuffle), not by distant seeking (jumps)** — jumps land at segment starts where
predecessors are resident; it is the shuffle WITHIN segments that forces recovery.

### A.4 The correlation — offline now, in-run deferred (FI-11) — COORDINATOR: WILL BE NEEDED
The FI-11 question — how much recovery is evict-then-rebuild of a JUST-EVICTED region — is answered by
correlating the ring (what was evicted) against D-SUM-12 (what was rebuilt):
- OFFLINE correlation: AVAILABLE NOW, zero code. Read [DSUM10-RING-*] dumps and D-SUM-12 recovered
  targets/spans FROM THE SAME RUN; ask "were the recovered frames in the evicted region." USE: during
  the FI-11 inherent-vs-tunable analysis, post-arc / before the real-footage campaign.
- IN-RUN correlation COUNTER (live "recovered target was in the recently-evicted ring" flag): DEFERRED.
  It needs a getFrame-side read of cache-internal ring state -> breaks the getFrame/cache family
  boundary and couples the DSUM10+DSUM12 gates. Build ONLY if offline proves insufficient OR the signal
  is wanted LIVE (e.g. watching churn in real time during the real-footage campaign). If built: a small
  DSUM10+DSUM12-gated follow-up, investigate-first, observe-only, the cross-family read scoped with care.
- **COORDINATOR STANDING NOTE: the coordinator intuits the in-run counter WILL eventually be needed.
  Keep it visible in every plan revision. Do not let it decay to a forgotten FI line.**

### A.5 Interpretation guide for ring-era artifacts (for whoever does the FI-11 analysis)
- [DSUM10-GAP-HISTO] (eviction-to-re-request gap): SMALL-gap cluster => over-eviction (TUNABLE);
  broad spread => inherent workload behaviour.
- [DSUM10-TOP-THRASH]: few HIGH-count frames => localized thrash (nameable culprit, tunable);
  many x1 => broad (inherent).
- Remember the counter feeding these hooks the LOOKUP path — under -r 1 they read near-zero even with
  real churn (the FI-11 finding). The recovery-side numbers (D-SUM-12) are the primary churn signal;
  the ring dumps are the eviction evidence to correlate against.

---

## PART B — THE PLAN/RESULT PLAN-TRACE FAMILY (DIAG.3c) — SPEC DRAFTED, NOT BUILT

### B.1 What it is (the intent)
A per-output-frame PLAN-vs-RESULT trace. For each requested output frame, TWO matched records:
- **O record** (phase "O", written at arInitial EXIT): the PLAN — strategy + intent per item.
- **R record** (phase "R", written at arAllFramesReady EXIT): the RESULT — outcome per item, or where
  it failed.
Pairing is by frame number (under fmUnordered the two records are time-separated and interleaved with
other frames). The pair answers, for any frame: what did we PLAN to do, what ACTUALLY happened, and if
it died — where and why.

### B.2 The vocabulary (five enum sets — spec v1 §4; source-derived)
```text
Set 1  O-frame STRATEGY: CACHE_HIT / FRAME0 / PRED_PRESENT / RECOVERY_EXACT / RECOVERY_FLOOR (+NONE guard)
Set 2  O-item INTENT ROLES (labelled lists + composed one-letter machine codes):
       target T / predecessor P / anchor A / floor F / holes H / sources S / pinned N
       (a frame can hold MULTIPLE roles: overlap is real and shown honestly, e.g. 00000120=HS)
Set 3  R-frame OUTCOME: RETURNED_CACHE_HIT / RETURNED_COMPUTED / RETURNED_RECOVERED / FAILED
Set 4  R-item OUTCOME: C computed / K adopted_skipped / L post_compute_loser / U unpinned / N none /
       X not_reached / E error_here   (X/E need the bail-path writes — the invasive part)
Set 5  FAILURE-REASON (~13 categories on FAILED; the BRANCH context is already carried by the frame
       code, so category+strategy reconstructs the exact bail message)
```
Glyph convention: frame numbers ZERO-PADDED; trailing `*` = checkpoint-protected (legend-defined,
extensible with further glyphs later). Legends print at the top of every dump.

### B.3 Design decisions AND THE REASONING BEHIND THEM (the exploration — do not lose)
1. **pinned/unpinned are per-item FACTS, not counts.** We explored and REJECTED pin balances here twice:
   (i) a GLOBAL pin-count comparison across the O/R phases is SEMANTICALLY INVALID — between one frame's
   arInitial and its arAllFramesReady, OTHER frames pin/unpin on the same shared cache, so the delta
   mixes everyone's activity; (ii) even plan-local taken/released COUNTS were dropped — coordinator
   ruling: "this exercise is about SEEING PLANS, not pin counts." Only the per-item facts remain:
   pinned (O-item), unpinned (R-item). (Valid balances live elsewhere: D-SUM-04 cache-side, D-SUM-12
   plan lifecycle.)
2. **Buffered clean BLOCKS, not streaming.** We explored streaming (crash-proof by construction) and
   REJECTED it on a coordinator requirement that is easy to forget and easy to violate: the output must
   be CLEAN, SELF-CONTAINED, COPY-PASTEABLE BLOCKS that can be pasted into designer/coder chats and
   tools. A live stream interleaved across frames (and with all other stderr) is grep-and-reconstruct;
   a block is copy-and-go. The three-party relay workflow is block-oriented.
3. **Failure survival WITHOUT streaming = dump-on-bail.** The worry: VS tears down after a bail, so an
   end-of-run-only dump loses exactly the case you most want. Traced the actual failure behaviour:
   EVERY bail is uniform — cnr3_set_filter_error(...) then `return nullptr`; no abort/throw/assert.
   So the failing getFrame call itself is the reliable place to flush: ON BAIL, dump the buffer-so-far
   as ONE clean sorted block (same format, "as far as we got"), once-guarded so end+bail cannot
   double-emit. The failed frame's E/X items + failure-reason are the last entries. Clean-end OR bail:
   exactly one block per run either way.
4. **FLUSH-ALWAYS is a HARD requirement** (now ratified as R-PROCESS-24): per-line flush is load-bearing
   for (i) crash-survival (bytes on the wire before `return nullptr`) and (ii) multithread ordering
   (unflushed lines reorder vs events, corrupting the datetime view). no_flush is FORBIDDEN in this
   family. The writer default already flushes; the block dump ends with an explicit flush.
5. **Windowed preallocated buffer, NO ring.** Records only for from_frame <= n <= to_frame (inclusive),
   tested at each function exit; capacity = 2*(window size). Window-bounded => cannot overflow => no
   ring, no saturation machinery needed HERE (contrast the D-SUM-10/13 containers, which are unbounded
   inputs and need the saturation discipline).
6. **Keys and time.** Zero-padded frame + a monotonic per-instance action_seq (arrival order independent
   of frame number); enter+exit datetimes per record + derived ms. Hot path stores steady_clock ticks
   (cheap, monotonic, sortable); the READABLE UTC column is derived AT DUMP TIME from a once-captured
   (steady,system) anchor pair — formatting happens single-threaded at the dump, not on the hot path.
7. **Three sorted views**, each legend-headed, each sub-#ifdef gated: (a) by enter-datetime+phase
   (temporal — what arrived/completed when), (b) by frame+phase (pairing — each plan directly above its
   result), (c) by phase+frame (all opens as a block, all results as a block). External re-sorting also
   works because of the zero-padded keys — that was the point of them.
8. **Emission format**: human-primary labelled lists (one per line, O and R align by eye) + appended
   machine codes line (codes=[frame=ROLES,...]) for parsers. Both audiences served by one record.

### B.4 The coder cross-check findings (2026-07-04) — INPUTS TO SPEC v2 (with the spec v1)
1. **`sources` must be BRANCH-SPECIFIC** — a REAL GAP in spec v1: cache_hit/frame0/pred_present request
   frame n WITHOUT populating source_request_frame_numbers (that vector is recovery-only), so the
   sources role cannot be derived from one field; derive per-branch.
2. **Site-to-failure-category TABLE**, built from the actual source lines, NOT a message parser.
3. **Add categories**: ALLOCATION_FAILED, RECOVERY_PLAN_FAILED_OR_REFUSED, HOT_ZONE_OBSERVATION_FAILED —
   the bail-site count is >50 once arInitial + top-level are included (spec v1's "~50" was AR-side only).
4. **from/to = COMPILE-TIME for the first cut** (DECIDED — mirrors the gate style, easier macro-off proof;
   runtime .vpy params can come later if wanted).
5. **dump-on-bail is the INVASIVE part** — it and the E/X bail-site writes touch the >50 setFilterError
   sites (control-flow-adjacent, R-PROCESS-21/25 territory).

### B.5 Sequencing and the likely split (recorded in Condensed Plan v1.6 / Doc B)
DIAG.3c comes AFTER DIAG.3b, LAST among the getFrame families, precisely because of B.4(5): it is the
only diagnostic that touches the bail sites. LIKELY SUB-SPLIT at scoping time:
- **3c.1** — the buffered plan/result CAPTURE + clean-end dump: pure observe-only, NO bail touch,
  provable with the standard R-PROCESS-19 pattern. (Sets 1-3 + the C/K/L/U/N subset of Set 4.)
- **3c.2** — the DUMP-ON-BAIL mechanism + the E/X + failure-reason bail-site writes: the invasive part,
  its own R-PROCESS-21/25 review, with the site-to-category table as its foundation artifact.
Decide the split when 3c is scoped, not before. Path: spec v1 + B.4 + this document -> SPEC v2 ->
3c scope (designer) -> coder confirm -> patch(es).

### B.6 What 3c gives the project once built (why it earned all this design effort)
- Failure forensics: one copy-pasteable block showing, for a dead run, every frame's plan, how far each
  got, the item it died on (E), what never ran (X), and why (category) — reconstructable offline.
- Plan-quality audit: systematic comparison of intended holes/sources vs actual outcomes (the K/L/C
  split exposes wasted compute and race losses frame-by-frame).
- The per-frame counterpart to the AGGREGATE families: D-SUM-03/12/13 say how much recovery/recompute
  happened; the plan-trace says WHICH frames and EXACTLY HOW, when an aggregate anomaly needs drilling.

---

## PART C — how A and B relate (the one-paragraph mental model)
The RING tells you what the cache THREW AWAY. D-SUM-12 tells you what the recovery path REBUILT. Their
correlation (offline now; in-run later if needed — FI-11) answers whether we rebuild what we just threw
away (tunable over-eviction) or genuinely new work (inherent). The PLAN-TRACE (3c) is the microscope
under all of it: when the aggregates say something odd happened, the plan-trace shows the exact frames,
plans, outcomes, and failure points. Aggregates first (cheap, always-on-able); microscope second
(windowed, on-demand); both share the house disciplines — derived-capacity/saturation-honest containers,
flush-always (R-PROCESS-24), observe-only gates (R-PROCESS-19), propose-before-transform (R-PROCESS-25).
