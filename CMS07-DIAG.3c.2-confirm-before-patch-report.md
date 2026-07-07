# CMS07-DIAG.3c.2 confirm-before-patch report

Source inspected: uploaded post-DIAG.3c.1 `src(19).zip`.

## Verdict

Do not patch yet. The scope is safe to proceed to designer review as a confirm report, but not safe to implement without review because DIAG.3c.2 touches all proven bail sites. The live source re-derives the expected 65 `cnr3_set_filter_error` call sites exactly: 14 in `cnr3_arInitial.cpp`, 50 calls in `cnr3_arAllFramesReady.cpp` plus the shared helper definition, and 1 in `vapoursynth-Cnr3.cpp`.

## Mechanism recommendation

### M1: prefer per-site additive FAILED-record writer calls, not a `cnr3_set_filter_error()` category parameter

Recommended: mechanism (a), but with shared helper builders to avoid duplicated logic. Each bail site should add a gated call such as `cnr3_diag_plantrace_observe_failed_*()` before the existing `cnr3_set_filter_error(...)` and `return nullptr`.

Reason: a category-only parameter on `cnr3_set_filter_error()` is not enough to centralize the failed-record write. The helper currently receives only `frame_ctx`, `vsapi`, and `message`; it does not receive `Cnr3FilterData`, `n`, `frame_data`, `request_data`, the branch-local progress vectors/outcomes, or the computed E/X item. Passing all of that through the common helper would create a wider, more coupled signature than the per-site additive call and would still require branch-specific state assembly at the sites.

Per-site calls are more lines, but they keep the failure facts close to the only place where those facts are still known. This also avoids coupling the generic `setFilterError` wrapper to plantrace internals.

### M2: keep an explicit bail-arm dump

Recommended: add a once-guarded bail-arm dump. Do not rely on `cnr3_free_filter()` to run after a VapourSynth `setFilterError` bail. Source inspection cannot prove that teardown is reliable or timely after every error, and R-PROCESS-24 wants bytes flushed before the failing path returns. Use the same `buffer.dumped` once-guard as clean-end. If `cnr3_free_filter()` later runs, the clean-end dump observes `dumped=true` and emits nothing.

## Gate recommendation

Use the existing master plantrace compute/print gates only. Do not add a nested bail sub-gate unless the designer explicitly wants a 3c.1-success-only / 3c.2-bail-off diagnostic build. A nested gate would add 65 extra preprocessor decision points for limited value; master-off already provides the R-PROCESS-19 byte-identical exit gate.

## Reconstructability summary

- `arInitial` and top-level getFrame bail sites produce minimal FAILED records: frame identity when known, `fail_reason`, E on the known requested frame or relevant pre-publication item, no O record, no X list. The 3c.1 O guard only emits when `frame_data` is successfully published, so unpublished arInitial failures cannot be paired with an O record.
- `arAllFramesReady` bail sites normally have the O record and live `request_data` until the existing `cnr3_discard_frame_data_with_cache()` call. Therefore the failed-record write must happen before that discard, or the needed request/progress facts must be copied before the discard.
- The three discharge-failure sites are special: the failure is known only after calling `cnr3_discard_frame_data_with_cache()`, but that helper deletes `request_data`. For those sites, copy the needed plan/progress facts into local diagnostic-only values before discard, then write the FAILED record if `discard_status` is not OK.
- Recovery progress can be reconstructed from `recovery_floor_outcome` and `per_hole_outcomes`; target-store status must be added from the local branch variable where available before the FAILED write.
- X for recovery is plan remainder: planned hole frames not already C/K/L and not E. If failure occurs before the target, include later holes plus target where the target is part of the unreached plan. For target-stage failures, X is normally empty because holes are already reached.

## Category counts

| Category | Count |
|---|---:|
| ACQUIRE_REF_FAILED | 8 |
| ADOPT_FAILED | 3 |
| ALLOCATION_FAILED | 2 |
| BYTE_ESTIMATE_FAILED | 3 |
| COPYFRAME_FAILED | 5 |
| COPYFRAME_SOURCE_ALIAS | 5 |
| DISCHARGE_FAILED | 3 |
| FRAMEDATA_MISSING_OR_UNKNOWN | 4 |
| HOT_ZONE_OBSERVATION_FAILED | 1 |
| INVALID_BRANCH_FOUNDATION | 1 |
| INVALID_LIFECYCLE | 8 |
| RECOVERY_PLAN_FAILED_OR_REFUSED | 4 |
| SCENE_PROCESSING_FAILED | 3 |
| SOURCE_NOT_REQUESTED | 4 |
| SOURCE_RETRIEVAL_FAILED | 6 |
| STORE_PRUNE_FAILED | 5 |

Note: `AI-06` is a mixed runtime-cause source location: recovery refusal is category 15, but discard failure at the same site is category 8. The implementation should split that site with a local category variable or two adjacent gated FAILED-record calls, not parse the message text.

## Site-to-category table

| ID | File:line | Function/branch | Message summary | Category | E/X/progress note |
|---|---|---|---|---|---|
| AI-01 | `cnr3_arInitial.cpp:172` | publish cache-hit | invalid cache-hit frameData publication | 9 INVALID_LIFECYCLE | minimal arInitial failure; frame n known; no O published; E=n, X=[] |
| AI-02 | `cnr3_arInitial.cpp:219` | publish predecessor-present | invalid predecessor-present pinned start | 9 INVALID_LIFECYCLE | minimal arInitial failure; frame n known; no O published; E=n or predecessor depending final convention; X=[] |
| AI-03 | `cnr3_arInitial.cpp:255` | publish frame0 | invalid frame-0 frameData publication | 9 INVALID_LIFECYCLE | minimal arInitial failure; frame n known; no O published; E=n, X=[] |
| AI-04 | `cnr3_arInitial.cpp:565` | start recovery | invalid recovery frameData start | 9 INVALID_LIFECYCLE | minimal arInitial failure; frame n known; no O published; E=n, X=[] |
| AI-05 | `cnr3_arInitial.cpp:593` | start recovery | bounded recovery plan failed | 15 RECOVERY_PLAN_FAILED_OR_REFUSED | minimal arInitial failure; plan not published; E=n, X=[] |
| AI-06 | `cnr3_arInitial.cpp:632` | start recovery | recovery refusal OR refusal pin-discharge failure | 15 RECOVERY_PLAN_FAILED_OR_REFUSED | MIXED site: normal category 15; if discard_status fails category 8 is more exact. Recommend split by local category variable before failed-record write. |
| AI-07 | `cnr3_arInitial.cpp:655` | start recovery | failed to derive floor-start holes | 15 RECOVERY_PLAN_FAILED_OR_REFUSED | minimal arInitial failure; recovery request_data unpublished; E=n/floor if available, X=[] |
| AI-08 | `cnr3_arInitial.cpp:669` | start recovery | failed to derive recovery source request set | 15 RECOVERY_PLAN_FAILED_OR_REFUSED | minimal arInitial failure; recovery request_data unpublished; E=n, X=[] |
| AI-09 | `cnr3_arInitial.cpp:685` | start recovery | failed to allocate per-hole outcome state | 14 ALLOCATION_FAILED | minimal arInitial failure; recovery request_data unpublished; E=n, X=[] |
| AI-10 | `cnr3_arInitial.cpp:748` | arInitial top | frameData unexpectedly non-null at arInitial | 9 INVALID_LIFECYCLE | minimal arInitial failure; no request_data allocated; E=n, X=[] |
| AI-11 | `cnr3_arInitial.cpp:767` | arInitial top | failed to allocate frameData | 14 ALLOCATION_FAILED | minimal arInitial failure; no request_data; E=n, X=[] |
| AI-12 | `cnr3_arInitial.cpp:786` | arInitial top | hot-zone observation failed | 16 HOT_ZONE_OBSERVATION_FAILED | minimal arInitial failure; request_data unpublished and then deleted; E=n, X=[] |
| AI-13 | `cnr3_arInitial.cpp:814` | arInitial cache-hit route | failed during cache-hit pin attempt | 5 ACQUIRE_REF_FAILED | minimal arInitial failure; cache lookup/pin failed; E=n, X=[] |
| AI-14 | `cnr3_arInitial.cpp:861` | arInitial predecessor route | failed during predecessor pin attempt | 5 ACQUIRE_REF_FAILED | minimal arInitial failure; predecessor lookup/pin failed; E=n-1 preferred; X=[] |
| AR-01 | `cnr3_arAllFramesReady.cpp:1058` | cache_hit | invalid frameData cache-hit lifecycle | 9 INVALID_LIFECYCLE | request_data alive before discard; progress=[]; E=n; X=[] |
| AR-02 | `cnr3_arAllFramesReady.cpp:1073` | cache_hit | triggering source frame retrieval failed | 3 SOURCE_RETRIEVAL_FAILED | request_data alive; source trigger only; progress=[]; E=n; X=[] |
| AR-03 | `cnr3_arAllFramesReady.cpp:1108` | cache_hit | pinned cached output[N] was not retrievable | 13 FRAMEDATA_MISSING_OR_UNKNOWN | request_data alive; progress=[]; E=n; X=[] |
| AR-04 | `cnr3_arAllFramesReady.cpp:1144` | cache_hit | failed to discharge cache-hit pin-list | 8 DISCHARGE_FAILED | copy needed facts before discard; if discard fails write FAILED with progress=[]; E=n; X=[] |
| AR-05 | `cnr3_arAllFramesReady.cpp:1192` | pred_present | invalid frameData predecessor/source lifecycle | 9 INVALID_LIFECYCLE | request_data alive before discard; progress=[]; E=n; X=[] |
| AR-06 | `cnr3_arAllFramesReady.cpp:1207` | pred_present | source frame retrieval failed | 3 SOURCE_RETRIEVAL_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-07 | `cnr3_arAllFramesReady.cpp:1229` | pred_present | failed to acquire predecessor compute reference | 5 ACQUIRE_REF_FAILED | request_data alive; progress=[]; E=predecessor_frame; X=[] |
| AR-08 | `cnr3_arAllFramesReady.cpp:1246` | pred_present | copyFrame failed | 1 COPYFRAME_FAILED | request_data alive; source retrieved; progress=[]; E=n; X=[] |
| AR-09 | `cnr3_arAllFramesReady.cpp:1261` | pred_present | copyFrame returned source alias | 2 COPYFRAME_SOURCE_ALIAS | request_data alive; progress=[]; E=n; X=[] |
| AR-10 | `cnr3_arAllFramesReady.cpp:1304` | pred_present | predecessor-present scene processing failed | 11 SCENE_PROCESSING_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-11 | `cnr3_arAllFramesReady.cpp:1331` | pred_present | failed to compute live output byte estimate | 12 BYTE_ESTIMATE_FAILED | request_data alive; output computed but not stored; progress=[]; E=n; X=[] |
| AR-12 | `cnr3_arAllFramesReady.cpp:1367` | pred_present | failed to store/return authoritative output[N] | 7 STORE_PRUNE_FAILED | request_data alive; store_status available; progress may include C/L if store happened but no return; E=n; X=[] |
| AR-13 | `cnr3_arAllFramesReady.cpp:1398` | pred_present | failed to discharge predecessor pin-list | 8 DISCHARGE_FAILED | copy result facts before discard; E=n; X=[] |
| AR-14 | `cnr3_arAllFramesReady.cpp:1452` | recovery | invalid recovery frameData lifecycle | 9 INVALID_LIFECYCLE | request_data alive before discard; plan may exist; progress=[]; E=n; X=derived only if plan valid |
| AR-15 | `cnr3_arAllFramesReady.cpp:1479` | recovery | invalid recovery branch foundation | 10 INVALID_BRANCH_FOUNDATION | request_data alive; progress=[]; E=n/floor depending branch; X=plan remainder if valid |
| AR-16 | `cnr3_arAllFramesReady.cpp:1493` | recovery | failed to compute recovery output byte estimate | 12 BYTE_ESTIMATE_FAILED | request_data alive; progress=[]; E=n; X=plan remainder if valid |
| AR-17 | `cnr3_arAllFramesReady.cpp:1520` | recovery floor | floor source was not requested at arInitial | 4 SOURCE_NOT_REQUESTED | request_data alive; progress=[]; E=floor; X=holes+target |
| AR-18 | `cnr3_arAllFramesReady.cpp:1539` | recovery floor | pre-compute floor adopt-and-skip lookup failed | 5 ACQUIRE_REF_FAILED | request_data alive; progress=[]; E=floor; X=holes+target |
| AR-19 | `cnr3_arAllFramesReady.cpp:1566` | recovery floor | floor source frame retrieval failed | 3 SOURCE_RETRIEVAL_FAILED | request_data alive; progress=[]; E=floor; X=holes+target |
| AR-20 | `cnr3_arAllFramesReady.cpp:1585` | recovery floor | floor copyFrame failed | 1 COPYFRAME_FAILED | request_data alive; progress=[]; E=floor; X=holes+target |
| AR-21 | `cnr3_arAllFramesReady.cpp:1602` | recovery floor | floor copyFrame returned source alias | 2 COPYFRAME_SOURCE_ALIAS | request_data alive; progress=[]; E=floor; X=holes+target |
| AR-22 | `cnr3_arAllFramesReady.cpp:1638` | recovery floor | failed to adopt floor fresh-start output | 6 ADOPT_FAILED | request_data alive; progress=[]; E=floor; X=holes+target |
| AR-23 | `cnr3_arAllFramesReady.cpp:1672` | recovery floor | failed to store/pin/prune floor fresh-start output | 7 STORE_PRUNE_FAILED | request_data alive; progress may include floor C/K/L if status set; E=floor; X=holes+target |
| AR-24 | `cnr3_arAllFramesReady.cpp:1730` | recovery hole loop | hole source was not requested at arInitial | 4 SOURCE_NOT_REQUESTED | request_data alive; progress from prior per_hole_outcomes/floor; E=current hole; X=later holes+target |
| AR-25 | `cnr3_arAllFramesReady.cpp:1751` | recovery hole loop | pre-compute hole adopt-and-skip lookup failed | 5 ACQUIRE_REF_FAILED | request_data alive; progress from prior outcomes; E=current hole; X=later holes+target |
| AR-26 | `cnr3_arAllFramesReady.cpp:1769` | recovery hole loop | failed to acquire hole predecessor compute reference | 5 ACQUIRE_REF_FAILED | request_data alive; progress from prior outcomes; E=current hole predecessor or hole (needs final convention); X=current/later holes+target if E=pred |
| AR-27 | `cnr3_arAllFramesReady.cpp:1794` | recovery hole loop | hole source frame retrieval failed | 3 SOURCE_RETRIEVAL_FAILED | request_data alive; progress from prior outcomes; E=current hole; X=later holes+target |
| AR-28 | `cnr3_arAllFramesReady.cpp:1815` | recovery hole loop | hole copyFrame failed | 1 COPYFRAME_FAILED | request_data alive; progress from prior outcomes; E=current hole; X=later holes+target |
| AR-29 | `cnr3_arAllFramesReady.cpp:1830` | recovery hole loop | hole copyFrame returned source alias | 2 COPYFRAME_SOURCE_ALIAS | request_data alive; progress from prior outcomes; E=current hole; X=later holes+target |
| AR-30 | `cnr3_arAllFramesReady.cpp:1874` | recovery hole loop | P.11C hole processing failed | 11 SCENE_PROCESSING_FAILED | request_data alive; progress from prior outcomes; E=current hole; X=later holes+target |
| AR-31 | `cnr3_arAllFramesReady.cpp:1896` | recovery hole loop | failed to adopt computed hole output | 6 ADOPT_FAILED | request_data alive; progress from prior outcomes; E=current hole; X=later holes+target |
| AR-32 | `cnr3_arAllFramesReady.cpp:1945` | recovery hole loop | failed to store/pin/prune computed recovery hole | 7 STORE_PRUNE_FAILED | request_data alive; progress from prior outcomes plus current status if recorded; E=current hole; X=later holes+target |
| AR-33 | `cnr3_arAllFramesReady.cpp:1988` | recovery target | target source was not requested at arInitial | 4 SOURCE_NOT_REQUESTED | request_data alive; progress from floor/per_hole_outcomes; E=target n; X=[] |
| AR-34 | `cnr3_arAllFramesReady.cpp:2007` | recovery target | failed to acquire target predecessor compute reference | 5 ACQUIRE_REF_FAILED | request_data alive; progress from floor/per_hole_outcomes; E=predecessor or target (needs final convention); X=[] |
| AR-35 | `cnr3_arAllFramesReady.cpp:2023` | recovery target | target source frame retrieval failed | 3 SOURCE_RETRIEVAL_FAILED | request_data alive; progress from floor/per_hole_outcomes; E=target n; X=[] |
| AR-36 | `cnr3_arAllFramesReady.cpp:2040` | recovery target | target copyFrame failed | 1 COPYFRAME_FAILED | request_data alive; progress from floor/per_hole_outcomes; E=target n; X=[] |
| AR-37 | `cnr3_arAllFramesReady.cpp:2055` | recovery target | target copyFrame returned source alias | 2 COPYFRAME_SOURCE_ALIAS | request_data alive; progress from floor/per_hole_outcomes; E=target n; X=[] |
| AR-38 | `cnr3_arAllFramesReady.cpp:2099` | recovery target | P.11C target processing failed | 11 SCENE_PROCESSING_FAILED | request_data alive; progress from floor/per_hole_outcomes; E=target n; X=[] |
| AR-39 | `cnr3_arAllFramesReady.cpp:2142` | recovery target | failed to store/return authoritative target output | 7 STORE_PRUNE_FAILED | request_data alive; progress from floor/per_hole_outcomes plus target status if available; E=target n; X=[] |
| AR-40 | `cnr3_arAllFramesReady.cpp:2206` | recovery | failed to discharge recovery pin-list | 8 DISCHARGE_FAILED | copy progress before discard; E=target n; X=[] |
| AR-41 | `cnr3_arAllFramesReady.cpp:2262` | frame0 | source frame was not requested in this activation | 4 SOURCE_NOT_REQUESTED | request_data alive before discard; progress=[]; E=n; X=[] |
| AR-42 | `cnr3_arAllFramesReady.cpp:2277` | frame0 | source frame retrieval failed | 3 SOURCE_RETRIEVAL_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-43 | `cnr3_arAllFramesReady.cpp:2293` | frame0 | copyFrame failed | 1 COPYFRAME_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-44 | `cnr3_arAllFramesReady.cpp:2307` | frame0 | copyFrame returned source alias | 2 COPYFRAME_SOURCE_ALIAS | request_data alive; progress=[]; E=n; X=[] |
| AR-45 | `cnr3_arAllFramesReady.cpp:2336` | frame0 | failed to add cache frame reference | 5 ACQUIRE_REF_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-46 | `cnr3_arAllFramesReady.cpp:2359` | frame0 | failed to adopt cache frame reference | 6 ADOPT_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-47 | `cnr3_arAllFramesReady.cpp:2379` | frame0 | failed to compute frame-0 output byte estimate | 12 BYTE_ESTIMATE_FAILED | request_data alive; progress=[]; E=n; X=[] |
| AR-48 | `cnr3_arAllFramesReady.cpp:2421` | frame0 | failed to store/prune output[0] checkpoint | 7 STORE_PRUNE_FAILED | request_data alive; progress may include store status if available; E=n; X=[] |
| AR-49 | `cnr3_arAllFramesReady.cpp:2503` | arAll top | missing frameData at arAllFramesReady | 13 FRAMEDATA_MISSING_OR_UNKNOWN | no request_data; minimal FAILED record; E=n; X=[] |
| AR-50 | `cnr3_arAllFramesReady.cpp:2560` | arAll dispatch | unknown frameData branch at arAllFramesReady | 13 FRAMEDATA_MISSING_OR_UNKNOWN | request_data alive before discard but branch unknown; minimal FAILED record; E=n; X=[] |
| TOP-01 | `vapoursynth-Cnr3.cpp:251` | top-level getFrame | invalid getFrame state | 13 FRAMEDATA_MISSING_OR_UNKNOWN | data/source/frame_data/core/vsapi invalid; minimal/no instance if data null; E=n if possible; X=[] |

## Proposed proof method

1. Add selftest/fixture paths that induce one arInitial bail and one arAllFramesReady bail without changing production success paths. Preferred proof triggers:
   - arInitial: call `cnr3_arInitial()` with a deliberately non-null `frame_data` slot to hit the `frameData was unexpectedly non-null at arInitial` site.
   - arAllFramesReady: call the frame0 or predecessor branch with a stub VSAPI where `getFrameFilter()` returns null, hitting a SOURCE_RETRIEVAL_FAILED site with real request_data and an existing O record.
2. Verify the bail path writes a FAILED R record with `outcome=FAILED`, correct `fail_reason`, E, X as applicable, and per-line flush.
3. Verify the bail dump emits BEGIN/legend/records/END before the failing function returns, and a later clean-end dump does not duplicate it.
4. R-PROCESS-19: master gate OFF compiles out all 3c.2 additions and leaves the 65 sites as the original `cnr3_set_filter_error(...)` + `return nullptr` pattern.
5. Clean-run S1/S7/S8 must remain unchanged from DIAG.3c.1: no FAILED records, no `fail_reason`, no X/E.

## Expected touched files

Likely required:

```text
cnr3_arInitial.cpp
cnr3_arAllFramesReady.cpp
vapoursynth-Cnr3.cpp
cnr3_diagnostics.h
cnr3_diagnostics.cpp
cnr3_plugin_internal.h
cnr3_cache_core_selftest_main.cpp
cnr3_build_config.h   (marker only unless designer requests a nested bail sub-gate)
```

Not expected: cache core files, project files, pin-list accessor, production `Cnr3LiveRecoveryHoleOutcome` expansion.

## Items for designer decision before patch

1. Confirm mechanism choice M1(a): per-site additive FAILED writer + shared builders, not `cnr3_set_filter_error()` centralization.
2. Confirm M2: explicit once-guarded bail-arm dump before return, not clean-end-only.
3. Confirm no nested bail sub-gate.
4. Confirm E convention for acquire-ref failures that occur on predecessor/base frames: use the predecessor/base frame as E, or force E to the requested output frame for non-recovery display simplicity.
5. Confirm `AI-06` mixed category handling: runtime local category variable selecting 15 vs 8 is acceptable despite one source location.
