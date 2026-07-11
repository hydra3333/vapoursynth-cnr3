# CNR3 — Patch Notes: CMS07-DIAG.intent-counted-lookups

## Status

Prepared for designer review. This is a single source patch against baseline `CMS07-DIAG.honest-cache-hit-metrics`, changing the lookup-counter semantics from uniform primitive counting to intent-counted probes.

Marker on success:

```text
CMS07-DIAG.intent-counted-lookups
```

## Scope reconciliation

The designer response v2 was applied as controlling amendment to scope v3:

- `hit_only` means: miss counts nothing; hit counts `query+1` and `hit+1` together on the found path.
- 10a and 10b both receive defaulted duplicate-count policy treatment.
- D-SUM-04 / D-SUM-HEALTH prose is updated from "both entry points" to "intent-counted probes" semantics.
- `cache_lookup_hit_rate_percent` is kept as the hits-percent row.
- Only one new health row is added: `cache_lookup_misses_percent`.
- `cache_lookup_hits_percent` is not added.

## Source baseline check

Uploaded source `src(24).zip` was checked before patching:

```text
src/cnr3_build_config.h: CNR3_EDIT_VERSION = "CMS07-DIAG.honest-cache-hit-metrics"
```

The source also already contained the predecessor patch's D-SUM-04 rows:

```text
cache_lookup_queries_total
cache_lookup_hits
cache_lookup_misses
```

## Changed files

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_arInitial.cpp
src/cnr3_build_config.h
src/cnr3_cache_core.cpp
src/cnr3_cache_core.h
src/cnr3_cache_diagnostics.cpp
src/cnr3_diagnostics.cpp
```

No cache algorithm, lookup result semantics, pin/store/recovery control flow, ownership transfer, or pixel path is intentionally changed.

Diff stat:

```text
src/cnr3_arAllFramesReady.cpp  |   6 +-
src/cnr3_arInitial.cpp         |   9 ++-
src/cnr3_build_config.h        |   2 +-
src/cnr3_cache_core.cpp        | 137 ++++++++++++++++++++++++++++++++++-------
src/cnr3_cache_core.h          |  35 ++++++++---
src/cnr3_cache_diagnostics.cpp |   5 +-
src/cnr3_diagnostics.cpp       |  24 +++++++-
7 files changed, 179 insertions(+), 39 deletions(-)
```

## Implementation summary

### Policy enum and plumbing

Added diagnostic-only policy enum in `src/cnr3_cache_core.h`:

```cpp
enum class Cnr3LookupCountPolicy {
    none,
    full,
    hit_only
};
```

Public and locked lookup primitives now accept a defaulted `Cnr3LookupCountPolicy::none`:

```text
lookup_frame_and_add_ref(..., Cnr3LookupCountPolicy count_policy = none)
lookup_frame_and_record_pin(..., Cnr3LookupCountPolicy count_policy = none)
lookup_frame_and_add_ref_locked(..., Cnr3LookupCountPolicy count_policy = none)
lookup_frame_and_record_pin_locked(..., Cnr3LookupCountPolicy count_policy = none)
pin_frame_locked(..., Cnr3LookupCountPolicy count_policy = none)
```

Store duplicate-detect helpers now accept defaulted duplicate policy:

```text
store_owned_frame_locked(..., Cnr3LookupCountPolicy duplicate_count_policy = none)
store_owned_frame_and_record_pin_locked(..., Cnr3LookupCountPolicy duplicate_count_policy = none)
store_owned_frame_and_record_pin(..., Cnr3LookupCountPolicy duplicate_count_policy = none)
```

### Policy behaviour

Implemented behaviour matches designer Ruling 1:

```text
full miss:      query+1, hit+0  -> derived miss+1
full hit:       query+1, hit+1  -> derived miss+0
hit_only miss:  query+0, hit+0  -> no event
hit_only hit:   query+1, hit+1  -> derived miss+0
none:           query+0, hit+0  -> no event
```

### Counted sites

| site | file:line after patch | policy | notes |
|---|---:|---|---|
| 1 | `src/cnr3_arInitial.cpp:913-917` | `hit_only` | requested frame N; hit means pre-produced / bubbling win; miss suppressed |
| 2 | `src/cnr3_arInitial.cpp:967-971` | `full` | predecessor N-1 fast path; miss routes to recovery |
| 3 | `src/cnr3_cache_core.cpp:3660-3679` | mixed | walk N-1 is hit-only; all other candidates full |
| 8 floor | `src/cnr3_arAllFramesReady.cpp:1762-1766` | `hit_only` | floor adopt; found adopts, miss suppressed |
| 8 hole | `src/cnr3_arAllFramesReady.cpp:2036-2040` | `hit_only` | hole adopt; found adopts, miss suppressed |
| 10a | `src/cnr3_cache_core.cpp:2879-2885` | `hit_only` when opted in | AS2 duplicate-detect found path only |
| 10b | `src/cnr3_cache_core.cpp:2746-2752` | `hit_only` when opted in | plain-store duplicate-detect found path only |

Formerly counted sites 5, 6, 7, 9, and 11 now use default `none` and do not opt in.

### Health row changes

Kept:

```text
cache_lookup_hit_rate_percent = cache_lookup_hits / cache_lookup_queries_total
```

Added:

```text
cache_lookup_misses_percent = cache_lookup_misses / cache_lookup_queries_total
```

The new row uses the same D-SUM-04 compute/print gate and the same disabled-row style. The comment records:

```text
cache_lookup_hit_rate_percent + cache_lookup_misses_percent == 100.000 when live
```

## Required caller-map / anomaly sweep

I re-derived the caller maps from source grep before patching. No unclassified live route was found.

### 10b route map: `store_owned_frame_locked`

| route | source line before patch | classification | final policy |
|---|---:|---|---|
| direct live route from `store_owned_frame_and_prune_impl` | `cnr3_cache_core.cpp:1110` | live plain store | `hit_only` |
| nested call from 10a `store_owned_frame_and_record_pin_locked` | `cnr3_cache_core.cpp:2858` | internal AS2 nested duplicate check | explicit `none` |
| `store_noncheckpoint_owned_frame_locked` wrapper | `cnr3_cache_core.cpp:2683` | selftest-only public-store route | default `none` |
| `store_checkpoint_owned_frame_locked` wrapper | `cnr3_cache_core.cpp:2690` | selftest-only public-store route | default `none` |

### 10a route map: `store_owned_frame_and_record_pin_locked`

| route | source line before patch | classification | final policy |
|---|---:|---|---|
| live AS2 route from `store_owned_frame_and_prune_impl` | `cnr3_cache_core.cpp:1066` | only live route | `hit_only` |
| public wrapper `store_owned_frame_and_record_pin` | `cnr3_cache_core.cpp:790` | selftest-only route | default `none` |
| repair delegate `store_recovery_plan_hole_owned_frame_and_record_pin` through public wrapper | `cnr3_cache_core.cpp:841` | selftest-only route in current source | inherited default `none` |

### Lookup primitive route map

| function | live callers | selftest callers | final policy handling |
|---|---|---|---|
| `lookup_frame_and_add_ref` | arAll sites 6, 7, 9, 11 (`1088`, `1191`, `1350`, `2066`, `2376`) | many | all current callers default `none` |
| `lookup_frame_and_add_ref_locked` | public wrapper only | via public wrapper | inherits public wrapper policy; currently all default `none` |
| `lookup_frame_and_record_pin` | site 1 `hit_only`, site 2 `full`, site 8 floor/hole `hit_only` | many | explicit live opt-ins only; selftest default `none` |
| `lookup_frame_and_record_pin_locked` | public wrapper; anchor pin site 5 | via public wrapper | public wrapper passes policy; anchor pin explicitly `none` |
| `pin_frame_locked` | via `lookup_frame_and_record_pin_locked`; AS2 post-store pin | no direct public access | lookup-pin path inherits policy; AS2 post-store pin explicitly `none` |

### Walk route map

| function | callers | final policy handling |
|---|---|---|
| `plan_bounded_recovery_search_locked` | public `plan_bounded_recovery_search`; locked anchor-wrapper `plan_bounded_recovery_search_and_record_anchor_pin_locked` | counts inside loop: first N-1 probe hit-only, all other candidates full |

The hole-catalogue loop remains untouched and uninstrumented.

### Sweep result

Sweep result: clean after applying the designer rulings. No extra live route, selftest route, wrapper, or delegate was found beyond the maps above.

Patch self-check also explicitly confirmed the AS2 post-store pin remains `none` and does not inherit the duplicate policy; that prevents site 12 / guaranteed-present pin bookkeeping from entering the lookup metrics.

## Selftest assertion check

Search result:

```text
rg "cache_lookup_" cnr3_cache_core_selftest.cpp cnr3_cache_core_selftest.h cnr3_cache_core_selftest_main.cpp
# no matches
```

No selftest fixture asserts on `cache_lookup_*` values. The canonical 4-way count is therefore expected to remain 56/56 unchanged.

## Whole-diff deletion enumeration

The patch removes only the following kinds of lines:

1. Old one-line / shorter call arguments replaced by multi-line calls with explicit policy.
2. The old edit marker string.
3. Old function signatures replaced by defaulted policy signatures.
4. The old unconditional primitive `observe_cache_lookup_query_locked()` / `observe_cache_lookup_hit_locked()` calls, replaced by policy-gated calls in the same functions.
5. Old D-SUM prose that said the lookup metric counted both lookup entry points.
6. The old derived-misses comment tied to uniform `query++ before find` semantics.

No early return, loop-bound expression, branch predicate, store/lookup semantic line, pin ownership line, source-frame lifecycle line, or pixel-processing line is deleted.

Mechanical deletion count from patch:

```text
39 removed lines total
- 4 call-argument lines in arInitial / arAllFramesReady replaced by policy-explicit calls
- 1 edit_version line replaced
- 8 header signature lines replaced by defaulted policy signatures
- 13 cache-core signature / call-forwarding lines replaced by policy-forwarding forms
- 4 old unconditional D-SUM-04 primitive observer lines replaced by policy-gated observer blocks
- 5 diagnostics/prose/comment lines replaced
- remaining removals are call-continuation lines replaced by explicit policy arguments
```

## Validation performed in sandbox

Patch apply validation:

```text
git apply --check CMS07-DIAG.intent-counted-lookups.patch                         PASS
git apply --check --whitespace=error CMS07-DIAG.intent-counted-lookups.patch      PASS
git apply CMS07-DIAG.intent-counted-lookups.patch                                 PASS
git diff --check                                                                  PASS
```

Limited syntax validation with a local fake VapourSynth header:

```text
g++ -std=c++20 -fsyntax-only -I/fake_vs -I. cnr3_cache_core.cpp                   PASS
g++ -std=c++20 -fsyntax-only -I/fake_vs -I. cnr3_cache_diagnostics.cpp cnr3_diagnostics.cpp PASS
```

I did not run VS2026 builds or the canonical 4-way in the sandbox. Those remain coordinator-side.

## Expected coordinator proof gate

After designer review and patch application:

```text
Canonical 4-way (R-PROCESS-26): expected 56/56 unchanged
Release forced-fail: expected 55/56, exit 1, invariant_violation
R-PROCESS-19 macro-off byte-identical: designer/coordinator harness
Designer L1/L2/new-oracle checks: designer harness
```

## Patch application commands

Use from repo root after saving the patch file there:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github"

git status --short
git branch --show-current
git config --get core.autocrlf

git apply --check --ignore-whitespace CMS07-DIAG.intent-counted-lookups.patch
git apply --ignore-whitespace CMS07-DIAG.intent-counted-lookups.patch

git diff --check
git status --short
```

Expected modified files after apply:

```text
 M src/cnr3_arAllFramesReady.cpp
 M src/cnr3_arInitial.cpp
 M src/cnr3_build_config.h
 M src/cnr3_cache_core.cpp
 M src/cnr3_cache_core.h
 M src/cnr3_cache_diagnostics.cpp
 M src/cnr3_diagnostics.cpp
```

## Canonical 4-way commands

After VS2026 builds of `cnr3` and `cnr3_cache_core_selftest` in Debug|x64 and Release|x64:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"
x64\Debug\cnr3_cache_core_selftest.exe 1>NUL
echo Debug normal exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe 1>NUL
echo Release normal exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL
echo Release forced-fail exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL
echo Release verbose exit_code=%ERRORLEVEL%
```

Expected:

```text
Debug normal exit_code=0
Release normal exit_code=0
Release forced-fail exit_code=1
Release verbose exit_code=0
```
