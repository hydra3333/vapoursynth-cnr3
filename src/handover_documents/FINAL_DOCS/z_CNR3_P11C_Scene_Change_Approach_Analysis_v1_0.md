# CNR3 — P.11C Scene-Change Uniform Wiring: Approach Analysis

**Version:** v1.0
**Date:** 2026-06-27
**Status:** APPROACH-FIRST analysis (pre-scope), same treatment given to D.4/D.5. Verified against
committed source (src.zip) and CMS07.13. NOT a scope; the next step is to take the two composition
seams to the coder for its read on the actual code, then draft a scope.
**Controlling:** CMS07.13 / Production Spec v2.11 / Document A v3.5 / Document B v3.5.1 / slimmed DELTA v4.12.
**Branch:** dev_cache_manager. Baseline: committed through D.5 (branch-(d) recovery arc COMPLETE), 52/52.

---

## 0. What P.11C is (one paragraph)

P.11C wires scene-change handling into the LIVE getFrame path, uniformly across branch-a (fresh-start),
branch-c (predecessor-present), and branch-d (recovery), BEFORE the first real-footage test. The pixel-
level scene-change DETECTION and the RESET behaviour already exist and were proven in the caller-supplied
pixel arc; the CMS design (cut -> checkpoint promotion) is fully settled. What P.11C develops is the
live ENABLING of the existing detector plus the one new live behaviour: a detected cut stores its output
WITH the checkpoint flag set, so the cut becomes a recovery anchor. It is a wiring-and-compose phase
(closer to K.1F than to the recovery arc), with one genuine subtlety: cut detection inside the recovery
branches, where the predecessor is reconstructed and multiple frames fill per activation.

The stated goal (coordinator): "complete whatever it takes to get scene detection and its associated
logic developed and in place."

---

## 1. WHERE scene detection occurs — the recorded decision (answer to "was that recorded?")

YES — recorded durably in CMS07.13. The decision that scene detection happens DURING COMPUTE (a pixel-
layer concern), not as a cache-slot operation, is stated in three places:

- **CMS §1 / §A4** (the causal-algorithm statement): "Scene-change (cut) detection runs *during compute*
  as part of comparing frame N to its predecessor. On a true cut at N, output[N] is computed as a fresh
  start (copy source chroma, skip the recursive blend), so output[N] does NOT depend on output[N-1]. A
  cut therefore SEVERS the recursive chain at N." (A third reset point alongside frame 0 and the floor.)

- **CMS §9.2** (arAllFramesReady compute): "Scene-change detection runs during each compute ... if a cut
  is detected at hole K, output[K] is computed as a fresh start ... and stored WITH the checkpoint flag
  set (Section 6.4); otherwise output[K] = blend(output[K-1], source[K]). Either way it is store-and-
  pinned and the ascending walk continues. A cut severs the chain at K."

- **CMS §9.2 routing skeleton**: the ascending fill loop computes each hole "scene-change-aware (Section
  9.2)". So per-hole detection is SPECIFIED, not a gap.

Plus **CMS §6.4** (cut -> checkpoint), **§6.5** (cut-near-grid: keep both), **§6.6** (checkpoint flag is
monotonic under duplicate stores: promote-only, never demote, flag-set INSIDE the AS2 store atomic),
and **AS6** (checkpoint establish on detected-cut store). The design is complete; P.11C implements it live.

**Net:** the "where does scene detection occur" question is settled in the CMS — it occurs during pixel
compute, per-frame (and per-hole in recovery), and its result drives both the reset and the checkpoint
flag. P.11C does not re-decide this; it wires the existing, settled design into the live path.

---

## 2. The check-calc-check clarification (coordinator's question, verified)

The **check-calc-check (adopt-skip / first-in-best-dressed)** pattern is a CACHE-SLOT concern, and it was
for the HOLE-FILLING / output-slot occupancy under concurrency — NOT for scene detection. (Coordinator's
recollection confirmed.) D.4 proved it as primitives. It answers "is this output frame number already
present? (check) -> if not, compute (calc) -> store, handling a concurrent duplicate (check / first-in-
best-dressed)."

**Scene-change detection is a PIXEL-COMPUTE concern**, separate and sequential. Verified from the live
predecessor-present flow in `cnr3_arAllFramesReady.cpp`:

```
compute pixels   ->   cnr3_process_caller_supplied_vapoursynth_frame_triplet(...)   [detection+reset live here]
then store       ->   cnr3_store_live_output_frame_for_authoritative_return(...)    [the check-calc-check store]
```

Detection sits strictly inside the "calc" step, BEFORE the store. The two patterns are separate and meet
at exactly ONE point: **the store must learn the scene_change result from the pixel-compute summary so it
knows whether to set the checkpoint flag.** That single bit of threading is the new wiring P.11C adds.

---

## 3. What already EXISTS (proven; do NOT rebuild)

Verified present and implemented in committed source:

- **The detector** — `cnr3_detect_scene_change_from_scalar_planes` (cnr3_frame_processing.cpp ~line 623):
  a real diff-threshold detector. It accumulates absolute luma diff (downsampled luma) plus chroma U/V
  diffs into `diff_total`, and sets `stats.scene_change = true` when `diff_total > scene_change_threshold`.
  Inputs (verified signature):
    current_downsampled_luma_plane, previous_downsampled_luma_plane,
    current_source_u_plane, previous_filtered_u_plane,
    current_source_v_plane, previous_filtered_v_plane,
    sub_sampling_w/h, bits_per_sample, Cnr3SceneChangeConfig (carries scene_change_threshold), stats out.

- **The reset** — in the caller (cnr3_frame_processing.cpp ~line 1593): when detection is used AND
  `scene_stats.scene_change` is true, the output chroma is set to a copy of the CURRENT chroma
  (`scene_change_reset_output_used`), bypassing the recursive blend; otherwise the recursive
  `cnr3_process_chroma_plane_from_downsampled_luma` runs (`recursive_chroma_blend_used`). The summary
  reports scene_change_detection_used / scene_change_detected / scene_change_reset_output_used /
  recursive_chroma_blend_used.

- **The CMS design** — §6.4 / §6.5 / §6.6 / AS6 (see §1 above). Fully settled.

- **The cache primitives** — checkpoint store already exists (store_checkpoint_owned_frame), used and
  proven in the D.5 prune-survival selftest. The recovery store/pin path (D.1-D.5) is complete.

---

## 4. What is DEFERRED — what P.11C must actually develop

The live path currently calls `cnr3_process_caller_supplied_vapoursynth_frame_triplet` but DELIBERATELY
passes `scene_config = nullptr`. Verified by the live KDT trace fields: `p11c_called=0
scene_change_deferred=1` on every live branch. So detection EXISTS but is switched OFF live. P.11C is
therefore three wiring tasks, NOT a from-scratch build:

**(A) Enable detection live.** Supply a real `scene_config` (carrying the threshold) into the live
processing call, so detection + reset run live. Uniformly across branch-a / branch-c / branch-d.

**(B) Thread scene_change -> the store.** The new live behaviour: a detected cut stores output[K] WITH the
checkpoint flag set (CMS §6.4 / AS6), via the existing checkpoint-eligible store path; if the slot is
already present, PROMOTE it (raise-only, inside the AS2 store atomic, per §6.6). Threads the scene_change
bit from the process summary into the store decision (currently the store does not receive it).

**(C) Uniformity.** Wire (A) and (B) across branch-a/c/d together — scene-change handled identically
regardless of which branch computed the frame. (This is why scene-change was deferred UNIFORMLY through
the whole D-series rather than per-branch.)

**(D) Reset is free.** Once (A) enables detection, the reset already happens inside the detector/caller.
No new reset logic.

**Threshold source — the one open parameter decision.** `scene_change_threshold` is a config value. Where
does the live `scene_config` get it: a plugin parameter (rpGeneral-style), a compile constant, or a CMS-
defined default? Small but real; settle during scoping.

---

## 5. The interlock with the recovery arc (why this composes cleanly)

Per CMS §6.4, a cut-checkpoint is "just a present output the Phase-1 descending search (§9.5) finds like
any other." So P.11C needs NO new recovery logic — it FEEDS the recovery machinery D.1-D.5 already proved.
P.11C produces checkpoints; the recovery arc consumes them. The cut -> checkpoint promotion is the ONLY
new cache interaction, and §6.6 + the existing AS2 checkpoint store specify exactly how.

---

## 6. The one genuine subtlety (the thing for the coder)

Scene detection INSIDE the recovery branches. Two verified facts make this non-trivial:

1. **The detector compares against the PREVIOUS FILTERED output, not source[N-1].** Verified: the live
   caller fills the detector's `previous_*` chroma planes from `views.previous_filtered_u/v`, and the luma
   comparison uses current vs previous DOWNSAMPLED luma. In the recovery branches the predecessor is
   RECONSTRUCTED (floor-fresh-start or hole-fill output), so detection runs against that reconstructed
   filtered frame.

2. **Recovery fills MULTIPLE frames per activation, any of which could be a cut.** CMS §9.2 already
   specifies per-hole detection ("if a cut is detected at hole K ... stored WITH the checkpoint flag
   set ... the ascending walk continues"). So the detect -> checkpoint-flag threading must occur PER
   FILLED HOLE during the ascending walk, not only for the final output[N].

This is where "wire up existing detection" meets "the recovery machinery just built." It is the part most
worth the coder's read on the actual code seams.

---

## 7. The approach decisions to settle (the questions for the coder)

1. **Threshold source** for the live scene_config (plugin param / compile constant / CMS default).
2. **Cut detection within recovery branches** — predecessor reconstructed; per-hole detection during the
   ascending fill; confirm the existing fill loop's "scene-change-aware" comment maps to a real wiring
   point and that each hole's store can carry the checkpoint flag.
3. **Checkpoint promotion under the store atomic** — route the cut-detected case to the checkpoint store /
   promote path INSIDE the AS2 atomic (§6.6), coexisting with the recovery pin-record (D.5 proved a pinned
   checkpoint survives prune; confirm flag + pin coexist on the same slot in the live store).
4. **Proof strategy** — synthetic footage with constructed cuts, like D.1-D.5: cut detected -> reset used
   (chroma = current, not blended) -> output[K] stored WITH checkpoint flag -> a later recovery finds it
   as an anchor. Live-harness proof (like D.1-D.3), possibly plus a selftest for the promotion primitive
   (like D.4). REAL-FOOTAGE validation is DEFERRED to the later campaign (needs diagnostics + D-SUM-14).
5. **fmParallel interaction** — §6.6's whole rationale is out-of-order scheduling caching a frame non-
   checkpoint before its cut is detected, then promotion fixing it. P.11C's design note must carry the
   interleaving analysis showing the promote path is concurrency-correct (flag monotonic, set inside the
   atomic).

---

## 8. Honest read & recommendation

P.11C is SMALLER than it first looked — detection + reset exist and are proven; the CMS design is fully
settled; the only new live behaviour is the cut -> checkpoint store/promotion, and the only real subtlety
is cut detection inside the recovery branches (reconstructed predecessor, per-hole). It is a wiring-and-
compose phase, closer to K.1F than to the recovery arc.

RECOMMENDATION (matching the D.4/D.5 pattern that landed cleanly): take the approach to the CODER now —
specifically the two composition seams, §7 items 2 and 3 — to get its read on the actual code before
drafting a scope. Then draft the P.11C scope from the combined analysis.

---

## 9. Proof & sequencing context (recorded in the ledger)

- P.11C's proof is SYNTHETIC (constructed cuts), KDT-observable, like D.1-D.5. Real-footage validation
  folds into the later campaign once diagnostics exist.
- The clip-test harness (test_000_Example_576p50.vpy/.bat) has little verification value WITHOUT the
  diagnostics; the large campaign is sequenced AFTER the diagnostics arc.
- Sequence: P.11C (this) -> diagnostics arc (D-SUM families + gates + severity/abort + D-SUM-14 scene-
  change telemetry + D-SUM-02 memory via salvage) -> first verifiable real-footage run + campaign ->
  fmParallel arc.
- D-SUM-14 (scene-change / recursive-reset / checkpoint-promotion summary) is the diagnostics family that
  will later observe P.11C on real footage; it is part of the diagnostics arc, NOT bundled into P.11C.

---

*End of CNR3 P.11C Scene-Change Approach Analysis v1.0.*
