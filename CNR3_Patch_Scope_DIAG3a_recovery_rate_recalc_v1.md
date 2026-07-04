# CNR3 PATCH SCOPE — DIAG.3a v1: D-SUM-03 recovery-search + D-SUM-12 recovery-plan/rate + D-SUM-13 recalculation

**From:** designer/reviewer (W3D), via coordinator (W3X).
**Type:** formal patch scope (R-PROCESS-19 observe-only proof = exit gate; R-PROCESS-21 additive-only).
**Baseline:** current post-DIAG.2b committed source (verified: CNR3_DSUM05_FAIL / ownership_diag_stats_ /
observe_store_outcome_locked present). Gates for all seven DIAG.3 families verified present in
cnr3_build_config.h with the two-gate #error pattern; consume UNCHANGED, add nothing.

## 0. BATCHING (coordinator decision, tabled per standing preference)

The condensed plan lists DIAG.3 as ONE batch of seven families (03/06/07/09/12/13/14). Designer
recommendation: **split into DIAG.3a + DIAG.3b**, this scope being 3a.

```text
DIAG.3a (THIS SCOPE): D-SUM-03 + D-SUM-12 + D-SUM-13 — the recovery/recompute/churn trio.
  These three share one domain (the recovery path) and together ANSWER FI-11 (the coordinator's
  inherent-vs-tunable churn question). Highest-value first.
DIAG.3b (next scope): D-SUM-06 + D-SUM-07 + D-SUM-09 + D-SUM-14 — the lifecycle balances
  (source/temp-output/return-transfer) + scene-reset family.
RATIONALE: seven greenfield families in one patch is >2x the DIAG.2b review size (3 families,
  1194 lines); two batches keep each review cycle tractable and put the FI-11 answer first.
  If the coordinator prefers ONE batch, the 3b families append to this scope as a v2 — say so
  before the coder starts.
```

## 1. Architecture — the D-SUM-01 pattern (NOT the DIAG.2 cache-member pattern)

These families observe the getFrame path, not the cache core. Follow D-SUM-01's proven structure:

```text
- STATE: one gated stats struct per family in cnr3_diagnostics.h (like Cnr3DiagDsum01RequestOrderStats),
  instantiated in the filter INSTANCE DATA (cnr3_plugin_internal.h, alongside dsum01_request_order).
- SYNC: getFrame runs on multiple threads (fmUnordered) — use the SAME per-instance diagnostics-only
  mutex mechanism D-SUM-01 uses (coder: confirm the exact D-SUM-01 mechanism from source and mirror it;
  do NOT invent a new one). Observe under the diag mutex; snapshot; format+write stderr OUTSIDE.
- OBSERVE HELPERS: free functions taking stats& (cnr3_diag_dsum03_observe_..., etc), gated by each
  family's COMPUTE macro; called from arInitial/arAllFramesReady at the sites in §2-§4.
- SNAPSHOT + WRITER: snapshot struct + snapshot fn (compute gate); writer in cnr3_diagnostics.cpp
  (print gate), [DSUM-SUMMARY] tag, interpretation line from the gate comment, flush at writer end.
- EMISSION: cnr3_free_filter in numeric order (01, 03, 04, 05, 06*, 07*, 08, 09*, 10, 11, 12, 13, 14*)
  (* = when their batch lands). Selftest: synthetic reference emitters per the established pattern.
- FILES expected: cnr3_diagnostics.{h,cpp}, cnr3_plugin_internal.h, cnr3_arInitial.cpp,
  cnr3_arAllFramesReady.cpp, vapoursynth-Cnr3.cpp, cnr3_cache_core_selftest_main.cpp.
  NO cache-core change expected for 3a (all observation is getFrame-side). NO build_config change.
```

## 2. D-SUM-03 — RECOVERY_SEARCH (gate-comment fields, verbatim)

```text
FIELDS (build_config gate comment is the spec):
  search_attempts, search_successes, search_failures, depth_histogram,
  terminated_on_present_output, terminated_on_frame0, terminated_on_bound,
  terminated_on_failure, holes_filled.
HOOKS (arInitial, the bounded recovery search — plan_bounded_recovery_search_and_record_anchor_pin
  and the plan-acceptance functions cnr3_exact_anchor_recovery_plan_is_accepted /
  cnr3_floor_fresh_start_recovery_plan_is_accepted, ~cnr3_arInitial.cpp:128-217 region):
  - attempt counted once per bounded search invocation;
  - termination reason from the search outcome (present-output found / frame0 / bound B hit / failure);
  - depth = how far back the search walked before terminating (histogram bins mirroring the
    D-SUM-01 gap-histogram bin style; depth relates to B=50 so bins like 1/2-5/6-15/16-30/31-50);
  - holes_filled reconciled at arAllFramesReady (holes actually computed for accepted plans).
INTERPRETATION (gate comment): deep search is not automatically bad; repeated deep search may indicate
  retention/prune/workload pressure; denominator mismatches or bounded-start honesty failures are serious.
```

## 3. D-SUM-12 — RECOVERY_PLAN + RECOVERY-RATE (the FI-11 answer)

```text
FIELDS part 1 (gate comment, verbatim):
  recovery_plans_created, recovery_plans_destroyed, recovery_plan_balance,
  nearest_present_output_found, holes_identified, holes_filled,
  source_frames_for_holes_requested, source_frames_for_holes_retrieved,
  fallback_failures, bounded_start_honesty_failures.
FIELDS part 2 (DESIGNER ADDITION per Condensed Plan v1.5 churn-field priority — coder cross-check):
  frames_total, frames_cache_hit, frames_pred_present, frames_frame0,
  frames_recovered_exact, frames_recovered_floor,
  recovery_rate_percent (recovered / total, derived at write time),
  recovery_span_sum / recovery_span_max (span = target - anchor for EXACT; floor restarts counted
  separately), mean derived at write time.
  THIS is the counter that answers FI-11 / inherent-vs-tunable: S1 in-order should read ~0%;
  S7/S8 give the deterministic -r 1 recovery-rate baseline per arrival pattern; the num_threads>1
  comparison later isolates concurrency-specific churn.
HOOKS: arInitial branch decision (the five-way strategy: cache_hit / frame0 / predecessor_present /
  recovery exact_anchor / recovery floor_fresh_start) for the rate fields; plan create/destroy at the
  recovery-plan lifecycle in arInitial + the frameData holder teardown; holes identified at plan
  construction, holes filled reconciled at arAllFramesReady (the Cnr3LiveRecoveryHoleOutcome walk).
FI-11 CORRELATION (evict-then-rebuild): DIAG.3a baseline = EXTERNAL correlation — D-SUM-12's
  recovered-target/anchor/span data is read alongside D-SUM-10's [DSUM10-RING-*] evicted-frame dumps
  offline; no runtime cross-family read, no gate coupling. OPTION (coder assess feasibility only, do
  not implement unless trivial AND cleanly gated behind BOTH DSUM10 and DSUM12): a read-only
  "recovered target was in the recently-evicted ring" counter. If non-trivial: defer, note in report.
```

## 4. D-SUM-13 — RECALCULATION (bounded per-frame map — the one data-structure decision)

```text
FIELDS (gate comment, verbatim):
  recalculated_frame_count, recalculation_depth_histogram, max_recalculation_depth,
  frames_recalculated_once, frames_recalculated_multiple_times.
HOOK: a frame is "recalculated" when it is COMPUTED again after having been computed before in this
  instance — sources: recovery hole recompute of a previously-computed frame, and the
  duplicate/post-compute-loser path (computed but lost the race). Count at the compute-completion
  sites in arAllFramesReady (coder: enumerate the exact compute-completion points; the
  Cnr3LiveRecoveryHoleOutcome::computed and adopted_post_compute_loser outcomes are the anchors).
DATA STRUCTURE (the DIAG.2a ring lesson applies): per-frame compute counts need a BOUNDED container.
  Requirement: derived capacity (same self-derivation discipline as the D-SUM-10 ring — from
  active_ceiling and B, k multiplier), preallocated, SATURATION-HONEST (a saturated flag printed in
  the summary: "counts are lower bounds"). Coder proposes the concrete container (bounded flat map /
  open-addressed array keyed by frame) in the confirm report BEFORE patching. No unbounded std::map
  growth on long clips.
INTERPRETATION (gate comment): some recalculation expected under out-of-order stress; recalculation
  with clean ownership is not failure; deep/repeated recalculation may indicate retention/prune problems.
```

## 5. Cross-cutting (all three)

```text
- OBSERVE-ONLY (R-PROCESS-19): each family's COMPUTE macro off => struct/hooks/writer compile OUT;
  behaviour byte-identical; independent gating. No hook alters any return, status, plan decision,
  or control flow. The bail paths (~50 setFilterError sites) are NOT touched in DIAG.3a (that is the
  plan/result plan-trace family's territory, separate decision).
- ADDITIVE (R-PROCESS-21): hooks at existing sites; no restructure of arInitial/arAllFramesReady
  logic. If clean hosting needs a control-flow change, STOP and propose (the DIAG.2b A2 precedent:
  designer can sanction a mechanical transformation, but it must be proposed first).
- The multi-exit lesson (DIAG.2b §3.2): where a hooked function has multiple outcome-known exits,
  observe at every exit exactly-once (flag pattern) rather than a single end hook that misses
  early returns. Applies especially to the arInitial branch decision and the search termination.
- [DSUM-SUMMARY] tag; flush-per-line default + explicit flush at writer end; no stderr inside any lock.
```

## 6. Deferred / boundaries

```text
- D-SUM-06/07/09/14 -> DIAG.3b (unless coordinator chooses one batch).
- Plan/result plan-trace family: SEPARATE spec (CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_
  Spec_v1.md) — goes to the coder for sensibility/gap cross-check ALONGSIDE this scope (§8 item 5);
  implementation is NOT in DIAG.3a.
- FI-12 (primitive ref balance), FI-13 (production-dup promotion): unchanged, out of scope.
- The in-run FI-11 ring-correlation counter: optional, feasibility-assess only (§3).
```

## 7. Proof gate

```text
1. Four-way, default config, all gates ON: 56/56 / 56/56 / 55/56 exit 1 / 56/56; D-SUM-03/12/13
   [DSUM-SUMMARY] blocks emit (selftest synthetic reference + real fields at free-filter).
2. R-PROCESS-19 matrix (five configs): all ON; 03 off; 12 off; 13 off; all three off. Each: clean
   build, four-way identical, family block absent when off. Temporary build_config edits not committed.
3. S-series -r 1 (S1/S3/S7/S8) EXPECTED SHAPES (the D-SUM-12 rate is the headline):
   S1: recovery_rate ~0% (in-order control; frames_pred_present dominates), recalc ~0.
   S3: small in-window recovery (exact anchors, short spans <= zone size), rate low.
   S7/S8: the deterministic recovery-rate baseline — meaningful exact/floor split, spans vs B=50,
   recalc counts consistent with the 168-eviction churn; D-SUM-13 saturation flag FALSE.
   Read D-SUM-12's recovered targets against D-SUM-10's ring dumps = the FI-11 external correlation.
4. Prior families (01/04/05/08/10/11) unchanged: balances still zero, violations zero, matrix intact.
```

## 8. CODER BRIEF — investigate + confirm BEFORE patching (the DIAG.2b discipline)

```text
1. Re-validate baseline is post-DIAG.2b (CNR3_DSUM05_FAIL, ownership_diag_stats_,
   observe_store_outcome_locked present) and the 03/12/13 gates exist, families greenfield.
2. D-SUM-01 pattern: confirm from source exactly where its stats live, how they are synchronized
   under fmUnordered (the per-instance diag mutex mechanism), and mirror it — report the mechanism.
3. D-SUM-03: enumerate the bounded-search invocation + termination points (all outcome exits) and
   confirm the depth metric is cleanly derivable; propose the depth histogram bins.
4. D-SUM-12: confirm the plan create/destroy lifecycle points balance (every created plan destroyed
   on every path incl. bails); confirm the five-way branch decision site for the rate fields; confirm
   holes_identified vs holes_filled reconciliation is observable without touching bail control flow;
   ASSESS (do not implement unless trivial) the optional DSUM10+DSUM12 ring-correlation counter.
5. D-SUM-13: enumerate the compute-completion sites; PROPOSE the bounded per-frame container
   (capacity derivation, saturation honesty) for designer approval in the confirm report.
6. PLAN/RESULT SPEC CROSS-CHECK (parallel item, report separately): read
   CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v1.md and cross-check for sensibility +
   gaps against current source: the five enum sets vs the real branches/outcomes, the O-item role
   completeness, E/X feasibility at the ~50 bail sites, the ~13 failure-reason categories vs the
   actual messages, the buffered-block/dump-on-bail architecture, from/to compile-time vs runtime.
   This is REVIEW ONLY — no plan-trace implementation in DIAG.3a.
7. Report findings + confirmations, then generate the DIAG.3a patch against the current baseline.
```
