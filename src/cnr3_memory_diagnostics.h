#pragma once

/*
    CNR3 memory diagnostics scaffold.

    CMS07-B.2.5 keeps this module memory-specific and aligns it with the
    generic diagnostics-output boundary introduced in CMS07-B.2.4.

    Generic stderr output belongs in cnr3_diagnostics.*.

    Cache diagnostics belong in cnr3_cache_diagnostics.* and must not be used as
    a generic print/flush helper by this module.

    This module is reserved for future memory-specific diagnostics, including
    D-SUM-02 support, for example:
        - process working-set observations;
        - process private-usage observations;
        - system memory observations;
        - min/average/max tracking;
        - baseline/final/post-cleanup comparisons;
        - human interpretation of memory movement.

    CMS07-B.2.5 intentionally introduces no memory sampling, no D-SUM-02
    counters, no D-SUM-02 printer, no active stderr output, and no dependency on
    cache diagnostics.

    Future implementation rule:
        When memory diagnostics later need to write stderr text, they should call
        cnr3_diagnostics.* helpers directly. They must not route output through
        cnr3_cache_diagnostics.*.

    Lock-scope rule:
        Future memory formatting and printing must not occur while holding a
        CMS07 cache atomic/locked scope.
*/