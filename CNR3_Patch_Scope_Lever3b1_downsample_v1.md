# CNR3 PATCH SCOPE — Lever 3b.1: inlined scalar-domain downsample tap-average

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code). Implement exactly this; propose back
for review before commit. **Lever 3b.1 ALONE.** OUT OF SCOPE: interior/edge split (possible 3b.2),
per-tap validation hoist (possible 3b.2), 3a.2 (unpack range-check hoist), 3c (blend), buffer-free
traversal, removal of the native->scalar bridge, Path B/SIMD.
**Target:** `cnr3_frame_processing.cpp`, `cnr3_downsample_luma_plane_to_chroma_grid` (~L2286) — the
SCALAR-domain tap-average. (The bridge `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid` above it
is UNTOUCHED.)
**Controlling docs:** CMS07.15 (no design change), FI-10. Companion: map v0.2.
**Builds on:** committed AVX2 + 0A + 0B + 3a.1 (cumulative -54% vs pre-lever). Confirmed with coder:
straight call-elimination, no split, no validation hoist.

---

## 1. Goal

Remove the THREE per-output-sample calls (`cnr3_downsample_luma_tap_coordinates`, four
`cnr3_plane_sample_at`, `cnr3_downsample_luma_sample`) from the downsample hot loop by inlining them and
hoisting the loop-invariants — same medicine as 3a.1 (which bought -37% from call elimination alone).
Keep the existing `resolved_outputs` buffer, the publish loop, the function contract, and the EXACT
arithmetic + clamp geometry.

## 2. Domain (confirmed from source)

This function reads the source luma as an ALREADY-SCALAR int plane
(`source_luma_plane.samples[(y*stride)+x]` via `cnr3_plane_sample_at`) — NOT native bytes. There is NO
bit-depth/endian concern here (that was 3a.1). Samples are already widened ints. So 3b.1 is scalar->scalar
tap averaging; the interesting part is the INDEXING (tap coordinates + edge clamp), not the load.

## 3. THE EXACT CLAMP RULE — pin this verbatim (the single highest correctness risk)

From `cnr3_downsample_luma_tap_coordinates`, the tap coordinates are **ASYMMETRIC between x and y**:

```text
x0 = chroma_x << sub_sampling_w
y0 = chroma_y << sub_sampling_h
(validate: x0 < luma_width and y0 < luma_height, else invalid_argument)

x1 = (x0 < luma_width  - 1) ? (x0 + 1)              : x0      // ALWAYS x0+1 when available,
                                                              // REGARDLESS of sub_sampling_w
y1 = (y0 + sub_sampling_h < luma_height) ? (y0 + sub_sampling_h) : y0   // uses sub_sampling_h
```

**The asymmetry is load-bearing and counterintuitive:**
- Horizontal: `x1` is `x0 + 1` when in range **even when `sub_sampling_w == 0`** (4:4:4 / 4:4:0 still
  read the NEXT horizontal sample, then clamp at the last column).
- Vertical: `y1` is `y0 + sub_sampling_h`, so when `sub_sampling_h == 0` the second row IS the same row.

Resulting per-format tap geometry (MUST be reproduced exactly):
```text
4:2:0 (ssw=1,ssh=1): x0=2x, x1=min(2x+1,last_x); y0=2y, y1=min(2y+1,last_y)
4:2:2 (ssw=1,ssh=0): x0=2x, x1=min(2x+1,last_x); y0=y,  y1=y
4:4:0 (ssw=0,ssh=1): x0=x,  x1=min(x+1, last_x);  y0=2y, y1=min(2y+1,last_y)
4:4:4 (ssw=0,ssh=0): x0=x,  x1=min(x+1, last_x);  y0=y,  y1=y   <-- NOT four identical taps
```

**REQUIRED safety comment** at the inlined coordinate math, stating: `x1` intentionally uses `x0 + 1`
even when `sub_sampling_w == 0`, while `y1` uses `y0 + sub_sampling_h` — do not "symmetrise" this.

## 4. The average (fixed math)

After all four taps are range-checked in `[0, sample_peak]`:
```text
output = (tl + tr + bl + br + 2) >> 2
```
Reproduce bit-exactly. The `+2` rounding and `>>2` must not change. P.4A/P.7A decide.

## 5. What to change

Replace the hot loop's three per-sample calls with inlined math + hoisted invariants:

```text
Before the loops (hoist once):
  - sample_peak (from bits_per_sample, once)
  - rely on the existing expected-dimension derivation for sub_sampling_w/h validity (already computed)
  - source samples base pointer + source stride
  - source luma width/height, expected output width/height

Per output (y, x):
  - x0 = x << sub_sampling_w;  y0 = y << sub_sampling_h;
  - x1 = (x0 < luma_width  - 1) ? x0 + 1              : x0;
  - y1 = (y0 + sub_sampling_h < luma_height) ? y0 + sub_sampling_h : y0;
  - tl = samples[y0*stride + x0]; tr = samples[y0*stride + x1];
    bl = samples[y1*stride + x0]; br = samples[y1*stride + x1];
  - if any of tl,tr,bl,br outside [0, sample_peak]: return invalid_argument;   // keep the validation
  - resolved_outputs[y*expected_output_width + x] = (tl + tr + br + bl + 2) >> 2;

Leave the SECOND publish loop (resolved_outputs -> output plane) UNCHANGED.
```

(Note the initial `x0 >= luma_width || y0 >= luma_height` guard from the tap helper: preserve its effect
— those cases currently return invalid_argument. Confirm whether the expected-dimension derivation makes
`x0/y0` always in-range for valid dims; if so the guard is defensive, keep an equivalent.)

## 6. Hard constraints — the seven guards (do / do not)

```text
DO:
  1. Preserve the ASYMMETRIC x1/y1 rule EXACTLY (x1 = x0+1-when-available regardless of ssw; y1 = y0+ssh).
  2. Keep 4:4:4 reading horizontally-adjacent taps (x0, x0+1) — do NOT collapse to four identical taps.
  3. Keep (tl+tr+bl+br+2)>>2 exactly.
  4. Keep all four per-tap range checks; an out-of-range tap returns invalid_argument.
  5. Preserve no-partial-output: write only resolved_outputs in the compute loop; the publish loop
     (unchanged) is the sole writer to the output plane, so a mid-compute failure leaves output untouched.
  6. Preserve output padding by leaving the publish loop unchanged.
  7. This is scalar int domain — do NOT introduce any native-byte / endian handling here.
  + Add the REQUIRED safety comment (section 3) on the asymmetric clamp.

DO NOT:
  - Change rounding, clamp geometry, or the 4:x:x dimension mapping.
  - Split interior/edge (possible 3b.2, after measurement).
  - Drop or restructure the per-tap validation (possible 3b.2).
  - Keep cnr3_downsample_luma_tap_coordinates / cnr3_downsample_luma_sample as per-sample CALLS (inline
    them — they are the overhead being removed). Leave the helpers defined for any other callers.
  - Remove the native->scalar bridge or go buffer-free / feed blend directly.
  - Touch 3a.*, the blend (3c), scene-change, cache, or commit discipline. No CMS/invariant change.
```

## 7. Correctness argument

Old loop: per-sample tap-coordinate call (asymmetric clamp) + four scalar reads + average call (range
check + rounded average) -> resolved_outputs. New loop: identical clamp math inlined + identical four
reads + identical range check + identical `(sum+2)>>2` -> resolved_outputs. Same values, same
invalid-input behaviour, same layout; publish loop and contract untouched. Bit-identical output.

## 8. Vectorisation: REPORT, not a gate (per 3a.1 precedent)

Re-run `/Qvec-report:2` (Release /O2 /arch:AVX2). Expect the loop MAY still report a blocker (the per-tap
range-check branch and/or the clamp conditional), same pattern as 3a.1 where the 506 shifted from the
call to the validation branch. That is ACCEPTABLE — the primary win is call elimination (which the
profile shows regardless). A blocker here routes a possible 3b.2 (validation/clamp hoist, interior/edge
split). Record vectorized-or-reason-code either way.

## 9. Proof gate

```text
1. Build Debug + Release (both projects), dev-trace ON, /arch:AVX2 on.
2. Four-way selftest: Debug normal 56/56; Release normal 56/56; forced-fail 55/56 exit 1; verbose 56/56.
3. Value-identity UNCHANGED: P.4A (downsample rounding + edge clamp), P.7A (downsample traversal),
   P.9A (native luma downsample bridge), P.11B (composition). Any assertion edit = RED FLAG.
   [P.4A is the key gate — it pins the clamp geometry this patch reproduces.]
4. Re-run /Qvec-report:2 on the downsample loop; record vectorized-or-reason-code (section 8).
5. Re-profile vs post-3a.1 baseline (42,748 samples; profiler_test_01, -r 1, dev-trace OFF). Report:
   total change; cnr3_downsample_luma_plane_to_chroma_grid self-time; whether the per-sample tap/average
   CALLS left the hot path.
6. Value-preserving or it is wrong.
```

## 10. Expected result (calibration)

The downsample chain is ~30% of frame time post-3a.1 (`cnr3_downsample_luma_plane_to_chroma_grid` ~3,868
self plus tap/average call overhead). Removing three per-sample calls should be a real win (as 3a.1's
call-elimination was), even if the loop only partially vectorises. If negligible or negative, report it —
do not tune to a number. 3b.2 (validation/clamp hoist, interior/edge split) and 3c (blend) carry the rest.
