#pragma once

/*
    CNR3 frame-processing scaffold.

    CMS07-B.2.7 keeps this module as a future pixel-layer boundary only.

    The settled algorithmic boundary is:

        output[N] depends on source[N] and previous filtered output[N-1].

    The predecessor is the previous filtered OUTPUT frame, not source[N-1].
    That distinction is essential to CNR2/CNR3 recursive temporal behaviour and
    must not be weakened when this module later receives implementation code.

    CMS07.0 V8.1 controls the future pixel-layer arithmetic. In summary:
        - operate on native subsampling;
        - operate at native integer bit depth;
        - preserve 8-bit-domain parameter semantics;
        - use the settled CNR2-compatible response-table behaviour;
        - use a sufficiently wide accumulator for weighted blends;
        - perform scene-change/reset decisions inside the compute path.

    This module must not own or inspect:
        - cache slots, pins, checkpoints, hot zones, prune, or recovery state;
        - VapourSynth request lifecycle state;
        - per-instance cache authority;
        - diagnostics counters or summary printers.

    Future implementation may use VapourSynth frame data only in an explicit
    pixel-layer phase. CMS07-B.2.7 intentionally introduces no VSFrame access,
    no pixel loops, no response-table calls, no scene-change implementation,
    no diagnostics output, and no cache dependency.
*/
