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

#include <cstdio>
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

inline constexpr int CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT = 255;
inline constexpr int CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT = 255;

Cnr3ResponseTableConfig cnr3_make_k1e2_proof_default_response_table_config(
    int sample_peak
) noexcept {
    Cnr3ResponseTableConfig config{};
    config.sample_peak = sample_peak;

    /*
        CMS07-K.1E.2 proof/default response-table config: enough to drive
        the proven P.11B live path; final user option parsing and full
        default-policy surface are deferred to the post-keystone
        instance-config/error-surface step.
    */
    config.y.threshold_8bit = CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT;
    config.y.strength_8bit = CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT;
    config.y.curve = Cnr3ResponseCurveKind::narrow;

    config.u.threshold_8bit = CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT;
    config.u.strength_8bit = CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT;
    config.u.curve = Cnr3ResponseCurveKind::narrow;

    config.v.threshold_8bit = CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT;
    config.v.strength_8bit = CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT;
    config.v.curve = Cnr3ResponseCurveKind::narrow;

    return config;
}

Cnr3Status cnr3_initialise_k1e2_live_pixel_config(
    const VSVideoInfo& video_info,
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
    data.scene_change_scdthr = CNR3_P11C_DEFAULT_SCDTHR;

    Cnr3Status status = cnr3_make_scene_change_config_from_vscnr2_scdthr(
        data.scene_change_scdthr,
        video_info.width,
        video_info.height,
        data.bits_per_sample,
        data.sub_sampling_w,
        data.sub_sampling_h,
        false,
        data.scene_change_config
    );

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    const int sample_peak = (1 << data.bits_per_sample) - 1;
    const Cnr3ResponseTableConfig table_config =
        cnr3_make_k1e2_proof_default_response_table_config(sample_peak);

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

    std::fprintf(
        stderr,
        "CNR3[%d] INFO CONFIG: filter_mode=%s (compile-time selector)\n",
        data->config.instance_id.value,
        CNR3_SELECTED_FILTER_MODE_TEXT
    );
#endif

    const Cnr3Status pixel_config_status = cnr3_initialise_k1e2_live_pixel_config(
        *source_info,
        *data
    );

    if (!cnr3_status_is_ok(pixel_config_status)) {
        cnr3_free_filter(data, core, vsapi);
        vsapi->mapSetError(
            out,
            "CNR3 K.1E.2 proof/default config: unsupported clip format or response-table build failure."
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
        "clip:vnode;",
        "clip:vnode;",
        cnr3_create_filter,
        nullptr,
        plugin
    );
}
