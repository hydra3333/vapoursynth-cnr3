# CMS07-DIAG.honest-cache-hit-metrics - replacement patch notes

## Status

Replacement patch generated from the committed CMS07-DIAG.derived-health-ratios source baseline.

This replacement patch contains the designer-approved CMS07-DIAG.honest-cache-hit-metrics diff plus the one missing D-SUM-04 derived emission row:

- `cache_lookup_misses = cache_lookup_queries_total - cache_lookup_hits`, underflow-guarded.

Marker remains unchanged:

- `CNR3_EDIT_VERSION = "CMS07-DIAG.honest-cache-hit-metrics"`

## Scope review

The v3 scope records the formerly outstanding `cache_lookup_misses` D-SUM-04 row as delivered and ready for build plus proof. The replacement patch implements that row only; it does not add a stored misses counter.

## Cold source confirmation after replacement patch is applied

### D-SUM-04 emission insertion

File: `src/cnr3_cache_diagnostics.cpp`

- D-SUM-04 emission function gate: line 268, `#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)`
- Safety pairing in config: `src/cnr3_build_config.h` lines 213-219 define `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`, derive `CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE`, and error if PRINT is enabled without COMPUTE.
- New derived row starts at line 312.
- Row label line: line 313, `cache_lookup_misses`.
- Placement: immediately after line 311 `cache_lookup_hits`, before line 318 `lookup_refs_acquired`.

Inserted code:

```cpp
    cnr3_cache_diag_write_uint64_row(
        instance_id, "D-SUM-04", "D-SUM-04", "cache_lookup_misses",
        stats.cache_lookup_queries_total >= stats.cache_lookup_hits
            ? (stats.cache_lookup_queries_total - stats.cache_lookup_hits)
            : 0U);   // derived; the else-branch is unreachable when the two increments are placed correctly
                     // (query++ before the find, hit++ on the found branch) -- reaching it signals a placement bug.
```

### Lookup counter placement unchanged from reviewed patch

File: `src/cnr3_cache_core.cpp`

- `lookup_frame_and_add_ref_locked(...)`
  - query observer: lines 3788-3790, immediately before `frame_index_.find(frame_number)` at line 3792
  - hit observer: lines 3801-3803, first diagnostic statement after the not-found branch returns
- `pin_frame_locked(...)`
  - query observer: lines 3886-3888, immediately before `frame_index_.find(frame_number)` at line 3890
  - hit observer: lines 3899-3901, first diagnostic statement after the not-found branch returns

Both increment sites remain under the D-SUM-04 compute gate:

- `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`

Observer-helper style remains as reviewed:

- `observe_cache_lookup_query_locked()` calls `cnr3_cache_ownership_diagnostic_observe_cache_lookup_query(...)`
- `observe_cache_lookup_hit_locked()` calls `cnr3_cache_ownership_diagnostic_observe_cache_lookup_hit(...)`

## Fence confirmation

Unchanged from the approved patch:

- no cache algorithm change
- no lookup semantics change
- no pin/ref ownership change
- no prune/recovery change
- no returned-frame behavior change
- no third stored misses counter
- no new `Cnr3CacheOwnershipDiagnosticStats` field for misses
- no changes to `observe_cache_lookup_query_locked` placement
- no changes to `observe_cache_lookup_hit_locked` placement
- no changes to `observe_lookup_miss_rechurn_locked`
- no changes to `lookup_refs_acquired` or `pins_acquired`
- no changes to the three per-frame health rows
- no changes to `cache_lookup_hit_rate_percent`
- no edit-version change beyond the already-approved marker

A source grep found `cache_lookup_misses` only in `src/cnr3_cache_diagnostics.cpp` at the new derived emission row.

## Whole-patch scan against prior approved patch

Compared against the designer-approved `CMS07-DIAG.honest-cache-hit-metrics.patch`, the replacement patch differs only in `src/cnr3_cache_diagnostics.cpp`:

- patch index and hunk length metadata changed for that file
- six added source lines insert the `cache_lookup_misses` derived row immediately after `cache_lookup_hits`

No other source file hunk content changed versus the prior approved patch.

An interdiff file is provided separately:

- `old_vs_replacement_patch.diff`

## Sandbox validation performed

Validation was run against a fresh copy of the uploaded source baseline.

```text
git apply --check CMS07-DIAG.honest-cache-hit-metrics.patch                 PASS
git apply --check --whitespace=error CMS07-DIAG.honest-cache-hit-metrics.patch PASS
git apply CMS07-DIAG.honest-cache-hit-metrics.patch                         PASS
git diff --check                                                            PASS
```

Changed files after applying the replacement patch:

```text
src/cnr3_build_config.h
src/cnr3_cache_core.cpp
src/cnr3_cache_core.h
src/cnr3_cache_diagnostics.cpp
src/cnr3_cache_diagnostics.h
src/cnr3_diagnostics.cpp
src/cnr3_diagnostics.h
src/vapoursynth-Cnr3.cpp
```

## Coordinator build and canonical 4-way

VS2026 build and runtime selftests are coordinator-side Windows steps. After applying the replacement patch, build these four VS2026 targets:

```text
cnr3                         Debug|x64
cnr3                         Release|x64
cnr3_cache_core_selftest      Debug|x64
cnr3_cache_core_selftest      Release|x64
```

Then run the canonical R-PROCESS-26 command block exactly in this separated form:

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

Expected exit codes:

```text
Debug normal exit_code=0
Release normal exit_code=0
Release forced-fail exit_code=1
Release verbose exit_code=0
```

Expected selftest count remains unchanged at 56/56 for normal and verbose runs; the forced-fail harness is expected to fail intentionally with exit code 1.

## Commit suggestion after coordinator PASS and designer review

```text
CMS07-DIAG.honest-cache-hit-metrics: print derived cache lookup misses

Add the derived D-SUM-04 cache_lookup_misses summary row immediately after
cache_lookup_hits, using cache_lookup_queries_total - cache_lookup_hits with
an underflow guard.

This completes the designer-approved honest cache-hit metrics replacement
patch without adding a third stored counter or changing the reviewed lookup
query/hit observer placements.

Verified:
- VS2026 Debug|x64 cnr3 build: PASS
- VS2026 Release|x64 cnr3 build: PASS
- VS2026 Debug|x64 cnr3_cache_core_selftest build: PASS
- VS2026 Release|x64 cnr3_cache_core_selftest build: PASS
- Debug normal: 56/56 PASS, exit_code=0
- Release normal: 56/56 PASS, exit_code=0
- Release forced-fail harness: expected FAIL, exit_code=1
- Release verbose: 56/56 PASS, exit_code=0
```
