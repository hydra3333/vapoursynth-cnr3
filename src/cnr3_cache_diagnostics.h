#pragma once

/*
    CNR3 cache diagnostics scaffold.

    CMS07-B.2.4 keeps this module cache-specific.

    Generic stderr output belongs in cnr3_diagnostics.*. This module must not
    own generic print/flush helpers because memory diagnostics, VapourSynth
    integration, future proof summaries, and other modules also need diagnostic
    output without depending on cache diagnostics.

    This module is reserved for future cache-specific diagnostic state and
    D-SUM support, for example:
        - cache integrity summaries;
        - ownership / pin / lookup-ref balance summaries;
        - store / duplicate-store summaries;
        - prune / eviction summaries;
        - hot-zone summaries;
        - recovery-search and recovery-plan summaries.

    CMS07-B.2.4 intentionally introduces no cache diagnostic counters, no D-SUM
    printers, no cache-state inspection, and no stderr output from this module.
*/
