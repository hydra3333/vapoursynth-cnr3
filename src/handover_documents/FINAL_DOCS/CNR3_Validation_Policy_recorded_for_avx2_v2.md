# CNR3 — VALIDATION POLICY (recorded decision, feeds 3a.2 / 3b.2 / 3c)

**Decision owner:** coordinator (W3X, Dave). **Recorded by:** designer (W3D). **Date context:** marshalling
arc, post-3b.1. **Status:** ADOPTED policy — governs the vectorisation-unblocking steps.

## The policy (one sentence)

**CNR3 DEFENDS against non-conformant source input at the plane boundary, TRUSTS its own validated
intermediates thereafter, and ALWAYS clamps at the final output store.**

## Why (domain justification)

CNR3's real inputs include VHS/analogue captures from low-cost capture hardware that can emit glitches —
samples outside `[0, sample_peak]` for the declared bit depth. The VapourSynth C API does NOT enforce
per-sample range at the boundary (`getWritePtr` exposes raw plane memory; a broken/glitchy upstream can
write e.g. 65535 into a nominal-10-bit uint16 plane). Because output[N] feeds output[N+1] (recursive), a
single bad sample can seed the recursion, so silent propagation is worse than for a stateless filter.
Therefore CNR3 must DETECT out-of-range source samples — it may not simply trust the declared bit depth.

## The three-tier consequence (governs KEEP/HOIST/REMOVE)

```text
TIER 1 — SOURCE BOUNDARY (the defence gate). KEEP the guarantee; HOIST the shape.
  Site: cnr3_copy_native_plane_to_scalar_buffer (native VS plane -> scalar), and the standalone
        cnr3_load_native_plane_sample helper.
  Every sample read from a raw VS source plane is validated to [0, sample_peak]. This gate STAYS.
  For vectorisation, its SHAPE is restructured (hoisted out of the per-sample loop) but the guarantee
  is identical: invalid source still returns invalid_argument before any scalar publish.

TIER 2 — CNR3-OWNED VALIDATED INTERMEDIATES (trust downstream). REMOVE from production hot path.
  Sites: scene-change scalar reads; downsample taps; blend input samples.
  These values ONLY reach these sites THROUGH the Tier-1 gate. Once Tier 1 has validated them, re-checking
  is redundant. REMOVE from the production path — BUT only via production-private paths; do NOT weaken any
  shared/proof helper's contract (specialise the caller, leave the tested primitive intact).
  INVARIANT THIS RELIES ON: no Tier-2 buffer may be populated by any path that bypasses the Tier-1 gate.
  If future code adds such a path, that code must validate at its own boundary, or Tier-2 removal becomes
  unsafe. THIS INVARIANT MUST BE PRESERVED for the trust-downstream posture to hold.

TIER 3 — CONSTRUCTION-BOUNDED VALUES (bounded by design). REMOVE per-sample checks; guard provenance.
  Sites: y_response / chroma_response (response-table outputs).
  The response tables are bounded to [0, sample_peak] BY CONSTRUCTION (build_cnr3_response_tables clamps
  at build, zero-fills, and returns zero for out-of-range diffs). So per-sample response-range checks are
  redundant GIVEN the tables come from the sanctioned builder. REMOVE per-sample; if arbitrary
  Cnr3ResponseTables could ever be injected, validate table values ONCE outside the hot loop (provenance
  guard), not per sample.

ALWAYS — FINAL OUTPUT STORE. Untouched, non-negotiable.
  The final blended sample is clamped/validated at the native store regardless of any of the above. This
  is a separate, permanent gate and no optimisation weakens it.
```

## Scope discipline this implies

- **3a.2** = Tier-1 HOIST on the unpack (the big leaf). Keep the guarantee, restructure for vectorisation.
- **3b.2 / 3c** = Tier-2/Tier-3 REMOVE, each via a production-private path, each RE-STATING the provenance
  invariant it relies on, none weakening a shared proof helper.
- Any REMOVE is safe ONLY where provenance is PROVEN from source (not assumed). "Unsure" stays KEEP/HOIST.
- Valid-input value-identity is non-negotiable throughout; the P-series selftests remain the arbiter.
