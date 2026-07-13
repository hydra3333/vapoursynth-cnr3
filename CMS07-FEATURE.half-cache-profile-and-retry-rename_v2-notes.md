# CMS07-FEATURE.half-cache-profile-and-retry-rename_v2 patch notes

## Purpose

Replacement patch for `CMS07-FEATURE.half-cache-profile-and-retry-rename.patch`.

This version keeps the accepted HALF-500 profile / PlanRetry rename work, and fixes the designer-confirmed blocker in the hot-zone capacity tests.

## Baseline

Built against the reconstructed `CMS07-EXPERIMENT.plan-retry-bias` source baseline used for the prior patch generation.

## Scope implemented

### A. HALF-500 cache profile

Adds `CNR3_CACHE_PROFILE_HALF` as a gated profile beside TINY and NORMAL.

HALF profile starts as NORMAL except:

```text
CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 500
CNR3_CACHE_MAX_HOT_ZONES             = 3
```

The zone-count reduction is the CR4-preserving companion change. It keeps the existing protected-set static_assert relationship intact:

```text
max_protected = 3 * (50 + 10) + 48 = 228
CR4 requires 500 >= 2 * 228 = 456
```

The policy-constants selftest HALF branch expects:

```text
profile_name        = "half-500"
active_ceiling_max  = 500U
max_hot_zones       = 3U
max_protected       = 228U
grid_floor          = 15U
```

### B. PlanRetry gate rename

Renames:

```cpp
CNR3_EXPERIMENT_PLAN_RETRY_BIAS
```

to:

```cpp
CNR3_ENABLE_PLAN_RETRY_BIAS
```

The PlanRetry per-knob names are unchanged. The accepted sleep default is set to:

```cpp
CNR3_PLAN_RETRY_SLEEP_MS = 50
```

### C. v2 blocker fix: NORMAL-geometry hot-zone tests skipped under HALF

The rejected v1 patch made the hardcoded observation arrays compile under HALF but did not fix the runtime semantics of the two NORMAL-geometry tests.

v2 changes both tests to follow the existing TINY precedent and skip under HALF:

```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY) || defined(CNR3_CACHE_PROFILE_HALF)
    cnr3_cache_core_selftest_skip_line("hot_zone_capacity_merge_lifecycle");
    return Cnr3Status::ok;
#else
    ... NORMAL geometry test body ...
#endif
```

and:

```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY) || defined(CNR3_CACHE_PROFILE_HALF)
    cnr3_cache_core_selftest_skip_line("hot_zone_dsum11_counter_model");
    return Cnr3Status::ok;
#else
    ... NORMAL geometry D-SUM-11 test body ...
#endif
```

The explicit array sizes are retained in the NORMAL-only bodies:

```cpp
const int observations[CNR3_CACHE_MAX_HOT_ZONES] = { ... };
const int observations[CNR3_CACHE_MAX_HOT_ZONES - 1U] = { ... };
```

This avoids pretending that the NORMAL five-zone hardcoded assertion geometry is meaningful under HALF's three-zone profile.

Future work, if HALF becomes a shipping default: add real HALF-specific three-zone capacity/merge assertion geometry as a separate proof patch.

## Changed files

```text
src/cnr3_arInitial.cpp
src/cnr3_build_config.h
src/cnr3_cache_core.h
src/cnr3_cache_core_selftest.cpp
src/cnr3_plugin_internal.h
src/vapoursynth-Cnr3.cpp
```

## Sandbox validation

```text
git apply --check                                  PASS
git apply --check --whitespace=error              PASS
git diff --check after apply                       PASS
source-tree grep old macro                         0
source-tree grep new macro                         15
```

Linux/GCC syntax smoke checks passed for the touched translation units under:

```text
DEFAULT
CNR3_CACHE_PROFILE_HALF
CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY
CNR3_ENABLE_PLAN_RETRY_BIAS
CNR3_CACHE_PROFILE_HALF + CNR3_ENABLE_PLAN_RETRY_BIAS
```

These are syntax-level checks only. They do not replace the required Windows/VS2026 build and runtime selftest gate.

## Required local gate

Run the v4 eight-item gate locally:

```text
1. DEFAULT build, no HALF, no TINY, PlanRetry OFF: canonical 4-way selftest.
2. R-PROCESS-19 byte-identical S8 default proof vs prior commit.
3. HALF build, PlanRetry OFF: canonical selftest, policy constants prove 500/3/228/15.
4. CR4/static_assert report: Option 1 compiled clean; static_asserts intact.
5. Rename grep-all clean in source tree.
6. NORMAL + CNR3_ENABLE_PLAN_RETRY_BIAS smoke: DSUM-PLANRETRY reports.
7. HALF + CNR3_ENABLE_PLAN_RETRY_BIAS composition smoke.
8. TINY scaffold smoke.
```

## Expected designer-review resolution

This v2 patch directly addresses the rejected v1 blocker:

```text
The NORMAL five-zone hot-zone capacity geometry is not run under HALF.
HALF skips these two geometry-specific tests using the same precedent as TINY.
No HALF-specific assertion geometry is invented in this patch.
```
