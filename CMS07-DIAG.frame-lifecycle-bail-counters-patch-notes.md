# CMS07-DIAG.frame-lifecycle-bail-counters patch notes

## Baseline

- Patch base: `src(26).zip`.
- Verified source marker before patch: `CMS07-DIAG.lookup-site-breakdown`.
- New marker: `CMS07-DIAG.frame-lifecycle-bail-counters`.
- Scope: observe-only D-SUM-04 diagnostic counters. No cache, lookup, pin, store, recovery, ownership, pixel-path, or return semantics are changed.

## What this patch adds

Adds a D-SUM-04 frame-lifecycle summary with independently counted lifecycle events. Every event prints `total` plus five mutually exclusive origins:

- `frame0_fresh_start`
- `floor_fresh_start`
- `ordinary_target`
- `recovery_hole`
- `recovery_target`

Events added:

- `bailed_before_compute_since_already_in_cache`
- `frames_computed`
- `bailed_after_compute_because_another_activation_stored_it_first`
- `computed_but_returned_after_duplicate_store`
- `frames_computed_and_stored`

The emitted D-SUM-04 block includes plain-English legend/purpose text and print-only self-checks. No self-check affects `Cnr3Status`, selftest pass/fail, cache state, or frame output.

## Expected modified files

```text
 M src/cnr3_arAllFramesReady.cpp
 M src/cnr3_build_config.h
 M src/cnr3_cache_core.cpp
 M src/cnr3_cache_core.h
 M src/cnr3_cache_diagnostics.cpp
 M src/cnr3_cache_diagnostics.h
 M src/vapoursynth-Cnr3.cpp
```

## Route map / anomaly sweep

### Event a: bailed_before_compute_since_already_in_cache

Live counted routes only:

- `floor_fresh_start`: `src/cnr3_arAllFramesReady.cpp:1796-1800`, floor adopt found branch.
- `recovery_hole`: `src/cnr3_arAllFramesReady.cpp:2093-2097`, hole adopt found branch.

No frame0, ordinary-target, or recovery-target before-compute adopt route was found. Those buckets are intentionally printed as structural-zero proof.

### Event b: frames_computed

Live counted routes only:

- `ordinary_target`: `src/cnr3_arAllFramesReady.cpp:1499-1503`, after ordinary target pixel-processing success and before store.
- `floor_fresh_start`: `src/cnr3_arAllFramesReady.cpp:1956-1960`, after floor fresh-start copy/adoption succeeds and before AS2 store.
- `recovery_hole`: `src/cnr3_arAllFramesReady.cpp:2290-2294`, after recovery-hole pixel-processing success and before AS2 store.
- `recovery_target`: `src/cnr3_arAllFramesReady.cpp:2607-2611`, after recovery-target pixel-processing success and before authoritative store.
- `frame0_fresh_start`: `src/cnr3_arAllFramesReady.cpp:2976-2980`, after frame0 fresh-start copy/adoption succeeds and before production store.

This follows the designer's definition of computed: the activation produced the output frame itself, either by full pixel processing or by fresh-start copy. Adopted frames are not counted as computed.

### Event e: bailed_after_compute_because_another_activation_stored_it_first

Live counted routes only:

- `ordinary_target` / `recovery_target`: `src/cnr3_arAllFramesReady.cpp:1089-1094`, authoritative-return helper duplicate branch. The origin is passed by the two live callers:
  - ordinary target: `src/cnr3_arAllFramesReady.cpp:1550-1555`
  - recovery target: `src/cnr3_arAllFramesReady.cpp:2629-2634`
- `floor_fresh_start`: `src/cnr3_arAllFramesReady.cpp:2004-2009`, AS2 floor `duplicate_existing_slot` outcome.
- `recovery_hole`: `src/cnr3_arAllFramesReady.cpp:2385-2390`, AS2 hole `duplicate_existing_slot` outcome.

Important precision point: AS2 `store_status == ok` is not enough to count a stored winner. If `duplicate_existing_slot` is true, the event is `e`, not `f`.

### Event x: computed_but_returned_after_duplicate_store

Live counted route only:

- `frame0_fresh_start`: `src/cnr3_arAllFramesReady.cpp:3033-3038`, frame0 direct production store returned after duplicate store acceptance.

The other four origin buckets are printed as structural-zero proof.

### Event f: frames_computed_and_stored

Live counted routes only:

- `ordinary_target` / `recovery_target`: `src/cnr3_arAllFramesReady.cpp:1025-1030`, authoritative-return helper `ok` branch. The origin is passed by the two live callers:
  - ordinary target: `src/cnr3_arAllFramesReady.cpp:1550-1555`
  - recovery target: `src/cnr3_arAllFramesReady.cpp:2629-2634`
- `floor_fresh_start`: `src/cnr3_arAllFramesReady.cpp:2010-2014`, AS2 floor non-duplicate stored outcome.
- `recovery_hole`: `src/cnr3_arAllFramesReady.cpp:2391-2395`, AS2 hole non-duplicate stored outcome.
- `frame0_fresh_start`: `src/cnr3_arAllFramesReady.cpp:3039-3043`, frame0 direct production store `ok` outcome.

## Caller-map / selftest protection

- The authoritative-return helper gets a defaulted `Cnr3FrameLifecycleOrigin lifecycle_origin = Cnr3FrameLifecycleOrigin::unspecified` at `src/cnr3_arAllFramesReady.cpp:969`.
- Only the two live authoritative-return callers pass explicit lifecycle origins:
  - ordinary target passes `ordinary_target`.
  - recovery target passes `recovery_target`.
- `unspecified` is a silent default and the observer returns without incrementing any lifecycle bucket.
- Cache-core selftest routes do not call the new public lifecycle observer methods and do not pass lifecycle origins into live-only helpers.

## Gating proof

- `Cnr3FrameLifecycleOrigin` is an ungated default-parameter enum, matching the approved Ruling-5 pattern for inert parameter surface.
- All new diagnostic work is under `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE` or the existing D-SUM-04 print gate:
  - per-event counter structs and fields;
  - observer helpers;
  - public lock-owning observer methods;
  - all live bump call sites;
  - lifecycle emission and self-checks.
- D-SUM-04 gate-off syntax validation was run in a temporary copy by commenting out `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`; touched translation units still parsed successfully with fake VapourSynth headers.

## Self-checks emitted

The D-SUM-04 lifecycle block verifies these using independently counted fields:

- each event total equals the sum of its five origin buckets;
- `frames_computed == frames_computed_and_stored + bailed_after_compute + computed_but_returned_after_duplicate_store`, total and per-origin;
- `bailed_before_compute` ties to site7a/site7b lookup-site hits;
- `ordinary_target + recovery_target` duplicate discards tie to D-SUM-07 `duplicate_computed_but_discarded` when D-SUM-07 compute is enabled and a pointer is supplied;
- stored production origins tie to D-SUM-08 production stores and stored AS2 origins tie to D-SUM-08 AS2 stores when D-SUM-08 compute is enabled and a pointer is supplied;
- `frames_computed <= D-SUM-07 temporary_outputs_created` is printed as an expectation, not a mismatch identity. Equality is expected on clean runs.

## R-PROCESS-25 discipline

- Proven live compute/store functions received only diagnostic counting statements and one defaulted diagnostic parameter on the authoritative-return helper.
- No control-flow changes, no early-return changes, no reordering of existing decisions, no loop-bound changes, and no existing counter row renames.
- Existing D-SUM-07, D-SUM-08, D-SUM-12, merged lookup counters, and lookup-site breakdown counters are not semantically changed.

## Sandbox validation

Patch validation from a fresh copy of `src(26).zip`:

```text
git apply --check                                  PASS
git apply --check --whitespace=error               PASS
git apply                                          PASS
git diff --check                                   PASS
```

Limited syntax validation with fake VapourSynth headers:

```text
D-SUM-04 gate ON:
  cnr3_cache_core.cpp            PASS
  cnr3_cache_diagnostics.cpp     PASS
  cnr3_arAllFramesReady.cpp      PASS
  vapoursynth-Cnr3.cpp           PASS

D-SUM-04 gate OFF in a temporary copy:
  cnr3_cache_core.cpp            PASS
  cnr3_cache_diagnostics.cpp     PASS
  cnr3_arAllFramesReady.cpp      PASS
  vapoursynth-Cnr3.cpp           PASS
```

Not run here: VS2026 builds, real plugin load, canonical 4-way selftest, R-PROCESS-19 S8 byte-identical proof, L1/L2 runtime oracles.

## Expected proof gate after coordinator applies

- VS2026 builds:
  - `cnr3 Debug|x64`
  - `cnr3 Release|x64`
  - `cnr3_cache_core_selftest Debug|x64`
  - `cnr3_cache_core_selftest Release|x64`
- Canonical 4-way remains 56/56, with forced-fail 55/56 and exit 1.
- D-SUM-04 selftest path should print the new lifecycle block with all-zero lifecycle counters unless live routes are exercised by the run.
- Designer L1/L2 oracle proof should verify the five-origin values from the ruling document.
- R-PROCESS-19 macro-off S8 A/B remains byte-identical.
