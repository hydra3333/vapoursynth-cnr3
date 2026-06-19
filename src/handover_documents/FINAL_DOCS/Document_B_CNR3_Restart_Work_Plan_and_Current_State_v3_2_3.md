# Document B — CNR3 Work Plan and Current Build State (CMS07.3, RESUME)

**Version:** v3.2.3 (RESUME-state work plan; focused status update. The version label is
kept at the "3.2" generation to stay aligned with Document A v3.2; the `.3` patch level
marks this update. Records C.14A as proven/committed and the isolated cache-core proving
milestone as REACHED; reframes "next" as the downstream pixel/integration arc. See §8.
Earlier sections carry forward except the version pointers below.)  
**Date:** 2026-06-19  
**Role:** Current-state / work-plan document. It states the controlling authority, the
**current build state**, the working method that has emerged, the immediate next phase,
the proof obligations, and what must not be implemented yet.

**Generation source:** repository git history (authoritative build state) + Production
Spec v2.6 §3A + CMS07.3.  
**Precedence:** volatile. If this document ever conflicts with the latest prevailing CMS,
the CMS wins. If it conflicts with Production Spec §3A on register-owned rules, §3A wins.

**v3.2.3 status note (MILESTONE — the isolated cache-core proving arc is complete):** The
controlling CMS is **CMS07.3**; the Production Spec is **v2.6**; the diagnostics spec is
**v1.4.1**. The cache core is now proven through **CMS07-C.14A** (32/32 selftests), the
aggregate capstone — H.2A (anchor pin-record), H.3A (AS2 store-consumer), C.13B (recovery
contiguity guard), and C.14A (aggregate workload + R-PROCESS-19 observe-only equivalence)
are all committed and pushed. **C.14A completes the isolated cache-core milestone:** all
cache-core mechanisms (lookup/pin, AS2 store/adopt incl. duplicate/adopt, checkpoint
monotonicity, hot-zone movement, prune trigger/select/AS5 execution, the full recovery
chain, pin-list discharge accounting) are proven to compose correctly under one combined
workload, and the D-SUM-11 compute gate is proven observe-only in aggregate (macro-on and
macro-off behavioural outcomes identical). The body of this document (§§4–5, 11) was
written at the H.2A-next moment and still describes the H.1A state; for the authoritative
current build state always confirm from the repository (§3). **The project now pivots from
the isolated-cache-core arc to the downstream arc — pixel-layer salvage, then VapourSynth
getFrame integration, then VS2026 project wiring — see §8.**

---

## 0. IMPORTANT — this is a RESUME, not a fresh start

The earlier Document B (v3.1) described starting the cache-core build from scratch (rename
files to `.txt`, build the first milestone, etc.). **That is no longer the situation and
that framing is obsolete.** The CNR3 cache-core build is well advanced: it has been built
incrementally and proven phase-by-phase, and is currently proven through phase
**CMS07-H.1A**, with **28 of 28 isolated cache-core selftests passing**.

A coder chat reading this pack is **resuming an in-progress, proven build**. Do not
re-propose the file layout, do not rename files, do not rebuild already-proven phases, and
do not treat the "first milestone" as the current task. The current task is the **next
phase** (see §5). Confirm the build state for yourself from the repository before
proposing anything (see §3).

---

## 1. Controlling authority

- The latest prevailing CMS is **CMS07.2** (`cnr3_cache_manager_design_v7_2.md`), the
  controlling design authority. It supersedes CMS07.1 and CMS07.0.
  - CMS07.1 added §6.6 (checkpoint flag is **monotonic** under duplicate stores: a
    checkpoint-eligible duplicate may **promote** an existing non-checkpoint slot; a
    non-checkpoint duplicate never **demotes** a checkpoint; first-in-best-dressed governs
    the frame *data*).
  - CMS07.2 added a non-normative **companion document** reference
    (`CNR3_CMS_Future_Investigations_and_Open_Questions_v7.2.md`), which is NOT part of the
    coder handover pack and NOT controlling. Ignore it for implementation; it records
    deferred tuning questions only.
- References to "CMS07.0" anywhere (including reproduced rule text in Document A) mean the
  latest prevailing CMS, currently CMS07.2, per the CMS's own version-neutrality rule.
  Specific CMS section pointers are version-specific and must be re-checked against CMS07.2.
- If the CMS conflicts with, or is unclear in alignment with, prior material, the CMS wins
  unless the user explicitly says otherwise.
- If the CMS itself is silent, ambiguous, or incomplete on an implementation point, **stop
  and ask.** Do not guess or improvise.

---

## 2. Handover-pack state

```text
Controlling design:   CMS07.2 (cnr3_cache_manager_design_v7_2.md), included unchanged.
CMS companion:        CNR3_CMS_Future_Investigations_and_Open_Questions_v7.2.md
                      (NON-NORMATIVE, NOT in this pack as authority; reference only).
Production Spec:      v2.4 (CNR3_Handover_Pack_Production_Spec_v2_4.md), §3A populated,
                      includes R-PROCESS-19.
Diagnostics spec:     v1.3 (cnr3_diagnostics_specification_v1_3.md), subordinate to the
                      CMS and §3A.
Document A:           v3.2, reproduces §3.2 canonical context and the §3A register
                      (including R-PROCESS-19).
Document B:           this v3.2 resume-state work plan.
Coder introduction:   the v3.2 resume introduction.
Code state:           CMS07 cache-core built and proven through CMS07-H.1A (28/28
                      selftests). Source is in active .h/.cpp build under vs/cnr3.
```

---

## 3. FIRST ACTION in the new coder chat — confirm the build state from the repository

Before proposing or coding anything, the coder must re-establish the build state from the
authoritative source (the repository), not from this document's say-so. This restores the
project's standing "prove it, do not assert it" discipline from the first action:

```text
1. Read the recent git log (e.g. last ~25 commits). Confirm the latest commit is the
   CMS07-H.1A bounded recovery search scaffold, and that the F-series and G-series
   phases listed in section 4 are present.

2. Read src/cnr3_build_config.h and confirm the edit-version marker reads:
       CMS07-H.1A-as1-bounded-recovery-search-scaffold-proof

   (How this works: cnr3_build_config.h holds an inline constexpr string
   CNR3_EDIT_VERSION that is bumped to the current phase name at each phase. The
   selftest runner prints it as "edit_version: ..." on every run, so the console
   output always identifies which build/phase produced it. It is for human
   diagnostics and build identification ONLY and must never be used for control
   flow. Bumping it is part of each phase's edit.)

3. Build and run the isolated cache-core selftest (Debug and Release) and confirm:
       normal:       28/28 PASS, exit 0
       forced-fail:  27/28 PASS, 1 FAIL, exit 1   (--force-fail-for-harness-proof)
       verbose:      28/28 PASS, exit 0           (--verbose)

If any of these do not match, STOP and report the discrepancy to the user before doing
anything else.
```

The repository is: `https://github.com/hydra3333/vapoursynth-cnr3` (local working tree
under `E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github`). Builds are done in
**Visual Studio 2026**, x64.

---

## 4. Current build state — phases proven (history of record)

The cache core was built incrementally, each phase proven in isolation before the next, in
this order (this is the committed history; treat it as done and proven, not to be redone):

```text
Foundations and data model:
    B/C-series   scaffold, data model (slot = const VSFrame* + frame number +
                 pin_count + is_checkpoint), slot-ID source (uint64), single
                 non-recursive std::mutex (RAII-guard only), store, lookup/addref,
                 clear/teardown, selftest runner + forced-fail harness.

Pins and pin-list (AS1 / section 4.3 indivisibility):
    C.7          slot pin/unpin lifecycle
    C.8          lookup-pin reservation lifecycle
    D.1          per-invocation pin-list lifecycle
    D.2A         AS-scope comment audit/alignment to the section 8.7 register
    D.3A         AS1 combined lookup_frame_and_record_pin() — pin-and-record indivisible,
                 pin-list capacity reserved BEFORE the lock, gapped public pin APIs
                 removed (gap closed by construction)
    E.2A         reconciled the original E.2 obligation to the D.3A helper

Store + first-in-best-dressed + monotonic checkpoint (section 6.6):
    E.1A         non-checkpoint store helper, first-in-best-dressed, loser freed
                 OUTSIDE the lock
    E.3A         checkpoint store flag; corrected to CMS07.1 monotonic rule
                 (promote-on-eligible-duplicate, never-demote, data first-in-best-dressed)

Prune building blocks:
    F.1A         central remove helper (rejects pinned; detach under lock; free after lock)
    F.2A         bounded selected-detach (AS5 batch-detach shape)
    F.3A         unpinned non-checkpoint selection (hot-zone-exclusion deferred)
    F.4A         checkpoint retention boundary (retain floor; never frame 0; never pinned)

Hot-zone model and prune assembly:
    G.1A         cache policy constants + CR1-CR5 coherence comments + static_asserts
    G.2A         hot-zone data model
    G.3A         hot-zone slide/spawn
    G.4A         hot-zone capacity merge
    G.5A         hot-zone retirement/decay
    G.6A         D-SUM-11 hot-zone counter model (observe-only; first production D-SUM)
    G.7A         hot-zone prune-protection selection (the hot-zone-exclusion clause)
    G.8A         prune-victim distance ordering (greatest distance from NEAREST active
                 hot-zone boundary; multi-zone min-distance metric)
    G.9A         composite prune candidate selection (assembles all clauses, read-only,
                 bounded global top-K across checkpoint/non-checkpoint pools)
    G.10A        prune trigger hysteresis decision (pure arithmetic: fire strictly above
                 active_ceiling * 11/10; prune TOWARD active_ceiling; hysteresis gap)
    G.11A        AS5 prune execution (decide+select+detach under one lock; batch freeFrame
                 OUTSIDE the lock; K-bounded)
    G.13A        D-SUM-11 prune-rejection counter wiring (observe-only; R-PROCESS-19
                 macro-off proof produced and clean)

Store-and-record atomic (AS2):
    G.12A        AS2 combined store_owned_frame_and_record_pin() — store OR adopt
                 first-in-best-dressed winner + monotonic checkpoint promotion + pin +
                 pin-list record, all under one lock; loser freed OUTSIDE the lock (held
                 in the by-value public parameter so it releases after the nested lock
                 scope — this is commented in the code and must not be "simplified").
                 NOTE: every AS2 call records one pin to discharge, INCLUDING a
                 duplicate-store call that rejects the incoming frame and pins the
                 existing winner.

Recovery planning (AS1, read-only):
    H.1A         bounded recovery search scaffold (CURRENT LATEST). Read-only planning:
                 descend from requested_frame - 1, inclusive lower bound
                 requested_frame - B clamped to 0 (B = HOT_ZONE_BACK_RADIUS = 50),
                 nearest present cached output wins (checkpoint flag irrelevant to the
                 search), hole catalogue = anchor+1 .. requested-1, requested frame is
                 the repair TARGET and is NOT a hole-catalogue entry. No pins, no AS2,
                 no source, no recompute.
```

Current selftest count: **28**. Edit marker: `CMS07-H.1A-as1-bounded-recovery-search-scaffold-proof`.

---

## 5. Immediate next phase — CMS07-H.2A (to be regenerated)

The next phase is **CMS07-H.2A — AS1 recovery anchor pin-record proof.** It extends H.1A
just far enough to hold the selected recovery anchor safe.

```text
H.2A purpose:
    Compose the H.1A bounded recovery planner with the existing D.3A-style
    lookup-pin-record primitive, so the selected anchor is pinned and recorded under one
    cache lock.

H.2A must apply the same lessons proven at D.3A and G.12A:
    - reserve the hole-catalogue capacity AND one pin-list entry BEFORE the lock;
    - bounded recovery search + anchor pin + pin-list record occur under ONE lock;
    - no split public pin path (no bare public pin/record that could be used separately);
    - no-anchor case records no pin; requested-frame-only case records no pin;
    - pinned anchor prevents clear() until the pin is discharged;
    - discharge returns cache total_pin_count and pin-list count to zero;
    - cache invariants remain clean.

H.2A explicitly does NOT:
    - call AS2; store through recovery; recompute; request or retrieve source frames;
      return frames; prune; change any D-SUM gate; wire getFrame; touch source lifecycle
      or pixel behaviour.

H.2A does NOT trigger R-PROCESS-19 (it introduces/changes no D-SUM compute gate).
```

**IMPORTANT:** an earlier H.2A patch was drafted by the previous coder chat but was
**never reviewed and never applied, and has been discarded.** Do not look for or rely on
it. Regenerate H.2A fresh from CMS07.2 and the H.1A code, as a read-first patch (the user
reviews the patch before applying), consistent with how D.3A and G.12A were handled.

---

## 6. The working method (how this project runs — follow it)

This project has a settled, proven working rhythm. Follow it exactly:

```text
1. Stop-review-approve gates:
   The coder proposes/analyses and reports. The user (with a separate designer review)
   checks the proposal against the spec. Only after the user approves does the coder
   produce code. Do not jump from idea to committed code.

2. Read-first patches for load-bearing phases:
   For any phase touching atomic/lock boundaries, pin-and-record indivisibility, the
   prune trigger, recovery bounds, or AS2 — produce the patch for review BEFORE applying.
   The user will have it read against the spec first.

3. Prove it, do not assert it ("a test that can only pass is not a proof"):
   Every phase ships a selftest with GENUINE failure modes — the test must be able to
   FAIL if the behaviour is wrong, and the scenario must be constructed so a plausible
   wrong implementation produces a DIFFERENT, detectable result. Avoid scenarios where
   the correct answer and a likely-wrong answer coincide.

4. The four-way test run is the standard exit evidence for a phase:
       x64 Debug   normal        -> expect N/N PASS, exit 0
       x64 Release normal        -> expect N/N PASS, exit 0
       x64 Release --force-fail-for-harness-proof -> expect (N-1)/N, 1 FAIL, exit 1
       x64 Release --verbose     -> expect N/N PASS, exit 0
   Report ACTUAL console output, not predicted. The forced-fail run proves the runner
   can actually fail. (Plus the R-PROCESS-19 macro-off run whenever a D-SUM gate changes.)

5. Selftest count discipline:
   Behaviour-adding phases add exactly one selftest (count rises by 1). Audit /
   reconciliation / comment-only / corrective phases do NOT add a test (count stays).
   The forced-fail harness count tracks the total.

6. --verbose trace:
   The selftest harness prints a human-followable trace per scenario ONLY under
   --verbose (normal runs stay quiet). This is test infrastructure (it may print); it is
   on the test-harness side of the diagnostics boundary, separate from production D-SUM
   counters, so the eventual aggregate proof can still show production counters are
   observe-only. Add a --verbose trace for each new scenario.

7. Commit discipline:
   On PASS, commit with a Visual Studio-style title/body (R-PROCESS-04). Title form:
   "CMS07-<phase>: <short imperative>". The body lists what the phase adds, the explicit
   deferrals ("does NOT ..."), and a Verified: block with the actual run results.

8. Module-boundary discipline for diagnostics:
   Production D-SUM counter state lives in cnr3_cache_diagnostics.*; the generic
   cnr3_diagnostics.* is the stderr OUTPUT BOUNDARY only and must not accumulate D-SUM
   counters. Counter increments are observe-only, allocation-free, saturating, and never
   alter behaviour or print inside a lock.
```

---

## 7. Invariant lock/ownership disciplines (CMS-defined; never violate)

These have been held at every atomic so far and must continue:

```text
- ONE cache-wide non-recursive std::mutex; RAII lock_guard only (no manual lock/unlock).
- AS1-AS7 atomic-scope register (CMS section 8.7) is designer-owned and inviolable:
  implement each scope exactly; do not shrink, split, merge, reorder, or reinterpret.
  If implementation reveals a needed operation the register does not cover, raise it to
  the user; do not invent an ad-hoc lock scope.
- Decide INSIDE the lock, execute the slow part OUTSIDE it. The in-lock pinning/detaching
  is what makes the outside-lock work safe (find-then-pin, decide-then-detach).
- freeFrame is NEVER called inside the cache lock. Detach under the lock, accumulate the
  owned frames, free them after the lock releases (in prune: batch freeFrame outside).
- pin-and-record is indivisible (section 4.3); pin-list capacity is reserved BEFORE the
  lock so the in-lock append is allocation-free.
- V5 firewall: VapourSynth ref-count atomicity protects a single addFrameRef/freeFrame
  only. It gives NOTHING over lock scopes and is not a licence to pin outside the lock or
  shrink any critical section.
- VapourSynth lifecycle (section 4.3 rule): any source frame retrieved in
  arAllFramesReady must have been requested in arInitial of the same activation. (Not yet
  reached in the build, but binding when getFrame integration arrives.)
- Checkpoint is a separate eviction-protection flag, NOT a pin. There is exactly one pin
  concept: consumer-claim recorded on the per-invocation pin-list.
- Hot zones are prune-policy hints only, NOT active-liveness guarantees. Pins guarantee
  liveness.
```

---

## 8. Remaining work plan (after C.14A — cache-core milestone reached)

```text
STATUS UPDATE (2026-06-19): H.2A (AS1 recovery anchor pin-record), H.3A (AS2 recovery
store-consumer), and C.13B (recovery-plan contiguity guard) are all PROVEN, committed, and
pushed. Selftest count is now 31/31. The G.12A usage note was made load-bearing at H.3A
exactly as anticipated (every AS2 call records one pin to discharge, including the
duplicate/adopt case where the incoming frame is rejected and the existing winner is
pinned) and was proven there.

C.13B — recovery-plan contiguity guard — PROVEN (committed 2026-06-19):
    A production hard-status guard enforcing the CMS §9.6 current-minimal-recovery contract
    (nearest-anchor + contiguous holes). An internal validator
    (cnr3_current_minimal_recovery_plan_status) is called at every success return of the
    planner atomic plan_bounded_recovery_search_locked() (source guard, in-lock, pure
    bounded arithmetic) AND at the start of store_recovery_plan_hole_owned_frame_and_record_pin()
    before AS2 delegation (consumer guard, outside the lock). A non-contiguous /
    AS3-positive / requested-as-hole / self-inconsistent plan returns invariant_violation.
    This makes CMS §9.6's contiguity invariant self-enforcing: a future maintainer who
    breaks contiguity (e.g. while implementing the deferred sparse-plan revision) trips the
    guard rather than producing wrong output silently. The consumer guard also consolidated
    the three former ad-hoc H.3A plan checks into the single authoritative validator, so the
    planner and consumer now enforce the same contract via the same code. Detection only:
    the cache core returns hard status and emits NO stderr; the developer-alert is future
    integration work (CMS §9.6.4).

H.4A / AS3 — DEFERRED (decision 2026-06-19, CMS §9.6):
    AS3 (reused-frame pin during ascending fill) is RESERVED but DEFERRED. Under the
    currently proven nearest-present-start-point + contiguous-hole planner, no AS3-positive
    reused-intermediate state is reachable: a present frame between start point and
    requested would have become the start point; an absent one is a planned hole consumed
    by AS2. So AS3 has no reachable trigger and must NOT be built against an unreachable
    synthetic plan shape, nor the planner extended to sparse reused-intermediate frames,
    without a future separately approved sparse-plan / recompute-avoidance CMS revision.
    The concurrent "planned hole became present before this activation's AS2 store" case is
    already handled correctly by AS2 first-in-best-dressed duplicate/adopt (proven H.3A);
    it is expected fmParallel-class concurrency, not an error. Current recovery correctness
    path = H.2A anchor pin-record + H.3A AS2 planned-hole store/adopt; requested frame N is
    handled separately by later return/output authority. NOTE: the C.13B guard is the
    tripwire that will need revising/relaxing when the sparse-plan revision is undertaken.

C.14A — aggregate cache-core proof — PROVEN (committed 2026-06-19): THE CACHE-CORE MILESTONE.
    Combined-workload proof across lookup/pin, AS2 store/adopt (incl. duplicate/adopt),
    checkpoint monotonicity, hot-zone movement, prune trigger/select/AS5 execution, and the
    full recovery chain (bounded planning, anchor pin-record, AS2 planned-hole fill),
    composed in ONE selftest (aggregate_cache_core_workload) with four labelled
    sub-scenarios. Count 31 -> 32. It BUILT ON the proven C.13B guard — exercising it
    (rejecting a hand-constructed corrupt plan), not re-implementing it.
    R-PROCESS-19 AGGREGATE OBSERVE-ONLY PROOF DONE: the same aggregate selftest was run with
    CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE defined AND manually commented out; both produced
    identical non-D-SUM behaviour (Release normal 32/32 exit 0, forced-fail 31/32 exit 1,
    verbose 32/32 exit 0), proving the D-SUM-11 compute gate is observe-only under combined
    load. The behavioural assertions never read D-SUM counters. This is the culmination the
    compute-gate discipline (G.6A, G.13A, every D-SUM touch) was built toward.
    *** With C.14A, the isolated cache-core proving arc is COMPLETE. All cache-core
    mechanisms are proven to compose correctly, and diagnostics are proven observe-only in
    aggregate. The project now pivots to the downstream arc below. ***

NEXT — downstream of the now-proven, complete cache core (the new arc):
    1. Pixel-layer salvage (V8.1): native-depth int64 accumulator, weighted blend,
       response tables, downsampled-luma, in-compute scene-change detection. Salvage from
       the high-value inventoried files (see §8.5): the explicit-previous-output processing
       core, the vscnr2 response tables, the memory diagnostics — study/adapt per-case with
       approval. CNR2 / vscnr2 is pixel-maths reference ONLY (never its recovery/predecessor
       logic — see R-ARCH-06). This is the likely next phase; propose scope as text first.
    2. VapourSynth getFrame integration (arInitial/arAllFramesReady), source request/
       retrieve lifecycle, return-transfer. The Category-B developer-alert (CMS §9.6.4)
       belongs here, at integration/error-mapping time: it is the EMISSION half of what the
       C.13B guard DETECTS — map the hard status to clean filter failure plus a bounded
       one-shot stderr developer-alert outside locks; expected Category-A duplicate/adopt
       stays silent. The VS-LIFECYCLE-01 rule (source frames retrieved in arAllFramesReady
       must have been requested in arInitial of the same activation) becomes binding here.
    3. Visual Studio 2026 project wiring for the full plugin build.

    NOTE on the arc shift: phases 1-3 leave the isolated cache-core selftest harness and
    move into pixel maths and live VapourSynth interaction. The PDAP delivery process,
    read-first review for load-bearing work, genuine-failure-mode tests, and explicit-known
    expected values all still apply, but the proof surface changes (pixel-correctness proofs
    and VS-integration proofs rather than cache-core selftests). Expect to discuss the new
    proof approach when the pixel/integration phases are proposed.
```

---

## 8.5 Salvage reference inventory (so the wheel is not reinvented)

Old pre-CMS07 source is retained as `.txt` reference files under
`src/superseded_by_v7/`. They are OUT of the active build. This inventory exists so the
coder knows what already exists and does not rewrite salvageable pixel-maths from scratch.

**This inventory is inferred from filenames, file headers, and project history. It is a
pointer, not an authority. Per R-ARCH-05/06/07, salvage is the SECOND step (after the
cache core is proven complete), and EVERY salvage is per-case: the coder must open and
inspect the specific file and get explicit user approval before copying or adapting
anything. Nothing here is copied silently.**

```text
HIGH-VALUE pixel-maths / utility salvage (study and adapt; do NOT rewrite from scratch):

  cnr3_frame_internal_processing.cpp/.h.txt
      The per-frame pixel/plane chroma-processing core. ALREADY architecturally
      aligned to CMS07: its header states it is deliberately separate from cache
      and scheduling policy (matches R-ARCH-02/03). Key salvage target is the
      function:
          process_cnr3_frame_with_explicit_previous_output(
              d, frame_number, src, previous_output, dst, frameCtx, vsapi)
      which takes the predecessor output as an EXPLICIT parameter — exactly the
      CMS explicit-predecessor pixel boundary. Salvage the pixel maths and this
      interface shape.
      CAUTION: the sibling process_cnr3_frame() (no explicit previous) is the
      CNR2-style internal-predecessor fallback; do NOT carry its predecessor/
      recovery semantics — CMS07's cache/recovery owns that now (R-ARCH-06).

  cnr3_response_tables.cpp/.h.txt
      vscnr2-style signed-difference Y/U/V weight-table construction (cosine
      response curve, per-plane narrow/wide, table[signed_diff + offset], value
      range 0..sample_peak). Pure table-building, no blend, no cache. Directly
      salvageable (R-ARCH-04). Documents the CNR2-compatibility quirks (mode
      chars 'x'=narrow/'o'=wide; strength/2 integer division). 

  cnr3_memory_diagnostics.cpp/.h.txt
      Process/system memory diagnostics with a well-developed structure
      (Cnr3MemorySnapshot + Cnr3MemoryStats: working set, private usage, system
      phys/pagefile/virtual, performance-info commit/kernel; min/max/sum
      accumulators; baseline-at-create for delta reporting). Significant prior
      design investment; valuable for validating cache memory behaviour at the
      C.14A aggregate stage and beyond (R-ARCH-04). 
      CAUTION: comments reference the OLD "v005" cache manager and it is
      Windows-only (GetProcessMemoryInfo / GlobalMemoryStatusEx /
      GetPerformanceInfo). Salvage the measurement/accumulation machinery and
      re-point it at the CMS07 cache counts.

TREAT WITH GREAT CAUTION — selective salvage only:

  cnr3_common.h.txt
      Likely shared types/constants/helpers (e.g. the Cnr3Data struct,
      cnr3_clamp_int, mode handling). Some small utilities are reusable, but this
      is the most likely place for stale CMS06-era assumptions to ride along.
      Inspect selectively, lift only specific named items with per-case approval,
      never wholesale.

REFERENCE WHEN INTEGRATING VAPOURSYNTH (not salvage-logic; read for shape only):

  vapoursynth-Cnr3.cpp.txt
      Old VapourSynth plugin registration + getFrame integration (filter
      registration, arInitial/arAllFramesReady call structure, return transfer).
      Useful as a SHAPE reference when getFrame integration finally arrives, but
      the actual integration must follow CMS07's AS1-AS7 register and lifecycle
      rules — NOT the old cache/recovery interaction in this file.

  CNR3_VapourSynth_Registration_and_Call_Structure_v0.6_with_call_trees.md
      A design/reference document (not code) describing the old VS registration
      and call trees. Useful orientation reading for the getFrame phase; it
      describes the OLD model, so it is orientation, not a spec.

QUARANTINE — do NOT open for ideas, do NOT salvage logic (history only):

  cnr3_output_cache_manager.cpp/.h.txt
      The OLD cache manager. This is precisely what CMS07 REPLACES. Its logic
      embodies the retired concepts (R-RETIRED-01..05: deferred pinning,
      held-ref predecessor reservation, checkpoint-as-pin, zone-as-findability,
      bounded-warmup window). Opening it for "ideas" is the main route by which
      retired concepts creep back. Do not use.

  old_cnr3_strict_cache.cpp/.h.txt
      An even older strict-streaming cache (the bridge retired by R-RETIRED-07).
      Reference-only-for-history; do not salvage its logic.

  cnr3_build_config.h.txt
      The OLD build config, superseded by the active cnr3_build_config.h. Nothing
      to salvage; reference only.
```

---

## 9. Do-not-implement list (still in force)

```text
- No continuation of CMS06.x / H15.6B; no patching of the old cache manager.
- No old cache concepts: deferred pinning, held-ref predecessor reservation,
  checkpoint-as-pin, zone-as-findability-guarantee, bounded-warmup conservative source
  window. (See the retired-fact entries R-RETIRED-01..07 in Document A / §3A.)
- No getFrame / VapourSynth wiring until the cache core is proven complete (through the
  C.14A aggregate proof). H.2A/H.3A are recovery PLANNING and store-consumer proofs in
  isolation, still no getFrame.
- No old-.txt-code salvage copied into new files without explicit per-case approval.
- No CNR2 recovery/predecessor logic, ever.
- No file renaming, file creation beyond the agreed phase, salvage copy, getFrame
  integration, or mutex/lock-scope change without explicit user discussion, agreement,
  and instruction.
- Do not re-open the diagnostics-spec v1.4 pointer or the memory-diagnostics fold during
  a coding phase; those are separate deferred documentation tasks.
```

---

## 10. Proof obligations carried toward the milestone

The original ownership/eviction proof obligations remain the bar, now proven
incrementally and to be confirmed together at C.14A:

```text
- pin/unpin balance = 0
- lookup-ref balance = 0 (acquired == released + transferred)
- no leaks; no double-free
- eviction never selects a pinned / checkpoint / in-zone slot
- prune fires only above active_ceiling * overflow_factor and prunes toward
  active_ceiling (never to the trigger point, never to empty) — hysteresis intact
- checkpoint flag is monotonic under duplicate stores (promote allowed, never demote)
- recovery search is bounded (never walks below the recovery window) and selects the
  nearest present output regardless of checkpoint flag
- shutdown clear() releases everything, with a warning on any non-zero pin
- diagnostics are observe-only: a compute-macro-disabled build preserves all non-D-SUM
  behaviour (R-PROCESS-19)
```

---

## 11. Current status summary

```text
Design authority:   CMS07.2 (cnr3_cache_manager_design_v7_2.md).
Production Spec:    v2.4 (§3A populated; includes R-PROCESS-19).
Diagnostics spec:   v1.3 (subordinate).
Code state:         CMS07 cache core proven through CMS07-H.1A; 28/28 selftests.
                    Edit marker CMS07-H.1A-as1-bounded-recovery-search-scaffold-proof.
Immediate task:     Confirm build state from the repo (section 3), then regenerate and
                    review CMS07-H.2A (recovery anchor pin-record) as a read-first patch.
Discarded:          The previous chat's unreviewed H.2A patch (do not use).
After that:         H.3A (recovery AS2 store-consumer), then C.14A aggregate proof, then
                    pixel salvage, then VapourSynth integration, then VS2026 project.
Pixel layer:        Deferred to salvage; CNR2 is pixel-maths reference only.
```
