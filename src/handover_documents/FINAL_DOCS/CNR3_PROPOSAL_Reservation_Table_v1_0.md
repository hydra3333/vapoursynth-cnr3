# CNR3 — PROPOSAL: the Reservation Table (in-flight registry) for fmParallel — v1.0

**Date:** 2026-07-16. **Status:** DESIGNED AND PARKED — settled in a W3X/W3D design session; no scope issued,
no code written. **Baseline at writing:** `CMS07-FIX.provenance-flush` (all option-parser + rider + flush
work committed). **Pick-up:** a future designer starts at §8 (the two-step plan) and §9 (open questions);
everything above §8 is settled decision, not open discussion.

---

## 1. The problem (diagnosed 2026-07-13, evidence in A2_first_findings_v3.md)

Under fmParallel, VapourSynth runs multiple CNR3 activations concurrently. Activation N+1 needs output[N].
A peer is currently COMPUTING N, unpublished. The cache can only answer "present" or "absent" — it has no
way to say "in flight" — so N+1 plans a recovery as if N were genuinely missing. Correctness ALWAYS holds
(x=0 across ~95 sweep cells, every mode, every depth): the existing bail-before-compute (K: hole already
stored when re-checked at compute time -> skip) and bail-after-compute (L: computed but lost the insert
race -> discard) trim the damage per-hole. The residual harm is WASTED WORK: holes that slip between the
bail-check and the store (C-duplicates and L-losses), plus planning overhead, plus source frames fetched
for holes someone else was already producing. Measured (3000-frame clip, HALF, 24T): fmParallel = 5342
computes for 3000 frames (78% overcompute) at 126 fps; fmParallelRequests = 3001 computes at 337 fps.

**The reservation table is the third cache answer — "in flight" — plus the machinery to await it.**
Pattern: memo-future / singleflight specialised to a recursive chain. Closest production analogue: ffmpeg
frame-threading progress (ff_thread_report_progress / ff_thread_await_progress) — shared per-frame state +
mutex + condition variable; consumers sleep on the condvar (kernel sleep, no polling, no messages), the
producer updates the state and broadcasts; woken threads re-check the predicate. Ours is frame-granular
(the predicate is "frame N is published in the cache") because CNR3 publishes whole frames.

## 2. Why this is parked, honestly

**The ship does not need it.** The PyPI ship config is settled and triple-confirmed (committed code comment,
A2 v3 verbatim, Document B v5.0, W3X sign-off 2026-07-16): **fmParallelRequests + HALF-500 + plan-retry
OFF = 337 fps, ~0 waste.** The reservation table's only payoff is unlocking fmParallel's theoretical
headroom (overlapped compute of independent chains). Whether that headroom beats 337 fps on real footage is
an EMPIRICAL question the mechanism itself will answer — and if it does not, this branch gets honourably
retired with data. Also open: per-frame compute may be cheap enough that 78% overcompute costs less
wall-clock than it appears (W3X). Build Step A cheaply, measure via Step B, decide on numbers.

## 3. Settled design decisions (W3X rulings, 2026-07-16 session — do not re-litigate without cause)

D-R1. **Straight to Stage 2; no shipped Stage 1.5.** (Stage 1.5 = registry as classification-only.) Its
      instrumentation survives as Step B counters, but no intermediate deliverable.
D-R2. **Two clean code paths, no cross-path #ifdef lace.** Planner and orchestrator prologue/epilogue are
      SEPARATE implementations behind an exactly-one compile selector. #ifdefs live at ONE dispatch point.
D-R3. **The per-hole compute/publish engine is SHARED, not forked.** Bail-before, compute, insert,
      bail-after, pinning — the most gate-proven code in the project — exists ONCE, taking a small hook
      struct (claim-at-compute-start, publish-broadcast). Classic passes inlined no-ops so its GENERATED
      CODE is unchanged (byte-identity then confirms rather than hopes). Fork the decisions, share the
      machinery.
D-R4. **Both bails are RETAINED on the reservation path.** Cheap, proven, and the safety net for residual
      timing windows. L -> ~0 is the acceptance metric, with the bails still there to catch stragglers.
D-R5. **Await timeout = LOUD CLIP-FAIL, not fallback-to-compute.** W3X ruling: a generous deadline
      (10 seconds per await; W3X: "very few scripts run at 0.1 fps") can only be exceeded by pathology —
      leaked claim, deadlock, dying machine — and failing loudly (distinct AWAIT-TIMEOUT diagnostic: frame,
      claimant, elapsed) converts silent hangs into diagnosable failures, like an invariant violation.
      Genuine claimant frame-errors remain clip-fatal exactly as today (a failed frame is a dud clip by
      definition). NOTE: awaits can CHAIN (B awaits A who awaits C); no cycles are possible (frame order),
      each hop makes progress, worst case is hops x 10s; a broken chain still fails loudly at the first
      ghost entry.
D-R6. **Fetch avoidance is IN (arInitial skips requesting source frames for holes <= the awaited in-flight
      frame).** Consequence accepted with D-R5: no compute-fallback exists, so not having those source
      frames is fine. This removes any need for multi-round activation (staged re-requesting) entirely.
D-R7. **Registry sized >= 2 x numThreads** (runtime, instance startup). Rationale: alive activations
      (arInitial done, parked on upstream) are NOT capped at numThreads — only executing workers are.
      Cheap either way; exact sizing estimated at scope time; defined saturation behaviour required
      (unregistered = treated as absent = today's behaviour, degrades safely).
D-R8. **Two-level registration.** INTENT-MARK at arInitial ("an activation for N exists") — used for plan
      shaping and fetch decisions, NEVER awaited on. HARD CLAIM at compute-start in arAllFramesReady
      ("N is being computed right now") — the ONLY thing bounded waits are placed against. This makes the
      upstream-decode-delay question moot for waits: claimants' upstream waits lengthen nobody's await.
D-R9. **Plan-retry (`CNR3_ENABLE_PLAN_RETRY_BIAS`) belongs to the CLASSIC path only** — it is the classic
      planner's private knob, physically inside that implementation. The reservation path contains no retry
      concept (awaits replace guessing). Enforced by #error guards (§6). Containment:
      ```
      CNR3_PLANNER_CLASSIC
       ├── plan-retry OFF   <- THE SHIP (fmParallelRequests + HALF-500)  [settled, do not change]
       └── plan-retry ON    <- fmParallel mitigation only (S=50/2/4; R-ARCH-08)
      CNR3_PLANNER_RESERVATION
       └── awaits, no retry concept
      ```
      HISTORY NOTE (a memory trap that already fired once): "fmParallelRequests with retry ON" was a real
      but INTERIM state on 2026-07-14, superseded the same day by W3X's own catch ("with fmParallelRequests
      there is no benefit to plan-retry?") -> counters showed 8903 useless sleeps -> OFF measured 2.7x
      faster (337 vs 122 fps) AND cleaner (1 vs 139 duplicates). Ship is OFF. Verified against committed
      code + A2 v3 + W3X verbatim sign-off.

## 4. The mechanism, end to end (what happens where)

**0. Instance startup.** Registry allocated: >= 2 x numThreads entries of
   {output_frame, state: INTENT|CLAIMED, done_flag, condvar}; one instance mutex. RULE: the mutex is never
   held across compute, across an await, or across any VS API call.

**1. arInitial (activation for output frame N):**
   a. INTENT-MARK N; construct the RAII deregistration guard (fires on EVERY activation exit path).
   b. Reservation planner runs (new code path): identify predecessor/holes as today BUT consult the
      registry per hole.
   c. **Collapse-at-F rule** (the biggest structural win): walking back from N, stop at the FIRST in-flight
      frame F (intent or claim) if one exists before the checkpoint floor. Plan = await F, compute only
      F+1..N. Recursion makes every hole earlier than F irrelevant to this activation — F is a future
      checkpoint. No F -> classic plan to the floor.
   d. Fetch: request source frames ONLY for holes this plan will compute (D-R6); always request N's own
      source. Return; VS parks the activation.

**2. arAllFramesReady:**
   a. AWAIT phase (if plan anchored on F): condvar wait-with-deadline loop on F's entry, re-checking the
      cache on each wake (F may arrive via normal publish). Total budget 10s -> loud AWAIT-TIMEOUT
      clip-fail (D-R5).
   b. COMPUTE phase, per hole ascending, via the SHARED engine with reservation hooks:
      bail-before-compute (K path, retained) -> CLAIM at compute-start -> compute -> insert into cache and,
      inside the same critical section, set done_flag + broadcast -> bail-after-compute (L path, retained).
   c. Produce and publish output N; broadcast N's entry.
   d. EVERY exit (success, error, scene-reset, bail-out, invariant-abort): the RAII guard deregisters.
      A leaked entry is the one bug class that corrupts the whole mechanism (waiters burn full deadlines on
      ghosts; classification rots) -> this guard gets a K.1E-style tiered-fatality audit at review.

**3. Counters from day one (D-SUM):** holes_in_flight_total / holes_absent_total (was collapse useful?);
   await_success_total / await_timeout_total (deadline right? mechanism healthy?);
   source_fetches_avoided_total (D-R6's payoff, measured); K and L as ever (L -> ~0 = the table works);
   plus await_wait_ms distribution if cheap.

## 5. What is genuinely different vs shared (audit map for the reviewer)

- **arInitial:** SUBSTANTIALLY DIFFERENT — near-rewrite of the decision core (registry consult,
  collapse-at-F, selective fetch, intent-mark). The classic two-pass/mutex structure may not survive
  contact; do not attempt to share it.
- **arAllFramesReady:** structurally different prologue (await phase — no classic counterpart) and epilogue
  guarantee (RAII deregister), SAME proven spine in the middle (the shared engine, D-R3).
- **Pixel layer, tables, cache core, scene detection, profiles:** untouched and unaware. Reservation-ON
  changes SCHEDULING only; output pixels remain identical (x=0 continues to be the invariant).

## 6. Build-config surface (lands in Step A)

```c
/* exactly-one planner selector (same pattern + #error guard as the filter-mode selector) */
#define CNR3_PLANNER_CLASSIC 1
//#define CNR3_PLANNER_RESERVATION 1

#if defined(CNR3_PLANNER_RESERVATION) && defined(CNR3_ENABLE_PLAN_RETRY_BIAS)
#error plan-retry-bias is a classic-planner mitigation; the reservation planner replaces it with awaits
#endif
#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS) && !defined(CNR3_FILTER_MODE_PARALLEL)
#error plan-retry-bias is valid only under fmParallel (R-ARCH-08: it throttles other modes ~2.7x)
#endif   /* <- this second guard is parked item PQ-6, landing here */
```
Valid matrix: classic+OFF (ship), classic+ON (fmParallel only), reservation+OFF. reservation+ON: won't compile.

## 7. Hazards register (bake in from day one)

H1. Leaked registry entry (the crux) — RAII on all exits; tiered-fatality audit; AWAIT-TIMEOUT is the
    runtime tripwire if it ever happens anyway.
H2. Lock discipline — registry mutex never across compute/await/VS-API; broadcast inside the cache-insert
    critical section so publish and signal are atomic w.r.t. waiters.
H3. Worker starvation — VS worker pool is finite; awaits are bounded (10s) so the worst case is a loud
    fail, never a silent all-workers-waiting hang. No unbounded wait anywhere, ever.
H4. Chained awaits — legal, terminating (frame order forbids cycles), but deadline budgets stack; the
    AWAIT-TIMEOUT diagnostic must name the awaited frame + claimant so chains are readable post-mortem.
H5. Registry saturation — defined behaviour: treat as absent (today's semantics), count it.

## 8. THE TWO-STEP PLAN (W3X ruling: big steps, not small; two, not one — diff auditability)

**STEP A — restructure, proven by IDENTITY (one patch).**
  - Introduce the planner selector (§6), reservation side absent or #error-stubbed.
  - Hookify the shared engine (D-R3): hook struct parameter, classic = inlined no-ops, ZERO codegen change.
  - Move the classic planner + plan-retry intact behind the selector.
  - Land BOTH #error guards (incl. PQ-6).
  - GATE: classic build BYTE-IDENTICAL to pre-A (R-PROCESS-19); 4-way 57/57 unchanged; deletion scan is
    mechanical — every removed line must reappear verbatim elsewhere (everything is moved or inert).
**STEP B — the reservation path, proven by BEHAVIOUR (one patch).**
  - Registry + intent-mark + RAII deregistration; reservation planner (consult, collapse-at-F, selective
    fetch); await phase (condvar deadline loop, 10s, loud fail); claim-at-compute + publish-broadcast via
    the Step-A hooks; all §4.3 counters.
  - GATE: classic build STILL byte-identical (proves B touched nothing outside the selector); reservation
    build: 4-way; x=0; invariants 0; L -> ~0 and duplicates -> ~0 on the fmParallel -r ladder; counters
    sane (await_timeout == 0 in healthy runs); then THE benchmark — real footage, reservation-fmParallel vs
    the 337 fps ship. That number decides ship-or-park.
Both steps get full due process: enriched scope, gap analysis with file:line evidence (R-PROCESS-31),
confirm-before-patch, one-to-one diff-vs-gaps audit. These are the largest patches since the K keystone.

## 9. OPEN QUESTIONS (first deliverables of Scope A / Scope B)

Q1 (Scope A, MANDATORY FIRST): cold-verify arInitial's ACTUAL pass/mutex structure (W3X recalls two mutexes
    — predecessor-identification and hole-walking). Everything in §4.1 assumes it can host the new planner
    cleanly. Memory does not cut patches (R-PROCESS-28/29 scar tissue).
Q2 (Scope A): engine hookification with provably zero codegen change on classic — template vs function-ptr
    vs if-constexpr; the byte-identical gate is the arbiter, but the mechanism should be chosen for it.
Q3 (Scope B): claim granularity — per-hole claims (simple, more mutex traffic) vs claim-the-run (one claim
    covering F+1..N; coarser, one waiter wakeup at run end). Lean per-hole first for observability.
Q4 (Scope B): does the intent-mark at arInitial carry enough info for collapse-at-F, or does F need to be
    validated again at arAllFramesReady time (the intent may have exited by then — RAII removed it — in
    which case the plan's await target is gone and the plan must degrade... NOTE: with D-R6 fetch avoidance
    the degraded plan LACKS source frames; resolution options: (a) treat vanished-F-before-await like
    timeout = loud fail; (b) re-check F at award time and re-request sources via one extra activation round
    ONLY in this rare case; (c) await F's PUBLISH rather than its entry — the cache re-check in the await
    loop already covers publish-then-deregister. Option (c) likely suffices: F deregistering WITHOUT
    publishing means F failed -> clip is failing anyway. TO BE SETTLED IN SCOPE B.)
Q5 (Scope B): await deadline — 10s per await confirmed by W3X; confirm per-await vs per-plan budget when
    awaits chain.
Q6 (after B): if reservation-fmParallel does not beat 337 fps on real footage, the park/retire decision —
    with the mechanism's counters as the recorded evidence either way.

## 10. Pointers for the future designer

- Evidence: A2_first_findings_v3.md (mode table, plan-retry sweep, the 8903-sleeps finding, HALF A/B).
- Rules: Document A v5.0 (R-PROCESS-19/25/26/28..33, R-ARCH-08); provenance ledger v3.0 (decisions incl.
  ship-config sign-offs); Document B v5.0 §NEXT WORK item 4 (this proposal supersedes that stub).
- The design conversation itself (collapse-at-F derivation, ffmpeg condvar explanation, the two-level
  intent/claim reasoning, W3X's rulings) is in the 2026-07-16 designer-chat transcript.
- Deep-research ranking of the fix approaches: banked doc "111-Eliminating Redundant Recomputation..."
  (reservation/in-flight registry = #1; parallel-prefix inapplicable — the blend is nonlinear).
