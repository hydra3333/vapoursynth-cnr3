# CMS07-DIAG.honest-cache-hit-metrics — patch notes

## Scope

Implements the designer-approved honest cache-hit metrics adjustment:

- replace the misleading `[DSUM-HEALTH] cache_hit_and_supplied_percent` row with three D-SUM-12 branch rows:
  - `pred_returned_from_cache_percent = frames_pred_present / frames_total`
  - `current_frame_returned_from_cache_percent = frames_cache_hit / frames_total`
  - `cache_hit_percent = (frames_pred_present + frames_cache_hit) / frames_total`
- add true lookup hit-rate instrumentation under D-SUM-04:
  - `cache_lookup_queries_total`
  - `cache_lookup_hits`
  - `cache_lookup_hit_rate_percent = cache_lookup_hits / cache_lookup_queries_total`
- update `CNR3_EDIT_VERSION` to `CMS07-DIAG.honest-cache-hit-metrics`.

## Cold confirmation

Source inspection found the two lookup entry points named by the designer:

- `Cnr3OutputCacheCore::lookup_frame_and_add_ref_locked(...)`
- `Cnr3OutputCacheCore::pin_frame_locked(...)`

These are also the same two sites that call `observe_lookup_miss_rechurn_locked(...)` on not-found lookup paths.

The existing `lookup_refs_acquired` counter is not a true lookup hit-rate source: it increments only after `VSAPI::addFrameRef()` succeeds in `lookup_frame_and_add_ref_locked(...)`, and does not count the pin lookup path. The new D-SUM-04 query/hit counters therefore do not reuse `lookup_refs_acquired`.

## Placement

In both lookup entry points:

- `cache_lookup_queries_total` increments after early reject/invariant paths and immediately before `frame_index_.find(frame_number)`.
- `cache_lookup_hits` increments as the first diagnostic statement on the found path, immediately after the not-found branch returns and before slot-position validation.

No lookup return status, frame-index search, slot validation, pin, add-ref, miss observer, cache algorithm, or returned-frame behavior is changed.

## Health rows

Removed from the health block:

- `cache_hit_and_supplied_percent`

Added to the health block:

- `pred_returned_from_cache_percent`
- `current_frame_returned_from_cache_percent`
- `cache_hit_percent`
- `cache_lookup_hit_rate_percent`

The health block now includes D-SUM-04 in its print-gate wrapper and receives a fresh final D-SUM-04 ownership snapshot from `cnr3_free_filter`, consistent with the previous Option A quiescent snapshot model.

## D-SUM-04 raw rows

Adds two raw D-SUM-04 emitted rows:

- `cache_lookup_queries_total`
- `cache_lookup_hits`

`cache_lookup_misses` remains derived and is not added as a stored counter.

## Gating

- The three D-SUM-12 cache branch rows gate on D-SUM-12 COMPUTE && PRINT.
- `cache_lookup_hit_rate_percent` gates on D-SUM-04 COMPUTE && PRINT.
- When a source family is disabled but the health block emits due to another family, the affected row emits a static disabled marker and references no disabled field.
- When all source-family print gates are off, the health block is compiled/called out as before.

## Validation performed in sandbox

- `git apply --check`: PASS
- `git apply --check --whitespace=error`: PASS
- `git diff --check`: PASS
- Old health label `cache_hit_and_supplied_percent`: absent after patch
- Lookup query/hit instrumentation sites: exactly two query sites and two hit sites, at the approved lookup entry points

Full VS2026 build and proof harness remain local-Windows steps.

## Expected changed files

- `src/cnr3_build_config.h`
- `src/cnr3_cache_core.cpp`
- `src/cnr3_cache_core.h`
- `src/cnr3_cache_diagnostics.cpp`
- `src/cnr3_cache_diagnostics.h`
- `src/cnr3_diagnostics.cpp`
- `src/cnr3_diagnostics.h`
- `src/vapoursynth-Cnr3.cpp`
