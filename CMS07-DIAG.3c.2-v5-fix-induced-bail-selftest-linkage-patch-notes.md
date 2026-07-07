# CMS07-DIAG.3c.2 v5 overlay notes — fix induced-bail selftest linkage

## Purpose

Fix the v4 build failure in `cnr3_cache_core_selftest`:

- unresolved `cnr3_arInitial`
- unresolved `cnr3_arAllFramesReady`
- unresolved `cnr3_discard_frame_data_with_cache`

The failure occurs because the selftest executable does not link the live getFrame translation units.

## Change strategy

This overlay removes the selftest's direct calls to unlinked live getFrame functions. To keep the recovery X-derivation proof meaningful, the recovery FAILED result derivation is moved into an inline private helper in `cnr3_plugin_internal.h`:

- `cnr3_live_plantrace_make_failed_result_from_request()`

The live `arAllFramesReady` FAILED-record path now calls this shared helper, and the selftest calls the same helper with a constructed recovery `Cnr3LiveGetFrameFrameData`. This proves the same request-data/per-hole-outcome X-derivation logic used by the live AR path without requiring the selftest executable to link the live getFrame translation units.

## Scope limitation

This is build-safe and exercises the load-bearing derivation logic, but it is not a direct invocation of `cnr3_arInitial()` / `cnr3_arAllFramesReady()` inside the selftest executable. A direct live invocation would require either linking live getFrame translation units into the selftest project or creating a separate project/harness that already links them.

## Files changed from v4

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_cache_core_selftest.cpp
src/cnr3_plugin_internal.h
```

## Expected selftest totals

With `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` enabled:

```text
Debug normal:          57/57 PASS, exit 0
Release normal:        57/57 PASS, exit 0
Release forced-fail:   56/57 expected FAIL, invariant_violation, exit 1
Release verbose:       57/57 PASS, exit 0
```

With `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE` macro-off:

```text
Debug normal:          56/56 PASS, exit 0
Release normal:        56/56 PASS, exit 0
Release forced-fail:   55/56 expected FAIL, invariant_violation, exit 1
Release verbose:       56/56 PASS, exit 0
```

## Validation performed in sandbox

```text
git apply --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
g++ syntax-only cnr3_cache_core_selftest.cpp: PASS
g++ syntax-only cnr3_arAllFramesReady.cpp: PASS
```
