#pragma once

/*
    CNR3 response tables scaffold.

    CMS07-B.2.6 keeps this module as a future pixel-layer boundary only.

    The old response-table code is useful salvage reference, but it must not be
    copied back as active implementation during this scaffold phase. Actual
    response-table implementation belongs in a later explicit pixel-layer phase
    governed by CMS07.0 V8.1.

    Future responsibilities of this module may include:
        - CNR2-compatible response-table construction;
        - signed chroma-difference lookup support;
        - mode-dependent narrow/wide response behaviour;
        - bit-depth-aware scaling while preserving 8-bit-domain semantics;
        - table validation used by the pixel-processing layer.

    This module must not own or inspect:
        - VapourSynth nodes;
        - VapourSynth maps;
        - VapourSynth frame requests;
        - cache slots, pins, checkpoints, hot zones, prune, or recovery state;
        - diagnostics counters or summary printers.

    CMS07-B.2.6 intentionally introduces no tables, no table-building logic,
    no mode parsing, no pixel-processing arithmetic, no diagnostics output, and
    no dependency on VapourSynth headers.
*/