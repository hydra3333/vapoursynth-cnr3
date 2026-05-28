#pragma once

#include "VapourSynth4.h"
#include "VSHelper4.h"

// -----------------------------------------------------------------------------
// CNR3 cache manager
//
// This is intentionally only the strict-streaming subset of the future cache
// manager design. The actual cache object is owned by Cnr3Data, so each CNR3
// filter instance has its own independent recursive state.
// -----------------------------------------------------------------------------

struct Cnr3CacheManager {
    /*
        Minimal cache/state manager.

        This is intentionally only the strict streaming subset of the future
        cache manager design.

        Invariant:
            prev_output holds a read-only reference to output[next_needed - 1],
            or nullptr before frame 0 has been processed.

        Initial Policy A:
            Only frame n == next_needed is accepted.

        Future Policy C can extend this struct with:
            reorder buffer
            recent output cache
            checkpoint store
            recovery state
            seek mode
    */
    const VSFrame *prev_output = nullptr;
    int next_needed = 0;
};
// -----------------------------------------------------------------------------
// END CNR3 cache manager
// -----------------------------------------------------------------------------

void cnr3_cache_clear(
    Cnr3CacheManager &cache,
    const VSAPI *vsapi
);

void cnr3_cache_store_output_frame(
    Cnr3CacheManager &cache,
    const VSFrame *output_frame,
    int frame_number,
    const VSAPI *vsapi
);
