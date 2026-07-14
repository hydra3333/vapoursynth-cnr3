#include "cnr3_diagnostics.h"
#include "cnr3_build_config.h"
#include "cnr3_cache_core.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

    [[nodiscard]] const char* cnr3_diag_safe_text(
        const char* text
    ) noexcept {
        return (text != nullptr) ? text : "(null)";
    }


    [[nodiscard]] bool cnr3_diag_frame_in_plantrace_window(
        int frame_number
    ) noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        return frame_number >= CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME &&
            frame_number <= CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME;
#else
        (void)frame_number;
        return false;
#endif
    }

    [[nodiscard]] bool cnr3_diag_plantrace_frame_is_checkpoint(
        int frame_number
    ) noexcept {
        return frame_number == 0 ||
            (frame_number > 0 && (frame_number % CNR3_CACHE_CHECKPOINT_INTERVAL) == 0);
    }

    [[nodiscard]] std::uint64_t cnr3_diag_decimal_digits_u64(
        std::uint64_t value
    ) noexcept {
        std::uint64_t digits = 1;

        while (value >= 10U) {
            value /= 10U;
            ++digits;
        }

        return digits;
    }

    [[nodiscard]] std::uint64_t cnr3_diag_abs_int_as_u64(
        int value
    ) noexcept {
        if (value >= 0) {
            return static_cast<std::uint64_t>(value);
        }

        return static_cast<std::uint64_t>(-(static_cast<long long>(value)));
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


#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)

namespace {

    [[nodiscard]] const char* cnr3_diag_plantrace_phase_name(
        Cnr3DiagPlanTracePhase phase
    ) noexcept {
        switch (phase) {
        case Cnr3DiagPlanTracePhase::open:
            return "O";
        case Cnr3DiagPlanTracePhase::result:
            return "R";
        }

        return "?";
    }

    [[nodiscard]] const char* cnr3_diag_plantrace_strategy_name(
        Cnr3DiagPlanTraceStrategy strategy
    ) noexcept {
        switch (strategy) {
        case Cnr3DiagPlanTraceStrategy::cache_hit:
            return "CACHE_HIT";
        case Cnr3DiagPlanTraceStrategy::frame0:
            return "FRAME0";
        case Cnr3DiagPlanTraceStrategy::predecessor_present:
            return "PRED_PRESENT";
        case Cnr3DiagPlanTraceStrategy::recovery_exact:
            return "RECOVERY_EXACT";
        case Cnr3DiagPlanTraceStrategy::recovery_floor:
            return "RECOVERY_FLOOR";
        case Cnr3DiagPlanTraceStrategy::none:
        default:
            return "NONE";
        }
    }

    [[nodiscard]] const char* cnr3_diag_plantrace_outcome_name(
        Cnr3DiagPlanTraceOutcome outcome
    ) noexcept {
        switch (outcome) {
        case Cnr3DiagPlanTraceOutcome::returned_cache_hit:
            return "RETURNED_CACHE_HIT";
        case Cnr3DiagPlanTraceOutcome::returned_computed:
            return "RETURNED_COMPUTED";
        case Cnr3DiagPlanTraceOutcome::returned_recovered:
            return "RETURNED_RECOVERED";
        case Cnr3DiagPlanTraceOutcome::failed:
            return "FAILED";
        case Cnr3DiagPlanTraceOutcome::none:
        default:
            return "NONE";
        }
    }

    [[nodiscard]] const char* cnr3_diag_plantrace_fail_reason_name(
        Cnr3DiagPlanTraceFailReason reason
    ) noexcept {
        switch (reason) {
        case Cnr3DiagPlanTraceFailReason::copyframe_failed:
            return "COPYFRAME_FAILED";
        case Cnr3DiagPlanTraceFailReason::copyframe_source_alias:
            return "COPYFRAME_SOURCE_ALIAS";
        case Cnr3DiagPlanTraceFailReason::source_retrieval_failed:
            return "SOURCE_RETRIEVAL_FAILED";
        case Cnr3DiagPlanTraceFailReason::source_not_requested:
            return "SOURCE_NOT_REQUESTED";
        case Cnr3DiagPlanTraceFailReason::acquire_ref_failed:
            return "ACQUIRE_REF_FAILED";
        case Cnr3DiagPlanTraceFailReason::adopt_failed:
            return "ADOPT_FAILED";
        case Cnr3DiagPlanTraceFailReason::store_prune_failed:
            return "STORE_PRUNE_FAILED";
        case Cnr3DiagPlanTraceFailReason::discharge_failed:
            return "DISCHARGE_FAILED";
        case Cnr3DiagPlanTraceFailReason::invalid_lifecycle:
            return "INVALID_LIFECYCLE";
        case Cnr3DiagPlanTraceFailReason::invalid_branch_foundation:
            return "INVALID_BRANCH_FOUNDATION";
        case Cnr3DiagPlanTraceFailReason::scene_processing_failed:
            return "SCENE_PROCESSING_FAILED";
        case Cnr3DiagPlanTraceFailReason::byte_estimate_failed:
            return "BYTE_ESTIMATE_FAILED";
        case Cnr3DiagPlanTraceFailReason::framedata_missing_or_unknown:
            return "FRAMEDATA_MISSING_OR_UNKNOWN";
        case Cnr3DiagPlanTraceFailReason::allocation_failed:
            return "ALLOCATION_FAILED";
        case Cnr3DiagPlanTraceFailReason::recovery_plan_failed_or_refused:
            return "RECOVERY_PLAN_FAILED_OR_REFUSED";
        case Cnr3DiagPlanTraceFailReason::hot_zone_observation_failed:
            return "HOT_ZONE_OBSERVATION_FAILED";
        case Cnr3DiagPlanTraceFailReason::none:
        default:
            return "NONE";
        }
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_unsigned(
        std::uint64_t value,
        int width
    ) {
        char text[64] = {};
        std::snprintf(
            text,
            sizeof(text),
            "%0*llu",
            width,
            static_cast<unsigned long long>(value)
        );
        text[sizeof(text) - 1U] = '\0';
        return std::string{text};
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_key_frame(
        int frame_number,
        int width
    ) {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return "<none>";
        }

        return cnr3_diag_plantrace_format_unsigned(
            static_cast<std::uint64_t>(frame_number),
            width
        );
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_role_frame(
        int frame_number,
        bool include_checkpoint_marker
    ) {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return "<none>";
        }

        std::string text = std::to_string(frame_number);

        if (include_checkpoint_marker &&
            cnr3_diag_plantrace_frame_is_checkpoint(frame_number)) {
            text += "*";
        }

        return text;
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_frame_list(
        const std::vector<int>& frames
    ) {
        std::string text{"["};

        for (std::size_t i = 0U; i < frames.size(); ++i) {
            if (i != 0U) {
                text += ",";
            }

            text += cnr3_diag_plantrace_format_role_frame(frames[i], true);
        }

        text += "]";
        return text;
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_single_frame_list(
        int frame_number
    ) {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return "[]";
        }

        return cnr3_diag_plantrace_format_frame_list(
            std::vector<int>{ frame_number }
        );
    }

    void cnr3_diag_plantrace_add_code(
        std::vector<std::pair<int, std::string>>& codes,
        int frame_number,
        char code
    ) {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return;
        }

        for (auto& entry : codes) {
            if (entry.first == frame_number) {
                if (entry.second.find(code) == std::string::npos) {
                    entry.second += code;
                }
                return;
            }
        }

        codes.push_back(std::make_pair(frame_number, std::string(1U, code)));
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_codes(
        std::vector<std::pair<int, std::string>> codes
    ) {
        std::sort(
            codes.begin(),
            codes.end(),
            [](const auto& left, const auto& right) noexcept {
                return left.first < right.first;
            }
        );

        std::string text{"["};

        for (std::size_t i = 0U; i < codes.size(); ++i) {
            if (i != 0U) {
                text += ",";
            }

            text += cnr3_diag_plantrace_format_role_frame(codes[i].first, true);
            text += "=";
            text += codes[i].second;
        }

        text += "]";
        return text;
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_open_codes(
        const Cnr3DiagPlanTraceOpenFields& open
    ) {
        std::vector<std::pair<int, std::string>> codes{};

        cnr3_diag_plantrace_add_code(codes, open.target_frame, 'T');
        cnr3_diag_plantrace_add_code(codes, open.predecessor_frame, 'P');
        cnr3_diag_plantrace_add_code(codes, open.anchor_frame, 'A');
        cnr3_diag_plantrace_add_code(codes, open.floor_frame, 'F');

        for (const int frame : open.hole_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'H');
        }
        for (const int frame : open.source_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'S');
        }
        for (const int frame : open.pinned_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'N');
        }

        return cnr3_diag_plantrace_format_codes(std::move(codes));
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_result_codes(
        const Cnr3DiagPlanTraceResultFields& result
    ) {
        std::vector<std::pair<int, std::string>> codes{};

        for (const int frame : result.computed_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'C');
        }
        for (const int frame : result.adopted_skipped_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'K');
        }
        for (const int frame : result.post_compute_loser_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'L');
        }
        for (const int frame : result.not_reached_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'X');
        }
        for (const int frame : result.error_here_frames) {
            cnr3_diag_plantrace_add_code(codes, frame, 'E');
        }

        return cnr3_diag_plantrace_format_codes(std::move(codes));
    }

    [[nodiscard]] std::string cnr3_diag_plantrace_format_utc_ms(
        long long epoch_ms
    ) {
        const std::time_t epoch_seconds =
            static_cast<std::time_t>(epoch_ms / 1000LL);
        int millisecond_part = static_cast<int>(epoch_ms % 1000LL);

        if (millisecond_part < 0) {
            millisecond_part += 1000;
        }

        std::tm utc_tm{};
#if defined(_WIN32)
        gmtime_s(&utc_tm, &epoch_seconds);
#else
        gmtime_r(&epoch_seconds, &utc_tm);
#endif

        char text[40] = {};
        std::snprintf(
            text,
            sizeof(text),
            "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
            utc_tm.tm_year + 1900,
            utc_tm.tm_mon + 1,
            utc_tm.tm_mday,
            utc_tm.tm_hour,
            utc_tm.tm_min,
            utc_tm.tm_sec,
            millisecond_part
        );
        text[sizeof(text) - 1U] = '\0';
        return std::string{text};
    }

    [[nodiscard]] long long cnr3_diag_plantrace_utc_ms_for_tick(
        Cnr3DiagPlanTraceTick tick,
        Cnr3DiagPlanTraceTick anchor_tick,
        long long anchor_epoch_ms
    ) noexcept {
        if (tick >= anchor_tick) {
            return anchor_epoch_ms +
                static_cast<long long>((tick - anchor_tick) / 1000000ULL);
        }

        return anchor_epoch_ms -
            static_cast<long long>((anchor_tick - tick) / 1000000ULL);
    }

    [[nodiscard]] int cnr3_diag_plantrace_frame_width() noexcept {
        const std::uint64_t from_abs = cnr3_diag_abs_int_as_u64(
            CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME
        );
        const std::uint64_t to_abs = cnr3_diag_abs_int_as_u64(
            CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME
        );
        const std::uint64_t max_abs = from_abs > to_abs ? from_abs : to_abs;
        const std::uint64_t digits = cnr3_diag_decimal_digits_u64(max_abs);
        return static_cast<int>(digits > 8U ? digits : 8U);
    }

    [[nodiscard]] std::uint64_t cnr3_diag_plantrace_window_frame_count() noexcept {
        if (CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME < CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME) {
            return 0U;
        }

        return static_cast<std::uint64_t>(
            CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME -
            CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME + 1
        );
    }

    [[nodiscard]] int cnr3_diag_plantrace_action_seq_width() noexcept {
        const std::uint64_t max_records =
            2ULL * cnr3_diag_plantrace_window_frame_count();
        const std::uint64_t digits = cnr3_diag_decimal_digits_u64(max_records);
        return static_cast<int>(digits > 8U ? digits : 8U);
    }

    void cnr3_diag_plantrace_write_line(
        Cnr3InstanceId instance_id,
        const std::string& text
    ) noexcept {
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            "DSUM-PLANTRACE",
            text.c_str(),
            Cnr3StderrFlushPolicy::flush
        );
    }

    void cnr3_diag_plantrace_write_legend(
        Cnr3InstanceId instance_id
    ) noexcept {
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend phase    O = plan_open"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend phase    R = plan_result"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend strategy CACHE_HIT / FRAME0 / PRED_PRESENT / RECOVERY_EXACT / RECOVERY_FLOOR   (O)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend outcome  RETURNED_CACHE_HIT / RETURNED_COMPUTED / RETURNED_RECOVERED / FAILED  (R)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   target                   the requested output frame N"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   predecessor              frame N-1 used as the compute base (PRED_PRESENT)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   anchor                   recovery start frame with a present output (RECOVERY_EXACT)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   floor                    fresh-start base frame when no anchor exists (RECOVERY_FLOOR)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   holes                    absent output frames the plan will compute/fill"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   sources                  source frames queued for fetch ([N] for non-recovery branches)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-item   pinned                   frames this plan pinned as its compute base"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend R-item   computed                 frames computed and stored this plan"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend R-item   adopted_skipped          frames found already present; pinned, compute skipped"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend R-item   post_compute_discarded   frames computed then discarded (lost the race)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-code   T = target        P = predecessor   A = anchor   F = floor"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend O-code   H = hole          S = source        N = pinned"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend R-code   C = computed      K = adopted_skipped"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend R-code   L = post_compute_discarded          N = none"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend R-code   X = not_reached                    E = error_here"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend glyph    *  checkpoint-grid frame (rides on a frame number in a role list or codes)"
        );
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] legend codes    per-frame composed roles/outcomes, e.g. codes=[6=PN,7=TS] or codes=[120=K,121=C]"
        );
    }

    void cnr3_diag_plantrace_write_record(
        Cnr3InstanceId instance_id,
        const Cnr3DiagPlanTraceRecord& record,
        Cnr3DiagPlanTraceTick anchor_tick,
        long long anchor_epoch_ms,
        int frame_width,
        int action_seq_width
    ) noexcept {
        try {
            const long long enter_epoch_ms = cnr3_diag_plantrace_utc_ms_for_tick(
                record.enter_tick,
                anchor_tick,
                anchor_epoch_ms
            );
            const long long exit_epoch_ms = cnr3_diag_plantrace_utc_ms_for_tick(
                record.exit_tick,
                anchor_tick,
                anchor_epoch_ms
            );
            const double run_ms =
                record.exit_tick >= record.enter_tick
                ? static_cast<double>(record.exit_tick - record.enter_tick) / 1000000.0
                : 0.0;

            const std::string enter_tick_text = cnr3_diag_plantrace_format_unsigned(
                record.enter_tick,
                20
            );
            const std::string exit_tick_text = cnr3_diag_plantrace_format_unsigned(
                record.exit_tick,
                20
            );
            const std::string seq_text = cnr3_diag_plantrace_format_unsigned(
                record.action_seq,
                action_seq_width
            );
            const std::string frame_text = cnr3_diag_plantrace_format_key_frame(
                record.frame_number,
                frame_width
            );
            const std::string enter_utc = cnr3_diag_plantrace_format_utc_ms(enter_epoch_ms);
            const std::string exit_utc = cnr3_diag_plantrace_format_utc_ms(exit_epoch_ms);
            char run_text[48] = {};
            std::snprintf(run_text, sizeof(run_text), "%.3f", run_ms);
            run_text[sizeof(run_text) - 1U] = '\0';

            std::string line{"[DSUM-PLANTRACE] "};
            line += cnr3_diag_plantrace_phase_name(record.phase);
            line += " enter_tick=";
            line += enter_tick_text;
            line += " seq=";
            line += seq_text;
            line += " frame=";
            line += frame_text;
            line += " exit_tick=";
            line += exit_tick_text;

            if (record.phase == Cnr3DiagPlanTracePhase::open) {
                line += " strategy=";
                line += cnr3_diag_plantrace_strategy_name(record.open.strategy);
                line += " enter_utc=";
                line += enter_utc;
                line += " exit_utc=";
                line += exit_utc;
                line += " run_ms=";
                line += run_text;
                line += " target=";
                line += cnr3_diag_plantrace_format_single_frame_list(record.open.target_frame);
                line += " predecessor=";
                line += cnr3_diag_plantrace_format_single_frame_list(record.open.predecessor_frame);
                line += " anchor=";
                line += cnr3_diag_plantrace_format_single_frame_list(record.open.anchor_frame);
                line += " floor=";
                line += cnr3_diag_plantrace_format_single_frame_list(record.open.floor_frame);
                line += " holes=";
                line += cnr3_diag_plantrace_format_frame_list(record.open.hole_frames);
                line += " sources=";
                line += cnr3_diag_plantrace_format_frame_list(record.open.source_frames);
                line += " pinned=";
                line += cnr3_diag_plantrace_format_frame_list(record.open.pinned_frames);
                line += " codes=";
                line += cnr3_diag_plantrace_open_codes(record.open);
                cnr3_diag_plantrace_write_line(instance_id, line);
                return;
            }

            line += " outcome=";
            line += cnr3_diag_plantrace_outcome_name(record.result.outcome);
            if (record.result.outcome == Cnr3DiagPlanTraceOutcome::failed) {
                line += " fail_reason=";
                line += cnr3_diag_plantrace_fail_reason_name(record.result.fail_reason);
            }
            line += " enter_utc=";
            line += enter_utc;
            line += " exit_utc=";
            line += exit_utc;
            line += " run_ms=";
            line += run_text;
            line += " computed=";
            line += cnr3_diag_plantrace_format_frame_list(record.result.computed_frames);
            line += " adopted_skipped=";
            line += cnr3_diag_plantrace_format_frame_list(record.result.adopted_skipped_frames);
            line += " post_compute_discarded=";
            line += cnr3_diag_plantrace_format_frame_list(record.result.post_compute_loser_frames);
            line += " codes=";
            line += cnr3_diag_plantrace_result_codes(record.result);
            cnr3_diag_plantrace_write_line(instance_id, line);
        }
        catch (...) {
            cnr3_diag_plantrace_write_line(
                instance_id,
                "[DSUM-PLANTRACE] formatting_error"
            );
        }
    }

} // namespace

Cnr3DiagPlanTraceTick cnr3_diag_plantrace_sample_tick() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<Cnr3DiagPlanTraceTick>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
    );
}

void cnr3_diag_plantrace_observe_open(
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    Cnr3DiagPlanTraceTick exit_tick,
    const Cnr3DiagPlanTraceOpenFields& fields
) noexcept {
    if (!cnr3_diag_frame_in_plantrace_window(frame_number)) {
        return;
    }

    Cnr3DiagPlanTraceRecord record{};
    record.phase = Cnr3DiagPlanTracePhase::open;
    record.frame_number = frame_number;
    record.enter_tick = enter_tick;
    record.exit_tick = exit_tick;
    record.open = fields;

    try {
        std::lock_guard<std::mutex> lock(buffer.mutex);

        if (!buffer.time_anchor_set) {
            const auto system_now = std::chrono::system_clock::now().time_since_epoch();
            buffer.system_anchor_epoch_ms = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(system_now).count()
            );
            buffer.steady_anchor_tick = enter_tick;
            buffer.time_anchor_set = true;
        }

        if (buffer.records.capacity() == 0U) {
            buffer.records.reserve(
                static_cast<std::size_t>(2ULL * cnr3_diag_plantrace_window_frame_count())
            );
        }

        record.action_seq = ++buffer.next_action_seq;
        buffer.records.push_back(std::move(record));
    }
    catch (...) {
        std::lock_guard<std::mutex> lock(buffer.mutex);
        buffer.reserve_failed = true;
    }
}

void cnr3_diag_plantrace_observe_result(
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    Cnr3DiagPlanTraceTick exit_tick,
    const Cnr3DiagPlanTraceResultFields& fields
) noexcept {
    if (!cnr3_diag_frame_in_plantrace_window(frame_number)) {
        return;
    }

    Cnr3DiagPlanTraceRecord record{};
    record.phase = Cnr3DiagPlanTracePhase::result;
    record.frame_number = frame_number;
    record.enter_tick = enter_tick;
    record.exit_tick = exit_tick;
    record.result = fields;

    try {
        std::lock_guard<std::mutex> lock(buffer.mutex);

        if (!buffer.time_anchor_set) {
            const auto system_now = std::chrono::system_clock::now().time_since_epoch();
            buffer.system_anchor_epoch_ms = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(system_now).count()
            );
            buffer.steady_anchor_tick = enter_tick;
            buffer.time_anchor_set = true;
        }

        if (buffer.records.capacity() == 0U) {
            buffer.records.reserve(
                static_cast<std::size_t>(2ULL * cnr3_diag_plantrace_window_frame_count())
            );
        }

        record.action_seq = ++buffer.next_action_seq;
        buffer.records.push_back(std::move(record));
    }
    catch (...) {
        std::lock_guard<std::mutex> lock(buffer.mutex);
        buffer.reserve_failed = true;
    }
}

void cnr3_diag_plantrace_observe_failed_result_and_dump(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    const Cnr3DiagPlanTraceResultFields& fields
) noexcept {
    cnr3_diag_plantrace_observe_result(
        buffer,
        frame_number,
        enter_tick,
        cnr3_diag_plantrace_sample_tick(),
        fields
    );
#if defined(CNR3_DIAG_PRINT_DSUM_PLANTRACE)
    cnr3_diag_plantrace_write_bail_dump_to_stderr(instance_id, buffer);
#endif
}

void cnr3_diag_plantrace_observe_minimal_failed_and_dump(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer,
    int frame_number,
    Cnr3DiagPlanTraceFailReason fail_reason,
    int error_frame
) noexcept {
    Cnr3DiagPlanTraceResultFields fields{};
    fields.outcome = Cnr3DiagPlanTraceOutcome::failed;
    fields.fail_reason = fail_reason;
    if (cnr3_frame_number_is_valid(error_frame)) {
        fields.error_here_frames.push_back(error_frame);
    }

    const Cnr3DiagPlanTraceTick enter_tick = cnr3_diag_plantrace_sample_tick();
    cnr3_diag_plantrace_observe_failed_result_and_dump(
        instance_id,
        buffer,
        frame_number,
        enter_tick,
        fields
    );
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM_PLANTRACE)

namespace {

void cnr3_diag_plantrace_write_dump_to_stderr(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer
) noexcept {
    std::vector<Cnr3DiagPlanTraceRecord> records{};
    Cnr3DiagPlanTraceTick anchor_tick = 0;
    long long anchor_epoch_ms = 0;
    bool reserve_failed = false;
    bool already_dumped = false;

    try {
        std::lock_guard<std::mutex> lock(buffer.mutex);
        already_dumped = buffer.dumped;

        if (!buffer.dumped) {
            records = buffer.records;
            anchor_tick = buffer.steady_anchor_tick;
            anchor_epoch_ms = buffer.system_anchor_epoch_ms;
            reserve_failed = buffer.reserve_failed;
            buffer.dumped = true;
        }
    }
    catch (...) {
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] snapshot_error"
        );
        return;
    }

    if (already_dumped) {
        return;
    }

    const int frame_width = cnr3_diag_plantrace_frame_width();
    const int action_seq_width = cnr3_diag_plantrace_action_seq_width();
    const std::string record_count = std::to_string(records.size());

    cnr3_diag_plantrace_write_line(
        instance_id,
        std::string{"[DSUM-PLANTRACE] BEGIN schema=3c1v1 instance="} +
        std::to_string(instance_id.value) +
        " window=[" + std::to_string(CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME) +
        "," + std::to_string(CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME) +
        "] records=" + record_count
    );

    cnr3_diag_plantrace_write_legend(instance_id);

    if (reserve_failed) {
        cnr3_diag_plantrace_write_line(
            instance_id,
            "[DSUM-PLANTRACE] warning reserve_or_append_failed records_may_be_incomplete"
        );
    }

    for (const Cnr3DiagPlanTraceRecord& record : records) {
        cnr3_diag_plantrace_write_record(
            instance_id,
            record,
            anchor_tick,
            anchor_epoch_ms,
            frame_width,
            action_seq_width
        );
    }

    cnr3_diag_plantrace_write_line(
        instance_id,
        std::string{"[DSUM-PLANTRACE] END   schema=3c1v1 instance="} +
        std::to_string(instance_id.value) +
        " records=" + record_count +
        " truncated=" + (reserve_failed ? "1" : "0")
    );
    cnr3_diag_flush_stderr();
}

} // namespace

void cnr3_diag_plantrace_write_clean_end_dump_to_stderr(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer
) noexcept {
    cnr3_diag_plantrace_write_dump_to_stderr(instance_id, buffer);
}

void cnr3_diag_plantrace_write_bail_dump_to_stderr(
    Cnr3InstanceId instance_id,
    Cnr3DiagPlanTraceBuffer& buffer
) noexcept {
    cnr3_diag_plantrace_write_dump_to_stderr(instance_id, buffer);
}
#endif


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


#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)

void cnr3_diag_dsum06_observe_source_requests(
    Cnr3DiagDsum06SourceFrameLifecycleStats& stats,
    std::size_t request_count
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_add(
        stats.source_frames_requested_total,
        static_cast<std::uint64_t>(request_count)
    );
}

void cnr3_diag_dsum06_observe_source_retrieve(
    Cnr3DiagDsum06SourceFrameLifecycleStats& stats,
    bool same_activation_requested,
    bool retrieved
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    if (!same_activation_requested) {
        cnr3_diag_saturating_increment(stats.same_activation_request_violations);
    }

    if (!retrieved) {
        cnr3_diag_saturating_increment(stats.partial_acquire_failures);
        return;
    }

    cnr3_diag_saturating_increment(stats.source_frames_retrieved_total);
    cnr3_diag_saturating_increment(stats.current_source_frame_count);

    if (stats.current_source_frame_count > stats.source_frame_count_max) {
        stats.source_frame_count_max = stats.current_source_frame_count;
    }
}

void cnr3_diag_dsum06_observe_source_release(
    Cnr3DiagDsum06SourceFrameLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    cnr3_diag_saturating_increment(stats.source_frames_released_total);

    if (stats.current_source_frame_count == 0U) {
        cnr3_diag_saturating_increment(stats.source_frame_release_balance_errors);
        return;
    }

    cnr3_diag_saturating_decrement(stats.current_source_frame_count);
}

Cnr3DiagDsum06SourceFrameLifecycleSnapshot
cnr3_diag_dsum06_snapshot_source_frame_lifecycle(
    const Cnr3DiagDsum06SourceFrameLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum06SourceFrameLifecycleSnapshot snapshot{};
    snapshot.source_frames_requested_total = stats.source_frames_requested_total;
    snapshot.source_frames_retrieved_total = stats.source_frames_retrieved_total;
    snapshot.source_frames_released_total = stats.source_frames_released_total;
    snapshot.source_frame_release_balance =
        static_cast<long long>(stats.source_frames_retrieved_total) -
        static_cast<long long>(stats.source_frames_released_total);
    snapshot.same_activation_request_violations = stats.same_activation_request_violations;
    snapshot.source_frame_count_max = stats.source_frame_count_max;
    snapshot.partial_acquire_failures = stats.partial_acquire_failures;
    snapshot.source_frame_release_balance_errors =
        stats.source_frame_release_balance_errors;
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM06_SOURCE_FRAME_LIFECYCLE)

void cnr3_diag_dsum06_write_source_frame_lifecycle_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum06SourceFrameLifecycleStats& stats
) noexcept {
    const Cnr3DiagDsum06SourceFrameLifecycleSnapshot snapshot =
        cnr3_diag_dsum06_snapshot_source_frame_lifecycle(stats);

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-06",
        "[DSUM-SUMMARY] D-SUM-06 source-frame lifecycle summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-06",
        "[DSUM-SUMMARY] D-SUM-06 interpretation: retrieved/released must balance; retrieve without same-activation request is a lifecycle violation; partial acquire failure must be inspected even when cleanup is clean"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "source_frames_requested_total", snapshot.source_frames_requested_total);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "source_frames_retrieved_total", snapshot.source_frames_retrieved_total);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "source_frames_released_total", snapshot.source_frames_released_total);
    cnr3_diag_write_int64_row(instance_id, "D-SUM-06", "D-SUM-06", "source_frame_release_balance", snapshot.source_frame_release_balance);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "same_activation_request_violations", snapshot.same_activation_request_violations);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "source_frame_count_max", snapshot.source_frame_count_max);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "partial_acquire_failures", snapshot.partial_acquire_failures);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-06", "D-SUM-06", "source_frame_release_balance_errors", snapshot.source_frame_release_balance_errors);

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)

void cnr3_diag_dsum07_observe_temporary_output_created(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.temporary_outputs_created);
}

void cnr3_diag_dsum07_observe_temporary_output_stored(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.temporary_outputs_stored);
}

void cnr3_diag_dsum07_observe_temporary_output_released(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.temporary_outputs_released);
}

void cnr3_diag_dsum07_observe_temporary_output_transferred(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.temporary_outputs_transferred);
    cnr3_diag_saturating_increment(stats.caller_still_owns_temporary_output);
}

void cnr3_diag_dsum07_observe_duplicate_computed_but_discarded(
    Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.duplicate_computed_but_discarded);
}

Cnr3DiagDsum07TempOutputLifecycleSnapshot
cnr3_diag_dsum07_snapshot_temp_output_lifecycle(
    const Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum07TempOutputLifecycleSnapshot snapshot{};
    snapshot.temporary_outputs_created = stats.temporary_outputs_created;
    snapshot.temporary_outputs_stored = stats.temporary_outputs_stored;
    snapshot.temporary_outputs_released = stats.temporary_outputs_released;
    snapshot.temporary_outputs_transferred = stats.temporary_outputs_transferred;
    snapshot.temporary_output_balance =
        static_cast<long long>(stats.temporary_outputs_created) -
        static_cast<long long>(
            stats.temporary_outputs_stored +
            stats.temporary_outputs_released +
            stats.temporary_outputs_transferred
        );
    snapshot.caller_still_owns_temporary_output =
        stats.caller_still_owns_temporary_output;
    snapshot.duplicate_computed_but_discarded =
        stats.duplicate_computed_but_discarded;
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM07_TEMP_OUTPUT_LIFECYCLE)

void cnr3_diag_dsum07_write_temp_output_lifecycle_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum07TempOutputLifecycleStats& stats
) noexcept {
    const Cnr3DiagDsum07TempOutputLifecycleSnapshot snapshot =
        cnr3_diag_dsum07_snapshot_temp_output_lifecycle(stats);

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-07",
        "[DSUM-SUMMARY] D-SUM-07 temporary-output lifecycle summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-07",
        "[DSUM-SUMMARY] D-SUM-07 interpretation: duplicate computed/discarded may be normal under stress; clean ownership is the key question -- no leak, no double-free, no ambiguous owner, documented balance equation"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-07",
        "[DSUM-SUMMARY] D-SUM-07 note: duplicate_computed_but_discarded overlaps temporary_outputs_released; the balance remains created == stored + released + transferred"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-07", "D-SUM-07", "temporary_outputs_created", snapshot.temporary_outputs_created);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-07", "D-SUM-07", "temporary_outputs_stored", snapshot.temporary_outputs_stored);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-07", "D-SUM-07", "temporary_outputs_released", snapshot.temporary_outputs_released);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-07", "D-SUM-07", "temporary_outputs_transferred", snapshot.temporary_outputs_transferred);
    cnr3_diag_write_int64_row(instance_id, "D-SUM-07", "D-SUM-07", "temporary_output_balance", snapshot.temporary_output_balance);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-07", "D-SUM-07", "caller_still_owns_temporary_output", snapshot.caller_still_owns_temporary_output);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-07", "D-SUM-07", "duplicate_computed_but_discarded", snapshot.duplicate_computed_but_discarded);

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)

namespace {

    void cnr3_diag_dsum09_increment_no_reason_locked(
        Cnr3DiagDsum09ReturnTransferStats& stats,
        Cnr3DiagDsum09ReturnNoReason no_reason
    ) noexcept {
        switch (no_reason) {
        case Cnr3DiagDsum09ReturnNoReason::hard_store_failure:
            cnr3_diag_saturating_increment(stats.return_no_hard_store_failure);
            break;
        case Cnr3DiagDsum09ReturnNoReason::store_status_not_returnable:
            cnr3_diag_saturating_increment(stats.return_no_store_status_not_returnable);
            break;
        case Cnr3DiagDsum09ReturnNoReason::duplicate_winner_lookup_failed:
            cnr3_diag_saturating_increment(stats.return_no_duplicate_winner_lookup_failed);
            break;
        case Cnr3DiagDsum09ReturnNoReason::null_return_frame:
            cnr3_diag_saturating_increment(stats.return_no_null_return_frame);
            break;
        case Cnr3DiagDsum09ReturnNoReason::discard_failed_after_return_ready:
        default:
            cnr3_diag_saturating_increment(stats.return_no_discard_failed_after_return_ready);
            break;
        }
    }

} // namespace

void cnr3_diag_dsum09_observe_return_decision(
    Cnr3DiagDsum09ReturnTransferStats& stats,
    bool return_allowed,
    Cnr3DiagDsum09ReturnNoReason no_reason
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    cnr3_diag_saturating_increment(stats.return_decisions_checked);

    if (return_allowed) {
        cnr3_diag_saturating_increment(stats.return_decision_yes);
        return;
    }

    cnr3_diag_saturating_increment(stats.return_decision_no);
    cnr3_diag_dsum09_increment_no_reason_locked(stats, no_reason);
}

void cnr3_diag_dsum09_observe_return_no_reason(
    Cnr3DiagDsum09ReturnTransferStats& stats,
    Cnr3DiagDsum09ReturnNoReason no_reason
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_dsum09_increment_no_reason_locked(stats, no_reason);
}

void cnr3_diag_dsum09_observe_return_transfer(
    Cnr3DiagDsum09ReturnTransferStats& stats,
    bool transfer_succeeded,
    bool output_authoritative
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    cnr3_diag_saturating_increment(stats.return_transfer_attempted);
    if (transfer_succeeded) {
        cnr3_diag_saturating_increment(stats.return_transfer_succeeded);
    }
    if (output_authoritative) {
        cnr3_diag_saturating_increment(stats.output_authoritative);
    }
}

void cnr3_diag_dsum09_observe_lookup_ref_acquired(
    Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.lookup_ref_acquired);
}

void cnr3_diag_dsum09_observe_lookup_ref_transferred(
    Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.lookup_ref_transferred);
}

void cnr3_diag_dsum09_observe_lookup_ref_released(
    Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);
    cnr3_diag_saturating_increment(stats.lookup_ref_released);
}

Cnr3DiagDsum09ReturnTransferSnapshot cnr3_diag_dsum09_snapshot_return_transfer(
    const Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum09ReturnTransferSnapshot snapshot{};
    snapshot.return_decisions_checked = stats.return_decisions_checked;
    snapshot.return_decision_yes = stats.return_decision_yes;
    snapshot.return_decision_no = stats.return_decision_no;
    snapshot.return_no_hard_store_failure = stats.return_no_hard_store_failure;
    snapshot.return_no_store_status_not_returnable = stats.return_no_store_status_not_returnable;
    snapshot.return_no_duplicate_winner_lookup_failed = stats.return_no_duplicate_winner_lookup_failed;
    snapshot.return_no_null_return_frame = stats.return_no_null_return_frame;
    snapshot.return_no_discard_failed_after_return_ready = stats.return_no_discard_failed_after_return_ready;
    snapshot.return_transfer_attempted = stats.return_transfer_attempted;
    snapshot.return_transfer_succeeded = stats.return_transfer_succeeded;
    snapshot.lookup_ref_transferred = stats.lookup_ref_transferred;
    snapshot.lookup_ref_released = stats.lookup_ref_released;
    snapshot.lookup_ref_acquired = stats.lookup_ref_acquired;
    snapshot.lookup_ref_balance =
        static_cast<long long>(stats.lookup_ref_acquired) -
        static_cast<long long>(stats.lookup_ref_transferred + stats.lookup_ref_released);
    snapshot.output_authoritative = stats.output_authoritative;
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER)

void cnr3_diag_dsum09_write_return_transfer_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum09ReturnTransferStats& stats
) noexcept {
    const Cnr3DiagDsum09ReturnTransferSnapshot snapshot =
        cnr3_diag_dsum09_snapshot_return_transfer(stats);

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-09",
        "[DSUM-SUMMARY] D-SUM-09 return-transfer summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-09",
        "[DSUM-SUMMARY] D-SUM-09 interpretation: decision and transfer are separate and both accounted; yes-without-transfer needs cleanup/error accounting; lookup_ref_balance must remain zero"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_decisions_checked", snapshot.return_decisions_checked);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_decision_yes", snapshot.return_decision_yes);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_decision_no", snapshot.return_decision_no);
    cnr3_diag_write_text_line(instance_id, "D-SUM-09", "[DSUM-SUMMARY] D-SUM-09 return_no_reason_split decision-stage");
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_no_hard_store_failure", snapshot.return_no_hard_store_failure);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_no_store_status_not_returnable", snapshot.return_no_store_status_not_returnable);
    cnr3_diag_write_text_line(instance_id, "D-SUM-09", "[DSUM-SUMMARY] D-SUM-09 return_no_reason_split transfer-stage");
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_no_duplicate_winner_lookup_failed", snapshot.return_no_duplicate_winner_lookup_failed);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_no_null_return_frame", snapshot.return_no_null_return_frame);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_no_discard_failed_after_return_ready", snapshot.return_no_discard_failed_after_return_ready);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_transfer_attempted", snapshot.return_transfer_attempted);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "return_transfer_succeeded", snapshot.return_transfer_succeeded);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "lookup_ref_transferred", snapshot.lookup_ref_transferred);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "lookup_ref_released", snapshot.lookup_ref_released);
    cnr3_diag_write_int64_row(instance_id, "D-SUM-09", "D-SUM-09", "lookup_ref_balance", snapshot.lookup_ref_balance);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-09", "D-SUM-09", "output_authoritative", snapshot.output_authoritative);

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


#if \
    defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE) || \
    defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER) || \
    defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION) || \
    defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN) || \
    defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)

namespace {

    void cnr3_diag_write_health_text_row(
        Cnr3InstanceId instance_id,
        const char* label,
        const char* value
    ) noexcept {
        char message[256] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-HEALTH] %-44s %s",
            label != nullptr ? label : "(null)",
            value != nullptr ? value : "(null)"
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                "D-SUM-HEALTH",
                "[DSUM-HEALTH] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            "D-SUM-HEALTH",
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_health_double_row(
        Cnr3InstanceId instance_id,
        const char* label,
        double value
    ) noexcept {
        char message[256] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-HEALTH] %-44s %.3f",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                "D-SUM-HEALTH",
                "[DSUM-HEALTH] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            "D-SUM-HEALTH",
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_diag_write_health_disabled_row(
        Cnr3InstanceId instance_id,
        const char* label,
        const char* source_family
    ) noexcept {
        char value[64] = {};

        const int written = std::snprintf(
            value,
            sizeof(value),
            "(source %s disabled)",
            source_family != nullptr ? source_family : "D-SUM-??"
        );

        if (written < 0) {
            cnr3_diag_write_health_text_row(instance_id, label, "(source D-SUM-?? disabled)");
            return;
        }

        value[sizeof(value) - 1U] = '\0';
        cnr3_diag_write_health_text_row(instance_id, label, value);
    }

} // namespace

void cnr3_diag_write_health_ratio_row(
    Cnr3InstanceId instance_id,
    const char* label,
    std::uint64_t numerator,
    std::uint64_t denominator
) noexcept {
    if (denominator == 0U) {
        cnr3_diag_write_health_text_row(instance_id, label, "n/a");
        return;
    }

    const double value =
        (static_cast<double>(numerator) * 100.0) /
        static_cast<double>(denominator);
    cnr3_diag_write_health_double_row(instance_id, label, value);
}

void cnr3_diag_write_health_mean_row(
    Cnr3InstanceId instance_id,
    const char* label,
    std::uint64_t numerator,
    std::uint64_t denominator
) noexcept {
    if (denominator == 0U) {
        cnr3_diag_write_health_text_row(instance_id, label, "n/a");
        return;
    }

    const double value =
        static_cast<double>(numerator) /
        static_cast<double>(denominator);
    cnr3_diag_write_health_double_row(instance_id, label, value);
}

void cnr3_diag_write_derived_health_summary_to_stderr(
    Cnr3InstanceId instance_id
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    , const Cnr3CacheOwnershipDiagnosticStats* dsum04_ownership_balance
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    , const Cnr3DiagDsum09ReturnTransferStats* dsum09_return_transfer
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    , const Cnr3CachePruneDiagnosticStats* dsum10_prune_eviction
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    , const Cnr3DiagDsum12RecoveryPlanStats* dsum12_recovery_plan
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
    , const Cnr3DiagDsum13RecalculationStats* dsum13_recalculation
#endif
) noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER) && defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER)
    const Cnr3DiagDsum09ReturnTransferSnapshot dsum09 =
        dsum09_return_transfer != nullptr
        ? cnr3_diag_dsum09_snapshot_return_transfer(*dsum09_return_transfer)
        : Cnr3DiagDsum09ReturnTransferSnapshot{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    const Cnr3DiagDsum12RecoveryPlanSnapshot dsum12 =
        dsum12_recovery_plan != nullptr
        ? cnr3_diag_dsum12_snapshot_recovery_plan(*dsum12_recovery_plan)
        : Cnr3DiagDsum12RecoveryPlanSnapshot{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION) && defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)
    const Cnr3DiagDsum13RecalculationSnapshot dsum13 =
        dsum13_recalculation != nullptr
        ? cnr3_diag_dsum13_snapshot_recalculation(*dsum13_recalculation)
        : Cnr3DiagDsum13RecalculationSnapshot{};
#endif

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-HEALTH",
        "[DSUM-HEALTH] derived health ratios"
    );

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    // pred_returned_from_cache_percent = frames_pred_present / frames_total.
    // Source raw counters: D-SUM-12 recovery-plan summary (frames_pred_present, frames_total).
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "pred_returned_from_cache_percent",
        dsum12.frames_pred_present,
        dsum12.frames_total
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "pred_returned_from_cache_percent", "D-SUM-12");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    // current_frame_returned_from_cache_percent = frames_cache_hit / frames_total.
    // Source raw counters: D-SUM-12 recovery-plan summary (frames_cache_hit, frames_total).
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "current_frame_returned_from_cache_percent",
        dsum12.frames_cache_hit,
        dsum12.frames_total
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "current_frame_returned_from_cache_percent", "D-SUM-12");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    // cache_hit_percent = (frames_pred_present + frames_cache_hit) / frames_total.
    // Source raw counters: D-SUM-12 recovery-plan summary (frames_pred_present, frames_cache_hit, frames_total).
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "cache_hit_percent",
        dsum12.frames_pred_present + dsum12.frames_cache_hit,
        dsum12.frames_total
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "cache_hit_percent", "D-SUM-12");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    // cache_miss_recovery_plan_percent = (frames_recovered_exact + frames_recovered_floor) / frames_total.
    // Source raw counters: D-SUM-12 recovery-plan summary (frames_recovered_exact, frames_recovered_floor, frames_total).
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "cache_miss_recovery_plan_percent",
        dsum12.frames_recovered_exact + dsum12.frames_recovered_floor,
        dsum12.frames_total
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "cache_miss_recovery_plan_percent", "D-SUM-12");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    // recovery_plan_holes_filled_percent = holes_filled / holes_identified.
    // Source raw counters: D-SUM-12 recovery-plan summary (holes_filled, holes_identified).
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "recovery_plan_holes_filled_percent",
        dsum12.holes_filled,
        dsum12.holes_identified
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "recovery_plan_holes_filled_percent", "D-SUM-12");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER) && defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER)
    // return_to_vs_success_percent = return_transfer_succeeded / return_transfer_attempted.
    // Source raw counters: D-SUM-09 return-transfer summary (return_transfer_succeeded, return_transfer_attempted).
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "return_to_vs_success_percent",
        dsum09.return_transfer_succeeded,
        dsum09.return_transfer_attempted
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "return_to_vs_success_percent", "D-SUM-09");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION) && defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION)
    // recent_rechurn_vs_evicted_percent = frames_recently_evicted_then_re_requested / frames_evicted.
    // Source raw counters: D-SUM-10 cache summary (frames_recently_evicted_then_re_requested, frames_evicted).
    // Numerator is recency-gated (gap <= 3*BACK_RADIUS) by the prune-rechurn recency-gate fix.
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "recent_rechurn_vs_evicted_percent",
        dsum10_prune_eviction != nullptr ? dsum10_prune_eviction->frames_recently_evicted_then_re_requested : 0U,
        dsum10_prune_eviction != nullptr ? dsum10_prune_eviction->frames_evicted : 0U
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "recent_rechurn_vs_evicted_percent", "D-SUM-10");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION) && defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION)
    // frames_per_prune_event = frames_evicted / prune_events_triggered.
    // Source raw counters: D-SUM-10 cache summary (frames_evicted, prune_events_triggered); this is an average, not a percent.
    cnr3_diag_write_health_mean_row(
        instance_id,
        "frames_per_prune_event",
        dsum10_prune_eviction != nullptr ? dsum10_prune_eviction->frames_evicted : 0U,
        dsum10_prune_eviction != nullptr ? dsum10_prune_eviction->prune_events_triggered : 0U
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "frames_per_prune_event", "D-SUM-10");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE) && defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)
    // cache_lookup_hit_rate_percent = cache_lookup_hits / cache_lookup_queries_total.
    // Source raw counters: D-SUM-04 ownership-balance summary (cache_lookup_hits, cache_lookup_queries_total).
    // This is the intent-counted probe hit rate, not lookup_refs_acquired.
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "cache_lookup_hit_rate_percent",
        dsum04_ownership_balance != nullptr ? dsum04_ownership_balance->cache_lookup_hits : 0U,
        dsum04_ownership_balance != nullptr ? dsum04_ownership_balance->cache_lookup_queries_total : 0U
    );

    const std::uint64_t cache_lookup_misses =
        (
            dsum04_ownership_balance != nullptr &&
            dsum04_ownership_balance->cache_lookup_queries_total >= dsum04_ownership_balance->cache_lookup_hits
        )
        ? (
            dsum04_ownership_balance->cache_lookup_queries_total -
            dsum04_ownership_balance->cache_lookup_hits
        )
        : 0U;
    // cache_lookup_misses_percent = cache_lookup_misses / cache_lookup_queries_total.
    // Complements cache_lookup_hit_rate_percent; when live, the two sum to 100.000.
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "cache_lookup_misses_percent",
        cache_lookup_misses,
        dsum04_ownership_balance != nullptr ? dsum04_ownership_balance->cache_lookup_queries_total : 0U
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "cache_lookup_hit_rate_percent", "D-SUM-04");
    cnr3_diag_write_health_disabled_row(instance_id, "cache_lookup_misses_percent", "D-SUM-04");
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION) && defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)
    // recalc_multiple_vs_recalced_frames_percent = frames_recalculated_multiple_times / (frames_recalculated_once + frames_recalculated_multiple_times).
    // Source raw counters: D-SUM-13 recalculation summary; denominator is distinct frames, not recalculation event count.
    const std::uint64_t recalculated_distinct_frames =
        dsum13.frames_recalculated_once + dsum13.frames_recalculated_multiple_times;
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "recalc_multiple_vs_recalced_frames_percent",
        dsum13.frames_recalculated_multiple_times,
        recalculated_distinct_frames
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "recalc_multiple_vs_recalced_frames_percent", "D-SUM-13");
#endif

#if \
    defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION) && defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION) && \
    defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN) && defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    // recalced_frames_vs_total_percent = (frames_recalculated_once + frames_recalculated_multiple_times) / frames_total.
    // Source raw counters: D-SUM-13 distinct-frame buckets and D-SUM-12 frames_total; not recalculated_frame_count events.
    cnr3_diag_write_health_ratio_row(
        instance_id,
        "recalced_frames_vs_total_percent",
        recalculated_distinct_frames,
        dsum12.frames_total
    );
#else
    cnr3_diag_write_health_disabled_row(instance_id, "recalced_frames_vs_total_percent", "D-SUM-13/D-SUM-12");
#endif

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-HEALTH",
        "[DSUM-HEALTH] note: ratios are derived from raw D-SUM family counters above; cache_hit_percent <- D-SUM-12 (frames_pred_present + frames_cache_hit) / frames_total."
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-HEALTH",
        "[DSUM-HEALTH] note: cache_lookup_hit_rate_percent and cache_lookup_misses_percent use D-SUM-04 intent-counted probes; recent_rechurn_vs_evicted_percent <- D-SUM-10 frames_recently_evicted_then_re_requested / frames_evicted."
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-HEALTH",
        "[DSUM-HEALTH] note: n/a = denominator was zero (the phenomenon did not occur), distinct from a 0% rate."
    );

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)

namespace {

    [[nodiscard]] bool cnr3_diag_dsum14_is_near_checkpoint_grid(
        int frame_number
    ) noexcept {
        if (frame_number < 0 || CNR3_CACHE_CHECKPOINT_INTERVAL <= 0) {
            return false;
        }

        const int remainder = frame_number % CNR3_CACHE_CHECKPOINT_INTERVAL;
        const int distance_before = remainder;
        const int distance_after = CNR3_CACHE_CHECKPOINT_INTERVAL - remainder;
        const int nearest_distance =
            distance_before < distance_after ? distance_before : distance_after;

        return nearest_distance <= 1;
    }

} // namespace

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
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    stats.scene_chroma_enabled = scene_chroma_used;
    stats.scene_threshold_used = scene_threshold;

    if (!scene_change_detected) {
        return;
    }

    cnr3_diag_saturating_increment(stats.scene_change_detections);

    if (scene_change_reset_output_used) {
        cnr3_diag_saturating_increment(stats.source_copy_reset_frames);
    }

    if (checkpoint_store_requested) {
        if (checkpoint_store_status == Cnr3Status::ok) {
            cnr3_diag_saturating_increment(
                stats.scene_change_checkpoint_store_successes
            );
        }
        else if (checkpoint_store_status == Cnr3Status::duplicate) {
            cnr3_diag_saturating_increment(
                stats.scene_change_checkpoint_store_duplicate_skips
            );
        }
        else {
            cnr3_diag_saturating_increment(
                stats.scene_change_checkpoint_store_errors
            );
        }
    }

    if (checkpoint_promoted) {
        cnr3_diag_saturating_increment(stats.scene_change_checkpoint_promotions);
    }

    if (scene_change_reset_output_used && !checkpoint_promoted) {
        cnr3_diag_saturating_increment(
            stats.scene_change_checkpoint_promotion_mismatches
        );
    }

    if (cnr3_diag_dsum14_is_near_checkpoint_grid(frame_number)) {
        cnr3_diag_saturating_increment(stats.cut_near_grid_checkpoint_count);
    }
}

Cnr3DiagDsum14SceneResetSnapshot cnr3_diag_dsum14_snapshot_scene_reset(
    const Cnr3DiagDsum14SceneResetStats& stats
) noexcept {
    std::lock_guard<std::mutex> lock(stats.mutex);

    Cnr3DiagDsum14SceneResetSnapshot snapshot{};
    snapshot.scene_change_detections = stats.scene_change_detections;
    snapshot.source_copy_reset_frames = stats.source_copy_reset_frames;
    snapshot.scene_change_checkpoint_promotions =
        stats.scene_change_checkpoint_promotions;
    snapshot.scene_change_checkpoint_store_successes =
        stats.scene_change_checkpoint_store_successes;
    snapshot.scene_change_checkpoint_store_duplicate_skips =
        stats.scene_change_checkpoint_store_duplicate_skips;
    snapshot.scene_change_checkpoint_store_errors =
        stats.scene_change_checkpoint_store_errors;
    snapshot.scene_change_checkpoint_promotion_mismatches =
        stats.scene_change_checkpoint_promotion_mismatches;
    snapshot.cut_near_grid_checkpoint_count = stats.cut_near_grid_checkpoint_count;
    snapshot.scene_chroma_enabled = stats.scene_chroma_enabled;
    snapshot.scene_threshold_used = stats.scene_threshold_used;
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM14_SCENE_RESET)

void cnr3_diag_dsum14_write_scene_reset_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3DiagDsum14SceneResetStats& stats
) noexcept {
    const Cnr3DiagDsum14SceneResetSnapshot snapshot =
        cnr3_diag_dsum14_snapshot_scene_reset(stats);

    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-14",
        "[DSUM-SUMMARY] D-SUM-14 scene-reset summary"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-14",
        "[DSUM-SUMMARY] D-SUM-14 interpretation: scene-change detection is pixel-layer observation; source-copy reset is algorithmic; checkpoint promotion is cache/store consequence; eligible reset without required promotion is a serious issue"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-14",
        "[DSUM-SUMMARY] D-SUM-14 note: source_copy_reset_frames counts scene-driven resets only; structural frame-0/floor fresh-starts remain in D-SUM-12"
    );
    cnr3_diag_write_text_line(
        instance_id,
        "D-SUM-14",
        "[DSUM-SUMMARY] D-SUM-14 note: near-grid means distance to nearest checkpoint grid <= 1; smaller checkpoint intervals make more frames near-grid"
    );

    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_change_detections", snapshot.scene_change_detections);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "source_copy_reset_frames", snapshot.source_copy_reset_frames);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_change_checkpoint_promotions", snapshot.scene_change_checkpoint_promotions);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_change_checkpoint_store_successes", snapshot.scene_change_checkpoint_store_successes);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_change_checkpoint_store_duplicate_skips", snapshot.scene_change_checkpoint_store_duplicate_skips);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_change_checkpoint_store_errors", snapshot.scene_change_checkpoint_store_errors);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_change_checkpoint_promotion_mismatches", snapshot.scene_change_checkpoint_promotion_mismatches);
    cnr3_diag_write_uint64_row(instance_id, "D-SUM-14", "D-SUM-14", "cut_near_grid_checkpoint_count", snapshot.cut_near_grid_checkpoint_count);
    cnr3_diag_write_bool_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_chroma_enabled", snapshot.scene_chroma_enabled);
    cnr3_diag_write_int64_row(instance_id, "D-SUM-14", "D-SUM-14", "scene_threshold_used", snapshot.scene_threshold_used);

    cnr3_diag_flush_stderr();
}

#endif
