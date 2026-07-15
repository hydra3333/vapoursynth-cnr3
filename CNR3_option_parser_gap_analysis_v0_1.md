# CNR3 option-parser gap analysis against cnr2 option mapping/spec v6.1 — v0.1

## Source basis and caveat

Controlling scope: `CNR3_cnr2_option_mapping_and_spec_v6.1.md`.

Source inspected for this gap analysis:

- `src(32).zip`, locally reconstructed as the latest uploaded CNR3 source base, with `CMS07-FIX.operational-response-defaults_v2-src32.patch` applied to reflect the committed interim defaults.
- `src_vapoursynth-cnr3-dev_cache_manager.zip` was used only to recover files that `src(32).zip` references but does not contain: `cnr3_response_tables.cpp/.h` and `cnr3_frame_processing.cpp/.h`.

Important evidence caveat: line numbers for `vapoursynth-Cnr3.cpp`, `cnr3_arAllFramesReady.cpp`, `cnr3_plugin_internal.h`, and `cnr3_build_config.h` come from `src(32).zip` plus the committed operational-defaults patch. Line numbers for `cnr3_response_tables.*` and `cnr3_frame_processing.*` come from the recovered full-source zip. Before coding, confirm those four recovered files match the local repo.

## Executive result

Gap-analysis status: **complete enough to scope the parser patch, with a source-completeness caveat for response/frame-processing files.**

Patch shape indicated by this analysis:

```text
Add create-time parse + strict validation for 11 descriptive CNR3 options:
  y_threshold, y_strength, u_threshold, u_strength, v_threshold, v_strength,
  y_curve, u_curve, v_curve, scene_threshold, scene_chroma.

No response-table curve-wiring change appears necessary: curve is already consumed by table construction.
No scene-math change should be included in the parser patch.
scene_chroma can be included in the base parser: existing chroma-inclusive detection appears to be plumbing-only.
threshold==0 table behaviour already appears implemented safely; parser must preserve 0 as accepted.
```

## Global parser evidence

Current create-time parser state:

- `vapoursynth-Cnr3.cpp:736-753`: `cnr3_create_filter()` reads only the required `clip` with `mapGetNode(in, "clip", ...)`.
- `vapoursynth-Cnr3.cpp:869-872`: registration signature is only `"clip:vnode;" -> "clip:vnode;"`.
- Repository grep over the reconstructed source found no `mapGetInt`, `mapGetFloat`, `mapGetData`, or equivalent create-time option reads for the 11 target options.

Therefore parse status is **MISSING for all 11 options**.

## Per-parameter stage table

Legend:

- EXISTS: implemented in current source.
- MISSING: absent in current source.
- PARTIAL: internal machinery exists, but create-time parser behaviour required by v6.1 does not yet exist.

| Param | Parse (create-time mapGet*) | Default fallback when omitted | Validate (range/string + throw) | Apply / downstream consumption |
|---|---|---|---|---|
| `y_threshold` | MISSING. Only `clip` is parsed (`vapoursynth-Cnr3.cpp:736-753`; signature only clip at `869-872`). | EXISTS. Constants at `vapoursynth-Cnr3.cpp:364-367`; assigned at `413`. | MISSING at parser level. Internal table builder clamps scaled threshold and safely handles zero (`cnr3_response_tables.cpp:47-53`), but there is no user-facing range check/throw. | EXISTS. `table_config` is built at `vapoursynth-Cnr3.cpp:471-476`; threshold is scaled at `cnr3_response_tables.cpp:176-179`; Y plane table built at `203`. |
| `y_strength` | MISSING. Same global parser evidence. | EXISTS. Constants at `vapoursynth-Cnr3.cpp:364-367`; assigned at `414`. | MISSING at parser level. Internal table builder clamps strength at `cnr3_response_tables.cpp:47-48`; no user-facing range check/throw. | EXISTS. Strength is scaled at `cnr3_response_tables.cpp:180-183`; Y plane table built at `203`. |
| `u_threshold` | MISSING. Same global parser evidence. | EXISTS. Constants at `vapoursynth-Cnr3.cpp:366-367`; assigned at `417`. | MISSING at parser level. Internal zero-safe table behaviour exists (`cnr3_response_tables.cpp:47-53`); no user-facing range check/throw. | EXISTS. Threshold is scaled at `cnr3_response_tables.cpp:176-179`; U plane table built at `209`. |
| `u_strength` | MISSING. Same global parser evidence. | EXISTS. Constants at `vapoursynth-Cnr3.cpp:366-367`; assigned at `418`. | MISSING at parser level. Internal clamp only (`cnr3_response_tables.cpp:47-48`); no user-facing range check/throw. | EXISTS. Strength is scaled at `cnr3_response_tables.cpp:180-183`; U plane table built at `209`. |
| `v_threshold` | MISSING. Same global parser evidence. | EXISTS. Constants at `vapoursynth-Cnr3.cpp:366-367`; assigned at `421`. | MISSING at parser level. Internal zero-safe table behaviour exists (`cnr3_response_tables.cpp:47-53`); no user-facing range check/throw. | EXISTS. Threshold is scaled at `cnr3_response_tables.cpp:176-179`; V plane table built at `215`. |
| `v_strength` | MISSING. Same global parser evidence. | EXISTS. Constants at `vapoursynth-Cnr3.cpp:366-367`; assigned at `422`. | MISSING at parser level. Internal clamp only (`cnr3_response_tables.cpp:47-48`); no user-facing range check/throw. | EXISTS. Strength is scaled at `cnr3_response_tables.cpp:180-183`; V plane table built at `215`. |
| `y_curve` | MISSING. Same global parser evidence. | EXISTS. Assigned wide at `vapoursynth-Cnr3.cpp:415`. | MISSING at parser level. No create-time string validation. | EXISTS. Curve enum is converted to `wide_response` at `cnr3_response_tables.cpp:184-185`; wide/narrow formula selected at `66-80`; Y plane table built at `203`. |
| `u_curve` | MISSING. Same global parser evidence. | EXISTS. Assigned narrow at `vapoursynth-Cnr3.cpp:419`. | MISSING at parser level. No create-time string validation. | EXISTS. Same curve consumption path at `cnr3_response_tables.cpp:184-195`; U plane table built at `209`. |
| `v_curve` | MISSING. Same global parser evidence. | EXISTS. Assigned narrow at `vapoursynth-Cnr3.cpp:423`. | MISSING at parser level. No create-time string validation. | EXISTS. Same curve consumption path at `cnr3_response_tables.cpp:184-195`; V plane table built at `215`. |
| `scene_threshold` | MISSING. Same global parser evidence. | EXISTS. `CNR3_P11C_DEFAULT_SCDTHR = 10.0` at `cnr3_build_config.h:165-173`; stored in `Cnr3FilterData` at `cnr3_plugin_internal.h:60`; assigned at `vapoursynth-Cnr3.cpp:453`. | PARTIAL. Internal `cnr3_scdthr_is_valid()` accepts finite `0.0..100.0` at `cnr3_frame_processing.cpp:281-284`, and `cnr3_make_scene_change_config_from_vscnr2_scdthr()` rejects invalid at `1050-1057`; no user-facing parser error path exists yet. | EXISTS. Config built at `vapoursynth-Cnr3.cpp:455-463`; used in live frame processing at `cnr3_arAllFramesReady.cpp:1454-1464`, `2244-2254`, `2561-2571`; detector compares `diff_total > scene_change_threshold` at `cnr3_frame_processing.cpp:1001-1005`. |
| `scene_chroma` | MISSING. Same global parser evidence. | EXISTS as current hardcoded false. `Cnr3SceneChangeConfig` defaults false (`cnr3_frame_processing.h:293-295`), and create path passes literal `false` at `vapoursynth-Cnr3.cpp:455-463`. | MISSING at parser level. Bool parser/validation does not exist. | EXISTS as plumbing path. `cnr3_make_scene_change_config_from_vscnr2_scdthr()` accepts `bool scene_chroma` at `cnr3_frame_processing.cpp:1035-1043`; stores it at `1098-1100`; detector reads it at `900-907` and includes U/V diffs only when true at `967-989`. |

## Unknown U1 — curve wiring

Answer: **WIRED**, subject to the source-completeness caveat above.

Evidence chain:

- `vapoursynth-Cnr3.cpp:413-423` assigns default threshold/strength/curve fields: Y wide, U narrow, V narrow.
- `vapoursynth-Cnr3.cpp:471-476` passes the resolved `Cnr3ResponseTableConfig` to `build_cnr3_response_tables()`.
- `cnr3_response_tables.h:38-47` defines `Cnr3ResponseCurveKind` and stores it in `Cnr3ResponsePlaneConfig`.
- `cnr3_response_tables.cpp:184-185` consumes the enum: `plane_config.curve == Cnr3ResponseCurveKind::wide`.
- `cnr3_response_tables.cpp:66-80` selects wide vs narrow formula from the resulting `wide_response` bool:
  - wide: `abs_diff * abs_diff * pi / (threshold * threshold)`;
  - narrow: `abs_diff * pi / threshold`.
- `cnr3_response_tables.cpp:187-195` passes `wide_response` into `build_cnr3_weight_table()`.

Finding: `y_curve/u_curve/v_curve` parsing would not be a silent no-op if wired to the existing config fields. No curve-wiring code change appears necessary, though proof tests are still required.

## Unknown U2 — scene_threshold formula parity

Answer: **CMS07-equivalent with native round-to-nearest; not CNR2-exact for all depths.**

Evidence:

- `cnr3_frame_processing.cpp:1035-1043` declares `cnr3_make_scene_change_config_from_vscnr2_scdthr(...)`, including `scdthr`, full dimensions, bit depth, subsampling, and `scene_chroma`.
- `cnr3_frame_processing.cpp:1071-1073` computes `max_pixel_diff` as `219` luma-only, or `((219 + (224 * 2)) >> (sub_sampling_w + sub_sampling_h))` when `scene_chroma` is true.
- `cnr3_frame_processing.cpp:1081-1086` computes the 8-bit-domain base as `scdthr * full_width * full_height * max_pixel_diff / 100`.
- `cnr3_frame_processing.cpp:1088-1099` scales by `sample_peak / 255.0` and rounds with `llround()`.
- `cnr3_frame_processing.cpp:1075-1079` comments explicitly state this is a recorded improvement over vsCnr2's power-of-two depth factor and truncation.

Patch implication: expose `scene_threshold` and pass it into the existing function, but do not change scene math in the option-parser patch. The patch report should state this parity classification explicitly.

## Unknown U3 — scene_chroma plumbing

Answer: **PLUMBING-ONLY**, subject to local repo confirmation.

Evidence:

- `vapoursynth-Cnr3.cpp:455-463` already calls `cnr3_make_scene_change_config_from_vscnr2_scdthr(...)` and currently passes literal `false` for `scene_chroma`.
- `cnr3_frame_processing.cpp:1035-1043` already accepts `bool scene_chroma`.
- `cnr3_frame_processing.cpp:1098-1100` stores `scene_chroma` into `out_config`.
- `cnr3_frame_processing.cpp:900-907` copies `config.scene_chroma` into stats and a local bool.
- `cnr3_frame_processing.cpp:967-989` conditionally adds U/V scene diffs when `scene_chroma` is true.
- `cnr3_arAllFramesReady.cpp:1454-1464`, `2244-2254`, and `2561-2571` pass `data.scene_change_config` to the live pixel path.

Patch implication: include `scene_chroma` in the base parser. It appears to require only parsing, validation/defaulting, and passing the resolved bool instead of literal `false` into the existing scene-config constructor.

## Additional finding — threshold-zero policy is already implemented internally

The v6.1 spec requires threshold zero to be accepted and handled without division by zero. Current table construction already appears to implement this internally:

- `cnr3_response_tables.cpp:47-48` clamps threshold/strength to `[0, sample_peak]`.
- `cnr3_response_tables.cpp:50-53` special-cases `threshold == 0`: set only the centre table entry to `strength`, then return OK.
- The cosine calculation begins only after that guard (`cnr3_response_tables.cpp:55-80`).

Patch implication: the parser should allow `0` for y/u/v thresholds and should not add a stricter `1..255` validation. The proof gate should still include explicit threshold-zero tests because this becomes user-reachable.

## Final patch-shape statement

Recommended final patch shape from this gap analysis:

```text
Files likely touched:
  - src/vapoursynth-Cnr3.cpp
  - README / plugin documentation file if present in repo
  - selftest or harness files for parser/default/invalid-option proof, if available
  - possibly response-table selftest only for threshold-zero proof coverage

Implementation work:
  1. Extend the VapourSynth function registration signature from clip-only to include:
       y_threshold:int:opt;
       y_strength:int:opt;
       u_threshold:int:opt;
       u_strength:int:opt;
       v_threshold:int:opt;
       v_strength:int:opt;
       y_curve:data:opt;
       u_curve:data:opt;
       v_curve:data:opt;
       scene_threshold:float:opt;
       scene_chroma:int/bool:opt;  // exact VS API bool representation to confirm
  2. Add create-time parsing with defaults matching v6.1.
  3. Add strict user-facing validation and mapSetError messages:
       thresholds/strengths: 0..255 inclusive;
       curves: exactly "wide" or "narrow";
       scene_threshold: finite 0.0..100.0 inclusive;
       scene_chroma: bool.
  4. Replace hardcoded default response config construction with resolved user/default config.
  5. Replace hardcoded scene_threshold and literal scene_chroma=false with resolved values.
  6. Extend response_config emission to include scene_threshold and scene_chroma, emitted from live resolved config.
  7. Add the required full parser-site maintainer documentation block.
  8. Add/maintain README user documentation and equivalence table.
  9. Prove default no-option output is byte-identical to the committed interim build.

Not indicated by this gap analysis:
  - No scene-math change.
  - No response-table formula change.
  - No curve-wiring change, unless the local repo differs from the recovered response_tables source.
  - No change to cache/recovery/getFrame scheduling.
```

## Local confirmation commands for the coordinator/developer

Run these in the actual repo to close the source-completeness caveat:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github"

git grep -n "mapGet" -- src/vapoursynth-Cnr3.cpp
git grep -n "registerFunction" -- src/vapoursynth-Cnr3.cpp
git grep -n "Cnr3ResponseCurveKind" -- src
git grep -n "wide_response" -- src
git grep -n "threshold == 0" -- src/cnr3_response_tables.cpp
git grep -n "cnr3_make_scene_change_config_from_vscnr2_scdthr" -- src
git grep -n "scene_chroma" -- src/cnr3_frame_processing.cpp src/cnr3_frame_processing.h src/vapoursynth-Cnr3.cpp src/cnr3_plugin_internal.h
```
