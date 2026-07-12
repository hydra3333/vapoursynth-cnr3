# CMS07-SCAFFOLD.filter-mode-selector_v2 patch notes

## Purpose

Replacement patch for `CMS07-SCAFFOLD.filter-mode-selector.patch`.

This v2 patch keeps the same functional scope but refines placement/provenance:

- the filter-mode selector block is placed above the edit-version marker comments in `cnr3_build_config.h`;
- the selected filter-mode text prefixes `CNR3_EDIT_VERSION`;
- the separate `filter_mode=<token>` startup/config line remains for simple log-tool parsing.

Committed default remains `fmUnordered`.

## Baseline

Patch base:

```text
CMS07-DIAG.frame-lifecycle-bail-counters
```

This is a replacement patch against the clean committed baseline, not a patch on top of the first selector patch.

## Changed files

```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
```

## Behavioural fence

Default build selects `CNR3_FILTER_MODE_UNORDERED`, so `createVideoFilter` still receives `fmUnordered`.

No cache, diagnostic family, counter, recovery, ownership, or pixel-path logic is changed. The only runtime diagnostic addition is the single creation-time provenance line:

```text
CNR3[<id>] INFO CONFIG: filter_mode=<token> (compile-time selector)
```

## Edit-version strings by mode

Committed default:

```text
fmUnordered:CMS07-SCAFFOLD.filter-mode-selector
```

Local experimental flips:

```text
fmParallelRequests:CMS07-SCAFFOLD.filter-mode-selector
fmParallel:CMS07-SCAFFOLD.filter-mode-selector
```

## R-PROCESS-25 notes

### `src/cnr3_build_config.h`

Location: immediately after the initial file-level build-configuration comment and before the existing edit-version marker comment.

Changes:

- add the filter-mode selector comment block;
- add exactly-one selected mode defines;
- add the exactly-one compile-time guard;
- add `CNR3_SELECTED_FILTER_MODE` and `CNR3_SELECTED_FILTER_MODE_TEXT`;
- update `CNR3_EDIT_VERSION` to concatenate the selected mode text prefix with the selector phase marker.

The selected VapourSynth mode token remains a macro so this header can still be included by selftest/non-VapourSynth translation units without requiring VapourSynth enum visibility until the token is expanded in `vapoursynth-Cnr3.cpp`.

### `src/vapoursynth-Cnr3.cpp`

Location: `cnr3_create_filter`, immediately before the existing `createVideoFilter` call.

Changes:

- add one `std::fprintf(stderr, ...)` startup/config line printing `filter_mode=<token>`;
- replace the single hardcoded `fmUnordered` argument to `createVideoFilter` with `CNR3_SELECTED_FILTER_MODE`.

No other `createVideoFilter` argument is changed, and no registration control flow is moved.

## Mechanical validation performed in sandbox

```text
git apply --check                         PASS
git apply --check --whitespace=error      PASS
git apply                                 PASS
git diff --check                          PASS
```

Old-patch replacement sequence was also checked:

```text
apply old v1 patch                        PASS
git apply -R --check old v1 patch         PASS
git apply -R old v1 patch                 PASS
git apply --check v2 patch                PASS
git apply v2 patch                        PASS
git diff --check                          PASS
```

Limited fake-header syntax validation:

```text
src/cnr3_cache_core_selftest_main.cpp      PASS
src/vapoursynth-Cnr3.cpp                   PASS
```

Selector guard validation:

```text
default selected mode                      PASS
parallel selected mode                     PASS
zero selected modes                        PASS: compile fails with intended #error
two selected modes                         PASS: compile fails with intended #error
```

## Proof still required on coordinator machine

- VS2026 builds:
  - `cnr3` Debug|x64
  - `cnr3` Release|x64
  - `cnr3_cache_core_selftest` Debug|x64
  - `cnr3_cache_core_selftest` Release|x64
- Canonical 4-way selftest.
- Default S8 byte-identical proof vs the prior committed build.
- Real VapourSynth run log contains `filter_mode=fmUnordered` once at filter creation.
- Local flip to `CNR3_FILTER_MODE_PARALLEL` compiles and prints `filter_mode=fmParallel`; no behavioural claim for non-default modes in this patch.
- Zero-selected and two-selected guard failure demonstrations in the real VS build if the designer requests machine-local proof.
