# CNR3 vs cnr2 — option/default mapping + option-parsing spec — v1

Source surveyed: AviSynth-vsCnr2 (Asd-g port of dubhater's VapourSynth Cnr2), src/vsCnr2.cpp + README.
This is the reference surface cnr3's option parser should target. cnr3's committed interim defaults were
verified against this and MATCH exactly.

## cnr2 complete option surface (authoritative)

AviSynth signature:
`vsCnr2(clip, string "mode", float "scdthr", int "ln", int "lm", int "un", int "um", int "vn", int "vm", bool "sceneChroma")`

| Option | Type | Default | Range / rule | Meaning |
|---|---|---|---|---|
| mode | string | `"oxx"` | 3 chars, each 'o'(wide) or 'x'(narrow); positions = Y,U,V | per-plane response curve shape |
| scdthr | float | 10.0 | 0.0–100.0 inclusive (else throw) | scene-change threshold, % of max possible frame chroma change |
| ln | int | 35 | 0–255 (else throw) | Y sensitivity to movement (denoise more = higher) |
| lm | int | 192 | 0–255 | Y denoise strength |
| un | int | 47 | 0–255 | U sensitivity |
| um | int | 255 | 0–255 | U strength |
| vn | int | 47 | 0–255 | V sensitivity |
| vm | int | 255 | 0–255 | V strength |
| sceneChroma | bool | false | — | include chroma in scene-change detection |

Notes from the code:
- **n = threshold ("sensitivity"), m = strength.** So cnr2's (ln,lm) = cnr3's (y.threshold_8bit, y.strength_8bit).
  Our interim defaults (Y 35/192, U/V 47/255) EXACTLY match cnr2 (ln=35,lm=192,un=vn=47,um=vm=255). Confirmed correct.
- **mode 'o' vs 'x' changes the TABLE FORMULA, not just a flag:**
  - wide ('o'):   `table[j] = m/2 * (1 + cos(j*j * PI / (n*n)))`   (j squared -> stays high near 0 longer, falls off sharply near +/-n)
  - narrow ('x'): `table[j] = m/2 * (1 + cos(j   * PI /  n   ))`   (linear-in-j cosine -> gentler, broader taper)
  This is the SAME cosine family cnr3 uses; cnr3's Cnr3ResponseCurveKind::wide/narrow must map to these two
  formulas. **OPEN ITEM (flagged in the fix review): confirm cnr3's curve enum actually selects the j*j vs j
  formula in the table build — if the enum is stored-but-unconsumed, mode would be a no-op.** This is the thing
  to verify when wiring option parsing.
- **scdthr math:** `diff_max = scdthr * width * height * max_pixel_diff / 100 << (depth-8)`, where max_pixel_diff
  = 219 (luma-only) or 219+2*224 >> subsampling (if sceneChroma). Per-frame `diff_total` accumulates |diff_y|
  (+ |diff_u|+|diff_v| if sceneChroma); if diff_total > diff_max the frame is a scene change -> the recursive
  chain resets (returns -1, frame passed through). This is cnr2's scene-change behaviour = cnr3's checkpoint/reset.
- **sceneChroma=false by default** — luma-only scene detection. THIS is the knob behind cnr3's residual
  desaturation on hard chroma-only cuts: with it false, a chroma-only cut isn't detected and the blend runs
  across it. cnr2 ships it false too, so cnr3 matching that default is correct; exposing the option lets users
  turn it on for chroma-heavy content (the 3/957 interlaced residual frames).
- **blend weight formula (identical to cnr3):**
  `weight_u = table_y[diff_y] * table_u[diff_u]` (luma gates chroma!), then
  `dst_u = (weight_u * prev_u + (shift - weight_u) * cur_u + round) >> shift2`. cnr3's blend matches.
  Note the cross-plane gating: the Y table multiplies the U/V weight, so luma movement suppresses chroma
  blending — confirm cnr3 preserves this (it appeared to in the frame_processing diff).

## cnr3 current state (post interim fix)

- Has internal per-plane config: threshold_8bit, strength_8bit, curve (Cnr3ResponseCurveKind::wide/narrow).
- Committed defaults now = cnr2 defaults (35/192 Y wide, 47/255 U/V narrow) -> "oxx".
- Has scdthr concept (scene reset / checkpoints) but exposure/semantics need checking against cnr2's formula.
- **NO user option parsing** — no mapGetInt/Float/Data for any of these; all hardcoded.
- sceneChroma equivalent: cnr3's scene detection is luma-based (matches cnr2 default); chroma-inclusive
  detection is the parked "scene_chroma policy" question = cnr2's sceneChroma=true path.

## MAPPING for the option-parsing patch

| cnr2 option | cnr3 target | status | action |
|---|---|---|---|
| mode "oxx" | y/u/v.curve enum | field exists | parse 3-char string -> 3 curve enums; VERIFY enum drives j*j vs j formula |
| scdthr 10.0 | scene-reset threshold | concept exists | parse float 0–100; confirm cnr3's reset uses same diff_max formula or document divergence |
| ln 35 | y.threshold_8bit | exists, default matches | parse int 0–255 |
| lm 192 | y.strength_8bit | exists, default matches | parse int 0–255 |
| un 47 | u.threshold_8bit | exists, default matches | parse int 0–255 |
| um 255 | u.strength_8bit | exists, default matches | parse int 0–255 |
| vn 47 | v.threshold_8bit | exists, default matches | parse int 0–255 |
| vm 255 | v.strength_8bit | exists, default matches | parse int 0–255 |
| sceneChroma false | chroma-inclusive scene detect | parked policy | expose as bool; ties to scene_chroma follow-up + the residual |

## Recommended sequencing

1. **Option-parsing patch** (the core follow-up): add the 8 params (mode, scdthr, ln/lm/un/um/vn/vm) with
   cnr2's exact names, defaults, and 0–255 / 0–100 validation-throw. Keep the `response_config:` emission
   updated to print the resolved values. This gives cnr3 parity with cnr2's user surface.
   - PRE-REQUISITE within this patch: resolve the curve-wiring open item (does Cnr3ResponseCurveKind actually
     select j*j vs j?). If not wired, wire it — otherwise `mode` parsing is a silent no-op.
2. **sceneChroma exposure** (can be same patch or a rider): expose the bool; default false = cnr2 parity.
   This is ALSO the fix path for the residual desaturation on chroma-only cuts (interlaced 178/643/759,
   music-video 74/76/77) — those users set sceneChroma=true.
3. **scdthr semantics reconciliation:** confirm cnr3's scene-reset uses cnr2's diff_max formula (or document
   why it differs). The interim already reports scdthr=10.0 in the analyser, so the concept is present.

## Naming decision for W3X

cnr2 uses terse names (ln/lm/un/um/vn/vm). Options:
(a) MATCH cnr2 exactly — best for user muscle-memory / drop-in replacement (recommended for a cnr2 successor).
(b) More descriptive (y_threshold/y_strength/...) — clearer but breaks drop-in compatibility.
Recommend (a) with the descriptive names documented, since cnr3 is positioned as the cnr2 successor for the
same VHS-restoration workflow.
