# Document A - CNR3 Project Context and Rules

**Document:** A of CNR3 handover pack  
**Version:** v1.9  
**Date:** 2026-06-11  
**Status:** Stable project context and operating-rules document; enhanced continuity-preserving update from v1.8, reconciled to CMS06.10 and the H15.6B predecessor-reservation decision.  
**Companion design authority:** CMS06.10, or any later cache design specification that explicitly supersedes CMS06.10.  
**Current matched pack:** A/B/C v1.9

---

## A0. v1.9 current-state override and non-regression note

This v1.9 pack is an enhanced checkpoint pack produced after CMS02-H16.3 and CMS02-H16.4 were completed and after the H15.6B design direction was revised by CMS06.10.

The controlling design authority for cache-manager work is now:

```text
cnr3_cache_manager_design_v6_10.md
```

CMS06.10 supersedes CMS06.9 and records the settled H15.6B predecessor-reservation design. Any earlier H15.6B wording based only on a fail-closed source-request reduction patch is superseded. The fail-closed-only H15.6B draft patch must not be committed or resumed as the implementation direction.

The current forward path is:

```text
H15.6B.1 / arInitial predecessor reservation lifecycle proof
H15.6B.2 / reserved-predecessor fast-path consumption proof
H15.6B.3 / active sequential source-request reduction
```

The key architectural change is that H15.6B must not merely check `output_cache[N-1]` in `arInitial` and look it up again in `arAllFramesReady`. Instead, `arInitial` must atomically find-and-addref `output_cache[N-1]`, carry the caller-owned predecessor ref in per-invocation `frameData`, and then `arAllFramesReady` must consume or release that carried ref with explicit ownership balance.

The following standing rules are now part of the project context and must be retained in future handovers and CMS updates:

```text
1. Lookup-addref ownership must be proven for ordinary cached-frame use.
2. Checkpoint pin/unpin ownership must be proven for checkpoint/recovery anchors.
3. Future cache-affecting phases must report both ownership systems where relevant.
4. The reserved-predecessor frameData field follows single-ownership/null-on-consume discipline.
5. The future H17 sparse-hole/minimal fallback recovery optimisation remains deferred.
```

### A0.1 Single-ownership/null-on-consume rule

When `frameData` owns a reserved predecessor `VSFrame*` reference:

```text
- the ref is owned by frameData while the field is non-null;
- successful fast-path consumption must release or otherwise consume that ref exactly once;
- successful consumption must set the frameData field to nullptr;
- the frameData destroy/release path releases the ref only if the field is still non-null;
- no path may both consume the ref and leave it non-null for cleanup;
- no path may assume another path releases it without a visible ownership transition.
```

This rule prevents both double-release and leak. It is a formal H15.6B.2 proof requirement.

### A0.2 Future H17 sparse-hole/minimal fallback recovery optimisation

The H16.3 predecessor-missing fallback is correctness-first and conservative. It may retrieve and recompute a bounded lower frame even when that output is already cached, relying on first-in-best-dressed duplicate handling to discard duplicate output safely. A later H17-style optimisation should start from the nearest suitable cached predecessor or checkpoint inside the bounded recovery window, retrieve only the missing source frames needed to walk forward, compute/store only missing outputs plus the requested output, and return through output-cache authority.

This is not part of H15.6B. Do not mix sparse-hole optimisation with H15.6B reservation or source-request reduction.

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

1. `Document_A_CNR3_Project_Context_and_Rules_v1.8.md`
2. `Document_B_CNR3_Decision_Log_v1.8.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.8.md`

Companion documents should be uploaded with this pack when relevant:

- the latest CMS06.3 or later cache design specification;
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

CMS06.3 must be followed before attempting future `fmParallelRequests` support. CMS06.3 also records that hot-zone request registration at `arInitial` is now implemented and must be preserved, and that actual recovery computation needs an explicit-predecessor processing boundary before CMS02-G.10D.

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

The output cache is being built and proven gradually. As of the v1.6 handover, normal `arAllFramesReady` cache hits may return a caller-owned cached frame through the implemented `CACHE-HIT-RETURN` path, while cache misses still fall through to the strict-streaming/new-computation path. Recovered outputs have not yet been generally returned as output-authoritative frames. This distinction matters: cache-hit reuse exists, but full recovery/output-cache authority is not complete.

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
    CMS06.3-or-later output-cache manager, including pools, index,
    checkpoint pinning, hot zones, store/prune, lookup addref helpers,
    cache-hit return support, validation, and diagnostics.

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


## A10A. Durable development rules and override discipline

These rules are project-wide development rules, not merely current-phase notes.
They apply to future recovery, bounded warm-up, checkpoint, output-cache,
`fmUnordered`, `fmParallelRequests`, and final `fmParallel` work unless a later
explicit decision supersedes them.

General override discipline for this section:

```text
Do not depart from these rules silently. Any intentional departure requires
explicit clarification, discussion, agreement, and documentation of the reason,
scope, and expected safety impact before implementation proceeds.
```

### A10A.1 Reuse existing processing boundaries; do not create parallel pixel algorithms

Recovery, bounded warm-up, checkpoint recovery, cache-fill, and future
`fmUnordered` / `fmParallelRequests` work must not duplicate blend, chroma,
luma, downsampled-luma, scene-change, frame-copy, or pixel calculation logic
unless no safe existing boundary exists.

Preferred rule:

```text
reuse existing frame-processing helpers;
make orchestration, ownership, cache, scheduling, and output-authority transfer
the things being proven.
```

The current preferred recovery/warm-up compute boundary is:

```cpp
process_cnr3_frame_with_explicit_previous_output(...)
```

Avoid using the old compatibility wrapper for recovery/warm-up compute:

```cpp
process_cnr3_frame(...)
```

because it remains tied to old strict-streaming predecessor state.

If existing helpers are insufficient, stop and design a small named
processing-layer boundary explicitly. Do not smuggle ad hoc pixel or copy logic
into VapourSynth lifecycle or cache proof code.

### A10A.2 Ownership and release-balance discipline

Every acquired or allocated frame must have a provable owner and a provable
release or transfer path.

Required rule:

```text
Every retrieved source frame must be released on every success, failure,
partial-acquire, early-return, and cleanup path.

Every temporary local output frame must be released on every success, failure,
partial-compute, early-return, and cleanup path unless a later phase explicitly
proves and documents ownership transfer.
```

Any phase involving source-frame retrieval, temporary local output allocation,
cache lookup refs, checkpoint pins, or cache-owned refs must include diagnostics
proving final balance at relevant quiescent points.

### A10A.3 Output-authority discipline

Compute, store, return decision, return transfer, and general output-authority
transition must remain separately provable unless explicit agreement says
otherwise.

A proof-only path must clearly state what it does and what it does not do.
Proof-only diagnostics should make clear whether the path:

```text
- computes local outputs;
- stores into output_cache;
- returns a recovered/cache output;
- transfers a lookup/local/cache ref to VapourSynth;
- changes output authority;
- mutates old strict-streaming state.
```

Do not combine local compute, cache store, return decision, return transfer,
and general output-authority transition if a failure would become ambiguous.

### A10A.4 Bounded-start honesty

When a no-prior-checkpoint bounded warm-up starts at `S > 0`, the start frame is
a bounded reset/start approximation unless a later phase proves exact
predecessor history. It must not be described as exact full-history recursion.

Logs or summaries should distinguish:

```text
actual_source_frame
warmup_start_frame
processing_frame_number
predecessor_frame_number
bounded_start_uses_frame0_reset_path
```

### A10A.5 CMS02-J0 mandatory pre-fmParallelRequests checkpoint

`CMS02-J0 / pre-fmParallelRequests cleanup and observability review` is a
mandatory checkpoint before `CMS02-J / fmParallelRequests wiring and proving`.

CMS02-J must not start until CMS02-J0 has been evaluated and performed, or until
specific intentional deferrals have been explicitly documented with reasons,
scope, and expected safety impact.

CMS02-J0 must review:

```text
- obsolete proof-only scaffolds;
- accumulated proof gates and proof-only data structures;
- diagnostic verbosity and compile-time gating;
- safety/health counters that should remain non-gated;
- high-volume traces and temporary maps that should be gated or removed;
- source lifecycle observability;
- hot-zone observability;
- checkpoint and non-checkpoint recovery observability;
- fill-holes, duplicate, and first-in-best-dressed observability;
- cache store/prune/remove/clear/ceiling observability;
- lookup-ref transfer/release observability;
- cache addFrameRef/freeFrame balance;
- missing predecessor/checkpoint/refusal diagnostics;
- memory diagnostics and summaries;
- whether mature proof/diagnostic helpers should be moved out of
  vapoursynth-Cnr3.cpp into dedicated diagnostic/observability translation units.
```

### A10A.6 Old strict-state final-goal review

Before final `fmParallelRequests` / `fmParallel` readiness and before final
output-cache authority, review whether the following are obsolete, hazardous, or
must be redesigned:

```text
old_strict_cache.next_needed
old_strict_cache.prev_output
process_cnr3_frame(...) compatibility wrapper
any path assuming serial output order
```

These must not silently remain authoritative in a final parallel design.

### A10A.7 Deferred and hard-gate item preservation

Named deferred items, cleanup obligations, fmParallel / fmParallelRequests
readiness blockers, diagnostic obligations, and future hard gates must not be
silently dropped. They must be preserved in Document C until completed,
superseded, explicitly retired, or deliberately deferred with documented
agreement.

Document A does not carry the exhaustive current deferred-item list, because
that list is current-state material and can change from phase to phase. The
detailed active list belongs in Document C, and any design-authoritative items
belong in the latest CMS06.x design specification.

Required rule:

```text
Current named deferred items are not listed exhaustively in Document A.
They must be carried in Document C and, where design-authoritative, in the
latest CMS06.x design specification.

If a future chat intends not to carry forward a named deferred item, cleanup
obligation, readiness blocker, diagnostic obligation, or hard gate, that
requires explicit clarification, discussion, agreement, and documentation of
the reason, scope, and expected safety impact before proceeding.
```

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

## A14. Required phase and SubPhase naming rules

Future handover documents must distinguish major CMS02 phases from local subphase/proof-step notation.

Use the expanded form wherever confusion is possible:

```text
CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run
CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof
```

Compact labels such as `CMS02-G.10D.7` may still appear in log markers, compact tables, and commit titles. However, the expanded form must appear in handover documents and in chat responses. If a compact commit title could be confused with a major phase label, include the expanded form in the commit body.

Do not confuse local SubPhase notation such as `G10D.N` with major phases such as `CMS02-D`, `CMS02-F`, or `CMS02-G`.

---

## A15. Commit title and body rules

Development normally occurs in Visual Studio 2026 with commits pushed to GitHub. When a development/test phase or SubPhase is marked PASS, provide a Visual Studio-compatible commit message unless the user asks otherwise.

The commit block format is:

```text
<one-line title>

<body text>
```

The title should identify the phase/SubPhase. The body should explain what the phase intended to prove, what changed, what the proof/test showed, and any disabled-state restoration or hard-gate status.

---

## A16. New-chat boot sequence

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.8.md`
2. `Document_B_CNR3_Decision_Log_v1.8.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.8.md`
4. CMS06.3 or later cache design specification
5. Current source files/logs relevant to the next task

Rules for the new chat:

```text
Treat Document C as the source of truth for current implementation status.
Treat Document B as the source of truth for settled decisions.
Treat CMS06.3-or-later as the detailed cache-manager design reference.
If CMS06.3-or-later and current code appear to conflict, stop and ask for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in Document C's do-not-implement section.
```


---

## A17. Continuity and non-regression note for v1.6

Document A v1.6 was based on the approved Document A v1.4 baseline, not on the abbreviated v1.5 draft. The v1.6 update is intentionally conservative: it preserves the v1.4 project-orientation material, examples, scheduling explanations, and safety rationale, while updating design authority wording to CMS06.3 and adding the phase/SubPhase naming and commit-message rules required by the approved handover production specification v1.4.


---

## A18. Continuity and non-regression note for v1.7

Document A v1.7 is continuity-preserved from v1.6. It adds the durable
development rules and override discipline required by
`CNR3_Handover_Pack_Production_Spec_v1.5.md`, and updates the design-authority
wording to CMS06.6.

The new durable-rule section is intended to prevent future chats from silently
reversing settled safety policy. In particular, it preserves the H5 learning
that recovery/warm-up compute should reuse existing frame-processing boundaries
and must not grow parallel pixel/copy algorithms inside cache proof code.


## A19. Continuity and non-regression note for v1.8

Version v1.8 updates the stable project context to match the post-CMS02-H15.5
state. It does not relax any durable rule from v1.7 and does not treat the
output-cache work as a mere optimisation.

Current stable interpretation for a new chat:

```text
- CMS06.7 is the current accepted cache-manager design authority available to
  this handover pack.
- A separate CMS06.8 proposed-delta document accompanies this pack. It is a
  proposal for designer/spec incorporation, not yet the accepted base authority
  unless explicitly adopted later.
- The selected output-cache authority path now has a proven sequential
  fast-path return-transfer path for eligible sequential frames after frame 0.
- Frame 0 still uses the bounded-warmup normal path.
- arInitial source-frame requests have not yet been reduced for the sequential
  fast path; CMS02-H15.6 is expected to address that next.
- old_strict_cache remains quarantined from selected output authority and must
  not regain authority silently.
- No fmParallelRequests or fmParallel readiness claim has been made.
```

This v1.8 note is a continuity addition only. It does not supersede detailed
current-state wording in Document C.


---

## A20. Continuity and non-regression note for v1.9

v1.9 preserves the v1.8 project context and adds the CMS06.10 design reconciliation. The most important changes are the H15.6B three-part split, the frameData reserved-predecessor ownership rule, the single-ownership/null-on-consume proof requirement, the lookup-addref/checkpoint-pin standing proof rule, and the future H17 sparse-hole optimisation note.

If a later handover is generated, these v1.9 additions must not be compressed away. They are safety-relevant and affect the next coding phase directly.
