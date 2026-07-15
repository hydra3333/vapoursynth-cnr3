# CNR3 vs cnr2 — option/default mapping + option-parsing spec — v5

(v5 = full reconciliation pass per coder response 4: cnr2-exact names are REFERENCE-ONLY everywhere; the
implementation target is the descriptive CNR3 surface below. Also adopts the coder's full in-code
documentation requirement (superseding W3D's 'condensed' wording) and the 12-item proof gate.
v4 incorporated the coder scope review: threshold-zero policy, bit-depth policy, curve-wiring proof form,
scdthr reconciliation rule, sceneChroma split rule, expanded proof list — reconciled with the W3X naming
decisions of v2/v3 which the coder review predates: descriptive names, three explicit curve params.)

Source surveyed: AviSynth-vsCnr2 (Asd-g port of dubhater's VapourSynth Cnr2), src/vsCnr2.cpp + README.
This is the reference surface cnr3's option parser should target. cnr3's committed interim defaults were
verified against this and MATCH exactly.

## IMPLEMENTATION TARGET — the CNR3 user-facing surface (authoritative for the parser patch)

Eleven parameters (ten if scene_chroma is deferred to a rider per split rule):

| CNR3 param | default | range / rule | maps cnr2 |
|---|---|---|---|
| y_threshold | 35 | 0-255 int, throw outside; ==0 special-cased | ln |
| y_strength | 192 | 0-255 int, throw outside | lm |
| u_threshold | 47 | 0-255 int; ==0 special-cased | un |
| u_strength | 255 | 0-255 int | um |
| v_threshold | 47 | 0-255 int; ==0 special-cased | vn |
| v_strength | 255 | 0-255 int | vm |
| y_curve | "wide" | exactly "wide" or "narrow", throw otherwise | mode[0]='o' |
| u_curve | "narrow" | exactly "wide" or "narrow" | mode[1]='x' |
| v_curve | "narrow" | exactly "wide" or "narrow" | mode[2]='x' |
| scene_threshold | 10.0 | 0.0-100.0 float inclusive, throw outside | scdthr |
| scene_chroma | false | bool (base parser IF plumbing-only, else rider) | sceneChroma |

`mode` is NOT a CNR3 option. cnr2 names appear in documentation/equivalence tables only.
All cnr2-name mentions in the sections below are REFERENCE MATERIAL describing cnr2, not the CNR3 surface.

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

(REFERENCE-ONLY mapping of cnr2 concepts to cnr3 internals — the parser implements the CNR3 surface at
the top of this document, NOT these cnr2 names.)

| cnr2 option (reference) | cnr3 internal field | status | note |
|---|---|---|---|
| mode "oxx" | y/u/v.curve enum | field exists | replaced by y_curve/u_curve/v_curve params; VERIFY enum drives j*j vs j formula |
| scdthr 10.0 | scene-reset threshold | concept exists | exposed as scene_threshold; confirm/report diff_max formula parity |
| ln/lm 35/192 | y.threshold_8bit / y.strength_8bit | exists, defaults match | exposed as y_threshold / y_strength |
| un/um 47/255 | u.* | exists, defaults match | exposed as u_threshold / u_strength |
| vn/vm 47/255 | v.* | exists, defaults match | exposed as v_threshold / v_strength |
| sceneChroma false | chroma-inclusive scene detect | parked policy | exposed as scene_chroma per split rule |

## Recommended sequencing

1. **Option-parsing patch** (the core follow-up): add the ELEVEN CNR3 descriptive params per the
   IMPLEMENTATION TARGET table at the top (y/u/v threshold+strength+curve, scene_threshold, scene_chroma
   per split rule), with cnr2-identical defaults and validation-throw ranges. Extend the `response_config:`
   emission
   updated to print the resolved values. This gives cnr3 parity with cnr2's user surface.
   - PRE-REQUISITE within this patch: resolve the curve-wiring open item (does Cnr3ResponseCurveKind actually
     select j*j vs j?). If not wired, wire it — otherwise `mode` parsing is a silent no-op.
2. **sceneChroma exposure** (can be same patch or a rider): expose the bool; default false = cnr2 parity.
   This is ALSO the fix path for the residual desaturation on chroma-only cuts (interlaced 178/643/759,
   music-video 74/76/77) — those users set sceneChroma=true.
3. **scdthr semantics reconciliation:** confirm cnr3's scene-reset uses cnr2's diff_max formula (or document
   why it differs). The interim already reports scdthr=10.0 in the analyser, so the concept is present.

## Naming decision (W3X DECIDED: option b — descriptive names)

W3X ruling: use descriptive names, NOT cnr2's terse ln/lm/un/um/vn/vm — conditional on documentation:
1. **A name-equivalence table is MANDATORY** in (a) the plugin README and (b) a comment block at the
   option-parsing site in code, mapping every cnr2 name to its cnr3 name with defaults. Migrating users
   rename once via the table; future readers get self-documenting scripts.
2. Proposed names (final naming to be confirmed at option-patch scope time):
   ln->y_threshold, lm->y_strength, un->u_threshold, um->u_strength, vn->v_threshold, vm->v_strength,
   scdthr->scene_threshold, sceneChroma->scene_chroma.
3. DECIDED (W3X): `mode` becomes THREE explicit per-plane params: y_curve / u_curve / v_curve, each
   "wide" | "narrow" (defaults: y_curve="wide", u_curve="narrow", v_curve="narrow" = cnr2's "oxx").
   The positional "oxx" string is retired; the equivalence table maps it.
4. Defaults and validation ranges remain cnr2-identical regardless of names (35/192, 47/255, 10.0, false;
   throw outside 0-255 / 0.0-100.0).
5. **W3X requirement: user-facing docs must explain options and their EFFECTS more clearly than cnr2's
   README did.** Draft text below — to be carried into the plugin README and refined at option-patch time.

## Coder-review amendments (ACCEPTED, reconciled with the naming decisions)

1. **Threshold-zero policy (the key catch).** cnr2 documents thresholds 0-255 but its table formula divides
   by n and n*n — at n==0 that is NaN/undefined behaviour cnr2 never guarded. CNR3 keeps the 0-255 range for
   API-range parity but MUST special-case threshold==0 in table construction: do not evaluate the cosine;
   set only the centre (diff==0) entry to the configured strength, all other entries zero. Semantics: at
   threshold 0, only exactly-identical samples retain previous chroma — the natural limit. Never divide by n.

2. **Curve param validation (supersedes the coder's mode-permissiveness question).** The coder proposed
   replicating cnr2's permissive mode parsing ('x'=narrow, ANY other char=wide, first 3 chars of >=3). With
   mode retired for y_curve/u_curve/v_curve strings, validation is STRICT instead: each must be exactly
   "wide" or "narrow", else throw. cnr2's permissive quirk is recorded in the equivalence table note only:
   (cnr2 mode: 'x' meant narrow; any other character behaved as wide).

3. **High-bit-depth scaling policy.** 8-bit defaults and 8-bit table behaviour must match cnr2 exactly.
   For 9-16-bit, CNR3 KEEPS its established CMS07 native-depth round-to-nearest scaling (NOT cnr2's
   <<(depth-8)) unless an exact-emulation phase is separately scoped. State this in the patch notes so
   high-bit-depth divergence is never mistaken for a bug.

4. **Curve-wiring proof (formalised).** The implementation patch report MUST include file:line evidence in
   the form: "Curve wiring confirmed: config.*.curve is consumed in the response-table build path and
   selects the wide (j*j) vs narrow (j) formula." If not currently wired, wiring it is IN SCOPE for the
   option patch (otherwise the curve params are a silent no-op).

5. **scdthr (scene_threshold) reconciliation.** Expose with default 10.0, validate 0.0-100.0 inclusive,
   throw outside. Do NOT change scene math inside the option-parser patch. The patch report must state
   whether CNR3's underlying reset formula is cnr2-exact (diff_max = scdthr*W*H*max_pixel_diff/100,
   max_pixel_diff=219 luma-only), CMS07-equivalent with native rounding, or intentionally divergent. If a
   mismatch is found: document it and defer reconciliation to its own patch unless trivially small.

6. **scene_chroma split rule.** Include in the base option parser ONLY if the chroma-inclusive detection
   path already exists and needs create-time plumbing alone. If enabling it requires algorithmic scene-
   detection changes, split to a rider patch — do not mix scene-math changes into the parser patch.

7. **Emission extension.** The response_config line gains the two new resolved values:
   `response_config: y=35/192/wide u=47/255/narrow v=47/255/narrow scene_threshold=10.0 scene_chroma=false`
   — printed from live config, as established.


8. **IN-CODE documentation is a deliverable — FULL, not condensed (coder requirement, W3D accepted,
   superseding the earlier 'condensed' wording).** At the option-parsing site in vapoursynth-Cnr3.cpp, a
   structured maintainer comment block MUST contain ALL of:
   (1) the full cnr2->cnr3 equivalence table: cnr2 name, cnr3 name, default, validation range, short meaning;
   (2) curve equivalence: mode="oxx" == y_curve wide / u_curve narrow / v_curve narrow; cnr2 'x' meant
       narrow and any non-'x' behaved as wide; CNR3 intentionally uses strict "wide"/"narrow" strings;
   (3) per-parameter behavioural explanations (thresholds: noise-vs-content boundary, raise=denoise more/
       risk ghosting; the y_threshold special note: Y is NOT filtered, the Y response is a luma-change
       guard gating chroma via cross-plane weighting; strengths: peak pull, threshold=reach/strength=peak;
       curves: wide=j*j cosine strong-near-zero/sharp-falloff, narrow=j cosine steady taper;
       scene_threshold: reset sensitivity, lower=more resets; scene_chroma: false=cnr2 parity, true helps
       chroma-only cuts / colour lighting shifts / flashing lights);
   (4) safety/compatibility notes: threshold==0 accepted but special-cased (centre-only strength, never
       divide by zero); 8-bit cnr2-compatible; 9-16-bit follows CMS07 round-to-nearest unless separately
       scoped;
   (5) maintainer warning: defaults must remain operational cnr2-equivalent (35/192/wide, 47/255/narrow,
       10.0, false) and response_config must be emitted from live resolved config so default regressions
       are visible in logs.
   Structured documentation, not design-history prose — but complete enough that a maintainer five years
   later can make a safe change from the source file alone. Rationale: K.1E.2's placeholder defaults
   survived because no local context beside the defaulting path made them obviously wrong.

## REQUIRED FIRST DELIVERABLE — gap analysis (before any patch)

Before scoping or writing the patch, the coder returns a GAP ANALYSIS against this spec, in this shape.
The later patch diff must map one-to-one onto the reported gaps; diff content with no corresponding gap
is a review finding.

### Per-parameter stage table (all 11 params)

For EACH of: y_threshold, y_strength, u_threshold, u_strength, v_threshold, v_strength, y_curve, u_curve,
v_curve, scene_threshold, scene_chroma — report each stage as EXISTS (file:line) / MISSING / PARTIAL:

| param | parse (create-time mapGet*) | default (fallback when omitted) | validate (range/string + throw) | apply (value consumed downstream, evidence) |

Expected shape (coder to confirm or correct with evidence, not assume):
- parse: MISSING for all 11 (only "clip" is read today).
- default: EXISTS as the committed hardcoded assignments (become the fallback path).
- validate: MISSING for all.
- apply: EXISTS+PROVEN for the six threshold/strength (the interim fix changed behaviour through these
  fields); UNKNOWN for the three curves; TBD for scene_threshold and scene_chroma (see unknowns below).

### The three unknowns (each needs a definitive file:line answer)

U1. CURVE WIRING: trace config.y/u/v.curve from assignment to the response-table build. Is
    Cnr3ResponseCurveKind consumed to select the wide (j*j) vs narrow (j) cosine formula? Answer one of:
    (a) WIRED — evidence file:line of the selection branch; or
    (b) INERT — the enum is stored but never read by table construction; wiring it is then IN SCOPE for
        the option patch. (Also note: if INERT, the committed interim "Y wide" default is not currently
        taking effect — state what curve the tables are ACTUALLY built with today.)

U2. SCENE_THRESHOLD FORMULA PARITY: locate cnr3's scene-reset threshold math and state whether it is
    cnr2-exact (diff_max = scdthr * W * H * max_pixel_diff / 100, max_pixel_diff=219 luma-only,
    <<(depth-8)), CMS07-equivalent with native rounding, or divergent — with file:line. NO scene-math
    changes in the parser patch; this is a parity STATEMENT.

U3. SCENE_CHROMA PLUMBING: does a chroma-inclusive scene-detection path already exist such that
    scene_chroma=true is create-time plumbing only (pass a bool)? Answer PLUMBING-ONLY (include in base
    patch) or NEEDS-ALGORITHM-WORK (split to rider patch), with file:line of where the bool would land.

### Output of the gap analysis

A short document: the stage table, the three unknown answers with evidence, and a resulting FINAL PATCH
SHAPE statement ("the patch adds: parse+validate for 11; curve wiring [yes/no]; scene_chroma [in/rider]").
W3D cold-checks the evidence, then the implementation scope is confirmed from it.

## Proof gate for the option-parsing patch (12 items, coder response 4 adopted)

1. Debug+Release x64 build.
2. Canonical 4-way selftest; count unchanged unless deliberately justified.
3. Source proof all CNR3 descriptive options parse from create-time input: y_threshold, y_strength,
   u_threshold, u_strength, v_threshold, v_strength, y_curve, u_curve, v_curve, scene_threshold,
   (scene_chroma if in base).
4. Default .vpy (no options): response_config identical to committed interim defaults (+scene fields if emitted).
5. Explicit-defaults .vpy: response_config identical to the no-options run.
6. Non-default .vpy: response_config reflects supplied values.
7. Invalid-option tests throw clear errors: scene_threshold <0 / >100; threshold/strength <0 / >255;
   curve strings other than exactly "wide"/"narrow".
8. threshold==0 tests: y/u/v_threshold=0 do not crash or NaN; centre-only table behaviour verified.
9. Curve proof: wide/narrow per plane actually changes table construction; report includes file:line
   evidence that Cnr3ResponseCurveKind selects the j*j vs j cosine formula.
10. Default-output proof: default-run output after the parser patch is byte-identical to the committed
    interim build (parser plumbing must not perturb the no-argument path).
11. Documentation proof: parser-site comment block contains the full equivalence table AND the full
    per-parameter behavioural explanations; README carries the user-facing docs; comment and README agree
    on names, defaults, ranges, cnr2 mapping, and threshold-zero semantics.
12. response_config proof: emitted from live resolved config, not constants; includes resolved
    threshold/strength/curve values and scene_threshold/scene_chroma where live.

## Draft user documentation (plain-English, for the future README)

CNR3 is a temporal chroma denoiser: it steadies colour (U/V) between frames while leaving brightness (Y)
untouched. For each pixel it asks "how much did this spot really change since the previous frame?" and,
where the change looks like noise rather than motion, gently pulls this frame's colour toward the previous
frame's already-cleaned colour. All options tune that one judgement.

**y_threshold / u_threshold / v_threshold** (0-255; defaults 35 / 47 / 47)
  How big a frame-to-frame difference each plane is willing to treat as "just noise". Differences LARGER
  than the threshold are treated as real content (motion, cuts) and passed through untouched; differences
  smaller are candidates for smoothing. Raise a threshold to denoise more aggressively (risking colour
  ghosting/smearing on moving objects); lower it to be more conservative. y_threshold is special: it does
  not filter luma — it uses LUMA change as a guard for the chroma decision (if brightness moved at that
  spot, something real happened, so leave the colour alone).

**y_strength / u_strength / v_strength** (0-255; defaults 192 / 255 / 255)
  Once a difference is judged noise-like, how HARD to pull toward the previous frame's colour. 255 = up to
  near-total reuse of the previous cleaned value for tiny differences; lower values blend more gently.
  Strength scales the peak of the response; threshold sets its reach.

**y_curve / u_curve / v_curve** ("wide" | "narrow"; defaults "wide" / "narrow" / "narrow")
  The SHAPE of the response between zero difference and the threshold. "wide" stays near full strength for
  small differences and falls off sharply near the threshold — more effective smoothing, slightly bolder.
  "narrow" tapers off steadily from the start — gentler, more cautious. Default gives luma a wide guard
  (small brightness flicker should not block chroma cleaning) and chroma narrow responses (treat colour
  changes conservatively).

**scene_threshold** (0.0-100.0; default 10.0)
  Scene-change detector sensitivity, as a percentage of the maximum possible whole-frame change. When a
  frame differs from the previous one by more than this, CNR3 declares a scene change and RESETS — the new
  frame passes through unfiltered and smoothing restarts from it. Lower = more sensitive (more resets,
  safer on rapid cutting); higher = fewer resets (better continuity, riskier across missed cuts).

**scene_chroma** (true/false; default false)
  Whether colour changes count toward scene-change detection, or brightness only (the default, matching
  cnr2). Leave false for typical footage. Set TRUE for material with colour-only transitions (lighting
  colour shifts, flashing stage lights, chroma-heavy cuts): it prevents the filter smoothing across a cut
  that brightness alone cannot see — the exact cause of rare colour-washing on such content.

(Name equivalence for cnr2 users: ln=y_threshold, lm=y_strength, un=u_threshold, um=u_strength,
vn=v_threshold, vm=v_strength, mode="oxx"=y_curve/u_curve/v_curve wide/narrow/narrow,
scdthr=scene_threshold, sceneChroma=scene_chroma. Defaults are identical to cnr2.)
