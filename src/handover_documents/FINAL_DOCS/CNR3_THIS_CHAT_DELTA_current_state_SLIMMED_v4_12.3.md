# CNR3 — THIS-CHAT DELTA: current-state companion (SLIMMED, through D.5)

**Version:** v4.12 (SLIMMED). Supersedes v4.11 (full per-phase ledger through D.5). This is the live
per-phase ledger: a one-line committed-phase INDEX, the one don't-re-derive technical finding kept in
full, the ACTIVE/NEXT phase in full, and the open owed-items. Full per-phase detail (golden chains,
proof records, prior-phase briefs) lives in the `dev_cache_manager` branch git history (prior DELTA
versions v4.0-v4.11 + the test-artifact harnesses/derivation scripts) and Document A's build-state note;
it is committed to main at project end. Nothing durable is lost — it is migrated, not truncated.
**Date:** 2026-06-27

---

## 1. CURRENT BASELINE (confirm from repo)

```text
Committed/pushed through:  CMS07-P.11C.5-scene-cut-checkpoint-recovery-anchor-proof (P.11C ARC CLOSED; .1-.5 all PROVEN)
Selftest count:            52/52 PASS  (forced-fail 51/52 exit 1; verbose 52/52)
Controlling CMS:           CMS07.13  (cnr3_cache_manager_design_v7_13.md)
Production Spec:           v2.11
Document A:                v3.5   (project context + standing rules; reproduces Spec §3.2/§3A + charter §3A.5.0)
Document B:                v3.5.1 (current-state work plan; top UPDATE block is authoritative)
Branch:                    dev_cache_manager
```
The repository is the authority — confirm CNR3_EDIT_VERSION and the selftest count from committed source.

## 2. COMMITTED-PHASE INDEX (one line per phase; full detail in dev-branch git history)

The live getFrame dispatch is FEATURE-COMPLETE across all four branches; the branch-(d) recovery arc is
COMPLETE. Per-phase golden chains and proof records are in the dev-branch git history (prior DELTA
versions + test-artifacts) and reproducible from the `D*_golden_chain_derivation.py` scripts.

```text
K.1A-K.1D    request-plan structures + KDT trace -> first REAL output frame (copyFrame fresh-start)   [git]
K.1E.2/.3    live predecessor-present compute; CLOSES R-ARCH-06 (recursive filtered-pred, 163/93)      [git]
K.1F         live direct cached-output return (branch-b cache-hit; Option-C lifecycle trigger)         [git; §3 below]
K.1G         plugin source split (no behaviour change)                                                 [git]
Recovery-Step-0  AS4 single-lock batch discharge (discharge_all takes cache_mutex once; 48->49)        [git; CMS §8.7]
D.1          exact-anchor SINGLE-hole recovery (plugin-only) — DONE                                    [git; Doc A]
D.2          exact-anchor MULTI-hole (k>=2) + bounded-window refusal (plugin-only) — DONE              [git; Doc A]
             (RUN C bounded-window refusal is TRANSITIONAL — superseded by D.3 floor-fresh-start)
D.3          floor-fresh-start recovery (plugin-only) — DONE                                           [git; Doc A]
             (materialized-floor-is-the-foundation invariant recorded in CMS §9.5 / CMS07.13)
D.4          pre-compute adopt-skip + first-in-best-dressed PRIMITIVES (selftest; 49->51) — DONE       [git; Doc A]
D.5          recovery-pin-survives-real-prune-pass (selftest, paired control; 51->52) — DONE           [git; Doc A]
             >>> branch-(d) RECOVERY ARC COMPLETE (D.1-D.5). Only deferred confidence: real concurrent
             >>> (fmParallel) scheduling, bounded to the fmParallel validation phase.
P.11C.2      live scene-change config + scdthr->threshold helper + central store-request/checkpoint   [git]
             skeleton + not-applicable KDT (frame-0/floor). Detection NOT enabled (branch-c/d deferred
             to P.11C.3/.4/.5). Threshold uses verified vsCnr2 full-frame shape + P.2A-style proportional
             round-to-nearest depth scaling (NOT vsCnr2 <<(depth-8)). 52/52 unchanged. COMMITTED.
P.11C.3      branch-c (predecessor-present) scene detection ENABLED: live _with_scene_change call +    [git]
             force_checkpoint=scene_change_detected. Proven via external .vpy (live default scdthr=10.0,
             threshold=1402): C-control diff_total=64/samples=16/detected=0/blend (byte 109 between
             100..160); C-cut diff_total=2040/samples=2/detected=1/reset (byte ==current 160/80)/
             store_as_checkpoint=1. Four-way 52/52. frame-0 not_applicable boundary confirmed. COMMITTED.
P.11C.4      branch-d (RECOVERY) scene detection ENABLED: per-hole + target _with_scene_change +         [git]
             grid-OR-detected checkpoint store; floor store + floor anchor-record LEFT grid-only
             (classified-site patch, not pattern-replace). Holes report ACTUAL resulting_slot_is_checkpoint;
             target reports _expected (status-only wrapper); adopted-skip reports not_run. Atomicity
             inherited (one-lock store+flag+pin, D.5 composition). Four-way 52/52. Proven via recovery
             .vpy adapted from D3 floor-fresh-start trigger (threshold=5606, 16x16): D-control all
             detected=0 (256/64); D-cut-at-hole hole2 detected=1/6160/10/reset/checkpoint=1(actual),
             target no-cut blend byte 255/151/83 (degeneracy fix proven); D-cut-at-target target
             detected=1/6120/10/reset/checkpoint_expected=1, reset byte 255/160/80. Both Debug+Release.
             COMMITTED.
P.11C.5      scene-cut checkpoint FOUND AS RECOVERY ANCHOR (arc finale). KDT-only: print            [git]
             anchor_is_checkpoint=%d in the live exact-anchor recovery trace (from already-populated
             recovery_plan.anchor_is_checkpoint); trace-only, no behavioural change. New cache-core
             composition selftest (52->53) proves the CACHE HALF: an UNPINNED NON-GRID frame stored as
             the sole checkpoint (frame 165) survives a real bounded prune by CHECKPOINT CLASS while an
             ordinary non-checkpoint (frame 1) is evicted (total_pin_count=0 throughout -> survival by
             class, NOT pin), then read-only bounded recovery for frame 167 anchors exactly on 165 with
             anchor_is_checkpoint=true and holes={166} (166 absent by construction). Composes with
             P.11C.4 (live scene detection feeds the checkpoint store route). Four-way 53/53 / 53/53 /
             52/53 forced-fail / 53/53, both Debug+Release. COMMITTED. >>> P.11C SCENE-CHANGE ARC CLOSED.
```

## 3. K.1F + R-LIFECYCLE (the API4 lifecycle resolution — important, don't re-derive)

**The question:** can a cache-hit (output[N] already present) return without the arAllFramesReady
phase? Investigated via multiple AI reviews + R76 doc + the R76 vsthreadpool completion path.

**The finding (settled, do not re-open without new authoritative source):**
- **Option A** (return cached frame directly at arInitial): documented only for SOURCE filters;
  NOT established safe for a non-source filter under fmParallel. REJECTED.
- **Option B** (zero-request arInitial->NULL, then return at arAllFramesReady): DISPUTED — a
  getFrame that requests ZERO frames at arInitial and returns NULL is **not guaranteed an
  arAllFramesReady callback** under R76 (zero-pending may be terminal). Not confirmable from
  quotable core source. REJECTED.
- **Option C** (ADOPTED): request exactly ONE real source frame (source[N]) at arInitial as a
  lifecycle TRIGGER to guarantee arAllFramesReady fires; return the cached output there. Valid
  under every reading, strictly inside the documented contract.

**R-LIFECYCLE (now normative in CMS §9A.1.1):** EVERY CNR3 getFrame branch requests >=1 REAL
source frame at arInitial and returns ONLY at arAllFramesReady. The branch-(b) cache hit requests
source[N] as a trigger (retrieved and IMMEDIATELY FREED at arAllFramesReady — a normal owned ref,
not consumed for compute, not stored, freed outside any cache lock). Honest cost: a cache-hit
return can be blocked by a source[N] failure even though output[N] is cached (accepted; output[N]
was produced from that same source in the same graph).

**"need locking" (settled):** means the filter's OWN mutex protecting shared per-instance state
(CNR3's cache_mutex_), a property of internal design, NOT imposed by mode and NOT constraining
fmParallel. CNR3 already satisfies it mode-independently. The fmUnordered doc sentence is advice
to avoid fmSerial, not a constraint blocking fmParallel.

**K.1F IMPLEMENTATION (plugin-only; lookup_frame_and_record_pin / lookup_frame_and_add_ref /
discharge_all — all pre-existing):**
```text
arInitial cache-hit:  lookup_frame_and_record_pin(N, frameData.pin_list)  -- pins output[N] so
                        a concurrent prune cannot evict it across the gap (AS1 rationale applies)
                      record branch=CACHE-HIT, requested_frame=N
                      requestFrameFilter(N, source)  -- the trigger
                      return NULL
arAllFramesReady:     getFrameFilter(N) -> freeFrame immediately (trigger, not consumed)
                      lookup_frame_and_add_ref(N)  -- present by pin; if absent -> invariant
                        violation, surfaced, NOT a garbage return (defensive assert)
                      discharge_all (Step-0 batch discharge; first live use in getFrame)
                      transfer cached ref to caller
```
Field rename this session: frameData `predecessor_pin_list` -> `pin_list` (now shared by the
predecessor and cache-hit branches).

**K.1F PROOF (Debug + Release):** four-way unchanged 49/49; live harness green — cache-hit returns
output[2]=128/163/93 with branch=CACHE-HIT, pixel_compute=0/p11b=0/p11c=0, trigger
requested=1/retrieved=1/consumed=0/released=1, cache_hit_pin_balance=0; regression intact
(1->161/95, 2->163/93 still compute on first request); negative control holds (first/uncached
request does NOT take cache-hit); repeated-frame-0 proves present-N dispatch precedes the n==0 gate.

**HARNESS LESSON (carry to D.1):** CNR3 has a normal downstream VS core cache, so a re-request of
an already-produced frame could be served by the CORE cache and never re-enter CNR3::getFrame
(false pass). Defeat it with `clip.std.SetVideoCache(mode=0)` on the CNR3 node (R76 mode=0 =
always disable). NOTE: SetVideoCache is a side-effecting node method that RETURNS None — call it,
do NOT reassign (`filtered.std.SetVideoCache(mode=0)`, not `filtered = ...`). The CACHE-HIT KDT
line is the definitive self-validating proof: present => getFrame re-entered and branch fired;
bytes-match-WITHOUT-KDT => core cache intercepted => INCONCLUSIVE (not a pass). Harness files:
test_K1F_once_only_harness_AB.vpy / .bat + check_y4m_constant_plane.py (committed with K.1F).

---

## 4. ACTIVE / NEXT PHASE — P.11C CLOSED; NEXT = first REAL-FOOTAGE validation

>>> P.11C SCENE-CHANGE ARC CLOSED (.1 layout, .2 skeleton, .3 branch-c, .4 branch-d, .5 scene-cut
checkpoint recovery-anchor). Scene detection is now wired uniformly across branch-a/c/d, proven on
synthetic harnesses both Debug+Release, committed through CMS07-P.11C.5. The live getFrame dispatch is
feature-complete across all four branches WITH scene-change handling.

NEXT PHASE = FIRST REAL-FOOTAGE VALIDATION. P.11C existed specifically so real video (which HAS cuts)
can be processed without blending chroma across a cut. This is the first run of CNR3 on actual footage
(not synthetic constant-plane clips); likely includes the larger 576p50 campaign. Not yet scoped --
designer-owed scope when the coordinator opens it. SEQUENCE AFTER: doc-set refresh + diagnostics forward
plan + Document B tidy (the owed "must happen soon" pass, §5), THEN the diagnostics arc (condensed 4-phase
plan in CNR3_Diagnostics_Arc_Condensed_Plan_v1_0.txt), THEN the fmParallel arc (the end goal).

--- historical detail of the now-closed P.11C arc follows ---

P.11C wires scene-change handling uniformly across branch-a/c/d before the first REAL-footage test.
Scene-change is currently DEFERRED uniformly (scene_change_deferred=1) because the D-series proofs run
on synthetic constant-plane test footage that has NO cuts. Real footage has cuts, so P.11C must wire
detection in before real video: a deferred P.11C would (a) blend chroma across a cut (visibly wrong),
and (b) fail to set the cut-checkpoint recovery relies on. **A detected cut PROMOTES to a checkpoint
(CMS §6.4 / §9.5) = an exact, longer-retained recovery anchor found naturally by the descending search**
— so P.11C INTERLOCKS with the recovery machinery just completed. Sequence: P.11C uniform wiring ->
first real-footage validation. P.11C touches the pixel pipeline AND the checkpoint/recovery interaction;
it is its own phase, not part of any D-phase. NEXT ACTION: designer-owed P.11C scope.

## 5. OWED-ITEMS LEDGER (none blocking P.11C)

- **CANDIDATE (not actioned) — production helper for adopt-or-compute outcome plumbing.** Surfaced
  during D.4 scoping: the floor and hole live paths duplicate the adopt-or-compute DECISION choreography
  inline, though the ownership-critical PRIMITIVES are already shared (lookup_frame_and_record_pin;
  AS2 store summary duplicate_existing_slot). A helper consolidating the decision is a POSSIBLE future
  cleanup — to be decided ON ITS OWN MERITS (R-PROCESS-21: proven-code change needs proposal + approval
  + selftest), justified by single-source-of-truth / fmParallel-readiness, NOT by testability, and only
  if the floor/hole logic drifts or fmParallel review shows a need. The floor/hole compute/store bodies
  differ enough (fresh-start vs predecessor-blend; generic store vs planned-hole wrapper; scalar vs
  indexed outcome) that a premature helper risks a parameter-heavy abstraction obscuring the D.3 safety
  story. NOT actioned now; recorded so it is neither lost nor rushed.

- **CANDIDATE (not actioned) — Document B deep tidy (option 2), timed BEFORE the fmParallel arc.** Document
  B v3.5.1 currently uses the layered convention: the v3.5.1 UPDATE block at the top is the authoritative
  current state (D.5 / recovery complete / P.11C next), and the older body (restart-era §5 next-phase=H.2A,
  the .txt-rename transition state, the §11 CMS07.2/28-selftest snapshot, the pixel-arc §8 work plan) is
  retained under explicit SUPERSEDED banners as history of record. This is correct and complete for
  handover (option 1, chosen). The deeper tidy (option 2) — promote the durable scaffolding (working
  method, invariant disciplines, do-not-implement) into clean current sections and move the genuinely-dead
  restart-era instances into a single consolidated "Historical / superseded restart-era record" appendix,
  so live state and archive are cleanly separated rather than banner-interleaved — is DEFERRED to the
  P.11C -> fmParallel seam. Rationale: the fmParallel arc is a major new concern (the deferred concurrent-
  scheduling confidence from D.4/D.5) and deserves a clean current-state Document B to start from; the
  recovery+pixel era closes at P.11C/first-real-footage, which is the natural boundary for the restructure.
  Do it as its own focused pass (like this handover refresh), NOT mid-handover and NOT during fmParallel.

- **FULL DOC-SET REFRESH — timed at the P.11C-closed / FIRST-REAL-FOOTAGE-PROVEN seam (coordinator: "must
  happen soon").** When P.11C.5 commits and first real-footage validation passes, do ONE focused
  documentation pass that: (a) refreshes the whole handover set (CMS, Production Spec, Documents A/B,
  DELTA, Reviewer/Role/Coder-Restart intros, Future Investigations) to the "P.11C arc CLOSED + real footage
  PROVEN" state; (b) folds in the FORWARD PLAN for the diagnostics arc -- the condensed 4-phase plan in
  CNR3_Diagnostics_Arc_Condensed_Plan_v1_0.txt (DIAG.1 framework+D-SUM-01+observe-only proof; DIAG.2
  cache-core family batch; DIAG.3 getFrame/recovery/return batch; DIAG.4 memory + close) so the next arc
  has a recorded entry point; (c) executes the Document B deep tidy (option 2, above) as part of the same
  pass since it shares this exact seam. This is the natural boundary: recovery+pixel+scene era closes, the
  diagnostics + fmParallel eras begin from a clean current-state doc set. Do it as its own focused pass,
  NOT mid-arc. NOTE: diagnostics could optionally run PARTLY before real footage (D-SUM-01/03/14 aid
  first-footage debug; observe-only so it blocks nothing) -- placement is a coordinator decision at the seam.

- **TEST ARTIFACTS / GOLDEN PROVENANCE (housekeeping; add to repo alongside the existing harnesses).**
  The recovery harnesses and their golden-derivation scripts should live in the repo test area as
  regression bases for later phases (D.3+ must keep D.1 and D.2 green):
    * test_D1_once_only_harness_AB.vpy/.bat (D.1 single-hole), test_D2_once_only_harness_AB.vpy/.bat
      (D.2 multi-hole + bounded-window refusal), and test_D3_once_only_harness_AB.vpy/.bat (D.3
      floor-fresh-start RUN A + floor-byte/hole cache-hit RUN B + D.2 exact-anchor regression RUN C +
      D.1 regression RUN D + negative control RUN E + passthrough RUN G). All reuse
      test_K1F_check_y4m_constant_plane.py.
    * D.4 and D.5 have NO separate harness or golden script: their proofs are cache-core selftest cases
      shipping in the selftest target itself. D.4 = two cases (present-frame adopt-skip primitive +
      first-in-best-dressed duplicate; 49->51). D.5 = one case (recovery-pin-survives-bounded-prune,
      paired control/protected; 51->52). Status/ownership/identity proofs, not pixels.
    * D1_golden_chain_derivation.py, D2_golden_chain_derivation.py, and D3_golden_chain_derivation.py
      are the GOLDENS PROVENANCE:
      small Python that INDEPENDENTLY re-derives the expected pixel values (D.1 147/109; D.2 anchor
      72/184 -> holes 148/96, 149/95 -> target 148/100) from the response-table + P.11B blend maths,
      (D.3 floor 56/176 -> holes 144/111, 145/109 -> target 144/113), and self-check by reproducing the
      known K.1E.3 161/95 & 163/93. They are the "answer key with its working shown" that proves the
      harness goldens are independently correct, not circular.
      Keep them with the harnesses; extend the same way for D.3-D.5 chains. NOT in the coder pack /
      not controlling design; they are test provenance.

- **D.2 BOUNDED-WINDOW REFUSAL — code reports only what the bounded search observed (decided, scope v2).**
  D.2's recovery refusal emits a SINGLE honest reason: **no-in-window-anchor** (anchor_found==false for
  the bounded interval [max(0,N-B), N-1], B=50). The code does NOT distinguish "an older anchor exists
  beyond the window" from "no prior output exists at all" — both are identical to a bounded search, and
  telling them apart would require an UNBOUNDED out-of-window search, defeating the purpose of bounding.
  The HARNESS proves the bounded-window-exceeded case BY CONSTRUCTION (Run C: establish output[0], then
  request output[52]; nearest anchor is 52 back > B=50). No cache-core helper or out-of-window search is
  added merely to enrich a reason string. (If an EXISTING primitive distinguishes the two for free, the
  richer label may be emitted, but nothing is to be added for it.) Rationale: diagnostic honesty (a label
  means only what the code knows) + avoid an unbounded scan in a refusal path. Decided designer+coder
  2026-06-27 (D.2 scope v2, coder review correction 2).

- **CMS-GAP RESOLVED (into CMS07.12) — bounded-search reporting semantics now explicit in the CMS.**
  STATUS: DONE. The clarification was added to CMS §9.5 Phase 1 (the search reports only within
  [max(0,N-B), N-1]; no-in-window-anchor does not distinguish out-of-window-anchor from no-prior-output;
  refusal reports no-in-window-anchor only) and the interval upper bound corrected to N-1. No behaviour
  change (D.2 already ships it). Relevant to D.3 (same no-in-window-anchor boundary). Original candidate:** The D.2 refusal decision above rests on a principle that recurs in D.3 (floor-
  fresh-start) and D.5 (prune pressure): *the bounded recovery search reports presence/absence only
  within [max(0,N-B), N-1]; absence within the window is not distinguished from absence of any prior
  output, and no out-of-window search is performed.* This is arguably ENTAILED by "bounded search" and
  may need no CMS text — OR a one-line clarification in CMS §9.1/§9.5 would make the boundary's semantics
  explicit for every recovery phase and would have prevented the D.2 scope's initial over-specification.
  RAISED as a CMS-GAP candidate (charter case b, low bar) for coordinator decision: add the clarifying
  line at the next CMS edit (e.g. bundled with the Doc A regeneration session), defer, or judge it
  already-implied and skip. NOT blocking D.2 (scope v2 stands regardless). Recorded so the principle is
  not silently re-introduced as an overreach in a later phase.

- **DOC-SET REGENERATION — Document A and Document B (deferred to a near-future dedicated session).**
  The current-era handover docs are refreshed (Production Spec v2.10, Role Handover v1.9, Reviewer
  Introduction v3.2, Coder Restart Introduction v6.0, this DELTA) and all carry the new Design
  Alignment and Escalation Charter (full text in Production Spec §3A.5.0 and Role Handover Part 3 §D0).
  STILL OWED:
    * **Document A** is at **v3.4 on the stale K.1D / CMS07.8 / 47-47 baseline** (it self-describes as
      "only a version number bump"; the Production Spec long noted "until Document A is regenerated").
      Needs a full STATE regeneration to **v3.5**: CMS07.8 -> CMS07.10, committed-through K.1D -> D.1,
      47/47 -> 49/49, K.1E-branch-(c)-in-flight -> all-four-branches-live, AND it must faithfully
      reproduce the Production Spec §3A register INCLUDING the charter at §3A.5.0 (per R-PACK-02:
      Document A reproduces §3A; on mismatch §3A wins).
    * **Document B** — confirm the committed current version (a v3.5 with the K.1F update block was
      produced; verify it is the repo's latest and bump to carry the D.1 + K.1G state + charter
      pointer if not already present). Target **v3.5** (or higher).
  ACTION TRIGGER: the **Coder Restart Introduction v6.0 already forward-references Document A v3.5 and
  Document B v3.5** as expected current versions (with a built-in warning that an older Doc A is stale).
  Do this in a near-future dedicated session — it is a large rewrite, deliberately NOT rushed at the
  tail of a long chat (the lesson from prior sudden chat deaths). NOT blocking D.2.

- **SCENE-CHANGE / P.11C deferral (shared across ALL live branches — record kept here so it is
  actioned, not lost).** The live getFrame keystones (branch-c K.1E.3, and branch-d D.1 onward)
  compute via P.11B and DEFER the P.11C scene-change/reset check, emitting
  `p11c_called=0 scene_change_deferred=1`. CMS §9.2 / §6.4 specify the COMPLETE recovery+compute as
  scene-change-aware: a cut detected during compute makes that output a fresh-start (copy source
  chroma, skip the recursive blend) and stores it WITH the checkpoint flag set (a cut frame is the
  ideal recovery anchor, §9.5/§12B). Deferring P.11C is correctness-safe ONLY while inputs contain no
  cuts (true for the synthetic constant-plane harnesses); on real footage a missed cut would blend
  across a scene boundary (wrong output) AND fail to establish the cut-checkpoint that recovery relies
  on. ACTION TRIGGER (action reasonably as soon as safe): wire P.11C UNIFORMLY across branch-a/c/d as
  its own keystone — NOT folded into any single branch — at the FIRST of: (i) before any real-footage /
  non-synthetic test; (ii) before branch-(d) is exercised on content where cuts can occur; (iii) once
  the live dispatch is otherwise complete (post-D.1..D.5) and before the keystone series is declared
  done. It must not be deferred past the point where real video is processed. Owning: CMS §9.2 (compute
  scene-change-aware), §6.4 (cut frame -> checkpoint), §A4 (in-compute accumulation/threshold), P.11C
  (proven in cache-core selftest, awaiting live wiring).
- branch-(d) isolated-pin causal proof.
- K.1E.2/E.3/K.1F proof-default response-table config -> real instance-config option parsing.
- **CLIP-TEST HARNESS depends on the diagnostics (confirmed 2026-06-27).** The real-footage clip-test
  harness (e.g. test_000_Example_576p50.vpy/.bat — runs 576p50 through the live plugin to NUL/encode) is
  of little verification value WITHOUT the D-SUM end-of-run summaries + selectively-gated concise per-frame
  telemetry in place: a bare run only shows it did not crash, not whether pin balance held, recovery fired
  correctly, integrity stayed clean, or scene-change/checkpoint-promotion behaved across thousands of real
  frames. Therefore the large clip-test CAMPAIGN is sequenced AFTER the diagnostics arc. P.11C's proof does
  NOT use this harness — P.11C is proven on SYNTHETIC footage with constructed cuts (KDT-observable: cut ->
  reset-used -> output[K] stored WITH checkpoint flag -> a later recovery finds it as an anchor), exactly as
  D.1-D.5 were proven on synthetic footage; the REAL-FOOTAGE validation of P.11C folds into the later
  campaign once diagnostics exist.

- **DIAGNOSTICS ARC — sequenced AFTER P.11C, BEFORE the real-footage campaign (decided 2026-06-27).**
  The D-SUM diagnostics framework is fully DESIGNED (cnr3_diagnostics_specification_v1_5.md: §2.3 the
  per-summary COMPUTE/PRINT compile-time gate pattern with the paired #error "cannot print without
  compute" cross-check, print-subordinate-to-compute; §4 the 14-family catalogue D-SUM-01..14; R-PROCESS-19
  compute-disabled observe-only proof obligation; the FAIL/WARN/INFO severity model) and
  cnr3_memory_diagnostics_spec_v2.md (D-SUM-02). What is OWED is the IMPLEMENTATION: the src.zip shells
  carry only the D-SUM-11 hot-zone COUNTER MODEL (Cnr3CacheHotZoneDiagnosticStats + observers) and NO
  end-of-run formatting/printing for any family; memory-diag is an explicit placeholder.
  SEQUENCE (big-picture aligned): (1) P.11C scene-change calc FIRST (synthetic-proven, like D.1-D.5;
  its real-footage validation folds into the later campaign). (2) THEN the diagnostics arc — implement the
  D-SUM families with their compute/print gates + per-family R-PROCESS-19 observe-only proofs, including
  D-SUM-14 (scene-change/recursive-reset/checkpoint-promotion telemetry — NOT bundled into P.11C) and
  D-SUM-02 (memory, via salvage from the archived deprecated memory-diag code in GitHub). The diagnostics
  give BOTH the verifiable real-run telemetry (ownership/pin-balance D-SUM-04, integrity D-SUM-05, prune
  D-SUM-10, recovery D-SUM-12) AND the selectively-gated concise per-frame watch telemetry (hot-zone D-SUM-11,
  prune, hole-filling) needed to observe behaviour under fmParallel. (3) THEN first verifiable real-footage
  run + the large 576p50 campaign. (4) THEN the fmParallel arc.
  FIRST STEP of the diagnostics arc (Claude-owed): produce a concise 2-line summary of EACH of the 14
  D-SUM families (purpose + what it gates/observes) so the coordinator can choose the core implementation
  subset vs deferred. The end-of-run integrity report + abort_on_error (default False) + warn-vs-hard-fail
  severity policy (below) are PART OF this diagnostics arc, not separate.
  CODER PREP (recorded; action at diagnostics-arc kickoff, NOT during P.11C): the diagnostics-arc coder
  package includes cnr3_diagnostics_specification_v1_5.md + cnr3_memory_diagnostics_spec_v2.md + a pointer
  to the ARCHIVED deprecated memory-diag code, framed as ORIENTATION + SALVAGE REFERENCE — the memory-diag
  is mostly salvageable but is DEPRECATED (predates CMS07 / the D-SUM gate framework / R-PROCESS-19), so it
  is adapt-to-the-current-gate-pattern-and-prove-observe-only, NOT paste-in, and per R-PROCESS-08 /
  R-ARCH-05/07 the coder reads early but does NOT implement until the diagnostics phase is scoped+approved.

- longer sequential run beyond N==2; end-of-run integrity report + abort_on_error (default False)
  + warn-vs-hard-fail severity policy.
- full CMS fmParallel implications review (the CMS07.9 skim was non-exhaustive; FI-05 two-instance
  resource model likely genuine gap).
- fmParallel-phase (companion FI register): FI-05/06/07/08; operational two-instance diagnostics;
  test-tunable hot-zone/prune thresholds; cache-hit fast-path (Option A/B) revisit only if the one
  trigger-fetch on cache-hit is ever measured as significant AND confirmed from quotable core source.

---

## 6. DOCUMENT SET (current versions — new-chat reading order)

```text
CMS (design authority)     cnr3_cache_manager_design_v7_13.md             (CMS07.13)
Production Spec            CNR3_Handover_Pack_Production_Spec_v2_11.md     (§3.2 context master; §3A register + charter §3A.5.0)
Document A                Document_A_CNR3_Project_Context_and_Standing_Rules_v3_5.md   (context + standing rules)
Document B                Document_B_..._Current_State_v3_5_1.md          (current-state; top UPDATE block authoritative)
Diagnostics spec          cnr3_diagnostics_specification_v1_5.md          (subordinate)
Role/Reviewer Handover    CNR3_Designer_Reviewer_Role_Handover_v<current>.md
Reviewer Intro            CNR3_Handover_Introduction_to_new_reviewer_chat_v<current>.md
Coder Restart Intro       CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v<current>.md
Current-state (this)      THIS slimmed DELTA (live per-phase ledger)
```
**Authority:** CMS -> Production Spec §3A -> diagnostics -> handover pack. Repository wins over any
document on build state. **Reading order:** Reviewer/Coder intro -> CMS07.13 -> Document A v3.5 ->
Document B v3.5.1 (top block) -> this DELTA.
*(Intro/Role-Handover versions are swept to current as the final step of the doc refresh.)*

— End of CNR3 THIS-CHAT DELTA (slimmed), v4.12.
