# CNR3 DIAG.2b Pre-Patch Inventory Review

## Executive finding

The DIAG.2b scope is broadly sensible, but **D-SUM-04 needs a correction before patching**.

The designer's concern is valid: a naive D-SUM-04 balance can falsely report leaks if it counts frame-ref acquisitions without observing all corresponding release/transfer paths. In the current source, the **pin balance is cleanly observable**, but the **frame-reference balance is not complete if implemented as "addFrameRef vs freeFrame" only inside `cnr3_cache_core.cpp`**.

Recommendation:

```text
Do not implement D-SUM-04 as a simple frame_refs_acquired - frame_refs_released net-zero counter.

Implement D-SUM-04 as two narrower, source-grounded balances:

1. Slot pin balance:
   pins_taken == pins_discharged
   cross-checked against total_pin_count()

2. Lookup-ref handoff balance:
   lookup_refs_acquired == lookup_refs_released_by_cache_core + lookup_refs_handed_to_owned_ref

Do not attempt to count every Cnr3OwnedFrameRef/freeFrame path as a global per-instance ref balance in DIAG.2b.
```

That directly addresses the "missed release site falsely reports a leak" risk.

## D-SUM-04 ownership-balance review

The scope says D-SUM-04 should detect ref/pin leaks and net to zero at teardown, with hooks at addFrameRef/freeFrame/pin/unpin/discharge sites. It also specifically asks us to verify whether these are all lifecycle points or whether batch/adoption/transfer sites also matter.

### Pin lifecycle: complete and safe

For pins, the current proposal is sound with one correction: **do not count both `discharge_pin_list()` and `unpin_frame_locked()` as separate discharges**, or the count will double-report. The complete pin authority is:

```text
pins_taken:
  increment only when pin_frame_locked() actually increments slot.pin_count.

pins_discharged:
  increment only when unpin_frame_locked() actually decrements slot.pin_count.

discharge_pin_list():
  do not separately increment pins_discharged; it already delegates to unpin_frame_locked().
```

This catches normal discharge, public unpin, AS4 batch discharge, and rollback unpin paths. `total_pin_count()` is a valid independent teardown cross-check.

### Frame-reference lifecycle: proposed scope is incomplete as written

The source has these relevant categories:

```text
A. lookup_frame_and_add_ref_locked()
   addFrameRef() on cached output, then public wrapper adopts into Cnr3OwnedFrameRef.

B. cnr3_arAllFramesReady.cpp store paths
   addFrameRef(output_frame) creates a cache-owned retained frame ref before store.

C. Cnr3OwnedFrameRef::reset() / destructor
   releases owned frame references via freeFrame().

D. transfer_to_caller()
   transfers a frame reference out; no freeFrame occurs there.
```

If D-SUM-04 only observes `lookup_frame_and_add_ref_locked()` acquisition and the rare direct `freeFrame()` at the public wrapper's adoption-failure path, it will falsely report normal lookup references as leaks, because normal releases happen later through `Cnr3OwnedFrameRef::reset()` or transfer-to-caller paths outside the cache-core hook.

The safest DIAG.2b correction is therefore **not** to claim a global VSFrame ref leak detector. Instead, track the lookup handoff invariant that the cache core can actually prove:

```text
lookup_refs_acquired
lookup_refs_released_by_cache_core
lookup_refs_handed_to_owned_ref

Invariant:
  lookup_refs_acquired == lookup_refs_released_by_cache_core + lookup_refs_handed_to_owned_ref
```

This matches the existing build-config wording better than the proposed simplified field list: the existing D-SUM-04 gate comments already describe `lookup_refs_acquired`, `lookup_refs_released`, `lookup_refs_transferred`, and `acquired == released + transferred`.

For cache-retained store references, I would **not** fold them into D-SUM-04 net-ref-at-teardown unless we also instrument all store-reference adoption, insertion, duplicate rejection, detach, clear, and RAII release paths. That is too broad and too easy to get wrong in DIAG.2b. Store-retained frame counts are better represented in D-SUM-05/D-SUM-08 as cache state/store outcome, not as a zero-at-summary ref balance.

## D-SUM-05 cache-integrity review

The D-SUM-05 idea is sound: observe structural invariant checks, report violations, and sample structural context.

But the source does **not** look like "about 8 invariant_violation returns." There are many `invariant_violation` return sites across helper validation, store, prune, recovery, lookup, pin, unpin, and clear. Instrumenting every return manually would be brittle and invasive.

Recommended correction:

```text
Primary hook:
  cache_state_invariants_hold_locked()

Record:
  invariant_checks_performed++
  invariant_violations_detected++ when it returns false

Optional secondary tags:
  sample a small site tag at selected callers if needed, but do not attempt to hook every invariant_violation return in DIAG.2b.
```

This is safer because `cache_state_invariants_hold_locked()` is the central structural invariant surface. It already includes the hot-zone model invariant check, frame index/slot consistency, checkpoint-position consistency, pin-count sanity, and slot validity.

For structural samples, the proposed prune/store sample points are sensible:

```text
slot_count_min/max
checkpoint_count_min/max
non_checkpoint_count_min/max
checkpoint_retain_headroom_min
total_pin_count snapshot
```

## D-SUM-08 store/duplicate review

D-SUM-08 is mostly sound and low-risk. The scope's purpose and fields match the intended store/duplicate telemetry.

Two corrections:

```text
1. The full store outcome is in Cnr3CombinedStoreAndPruneSummary,
   not solely in Cnr3CacheAs2StoreRecordSummary.

2. checkpoint_promotions is only fully detectable for AS2 from the current as2_summary.checkpoint_promoted field.
   Production duplicate checkpoint promotion is not directly exposed in Cnr3CombinedStoreAndPruneSummary.
```

So either:

```text
Option 1:
  Count only AS2 checkpoint promotions and label the field as as2_checkpoint_promotions.

Option 2:
  Add a source-grounded promotion flag to Cnr3CombinedStoreAndPruneSummary so production and AS2 promotions are both observable.
```

Option 1 is lower risk and probably sufficient for DIAG.2b. Option 2 is still observe-only but expands the summary surface.

Also, the "same sink as DIAG.2a observe_prune_execution_locked" wording is slightly imprecise. DIAG.2a's prune observer sits inside the prune execution path. D-SUM-08 should sit at the combined store/prune wrapper level, after store outcome is known, because the store outcome is not a prune-execution fact.

## Baseline and gating review

Confirmed from `src(15).zip`:

```text
Post-DIAG.2a baseline: yes
prune_diag_stats_ present: yes
D-SUM-10 re-churn ring present: yes
D-SUM-04 gate present: yes
D-SUM-05 gate present: yes
D-SUM-08 gate present: yes
D-SUM-04/05/08 structs/hooks/writers present: no, greenfield
```

The scope says the three families are greenfield and build on DIAG.2a's mutable diagnostic member pattern, while consuming existing gates unchanged. That matches the source.

No `build_config.h` change should be made for DIAG.2b; the scope explicitly says the gates must be consumed unchanged and no new dump subflags are needed.

## Recommended amended DIAG.2b implementation shape

```text
D-SUM-04:
  pins_taken
  pins_discharged
  peak_live_pins
  net_pins_at_teardown
  total_pin_count_cross_check

  lookup_refs_acquired
  lookup_refs_released_by_cache_core
  lookup_refs_handed_to_owned_ref
  lookup_ref_handoff_balance

  Do not claim complete global VSFrame ref leak detection.

D-SUM-05:
  invariant_checks_performed
  invariant_violations_detected
  first_violation_site
  structural min/max samples
  total_pin_count sample
  cache_state sample at summary

D-SUM-08:
  stores_total
  stores_by_kind[4]
  duplicates_seen
  incoming_rejected
  as2_checkpoint_promotions, or add combined-summary promotion signal
  store_failures
```

## Recommendation

Proceed with DIAG.2b, but with the D-SUM-04 correction above.

I would send this back to the designer as the coder finding:

```text
D-SUM-04 pins are safely complete if counted only at pin_frame_locked success and unpin_frame_locked success, with no separate discharge_pin_list discharge count.

D-SUM-04 frame refs are not safely complete as a simple acquired/released net-zero balance. The current source has addFrameRef sites outside cache_core and releases through Cnr3OwnedFrameRef reset/destructor/transfer paths. A naive counter would falsely report leaks.

Recommended fix: narrow D-SUM-04 to a provable lookup-ref handoff balance:
lookup_refs_acquired == lookup_refs_released_by_cache_core + lookup_refs_handed_to_owned_ref,
plus the separate slot-pin balance and total_pin_count cross-check.

Do not claim complete global VSFrame reference leak detection in DIAG.2b.
```

That preserves R-PROCESS-19/R-PROCESS-21: observe-only, additive placement, independent gating, snapshot under lock, and stderr write outside locks.
