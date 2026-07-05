# CMS07-DIAG.3b lifecycle/return/scene patch notes

## Scope

Implements the DIAG.3b observe-only batch against the post-DIAG.3a baseline:

- D-SUM-06 source-frame lifecycle balance
- D-SUM-07 temporary-output lifecycle balance
- D-SUM-09 return-transfer balance and decision split
- D-SUM-14 scene-reset summary reader

The patch follows designer decisions C1-C4 and C-ALIAS. It does not add or change build gates, project files, cache-core files, or build configuration.

## Files changed

- `src/cnr3_arAllFramesReady.cpp`
- `src/cnr3_arInitial.cpp`
- `src/cnr3_cache_core_selftest_main.cpp`
- `src/cnr3_diagnostics.cpp`
- `src/cnr3_diagnostics.h`
- `src/cnr3_plugin_internal.h`
- `src/vapoursynth-Cnr3.cpp`

## What this patch adds

### D-SUM-06 SOURCE_FRAME_LIFECYCLE

- Adds per-instance source-frame lifecycle stats, snapshot, observer helpers, and summary writer.
- Counts source requests in `arInitial` at all existing request sites.
- Counts all source-side `getFrameFilter` retrieve classes in `arAllFramesReady`.
- Counts source-side releases, including NULL-copy and alias-copy branches per C-ALIAS.
- Reports:
  - `source_frames_requested_total`
  - `source_frames_retrieved_total`
  - `source_frames_released_total`
  - `source_frame_release_balance`
  - `same_activation_request_violations`
  - `source_frame_count_max`
  - `partial_acquire_failures`
  - `source_frame_release_balance_errors`

### D-SUM-07 TEMP_OUTPUT_LIFECYCLE

- Adds per-instance temporary-output lifecycle stats, snapshot, observer helpers, and summary writer.
- Counts `temporary_outputs_created` only for successful non-null, non-alias `copyFrame()` outputs.
- Counts AS2 floor/hole ownership adoption as `temporary_outputs_stored`.
- Keeps production-path `addFrameRef(output_frame)` cache copies outside this balance per C1.
- Counts original temporary outputs returned to VapourSynth as `temporary_outputs_transferred`.
- Counts temporary-output frees as `temporary_outputs_released`.
- Counts duplicate first-in-best-dressed losers in both `temporary_outputs_released` and `duplicate_computed_but_discarded`.
- Reports balance as `created - (stored + released + transferred)`.

### D-SUM-09 RETURN_TRANSFER

- Adds per-instance return-transfer stats, snapshot, observer helpers, and summary writer.
- Uses additive outcome-known hooks only; no return-decision refactor.
- Tracks return decisions, decision yes/no, return-transfer attempts/successes, return-side lookup-ref transfer/release balance, and output-authoritative returns.
- Prints `return_no_reason_split` as visually separated decision-stage and transfer-stage rows:
  - decision-stage: `hard_store_failure`, `store_status_not_returnable`
  - transfer-stage: `duplicate_winner_lookup_failed`, `null_return_frame`, `discard_failed_after_return_ready`
- Return-side lookup refs are only the getFrame return-boundary refs (`returned_cache_ref`, `cached_winner_ref`), separate from D-SUM-04 cache-side accounting.

### D-SUM-14 SCENE_RESET

- Adds per-instance scene-reset stats, snapshot, observer helper, and summary writer.
- Reads existing `Cnr3CallerSuppliedFrameProcessSummary` and store outcome data.
- Counts `source_copy_reset_frames` only for scene-driven resets, not structural frame-0/floor fresh-starts.
- Counts checkpoint store successes, duplicate skips, errors, promotions, and promotion mismatches.
- Implements `cut_near_grid_checkpoint_count` as distance-to-nearest checkpoint grid <= 1:
  `min(n % I, I - (n % I)) <= 1`.
- Writer prints the tiny-profile caveat: with checkpoint interval 3, every frame is near a grid multiple.

## What this patch deliberately does not do

- No `cnr3_build_config.h` changes.
- No `.vcxproj` changes.
- No cache-core changes.
- No DIAG.3c plan/result plan-trace work.
- No bail-path `setFilterError` diagnostic writes.
- No control-flow refactor of `cnr3_live_store_status_allows_return()` or authoritative-return routing.

## Sandbox validation performed

From a clean post-DIAG.3a sandbox baseline:

- `git apply --check CMS07-DIAG.3b-lifecycle-return-scene.patch`: PASS
- `git apply --check --whitespace=error CMS07-DIAG.3b-lifecycle-return-scene.patch`: PASS
- `git apply CMS07-DIAG.3b-lifecycle-return-scene.patch`: PASS
- `git diff --check`: PASS
- C++20 syntax-only validation over touched translation units using local VapourSynth API stubs: PASS

R-PROCESS-19 syntax-only macro-off sanity matrix over touched translation units:

- all DIAG.3b families ON: PASS
- D-SUM-06 compute OFF: PASS
- D-SUM-07 compute OFF: PASS
- D-SUM-09 compute OFF: PASS
- D-SUM-14 compute OFF: PASS
- all four DIAG.3b compute gates OFF: PASS

Not performed in sandbox:

- VS2026 Debug/Release build
- `cnr3_cache_core_selftest.exe` execution
- S-series `.vpy` runs

## Apply sequence

From repo root on the active development branch:

```bat
git status --short
git apply --check CMS07-DIAG.3b-lifecycle-return-scene.patch
git apply CMS07-DIAG.3b-lifecycle-return-scene.patch
git diff --check
git status --short
```

Expected changed files after apply:

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_arInitial.cpp
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_diagnostics.cpp
src/cnr3_diagnostics.h
src/cnr3_plugin_internal.h
src/vapoursynth-Cnr3.cpp
```

## Required local proof gate

### Four-way all-on

Build Debug x64 and Release x64, then run:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"

x64\Debug\cnr3_cache_core_selftest.exe 1>NUL
echo DIAG3b Debug normal exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe 1>NUL
echo DIAG3b Release normal exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL
echo DIAG3b Release forced-fail exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL
echo DIAG3b Release verbose exit_code=%ERRORLEVEL%
```

Expected pattern:

```text
DIAG3b Debug normal:          56/56 PASS, exit_code=0
DIAG3b Release normal:        56/56 PASS, exit_code=0
DIAG3b Release forced-fail:   55/56 FAIL, exit_code=1
DIAG3b Release verbose:       56/56 PASS, exit_code=0
```

Expected summary blocks: D-SUM-06, D-SUM-07, D-SUM-09, and D-SUM-14 emit.

### R-PROCESS-19 six-config matrix

Run six configs:

1. all four DIAG.3b families ON
2. D-SUM-06 OFF
3. D-SUM-07 OFF
4. D-SUM-09 OFF
5. D-SUM-14 OFF
6. all four DIAG.3b families OFF

Each config requires clean Debug/Release build and the same four-way command block. Temporary `cnr3_build_config.h` macro-off edits are not committed.

### S-series -r 1

Run S1/S3/S7/S8 and verify:

- D-SUM-06 `source_frame_release_balance == 0`
- D-SUM-06 `same_activation_request_violations == 0`
- D-SUM-06 `partial_acquire_failures == 0`
- D-SUM-07 `temporary_output_balance == 0`
- D-SUM-09 `lookup_ref_balance == 0`
- D-SUM-09 `return_decision_yes + return_decision_no == return_decisions_checked`
- D-SUM-14 `scene_change_checkpoint_promotion_mismatches == 0`
- prior families 01/03/04/05/08/10/11/12/13 unchanged

A non-zero balance with no real leak means a missed observe site; stop and review before committing.
