#pragma once

/*
    CNR3 memory diagnostics.

    CMS07-A.2 skeleton only.

    This module will later implement CMS07 / D-SUM-02 memory diagnostics.

    The old memory diagnostics files are approved salvage reference material,
    not direct authority.

    Expected later responsibilities:
        - take process/system memory snapshots;
        - accumulate sample statistics;
        - record baseline / pre-cleanup / post-cleanup / final samples;
        - print D-SUM-02 summaries to stderr.

    Memory movement is interpretive. It can help detect leaks, runaway growth,
    and failure to release after cleanup, but min-to-max movement alone is not
    proof of a leak. Persistent post-cleanup elevation is more important than
    normal in-run growth.
*/