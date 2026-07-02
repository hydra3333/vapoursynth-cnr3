# CNR3 PATCH SCOPE — Lever E: scene-change local-accumulator + inline hygiene

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (R-PROCESS-21). Implement exactly this; propose back for review before commit.
**Lever E ALONE.** Small cumulative-fps win; VALUE-IDENTITY is the whole game here (see the early-exit note).
**Target:** `cnr3_frame_processing.cpp`, `cnr3_detect_scene_change_from_scalar_planes`.
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** ... + F/3c + Staging. Cumulative ~-79%.

---

## 1. Goal

The scene-change detector runs per frame. Its accumulation loop calls `cnr3_plane_sample_at` x6 and
`cnr3_add_scene_diff` / `cnr3_abs_int64` per sample, accumulating into `stats.diff_total` (a struct field)
through a status-returning helper. This is the A-lite/E hygiene move: inline the per-sample calls, use
row pointers, and accumulate into a LOCAL int64 — then write the local back to stats.diff_total. Small,
per-frame, low-risk. NOT expected to move the total much (it is not a top leaf); worth it only for
cumulative fps under the every-cycle framing.

## 2. THE LOAD-BEARING CONSTRAINT — preserve the per-sample early-exit EXACTLY

This is the whole risk in E. The current loop has, PER SAMPLE, after accumulating:
```text
if (stats.diff_total > config.scene_change_threshold) { stats.scene_change = true; return ok; }
```
So the loop STOPS at the first sample that crosses the threshold, and `stats.diff_total` at return is the
PARTIAL sum up to and including that sample — NOT the full-plane sum. Two things follow that the patch MUST
preserve bit-exactly:
```text
(a) The BOOLEAN decision (scene_change true/false) — obviously.
(b) The REPORTED diff_total VALUE at return — it is the partial sum at the trip point. If you accumulate
    into a local and only check the threshold PER ROW (or at loop end), you will OVERSHOOT: diff_total will
    include samples PAST the trip point, changing the reported value even though the boolean is the same.
    P.11B ("strict diff_total > threshold fires reset; equality keeps recursive chroma blend active") and
    any test reading diff_total exactly WILL catch this.
```
Therefore: **keep the threshold check PER SAMPLE** (after each sample's accumulation, in the same order:
luma, then U, then V when scene_chroma). The local accumulator must be checked against the threshold at the
SAME points the current code checks stats.diff_total, and on trip, write the local back to stats.diff_total
BEFORE returning so the reported value matches. Do NOT hoist the check to per-row.

## 3. What to change (hygiene only, semantics identical)

```text
- Row pointers for the 6 input planes (they are scalar int planes; __restrict where non-aliasing).
- Inline cnr3_plane_sample_at (row[x]) — remove the per-sample call.
- Accumulate into a LOCAL std::int64_t diff_total_local (mirrors stats.diff_total), inline the
  cnr3_abs_int64 and the add (cnr3_add_scene_diff's overflow-checked add — see constraint below).
- Preserve the luma << luma_scale_shift, the abs, the scene_chroma-gated U/V adds, in the SAME ORDER.
- Keep ++stats.samples_examined per sample (or mirror locally + write back — must match final value).
- Per sample, after the same accumulation, check diff_total_local > threshold; on trip: write
  diff_total_local -> stats.diff_total, set scene_change, (write samples_examined), return ok.
- On normal loop completion: write diff_total_local -> stats.diff_total (and samples_examined).
- Keep the per-sample Tier-2 range validation OR handle per policy — see constraint.
```

## 4. Constraints (do / do not)

```text
DO:
  - Preserve the per-sample early-exit and the EXACT diff_total value at every return point (section 2).
  - Preserve cnr3_add_scene_diff's OVERFLOW behaviour: it is a checked add returning status. If you inline
    the add, you MUST reproduce its overflow check (it returns a status on overflow -> the loop returns that
    status). Do NOT silently drop overflow detection by using a raw += . Reproduce the exact add semantics,
    or KEEP calling cnr3_add_scene_diff (the call is cheap relative to the risk — inlining the ADD is
    optional; inlining plane_sample_at is the real win). PREFER keeping cnr3_add_scene_diff unless you can
    prove the overflow semantics are reproduced exactly.
  - Preserve accumulation ORDER (luma, U, V) — int64 add is associative so order is value-neutral for the
    SUM, but the per-sample threshold check means the TRIP POINT is order-sensitive; keep the order.
  - Validation: the 6 inputs are Tier-2 (came through Tier-1). Per policy the per-sample range checks are
    removable in a production-private path IF you add an up-front pre-pass OR keep them. Given E is a small
    win and correctness-sensitive, PREFER keeping the per-sample checks (they are not the bottleneck) unless
    a hoisted pre-pass is trivially clean. Do NOT weaken the guarantee.
DO NOT:
  - Hoist the threshold check to per-row / loop-end (breaks the reported diff_total — section 2).
  - Drop cnr3_add_scene_diff's overflow check.
  - Change scene_chroma gating, luma_scale_shift, abs, or accumulation order.
  - Introduce SIMD intrinsics.
  - Touch any other function. Lever E alone.
```

## 5. Correctness argument

The sum is accumulated identically (same terms, same order, same abs, same shift); the only change is
call-inlining + a local mirror of stats.diff_total. The per-sample threshold check is preserved at the same
points, so the trip point — and thus the reported diff_total and the boolean — are identical. Overflow
semantics preserved (kept via cnr3_add_scene_diff or exactly reproduced). So value-identical on all inputs.

## 6. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2.
2. Four-way selftest, dev-trace ON: 56/56 / 56/56 / 55/56 exit 1 / 56/56.
3. Value-identity: P.11B (the scene-change trace: "strict diff_total > threshold fires reset; equality
   keeps recursive chroma blend active") UNCHANGED — this is THE gate; it reads the diff_total/threshold
   relationship. Plus any P-series scene-change vector. Any assertion edit = RED FLAG (means the trip point
   or reported value diverged).
4. Profile vs post-Staging ~19,470 baseline, 2-3 runs. Report total + scene-change function self-time.
   Save Call Tree to a file under /mnt/user-data/uploads/ (read-from-disk is the reliable path).
5. Value-preserving or it is wrong.
```

## 7. Expected result

Scene-change is per-frame but was NOT a top profile leaf, so expect a SMALL change (low single digit at
most), possibly flat. Commit if value-clean AND (measurably helps OR materially cleaner). This is a
cumulative-fps cleanup, not a headline lever. Honest either way.

## 8. Out of scope

D (SIMD downsample — separate, under investigation), Lever B (pooling), the chroma-unpack fusion (Path C -
not pursued), any other function, SIMD intrinsics, CMS/invariant change.
