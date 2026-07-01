# CNR3 PATCH SCOPE — Lever 0A: staged native full-res luma passthrough

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21 — proven pixel-path code). Implement exactly this; propose the
patch back for review before commit. Confirmed separation with coder: this is Lever 0A ALONE; Lever 0B
(redundant inner staging) is a separate later patch and is OUT OF SCOPE here.
**Target file:** `cnr3_frame_processing.cpp` (the caller-supplied real-frame pixel-composition body).
**Controlling docs:** CMS07.15 (no design change), FI-10. Companion: data-flow map v0.2.

---

## 1. Goal (one sentence)

Delete the full-resolution luma native->int->native round-trip (the largest pure-marshalling plane, and
the one that does NO math), replacing it with a native luma passthrough staged into `staged_y`, while
preserving the existing all-or-nothing final Y/U/V commit discipline exactly.

## 2. Why (the finding this acts on)

Full-res luma is currently unpacked from `views.current_source_y` into `current_luma_storage`
(`std::vector<int>`), then staged back to native bytes and committed — but it is read by NOTHING else
(not the downsample, not scene-change, not the chroma blend; CNR3 filters chroma, luma is passthrough).
So the entire int round-trip for the largest plane is pure overhead. Profiling attributes ~50% of
per-frame time to native<->scalar marshalling; luma is the biggest single plane in it (full-res, ~4x a
chroma plane's area). This is the safest possible first cut: it touches the plane with no arithmetic.

## 3. Current code (the exact sites, for reference)

- **L1387:** `std::vector<int> current_luma_storage;` — declaration.
- **L1397-1401:** `cnr3_allocate_scalar_plane_storage(luma_width, luma_height, current_luma_storage)` —
  allocation.
- **L1487-1496:** builds `current_luma_mutable` view over `current_luma_storage`, then
  `cnr3_copy_native_plane_to_scalar_buffer(views.current_source_y, current_luma_mutable)` — the UNPACK
  (native -> int). THIS is the ~17%-self `load_native_plane_sample` cost for the luma plane.
- **L1711-1727:** builds `const Cnr3ConstPlaneBufferView current_luma{...}` over `current_luma_storage`,
  then `cnr3_stage_scalar_plane_to_native_bytes(current_luma, views.destination_y, staged_y)` — the
  REPACK (int -> native staging).
- **L1767-1776:** the atomic tail — validate `staged_y/u/v`, then
  `cnr3_commit_staged_native_active_samples(staged_*, views.destination_*)` for all three. THIS ORDERING
  IS THE ATOMICITY INVARIANT AND MUST NOT CHANGE.

## 4. What to change

Replace the luma unpack + luma-repack with a single native passthrough that fills `staged_y` directly
from `views.current_source_y`, landing at the SAME `staged_y` the existing commit consumes.

**DELETE:**
- The `current_luma_storage` declaration (L1387).
- Its allocation (L1397-1401).
- The `current_luma_mutable` view + `cnr3_copy_native_plane_to_scalar_buffer(views.current_source_y, ...)`
  unpack (L1487-1496).
- The `current_luma` const-view + `cnr3_stage_scalar_plane_to_native_bytes(current_luma,
  views.destination_y, staged_y)` call (L1715-1727) — because `staged_y` will now be filled natively
  instead.

**ADD:** a native active-row passthrough that fills `staged_y` from `views.current_source_y` with the
same size/shape the existing staging produced, so the downstream validity check
(`cnr3_staged_native_active_copy_is_valid(staged_y, views.destination_y)`) and
`cnr3_commit_staged_native_active_samples(staged_y, views.destination_y)` see an identical `staged_y`.

- Prefer a small dedicated helper (e.g. `cnr3_stage_native_plane_passthrough(source_native_view,
  destination_shape, staged_bytes)`) that copies the ACTIVE region row-by-row honoring both source and
  destination strides, mirroring the exact staged-bytes layout
  `cnr3_stage_scalar_plane_to_native_bytes` produces (same `stride_bytes * height` sizing, same
  zero-init of padding, same active-sample placement) — so `staged_y` is byte-for-byte what it was.
- The source luma is already native bytes; this is a native->native active-region copy, NOT an int
  round-trip. No `load_native_plane_sample` per-sample call, no `std::vector<int>`.
- Keep the same early-return-on-error shape the current staging call has.

**DO NOT MOVE the commit.** `staged_y` is still committed only in the existing L1774 tail, after the
combined `staged_y/u/v` validity gate. The luma bytes must NOT reach `views.destination_y` before U/V
validation.

## 5. Hard constraints (do / do not)

```text
DO:
  - Remove current_luma_storage, its allocation, the native->int luma unpack, and the int->native luma repack.
  - Fill staged_y natively from views.current_source_y (active-region, stride-honoring, padding-zeroed
    to match the existing staged-bytes layout exactly).
  - Preserve the existing final Y/U/V validity-gate + commit ordering byte-for-byte in behaviour.
  - Match the staged_y contents the old path produced (so the validity check and commit are unchanged).

DO NOT:
  - Write to views.destination_y before the U/V validity gate (breaks late-failure atomicity — the
    original direct-copy idea did this; it is WRONG).
  - Touch current_downsampled_luma_storage / previous_downsampled_luma_storage or the native downsample
    read (views.current_source_y / views.previous_filtered_y -> the blend GUIDE). Luma-as-guide is a
    SEPARATE read and stays exactly as-is.
  - Touch U/V scalar staging, output_u/v, or chroma blend maths.
  - Remove or alter cnr3_copy_scalar_buffer_to_native_plane / the inner resolved_bytes staging (that is
    Lever 0B, a separate patch).
  - Introduce any typed-row-pointer chroma traversal (that is Lever 3).
  - Change cnr3_stage_scalar_plane_to_native_bytes' signature or behaviour (U/V still use it unchanged).
```

## 6. Correctness argument (why this is value-preserving)

The old path: native luma -> int buffer -> native staged bytes -> commit. The new path: native luma ->
native staged bytes -> commit. Both produce the same `staged_y` (the luma is copied UNCHANGED in both;
no math is applied to luma anywhere), so the committed `destination_y` is byte-identical. The int buffer
was only ever a pass-through container for luma. Removing it changes no value. The atomicity invariant is
untouched because the commit ordering and the combined validity gate are unchanged.

## 7. Proof gate (all required before the patch is accepted)

```text
1. Build Debug + Release (both projects), CNR3_KEYSTONE_DEV_TRACE ON, /arch:AVX2 on (unchanged).
2. Four-way selftest:
     Debug   normal              56/56  PASS
     Release normal              56/56  PASS
     Release --force-fail...     55/56  FAIL, exit 1
     Release --verbose           56/56  PASS
3. P.11B must still prove, specifically:
     - output luma is staged and copied UNCHANGED from current source luma;
     - chroma U/V blended vs previous filtered output (untouched);
     - invalid late-sample paths leave destination planes UNCHANGED (the atomicity this patch preserves).
   If P.11B needs any assertion adjustment to reflect the native (vs int-staged) luma path, that is a
   RED FLAG — the observable result must be identical, so P.11B should pass UNCHANGED. Flag it if not.
4. Re-profile against the fresh AVX2 baseline (profiler_test_01, -r 1, normal build, dev-trace OFF for
   timing). Report:
     - total per-frame time change;
     - change in cnr3_load_native_plane_sample self-time (should DROP — luma unpack gone);
     - change in the luma staging/commit cost;
     - confirmation the luma int round-trip left the hot profile;
     - one fewer std::vector<int> allocation per frame (current_luma_storage gone).
```

## 8. Expected result (calibration, not a target)

A modest but real win: removes the largest-plane scalar round-trip and one per-frame int allocation. It
will NOT fix chroma unpack/repack or downsample marshalling (those are later levers). If the measured
change is negligible or NEGATIVE, that is itself a finding — report it; do not tune to hit a number.

## 9. Out of scope (explicit, so the patch stays clean)

Lever 0B (redundant inner `resolved_bytes` staging), Lever 1 (buffer pooling), Lever 2 (pass fusion),
Lever 3 (typed-row-pointer), any chroma-path change, any CMS/invariant change, any harness change (the
`.vpy`/`.bat` are the designer's deliverable). This patch is luma-passthrough-staging and nothing else.
