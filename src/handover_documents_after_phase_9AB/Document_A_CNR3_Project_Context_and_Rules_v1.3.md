# Document A - CNR3 Project Context and Rules

**Document:** A of CNR3 handover pack  
**Version:** v1.3  
**Date:** 2026-06-06  
**Status:** Stable project context and operating-rules document; update rarely.  
**Companion design authority:** CMS06.1, or any later cache design specification that explicitly supersedes CMS06.1.  
**Current matched pack:** A/B/C v1.3

---

## A1. Purpose of this document

This document gives a new AI chat or human maintainer the project context needed to safely continue CNR3 development.

A new chat must assume it remembers nothing from earlier chats. This document explains:

- what CNR3 is;
- why the project exists;
- what VapourSynth is doing that makes CNR3 difficult;
- why recursive temporal filtering is fragile under out-of-order scheduling;
- why the output cache manager is correctness-critical rather than a performance convenience;
- what safety rules must not be violated;
- what coding and patch-instruction rules must be followed.

This document is part of a three-document handover pack:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.3.md`
2. `Document_B_CNR3_Decision_Log_v1.3.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.3.md`

Companion documents should be uploaded with this pack when relevant:

- the latest CMS06.1 or later cache design specification;
- current relevant `.h` and `.cpp` files;
- latest build/test logs where the next task depends on them;
- any review or simulation plan required by the current phase.

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

A naive previous-frame-only implementation fails at frame 3 because the correct filtered `output[2]` does not yet exist. It may reject the request, use the wrong predecessor, or corrupt the recursive chain.

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

CNR3 is intended especially for analogue/video-capture material where chroma shimmer, dot-crawl-like instability, or temporal chroma noise can be reduced by reusing controlled amounts of previous filtered chroma.

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

## A5. Why VapourSynth scheduling is central

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
- `arAllFramesReady` is not the same thing as frame N being in display order.
- A filter must not assume `N - 1` has already been requested or produced.
- A filter must not assume only nearby frame numbers will be requested.
- A filter must not allow pruning to ignore active requests.
- A recursive filter must deliberately manage predecessor availability.

Hot-zone activity should be registered at `arInitial`, not deferred to `arAllFramesReady`, before future `fmParallelRequests` or `fmParallel` work.

---

## A6. VapourSynth filter modes relevant to CNR3

### A6.1 `fmUnordered`

Current CNR3 uses `fmUnordered`.

In `fmUnordered`:

- only one `getFrame` call enters a given filter instance at a time;
- this reduces concurrent entry risk;
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

The current old strict-streaming bridge can still reject out-of-order frame requests. That bridge is temporary and remains the authoritative returned-output path until cache-hit/recovery paths are proven and explicitly switched on.

### A6.2 `fmParallelRequests`

In `fmParallelRequests`:

- multiple requests may be in flight;
- request and readiness order may interleave;
- a frame may be requested before its predecessor output has been computed;
- cache metadata, hot zones, pins, lookup-owned references, and pruning become more safety-critical;
- request activity must be registered early, at `arInitial`;
- predecessor availability must not rely on a single global previous-frame slot;
- pruning must not evict frames needed by in-flight work.

Example:

```text
Request 100 begins.
Request 101 begins.
Request 100 has not completed.
Request 101 needs output[100].

The cache manager must not allow output[100] to be pruned, lost,
observed through a dangling pointer, or double-owned.
```

CMS06.1 must be followed before attempting future `fmParallelRequests` support.

### A6.3 `fmParallel`

Full `fmParallel` is a significant final design goal, but it is out of scope for immediate implementation unless a later design specification explicitly authorises it.

In `fmParallel`:

- fully concurrent processing is more difficult;
- metadata mutexes alone are not enough to prove recursive correctness;
- correctness may require active-computation tracking, dependency waits, or condition variables;
- the cache-manager metadata helpers can be designed to remain thread-safe, but full algorithmic correctness still needs a later dedicated design review.

Interim development may pass through `fmUnordered` and `fmParallelRequests`, but design and coding must avoid choices that block eventual safe `fmParallel` operation unless explicitly justified and recorded.

---

## A7. Why the cache manager exists

The cache manager is a correctness subsystem, not a performance cache.

It exists to support:

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
- no stale cache index entries.

The output cache is being built and proven gradually. While it is not output-authoritative, it may still store and prune real produced frames for proving. It must not affect returned pixels until the current handover and code explicitly say output authority has moved.

---

## A8. Prevailing interim and long-term goals

Interim goals:

- remain correct under `fmUnordered` now;
- build output-cache infrastructure that is structurally compatible with future `fmParallelRequests`;
- keep mutex, ownership, cleanup, and frameData placement choices compatible with the final `fmParallel` target where practical;
- prove each ownership and lifetime rule before adding the next layer.

Long-term goal:

- operate safely under `fmParallel`, subject to a later explicit design spec and review.

Current development posture:

```text
safe under fmUnordered now
structurally compatible with future fmParallelRequests
final design target: fmParallel
```

---

## A9. Current high-level architecture

Major source areas:

```text
vapoursynth-Cnr3.cpp:
    Main VapourSynth lifecycle, parameter parsing, getFrame path,
    cache orchestration, debug/proof scaffolding, plugin registration,
    and teardown.

cnr3_common.h:
    Shared data structures, including Cnr3Data. Cnr3Data is per-instance
    and owns both the old strict-streaming bridge and the new output cache.

cnr3_frame_internal_processing.h/.cpp:
    Per-frame pixel/plane work. This layer performs luma copy, downsampled
    luma buffers, scene-change detection, and recursive chroma processing.
    It must not own cache policy or VapourSynth scheduling policy.

cnr3_response_tables.h/.cpp:
    Response-table building and table diagnostics.

cnr3_output_cache_manager.h/.cpp:
    CMS06.1-or-later output-cache manager, including pools, index,
    checkpoint pinning, hot zones, store/prune, lookup addref helpers,
    validation, and diagnostics.

cnr3_memory_diagnostics.h/.cpp:
    Development memory diagnostics.

old_cnr3_strict_cache.h/.cpp:
    Old strict-streaming cache bridge retained while output_cache is not yet
    output-authoritative.
```

Avoid old names in current code descriptions unless explicitly documenting historical renames.

---

## A10. Safety-critical rules

Mandatory cache/threading/reference rules:

- All mutable output-cache state must be protected by `cache.cache_mutex`.
- Public output-cache helpers lock internally unless documented otherwise.
- Helpers ending in `_externally_locked` must only be called while the caller already holds `cache.cache_mutex`.
- `cache_index` is non-owning.
- `non_checkpoint_pool` and `checkpoint_pool` own retained `VSFrame` references.
- Every cache-owned `addFrameRef` must be balanced by exactly one `freeFrame`.
- Lookup helpers returning a frame outside the mutex must return a caller-owned `addFrameRef`.
- The caller must free or transfer caller-owned lookup references on every exit path.
- Duplicate store is first-in-best-dressed: the first stored frame is the source of truth.
- Duplicate store must not take another `addFrameRef`.
- All pruning/removal must preserve ordered frame-number semantics.
- No dangling frames or dangling slots are acceptable.
- No stale `cache_index` entries are acceptable.
- Public helpers must not deadlock by calling other locking public helpers while already holding the mutex.
- `_externally_locked` helpers may call other `_externally_locked` helpers only under the documented lock ownership.

Hard gate:

```text
If safety diagnostics show unexpected values, stop.
Do not proceed until the discrepancy is understood.
```

---

## A11. Diagnostic philosophy

Instrumentation is proof of safety, not decoration.

Diagnostics must prove:

- store attempts/successes/failures;
- prune attempts/successes/failures;
- validation success/failure;
- integrity errors;
- cache-side reference balance;
- caller-side lookup reference balance when lookup-owned references are active;
- checkpoint pin/unpin balance;
- hot-zone behaviour;
- hard-ceiling aborts;
- teardown clear behaviour;
- memory state where relevant.

Expected hard gates at quiescent points:

```text
invariants_ok=1
integrity_errors=0
validation_failures=0
ref_balance_errors=0
lookup_ref_balance=0 when lookup-owned references are involved
cache addref/free balance equals live cached slots before clear
cache addref/free balance is 0 after clear
clear_successes=1 after teardown when cached frames existed
```

Diagnostics may be verbose during proof phases. Longer tests need compile-time diagnostic verbosity controls later, but detailed proof logs must remain available when proving a specific invariant.

---

## A12. Coding Rule 1 - Code comments

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

## A13. Coding Rule 2 - Code update instructions

When producing code changes for a human to apply:

- Each phase or set of changes, and each individual change within them, must be uniquely identifiable.
- State the file and function/location explicitly before each block.
- Use before/after blocks showing exact existing code and exact replacement.
- Always include enough surrounding context to make the before block uniquely findable.
- The before block must match exactly one location in the named file.
- If a snippet would match multiple locations, extend the context until unique.
- The after block must be the exact intended replacement.
- Avoid abstract "insert near" instructions unless backed by actual file context and clear enough for a human reader.
- When adding a new function, show the insertion point using the end of the preceding function and the start of the following function.

---

## A14. New-chat boot sequence

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.3.md`
2. `Document_B_CNR3_Decision_Log_v1.3.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.3.md`
4. CMS06.1 or later cache design specification
5. Current source files/logs relevant to the next task

Rules for the new chat:

```text
Treat Document C as the source of truth for current implementation status.
Treat Document B as the source of truth for settled decisions.
Treat CMS06.1-or-later as the detailed cache-manager design reference.
If CMS06.1-or-later and current code appear to conflict, stop and ask for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in Document C's do-not-implement section.
```
