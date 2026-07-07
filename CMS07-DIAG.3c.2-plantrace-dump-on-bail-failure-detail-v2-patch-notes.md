# CMS07-DIAG.3c.2 plantrace dump-on-bail failure detail — v2 replacement patch notes

## Purpose

This is a replacement for:

`CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail.patch`

Use this v2 patch instead:

`CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail-v2.patch`

The v2 patch preserves the designer-approved DIAG.3c.2 substance and addresses the designer review findings before the four-way build.

## Designer review findings addressed

### D3C2-1 build-safety

Confirmed and preserved: the new FAILED helper definitions are inside `#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)` regions.

Confirmed helpers:

- `cnr3_diag_plantrace_observe_initial_failed()` in `cnr3_arInitial.cpp` is inside the arInitial plantrace region.
- `cnr3_diag_plantrace_make_failed_result_from_request()` in `cnr3_arAllFramesReady.cpp` is inside the AR plantrace region.
- `cnr3_diag_plantrace_observe_failed_from_request()` in `cnr3_arAllFramesReady.cpp` is inside the AR plantrace region.
- `cnr3_diag_plantrace_observe_failed_with_progress()` in `cnr3_arAllFramesReady.cpp` is inside the AR plantrace region.

Therefore master-off should compile those helpers out with the rest of the plantrace family.

### D3C2-2 code-quality indentation cleanup

Fixed:

- Plantrace `#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)` / `#endif` directives are normalized to the surrounding project style rather than appearing with inconsistent partial indentation in nested blocks.
- Inserted observe calls now match the surrounding body indentation.
- The one side-effect reindent of an existing `cnr3_set_filter_error(...)` line in `cnr3_start_live_recovery()` was restored so the proven line remains visually untouched and the diff remains a pure insertion before it.

## Scope preserved from v1

The v2 patch still implements:

- Set 5 `fail_reason` categories.
- `outcome=FAILED` records.
- Set 4 local `E` / `X` result codes.
- Per-site additive FAILED-record writes at all 65 bail sites.
- Explicit once-guarded bail dump using the shared plantrace dump path.
- Recovery X/E derivation.
- AI-06 code-derived category distinction between recovery refusal and discharge failure.
- Selftest reference fixture with a FAILED recovery record and non-empty X.

## Changed files

Expected changed files remain:

```text
src/cnr3_arInitial.cpp
src/cnr3_arAllFramesReady.cpp
src/vapoursynth-Cnr3.cpp
src/cnr3_diagnostics.h
src/cnr3_diagnostics.cpp
src/cnr3_cache_core_selftest_main.cpp
src/cnr3_build_config.h
```

No cache-core files, project files, pin-list accessor, or production `Cnr3LiveRecoveryHoleOutcome` enum change.

## Sandbox validation

Performed against the latest uploaded post-DIAG.3c.1 source:

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
```

Additional static checks:

```text
FAILED helper definitions inside plantrace #if: PASS
No remaining indented CNR3_DIAG_COMPUTE_DSUM_PLANTRACE #if/#endif artifacts: PASS
Expected changed file set only: PASS
```

## Not performed in sandbox

- VS2026 Debug|x64 build.
- VS2026 Release|x64 build.
- MSVC /W4 /WX warning proof.
- Four-way selftest execution.
- R-PROCESS-19 macro-off proof.
- S1/S7/S8 .vpy byte-identical proof.
- Induced arInitial / arAllFramesReady bail proof.

## Apply guidance

Do not apply both v1 and v2 patches.

If v1 was not applied locally, apply only v2.

If v1 was already applied locally, reset/revert v1 first, then apply v2 to the clean post-DIAG.3c.1 source.
