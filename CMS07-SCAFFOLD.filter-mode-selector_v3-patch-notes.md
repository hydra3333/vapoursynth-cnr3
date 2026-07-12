# CMS07-SCAFFOLD.filter-mode-selector_v3 patch notes

## Purpose

Replacement patch for the compile-time VapourSynth filter-mode selector.

This v3 keeps the v2 placement and registration/logging shape, but changes the edit-version composition from mode-prefix to mode-suffix so harnesses that match the phase marker at the start of `CNR3_EDIT_VERSION` continue to work.

Default resulting marker:

```text
CMS07-SCAFFOLD.filter-mode-selector:fmUnordered
```

Local non-default examples:

```text
CMS07-SCAFFOLD.filter-mode-selector:fmParallelRequests
CMS07-SCAFFOLD.filter-mode-selector:fmParallel
```

The separate startup provenance line remains:

```text
filter_mode=<token>
```

## Baseline

Patch base is the clean committed source at:

```text
CMS07-DIAG.frame-lifecycle-bail-counters
```

## Expected changed files

```text
M src/cnr3_build_config.h
M src/vapoursynth-Cnr3.cpp
```

## Change summary

### src/cnr3_build_config.h

- Inserts the filter-mode selector block above the existing edit-version marker comments.
- Keeps `CNR3_FILTER_MODE_UNORDERED` as the committed default.
- Adds an exactly-one compile-time guard for:
  - `CNR3_FILTER_MODE_UNORDERED`
  - `CNR3_FILTER_MODE_PARALLEL_REQUESTS`
  - `CNR3_FILTER_MODE_PARALLEL`
- Defines:
  - `CNR3_SELECTED_FILTER_MODE`
  - `CNR3_SELECTED_FILTER_MODE_TEXT`
  - `CNR3_SELECTED_FILTER_MODE_TEXT_SUFFIX`
- Updates `CNR3_EDIT_VERSION` to:

```cpp
inline constexpr const char* CNR3_EDIT_VERSION =
    "CMS07-SCAFFOLD.filter-mode-selector" CNR3_SELECTED_FILTER_MODE_TEXT_SUFFIX;
```

### src/vapoursynth-Cnr3.cpp

- Adds one startup/config provenance line before `createVideoFilter`:

```text
CNR3[<id>] INFO CONFIG: filter_mode=<token> (compile-time selector)
```

- Replaces only the hardcoded `fmUnordered` argument in `createVideoFilter` with `CNR3_SELECTED_FILTER_MODE`.
- No other registration argument is changed.

## R-PROCESS-25 note

Touched proven registration code only at the registration mode argument and immediately adjacent startup logging.

No cache code, D-SUM counter code, pixel-processing code, recovery code, ownership code, or request-plan logic is changed.

## Validation performed in sandbox

Against clean `src(28).zip` baseline:

```text
git apply --check                         PASS
git apply --check --whitespace=error      PASS
git apply                                 PASS
git diff --check                          PASS
```

Replacement workflow check:

```text
apply v2 patch                            PASS
git apply -R v2 patch                     PASS
apply v3 patch after v2 revert            PASS
git diff --check                          PASS
```

Limited syntax validation with fake VapourSynth headers:

```text
src/vapoursynth-Cnr3.cpp                  PASS
src/cnr3_cache_core_selftest_main.cpp     PASS
```

Selector text/marker validation:

```text
default selected mode:
  CNR3_EDIT_VERSION = CMS07-SCAFFOLD.filter-mode-selector:fmUnordered
  CNR3_SELECTED_FILTER_MODE_TEXT = fmUnordered

parallel selected mode:
  CNR3_EDIT_VERSION = CMS07-SCAFFOLD.filter-mode-selector:fmParallel
  CNR3_SELECTED_FILTER_MODE_TEXT = fmParallel
```

Guard validation:

```text
zero selected modes       PASS: compile fails with the intended #error
two selected modes        PASS: compile fails with the intended #error
```

Note: the sandbox guard check used fake headers and syntax/preprocessor validation only. The coordinator still needs the VS2026 build/proof gate.

## Not run here

- VS2026 `cnr3` Debug|x64 or Release|x64 builds.
- VS2026 `cnr3_cache_core_selftest` Debug|x64 or Release|x64 builds.
- Canonical 4-way selftest.
- S8 byte-identical proof.
- Real VapourSynth mode-line print check.
- Non-default `fmParallel` real build-and-print check.
