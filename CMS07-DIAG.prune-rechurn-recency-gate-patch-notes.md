# CMS07-DIAG prune-rechurn recency-gate patch notes

Patch: `CMS07-DIAG.prune-rechurn-recency-gate.patch`

## Scope implemented

Source patch only. Changed files:

- `src/cnr3_cache_core.cpp`
- `src/cnr3_cache_core.h`
- `src/cnr3_cache_diagnostics.h`
- `src/cnr3_cache_diagnostics.cpp`
- `src/cnr3_cache_core_selftest_main.cpp`

The S9e `.vpy` harness file was not present in the provided source bundle, so this patch does not modify it. Add the S9e scenario manually to the diagnostic `.vpy` or provide that file for a follow-up harness patch.

## Cold grep result before patch

Old counter references: exactly 4, matching the scope:

- `src/cnr3_cache_diagnostics.h`: field declaration
- `src/cnr3_cache_core.cpp`: increment site
- `src/cnr3_cache_diagnostics.cpp`: D-SUM-10 emission row
- `src/cnr3_cache_core_selftest_main.cpp`: fixture assignment

No pre-existing `frames_recently_evicted_then_re_requested` references were found.

Existing `eviction_gap` computation was in `src/cnr3_cache_core.cpp::observe_lookup_miss_rechurn_locked`, after the top-thrasher block and before histogram binning.

## Logic change

In `Cnr3OutputCacheCore::observe_lookup_miss_rechurn_locked(int frame_number)`:

- Leaves the full ring scan unchanged.
- Leaves newest-eviction selection unchanged.
- Hoists the existing normalized `eviction_gap` computation immediately after `if (!found) return;`.
- Reuses that one normalized value for both the recency counter and the existing histogram.
- Gates the renamed counter increment on:

```cpp
if (eviction_gap <= CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP) {
    cnr3_cache_diag_saturating_increment(
        prune_diag_stats_.frames_recently_evicted_then_re_requested
    );
}
```

The top-thrashers block, repeated-re-request accounting, and gap histogram binning are otherwise unchanged.

## Constant

Adds to `src/cnr3_cache_core.h` next to the hot-zone/back-radius constants:

```cpp
inline constexpr std::uint64_t CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP =
    static_cast<std::uint64_t>(CNR3_CACHE_HOT_ZONE_BACK_RADIUS) + 2U;
```

This profiles automatically:

- TINY-100: 15 + 2 = 17
- NORMAL: 50 + 2 = 52

## Rename

Renames the diagnostic field and emitted row label:

- old: `frames_evicted_then_re_requested`
- new: `frames_recently_evicted_then_re_requested`

The old field is deleted, not kept alongside.

D-SUM row width check: the new label length is 41 characters; existing `%-44s` formatting fits it without widening.

## Validation performed in sandbox

- `git apply --check`: PASS
- `git apply --check --whitespace=error`: PASS
- `git diff --check`: PASS
- Post-patch grep: old counter name absent from `src/`
- Post-patch grep: new counter name appears at exactly 4 source sites
- Syntax-only checks with fake VapourSynth API stubs:
  - `src/cnr3_cache_diagnostics.cpp`: PASS
  - `src/cnr3_cache_core.cpp`: PASS
  - `src/cnr3_cache_core_selftest_main.cpp`: PASS

## Required local proof still to run

Build/test:

- Debug|x64 build
- Release|x64 build
- Debug normal selftest
- Release normal selftest
- Release forced-fail harness
- Release verbose selftest

Proof configuration:

- TINY-100
- all D-SUM families on
- plantrace on
- `-r 1`

Proof matrix:

- S9c: `frames_recently_evicted_then_re_requested == 0`
- S9d: `frames_recently_evicted_then_re_requested == 0`
- S9/S9b: sharply reduced from old raw 481/565; any remaining value explainable by near eviction-gap bins and <= old-style total
- S9e: `frames_recently_evicted_then_re_requested > 0`

Histogram cross-check is qualitative/plausibility only: the counter should be same order as, and no greater than, the sum of near bins covering `Z`; exact equality is not required because printed bins are coarser than `Z`.

Final commit state: TINY-100, all-families, and plantrace are proof configuration only. Commit `cnr3_build_config.h` at normal project baseline unless the designer explicitly directs otherwise.
