#include "cnr3_frame_processing.h"

/*
    CMS07-B.2.7 frame-processing placeholder.

    Pixel-layer implementation will be added only in a later explicit phase.
    That phase must preserve the settled recursive boundary:

        output[N] = f(source[N], previous filtered output[N-1])

    It must not reintroduce old strict-streaming authority, old source[N-1]
    predecessor logic, cache lookup authority, or VapourSynth request lifecycle
    decisions into this module.
*/