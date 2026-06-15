#include "cnr3_diagnostics.h"

#include <cstdio>

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
