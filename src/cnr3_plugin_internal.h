/*
    CNR3 private VapourSynth plugin-integration declarations.

    This header is intentionally private to the VapourSynth plugin translation
    units. It carries live getFrame frameData, branch tags, and helper
    declarations that must be shared after splitting vapoursynth-Cnr3.cpp.

    Cache algorithms, prune policy, hot-zone mechanics, and pixel loops remain
    in their own layers; this file only describes plugin-side integration state.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#pragma once

#include "cnr3_build_config.h"
#include "cnr3_cache_core.h"
#include "cnr3_common.h"
#include "cnr3_diagnostics.h"
#include "cnr3_frame_processing.h"
#include "cnr3_instance_config.h"
#include "cnr3_owned_frame_ref.h"
#include "cnr3_response_tables.h"

#include "VapourSynth4.h"

#include <vector>

struct Cnr3FilterData {
    VSNode* source = nullptr;
    VSVideoInfo video_info{};
    Cnr3InstanceConfig config{};
    Cnr3OutputCacheCore output_cache{};
    Cnr3ResponseTables response_tables{};
    int bits_per_sample = 0;
    int sub_sampling_w = -1;
    int sub_sampling_h = -1;
    double scene_change_scdthr = CNR3_P11C_DEFAULT_SCDTHR;
    Cnr3SceneChangeConfig scene_change_config{};
#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
    Cnr3DiagDsum01RequestOrderStats dsum01_request_order{};
#endif
};

struct Cnr3LiveOutputStoreRequest {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    bool force_checkpoint = false;
};

enum class Cnr3LiveGetFrameBranch {
    none,
    cache_hit_return,
    frame0_fresh_start,
    predecessor_present_compute,
    recovery
};

enum class Cnr3LiveRecoveryBranch {
    none,
    exact_anchor,
    floor_fresh_start
};

enum class Cnr3LiveRecoveryHoleOutcome {
    none,
    computed,
    adopted_skipped,
    adopted_post_compute_loser
};

struct Cnr3LiveGetFrameFrameData {
    Cnr3LiveGetFrameBranch branch = Cnr3LiveGetFrameBranch::none;
    int requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
    bool source_requested = false;
    bool predecessor_pin_taken = false;
    bool cache_hit_pin_taken = false;
    Cnr3LiveRecoveryBranch recovery_branch = Cnr3LiveRecoveryBranch::none;
    Cnr3CacheRecoverySearchPlan recovery_plan{};
    int recovery_floor_frame = CNR3_INVALID_FRAME_NUMBER;
    Cnr3LiveRecoveryHoleOutcome recovery_floor_outcome = Cnr3LiveRecoveryHoleOutcome::none;
    std::vector<int> source_request_frame_numbers{};
    std::vector<Cnr3LiveRecoveryHoleOutcome> per_hole_outcomes{};
    Cnr3CachePinList pin_list{};
};

void cnr3_set_filter_error(
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi,
    const char* message
) noexcept;

Cnr3Status cnr3_discard_frame_data_with_cache(
    void** frame_data,
    Cnr3OutputCacheCore& output_cache
) noexcept;

bool cnr3_live_store_status_allows_return(
    Cnr3Status status
) noexcept;

Cnr3Status cnr3_store_live_output_frame_for_return(
    Cnr3FilterData& data,
    const Cnr3LiveOutputStoreRequest& request,
    VSFrame* output_frame,
    const VSAPI* vsapi,
    std::uint64_t frame_byte_count,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) noexcept;

void cnr3_trace_live_frame0_fresh_start(
    const Cnr3FilterData& data,
    int requested_frame,
    bool source_requested,
    bool source_retrieved,
    bool copy_frame_succeeded,
    const char* store_status_name,
    bool frame_returned
) noexcept;

void cnr3_trace_live_predecessor_present_compute(
    const Cnr3FilterData& data,
    int requested_frame,
    int predecessor_frame,
    Cnr3Status store_status,
    const Cnr3CallerSuppliedFrameProcessSummary& process_summary
) noexcept;

void cnr3_trace_live_cache_hit_return(
    const Cnr3FilterData& data,
    int requested_frame
) noexcept;

void cnr3_trace_live_after_frame2_not_yet_implemented(
    const Cnr3FilterData& data,
    int requested_frame
) noexcept;

const VSFrame* cnr3_complete_live_recovery(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_get_frame_live_cache_hit_return(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
);

const VSFrame* cnr3_complete_live_predecessor_present_compute(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_complete_live_frame0_fresh_start(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_arInitial(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_arAllFramesReady(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);
