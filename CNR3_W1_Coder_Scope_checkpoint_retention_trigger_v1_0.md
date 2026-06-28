# CNR3 W.1 — Coder Build Scope: independent checkpoint-retention prune trigger (CMS07.14 §7.4)

**Phase:** W.1 (first of the live cache-pressure wiring arc; the one genuinely NEW primitive).
**Authority:** CMS07.14 §7.4 (the decision), §7.1/§7.2/§7.3 (existing prune rules), §6.3 (the retention bound).
**Provenance:** Step 0 review, SR-C-04 = (B); `CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md`.
**Scope discipline:** read-first / propose / review / prove — like a K or D phase. This is a cache-core +
selftest phase. NO live getFrame wiring in W.1 (that is W.2/W.3). NO code until this scope is reviewed and
approved.

---

## 0. Why W.1 is a real primitive, not a wire-up

Every other piece of the wiring arc is "call an already-proven pass." W.1 is the exception: the cache core
today has NO trigger that fires on the checkpoint-flag count alone. CMS §7.4 records that it must, because on
cut-heavy content (every detected cut is flagged, §6.3/§6.4) the flagged count can exceed
`CHECKPOINT_MAX_RETAIN` (48) while total slots stay under the §7.2 capacity trigger (165) — so today
`CHECKPOINT_MAX_RETAIN` is not enforced. W.1 adds the trigger that enforces it. It is small, but it is NEW
selection-driving logic, so it gets full proof care.

---

## 1. READ-FIRST (orient before proposing; do not modify yet)

Read and confirm your understanding of these existing pieces. The whole point of W.1 is to REUSE the first
one and AVOID the second.

1. **`select_composite_prune_candidates_bounded_locked`** (cnr3_cache_core.cpp ~line 2416) — the selection
   path W.1 REUSES. Confirm it already: computes a checkpoint-select budget from
   `checkpoint_count_locked() - retain_checkpoint_count`; skips slots with `pin_count != 0` or
   `frame_is_inside_hot_zone_locked(frame)`; never selects frame 0; orders by
   `nearest_active_hot_zone_boundary_distance_locked` (greatest-distance-first). This is exactly the
   §7.4-required safety behaviour — so W.1 must drive THIS selector, not re-implement selection.
2. **`remove_unpinned_checkpoints_above_retain_count_bounded_locked`** (~line 1150) — the helper W.1 must
   NOT reuse as the live selection path. Confirm WHY: it does not honour hot-zone membership / distance
   ordering, so it can evict a checkpoint that is currently zone-protected (e.g. a recovery anchor). It may
   remain in the tree for its existing callers/tests; W.1 simply does not route the new trigger through it.
3. **`cnr3_calculate_cache_prune_trigger_decision`** (~line 503) + **`Cnr3CachePruneTriggerDecision`** struct
   (cnr3_cache_core.h ~387) — today keys ONLY on `current_slot_count > overflow_trigger`. W.1 adds a parallel
   checkpoint decision; see §3 for whether to extend this struct or add a sibling.
4. **`checkpoint_count_locked()`** (~line 1353) returns `checkpoint_slot_positions_.size()` — the flagged
   count. Confirm: there is NO checkpoint pool; `is_checkpoint` is a flag on the unified `slots_`; this count
   is an index size. (Step 0 terminology correction.)
5. Constants: `CNR3_CACHE_CHECKPOINT_MAX_RETAIN = 48`, `CNR3_CACHE_CHECKPOINT_MIN_RETAIN = 10`.
6. The last cache-core selftest (`...selftest_p11c5_scene_cut_checkpoint_recovery_anchor`) and its
   registration block — W.1's selftest mirrors this structure and bumps the count.

Report back (read-first outcome) confirming each, BEFORE proposing the patch. Flag any mismatch with the
above as a finding — do not paper over it.

---

## 2. The primitive's contract (what W.1 must build)

A new locked checkpoint-retention TRIGGER + its drive of the existing composite selector. Stated as a
contract:

**Trigger condition.** A checkpoint-retention prune is REQUIRED when
`checkpoint_count_locked() > CNR3_CACHE_CHECKPOINT_MAX_RETAIN`, evaluated independently of the §7.2 capacity
trigger (i.e. it may be required even when `current_slot_count <= overflow_trigger`).

**Target.** Reduce the flagged count toward `CNR3_CACHE_CHECKPOINT_MIN_RETAIN`. This is a SOFT target: a
flagged slot that is pinned, frame 0, or inside an active hot zone is RETAINED even if that keeps the count
above the target (§6.3 "limits are SOFT triggers"). So the post-pass flagged count may exceed MIN_RETAIN, or
even MAX_RETAIN, if enough flagged slots are protected — that is correct, not a failure.

**Selection.** ONLY checkpoint-flagged slots are candidates. Selection MUST use the existing composite
selector's checkpoint path (the one that honours pin_count == 0, outside-every-hot-zone,
frame != 0, greatest-hot-zone-distance-first). Non-checkpoint slots are never touched by this trigger.

**Bounding.** K-bounded, like §7.3 — at most `max_remove_count` victims per pass (caps lock-hold for burst
trims). Repeated passes converge toward the target across activations.

**Atomicity (§7.3).** DECIDE + DETACH under one `cache_mutex_`; FREE the detached `VSFrame*` refs in a
post-lock batch. No freeFrame under the lock.

**Composition with §7.2 (for W.3, but design for it now).** In the live combined helper (§7.5 step 5) the
§7.2 capacity trigger and this §7.4 checkpoint trigger are evaluated in the SAME locked step; when both fire,
both contribute to one bounded selection. W.1 must build the checkpoint trigger so it can be invoked EITHER
standalone (this phase's selftest) OR alongside the capacity trigger (W.3) without restructuring.

---

## 3. One design question for the coder to answer in the proposal

The existing `execute_bounded_prune_pass_locked` evaluates the capacity decision then calls the composite
selector. W.1 needs the checkpoint trigger reachable when capacity is NOT over threshold. Two shapes —
propose which, with reasoning:

- **(3a) Extend the trigger decision:** add a `checkpoint_prune_is_required` (and target) to
  `Cnr3CachePruneTriggerDecision`, compute it in `cnr3_calculate_cache_prune_trigger_decision` (which then
  also needs `checkpoint_count`), and let `execute_bounded_prune_pass_locked` proceed to selection if EITHER
  `prune_is_required` OR `checkpoint_prune_is_required`. Selection already handles the checkpoint budget.
- **(3b) Sibling pass:** leave the capacity decision untouched; add a separate
  `execute_checkpoint_retention_prune_pass(_locked)` that checks `checkpoint_count > MAX_RETAIN` and drives
  the composite selector with non-checkpoint selection disabled. The combined helper (W.3) calls both.

Designer lean: **(3a)** — one decision struct, one pass, EITHER trigger proceeds to one composite selection;
it composes most cleanly into the §7.5 single-step-5 and avoids two selection call sites. But the coder owns
the feasibility call; if (3a) muddies the capacity-only callers/tests, (3b) is acceptable. State your choice
and why in the proposal.

---

## 4. Selftest obligations (the proof — this is a selftest phase)

Add ONE new cache-core selftest (mirror the P.11C.5 registration pattern; bump count 53 -> 54). It must prove
the trigger fires below capacity AND honours every protection. Required assertions, as discrete scenarios in
one test (or a small registered set if cleaner):

1. **Fires below capacity:** store > MAX_RETAIN (e.g. 60) checkpoint-flagged slots, all unpinned, none in a
   hot zone, total slot_count BELOW the capacity overflow trigger. Assert: checkpoint trigger required;
   flagged count reduced toward MIN_RETAIN; non-checkpoint slots untouched; `total_pin_count == 0` throughout.
2. **Frame 0 never pruned:** include frame 0 as a flagged slot; assert it is never selected.
3. **Pinned checkpoint retained:** pin one flagged slot; assert it survives even though the count is over
   target (soft-retain), and pin balance is correct after.
4. **Hot-zone checkpoint retained:** place one flagged slot inside an active hot zone; assert it is not
   selected (protected by zone, not pin).
5. **Distance ordering:** with several evictable flagged slots, assert greatest-hot-zone-distance-first
   selection order (same comparator as the composite selector).
6. **K-bound:** with more removable than `max_remove_count`, assert at most `max_remove_count` detached in one
   pass, and that a second pass continues toward the target.
7. **Capacity untouched when only checkpoint trigger fires:** assert a below-capacity checkpoint trim does NOT
   evict non-checkpoint slots.
8. **freeFrame-after-unlock:** detached victim refs freed in the post-lock batch (mirror the existing
   prune-pass atomicity assertions).

Designer will compute / ratify golden counts for scenario 1 (how many of 60 flagged are removable given
MIN_RETAIN=10 and the protections) when reviewing the proposal — propose your construction and I will confirm
goldens, as in prior phases.

---

## 5. What W.1 must NOT do

- NO live getFrame / arInitial / arAllFramesReady wiring (W.2/W.3).
- NO reuse of `remove_unpinned_checkpoints_above_retain_count_bounded_locked` as the new selection path.
- NO change to §6.3 prose, the constants, MAX/MIN_RETAIN values, or any AS scope.
- NO change to non-checkpoint capacity behaviour (§7.2) other than (under 3a) extending the decision struct.
- NO new behaviour smuggled in (e.g. demoting checkpoints, touching frame data) — selection + detach only.

---

## 6. Deliverable + cadence

1. **Read-first outcome** (confirm §1) + **proposal** (answer §3; sketch the selftest construction for §4) —
   for designer review. NO patch yet.
2. After approval: canonical-LF patch (cache-core + selftest + registration; count 53 -> 54).
3. Four-way after apply/build (Debug 54/54 / Release 54/54 / forced-fail 53/54 exit 1 / verbose 54/54).
4. Designer verifies selftest output (esp. scenario 1 goldens + the soft-retain scenarios) -> commit.

Marker on commit: `CMS07-W.1-checkpoint-retention-trigger` (or the agreed marker scheme). This closes the
"new primitive" risk; W.2 (hot-zone observation wiring) and W.3 (the combined live helper) are then wiring of
proven componentry.
