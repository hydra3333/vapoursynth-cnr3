# CNR3 PATCH SCOPE — Lever Repack: hoist bit-depth branch + row-pointer commit (A-lite-for-repack)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21). Implement exactly this; propose back for review before commit.
**Repack lever ALONE.**
**Target:** `cnr3_frame_processing.cpp`, `cnr3_commit_staged_native_active_samples` (the repack / staged-bytes
commit). NOTE also the related `cnr3_stage_scalar_plane_to_native_bytes` (~4,700 leaf) — see section 8.
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2 + A-lite + C1. Cumulative ~-69% (93,914 -> ~29,250).

---

## 1. Goal + why this is safe

Post-C1 the second-largest leaf is the repack/staging side (`cnr3_stage_scalar_plane_to_native_bytes`
~4,700; and `cnr3_commit_staged_native_active_samples`). The repack is a PURE memory copy / byte-format
operation — no recursive arithmetic, nothing to get bit-wrong. This is the A-lite structural move applied
to the repack: hoist the bit-depth branch out of the per-sample loop and use row pointers, killing the
per-sample `cnr3_native_plane_byte_offset` multiply and the inner `storage_bytes == 1` branch (the
`C5004 loop-unswitched` / reason-501 blocker). Low risk, proven class (A-lite -7.4%, C1 -24.5% used this
same structural mechanism).

## 2. Current shape (verified from source)

```text
cnr3_commit_staged_native_active_samples(staged_bytes, destination):
  storage_bytes hoisted (good), BUT the inner loop still does, per sample:
    offset = cnr3_native_plane_byte_offset(x, y, stride_bytes, storage_bytes)   // per-sample multiply
    if (storage_bytes == 1) dst[offset] = staged[offset];                        // inner branch (unswitch)
    else memcpy(dst+offset, staged+offset, 2);
```

## 3. What to change — hoist branch to outer, row-pointer form

```text
- Resolve storage_bytes once (already done).
- Split the storage_bytes==1 vs ==2 decision to the OUTER level (outside both loops).
- 8-bit path: per row, dst_row = dst_base + y*stride_bytes; src_row = staged_base + y*stride_bytes;
    for x: dst_row[x] = src_row[x];        // linear byte copy, vectorisable
- 16-bit path: per row, reinterpret dst_row/src_row as uint16_t* at dst_base + y*stride_bytes;
    for x: dst_row[x] = src_row[x];        // 2-byte copy
  (Keep byte base + y*stride_bytes arithmetic; the staged buffer's layout must match destination stride —
   confirm from source that staged_bytes is stride_bytes-pitched, NOT width-packed; if staged is
   width-packed the src row stride differs from dst stride — use the correct one for each. THIS IS THE ONE
   THING TO VERIFY before writing the loop.)
- __restrict on dst and src pointers (they are distinct buffers: destination plane vs staged vector).
```

## 4. Hard constraints (do / do not)

```text
DO:
  - Hoist the bit-depth branch to the outer level; row-pointer form; __restrict on non-aliasing dst/src.
  - Preserve EXACT byte output: active samples written identically, native padding bytes untouched
    (the repack must not write padding it didn't before — confirm the loop bounds match today's
    destination.width / height, not stride).
  - Preserve the function contract (void, noexcept, early-return on bad storage_bytes).
  - VERIFY staged-buffer pitch (stride_bytes vs width) before choosing the src row stride — see section 3.
DO NOT:
  - Change what bytes are written or the padding behaviour.
  - Introduce SIMD intrinsics (auto-vectorisation of the clean loop only).
  - Touch the blend, the downsample, validation policy, or any arithmetic.
  - Bundle with F/3c or anything else — repack alone for measurement clarity.
```

## 5. Correctness argument

The per-sample `offset = cnr3_native_plane_byte_offset(x,y,stride,storage)` is exactly
`y*stride_bytes + x*storage_bytes`. Row-pointer form computes `y*stride_bytes` once per row and indexes
`x` (8-bit) or `x` as uint16 (16-bit) — identical addresses, identical bytes written. The outer branch
selects the same path the inner branch did. So byte output is identical for all inputs; this is a pure
loop-shape change.

## 6. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2.
2. Four-way selftest, dev-trace ON: 56/56 / 56/56 / 55/56 exit 1 / 56/56.
3. Value-identity: P.8A (scalar-to-native stores preserve active samples AND native padding bytes — the
   key gate here), P.11B unchanged. Any assertion edit = RED FLAG.
4. (Optional) /Qvec-report:2 — record whether the row loops vectorise (bonus). Flags are carried dirty;
   no need to toggle unless you want the datapoint.
5. Profile vs post-C1 ~29,250 baseline, 2-3 runs (noise band ~+/-1,300). Report total + the repack leaf
   (cnr3_stage_scalar_plane_to_native_bytes / commit) self-time.
6. Value-preserving or it is wrong.
```

## 7. Expected result (calibration)

The repack leaf is ~4,700. A-lite got -7.4% halving a similar-sized leaf via the same mechanism, so a
real-but-modest drop is plausible if the loop vectorises or the per-sample offset/branch removal bites.
Could also be flat if the copy is already memory-bound (like the 8-bit unpack was pre-A-lite). Honest
either way — commit if value-clean AND (measurably helps OR materially cleaner source), per the A-lite rule.

## 8. Note on cnr3_stage_scalar_plane_to_native_bytes

The larger repack-side leaf in the profile is `cnr3_stage_scalar_plane_to_native_bytes` (~4,700), which
converts scalar int -> native bytes (reason 506). If `cnr3_commit_staged_native_active_samples` is a
sub-step of it, this patch may help it too; if they are separate, a follow-on repack lever can target the
scalar->native staging conversion with the same discipline. Report which is which from source, and whether
this patch touches the ~4,700 leaf or a smaller sibling — that tells us if a follow-on is warranted.

## 9. Out of scope

F/3c (blend), B (pooling), D (exact SIMD downsample), E (scene-change), any arithmetic or validation-policy
change, SIMD intrinsics, CMS/invariant change.
