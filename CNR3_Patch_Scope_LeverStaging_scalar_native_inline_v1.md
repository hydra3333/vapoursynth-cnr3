# CNR3 PATCH SCOPE — Lever Staging (F/3c follow-on): inline scalar->native staging conversion

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code, ARITHMETIC narrow+clamp path).
Implement exactly this; propose back for review before commit. **Staging lever ALONE.**
**Target:** `cnr3_frame_processing.cpp` — the scalar->native STAGING conversion path:
`cnr3_stage_scalar_plane_to_native_bytes` -> `cnr3_copy_scalar_buffer_to_native_plane`.
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2 + A-lite + C1 + Repack + F/3c. Cumulative ~-75% (-> ~23,600).

---

## 1. Goal + what the profile shows

`cnr3_stage_scalar_plane_to_native_bytes` is the new TOP leaf (~4,753 total / ~203 self). The 203 self is
just its alloc+setup; it delegates to `cnr3_copy_scalar_buffer_to_native_plane`, where the ~4,550 lives.
Reading that function, there are TWO stacked inefficiencies:
```text
1. PER-SAMPLE CALL CHAIN: the conversion loop calls cnr3_store_native_plane_sample(...) +
   cnr3_plane_sample_at(...) per sample (the ~4,550 cost — same signature 3a.1/A-lite/C1/F3c crushed).
2. REDUNDANT SECOND BUFFER + SECOND PASS: it allocates a WHOLE extra `resolved_bytes` (stride*height),
   fills it per-sample, THEN a second loop copies resolved_bytes -> native_plane.data. This is the exact
   redundancy Lever 0B removed on the OTHER staging path; it survives HERE only because 0B deliberately
   left this SHARED primitive untouched (Option A). For the STAGING caller the destination is a throwaway
   staged buffer, so the resolved_bytes intermediate is pure waste.
```
This lever eliminates BOTH for the staging path: inline the narrow+clamp conversion (kill the per-sample
calls) AND write directly into the staged bytes (kill the resolved_bytes buffer + second pass).

## 2. CRITICAL constraint — do NOT modify the shared primitive (0B Option-A discipline)

`cnr3_copy_scalar_buffer_to_native_plane` is a SHARED, TESTED primitive. Lever 0B explicitly chose Option A
(specialise the caller, leave the shared function intact for real-destination callers). Follow the SAME
rule here:
```text
- Create a STAGING-SPECIFIC direct converter (production-private), OR inline the conversion directly into
  cnr3_stage_scalar_plane_to_native_bytes, writing straight into staged_bytes.
- Do NOT change cnr3_copy_scalar_buffer_to_native_plane's signature or behaviour — it stays the tested
  primitive for any real-destination caller. (0B established it is effectively single-caller in production
  now, but KEEP it intact — same decision as 0B.)
- If the staging path is its only production caller, the staging inline simply stops calling it; the shared
  function remains for selftests / future callers.
```

## 3. THE ARITHMETIC — scalar int -> native byte (narrow + clamp), reproduce EXACTLY

This is the REVERSE of the 3a.1 unpack (which widened native->int). The store narrows int->native with a
clamp. Reproduce whatever `cnr3_store_native_plane_sample` does today, bit-exactly:
```text
- Read scalar int sample (already validated upstream — Tier-2, came through the Tier-1 gate).
- Clamp/range per the store's current contract (confirm from cnr3_store_native_plane_sample: does it clamp
  to [0, sample_peak], or assert-in-range and store? Reproduce EXACTLY — this is the value-identity crux).
- Write native byte(s): 8-bit -> single uint8 store; 16-bit -> unaligned-safe 2-byte memcpy (per the
  3a.1/3a.2/Repack decision — do NOT reintroduce an unaligned uint16* cast, that was the Repack-v1 bug).
- Byte offset uses native stride: dst[y*stride_bytes + x*storage_bytes] (P.10A x*storage_bytes rule).
```
DO NOT compute in a narrow type — if any widening/arithmetic is needed, keep it in int/int64 and narrow
ONLY at the byte store (the same discipline as the whole arc: compute wide, narrow at store).

## 4. Structure — hoist + row-pointer + direct write (like A-lite/Repack)

```text
- Hoist storage_bytes, sample_peak (if the clamp needs it), base pointers, strides out of the loop.
- Split 8-bit / 16-bit to the OUTER level (no per-sample bit-depth branch).
- Row-pointer form: per row, dst_row = staged_base + y*stride_bytes; src_row = scalar_row;
    8-bit:  for x: dst_row[x] = (uint8)clamp(src_row[x]);
    16-bit: for x: unaligned-safe 2-byte store of (uint16)clamp(src_row[x]) at dst_row + x*2;
- Write DIRECTLY into staged_bytes — NO resolved_bytes intermediate, NO second copy pass.
- __restrict on the non-aliasing scalar-source vs staged-dest pointers.
- Preserve padding behaviour: staged_bytes is stride*height, zero-initialised (assign(...,0)); write only
  the active width*storage_bytes per row (padding stays zero, as today).
```

## 5. Validation (policy)

```text
- The scalar input is Tier-2 (produced through the Tier-1 source gate). Per policy, the per-sample input
  range check is removable in this production-private staging path — BUT confirm the store's current
  behaviour: if cnr3_store_native_plane_sample CLAMPS (rather than rejects), the clamp is part of the
  VALUE contract and MUST be reproduced (it is arithmetic, not validation). Do not drop a clamp that
  affects output; only drop a redundant range-REJECT that never fires on valid Tier-2 input.
- If the store REJECTS out-of-range (returns status), preserve no-partial-output: since staged_bytes is a
  throwaway buffer committed later at the outer all-or-nothing gate, an early reject is acceptable as long
  as it does not publish a partial DESTINATION (it doesn't — the commit is separate). Confirm against P.8A.
- State the provenance invariant in a comment (Tier-2 came through Tier-1).
```

## 6. Hard constraints (do / do not)

```text
DO:
  - Eliminate the per-sample call chain AND the resolved_bytes buffer + second pass, for the STAGING path.
  - Reproduce the store's narrow+clamp arithmetic EXACTLY (value-identity crux — confirm clamp vs reject).
  - 0B Option-A discipline: do NOT modify the shared cnr3_copy_scalar_buffer_to_native_plane.
  - Row-pointer, hoisted, outer bit-depth branch, unaligned-safe 16-bit store, __restrict, direct-to-staged.
  - Preserve staged padding (zero-init, write active only).
DO NOT:
  - Modify the shared primitive's contract/signature/behaviour.
  - Reintroduce an unaligned uint16* cast (Repack-v1 bug) — use memcpy for 16-bit.
  - Compute in a narrow type; narrow only at the byte store.
  - Drop a clamp that affects output (that is arithmetic, not removable validation).
  - Introduce SIMD intrinsics (auto-vectorisation of the clean loop only).
  - Touch the blend, downsample, unpack, pooling, or CMS/invariants.
  - Bundle anything else — staging lever alone.
```

## 7. Correctness argument

Old (staging path): scalar -> per-sample store into resolved_bytes -> copy resolved_bytes into staged_bytes.
New: scalar -> inlined narrow+clamp directly into staged_bytes, row-pointer, no intermediate. Same clamp,
same byte layout, same padding (zero-init + active-only writes) -> byte-identical staged_bytes. The removed
work is the redundant buffer + copy pass + the per-sample call overhead; the VALUE written is identical.
P.8A (scalar->native stores preserve active samples and native padding) is the arbiter.

## 8. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2.
2. Four-way selftest, dev-trace ON: 56/56 / 56/56 / 55/56 exit 1 / 56/56.
3. Value-identity: P.8A (scalar->native active + padding preservation — THE gate here), P.11B unchanged.
   Any assertion edit = RED FLAG. Confirm P.8A's invalid-sample no-partial-output case still holds.
4. (Optional) /Qvec-report:2 — record whether the staging conversion loop vectorises (8-bit likely; 16-bit
   memcpy may not). Bonus, not required — win is call-chain + buffer elimination.
5. Profile vs post-F/3c ~23,600 baseline, 2-3 runs (noise ~+/-1,300). Report total + the staging leaf
   (cnr3_stage_scalar_plane_to_native_bytes should collapse from ~4,753 like the other call-chain leaves).
6. Value-preserving or it is wrong.
```

## 9. Expected result (calibration)

~4,550 of the ~4,753 leaf is per-sample call chain + the redundant buffer/pass — both eliminated here. So a
MATERIAL drop is expected (same profile as A-lite/C1/F3c: call-chain-dominated leaf). This is the LAST big
call-chain leaf; after it, the remaining leaves (~1,300-1,600 unpack/downsample, ~920 alloc) are
diminishing-returns territory and Lever B (pooling) becomes the next candidate. Read absolute samples,
2-3 runs vs ~23,600.

## 10. Out of scope

B (pooling), D (exact SIMD downsample), E (scene-change), SIMD intrinsics, any change to the shared
cnr3_copy_scalar_buffer_to_native_plane, blend/downsample/unpack, CMS/invariant change.
