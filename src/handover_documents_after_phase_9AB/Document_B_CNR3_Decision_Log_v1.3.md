# Document B - CNR3 Decision Log

**Document:** B of CNR3 handover pack  
**Version:** v1.3  
**Date:** 2026-06-06  
**Status:** Stable decision record; update when major decisions change.  
**Companion design authority:** CMS06.1, or any later cache design specification that explicitly supersedes CMS06.1.  
**Current matched pack:** A/B/C v1.3

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
    Until explicitly changed, returned output remains strict-path output. The output cache may store/prune/prove behaviour but must not replace returned pixels.

Reference:
    Document C; `Cnr3Data`; `process_cnr3_frame`; CMS06.1.

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
    CMS06.1 Sections 2 and 4; `cnr3_output_cache_manager.*`.

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
    CMS06.1 Section 2.2; output-cache manager comments.

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
    CMS06.1 Section 2.1; `cnr3_output_cache_find_frame_and_add_ref`.

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
    CMS06.1 Section 4.9.

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
    CMS06.1 Section 4.2.

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
    CMS06.1 Sections 4.3 and 4.7.

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
    CMS06.1 Section 4.6; `cnr3_output_cache_set_ceiling`.

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
    CMS06.1 Section 4.6.

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
    CMS06.1 Section 4.4.

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
    G-PAR-HZ-ARINITIAL-01; CMS06.1 Section 4.8; Document C.

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
    `cnr3_frame_internal_processing.cpp`; Document C immediate next task.

---

## D20 - Compact/full diagnostic mode is deferred

Decision:
    A diagnostic verbosity redesign is deferred.

Rejected alternatives:
    Redesign diagnostics during recovery proof phases.

Reason:
    Recovery proofs need detailed logs. Long-run tests will eventually need less noise, but adding that during the safety-critical proof chain risks distracting from current invariants.

Implementation consequence:
    Preserve existing proof logs. Add compile-time log-volume controls later under G-DIAG-LOG-VOLUME-01.

Reference:
    G-DIAG-LOG-VOLUME-01; Document C.
