#include "cnr3_build_config.h"
#include "cnr3_memory_diagnostics.h"
#include "cnr3_cache.h"

void old_cnr3_strict_cache_clear(
    OldCnr3StrictStreamCache &cache,
    const VSAPI *vsapi
) {
    if (cache.prev_output != nullptr) {
        vsapi->freeFrame(cache.prev_output);
        cache.prev_output = nullptr;
    }

    cache.next_needed = 0;
}

void cnr3_cache_store_output_frame(
    OldCnr3StrictStreamCache &cache,
    const VSFrame *output_frame,
    int frame_number,
    const VSAPI *vsapi
) {
    if (cache.prev_output != nullptr) {
        vsapi->freeFrame(cache.prev_output);
        cache.prev_output = nullptr;
    }

    /*
        addFrameRef() keeps an additional reference to the frame. It does not
        deep-copy pixel data. That is fine here because VapourSynth frames are
        immutable after being returned.

        The stored reference must later be released with freeFrame().
    */
    cache.prev_output = vsapi->addFrameRef(output_frame);

    /*
        In strict streaming mode, after output frame N has been produced,
        the next frame we can correctly accept is N + 1.
    */
    cache.next_needed = frame_number + 1;
}
