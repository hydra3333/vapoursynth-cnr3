# CNR3 PATCH SCOPE — DIAG.2b v2: D-SUM-04 ownership-balance + D-SUM-05 cache-integrity + D-SUM-08 store/duplicate

**From:** designer/reviewer (W3D), via coordinator (W3X). **v2 supersedes v1.**
**Type:** formal patch scope (R-PROCESS-19 observe-only proof = exit gate; R-PROCESS-21 additive-only).
**v2 CHANGES (from the coder's pre-patch inventory review, all verified against current source):**
  1. D-SUM-04 RE-SITED — not a global VSFrame ref balance (unprovable in DIAG.2b: RAII releases via
     Cnr3OwnedFrameRef reset/dtor/transfer_to_caller are outside hookable call sites). Instead TWO narrow
     provable balances matching the build_config.h gate comment's own field names.
  2. D-SUM-05 RE-SITED — hook the single central cache_state_invariants_hold_locked() (verified: called at
     8+ sites), not the ~8 scattered invariant_violation returns.
  3. D-SUM-08 CORRECTED — read Cnr3CombinedStoreAndPruneSummary at the combined store/prune WRAPPER level
     (after store outcome known), not inside DIAG.2a's prune-execution observer; as2_checkpoint_promotions
     only (production-dup promotion not exposed in the summary — honest labelling).
**Baseline:** current post-DIAG.2a source (verified: prune_diag_stats_ + re-churn ring present; DSUM04/05/08
gates present with two-gate #error; all three families greenfield). **DIAG.2b = 04+05+08 bundled.**

---

## 1. D-SUM-04 — OWNERSHIP_BALANCE (v2: two narrow provable balances)

DESIGN INTENT (from the build_config.h DSUM04 gate comment, verbatim): fields pins_acquired, pins_released,
pin_balance, lookup_refs_acquired, lookup_refs_released, lookup_refs_transferred, lookup_ref_balance,
pin_list_records, pin_list_discharges, pin_list_balance. Interpretation: "pin_balance and lookup_ref_balance
must be zero after drain; acquired == released + transferred is the lookup-ref invariant."
So the ORIGINAL design was always two narrow balances — v1 wrongly invented a global ref balance. v2 restores intent.

```text
BALANCE A — SLOT PIN BALANCE (cleanly observable):
  pins_acquired   ++ ONLY at pin_frame_locked() success (actual slot.pin_count increment)
  pins_released   ++ ONLY at unpin_frame_locked() success (actual slot.pin_count decrement)
  pin_balance     = pins_acquired - pins_released  (MUST be 0 at teardown)
  *** DO NOT separately count discharge_pin_list() — it DELEGATES to unpin_frame_locked(); counting both
      double-reports. This single authority captures normal discharge, public unpin, AS4 batch discharge,
      and rollback unpin. ***
  CROSS-CHECK: sample total_pin_count() (existing public accessor) at teardown; compare to pin_balance.

BALANCE B — LOOKUP-REF HANDOFF INVARIANT (the provable frame-ref balance):
  lookup_refs_acquired            ++ at lookup_frame_and_add_ref_locked() addFrameRef success
  lookup_refs_released_by_cache_core ++ at the cache-core's own freeFrame of a lookup ref (adoption-failure/
                                       rebalance path, e.g. the public wrapper's failure freeFrame)
  lookup_refs_transferred         ++ when the lookup ref is handed into a Cnr3OwnedFrameRef (adopted out)
  lookup_ref_balance = acquired - (released_by_cache_core + transferred)   (MUST be 0)
  INVARIANT: lookup_refs_acquired == lookup_refs_released_by_cache_core + lookup_refs_transferred.

EXPLICIT NON-CLAIM: DIAG.2b does NOT claim a global per-instance VSFrame ref leak detector. Store-retained
  references and general OwnedFrameRef lifetime are OUT OF SCOPE here (see Deferred items §5) — they surface
  as cache STATE in D-SUM-05 / store OUTCOME in D-SUM-08, not as a zero-at-summary ref balance.
CONSTNESS: lookup path is const -> mutable diagnostic member (Option A; matches DIAG.2a prune_diag_stats_).
WRITER: [DSUM-SUMMARY] D-SUM-04 block: both balances + their zero-check + total_pin_count cross-check.
  Snapshot-under-lock, format+write OUTSIDE lock. Interpretation line: "pin_balance and lookup_ref_balance
  must be 0 after drain; non-zero = leak or missed handoff."
```

## 2. D-SUM-05 — CACHE_INTEGRITY (v2: central invariant surface)

```text
PRIMARY HOOK: cache_state_invariants_hold_locked() (verified central surface — includes hot-zone model
  invariant, frame-index/slot consistency, checkpoint-position consistency, pin-count sanity, slot validity).
  observe_invariant_check(held_bool): invariant_checks_performed++ always; invariant_violations_detected++
  when it returns false. ADDITIVE — the existing bool return and all callers UNCHANGED (observe-only).
  DO NOT hook the ~8 scattered invariant_violation returns individually (brittle/invasive).
OPTIONAL: a small first_violation_site tag if a violation ever fires (should never in a healthy run).
STRUCTURAL SAMPLES (observe-only, at already-computed points — prune decision / store):
  slot_count_min/max, checkpoint_count_min/max, non_checkpoint_count_min/max,
  checkpoint_retain_headroom_min (CNR3_CACHE_CHECKPOINT_MAX_RETAIN - current_checkpoint_count),
  total_pin_count sample, a cache_state sample at summary time.
CONSTNESS: mutable member if the hooked path is const (Option A).
WRITER: [DSUM-SUMMARY] D-SUM-05 block: checks_performed, violations_detected (flag NON-ZERO),
  first_violation_site, structural min/max samples. Interpretation: "violations should be 0; non-zero =
  structural-invariant breach." This family OBSERVES the existing invariant surface; it adds NO new guards.
```

## 3. D-SUM-08 — CACHE_STORE (v2: wrapper-level, AS2 promotions only)

```text
HOOK LEVEL: the COMBINED store/prune WRAPPER, AFTER the store outcome is known (store outcome is NOT a
  prune-execution fact) — NOT inside DIAG.2a's observe_prune_execution_locked. Read the full outcome from
  Cnr3CombinedStoreAndPruneSummary (the wrapper summary), which carries the store result.
STRUCT: Cnr3CacheStoreDiagnosticStats
  stores_total, stores_by_kind[4] (ProductionCheckpoint/NonCheckpoint, As2ConsumerCheckpoint/NonCheckpoint),
  duplicates_seen (duplicate_existing_slot), incoming_rejected (incoming_frame_rejected — first-in-best-
  dressed loser), as2_checkpoint_promotions (from as2_summary.checkpoint_promoted — AS2 ONLY; production
  duplicate promotion is NOT exposed in the combined summary, so we count and LABEL only the AS2 case),
  store_failures (store_status != ok).
OBSERVE HOOK (gated CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE): observe_store_outcome(combined_summary) at the
  wrapper, observe-only, reads already-computed fields, changes nothing.
WRITER: [DSUM-SUMMARY] D-SUM-08 block: totals, per-kind, duplicates/rejected/as2_promotions/failures.
  Interpretation: "duplicates + rejected count first-in-best-dressed races; as2_checkpoint_promotions are
  monotonic AS2 upgrades; store_failures should be 0."
NOTE: if production-duplicate checkpoint promotion is later wanted, that needs a source-grounded promotion
  flag added to Cnr3CombinedStoreAndPruneSummary — deferred (§5), not done in DIAG.2b.
```

## 4. Cross-cutting (all three) — unchanged from v1

```text
- OBSERVE-ONLY (R-PROCESS-19): each family's COMPUTE macro off => struct/hooks/writer compile OUT, behaviour
  byte-identical. No hook alters a return, a Cnr3Status, or any store/pin/eviction/lookup RESULT. Independent gating.
- ADDITIVE (R-PROCESS-21): hooks at existing sites; no proven method restructured. If clean hosting needs a
  control-flow change, STOP and propose first.
- LOCK DISCIPLINE (DIAG.1): observe under cache lock (members alongside prune_diag_stats_); snapshot, release,
  then format+write stderr OUTSIDE the lock.
- CONSTNESS: mutable diagnostic members where hooked methods are const (Option A).
- TAG: [DSUM-SUMMARY] for all three. build_config.h: consume DSUM04/05/08 gates UNCHANGED; ADD NOTHING (no
  rings, no dump sub-flags — plain summary families).
- EMISSION: filter free + cache-core selftest synthetic reference summary (as DIAG.1/2a).
- FILES: cnr3_cache_diagnostics.{h,cpp}, cnr3_cache_core.{h,cpp}, cnr3_cache_core_selftest_main.cpp,
  vapoursynth-Cnr3.cpp. (No build_config.h change.)
```

## 5. DEFERRED ITEMS (recorded here; to be migrated to Future Investigations as FI entries at the DIAG.2b commit reconcile)

```text
D1. GLOBAL / PRIMITIVE-LEVEL VSFrame REF BALANCE — a true per-instance "all frame-ref ownership nets to
    zero" detector. NOT done in DIAG.2b: RAII releases occur via Cnr3OwnedFrameRef reset()/destructor/
    transfer_to_caller() outside hookable call sites; a naive counter false-reports leaks. Would need
    instrumenting the Cnr3OwnedFrameRef PRIMITIVE (acquire + reset) — a separate, broader exercise.
    -> propose as FI-1x. Acceptance criterion when attempted: nets to zero on all S-series with no real leak.
D2. RECOVERY-PATH RE-CHURN — D-SUM-10's re-churn hooks the predecessor-lookup path; S7/S8 showed the costly
    evict-then-rebuild churn flows through the recovery/anchor path (plan_bounded_recovery_search_and_record
    _anchor_pin), which D-SUM-10 does not hook. -> D-SUM-12 (recovery-rate), DIAG.3. (Already in DIAG.2a
    commit msg; migrate to FI at reconcile.)
D3. PRODUCTION-DUPLICATE CHECKPOINT PROMOTION signal — not exposed in Cnr3CombinedStoreAndPruneSummary;
    D-SUM-08 counts AS2 promotions only. Adding a production-promotion flag is a (small, observe-only) summary-
    surface expansion, deferred out of DIAG.2b. -> FI or a later store-diagnostic refinement.
D4. (standing, pre-existing) Lever B allocation pooling (~587 leaf, needs fmUnordered lifetime proof);
    fmParallel concurrency churn test (num_threads>1, after -r 1 baseline); R-PROCESS-2x flush-per-line rule
    (needs Document A ratification); plan/result plan-trace family (drafted, awaiting coder cross-check, DIAG.3).
```

## 6. Proof gate

```text
1. Build Debug+Release, DEFAULT config, all three COMPUTE macros ON: four-way 56/56 / 56/56 / 55/56 exit 1 /
   56/56; D-SUM-04/05/08 [DSUM-SUMMARY] blocks emit with populated fields.
2. R-PROCESS-19 (exit gate): all three COMPUTE macros OFF -> compiles/links, families compile out, four-way
   IDENTICAL, .vpy on/off byte-identical. INDEPENDENT gate checks: each family off with the other two on.
3. R-PROCESS-21: prune/store/pin/lookup/invariant proven behaviour unchanged; cache-core selftests pass.
4. SANITY read (S-series -r 1 S1/S3/S7/S8): D-SUM-04 pin_balance == 0 AND lookup_ref_balance == 0 at teardown
   (no leak/missed handoff); D-SUM-05 violations == 0; D-SUM-08 stores-by-kind + duplicates/rejected/as2_
   promotions consistent with the run (e.g. S7/S8 show ProductionCheckpoint stores + AS2 activity).
   *** The zero-balance-at-teardown on ALL S-series is the acceptance criterion that the D-SUM-04 siting is
       COMPLETE — a non-zero-with-no-real-leak means a missed acquire/release/transfer path; iterate if so. ***
```

## 7. Out of scope

DIAG.3 families (incl. D-SUM-12), plan/result plan-trace, DIAG.4 memory D-SUM-02, any behavioural change,
existing DSUM01-14 gate lines, CMS design, DIAG.2a code (untouched), the deferred items in §5.

## 8. CODER — CONFIRM v2 (targeted; the broad investigation is done)

The v1 investigation resolved the big questions. For v2, confirm before generating the patch:
```text
1. Confirm the two D-SUM-04 balances are BOTH fully observable at the named sites and net to zero — in
   particular, is lookup_refs_transferred cleanly detectable at the OwnedFrameRef adoption/handoff point,
   and are there any OTHER cache-core lookup-ref release/transfer paths beyond the named ones? (This is the
   one place a missed path would false-report; the S-series zero-balance test is the backstop.)
2. Confirm cache_state_invariants_hold_locked() is the right SINGLE hook for D-SUM-05 and that observing it
   does not change its result or any caller.
3. Confirm the combined store/prune wrapper is the right single site for D-SUM-08 observe_store_outcome, and
   that as2_summary.checkpoint_promoted is the correct AS2-promotion signal.
4. Confirm all three compile out cleanly with their COMPUTE macros off (R-PROCESS-19).
Then generate the DIAG.2b patch against the current src baseline.
```
