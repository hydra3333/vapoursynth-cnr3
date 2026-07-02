# CNR3 — LEVER D (exact SIMD downsample) — INVESTIGATE & REPORT (no code)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** quick feasibility investigation. NO code. Determine whether D has any target that is BOTH hot AND
safely auto-vectorisable, before we scope a patch — because C1 may have changed the landscape underneath D.
**Target area:** the downsample tap-average — scalar `cnr3_downsample_luma_plane_to_chroma_grid` AND C1's
native `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`.
**Builds on committed:** ... + C1 + F/3c + Staging. Cumulative ~-79%.

---

## 1. Why investigate before scoping

D was catalogued as "exact SIMD `(a+b+c+d+2)>>2` on the ~1,374 downsample leaf." But C1 changed things:
```text
- The PRODUCTION downsample now goes through C1's NATIVE fused path
  (cnr3_downsample_native_luma_plane_to_scalar_chroma_grid), which reads native taps directly.
- The SCALAR cnr3_downsample_luma_plane_to_chroma_grid may now be largely BYPASSED in production (still the
  P.4A/P.7A-tested primitive, but maybe not the hot ~1,374 leaf).
So SIMD-ing the scalar function could be optimising a bypassed path (the 3a.2 trap: optimised code the
profile never runs).
- BOTH functions have the ASYMMETRIC x1 clamp: x1 = (x0 < width-1) ? x0+1 : x0 — a DATA-DEPENDENT index —
  plus a per-tap range-check branch. Both typically BLOCK auto-vectorisation. So even the hot native loop
  may not auto-vectorise, meaning D would require explicit SIMD INTRINSICS (Path B) — which the arc has
  deferred and which is NOT a "small safe win."
```

## 2. What to determine

```text
Q1 — Which function is the ~1,374 profile leaf: the C1 NATIVE downsample, the SCALAR one, or both? (Confirm
     from the profile call tree + who calls the scalar function in production now.) If the scalar function
     is bypassed in production, SIMD-ing it is a NON-WIN — report that.
Q2 — Can the HOT function's tap-average loop AUTO-vectorise (/Qvec-report:2) if the per-tap range-check is
     hoisted (like 3a.2) — OR does the asymmetric x1 data-dependent index block it regardless? Report the
     vec-report reason.
Q3 — If auto-vectorisation is blocked by the x1 clamp, is there a clean reshape that keeps P.4A EXACT
     (asymmetric x1, edge clamps, (a+b+c+d+2)>>2) AND vectorises WITHOUT explicit intrinsics? (e.g. split
     the interior — where x1 is always x0+1 — from the last column edge case, so the interior loop has a
     STATIC stride and vectorises, with a scalar tail for the final column.) Assess feasibility + whether
     the interior/edge split preserves P.4A bit-exactly.
Q4 — If the only path to SIMD is explicit intrinsics (Path B) on the small ~1,374 leaf, recommend
     NOT pursuing (Path B on a small leaf is disproportionate at -79%).
```

## 3. Verdict we need

```text
Report ONE of:
  WORTH IT   — the hot function can auto-vectorise via a clean P.4A-exact reshape (e.g. interior/edge split)
               -> we scope D as that reshape.
  NON-WIN    — the SIMD target is a bypassed scalar function, or the win is negligible -> skip D.
  PATH-B-ONLY — only explicit intrinsics would vectorise the hot loop -> skip D at -79% (disproportionate).
For Q1-Q4 give the source/profile finding. VPAVGB remains REJECTED (biased); any SIMD average must be the
EXACT widen-add-+2->>2 form. P.4A is the arbiter for any reshape.
```

## 4. Constraints

```text
- NO code — investigate and report the verdict + findings.
- Do NOT modify the shared scalar cnr3_downsample_luma_plane_to_chroma_grid contract (P.4A/P.7A primitive).
- Any reshape must keep the asymmetric x1, edge clamps, and (a+b+c+d+2)>>2 EXACT.
- Honest calibration: the leaf is ~1,374 at -79%. Only WORTH IT if a clean auto-vectorising reshape exists;
  otherwise skip. This is a cumulative-fps nicety, not a headline lever.
```
