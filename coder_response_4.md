Thanks. I agree with the main direction of v4, but I think v4 needs one important reconciliation pass before it becomes the implementation scope.

The key issue is that v4 now contains the later W3X decision — descriptive CNR3 option names and three explicit curve parameters — but earlier sections still carry pre-W3X wording that says the option parser should add cnr2’s exact names: `mode`, `scdthr`, `ln`, `lm`, `un`, `um`, `vn`, `vm`. That is now contradictory. Please revise v4 into v5 so the whole document consistently reflects the decided CNR3 user-facing surface.

The implementation target should be the descriptive CNR3 names, not the terse cnr2 names:

```text
y_threshold      default 35       range 0..255       maps cnr2 ln
y_strength       default 192      range 0..255       maps cnr2 lm
u_threshold      default 47       range 0..255       maps cnr2 un
u_strength       default 255      range 0..255       maps cnr2 um
v_threshold      default 47       range 0..255       maps cnr2 vn
v_strength       default 255      range 0..255       maps cnr2 vm

y_curve          default "wide"       maps cnr2 mode[0] = 'o'
u_curve          default "narrow"     maps cnr2 mode[1] = 'x'
v_curve          default "narrow"     maps cnr2 mode[2] = 'x'

scene_threshold  default 10.0     range 0.0..100.0   maps cnr2 scdthr
scene_chroma     default false                         maps cnr2 sceneChroma
```

So this is no longer an “8 parameter” parser. It is 10 parameters if `scene_chroma` is deferred, or 11 parameters if `scene_chroma` is included in the base option parser. `mode` is not a CNR3 parser option under the W3X decision; it is replaced by `y_curve`, `u_curve`, and `v_curve`. The cnr2 `mode="oxx"` spelling belongs in the equivalence documentation only.

I agree with these v4 technical decisions and would keep them:

```text
1. threshold==0 must be special-cased safely. Never divide by n or n*n.
2. y_curve/u_curve/v_curve validation should be strict: exactly "wide" or "narrow".
3. 8-bit defaults and 8-bit table behaviour must match cnr2.
4. 9-16 bit behaviour should keep CNR3's established CMS07 native-depth round-to-nearest scaling unless an exact cnr2-emulation phase is separately scoped.
5. Curve wiring must be proved by file:line evidence.
6. scene_threshold should be exposed with default 10.0 and validation 0.0..100.0 inclusive, but scene math should not be changed inside the option-parser patch unless the change is explicitly scoped.
7. scene_chroma should be included in the base parser only if the chroma-inclusive detection path already exists and needs create-time plumbing only. If enabling it requires algorithmic scene-detection changes, split it to a rider patch.
8. response_config emission should print all resolved live values, including scene_threshold and scene_chroma when present.
```

I also want to strengthen the documentation requirement.

This is not just a README issue. The CNR2-to-CNR3 conversion table and the user-facing explanation of each parameter must also live in the relevant CNR3 code, at or immediately adjacent to the option-parsing site in `vapoursynth-Cnr3.cpp`.

Please do not reduce that code comment to a very short/cryptic table only. From experience, a future human maintainer needs clarity in the code at the point where the parameters are read and defaulted. The README can carry polished user documentation, but the code should still contain enough explanation that a maintainer can understand the parser, defaults, ranges, cnr2 mapping, and behavioural effect without having to search external documents.

The parser-site comment should include all of the following:

```text
1. Full cnr2 -> cnr3 name-equivalence table:
   - cnr2 name
   - cnr3 name
   - default
   - validation range
   - short meaning

2. Curve equivalence:
   - cnr2 mode="oxx" equals:
       y_curve="wide"
       u_curve="narrow"
       v_curve="narrow"
   - cnr2 'x' meant narrow.
   - cnr2 non-'x' behaved as wide.
   - CNR3 intentionally uses strict "wide" / "narrow" strings.

3. Per-parameter behavioural explanation:
   - y_threshold/u_threshold/v_threshold:
       threshold controls how large a frame-to-frame difference may still be treated as noise-like.
       raising threshold denoises more aggressively but increases risk of smearing/ghosting.
       lowering threshold is more conservative.
   - y_threshold special note:
       y_threshold does not filter luma output. CNR3 leaves Y unchanged; the Y response acts as a luma-change guard for chroma filtering.
       This is important because luma motion suppresses chroma blending through cross-plane gating.
   - y_strength/u_strength/v_strength:
       strength controls how hard the blend can pull toward the previous filtered chroma when the difference is within the threshold.
       threshold sets reach; strength sets peak pull.
   - y_curve/u_curve/v_curve:
       "wide" is the j*j cosine response; it stays strong near zero difference and falls sharply near the threshold.
       "narrow" is the j cosine response; it tapers more steadily and conservatively.
   - scene_threshold:
       scene reset sensitivity, mapped from cnr2 scdthr.
       lower means more sensitive / more resets.
       higher means fewer resets / greater risk of smoothing across missed cuts.
   - scene_chroma:
       default false for cnr2 parity.
       true includes chroma in scene-change detection and helps chroma-only cuts, colour lighting shifts, flashing stage lights, and similar material.

4. Safety/compatibility notes:
   - threshold==0 is accepted for cnr2 range compatibility but must be special-cased.
   - threshold==0 means only the centre diff==0 table entry gets the configured strength; all nonzero differences receive zero blend.
   - table construction must never divide by zero.
   - 8-bit behaviour is cnr2-compatible.
   - 9-16 bit behaviour follows CNR3 CMS07 native-depth round-to-nearest scaling unless separately scoped otherwise.

5. Maintainer warning:
   - defaults must remain operational cnr2-equivalent:
       y=35/192/wide
       u=47/255/narrow
       v=47/255/narrow
       scene_threshold=10.0
       scene_chroma=false
   - response_config must be emitted from the live resolved config so accidental default regressions are visible in logs.
```

I am explicitly asking for clarity in the code rather than a minimal condensed version. The prior K.1E.2 regression survived because the live defaults became proof placeholders and there was not enough local explanatory context beside the defaulting/parser path to make that obviously wrong. The fix should not repeat that pattern. The code comment should be long enough to protect future maintainers from reintroducing placeholder defaults, breaking cnr2 parity, misunderstanding Y as filtered output, or treating the curve strings as cosmetic flags.

That does not mean the comment should become design-history prose. It should be structured maintainer documentation: a table plus clear operational explanations. But it should be complete enough that someone editing the parser five years later can make a safe change from the source file alone.

For v5, please also update the proof gate so it matches the W3X surface and the code-documentation requirement:

```text
1. Debug+Release x64 build.
2. Canonical 4-way selftest; count unchanged unless deliberately justified.
3. Source proof that all new CNR3 descriptive options are parsed from create-time input:
   y_threshold, y_strength, u_threshold, u_strength, v_threshold, v_strength,
   y_curve, u_curve, v_curve, scene_threshold, and optionally scene_chroma.
4. Default .vpy with no options:
   response_config identical to the committed interim defaults, plus scene fields if emitted.
5. Explicit-defaults .vpy:
   response_config identical to no-options run.
6. Non-default .vpy:
   response_config reflects supplied values.
7. Invalid-option tests:
   - scene_threshold < 0 and > 100 throw clear errors.
   - threshold/strength < 0 and > 255 throw clear errors.
   - y_curve/u_curve/v_curve values other than exactly "wide" or "narrow" throw clear errors.
8. threshold==0 tests:
   - y_threshold=0, u_threshold=0, v_threshold=0 do not crash or produce NaN.
   - centre-only table behaviour is verified.
9. Curve proof:
   - wide/narrow per plane actually changes table construction.
   - patch report includes file:line evidence that Cnr3ResponseCurveKind selects the j*j vs j cosine formula.
10. Default-output proof:
   - default-run output after parser patch is byte-identical to the committed interim build.
   - This proves adding parser plumbing did not perturb the no-argument default path.
11. Documentation proof:
   - parser-site comment block contains the full cnr2->cnr3 equivalence table.
   - parser-site comment block contains clear per-parameter behavioural explanations.
   - README contains the user-facing documentation.
   - parser-site comment and README agree on names, defaults, ranges, cnr2 mapping, and threshold-zero semantics.
12. response_config proof:
   - emitted from live resolved config, not constants.
   - includes the resolved threshold/strength/curve values and scene_threshold/scene_chroma if those are now part of the live option surface.
```

In short: I agree with v4’s corrected technical direction, but please issue v5 with the old cnr2-exact-name wording removed or clearly marked as reference-only, not implementation target. The implementation target is descriptive CNR3 options plus mandatory, clear, in-code maintainer documentation at the parser site.
