# CNR3 PATCH SCOPE — Lever C1: direct native-luma → scalar chroma-grid downsample (buffer elimination)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code, P.4A/P.9A territory). Implement exactly
this; propose back for review before commit. **Lever C1 ALONE.**
**Target:** `cnr3_frame_processing.cpp`, `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`.
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2 + A-lite. Cumulative ~-59% (93,914 -> ~38,750).

---

## 1. Goal + why this is the top lever now

Post-A-lite, the single biggest leaf is `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`
(~9.2-10.5k samples). Its cost is dominated by an INTERMEDIATE it does not need: for YUV420P8 it
allocates a **full-resolution `source_luma_scalar` int buffer** (`resize` ~1.5k) and fills it via
`cnr3_copy_native_plane_to_scalar_buffer` (~4.7k) — a full-res byte->int expansion — purely to feed
`cnr3_downsample_luma_plane_to_chroma_grid`, which reads only a QUARTER-area chroma grid out of it. This
runs TWICE per output frame (current source luma + previous filtered luma). C1 eliminates the full-res
int buffer by reading native luma bytes DIRECTLY at the downsample tap coordinates and producing the
chroma-grid output in one pass. This is the tractable, narrow subcase of buffer-elimination the 8-bit
hot-path review identified as C1 (NOT the broad "remove all int buffers" version). Precedent: Lever 0A
removed an analogous full-res luma round-trip on the OUTPUT side for -28%.

## 2. Current shape (verified from source) and the fusion

```text
NOW (two-stage, full-res intermediate):
  cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(native_luma, ssw, ssh, out_grid, summary):
    1. resize source_luma_scalar to full-res (width*height)              [~1.5k resize]
    2. cnr3_copy_native_plane_to_scalar_buffer(native_luma -> source_luma_scalar)  [~4.7k copy]
    3. cnr3_downsample_luma_plane_to_chroma_grid(source_luma_scalar -> out_grid)   [the tap-average]

C1 (fused, no full-res intermediate):
  cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(native_luma, ssw, ssh, out_grid, summary):
    - validate storage_bytes + views + expected out-grid dims (as the inner function does today)
    - for each output chroma-grid cell (x,y):
        read the FOUR native luma taps DIRECTLY from native_luma bytes at the SAME coordinates
        cnr3_downsample_luma_plane_to_chroma_grid computes today, widen each tap to int,
        compute the EXACT same (tl+tr+bl+br+2)>>2, write out_grid[x,y].
```

## 3. THE EXACT BEHAVIOUR TO REPRODUCE (P.4A — do not deviate)

The tap geometry and rounding are the load-bearing invariant. Reproduce EXACTLY what
`cnr3_downsample_luma_plane_to_chroma_grid` does today (this is the 3b.1-proven form):

```text
For output chroma-grid cell (x, y):
  x0 = x << sub_sampling_w
  y0 = y << sub_sampling_h
  x1 = (x0 + 1 < luma_width)  ? x0 + 1        : x0     <-- ASYMMETRIC: +1 REGARDLESS of ssw
  y1 = (y0 + sub_sampling_h < luma_height) ? y0 + sub_sampling_h : y0
  tl = luma[x0,y0]; tr = luma[x1,y0]; bl = luma[x0,y1]; br = luma[x1,y1]
  out[x,y] = (tl + tr + bl + br + 2) >> 2
```

CRITICAL: `x1 = x0 + 1` when available REGARDLESS of `sub_sampling_w` (so 4:4:4 reads horizontal
NEIGHBOURS, not four identical taps) — this is the 3b.1 asymmetric-clamp subtlety; carry its safety
comment. The edge clamps (x1->x0, y1->y0 at the right/bottom edge) must match. The `+2` round-to-nearest
and `>>2` must be bit-identical. All four subsampling shapes (4:2:0/4:2:2/4:4:0/4:4:4) must reproduce.

## 4. Native tap read (the new part) — bit-depth + validation policy

```text
- storage_bytes from bits_per_sample (as today). 8-bit: read uint8 tap; 16-bit: unaligned-safe memcpy
  of uint16 (per 3a.1/3a.2 decision), widen to int.
- Tap byte offset uses the native stride: luma_byte[ y*stride_bytes + x*storage_bytes ] (the x*storage_bytes
  rule P.8A/P.10A prove). Do NOT assume width==stride.
- VALIDATION POLICY (Tier-1 defend at source): the taps are read from a raw VS SOURCE plane. This is the
  SAME provenance as the unpack's Tier-1 gate. So C1 MUST still detect out-of-range native luma samples
  (VHS glitch defence) — do NOT silently drop the guarantee. Options, in priority order:
    (a) PREFERRED: keep the existing Tier-1 validation by validating the native luma plane ONCE up front
        (the same hoisted pre-pass shape as 3a.2: 8-bit type-guaranteed; 16-bit branch-free OR-accumulate
        scan -> reject before producing any output), THEN read taps unchecked in the fused loop.
    (b) If (a) is impractical, keep a per-tap range check — correctness first; we can hoist later.
  Either way: invalid source luma -> return invalid_argument, NO partial out_grid publish (P.9A no-partial).
- This is Tier-1, NOT Tier-2 removal: we are eliminating the full-res COPY/BUFFER, not the source-boundary
  guarantee. The guarantee stays; only the full-res int materialisation goes.
```

## 5. Hard constraints (do / do not)

```text
DO:
  - Eliminate the full-res source_luma_scalar vector + its resize + the cnr3_copy_native_plane_to_scalar_buffer
    call inside this bridge function.
  - Reproduce P.4A tap geometry, asymmetric x1, edge clamps, (a+b+c+d+2)>>2 EXACTLY.
  - Keep the Tier-1 source-luma range guarantee (validate native luma; reject-before-publish; no partial).
  - Keep out-grid dimension validation (expected chroma dims from luma dims + subsampling) and the summary
    contract identical.
  - Keep 8-bit and 16-bit correct; unaligned-safe 16-bit reads.
  - Keep it a clean row/tap-pointer form so it does not regress (and may auto-vectorise, bonus not required).

DO NOT:
  - Change cnr3_downsample_luma_plane_to_chroma_grid's SIGNATURE or its behaviour — it remains the
    scalar-plane path used by selftests (P.4A/P.7A) and any other caller. C1 adds a native-direct path in
    the BRIDGE function; it does not gut the shared scalar helper. (If the cleanest implementation factors
    the tap-average into a small shared inline used by BOTH, that's fine — but the scalar helper's public
    contract and its selftest behaviour must not change.)
  - Weaken the Tier-1 source guarantee, any shared proof helper, or the final output clamp.
  - Touch the blend (3c), pooling (B), scene-change, or the 16-bit-specific SIMD question.
  - Change CMS design or any invariant.
  - Bundle anything else — C1 alone for measurement clarity.
```

## 6. Correctness argument

Old: native luma -> (copy every sample to full-res int) -> downsample reads 4 taps per grid cell from the
int buffer -> (a+b+c+d+2)>>2. New: downsample reads the SAME 4 taps directly from native luma bytes (same
coordinates, same widen-to-int), same (a+b+c+d+2)>>2. Every output grid value is computed from the identical
four source samples with the identical arithmetic -> bit-identical out_grid. The only removed work is the
full-res materialisation of samples that were going to be read anyway (and the ~3/4 of them that were never
read at all). Tier-1: invalid source still rejected before publish. So value-identity holds on all inputs;
P.4A/P.7A/P.9A are the arbiters.

## 7. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2.
2. Four-way selftest, dev-trace ON: Debug 56/56; Release 56/56; forced-fail 55/56 exit 1; verbose 56/56.
3. Value-identity — the KEY gates for C1:
     P.4A (downsample tap geometry + rounding + all 4 subsampling shapes + edge clamps) UNCHANGED,
     P.9A (native luma downsample bridge composes; invalid-late-sample publishes no partial plane) UNCHANGED,
     P.7A (source-luma downsample traversal) UNCHANGED,
     P.8A/P.11B unchanged.
   Any P.4A/P.9A assertion edit = RED FLAG (means the fused path diverged from the proven average).
   [Specifically confirm P.9A's invalid-late-native-sample case still returns invalid_argument, no partial.]
4. (Optional) /Qvec-report:2 on the bridge — record if the fused tap loop vectorises (bonus, not required).
   Revert the .vcxproj flags before commit if you toggle them (you are carrying them dirty currently).
5. Profile vs post-A-lite ~38,750 baseline, 2-3 runs (noise band ~+/-1,300). Report: total; and
   cnr3_downsample_native_luma_plane_to_scalar_chroma_grid total (did the ~4.7k copy + ~1.5k resize
   disappear from the bridge?). THIS is the big leaf — a real drop is expected (0A precedent).
6. Value-preserving or it is wrong.
```

## 8. Expected result (calibration)

The bridge runs twice per frame and the eliminated copy+resize is ~6k combined per invocation in the
profile's attribution. Removing the full-res materialisation should show a MATERIAL total drop (unlike the
small-leaf levers) — 0A did exactly this class of thing for -28% on the output side. Read absolute samples,
2-3 runs vs ~38,750. If it comes back flat, something is still materialising the buffer — investigate before
commit. Honest either way.

## 9. Out of scope

3b.2 (Tier-2 removals), F/3c (blend), B (pooling), D (exact SIMD downsample), E (scene-change), the
16-bit SIMD question, any change to the scalar `cnr3_downsample_luma_plane_to_chroma_grid` public contract,
any validation-policy or CMS change.
