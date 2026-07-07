# CMS07-DIAG.3c.2 v4 overlay patch notes — induced live-bail selftest proof

## Purpose

Adds the coder-owned white-box induced-bail proof required before DIAG.3c.2 commit.
This is a selftest-only overlay on top of the approved DIAG.3c.2 v3 tree.

It covers both required live-path bail cases:

1. arInitial invalid-lifecycle bail:
   - calls the real `cnr3_arInitial()` with non-null incoming `frame_data`.
   - expects `return nullptr` and one `setFilterError` call.
   - verifies a minimal FAILED plantrace record:
     - `outcome=FAILED`
     - `fail_reason=INVALID_LIFECYCLE`
     - `E = requested frame`
     - `X = []`
     - no computed/adopted/post-compute-loser progress.
   - calls the clean-end dump after the bail dump under PRINT gate and verifies the once-guard does not append/change records.

2. recovery arAllFramesReady source-retrieval bail:
   - seeds real cache state with anchor output 29.
   - calls the real `cnr3_arInitial(33, ...)` to produce a recovery O record with holes 30/31/32 and source requests 30/31/32/33.
   - after arInitial, seeds output 30 into the cache so the live recovery loop adopt-skips it as progress.
   - calls the real `cnr3_arAllFramesReady(33, ...)` using a selftest VSAPI whose `getFrameFilter()` returns null.
   - expects the live recovery loop to fail at hole 31 with:
     - `outcome=FAILED`
     - `fail_reason=SOURCE_RETRIEVAL_FAILED`
     - `E = 31`
     - `X = [32,33]`
     - `adopted_skipped = [30]`
   - verifies `frame_data` is discharged/deleted and the bail dump once-guard is set.
   - calls clean-end dump under PRINT gate and verifies the once-guard does not append/change records.

## Changed file

```text
src/cnr3_cache_core_selftest.cpp
```

## Scope / fence

- Selftest-only overlay.
- No production getFrame logic change.
- No cache-core algorithm change.
- No project-file change.
- No build-config marker or gate change.
- Adds the test only when `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` is enabled.

## Expected selftest totals

With plantrace enabled:

```text
normal / verbose: 57/57 PASS
forced-fail:      56/57 expected FAIL, first_failed_status=invariant_violation, exit 1
```

With `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` macro-off:

```text
normal / verbose: 56/56 PASS
forced-fail:      55/56 expected FAIL, first_failed_status=invariant_violation, exit 1
```

The macro-off total remains 56 because the white-box induced-bail selftest is compiled with the plantrace family.

## Validation performed here

Against the approved DIAG.3c.2 v3 source tree:

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
syntax-only selftest.cpp with local VapourSynth stub, plantrace ON: PASS
syntax-only selftest.cpp with local VapourSynth stub, plantrace OFF: PASS
```

## Required local proof after applying

1. Build Debug|x64 and Release|x64 with plantrace enabled.
2. Run all-on four-way; expected baseline is 57 normal tests.
3. Confirm the log includes:
   - an arInitial minimal FAILED block with `fail_reason=INVALID_LIFECYCLE`, E=N, X=[].
   - a recovery live FAILED block with `fail_reason=SOURCE_RETRIEVAL_FAILED`, E=31, X=[32,33], progress `adopted_skipped=[30]`.
   - no duplicate clean-end dump for either induced bail.
4. Macro-off: comment out `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE`, rebuild Debug/Release, run four-way; expected baseline returns to 56.
5. Restore macro on and capture:
   - `git diff -- src\cnr3_build_config.h`
   - `git status --short`
