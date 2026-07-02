# CNR3 PATCH SCOPE — Lever A-lite: 8-bit unpack row-pointer/restrict diagnostic

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code) AND a DIAGNOSTIC experiment.
Implement exactly this; propose back for review before commit. **A-lite ALONE.**
**Target:** `cnr3_frame_processing.cpp`, `cnr3_copy_native_plane_to_scalar_buffer`, **8-bit path only**.
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2.
**Purpose:** the coder's Step-1 diagnostic — determine whether MSVC can vectorise the 8-bit conversion
(currently scalar, reason 501) when the loop is put in clean row-pointer form with `__restrict`, and
whether that moves the YUV420P8 total. This DIRECTLY TESTS the memory-bound hypothesis and de-risks C1.

---

## 1. What this experiment answers

3a.2 left the 8-bit conversion loop SCALAR (reason 501 — indexed `resolved_samples[(y*width)+x]` writes
and a separate per-sample publish loop). We do NOT yet know if that is because the loop is genuinely
memory-bound or because the loop FORM defeats the vectoriser. A-lite makes the form clean and reads the
result:
```text
- vectorises + total drops   -> the 8-bit copy had headroom; keep + commit.
- vectorises + total FLAT     -> the 8-bit copy is MEMORY-BOUND (confirmed) -> the win must come from NOT
                                 copying (C1), not from copying faster. Commit only if the row-pointer form
                                 is genuinely cleaner source; else discard. Either way the question is answered.
- does not vectorise          -> report the remaining reason code; likely proceed to C1 regardless.
```

## 2. What to change — row-pointer form + restrict, 8-bit path only

The 8-bit path currently (post-3a.2) does indexed writes into `resolved_samples` then a SEPARATE publish
loop with per-sample `cnr3_write_plane_sample`. Convert BOTH the 8-bit conversion loop and (if it is the
same 8-bit-relevant path) the publish loop to row-pointer form so the vectoriser sees linear contiguous
access, and annotate the non-aliasing pointers with MSVC `__restrict` (`__declspec(restrict)` is a
different thing — use the pointer form `int* __restrict p`).

```text
CONVERSION loop (8-bit path):
  for each row y:
    const uint8_t* __restrict src_row = base_bytes + (size_t)y * stride_bytes;
    int*           __restrict dst_row = resolved_samples.data() + (size_t)y * width;
    for x in [0,width):  dst_row[x] = (int)src_row[x];
  (widen-on-load preserved; NO per-sample range check on 8-bit — type-guaranteed per 3a.2; unchanged.)

PUBLISH loop (only if it is the scalar-plane write that follows, and only the form — NOT the contract):
  express as row pointers into scalar_plane's buffer + resolved_samples, __restrict where non-aliasing,
  so it too can vectorise. Do NOT change what cnr3_write_plane_sample semantically does (same values,
  same layout) — only remove the per-sample call/indexing form if it is safe and the plane layout allows
  a linear row walk. If the scalar-plane layout does NOT permit a clean row pointer (e.g. non-trivial
  stride/packing), LEAVE the publish loop as-is and only do the conversion loop — report that you did so.
```

Restrict is a PROMISE the pointers don't overlap. It is TRUE here: `base_bytes` is the const VS source
plane; `resolved_samples` is a freshly-allocated owned vector; `scalar_plane` is a distinct owned buffer.
None alias. But VERIFY there is no caller that passes overlapping/in-place buffers into this function
(there should not be — the unpack's whole job is native->separate-scalar). If any such caller exists,
do NOT apply restrict to that pair.

## 3. Hard constraints (do / do not)

```text
DO:
  - 8-bit path ONLY. Leave the 16-bit path (the 3a.2 vectorised C5001 loop + its pre-scan) UNTOUCHED.
  - Row-pointer form + __restrict on genuinely non-aliasing pointers.
  - Preserve value-identity EXACTLY: same resolved_samples, same scalar_plane, all inputs.
  - Preserve the function contract, signature, and the Tier-1 validation behaviour (8-bit is type-
    guaranteed in-range; no check to move, but do not introduce or drop any status path).
DO NOT:
  - Touch the 16-bit path, the validation pre-pass, or cnr3_load_native_plane_sample.
  - Change cnr3_write_plane_sample's semantics (values/layout) — only its call FORM in the publish loop,
    and only if safe; otherwise leave it.
  - Pool/reuse resolved_samples (that is Lever B, deferred) — allocation stays as-is this patch.
  - Introduce SIMD intrinsics (we want AUTO-vectorisation of the clean loop).
  - Weaken any shared proof helper or the final output clamp.
  - Combine with C1 or anything else — A-lite stands alone for measurement clarity.
```

## 4. Correctness argument

The conversion loop computes the identical `dst_row[x] = (int)src_row[x]` as the indexed form
`resolved_samples[y*width+x] = (int)row[x]` — same values, same positions, just expressed via a row
pointer instead of a flat index. `__restrict` changes no values; it only tells the compiler the buffers
don't overlap (true here). The publish loop, if converted, must produce the identical scalar_plane bytes.
So value-identity holds on all 8-bit inputs; 16-bit is untouched.

## 5. Vectorisation evidence (this is the point of the patch)

```text
- Re-run /Qvec-report:2 (Release /O2 /arch:AVX2) on cnr3_copy_native_plane_to_scalar_buffer.
- Report the reason code / vectorized verdict for BOTH the conversion loop and (if changed) publish loop.
- GOAL: does the 8-bit conversion loop flip from 501 to C5001?
- Then REVERT the .vcxproj flag edits before commit:
    git checkout -- vs/cnr3/cnr3.vcxproj vs/cnr3/cnr3_cache_core_selftest.vcxproj
```

## 6. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2.
2. Four-way selftest, dev-trace ON: Debug 56/56; Release 56/56; forced-fail 55/56 exit 1; verbose 56/56.
3. Value-identity: P.8A (native->scalar unpack, no-partial-output), P.9A, P.11B unchanged. Any assertion
   edit = RED FLAG.
4. /Qvec-report:2 verdict (section 5), then revert the .vcxproj edits.
5. Profile vs post-3a.2 ~41,850 baseline, 2-3 runs (noise band ~+/-1,300). Report: total; and
   cnr3_copy_native_plane_to_scalar_buffer self-time (did the 8-bit copy drop, or vectorise-but-flat?).
6. Value-preserving or it is wrong.
```

## 7. Commit rule (diagnostic-aware)

```text
Commit A-lite ONLY IF value-clean AND (measurably helps OR the row-pointer form is materially cleaner source).
If vectorised-but-flat and the new form is not cleaner -> discard the patch, record the finding (8-bit copy
is memory-bound), and proceed to C1. The EXPERIMENT'S RESULT is the deliverable either way.
Expected (calibration): given the confirmed 8-bit clip and the leaf now ~7-7.8k combined (smaller than the
earlier ~10k estimate), do NOT expect a large total move even if it vectorises. Its value is diagnostic:
it tells us whether C1 (eliminate the copy) is the right shape of fix.
```

## 8. Out of scope

C1 (native-luma downsample bridge — the next serious lever), F/3c (blend), B (pooling), D (exact SIMD
downsample), E (scene-change), the 16-bit path, any validation-policy change, any allocation change.
