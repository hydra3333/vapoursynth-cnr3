# CNR3 — Lever 0B: redundant inner staging buffer — INVESTIGATE, ASSESS, then IMPLEMENT

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigate-and-assess brief leading to a patch. Read the analysis, verify it against the
committed source, assess the two options and our lean, and either confirm (and implement the chosen
option) or report back if the source says something different. This is Lever 0B ALONE — Lever 1
(pooling), Lever 2 (fusion), Lever 3 (typed-row-pointer) are OUT OF SCOPE.
**Target file:** `cnr3_frame_processing.cpp`.
**Builds on:** Lever 0A (committed) — luma no longer uses this staging path; 0B affects U/V staging.
**Controlling docs:** CMS07.15 (no design change), FI-10. Companion: data-flow map v0.2.

---

## 1. Goal

Remove the redundant DOUBLE buffer + double pass in the scalar->native STAGING path, while keeping the
per-sample failure detection and the outer all-or-nothing commit exactly as-is.

## 2. The redundancy (our reading — please verify from source)

`cnr3_stage_scalar_plane_to_native_bytes` (~L482) is the U/V staging entry point. It:
1. allocates + zeroes the caller's `staged_bytes` (~L497);
2. builds a `staged_plane` view over `staged_bytes`;
3. calls `cnr3_copy_scalar_buffer_to_native_plane(scalar_plane, staged_plane)` (~L514).

`cnr3_copy_scalar_buffer_to_native_plane` (~L2134) — a GENERAL-PURPOSE all-or-nothing copy — then:
4. allocates + zeroes a SECOND buffer `resolved_bytes` (~L2156);
5. converts every scalar sample -> native into `resolved_bytes` via `cnr3_store_native_plane_sample`,
   returning early on any per-sample failure — THIS early-return-before-touching-destination is its
   all-or-nothing mechanism;
6. copies `resolved_bytes` -> the destination (which, in the staging case, IS `staged_bytes`).

So in the staging path the data lands in `staged_bytes` via a throwaway `resolved_bytes` intermediate,
with two allocations and two full passes. **Please confirm this call chain and the two-buffer/two-pass
shape from the committed source before implementing.**

## 3. WHY the intermediate is redundant *for staging specifically* (the key insight)

The `resolved_bytes` intermediate exists so that a mid-conversion failure leaves the REAL destination
untouched — genuine all-or-nothing for a caller writing into a live frame plane. That is correct and
necessary FOR THAT GENERAL CASE.

But in the staging path, the "destination" handed to `cnr3_copy_scalar_buffer_to_native_plane` is
`staged_bytes` — itself a THROWAWAY that is not committed to any real frame until a LATER, SEPARATE gate:
the combined `cnr3_staged_native_active_copy_is_valid(staged_y/u/v)` check followed by
`cnr3_commit_staged_native_active_samples` (~L1767-1776). If conversion fails, the stage returns non-ok,
the frame body returns early, and `staged_bytes` is discarded — the real destination planes are never
touched REGARDLESS of the inner buffer. So for staging, `resolved_bytes` is protecting a buffer that
needs no protection: the atomicity already lives one level UP. That double protection is the redundancy.

## 4. TWO ways to fix it — with our strong lean

### Option A (STRONG LEAN — safer, narrower)
Give the STAGING path its own direct scalar->native conversion that writes into `staged_bytes` in ONE
pass, no intermediate. Concretely: in `cnr3_stage_scalar_plane_to_native_bytes`, after allocating +
zeroing `staged_bytes`, convert scalar -> native DIRECTLY into it (per-sample, with the same per-sample
failure return) instead of delegating to `cnr3_copy_scalar_buffer_to_native_plane`. A small helper like
`cnr3_convert_scalar_plane_into_native_bytes(scalar_plane, staged_plane)` doing only the conversion loop
(no allocation — the caller already allocated) is the clean shape.

**Crucially, Option A leaves `cnr3_copy_scalar_buffer_to_native_plane` COMPLETELY UNTOUCHED**, so its
all-or-nothing guarantee is preserved for every OTHER caller that writes to a real destination. You only
change the staging path; the general-purpose copy and its behaviour are unmodified. Lowest blast radius.

### Option B (NOT our lean — broader, riskier)
Modify `cnr3_copy_scalar_buffer_to_native_plane` itself to skip `resolved_bytes` when it can determine
the destination is throwaway. This touches the GENERAL-PURPOSE function every caller uses, needs a way to
KNOW the destination is throwaway (a flag / separate entry point), and risks the all-or-nothing guarantee
for the callers that genuinely need it — for the same end win. We do NOT recommend this.

**Why A over B:** the redundancy is specific to the *staging* caller, so the fix belongs at the *staging*
caller — not in the shared function that other code relies on for real-destination atomicity. Option A
removes the redundant work exactly where it occurs and cannot regress any other caller. If your source
read reveals that `cnr3_copy_scalar_buffer_to_native_plane` has ONLY the staging caller (no other users),
tell us — that would change the analysis, but we believe it is general-purpose and used elsewhere; please
confirm its call sites.

## 5. CRITICAL CAUTION (the one thing that must not be got wrong)

The inner conversion loop calls `cnr3_store_native_plane_sample` PER SAMPLE, which can FAIL (out-of-range
scalar sample -> non-ok status, checked and early-returned). **The redundancy is the SECOND BUFFER + the
SECOND PASS — NEVER the per-sample validation.** The direct converter MUST keep the per-sample failure
check: an out-of-range scalar sample must still fail the stage (which then fails the whole frame at the
outer gate). Removing the buffer while accidentally dropping the per-sample validation would turn a
"remove redundant work" change into a "remove a safety check" change. Keep the validation; remove only the
duplication.

## 6. What we're asking

1. **Verify** the call chain (§2), the two-buffer/two-pass shape, and — importantly — the CALL SITES of
   `cnr3_copy_scalar_buffer_to_native_plane` (is it truly general-purpose with non-staging callers, as we
   assume?). Correct us if the source differs.
2. **Assess** Option A vs B and our lean. We expect you to confirm A; if you see a reason B is actually
   safer/cleaner, or a third option, say so.
3. **Implement the chosen option** (we expect A) as the patch proposal, preserving the per-sample
   validation (§5) and the outer all-or-nothing gate/commit exactly.

## 7. Correctness argument (for the chosen Option A)

Old staging path: scalar -> resolved_bytes (pass 1, per-sample validated) -> staged_bytes (pass 2, plain
copy). New staging path: scalar -> staged_bytes (one pass, per-sample validated). Final `staged_bytes`
contents identical (same conversion, same layout, same zeroed padding); only the throwaway intermediate
and its redundant second copy are removed. Per-sample failure still returns non-ok before any commit.
Outer all-or-nothing gate untouched -> destination atomicity identical. No value change.

## 8. Proof gate

```text
1. Build Debug + Release (both projects), dev-trace ON, /arch:AVX2 on.
2. Four-way selftest:
     Debug   normal            56/56 PASS exit 0
     Release normal            56/56 PASS exit 0
     Release --force-fail...   55/56 FAIL exit 1
     Release --verbose         56/56 PASS exit 0
3. P.11B must pass UNCHANGED (U/V blended vs previous filtered output; invalid late-sample paths publish
   no partial destination). P.8A native-store proofs must pass unchanged. Any assertion edit needed = RED
   FLAG (observable result must be identical).
4. Re-profile vs the post-0A baseline (profiler_test_01, -r 1, normal build, dev-trace OFF). Report:
     - total per-frame change vs post-0A;
     - change in cnr3_stage_scalar_plane_to_native_bytes cost;
     - change in per-frame allocation count / ntdll self-time (one fewer alloc per staged plane);
     - confirm cnr3_copy_scalar_buffer_to_native_plane is off the U/V staging hot path (Option A).
```

## 9. Hard constraints

```text
DO:
  - Remove the inner resolved_bytes allocation + second pass FROM THE STAGING PATH only (Option A).
  - Convert scalar -> native directly into the caller-provided staged_bytes, one pass.
  - KEEP the per-sample failure detection (see §5).
  - Preserve staged_bytes size/layout byte-for-byte (stride*height, zero-init, active placement, padding).

DO NOT:
  - Modify cnr3_copy_scalar_buffer_to_native_plane or its all-or-nothing behaviour (that is Option B).
  - Remove/weaken the per-sample validation.
  - Change cnr3_stage_scalar_plane_to_native_bytes' signature, the outer commit ordering, or the validity gate.
  - Touch luma passthrough (0A), the downsample guide, or chroma blend maths.
  - Introduce typed-row-pointer traversal (Lever 3) or buffer pooling (Lever 1).
```

## 10. Expected result (calibration, not a target)

Smaller than 0A: U/V staging only, removing a redundant buffer + pass rather than a whole round-trip.
Expect low-single-digit-percent plus one or two fewer allocations per frame. If negligible or negative,
report it — do not tune to a number.
