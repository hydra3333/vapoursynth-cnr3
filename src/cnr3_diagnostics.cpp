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

    void cnr3_diag_saturating_increment(
        std::uint64_t& value
    ) noexcept {
        if (value < UINT64_MAX) {
            ++value;
        }
    }

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
