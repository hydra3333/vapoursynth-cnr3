# CMS07-SCAFFOLD.filter-mode-selector — patch delivery notes

## Baseline

Uploaded source: `src(27).zip`.

Verified source marker before patch:

```cpp
inline constexpr const char* CNR3_EDIT_VERSION = "CMS07-DIAG.frame-lifecycle-bail-counters";
```

Patch marker:

```cpp
inline constexpr const char* CNR3_EDIT_VERSION = "CMS07-SCAFFOLD.filter-mode-selector";
```

## Scope implemented

This patch implements the compile-time VapourSynth filter-mode selector requested by the scope.

Default committed selector:

```cpp
#define CNR3_FILTER_MODE_UNORDERED 1
//#define CNR3_FILTER_MODE_PARALLEL_REQUESTS 1
//#define CNR3_FILTER_MODE_PARALLEL 1
```

The default therefore preserves the current committed `fmUnordered` behaviour.

## Changed files

```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
```

No cache, recovery, diagnostics-family counters, D-SUM rows, frame-processing, ownership, pinning, or pixel-path code was changed.

## R-PROCESS-25 notes

### src/cnr3_build_config.h

- Line 35: edit marker changed to `CMS07-SCAFFOLD.filter-mode-selector`.
- Lines 37-85: new filter-mode selector block inserted immediately below the edit marker, before the existing live scene-change default block.
- Lines 68-70: uncomment-exactly-one selector lines.
- Lines 72-74: hard compile-time guard for zero or multiple selected modes.
- Lines 76-85: selected mode token and selected mode display string.

This header still does not include VapourSynth headers. `CNR3_SELECTED_FILTER_MODE` is a token macro consumed only where the VapourSynth enum names are in scope.

### src/vapoursynth-Cnr3.cpp

- Lines 567-572: one new startup/provenance line prints the selected compile-time filter mode:

```text
CNR3[<id>] INFO CONFIG: filter_mode=<token> (compile-time selector)
```

- Line 580: the hardcoded `fmUnordered` createVideoFilter argument is replaced with `CNR3_SELECTED_FILTER_MODE`.

No other `createVideoFilter` argument was changed. The registration code was not reordered.

## Placement of the mode log line

There is no existing startup/config summary block adjacent to `createVideoFilter` in `vapoursynth-Cnr3.cpp`. The mode line is therefore placed immediately before the `createVideoFilter` call, after the instance configuration has been created and validated and after the dependency array has been prepared.

The line uses the existing instance id and stderr style:

```cpp
std::fprintf(
    stderr,
    "CNR3[%d] INFO CONFIG: filter_mode=%s (compile-time selector)\n",
    data->config.instance_id.value,
    CNR3_SELECTED_FILTER_MODE_NAME
);
```

The format intentionally contains the simple `filter_mode=<token>` field for later log-tool provenance parsing.

## Sandbox validation performed

Mechanical patch validation against a repo-root layout with `src/` paths:

```text
git apply --check                         PASS
git apply --check --whitespace=error      PASS
git apply                                 PASS
git diff --check                          PASS
```

Expected modified files after apply:

```text
 M src/cnr3_build_config.h
 M src/vapoursynth-Cnr3.cpp
```

## Selector guard validation performed

A small C++20 syntax harness including `cnr3_build_config.h` verified:

```text
default selected mode     PASS: CNR3_SELECTED_FILTER_MODE == fmUnordered
parallel selected mode    PASS: CNR3_SELECTED_FILTER_MODE == fmParallel
zero selected modes       PASS: compile fails with the intended plain-English #error
two selected modes        PASS: compile fails with the intended plain-English #error
```

Observed zero/two guard error text:

```text
CNR3 filter mode: uncomment exactly ONE of CNR3_FILTER_MODE_UNORDERED / _PARALLEL_REQUESTS / _PARALLEL in cnr3_build_config.h
```

## Limited syntax validation performed

Using fake VapourSynth headers for syntax-only checks only:

```text
src/vapoursynth-Cnr3.cpp                  PASS
src/cnr3_cache_core_selftest_main.cpp     PASS
```

These checks are not a substitute for VS2026 builds.

## Not run here

- VS2026 builds.
- Canonical 4-way selftest.
- S8 y4m byte-identical proof against the prior committed build.
- Real VapourSynth creation log check.
- Non-default `fmParallelRequests` or `fmParallel` VS2026 build-and-print checks.

## Coordinator proof gate expected

Default selection:

```text
cnr3 Debug|x64 build PASS
cnr3 Release|x64 build PASS
cnr3_cache_core_selftest Debug|x64 build PASS
cnr3_cache_core_selftest Release|x64 build PASS
canonical 4-way unchanged: 56/56, 56/56, forced-fail 55/56 e1, 56/56
S8 output byte-identical to prior committed default build
creation log contains: filter_mode=fmUnordered
```

Non-default selector checks for this patch only:

```text
flip to CNR3_FILTER_MODE_PARALLEL_REQUESTS: build-and-print check only, no behaviour claim
flip to CNR3_FILTER_MODE_PARALLEL: build-and-print check only, no behaviour claim
zero-selected guard: build fails with intended #error
two-selected guard: build fails with intended #error
```
