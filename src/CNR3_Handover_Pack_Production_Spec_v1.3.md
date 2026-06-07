# CNR3 Handover Pack Production Specification

**Document type:** Specification for producing future CNR3 handover packs  
**Applies to:** Future CNR3 handover packs, not to a single coding session only  
**Version:** v1.3
**Date:** 2026-06-07
**Status:** Current handover-pack production standard
**Current design authority wording:** CMS06.2 or any later cache design spec that explicitly supersedes CMS06.2

---

## 1. Purpose of This Specification

This specification defines the required structure, purpose, and content of future CNR3 handover packs.

The handover pack exists so that a new AI chat, with no memory of previous chats, can safely and accurately resume CNR3 development without:

- re-opening settled design decisions;
- losing implementation constraints;
- misunderstanding the project purpose;
- treating the cache manager as a mere optimisation;
- suggesting unsafe changes to the cache manager, threading model, reference-count discipline, or VapourSynth scheduling model;
- failing to understand which state is current and which design notes are historical.

The goal is not to make the handover as short as possible. The goal is to make it:

- current;
- structured;
- precise;
- resistant to stale or contradictory interpretation;
- sufficiently detailed for safe C++ development;
- compact enough that the most important information remains visible and usable;
- explicitly grounded in companion design documents, current source code, and current logs.

A single handover document cannot reliably serve every purpose. Stable project context, stable decision rationale, and volatile session state have different lifetimes and must not be mixed into one long diary-style document.

The CNR3 handover pack therefore consists of:

1. `Document_A_CNR3_Project_Context_and_Rules_<version>.md`
2. `Document_B_CNR3_Decision_Log_<version>.md`
3. `Document_C_CNR3_Current_Session_Handover_<version>.md`
4. Companion reference documents, especially the latest CMS06.2-or-later cache design specification
5. Current source files and logs relevant to the next task

The intended new-chat boot sequence is:

1. Read `Document_A_CNR3_Project_Context_and_Rules_<version>.md`.
2. Read `Document_B_CNR3_Decision_Log_<version>.md`.
3. Read `Document_C_CNR3_Current_Session_Handover_<version>.md`.
4. Read the latest CMS06.2-or-later cache design document if the task touches cache management.
5. Inspect the latest relevant source files before proposing code changes.
6. Inspect latest logs if the task depends on test evidence.
7. Treat the current session handover as the source of truth for current implementation state.
8. Treat the decision log as the source of truth for settled decisions.
9. Treat CMS06.2-or-later as the detailed design reference for cache-manager behaviour.
10. Do not re-litigate settled decisions unless current code, logs, or tests prove a genuine problem.

This structure is designed to be successful for handover from one chat to the next because it gives a new AI three different kinds of information in the order it needs them:

- the project’s purpose, environment, and danger profile;
- the reasons behind settled decisions;
- the exact current implementation state and next task.

This is especially important for CNR3 because the cache manager is not a minor optimisation. It is a correctness mechanism required to make a recursive temporal filter operate safely under modern VapourSynth scheduling.

---

## 2. Required Handover Pack Files

A complete handover pack contains exactly these three authored handover documents:

```text
A. Document_A_CNR3_Project_Context_and_Rules_<version>.md
B. Document_B_CNR3_Decision_Log_<version>.md
C. Document_C_CNR3_Current_Session_Handover_<version>.md
```

The same version number must be used for A, B, and C in a matched pack.

Example:

```text
Document_A_CNR3_Project_Context_and_Rules_v1.2.md
Document_B_CNR3_Decision_Log_v1.2.md
Document_C_CNR3_Current_Session_Handover_v1.2.md
```

The latest version of the handover pack should normally also be bundled into a zip file eg:

```text
CNR3_Handover_Pack_v1.2.zip
```

Companion documents are not part of the three-document handover pack, but are required when relevant. These include:

```text
latest CMS06.2-or-later cache design spec
latest source files relevant to the next task
latest logs relevant to the next task
review/simulation plans when applicable
```

---

# 3. Document A — `Document_A_CNR3_Project_Context_and_Rules_<version>.md`

## 3.1 Purpose

Document A is the stable preamble and operating-rules document.

It seeks to provide context and relevant information to facilitate understanding for both humans and AIs.    
It must not lose context or detail without excellent reason.    
It should change rarely.    
It should be based on the previous version of Document A to ensure no loss of context or detail; generally, some prior version were reviewed and approved.    
It should not contain very long wordy prose without excellent reason.    
It should not be re-summarized (generally losing relevant detail) without excellent reason.    

It should be uploaded at the start of every new CNR3 development chat.

Its purpose is to give a new AI or human maintainer enough grounding to understand:

- what CNR3 is;
- why the project exists;
- what VapourSynth is doing that makes the project difficult;
- why recursive temporal filtering is fragile under out-of-order scheduling;
- why cache management is central to correctness;
- evolving relevant interim and long term goals (eg operating safely under fmUnordered, fmParallelRequests, fmParallel);
- what safety constraints are non-negotiable;
- what coding and handover rules must be followed.

Document A is not a session log. It must not contain rapidly changing phase status except for broad architectural facts.

## 3.2 Mandatory extended project-orientation preamble

Document A must contain an extended project-orientation preamble.

This preamble is not optional. It must be detailed enough that a new AI chat or human maintainer, starting with no prior memory, understands the project before touching code.

The preamble must not be a short executive summary. It must give enough background for a reader to understand why CNR3 is fragile, why VapourSynth scheduling matters, why the cache manager exists, and why instrumentation is a safety requirement.

At minimum, the preamble must explain all of the following.

### 3.2.1 What CNR3 is

Document A must explain that CNR3 is:

- a VapourSynth plugin;
- VapourSynth API4-only;
- a recursive temporal chroma stabiliser;
- inspired by CNR2/vscnr2-style processing;
- intended for analogue/video-capture chroma instability such as shimmer, chroma noise, or temporal colour instability;
- prioritising correctness and safety before parallel performance.

### 3.2.2 What VapourSynth is doing

Document A must explain at a practical level:

- VapourSynth calls filters through a frame-request model;
- the filter receives `getFrame` calls;
- `arInitial` is the request-arrival stage;
- `arAllFramesReady` is the stage where requested upstream/source frames are available;
- a filter must not assume that frame `N - 1` has already been requested or produced merely because frame `N` is now being requested;
- frame request order may differ from display order;
- scheduling behaviour depends on the filter mode and graph.

### 3.2.3 What the relevant VapourSynth modes mean

Document A must include a dedicated VapourSynth scheduling section with at least these modes.

#### `fmUnordered`

Required explanation:

- one `getFrame` call enters a filter instance at a time;
- this reduces concurrent entry risk;
- it does not guarantee in-order frame numbers;
- request order may still be `0, 3, 1, 2`;
- current CNR3 uses `fmUnordered`;
- the old strict-streaming bridge can still reject out-of-order frame requests;
- `fmUnordered` is safer than fully concurrent modes, but does not by itself solve the recursive predecessor problem.

Required example:

```text
Requested order:
    0, 1, 2, 3

Strict recursive processing works.

Requested order:
    0, 3, 1, 2

A naive recursive filter fails at frame 3 because output[2] does not yet exist.
```

#### `fmParallelRequests`

Required explanation:

- multiple requests may be in flight;
- request and readiness order may interleave;
- cache metadata, hot zones, pins, lookup-owned references, and pruning become more safety-critical;
- request activity should be registered at `arInitial`;
- predecessor availability must not rely on a single global previous-frame slot;
- pruning must not evict frames needed by in-flight work.

Required example:

```text
Request 100 begins.
Request 101 begins.
Request 100 has not completed.
Request 101 needs output[100].

The cache manager must not allow output[100] to be pruned, lost,
observed through a dangling pointer, or double-owned.
```

#### `fmParallel`

Required explanation:

- fully concurrent processing is more difficult;
- metadata mutexes alone are not enough to prove recursive correctness;
- correctness may require active-computation tracking, dependency waits, or condition variables;
- Full `fmParallel` is a significant final design goal, however is out of scope for
  interim development and testing; interim design and development may not 
  (unless absolutely necessary to the interim step) implement anything which
  prevents progress toward full `fmParallel`, and should keep in mind a need to
  be compatible with the final goal;

### 3.2.4 Why recursive processing is dangerous under out-of-order scheduling

Document A must clearly explain:

```text
output[N] depends on output[N - 1]
```

and that:

- `output[N - 1]` must be the already-filtered predecessor;
- `source[N - 1]` is not sufficient;
- a naive “previous frame only” member variable fails when requests are not in ascending order;
- out-of-order scheduling can lead to missing predecessors, incorrect output, or forced rejection unless a safe cache/recovery system exists.

### 3.2.5 Why the cache manager exists

Document A must state explicitly:

```text
The cache manager is a correctness subsystem, not a performance cache.
```

It must explain that the cache manager exists to support:

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

### 3.2.6 Outline Interim and Long Term goals

 It must outline interim and long term goals, with the understanding that over time goals will evolve.   
 For example,
    Interim:
        ensure designing and coding which operates safely under fmUnordered then fmParallelRequests then fmParallel
        but always with a view to ensuring alignment with the long term goals    
    Long Term:
        operating safely under fmParallel

### 3.2.7 What safety means

Document A must define cache/threading safety in concrete terms:

- no dangling frames;
- no dangling cache slots;
- no leaked `VSFrame` references;
- no double-free;
- no unsafe prune of needed predecessors;
- no stale `cache_index` entries;
- mutex ownership must be correct;
- `_externally_locked` helpers must only be called with the mutex held;
- public helpers must not deadlock by calling other locking public helpers;
- `addFrameRef`/`freeFrame` ownership must be provably balanced;
- lookup-owned references must be freed or transferred on every exit path.

### 3.2.8 Why diagnostics are mandatory

Document A must state that instrumentation is proof of safety, not decoration.

It must require diagnostics to prove:

- store attempts/successes/failures;
- prune attempts/successes/failures;
- validation success/failure;
- integrity errors;
- cache-side reference balance;
- caller-side lookup reference balance when lookup starts;
- hot-zone behaviour;
- hard-ceiling aborts;
- teardown clear behaviour;
- memory state where relevant.

It must include the hard-gate principle:

```text
If safety diagnostics show unexpected values, stop.
Do not proceed until the discrepancy is understood.
```

## 3.3 Required Document A sections

Document A must include at least these sections or clear equivalents:

```text
A1. Purpose of this document
A2. Project identity
A3. Algorithmic core
A4. Why VapourSynth scheduling is central
A5. VapourSynth filter modes relevant to CNR3
A6. Why the cache manager exists
A7. Prevailing Interim and Long Term Goals
A8. Current high-level architecture
A9. Safety-critical rules
A10. Diagnostic philosophy
A11. Coding Rule 1 — Code comments
A12. Coding Rule 2 — Code update instructions
A13. Required Phase and SubPhase Naming Rules
A14. Commit Title and Body Rules
A15. New-chat boot sequence
```

Additional sections may be added when useful, but the listed material must not be omitted.

## 3.4 Required high-level architecture content

Document A must describe the major source areas using current naming, for example:

```text
vapoursynth-Cnr3.cpp:
    Main VapourSynth lifecycle, parameters, getFrame path, diagnostics, pixel processing.

cnr3_common.h:
    Shared data structures, including Cnr3Data and old/new cache members.

cnr3_response_tables.h/.cpp:
    Response table building and table diagnostics.

cnr3_output_cache_manager.h/.cpp:
    CMS06.2-or-later output-cache manager.

old_cnr3_strict_cache_*:
    Old strict-streaming cache functions retained while output_cache is not yet output-authoritative.
```

Avoid old names in current code descriptions unless explicitly documenting historical renames.

## 3.5 Required safety-critical rules

Document A must include at least:

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

## 3.6 Required coding rules

Document A must include Rule 1 and Rule 2.

### Rule 1 — Code comments

Comments should be concise, relevant, and informative for future maintainers.

Avoid undue blank lines, repeated wording, excessive prose, and design-history prose inside code.

Do not over-compress safety-critical comments. Any comment that, if misread or removed, could lead a future maintainer to make an unsafe edit, must retain enough detail to preserve its exact meaning. This applies especially to:

- locking and threading invariants;
- ownership and lifetime rules;
- reference-count discipline;
- non-obvious preconditions and postconditions;
- invariants whose violation would cause silent bugs rather than immediate crashes.

Compress freely where code is self-explanatory. Preserve detail wherever a future edit guided by an incomplete comment could introduce a hard-to-find bug.

### Rule 2 — Code update instructions

When producing code changes for a human to apply:

- Each phase or set of changes, and each individual change within them, must be uniquely identifiable.
- State the file and function/location explicitly before each block.
- Use before/after blocks showing exact existing code and exact replacement.
- Always include enough surrounding context to make the before block uniquely findable.
- The before block must match exactly one location in the named file.
- If a snippet would match multiple locations, extend the context until unique.
- The after block must be the exact intended replacement.
- Avoid abstract “insert near” instructions unless backed by actual file context and ensure it would be very clear to a human reader.
- When adding a new function, show the insertion point using the end of the preceding function and the start of the following function.

## 3.7 Required Phase and SubPhase Naming Rules

Future handover documents must distinguish major CMS02 phases from local
subphase/proof-step notation, on the basis that at least one chat
confused phase and sub-phase - this situation is to be recognised and avoided.

Always use expanded phase/subphase naming so as to avoid confusion.

Preferred format:
```
    CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run
    CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
    CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof
```

Compact labels such as "CMS02-G.10D.7" may be used in log markers,
compact tables, and commit titles. However, the expanded form must always
appear in chat responses and handover documents and in CMS06.2-or-later.
It must always appear and in the commit body whenever the compact form
in the title could lead to misinterpretation or phase/subphase confusion with identification.
Preferred commit title style:
```
Complete CMS02-G / SubPhase G10D.7 recovery-return decision dry-run
```

Do not allow local subphase notation such as G10D.N to be confused with major
CMS02-D, CMS02-F, or CMS02-G phases.

## 3.8 Commit Title and Body Rules

Development occurs generally in the latest-updated Visual Studio 2026
connected to github and where commits are recommended after every subphase.

When recommending commits, eg at end of phases and subphases, recommend
a commit text block.  The commit text block format accepted by
Visual Studio 2026 is a block of text where
- the first line is the 1-line commit title followed by a blank line
- lines after that are body of the text commit message

The title should contain at least the phase/subphase.

The body should contain useful-to-a-maintainer summary information
pertaining to the commit, perhaps what the intent of and what was
achieved by the phase/subphase and optionally what test results
(if any) indicated.

---

# 4. Document B — `Document_B_CNR3_Decision_Log_<version>.md`

## 4.1 Purpose

Document B is the stable anti-rabbit-hole decision log.

It records major settled decisions and the reason alternatives were rejected.

It should be updated only when a significant architectural, safety, scheduling, cache, diagnostic, or implementation-process decision is made.

Document B must not be used as a session diary.

## 4.2 Required decision entry format

Each decision entry must use this structure:

```text
Decision DNN — Short title

Decision:
    What was decided.

Rejected alternatives:
    What was considered and rejected.

Reason:
    Why the decision was made.

Implementation consequence:
    What the code must or must not do.

Reference:
    CMS06.2-or-later section, appendix, handover section, source file, or test evidence.
```

The “Implementation consequence” field is mandatory. It tells the next AI what the decision means in code.

## 4.3 Required initial decision topics

Document B must include decision entries covering at least these topics:

```text
D01 — API4 only; do not use fmFrameState.
D02 — Strict-streaming bridge is temporary.
D03 — Output cache is per-instance, never global.
D04 — Ordered frame-number maps are required.
D05 — Pool slots own retained VSFrame references; cache_index is non-owning.
D06 — Lookup helpers must addFrameRef while holding the mutex.
D07 — Duplicate store is first-in-best-dressed.
D08 — Sliding hot zones, not extend-only.
D09 — Pruning must be hot-zone-aware and centralised through remove helper.
D10 — Active ceiling is frame-count based, derived from 1 GiB geometry budget.
D11 — Store path prunes before hard-ceiling rejection.
D12 — Bounded recovery must be modelled carefully.
D13 — Non-checkpoint pinning is deferred unless diagnostics prove need.
D14 — Diagnostics must prove safety before output-cache authority.
D15 — Bounded recovery sample differences are diagnostic, not automatic failure.
```

Additional decisions must be added as they become settled. Current known additional decision topics include:
```text
Hot-zone update belongs at arInitial.
Compact/full diagnostic mode is deferred.
```

Document B must record a D15 clarification rule:
```text
An exact sample match test between a bounded-recovery output frame and
a continuous strict-streaming output frame is diagnostic only. 
Exact sample match must not be a required return condition unless a later
explicit quality/tolerance decision changes that policy.
Reason:
    Bounded recovery from a checkpoint/back-window can legitimately differ from
    continuous recursive calculation from frame 0 because the predecessor history
    is different.
Implementation consequence:
    Difference-measurement diagnostics must record sample differences, but
    exact_match=0 must not by itself fail a bounded-recovery return proof.
```

## 4.4 When to update Document B

Update Document B when:

- a major design decision is made;
- a previously deferred approach becomes accepted or rejected;
- an old decision is superseded;
- a new safety invariant is adopted;
- a new diagnostic hard gate is adopted;
- a scheduling-mode policy changes.

Do not update Document B for every small code patch.

---

# 5. Document C — `Document_C_CNR3_Current_Session_Handover_<version>.md`

## 5.1 Purpose

Document C is the volatile current-state/session handover document.

It must be updated at every chat/session boundary.

It must be current, exact, and action-oriented. It must not repeat all design rationale. It should point to Document A, Document B, CMS06.2-or-later, current source files, and logs.

Document C is the current-state authority.

If a companion design spec contains a “current implementation state” snapshot, Document C overrides it for current implementation status. Design specs may contain snapshots that were accurate when written but later became stale.

## 5.2 Required Document C sections

Document C must include these sections or clear equivalents:

```text
C1. Read order for a new chat
C2. Repository/code context
C3. Current exact implementation status
C4. Latest test evidence
C5. Current diagnostic policy
C6. Immediate next task
C7. Do not implement in the next session unless explicitly chosen
C8. Remaining cleanup/deferred notes
C9. Safety checks before any future commit
C10. Recent commit messages or suggested commit messages
C11. New-chat starter prompt
```

Document C should also include a “Do not lose / named deferred items” subsection when there are named safety, diagnostic, or phase-planning notes that must be carried forward. Examples include hot-zone scheduling prerequisites, deferred diagnostic histograms, and long-run diagnostic verbosity controls.

Document C may include dated appendices for session-boundary summaries, provided the main current-state sections remain concise, current, and action-oriented.

Additional sections may be added when useful.

Clarification re C3. Current exact implementation status, it must incorporate:
When a later phase/subphase has overtaken an older phase/subphase label, 
then Document C must audit the older phase item-by-item.
For example, if CMS06.2 says CMS02-F was not started, but current source shows
some CMS02-F-labelled primitives or behaviours now exist, Document C must say:
- completed;
- superseded/overtaken;
- still open;
- proof evidence pending;
- not applicable.
Do not preserve stale "not started" wording without qualification.

## 5.3 Required current-state precedence wording

Document C must explicitly state:

```text
Treat this current session handover as the source of truth for current status.
Treat the decision log as the source of truth for settled decisions.
Treat CMS06.2-or-later as the detailed cache-manager design authority.
If the design spec and current code appear to conflict, stop and ask for clarification.
```

It must also state when companion design-spec implementation snapshots are known to predate later work, for example:

```text
CMS06.2 Section 14 predates the latest diagnostic work completed in this chat.
Use this Document C as the current implementation-state authority.
```

```text
If a companion design specification contains an implementation-status snapshot,
that snapshot may be stale. Document C must explicitly identify any known stale
snapshot sections and state which later commits, logs, or handover entries
supersede them.

When a companion design spec (eg CMS06.2) says a phase or item is "not started"
or "not yet implemented", the next chat must audit that statement against current
source and logs before treating it as current truth.
    Example:
        CMS06.2 says CMS02-F was not started at the time of its implementation
        snapshot. Later source and logs may have completed or superseded some
        CMS02-F-labelled obligations. Audit CMS02-F item-by-item against current
        source and logs; do not treat the old snapshot as a blanket blocker.
```

## 5.4 Required current exact status content

Document C must include:

```text
Current phase:
    <phase name>

Last completed phase:
    <phase name>

Current output authority:
    strict-streaming cache remains source of returned output
    or
    output_cache is now output-authoritative

Current output-cache role:
    <store/prune proving, cache-hit reuse, recovery, etc.>

Completed since companion design-spec snapshot:
    <list if applicable>

Current phase/subphase naming:
    Use expanded form, for example:
        CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
```


## 5.5 Required latest test evidence content

Document C must include the most recent meaningful test and what it proved.

At minimum, for cache work, it must include:

```text
test command(s)
clip type or size when relevant
store attempts/successes/failures
prune attempts/successes/failures
validation attempts/successes/failures
integrity_errors
ref_balance_errors
cache_addframeref_total
cache_freeframe_total
pre-clear balance
post-clear balance
clear attempts/successes/failures
hot-zone counters relevant to the phase
hard-gate result: PASS/FAIL
```

If no relevant test has been run since the last change, Document C must say so explicitly.

For phases that produce long one-line machine-readable cache summaries, Document
C must also include or require a human-readable final summary block when the
result is being used for decision-making. The block must not replace the
grep-friendly one-line diagnostic summary. It is an additional human audit aid.
For example, with recovery-store or duplicate-store phases include a human-readable
final duplicate/recompute waste summary with at least:
```text
- store attempts;
- store successes;
- duplicate skipped already cached;
- duplicate computed but discarded;
- computed-discarded ratio;
- useful-store ratio.

Example:
    Recovery duplicate/recompute waste summary
    ------------------------------------------
    Store attempts:                         N
    Store successes:                        N
    Computed but discarded duplicates:      N
    Already-cached duplicate skips:         N
    Computed-discarded ratio:
        duplicate_computed_but_discarded / store_attempts = XX.XX%
    Useful-store ratio:
        store_successes / store_attempts = XX.XX%
```

## 5.6 Required immediate next task content

Document C must make the immediate next task specific enough to start safely.

It must include:

```text
Immediate next task:
    <phase and short description>

Files likely needed:
    <file list>

High-level changes:
    <short list>

Safety constraints:
    <short list>

Design-compliance review requirement:
    <if applicable>
```

## 5.7 Required do-not-implement list

Document C must include a hard-stop list:

```text
Do not implement in the next session unless explicitly chosen:
    - ...
```

This list must be updated for the actual next task.

For cache-hit reuse work, it should normally include:

```text
- checkpoint recovery / hole-filling walks
- bounded warm-up recovery
- non-checkpoint pinning
- fmParallelRequests wiring
- full fmParallel support
- changes to recursive blend maths
- changes to scene-change detection
- mass diagnostic string renames
- diagnostic mode redesign unless explicitly chosen
```

Document C should separately preserve named deferred items that are
not immediate tasks but must not be forgotten; these should not be
mixed into the immediate “do not implement” list unless they are also
prohibited for the next session.

## 5.8 Required safety checks and hard gate

Document C must include:

```text
Build:
    Debug build must succeed.
    Release build should succeed before larger phase commits.

Run:
    short realclip or blankclip smoke test where relevant.
    any targeted test required by the current phase.

Check output-cache diagnostics:
    invariants_ok=1
    integrity_errors=0
    validation_failures=0
    ref_balance_errors=0
    store_failures=0 unless deliberately testing failure paths
    prune_after_store_failures=0 unless deliberately testing failure paths
    cache_addframeref_total - cache_freeframe_total matches total_cached_frame_count before clear
    cache_addframeref_total - cache_freeframe_total is 0 after clear
    clear_successes=1 after teardown when cached frames existed

Hard gate:
    If any of the above checks show unexpected values, stop.
    Do not proceed to the next task until the discrepancy is understood.
```

For phases involving lookup-owned references, also require:

```text
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

at quiescent points.

Document C must identify and carry forward information about any
known final fmParallelRequests and fmParallel readiness blockers.
For current CNR3, this may include:
- old_strict_cache.next_needed is not designed for final fmParallel operation;
- old_strict_cache.prev_output / next_needed must be retired, bypassed, or
  redesigned before final fmParallel authority;
- mutex placement and cache-manager locking must remain compatible with
  eventual fmParallel, even when current work is still fmUnordered.

## 5.9 Required current diagnostic policy

Document C must state the current diagnostic behaviour, including compact/full summary rules and any deferred diagnostic-mode decisions.

Example:

```text
Compact trace:
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

Future option:
    compact/full diagnostic mode is deferred.
```

---

# 6. Companion Design Document Rules

The current cache design authority must be described as:

```text
CMS06.2 or any later cache design spec that explicitly supersedes CMS06.2.
```

Do not write new handover specs that say “CMS05 is the authority” unless the project deliberately reverts to CMS05, which is not the current state.

The handover documents do not need to duplicate every CMS06.2 detail. They must identify:

- which CMS06.2 decisions are already implemented;
- which are pending;
- which are deferred;
- which are explicitly out of scope.

When a new chat is asked to modify output-cache code, upload CMS06.2-or-later with the handover pack.

The new chat should be instructed:

```text
Use CMS06.2-or-later as the detailed design reference.
Use the current session handover as the current implementation-state authority.
Use the decision log to avoid re-opening settled decisions.
If CMS06.2-or-later and current code appear to conflict, stop and ask for clarification rather than guessing.
```

When a later phase/subphase has overtaken an older phase/subphase label, 
then Document C must audit the older phase item-by-item.

For example, if CMS06.2 says CMS02-F was not started, but current source shows
some CMS02-F-labelled primitives or behaviours now exist, Document C must say:
```
- completed;
- superseded/overtaken;
- still open;
- proof evidence pending;
- not applicable.
```
Do not preserve stale "not started" wording without qualification.

---

# 7. Companion Source and Logs Rules

For coding tasks, the handover documents are not enough. Upload the relevant current source files.

At minimum, for cache-manager work:

```text
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_build_config.h
```

For diagnostics/memory work:

```text
cnr3_memory_diagnostics.h
cnr3_memory_diagnostics.cpp
```

For response-table or algorithm work:

```text
cnr3_response_tables.h
cnr3_response_tables.cpp
vapoursynth-Cnr3.cpp
```

Upload logs when the next task depends on observed behaviour.

If the uploaded source files conflict with handover text, the new chat must inspect and reason about the conflict rather than assuming either is automatically correct.
Document C remains the current-state authority, but actual source code is the authority for exact code locations and before/after patches.

---

# 8. New Chat Starter Prompt

Every handover pack should include a starter prompt similar to this:

```text
We are continuing CNR3 development.

Please read the uploaded documents in this order:

1. Document_A_CNR3_Project_Context_and_Rules_<version>.md
2. Document_B_CNR3_Decision_Log_<version>.md
3. Document_C_CNR3_Current_Session_Handover_<version>.md
4. CMS06.2-or-later cache design specification
5. Current source files/logs

Important:
- The new chat has no memory of prior chats.
- Treat Document_C_CNR3_Current_Session_Handover_<version>.md as the source of truth for current state.
- Treat Document_B_CNR3_Decision_Log_<version>.md as the source of truth for settled decisions.
- Treat CMS06.2-or-later as the detailed design reference.
- Do not re-litigate settled decisions unless current code or logs prove a real problem.
- Follow Rule 1 for code comments.
- Follow Rule 2 for before/after code update instructions.
- Do not implement anything listed in the current handover's "Do not implement" section.

First, confirm your understanding of the current state and immediate next task.
Then wait for the current code files if they have not already been uploaded.
```

---

# 9. Maintenance Rules for the Handover Pack

## 9.1 Updating Document A

Update Document A only when:

- project-wide rules change;
- coding/comment/update rules change;
- a major environment or architecture rule changes;
- the required explanatory preamble needs correction or expansion.

Do not update Document A for every phase.

## 9.2 Updating Document B

Update Document B when:

- a major decision is made;
- a previously deferred approach becomes accepted or rejected;
- an old decision is superseded;
- a new invariant or hard gate is adopted.

Do not use Document B as a session diary.

## 9.3 Updating Document C

Update Document C at every chat/session boundary.

It must always contain:

- current phase;
- last completed phase;
- current output authority;
- exact next task;
- latest meaningful test evidence;
- do-not-implement list;
- files likely needed;
- hard-gate safety checks;
- deferred/cleanup notes.

Document C is the most volatile document and must be treated as the current-state authority.

Document C may include dated appendices for session-boundary summaries, provided the main current-state sections remain concise, current, and action-oriented.

---

# 10. Success Criteria for a Handover Pack

The handover pack is successful if a new chat can answer these questions before touching code:

```text
What is CNR3?
Why is cache management required for correctness?
What is VapourSynth doing that makes this difficult?
What do arInitial and arAllFramesReady mean?
What VapourSynth scheduling hazards matter?
What is fmUnordered, and what does it not guarantee?
What is fmParallelRequests, and why does it increase cache safety risk?
Why is full fmParallel out of scope?
Why is recursive output[N] dependent on filtered output[N - 1]?
What is the current output-authoritative path?
What CMS06.2-or-later phase are we in?
What exactly was tested last?
What exactly is the next task?
What must not be implemented yet?
What files/functions are likely involved?
What mutex, ownership, and reference-count rules must not be broken?
Which decisions are settled and should not be re-litigated?
What hard-gate diagnostics must pass before proceeding?
```

A handover pack is incomplete if Document A does not give enough VapourSynth and recursive-filter background for a new AI or human maintainer to understand why the cache manager is correctness-critical before touching code.

A handover pack is incomplete if Document C does not clearly identify the current implementation state, immediate next task, do-not-implement list, and latest safety evidence.

A handover pack is incomplete if Document B omits settled decisions that would otherwise be likely to be re-opened.

---

# 11. Final Production Checklist

Before issuing a new handover pack, confirm:

```text
[ ] A, B, and C use the same version number.
[ ] Document A includes the mandatory extended project-orientation preamble.
[ ] Document A explains VapourSynth request order and modes in enough detail.
[ ] Document A states cache management is a correctness subsystem.
[ ] Document A includes Rule 1 and Rule 2.
[ ] Document B includes current settled decisions and new major decisions.
[ ] Document B references CMS06.2-or-later, not stale CMS05 authority wording.
[ ] Document C is updated to the exact latest state.
[ ] Document C explicitly overrides stale implementation snapshots in companion specs.
[ ] Document C includes latest test evidence and hard-gate result.
[ ] Document C includes the next task and do-not-implement list.
[ ] Document C includes current diagnostic policy.
[ ] Phase/subphase names use expanded format where needed, e.g. CMS02-G / SubPhase G10D.8 / short-intent-name.
[ ] Document C identifies stale companion-spec implementation snapshots.
[ ] Any older phase labelled "not started" is audited against current source/logs before being preserved.
[ ] Proof-only behaviour is clearly distinguished from production runtime policy and final output authority.
[ ] Exact sample match is not treated as a bounded-recovery return condition unless explicitly decided.
[ ] Duplicate/recompute waste summary is included when relevant.
[ ] Known fmParallel-readiness blockers, including old_strict_cache.next_needed, are carried forward.
[ ] Phases/subphases include commit messages or suggested commit messages unless intentionally omitted.
[ ] Companion CMS06.2-or-later design spec is identified.
[ ] Required source files/logs for the next task are identified.
[ ] The handover zip contains exactly the matched A/B/C documents.

```
