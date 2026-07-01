# CNR3 — Lever 3: typed-row-pointer pixel path — INVESTIGATE, ASSESS, then propose a SUB-STEP scope

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigate-and-assess brief. This is the BIG lever and it touches the most proven code
(P.7A-P.11B), so we are NOT scoping a single patch yet. Read the analysis + the vec-report evidence,
verify against source, assess the sub-staging and the risks, and report back with your recommended FIRST
sub-step and its approach. A concrete patch scope for that first sub-step follows your report.
**Target file:** `cnr3_frame_processing.cpp`. **Companion:** data-flow map v0.2.
**Controlling docs:** CMS07.15 (no design change — implementation optimisation only), FI-10.
**Builds on:** committed AVX2 build + Lever 0A (luma passthrough, -28%) + Lever 0B (staging cleanup).

---

## 1. Goal

Eliminate the native<->scalar MARSHALLING that dominates the remaining hot path: instead of unpacking
native pixel planes into `std::vector<int>` buffers, processing, and repacking, process DIRECTLY through
typed native pointers (`const uint8_t*` / `const uint16_t*` with stride), computing in the existing
wide accumulator and writing straight to native output. This removes the copies AND — per the evidence
below — is what lets the loops finally auto-vectorise under the `/arch:AVX2` we already committed.

## 2. The evidence: vec-report CONFIRMS the wall (this is the whole justification)

We ran `/Qvec-report:2` on Release `/O2 /arch:AVX2`, target `cnr3_frame_processing.cpp`. EVERY hot loop
reported **"loop not vectorized"**. The reason codes are decisive:

```text
cnr3_copy_native_plane_to_scalar_buffer (the UNPACK, ~24% self via load_native_plane_sample):
    reason 506  (loop body has a non-vectorizable operation — the per-sample FUNCTION CALL)
    reason 501  (induction/indexing the vectorizer can't reason about — the byte-offset arithmetic)
cnr3_downsample_luma_plane_to_chroma_grid:
    reason 506 + 1505/1106 (per-sample call + nested-loop structure)
cnr3_process_chroma_plane_from_downsampled_luma (the BLEND):
    reason 506 + 1505/1106 (per-sample calls: table lookup + store; plus structure)
our own staging helpers (0A/0B): also 506/501 (per-sample cnr3_store_native_plane_sample call)
```

Reading: **nothing vectorises, uniformly because a per-sample FUNCTION CALL sits in every loop body
(506) plus opaque per-sample indexing (501).** This is exactly the wall Lever 3 removes — inline the
per-sample load/store/compute into a flat typed loop over contiguous memory and the vectorizer can
finally act. The committed AVX2 is doing NOTHING on these loops today because of that call. (The
`std::vector::resize/assign` 1301/1200 messages are STL-internal allocation loops, NOT our targets —
ignore them.)

**This report is the BEFORE image.** Lever 3's proof gate re-runs the exact same `/Qvec-report:2` and
requires the target loops to flip from "not vectorized / 506" to "vectorized". "Written to vectorise" is
NOT acceptance; the report flipping + the profile dropping is.

## 3. The hot path after 0A/0B (what Lever 3 targets, from the post-0B profile)

```text
cnr3_load_native_plane_sample        ~16,546 self  <- the per-sample UNPACK (biggest leaf)
  inside cnr3_copy_native_plane_to_scalar_buffer (chroma planes + downsample input)
cnr3_downsample_luma_plane_to_chroma_grid  ~3,980 self
cnr3_process_chroma_plane_from_downsampled_luma ~2,429 self  <- the actual blend
staging now cleaned (0A/0B); cache manager <3%; denoise math still small.
```

So the residual ~50% is overwhelmingly the UNPACK side (native->int), plus the downsample and the blend.

## 4. Data-flow constraints Lever 3 MUST respect (from the map; verify from source)

- **The widen-on-load rule (the one correctness invariant):** read through the typed native pointer,
  WIDEN IMMEDIATELY to the EXISTING accumulator width (int / int64 as the current code uses), compute
  wide, narrow/clamp ONLY at the native store. Computing in the narrow (sample) type is the corrupting
  mistake the P.3A/P.5A int64 selftests catch. The arithmetic is UNCHANGED; only where the number is
  read from/written to changes.
- **Two bit-depth paths:** 8-bit = 1 byte (`const uint8_t*`), 9-16-bit = 2 byte LE (`const uint16_t*`).
  Template/branch on sample type; both must reproduce the exact results P.8A pins.
- **Edge/stride discipline:** the downsample clamps taps at plane edges (P.4A); padding is not active
  width (P.6A); byte offsets use `x * storage_bytes`. Typed traversal must clamp/stride identically.
- **The luma GUIDE stays intact:** the downsample of source luma into the chroma-grid guide (drives the
  blend's response-table lookup) is real math — Lever 3 changes HOW it reads (typed pointer) but not
  WHAT it computes.

## 5. Proposed SUB-STAGING (the key structural recommendation — assess this)

The vec-report shows the loops are NOT equal in difficulty. We propose Lever 3 be done in sub-steps,
easiest/highest-confidence first, each its own patch + proof + profile + re-vec-report:

```text
Lever 3a  UNPACK: replace cnr3_copy_native_plane_to_scalar_buffer's per-sample load with an inlined
          typed-row-pointer read (widen-on-load into the existing int buffer, OR directly feed the
          consumer). Simple arithmetic over contiguous memory -> HIGH confidence it vectorises once the
          per-sample call is gone. Biggest single leaf (~24% self). Likely the largest Lever-3 win.

Lever 3b  DOWNSAMPLE: typed-row-pointer traversal for cnr3_downsample_luma_plane_to_chroma_grid, taps
          read via typed pointer, (a+b+c+d+2)>>2 inline, edge clamp preserved. Medium confidence.

Lever 3c  BLEND (the HARD one): cnr3_process_chroma_plane_from_downsampled_luma. Flagged by the coder
          and CONFIRMED by the vec-report as the difficult case: it has a DATA-DEPENDENT response-table
          lookup (table[diff]) = a GATHER, which MSVC auto-vectorisation handles poorly or not at all,
          even after the per-sample call is inlined. So 3c may get a marshalling/copy win (removing the
          buffers) WITHOUT a vectorisation win, and may be the point where Path B (explicit VCL SIMD
          with a manual gather, integer types, no -ffast-math) is eventually needed. LOWEST confidence
          on auto-vec; scope and measure it on its own, with lowered expectations.
```

**We lean: do 3a first.** Highest confidence, biggest leaf, and it re-proves the "does inlining actually
flip the vec-report" question on the safest of the three before touching the blend. 3b next, 3c last and
separately. Do NOT attempt all three in one patch (measurement + bisection + risk all argue against it,
same as we kept 0A/0B separate).

## 6. What we're asking you to do (report, then we scope the first sub-step)

1. **Verify** the widen-on-load points, the two bit-depth paths, and the edge/stride discipline from the
   current source (post-0A/0B). Confirm the accumulator widths the arithmetic currently uses so the typed
   path preserves them exactly.
2. **Assess the sub-staging (3a/3b/3c) and our lean to 3a-first.** Is the unpack cleanly separable from
   its consumers, or is it so fused with the downsample/blend that 3a and 3b naturally come together?
   Tell us if the source suggests a different cut.
3. **Assess the blend-gather risk (3c).** From source: is the response-table lookup the gather we think
   it is? Do you foresee auto-vec cracking it after inlining, or is Path B likely required for 3c? We are
   NOT deciding Path B now — just want your early read so expectations are set.
4. **Recommend the FIRST sub-step and its approach** — we expect 3a (typed unpack), but say so in your
   terms, including whether 3a should feed the existing int buffer (smaller change, keeps downstream
   identical) or go fully buffer-free to its consumer (bigger change, more win, more risk).
5. **Flag any correctness risk** relative to P.3A/P.4A/P.5A/P.6A/P.8A/P.11B — especially any place the
   typed path could differ from the int-buffer path in rounding, clamping, or overflow.

## 7. Proof gate (will apply to EACH sub-step; the scope will restate)

```text
- Build Debug + Release (both projects), dev-trace ON, /arch:AVX2 on.
- Four-way selftest 56/56 (Debug normal, Release normal, forced-fail 55/56 exit 1, verbose 56/56).
- The relevant P-series proofs pass UNCHANGED (value-identity): P.8A native access, P.3A/P.5A blend +
  int64 accumulator, P.4A downsample rounding + edge clamp, P.6A traversal, P.11B composition. Any
  assertion edit needed = RED FLAG (result must be bit-identical, per profile and bit depth).
- Re-run /Qvec-report:2 on the target loop: it must FLIP from "not vectorized / 506" to "vectorized"
  (or, for 3c, report honestly what it does — a copy win without a vec win is a valid but explicitly
  noted outcome).
- Re-profile vs the post-0B baseline (67,891 samples; profiler_test_01, -r 1, dev-trace OFF). Report
  total change + the target leaf's self-time change + confirmation the int buffer/copy left the hot path.
- Value-preserving or it is wrong. No CMS/invariant change.
```

## 8. Hard constraints

```text
DO:
  - Preserve the exact arithmetic and accumulator widths (widen-on-load; narrow/clamp only at store).
  - Keep the two bit-depth paths correct (8-bit 1 byte, 16-bit 2 byte LE) per P.8A.
  - Preserve edge clamping and stride/padding discipline per P.4A/P.6A.
  - One sub-step per patch (3a, then 3b, then 3c) — separately proven, profiled, and vec-reported.

DO NOT:
  - Change any pixel value, rounding, clamp, or overflow behaviour (P-series selftests decide this).
  - Bundle the sub-steps.
  - Change the luma GUIDE computation (typed read is fine; the math is fixed).
  - Touch the cache manager, scene-change semantics, or the all-or-nothing commit discipline.
  - Introduce Path B / explicit SIMD / VCL in this arc without a separate decision (3c may PROMPT that
    decision later, but this brief does not authorise it).
  - Change CMS design or any invariant.
```

## 9. Note on expectations

3a is expected to be the real Lever-3 win (biggest leaf, high vectorisation confidence). 3b moderate.
3c may be copy-win-only if auto-vec can't crack the gather. Report measured results honestly per
sub-step; do not tune to a number. The cumulative Lever-3 target from FI-10 was ~1.5-2x on this path —
that is the DESTINATION across 3a+3b+3c, not a per-step promise.
