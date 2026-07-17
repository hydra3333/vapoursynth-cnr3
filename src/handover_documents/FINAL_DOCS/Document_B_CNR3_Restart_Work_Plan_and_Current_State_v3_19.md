# Document B — CNR3 Work Plan and Current Build State (CMS07.15, RESUME)

*** v3.19 UPDATE — 2026-07-08 — DIAG.3c.2 COMMITTED (dump-on-bail + failure detail); DIAG.3c plan-trace family CLOSED; DIAG.4 memory is the LAST in-plugin step ***
Supersedes v3.18. DIAG.3c.2 COMMITTED + PUSHED, marker CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail.
Failure half of the plan-trace: Set 5 fail_reason (16 site-derived categories), Set 4 X=not_reached / E=error_here,
once-guarded bail dump. Additive FAILED writes at all 65 bail sites; recovery X-derivation via the shared inline
helper cnr3_live_plantrace_make_failed_result_from_request (used by live AR + induced-bail selftest). Proof CLOSED:
all-on 57/57 x3 + 56/57; macro-off 56/56 x3 + 55/56 (compiles out); induced-bail (arInitial minimal INVALID_LIFECYCLE
E=n X=[]; recovery SOURCE_RETRIEVAL_FAILED E=31 X=[32,33] adopted_skipped=[30], once-guard no dup); A/B byte-identical
S1/S7/S8 + legend-once + 2f no-FAILED-on-clean. Standing (do not decay): live bail-site -> helper state-plumbing
diff-verified, confirm at first A-series real induced failure.

ROADMAP now: IN-PLUGIN DIAG ARC has ONE step left ->
    DIAG.4  memory D-SUM-02 (stub in cnr3_memory_diagnostics.cpp) per cnr3_memory_diagnostics_spec_v2.md; salvage-
            assessment of CMS06-era memory-diag code IN-SCOPE (salvage/guide/rebuild per part to CMS07 standard);
            whole-framework observe-only capstone proof. ARC CLOSE.
  Then external ANALYSIS TRACK A1 (plan-trace tool, absorbs FI-11 offline; may run parallel to DIAG.4) / A2
  (fmParallel = C1-under-race gate) / A3 (real 576p50 via A1). FI-11 in-run counter deferred-but-expected.

---

(Prior v3.18 banner retained as history:)

*** v3.18 UPDATE — 2026-07-07 — DIAG.3c.1 COMMITTED (capture + emission refinement, one commit); plan-trace spec v2.3; DIAG.3c.2 next ***
Supersedes v3.17. DIAG.3c.1 (observe-only plan-trace capture + clean-end dump) is COMMITTED + PUSHED, marker
CMS07-DIAG.3c.1-plantrace-clean-end-capture. The emission-format refinement (spec v2.3) is folded into the same
commit. Proof gate CLOSED, verified cold: plugin macro-off byte-identical (fc /b) PASS S1/S7/S8 (R-PROCESS-19
exit gate); four-way selftest all-on/macro-off/restored 56/56 x3 + 55/1; content sanity S1 + S8 on the single-
line format (O/R pairing, branch-specific sources, branch-derived pinned, no unpinned=, post_compute_discarded,
BEGIN/END counts match, truncated=0). Emission is now: one physical ASCII line per record, fixed schema (empty=
[]), pad only enter_tick/exit_tick/seq/frame, ONE natural-order block (VIEW_* sub-gates removed), legend once
aligned (O-item/R-item), per-instance BEGIN/END schema=3c1v1; L -> post_compute_discarded; U/unpinned dropped.

WORK PLAN / ROADMAP (adopted this session):
  IN-PLUGIN DIAG ARC (contiguous, one discipline; arc CLOSES at DIAG.4):
    3c.1  COMMITTED (this update)
    3c.2  NEXT — dump-on-bail + Set 4 X/E + Set 5 fail_reason across the 65 cnr3_set_filter_error sites;
          invasive, R-PROCESS-21/25; the source-line SITE-TO-CATEGORY TABLE (65 sites -> 16 categories) is its
          foundation artifact and the first coder confirm deliverable.
    4     memory D-SUM-02 (salvage-assessment of the CMS06-era memory-diag code IN-SCOPE; salvage/guide/rebuild
          per part to the CMS07 standard). ARC CLOSE-OUT.
  ANALYSIS TRACK (external Python; no gate/macro-off/proof discipline; not DIAG-numbered):
    A1  plan-trace analysis tool (single step; scope must enumerate the question-set up front); absorbs the
        FI-11 OFFLINE ring-vs-D-SUM-12 correlation. Not bound by plugin sequencing (needs only 3c plan-trace
        output) -> may run parallel to DIAG.4. Diagnoses WHY (recalc/thrash) + WHAT-COULD-BE-DONE
        (tunable over-eviction vs inherent arrival-disorder).
    A2  fmParallel concurrency churn test (num_threads>1 vs -r 1 baseline) = the C1-OWNERSHIP-UNDER-RACE
        acceptance gate owed from the 3b commit (the D-SUM-07 released/discard arm is unexercised under -r 1).
    A3  real-footage 576p50 campaign, analysed via A1.
  Standing: FI-11 in-run ring<->recovery correlation counter DEFERRED-BUT-EXPECTED (keep visible).

---

(Prior v3.17 banner retained as history:)

*** v3.17 UPDATE — 2026-07-06 — DIAG.3b COMMITTED (proof gate closed); plan-trace spec v2.2 controlling; DIAG.3c.1 scoped, coder assessing ***
Supersedes v3.16. DIAG.3b (D-SUM-06/07/09/14) is COMMITTED + PUSHED. Proof gate CLOSED, verified cold: six-config
R-PROCESS-19 matrix GREEN (28 selftest runs, block-presence exact, four-way identical 56/56 + 55/1 forced-fail);
S-series real-run (-r 1, S1/S3/S7/S8) all three balances (source_frame_release_balance / temporary_output_balance /
lookup_ref_balance) == 0, failure-category fields (same_activation_request_violations / partial_acquire_failures /
promotion_mismatches) == 0, D-SUM-07 equation created==stored+released+transferred exact (S8: 932=653+0+279), prior
families unchanged, normal profile. Committed with a SANCTIONED marker-only build_config touch (CNR3_EDIT_VERSION ->
CMS07-DIAG.3b-lifecycle-return-scene); commit message notes C1-C4, C-ALIAS, D-2. C1-OWNERSHIP-UNDER-RACE is a
REQUIRED FUTURE GATE, not a note: under -r 1 no race, so temporary_outputs_released == 0 and
duplicate_computed_but_discarded == 0 on all four — the D-SUM-07 released/discard arm (where double-free / ambiguous
owner would surface, exactly what C1/C-ALIAS reason about) is NOT exercised; C1 must be re-accepted on the
fmParallel / -r >1 run (treat that run as the C1 acceptance gate). Create/store/transfer site-completeness IS proven
under churn. PLAN-TRACE SPEC advanced to v2.2 (CONTROLLING for DIAG.3c): v2 (full revision, 6 findings resolved +
8 locked decisions with reasoning bound inline) -> v2.1 (D-V2-1: bail-site total 65 CALL sites not 66 — AR raw grep
51 includes the DEFINITION at cnr3_arAllFramesReady.cpp:526; 14+50+1=65) -> v2.2 (view (a) sort key
(enter_tick ASC, action_seq ASC), phase dropped as sort term; enter_tick-outside-lock / action_seq-inside-lock
capture invariant with fmUnordered failure mode bound in). DIAG.3c.1 SCOPED (scope v2) and HANDED TO CODER (spec
v2.2 + scope v2 + post-3b src); coder assessing. 3c SPLIT delineated: 3c.1 = buffered plan/result capture +
clean-end dump, observe-only (R-PROCESS-19), NO bail touch [ACTIVE — coder assessing]; 3c.2 = dump-on-bail + E/X +
Set 5 failure-reason writes across the 65 bail sites (invasive, R-PROCESS-21/25, site-to-category TABLE from live
source as foundation) [DEFERRED — own scope]. DESIGNER RECOMMENDATION: commit 3c.1 standalone then 3c.2 next;
combine-vs-split is the coordinator's call at the 3c.1 boundary. THEN: DIAG.4 memory (D-SUM-02) -> post-arc (FI-11
offline correlation analysis; fmParallel concurrency churn test [also the C1 gate]; real-footage 576p50 campaign).
FI-11 in-run ring<->recovery correlation counter DEFERRED-BUT-EXPECTED. CMS DESIGN UNCHANGED (07.15).

---

(Prior v3.16 banner retained as history:)

*** v3.16 UPDATE — 2026-07-04 — DIAG.3b patch APPROVED (D-2 retro-sanctioned); four-way PASS; matrix + S-series owed ***
Supersedes v3.15. DIAG.3b confirm report accepted with decisions C1-C4 + C-ALIAS (C1: temp-output balance counts
only genuinely-produced frames, the production addFrameRef cache copy is OUTSIDE the balance; C-ALIAS: alias/null
copyFrame is never a creation, the alias free is a D-SUM-06 SOURCE release; C2: additive return-decision hooks, no
allows_return refactor; C3: source_copy_reset_frames scene-driven only; C4: near-grid = distance<=1 both sides, tiny-
profile caveat). PATCH APPROVED with finding D-2 (pure hoist without prior proposal — retro-sanctioned; the propose-
first sequence is now RATIFIED as R-PROCESS-25, with R-PROCESS-24 flush-per-line, in Document A v3.13). Four-way
all-on PASS. Synthetic-vs-real reading discipline: selftest reference emitters deliberately feed non-zero failure-
category fixtures; real-run S-series must show those fields == 0. OWED before DIAG.3b commit: six-config matrix +
S-series (source_frame_release_balance / temporary_output_balance / lookup_ref_balance all == 0 on S1/S3/S7/S8;
same_activation_request_violations == 0; partial_acquire_failures == 0; promotion_mismatches == 0; prior families
unchanged). THEN: PlanResult spec v2 (inputs: spec v1 + the coder cross-check + CNR3_Ring_and_PlanTrace_Design_
Rationale_and_Intent_v1.md) -> DIAG.3c scope (likely 3c.1/3c.2) -> DIAG.4 memory -> post-arc (FI-11 offline
correlation analysis, fmParallel concurrency churn test, real-footage 576p50 campaign).

---
(Prior v3.15 banner retained as history:)


*** v3.15 UPDATE — 2026-07-04 — DIAG.3a COMMITTED; DIAG.3 split into 3a/3b/3c; NEXT = DIAG.3b ***
Supersedes v3.14. DIAG.3a (D-SUM-03 recovery-search + D-SUM-12 recovery-plan/rate + D-SUM-13 recalculation) is
COMMITTED + PUSHED. Proofs green: all-on four-way 56/56; R-PROCESS-19 five-config matrix; S-series -r 1 with
recovery_plan_balance==0 UNDER CHURN (S8: 171 plans created and destroyed via the single teardown), saturation
false. The v2 patch fixed defect D-1 (the destroy observer was defined but not invoked; now called in
cnr3_discard_frame_data_with_cache before delete, covering all ~51 teardown paths). Recovery-rate baseline:
S1 0% / S3 27.5% / S7 0.375% / S8 21.4% — recovery churn is ARRIVAL-DISORDER-driven (shuffle), not seek-driven (jumps).

DIAG.3 is now an explicit THREE-part sub-sequence (by domain + risk gradient):
  DIAG.3a  D-SUM-03/12/13  recovery/recompute/churn trio                       [COMMITTED]
  DIAG.3b  D-SUM-06/07/09/14  source/temp-output/return-transfer + scene-reset [NEXT]
  DIAG.3c  plan/result PLAN-TRACE family                                       [AFTER 3b; SPEC-v2 FIRST]
DIAG.3c is the per-frame plan-vs-result trace (spec CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v1.md;
coder cross-check done, findings -> spec v2 before scoping). It is LAST because it is the only DIAG family that
touches the ~50+ setFilterError BAIL SITES (control-flow-adjacent, R-PROCESS-21) — likely sub-split 3c.1 (buffered
capture, observe-only) / 3c.2 (dump-on-bail, invasive). Then DIAG.4 (memory D-SUM-02). CMS DESIGN UNCHANGED (07.15).

FI-11 (RING <-> RECOVERY CORRELATION) — DO NOT LOSE (coordinator intuits it WILL be needed): D-SUM-10's evicted-frame
ring (DIAG.2a) + D-SUM-12's recovered targets (DIAG.3a) together answer "how much recovery churn is evict-then-
rebuild of a just-evicted region." OFFLINE correlation is available NOW (read the two dumps from one run; use during
the FI-11 analysis, post-arc / pre-real-footage). The IN-RUN correlation counter is DEFERRED/CONDITIONAL (needs a
getFrame-side cache-ring read that breaks the family boundary; build only if offline proves insufficient or the
signal is wanted live). Keep visible; do not let it decay to a forgotten FI line.

---
(Prior v3.14 banner retained as history:)


*** v3.14 UPDATE — 2026-07-04 — DIAG.2b COMMITTED; NEXT = DIAG.3a (scoped) ***
Supersedes v3.13. DIAG.2b (D-SUM-04 ownership-balance + D-SUM-05 cache-integrity + D-SUM-08 store/duplicate) is
COMMITTED + PUSHED, all three proof passes green (all-on four-way 56/56; R-PROCESS-19 five-config matrix; S-series
-r 1 balances zero under churn). Amendments A1-A5 folded. Cache-core diagnostic batch (DIAG.2a + DIAG.2b) COMPLETE.
NEXT: DIAG.3a = D-SUM-03 recovery-search + D-SUM-12 recovery-plan/rate + D-SUM-13 recalculation (the recovery/churn
trio; D-SUM-12 recovery-rate answers FI-11). Proposed batch split DIAG.3a (03/12/13) then DIAG.3b (06/07/09/14) —
coordinator decision pending. Then DIAG.4 (memory D-SUM-02). Plan/result plan-trace family cross-check runs parallel
to DIAG.3a (review only). CMS DESIGN UNCHANGED (07.15).

---
(Prior v3.13 banner retained as history:)
# Document B — CNR3 Work Plan and Current Build State (CMS07.15, RESUME)

*** v3.13 UPDATE — 2026-07-04 — DIAGNOSTICS ARC ACTIVE; committed through DIAG.2a; NEXT = DIAG.2b (in flight)
    (supersedes the v3.12 banner below; advances build state from "marshalling arc complete, NEXT=Lever-B-or-diagnostics"
     to "diagnostics arc ACTIVE: DIAG.1 + skip-pass + DIAG.2a committed/pushed; DIAG.2b scoped v2 and in flight with the coder") ***
This is the newest current-state record. The repository is the authority — confirm CNR3_EDIT_VERSION and the
selftest count from committed source before acting.

DIAGNOSTICS ARC (D-SUM telemetry) is underway. COMMITTED + PUSHED: (1) selftest skip-pass fix (default KDT-off config
now honest 56/56); (2) DIAG.1 = D-SUM framework + D-SUM-01 request-order + R-PROCESS-19 observe-only proof; (3) DIAG.2a =
D-SUM-11 hot-zone writer + D-SUM-10 prune/eviction/re-churn (ring, gap-histo, top-thrash, bounded dumps), gate-matrix and
S-series proven. DIAG.2a finding banked as FI-11: re-churn hooks the predecessor-lookup path, but the costly evict-then-
rebuild churn flows through the recovery/anchor path (D-SUM-12's job, DIAG.3). ACTIVE: DIAG.2b (D-SUM-04 ownership-balance +
D-SUM-05 cache-integrity + D-SUM-08 store/duplicate), v2 scope issued (D-SUM-04 re-sited to two provable balances after the
coder's inventory review; D-SUM-05 -> central cache_state_invariants_hold_locked(); D-SUM-08 -> wrapper summary, AS2
promotions only). Deferrals recorded as FI-11/12/13. NEXT AFTER DIAG.2b: DIAG.3 (getFrame/recovery incl. D-SUM-12 recovery-
rate + the plan/result plan-trace family), then DIAG.4 (memory D-SUM-02). CMS DESIGN UNCHANGED (07.15).

---
(Prior v3.12 banner retained as history:)
# Document B — CNR3 Work Plan and Current Build State (CMS07.15, RESUME)


*** v3.12 UPDATE — 2026-07-02 — MARSHALLING-OPTIMISATION ARC SUBSTANTIALLY COMPLETE (~-80%); NEXT = LEVER-B-OR-DIAGNOSTICS (coordinator call)
    (supersedes the v3.11 banner below; advances build state from "tiny-scaffold committed, 56/56, NEXT=diagnostics" to
     "marshalling arc complete at ~-80%, 56/56, NEXT = Lever B pooling OR the diagnostics arc") ***
This is the newest current-state record. The repository is the authority — confirm CNR3_EDIT_VERSION and the
selftest count from committed source before acting.

Build state ADVANCED off the tiny-scaffold seam by a full MARSHALLING-OPTIMISATION ARC (FI-10 acted on). This was an
implementation-only performance arc: CMS DESIGN UNCHANGED (still 07.15), every lever value-identical (56/56 four-way,
P-series preserved), each separately profiled on the YUV420P8 production clip. Twelve committed levers reduced per-frame
native<->scalar marshalling to ~1/5 of its original cost:
  AVX2 (x64-only, /arch:AVX2, neutral) | 0A staged native luma passthrough -28% | 0B staging cleanup flat |
  3a.1 typed native->scalar unpack -37% | 3b.1 inlined downsample flat | 3a.2 hoist source range-check (16-bit
  vectorised C5001, flat-on-8bit) | A-lite 8-bit unpack row-pointer/restrict -7.4% | C1 direct native-luma downsample
  bridge -24.5% (buffer elimination, biggest single win) | Repack row-memcpy commit -4% | F/3c blend inline+hoist -15.7%
  | Staging scalar->native inline -17.5% | E scene-change local-accumulator -4%.
  CUMULATIVE ~-80% (93,914 -> ~18,660 samples, 3500f -r 1).
Key artefacts: the VALIDATION POLICY (defend-at-source Tier-1 / trust-downstream Tier-2 / bounded-by-construction
Tier-3 / final-clamp-always) was recorded and APPLIED (F/3c removals, Staging Tier-1 reproduction, C1 pre-pass). The
blend arithmetic was CROSS-VERIFIED by independent designer+coder derivations (product weight, convex combination,
int64 throughout, shift1 round-half-up, weight->previous-filtered directionality) — this caught two external-suggestion
landmines: a VPAVGB two-level average (measured +0.375-code recursion-COMPOUNDING bias) and a 32-bit blend accumulator
(overflows at 16-bit; int64 required). Both REJECTED.
Two candidates INVESTIGATED and DECLINED: Tier-2 chroma-unpack fusion (Path C — the ~1,874 chroma buffer is
LOAD-BEARING for scene-change detection + reset, not a free-standing materialisation like C1's luma bridge; fusing
either forks the code on scene_config or needs a full native scene/reset rewrite — disproportionate) and Lever D
exact-SIMD downsample (PATH-B-only — the hot native loop's asymmetric x1 clamp blocks auto-vectorisation; the scalar
function is bypassed in production).

Committed:     CNR3-OPT-LeverE-scenechange-local-accumulator  (marshalling arc COMPLETE, on top of CMS07-DIAG-tinycache-scaffold)
Selftests:     56/56 PASS (forced-fail 55/56 exit 1; verbose 56/56)  [unchanged by the arc — all levers value-identical]
Build:         x64-ONLY, /arch:AVX2 HARD REQUIREMENT (both projects). NOTE: diagnostic /Qvec-report:2 flags may be
               sitting DIRTY in vs/cnr3/*.vcxproj working tree (carried deliberately) — revert before committing
               project files: git checkout -- vs/cnr3/cnr3.vcxproj vs/cnr3/cnr3_cache_core_selftest.vcxproj
Controlling:   CMS07.15 (UNCHANGED — the arc is implementation-only) / companion FI v7.16 (FI-10 arc-complete) /
               Production Spec v2.15 / Document A v3.11 / this Document B v3.12 / Role Handover v1.16 /
               Reviewer Intro v3.9 / Coder Restart Intro v6.7 / DELTA v4.23 / PixelPath Map v0.4
NEXT (coordinator call): EITHER (a) Lever B — allocation pooling of the per-frame scalar buffers (~587-sample
               `cnr3_allocate_scalar_plane_storage` leaf, the only remaining marshalling headroom). NOT a hot-loop
               change but a buffer-LIFETIME change → needs an fmUnordered thread-safety PROOF (one activation per
               instance may make an instance pool safe, but must be PROVEN, not assumed; and must not break if
               fmParallel ever becomes reachable). OR (b) declare the arc DONE at ~-80% and pivot to the DIAGNOSTICS
               arc (the tiny-scaffold's original purpose): Claude-owed 2-line-per-family D-SUM menu, then DIAG.1, plus
               the owed items (end-of-run integrity report, abort_on_error param, warn-vs-hard-fail severity, selftest
               skip-pass fix).
First actions: (1) confirm CNR3_EDIT_VERSION and 56/56 from the repo; (2) confirm the marshalling arc committed through
               Lever E; (3) take the coordinator's Lever-B-vs-diagnostics decision.

*** end v3.12 UPDATE ***



*** v3.11 UPDATE — 2026-07-01 — TINY-100 DIAGNOSTIC CACHE SCAFFOLD COMMITTED (56/56); NEXT = DIAGNOSTICS ARC
    (supersedes the v3.10 banner below; advances build state from "W.3 done, 55/55" to "scaffold done, 56/56") ***
This is the newest current-state record. The repository is the authority — confirm CNR3_EDIT_VERSION and the
selftest count from committed source before acting.

Build state ADVANCED on top of W.3: the TINY-100 diagnostic-cache scaffold is committed. A compile-time toggle
CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY (build_config.h, shipped OFF) selects a pre-computed small-but-safe cache
profile so capacity + checkpoint eviction fire on a ~200-frame live run instead of ~1300 — the short-run FIXTURE
the diagnostics arc will read. It changes NO CMS design (production constants unchanged, behind a compile guard;
the tiny profile re-proves the same static_assert safety chain at compile time). A profile-agnostic
protection-under-eviction selftest was added (proves pinned + hot-zone frames survive a real prune in BOTH
profiles), moving the count 55 -> 56. A CNR3_CACHE_PROFILE_NAME marker was added (selftest heading + live KDT
profile=%s token, dev-trace-only). Proof: toggle-OFF four-way 56/56 (forced-fail 55/56 exit 1); toggle-ON tiny
selftest exit 0 with 13 visible skips + tiny protection test passing; designer live harness
test_TINY_live_eviction_proof PASS (profile=tiny-100 + cap_trigger + ckpt_trigger with detached>0).

Also banked this pass (INVESTIGATION ONLY, not actioned): a VS2026 profile of the NORMAL build on a sequential
real-footage encode measured native<->scalar plane MARSHALLING at ~50% of per-frame cost (denoise math <10%,
cache manager <3%) — recorded as FI-10 in Future Investigations v7.15; a typed-row-pointer pixel-path rewrite is
the candidate lever but a SEPARATE future R-PROCESS-21 arc. The earlier ~3 fps concern was the tiny-diag pruning
cadence, NOT the cache architecture (normal build ~46 fps sequential).

Committed:     CMS07-DIAG-tinycache-scaffold  (on top of CMS07-W.3-combined-live-store-prune-helper)
Selftests:     56/56 PASS (forced-fail 55/56 exit 1; verbose 56/56)  [was 55/55; +1 = protection-under-eviction test]
Controlling:   CMS07.15 (UNCHANGED — scaffold is not a design change) / companion FI v7.15 (FI-10 added) /
               Production Spec v2.15 / Document A v3.11 / this Document B v3.11 / Role Handover v1.15 /
               Reviewer Intro v3.8 / Coder Restart Intro v6.6 / DELTA v4.17
NEXT ARC:      DIAGNOSTICS (unchanged) — the tiny scaffold is its enabling fixture. First step: the Claude-owed
               2-line-per-family D-SUM menu so the coordinator picks the core subset (D-SUM-10/11/12 = prune /
               hot-zone / recovery policy-health), then DIAG.1 (framework + one reference family + macro-off proof).
First actions: (1) confirm CNR3_EDIT_VERSION and 56/56 from the repo; (2) start the D-SUM family menu.

*** end v3.11 UPDATE ***


*** v3.10 UPDATE — 2026-06-30 — LIVE CACHE-PRESSURE WIRING ARC COMPLETE (W.1+W.2+W.3); NEXT = DIAGNOSTICS ARC
    (supersedes the v3.9 banner below; advances build state from "W.1 next, 53/53" to "W.3 done, 55/55") ***
This is the newest current-state record. The repository is the authority — confirm CNR3_EDIT_VERSION and the
selftest count from committed source before acting.

Build state ADVANCED: the live cache-pressure wiring arc is COMPLETE. W.1 (§7.4 checkpoint-retention trigger),
W.2 (§7.6 arInitial hot-zone observation), and W.3 (§7.5 combined live store-and-prune helper) are all done and
committed; live eviction + retirement are wired. W.3 proof: four-way 55/55 (forced-fail 54/55 exit 1; verbose
55/55) + the designer eviction-proof live A/B harness PASS (byte-identical output under live eviction; capacity
AND checkpoint triggers proven to fire and detach victims; recovery + AS2 floor/hole exercised live). The AS2
production-vs-pinned store-status RETURN contract surfaced by W.3 is recorded in CMS07.15 §7.5 (additive).

Committed:     CMS07-W.3-combined-live-store-prune-helper
Selftests:     55/55 PASS (forced-fail 54/55 exit 1; verbose 55/55)
Controlling:   CMS07.15 / companion v7.14 / Production Spec v2.15 / Document A v3.11 / this Document B v3.10 /
               Role Handover v1.15 / Reviewer Intro v3.8 / Coder Restart Intro v6.6 / DELTA v4.16
NEXT ARC:      DIAGNOSTICS (the 14-family D-SUM observe-only telemetry; condensed 4-phase plan v1.4 DIAG.1-.4),
               sequenced BEFORE the real-footage 576p50 campaign (coordinator decision 2026-06-30: the W.3 live
               harness proves eviction SAFE but is blind to eviction-POLICY health — over-prune / thrash /
               hot-zone efficacy / recovery churn; D-SUM-10/11/12 are the instruments, so the footage campaign
               runs instrumented). First step: the Claude-owed 2-line-per-family D-SUM menu so the coordinator
               picks the core subset, then DIAG.1 (framework + one reference family + observe-only macro-off proof).
First actions: (1) confirm CNR3_EDIT_VERSION = CMS07-W.3-... and 55/55 from the repo;
               (2) reconcile the diagnostics provenance/plan headers to post-W.3 (done in this pass), then start the menu.

*** end v3.10 UPDATE ***

*** v3.9 UPDATE — 2026-06-29 — STEP 0 CLOSED; CMS07.14 banked; NEXT = W.1 (live cache-pressure wiring impl)
    (supersedes the v3.8 banner below; same build state, advanced CMS pointer + next-action) ***
This is the newest current-state record. The repository is the authority — confirm CNR3_EDIT_VERSION and the
selftest count from committed source before acting.

Build state UNCHANGED (P.11C arc CLOSED .1-.5, committed CMS07-P.11C.5, 53/53; live getFrame feature-complete
with scene handling). Since the v3.8 banner: the Step 0 joint review (designer+coder+coordinator) CLOSED with
all 13 findings agreed/resolved. Its decisions are now in the design authority as CMS07.14 (ADDITIVE over
07.13): §7.4 independent checkpoint-retention trigger (enforces the §6.3 MAX_RETAIN bound — the capacity
trigger alone did not, on cut-heavy footage), §7.5 the combined locked store-and-prune wiring contract
(6-step order), §7.6 the arInitial hot-zone observation prerequisite for unpinned produced output. No existing
rule/constant/AS-scope changed. Provenance: CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md.

```text
Controlling:    CMS07.14 / companion v7.13.4 / Production Spec v2.14 / Document A v3.9 / this Document B v3.9 /
                Role Handover v1.12 / Reviewer Intro v3.5 / Coder Restart Intro v6.4 / DELTA v4.14 /
                Diagnostics Plan v1.3.
Next phase:     LIVE CACHE-PRESSURE WIRING implementation (Step 0 contract settled). Order:
                  W.1 — §7.4 independent checkpoint-retention trigger as a PROVEN cache-core primitive +
                        selftest (the one genuinely new piece of logic; read-first/propose/review/prove like a
                        K/D phase; selftest count 53 -> 54). Designer scope:
                        CNR3_W1_Coder_Scope_checkpoint_retention_trigger_v1_0.md. Coder read-first + 3a proposal
                        already produced and ACCEPTED (incl. the non-checkpoint-disable guard and the K-bound
                        convergence clarification: trims while count>MAX, not guaranteed reach-MIN).
                  W.2 — hot-zone observation wiring at arInitial (§7.6; lower risk; prerequisite).
                  W.3 — the combined live store-and-prune helper (§7.5 six-step order), wiring §7.2 + §7.4
                        triggers into the live path, temporary KDT for the live proof (SR-C-06).
                Then real-footage validation -> diagnostics (condensed 4-phase) -> fmParallel.
First actions:  (1) confirm CNR3_EDIT_VERSION = CMS07-P.11C.5... and 53/53 from the repo;
                (2) resume W.1 at the patch step (read-first + proposal already accepted).
```
*** end v3.9 UPDATE ***


*** v3.8 UPDATE — 2026-06-28 — HANDOVER-SAFETY MERGE; NEXT = STEP 0 CMS sensibility/gap review
    (supersedes the v3.6 block below; same build state, advanced pointers + Step 0 next-action) ***
This is the newest current-state record. The repository is the authority — confirm CNR3_EDIT_VERSION and
the selftest count from committed source before acting.

Build state is UNCHANGED from the v3.6 block below (P.11C arc CLOSED .1-.5, committed CMS07-P.11C.5, 53/53;
live getFrame feature-complete with scene handling). This v3.8 update is a HANDOVER-SAFETY pass that (a)
advances all doc-set version pointers to the merged pack (Spec v2.14 / Document A v3.8 / Role Handover v1.12
/ Reviewer Intro v3.5 / Coder Restart Intro v6.4 / DELTA v4.14 / Future Investigations v7.13.3 / Diagnostics
Plan v1.3 / CMS07.13 with a front-matter currency fix), correcting stale concrete pointer lines flagged in
the coder handover review; and (b) records the banked **STEP 0** next-action:

```text
NEXT (Step 0): joint CMS SENSIBILITY / GAP REVIEW for hot-zone + prune live wiring, BEFORE any wiring patch.
               Do NOT assume the CMS is reliable as-is merely because the prune/hot-zone componentry is
               proven (built + selftest-proven but ZERO live callers). Review whether the CMS is still
               sensible and complete against the post-P.11C.5 implementation state; the live prune-TRIGGER
               contract (when the store path fires prune; safety vs the active pin_list and the
               arInitial->arAllFramesReady gap) is the load-bearing PART of that review. A CMS clarification
               or version bump MAY be a legitimate output of Step 0.
               Provisional sequence (subject to review): Step 0 -> hot-zone observation/retirement wiring ->
               live prune-trigger wiring -> real-clip validation (+ diagnostics/telemetry placement in the
               approved order) -> fmParallel. The prune PASS is proven; the live TRIGGER + its lifecycle
               safety are the wiring work ("proven componentry != proven wiring").
```
The full ground-truth audit of the live-wiring gap and the CMS-area-by-area reliability assessment are in
the v3.6 block below (retained) and in the DELTA owed-items (FI-09). The deferred Document B deep tidy
remains owed at the real-footage seam.
*** end v3.8 UPDATE ***


*** v3.6 UPDATE — 2026-06-28 — P.11C SCENE-CHANGE ARC CLOSED (.1-.5); NEXT = live cache-pressure WIRING
    (supersedes the D.5-era v3.5.1 block and all earlier update blocks below) ***
This is the newest current-state record. The repository is the authority — confirm
CNR3_EDIT_VERSION and the selftest count from committed source before acting.

Since the D.5 update, the P.11C scene-change arc was scoped, built, proven, and committed in full
(all committed/pushed, both configs). P.11C IMPLEMENTED the scene-change design the CMS already
specified (§6.3/§6.4/§6.5/detection-during-compute) — a STATE advance, NOT a CMS rule change; the
controlling CMS is UNCHANGED at CMS07.13:

- **P.11C.1** scene-change uniform-wiring layout (plugin-only): the branch-a/c/d wiring skeleton/scope.
- **P.11C.2** live scene-change config + scdthr->threshold helper + central store-request/checkpoint
  routing (plugin-only). Threshold uses the verified vsCnr2 full-frame shape with proportional
  round-to-nearest depth scaling; default scdthr=10.0.
- **P.11C.3** branch-c (predecessor-present) scene detection ENABLED (plugin-only; .vpy-proven): live
  _with_scene_change call + reset-on-cut chroma + checkpoint promotion; KDT scene fields.
- **P.11C.4** branch-d (RECOVERY) scene detection ENABLED (plugin-only; .vpy-proven): per-hole + target
  _with_scene_change; KDT honesty (computed holes report actual scene fields; early-skip adopted holes
  report scene_change_not_run; target reports status-only). Atomicity inherited (store+flag+promote+pin
  under one cache_mutex_ lock).
- **P.11C.5** scene-cut checkpoint found as recovery anchor (selftest; count 52 -> 53): proves the CACHE
  HALF — an unpinned non-grid frame stored as the sole checkpoint survives a real bounded prune by
  CHECKPOINT CLASS while an ordinary non-checkpoint is evicted (total_pin_count=0 throughout -> survival
  by class, not pin), then read-only bounded recovery anchors exactly on that checkpoint with
  anchor_is_checkpoint=true and holes={the absent successor}. Composes with P.11C.4 (live detection feeds
  the checkpoint store route). KDT-only addition: anchor_is_checkpoint printed in the live exact-anchor
  recovery trace. >>> P.11C SCENE-CHANGE ARC CLOSED.

No CMS revision since D.5: the CMS carries only an additive implementation-state note (P.11C now
implemented+proven); it remains CMS07.13 with no rule/constant/AS-scope/section change.

```text
Code state:     Committed through CMS07-P.11C.5-scene-cut-checkpoint-recovery-anchor-proof; 53/53 selftests
                (forced-fail 52/53 exit 1; verbose 53/53). The live getFrame dispatch is FEATURE-COMPLETE
                across all four branches WITH SCENE HANDLING: cache-hit (b), fresh-start (a),
                predecessor-present (c), recovery (d). Both the branch-(d) recovery arc (D.1-D.5) and the
                P.11C scene-change arc (.1-.5) are COMPLETE. Only deferred confidence is real concurrent
                (fmParallel) scheduling.
Controlling:    CMS07.14 (additive §7.4-§7.6 live-wiring contract over 07.13; no existing rule changed) /
                companion v7.13.4 / Production Spec v2.14 / Document A v3.9 / diagnostics v1.5.
Next phase:     LIVE CACHE-PRESSURE WIRING — the last missing FUNCTIONALITY. Audit against the committed
                P.11C.5 source (this session, ground truth) found the prune + hot-zone LOGIC fully built
                and selftest-proven but with ZERO live callers: execute_bounded_prune_pass=0,
                record_hot_zone_observation=0, retire/merge/trigger-decision=0. store_owned_frame_locked
                APPENDS without consulting the prune trigger (grows unbounded; only returns
                capacity_exceeded at the vector hard max). So the live cache currently NEVER prunes and
                NEVER records hot-zone observations. Everything else is wired or test/diag-only by design
                (lookup/store/recovery/pin-discharge WIRED; observers like total_pin_count diag-only;
                non-pinning plan_bounded_recovery_search is the selftest variant). NOTHING ELSE functional
                is missing. PLAN (coordinator option B): wire hot-zone observation (CMS §5.7: at arInitial)
                THEN pruning, THEN real-clip runs. SPEC RELIABILITY: policy is reliable as-is (CMS §5.7
                hot-zone-at-arInitial, §5.3-5.6 lifecycle, §6.3 retention, §5.5 safety) — but the live
                prune-TRIGGER contract (exactly WHEN the live store path invokes the prune pass, and how
                that is safe against the active pin_list and the arInitial->arAllFramesReady gap) is NOT
                pinned down at wiring level and needs a focused designer+coder review BEFORE coding.
                Scope it single-activation-now; concurrent prune is part of the fmParallel arc. AFTER the
                wiring: first real-footage validation -> diagnostics (condensed 4-phase) -> fmParallel.
                ("Proven componentry != proven wiring": the prune PASS is proven; the live TRIGGER and its
                lifecycle safety are the actual wiring work — same component-vs-wiring distinction as the
                K-phases.)
First actions:  (1) confirm CNR3_EDIT_VERSION = CMS07-P.11C.5... and 53/53 from the repo; (2) designer+coder
                review of the live prune-trigger contract (CMS-clarification + approach analysis, like the
                P.11C.5 read-first); (3) THEN scope hot-zone observation wiring (lower risk; §5.7 specifies
                the point), then prune wiring.
Doc set:        Production Spec v2.14, Document A v3.8, this Document B v3.8, CMS v7.13 (additive note +
                front-matter currency fix), Role Handover v1.12, Reviewer Intro v3.5, Coder Restart Intro
                v6.4, Future Investigations v7.13.3, Diagnostics Condensed Plan v1.3. The live per-phase ledger is the current DELTA
                (CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.md) — read it for full per-phase detail
                and the owed-items (incl. the deferred Document B deep tidy at the real-footage seam).
```
*** end v3.6 UPDATE ***


*** v3.5.1 UPDATE — 2026-06-27 — COMMITTED THROUGH D.5; branch-(d) RECOVERY ARC COMPLETE
    (supersedes the K.1F-era v3.5 block and all earlier update blocks below) ***
This is the newest current-state record. The repository is the authority — confirm
CNR3_EDIT_VERSION and the selftest count from committed source before acting.

Since the K.1F update, the branch-(d) recovery arc was scoped, built, proven, and committed in full
(all committed/pushed, both configs):

- **D.1** exact-anchor SINGLE-hole recovery (plugin-only). Live: recovered output computed from the
  pinned in-window anchor; pin balance 0.
- **D.2** exact-anchor MULTI-hole (k>=2) recovery + bounded-window refusal (plugin-only). NOTE: D.2's
  RUN C bounded-window refusal is now TRANSITIONAL — superseded by D.3 floor-fresh-start for reachable
  in-range N (genuine refusal narrows to structural/impossible cases).
- **D.3** floor-fresh-start recovery (plugin-only): when the bounded descending search finds no
  in-window anchor, recovery fresh-starts the floor max(0,N-B) (copyFrame(source[floor]), chroma
  unchanged, no predecessor) then walks holes ascending to N. CONVERTED the D.2-era no-in-window-anchor
  refusal into floor-fresh-start. Established the materialized-floor-is-the-foundation invariant
  (recorded in CMS §9.5 / CMS07.13, the D.3 patch comment, and the DELTA).
- **D.4** pre-compute adopt-skip + first-in-best-dressed PRIMITIVES (selftest-only): two cache-core
  cases proving the context-free adopt-skip primitive (present frame -> lookup_frame_and_record_pin
  adopts/pins/skips-compute, AS4 balances) and the post-compute duplicate loser (winner kept by
  identity, loser released once). Selftest 49 -> 51. Real-race firing deferred to fmParallel.
- **D.5** recovery-pin-survives-real-prune-pass (selftest-only): one paired-control case proving a
  recovery foundation pinned via the real recovery helper survives a real bounded AS5 prune pass that
  would otherwise have detached it (control detaches 165; protected pins 165 -> 164 detached instead;
  survivor usable by identity; AS4 balances). Selftest 51 -> 52. Real prune-during-recovery scheduling
  deferred to fmParallel.

CMS revisions since K.1F: **CMS07.11** added §0A (the Design Alignment and Escalation Charter, mirrored
in Production Spec §3A.5.0 and Role Handover §D0); **CMS07.12** clarified the bounded-search report
semantics in §9.5; **CMS07.13** made the materialized-floor-is-the-foundation invariant explicit in §9.5.
None of CMS07.11/.12/.13 added a design rule, AS scope, or section-number change (all clarification/
governance). The corrected CMS07.10 noted in the K.1F block was committed.

```text
Code state:     Committed through CMS07-D.5; 52/52 selftests (forced-fail 51/52 exit 1; verbose 52/52).
                The live getFrame dispatch is FEATURE-COMPLETE across all four branches: cache-hit (b),
                fresh-start (a), predecessor-present (c), recovery (d). The branch-(d) recovery arc is
                COMPLETE (D.1-D.5). Only deferred confidence is real concurrent (fmParallel) scheduling.
Controlling:    CMS07.13 / companion v7.13 / Production Spec v2.11 / Document A v3.5 / diagnostics v1.5.
Next phase:     P.11C scene-change uniform wiring across branch-a/c/d (NOT a recovery phase). Scene-change
                is currently deferred uniformly (scene_change_deferred=1) on synthetic cut-free test
                footage; P.11C wires it in across all branches before the first REAL-footage test
                (detected cuts promote to checkpoints = recovery anchors, so P.11C interlocks with the
                recovery machinery). After P.11C: first real-footage validation.
First actions:  (1) confirm CNR3_EDIT_VERSION = CMS07-D.5... and 52/52 from the repo; (2) scope P.11C
                (designer-owed) — it touches the pixel pipeline AND the checkpoint/recovery interaction.
Doc set:        Production Spec v2.11, Document A v3.5, this Document B v3.5.1, CMS v7.13. The live
                per-phase ledger is the current DELTA (being slimmed to a phase-index + active-phase
                detail). Recovery-arc per-phase detail lives in the DELTA history and Document A's
                build-state note.
```
*** end v3.5.1 UPDATE ***


**Version:** Document B v3.9 (Step 0 CLOSED; controlling CMS advanced to 07.14; next = W.1 wiring impl; supersedes v3.8 which was the handover-safety merge after the coder review; build state unchanged from v3.6 = P.11C.5 closed; controlling CMS07.13 UNCHANGED / Spec v2.14 / Document A v3.8; next = STEP 0 CMS sensibility/gap review before live cache-pressure wiring). The v3.8 UPDATE banner and the v3.6 UPDATE block at the top are the prevailing current-state record; all blocks below are retained history. The v3.6 UPDATE block at the very top is the prevailing current-state record; all update blocks below it (v3.5.1 D.5-era and earlier) are retained as history of record (superseded). Per Production Spec §4, Document B is the volatile current-state document, re-issued each session against the prevailing phase; the durable scaffolding (working method, invariant disciplines, do-not-implement list, salvage inventory) is carried forward unchanged. v3.5 advanced K.1E.3 -> K.1F; v3.4 was a version bump over the v3.2.9.x generation.  
v3.2.9.2 (RESUME-state work plan; focused status update. The version label is kept at
the "3.2" generation to stay aligned with Document A v3.2; the `.9` patch level marks this update.
Records the **keystone now under way and committed through K.1D** — K.1A (request-plan structures +
temporary KDT dev-trace), K.1B (direct cached-output-return ownership, synthetic), K.1C (live
getFrame passthrough scaffold), and **K.1D (the first REAL output frame: copyFrame fresh-start
store/return)** — on top of the caller-supplied pixel path (P.10A–P.11C), the scalar→native bridge
(P.7A–P.9A), the scalar pixel pipeline (P.1A–P.6A), and the C.14A cache-core milestone. **Selftest
count is now 47/47** (forced-fail 46/47, exit 1; verbose 47/47). The next phase is **K.1E branch-(c)**
(live predecessor-present frame-1 compute), in flight at acknowledgement-accepted / pre-patch. The
full delta of the keystone work is recorded in the companion document
`CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md`; the v3.2.9.2 status note below summarises
it in Document B's format. Earlier sections carry forward except the version pointers below.)
**Date:** 2026-06-23
**Role:** Current-state / work-plan document. It states the controlling authority, the **current
build state**, the working method, the immediate next phase, the proof obligations, and what must
not be implemented yet.
**Generation source:** repository git history (authoritative build state) + Production Spec v2.6 §3A
+ CMS07.7.
**Precedence:** volatile. If this document ever conflicts with the latest prevailing CMS, the CMS
wins. If it conflicts with Production Spec §3A on register-owned rules, §3A wins.

**v3.2.9.1 status note (the KEYSTONE is under way — committed through K.1D; K.1E branch-(c) in flight):**
The controlling CMS is now **CMS07.7** (`cnr3_cache_manager_design_v7_7.1.md`); the Production Spec is
**v2.6**; the diagnostics spec is now **v1.5**; the non-normative companion is now **v7.8**
(`CNR3_CMS_Future_Investigations_and_Open_Questions_v7_8.md`, which contains **FI-04**). On top of the
caller-supplied pixel path (P.10A–P.11C) and the C.14A cache-core milestone, the **cache↔pixel /
getFrame keystone is now under way** — the hard designer gate where the proven cache core meets the
proven pixel chain inside VS getFrame scheduling. **Four keystone phases are committed and pushed;
selftest count is now 47/47** (forced-fail 46/47, exit 1; verbose 47/47, all priors present). The
keystone is being decomposed K.1A–K.1G.

- **K.1A — keystone request-plan structures + temporary KDT dev-trace** (count →46). Request-plan
  branch enum/struct; recovery request representation is a holes-list / source-set (never a blanket
  span); the hard-status branch is a **carrier** for existing C.13B guard results, not a new validator;
  `[KDT]`/`[KDT-SUMMARY]` formatting is driven by the plan structure. Guarded by `CNR3_KEYSTONE_DEV_TRACE`
  in `cnr3_build_config.h`. No getFrame wiring, no source lifecycle, no pixel-path call, no cache-semantic
  change, no VS header edit.
- **K.1B — direct cached-output-return ownership proof** (count →47), **synthetic-first**, using the
  **real** `Cnr3OwnedFrameRef` and **real** cache lookup/addref operations (counters OBSERVE real ops).
  Three cases: success 1/0/1 (acquired/released/transferred); cleanup-before-transfer 1/1/0; no-acquire
  miss 0/0/0. The synthetic sink models the getFrame-return boundary. The **real `VSFrame`
  return-to-VapourSynth was explicitly OWED** here — and is now expected to retire INSIDE branch-(c)
  work (the internal cached-predecessor hit), not via a separate getFrame-re-entry proof.
- **K.1C — live getFrame passthrough scaffold** (plugin-only; count stays 47). First live getFrame step,
  with **five R-ARCH-06 fences**: removable guard; a **distinct callback that gets replaced not extended**;
  the scaffold frame is **never cached / never a predecessor / never checkpointed** (structurally prevents
  contamination); a `[KDT] SCAFFOLD_NOT_FILTERED` marker; a return-point comment. **[KDT] is emitted ONLY
  inside getFrame, never at plugin load/registration.** A/B byte-compare harness green. Files:
  `src/vapoursynth-Cnr3.cpp` + `src/cnr3_build_config.h` only.
- **K.1D — live frame-0 fresh-start store/return** (plugin-only; count stays 47). **The first REAL CNR3
  output frame**: output[0] created, stored as cache-authoritative checkpoint, and returned through live
  getFrame; N>0 cleanly refused. Reached via `copyFrame(source, core)` (a bitwise, writable, caller-owned
  duplicate) because frame-0 fresh-start output[0] = source[0] byte-for-byte (no predecessor, no blend;
  luma always source-copy, chroma source-copy when no predecessor) — so **no proven code is touched**
  (zero contact with `cnr3_frame_processing.cpp` / P.11C). Verified against five review bars (ownership:
  `copyFrame` ref + `addFrameRef` ref = two owners each freed once, source freed-and-nulled after copy,
  post-store failure frees only the returned ref while the moved owned-ref handles the cache ref; defensive
  null/alias guards; proven code untouched; KDT `FRAME0-FRESH-START` / `REAL_OUTPUT_FRAME0`, N>0
  `NOT-YET-IMPLEMENTED branch=nonzero-before-predecessor-wiring`, guarded by
  `CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF`, stderr-only; N>0 gated before arInitial). Four-way clean
  (47/47); A/B harness green (frame-0 byte-identical; N>0 clean refusal leaves a header-only y4m, no FRAME
  marker).
*** v3.2.9.2 UPDATE — 2026-06-24 — KEYSTONE COMMITTED THROUGH K.1E.3 (supersedes the K.1D-era
status note above; confirm from the repository) ***

The keystone branch-(c) live path is now committed and pushed through K.1E.3, and the cache-core
selftest count is now 48/48 (forced-fail 47/48, exit 1; verbose 48/48). The live recursive chain
output[0] -> output[1] -> output[2] is runtime-proven in BOTH Debug and Release.

- K.1E.1 — frameData pin-gap synthetic proof (selftest count -> 48). Holder carries predecessor
  frame number + caller-owned pin-list only; no predecessor VSFrame ref crosses the
  arInitial/arAllFramesReady gap; normal and abandoned/free paths use one discharge-before-delete
  helper; an intervening prune during the gap leaves the pinned predecessor present and retrievable.
- K.1E.2 — live frame-1 predecessor-present compute (plugin-only; count stays 48). arInitial pins
  cached output[0]; arAllFramesReady acquires a short-lived compute ref via lookup_frame_and_add_ref(0),
  calls proven P.11B, production-stores output[1] (no consumer pin), releases the compute ref, and
  discharges the pin-list on every exit path. Gate edit n!=0 -> n>1; rpStrictSpatial -> rpGeneral.
  Golden: source[1]=128/224/32 -> output[1]=128/161/95 (three-way-distinct discriminator).
- K.1E.3 — recursive filtered-predecessor distinction at N=2 (plugin-only; count stays 48). Bounded
  generalisation: branch-(c) now covers N==1 and N==2 with predecessor = N-1; N>2 a clean marked
  refusal (after-frame2-before-recovery-wiring). Golden: source[2]=128/192/64 -> output[2]=128/163/93.

R-ARCH-06 — CLOSED (proven at N=2, K.1E.3): the filtered-output-not-source predecessor distinction is
now byte-observable and proven. At N==1, output[0]==source[0] (fresh start) made it invisible; at N==2,
cached output[1] (161/95) differs from source[1] (224/32), and output[2]=163/93 is reachable ONLY from
the cached filtered output[1] — a source[1]-substitution bug yields 222/34, a passthrough yields 192/64,
both byte-distinct on U and V. The prior N=1-limitation proof note is superseded by this closure.

CURRENT STATUS (supersedes the v3.2.9.1 summary in section 11; repository is authoritative):
    Code state:     Keystone committed through CMS07-K.1E.3; 48/48 selftests (forced-fail 47/48 exit 1;
                    verbose 48/48). Edit marker CMS07-K.1E.3-live-frame2-sequential-filtered-
                    predecessor-distinction-proof.
    Live proof:     Debug AND Release harness green — frame 0 fresh-start byte-identical; frame 1
                    golden 128/161/95; frame 2 golden 128/163/93; frame 3 clean N>2 refusal.
    Immediate task: Designer-led recovery (branch-(d)) scoping pass BEFORE any coder scope (see the
                    recovery design-drivers below and the AS4 wording gate).
    After that:     branch-(d) bounded recovery; multi-frame VS-LIFECYCLE-01; longer sequential run
                    (N>2); live scene-change/P.11C wiring; post-K.1G KDT cleanup; fmParallel.

RECOVERY-ONWARD DESIGN-DRIVERS (decided this session; shape the recovery phase — not yet owed work):
    - Sequencing: recovery (branch-(d)) BEFORE live hot-zone tracking; live hot-zone tracking after.
    - Recovery is built in LARGER steps than K.1E.x, but each step still ends in a byte-exact,
      harness-proven acceptance result. Proposed breakdown: single-hole recovery from a pruned
      predecessor -> multi-hole/branch-(d) bounded recovery -> recovery under live prune pressure.
    - Diagnostics: recovery-onward must include an OPERATIONAL fmParallel-observable trace tier,
      sufficient to watch TWO CNR3 instances under fmParallel (interlaced) during pre-release
      verification. Every line instance- AND thread/frame-attributed, atomically emitted, low-distortion
      (non-serializing), demuxable in two dimensions (instance x thread); cross-instance independence
      (pin-ledger conservation, prune isolation) must be assertable. The emission mechanism is itself a
      proof-worthy concurrent component.
    - Verification config: the real instance-config surface must expose TEST-TUNABLE hot-zone sizing and
      prune-trigger thresholds, so pre-release verification can deliberately lower them to FORCE
      prune/eviction/recovery paths on small clips — alongside production-like-threshold runs confirming
      those paths stay correctly DORMANT. (Prune hysteresis G.10A exists; the knobs are owed.)
*** end v3.2.9.2 UPDATE ***

*** v3.5 UPDATE — 2026-06-25 — COMMITTED THROUGH K.1F (supersedes the K.1E.3-era state above) ***
Since the K.1E.3 update, the following landed (all committed/pushed unless noted); the repository
is the authority — confirm CNR3_EDIT_VERSION and selftest count from committed source.

- **CMS07.9** (additive over 07.8): pre-compute adopt-and-skip made NORMATIVE in §9.2 recovery
  per-hole fill (§9.6.5); caught a real fmParallel assumption (AS3-"unreachable" was a plan-time
  claim presented as act-time; under fmParallel a hole already present at act-time IS reachable).
  Companion -> v7.9 (FI-04 resolved into §9.7.7; FI-05 two-instance resource model flagged as a
  likely genuine gap, NOT blocking branch-d; FI-06/07/08).
- **Recovery-Step-0** (AS4 single-lock batch discharge): public Cnr3CachePinList::discharge_all
  delegates to Cnr3OutputCacheCore::discharge_pin_list taking cache_mutex_ ONCE. Cache-core only;
  selftest count 48 -> 49 (case 7 = single-lock structural proof).
- **K.1F** (live direct cached-output return, branch-b): present-N cache hit pins output[N] at
  arInitial (gap protection), requests source[N] as an Option-C lifecycle TRIGGER, returns the
  cached frame at arAllFramesReady via lookup_frame_and_add_ref + the Step-0 batch discharge; the
  trigger source is retrieved and immediately freed (not consumed). Plugin-only; four-way unchanged
  49/49; live harness Debug+Release green (cache-hit 128/163/93 with branch=CACHE-HIT, core cache
  defeated via SetVideoCache(mode=0); repeated-frame-0 proves present-N dispatch precedes the n==0
  gate). frameData field renamed predecessor_pin_list -> pin_list (now shared).
- **CMS07.10** (CORRECTION to §9A.1.1 — R-LIFECYCLE, proven by K.1F): every CNR3 getFrame branch
  requests >=1 real source frame at arInitial and returns only at arAllFramesReady; zero-request
  arInitial->NULL is NOT guaranteed an arAllFramesReady callback under R76. Options A (arInitial
  return) and B (zero-request) rejected as unverified for a non-source filter under fmParallel.
  Companion -> v7.10. (A corrected CMS07.10 — four editorial/consistency fixes incl. §9.7.1
  branch-(b) wording aligned to R-LIFECYCLE — was staged at end of the producing chat; COMMIT IT
  FIRST next session before D.1.)

```text
Code state:     Committed through CMS07-K.1F; 49/49 selftests (forced-fail 48/49 exit 1;
                verbose 49/49). Live getFrame dispatch now handles ALL FOUR branches except
                recovery: present-N cache-hit (b), fresh-start (a), predecessor-present (c).
Controlling:    CMS07.10 / companion v7.10 / Production Spec v2.9 / diagnostics v1.5.
Next phase:     branch-(d) D.1 (exact-anchor single-hole recovery). The detailed current state,
                the settled Option-C/R-LIFECYCLE lifecycle finding, the K.1F harness lesson, and
                the full D.1 brief + branch-(d) arc D.1-D.5 are in
                CNR3_THIS_CHAT_DELTA_keystone_through_K1F_v4.md (the newest current-state record).
First actions:  (1) commit the corrected CMS07.10; (2) compute the D.1 golden chain + draft the
                D.1 coder scope (designer-owed, DELTA v4 §5).
```
*** end v3.5 UPDATE ***


**THE K.1D REORIENTATION (durable lesson — see DELTA §2).** The FIRST K.1D patch was **DROPPED** because
it silently rewrote the body of the proven, selftested P.11C function and introduced a second
source-to-output copy orchestration, with undisclosed scope broadening into `cnr3_frame_processing.cpp`.
The standard sharpened and now load-bearing: **proven, selftested code is never modified — behaviour OR
internals — without explicit visible planning and designer approval IN ADVANCE; a passing four-way after
swapping internals is NOT proof of equivalence; if reuse appears to require touching proven code, RAISE it
as a design question, do not route around it.** The patch was withdrawn to the proposal stage (not
patched-and-fixed); the copyFrame solution above was the reorientation outcome — smaller, safer, no
proven-code contact.

**THE IMMEDIATE NEXT PHASE — K.1E branch-(c)** (`CMS07-K.1E-live-predecessor-present-frame1-compute-proof`),
in flight at **acknowledgement-accepted / pre-patch**. N==1 after K.1D stored output[0]: at `arInitial`
acquire cached output[0] as predecessor (real lookup/addref, carried in frameData) and request source[1];
at `arAllFramesReady` retrieve source[1], compute output[1] via the **proven P.11B** composition,
**release** the predecessor after use, store output[1] per existing checkpoint policy, return output[1].
Ownership (OPPOSITE tail to K.1B): acquired=1, released=1, transferred=0, balance=0. **Dependency
declaration changes `rpStrictSpatial` → `rpGeneral`** (resolves FI-04; conservative-correct for a
recursive filter; `fmUnordered` stays — `requestPattern` is a separate layer from `filterMode` and does
not affect the CMS7 cache). N>1 clean refusal (`branch=after-frame1-before-recovery-wiring`). Proves N==1
only.
**[2026-06-23: the predecessor step and ownership tail in this paragraph are SUPERSEDED — predecessor
handling is now PIN-CARRY; see the pin-carry decision note immediately below.]**

**[2026-06-23 — PIN-CARRY DECISION (additive; supersedes the predecessor-handling and ownership wording above).]**
K.1E branch-(c) sources the predecessor by PIN-CARRY, not by taking a second VSFrame reference. The
foundational locking/pinning cross-check returned GREEN LIGHT — all Tier-1 fatals PASS on two independent
reads, and the per-invocation pin-list is caller-owned (INV-D1), so this is thin USE of already-proven
machinery, not a cache-core internals change.
- **Predecessor step** (supersedes "acquire cached output[0] as predecessor (real lookup/addref, carried
  in frameData)" above): at `arInitial`, PIN cached output[0] via the proven AS1 fused
  `lookup_frame_and_record_pin` — it returns a BORROWED `const VSFrame*` and records a consumer-pin on the
  per-invocation pin-list, atomically; carry {borrowed pointer + predecessor frame number + pin-list} in
  frameData; request source[1]; return NULL. At `arAllFramesReady`, use the borrowed (still-pinned)
  predecessor into the proven P.11B, then DISCHARGE the pin. Discharge is wired on BOTH the
  `arAllFramesReady` arm AND the `arError` arm; the doubly-abandoned case (activation abandoned AND the
  frameData free callback never runs) is the benign residual below. No second VSFrame reference is ever
  taken for the predecessor — it is borrowed, kept alive by the pin (liveness comes from the pin, INV-B2;
  NOT from output[0]'s checkpoint status — that would be the retired checkpoint-as-pin reasoning,
  R-RETIRED-03).
- **Ownership tail** (supersedes "acquired=1, released=1, transferred=0, balance=0" above, the
  `pred_released=1` / `pred_balance=0` figures in confirmation 2 below, and the restatement in the §10
  v3.2.9.1 NOTE): the proof obligation is a PIN-LEDGER, not a ref-ledger — pin taken=1, discharged=1,
  `pin_count` balance=0, with ZERO predecessor VSFrame refs acquired or freed (borrowed). `transferred=1`
  applies to output[1] ONLY (the K.1B-proven return path), not to the predecessor.
- **KDT consequence:** the frame-1 KDT line's ownership fields move to pin-ledger terms (e.g.
  `pred_pinned` / `pred_discharged` / `pred_pin_balance`, with no acquired/transferred for the borrowed
  predecessor); exact field names to be settled when the K.1E text plan is produced.
- **Benign residual (confirmed from committed code):** an abandoned activation's worst-case residual is an
  UNDISCHARGED PIN — frame-safe and crash-safe (`~Cnr3OutputCacheCore` is `= default` and RAII-frees each
  slot's `Cnr3OwnedFrameRef`; `clear()` is not on the free path → no frame-ref leak, no lifecycle_violation
  at teardown) but SILENT today; surfaced by the future end-of-run integrity report (deferred owed item).
- **New K.1E work (not a pre-existing fact):** discharge must be WIRED INTO the frameData free callback —
  the single point covering normal completion AND a VS-freed abandon — new getFrame-side glue in
  `vapoursynth-Cnr3.cpp` (cross-check INV-F3).
This note does NOT change CMS §8.7 or any cache-core code; it records how K.1E sources the predecessor.

**Designer review of the K.1E branch-(c) proposal: three confirmations accepted; a FOURTH is drafted but
NOT yet sent** (see DELTA §4):
  1. **Defer scene-change** — K.1E is predecessor-present composition only; P.11C already proves reset
     for a given threshold.
  2. **Frame-1 acceptance = predecessor WIRING proof, not blend math** (P.11B owns the math). KDT must
     prove the predecessor was specifically cached output[0] (`pred=0`, `pred_source=output_cache`,
     `pred_lookup=hit`) and was released (`pred_released=1`, `pred_balance=0`); AND there must be at
     least one **known-answer vector** so frame 1 has a real byte-check, not pure KDT self-report.
  3. **P.11B-call scope = thin exposure of proven code only** — P.11C body untouched; no re-routing of
     proven internals; no new pixel/copy algorithm; report-before-broadening. (The bar to watch hardest,
     per the dropped K.1D patch.)
  4. **[NOT YET SENT — the immediate next action]** Temporary live-path code (the N==1 gate / N>1 refusal
     control-flow, any scene-change-deferral stub, the KDT line) must be **uniformly, greppably marked**
     and annotated with **what replaces it and when** (cleanup = grep-and-remove, not archaeology); AND
     ask the coder to confirm the K.1C scaffold is fully removed from the committed tree.

**OPEN VERIFICATION carried into the next chat (DELTA §7):** confirm the K.1C scaffold (old guard
`CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD`, the passthrough callback, `SCAFFOLD_NOT_FILTERED`) is fully gone
from the **actual committed** `src/vapoursynth-Cnr3.cpp` and `src/cnr3_build_config.h` — it was renamed/
replaced at K.1D and verified in the K.1D DIFF, but NOT re-verified against the committed tree.
(Caution: a `vapoursynth-Cnr3_cpp.txt` circulating in uploads is a STALE 9,361-line copy that is NOT the
K.1D-committed file; its "scaffold" hits are a different family, `CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_
WARMUP_*_SCAFFOLD`. Audit the repo, not that file.) Also confirm the full set of files the keystone
touched against repo history (it appears larger than the two-file K.1D patch implied).

**Temporary diagnostics:** the keystone KDT dev-trace (`CNR3_KEYSTONE_DEV_TRACE`, `[KDT]`/`[KDT-SUMMARY]`)
is intentionally present and is scheduled for removal **post-K.1G** per diagnostics spec v1.5 §2.8 — both
the K.1A plan-driven formatter and the live frame-0 / scaffold formatters, plus the temporary guards.

**Confirmed error-frame signature (useful for all future N>0 / error checks):** when vspipe pulls a
frame the plugin refuses via `setFilterError`, it leaves a **header-only y4m** (a single `YUV4MPEG2 …`
line, ~49 bytes, `XLENGTH` reflecting the requested range, **no `FRAME` marker, no payload**) and exits
nonzero. So "header-only y4m, no FRAME marker = clean refusal." The robust N>0 signals are the nonzero
exit + the `NOT-YET-IMPLEMENTED` line; the output-file check is belt-and-braces.

**Owed after K.1E (carried):** branch (d) bounded recovery live wiring; multi-frame VS-LIFECYCLE-01
request-set proof; live scene-change threshold derivation + reset wiring; longer sequential recursive run
beyond N==1; post-K.1G KDT cleanup; the real `VSFrame` return (from K.1B, now retiring inside branch-(c));
fmParallel (a correctness phase); typed-row-pointer-vs-memcpy (a measured fmParallel performance phase,
proven bit-exact-output identical).

**[2026-06-23] Owed (CMS-text decision) — AS4 vs `discharge_all` atomicity-wording discrepancy:**
`discharge_all()` takes one lock per token via `unpin_frame()`, while CMS §8.7 AS4 specifies one lock
acquisition for the whole list. Correctness-safe today (INV-B2 guarantees no slot being discharged can
vanish mid-walk); a register-vs-code WORDING discrepancy only. RULE ON before multi-pin recovery relies
on it. Options: (a) relax AS4 wording to per-token, or (b) add a single-lock batch-discharge variant to
match the register. Tied to the branch-(d) recovery step where multi-pin carriage first appears. (Records
the decision as owed; does NOT change CMS §8.7 or `discharge_all`.)

**Acceptance method unchanged:** four-way (Debug N/N exit 0; Release N/N exit 0; Release
`--force-fail-for-harness-proof` (N-1)/N exit 1; Release `--verbose` N/N exit 0, all priors) + the
coordinator-side harness (frame-0 A/B byte-identical; for K.1E add the frame-1 KDT check + known-answer
byte-check; retain the N>0/N>1 clean refusal). The K.1E-shaped harness is **not yet built** and is a
different shape (frame-1 is not byte-identical to source) — likely needs a constructed deterministic
source so output[1] is computable by hand (DELTA §5). The body of this document (§§4–5, 11) still
describes the H.1A-era state; always confirm current build state from the repository (§3).

**v3.2.8 status note (real-frame pixel path COMPLETE on caller-supplied frames — P.10A–P.11C proven):**
The controlling CMS is **CMS07.3**; the Production Spec is **v2.6**; the diagnostics spec is
**v1.4.1**. On top of the scalar→native bridge (P.7A–P.9A), the scalar pixel pipeline (P.1A–P.6A),
and the C.14A cache-core milestone, **four further phases are now proven, committed, and pushed**,
completing the real-frame pixel path on caller-supplied frames. **Selftest count is now 45/45**
(forced-fail 44/45, exit 1; verbose 45/45).
- **P.10A — VapourSynth plane-view adapter** (count →42). The FIRST real-VS-frame-memory boundary:
  converts VSAPI getFrameWidth/getFrameHeight/getStride/getReadPtr/getWritePtr into the proven P.8A
  native byte-plane views, with bit-depth/plane/dimension/stride validation and a **2-byte stride
  alignment requirement** (`stride % storage_bytes == 0`) before publication — stricter than the
  synthetic layer because this is real frame memory. **memcpy retained; reinterpret_cast<uint16_t*>
  deferred.** getWritePtr-only (avoids read-pointer invalidation). Clear-on-reject (no stale frame
  pointers). Header uses forward declarations (`struct VSAPI; struct VSFrame;`); the test uses a MOCK
  VSAPI over real byte buffers (verified faithful: getStride in bytes, correctly-offset pointers,
  call-counting proves getWritePtr-only). **This phase made `cnr3_frame_processing.cpp` a member of
  the `cnr3` PLUGIN project** (previously selftest-only since P.3A) — now in BOTH projects.
- **P.11A — caller-supplied frame-triplet / plane-set validation** (count →43). Validates and
  assembles the nine plane views (current-source Y/U/V, previous-filtered-output Y/U/V, destination
  Y/U/V) via the P.10A adapter, with dimension/format compatibility checks and clear-on-reject so no
  stale plane pointer survives a failed validation. No pixel processing, no lifecycle.
- **P.11B — caller-supplied real-frame pixel composition** (count →44). The FIRST end-to-end real
  output frame: Y copied unchanged from current source; U/V recursively blended against the
  previous-filtered-output planes. **All-or-nothing destination commit proven by construction**
  (stage Y+U+V into local buffers → validate all three → commit the three destination planes
  back-to-back with nothing fallible between). **R-ARCH-06 predecessor semantics proven at pixel
  level**: a decoy source[N-1] (U=220,V=230) differing by 100/plane from the true
  previous-filtered-output (U=20,V=30) yields, under near-max weight, U=21/V=31 (uses the
  predecessor) vs the decoy's U=219/V=229 — both asserted, strongly discriminating. **memcpy
  retained; typed-row-pointer optimization deferred to a measured fmParallel performance phase.**
- **P.11C — caller-supplied scene-change/reset** (count →45). Composes the P.11B path with
  scene-change detection. Accumulates `abs((cur_downsampled_luma − prev_downsampled_luma) <<
  (subw+subh))` (+ `abs(u_diff)+abs(v_diff)` if scene_chroma) and applies the **strict rule
  `diff_total > threshold` → reset** (equality keeps the blend active — verified against vsCnr2.cpp;
  a boundary vector proves diff_total==threshold does NOT fire). On reset, outputs current-source
  Y/U/V (no blend). Preserves the all-or-nothing commit; accumulation is overflow-guarded (a
  defensive improvement over vsCnr2). **Threshold DERIVATION is deferred** to plugin/instance config
  (`Cnr3SceneChangeConfig` accepts a pre-computed int64 threshold + scene_chroma bool); the future
  derivation must reproduce vsCnr2's `diff_max = (scdthr·w·h·max_pixel_diff/100) << (depth−8)` with
  `max_pixel_diff = scene_chroma ? ((219+224·2)>>(subw+subh)) : 219` AND match P.11C's accumulation
  units, with the same scene_chroma driving both derivation and accumulation.
**The entire pixel path is now proven on CALLER-SUPPLIED frames** — real VS frame memory →
composition → predecessor semantics → scene-change. **What remains is the cache↔pixel / getFrame
keystone**: the proven cache core (through C.14A) connects to the proven pixel chain (through P.11C)
inside VS getFrame scheduling. This is the HARD DESIGNER GATE (proposal-plus-read-first; do not apply
without designer review). Its load-bearing rules: R-ARCH-06 (predecessor = previous filtered output
from cache/recovery, NEVER source[N-1]); VS-LIFECYCLE-01 (frames retrieved in arAllFramesReady must
be requested in arInitial); cache-lookup addref released/transferred exactly once; source-frame refs
released on every path; checkpoint/recovery pin balance; return-transfer/cache-store ownership
explicit; the Category-B developer-alert (emission half of what C.13B detects, CMS §9.6.4). The body
of this document (§§4–5, 11) still describes the H.1A-era state; always confirm current build state
from the repository (§3).

**v3.2.7 status note (scalar→native bridge COMPLETE — P.7A–P.9A proven):** The controlling
CMS is **CMS07.3**; the Production Spec is **v2.6**; the diagnostics spec is **v1.4.1**. On top
of the complete scalar pixel pipeline (P.1A–P.6A) and the C.14A cache-core milestone, **three
further phases are now proven, committed, and pushed** (git: acb5080 P.7A, a6fd09c P.8A, ab37443
P.9A), completing the bridge from native byte buffers through the scalar pixel chain. **Selftest
count is now 41/41** (forced-fail 40/41, exit 1; verbose 41/41).
- **P.7A — source-luma downsample traversal** (count →39). Produces the downsampled-luma scalar
  plane that P.6A consumes, by traversing a scalar source-luma plane with the proven P.4A
  tap-coordinate + four-tap helpers. Output dims via the guarded ceil form
  `(luma + ((1<<sub)-1)) >> sub`. NOTE: that ceil derivation is a **scalar-proof-harness device
  only** (so odd-size vectors can stress the right/bottom/corner clamp); **real VS integration
  must use actual per-plane frame dimensions from VapourSynth and validate real subsampled source
  dimensions before this layer.** Clamps against SOURCE dims, validates output view against the
  derived dims, two-pass no-partial-publish.
- **P.8A — native byte-plane access** (count →40). Native byte-plane views + load/store/copy
  helpers. **8-bit → 1 byte/sample; 9..16-bit → 2 bytes/sample; outside 8..16 → invalid_argument.**
  Scalar domain stays `int` in 0..sample_peak. **Column byte offset is `x * storage_bytes`, not
  `x`, so adjacent two-byte samples do not overlap** (the 10-bit vectors prove this). Uses
  **`memcpy` for two-byte load/store** (independent of synthetic-buffer alignment; native-endian =
  little-endian on x86/x64, which matches VapourSynth) — deliberately avoids premature
  `reinterpret_cast<uint16_t*>`; the real-frame phase decides typed row-pointer handling against
  the actual VS frame-memory contract. Validation: non-null, bit depth 8..16, positive dims,
  `stride_bytes >= width*storage_bytes`, overflow-safe `height*stride_bytes`, in-bounds x/y,
  sample in 0..sample_peak. Whole-plane copies use P.6A/P.7A two-pass no-partial-publish.
- **P.9A — native luma downsample bridge** (count →41). Composes P.8A (native byte-plane access) →
  P.7A (scalar downsample): copies a synthetic native byte-buffer source-luma plane into a scalar
  int plane, then produces the downsampled-luma scalar plane P.6A consumes. The 10-bit vector
  proves `x*storage_bytes` two-byte addressing survives the COMPOSED path; the 8-bit vector covers
  the one-byte path. Still SYNTHETIC native byte buffers + scalar int buffers — **not real VS
  frame memory.** No-partial-publish preserved across invalid native sample / native stride /
  output-geometry cases.
**The scalar pixel pipeline AND the native byte-buffer access/bridge layer are now complete and
proven.** Everything from a native (synthetic) source-luma byte buffer → scalar planes → downsampled
luma → chroma blend → output chroma plane is proven on buffers Claude/the coder construct. **What
remains crosses into real VapourSynth machinery** (see §8 NEXT): real frame-memory adaptation
(getReadPtr/getWritePtr/getStride, actual per-plane dimensions, typed row-pointer vs memcpy decision
against the real frame contract); scene-change accumulation; and the getFrame/source-lifecycle/cache
integration where the proven CACHE CORE (through C.14A) finally meets the proven PIXEL CHAIN (through
P.9A). Operational item now load-bearing at the very next phase: confirm `cnr3_frame_processing.cpp`
is a member of the **`cnr3` plugin** project (it is in the selftest project from P.3A) — the real-frame
phase is the first to need it in the plugin build. The body of this document (§§4–5, 11) still
describes the H.1A-era state; always confirm current build state from the repository (§3).

**v3.2.6 status note (scalar pixel pipeline COMPLETE — P.3A–P.6A proven):** The controlling
CMS is **CMS07.3**; the Production Spec is **v2.6**; the diagnostics spec is **v1.4.1**. On top
of P.1A/P.2A and the C.14A cache-core milestone, **four further pixel phases are now proven,
committed, and pushed**, completing the scalar pixel decision pipeline. **Selftest count is now
38/38** (forced-fail 37/38, exit 1; verbose 38/38).
- **P.3A — weighted chroma blend** (count →35). Scalar int64 blend `dst = (weight*prev +
  (shift-weight)*cur + shift1) >> shift2`, with `shift2 = depth<<1`, `shift = 1LL<<shift2`,
  `shift1 = shift>>1`, `weight = y_response*chroma_response` (int64). VERIFIED bit-exact against
  vsCnr2.cpp (the source was fetched and read at P.3A). int64 accumulator proven overflow-safe at
  16-bit; convex-combination boundary (`weight ≤ sample_peak² < shift`) proven and documented;
  half-point round-half-up proven; validation-before-arithmetic with sentinel preservation.
- **P.4A — downsampled-luma** (count →36). Scalar `(a+b+c+d+2)>>2` 2×2 box average + tap-coordinate
  helper (`x0=cx<<subw`, `x1=x0+1`, `y0=cy<<subh`, `y1=y0+subh`). Reproduces vsCnr2's degenerate
  4:2:2/4:4:0/4:4:4 tap collapse. **Deliberate divergence (Position A, Dave-confirmed):** edge taps
  are CLAMPED (edge-replicated) rather than reading past the frame as vsCnr2 does — vsCnr2's edge
  read depends on AviSynth padding and is not a deliberate algorithmic value; the divergence is
  confined to the rightmost/bottom chroma edge strip and affects only the luma-difference feeding
  the chroma weight (luma output unchanged). Documented under the accuracy rule.
- **P.5A — signed-difference / table-lookup / blend bridge** (count →37). Composes P.1A lookup +
  P.2A geometry + P.3A blend + P.4A samples. **Signed differences (`current − previous`) stay
  signed `int` end-to-end into the total bounded lookup — no unsigned intermediate anywhere on the
  sample→diff→index path** (the long-standing signed/unsigned + wraparound concern, closed here and
  proven in both directions). Validates P.2A geometry before lookup; publishes result only on full
  success.
- **P.6A — chroma-plane traversal** (count →38). **NOTE: this is a deliberate roadmap re-letter —
  P.6A is now chroma-plane traversal, NOT the original roadmap's "scene-change," which is deferred
  further** (scene-change accumulates DURING traversal, so traversal logically precedes it).
  Row-major traversal over strided scalar `int` planes composing the P.5A kernel per sample;
  stride-aware indexing `(y*stride)+x` with validation `stride ≥ width` (no row-spill) and
  `height ≤ INT_MAX/stride` (no index overflow); padding columns preserved; plane-level
  no-partial-publish (two-pass: compute-all-into-local, write-output-only-on-full-success).
**Governing decisions settled this generation (now load-bearing; see Role Handover v1.3 Part 6A):**
(1) **Accuracy rule** — accuracy upgrades are permitted ONLY where vsCnr2 is *accidentally lossy*
(the only instance is the 8-bit→native parameter scaling, which P.2A upgrades to round-to-nearest);
*definitional* integer arithmetic (the `strength/2` curve quantisation, the blend `>>shift2`/`shift1`
rounding, the luma `+2>>2`) is reproduced bit-exact. (2) **Three-layer compatibility claim** —
bit-exact CNR2 blend arithmetic and response-curve construction; proportional parameter scaling
identical to CNR2 at 8/16-bit and deliberately correcting CNR2's integer-factor truncation at
10/12/14-bit. (3) **P.4A edge-clamp** divergence as above.
**The scalar pixel pipeline is therefore COMPLETE and proven.** The **next phases cross from scalar
proof into real VapourSynth machinery** (see §8 NEXT): source-luma downsample as a real buffer pass;
actual 8/16-bit frame-buffer access with `reinterpret_cast<uint16_t*>` stride handling; scene-change
accumulation; and the getFrame/source-lifecycle/cache integration where the proven cache core
(through C.14A) finally connects to the proven pixel kernel (through P.6A). Two operational items
become live there: `cnr3_frame_processing.cpp` must be confirmed a member of the **`cnr3` plugin**
project (it is in the selftest project from P.3A; the plugin build will need it), and the proof
surface shifts (real-frame traversal needs synthetic frame buffers or a different proof approach
than scalar reference vectors). The body of this document (§§4–5, 11) still describes the H.1A-era
state; always confirm current build state from the repository (§3).

**v3.2.5 status note (pixel arc advancing — P.2A proven):** The controlling CMS is
**CMS07.3**; the Production Spec is **v2.6**; the diagnostics spec is **v1.4.1**. On top of
the C.14A cache-core milestone and P.1A, the second pixel phase **CMS07-P.2A**
(response-table configuration surface) is now proven, committed, and pushed. **Selftest
count is now 34/34** (forced-fail 33/34, exit 1; verbose 34/34). P.2A added an
instance-agnostic response-table config surface (`Cnr3ResponseCurveKind{narrow,wide}`,
`Cnr3ResponsePlaneConfig{threshold_8bit, strength_8bit, curve}`,
`Cnr3ResponseTableConfig{sample_peak, y, u, v}`, `Cnr3ResponseTables{...}`) plus the helpers
`cnr3_scale_8bit_parameter_to_sample_peak`, `cnr3_response_table_geometry_for_sample_peak`,
and `build_cnr3_response_tables` (build-local-then-publish-on-full-success; no partial
publish on invalid config). It did NOT resurrect Cnr3Data, parse VSMap or mode strings, or
wire any VS/getFrame/cache code. **Two facts were verified against original source during
P.2A review and are now load-bearing for the pixel arc:** (1) the 8-bit→native parameter
scaling `(clamp(value,0,255) * sample_peak + 127) / 255` (int64-widened, round-to-nearest)
is EXACTLY the prior-integration helper `scale_8bit_parameter_to_bit_depth()` in
`superseded_by_v7/vapoursynth-Cnr3.cpp` — faithful salvage, not invented; (2) P.2A's
table geometry (`table_offset = sample_peak`, `table_size = sample_peak*2+1`; 255/511 at
8-bit) differs from the old source's (`sample_peak+1` / `offset*2+1`; 256/513) but was proven
by full signed-difference sweep [-255,+255] to return IDENTICAL looked-up values — the old
`+1`/`+2` merely left two never-read slots, so P.2A's geometry is equivalent and tighter and
is the forward convention. The true upstream origin of the scaling (and the blend arithmetic)
will be re-confirmed against the **CNR2 source at P.3A**. The **next phase is P.3A** (weighted
blend), at which Dave uploads CNR2/vscnr2 so the designer can verify the blend line-by-line
AND retro-confirm P.2A scaling/geometry/signedness against the true origin. The pixel-layer
bit-depth/colour-space/luma facts confirmed from CNR2 source (this session) and the P.3A–P.5A/VS
review checklist are now recorded in the **Designer/Reviewer Role Handover (v1.2)** §§ (see that
document's pixel-arc reference and review-checklist sections). The body of this document (§§4–5,
11) still describes the H.1A-era state; always confirm current build state from the repository
(§3).

**v3.2.4 status note (pixel arc underway — P.1A proven):** The controlling CMS is
**CMS07.3**; the Production Spec is **v2.6**; the diagnostics spec is **v1.4.1**. The cache
core was proven through **CMS07-C.14A** (the isolated cache-core milestone — see the v3.2.3
note retained below), and the first downstream pixel phase **CMS07-P.1A**
(response-table salvage and vector proof) is now proven, committed, and pushed. **Selftest
count is now 33/33** (forced-fail 32/33, exit 1). P.1A salvaged the pure vscnr2
signed-difference response-table helpers (`get_cnr3_table_value_for_signed_diff`,
`build_cnr3_weight_table` adapted to `Cnr3Status`) into the active `src/cnr3_response_tables.*`,
proving exact-integer reference vectors (independently verified); it did NOT resurrect
Cnr3Data or wire any VS/VSMap/getFrame/cache code. The **next phase is P.2A** (pixel-
configuration parameter surface for response tables), not yet proposed. The body of this
document (§§4–5, 11) still describes the H.1A-era state; always confirm current build state
from the repository (§3). The companion to this document for HOW the design/review role is
performed is the **Designer/Reviewer Role Handover (currently v1.1)**.

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
*** SUPERSEDED (v3.5.1): this section records a restart-era instance of "the immediate next phase." H.2A is long complete and the entire keystone + branch-(d) recovery arc (D.1-D.5) is committed. The PREVAILING next phase is P.11C scene-change uniform wiring — see the v3.5.1 UPDATE block at the top of this document. The text below is retained as history of record only. ***
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

## 8. Remaining work plan (after P.1A — pixel arc underway; cache-core milestone reached at C.14A)
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
PIXEL ARC STARTED — P.1A PROVEN (committed 2026-06-19):
    CMS07-P.1A (response-table salvage and vector proof) is committed and pushed; count 33/33.
    It salvaged the pure vscnr2 signed-difference response-table helpers
    (get_cnr3_table_value_for_signed_diff = total/safe lookup, out-of-range -> 0;
    build_cnr3_weight_table = the cosine-curve weight table, adapted to return Cnr3Status for
    invalid_argument / allocation_failed) into the active src/cnr3_response_tables.*, proving
    exact-integer reference vectors (independently verified: narrow 255/10 d5=127, wide d5=216,
    peak 254 from the strength/2 integer-division quirk; 200/20 family narrow d7=145, wide
    d7=192; threshold-zero; clamp groups; plus an invalid-argument path test). It did NOT
    salvage build_cnr3_lookup_tables(Cnr3Data&, VSMap*, ...) (deferred to the config-parsing
    surface; Cnr3Data NOT resurrected), and touched no cache/VS/getFrame/diagnostics code. The
    test was added to the existing cnr3_cache_core_selftest runner (no new module yet — the
    baseline lacks .vcxproj, so new-module wiring could not be verified; revisit a separate
    pixel selftest module at P.3A when the suite grows).
P.2A PROVEN (committed 2026-06-19):
    CMS07-P.2A (response-table configuration surface) is committed and pushed; count 34/34
    (forced-fail 33/34, exit 1; verbose 34/34). It added an instance-agnostic config surface
    (Cnr3ResponseCurveKind{narrow,wide}; Cnr3ResponsePlaneConfig{threshold_8bit, strength_8bit,
    curve}; Cnr3ResponseTableConfig{sample_peak, y, u, v}; Cnr3ResponseTables{...}) and helpers
    cnr3_scale_8bit_parameter_to_sample_peak, cnr3_response_table_geometry_for_sample_peak, and
    build_cnr3_response_tables. build_cnr3_response_tables builds into a local object and
    publishes (via swap) only after Y, U and V all succeed — invalid config never partially
    overwrites an existing valid table set (proven byte-for-byte). The per-plane Y/U/V decomposition
    matches the authoritative old build_cnr3_lookup_tables exactly (mode[0/1/2]->Y/U/V curve;
    n=threshold, m=strength). Two source-verified facts now load-bearing (see Role Handover v1.3):
      - 8-bit->native scaling = (clamp(value,0,255) * sample_peak + 127) / 255, int64-widened,
        round-to-nearest. VERIFIED EXACT against scale_8bit_parameter_to_bit_depth() in
        superseded_by_v7/vapoursynth-Cnr3.cpp; CNR2-origin re-confirmed at P.3A (see below).
      - Geometry (offset=sample_peak, size=sample_peak*2+1; 255/511 at 8-bit) PROVEN lookup-
        equivalent to the old source's offset+1/size+2 across [-255,+255]; P.2A's is the forward
        convention.
P.3A PROVEN (committed 2026-06-20): weighted chroma blend
    CMS07-P.3A; count 34->35 (forced-fail 34/35; verbose 35/35). Scalar int64 weighted blend:
    dst = (weight*prev + (shift-weight)*cur + shift1) >> shift2; shift2 = depth<<1;
    shift = 1LL<<shift2; shift1 = shift>>1; weight = y_response*chroma_response (int64).
    VERIFIED BIT-EXACT against vsCnr2.cpp (source fetched and read during P.3A review). int64
    accumulator proven overflow-safe at 16-bit (weights exceed INT32_MAX); convex-combination
    boundary (weight <= sample_peak^2 < shift) proven at the max-weight boundary and documented in
    code; half-point round-half-up proven; validation-before-arithmetic with output unchanged on
    invalid input. The CNR2 source cross-check also settled the SCALING POLICY (see accuracy rule)
    and confirmed the three-layer compatibility claim. P.3A added a VS project change (it wired the
    pre-existing cnr3_frame_processing.cpp into the selftest .vcxproj — committed with the phase).
P.4A PROVEN (committed 2026-06-20): downsampled-luma
    CMS07-P.4A; count 35->36 (forced-fail 35/36; verbose 36/36). Scalar (a+b+c+d+2)>>2 2x2 box
    average + tap-coordinate helper (x0=cx<<subw, x1=x0+1, y0=cy<<subh, y1=y0+subh). Reproduces
    vsCnr2's degenerate 4:2:2/4:4:0/4:4:4 tap collapse. DELIBERATE DIVERGENCE (Position A, confirmed
    by Dave): edge taps are CLAMPED (edge-replicated) rather than reading past the frame as vsCnr2
    does. Rationale: vsCnr2's edge read relies on AviSynth frame-padding slack and is not a
    deliberate algorithmic value; clamping is safe and arguably more correct. Confined to the
    rightmost/bottom chroma edge strip; affects only the luma-difference feeding the chroma weight
    (luma output unchanged). Documented under the accuracy rule. Non-behavioural refactor: sample_peak
    factored into cnr3_sample_peak_for_bit_depth, used by the P.3A blend helper (P.3A behaviour
    unchanged; all P.3A vectors still pass).
P.5A PROVEN (committed 2026-06-20): signed-difference / table-lookup / blend bridge
    CMS07-P.5A; count 36->37 (forced-fail 36/37; verbose 37/37). Composes P.1A total lookup +
    P.2A geometry + P.3A blend + P.4A samples into the per-sample chroma decision. THE SIGNED PATH
    IS PROVEN: signed_diff = current - previous is computed and carried as signed int end-to-end
    into get_cnr3_table_value_for_signed_diff — NO unsigned intermediate anywhere on the
    sample->diff->index path (the long-standing signed/unsigned + wraparound concern, closed and
    proven in both directions, plus the zero-response/table-miss and extreme +-255 paths). Validates
    P.2A geometry (table_offset == sample_peak; both tables sample_peak*2+1) before lookup; publishes
    result only on full success.
P.6A PROVEN (committed 2026-06-20): chroma-plane traversal
    CMS07-P.6A; count 37->38 (forced-fail 37/38; verbose 38/38).
    ** DELIBERATE ROADMAP RE-LETTER: P.6A is CHROMA-PLANE TRAVERSAL, not the original roadmap's
       "scene-change". Scene-change is deferred further (it accumulates DURING traversal, so
       traversal logically precedes it). The phase letters below supersede the earlier roadmap. **
    Row-major traversal over strided scalar int planes, composing the P.5A kernel per sample.
    Stride-aware indexing (y*stride)+x with validation stride >= width (no row-spill past the row
    into the next/buffer) and height <= INT_MAX/stride (no index overflow). Padding columns preserved
    (only active width written). Plane-level no-partial-publish: two-pass (compute all samples into a
    local buffer, write to the output plane only after the whole plane validates and computes).
    STILL SCALAR int BUFFERS — not real frame memory.
P.7A PROVEN (committed 2026-06-20, git acb5080): source-luma downsample traversal
    CMS07-P.7A; count 38->39. Produces the downsampled-luma scalar plane P.6A consumes, by
    traversing a scalar source-luma plane with the proven P.4A tap-coordinate + four-tap helpers.
    Output dims via guarded ceil (luma + ((1<<sub)-1)) >> sub. ** That ceil derivation is a
    SCALAR-PROOF-HARNESS device only (so odd-size vectors stress right/bottom/corner clamp); REAL
    VS integration must use actual per-plane frame dimensions and validate real subsampled source
    dims before this layer. ** Clamps against SOURCE dims (not output); validates output view
    against the derived dims; 4:2:0/4:2:2/4:4:0/4:4:4 shapes proven; two-pass no-partial-publish.
P.8A PROVEN (committed 2026-06-20, git a6fd09c): native byte-plane access
    CMS07-P.8A; count 39->40. Native byte-plane views + load/store/whole-plane-copy helpers.
    STORAGE: 8-bit -> 1 byte/sample; 9..16-bit -> 2 bytes/sample; outside 8..16 -> invalid_argument.
    Scalar domain stays int in 0..sample_peak. ** Column byte offset is x*storage_bytes, NOT x, so
    adjacent two-byte samples do not overlap (the 10-bit vectors prove this). ** Uses memcpy for
    two-byte load/store (independent of buffer alignment; native-endian = little-endian on x86/x64,
    matching VapourSynth) — deliberately NOT reinterpret_cast<uint16_t*> yet; the real-frame phase
    decides typed row pointers vs memcpy against the actual VS frame contract. Validation: non-null,
    bit depth 8..16, positive dims, stride_bytes >= width*storage_bytes, overflow-safe
    height*stride_bytes, in-bounds x/y, sample in 0..sample_peak. Copies use two-pass no-partial-publish.
P.9A PROVEN (committed 2026-06-20, git ab37443): native luma downsample bridge
    CMS07-P.9A; count 40->41. Composes P.8A (native byte-plane access) -> P.7A (scalar downsample):
    copies a synthetic native byte-buffer source-luma plane into a scalar int plane, then produces
    the downsampled-luma scalar plane P.6A consumes. The 10-bit vector proves x*storage_bytes
    two-byte addressing survives the COMPOSED path; the 8-bit vector covers the one-byte path. Still
    SYNTHETIC native byte buffers + scalar int buffers — NOT real VS frame memory. No-partial-publish
    preserved across invalid native sample / native stride / output-geometry cases.
P.10A PROVEN (committed 2026-06-20): VapourSynth plane-view adapter
    CMS07-P.10A; count 41->42. The FIRST real-VS-frame-memory boundary. Converts VSAPI getFrameWidth/
    getFrameHeight/getStride/getReadPtr/getWritePtr into the proven P.8A native byte-plane views, with
    bit-depth/plane/dimension/stride validation and a 2-byte STRIDE ALIGNMENT requirement
    (stride % storage_bytes == 0) before publication — stricter than the synthetic layer because this
    is real frame memory. ** memcpy retained; reinterpret_cast<uint16_t*> still deferred. ** getWritePtr-
    only (avoids read-pointer invalidation). Clear-on-reject (no stale frame pointers). Header uses
    forward declarations (struct VSAPI; struct VSFrame;); the test uses a MOCK VSAPI over real byte
    buffers, verified faithful (getStride in bytes, correctly-offset pointers, call-counting proves
    getWritePtr-only, stride-7 rejection proves the alignment guard fires). ** This phase made
    cnr3_frame_processing.cpp a member of the cnr3 PLUGIN project (previously selftest-only) — now in
    BOTH projects. **
P.11A PROVEN (committed 2026-06-20): caller-supplied frame-triplet / plane-set validation
    CMS07-P.11A; count 42->43. Validates and assembles the nine plane views (current-source Y/U/V,
    previous-filtered-output Y/U/V, destination Y/U/V) via the P.10A adapter, with dimension/format
    compatibility checks and clear-on-reject so no stale plane pointer survives a failed validation.
    No pixel processing, no lifecycle, no cache.
P.11B PROVEN (committed 2026-06-20): caller-supplied real-frame pixel composition
    CMS07-P.11B; count 43->44. The FIRST end-to-end real output frame. Y copied unchanged from current
    source; U/V recursively blended against the previous-filtered-output planes. ** ALL-OR-NOTHING
    destination commit proven by construction: stage Y+U+V into local buffers, validate all three,
    commit the three destination planes back-to-back with nothing fallible between. ** R-ARCH-06
    PREDECESSOR SEMANTICS proven at pixel level: a decoy source[N-1] (U=220,V=230) differing by
    100/plane from the true previous-filtered-output (U=20,V=30) yields, under near-max weight, U=21/
    V=31 (uses predecessor) vs the decoy's U=219/V=229 — both asserted, strongly discriminating.
    Per-plane dims (luma plane-0 dims; chroma subsampled dims). response_tables.y = luma-difference
    response into the weight; .u/.v = chroma response; shared downsampled-luma feeds both U and V.
    ** memcpy retained; typed-row-pointer optimization deferred to a measured fmParallel performance
    phase (recorded in summary flags + trace). No VS header modified. **
P.11C PROVEN (committed 2026-06-20): caller-supplied scene-change/reset
    CMS07-P.11C; count 44->45. Composes the P.11B path with scene-change detection. Accumulates
    abs((cur_downsampled_luma - prev_downsampled_luma) << (subw+subh)) (+ abs(u_diff)+abs(v_diff) if
    scene_chroma) and applies the STRICT rule diff_total > threshold -> reset; equality keeps the blend
    active (VERIFIED against vsCnr2.cpp; a boundary vector proves diff_total==threshold does NOT fire).
    On reset, outputs current-source Y/U/V (no blend). Preserves all-or-nothing commit; accumulation is
    overflow-guarded (defensive improvement over vsCnr2). ** THRESHOLD DERIVATION is DEFERRED to plugin/
    instance config: Cnr3SceneChangeConfig accepts a pre-computed int64 threshold + scene_chroma bool.
    The future derivation must reproduce vsCnr2's diff_max = (scdthr*w*h*max_pixel_diff/100) <<
    (depth-8) with max_pixel_diff = scene_chroma ? ((219+224*2)>>(subw+subh)) : 219, AND must match
    P.11C's accumulation units, with the SAME scene_chroma driving both derivation and accumulation. **
    P.11C is the per-SAMPLE-early-stop form; vsCnr2 checks per-ROW — proven IDENTICAL final decision
    because diff_total is monotonic.
    ** REFINED DECOMPOSITION (recorded so it is not lost): the original roadmap's coarse "real-frame /
    scene-change / getFrame integration" was decomposed into the safer sequence P.10A (plane adapter)
    -> P.11A (triplet validation) -> P.11B (composition) -> P.11C (scene-change), ALL on CALLER-SUPPLIED
    frames, BEFORE the getFrame keystone. This de-risks the keystone: the pixel path is fully proven on
    caller-supplied frames, so the keystone reasons ONLY about frame SOURCING / lifecycle / ownership
    over an already-proven pixel path. Scene-change was deliberately placed BEFORE the keystone (it is a
    pixel-path decision), keeping the keystone's surface purely lifecycle/sourcing. **
NEXT — the entire pixel path is proven on CALLER-SUPPLIED frames; what remains is the keystone and
post-keystone integration:
    1. CACHE<->PIXEL / getFrame KEYSTONE (THE HARD DESIGNER GATE — proposal-plus-read-first; do NOT
       apply without designer review). Connects the proven CACHE CORE (through C.14A) to the proven
       PIXEL CHAIN (through P.11C) inside VS getFrame scheduling. Expected split: arInitial request
       planning; arAllFramesReady retrieval + release; predecessor acquisition from cache/recovery ONLY
       (NEVER source[N-1], R-ARCH-06); pixel processing via the proven P.11B/P.11C caller-supplied
       helpers; store + return-transfer ownership; fallback/error cleanup. Load-bearing rules:
       VS-LIFECYCLE-01 (frames retrieved in arAllFramesReady requested in arInitial); cache-lookup
       addref released/transferred EXACTLY once; source-frame refs released on EVERY path;
       checkpoint/recovery pin balance; return-transfer/cache-store ownership explicit; the AS1-AS7
       atomic-scope register and the lock/ownership disciplines (decide-inside-lock, freeFrame-outside-
       lock, RAII guard) become directly live again; the Category-B developer-alert (CMS §9.6.4) is the
       EMISSION half of what the C.13B guard DETECTS — map a hard status to clean filter failure plus a
       bounded one-shot stderr developer-alert OUTSIDE locks; expected Category-A duplicate/adopt stays
       silent.
    2. THRESHOLD DERIVATION for scene-change (the deferred plugin-config arithmetic) — reproduce
       vsCnr2's diff_max faithfully (see the P.11C entry), match P.11C's accumulation units, watch the
       (219+224*2)>>(subw+subh) precedence and the <<(depth-8) bit-depth scaling.
    3. fmParallel — this is a CORRECTNESS phase, not a performance phase: it is where the cache/recovery
       concurrency-correctness is finally exercised. Comes AFTER the keystone (which wires single-
       threaded getFrame first). "Prove it works under fmParallel" is the real remaining proof.
    4. Plugin registration/build wiring; diagnostics cleanup / scaffold retirement; real workload tests;
       final doc updates.
    5. TYPED-ROW-POINTER vs memcpy decision — deferred to a MEASURED fmParallel performance phase; any
       optimisation must be proven BIT-EXACT-OUTPUT identical to the memcpy path before acceptance.
    NOTE on the arc: the keystone and fmParallel re-enter the cache-core's concurrency-correctness
    domain (the reason the cache architecture exists). The PDAP delivery process, read-first review for
    load-bearing work, genuine-failure-mode tests, and explicit-known expected values all still apply.
    PROCESS NOTE: P.1A was reviewed read-first POST-COMMIT (sound — low-risk pure maths); every phase
    since (P.2A–P.11C) was reviewed read-first BEFORE apply (or, for the designer-trigger phases P.10A/
    P.11B, proposal-reviewed then patch-reviewed), with every reference vector independently recomputed
    by the designer and (from P.3A) cross-checked against the fetched vsCnr2.cpp source. The keystone is
    the single most important designer-review point remaining. See the Designer/Reviewer Role Handover
    (v1.5) for the full review discipline, the pixel-arc reference, the accuracy rule, and the review
    checklist (the byte-plane/stride/real-frame/scene-change items are now done; the live hunting list
    is the getFrame/cache keystone).

    *** v3.2.9.1 UPDATE: the NEXT item (1) above — the cache↔pixel / getFrame keystone — is no longer
    "next"; it is UNDER WAY and committed through K.1D, with K.1E branch-(c) in flight. See the v3.2.9.1
    status note at the top of this document and the companion DELTA
    (CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md) for the keystone decomposition
    (K.1A–K.1G), the four committed phases, the K.1D reorientation lesson, and the owed items. The
    keystone load-bearing rules listed in item (1) remain exactly the bars in force. ***
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
*** v3.2.9.1 NOTE: the "no getFrame wiring until the cache core is proven complete" bar was
satisfied at C.14A; getFrame integration is now the live keystone work (K.1A–K.1E). The
remaining do-not-implement items (no CMS06.x/H15.6B, no old cache concepts, no CNR2
recovery/predecessor logic, no unapproved salvage/file/lock-scope changes) remain in force.
The keystone has ADDED a load-bearing rule: proven/selftested code is never modified —
behaviour or internals — without explicit visible planning and designer approval in advance
(see the v3.2.9.1 status note and DELTA §2). ***

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
*** v3.2.9.1 NOTE: the lookup-ref balance obligation (acquired == released + transferred) is
now exercised LIVE in the keystone — K.1B proved it synthetically (success 1/0/1); K.1E
branch-(c) proves the OPPOSITE disposition live (acquired=1, released=1, transferred=0,
balance=0: a predecessor is consumed-and-released, not transferred). ***

*** [2026-06-23 UPDATE — PIN-CARRY] Under the pin-carry decision (see the v3.2.9.1 status note at the top of
this document), the K.1E predecessor obligation is restated in PIN-LEDGER terms: pin taken=1, discharged=1,
`pin_count` balance=0, with ZERO predecessor VSFrame refs (borrowed, kept alive by the pin); `transferred=1`
applies to output[1] only (the K.1B return path). The lookup-ref balance obligation above remains correct
for output[1] and for any genuinely owned-ref consumer; the K.1E predecessor specifically is pin-carry,
not ref-carry. ***

*** v3.2.9.2 NOTE:    

*** [2026-06-24 UPDATE — R-ARCH-06 LIVE-PROVEN at N=2 (K.1E.3)] The pin-ledger obligation (pin taken=1,
discharged=1, balance=0, zero predecessor refs carried) is now proven LIVE at BOTH N==1 and N==2, and the
filtered-output-not-source predecessor identity is byte-proven at N==2 (output[2]=163/93 from cached
output[1], not source[1]). This closes the R-ARCH-06 distinction obligation for the bounded branch-(c)
path. Multi-pin discharge balance (anchor + holes) remains OWED to branch-(d) recovery, gated by the AS4
vs discharge_all wording decision. ***

---

## 11. Current status summary
*** SUPERSEDED (v3.5.1): the status block below is the CMS07.2 / 28-selftest restart-era snapshot, retained as history of record. The PREVAILING current status is in the v3.5.1 UPDATE block at the top: committed through CMS07-D.5, 52/52 selftests, CMS07.13 / Spec v2.11 / Document A v3.5, next phase P.11C. ***
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
*** v3.2.9.1 CURRENT STATUS SUMMARY (supersedes the H.1A-era summary directly above; confirm
from the repository) ***
```text
Design authority:   CMS07.7 (cnr3_cache_manager_design_v7_7.1.md).
Production Spec:    v2.6 (§3A populated; includes R-PROCESS-20 / PDAP).
Diagnostics spec:   v1.5 (subordinate; §2.8 = temporary keystone KDT, removed post-K.1G).
Companion:          v7.8 (CNR3_CMS_Future_Investigations_and_Open_Questions_v7_8.md; FI-04).
Role Handover:      v1.5 -> bump to v1.6 (state pointer only; see DELTA §9).
Code state:         Keystone committed through CMS07-K.1D; 47/47 selftests
                    (forced-fail 46/47 exit 1; verbose 47/47, all priors).
                    K.1A request-plan+KDT; K.1B cached-output-return ownership (synthetic);
                    K.1C live passthrough scaffold; K.1D first REAL output[0] via copyFrame.
Immediate task:     (1) Confirm build state from the repo (section 3, but for the K.1D marker
                    CMS07-K.1D-live-frame0-fresh-start-store-return-proof and count 47).
                    (2) Run the scaffold audit (DELTA §7) — confirm the K.1C scaffold is fully
                    removed from the committed tree.
                    (3) Send the FOURTH K.1E confirmation to the coder (temporary-code marking +
                    scaffold-removal question — drafted, NOT yet sent; DELTA §4).
                    (4) Then the coder produces the read-first K.1E branch-(c) patch; review it
                    against the three accepted confirmations + ownership balance + fences.
In flight:          CMS07-K.1E branch-(c) (live predecessor-present frame-1 compute), pre-patch.
After that:         branch (d) recovery; multi-frame VS-LIFECYCLE-01; live scene-change wiring;
                    longer sequential run; post-K.1G KDT cleanup; fmParallel (correctness);
                    typed-row-pointer-vs-memcpy (measured, bit-exact).
Companion DELTA:    CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md (authoritative
                    for the keystone delta; the repository is authoritative on build state).
Pixel layer:        Proven on caller-supplied frames through P.11C; CNR2 is pixel-maths
                    reference only. The keystone reasons about sourcing/lifecycle/ownership
                    over that already-proven pixel path.
```

— End of Document B v3.2.9.1.
