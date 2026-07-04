# CMS07-DIAG.2b patch notes — ownership, integrity, store telemetry

## Patch

File: `CMS07-DIAG.2b-ownership-integrity-store.patch`

Generated against the current `src(16).zip` baseline supplied in this chat.

## Scope implemented

- D-SUM-04 ownership balance:
  - slot pin balance counted only at `pin_frame_locked()` success and `unpin_frame_locked()` success;
  - lookup-ref handoff counted at the lookup add-ref success site, public-wrapper adoption success, and public-wrapper adoption-failure/freeFrame rebalance path;
  - `ownership_errors` increments only on the adoption-failure rebalance path;
  - pin-list record/discharge counters deliberately not implemented in DIAG.2b to avoid AS4 double-counting.
- D-SUM-05 cache integrity:
  - one invocation counter at `cache_state_invariants_hold_locked()` entry;
  - designer-approved `CNR3_DSUM05_FAIL(tag)` macro on the 18 existing `return false` sites;
  - first violation site captured once;
  - structural min/max and summary samples reported.
- D-SUM-08 cache store:
  - store outcome observed in `store_owned_frame_and_prune_impl()` after store outcome is known;
  - `store_failures` excludes `Cnr3Status::duplicate` per A1;
  - AS2 checkpoint promotions counted from `as2_summary.checkpoint_promoted` only.

## Changed files

```text
src/cnr3_cache_core.cpp
src/cnr3_cache_core.h
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_cache_diagnostics.cpp
src/cnr3_cache_diagnostics.h
src/vapoursynth-Cnr3.cpp
```

No `cnr3_build_config.h` change is included.

## Sandbox validation performed here

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git diff --check after apply: PASS
syntax-only C++20 compile over all .cpp files using a minimal local VapourSynth stub: PASS
syntax-only R-PROCESS-19 matrix using temporary build_config edits:
  all ON: PASS
  04 OFF / 05+08 ON: PASS
  05 OFF / 04+08 ON: PASS
  08 OFF / 04+05 ON: PASS
  04+05+08 OFF: PASS
```

The syntax-only compile is a sandbox sanity check only. The authoritative build/test remains the coordinator's VS2026 Debug/Release build and runtime gate on the real VapourSynth headers/libraries.

## Apply sequence from repo root

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\src"

git status --short

git apply --check "C:\path\to\CMS07-DIAG.2b-ownership-integrity-store.patch"
git apply --check --whitespace=error "C:\path\to\CMS07-DIAG.2b-ownership-integrity-store.patch"
git apply "C:\path\to\CMS07-DIAG.2b-ownership-integrity-store.patch"

git diff --check
git status --short
```

Adjust the `cd` path if your local repo root is one directory above `src`. Apply from the directory containing these source files, or from the repo root if that is where these files live.

## VS2026 four-way selftest gate

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"

x64\Debug\cnr3_cache_core_selftest.exe 1>NUL & echo exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe 1>NUL & echo exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL & echo exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL & echo exit_code=%ERRORLEVEL%
```

Expected default totals per designer gate:

```text
Debug normal:       56/56 PASS, exit 0
Release normal:     56/56 PASS, exit 0
Release forced-fail: 55/56, exit 1
Release verbose:    56/56 PASS, exit 0
```

## R-PROCESS-19 compile-out matrix

Temporarily comment out the relevant `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`, `CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY`, and/or `CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE` lines in `cnr3_build_config.h`, rebuild, run the four-way selftest, then revert those temporary edits before committing.

Required configurations:

```text
all ON
04 OFF / 05+08 ON
05 OFF / 04+08 ON
08 OFF / 04+05 ON
04+05+08 OFF
```

Do not commit the temporary `cnr3_build_config.h` macro-off edits.
