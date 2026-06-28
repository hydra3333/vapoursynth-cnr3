# CNR3 STEP 0 — Findings Register (SKELETON, empty — ready for Round 0)

**Companion to:** `CNR3_Step0_Joint_Review_PROCESS_v1_0.md` (read it first — it defines the rules).
**Subject:** joint CMS sensibility / gap review for hot-zone + prune LIVE WIRING, before any wiring patch.
**Build state at open:** P.11C arc CLOSED (.1-.5), committed CMS07-P.11C.5, 53/53. CMS07.13.
**This file is the single source of truth.** Append-only. One global comment sequence. LF line endings.

---

## REGISTER CONTROL BLOCK  (X keeps authoritative)

```text
Register status:        OPEN (Round 0 COMPLETE — designer findings seeded; ready to relay to C for Round 1)
Current round:          r0
Next free comment seq:  #0007
Authoritative copy:     held by X (coordinator)
Owners present:         D (designer), C (coder), X (coordinator)
```

**Open-finding tally (update each round):**

```text
BLOCKER: open 2 / total 2   (SR-D-01, SR-D-02)
MAJOR:   open 3 / total 3   (SR-D-03, SR-D-04, SR-D-05)
MINOR:   open 1 / total 1   (SR-D-06)
NIT:     open 0 / total 0
EXIT condition met: NO
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

### SR-D-01   [severity: BLOCKER]   [status: OPEN]
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

### SR-D-02   [severity: BLOCKER]   [status: OPEN]
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

### SR-D-03   [severity: MAJOR]   [status: OPEN]
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

### SR-D-04   [severity: MAJOR]   [status: OPEN]
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

### SR-D-05   [severity: MAJOR]   [status: OPEN]
Anchor: CMS §7.2 (active_ceiling x OVERFLOW_FACTOR; OVERFLOW_FACTOR=1.1; MIN_HARD_CEILING=150) / §6.3 (checkpoint MIN/MAX_RETAIN)
Finding (D): Two prune REGIMES, confirm both are wired, not just the non-checkpoint one. §7.2 is the
NON-checkpoint capacity trigger (pool > active_ceiling x OVERFLOW_FACTOR). §6.3 is the CHECKPOINT pool
trigger (pool > CHECKPOINT_MAX_RETAIN, prune toward CHECKPOINT_MIN_RETAIN). These are SEPARATE retention
classes with separate triggers. QUESTION: does the live wiring need to invoke BOTH (a non-checkpoint prune on
capacity overflow AND a checkpoint prune when the checkpoint pool exceeds MAX_RETAIN), and are both proven by
existing selftests such that wiring is "call the proven pass" for each? Confirm we are not wiring one regime
and silently leaving the other unwired (which would reproduce exactly the gap we are fixing).

Comments:
[#0005 | D | r0 | question]  Seeding. P.11C.5 proved checkpoint-class survival under the non-checkpoint prune
                            pass; I want to confirm the CHECKPOINT prune trigger (MAX_RETAIN overflow) also has
                            a proven pass and a live trigger site, so both regimes are wired together. C:
                            confirm from the source which proven entry points cover each regime.

### SR-D-06   [severity: MINOR]   [status: OPEN]
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



## --- Coder findings (SR-C-*) ---

<!-- SR-C-01 .. : to be added by C in Round 1 -->


## --- Coordinator findings / rulings (SR-X-*) ---

<!-- SR-X-01 .. : optional; X may raise findings or record rulings here -->


# =========================  END FINDINGS  =========================

---

## ROUND LOG (append one line per hand-off; newest at bottom)

```text
r0  D  seeded 6 designer findings SR-D-01..06   seqs used: #0001-#0006
r1  ->  (pending) relay to C: comment on SR-D-*, raise SR-C-*
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
