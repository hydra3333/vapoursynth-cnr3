# CNR3 STEP 0 — Findings Register (Round 3 C response appended)

**Companion to:** `CNR3_Step0_Joint_Review_PROCESS_v1_1.md` (read it first — it defines the rules).
**Subject:** joint CMS sensibility / gap review for hot-zone + prune LIVE WIRING, before any wiring patch.
**Build state at open:** P.11C arc CLOSED (.1-.5), committed CMS07-P.11C.5, 53/53. CMS07.13.
**This file is the single source of truth.** Append-only. One global comment sequence. LF line endings.

---

## HANDOFF SUMMARY — r4 -> C (read this first)

> **Round 4 (designer + X ruling) summary for the coder.** Two asks for you this round: (1) confirm you
> AGREE with the X ruling on SR-C-04 = option **(B)**; (2) confirm you AGREE with the one-line STATUS of
> every finding in the table below (flag any you dispute). No code yet.
>
> **X RULING THIS ROUND — SR-C-04 = (B) (add the independent checkpoint-retention trigger).** Designer
> reversed from (A) to (B) after verifying your #0037 scenario against live constants: MAX_RETAIN=48,
> overflow trigger=165; cut-heavy footage promotes ~every cut to is_checkpoint, so the flagged set can
> exceed 48 while slot_count stays under 165 -> under (A), MAX_RETAIN is unenforced in CNR3's core (cut-heavy)
> workload. So (B): CMS §6.3 stays as written (MAX_RETAIN is a real bound); close the gap with a new PROVEN
> checkpoint-retention trigger, built to the SR-D-07 helper order + single-activation scope, concurrent case
> deferred to fmParallel. Reuse/extend the composite selector (honour hot-zone/distance ordering); do NOT
> reuse remove_unpinned_checkpoints_above_retain_count_bounded_locked as-is.
>
> **FULL FINDINGS STATUS (confirm each, or dispute):**
> ```
> SR-D-01 BLOCKER AGREED    Prune trigger in a NEW combined locked helper, not inside store_owned_frame_locked
>                          (store-before-pin there would leave the slot unpinned). [verified]
> SR-D-02 BLOCKER AGREED    Lifecycle split: AS2 consumer inputs pin-protected; target/frame0 outputs
>                          hot-zone-protected, not pinned. [C agreed #0035]
> SR-D-03 MAJOR   AGREED    Hot-zone observation wired at arInitial (CMS 5.7), frame number only; no subtlety.
> SR-D-04 MAJOR   RESOLVED  Scope: single-activation now; concurrent -> fmParallel (FI-06/07/08); built to
>                          all-under-cache_mutex_ so no rework.
> SR-D-05 MAJOR   RESOLVED  (by ruling B) Independent checkpoint-flag-count retention trigger to be added;
>                          CMS 6.3 unchanged. [re-stated: no "pool"; flag+retention on unified slots_]
> SR-D-06 MINOR   RESOLVED  Non-checkpoint capacity trigger is self-debouncing by construction.
> SR-D-07 MAJOR   RESOLVED  6-step combined-helper order ratified + coder-confirmed buildable; consumer-vs-
>                          production distinction determinable at call site. [C confirmed #0038]
> SR-C-01 BLOCKER AGREED    Combined-helper shape: store -> set flag -> pin-if-AS2-consumer -> retire stale
>                          zones -> prune decide/detach -> unlock -> free. [verified]
> SR-C-02 BLOCKER AGREED    Target/frame0 output[N] protected by arInitial hot-zone observation of N, not a
>                          pin; observation is a HARD prerequisite of the store-prune. [verified]
> SR-C-03 MAJOR   AGREED    Pass a conservative once-computed cached-output byte estimate to the trigger,
>                          computed outside the cache lock.
> SR-C-04 MAJOR   RESOLVED  (by X ruling = B) add independent checkpoint-retention trigger; C to CONFIRM.
> SR-C-05 MAJOR   AGREED    Hot-zone retirement is lazy, retire-before-select inside the prune pass (CMS 5.6).
> SR-C-06 MINOR   AGREED    Temporary KDT for the live wiring proof, foldable into D-SUM-10/11 later.
> ```
>
> **ASSESSMENT vs atomicity / safety / reliability / end-goal (designer view; your view requested):**
> - Atomicity: STRONG — one cache_mutex_ scope (store->flag->pin->retire->prune-decide/detach), frees after
>   unlock (CMS 7.3 / FI-08). SR-C-01 strengthened it by stopping a store/pin split.
> - Safety: STRONG — target/frame0 prune-safety rests on arInitial hot-zone observation, now an explicit
>   ordering prerequisite (not an assumption).
> - Reliability: the SR-C-04 fix (B) is the load-bearing one — without it MAX_RETAIN is unenforced on
>   cut-heavy footage. With (B), the checkpoint set is bounded as designed.
> - End-goal (fmParallel): intact — single-mutex discipline forward-extends; (B) also helps the concurrent
>   cut-heavy case, so fixing now (not deferring) is end-goal-coherent.
>
> **YOUR ASKS (record as comments from #0042):**
>  (1) Confirm AGREE with X ruling SR-C-04 = (B), or raise any SINGLE-ACTIVATION safety concern with adding
>      a second prune path inside the same combined helper.
>  (2) Confirm AGREE with each finding's status above, or dispute specific ones.
>  (3) Raise any new SR-C-* if something is still missed.
> If you agree on both, the EXIT CONDITION is met and Step 0 closes (output: the ratified wiring contract +
> the SR-C-04=(B) decision; CMS 6.3 prose unchanged). Per PROCESS v1.1 §9, replace this blurb when you relay
> back. Next free comment seq: #0042.

## REGISTER CONTROL BLOCK  (X keeps authoritative)

```text
Register status:        OPEN (r4: X ruling SR-C-04=B + SR-D-07 closed; C to confirm B + statuses -> then EXIT)
Current round:          r4
Next free comment seq:  #0042
Authoritative copy:     held by X (coordinator)
Owners present:         D (designer), C (coder), X (coordinator)
```

**Open-finding tally (update each round):**

```text
BLOCKER: open 0 / total 4   (all AGREED)
MAJOR:   open 0 / total 7   (SR-C-04/SR-D-05 RESOLVED-by-ruling B, C to confirm; rest AGREED/RESOLVED)
MINOR:   open 0 / total 2   (all AGREED)
NIT:     open 0 / total 0
EXIT condition met: PENDING — met once C confirms (B) + statuses in r5
```

---

## HOW TO USE THIS FILE (short form — full rules in the PROCESS doc)

- Finding ID: `SR-<D|C|X>-<NN>`, owner-tagged, append-only, never renumbered.
- Each finding: `[severity: BLOCKER|MAJOR|MINOR|NIT]` `[status: OPEN|ACKNOWLEDGED|AGREED|PARTIAL|DISAGREED|DEFERRED-to-<arc>|RESOLVED|WITHDRAWN|REJECTED]`, an `Anchor:`, and the finding text.
- Each comment: `[#<SEQ> | <D|C|X> | r<ROUND> | <verdict>]  reasoning (REQUIRED, even for agree)`.
  - `<SEQ>` = one global increasing integer across the whole file (max existing + 1). Not datetime.
  - verdict ∈ `agree | disagree | partial | ack | question | withdraw | RULING(X only)`.
  - Honest-agree rule: an agree that only holds for a narrower claim is a `partial`, scope stated.
- A finding reaches `RESOLVED` only when both latest verdicts align OR X rules. `disagree` never self-closes.

---

## WORKED EXAMPLE (illustrative only — DELETE or ignore; not a real finding)

```
### SR-D-00   [severity: MINOR]   [status: RESOLVED]
Anchor: CMS §5.7 / record_hot_zone_observation
Finding (D): Example — confirm the hot-zone observation wiring point is arInitial, not arAllFramesReady.

Comments:
[#0000 | C | r1 | agree]    Agree — §5.7 explicitly fixes the point at arInitial; the live call site is
                            unambiguous and needs no new design, only the wire-in. Scope unchanged.
[#0000 | D | r2 | agree]    Agree as stated; nothing narrowed. Recording the answer: observation is
                            recorded at arInitial request classification. -> RESOLVED.
```

---

# ===========================  FINDINGS  ===========================
# (Round 0: D appends the opening designer findings below. C appends SR-C-* in Round 1.
#  Nothing here yet — this is the empty skeleton.)


## --- Designer findings (SR-D-*) ---

### SR-D-01   [severity: BLOCKER]   [status: AGREED]
Anchor: CMS §7.2 (capacity trigger, "Store prunes before hard-ceiling rejection (D11)") / cnr3_cache_core.cpp store_owned_frame_locked / live callers in cnr3_arAllFramesReady.cpp
Finding (D): The CMS specifies a store-path prune trigger (§7.2: non-checkpoint prune fires when the pool
strictly exceeds active_ceiling x OVERFLOW_FACTOR; "store prunes before hard-ceiling rejection"). But in the
committed P.11C.5 source the live store path does NOT invoke it: store_owned_frame_locked appends and only
returns capacity_exceeded at the vector hard max; execute_bounded_prune_pass and
calculate_cache_prune_trigger_decision have ZERO callers in arInitial.cpp/arAllFramesReady.cpp (verified this
session). So §7.2's "store prunes before hard-ceiling rejection" is currently UNIMPLEMENTED in the live path.
QUESTION FOR THE REVIEW: confirm the agreed live call-site — does the prune trigger run inside
store_owned_frame_locked (under the same cache_mutex_ as the store), or in the caller in arAllFramesReady
immediately after store, before pin discharge? The CMS gives the policy but not the call-site; we must pin it
down before coding. This is a BLOCKER because it is the core of the wiring task.

Comments:
[#0001 | D | r0 | question]  Seeding. My design lean: trigger INSIDE store_owned_frame_locked under the held
                            cache_mutex_, so the check-and-prune is one atomic critical section with the store
                            (matches §7.3's decide/detach/free separation staying lock-coherent, and FI-08's
                            "check-if-prune AND prune as ONE cache_mutex_ critical section"). C: feasible at
                            that call-site, or do you see a reason to prune in the arAllFramesReady caller
                            instead? State the lifecycle implications either way.
[#0007 | C | r1 | partial]  Agree the live store-path prune trigger is unwired and must be settled before coding.
                            I do NOT agree with inserting the trigger literally inside store_owned_frame_locked as-is:
                            the combined AS2 helper calls store_owned_frame_locked first and only then pins/records
                            the stored/adopted slot, so a prune inserted inside that low-level primitive could see the
                            just-stored slot as unpinned. Feasible shape: a new combined helper whose public entry
                            pre-reserves prune vectors, then under one cache_mutex_ scope performs store/adopt,
                            pin/record where the caller is a consumer, and only then prune decide/detach; duplicate
                            losers and detached prune frames still free after unlock. See SR-C-01.

[#0019 | D | r2 | agree]    C is correct and I verified it in source: store_owned_frame_and_record_pin_locked
                            calls store_owned_frame_locked (line ~48) BEFORE record_pin (~99), so a prune inside the
                            low-level store would see the just-stored slot unpinned. I withdraw my "inside
                            store_owned_frame_locked" lean. AGREED call-site shape: a new combined locked helper that,
                            in ONE cache_mutex_ scope, does store/adopt -> checkpoint promote -> pin/record (if the
                            caller is an AS2 consumer) -> THEN prune decide/detach; losers + detached victims free
                            after unlock. This is the SR-C-01 contract. Narrowing recorded: the trigger lives in the
                            new combined helper, NOT in store_owned_frame_locked. -> resolves jointly with SR-C-01.


### SR-D-02   [severity: BLOCKER]   [status: AGREED]
Anchor: cnr3_arAllFramesReady.cpp pin_list.discharge_all (live) / CMS §5.5 (prune-after-criteria) / the arInitial->arAllFramesReady gap
Finding (D): Lifecycle safety of a live prune vs the active pin_list. The per-activation pins live in
request_data->pin_list and are discharged by discharge_all at the END of arAllFramesReady. If a store-path
prune fires DURING the same activation (SR-D-01), it must not evict a frame that the current activation still
needs (the just-stored frame, the recovery anchor, predecessor). The CMS asserts prune is safe because pins
underwrite correctness (§5.5) and store-and-pin is one atomic (so a gap cannot evict the just-stored frame).
QUESTION: confirm that at the moment a store-path prune would fire, EVERY frame the current activation still
needs is already pinned in pin_list (so the prune candidate set provably excludes them). Specifically: is the
just-stored frame pinned BEFORE the prune check in the same critical section, and are the recovery
anchor/holes pinned for the whole activation? If any needed frame is not yet pinned at prune time, that is the
gap to close.

Comments:
[#0002 | D | r0 | question]  Seeding. store_owned_frame_and_record_pin is named as if store+pin are one unit;
                            I read that as the just-stored frame being pinned in the same critical section, so
                            a same-section prune cannot select it (pin_count>0 excludes it per §6.3/§7.2). C:
                            confirm from the live code that this holds for ALL frames the activation needs at
                            the point prune fires, not just the just-stored one.
[#0008 | C | r1 | partial]  For predecessor-present and recovery compute inputs, yes: the current live code pins the
                            cache-hit output, predecessor, recovery anchor, floor, and computed/adopted holes through
                            request_data->pin_list until discharge_all at the end of arAllFramesReady. However the
                            final target output store and frame-0 store use isolated store helpers and do not pin the
                            produced output; that matches CMS production-never-pins, but it means target/output[N]
                            protection during a same-activation prune must come from the arInitial hot-zone observation
                            or an explicit temporary exclusion, not from pin_list. See SR-C-02.

[#0020 | D | r2 | partial]  Accept C's split and verified it: AS2 consumer inputs (cache-hit output, predecessor,
                            recovery anchor, floor, computed/adopted holes) ARE pinned via pin_list to discharge_all,
                            so a same-activation prune provably excludes them (pin_count>0). BUT the final target and
                            frame-0 stores go through store_checkpoint_owned_frame / cnr3_store_live_output_frame_for_return
                            and do NOT pin output[N] (CMS §4.1 production-never-pins). So SR-D-02's "every needed frame
                            is pinned" is TRUE for consumer inputs, FALSE for the produced output[N]. The gap is real
                            and is exactly SR-C-02. This stays PARTIAL until SR-C-02 settles the target/frame0
                            protection basis (hot-zone, which I agree with below). Scope narrowed accordingly.

[#0035 | C | r3 | agree]    Agree with D's narrowed split in #0020 and with SR-C-02's resolution: AS2 consumer
                            inputs (cache-hit output, predecessor, recovery anchor/floor, computed/adopted holes)
                            are protected by pins until discharge_all; target/frame0 produced output[N] is NOT
                            pinned and must instead be protected by the arInitial hot-zone observation of N before
                            any arAllFramesReady store-prune. So the original "every needed frame is pinned" claim
                            is intentionally narrowed to consumer inputs; production outputs rely on hot-zone
                            membership, keeping CMS §4.1 production-never-pins pure.


### SR-D-03   [severity: MAJOR]   [status: AGREED]
Anchor: CMS §5.7 (hot-zone update at arInitial) / record_hot_zone_observation (0 live callers)
Finding (D): Hot-zone OBSERVATION wiring point. §5.7 fixes it: "Zone activity is registered at arInitial, not
deferred to arAllFramesReady." record_hot_zone_observation has 0 live callers, so this is unimplemented but
the WHERE is specified. The wiring is: at arInitial request classification, record an observation for the
requested frame N. QUESTION: confirm there is no design subtlety beyond "call record_hot_zone_observation(N)
at arInitial" — e.g. does observation need the request KIND (cache-hit vs compute vs recovery), or just the
frame number? And must retirement (retire_decay_eligible_hot_zones, §5.5) be driven on the same arInitial
tick, or lazily inside the prune pass (§5.6 says the retirement test is answerable in the locked prune pass)?

Comments:
[#0003 | D | r0 | question]  Seeding. §5.6 says retirement is answerable cheaply inside the locked prune pass
                            from existing pin state, which suggests retirement need NOT be a separate arInitial
                            step — it can be evaluated lazily when prune runs. If so, only OBSERVATION is wired
                            at arInitial; retirement rides the prune pass. C: agree that split, or do you see a
                            need to drive retirement eagerly at arInitial?
[#0009 | C | r1 | partial]  Agree the observation point is arInitial and the current cache-core API takes only the
                            frame number, so request kind is not needed for the existing hot-zone model. Partial on
                            retirement: the current execute_bounded_prune_pass path does NOT call
                            retire_decay_eligible_hot_zones_locked; if retirement is meant to ride the prune pass, that
                            must be an explicit addition to the new live/cache-core prune wrapper. Otherwise stale zones
                            can remain active and suppress prune longer than the CMS decay model intends. See SR-C-05.

[#0021 | D | r2 | agree]    Agree with C #0009: observation point is arInitial and the API takes the frame number.
                            The retirement-timing half of my question is now its own finding (C raised SR-C-05) — I
                            address it there. For SR-D-03 itself, the OBSERVATION wiring (record_hot_zone_observation(N)
                            at arInitial request classification) is agreed with no subtlety beyond the frame number.
                            -> AGREED on observation; retirement tracked in SR-C-05.


### SR-D-04   [severity: MAJOR]   [status: RESOLVED]
Anchor: CMS §5.7 ("once multiple requests are in flight") / §7.2 / single-activation vs fmParallel
Finding (D): Scope boundary — single-activation NOW vs concurrent LATER. Much of the prune/hot-zone policy is
written FOR fmParallel (§5.7 "required for safety once multiple requests are in flight"; FI-06/07/08 concern
concurrent stores/observation). In the CURRENT live path there is only ever one activation in flight, so the
concurrency-safety questions are simpler now. PROPOSAL: Step 0 scopes and approves SINGLE-ACTIVATION wiring
only (one getFrame activation at a time; prune + hot-zone observation correct and safe in that regime), and
EXPLICITLY defers the concurrent case (multiple in-flight activations pruning/observing against the shared
cache) to the fmParallel arc (FI-06/07/08). QUESTION: do we all agree this scope split, and that wiring done
now must be written so it does not have to be torn up for fmParallel (i.e. lock discipline already correct,
just not yet stress-tested concurrently)?

Comments:
[#0004 | D | r0 | question]  Seeding. The lock discipline (all cache mutation under cache_mutex_) should make
                            single-activation wiring forward-compatible with fmParallel without rework — the
                            concurrent case adds contention/correctness STRESS, not a different call structure.
                            C: confirm that wiring to that discipline now won't need restructuring for
                            fmParallel, only validation.
[#0010 | C | r1 | agree]    Agree with the scope split: wire and prove the single-activation live policy now,
                            while explicitly deferring multi-activation/fmParallel stress to the fmParallel arc. The
                            implementation should still use the final lock discipline and cache-core APIs now: no
                            live-path shortcut, global sequencing flag, or caller-side cache peek should be introduced
                            merely because only one activation is expected today. With that constraint, the call
                            structure should not need redesign for fmParallel, only concurrent validation.

[#0022 | D | r2 | agree]    Both latest verdicts agree: single-activation wiring now, concurrent deferred to
                            fmParallel (FI-06/07/08), written to the all-mutation-under-cache_mutex_ discipline so the
                            concurrent case adds validation/contention stress, not a different call structure. Recording
                            the decision. -> RESOLVED (scope split approved).


### SR-D-05   [severity: MAJOR]   [status: RESOLVED-by-ruling (B); C to confirm]
Anchor: CMS §7.2 (active_ceiling x OVERFLOW_FACTOR; OVERFLOW_FACTOR=1.1; MIN_HARD_CEILING=150) / §6.3 (checkpoint MIN/MAX_RETAIN retention rule)
Finding (D): [HEADER CORRECTED in r2 per #0033 — original "checkpoint pool" wording was stale; there is NO
checkpoint pool. The cache is a SINGLE unified slots_ vector; "checkpoint" is a bool FLAG on a slot, governed
by a separate RETENTION RULE (MIN/MAX_RETAIN counts), not a pool. Corrected text follows; original wording is
preserved in the r2 comment thread (#0033) for the audit trail.]
Two prune REGIMES, confirm both are wired, not just the non-checkpoint one. §7.2 is the NON-checkpoint capacity
trigger (total slot_count > active_ceiling x OVERFLOW_FACTOR). §6.3 is the checkpoint-FLAG retention rule
(checkpoint-flagged slots trimmed toward MIN_RETAIN when the flagged count exceeds MAX_RETAIN). These are
SEPARATE retention behaviours over the SAME unified cache, not separate containers. QUESTION: does the live
wiring need a checkpoint-retention trigger that fires on (flagged-slot count > MAX_RETAIN) INDEPENDENTLY of the
total-slot capacity trigger, or is checkpoint trimming only performed as composite candidate selection WITHIN
the capacity-pressure pass? (Verified in r2: today it is the latter — no independent flag-count trigger. The
decision is SR-C-04 / X ruling: amend CMS §6.3 wording vs add an independent flag-count trigger.)

Comments:
[#0005 | D | r0 | question]  Seeding. P.11C.5 proved checkpoint-class survival under the non-checkpoint prune
                            pass; I want to confirm the CHECKPOINT prune trigger (MAX_RETAIN overflow) also has
                            a proven pass and a live trigger site, so both regimes are wired together. C:
                            confirm from the source which proven entry points cover each regime.
[#0011 | C | r1 | partial]  Agree both retention regimes must be addressed, but source inspection says the existing
                            execute_bounded_prune_pass does not independently fire on checkpoint_count >
                            CHECKPOINT_MAX_RETAIN. It first requires the section-7.2 active-ceiling trigger on total
                            slot_count, then includes checkpoint candidates only as part of that bounded pass via the
                            retain_checkpoint_count argument. So the existing helper proves composite selection under
                            capacity pressure, not a standalone checkpoint-pool trigger. This needs a design/code
                            decision before wiring. See SR-C-04.

[#0023 | D | r2 | agree]    C is correct and I verified it: cnr3_calculate_cache_prune_trigger_decision keys ONLY on
                            current_slot_count > active_ceiling*OVERFLOW_FACTOR; it does not receive checkpoint_count and
                            has no CHECKPOINT_MAX_RETAIN comparison. Checkpoints enter only as candidates within the
                            capacity-triggered pass via retain_checkpoint_count. So there is NO standalone §6.3 checkpoint
                            trigger today — a genuine CMS-vs-implementation gap. SR-D-05's "both regimes wired" cannot be
                            satisfied as-is; this becomes the decision in SR-C-04. -> superseded-by SR-C-04 (kept open
                            there).

[#0033 | D | r2 | question]  TERMINOLOGY CORRECTION (see #0032): my SR-D-05 wording "CHECKPOINT pool trigger" and
                            "checkpoint pool exceeds MAX_RETAIN" is WRONG — there is no checkpoint pool, only an
                            is_checkpoint flag on unified slots and a retention COUNT. Restate SR-D-05 as: "confirm
                            whether a prune is triggered by the count of checkpoint-FLAGGED slots exceeding MAX_RETAIN,
                            independent of the total-slot capacity trigger." Substance unchanged (answer is still: no
                            independent trigger today — see SR-C-04). Recording the correction rather than editing the
                            original, per append-only.

[#0036 | C | r3 | agree]    Confirmed: read every prior "checkpoint pool" phrase as checkpoint-FLAGGED-slot
                            retention over the unified `slots_` cache. There is no pool/container to build toward.
                            The remaining question is only whether checkpoint-flagged count gets an independent
                            trigger or is trimmed only when the total-slot capacity trigger invokes composite prune.

### SR-D-06   [severity: MINOR]   [status: RESOLVED]
Anchor: CMS §7.2 ("self-debouncing"; the capacity->overflow gap IS the hysteresis) / live re-fire behaviour
Finding (D): Confirm the hysteresis/self-debounce holds in the SINGLE-activation live regime. §7.2 says the
active_ceiling->overflow gap is the hysteresis and prune "won't re-fire until the pool climbs back over
threshold." In single-activation wiring, a prune that runs inside one store and brings the pool back toward
active_ceiling should not re-fire on the next store until overflow is re-crossed. QUESTION: confirm this is
automatic from the threshold comparison (no per-call guard needed) given the live store cadence — i.e. there
is no pathological re-fire-every-store case once wired.

Comments:
[#0006 | D | r0 | question]  Seeding. I believe this is automatic (the comparison pool > ceiling x FACTOR is
                            self-debouncing by construction), but flagging it MINOR so C confirms there is no
                            live-cadence edge case. C: confirm or raise.
[#0012 | C | r1 | agree]    Agree for the non-checkpoint active-ceiling trigger: the current arithmetic is
                            self-debouncing because prune_is_required is current_slot_count > overflow_trigger and
                            the target is active_ceiling. If max_remove_count caps a burst before target is reached,
                            later stores may trigger additional bounded passes; that is the intended K-bounded burst
                            behaviour, not a separate re-fire-every-store bug. Checkpoint-pool trigger behaviour is
                            separate and covered by SR-C-04.



## --- Coder findings (SR-C-*) ---
[#0024 | D | r2 | agree]    Agree with C #0012: the pool > ceiling*FACTOR comparison is self-debouncing by
                            construction for the non-checkpoint regime; no per-call guard needed in single-activation
                            cadence. (Any checkpoint-regime debounce rides on the SR-C-04 decision.) -> RESOLVED for the
                            non-checkpoint trigger.


### SR-C-01   [severity: BLOCKER]   [status: AGREED]
Anchor: cnr3_cache_core.cpp store_owned_frame_locked / store_owned_frame_and_record_pin_locked / CMS §7.3 AS5 / SR-D-01
Finding (C): The raw low-level store primitive is not a safe prune insertion point. In the current code,
store_owned_frame_and_record_pin_locked calls store_owned_frame_locked, then pins and records the stored/adopted
slot afterwards. A prune inserted literally inside store_owned_frame_locked would therefore run before the
AS2 pin exists and could select the just-stored non-checkpoint slot if it is outside hot zones. The live wiring
needs a new combined helper/contract, not a simple insertion into store_owned_frame_locked.

Comments:
[#0013 | C | r1 | question]  Proposed implementation contract for review: public live helpers pre-reserve the prune
                            work vectors before taking cache_mutex_; a locked internal helper then performs store/adopt,
                            applies checkpoint promotion, pins/records if the caller is an AS2 consumer, evaluates
                            prune, and detaches victims in that order. Duplicate loser refs and detached victim refs
                            release after unlock. D: does that satisfy the intended "store prunes before hard-ceiling
                            rejection" language, or do you want the CMS to name separate target-store and AS2-store
                            prune shapes?
[#0025 | D | r2 | agree]    Verified the store-then-pin ordering in source — finding is correct and load-bearing.
                            The proposed contract in #0013 (pre-reserve prune vectors outside the lock; one locked
                            internal helper does store/adopt -> promote -> pin/record-if-consumer -> prune decide/detach;
                            losers+victims free after unlock) is exactly right and matches §7.3 (decide/detach locked,
                            free after unlock) and FI-08 (check+prune one critical section). Answering #0013: yes this
                            satisfies "store prunes before hard-ceiling rejection"; and yes the CMS should name TWO store
                            shapes (AS2-consumer-store-with-pin vs target/production-store-no-pin) since their prune
                            safety basis differs (pin vs hot-zone) — see SR-C-02. -> AGREED; this is the wiring contract.


### SR-C-02   [severity: BLOCKER]   [status: AGREED]
Anchor: cnr3_arAllFramesReady.cpp cnr3_store_live_output_frame_for_return / frame-0 store_checkpoint_owned_frame / CMS §4.1 production-never-pins / CMS §5.7 hot-zone-at-arInitial
Finding (C): Final output stores are not AS2 consumer stores. The branch-c target, branch-d target, and frame-0
fresh-start paths store cache refs through isolated store helpers and do not pin output[N]; this is consistent with
CMS §4.1 "production never pins," but it means a live prune around those stores cannot rely on pin_list to protect
output[N]. For target/frame0 cache retention, the safety/protection basis must be hot-zone membership from the
arInitial observation of N, or an explicitly approved one-frame exclusion in the prune wrapper.

Comments:
[#0014 | C | r1 | question]  I lean to hot-zone protection as the clean answer: record_hot_zone_observation(N) at
                            arInitial before any arAllFramesReady store, so output[N] is inside [N-BACK,N+FORWARD]
                            when the later target/frame0 store and prune run. This keeps production pins pure and
                            avoids inventing a policy pin. D: confirm that target/frame0 store-prune safety should
                            be argued from hot-zone protection, not by adding a pin to returned/produced output.
[#0026 | D | r2 | agree]    Verified: target (cnr3_store_live_output_frame_for_return -> store_checkpoint_owned_frame)
                            and frame-0 stores do not pin output[N]. Agree with #0014: the protection basis for output[N]
                            during a same-activation prune is HOT-ZONE membership from the arInitial observation of N, NOT
                            a policy pin. Confirmed sound: observing N at arInitial centres the zone [N-50, N+10], so N is
                            inside its own zone and excluded from prune candidates (§6.3 "outside every hot zone"). This
                            keeps production-never-pins (§4.1) pure. So observation@arInitial is a HARD PREREQUISITE of
                            the target/frame0 store-prune, not just a policy nicety -> ordering: observe(N)@arInitial
                            BEFORE any arAllFramesReady store-prune. -> AGREED.


### SR-C-03   [severity: MAJOR]   [status: AGREED]
Anchor: execute_bounded_prune_pass(frame_byte_count, retain_checkpoint_count, max_remove_count) / live output frame geometry / CMS §7.2 active_ceiling
Finding (C): The live wiring contract needs to specify the frame_byte_count passed to the prune trigger. The proven
cache-core trigger requires a non-zero frame_byte_count, but the current live arAllFramesReady path has no helper that
computes the effective cached-output frame size for prune policy. The value affects active_ceiling directly, so it is
not a cosmetic implementation detail.

Comments:
[#0015 | C | r1 | question]  Candidate contract: compute a conservative cached-output byte estimate once per live
                            output geometry from the VapourSynth format/dimensions/subsampling/storage bytes, outside
                            cache_mutex_, and pass that value to the prune helper. Avoid calling VSAPI geometry or
                            doing heavy formatting inside the cache lock. D: should the estimate use active plane
                            sample bytes, VS frame strides, or an agreed conservative upper estimate?
[#0027 | D | r2 | agree]    Agree this is load-bearing (frame_byte_count feeds active_ceiling). Answering #0015:
                            use a CONSERVATIVE upper estimate of the cached-output frame size computed ONCE per live
                            output geometry (from VS format: width x height x bytes-per-sample summed across planes with
                            subsampling), captured outside cache_mutex_ at first output or filter init, then passed to the
                            prune helper. Conservative-high is the safe direction (overestimating frame bytes lowers
                            active_ceiling -> prunes a little earlier -> never under-protects memory). Do NOT call VSAPI
                            geometry or format inside the cache lock. -> AGREED on a conservative once-computed estimate;
                            exact byte formula to be fixed in the wiring scope.


### SR-C-04   [severity: MAJOR]   [status: RESOLVED-by-ruling (B); C to confirm]
Anchor: CMS §6.3 checkpoint pool trigger / select_composite_prune_candidates_bounded_locked / execute_bounded_prune_pass_locked / SR-D-05
Finding (C): Existing cache-core code does not appear to implement an independent checkpoint-pool prune trigger.
select_composite_prune_candidates_bounded_locked can select checkpoint candidates when checkpoint_count exceeds the
retain count, but execute_bounded_prune_pass_locked only reaches selection after the total slot-count active-ceiling
trigger fires. Therefore checkpoint_count > CHECKPOINT_MAX_RETAIN alone does not currently cause a prune pass if the
total cache is below the active-ceiling overflow threshold. This diverges from the CMS §6.3 wording that checkpoint
prune runs when the checkpoint pool exceeds CHECKPOINT_MAX_RETAIN.

Comments:
[#0016 | C | r1 | question]  This is either a CMS clarification/change or a missing cache-core/live primitive. If the
                            intended production policy is "checkpoint trim only during capacity-pressure prune," the
                            CMS §6.3 retention wording should be amended. If the intended policy is an independent
                            checkpoint pool bound, we need a small new/proven checkpoint-trigger pass or an extension
                            of execute_bounded_prune_pass before live wiring can honestly claim both regimes.
[#0028 | D | r2 | partial]  Verified the gap is real (trigger keys on slot_count only; no checkpoint_count >
                            MAX_RETAIN path). This needs a COORDINATOR/CMS decision, so I am not closing it. Two honest
                            options: (A) AMEND CMS §6.3 wording to "checkpoint trimming occurs as part of the
                            capacity-pressure bounded prune (composite selection via retain_checkpoint_count)" — i.e. the
                            implementation is right and the CMS prose over-promised an independent trigger; or (B) ADD a
                            small proven checkpoint-pool trigger pass (checkpoint_count > CHECKPOINT_MAX_RETAIN) before
                            live wiring. My design lean is (A): in practice the checkpoint pool is bounded by retention
                            during capacity prunes and by CHECKPOINT_MAX at promotion; a standalone trigger adds a second
                            prune path for little benefit pre-fmParallel. BUT this is X's call as it is a CMS change.
                            -> stays OPEN pending X ruling; flagged as a legitimate Step 0 CMS output either way.

[#0032 | D | r2 | question]  TERMINOLOGY CORRECTION (coordinator-flagged): there is NO "checkpoint pool" in the
                            live architecture and there has not been for a long time. Verified in source: slots_ is a
                            single std::vector<Cnr3CacheSlot> (one unified cache); is_checkpoint is a bool FLAG on each
                            slot (cnr3_cache_core.h:320); checkpoint_count_locked() returns checkpoint_slot_positions_.size(),
                            which is an INDEX of flagged-slot positions, not a separate container. CHECKPOINT_MAX_RETAIN/
                            MIN_RETAIN are RETENTION COUNTS (how many flagged slots survive a prune), not a pool capacity.
                            This matches CMS §6.1 ("a checkpoint is a FLAG, eviction-protected by a separate retention
                            rule, NOT a pin"). The SUBSTANCE of SR-C-04 stands — there is no independent prune trigger
                            keyed on (flagged-checkpoint count > MAX_RETAIN); checkpoints are only trimmed as candidates
                            within the slot_count capacity-pressure pass. But re-read every "checkpoint pool" phrase in
                            SR-C-04/SR-D-05/#0011/#0016/#0023/#0028 as "checkpoint-FLAGGED-slot retention" — there is no
                            pool to wire. C: please confirm you read it as flag+retention, so we don't build toward a
                            nonexistent pool container.

[#0037 | C | r3 | partial]  Advice for X ruling: I lean to option (B) if CHECKPOINT_MAX_RETAIN is intended as a real
                            bound, because option (A) can exceed MAX_RETAIN in single-activation. Cut-heavy content can
                            promote every output to is_checkpoint; for example frames 0..99 could yield roughly 100
                            flagged slots while total slot_count=100 remains below the minimum active-ceiling overflow
                            trigger of 165. With small frames the active ceiling can be much higher, so the overshoot
                            can be larger. Under (A), flagged slots are bounded only by the total cache capacity/active-
                            ceiling prune, not by CHECKPOINT_MAX_RETAIN. If X nevertheless accepts (A), CMS §6.3 must
                            say MAX_RETAIN is a retention target used during capacity-pressure prune, not an independent
                            trigger. For (B), new code is moderate, not large: extend the live/locked prune wrapper so
                            checkpoint_count_locked() > MAX_RETAIN can trigger selection even when slot_count is under
                            capacity pressure, with noncheckpoint selection disabled for that trigger. Do NOT wire the
                            older remove_unpinned_checkpoints_above_retain_count_bounded_locked as-is, because it does
                            not honour hot-zone membership/distance ordering. Reuse/extend the existing composite
                            selector path, add summaries/KDT, and add selftests for below-capacity flagged-count trim,
                            frame-0 retention, pinned/hot-zone checkpoint protection, and K-bounded repeated passes. So
                            (B) is feasible but is a real new proven primitive.
[#0040 | D | r4 | agree]    REVERSAL: I withdraw my earlier lean to (A). I verified the coder's #0037 scenario against
                            the live constants: CHECKPOINT_MAX_RETAIN=48, min active_ceiling=150 -> overflow trigger=165.
                            Cut-heavy content promotes every detected cut to is_checkpoint (CMS §6.3 "promote every detected
                            cut"), so e.g. frames 0..99 can give ~100 flagged slots while slot_count=100 stays UNDER the 165
                            capacity trigger -> 100 flagged > MAX_RETAIN 48 with NO prune firing. Under (A), MAX_RETAIN is
                            effectively unenforced in exactly CNR3's target workload (analogue/VHS = cut-heavy). The CMS's
                            own CR5 note ("if cut-heavy material keeps the pool pinned at 48... raise it") assumes an active
                            trim toward MAX_RETAIN, confirming MAX_RETAIN is intended as a REAL bound. So (A) is a
                            reliability regression; the correct resolution is (B). Coder's scoping accepted: extend the
                            composite-selector path so checkpoint_count_locked() > MAX_RETAIN can trigger selection even
                            below capacity pressure, non-checkpoint selection disabled for that trigger, honouring
                            hot-zone/distance ordering; do NOT reuse remove_unpinned_checkpoints_above_retain_count_bounded_locked
                            as-is (it ignores hot-zone ordering); add summaries/KDT + selftests (below-capacity flagged trim,
                            frame-0 retention, pinned/hot-zone checkpoint protection, K-bounded repeated passes). -> designer
                            position now (B).
[#0041 | X | r4 | RULING]   X RULING on SR-C-04 / SR-D-05: adopt option (B). Rationale: CHECKPOINT_MAX_RETAIN is intended
                            as a real bound; on cut-heavy footage (CNR3's core workload) option (A) leaves the checkpoint
                            flag-set unbounded below capacity pressure (verified #0040). Add the independent checkpoint
                            retention trigger as a new PROVEN primitive (coder scope in #0037/#0040), built to the SR-D-07
                            helper order and the single-activation scope (SR-D-04); the concurrent case rides fmParallel.
                            This is the Step 0 CMS output: CMS §6.3 stays as written (MAX_RETAIN is a real trigger); the
                            implementation gap is closed by (B), NOT by weakening the CMS prose. -> SR-C-04 and SR-D-05
                            RESOLVED via ruling. C: please CONFIRM agreement with (B) and with the scope, OR raise any
                            single-activation safety concern with adding a second prune path in the same helper.

### SR-C-05   [severity: MAJOR]   [status: AGREED]
Anchor: CMS §5.5/§5.6/§8.7 AS7 zone retirement / Cnr3OutputCacheCore::retire_decay_eligible_hot_zones / execute_bounded_prune_pass_locked
Finding (C): The current prune execution helper does not retire decay-eligible hot zones before candidate selection.
If live wiring only calls record_hot_zone_observation at arInitial and execute_bounded_prune_pass after stores, old
zones can remain active until some separate caller retires them. That would be correctness-safe but policy-wrong and
could make pruning ineffective by over-protecting stale regions.

Comments:
[#0017 | C | r1 | question]  I recommend the live contract explicitly says when AS7 runs. My implementation lean is:
                            arInitial records observation(N); the live prune wrapper calls the locked retirement
                            predicate for the current frame before candidate selection, in the same outer prune
                            critical section if feasible. D: confirm whether retirement should be eager at arInitial,
                            lazy inside prune, or both, and whether observation-before-retirement ordering matters.
[#0029 | D | r2 | agree]    Agree retirement must be explicit in the contract. Answering #0017: my position is LAZY
                            retirement INSIDE the prune pass, not eager at arInitial. Basis: CMS §5.6 says the
                            retirement test ("no pins in range AND decay_margin elapsed") is answerable cheaply in the
                            same locked prune pass from existing pin state. So the combined locked prune helper (SR-C-01)
                            should: retire decay-eligible zones FIRST (so stale zones stop over-protecting), THEN select
                            candidates. Observation(N) happens earlier at arInitial; retirement rides prune. Ordering
                            does matter: retire-before-select within the locked pass, else a stale zone wrongly shields a
                            candidate. So the new combined helper must call the locked retirement predicate before
                            candidate selection. -> AGREED: lazy retire-before-select inside the prune pass.


### SR-C-06   [severity: MINOR]   [status: AGREED]
Anchor: live proof observability / temporary KDT vs later D-SUM-10 and D-SUM-11
Finding (C): The hot-zone/prune wiring proof will need minimal live observability before the full diagnostics arc.
Existing cache-core selftests prove the componentry, but an external live proof needs to see that arInitial observation
and post-store prune were actually invoked and what the trigger decided. This does not have to be permanent D-SUM yet,
but the proof should not rely only on code inspection.

Comments:
[#0018 | C | r1 | question]  Proposed low-risk approach: add temporary KDT fields/lines for live hot-zone observation
                            and prune trigger/execution during the wiring phases, explicitly outside the formal D-SUM
                            framework and removable/replaceable during the diagnostics arc. D: acceptable, or should
                            the first wiring patch remain silent and prove live invocation by selftest-only hooks?



## --- Coordinator findings / rulings (SR-X-*) ---

<!-- SR-X-01 .. : optional; X may raise findings or record rulings here -->


[#0030 | D | r2 | agree]    Agree a live proof needs more than code inspection. Answering #0018: temporary KDT
                            fields/lines for (a) arInitial hot-zone observation and (b) post-store prune trigger
                            decision + execution outcome are acceptable and preferred over selftest-only hooks, since the
                            wiring's whole point is live-path behaviour. Keep them explicitly OUTSIDE the formal D-SUM
                            framework and removable/foldable into D-SUM-10/D-SUM-11 during the diagnostics arc. This
                            mirrors how K.1F/D-series used temporary KDT before diagnostics. -> AGREED: temporary KDT for
                            the wiring proof, foldable later.


### SR-D-07   [severity: MAJOR]   [status: OPEN]
Anchor: synthesis of SR-C-01 + SR-C-02 + SR-C-05 / the combined locked prune helper ordering
Finding (D): The exchange has converged on a SINGLE combined locked helper whose internal ORDER is now the core
of the wiring contract, and we should ratify that order explicitly so the coder builds to one agreed sequence.
Proposed canonical order, all inside one cache_mutex_ scope (vectors pre-reserved outside the lock per SR-C-03):
  (1) store / adopt the frame (store_owned_frame_locked or the target/frame0 store);
  (2) apply checkpoint promotion if applicable;
  (3) if the caller is an AS2 CONSUMER: pin/record the slot in pin_list (output[N] from target/frame0 is NOT
      pinned — it is protected by the arInitial hot-zone observation of N, SR-C-02);
  (4) retire decay-eligible hot zones (lazy, SR-C-05);
  (5) evaluate the prune trigger decision and, if required, select candidates + detach victims (SR-C-01);
  (6) unlock; free duplicate losers and detached victims OUTSIDE the lock.
PREREQUISITE (ordering across the activation): record_hot_zone_observation(N) at arInitial BEFORE any
arAllFramesReady store-prune, so output[N] is zone-protected at step 5 (SR-C-02).
QUESTION: C — does this canonical 6-step order match what you can build, and is step (3)'s consumer-vs-production
distinction cleanly determinable at the call site? X — please note step (5) depends on the SR-C-04 ruling
(whether a checkpoint-pool trigger is added or §6.3 is amended).

Comments:
[#0031 | D | r2 | question]  Seeding SR-D-07 to ratify the agreed helper order in one place. This is the consolidated
                            wiring contract emerging from SR-C-01/02/05; pinning it down now prevents drift when the
                            coder scopes the patch. C: confirm buildability + the step-(3) call-site distinction. X:
                            ruling needed on SR-C-04 feeds step (5).
[#0034 | D | r2 | question]  Clarification for SR-D-07 (see #0032): step (2) "checkpoint promotion" = SET the
                            is_checkpoint FLAG on the unified slot (not insert into a pool); step (5) prune selects
                            candidates from the single slots_ vector honouring the flag + retention rule. No pool anywhere
                            in the helper. Order otherwise unchanged.

[#0038 | C | r3 | agree]    Confirm buildable. The 6-step order is implementable if it is built as one new
                            cache-core/live-policy wrapper rather than by nesting the existing public
                            execute_bounded_prune_pass (which already takes cache_mutex_). Vectors and the byte estimate
                            are prepared outside; inside one cache_mutex_ scope the helper can store/adopt, apply
                            checkpoint flag promotion, optionally pin/record for AS2 consumers, lazily retire zones,
                            run the trigger/selection/detach contract, then unlock and let duplicate losers and
                            detached victims release outside. Step (3) is cleanly determinable at the call site:
                            recovery floor/hole AS2 paths pass a pin_list and require pin_recorded; target/frame0
                            production-store paths use the no-pin authoritative-return/store helper; cache-hit has no
                            store-prune. I recommend representing this distinction as two explicit wrapper entry points
                            or a small enum, not a boolean with ambiguous call sites. Step (5)'s exact trigger set
                            remains subject to X ruling on SR-C-04.
[#0039 | D | r4 | agree]    Accept the coder's r3 buildability confirmation (#0038): the 6-step combined-helper order
                            is buildable and the AS2-consumer-vs-production-store distinction is determinable at the call
                            site. No open question remains on the ORDER itself. Recording the order as the ratified wiring
                            contract. -> RESOLVED (helper order ratified; step (5) prune content depends on the SR-C-04
                            ruling below — i.e. whether the helper's prune step also carries an independent checkpoint
                            trigger).
# =========================  END FINDINGS  =========================

---

## ROUND LOG (append one line per hand-off; newest at bottom)

```text
r0  D  seeded 6 designer findings SR-D-01..06   seqs used: #0001-#0006
r1  C  appended comments on SR-D-01..06 and raised SR-C-01..06   seqs used: #0007-#0018
r2  D   appended comments on SR-C-* and raised SR-D-07   seqs used: #0019-#0034
r3  C   appended SR-D-02/SR-D-05/SR-C-04/SR-D-07 comments; no new findings   seqs used: #0035-#0038
```

---

## DECISIONS BANKED (filled as findings reach RESOLVED/DEFERRED — the Step 0 output accretes here)

```text
(none yet)
```

## CMS CHANGES PROPOSED BY THIS REVIEW (if any — a legitimate Step 0 output)

```text
(none yet)
```
