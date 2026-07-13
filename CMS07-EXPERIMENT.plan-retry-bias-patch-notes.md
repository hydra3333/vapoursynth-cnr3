# CMS07-EXPERIMENT.plan-retry-bias — patch delivery notes

## Baseline

Source snapshot used for patch construction: `src(31).zip`.

Relevant baseline marker before patch:

```cpp
#define CNR3_EDIT_VERSION_LITERAL "CMS07-SCAFFOLD.filter-mode-selector"
```

Patch marker:

```cpp
#define CNR3_EDIT_VERSION_LITERAL "CMS07-EXPERIMENT.plan-retry-bias"
```

The selected filter-mode suffix mechanism remains unchanged, so the runtime marker is still suffixed with `:fmUnordered`, `:fmParallelRequests`, or `:fmParallel`.

## Scope implemented

This patch implements the approved gated plan-retry biasing experiment.

Default committed state remains macro OFF:

```cpp
//#define CNR3_EXPERIMENT_PLAN_RETRY_BIAS 1
```

When the macro is OFF, the experiment counters, stats storage, retry sleep, retry loop, `std::this_thread::sleep_for`, `getCoreInfo()` retry-depth derivation, and `[DSUM-PLANRETRY]` summary block are not compiled.

When the macro is ON, arInitial may retry recovery plan formation when a candidate recovery plan has more than `CNR3_PLAN_RETRY_HOLE_THRESHOLD` holes and another attempt is available. Candidate plans dumped by the experiment are destroyed before sleeping; source requests are issued only for the kept plan.

## Changed files

```text
src/cnr3_build_config.h
src/cnr3_plugin_internal.h
src/cnr3_arInitial.cpp
src/vapoursynth-Cnr3.cpp
```

## Configuration added

Under `CNR3_EXPERIMENT_PLAN_RETRY_BIAS`:

```cpp
#define CNR3_PLAN_RETRY_SLEEP_MS        25
#define CNR3_PLAN_RETRY_HOLE_THRESHOLD  2
#define CNR3_PLAN_RETRY_MAX_CAP         4
```

Compile-time guard checks are included for invalid negative sleep/threshold and cap values less than 1.

## Per-instance retry limit

When enabled, the filter obtains `VSCoreInfo` once at filter creation:

```cpp
VSCoreInfo core_info{};
vsapi->getCoreInfo(core, &core_info);
```

and derives:

```text
plan_retry_max = min(CNR3_PLAN_RETRY_MAX_CAP, max(1, numThreads / 2))
```

using the active API4 `VSCoreInfo::numThreads` field.

## arInitial behaviour under the experiment

The retry probe is in the recovery route only.

For each candidate plan:

1. The existing bounded recovery plan path is used unchanged.
2. Exact-anchor and floor-fresh-start acceptance are checked by the existing acceptance helpers.
3. Floor-fresh-start holes are derived before the retry decision, so the retry threshold sees the real hole count.
4. The candidate plan is counted in `[DSUM-PLANRETRY]`.
5. If `hole_count > CNR3_PLAN_RETRY_HOLE_THRESHOLD` and another attempt remains:
   - the candidate plan is counted as dumped;
   - any candidate anchor pins are discharged;
   - candidate plan state is reset;
   - no source requests are issued for the dumped candidate;
   - no temporary output frame is created;
   - the activation sleeps for `CNR3_PLAN_RETRY_SLEEP_MS`;
   - planning is retried.
6. The last available attempt is always kept.
7. Source requests are issued only after the final kept plan is selected.

The sleep is after pin discharge and candidate reset. No cache lock is held while sleeping.

## `[DSUM-PLANRETRY]` summary

When the experiment macro is ON, `cnr3_free_filter()` emits a separate searchable block:

```text
[DSUM-PLANRETRY]
```

Counters printed:

```text
plan_retry_enabled
plan_retry_sleep_ms
plan_retry_hole_threshold
plan_retry_max_cap
plan_retry_max
plan_retry_plan_attempts_total
plans_dumped_total
retry_sleeps_total
plans_kept_on_attempt_1
plans_kept_on_attempt_2
plans_kept_on_attempt_3plus
dumped_plan_holes_total
kept_plan_holes_total
```

Print-only WARN self-checks:

```text
plan_retry_plan_attempts_total ==
    plans_dumped_total
  + plans_kept_on_attempt_1
  + plans_kept_on_attempt_2
  + plans_kept_on_attempt_3plus

retry_sleeps_total == plans_dumped_total
```

Self-check WARNs never alter output, cache state, return behaviour, or ownership.

## Deferred by scope

The following were intentionally not implemented:

```text
min/max hole counts
hole_delta_from_dump_to_keep
candidate_plans_* buckets
retry_no_sleep_* breakdowns
source_requests_avoided_estimate
```

## Sandbox validation performed

Patch apply checks against a repo-root layout with `src/` paths:

```text
git apply --check                         PASS
git apply --check --whitespace=error      PASS
git apply                                 PASS
git diff --check                          PASS
```

Limited syntax validation with GCC C++20 and the bundled R76 headers:

```text
macro OFF syntax:
  cnr3_arAllFramesReady.cpp               PASS
  cnr3_arInitial.cpp                      PASS
  cnr3_cache_core.cpp                     PASS
  cnr3_cache_core_selftest.cpp            PASS
  cnr3_cache_core_selftest_main.cpp       PASS
  cnr3_cache_diagnostics.cpp              PASS
  cnr3_diagnostics.cpp                    PASS
  cnr3_frame_processing.cpp               PASS
  cnr3_instance_config.cpp                PASS
  cnr3_owned_frame_ref.cpp                PASS
  cnr3_response_tables.cpp                PASS
  vapoursynth-Cnr3.cpp                    PASS

macro ON syntax:
  cnr3_arAllFramesReady.cpp               PASS
  cnr3_arInitial.cpp                      PASS
  cnr3_cache_core.cpp                     PASS
  cnr3_cache_core_selftest.cpp            PASS
  cnr3_cache_core_selftest_main.cpp       PASS
  cnr3_cache_diagnostics.cpp              PASS
  cnr3_diagnostics.cpp                    PASS
  cnr3_frame_processing.cpp               PASS
  cnr3_instance_config.cpp                PASS
  cnr3_owned_frame_ref.cpp                PASS
  cnr3_response_tables.cpp                PASS
  vapoursynth-Cnr3.cpp                    PASS
```

`cnr3_memory_diagnostics.cpp` was not included in the GCC syntax sweep because it includes Windows headers (`windows.h`) that are not available in this Linux sandbox.

## Not run here

The sandbox does not provide Visual Studio 2026 or the Windows VapourSynth runtime. Therefore the following remain coordinator-side proof gates:

```text
Debug|x64 cnr3 build
Release|x64 cnr3 build
Debug|x64 cnr3_cache_core_selftest build/run
Release|x64 cnr3_cache_core_selftest build/run
Release --force-fail-for-harness-proof
Release --verbose
macro-OFF S8 frame-output byte identity
macro-ON fmParallel -r 2 / -r 4 / -r 8 experiment ladder
macro-ON fmUnordered -r 1 no-hole control
macro-ON fmParallelRequests -r 4 no-waste control
```

## Expected coordinator proof matrix

Macro OFF:

```text
canonical 4-way selftest unchanged
S8 frame-output byte-identical to prior commit
edit_version contains CMS07-EXPERIMENT.plan-retry-bias:<mode>
```

Macro ON:

```text
canonical 4-way selftest unchanged
fmUnordered -r 1: no behaviour change where no holes form
fmParallel -r 2 / -r 4 / -r 8: report fps, duplicates, frames_computed, recalculated_frame_count, and [DSUM-PLANRETRY]
fmParallelRequests -r 4: expect near-zero plans_dumped_total
```

Success interpretation remains the approved scope rule: duplicate reduction is not a win if it is only sleep-throttling. A useful result requires dumped plans to re-form as smaller kept plans, visible as kept-plan holes falling relative to dumped-plan holes, without unacceptable fps collapse.
