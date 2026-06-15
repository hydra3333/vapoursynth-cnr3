#pragma once

/*
    CNR3 cache diagnostics.

    CMS07-A.2 skeleton only.

    This module will later hold ongoing D-SUM cache summaries.

    Early cache-core summaries:
        D-SUM-04  Ownership / pin / lookup-ref balance summary
        D-SUM-05  Cache integrity / teardown summary
        D-SUM-08  Cache store / duplicate-store / first-in-best-dressed summary
        D-SUM-10  Prune / eviction safety summary
        D-SUM-11  Hot-zone operation summary

    Later recovery summaries:
        D-SUM-03  Recovery-search summary
        D-SUM-12  Recovery planning / hole-filling summary
        D-SUM-13  Recalculation histogram

    Diagnostics must:
        - print to stderr only;
        - observe only when using DIAG_* gates;
        - perform formatting and printing outside locked/atomic cache scopes;
        - remain separate from temporary SCAFFOLD_* proof logic.
*/