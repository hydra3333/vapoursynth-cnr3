# CMS07-FEATURE.cnr2-descriptive-option-parser patch notes

## Scope

Implements the approved CNR3 descriptive option-parser surface from the v6/v6.1 option-mapping spec and verified gap analysis.

The patch adds create-time parsing, validation, application, live emission, maintainer documentation, and user documentation for these CNR3 options:

```text
y_threshold
y_strength
u_threshold
u_strength
v_threshold
v_strength
y_curve
u_curve
v_curve
scene_threshold
scene_chroma
```

The old cnr2 names remain reference-only documentation names. `mode` is not accepted as a CNR3 option.

## Source basis

Patch cut against the current uploaded GitHub source basis:

```text
src(33).zip
```

`src(33).zip` contains the previously missing files used by the gap analysis confirmation:

```text
cnr3_response_tables.cpp
cnr3_response_tables.h
cnr3_frame_processing.cpp
cnr3_frame_processing.h
```

The earlier source-completeness caveat is closed for this patch.

## Changed files

```text
README.md
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
```

`README.md` was not present in the uploaded source basis, so this patch creates it at the repository root. If the local repository has an existing `README.md` that was not included in the source zip, stop and merge that documentation hunk deliberately rather than overwriting existing documentation.

## What the patch adds

### src/vapoursynth-Cnr3.cpp

- Adds the full parser-site maintainer documentation block required by v6/v6.1.
- Adds `Cnr3CreateOptions` with cnr2-equivalent defaults.
- Adds create-time parsing helpers for optional integer, float, curve-string, and bool options.
- Adds strict validation:
  - thresholds and strengths: integer `0..255` inclusive;
  - curves: exactly `wide` or `narrow`;
  - `scene_threshold`: finite float `0.0..100.0` inclusive;
  - `scene_chroma`: integer/bool `0` or `1`.
- Replaces hardcoded response-table config construction with resolved parsed/default values.
- Replaces the literal scene-chroma `false` in scene-config construction with resolved `scene_chroma`.
- Replaces the literal scene-threshold default with resolved `scene_threshold`.
- Extends plugin registration so VapourSynth exposes the 11 descriptive options.
- Extends `response_config` emission so it now reports:

```text
response_config: y=35/192/wide u=47/255/narrow v=47/255/narrow scene_threshold=10.0 scene_chroma=false
```

The response line is emitted from the resolved live config path, not from constants.

### src/cnr3_build_config.h

- Bumps the edit marker to:

```text
CMS07-FEATURE.cnr2-descriptive-option-parser-CMS07-scene-threshold-debt
```

- Adds the required debt note: `scene_threshold` is exposed as CNR3's descriptive option for cnr2 `scdthr`, while the underlying scene-reset threshold math remains the existing CMS07 native-depth round-to-nearest equivalent. Exact cnr2 high-bit-depth scdthr emulation is deliberately not part of this parser-plumbing patch.

### README.md

- Adds user-facing documentation for all CNR3 descriptive options.
- Includes the cnr2-to-CNR3 name equivalence table.
- Explains threshold, strength, curve, scene-threshold, and scene-chroma effects.
- Documents threshold-zero safety semantics.
- Documents the 8-bit compatibility / 9..16-bit CMS07 scaling policy.

## Explicitly out of scope

No changes are made to:

```text
blend math
response-table formulas
threshold-zero table construction
curve wiring
scene-threshold math
scene-detection algorithm
cache/recovery/PlanRetry/filter-mode behaviour
output-authority/cache semantics
```

The patch only parses and applies user options to existing code paths.

## Gap-analysis contract mapping

| Gap item | Patch action |
|---|---|
| parse missing for all 11 options | add create-time parsing in `cnr3_parse_create_options()` |
| validate missing for all 11 options | add explicit range/string/bool validation helpers |
| defaults hardcoded | preserve same defaults in `Cnr3CreateOptions`; no-option path resolves to identical values |
| six threshold/strength apply path exists | feed resolved values into existing response-table config |
| three curve params parse-only because curve wiring is already confirmed | feed resolved curve enums into existing response-table config; no table-builder change |
| scene_threshold exists as CMS07-equivalent statement-not-change | parse and pass resolved value to existing scene-config constructor; no math change |
| scene_chroma plumbing-only | parse and pass resolved bool to existing scene-config constructor |
| full parser-site documentation required | add full maintainer block next to parser/defaulting path |
| README/user docs required | add `README.md` option documentation |
| marker/debt note required | bump marker and add debt note in `cnr3_build_config.h` |

## Source evidence retained from the verified gap analysis

The current source already contains the algorithmic pieces that this patch deliberately does not change:

```text
src/cnr3_response_tables.cpp:
  - threshold==0 centre-only behaviour at build_cnr3_weight_table lines 47-52 in src(33).zip.
  - curve wiring at build_cnr3_response_tables lines 168-187 in src(33).zip:
    plane_config.curve == Cnr3ResponseCurveKind::wide -> wide_response.

src/cnr3_frame_processing.cpp:
  - scene_chroma parameter and config storage at cnr3_make_scene_change_config_from_vscnr2_scdthr lines 1040-1100 in src(33).zip.
  - scene-threshold formula remains the existing CMS07 native-depth round-to-nearest equivalent.
```

## Apply sequence

From repository root on `dev_cache_manager`:

```bat
git status --short
git apply --ignore-whitespace --check CMS07-FEATURE.cnr2-descriptive-option-parser.patch
git apply --ignore-whitespace CMS07-FEATURE.cnr2-descriptive-option-parser.patch
git diff --check
git status --short
```

Fallback if `git apply` is not usable:

```bat
patch -p1 --binary < CMS07-FEATURE.cnr2-descriptive-option-parser.patch
```

If `README.md` already exists locally and the patch does not apply cleanly, stop and merge the README documentation deliberately; do not overwrite existing repository documentation silently.

## Sandbox validation performed here

```text
git apply --ignore-whitespace --check: PASS
git apply --check --whitespace=error: PASS
git apply: PASS
git diff --check: PASS
patch -p1 --binary --dry-run: PASS
g++ -std=c++20 -fsyntax-only src/vapoursynth-Cnr3.cpp with available R76 headers: PASS
```

No VS2026 build or VapourSynth runtime proof was run in this sandbox. The syntax-only check is not a replacement for the required Debug/Release VS2026 builds.

## Required post-apply proof gate

The designer-approved 12-item gate still applies. Numbering corrected per designer note: the byte-identical no-args output run is item 11, not item 10.

1. Debug x64 build PASS.
2. Release x64 build PASS.
3. Canonical 4-way selftest PASS, count unchanged unless justified.
4. Source proof all CNR3 descriptive options parse from create-time input.
5. Default `.vpy` no-options `response_config` equals committed interim defaults plus scene fields, with the new marker visible in the same startup/provenance run.
6. Explicit-defaults `.vpy` `response_config` equals the no-options run.
7. Non-default `.vpy` `response_config` reflects supplied values.
8. Invalid-option tests throw clear errors:
   - `scene_threshold < 0` and `scene_threshold > 100`;
   - threshold/strength `< 0` and `> 255`;
   - curve strings other than exactly `wide` or `narrow`;
   - `scene_chroma` outside `0..1`.
9. `threshold==0` tests do not crash or NaN and verify centre-only behaviour.
10. Curve proof: wide/narrow per plane actually changes table construction; report includes file:line evidence that `Cnr3ResponseCurveKind` selects the j*j versus j cosine formula.
11. Default-run output after parser patch is byte-identical to the committed interim build. This is the regression anchor: parser plumbing must not perturb the no-argument default path.
12. Documentation proof: parser-site comment and README agree on names, defaults, ranges, cnr2 mapping, and threshold-zero semantics.

## Suggested runtime smoke VPY calls

No-options default:

```python
clip = core.cnr3.CNR3(clip)
```

Explicit defaults:

```python
clip = core.cnr3.CNR3(
    clip,
    y_threshold=35,
    y_strength=192,
    u_threshold=47,
    u_strength=255,
    v_threshold=47,
    v_strength=255,
    y_curve="wide",
    u_curve="narrow",
    v_curve="narrow",
    scene_threshold=10.0,
    scene_chroma=False,
)
```

Non-default proof:

```python
clip = core.cnr3.CNR3(
    clip,
    y_threshold=12,
    y_strength=100,
    u_threshold=23,
    u_strength=200,
    v_threshold=34,
    v_strength=210,
    y_curve="narrow",
    u_curve="wide",
    v_curve="wide",
    scene_threshold=5.5,
    scene_chroma=True,
)
```

Expected non-default response-config shape:

```text
response_config: y=12/100/narrow u=23/200/wide v=34/210/wide scene_threshold=5.5 scene_chroma=true
```
