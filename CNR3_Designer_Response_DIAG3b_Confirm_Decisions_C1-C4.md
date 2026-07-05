# CNR3 — DESIGNER RESPONSE: DIAG.3b Confirm Report — Decisions C1-C4 (2026-07-04)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Re:** `CNR3_DIAG3b_Coder_Confirm_Report_to_Designer_2026-07-04.md`
**Status:** REPORT ACCEPTED. All load-bearing claims independently verified against the post-DIAG.3a source.
**Outcome:** PROCEED to DIAG.3b patch generation per the report's §7 file list + decisions C1-C4 below.

---

## 1. Verification statement (designer independently checked)

```text
VERIFIED TRUE (every claim checked against source):
- ALIAS PATHS: the four+frame0 copyFrame alias guards exist exactly as reported (arAllFramesReady
  937/1215/1413/1596 + frame-0), each a proven pointer-equality branch (output == source) freeing the
  single shared reference via the output-named variable. The coder's classification rule is CORRECT.
- DUAL-REFERENCE PRODUCTION PATTERN: addFrameRef(output_frame) at 674 and 1807 creates the CACHE's own
  reference while the original computed output remains the return candidate; the source comment at 1898
  explicitly documents "the extra addFrameRef() reference is stored in the [cache]". The coder's two-
  ownership-pattern analysis (AS2-adopt vs production-dual-ref) is source-accurate.
- ALLOWS_RETURN: cnr3_live_store_status_allows_return has EXACTLY ONE call site (1868, the frame-0 path);
  the authoritative-return helper decides via explicit ==ok / !=duplicate checks internally. Hooking only
  the named function would indeed miss most return decisions. Finding confirmed.
- D-SUM-14 FIELDS: all nine process-summary fields exist as listed; CNR3_CACHE_CHECKPOINT_INTERVAL exists
  (3 tiny / 10 normal) for the near-grid definition.
- SIX RETRIEVE CLASSES: accepted — the scope's candidate list was a subset; the coder's enumeration is the
  real inventory. The patch counts ALL SIX retrieve classes and ALL the release sites (incl. alias) or the
  balance will not close under churn.
```

## 2. DECISION C1 — D-SUM-07 ownership interpretation: the CODER'S interpretation is APPROVED

```text
- temporary_outputs_created  ++ ONLY for a successful, non-null, NON-ALIAS copyFrame result. Null creates
  nothing; an alias creates nothing (it IS the source reference — see C-ALIAS below).
- temporary_outputs_stored   ++ ONLY when the ORIGINAL created output is itself consumed by cache
  ownership: the AS2 floor/hole paths (reset_to_owned_frame of floor_output_frame / hole_output_frame).
- The production-path addFrameRef(output_frame) cache copy is a DISTINCT reference OUTSIDE this balance.
  Do NOT count it as stored (it would double-consume one creation and drive the balance negative), and do
  NOT redefine created to include it — "temporary output" means a produced frame, not a refcount. The
  cache-side reference's lifecycle is covered at the cache boundary (D-SUM-04 territory + OwnedFrameRef
  RAII), which is the clean layering.
- temporary_outputs_transferred ++ when the original leaves getFrame as the returned frame (store-ok
  production return, frame-0 return).
- temporary_outputs_released ++ when the original is freed (hard failure, process failure, duplicate
  loser, bad byte-count, etc).
- duplicate_computed_but_discarded ++ specifically for the duplicate-loser frees (a subset of released —
  count in BOTH released and this field; state that in the writer so the overlap is explicit and the
  balance equation stays created == stored + released + transferred).
- BALANCE: temporary_output_balance = created - (stored + released + transferred) == 0 at teardown.
```

## 3. DECISION C-ALIAS — the alias/null classification rule (verbatim for the patch)

```text
- NULL copyFrame: created NOT incremented; the branch's freeFrame(source) is a D-SUM-06 source release.
- ALIAS copyFrame (pointer-equality branch proven): created NOT incremented; the branch's
  freeFrame(output-named-variable) is a D-SUM-06 SOURCE release (it releases the single shared source
  reference). It is NOT a D-SUM-07 temp-output release. Count exactly once, in D-SUM-06 only.
- These rules keep BOTH balances closed: D-SUM-06 sees every source acquire/release including the alias
  and null paths; D-SUM-07 sees only genuinely-produced outputs.
```

## 4. DECISION C2 — D-SUM-09: ADDITIVE outcome-known hooks. The refactor is REJECTED.

```text
Routing all return decisions through cnr3_live_store_status_allows_return() would be a control-flow
change to proven code for diagnostic convenience — precisely what R-PROCESS-21 exists to prevent, and
unlike DIAG.2b's A2 there is NO necessity here (additive hooks fully suffice). Implement:
- observe at the frame-0 allows_return site (1868);
- observe inside/around cnr3_store_live_output_frame_for_authoritative_return once store_status is known,
  at each outcome-known exit (exactly-once discipline, DIAG.2b §3.2);
- the five proposed reason ROWS are accepted as rows (no new public enum), grouped as decision-stage
  (hard_store_failure, store_status_not_returnable) vs transfer-stage (duplicate_winner_lookup_failed,
  null_return_frame, discard_failed_after_return_ready) — keep the two groups visually separate in the
  writer so "decision" and "transfer" accounting stay distinct per the gate comment.
- lookup_ref_transferred/released: return-side Cnr3OwnedFrameRef objects only (returned_cache_ref,
  cached_winner_ref) — transfer_to_caller vs reset/release. lookup_ref_balance == 0.
- DISJOINTNESS (accepted as analyzed): D-SUM-09 is return-boundary accounting; D-SUM-04 is cache-core
  lookup-ref accounting; same underlying frame may be visible to both at different layers but no hook
  increments the other family's counters, and each balance closes independently.
```

## 5. DECISION C3 — D-SUM-14 source_copy_reset_frames: SCENE-DRIVEN ONLY

```text
Count scene_change_reset_output_used summaries ONLY. Frame-0 and floor fresh-starts are STRUCTURAL
resets already counted in D-SUM-12 (frames_frame0, frames_recovered_floor); including them here would
double-report and conflate scene behaviour with recovery structure. The D-SUM-14 writer adds ONE note
line cross-referencing D-SUM-12 for structural fresh-starts (self-documenting the boundary, in the
style of the D-SUM-04 A3 narrowing note).
```

## 6. DECISION C4 — cut_near_grid_checkpoint_count: distance-to-nearest-grid <= 1 (both sides)

```text
The coder's suggested (n % I == 0 or 1) misses the just-BEFORE-grid case. Definition (fixed radius 1,
BOTH sides): for a scene-change detection at frame n, with I = CNR3_CACHE_CHECKPOINT_INTERVAL:
    near_grid  iff  min(n % I, I - (n % I)) <= 1        (i.e. n % I in {0, 1, I-1})
HONESTY NOTE (print in the writer): with the TINY profile (I=3) every frame is within distance 1 of a
grid multiple, so this counter equals scene_change_detections in tiny builds and is only discriminating
in the NORMAL profile (I=10). One interpretation line stating this prevents misreading tiny-profile runs.
```

## 7. Also approved

```text
- D-SUM-06 six-retrieve-class inventory + the full release-site enumeration (incl. alias paths): the
  patch counts ALL of them; the scope's shorter candidate list is superseded by the coder's inventory.
- same_activation_request_violations observed immediately before each source retrieve where the branch's
  request predicate is false, BEFORE the existing bail (observe-only; the existing control flow is
  unchanged). partial_acquire_failures at getFrameFilter-null.
- 6-CONFIG R-PROCESS-19 matrix (all-on / 06-off / 07-off / 09-off / 14-off / all-four-off), each with the
  four-way block. Temporary build_config edits not committed.
- File list per report §7 (seven files; no build_config, no cache-core, no project files).
```

## 8. Proof gate (restated with the decided semantics)

```text
1. Four-way all-on: 56/56 / 56/56 / 55/56 exit 1 / 56/56; D-SUM-06/07/09/14 blocks emit.
2. R-PROCESS-19 six-config matrix: each clean + four-way identical + family block absent when off.
3. S-series -r 1 (S1/S3/S7/S8):
   - D-SUM-06 source_frame_release_balance == 0 on all four; same_activation_request_violations == 0;
     partial_acquire_failures == 0.
   - D-SUM-07 temporary_output_balance == 0 on all four (the C1 semantics make this provable; S7/S8's
     churn with duplicate losers is the hard test — duplicate_computed_but_discarded may be non-zero
     there and the balance must STILL close).
   - D-SUM-09 lookup_ref_balance == 0; decisions yes+no == checked; the reason rows sum consistently.
   - D-SUM-14: scene fields consistent (S7/S8 segment boundaries may register detections);
     promotion_mismatches == 0; the tiny-profile near-grid caveat line prints.
   - Prior families (01/03/04/05/08/10/11/12/13) unchanged: all balances 0, violations 0, failures 0.
4. A non-zero balance with no real leak = a missed site -> STOP and report (do not commit).
```

## 9. Authorization

Decisions C1-C4 + C-ALIAS issued. **Generate the DIAG.3b patch** against the post-DIAG.3a baseline.
Deliver .patch + patch-notes + apply commands. Designer diff-review will check hardest: (a) the alias/null
classification exactly per C-ALIAS at every alias block; (b) C1's stored-vs-addFrameRef separation (no
double-consume); (c) the six retrieve classes all counted; (d) additive-only at the return-decision sites
(no allows_return refactor); (e) six-config compile-out; (f) exactly-once at every multi-exit site.
