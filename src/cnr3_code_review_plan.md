# CNR3 Cache Manager — Code Review and Simulation Plan

**Date:** 2026-06-01
**Applies to:** Implementation based on CMS05 specification
**Companion document:** cnr3_cache_manager_design_v5.md (CMS05)

---

## 1. Purpose

This document describes the planned code review and simulation programme
to be carried out after implementation of the CNR3 output cache manager
(Phases CMS02-A through CMS02-H, or at a substantial intermediate
milestone).

The goal is to verify that the implementation is safe, correct, and
compliant with CMS05 before proceeding to fmParallelRequests wiring
(Phase CMS02-J) or the non-checkpoint pinning decision (Phase CMS02-I).

---

## 2. Scope and Limitations

### 2.1 What this review covers

- **Static analysis:** Reading and reasoning about the uploaded source
  code against the CMS05 spec. Tracing code paths manually through the
  implementation.
- **Mutex reasoning:** Static reasoning about whether mutexes are in
  the right places to prevent collisions and deadlocks. Cannot prove
  thread-safety at runtime, but can identify structural issues such as
  wrong lock order, missing locks on shared state, double-lock risks,
  and `_externally_locked` helpers called without the lock held.
- **Simulation:** Python Monte Carlo models of cache manager behaviour
  under realistic request patterns, verifying policy correctness,
  ref-count balance, and hot-zone behaviour.

### 2.2 What this review does not cover

- Code formatting, commenting style, variable naming.
- Compilation or execution of the actual C++ code.
- Runtime thread-safety proof (requires ThreadSanitizer or equivalent
  at runtime).
- Real memory consumption (requires actual VS frame sizes at runtime).
- VapourSynth API interaction correctness (requires VS runtime).

### 2.3 Focus

The primary focus is the normal near-linear encoding use case:
sequential frames with BestSource-style jitter, 6 threads, no large
jumps. Large-jump scenarios are a separate future exercise not covered
by this plan.

---

## 3. Static Analysis Review Items

### 3.1 Spec compliance

For each section of CMS05 that has a corresponding implementation, verify:

- The implementation matches the specified behaviour, or
- Any variation is documented and demonstrably achieves the same outcome
  safely and reliably.

Key sections to check: 2.2 (RC rules), 4.2 (hot zone lifecycle), 4.3
(pruning policy), 4.5.1 (rolling predecessor pattern), 4.5.2
(final-frame ownership transfer), 4.6 (ceiling and abort), 4.9
(first-in-best-dressed store idempotency), 9.5 (shutdown protocol).

### 3.2 RC rule compliance (per function)

Go through RC1–RC8 systematically and verify each is implemented:

- **RC1** — All cache-owned `addFrameRef` calls occur only inside the
  single store helper. No other path takes a cache-owned reference.
- **RC2** — All cache-owned `freeFrame` calls occur only inside the
  single remove helper. No raw `pool.erase()` or `cache_index.erase()`
  anywhere outside the remove helper.
- **RC3** — Store error paths that took `addFrameRef` before failure
  execute a balancing `freeFrame` before returning.
- **RC4** — Lookup helper error paths that took `addFrameRef` before
  failure execute a balancing `freeFrame` before returning nullptr.
- **RC5** — Every caller of the lookup helper frees or transfers the
  returned reference on every exit path: success, error, early return.
- **RC6** — `clear()` / destructor iterates all slots, `freeFrame`s
  each, warns on pinned checkpoints, clears indexes.
- **RC7** — `validate_invariants` includes the cache-side ref-balance
  check: `addframeref_total - freeframe_total == live slot count`.
- **RC8** — `store_frame` checks `cache_index` for frame number before
  taking `addFrameRef`. On duplicate, returns success without modifying
  state or taking a reference.

### 3.3 Error path completeness

For every function that can fail, trace all failure paths and verify:

- Any pinned checkpoint is unpinned exactly once.
- Any caller-owned VSFrame reference is freed exactly once.
- Any source frame obtained from VS is released.
- Any destination frame allocated but not returned is released.
- Hot zone state is not rolled back (correct — zones are left to retire
  naturally).
- Diagnostic counters are still incremented on failure paths.

### 3.4 Mutex correctness

Verify by static reasoning:

- Every access to shared cache state (`non_checkpoint_pool`,
  `checkpoint_pool`, `cache_index`, hot zone array, all stats counters)
  is performed while holding `cache_mutex`.
- Every `_externally_locked` helper is called only at call sites that
  already hold `cache_mutex`. No `_externally_locked` helper is called
  from a public (auto-locking) helper, which would deadlock.
- No public helper that locks internally calls another public helper
  that locks internally (double-lock / deadlock risk).
- Lock is always released on all exit paths (including error paths) of
  every function that acquires it.
- No code holds the mutex while calling back into VS API functions that
  might re-enter the cache manager (re-entrancy deadlock).

### 3.5 Hot zone lifecycle correctness

- Sliding rule: verify that `low` and `high` are recomputed from F
  using the radii constants (`low = max(0, F - BACK_RADIUS)`,
  `high = F + FORWARD_RADIUS`) — not incrementally updated, which could
  drift or accumulate errors.
- Jump detection: verify the threshold comparison is correct and
  consistent with the constant definition.
- fmUnordered retirement: verify eager retirement fires at `arInitial`
  and that stale zones are correctly identified and retired.
- fmParallelRequests retirement: verify the lazy retirement checks both
  conditions (no live frames in range AND no pinned checkpoint in range)
  before retiring a zone.
- Merge: verify the merge selects the two zones with the smallest
  boundary gap and produces a zone that covers both original ranges.

### 3.6 First-in-best-dressed store correctness

- Verify the `cache_index` lookup happens under the mutex before
  `addFrameRef` is taken (atomic check-and-store).
- Verify that on a duplicate, the existing slot is not mutated in any
  way.
- Verify that on a duplicate, the caller's supplied frame pointer is not
  touched by the store helper (the cache has not taken ownership).
- Verify the correct counters are incremented on the duplicate path.

### 3.7 Final-frame ownership transfer

- Verify that at the end of recovery walks, `release()` is used (not
  `freeFrame`) when the walk's `prev_ref` is the requested output frame
  being returned to VapourSynth.
- Verify there is no code path that calls `freeFrame` on a frame after
  its ownership has already been transferred to VS.
- Verify that intermediate predecessor frames (not the requested output)
  are correctly `freeFrame`d and not leaked.

### 3.8 Rolling predecessor reference pattern

- Verify that the walk always acquires the new predecessor reference
  *before* releasing the old one — no window of zero references to the
  current predecessor.
- Verify the pattern across all loop iterations including the first
  (source-copy initialisation) and last (final-frame transfer).
- Verify that on any error exit from within the loop, `prev_ref` and
  any in-progress `new_frame` are both freed.

### 3.9 Pruning correctness

- Verify the prune candidate selection uses hot-zone distance as the
  eviction key, not insertion order or frame-number magnitude.
- Verify all evictions go through the single remove helper (RC2).
- Verify the Phase A rule is in effect (no `pin_count` check on
  non-checkpoint frames until Phase CMS02-I).
- Verify checkpoint pruning applies the same hot-zone candidate
  filtering as non-checkpoint pruning.
- Verify frame zero is never pruned.

### 3.10 Shutdown completeness

- Trace `clear()` and verify: acquires mutex, iterates both pools,
  `freeFrame`s every frame, warns on stuck pins, erases all slots,
  clears `cache_index`, resets hot zones to inactive, calls
  `validate_invariants`.
- Verify `validate_invariants` after clear confirms ref-balance ==
  zero (no live slots).

### 3.11 Instrumentation coverage

- Every counter declared in CMS05 Section 6 is actually incremented at
  the right point in the code.
- No counter is incremented more than once per event.
- Counters that should be behind `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`
  are correctly gated.
- Counters that should always be collected (headline counters) are not
  behind a gate.
- No diagnostic output goes to stdout (all to stderr or debug log).
- `predecessor_missing_when_expected > 0` produces a prominent warning
  regardless of gate setting.
- Ref-balance failure at shutdown produces a prominent warning
  regardless of gate setting.

### 3.12 validate_invariants coverage

- Verify the invariant checks are actually checking what they claim.
  In particular, the ref-balance check should use live counter values,
  not a stale snapshot.
- Verify that `validate_invariants` is called after every mutation in
  development mode (or at minimum after store, remove, prune, and
  shutdown).
- Verify that `validate_invariants` failing causes a diagnostic output
  and returns a clear error indication (does not silently succeed).

### 3.13 Data structure usage

- `std::map` used for both pools — verify iteration order (ascending
  by frame number) is used correctly where needed and not assumed
  incorrectly elsewhere.
- `std::unordered_map` used for `cache_index` — verify no iteration
  order is assumed.
- Hot zone array (fixed size) — verify bounds are checked before
  accessing slots, no out-of-bounds indexing.
- All integer arithmetic involving frame numbers — verify no overflow
  risk for realistic frame counts (VHS at 25fps, hours of content).

### 3.14 fmUnordered / fmParallelRequests transition safety

- Verify that the only mode-dependent code is in the retirement helper,
  and that the `Cnr3CacheSchedulingMode` enum is threaded through
  correctly.
- Verify that the update-hot-zones helper has no mode-specific logic
  (both modes use identical sliding).
- Verify that the transition from fmUnordered to fmParallelRequests
  wiring (Phase CMS02-J) will not require changes to any of the core
  cache logic — only the retirement mode parameter changes.

### 3.15 Additional items

- **`Cnr3OwnedFrameRef` RAII wrapper:** verify destructor calls
  `freeFrame` correctly, `release()` correctly suppresses the
  destructor `freeFrame`, move semantics correctly transfer ownership,
  copy is correctly deleted.
- **Ceiling calculation:** verify the byte-budget formula uses the
  ceil-style subsampled dimension function for chroma planes, and that
  the result is clamped to `[MIN_HARD_CEILING, MAX_HARD_CEILING]`.
- **Counter naming:** verify that the verbose counter name
  `duplicate_store_computed_but_discarded` is present and not silently
  renamed to something less informative.

---

## 4. Monte Carlo Simulations

### 4.1 Approach

Python Monte Carlo simulations modelling the cache manager's policy
behaviour. Not running the C++ code — modelling the cache manager's
decisions (what gets stored, pruned, protected, and evicted) under
realistic request patterns.

The models verify that the policy produces the correct outcomes; the
static analysis verifies that the implementation matches the policy.
Together they give strong coverage.

### 4.2 Scope: Standard Use Case (This Exercise)

**Scenario:** Near-linear encoding. Sequential frames with BestSource-
style jitter. No large jumps.

**Parameters:**

- Frames: 200
- Threads: 6 (matching observed BestSource jitter of 0–6 frames)
- Jitter model: each frame dispatched at time =
  `frame_number + uniform_random(0, 6)`. Frames arrive at the cache
  in delivery-time order.
- Monte Carlo runs: 50 per sub-scenario (gives stable p95/p99
  statistics)

**Sub-scenarios:**

**(a) fmUnordered** — 6-thread jitter arrival model. Verify:
  - One active hot zone, sliding correctly with arrivals.
  - Zone low boundary advances as encode progresses (frames behind zone
    become prunable).
  - Pool size stays within `[CAPACITY, CAPACITY × OVERFLOW_FACTOR]`
    under steady operation.
  - Prune evicts only frames outside the hot zone.
  - No predecessor misses (`predecessor_missing_when_expected == 0`
    across all 50 runs).
  - Cache-side ref balance holds after every operation.
  - Caller-side lookup-ref balance holds at end of each simulated
    request.

**(b) fmParallelRequests** — Same jitter model but with genuine
  concurrent in-flight requests. 6 threads simultaneously in-flight,
  each with an assigned frame, interleaved in random order. Verify:
  - Hot zone correctly covers the spread of concurrent requests.
  - Lazy retirement does not retire a zone while requests are still
    in-flight within its range.
  - First-in-best-dressed store correctly handles the occasional
    duplicate-store race (two threads both miss the same frame).
  - `duplicate_store_computed_but_discarded` increments appropriately
    (occasionally, not frequently).
  - No predecessor misses across all 50 runs.
  - Ref balance holds throughout, including duplicate-store races.
  - Pool size stays within ceiling.

**(c) fmParallel (informational only)** — Same model with relaxed
  mutex ordering (simulating lock contention and out-of-order
  completion). Not part of the implementation target. Run to identify
  which invariants would be violated and what additional mechanisms
  fmParallel would require. Results inform future planning, not current
  implementation.

### 4.3 What the simulations measure and report

For each sub-scenario, across all 50 runs:

**Pool behaviour:**
- `max_non_checkpoint_pool_size_observed` (p50, p95, p99, max)
- `max_checkpoint_pool_size_observed` (p50, p95, p99, max)
- `prune_no_candidate_exists` rate (should be zero in normal operation)
- `cache_ceiling_hard_aborts` (should be zero in normal operation)

**Hot zone behaviour:**
- `hot_zone_allocations` per run (expect 1 for linear encoding)
- `hot_zone_slides` per run (expect ~200, one per frame)
- `hot_zone_merges` per run (expect 0 in normal linear operation)
- `hot_zone_retirements` per run (expect 0 for fmUnordered linear,
  occasional for fmParallelRequests)
- Distribution of zone width over time (should stay at BACK + FORWARD
  = 60 frames under steady sliding)

**Pruning behaviour:**
- `non_checkpoint_prune_skipped_in_hot_zone` per prune event
- Eviction distance from zone boundary (should always be > 0)
- Frames erroneously evicted from within the hot zone (must be 0)

**Reference-count balance:**
- `cache_addframeref_total - cache_freeframe_total` at end of each
  run (must equal live slot count, must equal 0 after simulated
  shutdown)
- `lookup_owned_ref_acquired - (released + transferred)` at end of
  each run (must equal 0)

**Correctness:**
- `predecessor_missing_when_expected` per run (must be 0 across all
  50 runs for normal use case)
- `duplicate_store_computed_but_discarded` per run (expect 0 for
  fmUnordered; occasional for fmParallelRequests)
- Recovery walk completeness (every requested frame successfully
  produced)

### 4.4 Large-jump simulation (separate future exercise)

Not part of this review. Planned separately to cover:
- Jump detection and new zone allocation.
- Old-zone lazy retirement under fmParallelRequests.
- Bounded warm-up recovery correctness.
- Duplicate-store race frequency under concurrent recovery walks.
- Ceiling approach under rapid consecutive jumps.

---

## 5. What to Upload for the Review

When ready for review, upload:

1. All `.cpp` and `.h` source files for the cache manager:
   - `cnr3_cache_manager.h`
   - `cnr3_cache_manager.cpp`
   - Any new files added during implementation

2. The main VapourSynth filter file:
   - `vapoursynth-Cnr3.cpp`

3. Supporting headers:
   - `cnr3_common.h`
   - `cnr3_build_config.h`
   - Any new headers

4. A brief implementation note covering:
   - Which CMS02 phases have been completed.
   - Any deliberate variations from the CMS05 spec and the rationale
     for each.
   - Any known issues or areas of uncertainty to focus on.
   - Which `Cnr3CacheSchedulingMode` is currently wired (expected:
     `FmUnordered` for initial phases).

The CMS05 spec document does not need to be re-uploaded — it is already
in context from prior sessions, or can be re-uploaded if the review is
in a new conversation.

---

## 6. Session Structure

Given the volume of material, the review is best split into two
sessions:

**Session 1 — Analysis:**

- Upload source files.
- Run simulations (Python, in-session).
- Perform static analysis and path tracing against the checklist in
  Section 3.
- Produce a structured findings document covering: spec compliance
  issues, RC rule violations, error path gaps, mutex concerns, and
  simulation results.

**Session 2 — Decision and remediation:**

- Start with the Session 1 findings document as input.
- Reason about priority and remediation for each finding.
- Produce a prioritised fix list and (where appropriate) specific
  guidance on how to address each finding.
- Decide whether the implementation is ready to proceed to Phase
  CMS02-I (empirical pinning decision) or whether findings require
  remediation first.

This split keeps each session's context clean and focused.

---

## 7. Model Recommendation

**Opus 4.7** for both sessions. The reasoning is the same as for the
spec work:

- Long-context reasoning across multiple large source files plus the
  spec simultaneously.
- The static analysis requires holding the entire spec and the code in
  working context at the same time.
- The simulation results need to be interpreted alongside both the
  spec and the code.

Sonnet is appropriate for shorter tasks within the development process
(quick questions, simple edits, formatting). Opus is appropriate for
this review.

---

## 8. Guiding Principle

From the CMS05 design process:

> The highest risk is not that a newly written helper is obviously wrong.
> The highest risk is that an older helper remains in the call path with
> assumptions from the previous design — mutex placement, ownership
> semantics, pruning behaviour, reference-count handling. The new code
> calls the old helper expecting CMS05 semantics, but the old helper
> silently provides v005 semantics.

The static analysis addresses this directly: the review checks complete
execution paths, not isolated edited functions.

