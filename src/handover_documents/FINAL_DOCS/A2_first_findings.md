# A2 — First fmParallel Findings (banked 2026-07-12)

First-ever fmParallel run of CNR3 (selector committed CMS07-SCAFFOLD.filter-mode-selector).
Clip: ~200 frames, TINY-100, no -r 1. Compared to the fmUnordered baseline (effectively in-order).

CORRECTNESS HELD UNDER REAL RACES:
- x (computed_but_returned_after_duplicate_store) = 0 -> no wrongly-kept frame ever returned.
- Spine self-check b == f + e + x held (1066 == f + 866 + 0), all 5 lifecycle self-checks OK.
- e (bailed_after_compute) 866 == D-SUM-08 duplicates_seen 866 (independent corroboration).
- Marker omission fixed: plugin log now carries edit_version=...:fmParallel AND filter_mode=fmParallel.

OVERCOMPUTE PROFILE (the A2 problem):
- frames_computed 1066 for 200 outputs = ~5.3x overcompute.
  Split: frame0 1, floor fresh-start 179, ordinary 1, recovery hole 687, recovery target 198.
  (fmUnordered baseline: 200 computed = frame0 1 + ordinary 199; zero recovery, zero races.)
- bailed_before_compute (adopt) 1999 -> cache caught huge would-be-redundant work by adoption.
- bailed_after_compute 866 -> pure wasted compute (two threads built same frame; loser discarded).
  Mostly recovery holes (670) + recovery targets (194).
- Concurrency destroyed the fast path: ordinary_target computes 199 -> 1.

INDICATIVE SPEED (single non-deterministic runs each; NOT a benchmark):
- fmUnordered: 200 frames / 2.32 s = 86.3 fps.
- fmParallel:  200 frames / 1.40 s = 142.8 fps  (~1.65x faster DESPITE 5.3x compute).
- Interpretation: spare cores absorb the redundant work on this tiny/light clip. On heavy 576p50
  footage each wasted compute is expensive, so the 5x overcompute likely erodes/erases the speedup.
  A2 target: cut redundant recovery/floor overcompute under concurrency (pinning/anchoring so peers
  don't both build the same hole) WITHOUT losing the parallel speedup. Efficiency, not correctness.

STATUS: correctness proven under race; efficiency poor; A2 = tuning question. C1-ownership-under-race
gate (owed from 3b) is satisfied on the correctness axis for this run.

---
## A2 CRITICAL FINDING (2026-07-12) — fmParallel cache-visibility defect

NORMAL-cache fmParallel run (active_ceiling 1000, 200-frame clip) INVERTS the expected result and
exposes a real concurrency defect. Compared runs (200 frames):

  fmUnordered -r1 NORMAL : computed 200 (ordinary 199+frame0 1); 0 recovery; 0 discards; 83.3 fps; self-checks OK.
  fmParallel  NORMAL     : computed 3750 (18.75x!); recovery-hole 3550; discards 3550; 35.2 fps; 2x MISMATCH.
  fmParallel  TINY       : computed 1066 (5.3x); recovery-hole 687; discards 866; 142.8 fps; self-checks OK.

THE CONTRADICTION (why this is a defect, not mere inefficiency):
- NORMAL cache NEVER evicted (D-SUM-10 frames_evicted 0, prune_events_triggered 0) -> all frames retained.
- Stores ARE landing (D-SUM-08 stores_total 3750).
- YET 3550 recovery-hole recomputes + 3550 duplicate discards occurred INTO a cache holding all its frames.
=> Concurrent activations are recomputing frames that ARE already in the cache. Stored frames are not
   reliably VISIBLE to a concurrent activation's lookup at recovery-decision time (store-completion vs
   lookup-visibility gap under fmParallel). Peer misses -> recovers -> recomputes -> loses store race ->
   discards. Systemic (3550x), scales with parallelism (NORMAL 18.75x >> TINY 5.3x), NOT eviction.

Bigger cache is WORSE, not better (inverts the cache's purpose) BECAUSE the bug is cross-thread visibility,
not capacity. This is the C1-ownership-under-race question; answer on this evidence: NOT yet correct/coherent
under fmParallel. (Output pixels may still be right: x=0 held, no wrong frame returned; the defect is cache
COHERENCE, not returned-pixel correctness -- but that needs confirming, not assuming.)

SELF-CHECK MISMATCH (same or related root): lifecycle f-counter production tally (14) disagrees with
D-SUM-08 production stores (200); AS2 186 vs 3550. The store-origin tagging in the lifecycle counters is
wrong under races. Diagnose whether same root cause as the visibility defect or a separate counting bug.
The self-check CAUGHT it -> the instrumentation earned its keep.

STATUS: HEADLINE OPEN ITEM for the next chat. Needs cold designer+coder investigation of the
store -> frame-index visibility / lock-scope / memory-ordering path under fmParallel. fmParallel must be
treated as NOT SAFE for production until resolved. Earlier "concurrency is just wasteful" framing is
SUPERSEDED by this (it is a coherence defect, not tuning).

Caveat: single non-deterministic runs; repeat to confirm magnitude, but the mechanism (recompute-of-present-
frames into a non-evicting cache) is structural, not variance.

---
## A2 CORRECTION (2026-07-12) — it is a CONCURRENCY-DEPTH CLIFF, not a general fmParallel defect

The -r onset ladder (NORMAL cache, 200 frames, fmParallel) REFUTES the "stores not visible to concurrent
lookups" hypothesis and the "not safe under fmParallel" framing. Supersede the prior CRITICAL FINDING with:

  -r 1 : computed 200, discards 0, recovery plans 0,   span 0    -- perfectly clean
  -r 2 : computed 200, discards 0, recovery plans 99,  span 2.0  -- recovers but ZERO waste
  -r 4 : computed 200, discards 0, recovery plans 111, span 2.3  -- recovers but ZERO waste
  -r 8 : computed 3750, discards 3550, plans 199, holes 4150, span 22 -- EXPLOSION

KEY POINTS:
- At in-flight depth <= 4, fmParallel is CORRECT AND EFFICIENT: 200 computes for 200 frames, zero
  duplicates, zero discards. Recovery happens (reordering) but finds near anchors (span ~2) and never
  recomputes. So there is NO pairwise store->lookup race; two/four activations coexist cleanly.
- The explosion is a CLIFF between -r 4 and -r 8: recovery_span_mean jumps 2.3 -> 22 (multi-checkpoint),
  holes 138 -> 4150, discards 0 -> 3550. Non-linear threshold, not gradual contention.
- The bug is gated by NUMBER OF SIMULTANEOUS IN-FLIGHT REQUESTS. Past a depth threshold the anchor search
  stops finding the near (present, unevicted) frames and walks back ~22, and many plans build the same holes
  and lose at store. Likely the "present-but-in-flight/reserved, not yet committed" case (coder list B).
- MISMATCH self-check confirmed a DEFINITION error (kept-stores vs attempted-stores; gap == discards
  exactly), NOT a counter bug. Fix the self-check expectation.

REVISED STATUS: fmParallel is safe/efficient at low in-flight depth; it has a concurrency-DEPTH cliff
between 4 and 8 in flight. Retract "fmParallel not safe" (too broad). This is a tractable, -r-controllable,
reproducible target -- far better than a general race.

NEXT (fresh chat): (1) bisect the cliff with -r 5,6,7 to pin onset depth; (2) instrument recovery-plan
creation + hole catalogue (coder list A/B) to see WHY span explodes past the threshold -- especially the
"present but in-flight/reserved" hole classification; (3) correct the store self-check definition.
The default -r (~8) is above the cliff; a practical mitigation may be capping in-flight depth while the
planner is fixed.

---
## A2 RE-CORRECTION (2026-07-12) — mode-verified ladder: predecessor-in-flight race from depth 2

The prior "-r ladder" was built FMUNORDERED (wrong mode) -> its clean results were invalid, and the
"depth cliff between 4 and 8" conclusion is RETRACTED. Rebuilt genuinely fmParallel (mode line verified
in every log). Correct onset (NORMAL-ish, 200 frames):

  -r 1 fmParallel : computed 200,  discards 0,   plans 0    -- clean (r1 ~ serial)
  -r 2 fmParallel : computed 397,  discards 197, plans 197  -- WASTE STARTS IMMEDIATELY
  -r 4 fmParallel : computed 775,  discards 575, plans 198
  -r 8 fmParallel : computed 3750, discards 3550, plans 199, span 22

TRUE DIAGNOSIS:
- There is NO safe plateau. Redundant work appears at the FIRST step of concurrency (-r 2: 197 discards).
  So it IS a fundamental pairwise race (the earlier "clean at <=4" was the fmUnordered artifact).
- Scales ~linearly with in-flight depth (discards 0->197->575->3550); recovery PLANS saturate ~198
  (nearly every frame triggers one) while HOLES-per-plan grow with depth (r8 span 22 vs r2 ~1) -> the
  compounding at high depth.
- MECHANISM = PREDECESSOR-IN-FLIGHT: activation for N+1 needs output N, which a peer is still COMPUTING
  (in-flight, not yet stored), so N+1 cannot see it -> recovers -> recomputes N -> loses store race ->
  discards. Confirmed dominant from depth 2. This is coder list-B case "present but in-flight/reserved
  by another activation", now the primary case.
- MISMATCH present from -r 2 (106 vs 200): same self-check DEFINITION issue (kept vs attempted stores),
  scales with discards. Not a counter bug.
- fps stayed high & flat (399-425) across r1/2/4 (light/TINY path, sub-second) -> wasted work is absorbed
  by cores here and INVISIBLE in wall-clock; only the counters expose it. Confirms it will bite on heavy
  576p50 footage where wasted compute cannot hide.

STATUS: fmParallel has a predecessor-in-flight redundant-recompute race active at any concurrency >=2,
scaling with -r. NOT a depth cliff. The fix target: make an activation whose predecessor is IN-FLIGHT
wait for / discover the in-flight result instead of going to recovery (reserve/await, not recompute).
NEXT: instrument recovery-plan creation to classify predecessor state (absent vs in-flight vs stored)
per coder list A/B; correct the store self-check definition; consider an in-flight-reservation mechanism
so a peer computing N publishes a reservation others await rather than duplicate.

VERIFY-MODE LESSON: always confirm filter_mode= in the log, never trust the -r value or filename for
which mode ran (this ladder was first run in the wrong mode and nearly yielded a false conclusion).

---
## A2 REFINEMENT (2026-07-12) — fmParallelRequests ladder isolates it to OVERLAPPED COMPUTE (R), not planning (O)

Mode-verified fmParallelRequests ladder (filter_mode= confirmed each log):
  -r 1 : computed 200, discards 0, plans 0    -- clean
  -r 2 : computed 200, discards 0, plans 198  -- recovers but ZERO waste
  -r 4 : computed 200, discards 0, plans 198  -- recovers but ZERO waste
vs full fmParallel at same depth: -r 2 = 397 computed / 197 discards; -r 4 = 775 / 575.

DISCRIMINATION (this corrects the earlier "predecessor-publication / plan-before-publish" hypothesis):
- fmParallelRequests ALSO plans before publication -- 198 recovery plans prove the predecessor-fastpath
  still misses and routes to recovery, exactly like fmParallel. YET it produces ZERO duplicate/discard.
- The ONLY difference between the modes is whether getFrame COMPUTES overlap:
    fmParallelRequests = concurrent REQUESTS/planning (O overlaps), but SERIAL compute (R does not overlap).
    fmParallel         = concurrent COMPUTE (both O and R overlap).
- So the defect requires genuinely OVERLAPPING COMPUTES of the same predecessor, NOT merely overlapping
  requests/planning. Under fmParallelRequests, by the time a recovery plan fills its hole the predecessor
  has already been computed+published (computes are serialised), so the hole is ADOPTED, not recomputed.
  Under fmParallel, two activations compute the same predecessor simultaneously; neither can adopt the
  other's unfinished work; both compute; one loses at store -> discard.

O vs R lens (also clarifies the mode semantics):
  fmUnordered        : one activation at a time.
  fmParallelRequests : O (plan / source-request) may overlap; R (getFrame compute) is SERIAL.
  fmParallel         : both O and R overlap.
  The defect lives ENTIRELY in overlapped R (concurrent compute).

REVISED FIX TARGET: an in-flight COMPUTE reservation -- when activation A begins computing frame N, it
publishes a reservation; activation B needing N discovers the reservation and AWAITS A's result instead of
computing N itself. (Publishing N earlier / faster alone is insufficient, since fmParallelRequests already
shows planning-before-publish is harmless; the harm is only when two computes run at once.)

PRACTICAL: fmParallelRequests may be a safe operating mode as-is (clean at -r 1/2/4, recovers without waste).
Full fmParallel needs the compute-reservation fix before it is efficient (correctness held throughout: x=0).
NEXT: instrument to confirm the concurrent-compute-of-same-frame path; prototype compute reservation/await;
correct the store self-check definition (kept vs attempted).

---
## A2 — fmParallelRequests CLEAN even under heavy recovery at unbounded depth (2026-07-12, corrected)

(Corrects an earlier entry read from a mislabelled log that showed 0 plans.) The real fmParallelRequests
no-`-r` run (mode-verified), NORMAL-ish 200 frames:
  computed 200, discards 0, duplicates 0, recovery_plans 199, holes 789, span_mean 4.99,
  out_of_order 4, 82.5 fps.
This is STRONGER evidence than a 0-plan run: fmParallelRequests here exercised recovery HEAVILY (199 plans,
789 holes, genuine reordering with out_of_order=4) and STILL discarded nothing -- every hole adopted, not
recomputed. 82 fps (not 400) is the honest throughput for the mode doing real work.

Invariant confirmed across ALL fmParallelRequests runs (-r 1/2/4 and unbounded): discards always 0,
regardless of how much recovery occurs. Serial compute => any recovery adopts, never recomputes.
Note: out_of_order=4 is the first non-zero D-SUM-01 arrival disorder seen in the whole investigation -- a
useful small-but-nonzero case for A1/Q-B order-reconstruction validation.

FINAL MODE TABLE (all mode-verified, 200-frame NORMAL-ish, correctness x=0 throughout):
  fmUnordered        : 0 wasted recompute. Shipping default.
  fmParallelRequests : 0 wasted recompute at every depth incl. unbounded, EVEN under heavy recovery
                       (199 plans/789 holes). USABLE AS-IS -- a safe concurrency step up from fmUnordered.
  fmParallel         : wasted recompute from depth>=2, scales with -r (197/575/3550 at r2/4/8). Needs the
                       in-flight COMPUTE-reservation fix. Correctness held (x=0) but work explodes.

The defect is exclusive to overlapped COMPUTE (fmParallel). fmParallelRequests overlaps requests/planning
only (serial compute) and is clean even when it recovers heavily. Definitive mode characterisation.
