# CNR3 Cache Manager — Code Review and Simulation Plan

**Date:** 2026-06-02
**Version:** CMS05.1
**Status:** Planned — to be executed after implementation of Phases CMS02-A
through CMS02-H (or at a substantial intermediate milestone)
**Companion document:** cnr3_cache_manager_design_v5_1.md (CMS05.1 spec)

---

## Changelog

### CMS05.1 — 2026-06-02 (initial version, matching spec CMS05.1)

- Document created. Covers static analysis, Monte Carlo simulations,
  and design-compliance review plan.
- Monte Carlo short runs set to 50 per scenario (down from original
  suggestion of 500; sufficient for p50 and rough p95 in a normal
  near-linear use case where tail behaviour should be benign).
- One 10,000-frame long run per scenario added to observe hot-zone
  sliding dynamics over time.
- CSV output specified with Excel visualisation guidance.
- `CNR3_CACHE_BYTE_BUDGET` updated to 1 GiB throughout, matching
  spec CMS05.1.
- Two-session structure and model recommendation added.

---

## 1. Purpose

This document describes the planned code review and simulation programme
to be carried out after implementation of the CNR3 output cache manager
(Phases CMS02-A through CMS02-H, or at a substantial intermediate
milestone).

The goal is to verify that the implementation is safe, correct, and
compliant with CMS05.1 before proceeding to fmParallelRequests wiring
(Phase CMS02-J) or the non-checkpoint pinning decision (Phase CMS02-I).

---

## 2. Scope and Limitations

### 2.1 What this review covers

- **Static analysis:** Reading and reasoning about the uploaded source
  code against the CMS05.1 spec. Tracing code paths manually through
  the implementation.
- **Mutex reasoning:** Static reasoning about whether mutexes are in
  the right places to prevent collisions and deadlocks. Cannot prove
  thread-safety at runtime, but can identify structural issues: wrong
  lock order, missing locks on shared state, double-lock risks, and
  `_externally_locked` helpers called without the lock held.
- **Simulation:** Python Monte Carlo models of cache manager behaviour
  under realistic request patterns, verifying policy correctness,
  ref-count balance, and hot-zone behaviour. These model the cache
  manager's *decisions* (what gets stored, pruned, protected, evicted)
  — they do not compile or run the C++ code.

### 2.2 What this review does not cover

- Code formatting, commenting style, variable naming.
- Compilation or execution of the actual C++ code.
- Runtime thread-safety proof (requires ThreadSanitizer or equivalent).
- Real memory consumption (requires actual VS frame sizes at runtime).
- VapourSynth API interaction correctness (requires VS runtime).

### 2.3 Focus

Primary focus: the normal near-linear encoding use case — sequential
frames with BestSource-style jitter, 6 threads, no large jumps.

Large-jump scenarios are a separate future exercise not covered by
this plan.

---

## 3. Static Analysis Review Items

### 3.1 Spec compliance

For each section of CMS05.1 that has a corresponding implementation,
verify:
- The implementation matches the specified behaviour, or
- Any variation is documented and demonstrably achieves the same
  outcome safely and reliably.

Key sections: 2.2 (RC rules), 4.2 (hot zone lifecycle), 4.3 (pruning),
4.5.1 (rolling predecessor), 4.5.2 (final-frame ownership transfer),
4.6 (ceiling and abort), 4.9 (first-in-best-dressed store), 9.5
(shutdown protocol).

### 3.2 RC rule compliance (per function)

Go through RC1–RC8 systematically:

- **RC1** — All cache-owned `addFrameRef` calls occur only inside the
  single store helper.
- **RC2** — All cache-owned `freeFrame` calls occur only inside the
  single remove helper. No raw `pool.erase()` or `cache_index.erase()`
  outside the remove helper.
- **RC3** — Store error paths that took `addFrameRef` execute a
  balancing `freeFrame` before returning.
- **RC4** — Lookup helper error paths that took `addFrameRef` execute
  a balancing `freeFrame` before returning nullptr.
- **RC5** — Every caller of the lookup helper frees or transfers the
  returned reference on every exit path.
- **RC6** — `clear()` / destructor iterates all slots, `freeFrame`s
  each, warns on pinned checkpoints, clears indexes.
- **RC7** — `validate_invariants` includes cache-side ref-balance
  check: `addframeref_total - freeframe_total == live slot count`.
- **RC8** — `store_frame` checks `cache_index` for frame number before
  taking `addFrameRef`. On duplicate, returns success without modifying
  state.

### 3.3 Error path completeness

For every function that can fail, trace all failure paths and verify:
- Any pinned checkpoint is unpinned exactly once.
- Any caller-owned VSFrame reference is freed exactly once.
- Any source frame obtained from VS is released.
- Any destination frame allocated but not returned is released.
- Hot zone state is not rolled back.
- Diagnostic counters are still incremented on failure paths.

### 3.4 Mutex correctness

Verify by static reasoning:
- Every access to shared state (`non_checkpoint_pool`,
  `checkpoint_pool`, `cache_index`, hot zone array, stats counters)
  is performed while holding `cache_mutex`.
- Every `_externally_locked` helper is called only at call sites that
  already hold `cache_mutex`.
- No public auto-locking helper calls another public auto-locking
  helper (double-lock / deadlock risk).
- Lock is released on all exit paths of every function that acquires it.
- No code holds the mutex while calling VS API functions that might
  re-enter the cache manager.

### 3.5 Hot zone lifecycle correctness

- **Sliding rule:** `low` and `high` are recomputed from F using the
  radii constants — not incrementally updated (drift risk).
- **Jump detection:** threshold comparison correct and consistent with
  the constant definition.
- **fmUnordered retirement:** eager retirement fires at `arInitial`;
  stale zones correctly identified.
- **fmParallelRequests retirement:** lazy retirement checks both
  conditions (no live frames in range AND no pinned checkpoint in
  range).
- **Merge:** selects two zones with smallest boundary gap, produces a
  zone covering both original ranges.

### 3.6 First-in-best-dressed store correctness

- `cache_index` lookup happens under the mutex before `addFrameRef`.
- On duplicate, existing slot not mutated in any way.
- On duplicate, caller's supplied frame pointer not touched.
- Correct counters incremented on the duplicate path.

### 3.7 Final-frame ownership transfer

- `release()` used (not `freeFrame`) when walk's `prev_ref` is the
  requested output frame being returned to VapourSynth.
- No path calls `freeFrame` after ownership already transferred to VS.
- Intermediate predecessor frames correctly `freeFrame`d, not leaked.

### 3.8 Rolling predecessor reference pattern

- Walk always acquires new predecessor reference before releasing old
  one — no zero-ref window.
- Pattern verified across all iterations including first
  (source-copy initialisation) and last (final-frame transfer).
- On error exit from within the loop, `prev_ref` and any in-progress
  `new_frame` are both freed.

### 3.9 Pruning correctness

- Eviction key is hot-zone distance, not insertion order or
  frame-number magnitude.
- All evictions go through the single remove helper (RC2).
- Phase A rule in effect (no `pin_count` check on non-checkpoint
  frames until Phase CMS02-I).
- Checkpoint pruning applies the same hot-zone candidate filtering.
- Frame zero is never pruned.

### 3.10 Shutdown completeness

- `clear()` acquires mutex, iterates both pools, `freeFrame`s every
  frame, warns on stuck pins, erases all slots, clears `cache_index`,
  resets hot zones, calls `validate_invariants`.
- `validate_invariants` after clear confirms ref-balance == zero.

### 3.11 Instrumentation coverage

- Every counter declared in CMS05.1 Section 6 is incremented at the
  right point.
- No counter is incremented more than once per event.
- Counters behind `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS` are correctly
  gated.
- Headline counters are not behind a gate.
- No diagnostic output goes to stdout.
- `predecessor_missing_when_expected > 0` produces a prominent warning
  regardless of gate setting.
- Ref-balance failure at shutdown produces a prominent warning
  regardless of gate setting.

### 3.12 validate_invariants coverage

- Ref-balance check uses live counter values, not a stale snapshot.
- `validate_invariants` called after every mutation in development mode
  (at minimum after store, remove, prune, and shutdown).
- Failure causes diagnostic output and returns a clear error indicator
  (does not silently succeed).

### 3.13 Data structure usage

- `std::map` for pools — iteration order (ascending by frame number)
  used correctly, not assumed incorrectly elsewhere.
- `std::unordered_map` for `cache_index` — no iteration order assumed.
- Hot zone array (fixed size) — bounds checked before access.
- Frame number integer arithmetic — no overflow risk for realistic
  frame counts (VHS at 25fps, hours of content).

### 3.14 fmUnordered / fmParallelRequests transition safety

- Only mode-dependent code is in the retirement helper.
- `Cnr3CacheSchedulingMode` enum threaded through correctly.
- Update-hot-zones helper has no mode-specific logic.
- Transition to fmParallelRequests wiring (Phase CMS02-J) requires
  no changes to core cache logic — only the retirement mode parameter.

### 3.15 Additional items

- **`Cnr3OwnedFrameRef` RAII wrapper:** destructor calls `freeFrame`,
  `release()` suppresses destructor `freeFrame`, move semantics
  transfer ownership correctly, copy is deleted.
- **Ceiling calculation:** uses ceil-style subsampled dimension for
  chroma planes; result clamped to `[150, 1000]`; `CNR3_CACHE_BYTE_BUDGET
  = 1024 * 1024 * 1024` (1 GiB).
- **Verbose counter name:** `duplicate_store_computed_but_discarded`
  present and not silently renamed.

---

## 4. Monte Carlo Simulations

### 4.1 Approach

Pure Python simulations modelling cache manager policy behaviour.
Not running the C++ code — modelling the cache manager's decisions
(what gets stored, pruned, protected, and evicted) under realistic
request patterns.

The models verify that the *policy* produces correct outcomes. The
static analysis verifies that the *implementation* matches the policy.
Together they give strong coverage.

**Computational cost:** Negligible. A 200-frame simulation with 50 runs
completes in under a second in Python. A 10,000-frame single run
completes in a few seconds. The simulation is not the expensive part
of the session — loading and reasoning about source files is.

### 4.2 Scope: Standard Use Case (This Exercise)

**Scenario:** Near-linear encoding. Sequential frames with BestSource-
style jitter. No large jumps.

**Parameters (short runs):**
- Frames: 200
- Threads: 6
- Jitter model: each frame dispatched at time =
  `frame_number + uniform_random(0, 6)`. Frames arrive at the cache
  in delivery-time order.
- Monte Carlo runs: **50 per sub-scenario**
  (gives solid p50 and rough p95; sufficient for normal use case where
  behaviour should be well-behaved throughout)

**Parameters (long run):**
- Frames: **10,000**
- Threads: 6, jitter 0–6
- Runs: **1 per sub-scenario**
- Purpose: observe hot-zone sliding dynamics, pool size stability,
  pruning cadence, and ref-count balance over an extended period.
  The long run shows *dynamics over time*; the short runs provide
  *statistical coverage* of the outcome distribution.

**Sub-scenarios:**

**(a) fmUnordered** — 6-thread jitter model. Verify:
- One active hot zone, sliding correctly.
- Zone low boundary advances as encode progresses (old frames prunable).
- Pool size stays within `[CAPACITY, CAPACITY × OVERFLOW_FACTOR]`
  under steady operation.
- Prune evicts only frames outside the hot zone.
- `predecessor_missing_when_expected == 0` across all 50 runs.
- Cache-side ref balance holds after every operation.
- Caller-side lookup-ref balance holds at end of each simulated request.

**(b) fmParallelRequests** — Same jitter model with genuine concurrent
in-flight requests. 6 threads simultaneously in-flight, interleaved in
random order. Verify:
- Hot zone correctly covers the spread of concurrent requests.
- Lazy retirement does not retire a zone while requests are in-flight
  within its range.
- First-in-best-dressed store handles occasional duplicate-store races.
- `duplicate_store_computed_but_discarded` increments occasionally
  but not frequently.
- `predecessor_missing_when_expected == 0` across all 50 runs.
- Ref balance holds throughout including duplicate-store races.

**(c) fmParallel (informational only)** — Same model with relaxed mutex
ordering (simulating out-of-order completion). Not part of the
implementation target. Run to identify which invariants would be
violated and what additional mechanisms fmParallel would require.
Results inform future planning, not current implementation.

### 4.3 What the simulations measure and report

For each sub-scenario, across all 50 short runs and the 1 long run:

**Pool behaviour:**
- `max_non_checkpoint_pool_size_observed` (mean, p95, max over short
  runs; time series in long run)
- `max_checkpoint_pool_size_observed` (same)
- `prune_no_candidate_exists` rate (should be zero in normal operation)
- `cache_ceiling_hard_aborts` (should be zero in normal operation)

**Hot zone behaviour:**
- `hot_zone_allocations` per run (expect 1 for linear encoding)
- `hot_zone_slides` per run (expect ~200, one per frame, in short runs)
- `hot_zone_merges` per run (expect 0 in normal linear operation)
- `hot_zone_retirements` per run
- Distribution of zone `[low, high]` width over time in long run

**Pruning behaviour:**
- `non_checkpoint_prune_skipped_in_hot_zone` per prune event
- Eviction distance from zone boundary (should always be > 0)
- Frames erroneously evicted from within the hot zone (must be 0)

**Reference-count balance:**
- `cache_addframeref_total - cache_freeframe_total` at end of each run
  (must equal live slot count, must equal 0 after simulated shutdown)
- `lookup_owned_ref_acquired - (released + transferred)` at end of each
  run (must equal 0)

**Correctness:**
- `predecessor_missing_when_expected` per run (must be 0 in all runs)
- `duplicate_store_computed_but_discarded` per run
- Recovery walk completeness (every requested frame produced)

### 4.4 CSV Output and Excel Visualisation

Each simulation run produces CSV files for further analysis.

**CSV files generated:**

`short_runs_summary.csv` — one row per run per sub-scenario. Columns:
```
scenario, run_id, max_pool_size, max_checkpoint_pool_size,
hot_zone_allocations, hot_zone_slides, hot_zone_merges,
hot_zone_retirements, prune_no_candidate, ceiling_aborts,
predecessor_missing, duplicate_store_discarded,
cache_ref_balance_end, lookup_ref_balance_end
```

`long_run_timeseries.csv` — one row per frame for each sub-scenario.
Columns:
```
scenario, frame_number, pool_size, checkpoint_pool_size,
hot_zone_low, hot_zone_high, hot_zone_width,
cumulative_prune_events, cumulative_slides,
running_cache_ref_balance, running_lookup_ref_balance
```

**How to visualise in Excel:**

1. **Open `long_run_timeseries.csv` in Excel** (File → Open → browse
   to file).

2. **Pool size over time:**
   - Select columns `frame_number` and `pool_size`.
   - Insert → Chart → Line chart.
   - This shows whether the pool stays bounded and whether pruning
     keeps up with stores. You want to see the line oscillating around
     100 (the soft target) without drifting upward.

3. **Hot zone boundaries over time:**
   - Select columns `frame_number`, `hot_zone_low`, `hot_zone_high`.
   - Insert → Chart → Line chart.
   - This is the key visualisation for the sliding mechanism. You want
     to see two parallel lines advancing in lockstep with
     `frame_number`, always separated by approximately 60 frames
     (BACK_RADIUS + FORWARD_RADIUS). If the lines spread apart or
     stop advancing, something is wrong with the sliding logic.

4. **Hot zone width over time:**
   - Select `frame_number` and `hot_zone_width`.
   - Insert → Chart → Line chart.
   - Should be a flat horizontal line at 61 (= BACK + FORWARD + 1)
     throughout normal linear operation. Any deviation indicates
     incorrect sliding behaviour.

5. **Reference-count balance over time:**
   - Select `frame_number` and `running_cache_ref_balance`.
   - Insert → Chart → Line chart.
   - Should match the pool size exactly at every frame. Any divergence
     is a ref-count discipline violation.

6. **Open `short_runs_summary.csv` for statistical summary:**
   - Use Excel's built-in `AVERAGE()`, `PERCENTILE()`, `MAX()` on each
     column to compute mean, p50, p95, and max across the 50 runs.
   - Key thing to check: `predecessor_missing` column must be all zeros.
     Any non-zero value is a critical finding.

7. **Compare sub-scenarios:**
   - Filter `short_runs_summary.csv` by `scenario` column to compare
     fmUnordered vs fmParallelRequests side by side.
   - The duplicate_store_discarded column should be near-zero for
     fmUnordered and occasionally non-zero for fmParallelRequests.

### 4.5 Large-jump simulation (separate future exercise)

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

4. A brief implementation note (plain text or markdown) covering:
   - Which CMS02 phases have been completed.
   - Any deliberate variations from CMS05.1 and the rationale for each.
   - Any known issues or areas of uncertainty to focus on.
   - Which `Cnr3CacheSchedulingMode` is currently wired (expected:
     `FmUnordered` for initial phases).

The CMS05.1 spec does not need to be re-uploaded — it is in context
from prior sessions, or can be re-uploaded if the review is in a new
conversation.

---

## 6. Session Structure

The review is best split into two sessions:

**Session 1 — Analysis:**
- Upload source files and implementation note.
- Run simulations (Python, in-session). Generate CSVs.
- Perform static analysis and path tracing against the checklist in
  Section 3.
- Produce a structured findings document covering: spec compliance
  issues, RC rule violations, error path gaps, mutex concerns, and
  simulation results with interpretation.

**Session 2 — Decision and remediation:**
- Start with the Session 1 findings document as input.
- Reason about priority and remediation for each finding.
- Produce a prioritised fix list with specific guidance.
- Decide whether the implementation is ready to proceed to Phase
  CMS02-I or whether findings require remediation first.

This split keeps each session's context clean and focused.

---

## 7. Model Recommendation

**Opus 4.7** for both sessions.

The static analysis requires holding the entire CMS05.1 spec and the
full source code in working context simultaneously. The simulation
results need to be interpreted alongside both. This is exactly the
long-context reasoning task for which Opus is well-suited.

Sonnet is appropriate for shorter tasks within the development process
(quick questions, simple edits). Opus is appropriate for this review.

---

## 8. Guiding Principle

From the CMS05.1 design process:

> The highest risk is not that a newly written helper is obviously wrong.
> The highest risk is that an older helper remains in the call path with
> assumptions from the previous design — mutex placement, ownership
> semantics, pruning behaviour, reference-count handling. The new code
> calls the old helper expecting CMS05.1 semantics, but the old helper
> silently provides v005 semantics.

The static analysis addresses this directly: the review checks complete
execution paths, not isolated edited functions.

