/*
    CNR3 - VapourSynth API4 temporal chroma stabiliser, based on the
    venerable CNR2/vscnr2.

    CNR3 is a redevelopment inspired by the CNR2/vscnr2 recursive temporal
    chroma-stabilisation model, using VapourSynth API4 only.

    Recursive processing and VapourSynth scheduling:
        The CNR2/vscnr2 algorithm is inherently temporal and recursive, which
        requires in-order serial frame processing in its original execution
        model.

        Processing SOURCE frame N into OUTPUT frame N requires access to the
        already-filtered OUTPUT arising from previously processed SOURCE frame
        N - 1:

            output[N] depends on both source[N] and output[N - 1]

        That makes the CNR2/vscnr2 algorithm naturally serial.

        Older VapourSynth-era recursive filters could sometimes rely on
        compatibility-style scheduling parameters and assumptions. In
        particular, fmFrameState meant only one thread would call a filter's
        getFrame function at a time and only one frame would be processed at a
        time.

        However, VapourSynth API4 documentation says fmFrameState is for
        compatibility only and MUST NOT BE USED IN NEW FILTERS. VapourSynth is
        also moving away from API3: current Windows binaries no longer
        distribute the API R3 headers, and general API R3 plugin support is
        only retained for now.

        CNR3 therefore HAS TO CHANGE from CNR2/vscnr2-era fmFrameState and API3
        assumptions in order to survive as a maintainable modern VapourSynth
        filter.

        CNR3 will initially target fmUnordered integration, then move through
        fmParallelRequests if approved by the phase plan, and finally move to
        fmParallel.

    Project scope:
        - VapourSynth API4 only.
        - Integer planar YUV only.
        - Primary target material: analogue video captures with temporal chroma
          instability, such as VHS/VHS-C and related restoration sources.
        - Correct recursive chroma-stabilisation behaviour before parallel
          performance work.

    Load-bearing recursive fact:
        Modern VapourSynth scheduling can and does request frames out of
        display order, so CNR3 must not rely on display-order calls, a single
        previous-output variable, or strict serial predecessor state.

        CNR3 is not a stateless image filter.

        To compute OUTPUT frame N, the filter needs:
            - SOURCE[N]
            - already-filtered OUTPUT[N - 1]

        The predecessor is the previous filtered OUTPUT, not SOURCE[N - 1].

            output[N] depends on source[N] and output[N - 1]

        This recursive dependency is the central reason CNR3 needs its own
        cache/recovery architecture.

    CMS07.0 restart architecture:
        CMS07.0 holds the new cache design and proof path.

        The first implementation milestone is the cache-manager core in
        isolation, before VapourSynth getFrame wiring and before pixel-layer
        salvage.

        The CMS07 cache manager is a correctness subsystem. It owns output
        frame reference slots, ordered index state, consumer-held pins,
        per-invocation pin-lists, checkpoint flags, hot zones, prune policy,
        recovery planning, validation, and cache diagnostics.

        It must prove:
            - pin/unpin balance is zero;
            - lookup-reference accounting balances;
            - no VSFrame reference leaks;
            - no double-free;
            - eviction never selects a pinned, checkpoint, or in-zone slot;
            - shutdown clear releases all cached references, with a warning on
              any non-zero pin count.

    File role:
        This translation unit is the VapourSynth integration layer.

        It should remain thin. Its long-term responsibilities are plugin
        registration, parameter parsing, instance creation/destruction,
        VapourSynth source-request/retrieve lifecycle, frameData allocation and
        cleanup, error mapping, and calls into the cache and pixel-processing
        layers after those layers are proven.

        It must not contain cache algorithms, recovery algorithms, prune policy,
        pixel loops, response-table construction, or memory-diagnostic internals.

        During CMS07-K.1D this file contains the temporary live getFrame frame-0
        proof path. It proves frame-0 source-verbatim fresh-start output
        creation, cache store, and return plumbing. Nonzero frames are refused
        until predecessor-present processing is wired.

    Filter-mode posture:
        The final operational target is fmParallel.

        Interim development may pass through safer or narrower stages, but code
        and design must not introduce assumptions that block eventual safe
        fmParallel operation unless the exception is explicit, temporary,
        justified, and recorded.

        Do not reintroduce fmFrameState compatibility assumptions.
        Do not reintroduce old strict-streaming output authority.
        Do not rely on call order as proof that OUTPUT[N - 1] exists.

    Pixel-layer boundary:
        Pixel/frame processing is a separate layer.

        When the pixel layer is later salvaged, it receives SOURCE[N] and an
        explicit previous OUTPUT frame supplied by the cache/recovery layer.
        The pixel layer must not find, cache, pin, recover, prune, schedule, or
        substitute predecessor frames.

        CNR2/vscnr2 may be used as pixel-maths guidance only. CNR3 must not
        adopt CNR2's serialized recovery/predecessor approximation. In
        particular, CNR3 must not substitute SOURCE[N - 1] for the required
        previous filtered OUTPUT[N - 1].

    Diagnostics and output:
        CNR3 must never write diagnostics, debug messages, status messages, or
        summaries to stdout. In common VapourSynth pipelines, stdout may carry
        frame data.

        Diagnostic and debug text must go to stderr.

        VapourSynth creation errors must use mapSetError().
        VapourSynth frame-processing errors must use setFilterError().

        Ongoing diagnostics use DIAG_* gates and observe only.
        Temporary proof scaffolds use SCAFFOLD_* gates, are clearly bounded,
        and must not become required for production correctness.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_plugin_internal.h"

#include "cnr3_instance_config.h"
#include "cnr3_response_tables.h"

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)
#include <mutex>
#endif

namespace {

#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)

int cnr3_planretry_derive_max_attempts(
    int num_threads
) noexcept {
    const int scaled_thread_limit = num_threads > 1 ? num_threads / 2 : 1;
    const int lower_bounded_limit = scaled_thread_limit > 1 ? scaled_thread_limit : 1;

    return lower_bounded_limit < CNR3_PLAN_RETRY_MAX_CAP
        ? lower_bounded_limit
        : CNR3_PLAN_RETRY_MAX_CAP;
}

void cnr3_planretry_write_u64_line(
    Cnr3InstanceId instance_id,
    const char* field_name,
    std::uint64_t value
) noexcept {
    std::fprintf(
        stderr,
        "CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] %-44s %llu\n",
        instance_id.value,
        field_name != nullptr ? field_name : "<null>",
        static_cast<unsigned long long>(value)
    );
}

void cnr3_planretry_write_i32_line(
    Cnr3InstanceId instance_id,
    const char* field_name,
    int value
) noexcept {
    std::fprintf(
        stderr,
        "CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] %-44s %d\n",
        instance_id.value,
        field_name != nullptr ? field_name : "<null>",
        value
    );
}

void cnr3_planretry_write_text_line(
    Cnr3InstanceId instance_id,
    const char* text
) noexcept {
    std::fprintf(
        stderr,
        "CNR3[%d] INFO DSUM-PLANRETRY: [DSUM-PLANRETRY] %s\n",
        instance_id.value,
        text != nullptr ? text : "<null>"
    );
}

void cnr3_planretry_write_summary_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3PlanRetryExperimentStats& stats,
    int plan_retry_max
) noexcept {
    struct Snapshot {
        std::uint64_t plan_attempts_total = 0;
        std::uint64_t plans_dumped_total = 0;
        std::uint64_t retry_sleeps_total = 0;
        std::uint64_t plans_kept_on_attempt_1 = 0;
        std::uint64_t plans_kept_on_attempt_2 = 0;
        std::uint64_t plans_kept_on_attempt_3plus = 0;
        std::uint64_t dumped_plan_holes_total = 0;
        std::uint64_t kept_plan_holes_total = 0;
    };

    Snapshot snapshot{};

    {
        std::lock_guard<std::mutex> lock{stats.mutex};
        snapshot.plan_attempts_total = stats.plan_attempts_total;
        snapshot.plans_dumped_total = stats.plans_dumped_total;
        snapshot.retry_sleeps_total = stats.retry_sleeps_total;
        snapshot.plans_kept_on_attempt_1 = stats.plans_kept_on_attempt_1;
        snapshot.plans_kept_on_attempt_2 = stats.plans_kept_on_attempt_2;
        snapshot.plans_kept_on_attempt_3plus = stats.plans_kept_on_attempt_3plus;
        snapshot.dumped_plan_holes_total = stats.dumped_plan_holes_total;
        snapshot.kept_plan_holes_total = stats.kept_plan_holes_total;
    }

    cnr3_planretry_write_text_line(
        instance_id,
        "plan-retry biasing experiment summary"
    );
    cnr3_planretry_write_i32_line(instance_id, "plan_retry_enabled", 1);
    cnr3_planretry_write_i32_line(
        instance_id,
        "plan_retry_sleep_ms",
        CNR3_PLAN_RETRY_SLEEP_MS
    );
    cnr3_planretry_write_i32_line(
        instance_id,
        "plan_retry_hole_threshold",
        CNR3_PLAN_RETRY_HOLE_THRESHOLD
    );
    cnr3_planretry_write_i32_line(
        instance_id,
        "plan_retry_max_cap",
        CNR3_PLAN_RETRY_MAX_CAP
    );
    cnr3_planretry_write_i32_line(
        instance_id,
        "plan_retry_max",
        plan_retry_max
    );

    cnr3_planretry_write_u64_line(
        instance_id,
        "plan_retry_plan_attempts_total",
        snapshot.plan_attempts_total
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "plans_dumped_total",
        snapshot.plans_dumped_total
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "retry_sleeps_total",
        snapshot.retry_sleeps_total
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "plans_kept_on_attempt_1",
        snapshot.plans_kept_on_attempt_1
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "plans_kept_on_attempt_2",
        snapshot.plans_kept_on_attempt_2
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "plans_kept_on_attempt_3plus",
        snapshot.plans_kept_on_attempt_3plus
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "dumped_plan_holes_total",
        snapshot.dumped_plan_holes_total
    );
    cnr3_planretry_write_u64_line(
        instance_id,
        "kept_plan_holes_total",
        snapshot.kept_plan_holes_total
    );

    const std::uint64_t kept_total =
        snapshot.plans_kept_on_attempt_1 +
        snapshot.plans_kept_on_attempt_2 +
        snapshot.plans_kept_on_attempt_3plus;

    const std::uint64_t expected_attempts =
        snapshot.plans_dumped_total + kept_total;

    if (snapshot.plan_attempts_total == expected_attempts) {
        cnr3_planretry_write_text_line(
            instance_id,
            "self-check attempts == dumped + kept buckets -> OK"
        );
    }
    else {
        std::fprintf(
            stderr,
            "CNR3[%d] WARN DSUM-PLANRETRY: [DSUM-PLANRETRY] self-check attempts == dumped + kept buckets -> WARN (%llu vs %llu)\n",
            instance_id.value,
            static_cast<unsigned long long>(snapshot.plan_attempts_total),
            static_cast<unsigned long long>(expected_attempts)
        );
    }

    if (snapshot.retry_sleeps_total == snapshot.plans_dumped_total) {
        cnr3_planretry_write_text_line(
            instance_id,
            "self-check retry_sleeps_total == plans_dumped_total -> OK"
        );
    }
    else {
        std::fprintf(
            stderr,
            "CNR3[%d] WARN DSUM-PLANRETRY: [DSUM-PLANRETRY] self-check retry_sleeps_total == plans_dumped_total -> WARN (%llu vs %llu)\n",
            instance_id.value,
            static_cast<unsigned long long>(snapshot.retry_sleeps_total),
            static_cast<unsigned long long>(snapshot.plans_dumped_total)
        );
    }
}

#endif

/*
    CNR3 option surface and cnr2/vscnr2 migration map.

    CNR3 intentionally exposes descriptive create-time option names. The old
    cnr2 names are reference-only migration names and are not accepted by the
    parser. This block is deliberately kept beside the parser/defaulting path so
    future maintainers can safely edit the option surface without rediscovering
    the historical cnr2 meanings from external documents.

        cnr2 name      CNR3 name        default       validation       meaning
        -------------------------------------------------------------------------------
        ln             y_threshold     35            0..255 int       luma-change guard threshold
        lm             y_strength      192           0..255 int       luma guard response strength
        un             u_threshold     47            0..255 int       U difference threshold
        um             u_strength      255           0..255 int       U response strength
        vn             v_threshold     47            0..255 int       V difference threshold
        vm             v_strength      255           0..255 int       V response strength
        mode[0]        y_curve         "wide"        wide|narrow      Y response-table curve
        mode[1]        u_curve         "narrow"      wide|narrow      U response-table curve
        mode[2]        v_curve         "narrow"      wide|narrow      V response-table curve
        scdthr         scene_threshold 10.0          0.0..100.0      scene-reset sensitivity
        sceneChroma    scene_chroma    false         bool             include chroma in scene reset

    Curve equivalence:
        cnr2 mode="oxx" maps to y_curve="wide", u_curve="narrow",
        v_curve="narrow". cnr2 treated the character 'x' as narrow and any
        non-'x' character as wide. CNR3 intentionally uses strict, explicit
        "wide" or "narrow" strings to avoid positional mode-string ambiguity.

    Behavioural notes:
        - threshold options decide how large a frame-to-frame difference may
          still be treated as noise-like. Raising a threshold denoises more
          aggressively but increases the risk of colour ghosting/smearing on
          moving objects; lowering it is more conservative.
        - y_threshold does not filter or modify output luma. CNR3 leaves Y
          unchanged. The Y response is a luma-change guard that gates chroma
          blending through the cross-plane response-table product: if brightness
          moved at that location, the chroma blend is suppressed because the
          change is more likely real content than chroma noise.
        - strength options set the peak pull toward the previous filtered chroma
          value once a difference is judged noise-like. Threshold sets the reach
          of the response; strength sets the maximum pull.
        - "wide" selects the j*j cosine response. It stays strong near zero
          difference and falls sharply near the threshold. "narrow" selects the
          j cosine response and tapers more steadily from zero to the threshold.
        - scene_threshold maps cnr2 scdthr onto CNR3's scene-reset threshold.
          Lower values are more sensitive and reset more often. Higher values
          reset less often and risk smoothing across missed cuts.
        - scene_chroma=false preserves cnr2's default luma-only scene reset.
          scene_chroma=true also counts chroma changes and is useful for
          chroma-only cuts, colour lighting shifts, flashing stage lights, and
          other material where brightness alone cannot see a cut.

    Safety and compatibility notes:
        - threshold==0 is accepted for cnr2 range compatibility. The response
          table builder special-cases it as centre-only: diff==0 receives the
          configured strength and all nonzero differences receive zero. Table
          construction must never divide by zero.
        - 8-bit defaults and 8-bit table behaviour are cnr2-compatible. For
          9..16-bit clips, CNR3 follows the CMS07 native-depth round-to-nearest
          scaling policy unless exact cnr2 high-depth emulation is separately
          scoped.
        - Operational defaults must remain cnr2-equivalent:
              y=35/192/wide, u=47/255/narrow, v=47/255/narrow,
              scene_threshold=10.0, scene_chroma=false.
        - response_config must be emitted from the live resolved configuration,
          not from constants, so accidental default regressions are visible in
          logs.
*/
inline constexpr int CNR3_DEFAULT_Y_THRESHOLD_8BIT = 35;
inline constexpr int CNR3_DEFAULT_Y_STRENGTH_8BIT = 192;
inline constexpr int CNR3_DEFAULT_UV_THRESHOLD_8BIT = 47;
inline constexpr int CNR3_DEFAULT_UV_STRENGTH_8BIT = 255;
inline constexpr double CNR3_DEFAULT_SCENE_THRESHOLD = CNR3_P11C_DEFAULT_SCDTHR;
inline constexpr bool CNR3_DEFAULT_SCENE_CHROMA = false;

struct Cnr3CreateOptions {
    int y_threshold = CNR3_DEFAULT_Y_THRESHOLD_8BIT;
    int y_strength = CNR3_DEFAULT_Y_STRENGTH_8BIT;
    int u_threshold = CNR3_DEFAULT_UV_THRESHOLD_8BIT;
    int u_strength = CNR3_DEFAULT_UV_STRENGTH_8BIT;
    int v_threshold = CNR3_DEFAULT_UV_THRESHOLD_8BIT;
    int v_strength = CNR3_DEFAULT_UV_STRENGTH_8BIT;
    Cnr3ResponseCurveKind y_curve = Cnr3ResponseCurveKind::wide;
    Cnr3ResponseCurveKind u_curve = Cnr3ResponseCurveKind::narrow;
    Cnr3ResponseCurveKind v_curve = Cnr3ResponseCurveKind::narrow;
    double scene_threshold = CNR3_DEFAULT_SCENE_THRESHOLD;
    bool scene_chroma = CNR3_DEFAULT_SCENE_CHROMA;
};

const char* cnr3_response_curve_kind_text(
    Cnr3ResponseCurveKind curve
) noexcept {
    switch (curve) {
    case Cnr3ResponseCurveKind::narrow:
        return "narrow";
    case Cnr3ResponseCurveKind::wide:
        return "wide";
    default:
        return "unknown";
    }
}

const char* cnr3_bool_text(bool value) noexcept {
    return value ? "true" : "false";
}

void cnr3_set_create_error(
    VSMap* out,
    const VSAPI* vsapi,
    const char* message
) noexcept {
    if (out != nullptr && vsapi != nullptr) {
        vsapi->mapSetError(out, message != nullptr ? message : "CNR3: create error.");
    }
}

void cnr3_set_option_error(
    VSMap* out,
    const VSAPI* vsapi,
    const char* option_name,
    const char* detail
) noexcept {
    char message[256]{};
    std::snprintf(
        message,
        sizeof(message),
        "CNR3: invalid %s option: %s",
        option_name != nullptr ? option_name : "<unknown>",
        detail != nullptr ? detail : "invalid value."
    );
    cnr3_set_create_error(out, vsapi, message);
}

void cnr3_format_int_option_expectation(
    char* buffer,
    std::size_t buffer_size,
    int minimum_value,
    int maximum_value,
    const char* expectation_override
) noexcept {
    if (buffer == nullptr || buffer_size == 0U) {
        return;
    }

    if (expectation_override != nullptr) {
        std::snprintf(buffer, buffer_size, "%s", expectation_override);
        return;
    }

    std::snprintf(
        buffer,
        buffer_size,
        "expected an integer in the range %d..%d inclusive.",
        minimum_value,
        maximum_value
    );
}

void cnr3_format_float_option_expectation(
    char* buffer,
    std::size_t buffer_size,
    double minimum_value,
    double maximum_value
) noexcept {
    if (buffer == nullptr || buffer_size == 0U) {
        return;
    }

    std::snprintf(
        buffer,
        buffer_size,
        "expected a finite float in the range %.1f..%.1f inclusive.",
        minimum_value,
        maximum_value
    );
}

void cnr3_format_curve_option_expectation(
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0U) {
        return;
    }

    std::snprintf(buffer, buffer_size, "expected exactly \"wide\" or \"narrow\".");
}

void cnr3_format_option_float_echo(
    double value,
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0U) {
        return;
    }

    buffer[0] = '\0';

    if (std::isnan(value)) {
        std::snprintf(buffer, buffer_size, "nan");
        return;
    }

    if (!std::isfinite(value)) {
        std::snprintf(buffer, buffer_size, "%s", std::signbit(value) ? "-inf" : "inf");
        return;
    }

    char* const end = buffer + buffer_size - 1U;
    const auto result = std::to_chars(buffer, end, value);

    if (result.ec != std::errc{}) {
        std::snprintf(buffer, buffer_size, "<format-error>");
        return;
    }

    *result.ptr = '\0';
}

void cnr3_format_curve_option_echo(
    const char* value,
    int value_size,
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0U) {
        return;
    }

    buffer[0] = '\0';

    if (value == nullptr || value_size <= 0) {
        std::snprintf(buffer, buffer_size, "%s", value == nullptr ? "<null>" : "");
        return;
    }

    constexpr int maximum_echo_bytes = 32;
    const int echo_length = value_size < maximum_echo_bytes ? value_size : maximum_echo_bytes;
    const bool truncated = value_size > maximum_echo_bytes;
    std::size_t output_length = 0U;

    for (int index = 0; index < echo_length && output_length + 1U < buffer_size; ++index) {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        buffer[output_length++] = byte >= 0x20U && byte <= 0x7eU
            ? static_cast<char>(byte)
            : '?';
    }

    if (truncated && output_length + 3U < buffer_size) {
        buffer[output_length++] = '.';
        buffer[output_length++] = '.';
        buffer[output_length++] = '.';
    }

    buffer[output_length] = '\0';
}

bool cnr3_option_present_once(
    const VSMap* in,
    VSMap* out,
    const VSAPI* vsapi,
    const char* option_name,
    bool& present
) noexcept {
    present = false;

    const int element_count = vsapi->mapNumElements(in, option_name);

    if (element_count < 0) {
        return true;
    }

    present = true;

    if (element_count != 1) {
        cnr3_set_option_error(
            out,
            vsapi,
            option_name,
            "expected exactly one value."
        );
        return false;
    }

    return true;
}

bool cnr3_parse_optional_int_option(
    const VSMap* in,
    VSMap* out,
    const VSAPI* vsapi,
    const char* option_name,
    int minimum_value,
    int maximum_value,
    int& resolved_value,
    const char* expectation_override = nullptr
) noexcept {
    bool present = false;

    if (!cnr3_option_present_once(in, out, vsapi, option_name, present)) {
        return false;
    }

    if (!present) {
        return true;
    }

    char expectation[96]{};
    cnr3_format_int_option_expectation(
        expectation,
        sizeof(expectation),
        minimum_value,
        maximum_value,
        expectation_override
    );

    int error = peSuccess;
    const std::int64_t value = vsapi->mapGetInt(in, option_name, 0, &error);

    if (error != peSuccess) {
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "incorrect value type, %.96s", expectation);
        cnr3_set_option_error(out, vsapi, option_name, detail);
        return false;
    }

    if (value < minimum_value || value > maximum_value) {
        char detail[128]{};
        std::snprintf(
            detail,
            sizeof(detail),
            "got %lld, %.96s",
            static_cast<long long>(value),
            expectation
        );
        cnr3_set_option_error(out, vsapi, option_name, detail);
        return false;
    }

    resolved_value = static_cast<int>(value);
    return true;
}

bool cnr3_parse_optional_bool_option(
    const VSMap* in,
    VSMap* out,
    const VSAPI* vsapi,
    const char* option_name,
    bool& resolved_value
) noexcept {
    int integer_value = resolved_value ? 1 : 0;

    if (!cnr3_parse_optional_int_option(
        in,
        out,
        vsapi,
        option_name,
        0,
        1,
        integer_value,
        "expected 0 or 1."
    )) {
        return false;
    }

    resolved_value = integer_value != 0;
    return true;
}

bool cnr3_parse_optional_float_option(
    const VSMap* in,
    VSMap* out,
    const VSAPI* vsapi,
    const char* option_name,
    double minimum_value,
    double maximum_value,
    double& resolved_value
) noexcept {
    bool present = false;

    if (!cnr3_option_present_once(in, out, vsapi, option_name, present)) {
        return false;
    }

    if (!present) {
        return true;
    }

    char expectation[96]{};
    cnr3_format_float_option_expectation(
        expectation,
        sizeof(expectation),
        minimum_value,
        maximum_value
    );

    int error = peSuccess;
    const double value = vsapi->mapGetFloat(in, option_name, 0, &error);

    if (error != peSuccess) {
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "incorrect value type, %.96s", expectation);
        cnr3_set_option_error(out, vsapi, option_name, detail);
        return false;
    }

    if (!std::isfinite(value) || value < minimum_value || value > maximum_value) {
        char value_text[64]{};
        cnr3_format_option_float_echo(value, value_text, sizeof(value_text));

        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "got %.32s, %.80s", value_text, expectation);
        cnr3_set_option_error(out, vsapi, option_name, detail);
        return false;
    }

    resolved_value = value;
    return true;
}

bool cnr3_data_equals_literal(
    const char* data,
    int data_size,
    const char* literal
) noexcept {
    if (data == nullptr || data_size < 0 || literal == nullptr) {
        return false;
    }

    const std::size_t literal_length = std::strlen(literal);
    const std::size_t actual_size = static_cast<std::size_t>(data_size);

    if (actual_size == literal_length) {
        return std::memcmp(data, literal, literal_length) == 0;
    }

    if (actual_size == literal_length + 1U) {
        return
            std::memcmp(data, literal, literal_length) == 0 &&
            data[literal_length] == '\0';
    }

    return false;
}

bool cnr3_parse_optional_curve_option(
    const VSMap* in,
    VSMap* out,
    const VSAPI* vsapi,
    const char* option_name,
    Cnr3ResponseCurveKind& resolved_value
) noexcept {
    bool present = false;

    if (!cnr3_option_present_once(in, out, vsapi, option_name, present)) {
        return false;
    }

    if (!present) {
        return true;
    }

    char expectation[96]{};
    cnr3_format_curve_option_expectation(expectation, sizeof(expectation));

    int data_error = peSuccess;
    const char* value = vsapi->mapGetData(in, option_name, 0, &data_error);

    int size_error = peSuccess;
    const int value_size = vsapi->mapGetDataSize(in, option_name, 0, &size_error);

    if (data_error != peSuccess || size_error != peSuccess || value == nullptr) {
        char detail[128]{};
        std::snprintf(detail, sizeof(detail), "incorrect value type, %.96s", expectation);
        cnr3_set_option_error(out, vsapi, option_name, detail);
        return false;
    }

    if (cnr3_data_equals_literal(value, value_size, "wide")) {
        resolved_value = Cnr3ResponseCurveKind::wide;
        return true;
    }

    if (cnr3_data_equals_literal(value, value_size, "narrow")) {
        resolved_value = Cnr3ResponseCurveKind::narrow;
        return true;
    }

    char value_text[36]{};
    cnr3_format_curve_option_echo(value, value_size, value_text, sizeof(value_text));

    char detail[128]{};
    std::snprintf(detail, sizeof(detail), "got \"%.35s\", %.80s", value_text, expectation);
    cnr3_set_option_error(out, vsapi, option_name, detail);
    return false;
}

bool cnr3_parse_create_options(
    const VSMap* in,
    VSMap* out,
    const VSAPI* vsapi,
    Cnr3CreateOptions& options
) noexcept {
    options = Cnr3CreateOptions{};

    return
        cnr3_parse_optional_int_option(in, out, vsapi, "y_threshold", 0, 255, options.y_threshold) &&
        cnr3_parse_optional_int_option(in, out, vsapi, "y_strength", 0, 255, options.y_strength) &&
        cnr3_parse_optional_int_option(in, out, vsapi, "u_threshold", 0, 255, options.u_threshold) &&
        cnr3_parse_optional_int_option(in, out, vsapi, "u_strength", 0, 255, options.u_strength) &&
        cnr3_parse_optional_int_option(in, out, vsapi, "v_threshold", 0, 255, options.v_threshold) &&
        cnr3_parse_optional_int_option(in, out, vsapi, "v_strength", 0, 255, options.v_strength) &&
        cnr3_parse_optional_curve_option(in, out, vsapi, "y_curve", options.y_curve) &&
        cnr3_parse_optional_curve_option(in, out, vsapi, "u_curve", options.u_curve) &&
        cnr3_parse_optional_curve_option(in, out, vsapi, "v_curve", options.v_curve) &&
        cnr3_parse_optional_float_option(in, out, vsapi, "scene_threshold", 0.0, 100.0, options.scene_threshold) &&
        cnr3_parse_optional_bool_option(in, out, vsapi, "scene_chroma", options.scene_chroma);
}

void cnr3_format_scene_threshold(
    double value,
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0U) {
        return;
    }

    buffer[0] = '\0';

    const int written = std::snprintf(buffer, buffer_size, "%.15g", value);

    if (written < 0 || static_cast<std::size_t>(written) >= buffer_size) {
        buffer[buffer_size - 1U] = '\0';
        return;
    }

    bool has_decimal_or_exponent = false;

    for (const char* cursor = buffer; *cursor != '\0'; ++cursor) {
        if (*cursor == '.' || *cursor == 'e' || *cursor == 'E') {
            has_decimal_or_exponent = true;
            break;
        }
    }

    if (has_decimal_or_exponent) {
        return;
    }

    const std::size_t length = std::strlen(buffer);

    if (length + 2U < buffer_size) {
        buffer[length] = '.';
        buffer[length + 1U] = '0';
        buffer[length + 2U] = '\0';
    }
}

void cnr3_emit_response_config_to_stderr(
    Cnr3InstanceId instance_id,
    const Cnr3ResponseTableConfig& config,
    double scene_threshold,
    bool scene_chroma
) noexcept {
#if defined(CNR3_EMIT_PLUGIN_STARTUP_PROVENANCE)
    char scene_threshold_text[64]{};
    cnr3_format_scene_threshold(
        scene_threshold,
        scene_threshold_text,
        sizeof(scene_threshold_text)
    );

    std::fprintf(
        stderr,
        "CNR3[%d] INFO CONFIG: response_config: y=%d/%d/%s u=%d/%d/%s v=%d/%d/%s scene_threshold=%s scene_chroma=%s\n",
        instance_id.value,
        config.y.threshold_8bit,
        config.y.strength_8bit,
        cnr3_response_curve_kind_text(config.y.curve),
        config.u.threshold_8bit,
        config.u.strength_8bit,
        cnr3_response_curve_kind_text(config.u.curve),
        config.v.threshold_8bit,
        config.v.strength_8bit,
        cnr3_response_curve_kind_text(config.v.curve),
        scene_threshold_text,
        cnr3_bool_text(scene_chroma)
    );
    std::fflush(stderr);
#else
    (void)instance_id;
    (void)config;
    (void)scene_threshold;
    (void)scene_chroma;
#endif
}

Cnr3ResponseTableConfig cnr3_make_response_table_config_from_create_options(
    int sample_peak,
    const Cnr3CreateOptions& options
) noexcept {
    Cnr3ResponseTableConfig config{};
    config.sample_peak = sample_peak;

    config.y.threshold_8bit = options.y_threshold;
    config.y.strength_8bit = options.y_strength;
    config.y.curve = options.y_curve;

    config.u.threshold_8bit = options.u_threshold;
    config.u.strength_8bit = options.u_strength;
    config.u.curve = options.u_curve;

    config.v.threshold_8bit = options.v_threshold;
    config.v.strength_8bit = options.v_strength;
    config.v.curve = options.v_curve;

    return config;
}

Cnr3Status cnr3_initialise_k1e2_live_pixel_config(
    const VSVideoInfo& video_info,
    const Cnr3CreateOptions& options,
    Cnr3FilterData& data
) noexcept {
    const VSVideoFormat* format = &video_info.format;

    if (format == nullptr ||
        format->colorFamily != cfYUV ||
        format->sampleType != stInteger ||
        format->numPlanes != 3) {
        return Cnr3Status::unsupported_format;
    }

    if (format->bitsPerSample < 8 || format->bitsPerSample > 16) {
        return Cnr3Status::unsupported_format;
    }

    if (format->subSamplingW < 0 || format->subSamplingW > 1 ||
        format->subSamplingH < 0 || format->subSamplingH > 1) {
        return Cnr3Status::unsupported_format;
    }

    data.bits_per_sample = format->bitsPerSample;
    data.sub_sampling_w = format->subSamplingW;
    data.sub_sampling_h = format->subSamplingH;
    data.scene_change_scdthr = options.scene_threshold;

    Cnr3Status status = cnr3_make_scene_change_config_from_vscnr2_scdthr(
        data.scene_change_scdthr,
        video_info.width,
        video_info.height,
        data.bits_per_sample,
        data.sub_sampling_w,
        data.sub_sampling_h,
        options.scene_chroma,
        data.scene_change_config
    );

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    const int sample_peak = (1 << data.bits_per_sample) - 1;
    const Cnr3ResponseTableConfig table_config =
        cnr3_make_response_table_config_from_create_options(sample_peak, options);

    cnr3_emit_response_config_to_stderr(
        data.config.instance_id,
        table_config,
        data.scene_change_scdthr,
        options.scene_chroma
    );

    return build_cnr3_response_tables(table_config, data.response_tables);
}

const VSFrame* VS_CC cnr3_get_frame_keystone_live_k1f_proof(
    int n,
    int activation_reason,
    void* instance_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    Cnr3FilterData* data = static_cast<Cnr3FilterData*>(instance_data);

    if (data == nullptr || data->source == nullptr || frame_data == nullptr || core == nullptr || vsapi == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        if (data != nullptr) {
            cnr3_diag_plantrace_observe_minimal_failed_and_dump(
                data->config.instance_id,
                data->dsum_plantrace,
                n,
                Cnr3DiagPlanTraceFailReason::framedata_missing_or_unknown,
                n
            );
        }
#endif
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: invalid getFrame state."
        );
        return nullptr;
    }

    if (activation_reason == arError) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data->output_cache);
        return nullptr;
    }

    if (activation_reason == arAllFramesReady) {
        return cnr3_arAllFramesReady(
            n,
            *data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    }

    if (activation_reason == arInitial) {
        return cnr3_arInitial(
            n,
            *data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    }

    return nullptr;
}

void VS_CC cnr3_free_filter(
    void* instance_data,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    Cnr3FilterData* data = static_cast<Cnr3FilterData*>(instance_data);

    if (data == nullptr) {
        return;
    }

#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER)
    cnr3_diag_dsum01_write_request_order_summary_to_stderr(
        data->config.instance_id,
        data->dsum01_request_order
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM03_RECOVERY_SEARCH)
    cnr3_diag_dsum03_write_recovery_search_summary_to_stderr(
        data->config.instance_id,
        data->dsum03_recovery_search
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_dsum06_write_source_frame_lifecycle_summary_to_stderr(
        data->config.instance_id,
        data->dsum06_source_frame_lifecycle
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    cnr3_diag_dsum07_write_temp_output_lifecycle_summary_to_stderr(
        data->config.instance_id,
        data->dsum07_temp_output_lifecycle
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    const Cnr3CacheOwnershipDiagnosticStats dsum04_ownership_balance_for_summary =
        data->output_cache.ownership_diagnostic_stats();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    const Cnr3CacheStoreDiagnosticStats dsum08_cache_store_for_summary =
        data->output_cache.cache_store_diagnostic_stats();
#endif
    cnr3_cache_ownership_diagnostic_write_summary(
        data->config.instance_id,
        dsum04_ownership_balance_for_summary
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        , &data->dsum07_temp_output_lifecycle
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
        , &dsum08_cache_store_for_summary
#endif
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY)
    cnr3_cache_integrity_diagnostic_write_summary(
        data->config.instance_id,
        data->output_cache.cache_integrity_diagnostic_stats()
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM08_CACHE_STORE)
    cnr3_cache_store_diagnostic_write_summary(
        data->config.instance_id,
        data->output_cache.cache_store_diagnostic_stats()
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_write_return_transfer_summary_to_stderr(
        data->config.instance_id,
        data->dsum09_return_transfer
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION)
    cnr3_cache_prune_diagnostic_write_summary(
        data->config.instance_id,
        data->output_cache.prune_diagnostic_stats()
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_write_summary(
        data->config.instance_id,
        data->output_cache.hot_zone_diagnostic_stats()
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN)
    cnr3_diag_dsum12_write_recovery_plan_summary_to_stderr(
        data->config.instance_id,
        data->dsum12_recovery_plan
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)
    cnr3_diag_dsum13_write_recalculation_summary_to_stderr(
        data->config.instance_id,
        data->dsum13_recalculation
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM14_SCENE_RESET)
    cnr3_diag_dsum14_write_scene_reset_summary_to_stderr(
        data->config.instance_id,
        data->dsum14_scene_reset
    );
#endif
#if defined(CNR3_DIAG_PRINT_DSUM_PLANTRACE)
    cnr3_diag_plantrace_write_clean_end_dump_to_stderr(
        data->config.instance_id,
        data->dsum_plantrace
    );
#endif
#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)
    cnr3_planretry_write_summary_to_stderr(
        data->config.instance_id,
        data->plan_retry_stats,
        data->plan_retry_max
    );
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
    cnr3_memory_record_and_print_snapshot(
        data->dsum02_memory,
        data->config.instance_id,
        "before cache clear",
        false
    );
#endif

    const Cnr3Status teardown_clear_status = data->output_cache.clear();

#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
    char post_cleanup_label[96]{};
    std::snprintf(
        post_cleanup_label,
        sizeof(post_cleanup_label),
        "after cache clear (clear=%s)",
        cnr3_status_name(teardown_clear_status)
    );

    cnr3_memory_record_and_print_snapshot(
        data->dsum02_memory,
        data->config.instance_id,
        post_cleanup_label,
        false
    );

    cnr3_memory_print_summary(
        data->dsum02_memory,
        data->config.instance_id
    );
#endif

#if \
    defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE) || \
    defined(CNR3_DIAG_PRINT_DSUM09_RETURN_TRANSFER) || \
    defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION) || \
    defined(CNR3_DIAG_PRINT_DSUM12_RECOVERY_PLAN) || \
    defined(CNR3_DIAG_PRINT_DSUM13_RECALCULATION)
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    const Cnr3CacheOwnershipDiagnosticStats dsum04_ownership_balance_snapshot =
        data->output_cache.ownership_diagnostic_stats();
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    const Cnr3CachePruneDiagnosticStats dsum10_prune_eviction_snapshot =
        data->output_cache.prune_diagnostic_stats();
#endif
    cnr3_diag_write_derived_health_summary_to_stderr(
        data->config.instance_id
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
        , &dsum04_ownership_balance_snapshot
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        , &data->dsum09_return_transfer
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
        , &dsum10_prune_eviction_snapshot
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
        , &data->dsum12_recovery_plan
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
        , &data->dsum13_recalculation
#endif
    );
#endif

    (void)teardown_clear_status;

    if (data->source != nullptr && vsapi != nullptr) {
        vsapi->freeNode(data->source);
        data->source = nullptr;
    }

    delete data;
}

void VS_CC cnr3_create_filter(
    const VSMap* in,
    VSMap* out,
    void* user_data,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)user_data;

    if (in == nullptr || out == nullptr || core == nullptr || vsapi == nullptr) {
        if (out != nullptr && vsapi != nullptr) {
            vsapi->mapSetError(out, "CNR3: invalid create-filter state.");
        }
        return;
    }

    int clip_error = 0;
    VSNode* source = vsapi->mapGetNode(in, "clip", 0, &clip_error);

    if (clip_error != peSuccess || source == nullptr) {
        vsapi->mapSetError(out, "CNR3: clip argument is required.");
        return;
    }

    const VSVideoInfo* source_info = vsapi->getVideoInfo(source);

    if (source_info == nullptr) {
        vsapi->freeNode(source);
        vsapi->mapSetError(out, "CNR3: clip must be a video node.");
        return;
    }

    Cnr3CreateOptions create_options{};

    if (!cnr3_parse_create_options(in, out, vsapi, create_options)) {
        vsapi->freeNode(source);
        return;
    }

    Cnr3FilterData* data = new (std::nothrow) Cnr3FilterData{};

    if (data == nullptr) {
        vsapi->freeNode(source);
        vsapi->mapSetError(out, "CNR3: failed to allocate filter instance data.");
        return;
    }

    data->source = source;
    data->video_info = *source_info;
    data->config = cnr3_make_default_instance_config();

    if (!cnr3_instance_config_is_valid(data->config)) {
        cnr3_free_filter(data, core, vsapi);
        vsapi->mapSetError(out, "CNR3: failed to initialise instance configuration.");
        return;
    }

#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)
    VSCoreInfo core_info{};
    vsapi->getCoreInfo(core, &core_info);
    data->plan_retry_max = cnr3_planretry_derive_max_attempts(core_info.numThreads);
#endif

#if defined(CNR3_EMIT_PLUGIN_STARTUP_PROVENANCE)
    std::fprintf(
        stderr,
        "CNR3[%d] INFO CONFIG: edit_version=%s\n",
        data->config.instance_id.value,
        CNR3_EDIT_VERSION
    );
    std::fflush(stderr);

    std::fprintf(
        stderr,
        "CNR3[%d] INFO CONFIG: filter_mode=%s (compile-time selector)\n",
        data->config.instance_id.value,
        CNR3_SELECTED_FILTER_MODE_TEXT
    );
    std::fflush(stderr);
#endif

    const Cnr3Status pixel_config_status = cnr3_initialise_k1e2_live_pixel_config(
        *source_info,
        create_options,
        *data
    );

    if (!cnr3_status_is_ok(pixel_config_status)) {
        cnr3_free_filter(data, core, vsapi);
        vsapi->mapSetError(
            out,
            "CNR3 create-time option config: unsupported clip format, scene config failure, or response-table build failure."
        );
        return;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
    cnr3_memory_record_and_print_snapshot(
        data->dsum02_memory,
        data->config.instance_id,
        "at cnr3_create (baseline)",
        true
    );
#endif

    VSFilterDependency dependencies[] = {
        { source, rpGeneral }
    };

    vsapi->createVideoFilter(
        out,
        "CNR3",
        &data->video_info,
        cnr3_get_frame_keystone_live_k1f_proof,
        cnr3_free_filter,
        CNR3_SELECTED_FILTER_MODE,
        dependencies,
        1,
        data,
        core
    );
}

} // namespace

VS_EXTERNAL_API(void) VapourSynthPluginInit2(
    VSPlugin* plugin,
    const VSPLUGINAPI* vspapi
) {
    if (plugin == nullptr || vspapi == nullptr) {
        return;
    }

    vspapi->configPlugin(
        "com.hydra3333.cnr3",
        "cnr3",
        "CNR3",
        VS_MAKE_VERSION(0, 1),
        VAPOURSYNTH_API_VERSION,
        0,
        plugin
    );

    vspapi->registerFunction(
        "CNR3",
        "clip:vnode;"
        "y_threshold:int:opt;"
        "y_strength:int:opt;"
        "u_threshold:int:opt;"
        "u_strength:int:opt;"
        "v_threshold:int:opt;"
        "v_strength:int:opt;"
        "y_curve:data:opt;"
        "u_curve:data:opt;"
        "v_curve:data:opt;"
        "scene_threshold:float:opt;"
        "scene_chroma:int:opt;",
        "clip:vnode;",
        cnr3_create_filter,
        nullptr,
        plugin
    );
}
