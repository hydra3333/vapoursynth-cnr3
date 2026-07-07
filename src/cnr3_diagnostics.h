#pragma once

#include "cnr3_common.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

/*
    CNR3 generic diagnostics output core.

    CMS07-B.2.4 introduces a small stderr-only diagnostic output boundary.

    This module may:
        - name diagnostic severities;
        - write one diagnostic line to stderr;
        - flush stderr explicitly;
        - include instance IDs in diagnostic text.

    This module must not:
        - accumulate D-SUM counters;
        - print D-SUM summaries;
        - inspect or modify cache state;
        - request or retrieve frames;
        - own or release frame references;
        - affect output frames or correct control flow.

    Diagnostic observation gates observe only. They must not affect correct
    program behaviour or output frames.

    Important lock rule:
        Callers must not call formatting or printing helpers while holding a
        CMS07 atomic/locked scope. Formatting and stderr output are intentionally
        outside cache ownership and atomic-scope mechanics.

    Stderr rule:
        Diagnostics, debug messages, proof summaries, and status text must not
        write to stdout. In common VapourSynth pipelines, stdout may carry frame
        data. Diagnostic text belongs on stderr.
*/

enum class Cnr3DiagnosticLevel : unsigned char {
    info = 0,
    warning,
    error
};

enum class Cnr3StderrFlushPolicy : unsigned char {
    flush = 0,
    no_flush
};

[[nodiscard]] constexpr const char* cnr3_diagnostic_level_name(
    Cnr3DiagnosticLevel level
) noexcept {
    switch (level) {
    case Cnr3DiagnosticLevel::info:
        return "INFO";
    case Cnr3DiagnosticLevel::warning:
        return "WARN";
    case Cnr3DiagnosticLevel::error:
        return "ERROR";
    }

    return "UNKNOWN";
}

/*
    Write one diagnostic line to stderr.

    Default behaviour is to flush stderr after the line. High-volume future
    callers may request no_flush and then call cnr3_diag_flush_stderr() once at
    a deliberate boundary.

    This is deliberately not a D-SUM printer. It is a low-level output helper
    for future diagnostics and proof summaries. It does not check diagnostic
    gates; callers remain responsible for calling it only from the relevant
    gated diagnostic path.

    Null component/message pointers are handled defensively.
*/
void cnr3_diag_write_line(
    Cnr3InstanceId instance_id,
    Cnr3DiagnosticLevel level,
    const char* component,
    const char* message,
    Cnr3StderrFlushPolicy flush_policy = Cnr3StderrFlushPolicy::flush
) noexcept;

/*
    Flush stderr explicitly.

    Use this after future high-volume diagnostic blocks that intentionally used
    Cnr3StderrFlushPolicy::no_flush for individual lines.
*/
void cnr3_diag_flush_stderr() noexcept;


#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)

using Cnr3DiagPlanTraceTick = std::uint64_t;

enum class Cnr3DiagPlanTracePhase : unsigned char {
    open = 0,
    result
};

enum class Cnr3DiagPlanTraceStrategy : unsigned char {
    none = 0,
    cache_hit,
    frame0,
    predecessor_present,
    recovery_exact,
    recovery_floor
};

enum class Cnr3DiagPlanTraceOutcome : unsigned char {
    none = 0,
    returned_cache_hit,
    returned_computed,
    returned_recovered,
    failed
};

enum class Cnr3DiagPlanTraceFailReason : unsigned char {
    none = 0,
    copyframe_failed,
    copyframe_source_alias,
    source_retrieval_failed,
    source_not_requested,
    acquire_ref_failed,
    adopt_failed,
    store_prune_failed,
    discharge_failed,
    invalid_lifecycle,
    invalid_branch_foundation,
    scene_processing_failed,
    byte_estimate_failed,
    framedata_missing_or_unknown,
    allocation_failed,
    recovery_plan_failed_or_refused,
    hot_zone_observation_failed
};

struct Cnr3DiagPlanTraceOpenFields {
    Cnr3DiagPlanTraceStrategy strategy = Cnr3DiagPlanTraceStrategy::none;
    int target_frame = CNR3_INVALID_FRAME_NUMBER;
    int predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
    int anchor_frame = CNR3_INVALID_FRAME_NUMBER;
    int floor_frame = CNR3_INVALID_FRAME_NUMBER;
    std::vector<int> hole_frames{};
    std::vector<int> source_frames{};
    std::vector<int> pinned_frames{};
};

struct Cnr3DiagPlanTraceResultFields {
    Cnr3DiagPlanTraceOutcome outcome = Cnr3DiagPlanTraceOutcome::none;
    Cnr3DiagPlanTraceFailReason fail_reason = Cnr3DiagPlanTraceFailReason::none;
    std::vector<int> computed_frames{};
    std::vector<int> adopted_skipped_frames{};
    std::vector<int> post_compute_loser_frames{};
    std::vector<int> not_reached_frames{};
    std::vector<int> error_here_frames{};
};

struct Cnr3DiagPlanTraceRecord {
    Cnr3DiagPlanTracePhase phase = Cnr3DiagPlanTracePhase::open;
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    std::uint64_t action_seq = 0;
    Cnr3DiagPlanTraceTick enter_tick = 0;
    Cnr3DiagPlanTraceTick exit_tick = 0;
    Cnr3DiagPlanTraceOpenFields open{};
    Cnr3DiagPlanTraceResultFields result{};
};

/*
    DSUM-PLANTRACE per-instance buffer.

    This diagnostics-only mutex protects the plan-trace vector and action_seq
    only. It is not a cache/CMS lock. enter_tick is sampled outside this mutex;
    action_seq is incremented inside the same critical section as append.
*/
struct Cnr3DiagPlanTraceBuffer {
    mutable std::mutex mutex{};
    std::uint64_t next_action_seq = 0;
    bool time_anchor_set = false;
    Cnr3DiagPlanTraceTick steady_anchor_tick = 0;
    long long system_anchor_epoch_ms = 0;
    bool dumped = false;
    bool reserve_failed = false;
    std::vector<Cnr3DiagPlanTraceRecord> records{};
};

[[nodiscard]] Cnr3DiagPlanTraceTick
cnr3_diag_plantrace_sample_tick() noexcept;

void cnr3_diag_plantrace_observe_open(
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    Cnr3DiagPlanTraceTick exit_tick,
    const Cnr3DiagPlanTraceOpenFields& fields
) noexcept;

void cnr3_diag_plantrace_observe_result(
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    Cnr3DiagPlanTraceTick exit_tick,
    const Cnr3DiagPlanTraceResultFields& fields
) noexcept;

void cnr3_diag_plantrace_observe_failed_result_and_dump(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    const Cnr3DiagPlanTraceResultFields& fields
) noexcept;

void cnr3_diag_plantrace_observe_minimal_failed_and_dump(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceFailReason fail_reason,
    int error_frame
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM_PLANTRACE)

void cnr3_diag_plantrace_write_clean_end_dump_to_stderr(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer
) noexcept;

void cnr3_diag_plantrace_write_bail_dump_to_stderr(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)

inline constexpr std::size_t CNR3_DIAG_DSUM01_GAP_HISTOGRAM_BIN_COUNT = 9U;

/*
    D-SUM-01 request-arrival / ordering diagnostic state.

    This state is per filter instance and observe-only. The mutex protects the
    diagnostic counters only; it is not a cache/CMS atomic-scope lock. Summary
    printing must take a snapshot and release this mutex before formatting or
    writing stderr.
*/
struct Cnr3DiagDsum01RequestOrderStats {
    mutable std::mutex mutex{};

    std::uint64_t ar_initial_count = 0;
    std::uint64_t ar_all_frames_ready_count = 0;

    bool have_requested_frame = false;
    int first_requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int last_requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int previous_requested_frame = CNR3_INVALID_FRAME_NUMBER;

    std::uint64_t monotonic_forward_count = 0;
    std::uint64_t same_frame_or_duplicate_count = 0;
    std::uint64_t backward_jump_count = 0;
    std::uint64_t forward_jump_count = 0;
    std::uint64_t out_of_order_count = 0;

    int max_forward_jump = 0;
    int max_backward_jump = 0;

    std::uint64_t arrival_gap_histogram[
        CNR3_DIAG_DSUM01_GAP_HISTOGRAM_BIN_COUNT
    ] = {};
};

struct Cnr3DiagDsum01RequestOrderSnapshot {
    std::uint64_t ar_initial_count = 0;
    std::uint64_t ar_all_frames_ready_count = 0;

    bool have_requested_frame = false;
    int first_requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int last_requested_frame = CNR3_INVALID_FRAME_NUMBER;

    std::uint64_t monotonic_forward_count = 0;
    std::uint64_t same_frame_or_duplicate_count = 0;
    std::uint64_t backward_jump_count = 0;
    std::uint64_t forward_jump_count = 0;
    std::uint64_t out_of_order_count = 0;

    int max_forward_jump = 0;
    int max_backward_jump = 0;

    std::uint64_t arrival_gap_histogram[
        CNR3_DIAG_DSUM01_GAP_HISTOGRAM_BIN_COUNT
    ] = {};
};

void cnr3_diag_dsum01_observe_ar_initial(
    Cnr3DiagDsum01RequestOrderStats& stats,
    int requested_frame
) noexcept;

void cnr3_diag_dsum01_observe_ar_all_frames_ready(
    Cnr3DiagDsum01RequestOrderStats& stats
) noexcept;

[[nodiscard]] Cnr3DiagDsum01RequestOrderSnapshot
cnr3_diag_dsum01_snapshot_request_order(
    const Cnr3DiagDsum01RequestOrderStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER)

void cnr3_diag_dsum01_write_request_order_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum01RequestOrderStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)

inline constexpr std::size_t CNR3_DIAG_DSUM03_DEPTH_HISTOGRAM_BIN_COUNT = 6U;

enum class Cnr3DiagDsum03RecoveryTermination : unsigned char {
    present_output = 0,
    frame0,
    bound,
    failure
};

/*
    D-SUM-03 recovery-search diagnostic state.

    This is per filter instance and observe-only. The mutex protects these
    counters only; it is not a cache/CMS atomic-scope lock. Summary printing
    snapshots the state and releases this mutex before formatting stderr.
*/
struct Cnr3DiagDsum03RecoverySearchStats {
    mutable std::mutex mutex{};

    std::uint64_t search_attempts = 0;
    std::uint64_t search_successes = 0;
    std::uint64_t search_failures = 0;
    std::uint64_t depth_histogram[
        CNR3_DIAG_DSUM03_DEPTH_HISTOGRAM_BIN_COUNT
    ] = {};
    std::uint64_t terminated_on_present_output = 0;
    std::uint64_t terminated_on_frame0 = 0;
    std::uint64_t terminated_on_bound = 0;
    std::uint64_t terminated_on_failure = 0;
    std::uint64_t holes_filled = 0;
};

struct Cnr3DiagDsum03RecoverySearchSnapshot {
    std::uint64_t search_attempts = 0;
    std::uint64_t search_successes = 0;
    std::uint64_t search_failures = 0;
    std::uint64_t depth_histogram[
        CNR3_DIAG_DSUM03_DEPTH_HISTOGRAM_BIN_COUNT
    ] = {};
    std::uint64_t terminated_on_present_output = 0;
    std::uint64_t terminated_on_frame0 = 0;
    std::uint64_t terminated_on_bound = 0;
    std::uint64_t terminated_on_failure = 0;
    std::uint64_t holes_filled = 0;
};

void cnr3_diag_dsum03_observe_search_result(
    Cnr3DiagDsum03RecoverySearchStats& stats,
    bool search_succeeded,
    Cnr3DiagDsum03RecoveryTermination termination,
    int search_depth
) noexcept;

void cnr3_diag_dsum03_observe_holes_filled(
    Cnr3DiagDsum03RecoverySearchStats& stats,
    std::size_t hole_count
) noexcept;

[[nodiscard]] Cnr3DiagDsum03RecoverySearchSnapshot
cnr3_diag_dsum03_snapshot_recovery_search(
    const Cnr3DiagDsum03RecoverySearchStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM03_RECOVERY_SEARCH)

void cnr3_diag_dsum03_write_recovery_search_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum03RecoverySearchStats& stats
) noexcept;

#endif


#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)

/*
    D-SUM-06 source-frame lifecycle diagnostic state.

    This is per filter instance and observe-only. It tracks plugin-side source
    request/retrieve/release balance only; output/cache/return references
    belong to D-SUM-07/D-SUM-09 and cache-side ownership diagnostics.
*/
struct Cnr3DiagDsum06SourceFrameLifecycleStats {
    mutable std::mutex mutex{};

    std::uint64_t source_frames_requested_total = 0;
    std::uint64_t source_frames_retrieved_total = 0;
    std::uint64_t source_frames_released_total = 0;
    std::uint64_t same_activation_request_violations = 0;
    std::uint64_t source_frame_count_max = 0;
    std::uint64_t partial_acquire_failures = 0;
    std::uint64_t source_frame_release_balance_errors = 0;

    std::uint64_t current_source_frame_count = 0;
};

struct Cnr3DiagDsum06SourceFrameLifecycleSnapshot {
    std::uint64_t source_frames_requested_total = 0;
    std::uint64_t source_frames_retrieved_total = 0;
    std::uint64_t source_frames_released_total = 0;
    long long source_frame_release_balance = 0;
    std::uint64_t same_activation_request_violations = 0;
    std::uint64_t source_frame_count_max = 0;
    std::uint64_t partial_acquire_failures = 0;
    std::uint64_t source_frame_release_balance_errors = 0;
};

void cnr3_diag_dsum06_observe_source_requests(
    Cnr3DiagDsum06SourceFrameLifecycleStats& stats,
    std::size_t request_count
) noexcept;

void cnr3_diag_dsum06_observe_source_retrieve(
    Cnr3DiagDsum06SourceFrameLifecycleStats& stats,
    bool same_activation_requested,
    bool retrieved
) noexcept;

void cnr3_diag_dsum06_observe_source_release(
    Cnr3DiagDsum06SourceFrameLifecycleStats& stats
) noexcept;

[[nodiscard]] Cnr3DiagDsum06SourceFrameLifecycleSnapshot
cnr3_diag_dsum06_snapshot_source_frame_lifecycle(
    const Cnr3DiagDsum06SourceFrameLifecycleStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM06_SOURCE_FRAME_LIFECYCLE)

void cnr3_diag_dsum06_write_source_frame_lifecycle_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum06SourceFrameLifecycleStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)

/*
    D-SUM-07 temporary-output lifecycle diagnostic state.

    This tracks genuinely produced non-alias temporary output frames. Production
    addFrameRef() cache copies are deliberately outside this balance; they are
    separate references owned at the cache boundary.
*/
struct Cnr3DiagDsum07TempOutputLifecycleStats {
    mutable std::mutex mutex{};

    std::uint64_t temporary_outputs_created = 0;
    std::uint64_t temporary_outputs_stored = 0;
    std::uint64_t temporary_outputs_released = 0;
    std::uint64_t temporary_outputs_transferred = 0;
    std::uint64_t caller_still_owns_temporary_output = 0;
    std::uint64_t duplicate_computed_but_discarded = 0;
};

struct Cnr3DiagDsum07TempOutputLifecycleSnapshot {
    std::uint64_t temporary_outputs_created = 0;
    std::uint64_t temporary_outputs_stored = 0;
    std::uint64_t temporary_outputs_released = 0;
    std::uint64_t temporary_outputs_transferred = 0;
    long long temporary_output_balance = 0;
    std::uint64_t caller_still_owns_temporary_output = 0;
    std::uint64_t duplicate_computed_but_discarded = 0;
};

void cnr3_diag_dsum07_observe_temporary_output_created(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

void cnr3_diag_dsum07_observe_temporary_output_stored(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

void cnr3_diag_dsum07_observe_temporary_output_released(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

void cnr3_diag_dsum07_observe_temporary_output_transferred(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

void cnr3_diag_dsum07_observe_duplicate_computed_but_discarded(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

[[nodiscard]] Cnr3DiagDsum07TempOutputLifecycleSnapshot
cnr3_diag_dsum07_snapshot_temp_output_lifecycle(
    const Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM07_TEMP_OUTPUT_LIFECYCLE)

void cnr3_diag_dsum07_write_temp_output_lifecycle_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)

enum class Cnr3DiagDsum09ReturnNoReason : unsigned char {
    hard_store_failure = 0,
    store_status_not_returnable,
    duplicate_winner_lookup_failed,
    null_return_frame,
    discard_failed_after_return_ready
};

/*
    D-SUM-09 return-transfer diagnostic state.

    Lookup-reference accounting here is return-boundary accounting for live
    getFrame Cnr3OwnedFrameRef objects. It is intentionally disjoint from
    D-SUM-04 cache-core lookup-reference accounting.
*/
struct Cnr3DiagDsum09ReturnTransferStats {
    mutable std::mutex mutex{};

    std::uint64_t return_decisions_checked = 0;
    std::uint64_t return_decision_yes = 0;
    std::uint64_t return_decision_no = 0;
    std::uint64_t return_no_hard_store_failure = 0;
    std::uint64_t return_no_store_status_not_returnable = 0;
    std::uint64_t return_no_duplicate_winner_lookup_failed = 0;
    std::uint64_t return_no_null_return_frame = 0;
    std::uint64_t return_no_discard_failed_after_return_ready = 0;
    std::uint64_t return_transfer_attempted = 0;
    std::uint64_t return_transfer_succeeded = 0;
    std::uint64_t lookup_ref_transferred = 0;
    std::uint64_t lookup_ref_released = 0;
    std::uint64_t output_authoritative = 0;

    std::uint64_t lookup_ref_acquired = 0;
};

struct Cnr3DiagDsum09ReturnTransferSnapshot {
    std::uint64_t return_decisions_checked = 0;
    std::uint64_t return_decision_yes = 0;
    std::uint64_t return_decision_no = 0;
    std::uint64_t return_no_hard_store_failure = 0;
    std::uint64_t return_no_store_status_not_returnable = 0;
    std::uint64_t return_no_duplicate_winner_lookup_failed = 0;
    std::uint64_t return_no_null_return_frame = 0;
    std::uint64_t return_no_discard_failed_after_return_ready = 0;
    std::uint64_t return_transfer_attempted = 0;
    std::uint64_t return_transfer_succeeded = 0;
    std::uint64_t lookup_ref_transferred = 0;
    std::uint64_t lookup_ref_released = 0;
    long long lookup_ref_balance = 0;
    std::uint64_t output_authoritative = 0;
    std::uint64_t lookup_ref_acquired = 0;
};

void cnr3_diag_dsum09_observe_return_decision(
    Cnr3DiagDsum09ReturnTransferStats& stats,
    bool return_allowed,
    Cnr3DiagDsum09ReturnNoReason no_reason
) noexcept;

void cnr3_diag_dsum09_observe_return_no_reason(
    Cnr3DiagDsum09ReturnTransferStats& stats,
    Cnr3DiagDsum09ReturnNoReason no_reason
) noexcept;

void cnr3_diag_dsum09_observe_return_transfer(
    Cnr3DiagDsum09ReturnTransferStats& stats,
    bool transfer_succeeded,
    bool output_authoritative
) noexcept;

void cnr3_diag_dsum09_observe_lookup_ref_acquired(
    Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept;

void cnr3_diag_dsum09_observe_lookup_ref_transferred(
    Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept;

void cnr3_diag_dsum09_observe_lookup_ref_released(
    Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept;

[[nodiscard]] Cnr3DiagDsum09ReturnTransferSnapshot
cnr3_diag_dsum09_snapshot_return_transfer(
    const Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER)

void cnr3_diag_dsum09_write_return_transfer_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)

/*
    D-SUM-12 recovery-plan / recovery-rate diagnostic state.

    This tracks only accepted/published recovery activations. Scratch
    Cnr3CacheRecoverySearchPlan values are deliberately excluded: creation is
    counted only when the recovery branch publishes frameData, and destruction
    is counted at the single frameData discard boundary for branch==recovery.
*/
struct Cnr3DiagDsum12RecoveryPlanStats {
    mutable std::mutex mutex{};

    std::uint64_t recovery_plans_created = 0;
    std::uint64_t recovery_plans_destroyed = 0;
    std::uint64_t nearest_present_output_found = 0;
    std::uint64_t holes_identified = 0;
    std::uint64_t holes_filled = 0;
    std::uint64_t source_frames_for_holes_requested = 0;
    std::uint64_t source_frames_for_holes_retrieved = 0;
    std::uint64_t fallback_failures = 0;
    std::uint64_t bounded_start_honesty_failures = 0;

    std::uint64_t frames_total = 0;
    std::uint64_t frames_cache_hit = 0;
    std::uint64_t frames_pred_present = 0;
    std::uint64_t frames_frame0 = 0;
    std::uint64_t frames_recovered_exact = 0;
    std::uint64_t frames_recovered_floor = 0;
    std::uint64_t recovery_span_sum = 0;
    std::uint64_t recovery_span_samples = 0;
    int recovery_span_max = 0;
};

struct Cnr3DiagDsum12RecoveryPlanSnapshot {
    std::uint64_t recovery_plans_created = 0;
    std::uint64_t recovery_plans_destroyed = 0;
    std::uint64_t nearest_present_output_found = 0;
    std::uint64_t holes_identified = 0;
    std::uint64_t holes_filled = 0;
    std::uint64_t source_frames_for_holes_requested = 0;
    std::uint64_t source_frames_for_holes_retrieved = 0;
    std::uint64_t fallback_failures = 0;
    std::uint64_t bounded_start_honesty_failures = 0;

    std::uint64_t frames_total = 0;
    std::uint64_t frames_cache_hit = 0;
    std::uint64_t frames_pred_present = 0;
    std::uint64_t frames_frame0 = 0;
    std::uint64_t frames_recovered_exact = 0;
    std::uint64_t frames_recovered_floor = 0;
    std::uint64_t recovery_span_sum = 0;
    std::uint64_t recovery_span_samples = 0;
    int recovery_span_max = 0;
};

void cnr3_diag_dsum12_observe_branch_cache_hit(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

void cnr3_diag_dsum12_observe_branch_frame0(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

void cnr3_diag_dsum12_observe_branch_pred_present(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

void cnr3_diag_dsum12_observe_recovery_plan_published(
    Cnr3DiagDsum12RecoveryPlanStats& stats,
    bool exact_anchor_recovery,
    bool floor_fresh_start_recovery,
    bool nearest_present_output_found,
    std::size_t hole_count,
    int exact_anchor_span
) noexcept;

void cnr3_diag_dsum12_observe_recovery_plan_destroyed(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

void cnr3_diag_dsum12_observe_holes_filled(
    Cnr3DiagDsum12RecoveryPlanStats& stats,
    std::size_t hole_count
) noexcept;

void cnr3_diag_dsum12_observe_hole_source_retrieved(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

void cnr3_diag_dsum12_observe_fallback_failure(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

void cnr3_diag_dsum12_observe_bounded_start_honesty_failure(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

[[nodiscard]] Cnr3DiagDsum12RecoveryPlanSnapshot
cnr3_diag_dsum12_snapshot_recovery_plan(
    const Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)

void cnr3_diag_dsum12_write_recovery_plan_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)

inline constexpr std::size_t CNR3_DIAG_DSUM13_RECALC_DEPTH_HISTOGRAM_BIN_COUNT = 7U;
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY_MULTIPLIER = 16U;
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY_FLOOR = 1024U;

#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY = 1600U;
#else
inline constexpr std::size_t CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY = 16000U;
#endif

static_assert(
    CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY >=
    CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY_FLOOR
);

struct Cnr3DiagDsum13ComputeCountEntry {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    std::uint16_t compute_count = 0;
    std::uint8_t max_depth = 0;
    bool occupied = false;
};

/*
    D-SUM-13 recalculation diagnostic state.

    The fixed map is deliberately per instance and compile-gated. With the
    normal profile it is 16000 compact entries, about 128 KiB per instance;
    with the tiny diagnostic profile it is 1600 entries. Saturation is honest:
    once full, later unseen frames are dropped and the writer says the counts
    are lower bounds.
*/
struct Cnr3DiagDsum13RecalculationStats {
    mutable std::mutex mutex{};

    std::uint64_t recalculated_frame_count = 0;
    std::uint64_t recalculation_depth_histogram[
        CNR3_DIAG_DSUM13_RECALC_DEPTH_HISTOGRAM_BIN_COUNT
    ] = {};
    int max_recalculation_depth = 0;
    std::uint64_t frames_recalculated_once = 0;
    std::uint64_t frames_recalculated_multiple_times = 0;

    std::uint64_t compute_observations_total = 0;
    bool compute_count_map_saturated = false;
    std::uint64_t compute_count_observations_dropped = 0;

    std::array<
        Cnr3DiagDsum13ComputeCountEntry,
        CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY
    > compute_count_map{};
};

struct Cnr3DiagDsum13RecalculationSnapshot {
    std::uint64_t recalculated_frame_count = 0;
    std::uint64_t recalculation_depth_histogram[
        CNR3_DIAG_DSUM13_RECALC_DEPTH_HISTOGRAM_BIN_COUNT
    ] = {};
    int max_recalculation_depth = 0;
    std::uint64_t frames_recalculated_once = 0;
    std::uint64_t frames_recalculated_multiple_times = 0;

    std::uint64_t compute_observations_total = 0;
    bool compute_count_map_saturated = false;
    std::uint64_t compute_count_observations_dropped = 0;
    std::size_t compute_count_map_capacity = CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY;
};

void cnr3_diag_dsum13_observe_compute_completion(
    Cnr3DiagDsum13RecalculationStats& stats,
    int frame_number,
    int recalculation_depth
) noexcept;

[[nodiscard]] Cnr3DiagDsum13RecalculationSnapshot
cnr3_diag_dsum13_snapshot_recalculation(
    const Cnr3DiagDsum13RecalculationStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)

void cnr3_diag_dsum13_write_recalculation_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum13RecalculationStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)

/*
    D-SUM-14 scene-reset diagnostic state.

    This is a getFrame-side summary reader. It surfaces already-computed scene
    process summaries and their checkpoint-store consequences without changing
    pixel, cache, or return control flow.
*/
struct Cnr3DiagDsum14SceneResetStats {
    mutable std::mutex mutex{};

    std::uint64_t scene_change_detections = 0;
    std::uint64_t source_copy_reset_frames = 0;
    std::uint64_t scene_change_checkpoint_promotions = 0;
    std::uint64_t scene_change_checkpoint_store_successes = 0;
    std::uint64_t scene_change_checkpoint_store_duplicate_skips = 0;
    std::uint64_t scene_change_checkpoint_store_errors = 0;
    std::uint64_t scene_change_checkpoint_promotion_mismatches = 0;
    std::uint64_t cut_near_grid_checkpoint_count = 0;
    bool scene_chroma_enabled = false;
    long long scene_threshold_used = 0;
};

struct Cnr3DiagDsum14SceneResetSnapshot {
    std::uint64_t scene_change_detections = 0;
    std::uint64_t source_copy_reset_frames = 0;
    std::uint64_t scene_change_checkpoint_promotions = 0;
    std::uint64_t scene_change_checkpoint_store_successes = 0;
    std::uint64_t scene_change_checkpoint_store_duplicate_skips = 0;
    std::uint64_t scene_change_checkpoint_store_errors = 0;
    std::uint64_t scene_change_checkpoint_promotion_mismatches = 0;
    std::uint64_t cut_near_grid_checkpoint_count = 0;
    bool scene_chroma_enabled = false;
    long long scene_threshold_used = 0;
};

void cnr3_diag_dsum14_observe_scene_outcome(
    Cnr3DiagDsum14SceneResetStats& stats,
    int frame_number,
    bool scene_chroma_used,
    long long scene_threshold,
    bool scene_change_detected,
    bool scene_change_reset_output_used,
    bool checkpoint_store_requested,
    Cnr3Status checkpoint_store_status,
    bool checkpoint_promoted
) noexcept;

[[nodiscard]] Cnr3DiagDsum14SceneResetSnapshot
cnr3_diag_dsum14_snapshot_scene_reset(
    const Cnr3DiagDsum14SceneResetStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM14_SCENE_RESET)

void cnr3_diag_dsum14_write_scene_reset_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum14SceneResetStats& stats
) noexcept;

#endif
