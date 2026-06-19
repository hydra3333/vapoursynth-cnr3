# CNR3 CMS — Future Investigations and Open Questions

**Date:** 2026-06-19
**Pairs with:** the CNR3 Cache Manager Design Specification (CMS) at the **identical
version number**, which is carried in this document's filename
(`CNR3_CMS_Future_Investigations_and_Open_Questions_v7.3.md` pairs with CMS07.3).
This document carries **no internal version number** — its version is its filename,
and that filename always matches the prevailing CMS version. The date above reflects
this document's last **content** change (which may be older than the filename version,
because this document re-versions in lockstep with the CMS even when its content is
unchanged — see the maintenance rule below). This v7.3 issue is both a lockstep
re-version to pair with CMS07.3 AND a genuine content change (the new FI-02 entry below),
so the date is updated.

---

## 0. Status — NON-NORMATIVE. NOT the design. NOT for implementation.

**This document is not part of the CMS and is not controlling.** It records open
design questions, deferred tuning, and investigations to be resolved later. It exists
so that good questions are not lost between sessions, and so that the CMS itself stays
purely normative (it states only what is decided and binding).

Read and obey the following:

- **Nothing in this document is a design rule.** Nothing here describes current,
  required, or approved behaviour.
- **Nothing here is implemented without formal prior approval.** An item leaves this
  document and becomes real ONLY by being adopted into the CMS through the normal
  design-change process (proposal → review/sign-off → a new CMS version). Until that
  happens, an item here has no authority of any kind.
- **This document is NOT part of the coder handover pack.** A coder restart chat must
  never receive it as an input, so it can never be mistaken for a directive. It is a
  designer/coordinator working document only.
- If you are implementing CNR3, the CMS (and the Production Spec §3A process rules) are
  your authorities. This document is background context for *future* design discussion,
  nothing more.

**Maintenance rule (lockstep versioning).** When the CMS version is bumped, this
companion is re-issued at the **identical** version number — even if its content has
not changed and only the filename changes. The rule is simply: *the companion and the
CMS always carry the same version number.* This guarantees zero ambiguity about which
companion goes with which CMS. The internal **date** is updated only when the content
actually changes, so the date tells you when an item was last genuinely revised, while
the filename tells you which CMS the document is paired with.

**How an item is resolved (lifecycle).** An open item proceeds:
1. recorded here as an open question, with its description and discussion-to-date;
2. assessed when its trigger arrives (or when the designer/coordinator chooses);
3. if adopted → written into the CMS as a real design change (new CMS version, with the
   usual diff-verify and sign-off), and the relevant build-config constants/coherence
   comments updated to match;
4. then removed from the open list here (optionally recorded in a short resolved log at
   the bottom, so the history of *why* a change was made is preserved).

---

## 1. Index of open items (maintained)

Each entry is a one-line descriptor. Full detail is in the correspondingly-numbered
section below. This index is kept current as items are added or resolved.

```text
FI-01  FORWARD_RADIUS tuning for higher thread counts — hot-zone forward window
       (=10) is sized for ~6 threads; likely needs raising (≈12) for higher
       parallelism. Efficiency-only; correctness is never affected. OPEN.
FI-02  Sparse-plan / recompute-avoidance recovery (the deferred AS3 work) — the
       current minimal recovery planner is nearest-anchor + contiguous-hole, so AS3
       has no reachable trigger and is deferred (CMS §9.6). A future sparse planner
       would give AS3 a job; the C.13B contiguity guard is the tripwire that must be
       revised when this is undertaken. OPEN.
```

---

## 2. Open items (full description and discussion-to-date)

---

### FI-01 — FORWARD_RADIUS tuning for higher thread counts

**Status:** OPEN — investigation deferred to the fmParallel tuning phase.

**Trigger (when to assess this):** when fmParallel tuning is undertaken, or when CNR3
is run at higher thread counts (roughly 12 threads and above), or whenever the
recovery-search / recompute diagnostics show elevated recompute under high-thread load.

**The constant in question.** `HOT_ZONE_FORWARD_RADIUS = 10` (CMS §10.x). The hot zone
for an arriving frame `F` spans `[F - BACK_RADIUS, F + FORWARD_RADIUS]`
= `[F - 50, F + 10]` (CMS §5.4). The zone is deliberately asymmetric: it protects 50
frames *behind* the active frame and 10 frames *ahead*.

**Why the forward window matters, and how it is used.** In normal forward playback the
hot zone advances toward higher frame numbers. The forward radius is the *anticipatory*
cushion: it keeps frames just ahead of the active frontier prune-protected, because the
zone is about to advance into them (they are likely-needed-soon). Frames more than 10
ahead are outside the zone and therefore prune-eligible; frames more than 50 behind
likewise fall out of the zone and become the genuinely-cold eviction candidates. This
asymmetric shape (50 back / 10 forward) is what gives prune its effective directional
bias in forward motion — the behind-frames become the evictable ones, the just-ahead
frames stay protected — without the prune *ordering* itself needing to be directional
(prune ordering is "greatest-distance-from-zone-first", CMS §7.1 / §5.5).

**Empirical basis of the current value (CMS §9A.9).** FORWARD_RADIUS=10 rests on
measured linear-encode request jitter from the prior design's simulation: p99 forward
jitter ≈ 8 under concurrent linear encoding. It is measured-and-margined, not arbitrary
— 10 covers a ~8-frame forward jitter with a small margin.

**Discussion to date — effect of thread count.** The key fact (CMS §10.3) is that
`MAX_HOT_ZONES = 5` scales with concurrent *distinct access regions*, NOT with thread
count: a linear pipe-to-ffmpeg workload uses ~1 zone regardless of how many threads run.
So more threads on linear work do not create more zones; they widen the *spread of
in-flight frames near the single advancing frontier*. The question is therefore whether
FORWARD_RADIUS=10 covers the forward spread of the in-flight frames under N threads:

- **~6 threads:** ~6 frames in flight near the frontier; forward spread comfortably
  within the measured jitter (~8). The leading frames sit inside the +10 window →
  protected. FORWARD_RADIUS=10 is **ample**.
- **~12 threads:** ~12 frames potentially in flight; forward spread is at or near the
  edge of the +10 window. This is the **thinning-margin** case: 10 covers ~8 jitter
  with margin, but tightly-packed 12-in-flight could occasionally reach +11/+12.
  **Probably still adequate**, and any miss is a cheap recompute (see correctness note).
- **~16 threads:** ~16 frames in flight; forward spread can plausibly exceed +10 (the
  leading frame might sit at +14/+15), placing some leading frames outside the zone and
  hence prune-eligible by the distance rule. At this level the +10 cushion is likely
  **too small** for the anticipatory role.

**Correctness is never affected — this is an efficiency matter only.** Pins, not zones,
guarantee the in-flight set (CMS §5.1, §5.2). Any frame a thread is actively computing
or holding as a predecessor is pinned, and §7.1's eviction predicate excludes pinned
slots unconditionally (`pin_count == 0` is the first conjunct). So even if the forward
cushion is overrun at high thread counts, the frames that matter are protected by pins;
the forward radius overrunning costs *recompute pressure* (a frame anticipated-but-not-
yet-pinned may be pruned and later recomputed), never wrong output. This is exactly the
§5.2 guarantee: the zone scheme's soft spots are harmless inefficiencies because pins
underwrite correctness.

**Likely change when this is assessed.** Raise `FORWARD_RADIUS` (a candidate value of
≈12 has been suggested as a first step for higher thread counts). This is a tuning
adjustment, not a redesign.

**Ripple effects to assess BEFORE changing the value** (these are why it is a CMS change,
not a free edit — the constants and their coherence rules live in CMS §10.x):

- **CR1 — JUMP_THRESHOLD is DERIVED** = `FORWARD_RADIUS + BACK_RADIUS + 1`. It is never
  set independently, so raising FORWARD_RADIUS automatically raises JUMP_THRESHOLD;
  confirm the derivation is recomputed (e.g. FORWARD_RADIUS 10→12 makes JUMP_THRESHOLD
  61→63).
- **CR4 — memory budget / headroom.** `active_ceiling ≥ ~2 × max-protected set`, where
  `max-protected ≈ MAX_HOT_ZONES × (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool`.
  Raising FORWARD_RADIUS raises the protected-set size (e.g. with the current others,
  5×(50+12)+48 ≈ 358 vs the prior ~348); confirm active_ceiling (1000) still
  comfortably exceeds ~2× that, which it does with large margin.
- **decay_margin bound.** Must remain `FORWARD_RADIUS ≤ decay_margin ≤ BACK_RADIUS`
  (currently 10 ≤ 20 ≤ 50). Raising FORWARD_RADIUS to 12 keeps the bound satisfied
  (12 ≤ 20 ≤ 50); but if FORWARD_RADIUS were ever raised above decay_margin (20),
  decay_margin would need raising too.

**How the need will be detected (diagnostics-informed).** The recovery-search summary
diagnostics (the D-SUM family covering search depth, holes filled, and recompute
pressure — e.g. D-SUM-03 and the hot-zone D-SUM-11 counters) are the instruments that
reveal whether the forward cushion is being overrun under a given thread count. The CMS
already anticipates validating the tuning guesses (K, MAX_RETAIN, decay_margin, B, and
by extension the radii) against a real run. So the decision to raise FORWARD_RADIUS
should be driven by observed recompute pressure at the target thread count, not by
guesswork.

**Authority note.** FORWARD_RADIUS and the CR coherence rules are CMS-owned (§10.x).
Changing the value is therefore a CMS revision (new CMS version, diff-verified), and the
coherence comments (CR1–CR5, decay_margin bound) in the build-config header
(`cnr3_build_config.h`, established in the G.1A constants phase) must be updated to match
the new value. This is not a Production Spec §3A change (it is design, not process) and
not a diagnostics-spec change.

---

### FI-02 — Sparse-plan / recompute-avoidance recovery (the deferred AS3 work)

**Status:** OPEN — deferred. Reserved as future design work; not currently scheduled.

**Trigger (when to assess this):** when recovery profiling under fmParallel shows that
recompute of already-present intermediate frames is a measurable cost worth avoiding, OR
when a future design need requires the recovery planner to represent present intermediate
frames distinct from holes. Not before — there is no correctness driver, only a possible
future efficiency one.

**Background — why AS3 is deferred.** CMS07.3 §9.6 clarified that the current proven
recovery planner is **nearest-present-start-point + contiguous-hole**: Phase-1 bounded
search descends from `requested-1`, the first present cached output becomes the
start/anchor, and the hole catalogue is contiguous from `anchor+1` through `requested-1`.
Under that planner there are only two states for a frame between the anchor and the
requested frame — it was present at plan time (so it *became* the anchor) or it was absent
(so it is a planned hole consumed through AS2). There is **no reachable third category** of
"present reused intermediate distinct from anchor and holes". The AS3 atomic (§8.7,
"reused-frame pin during ascending fill") therefore has no reachable trigger in the current
path and is **reserved but deferred**. The concurrent case people intuitively associate
with AS3 — a planned hole that becomes present (via another activation) before this
activation's AS2 store — is already handled correctly by AS2 first-in-best-dressed
duplicate/adopt (proven at H.3A): it is expected fmParallel-class concurrency, not an
error, and is correctness-complete without AS3.

**What a future sparse-plan revision would entail (to scope when assessed).** Giving AS3 a
real job requires the planner to be able to represent a non-contiguous plan — present
intermediate frames interleaved with genuine holes — which is a planner *data-model and
search* change, not just a new atomic. Likely pieces: a plan representation that catalogues
present-reused intermediates separately from absent holes; a Phase-1 search that does not
stop at the first present frame but continues cataloguing; AS3 itself (find-present →
pin → record under one lock, reusing the proven lookup-pin-record primitive, taking a
frame number with NO owned-frame parameter so it structurally cannot store/release); and a
revision of the CMS §9.1/§9.2/§9.5 recovery model. This is a CMS revision with its own
proposal, proof phases, and sign-off — materially larger than a single atomic.

**Interaction with the C.13B contiguity guard (IMPORTANT for the future implementer).**
Phase CMS07-C.13B added a production hard-status guard
(`cnr3_current_minimal_recovery_plan_status`) that enforces the current contiguity
contract: it is called at every success return of `plan_bounded_recovery_search_locked()`
and at the start of `store_recovery_plan_hole_owned_frame_and_record_pin()`, and it returns
`invariant_violation` for any non-contiguous / AS3-positive / requested-as-hole plan shape.
This guard is deliberately the **tripwire** that protects the current invariant against a
future maintainer who changes the planner without realising the downstream dependence on
contiguity. **Therefore, whoever implements the sparse-plan revision MUST revise or relax
the C.13B guard as an explicit, reviewed part of that work** — the guard will (correctly)
reject the very non-contiguous plans the sparse revision intends to produce, so it cannot
simply be left as-is. The guard firing is the signal that the contiguity assumption is
being changed and that the dependent recovery consumers (the H.3A AS2 consumer, the anchor
logic, and anything assuming contiguous holes) must be re-examined together. This is the
guard doing its job: forcing the future change to be deliberate and complete rather than
silent and partial.

**Correctness note.** Like FI-01, this is not a correctness gap in the current design —
the current minimal planner plus AS2 duplicate/adopt is correctness-complete for the
reachable recovery cases. Sparse-plan / AS3 is a potential future *efficiency* refinement
(avoid recompute when an intermediate is already present), gated on measured need.

**Authority note.** The recovery planner model and the AS register are CMS-owned (§8.7,
§9.x). A sparse-plan revision is therefore a CMS revision (new CMS version, diff-verified,
with proof phases), accompanied by the corresponding C.13B-guard revision in the cache
core. It is not a Production Spec §3A change and not, in itself, a diagnostics-spec change
(though the deferred D-SUM-12/D-SUM-13 recovery/recompute telemetry — see diagnostics spec
— would naturally be revisited at the same time).

---

## 3. Resolved items (history)

*(none yet — when an item is adopted into the CMS, move its descriptor here with the
CMS version that absorbed it, so the rationale is preserved.)*
