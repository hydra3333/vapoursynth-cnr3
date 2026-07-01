# CNR3 — Lever 3b: downsample tap-average — INVESTIGATE, ASSESS, then propose a scope

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigate-and-assess brief. Read the analysis, verify against source, assess the approach
and the risks, report back with your recommended approach + first scope. A patch scope follows your
report. **This is Lever 3b ALONE** — 3a.2 (range-check hoist, deferred), 3c (blend), buffer-free
traversal, Path B/SIMD are OUT OF SCOPE.
**Target:** `cnr3_frame_processing.cpp`, `cnr3_downsample_luma_plane_to_chroma_grid` (~L2286).
**Controlling docs:** CMS07.15 (no design change), FI-10. Companion: map v0.2 (its buffer-1 line is
stale post-0A; rest holds).
**Builds on:** committed AVX2 + 0A + 0B + **3a.1 (typed unpack, -37%, cumulative -54% vs pre-lever)**.

---

## 1. Arc context + honest framing (IMPORTANT — read before assessing)

3a.1 removed the per-sample `cnr3_load_native_plane_sample` CALL from the unpack and bought -37% total
— NOT from vectorisation (the loop still reports 506, blocked by the per-sample range-check branch) but
from eliminating the per-sample function-call overhead itself. That result reframes what we expect from
3b: **the primary win is likely CALL ELIMINATION again, with vectorisation a bonus that the validation
branches may still block.** Set expectations accordingly.

**A key distinction from 3a.1 (please confirm from source):** by the time
`cnr3_downsample_luma_plane_to_chroma_grid` runs, the source luma is ALREADY a SCALAR int plane
(produced by the 3a.1-optimised unpack). Its taps are read via `cnr3_plane_sample_at` = `view.samples[(y
* view.stride) + x]` — a plain int-array index, NOT a native-byte load. So 3b is **scalar->scalar
tap-averaging, not native->scalar unpacking.** There is no bit-depth/endian byte concern here (that was
3a.1's world); the samples are already widened ints. This makes 3b's per-sample MATH simpler but its
INDEXING (four taps with edge clamping) the interesting part.

## 2. Current structure (the target hot loop)

`cnr3_downsample_luma_plane_to_chroma_grid` (~L2286): after validating views + deriving expected output
dims + allocating `resolved_outputs` (int), the hot loop is `for y / for x` over the OUTPUT (chroma-grid)
dimensions, and per output sample calls THREE things:

```text
1. cnr3_downsample_luma_tap_coordinates(x, y, luma_w, luma_h, ssw, ssh, taps)
     -> computes tap positions x0=x<<ssw, y0=y<<ssh, and x1/y1 = CLAMPED to the last
        column/row when the tap would fall off the plane edge (the edge-clamp geometry P.4A pins).
        Re-validates ssw/ssh in [0,1] and shifted-coordinate validity PER SAMPLE.
2. four cnr3_plane_sample_at(source_luma_plane, {x0|x1},{y0|y1})  -> four scalar int reads.
3. cnr3_downsample_luma_sample(tl,tr,bl,br, bits, out)
     -> re-derives sample_peak, RANGE-CHECKS all four taps in [0,sample_peak], then
        out = (tl+tr+bl+br+2) >> 2  (rounded average).
```

Then a second loop publishes `resolved_outputs` -> `output_downsampled_luma_plane` (as in the unpack
pattern). So the per-output-sample cost is: a tap-coordinate CALL (with per-sample re-validation +
edge-clamp branch), four scalar reads, and an average CALL (with per-sample sample_peak re-derivation +
four range checks). Three calls + re-derived invariants per sample = the 506 wall, same disease as 3a.1.

## 3. Proposed approach (assess this)

Same medicine as 3a.1: **inline the per-sample calls and hoist the invariants**, keeping the existing
`resolved_outputs` buffer and the publish loop, preserving the contract and the exact arithmetic.

- Hoist ONCE: `sample_peak` (from bits_per_sample), `sub_sampling_w/h` validity (loop-invariant), the
  source luma `samples` base + `stride`, the output dims.
- Inline the tap-coordinate math: `x0 = x<<ssw; y0 = y<<ssh; x1 = min(x0+? , luma_width-1)` etc. —
  reproduce the EXACT edge-clamp rule from `cnr3_downsample_luma_tap_coordinates` (confirm the precise
  clamp: which tap clamps, to what, for 4:2:0 / 4:2:2 / 4:4:4 / 4:4:0 — P.4A is the arbiter).
- Inline the four scalar reads (`samples[y0*stride+x0]` etc.).
- Inline the average: range-check the four taps, then `(tl+tr+bl+br+2)>>2`. WIDEN is already int; the
  `+2 >> 2` rounding MUST be reproduced bit-exactly.
- Write `resolved_outputs[...]`; leave the publish loop unchanged.

**The interior-vs-edge split (the key structural question):** the edge clamp only affects the LAST
output column/row (where a tap would fall off). The vast INTERIOR has x1=x0+stride-step, y1=y0+..., no
clamp. So the loop MAY vectorise better if the interior (clamp-free, regular stride) is separated from
the edge tail (clamped, scalar). Assess whether splitting interior/edge is worth it for 3b, or whether
that is over-engineering for the first cut (we lean: do the straight inlined version FIRST, measure,
and only split interior/edge if the vec-report/ profile says the clamp branch is the blocker — same
"measure before the second refinement" discipline we used deferring 3a.2).

## 4. Data-flow / correctness constraints (verify from source)

- **The average is the math and it is FIXED:** `(tl+tr+bl+br+2)>>2`. Bit-exact reproduction; P.4A/P.7A
  decide. No changing rounding.
- **Edge clamp is load-bearing (P.4A):** taps that fall off the plane clamp to the last valid
  column/row. The inlined version must clamp IDENTICALLY. This is the single most likely place to
  introduce a value difference — treat it as the primary correctness risk.
- **Per-tap range validation:** the current code range-checks all four taps per sample. As in 3a.1, the
  redundancy/branch is what may block vectorisation, but the VALIDATION must be preserved (an
  out-of-range tap still returns invalid_argument, no partial output). Do NOT drop it; hoisting it for
  vectorisation is a later step (a "3b.2" analogous to 3a.2), NOT this patch.
- **4:x:x geometry:** sub_sampling_w/h in {0,1}; the shift `<<ss` and the expected-dimension mapping
  must be preserved exactly. Confirm the degenerate 4:4:4 (ss=0, taps collapse) and 4:4:0 / 4:2:2 cases.
- **Scalar domain:** taps read the scalar int plane (post-3a.1). No native-byte/endian concern here.

## 5. What we're asking

1. **Confirm the scalar-domain framing** (§1) — the downsample reads the already-unpacked scalar int
   luma, not native bytes. Correct us if any path still reads native here.
2. **Verify the exact edge-clamp rule** from `cnr3_downsample_luma_tap_coordinates` and the average from
   `cnr3_downsample_luma_sample`, so the inlined version reproduces both bit-exactly.
3. **Assess the approach (§3)** — inline the three calls + hoist invariants, keep `resolved_outputs` +
   publish loop. Is the tap-coordinate math cleanly inlinable, or is the clamp intricate enough that you
   recommend keeping `cnr3_downsample_luma_tap_coordinates` as a call and only inlining the reads +
   average? (Assess the cost/benefit — the tap call may be a meaningful share of the 506 wall.)
4. **Assess the interior/edge split (§3)** — worth it now, or defer to a "3b.2" after measuring?
   We lean defer; tell us if the source makes the split cheap enough to justify now.
5. **Recommend the first scope + its proof/measurement plan**, and flag any P.4A/P.7A/P.9A correctness
   risk — especially any clamp or rounding difference.

## 6. Proof gate (will apply to the scope)

```text
- Build Debug + Release (both projects), dev-trace ON, /arch:AVX2 on.
- Four-way selftest 56/56 (Debug normal, Release normal, forced-fail 55/56 exit 1, verbose 56/56).
- Value-identity UNCHANGED: P.4A (downsample rounding + edge clamp), P.7A (downsample traversal), P.9A
  (native luma downsample bridge), P.11B (composition). Any assertion edit = RED FLAG.
- Re-run /Qvec-report:2 on the downsample loop: report vectorized-or-reason-code (bonus, not gate —
  per-tap validation and/or edge clamp may still block a full flip; that is acceptable and routes a
  possible 3b.2).
- Re-profile vs the post-3a.1 baseline (42,748 samples; profiler_test_01, -r 1, dev-trace OFF). Report
  total change + cnr3_downsample_luma_plane_to_chroma_grid self-time + whether the per-sample tap/average
  CALLS left the hot path.
- Value-preserving or it is wrong. No CMS/invariant change.
```

## 7. Hard constraints

```text
DO:
  - Preserve the exact average (tl+tr+bl+br+2)>>2 and the edge-clamp rule bit-for-bit.
  - Hoist loop-invariants (sample_peak, ss validity, base/stride, dims); inline the per-sample calls.
  - Keep resolved_outputs + the publish loop + the function contract unchanged.
  - Keep per-tap range validation (invalid tap -> invalid_argument, no partial output).

DO NOT:
  - Change rounding, clamp geometry, or the 4:x:x dimension mapping.
  - Drop or restructure the per-tap validation (that is a later 3b.2, not this patch).
  - Go buffer-free / feed the blend directly (later).
  - Touch the blend (3c), the unpack (3a.*), scene-change, cache, or commit discipline.
  - Introduce Path B / explicit SIMD / VCL.
  - Change CMS design or any invariant.
```

## 8. Expected result (calibration)

The downsample chain is ~30% of frame time post-3a.1, with `cnr3_downsample_luma_plane_to_chroma_grid`
~3,868 self plus the tap/average call overhead. Removing three per-sample calls should be a real win
(as 3a.1's call-elimination was), even if the loop only partially vectorises. If it fully vectorises
(interior, clamp-free), larger. Report honestly; do not tune to a number. 3b.2 (validation/clamp
hoisting) and 3c (blend) carry the rest.
