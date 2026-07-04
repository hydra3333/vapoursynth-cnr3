# CMS07-DIAG.3a recovery-rate/recalculation patch notes

Patch: `CMS07-DIAG.3a-recovery-rate-recalc.patch`

## Scope

Implements DIAG.3a only:

- D-SUM-03 recovery-search summary
- D-SUM-12 recovery-plan / recovery-rate summary
- D-SUM-13 recalculation summary

Does not implement DIAG.3b families 06/07/09/14 and does not implement the future plan/result trace family.

## Designer amendments applied

- B1: `recovery_plans_created` is counted only at the recovery branch publish in `cnr3_start_live_recovery`, not at the three non-recovery `*frame_data = request_data` publish sites. `recovery_plans_destroyed` is counted at `cnr3_discard_frame_data_with_cache()` only when `branch == recovery` and the accepted-plan stats pointer is present.
- B2: holes identified/filled and hole-source request/retrieve counters use `recovery_plan.hole_frame_numbers` only. The target frame, floor base frame, and direct branches are excluded from hole counts.
- B3: D-SUM-13 uses a fixed-capacity open-addressed per-instance table with normal capacity 16000 and tiny-profile capacity 1600. The compact entry is intended to keep the normal per-instance footprint to about 128 KiB. Saturation sets `compute_count_map_saturated`, increments `compute_count_observations_dropped`, and the writer states that counts are lower bounds if saturated.
- B4: recalculation depth uses recovery distance from anchor/floor for recovery outputs; direct cache-hit/frame0/predecessor-present paths use depth 0.
- B5: the in-run DSUM10+DSUM12 ring-correlation counter is deferred. DIAG.3a relies on external/offline correlation between D-SUM-12 recovered-target/span data and D-SUM-10 ring dumps.

## Changed files

- `src/cnr3_arAllFramesReady.cpp`
- `src/cnr3_arInitial.cpp`
- `src/cnr3_cache_core_selftest_main.cpp`
- `src/cnr3_diagnostics.cpp`
- `src/cnr3_diagnostics.h`
- `src/cnr3_plugin_internal.h`
- `src/vapoursynth-Cnr3.cpp`

No `cnr3_build_config.h` change is included. The DIAG.3a scope said the 03/12/13 gates already exist and build_config is to be consumed unchanged. If an edit-version marker advance is desired, handle it as a separate coordinator/designer marker-only decision.

## Sandbox validation performed

From a temporary repo-root-style tree with files under `src/`:

```bat
cd /d <repo-root>

git apply --check CMS07-DIAG.3a-recovery-rate-recalc.patch
git apply --check --whitespace=error CMS07-DIAG.3a-recovery-rate-recalc.patch
git apply CMS07-DIAG.3a-recovery-rate-recalc.patch
git diff --check
```

Result: PASS.

C++20 syntax-only validation using a local minimal VapourSynth stub:

- all gates on: PASS over all `.cpp` files
- D-SUM-03 compute off: PASS over all `.cpp` files
- D-SUM-12 compute off: PASS over all `.cpp` files
- D-SUM-13 compute off: PASS over all `.cpp` files
- D-SUM-03/12/13 all off: PASS over all `.cpp` files

Not performed in sandbox:

- VS2026 Debug/Release build
- cache-core selftest execution
- R-PROCESS-19 runtime four-way matrix
- S-series `.vpy` harness runs

## Expected proof gate after designer diff review

1. Default all-on four-way:
   - Debug normal: 56/56 PASS, exit 0
   - Release normal: 56/56 PASS, exit 0
   - Release forced-fail: 55/56 FAIL, exit 1
   - Release verbose: 56/56 PASS, exit 0
   - D-SUM-03/12/13 blocks emit.

2. R-PROCESS-19 matrix:
   - all on
   - D-SUM-03 off
   - D-SUM-12 off
   - D-SUM-13 off
   - D-SUM-03/12/13 all off

   Each configuration should build cleanly and preserve the four-way result pattern. The disabled family block must be absent.

3. S-series `-r 1`:
   - S1/S3/S7/S8
   - `recovery_plan_balance == 0`
   - S1 recovery rate near 0%
   - S7/S8 provide deterministic exact/floor recovery-rate baseline
   - `compute_count_map_saturated == false`
   - prior D-SUM-04 balances remain zero; D-SUM-05 violations remain zero; D-SUM-08 store failures remain zero.

## Repo-root apply commands

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github"

git status --short
git apply --check "CMS07-DIAG.3a-recovery-rate-recalc.patch"
git apply --check --whitespace=error "CMS07-DIAG.3a-recovery-rate-recalc.patch"
git apply "CMS07-DIAG.3a-recovery-rate-recalc.patch"
git diff --check
git status --short
```
