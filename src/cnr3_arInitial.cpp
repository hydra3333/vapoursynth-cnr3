/*
    CNR3 live getFrame arInitial branch planning.

    This translation unit owns the plugin-side arInitial dispatch and source
    request setup. It does not own cache policy, prune policy, hot-zone logic,
    or pixel processing.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_plugin_internal.h"

#include <new>

Cnr3LiveCacheHitStartResult cnr3_try_start_live_cache_hit_return(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (*frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: frameData was unexpectedly non-null at arInitial."
        );
        return Cnr3LiveCacheHitStartResult::failed;
    }

    Cnr3LiveGetFrameFrameData* request_data =
        new (std::nothrow) Cnr3LiveGetFrameFrameData{};

    if (request_data == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: failed to allocate frameData."
        );
        return Cnr3LiveCacheHitStartResult::failed;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::cache_hit_return;
    request_data->requested_frame = n;

    /*
        K.1F cache-hit branch-b: present output[N] is a plan-time fact
        made at arInitial and acted on at arAllFramesReady. Pin it now so
        prune cannot evict the cached frame across the activation gap.
    */
    const Cnr3Status pin_status = data.output_cache.lookup_frame_and_record_pin(
        n,
        request_data->pin_list
    );

    if (pin_status == Cnr3Status::not_found) {
        delete request_data;
        return Cnr3LiveCacheHitStartResult::miss;
    }

    if (!cnr3_status_is_ok(pin_status)) {
        delete request_data;
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: failed to pin cached output[N]."
        );
        return Cnr3LiveCacheHitStartResult::failed;
    }

    request_data->cache_hit_pin_taken = true;
    request_data->source_requested = true;
    *frame_data = request_data;

    /*
        Option C lifecycle trigger: the cache-hit path does not consume
        source[N] for pixel computation, but requests one real source frame so
        the non-source filter returns only from arAllFramesReady.
    */
    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return Cnr3LiveCacheHitStartResult::started;
}

const VSFrame* cnr3_start_live_predecessor_present_compute(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (*frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: frameData was unexpectedly non-null at arInitial."
        );
        return nullptr;
    }

    Cnr3LiveGetFrameFrameData* request_data =
        new (std::nothrow) Cnr3LiveGetFrameFrameData{};

    if (request_data == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: failed to allocate frameData."
        );
        return nullptr;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::predecessor_present_compute;
    request_data->requested_frame = n;
    request_data->predecessor_frame = n - 1;
    *frame_data = request_data;

    const Cnr3Status pin_status = data.output_cache.lookup_frame_and_record_pin(
        request_data->predecessor_frame,
        request_data->pin_list
    );

    if (!cnr3_status_is_ok(pin_status)) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: cached predecessor output[N-1] was not available."
        );
        return nullptr;
    }

    request_data->predecessor_pin_taken = true;
    request_data->source_requested = true;

    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return nullptr;
}
const VSFrame* cnr3_start_live_frame0_fresh_start(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (*frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: frameData was unexpectedly non-null at arInitial."
        );
        return nullptr;
    }

    Cnr3LiveGetFrameFrameData* request_data =
        new (std::nothrow) Cnr3LiveGetFrameFrameData{};

    if (request_data == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: failed to allocate frameData."
        );
        return nullptr;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::frame0_fresh_start;
    request_data->requested_frame = n;
    request_data->source_requested = true;
    *frame_data = request_data;

    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return nullptr;
}
const VSFrame* cnr3_arInitial(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    const Cnr3LiveCacheHitStartResult cache_hit_start =
        cnr3_try_start_live_cache_hit_return(
            n,
            data,
            frame_data,
            frame_ctx,
            vsapi
        );

    if (cache_hit_start == Cnr3LiveCacheHitStartResult::started ||
        cache_hit_start == Cnr3LiveCacheHitStartResult::failed) {
        return nullptr;
    }

    if (n > 2) {
        cnr3_trace_live_after_frame2_not_yet_implemented(data, n);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: frames after 2 are not implemented before recovery wiring."
        );
        return nullptr;
    }

    if (n == 1 || n == 2) {
        return cnr3_start_live_predecessor_present_compute(
            n,
            data,
            frame_data,
            frame_ctx,
            vsapi
        );
    }

    return cnr3_start_live_frame0_fresh_start(
        n,
        data,
        frame_data,
        frame_ctx,
        vsapi
    );
}
