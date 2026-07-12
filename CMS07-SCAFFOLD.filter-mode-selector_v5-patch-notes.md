# CMS07-SCAFFOLD.filter-mode-selector_v5 — Patch Notes

## Status

Draft replacement patch for designer review.

This replaces the earlier selector patch variants rather than layering on top of them. It is intended to be applied against the clean committed baseline:

```text
CMS07-DIAG.frame-lifecycle-bail-counters
```

## Reason for v5

The v3 selector patch correctly suffixed `CNR3_EDIT_VERSION` in the selftest, and it correctly emitted the selected filter mode in the real VapourSynth plugin run. However, the real plugin creation path did not emit `CNR3_EDIT_VERSION`; only the cache-core selftest printed it.

The incident analysis showed that this was not a new selector regression. Available source snapshots back to `CMS07-G.6A` show `CNR3_EDIT_VERSION` as a selftest provenance marker, while the later live plugin creation path was never wired to print it.

v5 adds explicit, gated plugin startup provenance at the earliest safe creation point.

## Files changed

```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
```

## Functional scope

### cnr3_build_config.h

Adds the compile-time filter-mode selector above the edit-version marker comments:

```cpp
#define CNR3_FILTER_MODE_UNORDERED 1
//#define CNR3_FILTER_MODE_PARALLEL_REQUESTS 1
//#define CNR3_FILTER_MODE_PARALLEL 1
```

Adds exactly-one guard:

```cpp
#if (defined(CNR3_FILTER_MODE_UNORDERED) + defined(CNR3_FILTER_MODE_PARALLEL_REQUESTS) + defined(CNR3_FILTER_MODE_PARALLEL)) != 1
#   error "CNR3 filter mode: uncomment exactly ONE of CNR3_FILTER_MODE_UNORDERED / _PARALLEL_REQUESTS / _PARALLEL in cnr3_build_config.h"
#endif
```

Adds selected-mode tokens:

```cpp
CNR3_SELECTED_FILTER_MODE
CNR3_SELECTED_FILTER_MODE_TEXT
CNR3_SELECTED_FILTER_MODE_TEXT_SUFFIX
```

Adds plugin startup provenance gate immediately above the edit-version marker comments:

```cpp
#define CNR3_EMIT_PLUGIN_STARTUP_PROVENANCE 1
```

Updates the marker to suffix the selected mode while preserving the phase marker at the start:

```cpp
#define CNR3_EDIT_VERSION_LITERAL "CMS07-SCAFFOLD.filter-mode-selector"

inline constexpr const char* CNR3_EDIT_VERSION =
    CNR3_EDIT_VERSION_LITERAL CNR3_SELECTED_FILTER_MODE_TEXT_SUFFIX;
```

Committed-default marker:

```text
CMS07-SCAFFOLD.filter-mode-selector:fmUnordered
```

Local fmParallel marker:

```text
CMS07-SCAFFOLD.filter-mode-selector:fmParallel
```

### vapoursynth-Cnr3.cpp

Adds gated plugin startup provenance after instance config validation and before pixel configuration / D-SUM-02 creation snapshot:

```text
CNR3[<id>] INFO CONFIG: edit_version=CMS07-SCAFFOLD.filter-mode-selector:fmUnordered
CNR3[<id>] INFO CONFIG: filter_mode=fmUnordered (compile-time selector)
```

This is the earliest safe point because:

```text
- data exists;
- source is owned by data;
- data->config has been constructed and validated;
- instance_id is available;
- no cache lock is held;
- no frame request has been issued;
- createVideoFilter has not yet been called.
```

Replaces the hardcoded `fmUnordered` argument in `createVideoFilter` with:

```cpp
CNR3_SELECTED_FILTER_MODE
```

No other registration argument is changed.

## Validation performed in sandbox

```text
git apply --check                         PASS
git apply --check --whitespace=error      PASS
git apply                                 PASS
git diff --check                          PASS
```

Replacement workflow validation:

```text
apply v3 patch                            PASS
git apply -R v3 patch                     PASS
apply v5 patch after v3 revert            PASS
git diff --check                          PASS

apply v4 patch                            PASS
git apply -R v4 patch                     PASS
apply v5 patch after v4 revert            PASS
git diff --check                          PASS
```

Not run here:

```text
- VS2026 Debug/Release builds;
- canonical 4-way selftest;
- real VapourSynth plugin creation run;
- strings64.exe binary-retention proof;
- S8 byte-identical proof;
- zero/two-selected guard-failure builds;
- macro-off startup-provenance build.
```

## Recommended proof gate

1. Canonical 4-way selftest remains unchanged:

```text
Debug normal:        56/56 PASS, exit 0
Release normal:      56/56 PASS, exit 0
Release forced-fail: 55/56 expected FAIL, first_failed_status invariant_violation, exit 1
Release verbose:     56/56 PASS, exit 0
```

2. Selftest marker proof:

```text
edit_version: CMS07-SCAFFOLD.filter-mode-selector:fmUnordered
```

3. Real VapourSynth plugin creation provenance proof:

```text
CNR3[1] INFO CONFIG: edit_version=CMS07-SCAFFOLD.filter-mode-selector:fmUnordered
CNR3[1] INFO CONFIG: filter_mode=fmUnordered (compile-time selector)
```

4. DLL binary-retention proof:

```bat
"C:\000-PStools\strings64.exe" -n 8 "<path>\cnr3.dll" | findstr /i "CMS07-SCAFFOLD filter_mode fmUnordered"
```

5. Selector guard proof:

```text
zero selected modes: build fails with the intended #error
two selected modes:  build fails with the intended #error
```

6. Local non-default compile-and-print proof:

```text
CNR3_FILTER_MODE_PARALLEL selected locally
plugin creation log prints filter_mode=fmParallel
edit_version prints CMS07-SCAFFOLD.filter-mode-selector:fmParallel
restore fmUnordered before commit
```

7. Startup-provenance gate macro-off proof, if the designer wants it:

```text
comment/undef CNR3_EMIT_PLUGIN_STARTUP_PROVENANCE locally
plugin build succeeds
plugin creation log does not print CONFIG edit_version/filter_mode lines
restore gate before commit
```

8. Default output proof:

```text
Default fmUnordered Release output byte-identical to prior committed S8 y4m artefact.
```

## Revert current local selector patch and apply v5

If v3 is currently applied but not committed:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github"

git status --short

git apply -R --check --ignore-whitespace CMS07-SCAFFOLD.filter-mode-selector_v3.patch
git apply -R --ignore-whitespace CMS07-SCAFFOLD.filter-mode-selector_v3.patch

git diff --check
git status --short
```

If v4 is currently applied but not committed, use the same commands with `_v4.patch`.

Then apply v5:

```bat
git apply --check --ignore-whitespace CMS07-SCAFFOLD.filter-mode-selector_v5.patch
git apply --ignore-whitespace CMS07-SCAFFOLD.filter-mode-selector_v5.patch

git diff --check
git status --short
```

Expected modified files:

```text
 M src/cnr3_build_config.h
 M src/vapoursynth-Cnr3.cpp
```

