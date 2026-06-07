# Document C - CNR3 Current Session Handover

**Document:** C of CNR3 handover pack  
**Version:** v1.6  
**Date:** 2026-06-07  
**Status:** Volatile current-state/session handover document; update at every session boundary; continuity-preserved from v1.4 baseline.  
**Current design authority:** CMS06.3, or any later cache design specification that explicitly supersedes CMS06.3.  
**Current implementation authority:** This document, plus the uploaded current source files and latest logs.  
**Current matched pack:** A/B/C v1.6

---

## C1. Read order for a new chat

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.6.md`
2. `Document_B_CNR3_Decision_Log_v1.6.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.6.md`
4. `cnr3_CMS06.3_cache_manager_design_v6_2.md`, or any later spec explicitly superseding CMS06.3
5. current relevant source files
6. latest logs if the task depends on test evidence

Rules for the new chat:

```text
Treat this current session handover as the source of truth for current status.
Treat the decision log as the source of truth for settled decisions.
Treat CMS06.3-or-later as the detailed cache-manager design authority.
If the design spec and current code appear to conflict, stop and ask for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in "Do not implement in the next session".
```

CMS06.3 is the current cache design authority. Earlier CMS05.x and CMS06.1 documents are superseded except as history.

CMS06.3 incorporates the accepted CMS06.1 update recommendations after CMS02-G.9AB. The separate `CNR3_CMS06_1_Update_Recommendations_after_CMS02_G9AB.md` file is now historical and should not be treated as the primary design authority.

If a companion design spec contains a current implementation state snapshot, this Document C overrides it for current implementation status. CMS06.3 is aligned with the completed CMS02-G.7/G.8/G.9AB proof sequence and with the planned CMS02-G.10ABC dry-run compute orchestration phase.

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

Some comments and diagnostic labels still contain CMS05 or CMS05-3A because they identify historical proving phases or have not yet been renamed. CMS06.3 is the design authority. Treat CMS05/CMS06 wording drift as cleanup, not as a reason to change logic during G.10ABC.

---

## C3. Current exact implementation status

Current phase/SubPhase:

```text
CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review:
    PASS by review/decision.
```

Last completed committed implementation phase/SubPhase:

```text
CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run
```

Latest committed phase label reported by the user:

```text
Complete CMS02-G.10D.7 recovery-return decision dry-run
```

Current design authority:

```text
CMS06.3 or any later cache design specification that explicitly supersedes CMS06.3.
```

Current output authority:

```text
Partial transition only.

Implemented:
    `arAllFramesReady` cache-hit return path exists. If `output_cache` already
    contains frame N, the code can acquire a caller-owned lookup ref, mark it
    transferred, log CACHE-HIT-RETURN, and return the cached frame to VapourSynth.

Still not complete:
    Full recovery/output-cache authority is not complete. Cache misses still
    fall through to the strict-streaming/new-computation path. Recovered outputs
    are not yet generally returned from the output cache.
```

Current output-cache role:

```text
- stores/prunes real produced frames;
- supports cache-hit lookup/addFrameRef/transfer return;
- supports checkpoint, hot-zone, prune, store, validation, and pin/unpin scaffolding;
- supports debug/proof recovery compute/store/measurement/return-decision scaffolding;
- is not yet the general output-authoritative recovery path for all frames.
```

Current proof-gate policy:

```text
G10D proof gates are normally disabled in committed state.
Dedicated proof runs temporarily enable exactly the required gates, then restore
normal disabled state before commit unless the user explicitly chooses otherwise.
```

Completed since the v1.4 handover / CMS06.2 snapshot:

```text
CMS02-G / SubPhase G10ABC / dry-run recovery compute skeleton:
    complete / committed

CMS02-G / SubPhase G10D.1 / local-single-compute-proof:
    complete / committed

CMS02-G / SubPhase G10D.2 / local-bounded-walk-compute-proof:
    complete / committed

CMS02-G / SubPhase G10D.3 / cleanup/reference diagnostics proof:
    complete / committed

CMS02-G / SubPhase G10D.4 / proof scaffold cleanup/review comments:
    complete / committed

CMS02-G / SubPhase G10D.5 / local-bounded-walk-store-proof:
    complete / committed

CMS02-G / SubPhase G10D.6 / recovery-store-difference-measurement-proof:
    complete / committed

CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run:
    complete / committed

CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review:
    pass by review/decision; no code change required
```

CMS02-F status audit:

```text
The old CMS06.2 "CMS02-F not started" implementation snapshot is stale.
CMS06.3 corrects this.

Current understanding:
- `cnr3_output_cache_find_frame_and_add_ref()` is implemented.
- `arAllFramesReady` cache-hit lookup/transfer/return is implemented in code.
- lookup-owned acquired/released/transferred counters exist.
- cache-hit transfer code exists.
- A targeted log showing CACHE-HIT-RETURN with clean transferred lookup-ref
  balance may still be useful if not already captured in current logs.
- `Cnr3OwnedFrameRef` RAII wrapper is not implemented; explicit ref handling
  remains in use.
- CMS02-F is not a blanket blocker, but any remaining CMS02-F-labelled
  obligations must be reviewed item-by-item before final output authority.
```

Current phase/SubPhase naming rule:

```text
Use expanded form in handover documents and chat responses, for example:
    CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
    CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof
```

Important exact-match rule:

```text
`exact_match` is diagnostic only. It must not be used as a bounded-recovery
return condition unless a later explicit quality/tolerance decision changes
that policy.
```

Important fmParallel warning:

```text
`old_strict_cache.next_needed` and `old_strict_cache.prev_output` are not final
fmParallel authority. They must be retired, bypassed, or redesigned before final
fmParallel operation.
```

---

## C4. Latest test evidence

Most recent proof evidence discussed in this chat:

```text
CMS02-G / SubPhase G10D.6 enabled 20-frame difference-measurement proof:
    frames_checked=20
    frames_measured=19
    frames_skipped_no_cached_output=1
    frames_exact_match=19
    frames_with_differences=0
    structural_failures=0
    lookup_refs_released=19
    samples_compared=11819520
    samples_different=0
    max_abs_sample_diff=0
    sum_abs_sample_diff=0

CMS02-G / SubPhase G10D.6 enabled 3640-frame difference-measurement proof:
    frames_checked=3640
    frames_measured=3639
    frames_skipped_no_cached_output=1
    frames_exact_match=3639
    frames_with_differences=0
    structural_failures=0
    lookup_refs_released=3639
    samples_compared=2263749120
    samples_different=0
    max_abs_sample_diff=0
    sum_abs_sample_diff=0

CMS02-G / SubPhase G10D.6 disabled-state smoke:
    no active G10D.6 recovery measurement lines emitted
    store_attempts=20
    store_successes=20
    store_failures=0
    duplicate_skipped_already_cached=0
    duplicate_computed_but_discarded=0
    output_authoritative=0
    validation_failures=0
    integrity_errors=0
    ref_balance_errors=0
    lookup_ref_balance=0
    clear_successes=1
    addframeref_total=20
    freeframe_total=20
    final cache ref balance returns to zero
```

CMS02-G / SubPhase G10D.7 enabled proof evidence:

```text
FOR-DEBUG-ONLY-RECOVERY-RETURN-DECISION-SUMMARY
    frames_checked=20
    candidates_found=19
    frames_skipped_no_candidate=1
    would_be_returnable=19
    lookup_refs_released=19
    lookup_failures=0
    actual_recovered_returns=0
    output_authoritative=0
    would_transfer_lookup_ref_to_vapoursynth=0

Cache/ref counters:
    store_attempts=39
    store_successes=20
    store_failures=0
    duplicate_skipped_already_cached=19
    duplicate_computed_but_discarded=19
    validation_failures=0
    integrity_errors=0
    ref_balance_errors=0
    lookup_ref_balance=0
    clear_successes=1
    addframeref_total=20
    freeframe_total=20
    balance=0
```

CMS02-G / SubPhase G10D.7 disabled state:

```text
The user restored proof gates to false and restored the source-request back-frame
count to 2. No separate disabled smoke test was run for that commit.
```

CMS02-G / SubPhase G10D.8 evidence:

```text
Review/decision step only. No code change. No build or smoke test required.
```

Hard-gate result:

```text
PASS for the completed proof phases based on logs reviewed in-session.
```

Caveat:

```text
For the next code-changing phase, upload the current post-G10D.7 source files and
run the targeted proof required by that phase. Do not rely on this handover as a
substitute for inspecting current source before proposing code patches.
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
    periodic interval previously 500 frames unless changed in current source.

Proof diagnostics:
    controlled by compile-time flags in cnr3_build_config.h.
    dedicated proof runs temporarily enable exactly the needed proof gates.
    normal committed state disables proof gates again.
```

Required near-term diagnostic improvement:

```text
Add a human-readable duplicate/recompute waste summary block when duplicate
recompute waste is relevant to a proof or design decision. Raw counters already
exist, but a formatted block with counts and percentages is required by CMS06.3
and the handover production spec v1.4.
```

Deferred diagnostic items:

```text
G-DIAG-RECALC-HIST-01:
    compile-time recalculation histogram for frames computed once, twice, etc.

G-DIAG-LOG-VOLUME-01:
    compile-time log-volume controls for long proof/test runs.
```

---

## C6. Immediate next task

Recommended immediate next task:

```text
CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof
```

Goal:

```text
Prove the mechanics of actually returning a recovery-stored cached frame under a
debug/proof gate, without treating that as final general output-cache authority.
```

Important distinction:

```text
Allowed:
    proof-only recovery-return transfer mechanics under a dedicated gate.

Not allowed yet:
    final general output-cache authority;
    final recovery/output-authority transition;
    fmParallelRequests or fmParallel enablement;
    use of exact_match as a bounded-recovery return gate.
```

Files likely needed:

```text
cnr3_build_config.h
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_frame_internal_processing.h
cnr3_frame_internal_processing.cpp
```

High-level changes expected:

```text
1. Inspect current post-G10D.7 source before proposing patches.
2. Add/enable a proof gate for recovery-return transfer, if not already present.
3. Reuse the recovery compute/store path proven by G10D.5.
4. Acquire a caller-owned cached-frame lookup ref.
5. Transfer that ref to VapourSynth as the returned frame.
6. Increment/record lookup-owned transferred accounting.
7. Do not freeFrame() the transferred ref locally.
8. Free any normal strict-path dst if it was computed but not returned.
9. Destroy source_request_plan on all return/error paths.
10. Keep output authority explicitly proof-only.
11. Disable proof gates again before the normal-state commit unless the user explicitly chooses otherwise.
```

Safety constraints:

```text
- Do not treat G10D.9 as final output-cache authority.
- Do not enable fmParallelRequests.
- Do not enable fmParallel.
- Do not use exact_match as a return condition.
- Do not mutate old_strict_cache.prev_output from the recovery-return path.
- Do not mutate old_strict_cache.next_needed from the recovery-return path.
- Do not lose caller-owned lookup ref accounting.
- Do not skip cleanup of source_request_plan, source frames, local recovered frames, or unused dst frames.
```

Design-compliance requirement:

```text
Before final output authority, review remaining CMS02-F-labelled obligations,
recovery authority, duplicate/recompute waste, RAII-wrapper status, and the
old_strict_cache/fmParallel readiness issue against CMS06.3 and current source.
```

---

## C7. Do not implement in the next session unless explicitly chosen

Do not implement in CMS02-G / SubPhase G10D.9 unless the user explicitly changes the task:

```text
- final general output-cache authority
- a permanent production recovery-return policy
- fmParallelRequests wiring
- full fmParallel support
- non-checkpoint pinning
- bounded warm-up recovery as final production behaviour
- changes to recursive blend maths
- changes to scene-change detection
- mass diagnostic string renames
- diagnostic mode redesign beyond the specific duplicate/waste summary if chosen
- broad cleanup of old strict-streaming code
- retiring old_strict_cache.next_needed / prev_output without a specific design step
```

---

## C8. Do not lose / named deferred items

### G-PAR-HZ-ARINITIAL-01 - hot-zone update must be at `arInitial`

Hot-zone updates should be treated as prerequisite work before `fmParallelRequests` or `fmParallel` development. Under future concurrent request modes, deferring hot-zone update to `arAllFramesReady` would be unsafe because pruning must know about active request intent as early as possible.

Current status:

```text
Resolved in CMS06.2 and preserved in CMS06.3.
The current committed code updates hot zones at arInitial before requestFrameFilter(). Preserve this.
Do not move hot-zone updates back to arAllFramesReady.
```

### G-DIAG-RECALC-HIST-01 - compile-time recalculation histogram

Add a compile-time-only diagnostic table later showing how many frames were calculated once, twice, three times, etc. The table should be per-instance only and should count actual frame calculations, not cache returns. It should include the normal once-only row unless later log-volume policy says otherwise.

Purpose:

```text
Quantify duplicate recovery computation and first-in-best-dressed discard cost.
```

### G-DIAG-DUP-WASTE-SUMMARY-01 - human-readable duplicate/recompute waste summary

Add a human-readable summary block using existing raw counters:

```text
store_attempts
store_successes
duplicate_skipped_already_cached
duplicate_computed_but_discarded
```

The block should include counts and percentages such as:

```text
Computed-discarded ratio:
    duplicate_computed_but_discarded / store_attempts = XX.XX%

Useful-store ratio:
    store_successes / store_attempts = XX.XX%
```

This should not replace grep-friendly one-line summaries. It is an additional human audit aid.

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

### G-FMPAR-OLD-STRICT-AUTH-01 - old strict authority is not final fmParallel authority

`old_strict_cache.next_needed` and `old_strict_cache.prev_output` are not final fmParallel authority. Before final fmParallel operation, they must be retired, bypassed, or redesigned so final correctness does not depend on strict-streaming sequencing state.

### DEAD-SUPERSEDED-CODE-01 - later cleanup of compatibility wrappers and old strict path

At a later dead/superseded-code cleanup point, likely near the end of development, check whether the `process_cnr3_frame(...)` compatibility wrapper can be removed. It may become removable only after all normal and recovery paths have moved to explicit predecessor handling and the old strict-streaming authority path is retired.

---

## C9. Remaining cleanup/deferred notes

Deferred cleanup:

```text
- Some comments and diagnostics may still contain older CMS05/CMS06 wording.
  Do not mix broad wording cleanup into G10D.9 unless it is required to avoid a
  specific misunderstanding.

- Cnr3OwnedFrameRef RAII wrapper is not implemented. Explicit ref handling is
  currently acceptable while acquired == released + transferred remains clean.
  If explicit handling produces a balance error or review finds unsafe exit
  paths, implementing the wrapper becomes a corrective action before further
  development.

- General cache-hit return exists, but final recovery/output-cache authority is
  not complete.

- CMS02-F-labelled obligations are not a blanket blocker but must be audited
  item-by-item before final output authority.
```

Optional next diagnostic before or during G10D.9:

```text
Add the duplicate/recompute waste summary block if the user chooses to do it
before the first transfer proof. It is useful but should not be mixed into a
large authority change without care.
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
short realclip or blankclip smoke test where relevant;
targeted proof run required by the current phase;
disabled-state smoke test after restoring normal proof gates, unless the user explicitly chooses build-only or no-smoke-test for that commit.
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

For phases involving actual transfer to VapourSynth:

```text
lookup_owned_ref_transferred_total must increase by the number of transferred
caller-owned cached-frame refs, and those transferred refs must not be
freeFrame()d locally after transfer.
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

Recent completed commit themes after v1.4:

```text
CMS02-G / SubPhase G10ABC:
    dry-run recovery compute skeleton

CMS02-G / SubPhase G10D.1:
    local single-frame compute proof / prep

CMS02-G / SubPhase G10D.2:
    local bounded-walk compute proof

CMS02-G / SubPhase G10D.3:
    cleanup/reference diagnostics proof

CMS02-G / SubPhase G10D.4:
    proof scaffold cleanup/review comments

CMS02-G / SubPhase G10D.5:
    local bounded-walk store proof

CMS02-G / SubPhase G10D.6:
    recovery-store sample-difference measurement proof

CMS02-G / SubPhase G10D.7:
    recovery-return decision dry-run

CMS02-G / SubPhase G10D.8:
    output-authority-transition readiness review; no code change
```

Suggested commit-title style for next proof:

```text
Complete CMS02-G / SubPhase G10D.9 recovery-return transfer proof
```

The commit body should state whether recovered output was actually returned under the proof gate, how many lookup refs were transferred, whether normal strict output was bypassed for proof candidates, and whether final cache/ref and lookup balances returned cleanly.

---

## C12. New-chat starter prompt

```text
We are continuing CNR3 development after CMS02-G / SubPhase G10D.8.

Please read the uploaded documents in this order:

1. Document_A_CNR3_Project_Context_and_Rules_v1.6.md
2. Document_B_CNR3_Decision_Log_v1.6.md
3. Document_C_CNR3_Current_Session_Handover_v1.6.md
4. cnr3_cache_manager_design_v6_3.md, or any later spec explicitly superseding CMS06.3
5. Current source files/logs

Important:
- The new chat has no memory of prior chats.
- Treat Document_C_CNR3_Current_Session_Handover_v1.6.md as the source of truth for current state.
- Treat Document_B_CNR3_Decision_Log_v1.6.md as the source of truth for settled decisions.
- Treat CMS06.3 as the current detailed cache-manager design reference.
- Use expanded phase/SubPhase naming, for example CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof.
- Do not re-litigate settled decisions unless current code or logs prove a real problem.
- Follow Rule 1 for code comments.
- Follow Rule 2 for before/after code update instructions.
- Do not implement anything listed in the current handover's "Do not implement" section.
- Do not use exact_match as a bounded-recovery return condition.
- Do not enable fmParallelRequests or fmParallel.

First, confirm your understanding of the current state and immediate next task.
Then wait for the current code files if they have not already been uploaded.
```

---

## Appendix B - Summary from CMS02-G / SubPhase G10ABC through G10D.8

**Date:** 2026-06-07

### Summary

After the v1.4 handover, development completed the G10ABC and G10D.1 through G10D.8 proof chain. The chain moved from dry-run recovery compute scaffolding to local compute proof, bounded-walk compute proof, store proof, sample-difference measurement, and return-decision dry-run.

The latest committed code-changing SubPhase is CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run. CMS02-G / SubPhase G10D.8 was a review/decision step and required no code change.

### Key outcomes

- Recovery compute/store mechanics have been staged and proven under debug gates.
- Recovery-stored frames were compared against normal strict output in G10D.6; the tested sequential runs were exact matches, but exact match remains diagnostic only.
- G10D.7 proved that recovery-stored candidates could be looked up and would be returnable in a dry-run sense, but they were released and normal strict output was returned.
- The next proof may exercise actual transfer of a recovered cached frame to VapourSynth, but only under a proof gate and not as final general output-cache authority.
- CMS06.3 supersedes CMS06.2 and corrects stale CMS02-F status.
- The handover production spec v1.4 requires v1.6 to be based on the approved v1.4 documents, not the abbreviated v1.5 draft.

## Appendix A - Summary up to and including CMS02-G.9AB, prior to CMS02-G.10

**Date:** 2026-06-07

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
  - continue checking future work against CMS06.3 and the handover notes.
