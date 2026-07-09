# CNR3 — Diagnostics Arc: Findings, Decisions & Provenance

**Version:** v1.8 (supersedes v1.7)

*** v1.8 ADVANCE (2026-07-09): IN-PLUGIN DIAG ARC CLOSED at DIAG.4; two analysis-track diagnostics committed.
DECISIONS + FINDINGS banked:
(a) DIAG.4 COMMITTED + PUSHED (marker CMS07-DIAG.4-memory-dsum02-and-arc-close). Memory D-SUM-02 implemented from the
    stub: 5 snapshot points (baseline @cnr3_create / periodic @arAllFramesReady every 1000 / pre-cleanup / post-cleanup
    / summary @cnr3_free_filter); Cnr3MemoryStats accumulates 13 dynamic metrics min/avg/max + 3 peaks + baseline.
    DECISION: SEVEN metrics REMOVED from print+accumulation as misleading/duplicate (commit_limit +
    system_total_pagefile = elastic RAM+pagefile mislabeled as fixed total; system_total_virtual = 128TB const;
    perf_physical_total = dup of system_total_phys; system_avail/used_pagefile = commit-family not pagefile;
    process_pagefile_usage = dup of process_private_usage) -> dynamic set 16->13, retained as raw sampled fields only.
    DECISION: the ONE ungated production change (R-PROCESS-21/25) is explicit output_cache.clear() in cnr3_free_filter
    (status-captured + (void)'d, never aborts teardown, fail-safe -- declines if any slot pinned) to enable
    post-cleanup measurement; the diagnostic observes around it; pins proven zero at teardown (D-SUM-04 balance==0 same
    run). Memory spec advanced to v3.4. ARC CLOSED: all 14 D-SUM families live.
(b) PRUNE-RECHURN RECENCY-GATE COMMITTED (analysis-track; CMS07-DIAG.prune-rechurn-recency-gate). FINDING (the
    honesty-filter precedent): the D-SUM-10 frames_evicted_then_re_requested counter was MISLEADING -- it counted any
    ring match regardless of eviction age, so it was dominated by INTENDED far-revisits (read 481 on S9, all gap
    101-1000), not a cache fault. VERIFIED COLD: the ring lookup matches the EXACT frame, so requested==evicted and
    frame-number locality is vacuous; the only meaningful discriminator is eviction-recency (eviction_gap). DECISION:
    REPLACE (not keep alongside) with frames_recently_evicted_then_re_requested gated on
    eviction_gap <= CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP = 3*BACK_RADIUS. Z DERIVATION (data-driven): across
    S9/S9c/S9d/S9e every event is recovery-local (gap<=50) OR far-revisit (gap>=101), with 51-100 EMPIRICALLY EMPTY in
    every run -- 3xBACK_RADIUS (45 TINY / 150 NORMAL) sits in that valley, catching recovery-local, rejecting far
    revisits. Full ring scan / gap histogram / top-thrashers UNCHANGED (counter free-rides the already-computed
    eviction_gap). Proven S9/S9c/S9d=0, S9e=40 (both directions). DEFERRED (do not bundle): split
    observe_lookup_miss_rechurn_locked into scan + per-purpose observers (own future scope + byte-identical proof).
(c) (a)/(b) COUNTER CLASSIFICATION + DERIVED-HEALTH-RATIOS COMMITTED (analysis-track;
    CMS07-DIAG.derived-health-ratios). Every emitted counter was sorted list-(a) [cheap in-plugin scalar ratio] vs
    list-(b) [needs plan-trace parse / cross-family join -> A1], through an "is it meaningful / non-misleading?" filter.
    RESULT: 8 ratios emitted in a new additive [DSUM-HEALTH] end-of-run block (cache_hit_and_supplied_percent;
    cache_miss_recovery_plan_percent; recovery_plan_holes_filled_percent; return_to_vs_success_percent;
    recent_rechurn_vs_evicted_percent; frames_per_prune_event [average]; recalc_multiple_vs_recalced_frames_percent;
    recalced_frames_vs_total_percent). DECISION on denominators: #9/#10 use the DISTINCT-FRAME buckets
    (frames_recalculated_once + _multiple), NOT the recalculated_frame_count EVENT counter (mixing frames with events
    misreports). THREE candidate ratios REJECTED as misleading and routed to A1: recovery-search-hit
    (search_successes/search_attempts ~100% "a plan was formed" -- both exact-anchor and floor count as success; real
    signal is depth distribution + exact:floor split -> A1); hole-source-retrieval (retrieved/requested cross-LIFECYCLE
    -- requested is +hole_count at plan-publish, retrieved is +1 per-hole-executed; conflates "upstream failed" with
    "plan bailed" -> A1); recalc-and-stored (cross-FAMILY D-SUM-13 events vs D-SUM-07 store-time discards = the A1
    "L = adopted_post_compute_loser" rate -> A1). DECISION on honesty: zero denominator emits "n/a" (phenomenon did not
    occur, undefined) DISTINCT from a genuine 0.000 rate. Wiring Option A: fresh final QUIESCENT snapshots at teardown
    via existing helpers (clear() is D-SUM-10 counter-neutral -> health read matches family summary); no existing
    summary writer refactored. Per-ratio all-operands-live gating; "(source D-SUM-NN disabled)" markers; footnote
    records the #7->D-SUM-10 provenance + n/a convention. PROVEN: macro-on hand-check all 8 == raw counters (S8 576p50:
    534/800=66.750, (177+4)/800=22.625, 168/21=8.000, etc.); n/a (#9, 0/0) vs 0.000 (#10, 0/800) both shown; macro-off
    gates-out; macro-off BYTE-IDENTICAL frame output (fc /b, 497,668,851 bytes each); 56/56.
(d) PROCESS: CODER-CHAT CAUTION banked -- a coder chat produced two consecutive mangled run-instruction sets (invented
    run-folders / non-canonical paths) after a long reliable run; evaluate coder responses against known-good baselines
    until stable. The canonical 4-way build/run is ratified as Document A R-PROCESS-26 (fixed folder layout
    ...\github\vs\cnr3\x64\{Debug,Release}; cd + relative paths; console output; no run-folders/exe-copies). Two TINY
    log-string cleanup candidates banked (misleading "SKIPPED under ...TINYCACHE..." message; stale D-SUM-14 "tiny
    profile interval=3" note printing on NORMAL runs) -- observation-clarity backlog, not scheduled.
FORWARD: A1 (plan-trace tool; input set includes the 3 rejected ratios) / A2 (fmParallel = C1-under-race gate) / A3
(real 576p50 via A1). CMS design UNCHANGED at 07.15 (all three commits diagnostic-only). ***

*** v1.7 ADVANCE (2026-07-08): DIAG.3c.2 COMMITTED (dump-on-bail + failure detail); DIAG.3c plan-trace family CLOSED;
DIAG.4 memory is the last in-plugin step. DECISIONS banked:
(a) DIAG.3c.2 COMMITTED + PUSHED, marker CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail. Adds, on FAILURE
paths only: Set 5 fail_reason (16 categories, assigned per bail SITE by SOURCE LOCATION not runtime message-parse);
Set 4 local X=not_reached / E=error_here (plan-trace-local, production Cnr3LiveRecoveryHoleOutcome enum untouched);
a once-guarded bail dump sharing the 3c.1 clean-end path (exactly one block per run, bail OR clean-end). Per-site
ADDITIVE FAILED writes at all 65 cnr3_set_filter_error sites (14/50/1). Proof CLOSED: all-on four-way 57/57 x3 +
56/57; macro-off 56/56 x3 + 55/56 (the whole family compiles out; 65 sites revert to the original
set_filter_error+return nullptr = R-PROCESS-19 exit gate); A/B plugin byte-identical S1/S7/S8 + legend-once + 2f
no-FAILED-on-clean.
(b) SITE-TO-CATEGORY TABLE (coder-produced, designer-verified cold): all 65 sites mapped to the 16 categories,
counts summed to 65. AI-06 is a genuine MIXED site (recovery-refusal path also discharges pins) -> resolved by a
control-flow category split (15 RECOVERY_PLAN_FAILED_OR_REFUSED vs 8 DISCHARGE_FAILED), never message-parsing.
(c) MECHANISM decisions: M1(a) per-site additive FAILED-writer calls + shared builders (NOT a widened
cnr3_set_filter_error signature -- the common helper lacks the state); M2 explicit once-guarded bail-arm dump (do
NOT rely on cnr3_free_filter running after a bail; R-PROCESS-24 wants bytes flushed before the failing return);
master plantrace gate only (no bail sub-gate); E = the ACTUAL failing frame everywhere (not forced to record frame
n) with X = unreached recovery-plan remainder (holes + target), X=[] for non-recovery.
(d) INDUCED-BAIL PROOF (the load-bearing runtime proof; coder-owned white-box selftest): arInitial minimal FAILED
(records=1, no O, outcome=FAILED, fail_reason=INVALID_LIFECYCLE, codes=[7=E], empty progress); recovery FAILED
(records=2 = O + FAILED R, RECOVERY_EXACT anchor=29 holes=30/31/32 target=33; outcome=FAILED,
fail_reason=SOURCE_RETRIEVAL_FAILED, codes=[30*=K,31=E,32=X,33=X], adopted_skipped=[30*]); once-guard verified
(clean-end after bail appends nothing).
(e) FINDINGS through the 3c.2 cycle: D3C2-1 (new FAILED-helper DEFINITIONS must be inside the plantrace #if so
macro-off compiles them out with no C4505 -- D3C1E-1 recurrence; confirmed, macro-off build proves it);
D3C2-2 (inserted #if/observe blocks re-indented proven set_filter_error lines -> restored to pure insertion, v2);
duplicate X/E legend line printed twice per block -> v3 overlay removed one (legend-once, R4); v4 induced-bail
selftest failed to LINK (selftest exe does not link the getFrame TUs: unresolved cnr3_arInitial /
cnr3_arAllFramesReady / cnr3_discard_frame_data_with_cache) -> v5 REFACTOR: extract the recovery FAILED-result
derivation into the shared inline helper cnr3_live_plantrace_make_failed_result_from_request (plugin_internal.h,
gated); live AR delegates to it and the selftest calls the SAME helper with a constructed frame_data. Trade-off:
this proves the derivation LOGIC through production code (better than a hand-built fixture, no drift) but is NOT a
live cnr3_arAllFramesReady invocation. STANDING (do not decay): the live bail-site -> helper state-plumbing is
diff-verified across the 65 sites; confirm at the first A-series REAL induced failure. (Rejected Option 1 = linking
the getFrame TUs into the unit-test exe, as it risks dragging plugin registration into the test binary; the failure
path is diagnostic-only and fc /b already proves no frame/ownership perturbation, so the derivation-logic proof +
diff-verified wiring is sufficient for commit.)
(f) DIAG.3c plan-trace family (3c.1 capture + emission, 3c.2 failure) is COMPLETE. ARC CLOSES at DIAG.4 (memory
D-SUM-02; CMS06-era memory-code salvage-assessment in-scope). Then A1/A2/A3. C1-under-race gate owed on A2; FI-11
in-run counter deferred-but-expected. ***

(Prior v1.6 ADVANCE retained as history:)

*** v1.6 ADVANCE (2026-07-07): DIAG.3c.1 COMMITTED (capture + emission refinement, one commit); plan-trace spec
v2.3; DIAG.3c.2 next. DECISIONS banked:
(a) DIAG.3c.1 COMMITTED + PUSHED (observe-only plan-trace capture + clean-end dump), marker
CMS07-DIAG.3c.1-plantrace-clean-end-capture. Proof gate CLOSED, verified cold: plugin macro-off BYTE-IDENTICAL
(fc /b, plantrace-off vs on) PASS S1/S7/S8 — the R-PROCESS-19 observe-only exit gate, proven on the actual
getFrame paths (the selftest could not reach them); four-way selftest all-on/macro-off/restored 56/56 x3 + 55/1;
content sanity on the single-line format (S1 control + S8 all four branches well-formed; BEGIN/END counts match;
truncated=0; no unpinned=; post_compute_discarded present). Capture (RAII O-guard; four branch-local R captures
copy-before-discard; enter_tick-outside-lock / action_seq-inside-lock) unchanged and re-proven observe-only.
(b) EMISSION REFINEMENT folded into 3c.1 (spec v2.2 -> v2.3): one physical ASCII line per record, fixed schema
(empty=[]); pad only enter_tick/exit_tick/seq/frame (role-list/codes unpadded); ONE natural-order block
(action_seq order) with the three in-plugin sort VIEWS REMOVED (VIEW_* sub-gates deleted) — redundant with, and
strictly weaker than, external sort/A1; legend once, column-aligned, O-item/R-item; per-instance BEGIN/END
schema=3c1v1 (window/records/truncated=reserve_failed) for self-description + truncation detection. Sort keys L->R:
instance (via CNR3[n] prefix), enter_tick, seq, frame, exit_tick. Rationale: real-output inspection + the A1
external-analysis commitment; the single-line-sortability intent was never hardened to "one physical line" through
v1->v2->v2.2 (accountability = spec, not coder).
(c) VOCABULARY: Set 4 L display label -> post_compute_discarded (source enum adopted_post_compute_loser
UNCHANGED). Set 4 U / unpinned DROPPED from the R record — near-universal proven-safe pin-discharge, no thrash
signal; pin accounting lives in D-SUM-04 (cache ref balance) and D-SUM-12 (plan lifecycle), not the plan-trace.
Supersedes the B.3.1 "unpinned as per-item fact" half; coordinator-approved under R-PROCESS-25.
(d) FINDING D3C1E-1 (build-safety, caught at emission-delta diff review): the first emission-refinement delta
removed the unpinned_frames population but left make_cache_hit_result / make_computed_result with now-unused
parameters (C4100 under /W4, build failure under /WX). v2 delta fixed it by DROPPING the dead params and updating
all call sites (no (void) suppression); predecessor_frame_for_trace stays live via the existing trace call.
Lesson: removing a field's producer can strand its now-dead upstream params — sweep for them.
(e) DIAG.3c.2 remains the invasive half (dump-on-bail + Set 4 X/E + Set 5 fail_reason over the 65 bail sites,
R-PROCESS-21/25); the source-line SITE-TO-CATEGORY TABLE is its foundation artifact + first coder confirm
deliverable. ARC then closes at DIAG.4 (memory D-SUM-02). Analysis track A1/A2/A3 follows (A2 = the C1-ownership-
under-race gate owed from 3b). FI-11 in-run counter DEFERRED-BUT-EXPECTED. ***

(Prior v1.5 ADVANCE retained as history:)

*** v1.5 ADVANCE (2026-07-06): DIAG.3b COMMITTED (proof gate closed); plan-trace spec v2.2 controlling; DIAG.3c.1
scoped + handed to coder. DECISIONS banked:
(a) DIAG.3b COMMITTED + PUSHED (D-SUM-06/07/09/14). Proof gate CLOSED, verified cold: six-config R-PROCESS-19
matrix GREEN (28 selftest runs, block-presence pattern exact, four-way identical 56/56 + 55/1 forced-fail single
named induced failure, zero count anomalies); S-series real-run (-r 1, S1/S3/S7/S8 = 10/120/800/800 frames, no
bail) all three balances (source_frame_release_balance / temporary_output_balance / lookup_ref_balance) == 0,
failure-category fields (same_activation_request_violations / partial_acquire_failures / promotion_mismatches) == 0,
D-SUM-07 equation created==stored+released+transferred exact (S8: 932=653+0+279), global all-family non-zero sweep
empty, normal profile. Committed with a SANCTIONED marker-only build_config touch (CNR3_EDIT_VERSION ->
CMS07-DIAG.3b-lifecycle-return-scene — 3b adds no gates, so the marker is the only build_config change, explicitly
coordinator-sanctioned as a scope deviation from "no build_config change"); commit message notes C1-C4, C-ALIAS, D-2.
(b) C1-OWNERSHIP-UNDER-RACE = REQUIRED FUTURE GATE (not a coverage note). Under -r 1 no first-in-best-dressed race
occurs, so temporary_outputs_released == 0 and duplicate_computed_but_discarded == 0 on all four S-series. The
D-SUM-07 RELEASED/discard arm — the path where a double-free or ambiguous owner would surface, which is EXACTLY
what C1/C-ALIAS reason about — is therefore NOT exercised by -r 1. C1 ownership MUST be re-accepted on the
fmParallel / -r >1 run; that run is the C1 acceptance gate, not a nice-to-have. What -r 1 DID prove: create/store/
transfer site-completeness under real recovery churn (any unaccounted create site would have driven the balance
non-zero; it stayed zero). Banked so the deferral cannot be mistaken later for full-arm coverage.
(c) FIDELITY-REVIEW CORRECTION D-V2-1 (plan-trace spec): the bail-site total is 65 CALL sites, not 66. AR raw
`cnr3_set_filter_error` grep of 51 INCLUDES the function DEFINITION at cnr3_arAllFramesReady.cpp:526 (plugin_
internal.h:111 is the declaration); AR call sites = 50, so 14 (arInitial) + 50 (AR) + 1 (top-level) = 65. Provenance:
the cross-check report's "51 sites plus the helper function definition" phrasing conflated raw-count with site-count;
spec v2 propagated it in good faith (its raw grep counts were correct). Lesson: raw-grep count != call-site count —
exclude definitions; the authoritative 3c.2 site-to-category table re-derives 65 from live source regardless.
(d) PLAN-TRACE SPEC v2.2 view (a) sort key (clarification of settled design, folded without re-review): sort key =
ordered tuple (enter_tick ASC, action_seq ASC); `phase` DROPPED as a sort term (remains displayed). Reasoning:
enter_tick primary gives the printed UTC column a monotonic read; action_seq is the globally-unique deterministic
tie-break that supersedes phase (phase leaves same-tick TT/RR ties among different frames unresolved); O-before-R
falls out of action_seq automatically. CAPTURE INVARIANT banked: enter_tick sampled OUTSIDE the diagnostics mutex
(moving it in serializes for zero benefit); action_seq bumped INSIDE the mutex in the same critical section as the
buffer append (moving it out reintroduces the fmUnordered read-increment-write collision and destroys uniqueness —
its one job as the tie-break). Label "sort by enter_datetime" tightened to "sort by enter-tick (display column: UTC)"
to prevent a string-sort-on-datetime misread. (spec progression v2 -> v2.1[D-V2-1] -> v2.2[this].)
(e) DIAG.3c SPLIT delineated at scoping (split decision itself deferred to the coordinator at the 3c.1 boundary):
3c.1 = buffered plan/result capture + clean-end dump, OBSERVE-ONLY (R-PROCESS-19), touches NO bail site; 3c.2 =
dump-on-bail + Set 4 E/X + Set 5 failure-reason writes across the 65 bail sites (invasive, R-PROCESS-21/25, the
site-to-category TABLE built from live source as its foundation artifact). 3c.1 SCOPED (scope v2, carrying the v2.2
view-(a) clarifications) and HANDED TO CODER (spec v2.2 + scope v2 + post-3b src); the 3b coder continues (continuity).
DESIGNER RECOMMENDATION: commit 3c.1 standalone, then 3c.2 as the next cycle (keeps the clean R-PROCESS-19 observe-
only proof unentangled from the 65 proven-path touches); the coordinator's "capture fails from day one" direction is
honored either way since 3c.2 follows immediately. FI-11 in-run ring<->recovery correlation counter DEFERRED-BUT-
EXPECTED — kept visible. ***

(Prior v1.4 ADVANCE retained as history:)

*** v1.4 ADVANCE (2026-07-04): DIAG.3a COMMITTED; DIAG.3b approved, proof-gate in progress. DECISIONS banked:
(a) DIAG.3a B1-B5: recovery_plans_created branch-keyed at the RECOVERY publish only (four publish sites exist,
three are non-recovery); destroy at the single cnr3_discard_frame_data_with_cache teardown (one delete site,
~51 routing callers) — the balance provable by construction. D-SUM-13 = fixed-capacity open-addressed table
(16000/1600, saturation-honest). In-run ring-correlation DEFERRED (FI-11).
(b) DIAG.3a DEFECT D-1: destroy observer defined but never invoked (selftest synthetically balanced its own
block, masking it). Lesson RATIFIED into review discipline: VERIFY INVOCATION, NOT JUST DEFINITION — trace the
production call graph. Fixed in v2 (one gated call before delete); proven by S-series (S8: 171/171).
(c) RECOVERY-RATE BASELINE (-r 1): S1 0% / S3 27.5% / S7 0.375% / S8 21.4% — recovery churn is arrival-disorder-
driven, not seek-driven. First FI-11 data point.
(d) DIAG.3b C1-C4 + C-ALIAS (see Doc B v3.16 banner for the decisions). The alias insight: on the pointer-
equality branch, freeFrame(output-named-variable) releases the SOURCE reference — classify as D-SUM-06 source
release, never D-SUM-07; alias/null copyFrame is never a creation. The dual-reference production pattern:
addFrameRef(output_frame) gives the cache its own ref while the original remains the return candidate — the
cache copy is OUTSIDE the temp-output balance (C1).
(e) DIAG.3b FINDING D-2: a pure-function hoist (frame-0 return condition) implemented WITHOUT prior proposal.
Behaviourally identical (allows_return is pure); retro-sanctioned to keep the record complete; the propose-
first sequence ratified as R-PROCESS-25 (Document A v3.13), with R-PROCESS-24 (flush-per-line) ratified same pass.
(f) SYNTHETIC-VS-REAL reading discipline: selftest reference emitters deliberately feed one-of-each failure-
category fixtures (violations=1 etc) to prove the writers; these are NOT anomalies; real-run acceptance is the
S-series zeros. Reviewers must separate the synthetic block from the real-run block when reading digests. ***


*** v1.3 ADVANCE (2026-07-04): DIAG.2b COMMITTED (was in-flight in v1.2). KEY DECISIONS banked this cycle:
(a) A1 — D-SUM-08 store_failures counts (status != ok && status != duplicate); duplicate is a healthy
first-in-best-dressed outcome eligible for authoritative return, NOT a failure.
(b) A2 — D-SUM-05 observes the central cache_state_invariants_hold_locked() via a DESIGNER-SANCTIONED
CNR3_DSUM05_FAIL(tag) macro on the 18 return-false sites (macro-off byte-identical); checks counted once at entry.
This is the R-PROCESS-21 mechanical-transformation precedent: a proven-code transform is permitted when
explicitly proposed and designer-approved.
(c) A3 — D-SUM-04 pin-list fields consciously omitted (Option A, avoids AS4 double-count); narrowing printed as a
summary note so the gate-comment/field mismatch self-documents.
(d) A5 — ownership_errors tripwire on the wrapper adoption-failure rebalance path only.
(e) D-SUM-04 completeness PROVEN empirically: S-series -r 1 balances zero under 168-eviction churn (S7/S8) — the
FI-12 missed-release-path risk is retired for the two narrow balances.
(f) Coder handled two unspecified subtleties unprompted: unlocked-context re-lock for wrapper-site observers
(no race), and multi-exit exactly-once store observation. DIAG.3a now scoped (D-SUM-03/12/13). ***

**Date:** 2026-06-27
**Status:** Companion record of the diagnostics discussion, the source-state findings, the agreed
sequencing/scope decisions, and WHERE each fact was found. This is a working record for the diagnostics
arc (NOW the NEXT arc: AFTER the COMPLETE W.1→W.2→W.3 live cache-pressure wiring arc, and BEFORE the real-footage campaign — the 2026-06-30 coordinator decision, which matches this record's §4.1 ordering). It is NOT a scope and NOT a CMS change — the diagnostics DESIGN is
already settled in the two specs cited below; what is captured here is the implementation-state findings
and the coordination decisions made on 2026-06-27.
**Controlling:** CMS07.15 / Production Spec v2.15 / Document A v3.11 / Document B v3.10 / slimmed DELTA v4.16. (v1.1 header reconcile, 2026-06-30: state advanced from the v1.0 D.5/52-52 baseline to the W.3-closed/55-55 seam; the DESIGN BODY of this record is UNCHANGED and durable — only these state/sequencing headers are refreshed.)
**Branch:** dev_cache_manager. Baseline (v1.1): committed through CMS07-W.3 (live cache-pressure wiring arc COMPLETE), 55/55. (v1.0 baseline was D.5/52-52.)

*** v1.2 ADVANCE (2026-07-04): the diagnostics arc has EXECUTED through DIAG.2a. Committed + pushed: selftest skip-pass fix (honest default 56/56), DIAG.1 (framework + D-SUM-01 + R-PROCESS-19 observe-only proof), DIAG.2a (D-SUM-11 hot-zone writer + D-SUM-10 prune/eviction/re-churn). Baseline now post-DIAG.2a, 56/56 default config. KEY DECISIONS since v1.1: (a) [DSUM-SUMMARY] is the diagnostics summary tag, distinct from Keystone [KDT-SUMMARY] (a collision cost a debugging cycle in DIAG.1). (b) R-PROCESS-19 observe-only proof (macro-off byte-identical) is the arc's exit gate; DIAG.2a additionally proved INDEPENDENT gate matrices (10off/11on, 11off/10on). (c) Option A mutable-diagnostic-member pattern for const-method observation (consistent with mutable cache_mutex_). (d) DIAG.2a S7/S8 finding -> FI-11: D-SUM-10 re-churn hooks the predecessor-lookup path; the costly evict-then-rebuild churn is on the recovery/anchor path, which is D-SUM-12's job (DIAG.3). (e) DIAG.2b D-SUM-04 re-sited (coder inventory review, verified): global VSFrame ref balance is unprovable (RAII releases via Cnr3OwnedFrameRef reset/dtor/transfer_to_caller); use two narrow provable balances (pin balance + lookup-ref handoff invariant) per the build_config gate comment -> FI-12 records the deferred broad detector. (f) FI-13: production-dup checkpoint-promotion signal not exposed; D-SUM-08 counts AS2 promotions only. ***

---

## 0. Why this doc exists

The coordinator recalled "a lot of prior discussion" about compile-time-gated concise telemetry (watching
hot zones, pruning, hole-filling — needed under fmParallel) and asked whether it was recorded or only in
transcripts. It IS recorded — in the diagnostics spec. This doc consolidates: (1) where the design lives,
(2) what is actually in the source today vs what is owed, (3) the agreed sequencing and coder-prep
decisions, and (4) the provenance (where each fact was found) so nothing has to be re-derived from memory.

---

## 1. The diagnostics DESIGN is fully specified (not lost in transcripts)

The compile-time gating mechanism the coordinator remembered is settled and documented.

### 1.1 Where the design lives
- **`cnr3_diagnostics_specification_v1_5.md`** — the master diagnostics design:
  - **§2.3 "Observation gates observe only"** — the per-summary COMPUTE/PRINT compile-time gate pattern.
  - **§2.3.1** — the R-PROCESS-19 compute-disabled observe-only proof obligation.
  - **§4** — the 14-family D-SUM catalogue (D-SUM-01..14), each with purpose / activation / fields /
    human interpretation.
  - The FAIL / WARN-investigate / INFO severity model.
  - The per-frame `[KDT]` line vs end-of-run `[KDT-SUMMARY]` distinction.
- **`cnr3_memory_diagnostics_spec_v2.md`** — the memory-diagnostics design (D-SUM-02 specifically).

### 1.2 The compile-time gate pattern (the "ifdef decision" — found at spec §2.3, lines ~84-152)
Each diagnostic summary has TWO independent gates, COMPUTE and PRINT, with PRINT subordinate to COMPUTE
and a paired `#error` cross-check making "print-on / compute-off" a COMPILE failure:

```cpp
#define CNR3_DIAG_COMPUTE_DSUMxx_NAME 1            // comment out to disable COMPUTE
#if defined(CNR3_DIAG_COMPUTE_DSUMxx_NAME)
#   define CNR3_DIAG_PRINT_DSUMxx_NAME 1           // print only possible if compute is on
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUMxx_NAME) && !defined(CNR3_DIAG_COMPUTE_DSUMxx_NAME)
#   error "Cannot print DSUMxx_NAME without computing DSUMxx_NAME"
#endif
```

So: comment a `#define` to turn that telemetry off entirely (zero cost when off); the `#error` makes the
print-without-compute mistake impossible to compile. This is the recorded answer to "how that had to occur
with ifdef or something."

### 1.3 Selectively-gated per-family telemetry (the "watch it bubble along" requirement)
Each of the 14 families has its OWN independent gate pair, so any subset can be turned on alone — e.g.
just hot-zone (D-SUM-11), just prune (D-SUM-10), just recovery/hole-filling (D-SUM-12). This is exactly
the selective observation needed to watch hot zones, pruning, and hole-filling — and to isolate behaviour
under fmParallel. The per-frame `[KDT]` concise line is the "bubbling along" view; the end-of-run
`[KDT-SUMMARY]` D-SUM blocks are the verification view.

### 1.4 The observe-only guarantee
**R-PROCESS-19 (D-SUM compute-disabled observe-only proof)** — turning a compute gate OFF must not change
program behaviour or output frames. This is a register-owned rule (Production Spec §3A / Document A §3A);
the diagnostics spec §2.3.1 ties the gates to it. It keeps diagnostics from ever affecting correctness.

### 1.5 The 14 D-SUM families (found at spec §4, lines 368-381)
```
D-SUM-01  Frame request arrival / ordering summary
D-SUM-02  Memory diagnostics summary
D-SUM-03  Recovery-search summary
D-SUM-04  Ownership / pin / lookup-ref balance summary
D-SUM-05  Cache integrity / teardown summary
D-SUM-06  Source-frame request / retrieve / release summary
D-SUM-07  Temporary-output / owned-output-ref lifecycle summary
D-SUM-08  Cache store / duplicate-store / first-in-best-dressed summary
D-SUM-09  Return-decision / return-transfer summary
D-SUM-10  Prune / eviction safety summary
D-SUM-11  Hot-zone operation summary
D-SUM-12  Recovery planning / hole-filling summary
D-SUM-13  Recalculation histogram
D-SUM-14  Scene-change / recursive-reset / checkpoint-promotion summary
```

---

## 2. What is actually in the SOURCE today (findings from src.zip, 2026-06-27)

The diag files are SHELLS / scaffolds, not implementations. Verified line counts and content:

| File | Lines | State |
|------|-------|-------|
| `cnr3_diagnostics.h` | 92 | generic stderr-output boundary declaration (CMS07-B.2.4) |
| `cnr3_diagnostics.cpp` | 41 | minimal generic output core |
| `cnr3_cache_diagnostics.h` | 183 | **only the D-SUM-11 hot-zone COUNTER MODEL** — `struct Cnr3CacheHotZoneDiagnosticStats` + saturating-increment observers. Header comment: "introduces only the D-SUM-11 hot-zone counter model. It does not format or print summaries..." |
| `cnr3_cache_diagnostics.cpp` | 10 | reserved stub ("reserved for later cache-specific summary formatting") |
| `cnr3_memory_diagnostics.h` | 34 | scaffold (CMS07-B.2.5) |
| `cnr3_memory_diagnostics.cpp` | 10 | explicit PLACEHOLDER — "Memory sampling and D-SUM-02 accumulation/printing will be added only in a later explicit memory-diagnostics implementation phase." |

**Finding:** of the 14 specified families, exactly ONE (D-SUM-11) has a counter model, and NONE have
end-of-run formatting/printing. The diagnostics are a declared CONTRACT with stub bodies. What is OWED is
IMPLEMENTATION, not design.

**The D-SUM-11 counter model** (already present, `cnr3_cache_diagnostics.h`): a pure counter snapshot —
no formatting, printing, heap strings, cache-mutation authority, frame ownership, or control-flow. Counter
updates may occur inside cache-lock scopes as minimal observations; summary FORMATTING must be implemented
later, outside all cache locks. This is the template the other families' counter models should follow.

**Memory diagnostics salvage** (coordinator-supplied fact): the deprecated-but-mostly-useful memory-diag
implementation is ARCHIVED in the GitHub repo. It is a strong salvage reference for D-SUM-02 — but it is
DEPRECATED: it predates CMS07, the D-SUM gate framework, R-PROCESS-19, and the print-subordinate-to-
compute discipline. So it is adapt-to-the-current-gate-pattern-and-prove-observe-only, NOT paste-in.
"Mostly salvageable" yes; "drop in as-is" no.

---

## 3. Why the clip-test harness needs the diagnostics (finding)

The real-footage clip-test harness — `test_000_Example_576p50.vpy` / `.bat` (runs 576p50 through the live
plugin to NUL or to an ffmpeg encode) — has little verification value WITHOUT the D-SUM summaries +
selectively-gated concise telemetry in place. A bare run only shows it did not crash; it does not show
whether pin balance held (D-SUM-04), recovery fired correctly (D-SUM-12), integrity stayed clean
(D-SUM-05), prune stayed safe (D-SUM-10), or scene-change/checkpoint-promotion behaved (D-SUM-14) across
thousands of real frames. Therefore the large clip-test CAMPAIGN is sequenced AFTER the diagnostics arc.

---

## 4. DECISIONS AGREED (2026-06-27)

### 4.1 Sequencing (coordinator decision)
P.11C FIRST, then the diagnostics arc, then the campaign:
```
P.11C scene-change calc (synthetic-proven, like D.1-D.5; real-footage validation deferred)
  -> Diagnostics arc (D-SUM families + compute/print gates + per-family R-PROCESS-19 observe-only proofs;
                      includes D-SUM-14 scene-change telemetry and D-SUM-02 memory via salvage;
                      includes the end-of-run integrity report + abort_on_error + warn-vs-hard-fail
                      severity policy — these are PART OF this arc, not separate)
  -> first verifiable real-footage run + the large 576p50 campaign
  -> fmParallel arc (telemetry families now available to watch concurrency)
```
Rationale: P.11C is the last piece of pipeline CORRECTNESS; finishing it first means the diagnostics arc
instruments a complete pipeline rather than a moving target. (Trade-off acknowledged: P.11C is therefore
proven on SYNTHETIC footage; its real-footage validation folds into the campaign once diagnostics exist.)

### 4.2 D-SUM-14 belongs to the diagnostics arc, not P.11C
D-SUM-14 (scene-change / recursive-reset / checkpoint-promotion summary, spec §4 / detailed at spec
~line 1718) is the family that will OBSERVE P.11C on real footage. It is implemented in the diagnostics
arc, NOT bundled into P.11C. This keeps P.11C smaller (wiring + checkpoint promotion only) and keeps the
telemetry with the rest of the diagnostics work.

### 4.3 Core-subset choice is DEFERRED pending a 2-liner menu (Claude-owed first step of the arc)
The coordinator does not yet have enough per-family info to choose which D-SUM families form the core
implementation subset vs deferred. FIRST STEP of the diagnostics arc (Claude-owed): produce a concise
2-line summary of EACH of the 14 families (purpose + what it gates/observes) so the coordinator can choose
the core subset. Candidate core (for that discussion, not yet decided): the verification set D-SUM-04
(ownership/pin balance), D-SUM-05 (integrity/teardown), D-SUM-10 (prune safety), D-SUM-12 (recovery
planning); the watch set D-SUM-11 (hot-zone, already has its counter model); D-SUM-14 (scene-change);
D-SUM-02 (memory, via salvage); plus the severity / abort_on_error policy. The arc is implementable
incrementally — family by family, each with its own gates and its own R-PROCESS-19 observe-only proof.

### 4.4 Coder preparation (recorded; ACTION AT DIAGNOSTICS-ARC KICKOFF, NOT during P.11C)
When the diagnostics arc kicks off (after P.11C), the coder's restart/scope package includes:
- `cnr3_diagnostics_specification_v1_5.md`
- `cnr3_memory_diagnostics_spec_v2.md`
- a pointer to the ARCHIVED deprecated memory-diag code in GitHub
explicitly framed as: **orientation + salvage reference; the gate framework (spec §2.3) and R-PROCESS-19
observe-only proof govern; adapt, don't paste; await per-family scope.** Per R-PROCESS-08 / R-ARCH-05/07,
the coder reads early but does NOT implement until the diagnostics phase is scoped + approved. This
prepares the coder early (the memory-diag is mostly salvageable and readily implemented once adapted)
WITHOUT breaking the propose-review-approve discipline. It is recorded as a kickoff step rather than
actioned now, so handing the coder diagnostics material does not muddy P.11C as the live task.

---

## 5. Provenance (where each fact was found)

| Fact | Source location |
|------|-----------------|
| Compile-time COMPUTE/PRINT gate pattern + `#error` cross-check | diagnostics spec §2.3, lines ~84-152 |
| R-PROCESS-19 observe-only proof obligation | diagnostics spec §2.3.1; rule owned in Prod Spec §3A / Doc A §3A |
| The 14 D-SUM families | diagnostics spec §4, lines 368-381 |
| D-SUM-14 detailed catalogue entry | diagnostics spec ~line 1718 |
| `[KDT]` per-frame vs `[KDT-SUMMARY]` end-of-run | diagnostics spec, lines ~10, ~292 |
| D-SUM-11 hot-zone counter model (only implemented family) | `cnr3_cache_diagnostics.h` (183 lines), `struct Cnr3CacheHotZoneDiagnosticStats` ~line 42 |
| Diag files are shells (line counts) | src.zip: cnr3_diagnostics.{h,cpp}, cnr3_cache_diagnostics.{h,cpp}, cnr3_memory_diagnostics.{h,cpp} |
| Memory-diag is an explicit placeholder | `cnr3_memory_diagnostics.cpp` ~line 4 ("CMS07-B.2.5 memory diagnostics placeholder") |
| Memory-diag deprecated code archived in GitHub | coordinator-supplied |
| D-SUM-02 memory design | `cnr3_memory_diagnostics_spec_v2.md` |
| Clip-test harness | `test_000_Example_576p50.vpy` / `.bat` (uploads) |
| Sequencing + coder-prep decisions | this session (2026-06-27); recorded in slimmed DELTA v4.12 §5 |

---

## 6. Cross-reference

The binding ledger entries for all of the above live in the slimmed DELTA v4.12 §5 (OWED-ITEMS LEDGER):
the "DIAGNOSTICS ARC — sequenced AFTER P.11C" entry (with the 2-liner first step and the CODER PREP
sub-note) and the "CLIP-TEST HARNESS depends on the diagnostics" entry. This companion doc is the
expanded record; the DELTA is the binding ledger.

---

*End of CNR3 Diagnostics Arc: Findings, Decisions & Provenance v1.0.*
