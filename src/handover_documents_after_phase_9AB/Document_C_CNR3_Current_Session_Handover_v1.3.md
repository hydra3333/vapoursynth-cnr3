# Document C - CNR3 Current Session Handover

**Document:** C of CNR3 handover pack  
**Version:** v1.3  
**Date:** 2026-06-06  
**Status:** Volatile current-state/session handover document; update at every session boundary.  
**Current design authority:** CMS06.1, or any later cache design specification that explicitly supersedes CMS06.1.  
**Current implementation authority:** This document, plus the uploaded current source files and latest logs.  
**Current matched pack:** A/B/C v1.3

---

## C1. Read order for a new chat

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.3.md`
2. `Document_B_CNR3_Decision_Log_v1.3.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.3.md`
4. `cnr3_cache_manager_design_v6.1.md`, or any later spec explicitly superseding CMS06.1
5. current relevant source files
6. latest logs if the task depends on test evidence

Rules for the new chat:

```text
Treat this current session handover as the source of truth for current status.
Treat the decision log as the source of truth for settled decisions.
Treat CMS06.1-or-later as the detailed cache-manager design authority.
If the design spec and current code appear to conflict, stop and ask for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in "Do not implement in the next session".
```

CMS06.1 is the current cache design authority. Earlier CMS05.x documents are superseded except as history.

If a companion design spec contains a current implementation state snapshot, this Document C overrides it for current implementation status. CMS06.1 predates the completed CMS02-G.7/G.8/G.9AB proof sequence. Use this Document C as the current implementation-state authority.

---

## C2. Repository/code context

Known repository:

```text
https://github.com/hydra3333/vapoursynth-cnr3
```

Current source files uploaded for this handover:

```text
cnr3_build_config.h
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_frame_internal_processing.h
cnr3_frame_internal_processing.cpp
```

Other important project files:

```text
cnr3_response_tables.h/.cpp
cnr3_memory_diagnostics.h/.cpp
old_cnr3_strict_cache.h/.cpp
```

Current names:

```text
New output cache:
    Cnr3OutputCacheManager
    cnr3_output_cache_*

Old strict cache:
    OldCnr3StrictStreamCache
    old_cnr3_strict_cache_*
```

Avoid in new code unless documenting history:

```text
Cnr3CacheManagerV005
Cnr3CacheManager for the old strict cache
cache_manager_v005
cnr3_cache_manager_*
CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS
v005 wording in new comments
```

Some comments and diagnostic labels still contain CMS05 or CMS05-3A because they identify historical proving phases or have not yet been renamed. CMS06.1 is the design authority. Treat CMS05 wording drift as cleanup, not as a reason to change logic during G.10ABC.

---

## C3. Current exact implementation status

Current phase:

```text
Ready to start CMS02-G.10ABC:
    dry-run non-mutating recovery compute skeleton
```

Last completed phase:

```text
CMS02-G.9AB:
    recovery source-frame-set skeleton plus enabled proof,
    disabled state restored afterward
```

Current output authority:

```text
old_strict_cache remains the source of returned output.
output_cache is not output-authoritative.
```

Current output-cache role:

```text
output_cache stores/prunes real produced frames for proving.
output_cache supports direct cache-hit lookup/addref helpers.
output_cache has checkpoint, hot-zone, prune, store, validation, and pin/unpin scaffolding.
recovery proofs exist as debug-only scaffolding and are disabled in normal state.
```

Current build marker:

```text
CNR3_EDIT_VERSION = "CMS02-G9AB-source-frame-set-proof-disabled-v1"
```

Current proof gates in normal state:

```text
CNR3_FOR_DEBUG_ONLY_FORCE_CACHE_LOOKUP_PROBE = false
CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON = false
CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_WALK_SKELETON = false
CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_START_REF_SKELETON = false
CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON = false
CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_DECISION_WALK_SKELETON = false
CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_FRAME_SET_SKELETON = false
CNR3_FOR_DEBUG_ONLY_RECOVERY_SOURCE_REQUEST_BACK_FRAMES = 2
```

Completed since the CMS06.1 implementation snapshot:

```text
CMS02-G.7A:
    Added disabled per-invocation frameData source-request-plan skeleton.

CMS02-G.7B:
    Proved source-request-plan create/consume/destroy lifecycle.

CMS02-G.7C:
    Proved widened source-request/retrieve discipline.

CMS02-G.8A:
    Added disabled recovery decision/walk skeleton.

CMS02-G.8B:
    Proved decision/walk post-store, including checkpoint rollover to checkpoint 10.

CMS02-G.8C:
    Moved decision/walk proof point to pre-store position.

CMS02-G.8D:
    Proved pre-store decision/walk marks current frame as would_compute=1.

CMS02-G.9AB:
    Added and proved local recovery source-frame-set acquisition/release lifetime.
```

Current frame-processing boundary:

```text
process_cnr3_frame(...)
```

Important G.10 implication:

```text
process_cnr3_frame() currently reads the recursive predecessor from:
    d->old_strict_cache.prev_output

Therefore CMS02-G.10ABC must be dry-run only, or must only prepare a future
non-mutating processing boundary. It must not perform actual recovery
computation through process_cnr3_frame() until predecessor input is refactored
or otherwise made explicit and safe.
```

---

## C4. Latest test evidence

Latest relevant proof evidence:

```text
CMS02-G.9AB enabled proof:
    edit_version=CMS02-G9AB-source-frame-set-proof-v1
    source-frame-set proof lines present
    source frames acquired and released for checkpoint-to-request walk
    proof_ok=1 on observed walks
    unpin_ok=1 on observed walks
    lookup_ref_balance=0
    validation_failures=0
    integrity_errors=0
    ref_balance_errors=0

CMS02-G.9AB disabled proof:
    edit_version=CMS02-G9AB-source-frame-set-proof-disabled-v1
    no FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-* lines
    no FOR-DEBUG-ONLY-RECOVERY-DECISION-WALK-* lines
    no FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-* lines
    checkpoint pin/unpin counters returned to zero
    lookup-ref counters returned to zero
    normal cache/refcount gates remained clean
```

Most recent disabled-state log evidence seen:

```text
Output 10 frames
store_attempts=10
store_successes=10
store_failures=0
prune_after_store_attempts=10
prune_after_store_successes=10
prune_after_store_failures=0
validation_attempts=20
validation_successes=20
validation_failures=0
integrity_errors=0
ref_balance_errors=0
addframeref_total=10 before clear
freeframe_total=0 before clear
balance=10 before clear
clear_attempts=1 after teardown
clear_successes=1 after teardown
addframeref_total=10 after clear
freeframe_total=10 after clear
balance=0 after clear
checkpoint_pin_attempts=0
checkpoint_find_and_pin_attempts=0
checkpoint_unpin_attempts=0
lookup_ref_acquired=0
lookup_ref_released=0
lookup_ref_transferred=0
lookup_ref_balance=0
```

Hard-gate result:

```text
PASS
```

Caveat:

```text
The latest disabled-state evidence was a short run. Longer 12/20-frame proof
runs were used earlier in G.8 to prove checkpoint rollover. For G.10ABC,
start with a short run, then use a longer run only if the new dry-run log needs
checkpoint rollover evidence.
```

---

## C5. Current diagnostic policy

Current diagnostic behaviour:

```text
Compact output-cache frame trace:
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

Memory diagnostics:
    enabled through CNR3_MEMORY_DIAGNOSTICS.
    periodic interval currently 500 frames.

Proof diagnostics:
    controlled by compile-time flags in cnr3_build_config.h.
    dedicated proof runs temporarily enable exactly the needed proof gates.
    normal committed state disables proof gates again.
```

Deferred diagnostic item:

```text
G-DIAG-LOG-VOLUME-01:
    Add compile-time diagnostic verbosity controls for 50+ and 100+ frame tests.
    Routine long-run logs should show compact status plus detailed logs only for
    the currently active proof/change.
```

---

## C6. Immediate next task

Immediate next task:

```text
CMS02-G.10ABC:
    dry-run non-mutating recovery compute skeleton
```

Goal:

```text
Prepare and prove the orchestration shape for future recovery computation
without allocating recovered output frames, computing recovered pixels, storing
recovered outputs, returning recovered outputs, or mutating old strict-streaming
state.
```

Files likely needed:

```text
cnr3_build_config.h
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_frame_internal_processing.h
cnr3_frame_internal_processing.cpp
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
```

High-level changes expected:

```text
1. Add a new compile-time debug-only G.10 dry-run gate.
2. Add dry-run helper(s) that combine:
       recovery plan
       checkpoint frame number
       walk range
       source-frame-set availability
       would-need-prev-output / would-compute step logging
3. Keep actual processing untouched.
4. Run enabled proof.
5. Disable proof flag(s) again.
6. Commit only the disabled normal state.
```

Safety constraints:

```text
- No actual recovered frame computation in G.10ABC.
- Do not allocate recovered output frames in G.10ABC.
- Do not call process_cnr3_frame() for recovery in G.10ABC.
- Do not store recovered outputs.
- Do not return recovered outputs.
- Do not mutate d->old_strict_cache.prev_output.
- Do not mutate d->old_strict_cache.next_needed.
- Do not change output authority.
- Do not enable fmParallelRequests.
- Do not enable fmParallel.
```

Design-compliance review requirement:

```text
Before real recovered-frame computation, explicitly review the processing
boundary because process_cnr3_frame() currently obtains the predecessor from
old_strict_cache.prev_output. Actual recovery needs an explicit predecessor
input path or an equivalent non-mutating design.
```

---

## C7. Do not implement in the next session unless explicitly chosen

Do not implement in CMS02-G.10ABC unless the user explicitly changes the task:

```text
- actual recovered-frame computation
- allocation of recovered output frames
- calling process_cnr3_frame() for recovery computation
- storing recovered outputs in output_cache
- returning recovered outputs to VapourSynth
- making output_cache output-authoritative
- bounded warm-up recovery as real output
- non-checkpoint pinning
- fmParallelRequests wiring
- full fmParallel support
- changes to recursive blend maths
- changes to scene-change detection
- mass diagnostic string renames
- diagnostic mode redesign
- cleanup-only CMS05/CMS06 wording churn unless explicitly chosen
```

---

## C8. Do not lose / named deferred items

### G-PAR-HZ-ARINITIAL-01 - hot-zone update must be at `arInitial`

Hot-zone updates should be treated as prerequisite work before `fmParallelRequests` or `fmParallel` development. Under future concurrent request modes, deferring hot-zone update to `arAllFramesReady` would be unsafe because pruning must know about active request intent as early as possible.

Current status:

```text
The current code appears to update hot zones at arInitial. Preserve this.
```

### G-DIAG-RECALC-HIST-01 - compile-time recalculation histogram

Add a compile-time-only diagnostic table later showing how many frames were calculated once, twice, three times, etc. The table should be per-instance only and should count actual frame calculations, not cache returns. It should include the normal once-only row unless later log-volume policy says otherwise.

Purpose:

```text
Quantify duplicate recovery computation and first-in-best-dressed discard cost.
```

### G-DIAG-LOG-VOLUME-01 - long-run diagnostic throttling

As recovery testing moves to 50+ and 100+ frame runs, add compile-time diagnostic verbosity controls so routine long-run logs show only:

```text
- edit/version marker
- per-instance create/free summaries
- compact frame trace at chosen interval
- full summaries at start/end, failures, and selected milestone frames
- detailed proof logs for the currently active change only
```

Existing detailed logs should remain available behind compile-time flags because they are still useful when proving a specific cache/recovery invariant.

---

## C9. Remaining cleanup/deferred notes

Deferred cleanup:

```text
- Some comments and diagnostics still say CMS05 even though CMS06.1 is current.
  Do not mix broad wording cleanup into G.10ABC unless it is required to avoid
  a specific misunderstanding.

- Consider updating CMS06.1 Section 14 or creating a successor spec snapshot to
  reflect CMS02-G.7/G.8/G.9AB completion.

- Consider moving from proof-only recovery scaffolding to production recovery
  only after dry-run and local-compute proofs are complete.
```

Optional G.9C:

```text
A longer enabled source-frame-set proof after checkpoint rollover is optional.
It is not mandatory before G.10 because G.8 already proved checkpoint rollover
and G.9AB proved source-frame-set lifetime.
```

---

## C10. Safety checks before any future commit

Build:

```text
Debug build must succeed.
Release build should succeed before larger phase commits.
```

Run:

```text
Short realclip or blankclip smoke test where relevant.
Any targeted proof test required by the current phase.
```

Check output-cache diagnostics:

```text
invariants_ok=1
integrity_errors=0
validation_failures=0
ref_balance_errors=0
store_failures=0 unless deliberately testing failure paths
prune_after_store_failures=0 unless deliberately testing failure paths
cache_addframeref_total - cache_freeframe_total matches total_cached_frame_count before clear
cache_addframeref_total - cache_freeframe_total is 0 after clear
clear_successes=1 after teardown when cached frames existed
```

For phases involving lookup-owned references:

```text
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total

or equivalently:
    lookup_ref_balance=0
```

For phases involving checkpoint pins:

```text
checkpoint_pin_attempts == checkpoint_pin_successes + checkpoint_pin_failures
checkpoint_unpin_failures=0 unless deliberately testing failure paths
checkpoint_unpin_underflow_errors=0
checkpoint_active_pin_total=0 at quiescent/disabled state
```

Hard gate:

```text
If any of the above checks show unexpected values, stop.
Do not proceed to the next task until the discrepancy is understood.
```

---

## C11. Recent commit messages or suggested commit messages

Recent completed commit themes:

```text
CMS02-G.7A/G.7B/G.7C:
    source-request-plan lifecycle and widened source-request proof

CMS02-G.8A/G.8B/G.8C/G.8D:
    recovery decision/walk skeleton and pre-store would_compute proof

CMS02-G.9AB:
    recovery source-frame-set lifetime proof, disabled state restored
```

Suggested next commit message after G.10ABC, if proof passes and is disabled again:

```text
CMS02-G.10ABC: add dry-run recovery compute skeleton

Add a debug-only dry-run recovery compute skeleton for the future
checkpoint-to-request recovery path.

The dry-run proof logs the selected checkpoint, walk range, source-frame
availability, predecessor requirement, and would-compute decisions without
allocating recovered output frames, computing recovered pixels, storing
recovered outputs, returning recovered outputs, or mutating old strict-streaming
state.

Disable the proof gate again before committing normal state.

Smoke testing confirms proof-only traces are absent in the restored disabled
state, cache/refcount gates remain clean, checkpoint pin/unpin counters return
to zero, lookup-ref balance remains zero, and old strict-streaming output
authority remains unchanged.
```

---

## C12. New-chat starter prompt

```text
We are continuing CNR3 development.

Please read the uploaded documents in this order:

1. Document_A_CNR3_Project_Context_and_Rules_v1.3.md
2. Document_B_CNR3_Decision_Log_v1.3.md
3. Document_C_CNR3_Current_Session_Handover_v1.3.md
4. cnr3_cache_manager_design_v6.1.md, or any later CMS06.1-or-later cache design specification
5. Current source files/logs

Important:
- The new chat has no memory of prior chats.
- Treat Document_C_CNR3_Current_Session_Handover_v1.3.md as the source of truth for current state.
- Treat Document_B_CNR3_Decision_Log_v1.3.md as the source of truth for settled decisions.
- Treat CMS06.1 or later as the detailed design reference.
- Do not re-litigate settled decisions unless current code or logs prove a real problem.
- Follow Rule 1 for code comments.
- Follow Rule 2 for before/after code update instructions.
- Do not implement anything listed in the current handover's "Do not implement" section.

First, confirm your understanding of the current state and immediate next task.
Then wait for the current code files if they have not already been uploaded.
```

---

## Appendix A - Summary up to and including CMS02-G.9AB, prior to CMS02-G.10

**Date:** 2026-06-06

### Where this chat started

- The chat began with the CMS02-G cache/recovery proof sequence already underway.
- The output cache manager already had store/prune proving, checkpoint support, hot-zone diagnostics, lookup-ref balance counters, and cache/refcount validation.
- The major unfinished question was how to safely prepare bounded recovery under `fmUnordered` now, while keeping the design structurally compatible with future `fmParallelRequests` and ultimately `fmParallel`.
- Recovery was not allowed to compute or return recovered frames. The task was to prove scaffolding safely, one ownership/lifetime rule at a time.

### Phases completed in this chat

- **CMS02-G.7A - source-request-plan skeleton**
  - Added a per-invocation `frameData` source-request-plan skeleton.
  - Kept it disabled.
  - Proved it did not change runtime behaviour.

- **CMS02-G.7B - source-request-plan lifecycle proof**
  - Temporarily enabled the source-request plan.
  - Proved creation in `arInitial`, consumption in `arAllFramesReady`, and destruction on cleanup paths.
  - Disabled again before committing.

- **CMS02-G.7C - widened source-request range proof**
  - Temporarily widened the requested source-frame range.
  - Proved `arInitial` could request a bounded range and `arAllFramesReady` could retrieve/release extra source frames.
  - Confirmed normal output frame `N` still used the existing path.
  - Disabled again before committing.

- **CMS02-G.8A - disabled recovery decision/walk skeleton**
  - Added a disabled decision-walk skeleton.
  - It could prepare a bounded recovery plan, identify checkpoint/walk range, and log would-use-cache / would-compute decisions when enabled.
  - Disabled state was clean.

- **CMS02-G.8B - enabled post-store decision/walk proof**
  - Temporarily enabled the decision-walk skeleton.
  - Proved checkpoint selection, checkpoint pin/unpin, checkpoint-start reference acquisition/release, and checkpoint rollover from checkpoint 0 to checkpoint 10.
  - Because it ran post-store, most walk frames were already cached, which was expected.
  - Disabled again before committing.

- **CMS02-G.8C - move decision/walk proof to pre-store position**
  - Moved the decision-walk probe to the pre-store position, but left it disabled.
  - This prepared the proof to see the current requested frame before it was cached.

- **CMS02-G.8D - enabled pre-store decision/walk proof**
  - Temporarily enabled the pre-store decision-walk proof.
  - Proved the important shape:
    - earlier walk frames already cached: `would_compute=0`
    - current requested frame not yet cached: `would_compute=1`
  - Confirmed source coverage, checkpoint reference release, unpin success, and lookup-ref balance.
  - Disabled again before committing.

- **CMS02-G.9AB - source-frame-set skeleton plus proof**
  - Added a local per-invocation recovery source-frame-set helper.
  - Temporarily enabled proof that the recovery walk's source frames can be retrieved, held, and released.
  - Proved acquired/released counts match, checkpoint unpin succeeds, and no source-frame-set state is stored globally or in `Cnr3Data`.
  - Disabled again before committing.

### Current development status

- Recovery scaffolding exists for:
  - per-invocation source request planning;
  - widened source request/retrieve discipline;
  - bounded recovery plan selection;
  - checkpoint pin/unpin balance;
  - checkpoint-start output reference acquisition/release;
  - decision-walk logging;
  - pre-store `would_compute` detection;
  - local recovery source-frame-set lifetime.

- All of that remains proof-only and is disabled in the normal committed state.

- Normal runtime is still:
  - strict-streaming authoritative;
  - output cache is proving store/prune/checkpoint behaviour;
  - recovered outputs are not stored;
  - recovered outputs are not returned;
  - output authority has not changed.

- Safety checks are currently clean in the latest proof and disabled logs:
  - `invariants_ok=1`
  - `validation_failures=0`
  - `integrity_errors=0`
  - `ref_balance_errors=0`
  - lookup-ref balance returns to zero when used
  - cache-owned frame refs return to zero after clear
  - checkpoint pin/unpin counters return to zero when proof paths are disabled

### What remains

- **CMS02-G.10ABC - dry-run recovery compute skeleton**
  - Add a dry-run compute orchestration helper.
  - Log what the future recovery compute path would do.
  - Do not allocate recovered output frames yet.
  - Do not call the real frame-processing function for recovery yet.
  - Do not store or return recovered output.

- **CMS02-G.10D or later - first actual local recovered-frame computation proof**
  - Compute recovered frames locally.
  - Immediately release them.
  - Do not store or return them yet.
  - Prove no mutation of normal strict-streaming state.

- Later:
  - store recovered outputs in the output cache;
  - return recovered output as authoritative only after all prior proofs are clean;
  - handle out-of-order requests through recovery rather than rejection;
  - review hot-zone timing and mutex placement for `fmParallelRequests` and `fmParallel`;
  - add recalculation histogram diagnostics;
  - add log-volume controls for longer 50+ and 100+ frame tests;
  - update final design against CMS06.1 and the handover notes.
