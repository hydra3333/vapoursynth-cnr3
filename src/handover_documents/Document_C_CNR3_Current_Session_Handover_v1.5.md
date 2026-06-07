# Document C - CNR3 Current Session Handover v1.5

**Document type:** Current-state/session handover  
**Version:** v1.5  
**Date:** 2026-06-07  
**Status:** Current-state authority for the next CNR3 development chat  
**Companion design authority:** `cnr3_cache_manager_design_v6_3.md` / CMS06.3  
**Handover production spec used:** `CNR3_Handover_Pack_Production_Spec_v1.3.md`

---

## C1. Read order for a new chat

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.5.md`
2. `Document_B_CNR3_Decision_Log_v1.5.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.5.md`
4. `cnr3_cache_manager_design_v6_3.md`
5. Current source files and latest logs

Critical instructions:
- Treat this Document C as the source of truth for current implementation status.
- Treat Document B as the source of truth for settled decisions.
- Treat CMS06.3 as the detailed cache-manager design authority.
- If current source/logs conflict with this document, stop and ask for clarification.
- Inspect current source files before proposing code changes.
- Follow Rule 1 for code comments and Rule 2 for before/after update instructions.
- Do not implement anything in Section C7 unless explicitly chosen.

---

## C2. Repository/code context

Repository:
```text
https://github.com/hydra3333/vapoursynth-cnr3
```

Development environment:
```text
Visual Studio 2026
Branch: dev_cache_manager, unless current Git status says otherwise
VapourSynth API4 only
Current runtime mode: fmUnordered
```

Relevant source files for next cache/recovery work:
```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
src/cnr3_common.h
src/cnr3_output_cache_manager.h
src/cnr3_output_cache_manager.cpp
src/cnr3_frame_internal_processing.h
src/cnr3_frame_internal_processing.cpp
src/cnr3_memory_diagnostics.h
src/cnr3_memory_diagnostics.cpp
```

Upload latest source before the next chat proposes any code changes.

---

## C3. Current exact implementation status

Current phase:
```text
CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
```

Last completed committed phase:
```text
CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run
Commit title:
    Complete CMS02-G.10D.7 recovery-return decision dry-run
```

CMS02-G / SubPhase G10D.8 status:
```text
PASS by review/decision.
No runtime code change was made for G10D.8 in this chat.
```

Current design authority:
```text
CMS06.3
```

Current output authority:
```text
Partial output-cache authority:
    - arAllFramesReady cache-hit return is implemented.
    - On cache hit, output_cache can return cached_output to VapourSynth.
    - On cache miss, the code still falls through to strict-streaming/new computation.
    - Full recovery/output-cache authority is not complete.
```

Important distinction:
```text
CMS02-F cache-hit return is substantially implemented.
CMS02-G recovered-output return is not yet generally implemented.
CMS02-G / SubPhase G10D.7 was a dry-run decision proof only:
    actual_returned_recovered_output=0
    returned_normal_strict_output=1
    output_authoritative=0
```

Latest observed proof edit marker:
```text
CMS02-G10D7-recovery-return-decision-dry-run-v1
```

Note:
```text
The final disabled-state CNR3_EDIT_VERSION string after the G10D.7 commit was not directly verified in this handover.
Verify current src/cnr3_build_config.h before any next patch.
```

---

## C4. Completed since older companion design snapshots

CMS06.3 is current through CMS02-G / SubPhase G10D.8 and already corrects older CMS06.2 implementation-state drift.

Completed proof chain:

```text
CMS02-G / SubPhase G10ABC / recovery compute dry-run scaffold
    COMPLETE / committed

CMS02-G / SubPhase G10D.1 / local single-frame compute proof
    COMPLETE / committed

CMS02-G / SubPhase G10D.2 / local bounded-walk compute proof
    COMPLETE / committed

CMS02-G / SubPhase G10D.3 / diagnostics cleanup proof
    COMPLETE / committed

CMS02-G / SubPhase G10D.4 / proof-scaffold cleanup/review
    COMPLETE / committed

CMS02-G / SubPhase G10D.5 / local bounded-walk store proof
    COMPLETE / committed

CMS02-G / SubPhase G10D.6 / recovery-store sample-difference measurement proof
    COMPLETE / committed

CMS02-G / SubPhase G10D.7 / recovery-return decision dry-run
    COMPLETE / committed

CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review
    PASS by decision/review, no code change
```

---

## C5. CMS02-F audit status

Do not preserve old "CMS02-F not started" wording.

Corrected current audit:

```text
- cnr3_output_cache_find_frame_and_add_ref() is implemented.
- arAllFramesReady cache-hit lookup is implemented.
- cnr3_output_cache_note_lookup_ref_transferred() is implemented.
- CACHE-HIT-RETURN path is implemented.
- Cache misses still fall through to strict-streaming/new computation.
- Full recovery/output-cache authority is not complete.
- Cnr3OwnedFrameRef RAII wrapper is not implemented; explicit ref handling is used.
- Lookup acquired/released/transferred counters exist.
- Any remaining CMS02-F-labelled obligations must be audited item-by-item against current source/logs.
```

---

## C6. Latest meaningful test/proof evidence

Latest detailed proof evidence from this chat:

### CMS02-G / SubPhase G10D.6 / recovery-store sample-difference measurement proof

20-frame enabled proof:
```text
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
```

Longer 3640-frame enabled proof:
```text
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
store_attempts=7279
store_successes=3640
store_failures=0
duplicate_skipped_already_cached=3639
duplicate_computed_but_discarded=3639
validation_failures=0
integrity_errors=0
ref_balance_errors=0
lookup_ref_balance=0
addframeref_total=3640
freeframe_total=3640
final cache ref balance returns to zero after clear
```

Disabled-state smoke test for G10D.6:
```text
store_attempts=20
store_successes=20
store_failures=0
duplicate_skipped_already_cached=0
duplicate_computed_but_discarded=0
lookup_ref_balance=0
validation_failures=0
integrity_errors=0
ref_balance_errors=0
clear_successes=1
addframeref_total=20
freeframe_total=20
final cache ref balance returns to zero
```

### CMS02-G / SubPhase G10D.7 / recovery-return decision dry-run

20-frame enabled proof:
```text
frames_checked=20
candidates_found=19
frames_skipped_no_candidate=1
would_be_returnable=19
lookup_refs_released=19
lookup_failures=0
actual_recovered_returns=0
output_authoritative=0
would_transfer_lookup_ref_to_vapoursynth=0

store_attempts=39
store_successes=20
store_failures=0
duplicate_skipped_already_cached=19
duplicate_computed_but_discarded=19
validation_failures=0
integrity_errors=0
ref_balance_errors=0
lookup_ref_balance=0
addframeref_total=20
freeframe_total=20
final cache ref balance returns to zero after clear
```

Disabled state after G10D.7:
```text
User restored proof gates to disabled and source-request back-frame count to 2.
No separate disabled smoke test was run for the G10D.7 commit.
```

Hard-gate interpretation:
```text
G10D.6 and G10D.7 enabled proofs passed.
G10D.7 disabled state was restored but not separately smoke-tested.
Before starting G10D.9, inspect current source and consider a short baseline smoke test.
```

---

## C7. Current diagnostic policy

Current diagnostics include:
- compact per-frame output-cache trace;
- full output-cache summary at selected points;
- recovery difference-measurement proof summaries when enabled;
- recovery return-decision proof summaries when enabled;
- memory diagnostics where relevant.

Required hard checks:
```text
invariants_ok=1
integrity_errors=0
validation_failures=0
ref_balance_errors=0
store_failures=0 unless deliberately testing failures
prune_after_store_failures=0 unless deliberately testing failures
cache_addframeref_total - cache_freeframe_total matches live cached frame count before clear
cache_addframeref_total - cache_freeframe_total is 0 after clear
clear_successes=1 after teardown when cached frames existed
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

Near-term required diagnostic improvement from CMS06.3:
```text
Add a human-readable duplicate/recompute waste summary block.
Raw counters already exist:
    store_attempts
    store_successes
    store_skipped_already_cached / duplicate_skipped_already_cached
    duplicate_store_computed_but_discarded / duplicate_computed_but_discarded

The new block should include counts and percentages.
```

Example required format:
```text
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

---

## C8. Immediate next task

Recommended next task:
```text
CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof
```

Before code:
```text
1. Upload and inspect the latest committed source files.
2. Verify current cnr3_build_config.h disabled-state proof gates.
3. Verify current CNR3_EDIT_VERSION.
4. Confirm current arAllFramesReady cache-hit return path and output-cache summary behaviour.
5. Confirm where to add the duplicate/recompute waste summary block.
```

Likely first implementation substep:
```text
CMS02-G / SubPhase G10D.9.1 / duplicate-recompute-waste-summary
```

Then:
```text
CMS02-G / SubPhase G10D.9.2 / recovery-return-transfer-proof
```

The exact numbering may be adjusted, but use expanded naming.

High-level G10D.9 intent:
```text
Proof-only recovered-frame transfer mechanics under a debug gate.

Acquire caller-owned recovered cached frame.
Transfer that lookup reference to VapourSynth as returned output.
Record lookup_owned_ref_transferred_total.
Do not freeFrame() the transferred ref locally.
Free the normal strict-path dst if it was computed but not returned.
Destroy source_request_plan on the return path.
Do not use exact_match as the return condition.
Do not make this final general output-cache authority.
```

Files likely needed:
```text
src/cnr3_build_config.h
src/vapoursynth-Cnr3.cpp
src/cnr3_output_cache_manager.h
src/cnr3_output_cache_manager.cpp
src/cnr3_common.h
src/cnr3_frame_internal_processing.h
src/cnr3_frame_internal_processing.cpp
```

---

## C9. Do not implement in the next session unless explicitly chosen

Do not implement unless explicitly chosen:
```text
- final output_cache authority for all misses
- fmParallelRequests wiring
- full fmParallel support
- non-checkpoint pinning
- bounded warm-up recovery beyond the selected proof task
- tolerance/quality policy for bounded-recovery sample differences
- using exact_match as a return gate
- broad diagnostic redesign
- mass diagnostic string renames
- recursive blend maths changes
- scene-change detection changes
- Cnr3OwnedFrameRef RAII wrapper unless explicit ref handling shows a problem or the user chooses it
- old strict-cache deletion
```

---

## C10. Remaining cleanup / deferred notes

Carry forward:

```text
- Add human-readable duplicate/recompute waste summary block.
- Preserve exact_match as diagnostic only.
- Audit any older phase-labelled obligations item-by-item before treating them as blockers.
- Confirm whether final disabled-state CNR3_EDIT_VERSION after G10D.7 is as expected.
- old_strict_cache.next_needed and old_strict_cache.prev_output are not final fmParallel authority.
- At final fmParallel readiness/output-authority transition, retire, bypass, or redesign old_strict_cache authority.
- Dead/superseded-code cleanup should later check whether process_cnr3_frame(...) compatibility wrapper can be removed.
- Dead/superseded-code cleanup should check old_strict_cache.next_needed and related strict-streaming state.
- Cnr3OwnedFrameRef remains recommended but not implemented; make mandatory only if explicit ref handling becomes unreliable.
- Consider baseline disabled smoke test before G10D.9 if source state is uncertain.
```

---

## C11. Safety checks before any future commit

Before committing a future subphase:
```text
Debug build must succeed.
Release build should succeed before larger phase commits.
Run a short realclip or blankclip smoke test where relevant.
Run any targeted proof required by the subphase.
Check all hard-gate diagnostics.
```

For lookup/return transfer work:
```text
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

For recovered-output return proof:
```text
actual_recovered_returns must match expected proof count.
lookup_ref_transferred must increase for returned cached outputs.
lookup_ref_balance must be 0 at quiescence.
cache ref balance must return to 0 after clear.
output authority wording must remain proof-only unless explicitly changed.
```

If any unexpected value appears:
```text
Stop.
Do not proceed until understood.
```

---

## C12. Suggested next commit-message rule

When the next SubPhase passes, provide a Visual Studio 2026 commit block.

Preferred title style:
```text
Complete CMS02-G / SubPhase G10D.9.1 duplicate-recompute waste summary
```

or:
```text
Complete CMS02-G / SubPhase G10D.9 recovery-return transfer proof
```

Use the body to record:
- intent;
- proof gate state;
- exact test evidence;
- lookup acquired/released/transferred balance;
- cache add/free balance;
- whether disabled-state smoke test was run.

---

## C13. New-chat starter prompt

```text
We are continuing CNR3 development.

Please read the uploaded documents in this order:
1. Document_A_CNR3_Project_Context_and_Rules_v1.5.md
2. Document_B_CNR3_Decision_Log_v1.5.md
3. Document_C_CNR3_Current_Session_Handover_v1.5.md
4. cnr3_cache_manager_design_v6_3.md
5. Current source files and logs

Important:
- The new chat has no memory of prior chats.
- Treat Document C v1.5 as the source of truth for current implementation state.
- Treat Document B v1.5 as the source of truth for settled decisions.
- Treat CMS06.3 as the detailed cache-manager design authority.
- Use expanded naming such as CMS02-G / SubPhase G10D.9 / recovery-return-transfer-proof.
- Do not re-litigate settled decisions unless current code or logs prove a real problem.
- Follow Rule 1 for code comments.
- Follow Rule 2 for before/after code update instructions.
- Do not implement anything listed in Document C's "Do not implement" section unless explicitly chosen.
- Do not use exact_match as a bounded-recovery return condition.
- Do not treat G10D.9 as final output-cache authority.

First, confirm your understanding of:
1. the current status after CMS02-G / SubPhase G10D.8,
2. the immediate next task,
3. the files needed before proposing code changes,
4. whether enough information exists to proceed safely.

Do not propose code changes until current relevant source files have been inspected.
```
