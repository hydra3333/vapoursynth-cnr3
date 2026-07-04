#include "cnr3_build_config.h"

#include "cnr3_cache_core_selftest.h"

#include "cnr3_cache_core.h"

#include "cnr3_diagnostics.h"

#include <cstdio>
#include <cstring>

namespace {

    constexpr Cnr3InstanceId CNR3_SELFTEST_INSTANCE_ID{};

    constexpr const char* CNR3_SELFTEST_COMPONENT = "cache_core_selftest";

    void cnr3_selftest_write_summary_line(
        const char* message
    ) noexcept {
        cnr3_diag_write_line(
            CNR3_SELFTEST_INSTANCE_ID,
            Cnr3DiagnosticLevel::info,
            CNR3_SELFTEST_COMPONENT,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_selftest_write_summary_int_line(
        const char* label,
        int value
    ) noexcept {
        char message[128] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "%s%d",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_selftest_write_summary_line("formatting_error");
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_selftest_write_summary_line(message);
    }

    void cnr3_selftest_write_summary_text_line(
        const char* label,
        const char* value
    ) noexcept {
        char message[256] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "%s%s",
            label != nullptr ? label : "(null)",
            value != nullptr ? value : "<none>"
        );

        if (written < 0) {
            cnr3_selftest_write_summary_line("formatting_error");
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_selftest_write_summary_line(message);
    }

    Cnr3CacheCoreSelftestRunResult cnr3_selftest_make_forced_failure_result(
        const Cnr3CacheCoreSelftestRunResult& natural_result
    ) noexcept {
        Cnr3CacheCoreSelftestRunResult result = natural_result;

        if (result.total_count <= 0) {
            result.total_count = 1;
            result.passed_count = 0;
            result.failed_count = 1;
        }
        else if (result.passed_count > 0) {
            --result.passed_count;
            ++result.failed_count;
        }
        else {
            ++result.failed_count;
        }

        if (result.first_failed_test_name == nullptr) {
            result.first_failed_test_name = "forced_failure_for_harness_proof";
            result.first_failed_status = Cnr3Status::invariant_violation;
        }

        return result;
    }

    void cnr3_selftest_print_summary_to_stderr(
        const Cnr3CacheCoreSelftestRunResult& result
    ) noexcept {
        cnr3_selftest_write_summary_line("CNR3 cache-core selftest runner");
        cnr3_selftest_write_summary_text_line("edit_version: ", CNR3_EDIT_VERSION);
        cnr3_selftest_write_summary_text_line("cache_profile: ", CNR3_CACHE_PROFILE_NAME);
        cnr3_selftest_write_summary_line("");

        cnr3_selftest_write_summary_line("summary:");
        cnr3_selftest_write_summary_int_line("    total: ", result.total_count);
        cnr3_selftest_write_summary_int_line("    passed: ", result.passed_count);
        cnr3_selftest_write_summary_int_line("    failed: ", result.failed_count);

        if (cnr3_cache_core_selftest_run_result_passed(result)) {
            cnr3_selftest_write_summary_line("    result: PASS");
            cnr3_diag_flush_stderr();
            return;
        }

        cnr3_selftest_write_summary_line("    result: FAIL");
        cnr3_selftest_write_summary_text_line(
            "    first_failed_test_name: ",
            result.first_failed_test_name
        );
        cnr3_selftest_write_summary_text_line(
            "    first_failed_status: ",
            cnr3_status_name(result.first_failed_status)
        );

        cnr3_diag_flush_stderr();
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)

    void cnr3_selftest_emit_dsum01_reference_summary() noexcept {
        Cnr3DiagDsum01RequestOrderStats stats{};

        cnr3_diag_dsum01_observe_ar_initial(stats, 0);
        cnr3_diag_dsum01_observe_ar_all_frames_ready(stats);
        cnr3_diag_dsum01_observe_ar_initial(stats, 1);
        cnr3_diag_dsum01_observe_ar_all_frames_ready(stats);
        cnr3_diag_dsum01_observe_ar_initial(stats, 1);
        cnr3_diag_dsum01_observe_ar_initial(stats, 5);
        cnr3_diag_dsum01_observe_ar_initial(stats, 3);
        cnr3_diag_dsum01_observe_ar_initial(stats, 4);

#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER)
        cnr3_diag_dsum01_write_request_order_summary_to_stderr(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif



#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)

    void cnr3_selftest_emit_dsum03_reference_summary() noexcept {
        Cnr3DiagDsum03RecoverySearchStats stats{};

        cnr3_diag_dsum03_observe_search_result(
            stats,
            true,
            Cnr3DiagDsum03RecoveryTermination::present_output,
            3
        );
        cnr3_diag_dsum03_observe_search_result(
            stats,
            true,
            Cnr3DiagDsum03RecoveryTermination::bound,
            50
        );
        cnr3_diag_dsum03_observe_search_result(
            stats,
            false,
            Cnr3DiagDsum03RecoveryTermination::failure,
            50
        );
        cnr3_diag_dsum03_observe_holes_filled(stats, 4U);

#if defined(CNR3_DIAG_PRINT_DSUM03_RECOVERY_SEARCH)
        cnr3_diag_dsum03_write_recovery_search_summary_to_stderr(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)

    void cnr3_selftest_emit_dsum12_reference_summary() noexcept {
        Cnr3DiagDsum12RecoveryPlanStats stats{};

        cnr3_diag_dsum12_observe_branch_frame0(stats);
        cnr3_diag_dsum12_observe_branch_pred_present(stats);
        cnr3_diag_dsum12_observe_branch_cache_hit(stats);
        cnr3_diag_dsum12_observe_recovery_plan_published(
            stats,
            true,
            false,
            true,
            2U,
            3
        );
        cnr3_diag_dsum12_observe_hole_source_retrieved(stats);
        cnr3_diag_dsum12_observe_hole_source_retrieved(stats);
        cnr3_diag_dsum12_observe_holes_filled(stats, 2U);
        cnr3_diag_dsum12_observe_recovery_plan_destroyed(stats);

#if defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
        cnr3_diag_dsum12_write_recovery_plan_summary_to_stderr(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)

    void cnr3_selftest_emit_dsum13_reference_summary() noexcept {
        Cnr3DiagDsum13RecalculationStats stats{};

        cnr3_diag_dsum13_observe_compute_completion(stats, 10, 0);
        cnr3_diag_dsum13_observe_compute_completion(stats, 11, 1);
        cnr3_diag_dsum13_observe_compute_completion(stats, 11, 1);
        cnr3_diag_dsum13_observe_compute_completion(stats, 12, 5);
        cnr3_diag_dsum13_observe_compute_completion(stats, 12, 7);
        cnr3_diag_dsum13_observe_compute_completion(stats, 12, 9);

#if defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)
        cnr3_diag_dsum13_write_recalculation_summary_to_stderr(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)

    void cnr3_selftest_emit_dsum04_reference_summary() noexcept {
        Cnr3CacheOwnershipDiagnosticStats stats{};

        cnr3_cache_ownership_diagnostic_observe_pin_acquired(stats);
        cnr3_cache_ownership_diagnostic_observe_pin_acquired(stats);
        cnr3_cache_ownership_diagnostic_observe_pin_released(stats);
        cnr3_cache_ownership_diagnostic_observe_pin_released(stats);
        cnr3_cache_ownership_diagnostic_observe_lookup_ref_acquired(stats);
        cnr3_cache_ownership_diagnostic_observe_lookup_ref_acquired(stats);
        cnr3_cache_ownership_diagnostic_observe_lookup_ref_transferred(stats);
        cnr3_cache_ownership_diagnostic_observe_lookup_ref_released_by_cache_core(stats);
        stats.total_pin_count_crosscheck = 0;

#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)
        cnr3_cache_ownership_diagnostic_write_summary(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)

    void cnr3_selftest_emit_dsum05_reference_summary() noexcept {
        Cnr3CacheIntegrityDiagnosticStats stats{};

        cnr3_cache_integrity_diagnostic_observe_check(stats, 3U, 1U, 47U, 0);
        cnr3_cache_integrity_diagnostic_observe_check(stats, 5U, 2U, 46U, 1);
        cnr3_cache_integrity_diagnostic_set_summary_sample(stats, 5U, 2U, 46U, 0);

#if defined(CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY)
        cnr3_cache_integrity_diagnostic_write_summary(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)

    void cnr3_selftest_emit_dsum08_reference_summary() noexcept {
        Cnr3CacheStoreDiagnosticStats stats{};

        cnr3_cache_store_diagnostic_observe_store(stats, 0U, false, false, false, false);
        cnr3_cache_store_diagnostic_observe_store(stats, 1U, true, true, false, false);
        cnr3_cache_store_diagnostic_observe_store(stats, 2U, false, false, true, false);
        cnr3_cache_store_diagnostic_observe_store(stats, 3U, false, false, false, false);

#if defined(CNR3_DIAG_PRINT_DSUM08_CACHE_STORE)
        cnr3_cache_store_diagnostic_write_summary(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

    void cnr3_selftest_emit_dsum10_reference_summary() {
        Cnr3CachePruneDiagnosticStats stats{};

        cnr3_cache_prune_diagnostic_configure(
            stats,
            CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
            CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES
        );

        stats.prune_invocations = 3;
        stats.prune_events_triggered = 2;
        stats.frames_evicted = 6;
        stats.bytes_evicted = 6U * 4096U;
        stats.checkpoint_prunes = 1;
        stats.hot_zone_rejected = 4;
        stats.frames_evicted_then_re_requested = 3;
        stats.frames_re_requested_repeatedly = 1;
        stats.total_evicted_records = 6;
        stats.ring_live_count = 6;
        stats.ring_head = 6U % stats.ring_capacity;
        stats.gap_histogram[0] = 2;
        stats.gap_histogram[1] = 1;
        stats.top_thrasher_count = 2;
        stats.top_thrashers[0] = Cnr3CachePruneDiagnosticTopThrashEntry{ 47, 2 };
        stats.top_thrashers[1] = Cnr3CachePruneDiagnosticTopThrashEntry{ 82, 1 };

        const int sample_frames[] = { 40, 41, 47, 82, 47, 90 };
        for (std::size_t i = 0U; i < 6U && i < stats.recently_evicted_ring.size(); ++i) {
            stats.recently_evicted_ring[i] =
                Cnr3CachePruneDiagnosticRingEntry{
                    sample_frames[i],
                    static_cast<std::uint64_t>(i + 1U)
                };
        }

#if defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION)
        cnr3_cache_prune_diagnostic_write_summary(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)

    void cnr3_selftest_emit_dsum11_reference_summary() noexcept {
        Cnr3CacheHotZoneDiagnosticStats stats{};

        cnr3_cache_hot_zone_diagnostic_observe_create(stats);
        cnr3_cache_hot_zone_diagnostic_observe_slide(stats);
        cnr3_cache_hot_zone_diagnostic_observe_merge(stats);
        cnr3_cache_hot_zone_diagnostic_observe_decay(stats);
        cnr3_cache_hot_zone_diagnostic_observe_expiry(stats);
        cnr3_cache_hot_zone_diagnostic_observe_zone_count_sample(stats, 1U);
        cnr3_cache_hot_zone_diagnostic_observe_zone_count_sample(stats, 2U);
        cnr3_cache_hot_zone_diagnostic_observe_protected_range_sample(stats, 61);
        cnr3_cache_hot_zone_diagnostic_observe_protected_range_sample(stats, 80);
        cnr3_cache_hot_zone_diagnostic_observe_prune_rejections(stats, 4U);

#if defined(CNR3_DIAG_PRINT_DSUM11_HOT_ZONE)
        cnr3_cache_hot_zone_diagnostic_write_summary(
            CNR3_SELFTEST_INSTANCE_ID,
            stats
        );
#endif
    }

#endif

    bool cnr3_selftest_argument_is_present(
        int argc,
        char** argv,
        const char* argument_to_find
    ) noexcept {
        if (argv == nullptr || argument_to_find == nullptr) {
            return false;
        }

        for (int argument_index = 1; argument_index < argc; ++argument_index) {
            if (
                argv[argument_index] != nullptr &&
                std::strcmp(argv[argument_index], argument_to_find) == 0
                ) {
                return true;
            }
        }

        return false;
    }

} // namespace

int main(int argc, char** argv) {
    const bool force_failure_proof = cnr3_selftest_argument_is_present(
        argc,
        argv,
        "--force-fail-for-harness-proof"
    );

    const bool verbose = cnr3_selftest_argument_is_present(
        argc,
        argv,
        "--verbose"
    );

    cnr3_cache_core_selftest_set_verbose(verbose);

    const Cnr3CacheCoreSelftestRunResult natural_result =
        cnr3_cache_core_selftest_run_all();

    const Cnr3CacheCoreSelftestRunResult result =
        force_failure_proof ?
        cnr3_selftest_make_forced_failure_result(natural_result) :
        natural_result;

#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
    cnr3_selftest_emit_dsum01_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
    cnr3_selftest_emit_dsum03_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    cnr3_selftest_emit_dsum04_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    cnr3_selftest_emit_dsum05_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    cnr3_selftest_emit_dsum08_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    cnr3_selftest_emit_dsum10_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_selftest_emit_dsum11_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    cnr3_selftest_emit_dsum12_reference_summary();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
    cnr3_selftest_emit_dsum13_reference_summary();
#endif

    cnr3_selftest_print_summary_to_stderr(result);

    return cnr3_cache_core_selftest_run_result_passed(result) ? 0 : 1;
}
