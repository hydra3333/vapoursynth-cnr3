# CNR3 PATCH SCOPE — DIAG.3b v1: D-SUM-06 source-lifecycle + D-SUM-07 temp-output-lifecycle + D-SUM-09 return-transfer + D-SUM-14 scene-reset

**From:** designer/reviewer (W3D), via coordinator (W3X).
**Type:** formal patch scope (R-PROCESS-19 observe-only proof = exit gate; R-PROCESS-21 additive-only).
**Baseline:** current post-DIAG.3a committed source (verified: observe_recovery_plan_published /
compute_count_map_saturated present). Gates for all four DIAG.3b families verified present in
cnr3_build_config.h with the two-gate #error pattern; consume UNCHANGED, add nothing.
**Batch:** DIAG.3b = D-SUM-06 + D-SUM-07 + D-SUM-09 + D-SUM-14 (the lifecycle/return/scene batch). This
COMPLETES the getFrame observe-only-summary work; DIAG.3c (plan/result plan-trace, the bail-touching family)
follows separately.

## 1. Architecture — the D-SUM-01/03/12 getFrame pattern (established, mirror it)

Same as DIAG.3a. Per-family gated stats struct in cnr3_diagnostics.h, instantiated in the filter INSTANCE
DATA (cnr3_plugin_internal.h / Cnr3FilterData); per-instance diagnostics-only mutex; observe under the diag
mutex, snapshot, release, format+write OUTSIDE. Observe helpers gated by each family's COMPUTE macro, called
from arInitial/arAllFramesReady. Writer in cnr3_diagnostics.cpp (print gate), [DSUM-SUMMARY] tag,
interpretation line from the gate comment, flush at writer end. Free-filter emission in numeric order.
Selftest synthetic reference emitters per pattern. NO cache-core change expected; NO build_config change.

**THREE of these four are BALANCES** (06/07/09) — same discipline that made D-SUM-04/D-SUM-12 provable:
identify the single/bounded set of acquire and release sites, count each, net to zero at teardown, and make
the S-series zero-balance-under-churn the acceptance backstop. D-SUM-14 is a summary-reader (lowest risk).

## 2. D-SUM-06 — SOURCE_FRAME_LIFECYCLE (balance)

```text
FIELDS (gate comment, verbatim): source_frames_requested_total, source_frames_retrieved_total,
  source_frames_released_total, source_frame_release_balance, same_activation_request_violations,
  source_frame_count_max, partial_acquire_failures, source_frame_release_balance_errors.
HOOK SITES (verified):
  REQUEST  requestFrameFilter(n/source_frame_number, data.source, frame_ctx) — arInitial:67,102,132,574.
  RETRIEVE getFrameFilter(...) source-side — arAllFramesReady:808 (trigger), 892 (hole/source). EXCLUDE
           output/cache getFrameFilter (those are D-SUM-07/09 territory) — coder confirm each getFrameFilter
           is source-side vs output-side before counting.
  RELEASE  freeFrame(source_trigger_frame):826; freeFrame(source_frame):913,927,965. EXCLUDE freeFrame of
           output/cache/return frames (688 cache, 723/743/753/762 output, 1040 return) — those are D-SUM-07/09.
BALANCE: source_frame_release_balance = retrieved_total - released_total, MUST be 0 at teardown.
  same_activation_request_violations: a retrieve whose frame was NOT requested in the SAME activation
  (the VS-LIFECYCLE-01 invariant — request in arInitial, retrieve in arAllFramesReady of the same
  activation). partial_acquire_failures: a requested source that failed to retrieve (getFrameFilter null).
CONSTNESS/SYNC: getFrame-side, per-instance diag mutex. Multi-exit exactly-once at retrieve/release sites.
INTERPRETATION (gate comment): retrieved/released must balance; retrieve without same-activation request is
  a lifecycle violation; partial acquire failure must be inspected even when cleanup is clean.
```

## 3. D-SUM-07 — TEMP_OUTPUT_LIFECYCLE (balance)

```text
FIELDS (gate comment, verbatim): temporary_outputs_created, temporary_outputs_stored,
  temporary_outputs_released, temporary_outputs_transferred, temporary_output_balance,
  caller_still_owns_temporary_output, duplicate_computed_but_discarded.
HOOK SITES (verified, arAllFramesReady):
  CREATE   output frame produced (copyFrame/newVideoFrame of the computed output) — coder enumerate the
           exact creation points (the compute-completion sites; the K.1D/K.1E/D.3 branches each produce one).
  STORE    reset_to_owned_frame adopt into cache (682) / store_owned_frame path — stored++.
  RELEASE  freeFrame(output_frame):723,743,753,762,939,969 (the discard-on-failure / duplicate-loser paths).
  TRANSFER transfer_to_caller():779 (cached winner), 863 (returned cache ref) — transferred++.
BALANCE: temporary_output_balance = created - (stored + released + transferred), MUST be 0.
  caller_still_owns_temporary_output: transferred count that left via transfer_to_caller (the caller now owns
  it — expected, not a leak; this field documents the hand-off, not an error).
  duplicate_computed_but_discarded: computed an output that lost the first-in-best-dressed race and was freed
  (the adopted_post_compute_loser path) — normal under stress, counted for churn visibility.
CONSTNESS/SYNC: as above. Multi-exit exactly-once (the store impl / compute paths have several outcome exits).
INTERPRETATION (gate comment): duplicate computed/discarded may be normal under stress; clean ownership is
  the key question — no leak, no double-free, no ambiguous owner, documented balance equation.
```

## 4. D-SUM-09 — RETURN_TRANSFER (balance + decision split)

```text
FIELDS (gate comment, verbatim): return_decisions_checked, return_decision_yes, return_decision_no,
  return_no_reason_split, return_transfer_attempted, return_transfer_succeeded, lookup_ref_transferred,
  lookup_ref_released, lookup_ref_balance, output_authoritative.
HOOK SITES (verified):
  DECISION cnr3_live_store_status_allows_return(...) (arAllFramesReady:536) — decisions_checked++, and
           yes/no split on its result; return_no_reason_split categorizes the no-cases (coder propose the
           small reason enum from the allows_return logic — e.g. not_authoritative / store_failed / etc).
  TRANSFER the return-transfer boundary (transfer_to_caller for the returned frame; return frame vs
           return nullptr at the getFrame exit) — transfer_attempted/succeeded.
  LOOKUP-REF the return-side lookup ref (distinct from D-SUM-04's cache-side lookup-ref): transferred vs
           released at the return boundary; lookup_ref_balance MUST be 0. output_authoritative: the frame
           returned was the authoritative first-in-best-dressed output.
NOTE: D-SUM-09's lookup_ref_balance is the RETURN-side balance (getFrame output hand-off), NOT a duplicate
  of D-SUM-04's cache-side lookup-ref balance. Coder confirm the two are disjoint (different refs, different
  sites) so there is no double-count and each nets to zero independently.
INTERPRETATION (gate comment): decision and transfer are separate and both accounted; yes-without-transfer
  needs cleanup/error accounting; lookup_ref_balance must remain zero.
```

## 5. D-SUM-14 — SCENE_RESET (summary-reader, lowest risk)

```text
FIELDS (gate comment, verbatim): scene_change_detections, source_copy_reset_frames,
  scene_change_checkpoint_promotions, scene_change_checkpoint_store_successes,
  scene_change_checkpoint_store_duplicate_skips, scene_change_checkpoint_store_errors,
  scene_change_checkpoint_promotion_mismatches, cut_near_grid_checkpoint_count, scene_chroma_enabled,
  scene_threshold_used.
SOURCE: the data ALREADY EXISTS in process_summary (arAllFramesReady:99-103+): scene_change_detection_used,
  scene_chroma_used, scene_change_threshold, scene_change_diff_total, scene_change_samples_examined,
  scene_change_detected, scene_change_reset_output_used, recursive_chroma_blend_used. D-SUM-14 READS these
  at the process-summary-known point (observe_scene_outcome(process_summary)) — like D-SUM-08 reads the
  store summary. LOWEST RISK: no new lifecycle tracking, just surfacing an existing summary.
  The checkpoint-promotion fields correlate scene-reset -> checkpoint store: a scene-change-eligible reset
  should drive a checkpoint promotion; promotion_mismatches counts eligible-reset-WITHOUT-promotion (the
  gate comment's "serious issue"). Coder confirm the reset->promotion linkage is observable from the
  existing summary + the store outcome (may need reading both process_summary and the store summary at a
  common point).
INTERPRETATION (gate comment): scene-change detection is pixel-layer observation; source-copy reset is
  algorithmic; checkpoint promotion is cache/store consequence; eligible reset without required promotion
  is a serious issue.
```

## 6. Cross-cutting (all four)

```text
- OBSERVE-ONLY (R-PROCESS-19): each family's COMPUTE macro off => struct/hooks/writer compile OUT;
  behaviour byte-identical; independent gating. No hook alters any return, status, frame, or control flow.
  NO bail-path (setFilterError) writes — that is DIAG.3c's territory, explicitly out of DIAG.3b.
- ADDITIVE (R-PROCESS-21): hooks at existing sites; no restructure. If clean hosting needs a control-flow
  change, STOP and propose (the DIAG.2b A2 / DIAG.3a precedent).
- MULTI-EXIT exactly-once (DIAG.2b §3.2 / DIAG.3a): the retrieve/release/store/transfer sites have several
  outcome-known exits — observe at each exactly-once (flag pattern), not a single end hook.
- BALANCE COMPLETENESS: 06/07/09 each net to zero; the S-series zero-balance-under-churn is the acceptance
  backstop (as D-SUM-04/D-SUM-12). A non-zero balance with no real leak = a missed acquire/release/transfer
  site -> STOP and report.
- [DSUM-SUMMARY] tag; flush-per-line default + explicit flush at writer end; no stderr inside any lock.
- FILES: cnr3_diagnostics.{h,cpp}, cnr3_plugin_internal.h, cnr3_arInitial.cpp, cnr3_arAllFramesReady.cpp,
  vapoursynth-Cnr3.cpp, cnr3_cache_core_selftest_main.cpp. NO cache-core, NO build_config change.
```

## 7. Proof gate

```text
1. Four-way all-on: 56/56 / 56/56 / 55/56 exit 1 / 56/56; D-SUM-06/07/09/14 [DSUM-SUMMARY] blocks emit.
2. R-PROCESS-19 matrix (five configs): all on; 06 off; 07 off; 09 off; 14 off; all four off. (SIX configs
   if the coordinator wants each of the four isolated + all-off + all-on; designer accepts a 6-config matrix
   here given four families. Each: clean build, four-way identical, family block absent when off. Temporary
   build_config edits not committed.)
3. S-series -r 1 (S1/S3/S7/S8) — the balance acceptance:
   - D-SUM-06 source_frame_release_balance == 0 on all four; same_activation_request_violations == 0.
   - D-SUM-07 temporary_output_balance == 0 on all four.
   - D-SUM-09 lookup_ref_balance == 0 on all four; decision yes+no == decisions_checked.
   - D-SUM-14 scene fields consistent with the run (S7/S8 jumps => scene-change detections at segment
     boundaries; promotion_mismatches == 0).
   - prior families (01/03/04/05/08/10/11/12/13) unchanged.
4. Read alongside D-SUM-12/D-SUM-10 as before (no new correlation required for 3b).
```

## 8. CODER BRIEF — investigate + confirm BEFORE patching (DIAG.2b/3a discipline)

```text
1. Re-validate baseline is post-DIAG.3a (observe_recovery_plan_published, compute_count_map_saturated
   present) and the 06/07/09/14 gates exist, families greenfield.
2. D-SUM-06: confirm EACH getFrameFilter/freeFrame site is source-side vs output/cache/return-side (the scope
   lists candidates — verify the split; a miscount here breaks the balance). Confirm the same-activation
   request<->retrieve linkage is observable (VS-LIFECYCLE-01). Enumerate any source request/retrieve/release
   site NOT in the scope list.
3. D-SUM-07: enumerate the exact temp-output CREATE sites (compute-completion points) and confirm the
   create/store/release/transfer set balances (created == stored + released + transferred). This is the
   hardest balance — confirm no create path is unaccounted.
4. D-SUM-09: confirm the return-side lookup_ref is DISJOINT from D-SUM-04's cache-side lookup-ref (no double
   count); propose the return_no_reason_split enum from the allows_return logic.
5. D-SUM-14: confirm the scene fields are all readable from process_summary (+ store summary for the
   promotion linkage); confirm reset->promotion mismatch is observable.
6. Confirm all four compile out cleanly with their COMPUTE macros off (R-PROCESS-19), and the matrix config
   count (5 vs 6) you will run.
7. Report findings + confirmations, then generate the DIAG.3b patch against the post-DIAG.3a baseline.
   NO plan-trace / bail-path work (that is DIAG.3c).
```
