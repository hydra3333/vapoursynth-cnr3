# CNR3 Project Context and Rules

**Document:** A of CNR3 handover pack  
**Version:** v1.1  
**Date:** 2026-06-03  
**Status:** Stable context document; update rarely.  
**Companion design authority:** CMS06, or any later cache design spec that explicitly supersedes CMS06.

---

## A1. Purpose of this document

This document gives a new AI chat or human maintainer the project context needed to safely continue CNR3 development.

A new chat must assume it remembers nothing from earlier chats. This document explains:

- what CNR3 is;
- why the project exists;
- what VapourSynth is doing that makes CNR3 difficult;
- why recursive temporal filtering is fragile under out-of-order scheduling;
- why the CMS cache manager is correctness-critical rather than a performance convenience;
- what safety rules must not be violated;
- what coding and patch-instruction rules must be followed.

This document is part of a three-document handover pack:

1. `CNR3_Project_Context_and_Rules_v1.1.md`
2. `CNR3_Decision_Log_v1.1.md`
3. `CNR3_Current_Session_Handover_v1.1.md`

Companion documents should be uploaded with this pack when relevant:

- the latest CMS06 or later cache design specification;
- current relevant `.h` and `.cpp` files;
- latest build/test logs where the next task depends on them.

The handover pack is designed to let a new chat safely resume development without re-opening settled decisions or making unsafe suggestions about recursion, cache ownership, mutex locking, `VSFrame` reference counts, pruning, or VapourSynth frame scheduling.

---

## A2. Extended project-orientation preamble

This section is intentionally detailed. It exists so that a new AI or human maintainer understands the project before touching code.

CNR3 is not a normal stateless image filter. It is a recursive temporal filter. The output for one frame depends on the already-filtered output of a previous frame. That makes CNR3 unusually sensitive to VapourSynth scheduling, cache lifetime, reference ownership, and request ordering.

The core danger is simple:

```text
output[N] depends on output[N - 1]
```

The predecessor is not merely `source[N - 1]`. It is the already-filtered output frame `N - 1`.

If VapourSynth requests frames in display order, a naive implementation can appear to work:

```text
0, 1, 2, 3, 4
```

But a modern VapourSynth graph is not required to request frames in that order. A filter may see:

```text
0, 3, 1, 2, 4
```

A naive "previous frame only" implementation fails at frame 3 because the correct filtered `output[2]` does not yet exist. It may reject the request, use the wrong predecessor, or corrupt the recursive chain.

The CMS output cache exists to solve this correctness problem. It is not a performance cache. It is a correctness subsystem that must eventually provide safe cache-hit reuse, checkpoint recovery, bounded warm-up recovery, hot-zone-aware pruning, reference-count discipline, and future parallel-request safety.

This also means instrumentation is not optional. Diagnostics are proof of safety. If reference counts, prune counters, validation counters, or hot-zone counters show unexpected values, the next phase must stop until the discrepancy is understood.

---

## A3. Project identity

CNR3 is a VapourSynth API4-only chroma stabiliser inspired by the CNR2/vscnr2 temporal chroma-stabilisation algorithm.

The project aims to:

- redevelop the old CNR2/vscnr2-style behaviour in modern C++;
- target VapourSynth API4 only;
- avoid API3-era types, assumptions, and scheduling shortcuts;
- preserve recursive chroma-stabilisation correctness before pursuing parallel performance;
- support integer YUV formats only;
- operate safely under modern VapourSynth frame-request patterns;
- provide strong diagnostics for cache, threading, memory, and reference-count behaviour.

CNR3 is intended especially for analogue/video-capture material where chroma shimmer, dot crawl-like instability, or temporal chroma noise can be reduced by reusing controlled amounts of previous filtered chroma.

---

## A4. Algorithmic core

CNR3 is a recursive temporal chroma filter.

The load-bearing fact is:

```text
output[N] depends on source[N] and output[N - 1]
```

The predecessor `output[N - 1]` must be the already-filtered predecessor, not merely the previous source frame.

At a high level:

```text
For Y:
    copy source luma unchanged.

For U/V:
    compare current source chroma against previous filtered chroma.
    compare current downsampled luma against previous downsampled luma.
    use signed-difference response tables for Y and chroma.
    blend current source chroma with previous filtered chroma according to the combined response.
```

A simplified conceptual form is:

```text
weight = response_y(diff_y) * response_chroma(diff_chroma)

output_chroma[N] =
    weighted blend of:
        previous filtered output_chroma[N - 1]
        current source_chroma[N]
```

Scene-change detection matters because carrying previous filtered chroma across a true scene cut can smear or contaminate the new scene. When scene-change detection fires, the current design copies current source chroma and skips recursive chroma blending for that frame.

---

## A5. What VapourSynth is doing

VapourSynth evaluates a filter graph by asking filters for frames. CNR3 participates in that graph through a `getFrame` callback.

At a practical level:

```text
arInitial:
    VapourSynth is asking the filter to start work for frame N.
    The filter normally requests any upstream/source frames it needs.

arAllFramesReady:
    The requested upstream/source frames are available.
    The filter can read source frame data, produce output, and return a frame.
```

Important implications:

- `arInitial` is request-arrival.
- `arAllFramesReady` is not the same thing as "frame N is in display order."
- A filter must not assume `N - 1` has already been requested or produced.
- A filter must not assume only nearby frame numbers will be requested.
- A filter must not allow pruning to ignore active requests.
- A recursive filter must deliberately manage predecessor availability.

Current CNR3 now updates output-cache hot zones at `arInitial`, so request activity is registered as early as possible.

---

## A6. VapourSynth filter modes relevant to CNR3

### A6.1 `fmUnordered`

Current CNR3 uses `fmUnordered`.

In `fmUnordered`:

- only one `getFrame` call enters a given filter instance at a time;
- this protects per-instance mutable state from concurrent `getFrame` entry;
- it does not guarantee in-order frame numbers;
- VapourSynth may still request frames out of display order.

Example:

```text
Requested order:
    0, 1, 2, 3

Strict recursive processing works.

Requested order:
    0, 3, 1, 2

A naive recursive filter fails at frame 3 because output[2] does not yet exist.
```

`fmUnordered` is safer than fully concurrent modes, but it does not by itself solve recursive predecessor availability.

The current old strict-streaming bridge can reject out-of-order frame requests. That bridge is temporary and remains output-authoritative until CMS06 cache-hit and recovery paths are proven.

### A6.2 `fmParallelRequests`

In `fmParallelRequests`:

- multiple requests may be in flight;
- request and readiness order may interleave;
- a frame may be requested before its predecessor output has been computed;
- cache metadata, hot zones, pins, lookup-owned references, and pruning become more safety-critical;
- request activity must be registered early, at `arInitial`.

Example:

```text
Request 100 begins.
Request 101 begins.
Request 100 has not completed.
Request 101 needs output[100].

The cache manager must not allow output[100] to be pruned, lost,
observed through a dangling pointer, or double-owned.
```

CMS06 must be followed before attempting future `fmParallelRequests` support. The latest code alignment has already moved hot-zone updates to `arInitial`, which is required before future `fmParallelRequests` work.

### A6.3 `fmParallel`

Full `fmParallel` is explicitly out of scope for the current iteration.

It would require stronger reasoning about fully concurrent readers and writers, active computation state, dependency waits, or condition variables. Mutex-protected cache metadata alone does not prove full recursive correctness under `fmParallel`.

Do not implement full `fmParallel` support unless a later design spec explicitly says to do so.

---

## A7. Why the cache manager exists

The cache manager is not an optional optimisation. It is a correctness subsystem.

It exists because:

- CNR3 is recursive;
- output frame `N` depends on filtered output frame `N - 1`;
- VapourSynth may request frames out of order;
- the current strict-streaming bridge rejects out-of-order requests;
- a production-quality recursive API4 filter needs safe cache-hit reuse, checkpoint recovery, bounded warm-up recovery, hot-zone pruning, and eventually safe parallel-request behaviour.

The cache manager must support:

- safe predecessor availability;
- safe cache-hit reuse;
- checkpoints;
- holes;
- bounded recovery;
- pruning;
- bounded memory use;
- future parallel-request operation;
- no dangling frame pointers;
- no leaked `VSFrame` references;
- no double frees;
- no stale `cache_index` entries.

Unsafe cache changes can cause:

- incorrect recursive output;
- use-after-free;
- dangling cached frame pointers;
- leaked `VSFrame` references;
- double frees;
- silent chroma corruption;
- deadlocks;
- failures that only appear under different request schedules.

---

## A8. What safety means in this project

For CNR3 cache work, safety means:

- no dangling frames;
- no dangling cache slots;
- no leaked `VSFrame` references;
- no double-free;
- no unsafe prune of needed predecessors;
- no stale `cache_index` entries;
- correct mutex ownership;
- correct `_externally_locked` helper use;
- no public-helper deadlocks caused by nested public locking helpers;
- `addFrameRef`/`freeFrame` ownership is provably balanced;
- lookup-owned references are freed or transferred on every exit path;
- failure paths clean up all owned resources.

Hard rule:

```text
If safety diagnostics show unexpected values, stop.
Do not proceed until the discrepancy is understood.
```

---

## A9. Current high-level architecture

Current naming direction:

```text
vapoursynth-Cnr3.cpp:
    Main VapourSynth lifecycle, parameters, getFrame path, diagnostics, pixel processing.

cnr3_common.h:
    Shared data structures, including Cnr3Data and old/new cache members.

cnr3_response_tables.h/.cpp:
    Response table building and table diagnostics.

cnr3_output_cache_manager.h/.cpp:
    CMS06 output-cache manager.

old_cnr3_strict_cache_*:
    Old strict-streaming cache functions retained while output_cache is not yet output-authoritative.
```

Current source of returned output:

```text
old_strict_cache remains output-authoritative.
output_cache stores/prunes real produced frames for proving only.
```

---

## A10. Safety-critical rules

The following rules are load-bearing:

```text
- All mutable output-cache state must be protected by cache.cache_mutex.
- Public output-cache helpers lock internally unless documented otherwise.
- Helpers ending in _externally_locked must only be called while the caller already holds cache.cache_mutex.
- cache_index is non-owning.
- non_checkpoint_pool and checkpoint_pool own retained VSFrame references.
- Every cache-owned addFrameRef must be balanced by exactly one freeFrame.
- Lookup helpers returning a frame outside the mutex must return a caller-owned addFrameRef.
- The caller must free caller-owned lookup references on every exit path.
- Duplicate store is first-in-best-dressed: the first stored frame is the source of truth.
- Duplicate store must not take another addFrameRef.
- All pruning/removal must preserve ordered frame-number semantics.
- No dangling frames or dangling slots are acceptable.
```

---

## A11. Diagnostic philosophy

CNR3 must be exceptionally well instrumented.

Diagnostics must prove:

- store attempts/successes/failures;
- prune attempts/successes/failures;
- checkpoint pin/unpin behaviour;
- cache-owned add/free reference balance;
- caller-side lookup reference balance when lookup begins;
- validation success/failure;
- integrity errors;
- hot-zone behaviour;
- hard-ceiling aborts;
- teardown clear behaviour;
- memory snapshots where relevant.

Diagnostic output must never go to stdout. Debug/status output goes to stderr. VapourSynth user-facing errors use `mapSetError()` or `setFilterError()`.

Current diagnostic policy after the latest work:

```text
Compact output-cache trace:
    every processed frame.

Full output-cache summary:
    after create;
    frame 0;
    frame 1;
    every 100th frame;
    one frame before final;
    final frame;
    before free;
    after output_cache clear;
    any store/prune failure.
```

Future option, not yet implemented:

```text
Add compact/full diagnostic modes later if long logs become impractical.
```

---

## A12. Coding Rule 1 — Code comments

Comments should be concise, relevant, and informative for future maintainers.

Avoid undue blank lines, repeated wording, excessive prose, and design-history prose inside code.

Do not over-compress safety-critical comments. Any comment that, if misread or removed, could lead a future maintainer to make an unsafe edit, must retain enough detail to preserve its exact meaning. This applies especially to:

- locking and threading invariants;
- ownership and lifetime rules;
- reference-count discipline;
- non-obvious preconditions and postconditions;
- invariants whose violation would cause silent bugs rather than immediate crashes.

Compress freely where code is self-explanatory. Preserve detail wherever a future edit guided by an incomplete comment could introduce a hard-to-find bug.

---

## A13. Coding Rule 2 — Code update instructions

When producing code changes for a human to apply:

- Each phase or set of changes, and each individual change within them, must be uniquely identifiable.
- State the file and function/location explicitly before each block.
- Use before/after blocks showing exact existing code and exact replacement.
- Include enough surrounding context to make the before block uniquely findable.
- The before block must match exactly one location in the named file.
- If a snippet would match multiple locations, extend the context until unique.
- The after block must be the exact intended replacement.
- Avoid abstract "insert near" instructions unless backed by actual file context.
- When adding a new function, show the insertion point using the end of the preceding function and the start of the following function.

---

## A14. New-chat boot sequence

At the start of a new chat, upload/read in this order:

1. `CNR3_Project_Context_and_Rules_v1.1.md`
2. `CNR3_Decision_Log_v1.1.md`
3. `CNR3_Current_Session_Handover_v1.1.md`
4. latest CMS06 or later cache design spec
5. current relevant source files
6. latest logs if the next task depends on test evidence

Instruction to the new chat:

```text
Treat the Current Session Handover as the source of truth for current status.
Treat the Decision Log as the source of truth for settled decisions.
Treat CMS06 or later as the detailed cache-manager design authority.
If the design spec and current code appear to conflict, stop and ask for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
```
