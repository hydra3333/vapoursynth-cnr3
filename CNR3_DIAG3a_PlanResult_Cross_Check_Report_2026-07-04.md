# CNR3 DIAG.3a separate plan/result vocabulary cross-check report

**Role:** separate coder-side review of `CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v1.md`.  
**Implementation status:** review only; no plan-trace implementation in DIAG.3a.  
**Source inspected:** latest coordinator source upload `src(17).zip`.

## 0. Executive result

The plan/result diagnostic spec is directionally sound and aligns with the current getFrame branch model. However, it should remain separate from DIAG.3a patching. The main gaps are:

1. O-item `sources` cannot be derived solely from `source_request_frame_numbers`, because cache-hit, frame0, and predecessor-present branches request source frame `n` without populating that vector.
2. O/R `pinned`/`unpinned` per-frame lists are semantically derivable from branch facts and recovery outcomes, but the pin-list internals are private; do not require a generic pin-list enumerator unless designer explicitly wants a new accessor.
3. Failure categories mostly fit the current messages, but the current source has more than “~50” setFilterError sites when including arInitial and top-level getFrame. The plan-trace implementation will need a mechanical per-site reason mapping, not a loose message parser.
4. Dump-on-bail is the only truly invasive part. It is additive, but it touches many proven `return nullptr` sites and should remain a future patch with its own R-PROCESS-21 review.

## 1. Set 1 — O-frame-level strategy

Spec Set 1 matches current source.

Current strategy state:

- `Cnr3LiveGetFrameBranch`: `none`, `cache_hit_return`, `frame0_fresh_start`, `predecessor_present_compute`, `recovery` at `src/cnr3_plugin_internal.h:50-56`.
- `Cnr3LiveRecoveryBranch`: `none`, `exact_anchor`, `floor_fresh_start` at `src/cnr3_plugin_internal.h:58-62`.

Mapping is correct:

- `CACHE_HIT` = `cache_hit_return`
- `FRAME0` = `frame0_fresh_start`
- `PRED_PRESENT` = `predecessor_present_compute`
- `RECOVERY_EXACT` = `recovery + exact_anchor`
- `RECOVERY_FLOOR` = `recovery + floor_fresh_start`
- `NONE` remains guard/uninitialised only

## 2. Set 2 — O-item-level roles

The role list is mostly correct but needs one clarification.

### 2.1 target / predecessor / anchor / floor / holes

These roles are source-grounded:

- target = requested `n`, held in `request_data->requested_frame` (`src/cnr3_plugin_internal.h:73`).
- predecessor = `request_data->predecessor_frame` (`src/cnr3_plugin_internal.h:74`) for predecessor-present and recovery target foundation.
- anchor = `recovery_plan.anchor_frame_number` (`src/cnr3_cache_core.h:566`) for exact recovery.
- floor = `recovery_floor_frame` (`src/cnr3_plugin_internal.h:80`) for floor fresh-start.
- holes = `recovery_plan.hole_frame_numbers` (`src/cnr3_cache_core.h:571`).

### 2.2 sources role needs branch-specific derivation

The spec currently points to `source_request_frame_numbers`. That is correct for recovery, but incomplete for non-recovery branches.

Source facts:

- Cache-hit arInitial requests source `n` as an arAllFramesReady trigger at `src/cnr3_arInitial.cpp:53-64`, but does not populate `source_request_frame_numbers`.
- Frame0 arInitial requests source `n` at `src/cnr3_arInitial.cpp:118-123`, but does not populate `source_request_frame_numbers`.
- Predecessor-present arInitial requests source `n` at `src/cnr3_arInitial.cpp:91-97`, but does not populate `source_request_frame_numbers`.
- Recovery arInitial derives `source_request_frame_numbers` in `cnr3_fill_recovery_source_request_numbers()` at `src/cnr3_arInitial.cpp:262-301`.

Recommendation: define O-item `sources` as:

```text
if branch in CACHE_HIT / FRAME0 / PRED_PRESENT:
    sources = [n]
else if branch in RECOVERY_EXACT / RECOVERY_FLOOR:
    sources = source_request_frame_numbers
```

This preserves VS-LIFECYCLE-01 visibility for every branch.

### 2.3 pinned role should be derived, not generic-pin-list-enumerated

`Cnr3CachePinList` records private `Cnr3CacheSlotPinToken` entries. The token includes `frame_number` (`src/cnr3_cache_core.h:296-299`), but the vector is private (`src/cnr3_cache_core.h:1855-1856`) and only `pin_count()` is public (`src/cnr3_cache_core.h:1794-1796`).

Recommendation: plan-trace should not require a generic pin-list enumerator unless designer explicitly chooses that future surface. For O/R trace, derive pinned frames from branch facts:

- Cache-hit pinned frame = target `n`.
- Predecessor-present pinned frame = `n - 1`.
- Recovery exact initial pin = anchor frame.
- Recovery floor/holes pins are result-time facts from floor/hole store/adopt outcomes.

## 3. Set 3 — R-frame-level outcome

Spec Set 3 is sound.

Source returns:

- Cache-hit direct return: `cnr3_get_frame_live_cache_hit_return()` returns transferred cache ref at `src/cnr3_arAllFramesReady.cpp:716-797`.
- Frame0 returns copied/stored output at `src/cnr3_arAllFramesReady.cpp:1602-1759`.
- Pred-present returns computed output or cached duplicate winner at `src/cnr3_arAllFramesReady.cpp:799-988`.
- Recovery returns recovered target output or cached duplicate winner at `src/cnr3_arAllFramesReady.cpp:990-1600`.
- Failures are existing `cnr3_set_filter_error(...)` + `return nullptr` sites.

`RETURNED_COMPUTED` should cover frame0 and predecessor-present. `RETURNED_RECOVERED` should cover recovery exact/floor target return.

## 4. Set 4 — R-item-level outcomes

The source enum anchors are correct:

- `Cnr3LiveRecoveryHoleOutcome::computed`
- `adopted_skipped`
- `adopted_post_compute_loser`
- `none`

These are declared at `src/cnr3_plugin_internal.h:64-69` and stringified at `src/cnr3_arAllFramesReady.cpp:172-186`.

`X not_reached` and `E error_here` are not current source enums. They require bail-site writes exactly as the spec says. That is feasible, but it is not DIAG.3a work.

Recommendation: when plan-trace is implemented, keep `X/E` local to the plan-trace diagnostic result record, not as production enum values in `Cnr3LiveRecoveryHoleOutcome`, unless designer explicitly wants the live outcome enum expanded.

## 5. Set 5 — failure reason categories

The 13 categories cover the current messages reasonably well, but the implementation should use explicit per-site categories, not string parsing.

Observed setFilterError call count in current source:

- `cnr3_arInitial.cpp`: 14 sites.
- `cnr3_arAllFramesReady.cpp`: 51 sites plus the helper function definition.
- `vapoursynth-Cnr3.cpp`: 1 top-level getFrame state site.

The categories broadly fit:

- COPYFRAME_FAILED: floor/hole/target/frame0/pred copyFrame null.
- COPYFRAME_SOURCE_ALIAS: source alias guards.
- SOURCE_RETRIEVAL_FAILED: getFrameFilter returns null.
- SOURCE_NOT_REQUESTED: source-request lifecycle guards.
- ACQUIRE_REF_FAILED: predecessor/target lookup addref failures.
- ADOPT_FAILED: owned-frame adoption failures.
- STORE_PRUNE_FAILED: production/AS2 store/prune failures.
- DISCHARGE_FAILED: pin-list discharge failures.
- INVALID_LIFECYCLE: frameData branch/lifecycle mismatch.
- INVALID_BRANCH_FOUNDATION: recovery foundation mismatch.
- SCENE_PROCESSING_FAILED: P.11C failures.
- BYTE_ESTIMATE_FAILED: W.3 byte estimate failures.
- FRAMEDATA_MISSING_OR_UNKNOWN: missing/unknown branch/top-level invalid state.

Potential additions or clarifications before implementation:

- `ALLOCATION_FAILED` may be useful for arInitial frameData allocation and per-hole outcome allocation sites (`src/cnr3_arInitial.cpp:488-497`, `src/cnr3_arInitial.cpp:434-448`). These can be folded into INVALID_LIFECYCLE/FRAMEDATA_MISSING_OR_UNKNOWN, but an explicit category would be clearer.
- `RECOVERY_PLAN_FAILED_OR_REFUSED` may be useful for `bounded recovery plan failed` and recovery refusal (`src/cnr3_arInitial.cpp:348-397`). These do not fit neatly into the current 13 unless treated as INVALID_BRANCH_FOUNDATION or STORE/PRUNE, neither of which is exact.
- `HOT_ZONE_OBSERVATION_FAILED` exists at `src/cnr3_arInitial.cpp:500-517`; it could be FRAMEDATA_MISSING_OR_UNKNOWN or a new policy/diagnostic category.

Recommendation: keep the current 13 as draft, but before plan-trace patching add a site-to-category table and consider the three clarifications above.

## 6. Buffer / mutex / dump architecture

The architecture is sound for a future plan-trace patch:

- Per-instance buffer with diagnostics-only mutex matches D-SUM-01 discipline.
- Window-bounded preallocation avoids ring/saturation complexity.
- Pairing by frame and phase is appropriate for fmUnordered/fmParallel.
- Dump-on-bail is necessary if failure traces are required, because end-of-run only may lose the failure case.

Implementation caution: dump-on-bail means many bail sites must perform an additive diagnostic write before existing `return nullptr`. This is not DIAG.3a and should get a dedicated patch/review.

## 7. from/to compile-time vs runtime

Recommendation for first implementation: compile-time gates first.

Reasoning:

- Runtime `.vpy` parameters would change the plugin API surface and broaden the patch beyond observe-only internals.
- Compile-time `from/to` mirrors the existing diagnostic gate style and is easier to macro-off prove.
- Runtime parameters can be added later once the diagnostic semantics are proven.

## 8. Cross-check verdict

Plan/result spec is viable, with the following amendments recommended before implementation:

1. Define O-item `sources` branch-specifically; do not rely solely on `source_request_frame_numbers`.
2. Treat `pinned/unpinned` per-frame lists as derived diagnostic facts unless/until a pin-list enumerator is explicitly approved.
3. Create a source-line site-to-failure-category table before touching bail paths.
4. Consider adding `ALLOCATION_FAILED`, `RECOVERY_PLAN_FAILED_OR_REFUSED`, and `HOT_ZONE_OBSERVATION_FAILED`, or explicitly map them into the existing 13 categories.
5. Use compile-time from/to for the first implementation unless coordinator/designer decides to broaden plugin API.

No plan-trace implementation should be part of DIAG.3a.
