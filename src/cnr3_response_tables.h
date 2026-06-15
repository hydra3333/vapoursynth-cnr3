#pragma once

/*
    CNR3 response table helpers.

    CMS07-A.2 skeleton only.

    This module will later build and read the signed-difference response tables
    used by the CNR2/vscnr2-style Y/U/V weighting model.

    Rules:
        - cache-independent;
        - no Cnr3Data dependency;
        - no direct cache dependency;
        - no pixel loops;
        - no VapourSynth scheduling logic.

    CMS07.0 section 13 V8.1 controls the pixel-layer arithmetic and
    response-table salvage rules. Do not independently redesign the arithmetic
    or table semantics here.
*/