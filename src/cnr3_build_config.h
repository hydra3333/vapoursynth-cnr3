#pragma once

/*
    CNR3 build configuration and compile-time gates.

    CMS07-B.1 introduces central diagnostic gate definitions only.

    This file must not include VapourSynth headers and must not introduce cache
    behaviour, getFrame wiring, pixel processing, diagnostics counters, printing,
    or old-code salvage.

    Diagnostic observation gates observe only. They must not affect correct
    program behaviour or output frames.

    A behaviour-changing scaffold is not a diagnostic. It must use the project's
    behavioural-scaffold rules and must not use a CNR3_DIAG_* name.
*/

/*
    Edit/version marker.

    This string is for human diagnostics and build identification only. It must
    not be used for control flow.
*/
inline constexpr const char* CNR3_EDIT_VERSION = "CMS07-P.5A-signed-difference-table-lookup-blend-proof";

/*
    Temporary proof scaffold convention.

    CMS07-B.1 does not introduce any active CNR3_SCAFFOLD_* gate.

    Later phases may introduce a named CNR3_SCAFFOLD_* gate only when that
    phase explicitly needs temporary proof behaviour. The scaffold block must
    be visibly bounded in source comments, must state the owning phase, and
    must not be required for production correctness.
*/

// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-01
//  Name: Frame request arrival / ordering summary
//  Purpose: Shows observed request ordering so a human can see sequential, mostly sequential,
//           or strongly out-of-order arrival, including jumps, duplicates, and stress coverage.
//  Activation condition: Add when request-arrival observation exists; arInitial in full
//                        VapourSynth integration, or a synthetic driver in cache-core tests.
//  Likely collection: Collect requested frame number at request-arrival time and compare with
//                     previous request for the same instance; optionally keep bounded samples.
//  Field definitions: arInitial_count, arAllFramesReady_count if available,
//                     first_requested_frame, last_requested_frame, monotonic_forward_count,
//                     same_frame_or_duplicate_count, backward_jump_count, forward_jump_count,
//                     max_forward_jump, max_backward_jump, arrival_gap_histogram,
//                     out_of_order_count.
//  Human interpretation: Out-of-order arrivals are INFO under intentional stress, WARN if
//                        sequential order was expected, and not failures by themselves;
//                        impossible accounting is failure evidence.
#define CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER 1
#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
#   define CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER) && !defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
#   error "Cannot print DSUM01_REQUEST_ORDER without computing DSUM01_REQUEST_ORDER"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-02
//  Name: Memory diagnostics summary
//  Purpose: Shows process/system memory movement to help detect leaks, runaway cache growth,
//           failure to release after cleanup, and excessive memory pressure.
//  Activation condition: Add when memory sampling exists; likely implemented independently in
//                        the memory diagnostics utility source.
//  Likely collection: Take baseline, periodic in-run, pre-cleanup, post-cleanup, and final
//                     samples where available, obeying stderr and no-lock-printing rules.
//  Field definitions: process_working_set, process_private_usage, system_avail_phys,
//                     system_used_phys, commit_total, peak_working_set, peak_private_usage,
//                     Min/Avg/Max, Min->Max percent.
//  Human interpretation: process_private_usage is usually the best leak-suspicion indicator;
//                        working_set and system metrics are interpretive; persistent
//                        post-cleanup elevation matters more than normal in-run growth.
#define CNR3_DIAG_COMPUTE_DSUM02_MEMORY 1
#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
#   define CNR3_DIAG_PRINT_DSUM02_MEMORY 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM02_MEMORY) && !defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
#   error "Cannot print DSUM02_MEMORY without computing DSUM02_MEMORY"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-03
//  Name: Recovery-search summary
//  Purpose: Shows how recovery searched for a usable existing predecessor/start output,
//           including search depth and stop reason.
//  Activation condition: Add when CMS07 recovery-search logic exists.
//  Likely collection: Count near recovery-search decisions; perform summary formatting and
//                     printing outside locks.
//  Field definitions: search_attempts, search_successes, search_failures, depth_histogram,
//                     terminated_on_present_output, terminated_on_frame0,
//                     terminated_on_bound, terminated_on_failure, holes_filled.
//  Human interpretation: Deep search is not automatically bad; repeated deep search may
//                        indicate retention, prune, or workload pressure; denominator
//                        mismatches or bounded-start honesty failures are serious.
#define CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH 1
#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
#   define CNR3_DIAG_PRINT_DSUM03_RECOVERY_SEARCH 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM03_RECOVERY_SEARCH) && !defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
#   error "Cannot print DSUM03_RECOVERY_SEARCH without computing DSUM03_RECOVERY_SEARCH"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-04
//  Name: Ownership / pin / lookup-ref balance summary
//  Purpose: Shows whether ownership-sensitive mechanisms balanced, catching leaks, missing
//           releases, missing transfers, double-release symptoms, and pin-list discharge bugs.
//  Activation condition: Add as soon as ownership, pin, or lookup-reference machinery exists.
//  Likely collection: Count at pin/unpin, lookup addref/release/transfer, pin-list
//                     record/discharge, and ownership handoff points.
//  Field definitions: pins_acquired, pins_released, pin_balance, lookup_refs_acquired,
//                     lookup_refs_released, lookup_refs_transferred, lookup_ref_balance,
//                     pin_list_records, pin_list_discharges, pin_list_balance,
//                     ownership_errors.
//  Human interpretation: pin_balance and lookup_ref_balance must be zero after drain;
//                        acquired == released + transferred is the lookup-ref invariant.
#define CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE 1
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
#   define CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE) && !defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
#   error "Cannot print DSUM04_OWNERSHIP_BALANCE without computing DSUM04_OWNERSHIP_BALANCE"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-05
//  Name: Cache integrity / teardown summary
//  Purpose: Shows final cache state and cleanup result, including released ownership,
//           stale index entries, invalid slot state, pinned shutdown state, and ref errors.
//  Activation condition: Add when cache data structures and clear/shutdown logic exist.
//  Likely collection: Validate during cache integrity checks and teardown; measure final slot
//                     state before detach/clear and print after cleanup completes.
//  Field definitions: non_checkpoint_count, checkpoint_count, total_cached_frame_count,
//                     total_pin_count, has_pinned_checkpoints, invariants_ok,
//                     integrity_errors, validation_failures, ref_balance_errors,
//                     clear_successes, clear_failures.
//  Human interpretation: Non-zero cached count before cleanup may be normal; non-zero pins
//                        after drain, integrity errors, validation failures, ref balance
//                        errors, or clear failures are correctness failures.
#define CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY 1
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
#   define CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY) && !defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
#   error "Cannot print DSUM05_CACHE_INTEGRITY without computing DSUM05_CACHE_INTEGRITY"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-06
//  Name: Source-frame request / retrieve / release summary
//  Purpose: Shows source-frame lifecycle compliance: source frames retrieved upstream were
//           requested for the same activation and released exactly once.
//  Activation condition: Add when source-frame request/retrieve/release handling exists.
//  Likely collection: Count source requests at planning time, retrievals when acquired, and
//                     releases when owned source frames are released.
//  Field definitions: source_frames_requested_total, source_frames_retrieved_total,
//                     source_frames_released_total, source_frame_release_balance,
//                     same_activation_request_violations, source_frame_count_max,
//                     partial_acquire_failures, source_frame_release_balance_errors.
//  Human interpretation: Retrieved and released counts should balance; retrieve without
//                        same-activation request is a lifecycle violation; partial acquire
//                        failure must be inspected even when cleanup is clean.
#define CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE 1
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
#   define CNR3_DIAG_PRINT_DSUM06_SOURCE_FRAME_LIFECYCLE 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM06_SOURCE_FRAME_LIFECYCLE) && !defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
#   error "Cannot print DSUM06_SOURCE_FRAME_LIFECYCLE without computing DSUM06_SOURCE_FRAME_LIFECYCLE"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-07
//  Name: Temporary-output / owned-output-ref lifecycle summary
//  Purpose: Shows handling of newly computed or temporary output references before they are
//           stored, discarded, released, or transferred.
//  Activation condition: Add when temporary output creation exists.
//  Likely collection: Count at output creation, store attempt, store success/failure,
//                     duplicate discard, release, and transfer.
//  Field definitions: temporary_outputs_created, temporary_outputs_stored,
//                     temporary_outputs_released, temporary_outputs_transferred,
//                     temporary_output_balance, caller_still_owns_temporary_output,
//                     duplicate_computed_but_discarded.
//  Human interpretation: Duplicate computed/discarded outputs may be normal under stress;
//                        clean ownership is the key question: no leak, no double-free,
//                        no ambiguous owner, and documented balance equation.
#define CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE 1
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
#   define CNR3_DIAG_PRINT_DSUM07_TEMP_OUTPUT_LIFECYCLE 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM07_TEMP_OUTPUT_LIFECYCLE) && !defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
#   error "Cannot print DSUM07_TEMP_OUTPUT_LIFECYCLE without computing DSUM07_TEMP_OUTPUT_LIFECYCLE"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-08
//  Name: Cache store / duplicate-store / first-in-best-dressed summary
//  Purpose: Shows cache store attempts, duplicate store cases, and whether first-in-best-
//           dressed authority is preserved under out-of-order and future parallel paths.
//  Activation condition: Add when cache store logic exists.
//  Likely collection: Count at the single store helper or CMS store boundary.
//  Field definitions: store_attempts, store_successes, store_errors,
//                     duplicate_skipped_already_cached, duplicate_computed_but_discarded,
//                     first_in_best_dressed_duplicate_count.
//  Human interpretation: Duplicate counts are not automatically bad; duplicates with clean
//                        ownership are INFO/WARN by test intent, but overwrite, leak,
//                        ref imbalance, or genuine store errors are failures.
#define CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE 1
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
#   define CNR3_DIAG_PRINT_DSUM08_CACHE_STORE 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM08_CACHE_STORE) && !defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
#   error "Cannot print DSUM08_CACHE_STORE without computing DSUM08_CACHE_STORE"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-09
//  Name: Return-decision / return-transfer summary
//  Purpose: Shows the difference between deciding an output should be returned and actually
//           transferring ownership to the caller.
//  Activation condition: Add when output-return decision and transfer paths exist.
//  Likely collection: Count at return-decision and return-transfer boundaries.
//  Field definitions: return_decisions_checked, return_decision_yes, return_decision_no,
//                     return_no_reason_split, return_transfer_attempted,
//                     return_transfer_succeeded, lookup_ref_transferred,
//                     lookup_ref_released, lookup_ref_balance, output_authoritative.
//  Human interpretation: Decision and transfer are separate and both must be accounted;
//                        yes-without-transfer needs cleanup/error accounting, and
//                        lookup_ref_balance must remain zero.
#define CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER 1
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
#   define CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER) && !defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
#   error "Cannot print DSUM09_RETURN_TRANSFER without computing DSUM09_RETURN_TRANSFER"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-10
//  Name: Prune / eviction safety summary
//  Purpose: Shows whether pruning/eviction obeyed protection rules; prune must not evict
//           frames protected by CMS07.
//  Activation condition: Add when prune/eviction exists.
//  Likely collection: Count at prune candidate examination and detach decisions.
//  Field definitions: prune_attempts, prune_candidates_examined, prune_candidates_detached,
//                     prune_candidates_rejected_pinned, prune_candidates_rejected_checkpoint,
//                     prune_candidates_rejected_in_hot_zone, prune_batches,
//                     prune_k_limit_hits, hard_ceiling_abort_count, post_prune_cache_count.
//  Human interpretation: Pinned/checkpoint/in-zone rejection counts usually mean protections
//                        are active; any protected-frame eviction is FAIL; K-limit hits
//                        show prune pressure.
#define CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION 1
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
#   define CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION) && !defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
#   error "Cannot print DSUM10_PRUNE_EVICTION without computing DSUM10_PRUNE_EVICTION"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-11
//  Name: Hot-zone operation summary
//  Purpose: Shows whether hot-zone prune-policy hints are created, updated, merged, decayed,
//           expired, and used plausibly; it must not imply active liveness.
//  Activation condition: Add when hot-zone state exists.
//  Likely collection: Count at hot-zone create, slide, merge, decay, expiry, and prune
//                     rejection caused by hot-zone protection.
//  Field definitions: hot_zone_updates, zones_created, zones_slid, zones_merged,
//                     zones_decayed, zones_expired, zone_count_min/avg/max,
//                     protected_range_min/max, frames_rejected_from_prune_due_to_hot_zone.
//  Human interpretation: Hot zones are prune-policy hints only; active liveness is proven
//                        by pins, not zones; unbounded growth, no expiry, or corrupted
//                        ranges indicate policy problems.
#define CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE 1
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
#   define CNR3_DIAG_PRINT_DSUM11_HOT_ZONE 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM11_HOT_ZONE) && !defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
#   error "Cannot print DSUM11_HOT_ZONE without computing DSUM11_HOT_ZONE"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-12
//  Name: Recovery planning / hole-filling summary
//  Purpose: Shows whether recovery planning identified genuine holes, reused present frames,
//           requested only genuine-hole sources, and filled planned holes.
//  Activation condition: Add when recovery planning and hole-filling logic exist.
//  Likely collection: Count at plan create/destroy, bounded-start selection, hole
//                     identification, genuine-hole source request/retrieve, and fill result.
//  Field definitions: recovery_plans_created, recovery_plans_destroyed,
//                     recovery_plan_balance, nearest_present_output_found, holes_identified,
//                     holes_filled, source_frames_for_holes_requested,
//                     source_frames_for_holes_retrieved, fallback_failures,
//                     bounded_start_honesty_failures.
//  Human interpretation: Plan create/destroy must balance; holes identified and filled should
//                        match for successful plans; source requests must be for genuine holes,
//                        not a blanket backward source window.
#define CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN 1
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
#   define CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN) && !defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
#   error "Cannot print DSUM12_RECOVERY_PLAN without computing DSUM12_RECOVERY_PLAN"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-13
//  Name: Recalculation histogram
//  Purpose: Shows how often outputs were recomputed and how deep recomputation went, to help
//           detect cache/recovery policy degenerating into excessive recomputation.
//  Activation condition: Add when recomputation or hole filling can occur.
//  Likely collection: At each computed output, classify first computation versus
//                     recalculation and track depth or chain length by documented definition.
//  Field definitions: recalculated_frame_count, recalculation_depth_histogram,
//                     max_recalculation_depth, frames_recalculated_once,
//                     frames_recalculated_multiple_times.
//  Human interpretation: Some recalculation may be expected under out-of-order stress;
//                        recalculation with clean ownership is not failure by itself;
//                        deep/repeated recalculation may indicate retention/prune problems.
#define CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION 1
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
#   define CNR3_DIAG_PRINT_DSUM13_RECALCULATION 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION) && !defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
#   error "Cannot print DSUM13_RECALCULATION without computing DSUM13_RECALCULATION"
#endif
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-14
//  Name: Scene-change / recursive-reset / checkpoint-promotion summary
//  Purpose: Shows interaction between pixel-layer scene-change detection, recursive reset,
//           and cache checkpoint promotion; pixel layer reports metadata but does not set flags.
//  Activation condition: Add when pixel compute can report scene-change/reset metadata and
//                        the store path can apply checkpoint flags.
//  Likely collection: Pixel/frame processing reports scene-change/reset metadata; cache/store
//                     orchestration consumes it and counts both pixel observation and cache
//                     consequence.
//  Field definitions: frames_processed, scene_changes_detected, recursive_blend_frames,
//                     source_copy_reset_frames, scene_change_checkpoint_promotions,
//                     scene_change_checkpoint_store_successes,
//                     scene_change_checkpoint_store_duplicate_skips,
//                     scene_change_checkpoint_store_errors,
//                     scene_change_checkpoint_promotion_mismatches,
//                     cut_near_grid_checkpoint_count, scene_chroma_enabled,
//                     scene_threshold_used.
//  Human interpretation: Scene-change detection is a pixel-layer observation; source-copy
//                        reset is algorithmic; checkpoint promotion is cache/store consequence;
//                        eligible reset without required promotion is a serious issue.
#define CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET 1
#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)
#   define CNR3_DIAG_PRINT_DSUM14_SCENE_RESET 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM14_SCENE_RESET) && !defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)
#   error "Cannot print DSUM14_SCENE_RESET without computing DSUM14_SCENE_RESET"
#endif
// ---------------------------------------------------------------------------------------------
