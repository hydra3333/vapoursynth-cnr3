# CNR3 DIAG.3a coder confirm report — D-SUM-03 / D-SUM-12 / D-SUM-13

**Role:** coder-side investigation / confirmation report.  
**Patch status:** no patch generated yet.  
**Source inspected:** latest coordinator source upload `src(17).zip`, unpacked for inspection.  
**Scope:** DIAG.3a only: D-SUM-03 recovery-search, D-SUM-12 recovery-plan / recovery-rate, D-SUM-13 recalculation. DIAG.3b remains separate.

## 0. Executive result

Proceeding with the split is correct. DIAG.3a can be implemented as an additive getFrame-side diagnostic patch, but I recommend designer confirmation on two details before patching:

1. **D-SUM-12 plan create/destroy balance:** count only *accepted/published recovery plans* as `recovery_plans_created`, and count destroy at frameData teardown for recovery branch. This avoids unbalanced counts from arInitial refusal/early-bail scratch plans.
2. **D-SUM-13 bounded container:** use a fixed-capacity open-addressed per-instance compute-count table derived like D-SUM-10: `max(B, active_ceiling_max) * K`, with floor and saturation-honesty fields. I propose `K = 16`, floor `1024`, yielding normal capacity `16000` and tiny-cache capacity `1600`.

I do **not** recommend implementing the optional DSUM10+DSUM12 in-run ring-correlation counter in DIAG.3a. It is not trivial/clean: it would require cross-family snapshot/copy or cache-internal access from getFrame-side recovery observation. External correlation against D-SUM-10 ring dumps is the cleaner DIAG.3a baseline.

## 1. Baseline validation

The latest source is post-DIAG.2b:

- `src/cnr3_build_config.h:35` has `CNR3_EDIT_VERSION = "CMS07-DIAG.2b-ownership-integrity-store-root"`.
- `src/cnr3_build_config.h:187-193` has the D-SUM-03 compute/print gate and two-gate print-without-compute error.
- `src/cnr3_build_config.h:433-439` has the D-SUM-12 compute/print gate and two-gate error.
- `src/cnr3_build_config.h:458-464` has the D-SUM-13 compute/print gate and two-gate error.
- DIAG.2b markers are present: `CNR3_DSUM05_FAIL` at `src/cnr3_cache_core.cpp:184-186`, `ownership_diag_stats_` at `src/cnr3_cache_core.h:1753`, and `observe_store_outcome_locked` at `src/cnr3_cache_core.cpp:1976`.

No `build_config.h` gate additions are required for DIAG.3a. The patch should consume the existing 03/12/13 gates unchanged.

## 2. D-SUM-01 pattern to mirror

D-SUM-01 is a plugin/getFrame-side diagnostic, not a cache-core member diagnostic. DIAG.3a should mirror it.

Confirmed source pattern:

- Per-instance stats live in `Cnr3FilterData`: `src/cnr3_plugin_internal.h:40-42` stores `Cnr3DiagDsum01RequestOrderStats dsum01_request_order` under the compute gate.
- D-SUM-01 stats contain a diagnostics-only `mutable std::mutex`: `src/cnr3_diagnostics.h:102-113`.
- Observation helpers are free functions taking the stats object by reference: declarations at `src/cnr3_diagnostics.h:157-164`; implementations at `src/cnr3_diagnostics.cpp:240-273`.
- Observation locks only the diagnostic stats mutex, not the cache/CMS lock: `src/cnr3_diagnostics.cpp:248` and `src/cnr3_diagnostics.cpp:271`.
- Snapshot locks and copies to a by-value snapshot: `src/cnr3_diagnostics.cpp:275-299`.
- Writer formats and writes outside the stats lock via the snapshot: `src/cnr3_diagnostics.cpp:305-428`.
- arInitial observes request arrival at `src/cnr3_arInitial.cpp:481-486`.
- arAllFramesReady observes completion arrival at `src/cnr3_arAllFramesReady.cpp:1780-1784`.
- free-filter emission is in `src/vapoursynth-Cnr3.cpp:302-307`.

Implementation implication: add `Cnr3DiagDsum03RecoverySearchStats`, `Cnr3DiagDsum12RecoveryPlanStats`, and `Cnr3DiagDsum13RecalculationStats` to `Cnr3FilterData`, each under its own compute gate and each with its own diagnostics-only mutex. Do not add a new shared mutex and do not touch cache-core locking.

## 3. D-SUM-03 recovery-search confirmation

### 3.1 Bounded-search invocation

The live getFrame bounded search is invoked once per recovery routing attempt in `cnr3_start_live_recovery()`:

- Function starts at `src/cnr3_arInitial.cpp:331`.
- Scratch `Cnr3CacheRecoverySearchPlan recovery_plan{}` is created at `src/cnr3_arInitial.cpp:348`.
- The search call is `data.output_cache.plan_bounded_recovery_search_and_record_anchor_pin(...)` at `src/cnr3_arInitial.cpp:349-355`.

D-SUM-03 `search_attempts` should increment immediately before or immediately after this call, exactly once per invocation.

### 3.2 Termination / outcome points

Outcome sites in `cnr3_start_live_recovery()`:

- Hard search failure: `!cnr3_status_is_ok(plan_status)` at `src/cnr3_arInitial.cpp:357-365`. Count `search_failures` and `terminated_on_failure`.
- Exact-anchor accepted: `cnr3_exact_anchor_recovery_plan_is_accepted(...)` at `src/cnr3_arInitial.cpp:370-372`. Count `search_successes` and `terminated_on_present_output`.
- Floor fresh-start accepted: `cnr3_floor_fresh_start_recovery_plan_is_accepted(...)` at `src/cnr3_arInitial.cpp:373-381`. Count `search_successes`; termination reason should be `terminated_on_frame0` when `search_lower_frame == 0`, otherwise `terminated_on_bound`.
- Refusal after a syntactically ok search result: `else` at `src/cnr3_arInitial.cpp:382-397`. Count `search_failures`; if `recovery_plan.anchor_found` then terminate on present output but failed acceptance, otherwise frame0/bound depending on `search_lower_frame`.

The acceptance predicates are small and source-grounded:

- Exact anchor acceptance validates anchor found, anchor pin recorded, expected anchor frame, and contiguous hole list at `src/cnr3_arInitial.cpp:128-162`.
- Floor fresh-start acceptance validates no anchor, valid lower frame, requested frame, repair target, and empty hole catalogue at `src/cnr3_arInitial.cpp:164-182`.

### 3.3 Depth metric proposal

The cache-core search implementation descends from `requested_frame - 1` down to the inclusive lower bound:

- Lower/upper derived at `src/cnr3_cache_core.cpp:3571-3585`.
- Candidate loop descends at `src/cnr3_cache_core.cpp:3596-3620`.
- Anchor fields are set at `src/cnr3_cache_core.cpp:3626-3628`.
- Missing frames between anchor and requested are catalogued at `src/cnr3_cache_core.cpp:3630-3647`.

The current public plan does not expose an internal loop counter. A clean derived metric is:

```text
if !search_interval_has_frames:
    depth = 0
else if anchor_found:
    depth = search_upper_frame - anchor_frame_number + 1
else:
    depth = search_upper_frame - search_lower_frame + 1
```

This is exactly the number of candidate positions the bounded search must have examined before terminating, assuming no hard invariant failure. For hard failures, count `terminated_on_failure`; record depth as 0 unless the plan fields are valid enough to derive the conservative interval depth. I recommend the simpler initial policy: hard failures use depth 0, because they should be exceptional and are not the FI-11 normal-path signal.

Histogram bins should mirror the scope and B=50:

```text
0, 1, 2-5, 6-15, 16-30, 31-50, 51+
```

The `51+` bin is a guard/sanity bin; normal B=50 should never use it.

### 3.4 holes_filled reconciliation

`holes_filled` cannot be finalised in arInitial. Reconcile in arAllFramesReady after the per-hole walk, counting per-hole outcomes of `computed`, `adopted_skipped`, and `adopted_post_compute_loser` as filled. The per-hole outcomes are carried in `request_data->per_hole_outcomes` and set at:

- Adopted/skipped: `src/cnr3_arAllFramesReady.cpp:1232-1240`.
- Computed or post-compute-loser: `src/cnr3_arAllFramesReady.cpp:1396-1399`.

## 4. D-SUM-12 recovery-plan / recovery-rate confirmation

### 4.1 Five-way branch decision site

The five-way rate denominator should be observed at branch-publication / branch-acceptance sites in arInitial:

1. Cache hit: `cache_hit_pin_status` ok and `cnr3_publish_live_cache_hit_return(...)` at `src/cnr3_arInitial.cpp:523-535`; branch fields set at `src/cnr3_arInitial.cpp:53-57`.
2. Frame 0: `n == 0` at `src/cnr3_arInitial.cpp:547-555`; branch fields set at `src/cnr3_arInitial.cpp:118-123`.
3. Predecessor present: `predecessor_pin_status` ok at `src/cnr3_arInitial.cpp:565-581`; branch fields set at `src/cnr3_arInitial.cpp:91-97`.
4. Recovery exact-anchor: accepted at `src/cnr3_arInitial.cpp:370-372`, then branch fields published at `src/cnr3_arInitial.cpp:399-405`.
5. Recovery floor-fresh-start: accepted at `src/cnr3_arInitial.cpp:373-381`, then branch fields published at `src/cnr3_arInitial.cpp:399-405`.

`frames_total` should count only these successful branch publications, not early setFilterError bails before a strategy exists. This keeps the recovery-rate denominator semantically “planned frames” rather than “failed attempts”.

### 4.2 Recovery-plan create/destroy balance proposal

The source always default-constructs a `Cnr3CacheRecoverySearchPlan` inside every frameData object (`src/cnr3_plugin_internal.h:79`), so counting every value construction would be meaningless and would include non-recovery branches.

I propose this strict semantic balance:

- `recovery_plans_created`: increment exactly once when an accepted recovery branch is fully publishable, immediately before `*frame_data = request_data` at `src/cnr3_arInitial.cpp:450-451`.
- `recovery_plans_destroyed`: increment exactly once in frameData teardown when the request data has `branch == Cnr3LiveGetFrameBranch::recovery`, in `cnr3_discard_frame_data_with_cache()` before `delete request_data`, at `src/cnr3_arAllFramesReady.cpp:439-456`.

This balances every recovery plan that escapes arInitial and is later consumed or discarded by arAllFramesReady, including all arAllFramesReady bail paths because they already call `cnr3_discard_frame_data_with_cache(...)`.

I intentionally do **not** propose counting a created plan at `src/cnr3_arInitial.cpp:348` or `src/cnr3_arInitial.cpp:399`. There are several arInitial bails after branch assignment but before publication (`src/cnr3_arInitial.cpp:406-448`); counting earlier would require a second unpublished-delete destroy path and would make the balance more fragile without improving the FI-11 signal.

If designer wants scratch-plan lifecycle rather than accepted-plan lifecycle, the patch can count both, but I recommend accepted-plan lifecycle only for DIAG.3a.

### 4.3 Recovery-rate and span fields

Proposed observations:

- `frames_total`: increment at every successful branch publication.
- `frames_cache_hit`: cache-hit publication.
- `frames_pred_present`: predecessor-present publication.
- `frames_frame0`: frame-0 publication.
- `frames_recovered_exact`: accepted exact-anchor recovery publication.
- `frames_recovered_floor`: accepted floor-fresh-start recovery publication.
- `recovery_rate_percent`: derived at write time as `(frames_recovered_exact + frames_recovered_floor) / frames_total * 100`.
- `recovery_span_sum` / `recovery_span_max`: for exact-anchor recovery, use `requested_frame - anchor_frame_number`; for floor-fresh-start, keep separate floor counters rather than folding into exact span, consistent with scope wording.

### 4.4 Plan fields / hole reconciliation

Source-grounded observations:

- `nearest_present_output_found`: increment when accepted exact recovery has `recovery_plan.anchor_found` and `anchor_pin_recorded`.
- `holes_identified`: use `recovery_plan.hole_frame_numbers.size()` after final plan construction. For floor fresh-start, this list is rewritten by `cnr3_fill_floor_fresh_start_hole_numbers()` at `src/cnr3_arInitial.cpp:303-329`.
- `source_frames_for_holes_requested`: count only the frames in `hole_frame_numbers`, not the target frame `n`. For floor fresh-start, do not count `recovery_floor_frame` as a hole unless designer explicitly wants “floor output” included in hole accounting.
- `source_frames_for_holes_retrieved`: increment for successful `getFrameFilter(hole_frame, ...)` at `src/cnr3_arAllFramesReady.cpp:1271-1275`, not for target retrieval at `src/cnr3_arAllFramesReady.cpp:1435` and not for floor retrieval at `src/cnr3_arAllFramesReady.cpp:1098-1102`.
- `holes_filled`: count per-hole outcomes set at `src/cnr3_arAllFramesReady.cpp:1237-1240` and `src/cnr3_arAllFramesReady.cpp:1396-1399`.
- `fallback_failures`: increment on accepted floor-fresh-start arAllFramesReady failures that prevent the floor foundation or walk from completing. Candidate bail sites include floor source/request/copy/adopt/store failures at `src/cnr3_arAllFramesReady.cpp:1066-1182`.
- `bounded_start_honesty_failures`: increment when an accepted recovery plan later violates lifecycle/foundation checks, especially `src/cnr3_arAllFramesReady.cpp:1001-1015` and `src/cnr3_arAllFramesReady.cpp:1022-1039`.

### 4.5 Optional D-SUM-10 + D-SUM-12 ring-correlation counter

I recommend **defer**.

Feasibility finding:

- D-SUM-10 ring data is cache-core-owned in `Cnr3CachePruneDiagnosticStats`, including `recently_evicted_ring` at `src/cnr3_cache_diagnostics.h:182-190`.
- The public snapshot `Cnr3OutputCacheCore::prune_diagnostic_stats()` locks the cache and returns a by-value copy at `src/cnr3_cache_core.cpp:640-646` and `src/cnr3_cache_core.cpp:1808-1812`.
- A getFrame-side D-SUM-12 observer could call that snapshot, but it would copy the bounded ring vector and create cross-family observation coupling. A cache-core internal read would violate the DIAG.3a “no cache-core change expected” direction.

Therefore the clean DIAG.3a path is external/offline correlation: read D-SUM-12 recovered target/anchor/span data alongside D-SUM-10 `[DSUM10-RING-*]` dumps. Add the in-run counter later only if designer decides the cross-family coupling is worth it.

## 5. D-SUM-13 recalculation confirmation

### 5.1 Definition

A frame is “computed” when a getFrame path successfully creates an output for a frame and reaches a successful store/return or store/pin point. A “recalculation” event occurs when the same frame is computed after already having a prior compute count in this filter instance.

`adopted_skipped` is not a compute event. `adopted_post_compute_loser` is a compute event and a race-loser/adopt outcome.

### 5.2 Compute-completion sites

Source-grounded compute completion points:

1. **Frame 0 fresh-start:** successful store/prune of output[0] at `src/cnr3_arAllFramesReady.cpp:1711-1724`; count frame `n` after the store succeeds at `src/cnr3_arAllFramesReady.cpp:1725-1735`.
2. **Predecessor-present target:** `cnr3_store_live_output_frame_for_authoritative_return(...)` at `src/cnr3_arAllFramesReady.cpp:934-948`; count frame `n` after success at `src/cnr3_arAllFramesReady.cpp:950-958`. If `returned_cached_winner` is true, this was still computed and lost a production duplicate race.
3. **Floor fresh-start base:** floor output copied/adopted and stored via `store_as2_floor_and_prune(...)` at `src/cnr3_arAllFramesReady.cpp:1160-1170`; count `recovery_floor_frame` after success at `src/cnr3_arAllFramesReady.cpp:1173-1188`, including duplicate-existing-slot as post-compute-loser.
4. **Recovery holes:** hole output processed, adopted, and stored via `store_recovery_hole_and_prune(...)` at `src/cnr3_arAllFramesReady.cpp:1371-1383`; count `hole_frame` after success at `src/cnr3_arAllFramesReady.cpp:1385-1399`, including duplicate-existing-slot as post-compute-loser.
5. **Recovery target:** target output processed and stored/returned via `cnr3_store_live_output_frame_for_authoritative_return(...)` at `src/cnr3_arAllFramesReady.cpp:1512-1526`; count frame `n` after success at `src/cnr3_arAllFramesReady.cpp:1528-1536`. If `returned_cached_winner` is true, this is computed-but-lost.

Do not count:

- Cache-hit return (`src/cnr3_arAllFramesReady.cpp:716-797`): no compute.
- Recovery `adopted_skipped` hole/floor outcomes: existing output reused, no compute.
- Any bail before successful store/return or store/pin.

### 5.3 Recalculation depth proposal

Proposed documented depth:

- Frame0 fresh-start and predecessor-present direct target: depth 0.
- Floor fresh-start base: depth 0.
- Exact recovery hole: `hole_frame - recovery_plan.anchor_frame_number`.
- Exact recovery target: `n - recovery_plan.anchor_frame_number`.
- Floor recovery hole: `hole_frame - recovery_floor_frame` after the floor has materialized.
- Floor recovery target: `n - recovery_floor_frame`.

Depth histogram bins:

```text
0, 1, 2-5, 6-15, 16-30, 31-50, 51+
```

Normal bounded recovery should not exceed 50 except for a bug or future policy change; `51+` is a sanity/saturation bin.

### 5.4 Bounded container proposal

Use a per-instance fixed-capacity open-addressed table keyed by frame number. This avoids unbounded `std::map`/`unordered_map` growth and avoids hot-path allocation.

Proposed constants, in `cnr3_diagnostics.h` under `CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION`:

```cpp
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_MAP_CAPACITY_MULTIPLIER = 16U;
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_MAP_CAPACITY_FLOOR = 1024U;
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_MAP_CAPACITY =
    max(CNR3_DIAG_DSUM13_COMPUTE_MAP_CAPACITY_FLOOR,
        max(CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
            CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES) *
        CNR3_DIAG_DSUM13_COMPUTE_MAP_CAPACITY_MULTIPLIER);
```

Concrete capacity:

- Normal profile: `max(50, 1000) * 16 = 16000`.
- Tiny profile: `max(15, 100) * 16 = 1600`.

The table entry can be:

```cpp
struct Cnr3DiagDsum13ComputeCountEntry {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    std::uint32_t compute_count = 0;
};
```

Stats struct fields:

```cpp
mutable std::mutex mutex{};
std::array<Cnr3DiagDsum13ComputeCountEntry, CNR3_DIAG_DSUM13_COMPUTE_MAP_CAPACITY> compute_counts{};
std::size_t compute_count_entry_count = 0;
bool compute_count_map_saturated = false;
std::uint64_t compute_count_observations_dropped = 0;
std::uint64_t computed_output_observations = 0;
std::uint64_t recalculated_frame_count = 0;
std::uint64_t frames_recalculated_once = 0;
std::uint64_t frames_recalculated_multiple_times = 0;
std::uint64_t recalculation_depth_histogram[7] = {};
int max_recalculation_depth = 0;
```

Saturation policy:

- If a new frame cannot be inserted because the table is full, set `compute_count_map_saturated = true` and increment `compute_count_observations_dropped`.
- Existing-frame observations should still update if found.
- Writer must print a clear line: `compute_count_map_saturated yes/no; counts are lower bounds if saturated`.
- Normal S-series expectation: saturated false.

Open-addressing details:

- Linear probing is sufficient and deterministic.
- Hash can be a small unsigned mix of `frame_number` modulo table capacity.
- Since the table is per-instance and protected by a diagnostics-only mutex, no lock-free complexity is needed.
- No allocation, no unbounded growth, no cache-lock interaction.

## 6. Cross-cutting patch-shape proposal

Expected files for DIAG.3a patch:

- `src/cnr3_diagnostics.h`
- `src/cnr3_diagnostics.cpp`
- `src/cnr3_plugin_internal.h`
- `src/cnr3_arInitial.cpp`
- `src/cnr3_arAllFramesReady.cpp`
- `src/vapoursynth-Cnr3.cpp`
- `src/cnr3_cache_core_selftest_main.cpp`

No cache-core change is expected. No `build_config.h` gate change is expected. The edit-version marker may be advanced only if the coordinator/designer wants proof identity carried as in DIAG.2b.

## 7. R-PROCESS-19 / additive-only implications

Implementation can be kept observe-only:

- All stats structs, helpers, snapshots, writers, and hook calls compile out under their family compute gates.
- Hook calls occur after existing decisions/outcomes are already known.
- No hook should alter a return status, request set, branch decision, store decision, pinning, source retrieval, or pixel processing.
- arInitial/arAllFramesReady bails should not be restructured in DIAG.3a.
- Writers snapshot under the diagnostics mutex and format/write outside it.
- free-filter emission should be numeric order: 01, 03, 04, 05, 08, 10, 11, 12, 13 for currently landed families.

## 8. Questions for designer before patch generation

1. Confirm D-SUM-12 should count `recovery_plans_created/destroyed` for **accepted/published recovery plans only**, not scratch `Cnr3CacheRecoverySearchPlan` values created before recovery acceptance.
2. Confirm D-SUM-12 `holes_identified`, `holes_filled`, and `source_frames_for_holes_*` should count `recovery_plan.hole_frame_numbers` only, excluding target `n` and excluding floor fresh-start base unless separately requested.
3. Confirm D-SUM-13 capacity proposal: fixed open-addressed table, capacity derived as `max(B, active_ceiling_max) * 16` with floor 1024, saturation flag, dropped-observation count, no unbounded map.
4. Confirm D-SUM-13 depth definition as recovery distance from anchor/floor, with direct/fresh-start depth 0.
5. Confirm optional DSUM10+DSUM12 in-run ring-correlation counter is deferred for DIAG.3a, using external D-SUM-10 ring dump correlation instead.

If accepted, I can generate the DIAG.3a patch next.
