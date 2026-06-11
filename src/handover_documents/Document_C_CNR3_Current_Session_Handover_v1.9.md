# Document C - CNR3 Current Session Handover

**Document:** C of CNR3 handover pack  
**Version:** v1.9  
**Date:** 2026-06-11  
**Status:** Enhanced current-session handover through CMS02-H16.4 and CMS06.10 H15.6B restructuring; generated from v1.8 baseline with above-normal explanatory detail.  
**Companion design authority:** CMS06.10, or any later cache design specification that explicitly supersedes CMS06.10.  
**Current matched pack:** A/B/C v1.9

---

## C1. Read order for a new chat

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.9.md`
2. `Document_B_CNR3_Decision_Log_v1.9.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.9.md`
4. `cnr3_cache_manager_design_v6_10.md`
5. `CNR3_CMS06_10_Handover_Reconciliation_Notes_20260611.md`
6. current relevant source files from the user's repository
7. latest build/test logs if the task depends on evidence

Rules for the new chat:

```text
Treat this Document C v1.9 as the source of truth for current implementation status.
Treat Document B v1.9 as the source of truth for settled decisions.
Treat CMS06.10 as the current detailed cache-manager design authority.
Treat earlier H15.6B fail-closed-only patch artifacts as superseded draft work.
Do not implement active source-request reduction until H15.6B.1 and H15.6B.2 are proven.
Do not claim fmParallel readiness.
Do not resurrect old strict state as output authority.
Do not rely on fmUnordered serial callback ordering as a correctness foundation.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
```

---

## C2. Repository/code context

Known repository:

```text
https://github.com/hydra3333/vapoursynth-cnr3
```

Core files normally required before coding:

```text
cnr3_build_config.h
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_frame_internal_processing.h
cnr3_frame_internal_processing.cpp
```

The user's current committed state is after the pre-H15.6B checkpoint commit. That checkpoint records:

```text
CHECKPOINT before H15.6B active source-request reduction
```

It includes H16.3 and H16.4 as complete, records that H15.6B will be restructured, and records the future H17 sparse-hole/minimal fallback recovery optimisation for later handover/CMS updates.

---

## C3. Current exact implementation status

### C3.1 Completed/PASS status relevant to this handover

```text
H15.5  / sequential fast-path return-transfer proof          PASS / committed
H15.6A / arInitial request classification probe              PASS / committed
H16.1  / bounded checkpoint-search contract closeout          PASS / committed
H16.2  / bounded checkpoint-search helper proof               PASS / committed
H16.3  / predecessor-missing fallback return-transfer proof   PASS / committed
H16.4  / out-of-order fallback validation                     PASS / committed
```

The pre-H15.6B checkpoint commit has also been made.

### C3.2 Current controlling design after CMS06.10

CMS06.10 supersedes CMS06.9 and is now controlling. The most important change is that H15.6B is no longer a single active source-request reduction phase. It is split into:

```text
H15.6B.1 / arInitial predecessor reservation lifecycle proof
H15.6B.2 / reserved-predecessor fast-path consumption proof
H15.6B.3 / active sequential source-request reduction
```

The previous fail-closed-only H15.6B patch artifacts are superseded draft work. Do not commit or resume them as the implementation direction.

---

## C4. Latest proof/evidence summary

### C4.1 H16.3 summary

H16.3 deliberately removed cached predecessor output 5 for requested frame 6, forced the sequential fast path to decline, and proved bounded-warmup fallback could recompute/store/return requested output 6 through output-cache authority without old strict mutation.

Important evidence shape:

```text
requested=6
predecessor=5
predecessor_cache_contains_before=1
remove_attempted=1
remove_ok=1
predecessor_cache_contains_after=0
returned_fast_path_output=0
reason=predecessor-not-cached
fallback_returned_bounded_warmup_output=1
requested_output_cached=1
predecessor_cached_after_fallback=1
output_authoritative=1
old_strict_bypassed=1
old_strict_streaming_gate_quarantined=1
mutates_old_strict=0
proof_ok=1
```

Final cleanup showed clean cache/ref state. This proves conservative predecessor-missing fallback correctness.

### C4.2 H16.4 summary

H16.4 validated natural out-of-order request order:

```text
[1, 0, 3, 2, 4, 6, 5, 7, 14, 8, 9, 11]
```

It proved mixed fallback/fast-path selection:

```text
predecessor missing or non-sequential uncertain:
    source_request_reduction=0
    bounded-warmup fallback returns
    output_authoritative=1
    mutates_old_strict=0

predecessor present and safe:
    sequential fast path can return
    output_authoritative=1
    mutates_old_strict=0
```

Final cleanup showed clean cache/ref state.

---

## C5. Why H15.6B changed direction

The earlier H15.6B idea was:

```text
arInitial:
    check output_cache[N-1]
    if present, request only source N

arAllFramesReady:
    look up output_cache[N-1] again
    compute output N
```

That creates a time-of-check/time-of-use gap. If N-1 disappears after arInitial but before arAllFramesReady, fallback would need source frames that were not requested in the same arInitial activation, violating VS-LIFECYCLE-01.

Under current fmUnordered the gap is structurally unreachable, but final target fmParallel/fmParallelRequests cannot rely on serial callback ordering. CMS06.10 therefore adopts Option B:

```text
arInitial:
    atomically find-and-addref output_cache[N-1]
    carry that caller-owned ref in frameData

arAllFramesReady:
    use the carried ref directly
    release/null it according to single-ownership discipline
```

This makes predecessor validity an acquired ownership fact, not a hoped-for cache state.

---

## C6. Immediate next task

The next coding task is:

```text
CMS02-H15.6B.1 / arInitial predecessor reservation lifecycle proof
```

### C6.1 Goal

Add the infrastructure for frameData to own a reserved predecessor `VSFrame*` caller-owned lookup ref, acquired in arInitial by atomic find-and-addref, carried to arAllFramesReady, and released safely on every exit path.

### C6.2 Scope

H15.6B.1 must not reduce source requests.

```text
Allowed:
    add reserved predecessor fields to frameData/source-plan structure
    acquire N-1 with find_frame_and_add_ref in arInitial for class-B candidates
    carry ref across arInitial -> arAllFramesReady
    release ref via frameData cleanup path
    add diagnostics/counters proving lifecycle balance

Not allowed:
    active source-request reduction
    consuming the ref as the actual fast-path predecessor if that changes current fast-path sourcing
    sparse-hole optimisation
    old strict authority restoration
    fmParallel/fmParallelRequests claim
```

### C6.3 H15.6B.1 PASS criteria

Expected proof points:

```text
reserved_predecessor_acquired > 0 on safe sequential candidates
reserved_predecessor_expected matches acquired candidates
reserved_predecessor_present=1 when carried to arAllFramesReady
reserved_predecessor_released_by_cleanup accounts for non-consumed refs
reserved_predecessor_consumed=0 in B.1 if consumption is not yet active
reduced_fastpath_predecessor_lost=0
lookup_ref_balance=0
cache ref balance clean after cleanup
total_pin_count=0
has_pinned_checkpoints=0
checkpoint_unpin_underflow_errors=0
mutates_old_strict=0
```

If H15.6B.1 deliberately acquires and then releases without consuming, the summary must make that explicit. Do not confuse release-by-cleanup with consumption.

---

## C7. Next recommended subphases after H15.6B.1

### C7.1 H15.6B.2 / reserved-predecessor fast-path consumption proof

Goal:

```text
Use the carried frameData predecessor ref as the fast-path predecessor.
Eliminate/reduce the second arAllFramesReady lookup of N-1 for the reserved path.
Keep conservative source requests.
Prove null-on-consume and no double-release/no leak.
```

Mandatory proof assertion:

```text
single-ownership/null-on-consume:
    successful consumption releases or consumes the frameData ref exactly once
    frameData reserved_predecessor_frame is set to nullptr after consumption
    cleanup sees nullptr and does not release it again
```

### C7.2 H15.6B.3 / active sequential source-request reduction

Goal:

```text
For class-B candidates with reserved predecessor ref already acquired:
    request source N only

For all fallback/uncertain classes:
    keep conservative bounded-warmup source window
```

H15.6B.3 is the first subphase allowed to change the requested source-frame set.

---

## C8. Current diagnostic policy

The following evidence must continue to appear in logs/summaries where relevant:

```text
lookup_owned_ref_acquired_total
lookup_owned_ref_released_total
lookup_owned_ref_transferred_total
lookup_ref_balance
cache_addframeref_total
cache_freeframe_total
cache ref balance
checkpoint_pin_attempts
checkpoint_pin_successes
checkpoint_unpin_attempts
checkpoint_unpin_successes
checkpoint_unpin_failures
checkpoint_unpin_underflow_errors
total_pin_count
has_pinned_checkpoints
reserved_predecessor_acquired
reserved_predecessor_consumed
reserved_predecessor_released_by_cleanup
reserved_predecessor_expected
reserved_predecessor_present
reduced_fastpath_predecessor_lost
```

The `reduced_fastpath_predecessor_lost` counter is not normal fallback. It is an invariant-breach diagnostic and must be zero for PASS.

---

## C9. Ownership-proof standing rule

From this handover onward, every cache-affecting proof/validation phase that touches cache lookup, return, fallback, source-request reduction, checkpoint recovery, or cached-frame use must explicitly prove both ownership systems where applicable.

### C9.1 Lookup-addref ownership

For ordinary cached output frames:

```text
lookup addref is acquired while holding the cache mutex
caller-owned ref is released or transferred exactly once
lookup_ref_acquired == lookup_ref_released + lookup_ref_transferred
lookup_ref_balance == 0
```

### C9.2 Checkpoint pin ownership

For checkpoint/recovery anchors:

```text
pin/unpin counts are paired
no underflow
no unpin failures
no pinned checkpoint remains at cleanup
total_pin_count=0
has_pinned_checkpoints=0
```

Do not conflate these systems. Ordinary cached predecessor frames use lookup addref/release, not checkpoint pin/unpin. Checkpoint anchors may use pin/unpin for plan lifetime and lookup addref for actual frame access.

---

## C10. Future H17 sparse-hole/minimal fallback recovery optimisation

Track this as a future optimisation, not as H15.6B work.

Current conservative fallback may:

```text
retrieve/recompute a bounded lower frame that is already cached
discard the duplicate output through first-in-best-dressed duplicate handling
compute/store the missing forward frames and requested output
```

Future H17-style sparse-hole repair should:

```text
start from nearest suitable cached predecessor/checkpoint inside the bounded recovery window
retrieve only missing source frames needed to walk forward
compute/store only missing outputs plus the requested output
return through output-cache authority
```

Do not mix this with H15.6B.1/B.2/B.3.

---

## C11. Do not implement next unless explicitly chosen

Do not implement the following unless the user explicitly asks after reading this handover:

```text
- active source-request reduction before H15.6B.1 and H15.6B.2 pass;
- future H17 sparse-hole/minimal fallback optimisation;
- fmParallelRequests/fmParallel mode changes;
- non-checkpoint pinning;
- old strict-state final retirement;
- broad diagnostic cleanup;
- RAII Cnr3OwnedFrameRef conversion across the project;
- unrelated comment/layout/name cleanup.
```

---

## C12. Safety checks before any future commit

Before marking a future phase PASS or providing a commit message, verify:

```text
- exact edit marker matches the phase being tested;
- no unintended source-request reduction is active;
- old strict state remains bypassed/quarantined and is not mutated;
- lookup-ref ownership is balanced;
- cache addref/free balance is clean after cleanup;
- checkpoint pin/unpin ownership is balanced where relevant;
- frameData reserved predecessor ownership is released or consumed exactly once;
- null-on-consume is proven once B.2 begins;
- total_cached_frame_count=0 after cleanup;
- invariants_ok=1;
- integrity_errors=0;
- validation_failures=0;
- ref_balance_errors=0.
```

---

## C13. Recent commit messages / checkpoint messages

The user used this checkpoint message before H15.6B:

```text
CHECKPOINT before H15.6B active source-request reduction

- Checkpointed the output-cache authority proof state before enabling H15.6B.
- Confirmed H16.3 predecessor-missing fallback return-transfer proof is complete.
- Confirmed H16.4 out-of-order fallback validation is complete.
- Confirmed predecessor-missing and non-sequential fallback requests retain conservative bounded-warmup source requests.
- Confirmed predecessor-present requests can return through the sequential fast path.
- Confirmed old strict state remains bypassed/quarantined and is not mutated.
- Confirmed source-frame, local-output, lookup-ref, and cache-owned-frame balances are clean.
- Confirmed checkpoint pin/unpin ownership remains balanced with no pinned checkpoints at cleanup.
- Recorded that H15.6B will be the first active sequential source-request reduction phase.
- Recorded that future cache-affecting phases must explicitly prove lookup-addref ownership and checkpoint pin/unpin ownership where relevant.
- Recorded the future H17 sparse-hole/minimal fallback recovery optimization for later handover and CMS updates.
```

Note: after CMS06.10, H15.6B is still the umbrella for source-request reduction, but it is now split. H15.6B.1 and H15.6B.2 occur before active source-request reduction.

---

## C14. New-chat starter prompt

```text
We are continuing the CNR3 VapourSynth API4 plugin project.
Read Document A/B/C v1.9 and cnr3_cache_manager_design_v6_10.md first.
The current committed state is after H16.3, H16.4, and the pre-H15.6B checkpoint.
CMS06.10 is controlling.
The previous fail-closed-only H15.6B patch is superseded and must not be committed.
Next task is CMS02-H15.6B.1: arInitial predecessor reservation lifecycle proof.
Do not reduce source requests yet.
Implement and prove frameData ownership of a reserved predecessor VSFrame ref acquired by find_frame_and_add_ref in arInitial.
Prove release on all exit paths and clean lookup-ref/cache-ref/pin balances.
Follow Rule 1 and Rule 2.
```

---

## Appendix A - v1.9 update summary

v1.9 is an enhanced checkpoint handover produced after the H15.6B design review. It reconciles the handover pack with CMS06.10 and preserves v1.8 history below.

---

## Appendix B - Historical v1.8 Document C retained for continuity

The following content is retained for historical continuity. Where it conflicts with the v1.9 sections above or CMS06.10, v1.9/CMS06.10 controls.

# Document C - CNR3 Current Session Handover

**Document:** C of CNR3 handover pack  
**Version:** v1.8  
**Date:** 2026-06-11  
**Status:** Current session handover through CMS02-H15.5; generated from v1.7 baseline with current-state override and preserved historical context.  
**Companion design authority:** CMS06.7, or any later cache design specification that explicitly supersedes CMS06.7.  
**Current matched pack:** A/B/C v1.8

---

## C1. Read order for a new chat

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.8.md`
2. `Document_B_CNR3_Decision_Log_v1.8.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.8.md`
4. `cnr3_cache_manager_design_v6_7.md`
5. `CNR3_CMS06_8_Proposed_Delta_Clarifications_20260611.md` if discussing proposed CMS06.8 updates
6. current relevant source files
7. latest logs if the task depends on test evidence

Rules for the new chat:

```text
Treat this current session handover as the source of truth for current status.
Treat the decision log as the source of truth for settled decisions.
Treat CMS06.7 as the current accepted detailed cache-manager design authority.
Treat the CMS06.8 proposed-delta document as a proposal until explicitly adopted.
If the design spec and current code appear to conflict, inspect the source and stop for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in "Do not implement in the next session".
Do not depart from durable rules silently. Any intentional departure requires
explicit clarification, discussion, agreement, and documentation before proceeding.
```

CMS06.7 is the current accepted cache design authority in this pack. Earlier
CMS06.6, CMS06.5, CMS06.4, CMS06.3, CMS06.2, CMS06.1, CMS06, and CMS05.x
documents are superseded except as history.

If a companion design spec contains a current implementation state snapshot,
this Document C overrides it for current implementation status. If the current
source conflicts with this document, inspect the source and stop for
clarification rather than guessing.

---

## C2. Repository/code context

Known repository:

```text
https://github.com/hydra3333/vapoursynth-cnr3
```

Current source files required for the next coding session:

```text
cnr3_build_config.h
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_frame_internal_processing.h
cnr3_frame_internal_processing.cpp
```

The user's local tree after CMS02-H15.5 should contain the H15.5 committed
source or an equivalent manually-applied patch state:

```text
CNR3_EDIT_VERSION = CMS02-H15.5-sequential-fast-path-return-transfer-proof-v1-ENABLED
CNR3_CMS02_H15_ENABLE_SEQUENTIAL_FAST_PATH_RETURN_TRANSFER_PROOF = true
CNR3_CMS02_H15_ENABLE_OUTPUT_CACHE_AUTHORITY_NORMAL_PATH = true
```

The exact current source files should still be uploaded before the next chat
proposes code patches. This handover records expected state; exact before/after
patches must be grounded in the uploaded current source.

---

## C3. Current exact implementation status

### C3.1 Cache/output-authority migration status

The current selected path has progressed from bounded-warmup proofs to a
working sequential fast-path return-transfer proof.

Current behaviour after H15.5:

```text
Frame 0:
    no predecessor exists;
    falls back to bounded-warmup normal path;
    returns output_cache[0] through OUTPUT-CACHE-AUTHORITY-NORMAL-PATH-RETURN.

Frame N > 0 where output_cache[N-1] exists:
    lookup output_cache[N-1] and acquire caller-owned predecessor ref;
    acquire source frame N;
    allocate local output frame N;
    compute output N using process_cnr3_frame_with_explicit_previous_output();
    store output N in output_cache;
    release local output, source frame, and predecessor lookup refs as required;
    lookup output_cache[N];
    transfer that lookup ref as the returned frame;
    emit OUTPUT-CACHE-AUTHORITY-SEQUENTIAL-FAST-PATH-RETURN.
```

The old strict cache remains present but quarantined from selected output
authority. Current selected-path logs must continue proving:

```text
old_strict_bypassed=1
old_strict_streaming_gate_quarantined=1
mutates_old_strict=0
```

No claim has been made that the current implementation is ready for
`fmParallelRequests` or `fmParallel`.

### C3.2 Completed recent subphases

The current session progressed through these notable proof/cleanup phases:

```text
CMS02-H13  old strict streaming gate quarantine proof                         PASS
CMS02-H14.1 output-cache authority cutover scaffold                           PASS
CMS02-H14.2 selected output-cache authority cutover proof                      PASS
CMS02-H14.3 selected authority log/reason cleanup                             PASS
CMS02-H14.4 selected authority generic label cleanup                          PASS
CMS02-H14.5 selected authority helper name cleanup                            PASS
CMS02-H15.1 output-cache authority normal-path scaffold                       PASS
CMS02-H15.2 sequential predecessor-cache reuse probe                          PASS
CMS02-H15.3 sequential fast-path dry run                                      PASS
CMS02-H15.4 sequential fast-path compute/store proof                          PASS
CMS02-H15.5 sequential fast-path return-transfer proof                        PASS
```

### C3.3 Current known limitation after H15.5

H15.5 proves return-transfer, but `arInitial` still creates and requests the
bounded-warmup source plan before the fast-path return decision. This means the
next task is not to prove that fast-path return can work; that is done. The next
task is to reduce the source-frame request plan safely.

Current limitation:

```text
arInitial still requests bounded-warmup source frames for sequential fast-path
candidates even though arAllFramesReady can return through the sequential fast
path for frames N > 0 with cached output N-1.
```

This is a VapourSynth lifecycle-sensitive area. Any source frame retrieved in
`arAllFramesReady` must have been requested in the same callback activation's
`arInitial`. Do not change request behaviour without an explicit H15.6/H15.7
proof.

---

## C4. Latest test evidence

### CMS02-H15.1 normal-path scaffold

Evidence summary:

```text
edit_version=CMS02-H15.1-output-cache-authority-normal-path-scaffold-v1-ENABLED
Output 5 frames
OUTPUT-CACHE-AUTHORITY-NORMAL-PATH-END emitted
OUTPUT-CACHE-AUTHORITY-NORMAL-PATH-RETURN emitted
output_authoritative=1
old_strict_bypassed=1
old_strict_streaming_gate_quarantined=1
old_strict_state_unchanged=1
mutates_old_strict=0
lookup_ref_balance=0
final cache ref balance=0
```

### CMS02-H15.2 sequential predecessor-cache reuse probe

Evidence summary from 50-frame sequential validation:

```text
edit_version=CMS02-H15.2-sequential-predecessor-cache-reuse-probe-v1-ENABLED
Output 50 frames
frame 0: predecessor_lookup_attempted=0, reason=frame-zero-has-no-predecessor
frames 1 onward: predecessor_cache_hit=1, lookup_ref_released=1
lookup_ref_acquired=99
lookup_ref_released=49
lookup_ref_transferred=50
lookup_ref_balance=0
final cache ref balance=0
```

### CMS02-H15.3 sequential fast-path dry run

Evidence summary from 50-frame sequential validation:

```text
edit_version=CMS02-H15.3-sequential-fast-path-dry-run-v1-ENABLED
Output 50 frames
frame 0: would_request_current_source_only=0, would_compute_current_only=0
frames 1 onward: would_reuse_predecessor=1, would_request_current_source_only=1, would_compute_current_only=1
dry_run_only=1
output_authoritative=0 in dry run
existing normal path remains authoritative
lookup_ref_balance=0
final cache ref balance=0
```

### CMS02-H15.4 sequential fast-path compute/store proof

Evidence summary from 50-frame sequential validation:

```text
edit_version=CMS02-H15.4-sequential-fast-path-compute-store-proof-v1-ENABLED
Output 50 frames
frame 0: no predecessor, no fast-path compute/store attempt
frames 1 onward:
    predecessor_cache_hit=1
    current_source_acquired=1
    local_output_allocated=1
    process_ok=1
    store_ok=1
    predecessor_lookup_ref_released=1
    current_source_released=1
    local_output_released=1
proof_only=1
output_authoritative=0 in proof path
existing normal path remains authoritative
duplicate_store counts increase as expected because proof stores before fallback recompute
lookup_ref_balance=0
final cache ref balance=0
```

### CMS02-H15.5 sequential fast-path return-transfer proof

Evidence summary from 20-frame sequential validation:

```text
edit_version=CMS02-H15.5-sequential-fast-path-return-transfer-proof-v1-ENABLED
Output 20 frames
frame 0 falls back to OUTPUT-CACHE-AUTHORITY-NORMAL-PATH-RETURN
frames 1 onward emit OUTPUT-CACHE-AUTHORITY-SEQUENTIAL-FAST-PATH-RETURN
frames 1 onward:
    predecessor_cache_hit=1
    current_source_acquired=1
    local_output_allocated=1
    process_ok=1
    store_ok=1
    returned_lookup_attempted=1
    returned_lookup_success=1
    returned_lookup_ref_transferred=1
    predecessor_lookup_ref_released=1
    current_source_released=1
    local_output_released=1
    returned_fast_path_output=1
    output_authoritative=1
    mutates_old_strict=0
    proof_ok=1
```

Pre-cleanup H15.5 summary shape:

```text
total_cached_frame_count=20
addframeref_total=20
freeframe_total=0
balance=20
lookup_ref_acquired=39
lookup_ref_released=19
lookup_ref_transferred=20
lookup_ref_balance=0
store_attempts=20
store_successes=20
duplicate_skipped_already_cached=0
duplicate_computed_but_discarded=0
```

Final cleanup:

```text
total_cached_frame_count=0
invariants_ok=1
integrity_errors=0
validation_failures=0
ref_balance_errors=0
addframeref_total=20
freeframe_total=20
balance=0
lookup_ref_balance=0
clear_successes=1
clear_failures=0
```

Interpretation:

```text
H15.5 proves the important transition: sequential frames after frame 0 are now
computed, stored, looked up, transferred, and returned through the fast-path
output-cache authority path, with clean ownership accounting and without old
strict-state mutation.
```

---

## C5. Current diagnostic policy

Diagnostics remain proof instruments, not mere debug noise.

For H15.6/H15.7 work, preserve or improve visibility for:

```text
- edit version marker;
- arInitial source request decisions;
- fast-path eligibility and fallback reason;
- source-frame acquired/released counts;
- local-output acquired/released counts;
- predecessor lookup acquired/released/transferred counts;
- returned lookup ref transfer counts;
- output_authoritative path labels;
- old_strict bypass/quarantine/non-mutation evidence;
- cache add/free balance;
- lookup_ref_balance;
- final clear success/failure.
```

Do not send diagnostic output to stdout. CNR3 diagnostics must remain stderr/log
or equivalent diagnostic stream.

---

## C6. Immediate next task

Recommended next task:

```text
CMS02-H15.6 / arInitial source-plan reduction probe
```

Purpose:

```text
Prove the intended arInitial request decision for sequential fast-path candidates
without yet changing the actual VapourSynth source-frame request set.
```

Expected H15.6 behaviour:

```text
frame 0:
    bounded-warmup source request remains required

frame N > 0 with expected/available predecessor output N-1:
    log would_request_current_source_only=1
    log would_skip_bounded_warmup_source_plan=1
    continue using the already-proven H15.5 actual return path
```

H15.6 should be a probe/dry-run, not the active request-plan change. The active
source-request reduction should be H15.7 after H15.6 evidence is clean.

---

## C7. Next several recommended subphases

### CMS02-H15.6 — arInitial source-plan reduction probe

Do not yet change request behaviour. Log the intended request-plan decision.

### CMS02-H15.7 — arInitial source-plan reduction active proof

Actually request only source frame N for eligible sequential fast-path frames.
Expected 20-frame sequential source requests should reduce from a bounded-window
shape toward one source request per requested frame.

### CMS02-H15.8 — remove/demote bounded-warmup normal path from hot sequential path

Keep fallback for frame 0, holes, non-sequential requests, and failed
eligibility, but avoid constructing/running bounded-warmup normal machinery for
eligible sequential frames.

### CMS02-H15.9 — fast-path diagnostic consolidation

Rename remaining proof/migration labels where safe; preserve counters. Do not
hide ownership evidence.

### CMS02-H16.1 — out-of-order fallback validation

Validate request patterns such as:

```text
[1,0,3,2]
[0,2,1,3]
[5,0,1,2]
```

Expected rule:

```text
If output_cache[N-1] exists, sequential-style fast path may run.
If output_cache[N-1] does not exist, bounded warm-up/checkpoint fallback must run.
```

### CMS02-H16.2 — bounded fallback / checkpoint recovery integration

Refine fallback to use latest usable checkpoint within bounded interval or bounded
warm-up as required by CMS06.7.

### CMS02-H16.3 — old strict-state retirement preparation

Start removing or compiling out old strict authority only after selected paths no
longer read it for decisions and after validation proves no regression.

---

## C8. Do not implement in the next session unless explicitly chosen

Do not implement these during H15.6 unless explicitly chosen:

```text
- fmParallelRequests or fmParallel mode switch;
- old strict cache deletion;
- non-checkpoint pinning;
- checkpoint recovery redesign;
- major diagnostic throttling;
- RAII wrapper migration;
- broad comment/style cleanup;
- source-plan active reduction before H15.6 probe evidence is reviewed;
- any new pixel algorithm or duplicate frame-processing implementation.
```

---

## C9. Durable rules and forward hard gates still active

### CMS02-J0 - pre-fmParallelRequests cleanup and observability review

Still mandatory before CMS02-J / fmParallelRequests wiring. It must include, at
minimum:

```text
- old_strict_cache.next_needed and old_strict_cache.prev_output review;
- process_cnr3_frame compatibility wrapper review;
- selected output-authority path audit;
- debug-gate audit;
- ownership/ref-count audit;
- output-cache validation audit;
- diagnostics readability audit;
- pruning and hot-zone observability review.
```

### G-FMPAR-OLD-STRICT-AUTH-01 - old strict authority is not final fmParallel authority

Still active. H15.5 does not weaken this rule.

### G-DIAG-DUP-WASTE-SUMMARY-01 - human-readable duplicate/recompute waste summary

Still deferred. H15.5 reduced sequential duplicate recompute/store noise for
eligible frames, but the summary is still useful for fallback and future
parallel-request work.

### DEBUG-GATE-CODE-REVIEW-01 - debug-gated code review

Still active. Ensure proof/debug gates do not conceal production-required logic
before finalising authority paths.

---

## C10. Safety checks before any future commit

Before marking H15.6 or later PASS, confirm:

```text
- correct edit_version marker;
- output completes;
- no old strict next_needed rejection;
- mutates_old_strict=0 or equivalent;
- source-frame release balance is zero;
- local-output release balance is zero;
- lookup_ref_balance=0;
- cache add/free balance is zero after cleanup;
- invariants_ok=1;
- integrity_errors=0;
- validation_failures=0;
- ref_balance_errors=0;
- no stdout diagnostic regression;
- no unreviewed pixel algorithm fork.
```

---

## C11. Recent commit messages or suggested commit messages

Recent completed/pass phases should have been committed with messages similar to:

```text
Add H15.1 output-cache authority normal-path scaffold
Add H15.2 sequential predecessor-cache reuse probe
Add H15.3 sequential fast-path dry run
Add H15.4 sequential fast-path compute/store proof
Add H15.5 sequential fast-path return-transfer proof
```

For the latest H15.5 commit, recommended body:

```text
- Added a gated sequential fast-path return-transfer proof.
- Confirmed frame 0 falls back to the bounded-warmup normal path.
- Confirmed sequential frames 1 onward acquire cached output N-1 as predecessor.
- Confirmed sequential frames 1 onward acquire current source frame N, allocate local output N, compute output N, store it in output_cache, look it up, transfer the lookup ref, and return it.
- Confirmed OUTPUT-CACHE-AUTHORITY-SEQUENTIAL-FAST-PATH-RETURN is emitted for eligible sequential frames.
- Confirmed duplicate recompute/store noise drops to zero for the fast-path return-transfer proof.
- Confirmed old strict state is not mutated.
- Confirmed predecessor lookup refs, current source refs, local output refs, returned lookup refs, cache-owned refs, and final cache cleanup remain balanced.
```

---

## C12. New-chat starter prompt

```text
We are continuing the CNR3 VapourSynth plugin cache/output-authority migration.
Please read Document_A_CNR3_Project_Context_and_Rules_v1.8.md,
Document_B_CNR3_Decision_Log_v1.8.md, and
Document_C_CNR3_Current_Session_Handover_v1.8.md first. Then read
cnr3_cache_manager_design_v6_7.md. CMS06.8 proposed delta is available for
review but is not yet accepted design authority unless explicitly adopted.

Current state: CMS02-H15.5 has passed and should be committed. Sequential
frames after frame 0 can return through the output-cache authority sequential
fast path. Frame 0 still falls back to bounded-warmup normal path. The next
recommended task is CMS02-H15.6 / arInitial source-plan reduction probe.

Follow Rule 1 for comments and Rule 2 for code-update instructions. Do not
implement fmParallelRequests/fmParallel, old strict deletion, active source-plan
reduction, or broad cleanup unless explicitly asked.
```

---

## Appendix A - v1.8 update summary

v1.8 updates the current handover state from the H5/H6-era v1.7 baseline to the
post-H15.5 state. It preserves historical context by retaining the previous v1.7
handover below as historical material.

---

## Appendix B - Historical v1.7 Document C retained for continuity

The following material is retained as historical context only. Current status is
superseded by Sections C1-C12 above.

# Document C - CNR3 Current Session Handover

**Document:** C of CNR3 handover pack  
**Version:** v1.7  
**Date:** 2026-06-10  
**Status:** Volatile current-state/session handover document; update at every session boundary; continuity-preserved from v1.6 baseline.  
**Current design authority:** CMS06.6, or any later cache design specification that explicitly supersedes CMS06.6.  
**Current implementation authority:** This document, plus the user's latest local source files and latest logs.  
**Current matched pack:** A/B/C v1.7

---

## C1. Read order for a new chat

Read in this order:

1. `Document_A_CNR3_Project_Context_and_Rules_v1.7.md`
2. `Document_B_CNR3_Decision_Log_v1.7.md`
3. `Document_C_CNR3_Current_Session_Handover_v1.7.md`
4. `cnr3_cache_manager_design_v6_6.md`, or any later spec explicitly superseding CMS06.6
5. current relevant source files
6. latest logs if the task depends on test evidence

Rules for the new chat:

```text
Treat this current session handover as the source of truth for current status.
Treat the decision log as the source of truth for settled decisions.
Treat CMS06.6-or-later as the detailed cache-manager design authority.
If the design spec and current code appear to conflict, stop and ask for clarification.
Do not re-litigate settled decisions unless current code or logs prove a real problem.
Follow Rule 1 for code comments.
Follow Rule 2 for before/after code update instructions.
Do not implement anything listed in "Do not implement in the next session".
Do not depart from durable rules silently. Any intentional departure requires
explicit clarification, discussion, agreement, and documentation before proceeding.
```

CMS06.6 is now the current cache design authority. Earlier CMS06.5, CMS06.4,
CMS06.3, CMS06.2, CMS06.1, CMS06, and CMS05.x documents are superseded except
as history.

If a companion design spec contains a current implementation state snapshot,
this Document C overrides it for current implementation status. If the current
source conflicts with this document, inspect the source and stop for clarification
rather than guessing.

---

## C2. Repository/code context

Known repository:

```text
https://github.com/hydra3333/vapoursynth-cnr3
```

Current source files required for the next coding session:

```text
cnr3_build_config.h
vapoursynth-Cnr3.cpp
cnr3_common.h
cnr3_output_cache_manager.h
cnr3_output_cache_manager.cpp
cnr3_frame_internal_processing.h
cnr3_frame_internal_processing.cpp
```

The user's local tree after CMS02-H5 should contain:

```text
CNR3_EDIT_VERSION = CMS02-H5-bounded-warmup-local-compute-proof-v1-PASSED
CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF = false
CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF = false
```

The exact current source files should still be uploaded before the next chat
proposes code patches. This handover records expected state; exact before/after
patches must be grounded in the uploaded current source.

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

---

## C3. Current exact implementation status

Current phase/SubPhase:

```text
CMS02-H / SubPhase H6 / bounded-warmup-store-proof
    Next implementation phase.
```

Last completed phase/SubPhase:

```text
CMS02-H / SubPhase H5 / bounded-warmup-local-compute-proof
    Complete / PASS.
```

Current design authority:

```text
CMS06.6 or any later cache design specification that explicitly supersedes CMS06.6.
```

Current output authority:

```text
Partial transition only.

Implemented:
    arAllFramesReady cache-hit return path exists. If output_cache already
    contains frame N, the code can acquire a caller-owned lookup ref, mark it
    transferred, log CACHE-HIT-RETURN, and return the cached frame to VapourSynth.

Still not complete:
    Full recovery/output-cache authority is not complete. Cache misses still
    fall through to the strict-streaming/new-computation path. Bounded warm-up
    outputs are not yet stored or returned. Recovered outputs are not yet
    generally returned from the output cache.
```

Current output-cache role:

```text
- stores/prunes real produced frames;
- supports cache-hit lookup/addFrameRef/transfer return;
- supports checkpoint, hot-zone, prune, store, validation, and pin/unpin scaffolding;
- supports debug/proof recovery and bounded warm-up scaffolding;
- is not yet the general output-authoritative recovery path for all frames.
```

CMS02-H status:

```text
CMS02-H / SubPhase H4 / bounded-warmup-source-frame-set-request-acquire-release:
    Complete / PASS.

CMS02-H / SubPhase H5 / bounded-warmup-local-compute-proof:
    Complete / PASS.

CMS02-H / SubPhase H6 / bounded-warmup-store-proof:
    Next.

CMS02-H / SubPhase H7 / bounded-warmup-return-decision-dry-run:
    Pending.

CMS02-H / SubPhase H8+ / bounded-warmup return-transfer / output-authority readiness:
    Deferred until after H7 review.
```

CMS02-J0 status:

```text
CMS02-J0 / pre-fmParallelRequests cleanup and observability review:
    Mandatory future checkpoint before CMS02-J.

CMS02-J must not start until CMS02-J0 has been evaluated and performed, or until
specific intentional deferrals have been explicitly documented with reasons,
scope, and expected safety impact.
```

Important exact-match rule:

```text
exact_match is diagnostic only. It must not be used as a bounded-recovery return
condition unless a later explicit quality/tolerance decision changes that policy.
```

Important fmParallel warning:

```text
old_strict_cache.next_needed and old_strict_cache.prev_output are not final
fmParallel authority. They must be retired, bypassed, or redesigned before final
fmParallel operation.
```

---

## C4. Latest test evidence

### CMS02-H / SubPhase H4 enabled proof evidence

```text
frames_checked=20
plans_created=20
plans_destroyed=20
source_frames_requested_total=57
source_frames_retrieved_total=57
source_frames_released_total=57
source_frame_release_balance=0
source_frame_count_max=3
partial_acquire_failures=0
source_frame_release_balance_errors=0
proof_failures=0
```

Negative-authority evidence:

```text
would_compute_warmup_outputs=0
would_store_warmup_outputs=0
would_return_warmup_output=0
output_authoritative=0
mutates_old_strict=0
```

Disabled committed/smoke state:

```text
H4 proof gate disabled.
No H4 proof trace lines in disabled smoke.
Final edit marker:
    CMS02-H4-bounded-warmup-source-frame-set-request-acquire-release-v1-PASSED
```

Disabled-smoke cleanup evidence:

```text
non_checkpoint_count=0
checkpoint_count=0
total_cached_frame_count=0
has_pinned_checkpoints=0
total_pin_count=0
invariants_ok=1
integrity_errors=0
validation_failures=0
ref_balance_errors=0
addframeref_total=5
freeframe_total=5
balance=0
lookup_ref_balance=0
clear_successes=1
```

### CMS02-H / SubPhase H5 enabled proof evidence

```text
frames_checked=20
plans_seen=20
source_frames_retrieved_total=57
source_frames_released_total=57
source_frame_release_balance=0
source_frame_release_balance_errors=0
start_frame_zero_count=3
start_frame_nonzero_count=17
local_start_reset_copies=20
local_recursive_computes=37
local_outputs_allocated=57
local_outputs_released=57
local_output_release_balance=0
local_output_release_balance_errors=0
partial_acquire_failures=0
compute_failures=0
proof_failures=0
would_store_warmup_outputs=0
would_return_warmup_output=0
output_authoritative=0
mutates_old_strict=0
```

H5.1 / zero-start local rolling compute proof:

```text
PASS.
Covered requested frames 0, 1, and 2, proving local compute from 0..N using the
existing explicit-predecessor processing boundary.
```

H5.2 / bounded-start policy review/proof:

```text
PASS.
Covered S > 0 cases from requested frame 3 onward. For S > 0, local output[S]
uses existing frame-0/reset semantics as a bounded approximation start, then
S+1..N is computed recursively using explicit predecessor handoff.
```

H5-FIX1 trace fields were present and proved the distinction between timeline
source frame and reset semantics:

```text
actual_source_frame
warmup_start_frame
processing_frame_number
predecessor_frame_number
bounded_start_uses_frame0_reset_path
recursive_compute
uses_existing_explicit_previous_output_helper=1
```

Final edit marker after disabled smoke:

```text
CMS02-H5-bounded-warmup-local-compute-proof-v1-PASSED
```

Disabled-state smoke evidence:

```text
H5 proof gate disabled.
No FOR-DEBUG-ONLY-BOUNDED-WARMUP-LOCAL-COMPUTE lines.
No FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-PLAN-CREATED lines.
No FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-FRAME-ACQUIRED lines.
Output 5 frames in 0.01 seconds.
```

Final cache/ref cleanup after disabled smoke:

```text
non_checkpoint_count=0
checkpoint_count=0
total_cached_frame_count=0
has_pinned_checkpoints=0
total_pin_count=0
invariants_ok=1
integrity_errors=0
validation_failures=0
ref_balance_errors=0
addframeref_total=5
freeframe_total=5
balance=0
lookup_ref_balance=0
clear_successes=1
clear_failures=0
```

Hard-gate result:

```text
CMS02-H4: PASS
CMS02-H5: PASS
H5 disabled-state smoke: PASS
Ready for CMS02-H6: YES
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

Proof diagnostics:
    controlled by compile-time flags in cnr3_build_config.h.
    dedicated proof runs temporarily enable exactly the needed proof gates.
    normal committed state disables proof gates again.
```

CMS06.6 / CMS02-J0 diagnostic direction:

```text
Before CMS02-J0:
    New diagnostics should prefer compile-time gating where practical.

At CMS02-J0:
    Consolidation into the final compile-time / if constexpr diagnostic model
    becomes a deliverable/requirement.

Safety/health counters:
    Prefer keeping important counters non-gated unless there is a clear
    complexity, performance, or maintainability reason to gate them.

High-volume diagnostics:
    Gate verbose trace printing, temporary diagnostic maps, per-frame/per-source
    trace floods, memory snapshots, and proof-only heavy summaries when CMS02-J0
    performs diagnostic cleanup/consolidation.
```

---

## C6. Immediate next task

Recommended immediate next task:

```text
CMS02-H / SubPhase H6 / bounded-warmup-store-proof
```

Goal:

```text
Take the local outputs computed by the proven H5 bounded warm-up path and prove
store behaviour into output_cache under a dedicated proof gate.
```

Scope:

```text
Allowed:
    - reuse H4/H5 source-frame and local-compute scaffolding;
    - store locally computed warm-up outputs into output_cache under proof gate;
    - prove fill-holes/first-in-best-dressed/duplicate-discard behaviour;
    - prove addFrameRef/freeFrame balance for cache-owned stored frames;
    - prove cleanup and disabled-state smoke.

Not allowed:
    - returning bounded warm-up outputs;
    - changing general output authority;
    - making bounded warm-up production-authoritative;
    - enabling fmParallelRequests or fmParallel;
    - mutating old_strict_cache.prev_output or old_strict_cache.next_needed;
    - changing recursive pixel/blend/scene-change logic.
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
1. Inspect current post-H5 source before proposing patches.
2. Add or enable a dedicated H6 proof gate.
3. Reuse H4/H5 bounded warm-up source-plan and local-compute path.
4. Store local warm-up outputs into output_cache.
5. Prove first-in-best-dressed duplicate handling.
6. Prove cache-owned addFrameRef/freeFrame balance.
7. Do not return warm-up outputs.
8. Do not change output authority.
9. Do not mutate old strict-streaming state.
10. Restore H6 proof gate to disabled before committed/smoke state.
```

Design-compliance requirement:

```text
Perform a design-compliance review after H6. Verify that H6 remains store-only
and does not silently become a return-decision, return-transfer, or output-
authority phase.
```

---

## C7. Do not implement in the next session unless explicitly chosen

Do not implement in CMS02-H / SubPhase H6 unless the user explicitly changes the task:

```text
- return-decision dry-run for bounded warm-up outputs;
- return-transfer proof for bounded warm-up outputs;
- final general output-cache authority;
- permanent production bounded-warmup recovery policy;
- fmParallelRequests wiring;
- full fmParallel support;
- non-checkpoint pinning;
- changes to recursive blend maths;
- changes to scene-change detection;
- manual pixel/copy/blend/luma/chroma logic for bounded warm-up;
- broad diagnostic mode redesign;
- mass diagnostic string renames;
- broad cleanup of old strict-streaming code;
- retiring old_strict_cache.next_needed / prev_output without a specific design step.
```

---

## C8. Durable rules and forward hard gates still active

The following durable rules remain active and must not be departed from silently.
Any intentional departure requires explicit clarification, discussion,
agreement, and documentation of the reason, scope, and expected safety impact
before implementation proceeds.

```text
- Reuse existing frame-processing boundaries; do not create parallel pixel/frame algorithms.
- Every retrieved source frame must be released on every path.
- Every temporary local output frame must be released on every path unless ownership transfer is explicitly proven.
- Compute, store, return decision, return transfer, and output-authority transition remain separately provable.
- Bounded-start S > 0 behaviour must be described honestly as reset/start approximation unless exact predecessor history is proven.
- CMS02-J0 is mandatory before CMS02-J.
- old_strict_cache.next_needed / prev_output are not final fmParallel authority.
```

---

## C9. Do not lose / named deferred items

### CMS02-J0 - pre-fmParallelRequests cleanup and observability review

Mandatory future checkpoint before CMS02-J. See C3 and C8.

### G-PAR-HZ-ARINITIAL-01 - hot-zone update must be at `arInitial`

Hot-zone updates should be treated as prerequisite work before
`fmParallelRequests` or `fmParallel` development. Under future concurrent
request modes, deferring hot-zone update to `arAllFramesReady` would be unsafe
because pruning must know about active request intent as early as possible.

Current status:

```text
Resolved in CMS06.2 and preserved through CMS06.6.
The current committed code updates hot zones at arInitial before requestFrameFilter(). Preserve this.
Do not move hot-zone updates back to arAllFramesReady.
```

### G-DIAG-RECALC-HIST-01 - compile-time recalculation histogram

Add a compile-time-only diagnostic table later showing how many frames were
calculated once, twice, three times, etc. The table should be per-instance only
and should count actual frame calculations, not cache returns. It should include
the normal once-only row unless later log-volume policy says otherwise.

### G-DIAG-DUP-WASTE-SUMMARY-01 - human-readable duplicate/recompute waste summary

Add a human-readable summary block using existing raw counters when duplicate
recompute waste is relevant to a proof or design decision.

### G-DIAG-LOG-VOLUME-01 - long-run diagnostic throttling / compact-expanded debug options

As recovery testing moves to longer runs, add compile-time diagnostic verbosity
controls so routine long-run logs can run in compact mode while preserving
expanded/full mode for proof work.

CMS06.6 refines this: prefer compile-time gating for new diagnostics where
practical before CMS02-J0; make diagnostic consolidation a CMS02-J0 deliverable.

### DEBUG-GATE-CODE-REVIEW-01 - check debug-gated code for production-required logic

At CMS02-J0 or a focused cleanup/review point, check whether any code currently
inside debug/proof compile-time gates should be outside those gates for normal
runtime correctness, required diagnostics, or cleanup.

### G-FMPAR-OLD-STRICT-AUTH-01 - old strict authority is not final fmParallel authority

`old_strict_cache.next_needed` and `old_strict_cache.prev_output` are not final
fmParallel authority. Before final fmParallel operation, they must be retired,
bypassed, or redesigned so final correctness does not depend on strict-streaming
sequencing state.

### DEAD-SUPERSEDED-CODE-01 - later cleanup of compatibility wrappers and old strict path

At a later dead/superseded-code cleanup point, likely near the end of
development, check whether the `process_cnr3_frame(...)` compatibility wrapper
can be removed. It may become removable only after all normal and recovery paths
have moved to explicit predecessor handling and the old strict-streaming
authority path is retired.

---

## C10. Remaining cleanup/deferred notes

Deferred cleanup:

```text
- Some comments and diagnostics may still contain older CMS05/CMS06 wording.
  Do not mix broad wording cleanup into H6 unless it is required to avoid a
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

- Check whether any code inside debug/proof gates should later move outside
  those gates only as a focused review item. Do not mix that cleanup into H6.

- Investigate compact/expanded debug options as the focused implementation of
  G-DIAG-LOG-VOLUME-01, not as incidental cleanup during unrelated phases.
```

---

## C11. Safety checks before any future commit

Build:

```text
Debug build must succeed.
Release build should succeed before larger phase commits.
```

Run:

```text
short realclip or blankclip smoke test where relevant.
any targeted test required by the current phase.
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

For phases involving lookup-owned references, also require:

```text
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

For phases involving source-frame or local-output proof ownership, also require:

```text
source_frame_release_balance=0
source_frame_release_balance_errors=0
local_output_release_balance=0
local_output_release_balance_errors=0
```

Hard gate:

```text
If any expected safety diagnostic shows unexpected values, stop.
Do not proceed to the next task until the discrepancy is understood.
```

---

## C12. Recent commit messages or suggested commit messages

Suggested commit message for completed H5:

```text
Complete CMS02-H5 bounded warm-up local compute proof

- Added a proof-only bounded warm-up local compute path.
- Reused the H4-proven arInitial source request and arAllFramesReady acquire/release lifecycle.
- Reused process_cnr3_frame_with_explicit_previous_output() for local recursive compute.
- Avoided duplicating frame, pixel, blend, luma, chroma, or scene-change logic.
- Proved zero-start local rolling compute for frames 0..N.
- Proved bounded-start S > 0 handling using explicit frame-0 reset semantics for the local start frame.
- Added trace fields that distinguish actual source frame, warm-up start frame, processing frame number, and predecessor frame number.
- Confirmed H5 does not store warm-up outputs, return warm-up outputs, change output authority, or mutate old strict-streaming state.
- Enabled proof passed with 57 source frames retrieved/released, 57 local outputs allocated/released, and zero release-balance errors.
- Restored H5 proof gate to disabled committed state.
- Disabled-state smoke passed with clean cache/ref diagnostics:
  invariants_ok=1, integrity_errors=0, validation_failures=0,
  ref_balance_errors=0, lookup_ref_balance=0, and cache ref balance returned to zero after clear.
```

---

## C13. New-chat starter prompt

```text
We are continuing CNR3 development.

Please read the uploaded documents in this order:

1. Document_A_CNR3_Project_Context_and_Rules_v1.7.md
2. Document_B_CNR3_Decision_Log_v1.7.md
3. Document_C_CNR3_Current_Session_Handover_v1.7.md
4. cnr3_cache_manager_design_v6_6.md, or any later CMS06.6+ spec explicitly superseding it
5. Current source files/logs

Important:
- The new chat has no memory of prior chats.
- Treat Document_C_CNR3_Current_Session_Handover_v1.7.md as the source of truth for current state.
- Treat Document_B_CNR3_Decision_Log_v1.7.md as the source of truth for settled decisions.
- Treat CMS06.6-or-later as the detailed design reference.
- Do not re-litigate settled decisions unless current code or logs prove a real problem.
- Follow Rule 1 for code comments.
- Follow Rule 2 for before/after code update instructions.
- Do not implement anything listed in the current handover's "Do not implement" section.
- Do not depart from durable rules silently. Any intentional departure requires explicit clarification, discussion, agreement, and documentation before proceeding.

Current next task:
    CMS02-H / SubPhase H6 / bounded-warmup-store-proof.

First, confirm your understanding of the current state and immediate next task.
Then wait for the current code files if they have not already been uploaded.
```

---

## Appendix C - v1.7 update summary

This v1.7 handover pack was produced from the previous approved v1.6 A/B/C
baseline and the v1.5 handover production spec. It incorporates current CMS06.6
design direction and current H4/H5 proof results.

Major v1.7 updates:

```text
- CMS06.6 becomes the current design authority.
- CMS02-H4 PASS evidence preserved.
- CMS02-H5 PASS evidence preserved.
- CMS02-H6 is the immediate next task.
- Durable development rules added/cross-referenced.
- CMS02-J0 mandatory checkpoint added/cross-referenced.
- H5 bounded-start reset semantics carried forward.
- No-parallel-pixel-algorithm rule carried forward.
- Compile-time diagnostics direction carried forward as a CMS02-J0 deliverable.
- Old strict-state final-goal review carried forward.
```

The previous v1.6 appendices are retained below as historical context. They are
not the current-state authority where they conflict with the v1.7 main sections.

## Appendix D - Summary from CMS02-G / SubPhase G10ABC through G10D.8

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

## Appendix E - Summary up to and including CMS02-G.9AB, prior to CMS02-G.10

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
