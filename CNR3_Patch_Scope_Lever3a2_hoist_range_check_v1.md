# CNR3 PATCH SCOPE — Lever 3a.2: hoist the source range-check (unblock unpack vectorisation)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code). Implement exactly this; propose back
for review before commit. **Lever 3a.2 ALONE.** OUT OF SCOPE: 3b.2 (Tier-2 removals), 3c (blend), any
Class-B/C change, buffer-free traversal, Path B/SIMD-intrinsics, any change to the FINAL output clamp.
**Target:** `cnr3_frame_processing.cpp`, `cnr3_copy_native_plane_to_scalar_buffer` (the 3a.1 typed unpack).
**Governing policy:** CNR3_Validation_Policy_recorded_v1 — **TIER 1: KEEP the guarantee, HOIST the shape.**
**Controlling docs:** CMS07.15 (no design change), FI-10. **Builds on:** AVX2 + 0A + 0B + 3a.1 + 3b.1.

---

## 1. Goal + policy context

3a.1 removed the per-sample CALL but the unpack loop STILL reports `506` "not vectorized" — blocked by
the per-sample range-check BRANCH (`if (!in_range) return invalid_argument;`), a data-dependent
early-return the vectoriser cannot handle. Per the adopted validation policy, this is a **Tier-1 source
boundary gate** — it DEFENDS against non-conformant/glitchy VHS-capture input and MUST be kept. So 3a.2
does NOT remove it; it **restructures its shape** so the guarantee is identical but the hot conversion
loop becomes branch-free and can vectorise. This is the highest-value vectorisation target (the
~10k-sample unpack leaf, the biggest remaining cnr3 leaf).

## 2. The invariant to preserve (unchanged behaviour)

```text
For any input: if ANY source sample is out of [0, sample_peak] for the declared bit depth, the function
returns Cnr3Status::invalid_argument and does NOT publish to scalar_plane (no partial output).
For valid input: scalar_plane is byte/value IDENTICAL to today.
```

The ONLY thing that changes is WHEN/WHERE the check runs (a pre-pass instead of inside the conversion
loop), not WHETHER it runs or its result.

## 3. What to change — validate-then-convert (two phases)

Split the current single validated-conversion loop into: a **validation pre-pass** (branch-free-friendly,
per bit-depth), then a **branch-free conversion loop** (the vectorisation target).

```text
PHASE 0 (hoist, once — mostly already present from 3a.1):
  sample_peak (from bits_per_sample), base pointer, stride, storage_bytes, dims.

PHASE A — VALIDATION PRE-PASS (the Tier-1 gate, restructured):
  8-bit path (storage_bytes == 1):
    sample_peak == 255 and a uint8_t sample is ALWAYS in [0,255], so the range check is UNCONDITIONALLY
    satisfied. NO scan needed — the type guarantees it. (Document this with a comment.)
  9..16-bit path (storage_bytes == 2):
    a uint16_t sample CAN exceed sample_peak (e.g. 65535 in a 10-bit plane, peak 1023). Scan the active
    samples row-by-row via typed uint16_t reads (unaligned-safe per 3a.1's memcpy decision if alignment
    is not guaranteed) and detect any sample > sample_peak. Prefer a branch-free/reduction-friendly scan
    (e.g. OR-accumulate an out-of-range flag, or track a running max, then ONE compare after the scan) so
    the SCAN itself can also vectorise. If ANY sample is out of range -> return invalid_argument BEFORE
    Phase B (so no scalar_plane publish; no-partial-output preserved).
    (Lower bound: uint16_t is >= 0 intrinsically, so only the upper bound (> sample_peak) can fail.)

PHASE B — BRANCH-FREE CONVERSION (the vectorisation target):
  With validity already guaranteed by Phase A, the conversion loop has NO per-sample range check and NO
  early return: for each sample, typed load -> widen to int -> write resolved_samples. This loop should
  now be branch-free contiguous work the vectoriser can take. (Then the existing publish loop, unchanged.)
```

Net: same `resolved_samples`, same invalid-input behaviour (now detected in Phase A before any publish),
same output — but the hot conversion loop (Phase B) is branch-free.

## 4. Hard constraints (do / do not)

```text
DO:
  - Preserve the Tier-1 guarantee EXACTLY: out-of-range source -> invalid_argument, no scalar_plane publish.
  - 8-bit: drop the per-sample check as TYPE-GUARANTEED (uint8_t in [0,255] == sample_peak); comment why.
  - 16-bit: validate via a hoisted pre-scan BEFORE conversion; reject before any publish.
  - Make Phase B (conversion) branch-free so it can vectorise; keep widen-on-load.
  - Keep resolved_samples + the publish loop + the function contract + no-partial-output.
  - Preserve 3a.1's unaligned-safe 16-bit load decision (memcpy, unless alignment is proven).

DO NOT:
  - REMOVE the guarantee (this is Tier-1 defence, not Tier-2 redundancy). The check moves, it does not vanish.
  - Change the final output-store clamp (separate, permanent).
  - Touch Class-B/C sites (scene-change, downsample taps, blend, response checks) — those are 3b.2/3c.
  - Change cnr3_copy_native_plane_to_scalar_buffer's signature/contract, or the publish loop.
  - Modify cnr3_load_native_plane_sample (the standalone helper stays — Tier-1 per policy).
  - Introduce SIMD intrinsics / Path B (we want AUTO-vectorisation of the branch-free loop first).
  - Change CMS design or any invariant.
```

## 5. Correctness argument

Old: per-sample (load, widen, range-check, early-return-on-fail) -> resolved_samples. New: Phase A proves
all samples in range (8-bit by type; 16-bit by pre-scan) and returns invalid_argument before any publish
if not; Phase B then does (load, widen, write) with no check. For VALID input every sample is written
identically (the check passed in both old and new, it just ran earlier) -> byte-identical resolved_samples
and scalar_plane. For INVALID input both old and new return invalid_argument before publishing ->
no-partial-output identical. So value-identity holds on all inputs.

## 6. Vectorisation: THIS time it IS an evidence goal (but honest)

3a.2's PURPOSE is to unblock vectorisation, so the vec-report matters more than before:
```text
- Re-run /Qvec-report:2 (Release /O2 /arch:AVX2) on cnr3_copy_native_plane_to_scalar_buffer.
- GOAL: Phase B (conversion) flips from 506 to "vectorized". The 16-bit validation SCAN (Phase A) ideally
  vectorises too (reduction), but if it reports a reason code that is acceptable — the big win is the
  conversion loop.
- If Phase B still does NOT vectorise, report the exact reason code — there may be a remaining construct
  (e.g. the resolved_samples index arithmetic, reason 501) to address, which routes a possible 3a.3.
- Record vectorized-or-reason-code for BOTH phases.
```

## 7. Proof gate

```text
1. Build Debug + Release (both projects), dev-trace ON, /arch:AVX2 on.
2. Four-way selftest: Debug normal 56/56; Release normal 56/56; forced-fail 55/56 exit 1; verbose 56/56.
3. Value-identity UNCHANGED: P.8A (native access + invalid-sample no-partial-output — THE key gate here,
   it pins that out-of-range input still returns invalid_argument with no publish), P.9A, P.11B, and the
   downsample/blend consumers (P.4A/P.5A/P.6A). Any assertion edit = RED FLAG.
   [Specifically confirm P.8A's invalid-late-sample case still passes — the check moved to a pre-pass but
    must still reject the same inputs with no partial output.]
4. Re-run /Qvec-report:2; record both-phase verdicts (section 6).
5. Re-profile vs post-3a.1/3b.1 baseline (~42,400 mean; profiler_test_01, -r 1, dev-trace OFF; take 2-3
   runs given the noise band). Report: total change; cnr3_copy_native_plane_to_scalar_buffer self-time;
   whether the unpack leaf dropped (THIS is the big leaf — a vectorised conversion should show materially).
6. Value-preserving or it is wrong.
```

## 8. Expected result (calibration)

This is the first patch targeting the BIG leaf (~10k samples) with actual vectorisation as the mechanism
(not just call-elimination). If Phase B vectorises, expect a MATERIAL total drop (unlike 3b.1's small
leaf). If it only partially vectorises, still likely a real win from the branch-free restructure. If
negligible, report honestly + the vec-report reason. Read absolute samples, 2-3 runs vs the noise band.

## 9. Out of scope (explicit)

3b.2 (Tier-2 removals via production-private paths), 3c (blend / Tier-3), any Class-B/C change, any
final-output-clamp change, SIMD intrinsics/Path B, buffer-free traversal, CMS/invariant change.
