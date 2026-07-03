# CNR3 PATCH SCOPE — DIAG.2b: D-SUM-04 ownership-balance + D-SUM-05 cache-integrity + D-SUM-08 store/duplicate

**From:** designer/reviewer (W3D), via coordinator (W3X).
**Type:** formal patch scope (R-PROCESS-19 observe-only proof is the exit gate; R-PROCESS-21 additive-only).
Implement exactly this; propose back for review before commit. **DIAG.2b = D-SUM-04 + D-SUM-05 + D-SUM-08
bundled** (completes the cache-core batch). Excludes DIAG.3 getFrame/recovery families and DIAG.4 memory.
**Baseline:** current post-DIAG.2a committed source (verified: contains prune_diag_stats_ / re-churn ring).
**Builds on:** DIAG.1 framework ([DSUM-SUMMARY], cnr3_diag_write_line, snapshot-outside-lock), DIAG.2a
(the mutable-diagnostic-member Option-A pattern, the store-sink observe placement).
**Status of the three families in current source:** all THREE greenfield (gates present in build_config.h
with the two-gate #error pattern — DSUM04_OWNERSHIP_BALANCE, DSUM05_CACHE_INTEGRITY, DSUM08_CACHE_STORE —
but NO structs, NO hooks, NO writers yet). Consume the gates unchanged (do NOT alter existing gate lines).

---

## 1. Family overview + hook siting (all verified against current source)

```text
D-SUM-04 OWNERSHIP_BALANCE  — ref/pin lifetime balance (the VALID per-instance leak detector).
   Hooks at sites DIAG.2a already touches: addFrameRef / freeFrame / pin_frame_locked /
   unpin_frame_locked / discharge_pin_list. Some on const paths -> mutable member (Option A).
D-SUM-05 CACHE_INTEGRITY    — structural-invariant tripwire (should-always-be-zero violations) +
   structural sampling. Hooks the invariant-check surface (the ~8 invariant_violation returns and
   cnr3_cache_hot_zone_model_invariants_hold), independent of the store/pin sites.
D-SUM-08 CACHE_STORE        — store-outcome + duplicate-resolution observer. Hooks the SAME store sink
   DIAG.2a's observe_prune_execution_locked already sits at; reads already-computed store summary fields.
```

## 2. D-SUM-04 — OWNERSHIP_BALANCE (proposed fields + hooks)

PURPOSE: detect ref/pin leaks by a per-instance running balance that should net to zero at teardown.
This is the VALID balance (per-instance, cache-global, sampled at quiescent teardown) — contrast the
INVALID cross-phase plan-local balance we rejected for the plan/result diagnostic.

```text
STRUCT (proposed): Cnr3CacheOwnershipDiagnosticStats
   - frame_refs_acquired        (++ at each addFrameRef success: ~cnr3_cache_core.cpp:1524/3576 region)
   - frame_refs_released        (++ at each freeFrame:            ~1562 region + batch freeFrame sites)
   - pins_taken                 (++ at pin_frame_locked success:  ~3615)
   - pins_discharged            (++ at unpin_frame_locked success: ~3672 + discharge_pin_list loop ~1618)
   - peak_live_refs, peak_live_pins   (running max of acquired-released / taken-discharged)
   - net_refs_at_teardown, net_pins_at_teardown  (computed at summary time; SHOULD be 0)
   - CROSS-CHECK: total_pin_count() already exists as a public accessor — sample it at teardown and
     compare against (pins_taken - pins_discharged) as an independent integrity cross-check; report both.
OBSERVE HOOKS (additive, gated CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE):
   observe_ref_acquired() / observe_ref_released() / observe_pin_taken() / observe_pin_discharged().
   Placement is ADDITIVE at the existing add/free/pin/unpin sites — do NOT restructure them (R-PROCESS-21).
CONSTNESS: lookup_frame_and_add_ref path is const -> the ownership stats member is mutable (Option A,
   consistent with DIAG.2a's prune_diag_stats_ and the existing mutable cache_mutex_).
WRITER: [DSUM-SUMMARY] D-SUM-04 block: the counters, peak live, net-at-teardown (flag NON-ZERO as a
   LEAK), and the total_pin_count() cross-check. Snapshot-under-lock, format+write OUTSIDE lock.
INTERPRETATION LINE: "net refs/pins at teardown should be 0; non-zero indicates a leak."
```

## 3. D-SUM-05 — CACHE_INTEGRITY (proposed fields + hooks)

PURPOSE: an observe-only tripwire that the cache's structural invariants hold; should always read zero
violations. Also samples structural facts for context.

```text
STRUCT (proposed): Cnr3CacheIntegrityDiagnosticStats
   - invariant_checks_performed     (++ each time an invariant guard is evaluated)
   - invariant_violations_detected  (++ each time a guard trips -> the tripwire; SHOULD be 0)
   - first_violation_site           (a small tag/int identifying which guard first tripped, if any)
   - structural samples (observe-only snapshots, e.g. at prune-decision / store time):
       slot_count_min/max, checkpoint_count_min/max, non_checkpoint_count_min/max,
       checkpoint_retain_headroom (CNR3_CACHE_CHECKPOINT_MAX_RETAIN - current_checkpoint_count) min.
OBSERVE HOOKS (additive, gated CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY):
   - at the ~8 invariant_violation return points: observe_invariant_check(site_tag, held_bool) BEFORE the
     return (records performed++ always, violations++ when tripped). ADDITIVE — the existing return and
     its Cnr3Status are UNCHANGED (observe-only; must not alter control flow or the returned status).
   - sample structural counts where already computed (e.g. alongside the prune trigger decision, which
     already has current_slot_count / current_checkpoint_count; and cnr3_cache_hot_zone_model_invariants_hold).
CONSTNESS: mutable member if any hooked site is const (Option A).
WRITER: [DSUM-SUMMARY] D-SUM-05 block: checks_performed, violations_detected (flag NON-ZERO),
   first_violation_site, and the structural min/max samples. Snapshot-under-lock, write OUTSIDE lock.
INTERPRETATION LINE: "violations should be 0; non-zero indicates a cache structural-invariant breach."
NOTE: the invariant guards are EXISTING behaviour that already returns invariant_violation on breach; this
   family only OBSERVES whether/where they trip — it does NOT add new guards or change existing ones.
```

## 4. D-SUM-08 — CACHE_STORE (proposed fields + hooks)

PURPOSE: observe store outcomes and first-in-best-dressed duplicate resolution. Lowest-risk of the three —
the store summary fields already exist; this reads them at the store sink alongside DIAG.2a's observe.

```text
SOURCE FIELDS ALREADY PRESENT (Cnr3CacheAs2StoreRecordSummary + store path):
   store_kind (Cnr3CacheStoreKind: ProductionCheckpoint/NonCheckpoint, As2ConsumerCheckpoint/NonCheckpoint,
   Invalid), stored_frame_number, store_status, duplicate_existing_slot, incoming_frame_rejected.
STRUCT (proposed): Cnr3CacheStoreDiagnosticStats
   - stores_total, stores_by_kind[4] (per Cnr3CacheStoreKind, excluding Invalid)
   - duplicates_seen            (duplicate_existing_slot == true)
   - incoming_rejected          (incoming_frame_rejected == true — first-in-best-dressed loser count)
   - checkpoint_promotions      (duplicate that raised an existing slot to checkpoint — monotonic promote)
   - store_failures             (store_status != ok)
OBSERVE HOOK (additive, gated CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE):
   observe_store_outcome(const Cnr3CacheAs2StoreRecordSummary&) at store_owned_frame_and_prune_impl —
   the SAME sink where DIAG.2a's observe_prune_execution_locked already sits. Reads the already-computed
   summary; adds NOTHING to the store decision (observe-only). Place ALONGSIDE, not inside, the prune observe.
WRITER: [DSUM-SUMMARY] D-SUM-08 block: totals, per-kind breakdown, duplicates/rejected/promotions,
   failures. Snapshot-under-lock, write OUTSIDE lock.
INTERPRETATION LINE: "duplicates + rejected count first-in-best-dressed races; promotions are monotonic
   checkpoint upgrades; store_failures should be 0."
```

## 5. Cross-cutting requirements (all three)

```text
- OBSERVE-ONLY (R-PROCESS-19): with each family's COMPUTE macro undefined, its struct/hooks/writer compile
  OUT; behaviour byte-identical. No hook may alter a return value, a Cnr3Status, eviction/store/pin RESULTS,
  or control flow. Each family INDEPENDENTLY gated.
- ADDITIVE PLACEMENT (R-PROCESS-21): hooks are added at existing sites; NO proven method is restructured to
  host a hook. If clean hosting needs any control-flow change, STOP and propose it first.
- LOCK DISCIPLINE (DIAG.1): observe under the cache lock (members alongside prune_diag_stats_); snapshot,
  release, then format+write stderr OUTSIDE the lock. No stderr inside any cache/CMS lock.
- CONSTNESS: mutable diagnostic members where hooked methods are const (Option A; matches DIAG.2a).
- TAG: [DSUM-SUMMARY] for all three summary blocks (NOT [KDT-SUMMARY]).
- build_config.h: consume DSUM04/05/08 gates UNCHANGED; add NOTHING to build_config for DIAG.2b (unlike
  DIAG.2a, these families need no dump sub-flags — no rings, no bounded dumps, just summaries).
- EMISSION POINTS: same as DIAG.1/2a — filter free + the cache-core selftest synthetic reference summary.
- FILES expected to change: cnr3_cache_diagnostics.{h,cpp}, cnr3_cache_core.{h,cpp},
  cnr3_cache_core_selftest_main.cpp, vapoursynth-Cnr3.cpp. (No build_config.h change.)
```

## 6. Proof gate

```text
1. Build Debug+Release (both projects), DEFAULT config, all three COMPUTE macros DEFINED: four-way
   56/56 / 56/56 / 55/56 exit 1 / 56/56; the D-SUM-04/05/08 [DSUM-SUMMARY] blocks emit with populated fields.
2. R-PROCESS-19 observe-only proof (exit gate): rebuild with all three COMPUTE macros UNDEFINED -> compiles/
   links, all three families compile out, four-way IDENTICAL, .vpy on/off byte-identical. Plus INDEPENDENT
   gate checks: each family off with the other two on (04off/05on/08on, etc.) to prove independent gating.
3. R-PROCESS-21: prune/store/pin/lookup proven behaviour unchanged (only additive gated observation added);
   cache-core selftests pass unchanged (incl. the ownership/integrity/store scenarios).
4. SANITY read: run the S-series -r 1 (S1/S3/S7/S8) and confirm the new families populate sensibly —
   e.g. D-SUM-04 net refs/pins == 0 at teardown (no leak); D-SUM-05 violations == 0; D-SUM-08 store-by-kind
   and duplicates/rejected consistent with the run (e.g. S7/S8 show ProductionCheckpoint stores + prune).
```

## 7. Out of scope

DIAG.3 getFrame/recovery families (incl. D-SUM-12 recovery-rate), the plan/result plan-trace family, DIAG.4
memory D-SUM-02, any behavioural/policy change, existing DSUM01-14 gate lines, CMS design, the DIAG.2a
prune/re-churn/hot-zone code (untouched).

---

## 8. CODER RE-VALIDATE + INVESTIGATE BRIEF (please do this before generating the patch)

This scope's structs, fields, and hook sites are the DESIGNER's proposal, read from the current post-DIAG.2a
source. Before generating the DIAG.2b patch, please INDEPENDENTLY re-validate and investigate the whole lot
against your current src baseline, and report back sensibility + gaps:

```text
1. RE-VALIDATE SOURCE STATE: confirm your baseline is post-DIAG.2a (contains prune_diag_stats_, the
   re-churn ring). Confirm DSUM04_OWNERSHIP_BALANCE / DSUM05_CACHE_INTEGRITY / DSUM08_CACHE_STORE gates
   exist with the two-gate #error pattern and that all three families are still greenfield (no structs yet).

2. D-SUM-04 (ownership): re-validate the acquire/release sites (addFrameRef, freeFrame, pin_frame_locked,
   unpin_frame_locked, discharge_pin_list). Are these ALL the ref/pin lifecycle points, or are there others
   (e.g. batch freeFrame paths, adoption/transfer sites) the balance must also observe to net to zero?
   Confirm total_pin_count() is a valid teardown cross-check. Flag any acquire/release NOT on this list.

3. D-SUM-05 (integrity): re-validate the invariant-check surface. Are the ~8 invariant_violation returns +
   cnr3_cache_hot_zone_model_invariants_hold the right/complete set to observe? Is a site_tag scheme
   sensible, and which structural counts are cheaply samplable at already-computed points (prune decision,
   store)? Flag any invariant guard missed, or any that would be unsafe/costly to hook.

4. D-SUM-08 (store): re-validate Cnr3CacheAs2StoreRecordSummary fields (store_kind, stored_frame_number,
   store_status, duplicate_existing_slot, incoming_frame_rejected) and that store_owned_frame_and_prune_impl
   is the single common sink to observe (alongside DIAG.2a's observe_prune_execution_locked). Confirm the
   checkpoint-promotion case is detectable from the summary, or say what extra signal is needed.

5. FIELDS/WRITER: for each family, confirm the proposed struct fields are sufficient and source-grounded,
   suggest additions/removals, and confirm the [DSUM-SUMMARY] writer + snapshot-outside-lock pattern fits.

6. CONSTNESS/GATING: confirm which hooked sites are const (needing the mutable Option-A member) and that
   each family compiles out cleanly with its COMPUTE macro undefined (R-PROCESS-19).

Report findings (sensibility + gaps + any hook/field corrections) BEFORE generating the patch, same as the
DIAG.2a inventory pass. Then generate DIAG.2b against the current src baseline.
```
