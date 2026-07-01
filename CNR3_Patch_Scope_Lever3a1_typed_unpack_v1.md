# CNR3 PATCH SCOPE — Lever 3a.1: typed native->scalar unpack (Option A, existing-buffer)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — highest-risk proven pixel-path code). Implement exactly
this; propose back for review before commit. This is the FIRST sub-step of Lever 3. Lever 3b (downsample),
3c (blend), buffer-free traversal, and any consumer-direct feeding are OUT OF SCOPE.
**Target file / function:** `cnr3_frame_processing.cpp`, `cnr3_copy_native_plane_to_scalar_buffer`
(the general-purpose native->scalar unpack; ~L2069). **Option A confirmed** (coder + designer): modify
this helper's INTERNALS, keep its signature and contract; do NOT add a new helper (Option B) and do NOT
go buffer-free.
**Controlling docs:** CMS07.15 (no design change), FI-10. Companion: data-flow map v0.2 (note: v0.2's
buffer inventory predates Lever 0A and still lists `current_luma_storage`/buffer 1, which 0A removed —
that one line is stale; the rest of the map's analysis holds).

---

## 1. Goal

Remove the per-sample `cnr3_load_native_plane_sample(...)` CALL from the unpack loop — the `506`/`501`
auto-vectorisation wall the vec-report identified — by inlining a typed-row-pointer read that fills the
EXISTING `resolved_samples` buffer, then publishing to `scalar_plane` exactly as today. Attack the
biggest residual hot leaf (~16,546 self samples) without changing the helper's contract or any consumer.

## 2. Current structure (the exact target)

`cnr3_copy_native_plane_to_scalar_buffer` (~L2069) currently:
1. derives `storage_bytes` (once) + validates views + dimension match;
2. allocates `resolved_samples` (`std::vector<int>`, sample_count);
3. **FIRST loop (the target):** `for y / for x` calling `cnr3_load_native_plane_sample(native_plane, x,
   y, sample)` per sample (early-return on non-ok), writing `resolved_samples[y*width + x] = sample`;
4. **SECOND loop:** `for y / for x` writing `resolved_samples[...]` into `scalar_plane` via
   `cnr3_write_plane_sample`.

The per-sample call in step 3 is the wall. `cnr3_load_native_plane_sample` per call re-derives
storage_bytes, sample_peak, view validity, coordinate bounds; computes the byte offset; does a
storage-width-branched little-endian load (1 byte, or 2-byte LE via memcpy into uint16_t); then range-
checks the sample against `[0, sample_peak]`, returning `invalid_argument` if out of range.

## 3. What to change (ONLY the first loop, step 3)

Replace the first loop's per-sample CALL with an inlined typed-row-pointer read that reproduces
`load_native_plane_sample`'s load + range-check semantics EXACTLY, while hoisting the invariant
re-derivations out of the loop (they are already computed once by the caller):

- Hoist ONCE, before the loop: `storage_bytes` (already derived at the top), `sample_peak` (derive once
  via `cnr3_sample_peak_for_bit_depth`), the base byte pointer, `stride_bytes`. These are loop-invariant;
  re-deriving them per sample is part of what blocks vectorisation.
- Inner loop, per row `y`: compute the row base pointer once (`base + y * stride_bytes`), then iterate
  `x` reading through a TYPED pointer:
    - 8-bit (`storage_bytes == 1`): `const std::uint8_t* row = ...; sample = row[x];`
    - 9-16-bit (`storage_bytes == 2`): `const std::uint16_t* row = reinterpret_cast<const uint16_t*>(...);
      sample = row[x];` (this reproduces the current 2-byte little-endian load on a little-endian target;
      confirm the memcpy-vs-reinterpret equivalence holds — see §5 note).
    - WIDEN IMMEDIATELY to `int` (the existing accumulator). No arithmetic in the sample's native type.
- **Preserve the per-sample range check:** `if (!cnr3_value_is_inclusive_range(sample, 0, sample_peak))
  return Cnr3Status::invalid_argument;` — same invalid-sample behaviour, same early return, BEFORE any
  further work. (This branch may limit how fully the loop vectorises — that is acceptable; see §6.)
- Write `resolved_samples[y*width + x] = sample` as today.
- Leave the SECOND loop (resolved_samples -> scalar_plane) UNCHANGED.

Net: same `resolved_samples` contents, same early-return-on-invalid, same publish — only the per-sample
helper call is replaced by an inlined typed read with hoisted invariants.

## 4. Hard constraints — the seven value-identity guards (do / do not)

```text
DO (the coder's risk list, as the value-identity contract):
  1. An invalid native sample (> sample_peak) must still return invalid_argument.
  2. On invalid native input the helper must leave scalar_plane UNCHANGED (no-partial-output). Because
     the typed read still fills resolved_samples (a throwaway) and only the SECOND loop publishes, a
     mid-first-loop failure returns before ANY write to scalar_plane — preserved exactly. Keep it so.
  3. Scalar padding must remain untouched.
  4. 8-bit and 9..16-bit paths must both preserve native little-endian load semantics (bit-identical
     samples to the current memcpy path).
  5. No arithmetic narrowing: native sample -> int immediately, compute in int.
  6. The downsample GUIDE still receives identical scalar luma values.
  7. Scene-change and blend consumers still receive identical U/V scalar values.

DO NOT:
  - Change cnr3_copy_native_plane_to_scalar_buffer's signature or its no-partial-output contract.
  - Touch the SECOND loop, or cnr3_write_plane_sample.
  - Go buffer-free / feed downsample/blend directly (later sub-step).
  - Remove or restructure the per-sample range check (that would change invalid-input behaviour). Hoisting
    the range check out of the loop to aid vectorisation is a SEPARATE later step (3a.2), NOT this patch.
  - Modify cnr3_load_native_plane_sample itself (other callers may use it) — inline an equivalent read
    here; leave the helper for its other uses.
  - Touch downsample geometry, blend maths, scene-change, or the all-or-nothing commit.
  - Introduce Path B / explicit SIMD / VCL.
```

## 5. Correctness argument + one thing to confirm from source

Old first loop: per-sample call -> (re-derive invariants, offset, LE load, range check) -> resolved_samples.
New first loop: hoisted invariants + inlined typed LE load + same range check -> resolved_samples. The
resolved_samples contents are identical (same load, same validation, same layout); the second loop and
the contract are untouched. So scalar_plane output is bit-identical, and invalid-input still returns
before any publish.

**Confirm from source (coder):** that `reinterpret_cast<const uint16_t*>` row access is byte-equivalent
to the current `std::memcpy(&native_sample, bytes+offset, 2)` on the little-endian x64 target, AND that
`native_plane.stride_bytes` is uint16-alignment-safe for the 2-byte path (P.10A pinned "two-byte VS
strides must be storage-byte aligned"). If alignment is NOT guaranteed, keep the `memcpy`-per-sample
form (still inlined, still no function call) rather than a `uint16_t*` deref — memcpy of 2 bytes inlines
to the same load without an aliasing/alignment hazard, and still removes the `506` call. Prefer the safe
form if there is any doubt; the win is removing the CALL, not the memcpy.

## 6. Vectorisation: REPORT, not a hard gate (per coder, accepted)

Re-run `/Qvec-report:2` (Release /O2 /arch:AVX2, this TU) on the modified loop. The BEFORE image is:
`cnr3_copy_native_plane_to_scalar_buffer` at ~L2201/2202 = `not vectorized / 506`, ~L2221/2222 =
`501`.

```text
- If the typed first loop reports "vectorized": PASS the (bonus) vectorisation gate.
- If it does NOT vectorise (the per-sample range-check branch may block it): report the EXACT reason
  code. This is acceptable — the primary win of 3a.1 is removing the per-sample CALL overhead, which the
  PROFILE will show regardless. (Full vectorisation may require hoisting the range check = a separate
  3a.2 step, decided after seeing 3a.1's numbers.)
- In EITHER case: the profile must show whether removing the per-sample helper call reduced the
  cnr3_load_native_plane_sample / copy hot leaf.
```

## 7. Proof gate (all required)

```text
1. Build Debug + Release (both projects), dev-trace ON, /arch:AVX2 on.
2. Four-way selftest: Debug normal 56/56; Release normal 56/56; Release forced-fail 55/56 exit 1;
   Release verbose 56/56.
3. Value-identity — these pass UNCHANGED: P.8A (native byte access: 8-bit 1 byte, 9-16-bit 2 byte LE,
   x*storage_bytes offsets, invalid-sample no-partial-output), P.9A (native luma downsample bridge),
   P.11B (real-frame composition), and the downsample/blend proofs that consume the scalar buffers
   (P.4A/P.5A/P.6A). Any assertion edit = RED FLAG.
4. Re-run /Qvec-report:2 on this loop; record vectorized-or-reason-code (§6).
5. Re-profile vs the post-0B baseline (67,891 samples; profiler_test_01, -r 1, normal, dev-trace OFF).
   Report: total change; cnr3_load_native_plane_sample / cnr3_copy_native_plane_to_scalar_buffer self-
   time change; whether the per-sample-call cost left the hot path.
6. Value-preserving or it is wrong. No CMS/invariant change.
```

## 8. Expected result (calibration, not a target)

The unpack is the biggest residual leaf (~24% self). Removing the per-sample call overhead should be a
real win even if the loop only partially vectorises; if it fully vectorises (8-bit especially), larger.
The chroma unpack is called for both U and V and for the downsample input, so the leaf is hit multiple
times per frame — a meaningful share of the remaining ~50% marshalling. If negligible or negative,
report it — do not tune to a number. Full vectorisation (via later range-check hoisting) and the
downsample/blend sub-steps carry the rest of the Lever-3 target.

## 9. Out of scope

3a.2 (range-check hoisting for fuller vectorisation), 3b (downsample), 3c (blend), buffer-free traversal,
consumer-direct feeding, any change to `cnr3_load_native_plane_sample` or the second loop, Path B / VCL,
any chroma-maths / scene-change / cache / commit change, any CMS/invariant change.
