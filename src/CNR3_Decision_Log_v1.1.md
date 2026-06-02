# CNR3 Decision Log

**Document:** B of CNR3 handover pack  
**Version:** v1.1  
**Date:** 2026-06-03  
**Status:** Stable decision record; update when major decisions change.  
**Companion design authority:** CMS06, or any later cache design spec that explicitly supersedes CMS06.

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

The “Implementation consequence” field is mandatory because it tells the next AI what the decision means in code.

---

## D01 — API4 only; do not use `fmFrameState`

Decision:
CNR3 is VapourSynth API4-only and must not use API3-era types or compatibility-only scheduling modes.

Rejected alternatives:
Using `fmFrameState` to force old-style recursive scheduling.

Reason:
API4 documentation treats `fmFrameState` as compatibility-only and unsuitable for new filters.

Implementation consequence:
Use supported API4 scheduling. Do not solve recursion by reverting to deprecated scheduling assumptions.

Reference:
Project context and VapourSynth API4 policy.

---

## D02 — Strict-streaming bridge is temporary

Decision:
The old strict-streaming cache remains the correctness bridge until CMS06 cache-hit/recovery paths are proven.

Rejected alternatives:
Immediately making the new output cache output-authoritative.

Reason:
The recursive algorithm must remain correct while CMS06 infrastructure is built and tested incrementally.

Implementation consequence:
Until explicitly changed, returned output remains strict-path output. CMS06 output cache may store/prune for proving but must not affect returned pixels.

Reference:
Current session handover; CMS06 Sections 8, 14.

---

## D03 — CMS06 output cache is per-instance, never global

Decision:
Each CNR3 filter instance owns its own output cache.

Rejected alternatives:
Global/shared output cache.

Reason:
Different filter instances may process different clips, fields, or streams. Global cache state would cross-contaminate output and create unsafe threading/lifetime interactions.

Implementation consequence:
`Cnr3Data` owns the output cache. No static/global frame cache.

Reference:
`Cnr3Data`; `Cnr3OutputCacheManager`.

---

## D04 — Ordered frame-number maps are required

Decision:
Output cache pools use ordered frame-number containers.

Rejected alternatives:
Insertion-order, unordered, or recency-based cache logic for predecessor/checkpoint decisions.

Reason:
Recursive correctness depends on frame-number order. Nearest prior checkpoint means highest checkpoint frame number less than the requested frame, not most recently inserted.

Implementation consequence:
All checkpoint lookup, pruning, and recovery logic must use frame-number ordering.

Reference:
CMS06 Sections 2, 4, 9.

---

## D05 — Pool slots own retained `VSFrame` references; `cache_index` is non-owning

Decision:
`non_checkpoint_pool` and `checkpoint_pool` own cache-retained `VSFrame` references. `cache_index` only aliases those pointers.

Rejected alternatives:
Letting `cache_index` own references or freeing pointers through multiple structures.

Reason:
Multiple ownership paths would risk double-free, leaks, or dangling pointers.

Implementation consequence:
Store takes `addFrameRef`. Remove/clear release pool-owned refs exactly once. `cache_index` entries must never be freed separately.

Reference:
CMS06 Section 2.2.

---

## D06 — Lookup helpers must addFrameRef while holding the mutex

Decision:
A helper returning a cached frame for use after unlocking must call `addFrameRef` while still holding `cache_mutex`.

Rejected alternatives:
Return a borrowed pointer and let caller addref later.

Reason:
The frame could be pruned between lookup and caller addref.

Implementation consequence:
Lookup helper name must make ownership explicit, for example `find_frame_and_add_ref`. Caller must `freeFrame` the returned reference on every exit path.

Reference:
CMS06 Sections 2.1, 2.2, 9.2.

---

## D07 — Duplicate store is first-in-best-dressed

Decision:
If a frame number is already stored, duplicate store returns success but does not replace the stored frame and does not take another `addFrameRef`.

Rejected alternatives:
Treat duplicate store as failure, replace the existing frame, or take an additional retained ref.

Reason:
Duplicate computation can occur under race/recovery conditions. The first stored frame is the source of truth. Replacing or double-retaining risks reference-count corruption.

Implementation consequence:
Duplicate store increments duplicate-store diagnostics and returns true as an idempotent no-op.

Reference:
CMS06 Sections 2.2, 4.9.

---

## D08 — Sliding hot zones, not extend-only

Decision:
CMS06 uses sliding hot zones.

Rejected alternatives:
Extend-only zones.

Reason:
Extend-only zones can grow unbounded and prevent effective pruning. Sliding zones better match active work regions and are compatible with the rolling predecessor/reference model.

Implementation consequence:
Hot-zone update logic slides or allocates zones; pruning protects active hot-zone frames.

Reference:
CMS06 Sections 4.1, 4.2, Appendix A.

---

## D09 — Pruning must be hot-zone-aware and centralised through remove helper

Decision:
Pruning skips protected frames and removes frames through the central remove helper.

Rejected alternatives:
Direct arbitrary erasure from pools during pruning.

Reason:
Central removal preserves `cache_index` consistency, pool ownership, pinned checkpoint rules, and `freeFrame` accounting.

Implementation consequence:
Normal prune paths call the `_externally_locked` remove helper while holding `cache_mutex`.

Reference:
CMS06 Sections 4.3, 12.

---

## D10 — Active ceiling is a frame-count limit derived from 1 GiB geometry budget

Decision:
Runtime cache ceiling is a simple frame-count limit. It is derived at filter creation from clip geometry, bit depth, and a nominal 1 GiB byte budget, then clamped.

Rejected alternatives:
Hardcoded fixed frame count only, or runtime byte-accounting for every store.

Reason:
A frame-count ceiling is simple and robust at runtime, while geometry-based derivation adapts to clip size and format.

Implementation consequence:
`active_ceiling = clamp(byte_budget / estimated_frame_bytes, min=150, max=1000)`.

Reference:
CMS06 Section 4.6.

---

## D11 — Store path prunes before hard-ceiling rejection

Decision:
If a store would exceed `active_ceiling`, the store helper attempts hot-zone-aware pruning first and rejects only if pruning fails or the cache would still exceed the ceiling.

Rejected alternatives:
Immediate hard rejection before pruning.

Reason:
CMS06 should reject only after it cannot free safe candidates.

Implementation consequence:
The ceiling check remains before `addFrameRef`, preserving reference safety, but calls prune helpers first.

Reference:
CMS06 Section 4.6.

---

## D12 — Bounded recovery must be modelled carefully

Decision:
Bounded recovery is accepted in principle but must not be implemented as a simplistic unlimited rebuild or naive ad hoc walk.

Rejected alternatives:
Rebuild from frame 0, unlimited recursion, or unmodelled recovery walk.

Reason:
Recovery is the most complex part of making the recursive algorithm work under out-of-order scheduling.

Implementation consequence:
Implement bounded warm-up only when CMS06 specifies; use rolling predecessor references, fill-holes-only behaviour, final-frame ownership transfer, and complete cleanup on every error path.

Reference:
CMS06 Sections 4.5, 4.6.3.

---

## D13 — Non-checkpoint pinning is deferred

Decision:
Non-checkpoint pinning is deliberately deferred.

Rejected alternatives:
Add non-checkpoint pinning preemptively.

Reason:
It adds complexity and should only be introduced if testing proves hot zones, addref discipline, and existing checkpoint pinning are insufficient.

Implementation consequence:
Do not implement non-checkpoint pinning unless diagnostics such as `predecessor_missing_when_expected` prove it is needed.

Reference:
CMS06 Section 4.4.

---

## D14 — Diagnostics must prove safety before output-cache authority

Decision:
Before the output cache can become output-authoritative, diagnostics must prove store, prune, validation, reference balance, teardown, and later lookup ownership behaviour.

Rejected alternatives:
Enable cache reads or recovery before instrumentation proves basic safety.

Reason:
An unsafe recursive cache can silently corrupt output or crash later.

Implementation consequence:
Store/prune proving and teardown proof must remain green before cache-hit reuse. Cache-hit reuse then needs its own design-compliance review before recovery work.

Reference:
CMS06 Sections 8, 12, 14.

---

## D15 — Hot-zone update belongs at `arInitial`

Decision:
Hot-zone updates should occur at `arInitial`, when VapourSynth first requests a frame.

Rejected alternatives:
Updating hot zones only in `arAllFramesReady`.

Reason:
Under `fmUnordered`, the old placement was safe. Under future `fmParallelRequests`, request activity must be registered before concurrent cache-hit, store, recovery, or prune decisions can race around that request.

Implementation consequence:
`cnr3_output_cache_update_hot_zones()` is now called at `arInitial`. It must not also be called later for the same frame.

Reference:
CMS06 Section 4.8; latest current-session handover log evidence.

---

## D16 — Compact/full diagnostic mode is deferred

Decision:
A future compact/full diagnostic mode is reasonable but not implemented now.

Rejected alternatives:
Adding diagnostic modes immediately before the next functional cache phase.

Reason:
Current diagnostics are verbose but useful during cache proving. Adding debug-level modes now could distract from cache-hit/recovery work.

Implementation consequence:
Keep compact per-frame trace and throttled full summaries for now. Revisit diagnostic modes only if longer logs become impractical.

Reference:
Current session handover.
