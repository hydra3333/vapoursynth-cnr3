# CNR3 vs CNR2 Option Mapping Scope Review for Designer

## Context

This note reviews `CNR3 vs cnr2 - option/default mapping + option-parsing spec - v1` and the uploaded AviSynth-vsCnr2 source inspection, with the specific goal of making the next CNR3 option-parsing scope precise before implementation.

Overall assessment:

```text
APPROVE WITH REQUIRED CLARIFICATIONS
```

The proposed direction is correct: CNR3 should target the CNR2/vsCnr2 user surface before implementing option parsing. The committed interim defaults already match the CNR2 defaults, and the next patch should expose those parameters rather than hardcoding them.

However, several edge cases and compatibility decisions should be made explicit before coding, especially the threshold-zero behaviour.

---

## Confirmed CNR2 surface

CNR2 exposes the following call surface:

```text
vsCnr2(clip, string mode, float scdthr,
       int ln, int lm, int un, int um, int vn, int vm,
       bool sceneChroma)
```

Confirmed defaults:

```text
mode        = "oxx"
scdthr      = 10.0
ln / lm     = 35 / 192
un / um     = 47 / 255
vn / vm     = 47 / 255
sceneChroma = false
```

Interpretation:

```text
ln/un/vn = per-plane threshold/sensitivity
lm/um/vm = per-plane strength
mode[0]  = Y response curve
mode[1]  = U response curve
mode[2]  = V response curve
```

The committed CNR3 interim defaults match this surface:

```text
Y: threshold 35, strength 192, curve wide
U: threshold 47, strength 255, curve narrow
V: threshold 47, strength 255, curve narrow
```

So the default fix was correctly aligned with CNR2.

---

## Scope direction: approved

The proposed CNR3 option mapping is broadly correct:

| CNR2 option | CNR3 target | Recommended action |
|---|---|---|
| `mode` | `y/u/v.curve` | Parse into per-plane curve enums. |
| `scdthr` | scene reset threshold | Parse float, default 10.0, validate 0.0..100.0 inclusive. |
| `ln` | `y.threshold_8bit` | Parse int. |
| `lm` | `y.strength_8bit` | Parse int. |
| `un` | `u.threshold_8bit` | Parse int. |
| `um` | `u.strength_8bit` | Parse int. |
| `vn` | `v.threshold_8bit` | Parse int. |
| `vm` | `v.strength_8bit` | Parse int. |
| `sceneChroma` | chroma-inclusive scene detection | Expose bool if plumbing is already available; default false. |

The naming recommendation is also correct: use the terse CNR2 option names for compatibility and user muscle-memory, while documenting them descriptively in CNR3 documentation.

Recommended parser surface:

```text
mode
scdthr
ln
lm
un
um
vn
vm
sceneChroma
```

---

## Required amendment 1: threshold-zero policy

This is the main missing item.

CNR2 accepts threshold parameters `ln`, `un`, and `vn` in the range `0..255`, but the CNR2 table formula divides by the threshold value:

```text
wide:   table[j] = m/2 * (1 + cos(j*j*pi / (n*n)))
narrow: table[j] = m/2 * (1 + cos(j*pi / n))
```

If `n == 0`, that formula has a divide-by-zero or NaN path.

This should not be copied blindly into CNR3.

### Recommendation

CNR3 should accept `ln/un/vn = 0` for CNR2 API compatibility, but handle it explicitly and safely.

Recommended deterministic rule:

```text
If threshold == 0:
  - Do not evaluate the cosine formula.
  - Leave all table entries zero except the centre entry for diff == 0.
  - Set the centre diff==0 entry to the configured strength.
```

Rationale:

```text
- Preserves the CNR2 user-facing range 0..255.
- Avoids undefined/NaN behaviour.
- Gives sensible semantics: only exact same-value samples can retain previous chroma/luma.
- Any nonzero difference receives zero blend, which is the natural limiting behaviour for a zero threshold.
- For exact current==previous samples, centre strength is harmless because current and previous are identical.
```

Alternative policy:

```text
Reject threshold 0 and validate ln/un/vn as 1..255.
```

This is safer in a narrow engineering sense, but it breaks CNR2 range compatibility. Since CNR3 is positioned as a CNR2 successor, the preferred policy is to accept zero and special-case it.

### Scope text to add

```text
Threshold-zero policy: ln/un/vn accept 0 for CNR2 API compatibility, but CNR3 must special-case threshold==0 in table construction and must not evaluate the cosine formula with denominator zero. Recommended behaviour: only the centre diff==0 entry receives the configured strength; all nonzero diffs remain zero.
```

---

## Required amendment 2: mode parsing compatibility

The scope says `mode` is a 3-character string using `o` and `x`. That is the documented surface, but actual CNR2 behaviour appears more permissive.

Observed CNR2-style behaviour:

```text
- mode must provide at least three characters.
- only the first three characters are used.
- char == 'x' means narrow.
- any other character acts as wide.
```

So `o` is the intended/documented wide marker, but in source semantics, non-`x` behaves as wide.

### Recommendation

Use the compatibility policy:

```text
mode length must be >= 3.
Only the first three characters are used.
For each of the first three chars:
  'x' => narrow
  anything else => wide
```

Document recommended user values as `o` and `x`, but implement the source-compatible semantics.

Alternative strict policy:

```text
mode length must be exactly 3.
Each char must be 'o' or 'x'.
```

This is cleaner but less compatible. It is not recommended if CNR3 aims to behave as a CNR2 successor.

---

## Required amendment 3: high-bit-depth scaling policy

CNR2 scales 8-bit threshold/strength values for higher bit depths. CNR3 already has a CMS07 native-depth arithmetic policy based on round-to-nearest scaling and 64-bit-safe composition.

The scope should explicitly state whether CNR3 intends:

```text
A. CNR2-exact high-bit-depth integer scaling, or
B. CNR3 CMS07 native-depth round-to-nearest scaling.
```

### Recommendation

Keep CNR3's established CMS07 native-depth round-to-nearest scaling unless exact CNR2 high-bit-depth emulation is explicitly scoped.

Scope text to add:

```text
8-bit defaults and 8-bit table behaviour must match CNR2. For 9..16-bit operation, CNR3 continues to use its CMS07 native-depth round-to-nearest scaling policy unless a separate compatibility-emulation phase is explicitly scoped.
```

This avoids accidental high-bit-depth drift being mistaken for a bug later.

---

## Required amendment 4: curve wiring proof

The scope correctly flags this open item: parsing `mode` is only useful if `Cnr3ResponseCurveKind::wide/narrow` actually selects the intended table formula.

Required proof for the implementation patch:

```text
Cnr3ResponseCurveKind::wide/narrow is consumed by the response-table build path and selects the wide j*j formula vs the narrow j formula.
```

If this is not already wired, wiring it must be part of the option-parsing patch. Otherwise, `mode` parsing would be a silent no-op.

Expected report form:

```text
Curve wiring confirmed: config.*.curve is consumed in the response-table build path and controls the wide_response / narrow_response formula selection.
```

Include file:line evidence in the coder report.

---

## Required amendment 5: scdthr semantics reconciliation

The scope correctly says that CNR3's scene-reset threshold should be reconciled against CNR2's `scdthr` formula rather than assumed.

CNR2-style semantics:

```text
scdthr: 0.0..100.0 inclusive
sceneChroma=false by default
scene change when accumulated difference exceeds threshold
```

The CNR3 parser should expose `scdthr` now, but the patch report must state whether the underlying CNR3 threshold formula is:

```text
- CNR2-exact, or
- CMS07-equivalent with native-depth rounding, or
- intentionally divergent.
```

### Recommendation

```text
Expose scdthr with default 10.0 and validation 0.0..100.0 inclusive.
Do not change scene math inside the option parser patch unless a mismatch is found and explicitly scoped.
```

If there is a mismatch, document it and either:

```text
A. adjust CNR3 to CNR2-compatible semantics in the same patch, if small and safe; or
B. defer scene-threshold reconciliation to a separate patch.
```

---

## sceneChroma decision

The scope correctly identifies `sceneChroma` as the user-facing option related to residual chroma-only cut or motion cases.

CNR2 default:

```text
sceneChroma = false
```

CNR3 should preserve that default for compatibility.

### Recommendation

Include `sceneChroma` in the option parser if the chroma-inclusive scene-config path already exists and implementation only requires create-time plumbing.

If enabling it requires algorithmic changes beyond passing a bool into existing config construction, split it into a rider patch.

Scope wording:

```text
sceneChroma: expose bool, default false for CNR2 parity. When true, pass through to the existing chroma-inclusive scene-detection path if available. If not already wired, defer to a separate sceneChroma wiring patch rather than mixing algorithmic scene changes into the base option-parser patch.
```

---

## Validation recommendations for the option-parsing patch

Minimum patch proof:

```text
1. Build Debug x64 and Release x64.
2. Run 4-way selftest.
3. Run grep/source proof that all new options are parsed from create-time input.
4. Run default VPY with no options and confirm response_config remains:
   y=35/192/wide u=47/255/narrow v=47/255/narrow
5. Run explicit-default VPY and confirm identical response_config:
   mode="oxx", scdthr=10.0, ln=35, lm=192, un=47, um=255, vn=47, vm=255, sceneChroma=false
6. Run non-default VPY and confirm response_config reflects supplied values.
7. Run invalid option tests:
   - scdthr below 0
   - scdthr above 100
   - threshold/strength below 0
   - threshold/strength above 255
   - mode too short
8. Run threshold-zero proof:
   - ln=0 or un=0 or vn=0 must not crash, NaN, or produce undefined table values.
9. Run mode proof:
   - mode="oxx" -> Y wide, U/V narrow
   - mode="xxx" -> all narrow
   - mode="ooo" -> all wide
   - optional compatibility proof: non-'x' char maps to wide.
```

Recommended log/report additions:

```text
response_config: y=<ln>/<lm>/<wide|narrow> u=<un>/<um>/<wide|narrow> v=<vn>/<vm>/<wide|narrow> scdthr=<value> sceneChroma=<true|false>
```

The current `response_config` line should be extended to include `scdthr` and `sceneChroma`, because those become user-visible resolved config values.

---

## Final recommendation

Proceed with the option/default mapping as the basis for the next implementation scope, but amend it before coding.

Required amendments:

```text
1. Add explicit threshold-zero policy for ln/un/vn.
2. Choose source-compatible mode parsing: length >= 3, first three chars used, 'x' narrow, anything else wide.
3. State high-bit-depth scaling policy: CNR3 CMS07 native round-to-nearest unless exact CNR2 emulation is separately scoped.
4. Require proof that Cnr3ResponseCurveKind actually selects the j*j vs j table formula.
5. Require scdthr formula reconciliation statement.
6. Decide whether sceneChroma is included in the base option parser or split as a rider, depending on current CNR3 wiring.
```

Recommended conclusion:

```text
The proposed CNR2-compatible option surface is correct and should be implemented. The main correction is that CNR2's documented 0..255 threshold range includes zero, while the table formula divides by the threshold. CNR3 should preserve the range for compatibility but special-case threshold==0 safely instead of copying undefined CNR2 behaviour.
```
