#include "cnr3_diagnostics.h"

#include <cstdio>
#include <cstring>
#include <mutex>

namespace {

    [[nodiscard]] const char* cnr3_diag_safe_text(
        const char* text
    ) noexcept {
        return (text != nullptr) ? text : "(null)";
    }

    void cnr3_diag_saturating_increment(
        std::uint64_t& value
    ) noexcept {
        if (value < UINT64_MAX) {
            ++value;
        }
    }

    void cnr3_diag_saturating_decrement(
        std::uint64_t& value
    ) noexcept {
        if (value > 0U) {
            --value;
        }
    }

    void cnr3_diag_saturating_add(
        std::uint64_t& value,
        std::uint64_t amount
    ) noexcept {
        if (UINT64_MAX - value < amount) {
            value = UINT64_MAX;
            return;
        }

        value += amount;
    }

    [[nodiscard]] int cnr3_diag_clamp_nonnegative_depth(
        int depth
    ) noexcept {
        return depth > 0 ? depth : 0;
    }

    void cnr3_diag_write_uint64_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_id,
        const char* label,
        std::uint64_t value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %llu",
            dsum_id != nullptr ? dsum_id : "D-SUM-??",
            label != nullptr ? label : "(null)",
            static_cast<unsigned long long>(value)
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_int_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_id,
        const char* label,
        int value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %d",
            dsum_id != nullptr ? dsum_id : "D-SUM-??",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_int64_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_id,
        const char* label,
        long long value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %lld",
            dsum_id != nullptr ? dsum_id : "D-SUM-??",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_double_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_id,
        const char* label,
        double value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %.3f",
            dsum_id != nullptr ? dsum_id : "D-SUM-??",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_text_line(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* message
    ) noexcept {
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_bool_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_id,
        const char* label,
        bool value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %s",
            dsum_id != nullptr ? dsum_id : "D-SUM-??",
            label != nullptr ? label : "(null)",
            value ? "true" : "false"
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

} // namespace

void cnr3_diag_write_line(
    Cnr3InstanceId instance_id,
    Cnr3DiagnosticLevel level,
    const char* component,
    const char* message,
    Cnr3StderrFlushPolicy flush_policy
) noexcept {
    const int printable_instance_id =
        cnr3_instance_id_is_valid(instance_id) ? instance_id.value : 0;

    std::fprintf(
        stderr,
        "CNR3[%d] %s %s: %s\n",
        printable_instance_id,
        cnr3_diagnostic_level_name(level),
        cnr3_diag_safe_text(component),
        cnr3_diag_safe_text(message)
    );

    if (flush_policy == Cnr3StderrFlushPolicy::flush) {
        cnr3_diag_flush_stderr();
    }
}

void cnr3_diag_flush_stderr() noexcept {
    std::fflush(stderr);
}


#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)

namespace {

    enum class Cnr3DiagDsum01GapHistogramBin : std::size_t {
        same = 0,
        forward_1,
        forward_2_to_5,
        forward_6_to_30,
        forward_31_plus,
        backward_1,
        backward_2_to_5,
        backward_6_to_30,
        backward_31_plus
    };

    [[nodiscard]] Cnr3DiagDsum01GapHistogramBin cnr3_diag_dsum01_gap_bin(
        int gap
    ) noexcept {
        if (gap == 0) {
            return Cnr3DiagDsum01GapHistogramBin::same;
        }

        if (gap > 0) {
            if (gap == 1) {
                return Cnr3DiagDsum01GapHistogramBin::forward_1;
            }

            if (gap <= 5) {
                return Cnr3DiagDsum01GapHistogramBin::forward_2_to_5;
            }

            if (gap <= 30) {
                return Cnr3DiagDsum01GapHistogramBin::forward_6_to_30;
            }

            return Cnr3DiagDsum01GapHistogramBin::forward_31_plus;
        }

        const int backward_gap = -gap;

        if (backward_gap == 1) {
            return Cnr3DiagDsum01GapHistogramBin::backward_1;
        }

        if (backward_gap <= 5) {
            return Cnr3DiagDsum01GapHistogramBin::backward_2_to_5;
        }

        if (backward_gap <= 30) {
            return Cnr3DiagDsum01GapHistogramBin::backward_6_to_30;
        }

        return Cnr3DiagDsum01GapHistogramBin::backward_31_plus;
    }

    void cnr3_diag_dsum01_observe_gap(
        Cnr3DiagDsum01RequestOrderStats& stats,
        int gap
    ) noexcept {
        const Cnr3DiagDsum01GapHistogramBin bin =
            cnr3_diag_dsum01_gap_bin(gap);

        cnr3_diag_saturating_increment(
            stats.arrival_gap_histogram[static_cast<std::size_t>(bin)]
        );

        if (gap == 1) {
            cnr3_diag_saturating_increment(stats.monotonic_forward_count);
            if (gap > stats.max_forward_jump) {
                stats.max_forward_jump = gap;
            }
            return;
        }

        if (gap == 0) {
            cnr3_diag_saturating_increment(stats.same_frame_or_duplicate_count);
            return;
        }

        if (gap > 1) {
            cnr3_diag_saturating_increment(stats.forward_jump_count);
            if (gap > stats.max_forward_jump) {
                stats.max_forward_jump = gap;
            }
            return;
        }

        const int backward_gap = -gap;
        cnr3_diag_saturating_increment(stats.backward_jump_count);
        cnr3_diag_saturating_increment(stats.out_of_order_count);

        if (backward_gap > stats.max_backward_jump) {
            stats.max_backward_jump = backward_gap;
        }
    }

    void cnr3_diag_dsum01_write_uint64_row(
        Cnr3InstanceId instance_id,
        const char* label,
        std::uint64_t value
    ) noexcept {
        char message[192] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-01 %-36s %llu",
            label != nullptr ? label : "(null)",
            static_cast<unsigned long long>(value)
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                "D-SUM-01",
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';

        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            "D-SUM-01",
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_dsum01_write_int_row(
        Cnr3InstanceId instance_id,
        const char* label,
        int value
    ) noexcept {
        char message[192] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-01 %-36s %d",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                "D-SUM-01",
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';

        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            "D-SUM-01",
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_dsum01_write_text_line(
        Cnr3InstanceId instance_id,
        const char* message
    ) noexcept {
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            "D-SUM-01",
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

} // namespace

void cnr3_diag_dsum01_observe_ar_initial(
    Cnr3DiagDsum01RequestOrderStats& stats,
    int requested_frame
) noexcept {
    if (!cnr3_frame_number_is_valid(requested_frame)) {
        return;
    }

    std::lock_guard<std::mutex> lock(stats.mutex);

    cnr3_diag_saturating_increment(stats.ar_initial_count);

    if (!stats.have_requested_frame) {
        stats.have_requested_frame = true;
        stats.first_requested_frame = requested_frame;
        stats.last_requested_frame = requested_frame;
        stats.previous_requested_frame = requested_frame;
        return;
    }

    const int gap = requested_frame - stats.previous_requested_frame;

    cnr3_diag_dsum01_observe_gap(stats, gap);

    stats.last_requested_frame = requested_frame;
    stats.previous_requested_frame = requested_frame;
}

void cnr3_diag_dsum01_observe_ar_all_frames_ready(
    Cnr3DiagDsum01RequestOrderStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.ar_all_frames_ready_count);
}

Cnr3DiagDsum01RequestOrderSnapshot cnr3_diag_dsum01_snapshot_request_order(
    const Cnr3DiagDsum01RequestOrderStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum01RequestOrderSnapshot snapshot{};
    snapshot.ar_initial_count = stats.ar_initial_count;
    snapshot.ar_all_frames_ready_count = stats.ar_all_frames_ready_count;
    snapshot.have_requested_frame = stats.have_requested_frame;
    snapshot.first_requested_frame = stats.first_requested_frame;
    snapshot.last_requested_frame = stats.last_requested_frame;
    snapshot.monotonic_forward_count = stats.monotonic_forward_count;
    snapshot.same_frame_or_duplicate_count = stats.same_frame_or_duplicate_count;
    snapshot.backward_jump_count = stats.backward_jump_count;
    snapshot.forward_jump_count = stats.forward_jump_count;
    snapshot.out_of_order_count = stats.out_of_order_count;
    snapshot.max_forward_jump = stats.max_forward_jump;
    snapshot.max_backward_jump = stats.max_backward_jump;

    for (std::size_t i = 0U; i < CNR3_DIAG_DSUM01_GAP_HISTOGRAM_BIN_COUNT; ++i) {
        snapshot.arrival_gap_histogram[i] = stats.arrival_gap_histogram[i];
    }

    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER)

void cnr3_diag_dsum01_write_request_order_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum01RequestOrderStats& stats
) noexcept {
    const Cnr3DiagDsum01RequestOrderSnapshot snapshot =
        cnr3_diag_dsum01_snapshot_request_order(stats);

    cnr3_diag_dsum01_write_text_line(
        instance_id,
        "[DSUM-SUMMARY] D-SUM-01 request-arrival/order summary"
    );
    cnr3_diag_dsum01_write_text_line(
        instance_id,
        "[DSUM-SUMMARY] D-SUM-01 interpretation: out-of-order is INFO under stress; WARN only if sequential order was expected"
    );

    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "arInitial_count",
        snapshot.ar_initial_count
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "arAllFramesReady_count",
        snapshot.ar_all_frames_ready_count
    );

    if (snapshot.have_requested_frame) {
        cnr3_diag_dsum01_write_int_row(
            instance_id,
            "first_requested_frame",
            snapshot.first_requested_frame
        );
        cnr3_diag_dsum01_write_int_row(
            instance_id,
            "last_requested_frame",
            snapshot.last_requested_frame
        );
    }

    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "monotonic_forward_count",
        snapshot.monotonic_forward_count
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "same_frame_or_duplicate_count",
        snapshot.same_frame_or_duplicate_count
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "backward_jump_count",
        snapshot.backward_jump_count
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "forward_jump_count",
        snapshot.forward_jump_count
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "out_of_order_count",
        snapshot.out_of_order_count
    );
    cnr3_diag_dsum01_write_int_row(
        instance_id,
        "max_forward_jump",
        snapshot.max_forward_jump
    );
    cnr3_diag_dsum01_write_int_row(
        instance_id,
        "max_backward_jump",
        snapshot.max_backward_jump
    );

    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_same",
        snapshot.arrival_gap_histogram[0]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_forward_1",
        snapshot.arrival_gap_histogram[1]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_forward_2_to_5",
        snapshot.arrival_gap_histogram[2]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_forward_6_to_30",
        snapshot.arrival_gap_histogram[3]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_forward_31_plus",
        snapshot.arrival_gap_histogram[4]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_backward_1",
        snapshot.arrival_gap_histogram[5]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_backward_2_to_5",
        snapshot.arrival_gap_histogram[6]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_backward_6_to_30",
        snapshot.arrival_gap_histogram[7]
    );
    cnr3_diag_dsum01_write_uint64_row(
        instance_id,
        "gap_hist_backward_31_plus",
        snapshot.arrival_gap_histogram[8]
    );

    cnr3_diag_flush_stderr();
}

#endif


#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)

namespace {

    [[nodiscard]] std::size_t cnr3_diag_dsum03_depth_bin(
        int depth
    ) noexcept {
        const int clean_depth = cnr3_diag_clamp_nonnegative_depth(depth);

        if (clean_depth <= 1) {
            return static_cast<std::size_t>(clean_depth);
        }

        if (clean_depth <= 5) {
            return 2U;
        }

        if (clean_depth <= 15) {
            return 3U;
        }

        if (clean_depth <= 30) {
            return 4U;
        }

        return 5U;
    }

} // namespace

void cnr3_diag_dsum03_observe_search_result(
    Cnr3DiagDsum03RecoverySearchStats& stats,
    bool search_succeeded,
    Cnr3DiagDsum03RecoveryTermination termination,
    int search_depth
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    cnr3_diag_saturating_increment(stats.search_attempts);
    if (search_succeeded) {
        cnr3_diag_saturating_increment(stats.search_successes);
    }
    else {
        cnr3_diag_saturating_increment(stats.search_failures);
    }

    cnr3_diag_saturating_increment(
        stats.depth_histogram[cnr3_diag_dsum03_depth_bin(search_depth)]
    );

    switch (termination) {
    case Cnr3DiagDsum03RecoveryTermination::present_output:
        cnr3_diag_saturating_increment(stats.terminated_on_present_output);
        break;
    case Cnr3DiagDsum03RecoveryTermination::frame0:
        cnr3_diag_saturating_increment(stats.terminated_on_frame0);
        break;
    case Cnr3DiagDsum03RecoveryTermination::bound:
        cnr3_diag_saturating_increment(stats.terminated_on_bound);
        break;
    case Cnr3DiagDsum03RecoveryTermination::failure:
    default:
        cnr3_diag_saturating_increment(stats.terminated_on_failure);
        break;
    }
}

void cnr3_diag_dsum03_observe_holes_filled(
    Cnr3DiagDsum03RecoverySearchStats& stats,
    std::size_t hole_count
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_add(stats.holes_filled, static_cast<std::uint64_t>(hole_count));
}

Cnr3DiagDsum03RecoverySearchSnapshot cnr3_diag_dsum03_snapshot_recovery_search(
    const Cnr3DiagDsum03RecoverySearchStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum03RecoverySearchSnapshot snapshot{};
    snapshot.search_attempts = stats.search_attempts;
    snapshot.search_successes = stats.search_successes;
    snapshot.search_failures = stats.search_failures;
    snapshot.terminated_on_present_output = stats.terminated_on_present_output;
    snapshot.terminated_on_frame0 = stats.terminated_on_frame0;
    snapshot.terminated_on_bound = stats.terminated_on_bound;
    snapshot.terminated_on_failure = stats.terminated_on_failure;
    snapshot.holes_filled = stats.holes_filled;

    for (std::size_t i = 0U; i < CNR3_DIAG_DSUM03_DEPTH_HISTOGRAM_BIN_COUNT; ++i) {
        snapshot.depth_histogram[i] = stats.depth_histogram[i];
    }

    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM03_RECOVERY_SEARCH)

void cnr3_diag_dsum03_write_recovery_search_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum03RecoverySearchStats& stats
) noexcept {
    const Cnr3DiagDsum03RecoverySearchSnapshot snapshot =
        cnr3_diag_dsum03_snapshot_recovery_search(stats);

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-03",
        "[DSUM-SUMMARY] D-SUM-03 recovery-search summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-03",
        "[DSUM-SUMMARY] D-SUM-03 interpretation: deep search is not automatically bad; repeated deep search may indicate retention/prune/workload pressure"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "search_attempts", snapshot.search_attempts);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "search_successes", snapshot.search_successes);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "search_failures", snapshot.search_failures);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "depth_hist_0", snapshot.depth_histogram[0]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "depth_hist_1", snapshot.depth_histogram[1]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "depth_hist_2_to_5", snapshot.depth_histogram[2]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "depth_hist_6_to_15", snapshot.depth_histogram[3]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "depth_hist_16_to_30", snapshot.depth_histogram[4]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "depth_hist_31_to_50", snapshot.depth_histogram[5]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "terminated_on_present_output", snapshot.terminated_on_present_output);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "terminated_on_frame0", snapshot.terminated_on_frame0);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "terminated_on_bound", snapshot.terminated_on_bound);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "terminated_on_failure", snapshot.terminated_on_failure);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-03", "D-SUM-03", "holes_filled", snapshot.holes_filled);

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)

namespace {

    void cnr3_diag_dsum12_observe_frame_total_locked(
        Cnr3DiagDsum12RecoveryPlanStats& stats
    ) noexcept {
        cnr3_diag_saturating_increment(stats.frames_total);
    }

} // namespace

void cnr3_diag_dsum12_observe_branch_cache_hit(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_dsum12_observe_frame_total_locked(stats);
    cnr3_diag_saturating_increment(stats.frames_cache_hit);
}

void cnr3_diag_dsum12_observe_branch_frame0(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_dsum12_observe_frame_total_locked(stats);
    cnr3_diag_saturating_increment(stats.frames_frame0);
}

void cnr3_diag_dsum12_observe_branch_pred_present(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_dsum12_observe_frame_total_locked(stats);
    cnr3_diag_saturating_increment(stats.frames_pred_present);
}

void cnr3_diag_dsum12_observe_recovery_plan_published(
    Cnr3DiagDsum12RecoveryPlanStats& stats,
    bool exact_anchor_recovery,
    bool floor_fresh_start_recovery,
    bool nearest_present_output_found,
    std::size_t hole_count,
    int exact_anchor_span
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    cnr3_diag_dsum12_observe_frame_total_locked(stats);
    cnr3_diag_saturating_increment(stats.recovery_plans_created);
    cnr3_diag_saturating_add(stats.holes_identified, static_cast<std::uint64_t>(hole_count));
    cnr3_diag_saturating_add(
        stats.source_frames_for_holes_requested,
        static_cast<std::uint64_t>(hole_count)
    );

    if (nearest_present_output_found) {
        cnr3_diag_saturating_increment(stats.nearest_present_output_found);
    }

    if (exact_anchor_recovery) {
        cnr3_diag_saturating_increment(stats.frames_recovered_exact);
        const int clean_span = cnr3_diag_clamp_nonnegative_depth(exact_anchor_span);
        cnr3_diag_saturating_add(stats.recovery_span_sum, static_cast<std::uint64_t>(clean_span));
        cnr3_diag_saturating_increment(stats.recovery_span_samples);
        if (clean_span > stats.recovery_span_max) {
            stats.recovery_span_max = clean_span;
        }
    }
    else if (floor_fresh_start_recovery) {
        cnr3_diag_saturating_increment(stats.frames_recovered_floor);
    }
}

void cnr3_diag_dsum12_observe_recovery_plan_destroyed(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.recovery_plans_destroyed);
}

void cnr3_diag_dsum12_observe_holes_filled(
    Cnr3DiagDsum12RecoveryPlanStats& stats,
    std::size_t hole_count
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_add(stats.holes_filled, static_cast<std::uint64_t>(hole_count));
}

void cnr3_diag_dsum12_observe_hole_source_retrieved(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.source_frames_for_holes_retrieved);
}

void cnr3_diag_dsum12_observe_fallback_failure(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.fallback_failures);
}

void cnr3_diag_dsum12_observe_bounded_start_honesty_failure(
    Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.bounded_start_honesty_failures);
}

Cnr3DiagDsum12RecoveryPlanSnapshot cnr3_diag_dsum12_snapshot_recovery_plan(
    const Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum12RecoveryPlanSnapshot snapshot{};
    snapshot.recovery_plans_created = stats.recovery_plans_created;
    snapshot.recovery_plans_destroyed = stats.recovery_plans_destroyed;
    snapshot.nearest_present_output_found = stats.nearest_present_output_found;
    snapshot.holes_identified = stats.holes_identified;
    snapshot.holes_filled = stats.holes_filled;
    snapshot.source_frames_for_holes_requested = stats.source_frames_for_holes_requested;
    snapshot.source_frames_for_holes_retrieved = stats.source_frames_for_holes_retrieved;
    snapshot.fallback_failures = stats.fallback_failures;
    snapshot.bounded_start_honesty_failures = stats.bounded_start_honesty_failures;
    snapshot.frames_total = stats.frames_total;
    snapshot.frames_cache_hit = stats.frames_cache_hit;
    snapshot.frames_pred_present = stats.frames_pred_present;
    snapshot.frames_frame0 = stats.frames_frame0;
    snapshot.frames_recovered_exact = stats.frames_recovered_exact;
    snapshot.frames_recovered_floor = stats.frames_recovered_floor;
    snapshot.recovery_span_sum = stats.recovery_span_sum;
    snapshot.recovery_span_samples = stats.recovery_span_samples;
    snapshot.recovery_span_max = stats.recovery_span_max;
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)

void cnr3_diag_dsum12_write_recovery_plan_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum12RecoveryPlanStats& stats
) noexcept {
    const Cnr3DiagDsum12RecoveryPlanSnapshot snapshot =
        cnr3_diag_dsum12_snapshot_recovery_plan(stats);

    const long long recovery_plan_balance =
        static_cast<long long>(snapshot.recovery_plans_created) -
        static_cast<long long>(snapshot.recovery_plans_destroyed);
    const std::uint64_t recovered_total =
        snapshot.frames_recovered_exact + snapshot.frames_recovered_floor;
    const double recovery_rate_percent =
        snapshot.frames_total != 0U
        ? (static_cast<double>(recovered_total) * 100.0) /
            static_cast<double>(snapshot.frames_total)
        : 0.0;
    const double recovery_span_mean =
        snapshot.recovery_span_samples != 0U
        ? static_cast<double>(snapshot.recovery_span_sum) /
            static_cast<double>(snapshot.recovery_span_samples)
        : 0.0;

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-12",
        "[DSUM-SUMMARY] D-SUM-12 recovery-plan/rate summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-12",
        "[DSUM-SUMMARY] D-SUM-12 interpretation: plan create/destroy must balance; recovery rate distinguishes inherent arrival churn from tunable cache churn"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_plans_created", snapshot.recovery_plans_created);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_plans_destroyed", snapshot.recovery_plans_destroyed);
    cnr3_diag_write_int64_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_plan_balance", recovery_plan_balance);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "nearest_present_output_found", snapshot.nearest_present_output_found);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "holes_identified", snapshot.holes_identified);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "holes_filled", snapshot.holes_filled);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "source_frames_for_holes_requested", snapshot.source_frames_for_holes_requested);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "source_frames_for_holes_retrieved", snapshot.source_frames_for_holes_retrieved);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "fallback_failures", snapshot.fallback_failures);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "bounded_start_honesty_failures", snapshot.bounded_start_honesty_failures);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "frames_total", snapshot.frames_total);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "frames_cache_hit", snapshot.frames_cache_hit);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "frames_pred_present", snapshot.frames_pred_present);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "frames_frame0", snapshot.frames_frame0);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "frames_recovered_exact", snapshot.frames_recovered_exact);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "frames_recovered_floor", snapshot.frames_recovered_floor);
    cnr3_diag_write_double_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_rate_percent", recovery_rate_percent);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_span_sum", snapshot.recovery_span_sum);
    cnr3_diag_write_int_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_span_max", snapshot.recovery_span_max);
    cnr3_diag_write_double_row(instance_id, "D-SUM-12", "D-SUM-12", "recovery_span_mean", recovery_span_mean);

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)

namespace {

    [[nodiscard]] std::size_t cnr3_diag_dsum13_depth_bin(
        int depth
    ) noexcept {
        const int clean_depth = cnr3_diag_clamp_nonnegative_depth(depth);

        if (clean_depth <= 1) {
            return static_cast<std::size_t>(clean_depth);
        }

        if (clean_depth <= 5) {
            return 2U;
        }

        if (clean_depth <= 15) {
            return 3U;
        }

        if (clean_depth <= 30) {
            return 4U;
        }

        if (clean_depth <= 50) {
            return 5U;
        }

        return 6U;
    }

    [[nodiscard]] std::size_t cnr3_diag_dsum13_hash_frame_number(
        int frame_number
    ) noexcept {
        const std::uint32_t key = static_cast<std::uint32_t>(frame_number);
        return static_cast<std::size_t>(key * 2654435761UL) %
            CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY;
    }

    void cnr3_diag_dsum13_record_recalculation_locked(
        Cnr3DiagDsum13RecalculationStats& stats,
        int depth,
        std::uint16_t prior_compute_count
    ) noexcept {
        cnr3_diag_saturating_increment(stats.recalculated_frame_count);
        cnr3_diag_saturating_increment(
            stats.recalculation_depth_histogram[cnr3_diag_dsum13_depth_bin(depth)]
        );

        const int clean_depth = cnr3_diag_clamp_nonnegative_depth(depth);
        if (clean_depth > stats.max_recalculation_depth) {
            stats.max_recalculation_depth = clean_depth;
        }

        if (prior_compute_count == 1U) {
            cnr3_diag_saturating_increment(stats.frames_recalculated_once);
        }
        else if (prior_compute_count == 2U) {
            cnr3_diag_saturating_decrement(stats.frames_recalculated_once);
            cnr3_diag_saturating_increment(stats.frames_recalculated_multiple_times);
        }
    }

} // namespace

void cnr3_diag_dsum13_observe_compute_completion(
    Cnr3DiagDsum13RecalculationStats& stats,
    int frame_number,
    int recalculation_depth
) noexcept {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return;
    }

    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.compute_observations_total);

    const std::size_t start_index = cnr3_diag_dsum13_hash_frame_number(frame_number);
    for (std::size_t probe = 0U;
        probe < CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY;
        ++probe
        ) {
        const std::size_t index =
            (start_index + probe) % CNR3_DIAG_DSUM13_COMPUTE_COUNT_MAP_CAPACITY;
        Cnr3DiagDsum13ComputeCountEntry& entry = stats.compute_count_map[index];

        if (!entry.occupied) {
            entry.occupied = true;
            entry.frame_number = frame_number;
            entry.compute_count = 1U;
            entry.max_depth = static_cast<std::uint8_t>(
                cnr3_diag_clamp_nonnegative_depth(recalculation_depth) > 255
                ? 255
                : cnr3_diag_clamp_nonnegative_depth(recalculation_depth)
            );
            return;
        }

        if (entry.frame_number == frame_number) {
            const std::uint16_t prior_compute_count = entry.compute_count;
            if (entry.compute_count < UINT16_MAX) {
                ++entry.compute_count;
            }

            const int clean_depth = cnr3_diag_clamp_nonnegative_depth(recalculation_depth);
            if (clean_depth > static_cast<int>(entry.max_depth)) {
                entry.max_depth = static_cast<std::uint8_t>(clean_depth > 255 ? 255 : clean_depth);
            }

            cnr3_diag_dsum13_record_recalculation_locked(
                stats,
                recalculation_depth,
                prior_compute_count
            );
            return;
        }
    }

    stats.compute_count_map_saturated = true;
    cnr3_diag_saturating_increment(stats.compute_count_observations_dropped);
}

Cnr3DiagDsum13RecalculationSnapshot cnr3_diag_dsum13_snapshot_recalculation(
    const Cnr3DiagDsum13RecalculationStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum13RecalculationSnapshot snapshot{};
    snapshot.recalculated_frame_count = stats.recalculated_frame_count;
    snapshot.max_recalculation_depth = stats.max_recalculation_depth;
    snapshot.frames_recalculated_once = stats.frames_recalculated_once;
    snapshot.frames_recalculated_multiple_times = stats.frames_recalculated_multiple_times;
    snapshot.compute_observations_total = stats.compute_observations_total;
    snapshot.compute_count_map_saturated = stats.compute_count_map_saturated;
    snapshot.compute_count_observations_dropped = stats.compute_count_observations_dropped;

    for (std::size_t i = 0U; i < CNR3_DIAG_DSUM13_RECALC_DEPTH_HISTOGRAM_BIN_COUNT; ++i) {
        snapshot.recalculation_depth_histogram[i] = stats.recalculation_depth_histogram[i];
    }

    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)

void cnr3_diag_dsum13_write_recalculation_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum13RecalculationStats& stats
) noexcept {
    const Cnr3DiagDsum13RecalculationSnapshot snapshot =
        cnr3_diag_dsum13_snapshot_recalculation(stats);

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-13",
        "[DSUM-SUMMARY] D-SUM-13 recalculation summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-13",
        "[DSUM-SUMMARY] D-SUM-13 interpretation: recalculation with clean ownership is not failure; repeated/deep recalculation may indicate retention/prune pressure"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalculated_frame_count", snapshot.recalculated_frame_count);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_0", snapshot.recalculation_depth_histogram[0]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_1", snapshot.recalculation_depth_histogram[1]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_2_to_5", snapshot.recalculation_depth_histogram[2]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_6_to_15", snapshot.recalculation_depth_histogram[3]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_16_to_30", snapshot.recalculation_depth_histogram[4]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_31_to_50", snapshot.recalculation_depth_histogram[5]);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "recalc_depth_hist_51_plus", snapshot.recalculation_depth_histogram[6]);
    cnr3_diag_write_int_row(instance_id, "D-SUM-13", "D-SUM-13", "max_recalculation_depth", snapshot.max_recalculation_depth);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "frames_recalculated_once", snapshot.frames_recalculated_once);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "frames_recalculated_multiple_times", snapshot.frames_recalculated_multiple_times);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "compute_observations_total", snapshot.compute_observations_total);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "compute_count_map_capacity", static_cast<std::uint64_t>(snapshot.compute_count_map_capacity));
    cnr3_diag_write_bool_row(instance_id, "D-SUM-13", "D-SUM-13", "compute_count_map_saturated", snapshot.compute_count_map_saturated);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-13", "D-SUM-13", "compute_count_observations_dropped", snapshot.compute_count_observations_dropped);
    if (snapshot.compute_count_map_saturated) {
        cnr3_diag_write_text_line(
            instance_id,
            "D-SUM-13",
            "[DSUM-SUMMARY] D-SUM-13 saturated true; recalculation counts are lower bounds"
        );
    }

    cnr3_diag_flush_stderr();
}

#endif
