# CNR3 PATCH SCOPE — Lever F/3c: inline + hoist the chroma blend loop (top leaf)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code, RECURSIVE-ARITHMETIC path, highest
care). Implement exactly this; propose back for review before commit. **Lever F/3c ALONE.**
**Target:** `cnr3_frame_processing.cpp`, `cnr3_process_chroma_plane_from_downsampled_luma` (the ~6,640 top leaf).
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1, and the
CROSS-VERIFIED blend arithmetic (designer + coder independent derivations AGREED — section 3).
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2 + A-lite + C1 + Repack. Cumulative ~-70% (-> ~28,000).

---

## 1. Goal + why this is the top lever

`cnr3_process_chroma_plane_from_downsampled_luma` is the largest remaining leaf (~6,640 total, ~2,380
self -> ~4,260 in a per-sample CALL CHAIN). Per sample it calls `cnr3_blend_chroma_sample_from_response_tables`
(-> `cnr3_blend_chroma_sample`) plus four `cnr3_plane_sample_at` reads, writes an indexed
`resolved_outputs[(y*width)+x]`, then a SEPARATE per-sample `cnr3_write_plane_sample` publish loop. This is
the same low-self/high-total call-chain signature that 3a.1 (-37%), A-lite (-7.4%), and C1 (-24.5%) each
crushed by inlining + hoisting. F/3c applies that proven mechanism to the blend. The arithmetic is NOT
changed — it is reproduced BIT-EXACTLY from the cross-verified formula below.

## 2. What to change — STRUCTURAL only (inline + hoist + row-pointer)

```text
HOIST (compute once, outside both loops):
  - sample_peak (from bits_per_sample), shift2 = bits_per_sample<<1, shift = 1LL<<shift2, shift1 = shift>>1
  - response-table geometry / table_offset validation (do ONCE, not per sample)
  - base pointers + strides for all FOUR input planes + the output plane
  - y_response_table.data() / chroma_response_table.data() raw pointers (avoid per-sample vector indexing overhead)

FUSE conversion + publish into ONE row-pointer loop (removing resolved_outputs round-trip if safe, OR keep
it but row-pointer both loops — see section 5 note on the summary needing first/last):
  for each row y:
    const int* cur_luma_row  = ...; const int* prev_luma_row = ...;
    const int* cur_chr_row   = ...; const int* prev_chr_row  = ...;
    int* out_row = ...;
    for x in [0,width):
      inline the blend (section 3) directly: read the 4 samples via row pointers, compute, write out_row[x].
```

## 3. THE BLEND ARITHMETIC — reproduce BIT-EXACTLY (cross-verified; do NOT alter)

This formula was derived INDEPENDENTLY by the designer and the coder from source and AGREED on every term
(P.3A/P.5A are the arbiter). Reproduce it EXACTLY when inlining:

```text
luma_signed_diff   = current_downsampled_luma  - previous_downsampled_luma
chroma_signed_diff = current_source_chroma     - previous_filtered_chroma

y_response      = table lookup: get_cnr3_table_value_for_signed_diff(y_response_table, table_offset, luma_signed_diff)
chroma_response = table lookup: get_cnr3_table_value_for_signed_diff(chroma_response_table, table_offset, chroma_signed_diff)
                  (out-of-range signed diff -> table returns 0, per P.1A — preserve exactly)

weight  = (int64_t)y_response * (int64_t)chroma_response          // = cnr3_calculate_combined_blend_weight
shift2  = bits_per_sample << 1
shift   = 1LL << shift2
shift1  = shift >> 1

output  = (int64_t)(
              weight * (int64_t)previous_filtered_chroma
            + (shift - weight) * (int64_t)current_source_chroma
            + shift1
          ) >> shift2
```

CRITICAL invariants (any deviation = value corruption that COMPOUNDS through the temporal recursion):
- **int64 THROUGHOUT.** weight, both products, the sum, shift, shift1 are all 64-bit. Do NOT narrow to
  32-bit even for 8-bit input — this function is bit-depth-GENERIC and 32-bit overflows at 16-bit. (This
  is the exact trap an earlier external suggestion fell into; both independent derivations confirmed int64.)
- **Directionality:** `weight` multiplies PREVIOUS_FILTERED; `(shift - weight)` multiplies CURRENT_SOURCE.
  Do NOT swap. (P.3A proves the previous FILTERED output is used, not source[N-1].)
- **Rounding:** `+ shift1` before `>> shift2` (round-half-up). Preserve.
- **Table lookup semantics** (signed-diff indexing, out-of-range -> 0) reproduced exactly.

## 4. VALIDATION — production-private per policy (Tier-2/Tier-3), guarantee stated

Per CNR3_Validation_Policy_recorded_v1, the blend's per-sample checks are removable in a PRODUCTION-PRIVATE
path because:
```text
- The four INPUT samples (cur/prev downsampled luma, cur source chroma, prev filtered chroma) are Tier-2:
  they reached here THROUGH the Tier-1 source gate (unpack) / are CNR3-produced. Their range is guaranteed
  upstream. REMOVE the per-sample [0,sample_peak] input checks in the fused production loop.
- y_response / chroma_response are Tier-3: bounded [0,sample_peak] BY CONSTRUCTION (cnr3_response_tables.cpp
  clamps at build; out-of-range diff -> 0). REMOVE the per-sample response-range check.
- The response-table GEOMETRY / table_offset check stays but is HOISTED (done once, up front) — it is a
  provenance guard on the tables, cheap, and not per-sample.
INVARIANT RELIED ON (state it in a comment): no Tier-2 input reaches this loop bypassing the Tier-1 gate;
the response tables come from the sanctioned builder. If either ceases to hold, the checks must return.
The FINAL output is still bounded by the arithmetic (convex combination of in-range values) — but if the
current code clamps the output at store, PRESERVE that clamp.
```

## 5. Hard constraints (do / do not)

```text
DO:
  - Reproduce the section-3 arithmetic BIT-EXACTLY, int64 throughout, correct directionality + rounding.
  - Inline the per-sample call chain (cnr3_blend_chroma_sample_from_response_tables / cnr3_blend_chroma_sample
    / cnr3_plane_sample_at / cnr3_write_plane_sample) into a row-pointer fused loop.
  - Hoist all loop-invariants (section 2). Validate table geometry/offset ONCE up front; reject before publish.
  - __restrict on the genuinely non-aliasing plane pointers (4 const inputs + 1 output are distinct buffers).
  - Preserve the summary contract EXACTLY (width/height/samples_processed/first_output_sample/last_output_sample
    — first/last come from resolved_outputs.front()/back() today; if you fuse away resolved_outputs, capture
    first/last during the fused loop instead, same values). Mind the .front()/.back() on an empty vector is
    pre-existing behaviour — match today's behaviour, do not newly-crash on empty (guard if you change the shape).
  - Keep 8-bit and 16-bit correct (the samples are already scalar int here — no native byte reads in THIS
    function; the inputs are Cnr3ConstPlaneBufferView int planes. So no endian/memcpy concern — this is
    scalar->scalar, unlike the unpack).
DO NOT:
  - Alter the arithmetic, narrow to 32-bit, swap directionality, or change rounding. P.3A/P.5A are the arbiter.
  - Change the shared cnr3_blend_chroma_sample / cnr3_blend_chroma_sample_from_response_tables PUBLIC
    contracts — they remain the tested primitives used by P.3A/P.5A selftests. F/3c inlines their LOGIC into
    the production loop; it does not gut or alter the shared functions. (If you factor the combine into a
    shared inline helper used by BOTH the loop and the primitives, fine — but the primitives' behaviour and
    signatures must not change.)
  - Weaken the Tier-1 source gate, the response-table construction, or the final output clamp.
  - Introduce SIMD intrinsics / Path B. Auto-vectorisation of the clean loop is a BONUS, not required — the
    response-table GATHER (data-dependent index) will likely block full vectorisation (expect it; report the
    vec-report reason). The win here is call-chain elimination, like A-lite/C1, NOT SIMD.
  - Touch the staging leaf (repack-staging follow-on is separate), pooling (B), downsample, or CMS/invariants.
  - Bundle anything else — F/3c alone for measurement clarity.
```

## 6. Correctness argument

Old: per sample, call chain computes y_response/chroma_response via table lookup, weight = product, convex
blend with shift1 rounding >> shift2, int64. New: the SAME arithmetic inlined in the fused loop, reading the
same four samples via row pointers, same table lookups, same int64 weight/products/sum/shift, same
directionality, same rounding. Every output sample is computed identically -> bit-identical output plane.
Validation removed is redundant-by-provenance (Tier-2/Tier-3), so no VALID input changes behaviour; invalid
inputs cannot reach here without bypassing Tier-1 (invariant stated). Summary fields reproduced identically.
So value-identity holds on all inputs; P.3A/P.5A/P.6A + the blend-consuming gates are the arbiters.

## 7. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2.
2. Four-way selftest, dev-trace ON: 56/56 / 56/56 / 55/56 exit 1 / 56/56.
3. Value-identity — the KEY gates for F/3c (arithmetic in the recursive path):
     P.3A (blend combine: weight, convex combination, shift1 round-half-up, int64, directionality) UNCHANGED,
     P.5A (signed diffs feed the total lookup; response-table semantics) UNCHANGED,
     P.6A (blend composition) UNCHANGED,
     P.11B (real-frame composition; invalid-late-sample no-partial-publish) UNCHANGED.
   ANY assertion edit on P.3A/P.5A = RED FLAG — it means the inlined arithmetic diverged. STOP and diff the
   formula against section 3, do NOT edit the test.
4. (Optional) /Qvec-report:2 — record whether the fused loop vectorises. EXPECT partial/blocked on the
   response-table gather (data-dependent index, reason ~501/506). That is fine; the win is call elimination.
5. Profile vs post-Repack ~28,000 baseline, 2-3 runs (noise band ~+/-1,300). Report: total; and
   cnr3_process_chroma_plane_from_downsampled_luma self+total (the ~4,260 call-chain child cost should
   largely fold into a cheaper inlined self-time — expect a MATERIAL drop, like the other call-chain leaves).
6. Value-preserving or it is wrong.
```

## 8. Expected result (calibration)

~4,260 of the leaf's ~6,640 is per-sample call-chain overhead — the exact thing inlining removes. So a
MATERIAL total drop is expected (this is a big leaf, call-chain-dominated, same profile as 3a.1/C1). The
response-table gather won't vectorise, but that was never the mechanism. If it comes back small, report the
leaf's new self/total split so we see what remains (likely the gather + the int64 multiply). Read absolute
samples, 2-3 runs vs ~28,000.

## 9. Out of scope

Repack-staging follow-on (~4,745 leaf, separate), B (pooling), D (exact SIMD downsample), E (scene-change),
SIMD intrinsics/Path B, any arithmetic change, any change to the shared blend primitives' contracts, any
validation-policy or CMS change.
