# Document B - CNR3 Decision Log v1.5

**Document type:** Stable decision log  
**Version:** v1.5  
**Date:** 2026-06-07  
**Status:** Current for the CNR3 handover pack v1.5  
**Companion design authority:** `cnr3_cache_manager_design_v6_3.md` / CMS06.3

---

## B1. Purpose

This document records major settled decisions and why alternatives were rejected.

It is not a session diary. It should be updated only when a significant architectural, safety, scheduling, cache, diagnostic, or implementation-process decision is made.

Each decision has an implementation consequence so a new chat knows what the code must or must not do.

---

## Decision D01 - API4 only; do not use fmFrameState

Decision:
    CNR3 is VapourSynth API4-only. Do not use API3-era headers, types, or compatibility-only scheduling such as `fmFrameState`.

Rejected alternatives:
    Use older compatibility scheduling to hide recursive-ordering complexity.

Reason:
    VapourSynth API4 documentation treats `fmFrameState` as compatibility-only. CNR3 must be modern API4 code.

Implementation consequence:
    Use API4 headers and API4 lifecycle. Scheduling correctness must come from cache/recovery design, not `fmFrameState`.

Reference:
    Document A; CMS06.3.

---

## Decision D02 - Strict-streaming bridge is temporary

Decision:
    The old strict-streaming path exists as a correctness bridge, not final architecture.

Rejected alternatives:
    Treat `old_strict_cache.prev_output` and `old_strict_cache.next_needed` as permanent output authority.

Reason:
    Strict streaming works only under ascending processing. It is incompatible with final `fmParallel` authority.

Implementation consequence:
    Do not add final-design assumptions tied to `old_strict_cache.next_needed` or `prev_output`. Recovery code must use explicit predecessor handling.

Reference:
    CMS06.3 Sections 1, 13.4, 14.6.

---

## Decision D03 - Output cache is per-instance, never global

Decision:
    Output cache state is per filter instance.

Rejected alternatives:
    Global cache shared across streams/instances.

Reason:
    VapourSynth may instantiate filters multiple times, including field-separated interlaced processing. Global state risks cross-stream corruption.

Implementation consequence:
    Cache state belongs to `Cnr3Data` / `Cnr3OutputCacheManager` per instance.

Reference:
    CMS06.3; current architecture.

---

## Decision D04 - Ordered frame-number maps are required

Decision:
    Cache state must be strictly ordered by frame number.

Rejected alternatives:
    Unordered or insertion-order cache structures as primary authority.

Reason:
    Recovery, checkpoint lookup, pruning, and hot-zone logic require deterministic frame-number ordering.

Implementation consequence:
    All cache pools/indexes and pruning logic must preserve frame-number ordering semantics.

Reference:
    CMS06.3.

---

## Decision D05 - Pool slots own retained VSFrame references; cache_index is non-owning

Decision:
    `non_checkpoint_pool` and `checkpoint_pool` own cache-retained `VSFrame` references. `cache_index` aliases slots and does not own frames.

Rejected alternatives:
    Allow cache_index to own or free frames.

Reason:
    Multiple ownership paths create leaks/double-free risk.

Implementation consequence:
    Cache-owned `addFrameRef` occurs only in store. Cache-owned `freeFrame` occurs only through remove/clear helpers.

Reference:
    CMS06.3 Section 2.2.

---

## Decision D06 - Lookup helpers must addFrameRef while holding the mutex

Decision:
    A helper returning a cached frame outside `cache_mutex` must take `addFrameRef()` while still holding `cache_mutex`.

Rejected alternatives:
    Return borrowed pointers after releasing the mutex.

Reason:
    A prune after unlock could free the cached slot, leaving a dangling pointer.

Implementation consequence:
    Use `cnr3_output_cache_find_frame_and_add_ref()` for public lookups. Caller must release or transfer the caller-owned reference.

Reference:
    CMS06.3 Sections 2.1, 2.2.

---

## Decision D07 - Duplicate store is first-in-best-dressed

Decision:
    Store is idempotent by frame number. First stored frame wins; duplicate store returns success without taking ownership.

Rejected alternatives:
    Replace existing cached frames or take another `addFrameRef` for duplicates.

Reason:
    Replacement or double-reference breaks ownership and can leak or double-free.

Implementation consequence:
    On duplicate store: do not replace, do not addFrameRef, increment duplicate counters, and caller still owns its supplied frame.

Reference:
    CMS06.3 Section 4.9.

---

## Decision D08 - Sliding hot zones, not extend-only

Decision:
    Use sliding hot zones to represent current working ranges.

Rejected alternatives:
    Extend-only zones that grow indefinitely.

Reason:
    Extend-only zones defeat pruning. Sliding zones match linear encode/jitter behaviour while bounding memory.

Implementation consequence:
    Hot-zone update belongs at `arInitial`; do not move it back to `arAllFramesReady`.

Reference:
    CMS06.3 Sections 4.2, 4.8.

---

## Decision D09 - Pruning must be hot-zone-aware and centralised through remove helper

Decision:
    Pruning must avoid active hot zones and must remove frames through the central remove helper.

Rejected alternatives:
    Direct pool/index erasure or pruning solely by lowest frame number.

Reason:
    Direct erasure risks stale indexes and leaked/freed frames. Lowest-first can evict needed predecessors or checkpoints.

Implementation consequence:
    Use hot-zone-aware candidate selection and `cnr3_output_cache_remove_frame_externally_locked()`.

Reference:
    CMS06.3 Sections 4.3, 2.2.

---

## Decision D10 - Active ceiling is frame-count based, derived from 1 GiB geometry budget

Decision:
    Runtime hard ceiling is a frame count derived from actual format geometry and a nominal 1 GiB byte budget.

Rejected alternatives:
    Fixed byte accounting per store/prune or small fixed frame counts.

Reason:
    Frame-count ceiling is simple and sufficient for current proof work. Geometry-derived count avoids obvious under/over-allocation.

Implementation consequence:
    Keep current active ceiling calculation unless a later explicit decision changes it.

Reference:
    CMS06.3 Section 4.6.

---

## Decision D11 - Store path prunes before hard-ceiling rejection

Decision:
    Store attempts should prune eligible frames before rejecting due to hard ceiling.

Rejected alternatives:
    Reject immediately when the cache appears full.

Reason:
    A safe prune may make space without aborting.

Implementation consequence:
    Store/prune helpers must preserve invariants and ref balance on both success and failure paths.

Reference:
    CMS06.3 Section 4.6.

---

## Decision D12 - Bounded recovery must be modelled carefully

Decision:
    Bounded recovery is permitted as an explicit trade-off, but must be measured and documented.

Rejected alternatives:
    Assume bounded recomputation is always byte-identical to continuous strict streaming.

Reason:
    Recursive predecessor history can differ when recomputing from a bounded checkpoint/window rather than from frame 0.

Implementation consequence:
    Record sample differences. Do not use exact sample match as a required bounded-recovery return condition.

Reference:
    CMS06.3 Sections 13.8, 14.1.

---

## Decision D13 - Non-checkpoint pinning is deferred unless diagnostics prove need

Decision:
    Non-checkpoint pinning remains deferred.

Rejected alternatives:
    Add non-checkpoint pinning immediately before diagnostics prove hot zones are insufficient.

Reason:
    Hot zones plus caller-owned lookup refs may be enough for current stages. Extra pinning adds complexity.

Implementation consequence:
    Promote non-checkpoint pinning to mandatory if CMS06.3 trigger conditions appear, especially `predecessor_missing_when_expected != 0` in realistic tests.

Reference:
    CMS06.3 Section 4.4.

---

## Decision D14 - Diagnostics must prove safety before output-cache authority

Decision:
    Output-cache authority must be expanded only after diagnostics prove safety.

Rejected alternatives:
    Trust intended ownership logic without instrumentation.

Reason:
    Cache, pruning, and reference-count errors can be silent and severe.

Implementation consequence:
    Before each authority expansion, require clean store/prune/validation/integrity/ref-balance/lookup-balance evidence.

Reference:
    CMS06.3 Sections 6, 14.

---

## Decision D15 - Bounded recovery sample differences are diagnostic, not automatic failure

Decision:
    `exact_match` between a bounded-recovery output and continuous strict-streaming output is diagnostic only.

Rejected alternatives:
    Require `exact_match=1` before bounded recovery output can be considered returnable.

Reason:
    Bounded recovery from a checkpoint/back-window can legitimately differ from continuous recursive calculation from frame 0 because predecessor history may differ.

Implementation consequence:
    Difference-measurement diagnostics must record sample differences, but `exact_match=0` must not by itself fail a bounded-recovery return proof unless a later explicit quality/tolerance decision changes policy.

Reference:
    CMS06.3 Section 13.8.

---

## Decision D16 - Use expanded phase/SubPhase naming

Decision:
    Handover documents and chat responses must use expanded naming such as `CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review`.

Rejected alternatives:
    Use only compact labels such as `CMS02-G.10D.8`.

Reason:
    Compact notation caused confusion between major CMS02 phases and local proof SubPhases.

Implementation consequence:
    Compact labels may remain in logs/tables/commit titles, but expanded names must appear in handover documents and whenever ambiguity is possible.

Reference:
    Handover Pack Production Spec v1.3 Section 3.7; CMS06.3 changelog.

---

## Decision D17 - Duplicate/recompute waste needs a human-readable summary

Decision:
    Raw duplicate-store counters are not enough for human decision-making. Add a human-readable duplicate/recompute waste summary block as a near-term required diagnostic.

Rejected alternatives:
    Rely only on a very long machine-readable one-line summary.

Reason:
    The one-line summary is grep-friendly but difficult for a human to use when judging algorithmic waste.

Implementation consequence:
    Preserve the one-line summary, but add a multi-line final block with store attempts, successes, duplicate skips, computed-discarded counts, and percentages.

Reference:
    CMS06.3 Sections 6, 13.9; Handover Pack Production Spec v1.3.

---

## Decision D18 - Cnr3OwnedFrameRef is recommended but not yet mandatory

Decision:
    `Cnr3OwnedFrameRef` remains recommended, not mandatory, while explicit ref handling remains clean.

Rejected alternatives:
    Block all further proof work until RAII wrapper is implemented.

Reason:
    Explicit release/transfer handling has been proven clean so far. RAII becomes mandatory if balance errors or unreliable exit paths appear.

Implementation consequence:
    Continue explicit handling only while the invariant remains clean. If acquired/released/transferred balance fails or code review finds uncovered exit paths, implement RAII before further authority expansion.

Reference:
    CMS06.3 Section 2.2 and Section 9.1.

---

## Decision D19 - CMS02-F status must be audited item-by-item, not treated as stale "not started"

Decision:
    CMS02-F is substantially implemented in source snapshots reviewed, but not identical to final output-cache authority.

Rejected alternatives:
    Preserve CMS06.2 "CMS02-F not started" wording without qualification.

Reason:
    Current source includes cache-hit lookup, lookup-ref transfer accounting, and `CACHE-HIT-RETURN`. Cache misses still fall through to strict-streaming computation.

Implementation consequence:
    Handover docs must say: CMS02-F cache-hit return is implemented; full recovery/output-cache authority is not complete; any remaining CMS02-F-labelled obligations must be audited item-by-item against current source/logs.

Reference:
    CMS06.3 changelog and Sections 8, 14.3.
