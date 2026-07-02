# CNR3 — BLEND (F/3c) ARITHMETIC DERIVATION — INVESTIGATE & REPORT (no code, BLIND derivation)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** read-only investigation. NO code changes. **Derive the blend arithmetic INDEPENDENTLY from
source.** This brief deliberately does NOT state the formula — the designer has derived it separately, and
we will CROSS-CHECK your independent reading against the designer's. Convergence is the evidence; divergence
is a flag to chase before any code. Do not seek or accept a supplied formula; read the source and report
what it actually computes.
**Target module:** `cnr3_frame_processing.cpp` (+ `cnr3_response_tables.cpp` for table construction).
**Governing docs:** CMS07.15, P.3A / P.5A selftests (the bit-exact blend contract), CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2 + A-lite + C1. Cumulative ~-69%.

---

## 1. Why this is blind

An external tool (GAIS) recently RECONSTRUCTED the blend formula by inference and got it PARTLY right and
partly wrong. A mostly-right reconstruction is more dangerous than an obviously-wrong one, because it is
tempting to ratify. The only safe verification of a bit-exact recursive-filter contract is TWO INDEPENDENT
derivations from source that are then compared. You are one; the designer is the other. So: do not ask what
the formula "should" be — read `cnr3_blend_chroma_sample`, `cnr3_blend_chroma_sample_from_response_tables`,
and the supporting helpers, and state EXACTLY what they compute, bit-for-bit.

## 2. What to derive and report (from source only)

```text
1. THE COMBINE FORMULA. Read cnr3_blend_chroma_sample (the innermost combine). Report, exactly:
   - how the two response values (y_response, chroma_response) are combined into the blend weight
     (what does cnr3_calculate_combined_blend_weight actually compute?),
   - the full fixed-point blend expression (every term, the rounding addend, the shift),
   - which operand is weighted toward previous-filtered vs current-source (directionality matters — P.3A
     proves "previous filtered output is used, not source[N-1]").

2. ACCUMULATOR WIDTH. State the exact integer TYPE of every intermediate (weight, each product, the sum,
   the rounding addend, the shift). Is the accumulation 32-bit or 64-bit? WHY (what is the max magnitude
   at 8-bit AND at 16-bit)? This is load-bearing: the function is bit-depth-GENERIC — report whether a
   narrower type would be safe at 8-bit but UNSAFE at 16-bit.

3. THE SCALE/SHIFT DERIVATION. Read cnr3_blend_scale_for_bit_depth: what are shift, shift1, shift2 in terms
   of bits_per_sample? Confirm against P.3A's stated shift2 = depth<<1, shift = 1<<shift2, shift1 = shift>>1.

4. THE RESPONSE-TABLE LOOKUP. Read get_cnr3_table_value_for_signed_diff + how table_offset indexes a signed
   diff. What is the signed-diff range, the table size, and the out-of-range behaviour (P.1A: out-of-range
   diff -> zero)? Confirm the responses are bounded [0, sample_peak] BY CONSTRUCTION (cnr3_response_tables.cpp).

5. THE VALIDATION PRESENT TODAY. List every per-sample range check the blend path performs (on the four
   inputs, on the two responses, on the output). Classify each by the validation policy tiers:
   - Tier-2 (CNR3 intermediates that came through the Tier-1 source gate) -> removable in a production-
     private path, relying on the no-bypass invariant,
   - Tier-3 (response outputs bounded by table construction) -> removable with provenance guard,
   - final output clamp -> stays.

6. THE PER-SAMPLE OVERHEAD. What does the blend recompute per sample that is loop-invariant (sample_peak,
   table geometry/offset, shift/shift1/shift2, table pointers)? These are the hoist candidates. What
   per-sample CALLS exist (cnr3_blend_chroma_sample_from_response_tables -> cnr3_blend_chroma_sample ->
   helpers) that inlining would eliminate? (This is the A-lite/C1 structural win applied to the blend.)
```

## 3. Report format

```text
- The combine formula, written out, with exact types on every term.
- Accumulator width + the 8-bit vs 16-bit magnitude argument for why.
- shift/shift1/shift2 derivation.
- Response-table lookup + construction-bound facts.
- Validation inventory, tier-classified.
- Hoist/inline candidate list (loop-invariants + per-sample calls).
- Anything the source does that would surprise someone reconstructing from the function name.
```

## 4. Constraints

```text
- NO code this step. Derive and report only.
- Read the ACTUAL committed source; do not infer, do not accept an external reconstruction.
- The formula you report is what we will hold the eventual F/3c patch to (bit-exact). P.3A/P.5A are the arbiter.
- If anything is ambiguous in source, say so rather than guessing — ambiguity is a flag, not a fill-in.
```

## 5. What happens next

The designer compares your independent derivation against the designer's own. If they AGREE on the formula,
the accumulator width, the shift derivation, and the validation tiers, that convergence is the verification
— and we then scope F/3c as a STRUCTURAL patch (hoist loop-invariants, row-pointer traversal of the 4 input
planes + output, inline the per-sample call chain, __restrict) wrapping the VERIFIED arithmetic UNCHANGED,
with validation removals production-private per policy and the shared cnr3_blend_chroma_sample contract
protected (selftests use it). If the two derivations DIVERGE, we resolve the divergence against P.3A/P.5A
before any code. Either way, the arithmetic is never GAIS's and never inferred — it is the source, read twice.
