# Document A - CNR3 Project Context and Rules v1.5

**Document type:** Stable project context and operating rules  
**Version:** v1.5  
**Date:** 2026-06-07  
**Status:** Current for the CNR3 handover pack v1.5  
**Companion design authority:** `cnr3_cache_manager_design_v6_3.md` / CMS06.3, or any later cache design spec that explicitly supersedes CMS06.3  
**Read order:** Read this document first, then Document B, then Document C, then CMS06.3, then current source files/logs.

---

## A1. Purpose of this document

This document gives a new AI chat or human maintainer the stable CNR3 project context and rules needed before touching code.

It is not a session diary. It should change only when project-wide rules, coding rules, architecture rules, or explanatory context change.

The goal is to prevent a new chat from:
- re-opening settled design decisions;
- treating the output cache as a mere optimisation;
- misunderstanding VapourSynth scheduling;
- breaking recursive predecessor correctness;
- breaking mutex, ownership, or reference-count discipline;
- confusing major CMS02 phases with local SubPhase notation;
- carrying forward stale implementation snapshots from older companion specs.

---

## A2. Project identity

CNR3 is a VapourSynth API4-only plugin. It is a recursive temporal chroma stabiliser inspired by CNR2/vscnr2-style processing.

The intended problem domain is analogue/video-capture chroma instability such as chroma shimmer, chroma noise, or temporal colour instability.

The project prioritises correctness, safety, and maintainability before parallel performance. Current development is staged so that the design can eventually operate safely under `fmParallel`, even though current implementation and proving may pass through `fmUnordered` and later `fmParallelRequests`.

---

## A3. Algorithmic core

The algorithm is recursive:

```text
output[N] depends on source[N] and already-filtered output[N - 1]
```

The predecessor is the already-filtered output frame, not merely the prior source frame. This makes the filter fragile under out-of-order frame requests.

A simple member variable such as `prev_output` only works when frames are processed strictly in ascending order. It is not a final scheduling authority for out-of-order or parallel operation.

---

## A4. Why VapourSynth scheduling is central

VapourSynth uses a frame-request model.

A filter receives `getFrame` calls in stages:
- `arInitial`: request-arrival stage; the filter asks upstream for required frames.
- `arAllFramesReady`: requested upstream/source frames are available; the filter may compute or return output.

A filter must not assume that frame `N - 1` has already been requested, computed, or cached merely because frame `N` is now being requested. Frame request order may differ from display order.

Example:

```text
Requested order:
    0, 1, 2, 3

Strict recursive processing works.

Requested order:
    0, 3, 1, 2

A naive recursive filter fails at frame 3 because output[2] does not yet exist.
```

---

## A5. VapourSynth filter modes relevant to CNR3

### fmUnordered

CNR3 currently uses `fmUnordered`.

In `fmUnordered`, only one `getFrame` call enters a filter instance at a time. This reduces concurrent-entry risk, but it does not guarantee ascending frame numbers. Request order may still be out of order.

`fmUnordered` is safer than fully concurrent modes, but it does not by itself solve the recursive predecessor problem.

### fmParallelRequests

In `fmParallelRequests`, multiple requests may be in flight and readiness order may interleave.

Cache metadata, hot zones, checkpoint pins, lookup-owned references, and pruning become more safety-critical. Request activity should be registered at `arInitial`. Predecessor availability must not rely on a single global previous-frame slot. Pruning must not evict frames needed by in-flight work.

Example:

```text
Request 100 begins.
Request 101 begins.
Request 100 has not completed.
Request 101 needs output[100].

The cache manager must not allow output[100] to be pruned, lost,
observed through a dangling pointer, or double-owned.
```

### fmParallel

Full `fmParallel` is the final long-term operational target, but it is not the current implementation target.

Metadata mutexes alone are not enough to prove recursive correctness under `fmParallel`. Correctness may require active-computation tracking, dependency waits, condition variables, or another explicitly designed authority model.

Known warning carried forward:
- `old_strict_cache.next_needed` is not designed for final `fmParallel`.
- `old_strict_cache.prev_output` / `next_needed` must be retired, bypassed, or redesigned before final `fmParallel` output authority.
- Current proof work must not introduce assumptions tied to those fields that would block migration to full `fmParallel`.

---

## A6. Why the cache manager exists

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
- no stale `cache_index` entries.

---

## A7. Prevailing interim and long-term goals

Interim goals:
- keep current development safe under `fmUnordered`;
- preserve the path toward `fmParallelRequests`;
- preserve the path toward final `fmParallel`;
- avoid designs that only work by relying on `old_strict_cache.next_needed` as final authority;
- prove safety with diagnostics before broadening output authority.

Long-term goal:
- operate safely under `fmParallel`, with a final authority model that does not depend on a single strict-streaming previous-frame slot.

---

## A8. Current high-level architecture

Major source areas:

```text
vapoursynth-Cnr3.cpp:
    Main VapourSynth lifecycle, parameters, getFrame path, diagnostics,
    proof scaffolds, and high-level processing flow.

cnr3_common.h:
    Shared data structures, including Cnr3Data and old/new cache members.

cnr3_frame_internal_processing.h/.cpp:
    Frame-internal processing, including explicit-predecessor processing
    boundary used by recovery proof work.

cnr3_response_tables.h/.cpp:
    Response table building and diagnostics.

cnr3_output_cache_manager.h/.cpp:
    CMS06.3 output-cache manager: ordered frame maps, non-checkpoint and
    checkpoint pools, cache_index, store/remove/lookup helpers, validation,
    hot zones, pruning, and reference-count diagnostics.

cnr3_build_config.h:
    Development/proof gates and edit/version markers.

cnr3_memory_diagnostics.h/.cpp:
    Memory diagnostics and summaries.
```

Historical names should be avoided in current descriptions unless explicitly documenting a rename.

---

## A9. Safety-critical rules

Mandatory cache/threading/ownership rules:

```text
- All mutable output-cache state must be protected by cache.cache_mutex.
- Public output-cache helpers lock internally unless documented otherwise.
- Helpers ending in _externally_locked must only be called while the caller already holds cache.cache_mutex.
- cache_index is non-owning.
- non_checkpoint_pool and checkpoint_pool own retained VSFrame references.
- Every cache-owned addFrameRef must be balanced by exactly one freeFrame.
- Lookup helpers returning a frame outside the mutex must return a caller-owned addFrameRef.
- The caller must free or transfer caller-owned lookup references on every exit path.
- Duplicate store is first-in-best-dressed: the first stored frame is the source of truth.
- Duplicate store must not take another addFrameRef.
- All pruning/removal must preserve ordered frame-number semantics.
- No dangling frames or dangling slots are acceptable.
```

Caller-side lookup invariant at quiescent points:

```text
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

---

## A10. Diagnostic philosophy

Instrumentation is proof of safety, not decoration.

Diagnostics must prove at least:
- store attempts/successes/failures;
- prune attempts/successes/failures;
- validation success/failure;
- integrity errors;
- cache-side reference balance;
- caller-side lookup reference balance;
- hot-zone behaviour;
- hard-ceiling aborts;
- teardown clear behaviour;
- memory state where relevant.

Hard gate:

```text
If safety diagnostics show unexpected values, stop.
Do not proceed until the discrepancy is understood.
```

Machine-readable one-line summaries should remain grep-friendly. When a one-line summary becomes too dense for human decision-making, add a human-readable final block rather than removing the one-line summary.

---

## A11. Coding Rule 1 - Code comments

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

## A12. Coding Rule 2 - Code update instructions

When producing code changes for a human to apply:
- each phase or set of changes, and each individual change within them, must be uniquely identifiable;
- state the file and function/location explicitly before each block;
- use before/after blocks showing exact existing code and exact replacement;
- include enough surrounding context to make the before block uniquely findable;
- the before block must match exactly one location in the named file;
- if a snippet would match multiple locations, extend the context until unique;
- the after block must be the exact intended replacement;
- avoid abstract insertion instructions unless backed by actual file context;
- when adding a new function, show the insertion point using the end of the preceding function and the start of the following function.

---

## A13. Required phase and SubPhase naming rules

Use expanded phase/subphase names in handover documents and chat responses.

Preferred form:

```text
CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run
CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof
```

Compact labels such as `CMS02-G.10D.7` may be used in log markers, compact tables, and commit titles. However, the expanded form must appear in handover documents and in commit bodies whenever compact notation could be misread.

Do not confuse local SubPhase notation such as `G10D.N` with major CMS02-D, CMS02-F, or CMS02-G phases.

---

## A14. Commit title and body rules

Development generally occurs in Visual Studio 2026 connected to GitHub.

When a development/test phase or SubPhase is marked PASS, provide a Visual Studio friendly commit text block unless the user asks otherwise.

Commit text format:
- first line is the one-line title;
- second line is blank;
- following lines are the commit body.

The title should include at least the phase/SubPhase. The body should explain intent, what changed, what was proven, and key test evidence.

Preferred title style:

```text
Complete CMS02-G / SubPhase G10D.7 recovery-return decision dry-run
```

---

## A15. New-chat boot sequence

A new chat should be instructed to:

1. Read `Document_A_CNR3_Project_Context_and_Rules_v1.5.md`.
2. Read `Document_B_CNR3_Decision_Log_v1.5.md`.
3. Read `Document_C_CNR3_Current_Session_Handover_v1.5.md`.
4. Read `cnr3_cache_manager_design_v6_3.md`.
5. Inspect current source files before proposing code changes.
6. Inspect latest logs if the task depends on proof evidence.
7. Treat Document C as current-state authority.
8. Treat Document B as settled-decision authority.
9. Treat CMS06.3 as detailed cache-manager design authority.
10. Stop and ask if current source/logs conflict with handover text.
