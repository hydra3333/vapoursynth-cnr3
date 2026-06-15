#pragma once

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "cnr3_common.h"

// -----------------------------------------------------------------------------
// CNR3 frame-internal processing
//
// These helpers perform the per-frame pixel/plane work for CNR3. They are
// intentionally separate from the VapourSynth lifecycle, parameter parsing,
// cache orchestration, and plugin registration code in vapoursynth-Cnr3.cpp.
//
// This layer may examine, copy, downsample, compare, and manipulate frame plane
// data, but it must not own cache policy or VapourSynth scheduling policy.
// -----------------------------------------------------------------------------

bool process_cnr3_frame_with_explicit_previous_output(
    const Cnr3Data* d,
    int frame_number,
    const VSFrame* src,
    const VSFrame* previous_output,
    VSFrame* dst,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
);

bool process_cnr3_frame(
    const Cnr3Data* d,
    int frame_number,
    const VSFrame* src,
    VSFrame* dst,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
);
