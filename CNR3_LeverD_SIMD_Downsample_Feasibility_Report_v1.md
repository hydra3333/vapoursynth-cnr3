# CNR3 Lever D SIMD Downsample Feasibility Report v1

## Scope

This is a no-code feasibility report for Lever D: exact SIMD downsample of the `(a + b + c + d + 2) >> 2` luma tap-average path after C1, F/3c, and Staging.

The investigation checks whether there is a target that is both hot and safely auto-vectorisable without explicit SIMD intrinsics.

## Verdict

**PATH-B-ONLY for the hot production path.**

The scalar downsample primitive is no longer the production hot path, so SIMD work there is a **NON-WIN**. The production path uses `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`, and the available vectorisation evidence shows the compute loops remain blocked. A P.4A-exact interior/edge split is conceptually possible, but making it likely to auto-vectorise cleanly would require enough specialisation by storage width, subsampling, and edge shape that it is no longer a small safe Lever D cleanup. For a ~1.3k leaf at the post-Staging ~-79% point, this does not clear the value/safety bar.

Recommended action: **do not patch Lever D now.** Leave the scalar P.4A/P.7A primitive untouched. If exact SIMD is ever revisited, treat it as Path B / explicit-intrinsics or explicit specialised-kernel work, not as this arc's small auto-vectorisation cleanup.

## Q1 — Which function is the hot downsample leaf?

### Finding

The production frame-triplet path calls the C1 native fused downsample twice:

- current source luma -> current downsampled luma
- previous filtered luma -> previous downsampled luma

It does not call `cnr3_downsample_luma_plane_to_chroma_grid` in the production frame-triplet path.

The scalar function still exists as the P.4A/P.7A-tested primitive, but source inspection shows the production calls go to `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`.

### Assessment

- SIMD-ing `cnr3_downsample_luma_plane_to_chroma_grid`: **NON-WIN** for current production performance.
- Relevant hot target: `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`.

## Q2 — Can the hot native function auto-vectorise?

### Finding

The `/Qvec-report:2` evidence for the native downsample function shows the relevant loops not vectorised, with reason `506` on the loop sites and additional `1106` blockers. One loop reported vectorised, but this appears to be the 16-bit native source validation/pre-scan or another simple loop, not the tap-average compute loop that dominates the downsample leaf.

The native compute loop contains:

- dynamic tap coordinates: `x0 = x << sub_sampling_w`, `y0 = y << sub_sampling_h`
- asymmetric horizontal clamp: `x1 = (x0 < source_width - 1) ? x0 + 1 : x0`
- vertical edge clamp: `y1 = (y0 + sub_sampling_h < source_height) ? y0 + sub_sampling_h : y0`
- storage-width split: 8-bit direct byte reads, 16-bit unaligned-safe `memcpy` reads
- exact average: `(tl + tr + bl + br + 2) >> 2`

### Assessment

The hot native tap-average loop is **not currently auto-vectorising**. The reason is not merely a trivially removable validation branch; the tap indexing and edge rules make the loop shape data-dependent/generic enough that the compiler does not turn it into a clean vector loop.

## Q3 — Is there a clean P.4A-exact reshape that auto-vectorises without intrinsics?

### Finding

An interior/edge split can preserve value identity in principle:

- handle the interior columns where `x1 = x0 + 1` statically;
- handle the last-column edge case separately;
- similarly separate ordinary rows from the bottom-row clamp where useful;
- preserve the exact `(a + b + c + d + 2) >> 2` arithmetic;
- preserve the asymmetric 4:4:4/4:4:0 horizontal-neighbour rule.

However, the actual native function is generic over storage width and subsampling. For the compiler to have a high chance of auto-vectorising the hot compute loop, the code would likely need substantial specialisation:

- 8-bit versus 16-bit kernels;
- sub_sampling_w == 0 versus 1;
- sub_sampling_h == 0 versus 1;
- interior rows/columns versus edge tails;
- 16-bit unaligned native loads expressed in a vector-friendly form.

This starts to look like an explicit kernel family, not a small hygienic reshape.

### Assessment

A value-exact interior/edge split is **theoretically possible**, but a clean, low-risk, auto-vectorising patch is not established. The likely practical path to a meaningful SIMD win is explicit specialisation or intrinsics, which is Path B territory.

## Q4 — If only explicit SIMD/intrinsics would get the win, should D be pursued?

### Finding

At this point in the arc, the downsample leaf is roughly ~1.3k samples. The total has already fallen from ~93.9k to ~19.5k. The remaining downsample cost is visible but no longer a headline target.

### Assessment

No. Explicit SIMD/intrinsics or a broad specialised-kernel family is disproportionate for this leaf during the current marshalling arc. This is a valid place to stop D.

## Final recommendation

Do **not** scope a Lever D patch in this arc.

Reasons:

1. The scalar downsample primitive is bypassed by production and should not be optimised for the current hot path.
2. The hot native downsample loop does not auto-vectorise in its current exact shape.
3. A P.4A-exact auto-vectorising reshuffle would require enough storage/subsampling/edge specialisation that it is no longer a small safe cleanup.
4. Path B explicit SIMD/intrinsics is disproportionate for a ~1.3k leaf after a ~79% cumulative reduction.

Status: **D closed as PATH-B-ONLY / skip for now.**
