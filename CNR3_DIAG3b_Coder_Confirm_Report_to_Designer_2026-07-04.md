# CNR3 DIAG.3b coder confirmation report to designer

Date: 2026-07-04
Role: coder confirmation before patch generation
Scope: DIAG.3b v1, D-SUM-06 / D-SUM-07 / D-SUM-09 / D-SUM-14
Baseline inspected: reconstructed post-DIAG.3a source from src(17).zip plus CMS07-DIAG.3a-recovery-rate-recalc-v2.patch. Coordinator reports DIAG.3a v2 committed/pushed with build_config marker update; marker difference is not material to these findings.

## Executive status

Do not generate the DIAG.3b patch yet without designer confirmation on the points below.

The baseline and gates are confirmed, and the DIAG.3b architecture is implementable using the DIAG.3a pattern. However, the current post-DIAG.3a source has more live source-frame retrieve/release sites than the DIAG.3b scope's candidate list names, and D-SUM-07's balance equation needs an explicit ownership interpretation around `addFrameRef()` cache-store references versus the original computed `copyFrame()` reference returned to the caller.

## 1. Baseline and gates

Confirmed:

- Post-DIAG.3a indicators are present: `cnr3_diag_dsum12_observe_recovery_plan_published()` and `compute_count_map_saturated` exist.
- D-SUM-06 / D-SUM-07 / D-SUM-09 / D-SUM-14 compute/print gates exist in `cnr3_build_config.h` with the expected paired print-without-compute `#error` pattern.
- No `cnr3_build_config.h` patch is required for DIAG.3b.
- No cache-core or project-file change appears required from the confirm pass.

## 2. D-SUM-06 source-frame lifecycle confirmation

### 2.1 Source request sites

Confirmed source-side `requestFrameFilter()` sites in `cnr3_arInitial.cpp`:

1. Cache-hit activation trigger: `requestFrameFilter(n, data.source, frame_ctx)` in `cnr3_publish_live_cache_hit_return()`.
2. Predecessor-present compute: `requestFrameFilter(n, data.source, frame_ctx)` in `cnr3_publish_live_predecessor_present_compute_from_pinned_predecessor()`.
3. Frame-0 fresh start: `requestFrameFilter(n, data.source, frame_ctx)` in `cnr3_publish_live_frame0_fresh_start()`.
4. Recovery branch: loop over `request_data->source_request_frame_numbers`, each `requestFrameFilter(source_frame_number, data.source, frame_ctx)`.

No additional requestFrameFilter sites were found.

### 2.2 Source retrieve sites

Confirmed source-side `getFrameFilter(..., data.source, ...)` sites in `cnr3_arAllFramesReady.cpp`:

1. Cache-hit trigger source retrieve: `source_trigger_frame = getFrameFilter(n, data.source, frame_ctx)`.
2. Predecessor-present source retrieve: `source_frame = getFrameFilter(n, data.source, frame_ctx)`.
3. Recovery floor fresh-start source retrieve: `floor_source_frame = getFrameFilter(floor_frame, data.source, frame_ctx)`.
4. Recovery hole source retrieve: `source_frame = getFrameFilter(hole_frame, data.source, frame_ctx)`.
5. Recovery target source retrieve: `target_source_frame = getFrameFilter(n, data.source, frame_ctx)`.
6. Frame-0 source retrieve: `source_frame = getFrameFilter(n, data.source, frame_ctx)`.

No output/cache getFrameFilter sites were found; the return/cache paths use cache lookup/addref APIs, not VSAPI `getFrameFilter()`.

Important: the scope candidate list names only the cache-hit trigger and a single source-side getFrameFilter class. The post-DIAG.3a source now has six source-side retrieval classes. This is not a design conflict, but the patch must count all six retrieve classes for D-SUM-06 to balance under S-series churn.

### 2.3 Source release sites

Confirmed source-frame release sites in `cnr3_arAllFramesReady.cpp`:

Cache-hit trigger:

- `freeFrame(source_trigger_frame)` after trigger retrieval.

Predecessor-present compute:

- `freeFrame(source_frame)` if predecessor lookup/addref fails.
- `freeFrame(source_frame)` if `copyFrame()` fails.
- `freeFrame(output_frame)` on copyFrame alias path (`output_frame == source_frame`), which is logically releasing the source reference because the names alias.
- `freeFrame(source_frame)` after successful processing.

Recovery floor fresh-start:

- `freeFrame(floor_source_frame)` if floor `copyFrame()` fails.
- `freeFrame(floor_output_frame)` on floor copyFrame alias path (`floor_output_frame == floor_source_frame`), logically releasing the source reference.
- `freeFrame(floor_source_frame)` after successful floor copy.

Recovery hole:

- `freeFrame(source_frame)` if hole `copyFrame()` fails.
- `freeFrame(hole_output_frame)` on hole copyFrame alias path (`hole_output_frame == source_frame`), logically releasing the source reference.
- `freeFrame(source_frame)` after successful hole processing.

Recovery target:

- `freeFrame(target_source_frame)` if target `copyFrame()` fails.
- `freeFrame(target_output_frame)` on target copyFrame alias path (`target_output_frame == target_source_frame`), logically releasing the source reference.
- `freeFrame(target_source_frame)` after successful target processing.

Frame-0 fresh-start:

- `freeFrame(source_frame)` if frame-0 `copyFrame()` fails.
- `freeFrame(output_frame)` on frame-0 copyFrame alias path (`output_frame == source_frame`), logically releasing the source reference.
- `freeFrame(source_frame)` after successful frame-0 copy.

Important: alias-path releases must be counted by D-SUM-06 as source releases, even though the local variable freed has an output name, because the branch has already proven pointer equality to the source frame. Do not also count those alias releases as D-SUM-07 temporary-output releases.

### 2.4 Same-activation linkage

Confirmed observable:

- Simple branches have `request_data->source_requested`, `request_data->branch`, and `request_data->requested_frame`, so retrieval of `n` can be checked against the current activation.
- Recovery branch has `request_data->source_request_frame_numbers` and existing helper `cnr3_live_recovery_source_was_requested(*request_data, frame_number)`, already used before floor/hole/target retrievals.

Recommendation: D-SUM-06 should count `same_activation_request_violations` immediately before each source retrieve if the relevant branch request predicate is false. The existing control flow already bails on some of these conditions; the diagnostic hook can observe before the existing bail without changing behaviour.

## 3. D-SUM-07 temporary-output lifecycle confirmation and clarification needed

### 3.1 Create sites

Confirmed `copyFrame()` creation sites in `cnr3_arAllFramesReady.cpp`:

1. Predecessor-present output: `output_frame = copyFrame(source_frame, core)`.
2. Recovery floor fresh-start output: `floor_output_frame = copyFrame(floor_source_frame, core)`.
3. Recovery hole output: `hole_output_frame = copyFrame(source_frame, core)`.
4. Recovery target output: `target_output_frame = copyFrame(target_source_frame, core)`.
5. Frame-0 output: `output_frame = copyFrame(source_frame, core)`.

No `newVideoFrame()` creation site was found in the inspected getFrame path.

Only successful non-null, non-alias copyFrame results should count as `temporary_outputs_created`. Null copyFrame creates no frame. Alias-path returns should not count as temporary output creation; they are source-reference error paths and should be counted only under D-SUM-06 release as described above.

### 3.2 Store/release/transfer ownership interpretation

The current source has two distinct ownership patterns:

1. AS2 recovery floor/hole computed outputs:
   - The original computed `copyFrame()` output is adopted into `Cnr3OwnedFrameRef` via `reset_to_owned_frame()` and moved to the cache through `store_as2_floor_and_prune()` / `store_recovery_hole_and_prune()`.
   - For these paths, `temporary_outputs_stored++` cleanly consumes the created temporary output.

2. Production return paths (predecessor-present, recovery target, frame-0):
   - The original computed `copyFrame()` output remains the return candidate.
   - A separate cache reference is created via `addFrameRef(output_frame)`, adopted into `Cnr3OwnedFrameRef`, and stored in the cache.
   - The original computed output may then be transferred to the caller, or released if it loses a duplicate race.

Clarification required:

The scope says `temporary_output_balance = created - (stored + released + transferred)` and lists the `reset_to_owned_frame` cache-adopt site as STORE. If D-SUM-07 counts the production-path `addFrameRef()` cache copy as `temporary_outputs_stored` while also counting the original computed frame as transferred to the caller, the balance will double-consume a single `copyFrame()` creation and become negative.

Recommended coder interpretation for balance correctness:

- Count `temporary_outputs_stored` only when the original created temporary output reference itself is consumed by cache ownership (AS2 floor/hole paths, after successful `reset_to_owned_frame()` of `floor_output_frame` / `hole_output_frame`).
- Do not count production-path `addFrameRef(output_frame)` cache storage as consuming the original temporary output. That cache add-ref is a distinct reference and is outside the `copyFrame()` temp-output balance.
- Count production-path original temp output as either `temporary_outputs_transferred` when it leaves getFrame as the returned frame, or `temporary_outputs_released` when it is freed on hard failure, process failure, duplicate loser, bad byte-count, etc.
- Count `duplicate_computed_but_discarded` when a computed original output is freed because the store outcome was duplicate and a cached winner is returned instead.

Designer confirmation requested: approve this interpretation, or redefine D-SUM-07 `created` to include the extra cache `addFrameRef()` reference as a separate created/stored temporary-output unit. The latter is possible but should be explicit because it changes the semantic meaning of `temporary_outputs_created` away from only `copyFrame()`/`newVideoFrame()` output production.

## 4. D-SUM-09 return-transfer confirmation and clarification needed

### 4.1 Decision hook availability

The named function `cnr3_live_store_status_allows_return(Cnr3Status status)` currently exists and returns true for `ok` or `duplicate`. In the inspected source it is called only in the frame-0 store path. The predecessor-present and recovery-target paths make equivalent return decisions inside `cnr3_store_live_output_frame_for_authoritative_return()` using explicit `ok` / `duplicate` checks, not by calling `cnr3_live_store_status_allows_return()`.

Therefore, hooking only the named `allows_return` call site will miss most D-SUM-09 return decisions.

Recommended additive approach:

- Keep the existing control flow unchanged.
- Add observe hooks at each outcome-known return-decision point:
  - frame-0 explicit `cnr3_live_store_status_allows_return(store_status)` site;
  - `cnr3_store_live_output_frame_for_authoritative_return()` after `store_summary.store_status` is known;
  - helper outcome cases: hard store failure, `ok`, non-duplicate non-ok, duplicate with cached winner success/failure.

If the designer wants all decisions to flow through `cnr3_live_store_status_allows_return()`, that is a small refactor and should be explicitly authorized under R-PROCESS-21. The additive hook approach avoids refactoring.

### 4.2 Proposed return_no_reason_split

A small reason split can be expressed as rows rather than a new public enum, for example:

- `return_no_reason_hard_store_failure`
- `return_no_reason_store_status_not_returnable`
- `return_no_reason_duplicate_winner_lookup_failed`
- `return_no_reason_null_return_frame`
- `return_no_reason_discard_failed_after_return_ready`

The first two are decision-stage reasons. The latter three are transfer/outcome-stage reasons and can be tracked separately if the designer wants strict separation.

### 4.3 Transfer and lookup-ref boundaries

Confirmed return boundaries:

- Cache-hit return: `returned_cache_ref.transfer_to_caller()` returns the authoritative cached frame.
- Duplicate production return: `cached_winner_ref.transfer_to_caller()` returns the authoritative cached winner.
- Store-ok production return: original computed frame is returned directly as `return_frame` by the caller branch.
- Frame-0 return: original computed frame is returned directly as `output_frame` after store/prune and discard.

Recommended:

- Count `return_transfer_attempted` at each final getFrame return boundary once a non-null return frame has been selected.
- Count `return_transfer_succeeded` immediately before returning that non-null frame to VapourSynth.
- Count `lookup_ref_transferred` only for return-side `Cnr3OwnedFrameRef` objects whose ownership is moved by `transfer_to_caller()` (`returned_cache_ref`, `cached_winner_ref`).
- Count `lookup_ref_released` if such a return-side lookup ref is reset/released instead of transferred.

### 4.4 D-SUM-04 disjointness

D-SUM-09 is disjoint from D-SUM-04 if interpreted as return-boundary accounting only:

- D-SUM-04 remains the broad cache-core lookup-ref balance instrumentation.
- D-SUM-09 observes only whether a lookup ref selected for return was transferred or released at the getFrame return boundary.

The same underlying reference may be visible to both families at different abstraction layers, but each family has its own balance equation and no D-SUM-09 hook should increment D-SUM-04 counters.

## 5. D-SUM-14 scene-reset confirmation

Confirmed readable from `Cnr3CallerSuppliedFrameProcessSummary`:

- `scene_change_detection_used`
- `scene_chroma_used`
- `scene_change_detected`
- `scene_change_reset_output_used`
- `recursive_chroma_blend_used`
- `scene_change_threshold`
- `scene_change_diff_total`
- `scene_change_samples_examined`
- `frame_processed`

Confirmed store-result linkage is observable at common points where process summary and store summary are both in scope:

- Predecessor-present branch: `process_summary`, `store_status`, `store_as_checkpoint`.
- Recovery hole branch: `hole_process_summary`, `hole_store_summary.as2_summary`, `hole_store_as_checkpoint`.
- Recovery target branch: `target_process_summary`, `target_store_status`, `target_store_as_checkpoint`.
- Recovery floor fresh-start is source-copy reset style with floor store summary available, but it does not use a process-summary scene detection pass.
- Frame-0 fresh start is source-copy reset style and forced checkpoint store, with no process-summary scene detection pass.

Recommended D-SUM-14 mapping:

- `scene_change_detections`: count process summaries where `scene_change_detection_used && scene_change_detected`.
- `source_copy_reset_frames`: count `scene_change_reset_output_used` summaries, plus frame-0/floor fresh-start source-copy resets if designer wants algorithmic reset to include non-scene fresh starts. This needs designer semantic confirmation because the gate says scene reset, while frame0/floor are fresh-start/source-copy resets not necessarily scene-change detections.
- `scene_change_checkpoint_promotions`: count scene-change resets whose corresponding store request forced checkpoint storage.
- `scene_change_checkpoint_store_successes`: scene-change reset with store status `ok` or equivalent AS2 inserted/consumed success.
- `scene_change_checkpoint_store_duplicate_skips`: scene-change reset with duplicate/adopted outcome.
- `scene_change_checkpoint_store_errors`: scene-change reset where store hard/status failed.
- `scene_change_checkpoint_promotion_mismatches`: scene-change reset where checkpoint promotion/storage was expected but not observed.
- `cut_near_grid_checkpoint_count`: count scene-change resets near an existing checkpoint grid boundary if using frame number modulo `CNR3_CACHE_CHECKPOINT_INTERVAL`; exact near-grid distance needs designer definition if not already specified elsewhere.
- `scene_chroma_enabled`: a boolean summary of any scene summary with `scene_chroma_used`.
- `scene_threshold_used`: numeric threshold from the summaries; if multiple configs appear in one instance, report first/last/max or mismatch. Current config should be stable per instance.

Designer confirmation requested for two D-SUM-14 semantics:

1. Should `source_copy_reset_frames` include only scene-change-driven source-copy resets, or also frame-0/floor fresh-start source-copy resets?
2. What is the exact definition of `cut_near_grid_checkpoint_count`? Suggested implementation: count scene-change detections where `frame_number % CNR3_CACHE_CHECKPOINT_INTERVAL` is 0 or 1 (or within a designer-approved fixed radius).

## 6. R-PROCESS-19 matrix count

Recommendation: run the 6-config matrix, not 5, because DIAG.3b has four families and the designer explicitly accepts the 6-config form.

Matrix:

1. all ON
2. D-SUM-06 OFF
3. D-SUM-07 OFF
4. D-SUM-09 OFF
5. D-SUM-14 OFF
6. D-SUM-06/07/09/14 all OFF

Each config: clean Debug x64 + Release x64 build, then the same four-way selftest block.

## 7. Patch-generation status

Not patching yet is recommended because:

1. D-SUM-06 source retrieve/release set is materially larger than the scope's candidate list; this is solvable but should be noted.
2. D-SUM-07 needs designer agreement on whether cache `addFrameRef()` storage consumes the same temporary-output balance unit as `copyFrame()` output creation.
3. D-SUM-09 needs designer agreement that additive outcome-known hooks may supplement the single named `allows_return` function hook, because that function is not currently used by all return paths.
4. D-SUM-14 needs minor semantic confirmation for source-copy reset breadth and near-grid definition.

Once those confirmations are received, the DIAG.3b patch should be generated against the post-DIAG.3a baseline using the established files only:

- `src/cnr3_diagnostics.h`
- `src/cnr3_diagnostics.cpp`
- `src/cnr3_plugin_internal.h`
- `src/cnr3_arInitial.cpp`
- `src/cnr3_arAllFramesReady.cpp`
- `src/vapoursynth-Cnr3.cpp`
- `src/cnr3_cache_core_selftest_main.cpp`

No `src/cnr3_build_config.h`, no cache-core files, no project files.
