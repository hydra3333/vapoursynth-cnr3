#pragma once

#include "cnr3_common.h"

#include <cstddef>
#include <cstdint>
#include <mutex>

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
