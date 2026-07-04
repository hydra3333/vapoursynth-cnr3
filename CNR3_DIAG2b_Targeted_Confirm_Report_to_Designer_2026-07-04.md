# CNR3 DIAG.2b v2 targeted-confirm investigation report

**Prepared for:** designer/reviewer (W3D), via coordinator (W3X)  
**Prepared by:** coder role  
**Date:** 2026-07-04  
**Task:** targeted confirm requested by `CNR3_Patch_Scope_DIAG2b_ownership_integrity_store_v2.md` §8 before generating the DIAG.2b patch.  
**Source baseline inspected:** uploaded current source baseline from `src(16).zip`, extracted to `cnr3_src_unzipped`.  

---

## 0. Executive finding

DIAG.2b v2 is implementable against the inspected post-DIAG.2a source baseline, and the v2 re-siting is substantially correct.

Recommended outcome:

```text
PROCEED to DIAG.2b patch generation, with two recorded implementation notes:

1. D-SUM-05 can remain a single central hook, but the central function has many early returns. The safe additive implementation is to observe each return result inside cache_state_invariants_hold_locked() itself, or through a tiny return-observer helper used only inside that central function. Do not restructure the invariant body or hook scattered caller return sites.

2. D-SUM-08's store_failures definition should not count first-in-best-dressed duplicate outcomes as failures. In the current source, duplicate is a non-ok Cnr3Status, but it is explicitly an allowed store outcome for live return. Count store_failures as statuses other than ok and duplicate, or as not allowed by the existing live-store outcome policy. Counting store_status != ok would false-report healthy duplicates.
```

The second point is the only design/scope wording issue I found. It is not a blocker to DIAG.2b, but it should be accepted before patching because it affects the meaning of a displayed D-SUM-08 failure counter.

---

## 1. Inputs reviewed

Primary active scope:

- `CNR3_Patch_Scope_DIAG2b_ownership_integrity_store_v2.md`, especially §1-§8.
- The separately attached duplicate/current copy `CNR3_Patch_Scope_DIAG2b_ownership_integrity_store_v2(1).md` was consistent with the copy in `FINAL_DOCS.zip`.

Relevant standing/context documents:

- `Document_A_CNR3_Project_Context_and_Standing_Rules_v3_12.md`.
- `cnr3_cache_manager_design_v7_15.md`.
- `CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_24.md`.
- `CNR3_CMS_Future_Investigations_and_Open_Questions_v7_17.md`.
- `cnr3_diagnostics_specification_v1_5.md`.
- `cnr3_memory_diagnostics_spec_v2.md`.
- `CNR3_DIAG2b_pre_patch_inventory_review.md`.

Source files inspected most directly:

- `cnr3_build_config.h`
- `cnr3_cache_core.h`
- `cnr3_cache_core.cpp`
- `cnr3_cache_diagnostics.h`
- `cnr3_cache_diagnostics.cpp`
- `cnr3_cache_core_selftest_main.cpp`
- `cnr3_owned_frame_ref.h`
- `cnr3_owned_frame_ref.cpp`
- `cnr3_arAllFramesReady.cpp`
- `vapoursynth-Cnr3.cpp`

Version rule applied: where older document versions are mentioned by another document, I treated the latest available version in this handoff set as controlling unless the document itself said otherwise.

---

## 2. Applicable rules and constraints

### 2.1 R-PROCESS-19 is an exit gate

Document A states that any phase introducing or changing a D-SUM diagnostic compute gate must include compute-disabled observe-only proof: the relevant `CNR3_DIAG_COMPUTE_DSUMxx_*` macro undefined, clean compile/link, observation bodies compiled out, and pre-existing behaviour/selftests unchanged (`Document_A...v3_12.md`, lines 884-900). Document A also repeats that when a phase changes a D-SUM compute gate, the R-PROCESS-19 macro-off run is required in addition to the usual four-way selftest (`Document_A...v3_12.md`, lines 960-968).

DIAG.2b must therefore prove independent compile-out for D-SUM-04, D-SUM-05, and D-SUM-08, including the all-three-off case and each-family-off matrix requested by the designer scope.

### 2.2 R-PROCESS-21 prohibits hidden restructuring of proven code

Document A freezes proven behaviour and internals unless a specific visible proposal is approved first (`Document_A...v3_12.md`, lines 1061-1094). DIAG.2b hooks must therefore be additive and must not restructure proven cache, prune, pin, store, or invariant logic merely to host diagnostics.

### 2.3 D-SUM framework expectations

The diagnostics spec lists D-SUM-04 as ownership/pin/lookup-ref balance, D-SUM-05 as cache integrity/teardown, and D-SUM-08 as cache store/duplicate-store/first-in-best-dressed (`cnr3_diagnostics_specification_v1_5.md`, lines 363-381). It specifies:

- D-SUM-04 fields and interpretation: `pin_balance`, `lookup_ref_balance`, and `acquired == released + transferred` (`cnr3_diagnostics_specification_v1_5.md`, lines 745-842).
- D-SUM-05 purpose: final cache state and validation of stale index entries, invalid slot state, pinned shutdown state, and reference-balance errors (`cnr3_diagnostics_specification_v1_5.md`, lines 846-907).
- D-SUM-08 purpose: cache store behaviour and duplicate store cases; duplicates are not automatically bad and first-in-best-dressed must preserve existing authority (`cnr3_diagnostics_specification_v1_5.md`, lines 1126-1210).

### 2.4 Memory diagnostics spec relevance

`cnr3_memory_diagnostics_spec_v2.md` remains useful for output-quality expectations: no stdout, aligned informative tables, compact output, and snapshot/summary discipline. However, it is a CMS06-era memory-specific document: it names CMS06 as companion material (`cnr3_memory_diagnostics_spec_v2.md`, lines 3-7), while the current D-SUM work is governed by the CMS07 diagnostics specification and DIAG.2b scope. I therefore used it as style/context only, not as authority for D-SUM-04/05/08 semantics.

### 2.5 Future Investigations constraints

FI-11, FI-12, and FI-13 are directly relevant scope boundaries:

- FI-11: recovery-path re-churn is deferred to D-SUM-12 / DIAG.3, not DIAG.2b (`CNR3_CMS_Future_Investigations...v7_17.md`, lines 554-569).
- FI-12: global / primitive-level VSFrame ref balance is deferred because call-site hooking false-reports; DIAG.2b should implement the two narrow balances (`...v7_17.md`, lines 571-586).
- FI-13: production-duplicate checkpoint-promotion signal is not exposed in `Cnr3CombinedStoreAndPruneSummary`; DIAG.2b should count AS2 promotions only (`...v7_17.md`, lines 588-595).

These match the DIAG.2b v2 scope and should not be expanded during patching.

---

## 3. §8(1): D-SUM-04 ownership balance confirm

### 3.1 Slot pin balance is fully observable

Confirmed.

The authoritative pin acquisition point is `Cnr3OutputCacheCore::pin_frame_locked()`:

- It validates frame number, token state, cache invariants, frame-index lookup, slot validity, slot identity, and pin capacity.
- The actual state change is `++slot.pin_count` at `cnr3_cache_core.cpp:3660`.
- The token is populated immediately after at lines 3662-3663.

The authoritative pin release point is `Cnr3OutputCacheCore::unpin_frame_locked()`:

- It validates token, invariants, frame-index lookup, slot validity, slot identity, slot ID match, and positive pin count.
- The actual state change is `--slot.pin_count` at `cnr3_cache_core.cpp:3713`.
- The token is reset immediately after at line 3714.

All relevant release paths delegate to `unpin_frame_locked()`:

- Public `unpin_frame()` simply locks once and delegates to `unpin_frame_locked()` (`cnr3_cache_core.cpp:1589-1599`).
- AS4 `discharge_pin_list()` walks all valid pin-list tokens and calls `unpin_frame_locked()` (`cnr3_cache_core.cpp:1601-1626`).
- Rollback on pin-list record failure in `lookup_frame_and_record_pin_locked()` calls `unpin_frame_locked()` (`cnr3_cache_core.cpp:3587-3613`).
- Rollback on record failure in `store_owned_frame_and_record_pin_locked()` calls `unpin_frame_locked()` (`cnr3_cache_core.cpp:2658-2676`).

Therefore, the v2 instruction is correct: count `pins_acquired` only when `pin_frame_locked()` actually increments `slot.pin_count`, and count `pins_released` only when `unpin_frame_locked()` actually decrements `slot.pin_count`. Do not separately count `discharge_pin_list()` as a discharge source; that would double-count AS4 releases.

`total_pin_count()` exists as a public lock-owning cross-check and returns the lock-protected `total_pin_count_locked()` result (`cnr3_cache_core.cpp:650-654`). That is suitable for the requested teardown/sample cross-check.

### 3.2 Lookup-ref handoff balance is fully observable

Confirmed, with the important v2 non-claim preserved.

The cache-core lookup-ref acquisition point is `Cnr3OutputCacheCore::lookup_frame_and_add_ref_locked()`:

- It is private and lock-protected.
- It validates the frame-index and slot, obtains `cached_frame = slot.frame.get()`, then calls `vsapi->addFrameRef(cached_frame)` at `cnr3_cache_core.cpp:3576`.
- On success, it writes `*out_acquired_frame = acquired_frame` at line 3582 and returns `ok`.

The public wrapper `Cnr3OutputCacheCore::lookup_frame_and_add_ref()` is the only source call site for the locked helper in `cnr3_cache_core.cpp`:

- It declares `const VSFrame* acquired_frame = nullptr` (`cnr3_cache_core.cpp:1532`).
- It calls `lookup_frame_and_add_ref_locked()` under `cache_mutex_` (`cnr3_cache_core.cpp:1535-1543`).
- It adopts the acquired reference into `Cnr3OwnedFrameRef` at `out_frame.reset_to_owned_frame(acquired_frame, vsapi)` (`cnr3_cache_core.cpp:1553-1554`).
- If adoption fails, it rebalances by calling `vsapi->freeFrame(acquired_frame)` outside the cache lock (`cnr3_cache_core.cpp:1556-1564`).

Thus the narrow lookup-ref handoff counters have clean sites:

```text
lookup_refs_acquired:
  increment after addFrameRef returns non-null in lookup_frame_and_add_ref_locked().

lookup_refs_transferred:
  increment after out_frame.reset_to_owned_frame(acquired_frame, vsapi) succeeds in lookup_frame_and_add_ref(). This is handoff into Cnr3OwnedFrameRef, not transfer_to_caller().

lookup_refs_released_by_cache_core:
  increment immediately before/after the public wrapper's adoption-failure freeFrame(acquired_frame).
```

I found no other cache-core lookup-ref release or transfer path beyond these named ones. The private locked helper is called only by the public wrapper, and the public wrapper has exactly the success-adopt and failure-freeFrame branches for an acquired lookup reference.

### 3.3 Why the global VSFrame ref balance remains out of scope

The source confirms FI-12 and the v2 scope's non-claim.

`Cnr3OwnedFrameRef` is an RAII wrapper that releases its held reference from `reset()` using `VSAPI::freeFrame()` (`cnr3_owned_frame_ref.cpp:61-70`), and `reset()` is invoked from the destructor and move assignment (`cnr3_owned_frame_ref.cpp:3-5`, `16-30`). `transfer_to_caller()` deliberately empties the wrapper without freeing (`cnr3_owned_frame_ref.cpp:73-80`).

There are also non-lookup `addFrameRef()` sites outside `cnr3_cache_core.cpp`, for example the live output store path creates a retained cache reference at `cnr3_arAllFramesReady.cpp:607`, adopts it into `Cnr3OwnedFrameRef` at lines 614-618, and has failure cleanup at lines 620-623.

Therefore, DIAG.2b must not claim a global VSFrame leak detector. Store-retained references and general `Cnr3OwnedFrameRef` lifecycle belong to FI-12 / a future primitive-level ownership arc, not to the DIAG.2b D-SUM-04 lookup-ref handoff balance.

### 3.4 D-SUM-04 implementation recommendation

Use the following DIAG.2b meaning:

```text
pin_balance = pins_acquired - pins_released
lookup_ref_balance = lookup_refs_acquired - (lookup_refs_released_by_cache_core + lookup_refs_transferred)
```

At summary/drain, both balances must be zero. Also print `total_pin_count` as an independent cross-check.

If retaining the spec's `pin_list_records`, `pin_list_discharges`, and `pin_list_balance` fields, ensure the semantics do not double-count AS4. My recommendation for DIAG.2b is either:

```text
A. Keep the v2 scope minimal and print pin_balance + total_pin_count, without claiming a separate pin-list balance; or

B. If pin-list fields must be printed, count pin-list records at record_pin_without_allocation success and count pin-list discharges as successful token invalidations via unpin_frame_locked() from a previously recorded token. Do not count discharge_pin_list() itself as a release source.
```

Option A is lower risk and best aligned with the v2 two-balance correction. Option B is possible but needs care because pin-list ownership and slot pin state are related but not identical concepts.

---

## 4. §8(2): D-SUM-05 cache-integrity confirm

### 4.1 Central hook suitability

Confirmed.

`cache_state_invariants_hold_locked()` is the right single semantic surface for D-SUM-05. It currently checks:

- hot-zone model invariant (`cnr3_cache_core.cpp:3752-3755`);
- frame-index entries point to valid slots and matching frame numbers (`cnr3_cache_core.cpp:3757-3774`);
- slot validity, valid/invalid frame state, checkpoint flag state, and non-negative pin counts (`cnr3_cache_core.cpp:3776-3820`);
- checkpoint-position count and uniqueness (`cnr3_cache_core.cpp:3822-3853`);
- checkpoint-position slots are checkpoint/indexable slots (`cnr3_cache_core.cpp:3854-3865`).

The function is already used widely as the central structural invariant predicate. Examples include public `cache_state_invariants_hold()` (`cnr3_cache_core.cpp:656-660`) and many mutating helpers before/after critical mutations, including lookup, pin, unpin, store, prune, remove, clear, hot-zone observation/retirement, and recovery planning.

Therefore, D-SUM-05 should not hook scattered `return Cnr3Status::invariant_violation` sites across the source. That would be brittle and would broaden the phase into caller semantics. The single central structural predicate is the right surface.

### 4.2 Implementation note: many early returns inside the central function

There is one implementation detail that should be recorded before patching.

`cache_state_invariants_hold_locked()` currently returns `false` directly from many internal check failures and returns `true` only at the end. If DIAG.2b simply adds one hook at the final `return true`, it will count only passing checks and will miss violations. If it restructures the whole predicate into a single accumulated `bool ok`, that risks violating R-PROCESS-21 by rewriting proven internals.

Recommended safe pattern:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    return observe_cache_invariant_result_locked(false, "site_tag");
#else
    return false;
#endif
```

or an equivalent tiny helper/macro used only inside `cache_state_invariants_hold_locked()`.

The helper must:

- update only diagnostic counters/samples;
- be `noexcept`;
- return the input bool unchanged;
- not allocate, format, print, acquire locks, or call `cache_state_invariants_hold_locked()` recursively;
- be compiled out fully when `CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY` is undefined.

This still preserves the requested single hook *surface*: all observation remains inside `cache_state_invariants_hold_locked()`. It does not hook scattered caller returns and does not alter any caller result.

### 4.3 Structural samples

The requested structural samples are available without new cache semantics:

- `slot_count_locked()` returns `slots_.size()` (`cnr3_cache_core.cpp:1666-1668`).
- `checkpoint_count_locked()` returns `checkpoint_slot_positions_.size()` (`cnr3_cache_core.cpp:1674-1676`).
- Non-checkpoint count can be derived as `slot_count - checkpoint_count` after invariant consistency holds or sampled defensively using the current counts.
- `total_pin_count_locked()` already exists and backs public `total_pin_count()`.
- `checkpoint_retain_headroom_min` can be computed from the current checkpoint count and `CNR3_CACHE_CHECKPOINT_MAX_RETAIN`, using saturating/defensive arithmetic.

Recommendation: sample structural min/max from inside the same lock-held observer helper or at already lock-held store/prune points. Snapshot a copy under lock for output, and format/write outside lock.

---

## 5. §8(3): D-SUM-08 cache-store confirm

### 5.1 Combined store/prune wrapper is the right semantic site

Confirmed, with a minor definition caveat below.

The live W.3 store path is centralized through the combined store/prune wrappers:

- `store_production_output_and_prune()` (`cnr3_cache_core.cpp:815-850`);
- `store_as2_floor_and_prune()` (`cnr3_cache_core.cpp:852-888`);
- `store_recovery_hole_and_prune()` (`cnr3_cache_core.cpp:890-950`);
- all three delegate to the common `store_owned_frame_and_prune_impl()` (`cnr3_cache_core.cpp:953-1099`).

The current live call sites use these wrappers:

- production store from `cnr3_store_live_output_frame_for_return()` (`cnr3_arAllFramesReady.cpp:587-637`);
- frame-0 fresh-start production checkpoint store (`cnr3_arAllFramesReady.cpp:1713-1720`);
- recovery floor AS2 store (`cnr3_arAllFramesReady.cpp:1162`);
- recovery hole AS2 store (`cnr3_arAllFramesReady.cpp:1373`).

The combined summary exposes the store result and AS2 summary:

- `Cnr3CombinedStoreAndPruneSummary::store_kind`, `stored_frame_number`, `activation_target_frame`, `store_status`, `retire_status`, `prune_status`, `as2_summary`, and `prune_summary` are defined in `cnr3_cache_core.h:535-544`.
- `Cnr3CacheAs2StoreRecordSummary::duplicate_existing_slot`, `checkpoint_promoted`, and `incoming_frame_rejected` are defined in `cnr3_cache_core.h:483-493`.

That is the correct level for D-SUM-08. D-SUM-08 should not be hosted inside DIAG.2a's `observe_prune_execution_locked()` because store outcome is not a prune-execution fact.

### 5.2 `as2_summary.checkpoint_promoted` is the correct AS2 signal

Confirmed.

AS2 duplicate checkpoint promotion is determined inside `store_owned_frame_and_record_pin_locked()`:

- It records whether an existing slot was already a checkpoint before store (`cnr3_cache_core.cpp:2589-2612`).
- On duplicate, it sets `out_summary.duplicate_existing_slot = true`, `incoming_frame_rejected = true`, and `checkpoint_promoted = is_checkpoint && !existing_slot_was_checkpoint` (`cnr3_cache_core.cpp:2621-2626`).
- It later records resulting checkpoint status and `pin_recorded = true` (`cnr3_cache_core.cpp:2694-2698`).

The combined wrapper copies the AS2 summary into `out_summary.as2_summary` (`cnr3_cache_core.cpp:1024-1036`). Thus `as2_summary.checkpoint_promoted` is exactly the current source-grounded AS2-promotion signal.

Production duplicate checkpoint promotion exists inside the lower store helper, because `store_owned_frame_locked()` promotes an existing non-checkpoint slot on duplicate checkpoint store (`cnr3_cache_core.cpp:2486-2505`), but the production wrapper does not expose a production-specific promotion flag in `Cnr3CombinedStoreAndPruneSummary`. FI-13 correctly defers that signal.

### 5.3 D-SUM-08 caveat: do not count duplicate as `store_failures`

This is the one scope wording issue found.

The v2 scope says `store_failures (store_status != ok)`. In the current source, that would count first-in-best-dressed duplicates as failures, because `Cnr3Status::duplicate` is not `ok`:

- `cnr3_status_is_ok()` returns true only for `Cnr3Status::ok` (`cnr3_common.h:143-147`).
- `Cnr3Status::duplicate` is a separate status (`cnr3_common.h:91-100`).
- Live store logic explicitly allows both `ok` and `duplicate` as returnable store outcomes (`cnr3_arAllFramesReady.cpp:469-473`).
- The authoritative-return path handles duplicate by discarding the computed loser and returning a fresh reference to the cached winner (`cnr3_arAllFramesReady.cpp:680-713`).

The diagnostics spec also says duplicates are not automatically bad and are INFO/WARN with clean ownership, while genuine store errors must be explained (`cnr3_diagnostics_specification_v1_5.md`, lines 1171-1210).

Recommendation to designer:

```text
Change the DIAG.2b D-SUM-08 store_failures interpretation from:

  store_status != ok

to:

  store_status is neither ok nor duplicate

or equivalently:

  !cnr3_live_store_status_allows_return(store_status)

if that helper is considered appropriate to mirror in cache diagnostics.
```

Because `cnr3_live_store_status_allows_return()` lives in the plugin integration source, I would not call that helper from cache diagnostics unless it is moved or mirrored deliberately. The cache-layer implementation can simply count:

```text
store_failures += (store_status != Cnr3Status::ok && store_status != Cnr3Status::duplicate)
```

This keeps duplicate telemetry separate and prevents false failure rows when first-in-best-dressed does exactly what it is supposed to do.

### 5.4 Scope of `stores_total`

For a clean single-site implementation, define `stores_total` as valid combined store/prune wrapper attempts reaching the combined store/prune implementation. Public-wrapper invalid-argument failures before `store_owned_frame_and_prune_impl()` are not meaningful cache store attempts in healthy operation and should not force extra scattered hooks into DIAG.2b.

If the designer wants D-SUM-08 to count public-wrapper invalid-argument failures too, that is a broader wording change: it requires observing multiple early-return paths in `store_production_output_and_prune()`, `store_as2_floor_and_prune()`, and `store_recovery_hole_and_prune()`, not a single common wrapper site. My recommendation is not to broaden DIAG.2b for that.

---

## 6. §8(4): R-PROCESS-19 compile-out confirm

Confirmed as implementable.

Current `cnr3_build_config.h` already defines the D-SUM-04, D-SUM-05, and D-SUM-08 compute/print gate pairs and paired `#error` guards:

- D-SUM-04 gates: `cnr3_build_config.h:196-219`.
- D-SUM-05 gates: `cnr3_build_config.h:221-245`.
- D-SUM-08 gates: `cnr3_build_config.h:299-321`.

The v2 instruction to make no `build_config.h` change is correct.

The patch should use the existing pattern:

```text
CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE
  owns D-SUM-04 stats structs, mutable cache-core member, observe helpers, snapshot accessor, and selftest reference summary body.

CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE
  owns the D-SUM-04 writer declaration/definition and writer call sites.

CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY
  owns D-SUM-05 stats structs, mutable cache-core member, observe helpers, snapshot accessor, and selftest reference summary body.

CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY
  owns the D-SUM-05 writer declaration/definition and writer call sites.

CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE
  owns D-SUM-08 stats structs, mutable cache-core member, observe helpers, snapshot accessor, and selftest reference summary body.

CNR3_DIAG_PRINT_DSUM08_CACHE_STORE
  owns the D-SUM-08 writer declaration/definition and writer call sites.
```

All call sites must be guarded so that when the compute macro is off:

- no stats member is present;
- no observe helper is declared or called;
- no snapshot accessor is declared or called;
- no writer is declared or called;
- no synthetic selftest reference summary body is compiled;
- no D-SUM-04/05/08 stderr block is emitted.

This satisfies R-PROCESS-19 in design. Actual PASS remains to be proven by the macro-off build matrix after patch generation.

---

## 7. Proposed DIAG.2b patch shape

This is not the patch; it is the recommended implementation map for designer approval.

### 7.1 `cnr3_cache_diagnostics.h`

Add gated stats structs and tiny observe helpers.

Recommended structs:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
struct Cnr3CacheOwnershipDiagnosticStats {
    std::uint64_t pins_acquired = 0;
    std::uint64_t pins_released = 0;
    std::uint64_t lookup_refs_acquired = 0;
    std::uint64_t lookup_refs_released_by_cache_core = 0;
    std::uint64_t lookup_refs_transferred = 0;
    std::uint64_t ownership_errors = 0;
    int total_pin_count_at_summary = 0;
};
#endif
```

For D-SUM-05:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
struct Cnr3CacheIntegrityDiagnosticStats {
    std::uint64_t invariant_checks_performed = 0;
    std::uint64_t invariant_violations_detected = 0;
    const char* first_violation_site = nullptr;
    bool have_structural_sample = false;
    std::size_t slot_count_min = 0;
    std::size_t slot_count_max = 0;
    std::size_t checkpoint_count_min = 0;
    std::size_t checkpoint_count_max = 0;
    std::size_t non_checkpoint_count_min = 0;
    std::size_t non_checkpoint_count_max = 0;
    std::size_t checkpoint_retain_headroom_min = 0;
    int total_pin_count_at_summary = 0;
    std::size_t slot_count_at_summary = 0;
    std::size_t checkpoint_count_at_summary = 0;
};
#endif
```

For D-SUM-08:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
struct Cnr3CacheStoreDiagnosticStats {
    std::uint64_t stores_total = 0;
    std::uint64_t stores_production_checkpoint = 0;
    std::uint64_t stores_production_noncheckpoint = 0;
    std::uint64_t stores_as2_checkpoint = 0;
    std::uint64_t stores_as2_noncheckpoint = 0;
    std::uint64_t duplicates_seen = 0;
    std::uint64_t incoming_rejected = 0;
    std::uint64_t as2_checkpoint_promotions = 0;
    std::uint64_t store_failures = 0;
};
#endif
```

Field names can be adjusted to match the final writer naming, but the meanings should stay aligned with the above.

### 7.2 `cnr3_cache_core.h`

Add gated mutable members next to `prune_diag_stats_`, matching DIAG.2a's const-observer pattern:

```cpp
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    mutable Cnr3CacheOwnershipDiagnosticStats ownership_diag_stats_{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    mutable Cnr3CacheIntegrityDiagnosticStats integrity_diag_stats_{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    mutable Cnr3CacheStoreDiagnosticStats store_diag_stats_{};
#endif
```

Add lock-owning snapshot accessors under matching compute gates.

Add private observe helpers under matching compute gates.

### 7.3 `cnr3_cache_core.cpp`

D-SUM-04 hooks:

- `pin_frame_locked()` after `++slot.pin_count`.
- `unpin_frame_locked()` after `--slot.pin_count` / token reset.
- `lookup_frame_and_add_ref_locked()` after non-null `addFrameRef()` acquisition.
- `lookup_frame_and_add_ref()` after successful `reset_to_owned_frame()` adoption.
- `lookup_frame_and_add_ref()` adoption-failure branch immediately around `vsapi->freeFrame(acquired_frame)`.

D-SUM-05 hook:

- inside `cache_state_invariants_hold_locked()` only, using a return-result observer helper.
- sample structural stats from the same lock-held context.

D-SUM-08 hook:

- inside the combined store/prune implementation once the store outcome is known.
- preferably after prune outcome too if the final `Cnr3CombinedStoreAndPruneSummary` is already complete and still under the lock.
- count duplicate separately and do not include duplicate in `store_failures`.

### 7.4 `cnr3_cache_diagnostics.cpp`

Add D-SUM-04/05/08 writer functions under the print gates. Follow the existing D-SUM-10/11 style:

- `[DSUM-SUMMARY]` prefix.
- stderr only.
- no formatting inside cache locks.
- `cnr3_diag_flush_stderr()` at writer end, consistent with existing D-SUM-10/11 writers.

### 7.5 `vapoursynth-Cnr3.cpp`

In `cnr3_free_filter()`, add writers in numeric order after D-SUM-01 and before D-SUM-10/11:

```text
D-SUM-01
D-SUM-04
D-SUM-05
D-SUM-08
D-SUM-10
D-SUM-11
```

Use lock-owning snapshot accessors, so the writer receives a by-value snapshot and formats outside the cache lock.

### 7.6 `cnr3_cache_core_selftest_main.cpp`

Add synthetic reference summary emitters for D-SUM-04, D-SUM-05, and D-SUM-08, following the existing D-SUM-01/10/11 pattern. These should be compiled only when the matching compute gate is on and write only when the matching print gate is on.

---

## 8. Patch proof expectations after designer acceptance

Expected default selftest run remains:

```text
Debug normal:        56/56 PASS, exit 0
Release normal:      56/56 PASS, exit 0
Release forced-fail: 55/56 PASS, 1 expected forced failure, exit 1
Release verbose:     56/56 PASS, exit 0
```

Expected diagnostics evidence after DIAG.2b:

```text
[DSUM-SUMMARY] D-SUM-04 ... emits populated ownership/pin/lookup-ref balance fields.
[DSUM-SUMMARY] D-SUM-05 ... emits invariant checks, violations, and structural sample fields.
[DSUM-SUMMARY] D-SUM-08 ... emits stores_total, stores_by_kind, duplicates, incoming_rejected, as2_checkpoint_promotions, store_failures.
```

Required R-PROCESS-19 matrix after patch:

```text
all default gates ON
D-SUM-04 OFF, D-SUM-05/08 ON
D-SUM-05 OFF, D-SUM-04/08 ON
D-SUM-08 OFF, D-SUM-04/05 ON
D-SUM-04/05/08 all OFF
```

For each macro-off configuration:

```text
- compile/link cleanly;
- family's structs/hooks/writer compile out;
- no behaviour change in non-D-SUM selftests;
- expected default/forced/verbose selftest totals unchanged;
- if .vpy harness is run, macro-on/off output must be byte-identical aside from the intentionally absent D-SUM family summary blocks.
```

S-series acceptance criterion for D-SUM-04 remains as designer specified:

```text
S1/S3/S7/S8 -r 1:
  pin_balance == 0
  lookup_ref_balance == 0
  D-SUM-05 violations == 0
  D-SUM-08 fields consistent with observed store/duplicate/AS2 activity
```

---

## 9. Designer decision requested

Please confirm one wording/meaning correction before patch generation:

```text
For D-SUM-08, should store_failures count only genuine store failures:

  store_status != ok && store_status != duplicate

rather than the v2 text's literal:

  store_status != ok

?
```

Coder recommendation: **yes, count only genuine failures and exclude duplicate**, because current source treats `duplicate` as a normal first-in-best-dressed store outcome eligible for authoritative return, and the diagnostics spec says duplicate counts are not automatically bad when ownership remains clean.

If accepted, I can generate the DIAG.2b patch against `src(16).zip` using the implementation map above.

---

## 10. Coordinator-safe command skeleton for the later patch phase

No patch is attached to this report. Once the DIAG.2b patch is generated, the coordinator-side safe apply sequence should be:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"

git status --short
git apply --check "C:\path\to\CMS07-DIAG2b-ownership-integrity-store.patch"
git apply "C:\path\to\CMS07-DIAG2b-ownership-integrity-store.patch"
git diff --check
git status --short
```

Normal four-way selftest commands after VS2026 Debug/Release rebuild:

```bat
cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"

x64\Debug\cnr3_cache_core_selftest.exe 1>NUL & echo exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe 1>NUL & echo exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL & echo exit_code=%ERRORLEVEL%
x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL & echo exit_code=%ERRORLEVEL%
```

Macro-off proof requires editing/commenting the relevant `#define CNR3_DIAG_COMPUTE_DSUMxx_*` lines in `src\cnr3_build_config.h`, rebuilding, and rerunning the same four-way commands for each required configuration. Do not commit those macro-off edits.
