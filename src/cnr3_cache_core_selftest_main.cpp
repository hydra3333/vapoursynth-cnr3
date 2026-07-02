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

    cnr3_selftest_print_summary_to_stderr(result);

    return cnr3_cache_core_selftest_run_result_passed(result) ? 0 : 1;
}
