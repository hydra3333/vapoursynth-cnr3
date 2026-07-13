# CMS07-FEATURE.half-cache-profile-and-retry-rename - patch notes

## Scope

Implements the v4 HALF-500 cache profile + PlanRetry gate rename scope against the committed `CMS07-EXPERIMENT.plan-retry-bias` source baseline.

## Adds / changes

- Adds `CNR3_CACHE_PROFILE_HALF` in `cnr3_build_config.h`, mutually exclusive with `CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY`.
- Converts cache policy primitive profile knobs in `cnr3_cache_core.h` from TINY/NORMAL to TINY/HALF/NORMAL.
- Implements HALF Option 1 only:
  - `CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 500U`
  - `CNR3_CACHE_MAX_HOT_ZONES = 3U`
  - all other HALF primitive knobs copied from NORMAL as separate explicit values.
- Leaves derived cache constants derived from the primitives.
- Adds HALF expected-value branch to `cnr3_cache_core_selftest_cache_policy_constants()`:
  - profile name `half-500`
  - active ceiling max `500U`
  - max hot zones `3U`
  - max protected set `228U`
  - checkpoint grid floor `15U`
- Adjusts two selftest local observation arrays from profile-sized fixed arrays to unsized arrays, because HALF has fewer hot zones than NORMAL while those tests intentionally consume only the active `CNR3_CACHE_MAX_HOT_ZONES` count.
- Renames the PlanRetry enable gate:
  - from `CNR3_EXPERIMENT_PLAN_RETRY_BIAS`
  - to `CNR3_ENABLE_PLAN_RETRY_BIAS`
- Sets `CNR3_PLAN_RETRY_SLEEP_MS` to `50`.
- Updates the edit marker to `CMS07-FEATURE.half-cache-profile-and-retry-rename`.

## Changed files

- `src/cnr3_arInitial.cpp`
- `src/cnr3_build_config.h`
- `src/cnr3_cache_core.h`
- `src/cnr3_cache_core_selftest.cpp`
- `src/cnr3_plugin_internal.h`
- `src/vapoursynth-Cnr3.cpp`

## Sandbox validation performed

Patch mechanics:

```text
git apply --check CMS07-FEATURE.half-cache-profile-and-retry-rename.patch: PASS
git apply --check --whitespace=error CMS07-FEATURE.half-cache-profile-and-retry-rename.patch: PASS
git diff --check after apply: PASS
```

Rename grep, source tree only:

```text
CNR3_EXPERIMENT_PLAN_RETRY_BIAS: 0 occurrences
CNR3_ENABLE_PLAN_RETRY_BIAS: 15 occurrences
```

Static/syntax validation with GCC C++20, using the uploaded source and VapourSynth headers:

```text
-fsyntax-only src/cnr3_cache_core_selftest.cpp: PASS under default, HALF, TINY, PlanRetry-on, HALF+PlanRetry
-fsyntax-only src/cnr3_arInitial.cpp: PASS under default, HALF, TINY, PlanRetry-on, HALF+PlanRetry
-fsyntax-only src/vapoursynth-Cnr3.cpp: PASS under default, HALF, TINY, PlanRetry-on, HALF+PlanRetry
-fsyntax-only src/cnr3_cache_core.cpp: PASS under default, HALF, TINY, PlanRetry-on, HALF+PlanRetry
-fsyntax-only src/cnr3_arAllFramesReady.cpp: PASS under default, HALF, TINY, PlanRetry-on, HALF+PlanRetry
```

Profile static-assert spot checks:

```text
DEFAULT include of cnr3_cache_core.h: PASS
HALF include of cnr3_cache_core.h: PASS
TINY include of cnr3_cache_core.h: PASS
HALF derived checks: max_protected=228, grid_floor=15, active ceiling max=500, max hot zones=3
TINY derived checks: max_protected=48, grid_floor=10, active ceiling max=100, max hot zones=2
```

Mutual exclusion guard:

```text
CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY + CNR3_CACHE_PROFILE_HALF together: expected compile-time #error observed
```

## Validation not performed here

Windows/Visual Studio build and `cnr3_cache_core_selftest.exe` execution were not performed in this Linux sandbox. Please run the normal VS2026 proof gate locally.

## Expected local proof gate

1. Default build, no HALF, no TINY, PlanRetry off: canonical four-way selftest remains expected count.
2. R-PROCESS-19 byte-identical default proof against prior commit, marker aside.
3. HALF build, PlanRetry off: canonical four-way selftest passes and policy constants report HALF expected values.
4. CR4/static-assert report: Option 1, `MAX_HOT_ZONES=3`, `max_protected=228`, all static asserts pass.
5. Rename grep-all clean over source tree/build files.
6. NORMAL + `CNR3_ENABLE_PLAN_RETRY_BIAS` smoke.
7. Preferred: HALF + `CNR3_ENABLE_PLAN_RETRY_BIAS` composition smoke.
8. TINY scaffold smoke.

## Apply sequence

From repository root on `dev_cache_manager`:

```bat
git status --short
git apply --check CMS07-FEATURE.half-cache-profile-and-retry-rename.patch
git apply CMS07-FEATURE.half-cache-profile-and-retry-rename.patch
git diff --check
git status --short
```

## Suggested commit title

```text
CMS07-FEATURE: add HALF-500 cache profile and rename PlanRetry gate
```
