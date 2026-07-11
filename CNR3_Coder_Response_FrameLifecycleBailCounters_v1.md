# CNR3 — CODER RESPONSE: frame-lifecycle bail/compute/store counters — v1

**From:** coder (W3C)  
**To:** coordinator (W3X) and designer/reviewer (W3D)  
**Scope reviewed:** `CNR3_Patch_Scope_FrameLifecycleBailCounters_v1.md`  
**Source inspected:** uploaded `src(26).zip`  
**Source marker verified:** `CMS07-DIAG.lookup-site-breakdown` in `src/cnr3_build_config.h`.

## 1. Executive verdict

Do **not** patch v1 as written yet.

The requested diagnostic is useful and implementable in principle, but the cold source review found several real semantic hazards in the proposed event/origin/self-check definitions. These are not implementation details; they affect what the counters would mean.

The main issue is that the current three-origin split and several proposed cross-checks do not line up exactly with the live source paths:

1. `frame0_or_floor_fresh_start` conflicts with the proposed structural-zero claim for `bailed_before_compute` frame0/floor counts.
2. `frames_computed == D-SUM-07 temporary_outputs_created` is not a structural identity. D-SUM-07 counts temporary-frame creation after `copyFrame()`, before pixel processing succeeds.
3. `e_total == D-SUM-07 duplicate_computed_but_discarded` is not a structural identity. D-SUM-07 currently records that duplicate-discard event only in the authoritative-return helper, not for AS2 floor/hole duplicate losers.
4. The D-SUM-08 production-vs-AS2 store-family ties cannot be expressed cleanly using only the proposed three origin buckets, because the buckets cross D-SUM-08 store-family boundaries.
5. The current frame-0 duplicate-store path is a special fourth post-compute outcome: computed, not stored, not discarded, but still returned. If this can occur under parallel activation, `computed == stored + discarded` is not exhaustive as scoped.

I recommend W3D revise/rule on these points before patching.

## 2. Baseline and scope alignment

The uploaded source is the correct baseline for this scope:

```text
src/cnr3_build_config.h:35
inline constexpr const char* CNR3_EDIT_VERSION = "CMS07-DIAG.lookup-site-breakdown";
```

The requested patch is D-SUM-04-gated, observe-only, and should preserve the Ruling-5 pattern from the previous patch: new diagnostic work under `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`, defaulted parameters plain/ungated if needed, and no semantic/control-flow changes.

## 3. Cold source map: live arInitial routing

`src/cnr3_arInitial.cpp` routes live work as follows:

- Site-1 requested-frame present check: `cnr3_arInitial.cpp:913-919`.
  - If found, publishes `cache_hit_return` at `921-929`.
- Frame 0 fresh start: `cnr3_arInitial.cpp:950-958`.
- Site-2 predecessor fast path: `cnr3_arInitial.cpp:968-974`.
  - If found, publishes `predecessor_present_compute` at `976-986`.
- Recovery starts only after predecessor miss: `cnr3_arInitial.cpp:1007-1014`.
- Recovery plan live route opt-in remains at `cnr3_arInitial.cpp:621-629`.

This means the ordinary and frame0 branches have no pre-compute adopt check. The only adopt checks are inside the recovery branch in `arAllFramesReady`.

## 4. Cold source map: compute / production sites

The source has five production shapes, not one generic compute site.

### 4.1 Frame 0 fresh start — origin: frame0/floor fresh-start

Function: `cnr3_complete_live_frame0_fresh_start()`.

Relevant lines:

- `copyFrame(source_frame, core)`: `cnr3_arAllFramesReady.cpp:2759`.
- D-SUM-07 temporary output created: `2807-2811`.
- Store via production store family: `2906-2915`.
- Return original `output_frame` to VapourSynth after store acceptance: `2980-3005` region.

Important: this path does **not** call `cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change()`. It is a fresh-start copy path. If `frames_computed` includes frame0 as the L1 oracle requires, then this patch's word "computed" must mean "produced an output frame", not strictly "ran the CNR3 pixel blend function".

### 4.2 Ordinary predecessor-present compute — origin: ordinary_from_predecessor

Function: `cnr3_complete_live_predecessor_present_compute()`.

Relevant lines:

- Re-acquire already pinned predecessor: `1353-1360`.
- `copyFrame(source_frame, core)`: `1386`.
- D-SUM-07 temporary output created: `1436-1440`.
- Pixel processing call: `1441-1454`.
- Success check requiring `process_summary.frame_processed`: `1463-1486`.
- Authoritative store/return helper: `1524-1537`.

This is the clean ordinary compute path. The best direct `frames_computed` bump point is after the success check at `1463-1486`, not at D-SUM-07 temporary output creation.

### 4.3 Recovery floor fresh-start — origin conflict: frame0_or_floor_fresh_start, but recovery path

Function: `cnr3_complete_live_recovery()` floor branch.

Relevant lines:

- Pre-compute floor adopt check: `1768-1777`.
- If not adopted, `copyFrame(floor_source_frame, core)`: `1834`.
- D-SUM-07 temporary output created: `1888-1892`.
- No P.11C pixel-processing call for floor fresh-start.
- Store via AS2 store family: `1940-1950`.
- Duplicate-vs-computed outcome recorded in `recovery_floor_outcome`: `1976-1979`.

This path exposes the first major scope issue: the origin definition says floor fresh-start belongs to `frame0_or_floor_fresh_start`, but the adopt check exists and can skip floor computation. Therefore `bailed_before_compute_since_already_in_cache.frame0_or_floor_fresh_start` is **not structurally zero** if site7a hits. It is zero in the expected `-r 1` runs, but not structurally zero by source shape.

W3D needs to choose one of these:

- classify floor-adopt as `frame0_or_floor_fresh_start`, making that bucket non-structural-zero;
- classify floor-adopt as recovery because it occurs in recovery control flow, contradicting the stated production-origin definition;
- split `frame0` and `floor_fresh_start` into separate origins;
- or state that site7a floor-adopt is excluded from this lifecycle family, which would make `a_total == site7a.hits + site7b.hits` false.

### 4.4 Recovery hole fill — origin: recovery_hole_fill

Function: `cnr3_complete_live_recovery()` hole loop.

Relevant lines:

- Pre-compute hole adopt check: `2043-2053`.
- Re-acquire already pinned predecessor: `2075-2082`.
- `copyFrame(source_frame, core)`: `2142`.
- D-SUM-07 temporary output created: `2192-2196`.
- Pixel processing call: `2197-2210`.
- Success check requiring `hole_process_summary.frame_processed`: `2219-2243`.
- Store via AS2 store family: `2290-2301`.
- Duplicate-vs-computed outcome recorded in `per_hole_outcomes`: `2334-2337`.

For hole fills, the origin is unambiguous and the post-compute duplicate status is known after `hole_store_summary.as2_summary.duplicate_existing_slot`.

### 4.5 Recovery target compute — origin: recovery_hole_fill per scope wording

Function: `cnr3_complete_live_recovery()` target section.

Relevant lines:

- Re-acquire already pinned target predecessor: `2386-2394`.
- `copyFrame(target_source_frame, core)`: `2441`.
- D-SUM-07 temporary output created: `2491-2495`.
- Pixel processing call: `2496-2509`.
- Success check requiring `target_process_summary.frame_processed`: `2518-2542`.
- Authoritative store/return helper: `2551-2564`.

The scope explicitly says recovery origin includes the recovery target computed from filled holes. Therefore this target is `recovery_hole_fill`, but it stores through the **production** store family, not AS2.

This is the second major source/scope mismatch: `recovery_hole_fill` is not equivalent to D-SUM-08 AS2 consumer stores.

## 5. Store sites and duplicate outcomes

### 5.1 Generic authoritative-return helper

Function: `cnr3_store_live_output_frame_for_authoritative_return()`.

Used by:

- ordinary predecessor-present compute: call at `1528-1537`;
- recovery target compute: call at `2555-2564`.

Important lines inside helper:

- successful store branch: `1024-1043`;
- duplicate branch begins after `out_store_status == duplicate`: `1063-1084`;
- duplicate branch releases the computed loser and bumps D-SUM-07 `duplicate_computed_but_discarded`: `1076-1084`;
- then it fetches the cached winner: `1087-1094`.

For these two callers, origin is in caller scope and can be passed down as a defaulted diagnostic parameter. This helper is a good site for:

- `frames_computed_and_stored` on `out_store_status == ok`;
- `bailed_after_compute_because_another_activation_stored_it_first` on `out_store_status == duplicate`.

### 5.2 Frame0 direct production store

Function: `cnr3_complete_live_frame0_fresh_start()`.

Relevant lines:

- direct `store_production_output_and_prune`: `2906-2915`;
- duplicate is accepted by `cnr3_live_store_status_allows_return()`: `2920-2921` and `789-793`;
- on accepted duplicate, the function does not enter the failure/release path at `2932-2955` and later returns the original `output_frame`.

This is a third major issue. For frame0 duplicate store, the current code appears to have a possible post-compute outcome that is neither `frames_computed_and_stored` nor `bailed_after_compute...discarded`: the computed frame was not inserted into the cache, but it is still returned to the caller. If concurrent frame0 duplicate activations are possible, the proposed self-check `computed == stored + discarded` is not exhaustive.

W3D should either:

- prove this frame0 duplicate path cannot occur in the intended live scheduler;
- accept a fourth event such as `computed_but_returned_after_duplicate_store`;
- or redefine the frame0 duplicate as stored/discarded by policy, despite the source not discarding it.

### 5.3 AS2 floor and hole stores

Functions:

- `store_as2_floor_and_prune()` call site: `cnr3_arAllFramesReady.cpp:1940-1950`;
- `store_recovery_hole_and_prune()` call site: `2290-2301`.

Relevant duplicate status lines:

- floor: `recovery_floor_outcome` set from `floor_store_summary.as2_summary.duplicate_existing_slot` at `1976-1979`;
- hole: `per_hole_outcomes[hole_index]` set from `hole_store_summary.as2_summary.duplicate_existing_slot` at `2334-2337`.

Cache core route:

- AS2 store family is chosen by `store_as2_floor_and_prune()` / `store_recovery_hole_and_prune()` setting `Cnr3CacheStoreKind::As2Consumer*`: `cnr3_cache_core.cpp:924-959` and `962-1022`.
- `store_owned_frame_and_prune_impl()` calls `store_owned_frame_and_record_pin_locked(..., Cnr3LookupCountPolicy::hit_only)` for AS2 stores: `1100-1109`.

These AS2 duplicates are real post-compute losers for this lifecycle diagnostic, but D-SUM-07 currently does **not** call `cnr3_diag_dsum07_observe_duplicate_computed_but_discarded()` on these paths. It only records D-SUM-07 `temporary_output_stored` before the AS2 store call at `1933-1937` and `2276-2280`.

Therefore `e_total == D-SUM-07 duplicate_computed_but_discarded` is not a valid structural cross-family tie if AS2 duplicate losers are included in `e_total`.

## 6. Store-family mapping to D-SUM-08

The exact mapping is:

- `store_production_output_and_prune()` records D-SUM-08 production store families:
  - frame0 fresh-start: `cnr3_arAllFramesReady.cpp:2906-2915`;
  - ordinary predecessor-present target: through helper `1528-1537` -> `949` -> cache core `887-921`;
  - recovery target: through helper `2555-2564` -> `949` -> cache core `887-921`.
- `store_as2_floor_and_prune()` records D-SUM-08 AS2 consumer store families:
  - floor fresh-start: `1940-1950` -> cache core `924-959`.
- `store_recovery_hole_and_prune()` records D-SUM-08 AS2 consumer store families:
  - recovery holes: `2290-2301` -> cache core `962-1022`.

The proposed three origins cross these store-family lines:

- `frame0_or_floor_fresh_start` contains both production-store frame0 and AS2-store floor.
- `ordinary_from_predecessor` maps cleanly to production stores.
- `recovery_hole_fill` contains AS2-store holes and production-store recovery target.

Therefore the proposed tie `f_ordinary + f_frame0 vs D-SUM-08 production stores, f_recovery vs D-SUM-08 AS2 consumer stores` is not correct as a general statement. It only works if floor and recovery-target cases are absent, which is not true in recovery runs.

To keep the three origin buckets, the D-SUM-08 cross-check must be weakened or supplemented. To keep the D-SUM-08 cross-check exact, the lifecycle counters need either:

- a second independent store-family split, or
- more granular origins such as `frame0_fresh_start`, `floor_fresh_start`, `ordinary_target`, `recovery_hole`, and `recovery_target`.

## 7. Adopt check proof

Confirmed: ordinary fast-predecessor and frame0 direct branches have no pre-compute adopt check.

The only before-compute adopt checks in live code are recovery-path checks:

- floor adopt: `cnr3_arAllFramesReady.cpp:1768-1777`;
- hole adopt: `cnr3_arAllFramesReady.cpp:2043-2053`.

However, because the origin definition includes floor fresh-start in the first bucket, floor adopt prevents the first bucket from being structurally zero unless W3D reclassifies it.

## 8. Once-per-compute proof and safest bump points

For ordinary, hole, and recovery-target pixel processing, the best `frames_computed` bump point is after the existing success check that requires both `status ok` and `summary.frame_processed == true`:

- ordinary: after `1463-1486`;
- recovery hole: after `2219-2243`;
- recovery target: after `2518-2542`.

For frame0 and floor fresh-start copy paths, if W3D wants these included in `frames_computed`, the best bump point is after successful frame adoption into a cache-owned frame or immediately before the store call, because there is no P.11 process summary:

- floor: after successful `floor_owned_frame.reset_to_owned_frame()` and before/near `1940-1950`;
- frame0: after successful `cache_owned_frame.reset_to_owned_frame()` and before/near `2906-2915`.

This preserves once-per-real-output-production and excludes early failures, but it will not equal D-SUM-07 `temporary_outputs_created` on paths where temporary creation succeeds and later processing/adoption fails.

## 9. Caller-map / selftest observations

The new counters can be kept out of selftest routes if implemented in the live `arAllFramesReady` sites plus live store helper calls with defaulted diagnostic parameters. Where a cache-core store helper is touched, the existing opt-in/default discipline from lookup-site breakdown should be reused:

- default origin/tag means no lifecycle bump;
- live callers pass an explicit origin;
- selftest callers inherit default/no bump.

This is feasible, but care is required because the cache core public store helpers are also used by selftests.

## 10. Required W3D rulings before patch

I recommend the designer answer these before implementation:

1. **Floor-adopt classification:** if site7a floor adopt hits, does `bailed_before_compute_since_already_in_cache` increment `frame0_or_floor_fresh_start`, `recovery_hole_fill`, or neither?
2. **Origin granularity:** is the three-bucket split still required despite crossing D-SUM-08 store families, or may we split into more granular origins to make store-family ties exact?
3. **D-SUM-07 tie:** should `e_total == D-SUM-07 duplicate_computed_but_discarded` be removed/replaced, since AS2 duplicates are not currently represented there?
4. **`temporary_outputs_created` tie:** should `b_total == D-SUM-07 temporary_outputs_created` be downgraded to a successful-run expectation only, or removed as a structural self-check?
5. **Frame0 duplicate-store outcome:** is the frame0 duplicate path impossible, or should the lifecycle summary include a fourth post-compute outcome for computed-but-returned-after-duplicate-store?
6. **Definition of computed:** should `frames_computed` include copy-only fresh-start outputs (frame0/floor), as the L1 oracle requires, even though they do not run the P.11 pixel-processing function?

After these rulings, I expect the patch itself to be straightforward.

## 11. Patch readiness

Current status: **not ready to patch v1 verbatim**.

Ready after designer ruling: yes, provided the event taxonomy and self-checks are revised to match the source facts above.
