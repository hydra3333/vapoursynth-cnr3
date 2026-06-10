# Document B - CNR3 Decision Log

**Document:** B of CNR3 handover pack  
**Version:** v1.8  
**Date:** 2026-06-11  
**Status:** Stable decision record; update when major decisions change; continuity-preserved from v1.4 baseline with v1.8 decisions through CMS02-H15.5.  
**Companion design authority:** CMS06.7, or any later cache design specification that explicitly supersedes CMS06.7.  
**Current matched pack:** A/B/C v1.8

---

## B1. Purpose

This document prevents rabbit holes and accidental re-litigation of settled design decisions.

Each decision records:

```text
Decision:
    What was decided.

Rejected alternatives:
    What was considered and rejected.

Reason:
    Why the decision was made.

Implementation consequence:
    What the code must or must not do.

Reference:
    Design spec section, source file, log evidence, or handover section.
```

The implementation consequence field is mandatory because it tells the next AI what the decision means in code.

---

## D01 - API4 only; do not use `fmFrameState`

Decision:
    CNR3 is VapourSynth API4-only and must not use API3-era types or compatibility-only scheduling modes.

Rejected alternatives:
    Using `fmFrameState` to force old-style recursive scheduling.

Reason:
    `fmFrameState` is compatibility-only and unsuitable for new filters.

Implementation consequence:
    Use supported API4 scheduling. Do not solve recursion by reverting to deprecated scheduling assumptions.

Reference:
    Document A; source API policy comments.

---

## D02 - Strict-streaming bridge is temporary

Decision:
    The old strict-streaming cache remains the correctness bridge until output-cache cache-hit/recovery paths are proven and explicitly made authoritative.

Rejected alternatives:
    Immediately making the new output cache output-authoritative.

Reason:
    The recursive algorithm must remain correct while cache/recovery infrastructure is built and tested incrementally.

Implementation consequence:
    As of the CMS06.3/v1.6 state, normal `arAllFramesReady` cache hits may return cached output through the implemented `CACHE-HIT-RETURN` path. Cache misses still fall through to the strict-streaming/new-computation path. Recovered outputs are not yet generally returned as output-authoritative frames. Do not treat cache-hit reuse as completion of full recovery/output-cache authority.

Reference:
    Document C; `Cnr3Data`; `process_cnr3_frame`; CMS06.3.

---

## D03 - Output cache is per-instance, never global

Decision:
    Each CNR3 filter instance owns its own output cache.

Rejected alternatives:
    Global/shared output cache.

Reason:
    Different filter instances may process different clips, fields, or streams. Global cache state would cross-contaminate output and create unsafe threading/lifetime interactions.

Implementation consequence:
    `Cnr3Data` owns `Cnr3OutputCacheManager`. No static/global output frame cache.

Reference:
    `cnr3_common.h`; Document A.

---

## D04 - Ordered frame-number maps are required

Decision:
    Output cache pools use ordered frame-number containers for frame-number semantics.

Rejected alternatives:
    Insertion-order, unordered, or recency-based cache logic for predecessor/checkpoint decisions.

Reason:
    Recursive correctness depends on frame-number order. Nearest prior checkpoint means highest checkpoint frame number less than or equal to the requested frame, not most recently inserted.

Implementation consequence:
    All checkpoint lookup, pruning, and recovery logic must use frame-number ordering.

Reference:
    CMS06.3 Sections 2 and 4; `cnr3_output_cache_manager.*`.

---

## D05 - Pool slots own retained `VSFrame` references; `cache_index` is non-owning

Decision:
    `non_checkpoint_pool` and `checkpoint_pool` own cache-retained `VSFrame` references. `cache_index` only aliases those pointers.

Rejected alternatives:
    Letting `cache_index` own references or freeing pointers through multiple structures.

Reason:
    Multiple ownership paths would risk double-free, leaks, or dangling pointers.

Implementation consequence:
    Store takes `addFrameRef`. Remove/clear release pool-owned refs exactly once. `cache_index` entries must never be freed separately.

Reference:
    CMS06.3 Section 2.2; output-cache manager comments.

---

## D06 - Lookup helpers must addFrameRef while holding the mutex

Decision:
    A helper returning a cached frame for use after unlocking must call `addFrameRef` while still holding `cache_mutex`.

Rejected alternatives:
    Return a borrowed pointer and let the caller addref later.

Reason:
    Between unlocking and caller addref, prune/clear could remove the cache-owned reference and leave a dangling pointer.

Implementation consequence:
    Use `cnr3_output_cache_find_frame_and_add_ref()` for caller-owned lookup references. The caller must free or transfer every returned reference on every path.

Reference:
    CMS06.3 Section 2.1; `cnr3_output_cache_find_frame_and_add_ref`.

---

## D07 - Duplicate store is first-in-best-dressed

Decision:
    If `output[N]` already exists, a duplicate store returns success without replacing the cached frame and without taking another `addFrameRef`.

Rejected alternatives:
    Overwrite existing cached frame; reject duplicate store as hard failure; addref duplicate frame.

Reason:
    Overwrite risks leaks/dangling state. Rejection complicates overlapping recovery. Additional addref would break ownership balance.

Implementation consequence:
    First stored frame remains source of truth. Losing duplicate computation must release its own computed frame normally.

Reference:
    CMS06.3 Section 4.9.

---

## D08 - Sliding hot zones, not extend-only

Decision:
    Hot zones slide with request activity rather than extending indefinitely.

Rejected alternatives:
    Extend-only zones.

Reason:
    Extend-only zones eventually defeat pruning and memory bounds. Sliding zones protect current active work while allowing old regions to cool.

Implementation consequence:
    Hot-zone update should be frame-number based and request-driven. Pruning must avoid active hot zones.

Reference:
    CMS06.3 Section 4.2.

---

## D09 - Pruning must be hot-zone-aware and centralised through remove helper

Decision:
    Pruning selects candidates outside active hot zones and removes them through the single remove helper.

Rejected alternatives:
    Raw `erase()` from pools or cache index; lowest-frame-only eviction regardless of hot zones.

Reason:
    Raw erase would bypass `freeFrame` and index invariants. Lowest-frame-only eviction can remove frames still near active work.

Implementation consequence:
    Use central remove helper. Preserve cache-side reference balance and ordered frame-number semantics.

Reference:
    CMS06.3 Sections 4.3 and 4.7.

---

## D10 - Active ceiling is frame-count based, derived from 1 GiB geometry budget

Decision:
    Runtime cache ceiling is a frame-count limit computed at creation from estimated frame bytes and a 1 GiB nominal byte budget.

Rejected alternatives:
    Fixed frame ceiling independent of geometry; byte accounting on every store.

Reason:
    Format-aware frame count is simple and good enough for the first implementation. Per-store byte accounting is unnecessary complexity.

Implementation consequence:
    `cnr3_output_cache_set_ceiling()` computes `active_ceiling`. Store/prune logic enforces that ceiling.

Reference:
    CMS06.3 Section 4.6; `cnr3_output_cache_set_ceiling`.

---

## D11 - Store path prunes before hard-ceiling rejection

Decision:
    Store attempts pruning before rejecting due to hard ceiling.

Rejected alternatives:
    Reject immediately when current count is at the ceiling.

Reason:
    Safe prune may free a slot and allow store without abort.

Implementation consequence:
    Hard-ceiling abort should occur only after allowed pruning cannot make space.

Reference:
    CMS06.3 Section 4.6.

---

## D12 - Bounded recovery must be modelled carefully

Decision:
    Recovery is built in proof phases before any recovered output becomes authoritative.

Rejected alternatives:
    Implement full recovery immediately.

Reason:
    Recovery combines source-frame requests, checkpoint refs, output refs, source refs, cache store, pruning, and cleanup. Each lifetime rule must be proven separately.

Implementation consequence:
    Continue with staged proof work. Do not store/return recovered outputs until dry-run and local-compute proofs are clean.

Reference:
    CMS02-G.7/G.8/G.9AB; Document C.

---

## D13 - Non-checkpoint pinning is deferred unless diagnostics prove need

Decision:
    Non-checkpoint pinning is deferred.

Rejected alternatives:
    Add non-checkpoint pinning immediately.

Reason:
    Hot zones plus caller-owned `addFrameRef` may be sufficient for the near-term path. Non-checkpoint pinning adds complexity and should be triggered by evidence.

Implementation consequence:
    Promote non-checkpoint pinning only if diagnostics show predecessor-missing or prune-active-use hazards.

Reference:
    CMS06.3 Section 4.4.

---

## D14 - Diagnostics must prove safety before output-cache authority

Decision:
    Safety diagnostics must pass before advancing authority or parallelism.

Rejected alternatives:
    Trust code inspection alone.

Reason:
    Reference-count and scheduling bugs can be silent and catastrophic.

Implementation consequence:
    Treat unexpected non-zero error counters as a hard gate. Do not proceed until understood.

Reference:
    Document A; Document C hard gates.

---

## D15 - Hot-zone update belongs at `arInitial`

Decision:
    Request activity should update hot zones at `arInitial`.

Rejected alternatives:
    Defer hot-zone update to `arAllFramesReady`.

Reason:
    Under future `fmParallelRequests` or `fmParallel`, delaying hot-zone updates until readiness can be unsafe because multiple requests may be in flight and pruning must know about active request intent as early as possible.

Implementation consequence:
    Treat hot-zone-at-`arInitial` as a prerequisite before `fmParallelRequests`/`fmParallel` work. Do not move it back to `arAllFramesReady` without a new design decision.

Reference:
    G-PAR-HZ-ARINITIAL-01; CMS06.3 Sections 4.8, 13.3, and 14.4; Document C.

---

## D16 - Per-invocation `frameData` is preferred over shared current-request state

Decision:
    Source-request/recovery planning state must be carried per invocation, not as shared current-request state in `Cnr3Data`.

Rejected alternatives:
    Store current recovery/source-request plan in `Cnr3Data`.

Reason:
    Shared current-request state would become unsafe under future concurrent request modes.

Implementation consequence:
    Use `frameData` for arInitial-to-arAllFramesReady payloads. Keep recovery proof state local or per invocation.

Reference:
    CMS02-G.7A/G.7B/G.7C; `Cnr3ForDebugOnlyRecoverySourceRequestPlan`.

---

## D17 - Recovery proof paths must be disabled in normal committed state

Decision:
    Debug-only proof paths are committed disabled after proof runs.

Rejected alternatives:
    Leave enabled proof paths in normal state.

Reason:
    Enabled proof paths may request extra source frames, pin/unpin checkpoints, retrieve extra frames, or emit noisy logs. They are scaffolding, not normal runtime behaviour.

Implementation consequence:
    Normal committed state should have G.7/G.8/G.9 proof gates false unless a dedicated proof patch is active.

Reference:
    `cnr3_build_config.h`; Document C latest state.

---

## D18 - Recovery source-frame set is local and non-authoritative

Decision:
    The G.9 source-frame set is local to `arAllFramesReady` proof execution and is not stored in `Cnr3Data`.

Rejected alternatives:
    Keep a persistent source-frame bank in `Cnr3Data`.

Reason:
    Persistent shared recovery state would complicate concurrency and cleanup. The proof only needs to show local acquisition/release lifetime.

Implementation consequence:
    Future recovery source-frame handling must retain local/per-invocation ownership and release every acquired source frame on every path.

Reference:
    CMS02-G.9AB; `Cnr3ForDebugOnlyRecoverySourceFrameSet`.

---

## D19 - Dry-run compute orchestration must precede actual recovered-frame computation

Decision:
    CMS02-G.10ABC should be dry-run only.

Rejected alternatives:
    Immediately compute recovered frames from the G.9 source-frame set.

Reason:
    `process_cnr3_frame()` currently reads its predecessor from `d->old_strict_cache.prev_output`. Actual recovery computation needs a non-mutating processing boundary that accepts an explicit predecessor, or an equivalent safe design.

Implementation consequence:
    G.10ABC may log intended compute steps, but must not allocate recovered outputs, call the real processing function for recovery, store recovered outputs, return recovered outputs, or mutate old strict-streaming state.

Reference:
    `cnr3_frame_internal_processing.cpp`; CMS06.3 Section 13.4; Document C immediate next task.

---

## D20 - Compact/full diagnostic mode is deferred

Decision:
    A diagnostic verbosity redesign is deferred. This includes the remembered compact/expanded debug-options investigation.

Rejected alternatives:
    Redesign diagnostics during recovery proof phases.

Reason:
    Recovery proofs need detailed logs. Long-run tests will eventually need less noise, but adding that during the safety-critical proof chain risks distracting from current invariants.

Implementation consequence:
    Preserve existing proof logs. Add compile-time compact/expanded log-volume controls later under G-DIAG-LOG-VOLUME-01. Detailed proof logs must remain available when proving a specific cache/recovery invariant.

Reference:
    G-DIAG-LOG-VOLUME-01; Document C.

---

## D21 - Safe explicit-predecessor processing boundary is required before G.10D

Decision:
    Actual local recovery computation must not depend on or mutate `d->old_strict_cache.prev_output` or `d->old_strict_cache.next_needed`.

Rejected alternatives:
    Temporarily poke the strict-streaming previous-output slot so existing `process_cnr3_frame()` can be reused unchanged for recovery.

Reason:
    That shortcut would conflate local recovery state with the normal strict-streaming path. It could pass a simple smoke test while creating fragile ownership, ordering, and future-parallel hazards.

Implementation consequence:
    Before CMS02-G.10D begins, define or implement a safe processing boundary that computes `output[K]` from `source[K]`, an explicit previous filtered output frame for `K - 1`, and local per-invocation recovery state. The preferred, but not mandated, approach is to refactor or add a processing entry point that accepts an explicit predecessor `VSFrame*` parameter.

Reference:
    CMS06.3 Section 13.4; Document C immediate next task.



---

## D22 - Expanded phase/SubPhase naming is required

Decision:
    Handover documents and chat responses must use expanded phase/SubPhase naming where confusion is possible, for example `CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review`.

Rejected alternatives:
    Continue using only compact labels such as `CMS02-G.10D.8` in prose.

Reason:
    Compact labels made it too easy to confuse local G10D.N proof-step notation with major phases such as CMS02-D or CMS02-F.

Implementation consequence:
    Use expanded labels in handover documents and chat responses. Compact labels may remain in log markers, compact tables, and commit titles, but commit bodies and handover text must disambiguate when necessary.

Reference:
    CNR3_Handover_Pack_Production_Spec_v1.4; CMS06.3 changelog.

---

## D23 - Bounded-recovery sample differences are diagnostic, not automatic failure

Decision:
    `exact_match` in bounded-recovery comparison diagnostics is informational. Exact sample match must not be required as a bounded-recovery return condition unless a later explicit quality/tolerance decision changes that policy.

Rejected alternatives:
    Require every bounded-recovery output to be byte/sample-identical to continuous strict-streaming output before it may be considered returnable.

Reason:
    Bounded recovery from a checkpoint/back-window may legitimately have a different recursive predecessor history from continuous calculation from frame 0. A non-exact match can therefore be a valid property of the chosen bounded-recovery policy, not necessarily a safety failure.

Implementation consequence:
    Difference-measurement diagnostics must record sample differences, but `exact_match=0` must not by itself fail a bounded-recovery return proof. Structural errors, ownership errors, lookup failures where a candidate is required, and reference-balance failures remain hard failures.

Reference:
    CMS02-G / SubPhase G10D.6; CMS02-G / SubPhase G10D.8; CMS06.3 Section 13.8.

---

## D24 - Duplicate/recompute waste must be human-readable when used for decisions

Decision:
    The existing duplicate-store counters are necessary but not sufficient for human review. When duplicate/recompute waste is being used for a design or performance decision, provide a human-readable summary block with counts and percentages.

Rejected alternatives:
    Rely only on long one-line machine-readable output-cache summaries.

Reason:
    `duplicate_store_computed_but_discarded` and related counters quantify wasted duplicate computation caused by first-in-best-dressed store semantics. A human-readable block makes the waste profile auditable without manual grep/counting.

Implementation consequence:
    A near-term diagnostic task must add a formatted duplicate/recompute waste summary including store attempts, store successes, duplicate skipped already cached, duplicate computed but discarded, computed-discarded ratio, and useful-store ratio.

Reference:
    CMS06.3 Section 6 and Section 13.9; CNR3_Handover_Pack_Production_Spec_v1.4.

---

## D25 - CMS02-F status must be audited item-by-item, not treated as stale blanket text

Decision:
    CMS02-F must not be described from old design snapshots without checking the current source and logs. Its obligations must be audited item-by-item as completed, superseded/overtaken, still open, proof-evidence pending, or not applicable.

Rejected alternatives:
    Preserve stale CMS06.2-era wording that CMS02-F was simply not started.

Reason:
    Later source/proof work implemented or overtook several CMS02-F-labelled items, including the lookup/addref helper, caller-side lookup counters, and the normal `CACHE-HIT-RETURN` path at `arAllFramesReady`.

Implementation consequence:
    Current handovers must state that cache-hit return is implemented in code, while full recovery/output-cache authority is not complete. Any remaining CMS02-F-labelled obligations must be checked against the current source/logs before being treated as blockers.

Reference:
    CMS06.3 Sections 8, 14.3, 14.5, and 14.6; Document C v1.6 current status.

---

## D26 - `old_strict_cache` state is not final fmParallel authority

Decision:
    `old_strict_cache.next_needed` and `old_strict_cache.prev_output` are not suitable as final fmParallel output authority.

Rejected alternatives:
    Carry the strict-streaming predecessor slot and next-needed sequencing forward as the final parallel-ready authority model.

Reason:
    Those fields encode strict-streaming assumptions. They are useful during transitional proof work and cache-miss/new-computation fallback, but they are incompatible with final fully parallel output authority without redesign or retirement.

Implementation consequence:
    Future output-authority and fmParallel-readiness work must retire, bypass, or redesign these fields. New code must not deepen dependence on them in a way that blocks final fmParallel operation.

Reference:
    CMS06.3 Sections 1 and 14.6; named deferred cleanup item in Document C.


---

## D27 - Recovery/warm-up compute must reuse existing processing boundaries

Decision:
    Recovery, bounded warm-up, checkpoint recovery, and cache-fill compute
    should reuse existing frame-processing boundaries rather than duplicating
    pixel, blend, luma, chroma, downsampled-luma, scene-change, or copy logic.

Rejected alternatives:
    Adding separate manual copy or pixel-processing logic inside proof helpers,
    VapourSynth lifecycle code, or output-cache orchestration code.

Reason:
    Parallel calculation code risks drift from the production frame-processing
    path. H5 proved that bounded warm-up local compute can reuse the existing
    explicit-predecessor helper, including reset/start semantics, without
    duplicating pixel logic.

Implementation consequence:
    Prefer `process_cnr3_frame_with_explicit_previous_output(...)` for
    recovery/warm-up local compute. If the existing processing boundary is
    insufficient, design a small named processing-layer boundary explicitly
    instead of adding ad hoc pixel code to `vapoursynth-Cnr3.cpp`.

Override discipline:
    Do not depart from this decision silently. Any intentional departure
    requires explicit clarification, discussion, agreement, and documentation
    of the reason, scope, and expected safety impact before implementation
    proceeds.

Reference:
    CMS06.6 durable rule; CMS02-H / SubPhase H5 proof evidence.

## D28 - No parallel pixel/frame algorithms without explicit design agreement

Decision:
    Do not create parallel pixel/frame algorithms for recovery, bounded warm-up,
    checkpoint recovery, cache-fill, or future parallel-request work unless no
    safe existing processing boundary exists and the departure is explicitly
    agreed.

Rejected alternatives:
    Copying planes manually or implementing special-case pixel logic inside
    proof paths to make a phase convenient.

Reason:
    The project must prove orchestration, ownership, cache, scheduling, and
    authority transfer. It must not create subtly different frame algorithms in
    parallel with the real CNR3 processing layer.

Implementation consequence:
    New code must not duplicate blend, chroma, luma, downsampled-luma,
    scene-change, frame-copy, or pixel calculation logic without a named design
    decision and documentation.

Override discipline:
    Do not depart from this decision silently. Any intentional departure
    requires explicit clarification, discussion, agreement, and documentation
    of the reason, scope, and expected safety impact before implementation
    proceeds.

Reference:
    Handover pack production spec v1.5; CMS06.6 durable rule.

## D29 - Bounded-start S > 0 uses documented reset/start semantics unless superseded

Decision:
    When a no-prior-checkpoint bounded warm-up begins at `S > 0`, local
    output[S] uses explicit reset/start semantics. This is a bounded
    approximation start, not exact full-history recursion.

Rejected alternatives:
    Pretending that the bounded start is exact full-history recursion, or
    writing new manual copy logic for the start frame.

Reason:
    Exact recursion at S > 0 requires output[S - 1]. If no valid predecessor or
    checkpoint is available, the honest bounded behaviour is a documented
    reset/start approximation. Reusing the existing reset path avoids logic
    duplication.

Implementation consequence:
    Logs/summaries should distinguish `actual_source_frame`,
    `warmup_start_frame`, `processing_frame_number`,
    `predecessor_frame_number`, and `bounded_start_uses_frame0_reset_path`.

Override discipline:
    Do not depart from this decision silently. Any intentional departure
    requires explicit clarification, discussion, agreement, and documentation
    of the reason, scope, and expected safety impact before implementation
    proceeds.

Reference:
    CMS02-H / SubPhase H5 enabled proof; CMS06.6 bounded-start policy.

## D30 - Compute, store, return, transfer, and output authority remain separately provable

Decision:
    Local compute, cache store, return decision, return transfer, and general
    output-authority transition should remain separately provable unless the
    user explicitly agrees to combine a narrow low-risk step.

Rejected alternatives:
    Combining compute/store/return/authority changes into one broad phase where
    failure cause would be ambiguous.

Reason:
    CNR3 safety depends on knowing whether a failure is in computation,
    ownership, cache storage, lookup/transfer, or output-authority logic.

Implementation consequence:
    CMS02-H6 remains bounded-warmup-store-proof. CMS02-H7 remains
    bounded-warmup-return-decision-dry-run. H8/H9-style return transfer and
    output-authority readiness decisions remain deferred until after H7
    evidence.

Override discipline:
    Do not depart from this decision silently. Any intentional departure
    requires explicit clarification, discussion, agreement, and documentation
    of the reason, scope, and expected safety impact before implementation
    proceeds.

Reference:
    CMS06.6 phase structure and durable output-authority discipline rule.

## D31 - CMS02-J0 is mandatory before CMS02-J

Decision:
    Add `CMS02-J0 / pre-fmParallelRequests cleanup and observability review` as
    a mandatory checkpoint before `CMS02-J / fmParallelRequests wiring and
    proving`.

Rejected alternatives:
    Moving directly from CMS02-I into CMS02-J while accumulated proof scaffolds,
    diagnostic gates, obsolete proof helpers, and observability questions remain
    unresolved.

Reason:
    Before multiple in-flight requests are introduced, the codebase must have a
    clean observability and proof-support structure. Otherwise future failures
    may be obscured by stale proof scaffolds or inadequate counters.

Implementation consequence:
    CMS02-J must not start until CMS02-J0 has been evaluated and performed, or
    until specific intentional deferrals have been explicitly documented with
    reasons, scope, and expected safety impact.

Override discipline:
    Do not bypass CMS02-J0 silently. Any intentional deferral requires explicit
    clarification, discussion, agreement, and documentation of the reason,
    scope, and expected safety impact before CMS02-J proceeds.

Reference:
    CMS06.6 Section 8 / CMS02-J0; handover production spec v1.5 Section 2B.

## D32 - Future consolidated diagnostics prefer compile-time gating

Decision:
    New diagnostics should prefer compile-time gating where practical. Final
    consolidation into the intended compile-time/`if constexpr` model is a
    CMS02-J0 deliverable/requirement, not an immediate H-phase refactor.

Rejected alternatives:
    Requiring a broad diagnostics refactor immediately during active H-phase
    proof work, or designing final consolidated diagnostics to depend primarily
    on runtime debug state.

Reason:
    Making compile-time diagnostics a hard immediate requirement would invite
    refactoring churn during safety proof phases. CMS02-J0 is the correct
    cleanup and consolidation checkpoint.

Implementation consequence:
    During remaining H phases, prefer compile-time gating for new proof-only
    diagnostics where practical, but do not mix broad diagnostic redesign into
    unrelated safety phases. At CMS02-J0, perform the formal consolidation
    review.

Override discipline:
    Do not depart from this decision silently. Any intentional departure
    requires explicit clarification, discussion, agreement, and documentation
    of the reason, scope, and expected safety impact before implementation
    proceeds.

Reference:
    CMS06.6 CMS02-J0; designer review question 7.

## D33 - Old strict-streaming authority state must be reviewed before final parallel authority

Decision:
    Before final `fmParallelRequests` / `fmParallel` readiness and before final
    output-cache authority, review `old_strict_cache.next_needed`,
    `old_strict_cache.prev_output`, `process_cnr3_frame(...)`, and any path
    assuming serial output order.

Rejected alternatives:
    Allowing old strict-streaming authority state to silently remain load-bearing
    in a final parallel or output-cache-authoritative design.

Reason:
    `next_needed` and `prev_output` are strict-streaming concepts. They are not
    designed to be final authority under parallel request/order conditions.

Implementation consequence:
    These fields and compatibility wrappers must be retired, bypassed, or
    redesigned before final parallel/output-cache authority.

Override discipline:
    Do not depart from this decision silently. Any intentional retention or
    bypass requires explicit clarification, discussion, agreement, and
    documentation of the reason, scope, and expected safety impact before
    implementation proceeds.

Reference:
    CMS06.6 durable old-strict-state rule; prior D26.

## D34 - CMS02-H5 local compute proof is complete and H6 is next

Decision:
    CMS02-H / SubPhase H5 / bounded-warmup-local-compute-proof is complete and
    passed. The next CMS02-H phase is H6 / bounded-warmup-store-proof.

Rejected alternatives:
    Treating H5 as still pending after the enabled proof and disabled smoke, or
    combining H6 store proof with return-decision or output-authority changes.

Reason:
    H5 enabled proof showed balanced source frame retrieval/release, balanced
    local output allocation/release, zero compute/proof failures, and no store,
    return, output-authority, or old strict-state mutation. Disabled-state smoke
    restored proof gate to false and showed clean cache/ref cleanup.

Implementation consequence:
    Begin next work at CMS02-H / SubPhase H6 / bounded-warmup-store-proof.
    H6 must not return recovered output or change output authority.

Reference:
    CMS02-H5 proof logs; CMS06.6 current implementation state.


## D35 - Output-cache authority normal path supersedes H14 cutover terminology

Decision:
    H14 cutover terminology is now historical. The selected active path should
    use output-cache authority normal-path terminology unless a future phase is
    explicitly describing historical H14 proof evidence.

Rejected alternatives:
    Continuing to describe the selected active path as a temporary cutover path.

Reason:
    CMS02-H15.1 proved the terminology/scaffold transition without behavioural
    regression. Continuing to use cutover labels for the selected path would
    make future review confuse a normalising active path with an earlier proof
    scaffold.

Implementation consequence:
    New selected-path labels, gates, reasons, and comments should prefer
    `normal-path` or `sequential-fast-path` wording as appropriate. Do not
    resurrect `CUTOVER` labels in new active-path diagnostics.

Reference:
    CMS02-H15.1 validation; Document C v1.8 current-state section.

---

## D36 - Sequential predecessor-cache reuse is valid when output_cache[N-1] is present

Decision:
    For sequential frames after frame 0, cached output N-1 can be used as the
    explicit predecessor for computing output N when
    `cnr3_output_cache_find_frame_and_add_ref(N-1)` succeeds.

Rejected alternatives:
    Always recomputing a bounded warm-up source window even when output N-1 is
    already cached and available.

Reason:
    CMS02-H15.2 proved predecessor-cache lookup availability and balanced
    immediate release in sequential operation. CMS02-H15.3 then proved the
    intended reduced fast-path decision shape.

Implementation consequence:
    A sequential fast path may be selected only when the predecessor lookup
    succeeds and the caller proves release/transfer discipline. If predecessor
    lookup fails, fall back to bounded warm-up or later checkpoint recovery.

Reference:
    CMS02-H15.2 and CMS02-H15.3 validation; Document C v1.8 current-state
    section.

---

## D37 - Sequential fast path uses source N plus cached output N-1

Decision:
    The sequential fast-path computation for eligible N > 0 is allowed to
    acquire only source frame N and cached output frame N-1, then compute output
    N through the explicit-predecessor processing boundary.

Rejected alternatives:
    Creating a new pixel algorithm or relying on old strict prev_output state.

Reason:
    CMS02-H15.4 proved compute/store using cached output N-1 and current source
    N while preserving ownership balance and old strict-state quarantine.

Implementation consequence:
    The fast path must call the existing explicit-predecessor frame-processing
    boundary. It must not duplicate or fork pixel maths. It must release the
    predecessor lookup ref, current source ref, and local output ref on every
    non-transfer exit path.

Reference:
    CMS02-H15.4 validation; Document A durable rules; CMS06.7 Sections 2.2,
    4.5.1, and 13.12/13.13.

---

## D38 - Sequential fast-path return transfer is proven for eligible sequential frames

Decision:
    For eligible sequential frames after frame 0, the selected path may compute,
    store, look up, transfer, and return output N through the output-cache
    authority sequential fast path.

Rejected alternatives:
    Continuing to compute/store via the fast path but return only through the
    bounded-warmup normal path.

Reason:
    CMS02-H15.5 proved return-transfer mechanics with clean lookup-ref,
    cache-owned-frame, source-frame, local-output, and old strict-state
    accounting.

Implementation consequence:
    After a successful H15.5-style fast-path return-transfer, the bounded-warmup
    normal path must not also recompute/return the same eligible sequential
    frame. The fallback path remains required for frame 0, cache holes,
    non-sequential requests, and failed fast-path eligibility.

Reference:
    CMS02-H15.5 validation; Document C v1.8 current-state section.

---

## D39 - arInitial source-request reduction is not yet implemented after H15.5

Decision:
    H15.5 proves fast-path return transfer, but it does not yet fully optimise
    arInitial source requests. The next phase must address request-plan
    reduction separately.

Rejected alternatives:
    Treating H15.5 as final normal sequential operation.

Reason:
    VapourSynth requires any source frame retrieved in arAllFramesReady to have
    been requested in the same callback activation's arInitial. Changing that
    request plan is a separate safety-sensitive phase.

Implementation consequence:
    CMS02-H15.6 should first prove or stage the arInitial source-plan reduction
    for sequential fast-path candidates. Do not silently stop requesting bounded
    warm-up source frames without explicit H15.6/H15.7-style proof.

Reference:
    CMS06.7 Section 2.3; H15.5 validation; Document C v1.8 next-task section.

---

## D40 - Frame 0 remains a bounded-warmup normal-path fallback

Decision:
    Frame 0 has no predecessor and remains on the bounded-warmup normal path
    unless a later phase explicitly replaces that handling.

Rejected alternatives:
    Forcing frame 0 through the sequential fast path or pretending a predecessor
    exists.

Reason:
    The recursive dependency starts with an initial/reset frame. H15.2-H15.5 all
    correctly report frame 0 as ineligible for predecessor reuse.

Implementation consequence:
    Sequential fast-path eligibility requires N > 0 and successful predecessor
    lookup for output N-1. Frame 0 must retain reset/start handling.

Reference:
    CMS02-H15.2 through H15.5 validation; CMS06.7 Section 4.5.4.

---

## D41 - Old strict state remains quarantined after H15.5

Decision:
    H15.5 does not restore old strict authority. `old_strict_cache.next_needed`
    and `old_strict_cache.prev_output` remain quarantined from selected
    output-cache authority decisions.

Rejected alternatives:
    Using old strict state to decide sequential fast-path authority or to satisfy
    final output authority.

Reason:
    Old strict state is serial-order machinery and is incompatible with the
    final fmParallel target. H13 through H15.5 evidence keeps it non-mutating in
    the selected path.

Implementation consequence:
    Future H15.6+ and H16 phases must continue proving `mutates_old_strict=0`
    or equivalent. Any remaining old strict reads must be diagnostic or
    explicitly justified as temporary and non-authoritative.

Reference:
    CMS06.7 Sections 1, 13.17, and 14; H13-H15.5 validation.
