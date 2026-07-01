# CNR3 CMS — Future Investigations and Open Questions

**Date:** 2026-06-25 (last CONTENT change; filename re-versioned to v7.13 in lockstep with CMS07.13 on 2026-06-27 — no content change: the D.1-D.5 recovery arc resolved no open FI item; the fmParallel/concurrency items FI-01/02/03/05/06/07/08 remain open and are the subject of the upcoming fmParallel phase. 2026-06-28 currency note (no content change to existing FI items): the P.11C scene-change arc is CLOSED (.1-.5, committed CMS07-P.11C.5, 53/53); the next phase is live cache-pressure WIRING (hot-zone observation @arInitial then pruning into the live getFrame path). NEW item FI-09 added below for the SINGLE-ACTIVATION live prune-trigger contract that the wiring needs reviewed first; FI-06/07/08 continue to own the CONCURRENT (fmParallel) case. (v7.13.3 handover-safety update: FI-09 is reframed as part of STEP 0 -- a joint CMS sensibility/gap review for hot-zone + prune live wiring BEFORE any wiring patch; the CMS is not assumed reliable-as-is merely because the componentry is proven.) The condensed 4-phase diagnostics forward plan is in CNR3_Diagnostics_Arc_Condensed_Plan_v1_1.txt. 2026-06-29 (v7.13.4): STEP 0 is CLOSED. FI-09 (the single-activation live prune-trigger contract) is RESOLVED into the design authority as CMS07.14 §7.4 (independent checkpoint-retention trigger), §7.5 (combined-helper wiring contract), and §7.6 (arInitial observation prerequisite); provenance CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md. The controlling CMS is now CMS07.14 (additive over 07.13). FI-06/07/08 (CONCURRENT prune/observation under fmParallel) remain OPEN. Implementation owed: W.1 the §7.4 trigger primitive, then W.2 observation wiring, then W.3 the combined live helper. 2026-06-30 (v7.14): the live cache-pressure wiring arc W.1→W.2→W.3 is COMPLETE/committed (CMS07-W.3, 55/55 + eviction-proof live A/B harness PASS); the controlling CMS is now CMS07.15. NO FI item changed by W.3 — FI-01/02/03/05/06/07/08 remain OPEN for the fmParallel arc; the AS2 store-status RETURN contract surfaced by W.3 is recorded in CMS07.15 §7.5, NOT as an FI. NEXT arc = diagnostics (D-SUM), before the real-footage campaign.) 2026-07-01 (v7.15): filename bumped v7.14->v7.15 to re-align with the controlling CMS07.15 (lockstep mirror; no CMS content change). NEW item FI-10 added: a VS2026 profile of the NORMAL build on a sequential real-footage encode MEASURED native<->scalar plane marshalling at ~50% of per-frame cost, denoise math <10%, cache manager <3% — recorded as an OPEN investigation only (not a decision to act; a typed-row-pointer pixel-path rewrite is the candidate lever, a separate future R-PROCESS-21 arc). Also banked: the earlier ~3 fps concern was traced to TINY-100 diagnostic pruning frequency, NOT the cache architecture (normal build ~46 fps sequential). No existing FI item changed; FI-01/02/03/05/06/07/08 remain OPEN for fmParallel.)
**Pairs with:** the CNR3 Cache Manager Design Specification (CMS) at the **identical
version number**, which is carried in this document's filename
(`CNR3_CMS_Future_Investigations_and_Open_Questions_v7.13.md` pairs with CMS07.13).
This document carries **no internal version number** — its version is its filename,
and that filename always matches the prevailing CMS version. The date above reflects
this document's last **content** change (which may be older than the filename version,
because this document re-versions in lockstep with the CMS even when its content is
unchanged — see the maintenance rule below).

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
FI-02  CR2 is an empirical assertion, not a paper identity — BACK_RADIUS (B=50) must
       exceed the recursive chroma blend's effective settling length so the §9.5 floor
       fresh-start is invisible at N. Measure settling length on real footage at first
       .vpy runs and confirm B comfortably exceeds it. Correctness-of-approximation; OPEN.
FI-03  CR4 tension at the active_ceiling LOWER clamp — CR4 wants active_ceiling >=
       ~2x max-protected (~696 scatter-worst-case) but the clamp floor is 150; at the
       floor CR4 is violated. Probably benign for the ~1-zone linear case; rule does
       not condition on zone count. Decide restate/raise-clamp/document. OPEN.
FI-04  Scaffold getFrame registration (dependency declaration) — RESOLVED into
       CMS07.8 (rpGeneral, §9.7.7). See section 3 (resolved items) for rationale.

FI-05  Two-instance resource model under fmParallel (interlaced) - the CMS reasons
       about ONE instance's cache; two interlaced single-field instances sharing the
       host memory budget + thread pool are not yet addressed. Per-instance or shared
       active_ceiling? Do their prune triggers interact? Likely a genuine design gap
       (new content), to scope at the parallel phase. OPEN.

FI-06  Hot-zone update concurrency at arInitial - 5.7/D15 slides hot zone(s) at
       arInitial; under fmParallel many activations do so concurrently against a
       shared zone structure. Confirm appropriate atomicity and no pathological churn.
       Zones are prune-policy HINTS, so expected performance/churn, not correctness.
       OPEN.

FI-07  Hot-zone check/update inside the cache mutex - candidate RESOLUTION for FI-06:
       do the zone check/calc/slide at arInitial INSIDE cache_mutex_ (cheap, serialized,
       consistent; no separate zone lock). Confirm lock-hold stays short under fmParallel
       contention. To weigh when implementing hot zones. OPEN (option to table).

FI-08  First-in-best-dressed prune under concurrent stores - check-if-prune AND prune as
       ONE cache_mutex_ critical section; first thread under pressure prunes, next
       acquirer sees pruned cache and bails (self-debouncing via the lock). Open tuning:
       size prune-target hysteresis headroom to the concurrent-store burst (ties to
       FI-01). Count-based guard CONSIDERED and DEFERRED. To weigh when implementing
       pruning. OPEN (option to table).
FI-09  Live prune-TRIGGER contract for SINGLE-ACTIVATION wiring (NEW, 2026-06-28) - the prune PASS
       + hot-zone machinery are built and selftest-proven but have ZERO live callers; store appends
       without consulting the trigger. Before wiring prune into the live getFrame store path, a
       designer+coder review must pin down: exactly WHEN the store path invokes the bounded prune
       pass (after each over-ceiling store? batched?), and how that is SAFE against the active
       pin_list and the arInitial->arAllFramesReady gap (store-and-pin already one atomic per the
       CMS store note; the trigger call-site is the unspecified part). CMS policy is reliable
       (5.7 hot-zone-at-arInitial, 5.3-5.6 lifecycle, 6.3 retention, 5.5 safety); only the live
       trigger TIMING is unpinned. Scope SINGLE-ACTIVATION now; the CONCURRENT prune case is
       FI-06/07/08 (fmParallel). 'Proven componentry != proven wiring.' OPEN - resolve at the start
       of the live cache-pressure wiring phase. RESOLVED (2026-06-29) into CMS07.14 §7.4/§7.5/§7.6 via the closed Step 0 review.
       STEP 0 (banked): the wiring phase OPENED with a joint CMS
       sensibility/gap review (designer+coder+coordinator); FI-09's trigger-contract resolution is the
       load-bearing part of that review. A CMS clarification or version bump may be a legitimate output.

FI-10  Native<->scalar plane MARSHALLING dominates per-frame cost (NEW, 2026-07-01, MEASURED) - a
       VS2026 CPU sampling profile of the NORMAL build over a purely sequential real-footage run
       (576p25, -r 1, single instance) shows ~50% of per-frame time is spent copying VapourSynth
       native byte planes into std::vector<int> scalar buffers and staging results back to native
       bytes (cnr3_load_native_plane_sample ~17% self; cnr3_copy_native_plane_to_scalar_buffer +
       cnr3_stage_scalar_plane_to_native_bytes chain ~50% total), while the actual denoise math
       (downsample average, chroma blend, response-table lookup) is <10% and the whole cache/pin/
       prune machinery is <3%. Stable across 200/3500-frame runs and cache-on/off (sequential access
       has no reuse to amortize, so this IS the real single-instance encode cost). The P.11B selftest
       comment already deferred a 'typed row-pointer optimization' pending measurement; this is that
       measurement. OPEN - investigation only, NOT a decision to act; a typed-row-pointer / in-place
       native-access rewrite of the P.7A-P.11B pixel path is the candidate lever (est. ~1.5-2x on this
       path) but is a SEPARATE R-PROCESS-21 arc touching proven pixel-path code, to be scoped
       deliberately if/when undertaken. Distinct from FI-01/05/06/07/08 (those are CONCURRENCY under
       fmParallel; this is a PER-FRAME serial cost). Provenance: designer/coordinator profiling thread
       2026-07-01.
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

### FI-02 — CR2 (BACK_RADIUS vs blend settling length) is empirical, to be measured

**Status: OPEN. Correctness-of-approximation; to be resolved at first real `.vpy` runs
with development diagnostics. Surfaced 2026-06-21 while preparing CMS §9.7.**

CR2 (§10.2) requires `BACK_RADIUS` (B=50) to exceed "the effective settling length of
the recursive chroma blend," so that the §9.5 / §9.7.2 floor fresh-start (used when a
cold-region bounded search reaches the floor with no present anchor) is invisible at the
requested frame N. The floor fresh-start is exact from the floor forward but is a bounded
approximation relative to a true from-frame-0 recursion (the blend began at the floor,
not at 0) — see CMS §9.7.3 and the §9A.7 honesty rule.

The problem: "B exceeds the settling length" is an **empirical** claim about the blend's
convergence behaviour on real content, not an arithmetic identity. The other coherence
rules (CR1 JUMP_THRESHOLD=FWD+BACK+1=61; CR3 BACK_RADIUS≈5×CHECKPOINT_INTERVAL=50; CR5
MAX_RETAIN≥25; the decay_margin bound) are all paper-checkable and currently hold. CR2 is
the one rule whose truth can only be confirmed by measurement.

**Action when the `.vpy` real-runs land (with the keystone dev-trace diagnostics in
place):** measure the blend's actual settling length on representative footage (how many
frames until the recursive chroma output converges to within a negligible tolerance of a
from-0 run), and confirm B=50 comfortably exceeds it. Until measured, treat "the floor
approximation is invisible at N" as a **design assumption**, not a proven fact (consistent
with §9A.7, which already requires the floor-approximation use to be disclosed in
diagnostics and never described as exact full-history). If a future change shrinks B for
memory reasons, CR2 must be re-confirmed against this measurement.

**Authority note.** B and the CR rules are CMS-owned (§10.x). Any change to B is a CMS
revision; the coherence comments in `cnr3_build_config.h` must be updated to match.

---

### FI-03 — CR4 tension at the active_ceiling lower clamp (zone-count dependence)

**Status: OPEN. Interacts with real memory/zone behaviour; to be resolved at first real
`.vpy` runs. Surfaced 2026-06-21 while preparing CMS §9.7.**

CR4 (§10.2) requires `active_ceiling >= ~2 × max-protected`. Estimating the scatter worst
case: `max-protected ≈ MAX_HOT_ZONES × (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool =
5×(50+10) + 48 ≈ 348`, so `2× ≈ 696`. `active_ceiling` is derived from a 1 GiB budget and
**clamped to [150, 1000]** (§10.1).

At the **upper** clamp (1000) CR4 holds (1000 ≥ 696). But at or near the **lower** clamp
(150 — reached when frames are large, e.g. high-resolution 16-bit), 150 < 696, and indeed
150 < 348, so CR4 as literally stated is violated at the floor.

This is **probably benign** in practice: CR4's worst case assumes multi-zone scatter
(MAX_HOT_ZONES=5), whereas the current operational target — a linear pipe-to-ffmpeg
workload — uses ≈1 hot zone, making real `max-protected ≈ 1×60 + (a few retained) ≪ 348`.
So the 150 floor is very likely fine for the linear case. The issue is that the rule **as
written does not condition on zone count**, so it reads as violated whenever the ceiling
clamps low, even though the real protected set is small.

**Action (coordinator decision, deferred to the behavioural review since it interacts
with real memory and zone behaviour):** choose one of —
  (a) restate CR4 to use the **expected** (not maximum) hot-zone count for the
      lower-clamp case; or
  (b) **raise** the [150,1000] lower clamp so CR4 holds even at the floor; or
  (c) **document** that CR4 is a scatter-worst-case guide and the 150 floor is accepted
      for the linear ≈1-zone case, with the multi-zone case relying on the budget-derived
      (un-clamped) ceiling being well above 150.
Measure actual peak protected-set size and active_ceiling under representative workloads
before deciding.

**Authority note.** active_ceiling derivation, the clamp, and the CR rules are CMS-owned
(§10.x). Any change is a CMS revision; the `cnr3_build_config.h` coherence comments must
track it.

---

### FI-04 — Scaffold getFrame registration (dependency declaration) — RESOLVED

RESOLVED into CMS07.8 (§9.7.7: source-input dependency declared `rpGeneral`). Full rationale is in
section 3 (resolved items). This entry is retained here as a numbering placeholder so FI numbers are
stable; the active discussion has moved to the resolved-items history.

### FI-05 - Two-instance resource model under fmParallel (interlaced)

**Origin:** designer skim during the CMS07.9 recovery-scoping pass (2026-06-24). The pre-release
verification environment is TWO CNR3 instances, one per field, each running fmParallel over
interlaced content. Every resource statement in the CMS so far reasons about a SINGLE instance's
output cache under fmParallel; the two-instance case is not addressed.

**Open questions.** Is the memory budget / active_ceiling (7, 10.x) per-instance or shared across
both instances? If shared, do the two instances' capacity prune triggers (7.2) interact - can
instance A's store-time pressure force instance B to prune? If per-instance, how is the host memory
budget partitioned, static or adaptive? How is cross-instance INDEPENDENCE (each instance's
pin-ledger conservation and prune isolation) guaranteed and made observable/assertable (it must be,
per the diagnostics design-driver)?

**Assessment.** The skim candidate most likely to need NEW design content rather than a
clarification. NOT blocking branch-(d): single-instance recovery does not need it. To be scoped at
the fmParallel / two-instance phase with the full CMS fmParallel review. Correctness-adjacent (a
shared-budget interaction could cause one instance to evict frames the other still needs - though
the pin discipline, INV-B2, keeps that correct-but-wasteful, not wrong). OPEN.

### FI-06 - Hot-zone update concurrency at arInitial

**Origin:** designer skim during the CMS07.9 recovery-scoping pass (2026-06-24).

**Item.** 5.7 / D15 updates and slides hot zone(s) at arInitial. Under fmParallel, many activations
reach arInitial concurrently, each mutating the shared hot-zone structure. Confirm: (a) the zone
update/slide is under an appropriate atomic; (b) concurrent slides cannot churn pathologically;
(c) a zone update by activation X cannot make activation Y's in-flight prune-eligibility decision
unsafe (expected safe, because zones are demoted to prune-policy HINTS per intro item 2, so a
stale/raced hint causes a suboptimal prune, not a wrong result).

**Assessment.** Expected to be a performance/churn concern, not correctness, because hot zones are
hints not guarantees. To be confirmed EXPLICITLY at the full fmParallel review. OPEN.

---

### FI-07 - Hot-zone check/update inside the cache mutex (candidate resolution for FI-06)

**Origin:** designer/coordinator working thread, 2026-06-24 (fmParallel pre-thinking, parked for
the hot-zone implementation phase).

**Premise.** All mutexes are per-instance (FI-05); this item is entirely within ONE instance's
cache_mutex_.

**Candidate approach.** Perform the hot-zone check / distance calc / slide at arInitial INSIDE the
cache lock, rather than via a separate zone structure or zone lock. The zone calc is cheap (a few
integer comparisons/updates), so serializing it through cache_mutex_ gives every thread a
consistent zone read/update with no torn reads and no racing slides, and adds no new
synchronization primitive. Consistent with the existing CMS direction (6.3: zone-distance ordering
computed from pin-count state already maintained under the lock, NO parallel structure). Strong
candidate RESOLUTION for FI-06.

**Open question to confirm at implementation.** Lock-hold cost: under heavy fmParallel contention
every arInitial serializes briefly through cache_mutex_ for the zone update; confirm the hold is
short enough not to throttle the thread pool (expected yes; measure, do not assume).

**Status.** Option to TABLE when weighing hot-zone implementation options. Not blocking branch-(d).
OPEN.

### FI-08 - First-in-best-dressed prune under concurrent stores

**Origin:** designer/coordinator working thread, 2026-06-24 (fmParallel pre-thinking, parked for
the pruning implementation phase).

**Premise.** Per-instance cache_mutex_ (FI-05).

**Candidate approach.** Make prune execution a single cache_mutex_ critical section containing BOTH
the decision and the action: { check-if-prune-needed; then bail-or-prune }. Under capacity pressure
the FIRST thread to acquire the lock prunes; the NEXT acquirer sees a cache already under threshold
and bails. The mutex itself provides the debounce - no 'prune in progress' flag, no double-prune,
no second actor. Formalizes the self-debouncing the CMS 7.2 capacity trigger already gestures at,
and keeps concurrency-correctness in the one already-serialized place. (freeFrame stays OUTSIDE the
lock per 8.7 / 7.3; only decide+detach is under the lock.)

**Open tuning question.** What target does the first pruner prune TO? Pruning exactly to
active_ceiling lets N in-flight stores immediately re-cross the trigger -> churn. The existing
hysteresis (G.10A: trigger at active_ceiling x OVERFLOW_FACTOR, prune back toward active_ceiling)
is the lever; under fmParallel the headroom gap may need to be WIDER to absorb the concurrent-store
burst between one thread's prune and the next thread's check. Sizing that headroom to the burst
(scales with thread count) ties to FI-01.

**Considered and DEFERRED - count-based prune guard.** A count guard was considered on top of the
state-based debounce ('pruned X frames ago, under limit X -> increment guard and bail', requiring
prune to commence at a lower fill Y to reserve headroom for X+a guarded stores). REJECTED for now:
(1) adds no correctness, only prune-frequency reduction; (2) largely redundant with widening the
existing hysteresis gap (fewer re-triggers, no new state); (3) cost is backwards for this workload
- it trades away cache depth (lower fill Y) to save prune passes, i.e. spends the EXPENSIVE resource
(cache depth -> avoided recompute) to save the CHEAP one (prune = decide/detach under lock +
freeFrame outside; recompute = a whole frame through P.11B). Revisit ONLY if fmParallel profiling
shows prune-pass frequency or prune lock-hold is a MEASURED bottleneck, and even then prefer
widening the hysteresis gap before adding a counter.

**Architectural note (FI-07 + FI-08).** Both prefer pushing concurrency-correctness into the
existing cache_mutex_ (mutex-serialized check-then-act) over adding new primitives. A dedicated
pruning thread is the higher-complexity FALLBACK, considered only if mutex lock-hold under these
approaches is shown to throttle throughput.

**Status.** Option to TABLE when weighing pruning implementation options. Not blocking branch-(d).
OPEN.

---

### FI-10 - Native<->scalar plane marshalling dominates per-frame cost (MEASURED 2026-07-01)

**Origin.** Designer/coordinator profiling thread, 2026-07-01. Prompted by an apparent ~3 fps
observation that turned out to be the TINY-100 diagnostic-cache pruning frequency (a ~10x-more-often
prune cadence under the diag constants), NOT the cache architecture — a normal-build sequential run
delivers ~46 fps for the same 576p work. This item records what a proper profile then showed about
where the real per-frame time goes.

**Measurement.** VS2026 CPU Usage (sampling) profile, NORMAL (production-constant) Release build with
CNR3_KEYSTONE_DEV_TRACE OFF, real progressive footage (000_Example_576p25), one CNR3 instance,
`vspipe -r 1` (single request thread, deterministic per-frame attribution). Runs of 200 and 3500
frames, and with the core cache both defeated (SetVideoCache mode=0) and at VS default — all four
give the SAME shape (variance < ~1 percentage point on the top functions):

- `cnr3_load_native_plane_sample` — ~17% SELF (the innermost native-byte -> int per-sample load).
- `cnr3_copy_native_plane_to_scalar_buffer` — ~24% total (unpack a whole plane into std::vector<int>).
- `cnr3_stage_scalar_plane_to_native_bytes` — ~17% total (the reverse trip: pack int results back to
  native output bytes).
- `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid` — ~32% total (drives the copy above).
- **Together the native<->scalar marshalling is ~50% of per-frame time.**
- The actual signal processing is small: `cnr3_downsample_luma_plane_to_chroma_grid` ~4% self, the
  chroma blend inside `cnr3_process_chroma_plane_from_downsampled_luma` ~2.5% self, response-table
  lookup inlined and negligible. **Denoise math < ~10% combined.**
- The whole cache manager is <3%: `store_production_output_and_prune` ~1.4%,
  `lookup_frame_and_record_pin` ~0.5%, `record_hot_zone_observation` ~0.4%, `discharge_all` ~0.3%.
- Per-frame `std::vector<int>` allocate/resize/free churn is visible and plausibly feeds a portion of
  the kernel (ntdll) self-time, which fell from ~14% to ~10% as the run lengthened (first-touch page
  faults amortizing) — consistent with the allocation-churn reading, not proof of it.

**Why this is the REAL encode cost, not a harness artifact.** Sequential single-instance access
re-requests no frame, so the core cache has nothing to amortize; cache-on and cache-off are identical
here BY CONSTRUCTION. A straight `vspipe | ffmpeg` front-to-back encode (the actual restoration use
case) pays exactly this marshalling every frame. So unlike the eviction-path costs (which are a
tiny-constants artifact), this finding transfers directly to production.

**Sub-finding (2026-07-01, multi-thread run): parallelism induces recovery on SEQUENTIAL footage, and
each recovery re-runs the marshalling.** Running the SAME strictly-sequential profiler clip WITHOUT
`-r 1` (i.e. the VS thread pool issuing requests concurrently) put ~half the frames through
`cnr3_complete_live_recovery` (~52% of total) — even though the clip is sequential. Cause: the thread
pool issues getFrame requests OUT OF ORDER, so frame N can reach CNR3 before its predecessor N-1 is
produced; CNR3 then recovers the missing predecessor. The recovery MACHINERY is nearly free
(`start_live_recovery` ~0.3%, cache lookups ~0.5%); recovery is expensive because it RE-RUNS the same
`..._triplet_impl` pixel path (the same ~50% marshalling) to reconstruct each missing predecessor. So
under real multi-threaded encoding the effective cost is MORE THAN ONE marshalling pass per output
frame: (1 + recovery_rate) x the per-frame marshalling. Consequences: (a) the typed-pointer lever
below pays DOUBLY under parallelism — it speeds every frame AND every recovery re-run; (b) the
recovery RATE itself becomes a question — ~50% full-recovery on purely sequential footage is high, and
whether it is inherent to pool reordering or a tunable cache-retention/reorder-window mismatch is
exactly what the diagnostics arc (recovery churn = D-SUM-12; what-arrives-when vs what-it-triggers) is
built to measure. This does NOT change FI-10's investigation-only status; it enriches the case and
hands the diagnostics arc a concrete first metric.

**Interpretation.** The `std::vector<int>` scalar-buffer design was the right call for CORRECTNESS
(uniform int arithmetic, bit-depth independence 8/16-bit, clean per-plane logic) and is what the
P.1A-P.11B selftest chain proves. But it pays a full unpack copy + a full repack copy + a per-frame
allocation for every plane of every frame, so the marshalling costs ~5-6x the math it wraps. The
P.11B selftest trace already anticipated this: *"two-byte samples retain the P.8A memcpy path; typed
row-pointer optimization is deferred to fmParallel measurement."* This is that measurement.

**Source-read finding (2026-07-01, `cnr3_frame_processing.cpp` ~L1387-1740).** The pixel path holds
NINE concurrent scalar std::vector<int> buffers per activation (current_luma, current/previous
downsampled_luma, current/previous u, current/previous v, output u/v), and their lifetimes OVERLAP —
they are not sequential. Scene-change detection reads current+previous downsampled-luma AND
current/previous U AND current/previous V simultaneously, and both chroma-plane passes read the two
downsampled-luma buffers, so most of the nine must coexist. CONSEQUENCE: within-activation buffer
COLLAPSE/reuse (merging sequentially-used buffers) is essentially NOT available — the algorithm
genuinely needs them concurrent; the earlier guess that "U and V could share one buffer" is WRONG
(scene-change reads both at once). So the levers are NOT about reducing buffer COUNT: they are pooling
(remove the nine per-frame ALLOCATIONS, lever 1) and typed-pointer ELIMINATION (remove the buffers and
their copies entirely, lever 3). Reorganising the nine within the activation would buy ~nothing and is
explicitly NOT a task. (Recorded so the eventual arc does not waste a cycle rediscovering the buffers
cannot be merged.)

**Candidate levers, EASIEST-to-HARDEST (all NOT decisions — recorded for the eventual arc).**
1. **Reserve/reuse buffers, don't reallocate per frame (EASIEST, lowest risk).** The per-frame
   allocate/resize/free of the scalar std::vector<int>s is visible churn (and plausibly feeds the
   ~10% ntdll self-time via first-touch faults). Hold the scalar buffers in the per-activation
   frameData / a per-getFrame-call arena and `.clear()`+reuse rather than reallocate. NOTE (raised by
   coordinator): buffers must be per-ACTIVATION, not per-INSTANCE or global — under fmParallel each
   instance runs multiple concurrent activations, and two field-stream instances run in parallel too,
   so any shared/fixed buffer would need its own synchronization and would serialize the very
   parallelism we want. Per-activation (each getFrame call owns its scratch, lifetime = that call) is
   the only thread-safe reuse shape; it removes the ALLOCATION cost but keeps the copy. Partial win,
   but cheap and low-risk, and it does not touch the proven pixel maths.
2. **Fuse the unpack+process+repack passes (MEDIUM).** Today the plane is unpacked into an int buffer,
   processed, then repacked — three passes over the pixels. Fuse to fewer passes (process during
   copy / write native output directly from the processed value) to cut memory traffic and one buffer.
   Still uses int intermediates but touches each sample fewer times. Moderate rewrite of the P.6A/P.11B
   traversal; value-preserving.
3. **Typed-row-pointer in-place access (HARDEST, biggest win).** Template plane traversal on sample
   type (const uint8_t* / const uint16_t* with stride), do arithmetic in a wider accumulator inline,
   write straight to native output — deletes BOTH copies and the buffer entirely for the common path.
   Est. ~1.5-2x on this path (removes ~half of frame time), and under multi-thread the recovery
   re-runs benefit too. Largest change: touches P.7A-P.11B, must reproduce the exact 8/16-bit results
   the P.3A/P.5A/P.8A selftests pin (bit-exact blend/curve, int64 accumulator) — value-preserving or
   it is wrong. This is the full realization of the P.11B deferred optimization.
   Levers 1 and 2 are independently shippable stepping-stones toward 3; 1 alone is a safe quick win.

**Open questions to settle IF/WHEN an optimization arc is opened.**
1. Multi-thread ROI: the sub-finding above shows real parallel cost is (1 + recovery_rate) x per-frame
   marshalling; measure the recovery rate (diagnostics arc, D-SUM-12) so the lever's payoff is sized
   against the parallel workload, not just the -r 1 number.
2. Bit-depth handling: the typed path must preserve the exact 8-bit and 16-bit results the P.3A/P.5A/
   P.8A selftests pin (bit-exact blend/curve maths, int64 accumulator safety) — the rewrite is
   value-preserving or it is wrong.
3. Buffer ownership under fmParallel + two-instance: any buffer reuse (lever 1) must be per-ACTIVATION
   scratch (each getFrame call owns its own), never per-instance or global, or it reintroduces
   synchronization and serializes the concurrency. This constraint shapes lever 1's design.

**Scope / process.** This touches proven pixel-path code (P.7A-P.11B), so any action is an
R-PROCESS-21 arc (propose -> review -> selftest) with its own designer/coder cycle, SEPARATE from the
diagnostics (D-SUM) arc and from the fmParallel concurrency items (FI-01/05/06/07/08). It changes no
CMS design and no invariant; it is a performance investigation, not a design question.

**Status.** OPEN — investigation only. Measured and banked; not scheduled. Revisit when a pixel-path
performance arc is deliberately opened (candidate: alongside or after the fmParallel throughput work,
where the multi-thread question in (1) is answered anyway).

---

## 3. Resolved items (history)

### FI-04 - Scaffold getFrame registration (dependency declaration) - RESOLVED into CMS07.8

**Resolved:** CMS07.8 added 9.7.7, declaring the source-input dependency rpGeneral (not
rpStrictSpatial), because bounded recovery requests source N plus the sparse earlier sources for
genuine output holes, so rpStrictSpatial would misdeclare the filter once recovery is live;
rpGeneral is conservative-correct. fmUnordered / the fmParallel goal is unchanged and orthogonal.
The original FI-04 concern (do not carry the passthrough-correct K.1C scaffold registration
unchanged into real recursive filtering) is resolved by deliberate re-derivation in the CMS.
(Rationale preserved per the resolved-items convention.)

**RESULTS UPDATE (2026-07-02): the marshalling-optimisation arc acted on FI-10 and is largely
realised. Content note — CMS unchanged (still 07.15, no design change); this is the FI record catching
up to committed implementation.** The candidate typed-row-pointer rewrite became a staged R-PROCESS-21
arc, each step value-preserving (56/56 four-way, P-series unchanged) and profiled:
- **AVX2 + x64-only build** (committed): `/arch:AVX2` both projects, x64-only. Proven NEUTRAL — and the
  flag ALONE changed nothing in the profile, empirically proving the per-sample FUNCTION CALL is the
  auto-vectorisation WALL (vec-report `506`), i.e. AVX2's benefit is latent until the loops are reshaped.
- **Lever 0A** (committed): staged native luma passthrough — removed the full-res luma int round-trip.
  **-28% total** (93,914 -> 67,780 samples, 3500f -r 1). Win came from the repack side.
- **Lever 0B** (committed): removed redundant inner staging buffer (Option A, shared helper untouched).
  Flat total (cleanup), -1 alloc/staged plane.
- **Lever 3a.1** (committed): typed native->scalar unpack (removed the per-sample `load_native_plane_sample`
  CALL). **-37% total** (67,891 -> 42,748). `load_native_plane_sample` eliminated from the hot path.
  Cumulative vs pre-lever: **-54%**.
- **Lever 3b.1** (committed): inlined scalar-domain downsample tap-average (removed 3 calls/sample).
  Flat total within noise (4-run mean ~42,440) — the downsample FUNCTION dropped ~1,600 samples but is
  too small a slice to move the total. Confirmed: call-elimination on SMALL leaves does not move the total.
- **Vectorisation finding (vec-report `/Qvec-report:2`):** after call-elimination the hot loops STILL
  report `506` — the blocker SHIFTED from the function call to the per-sample VALIDATION BRANCH
  (data-dependent early-return). Uniform across unpack/downsample/blend/scene-change. So the next wins
  require UNBLOCKING vectorisation, not more call-elimination.
- **Validation audit + policy (2026-07-02):** whether the per-sample range checks may be removed to
  unblock vectorisation was audited by provenance class. Decision recorded in
  CNR3_Validation_Policy_recorded_v1: DEFEND at the source boundary (Tier 1 — kept, because VHS/analogue
  captures can genuinely emit out-of-range glitches and the VS API does not enforce per-sample range at
  the C boundary; HOIST its shape for vectorisation), TRUST validated intermediates downstream (Tier 2 —
  removable via production-private paths, relying on the invariant that no Tier-2 buffer is populated
  bypassing the Tier-1 gate), response-table outputs are bounded BY CONSTRUCTION (Tier 3 — removable with
  provenance guard), and the FINAL output clamp always stays.
- **Lever 3a.2** (IN PROGRESS): Tier-1 range-check HOIST on the big unpack leaf (8-bit check dropped as
  type-guaranteed; 16-bit pre-scan; branch-free conversion loop) — the first patch targeting the big
  leaf with VECTORISATION as the mechanism. DONE (committed). 16-bit conversion loop VECTORISED (C5001 — first vectorised hot loop in the codebase); P.8A/P.9A/P.11B unchanged (Tier-1 guarantee preserved in branch-free shape). BUT profile FLAT within noise (4 runs vs ~42,400 -> ~41,850 mean): the unpack leaf is MEMORY-bound, not compute-bound, so vectorising the widen/store did not reduce the dominant cost (native<->int buffer memory traffic + allocation). KEY FINDING: the residual marshalling is memory-bound — further SIMD on these loops won't help; the next real lever is ELIMINATING the int buffer (native->native / buffer-free / pass fusion), NOT more vectorisation. This redirects the remaining arc.
- **Still ahead:** 3b.2 (Tier-2 removals), 3c (blend / Tier-3, the data-dependent response-table gather —
  the hard case, possible Path B / explicit SIMD trigger). Levers 1/2 (pooling/fusion) optional per
  measured residual.
The multi-thread recovery sub-finding above is UNCHANGED and now MORE valuable: every per-frame speedup
also speeds every recovery re-run under parallelism. FI-10 remains formally OPEN until the arc closes
(3c decided), but its core thesis — marshalling dominates, typed pointers fix it — is now MEASURED-true.
