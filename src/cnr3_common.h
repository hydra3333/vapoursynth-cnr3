#pragma once

/*
    CNR3 minimal common definitions.

    CMS07-A.2 skeleton only.

    This header must remain small. It must not become a replacement for the old
    monolithic Cnr3Data structure.

    Do not add:
        - old strict-streaming cache state;
        - CMS06 output-cache-manager state;
        - old proof-phase fields;
        - pixel-processing state;
        - response-table storage;
        - cache-core runtime state;
        - VapourSynth getFrame lifecycle state.

    Instance identity boundary:
        A process-wide atomic monotonic allocator may later assign
        human-readable instance_id values at filter instance creation.

        That allocator is only for diagnostics and log separation.

        It is not shared filter state and must not be used for cache ownership,
        frame lookup, scheduling, recovery, or cross-instance communication.

        Each CNR3 filter instance owns its own cache, diagnostics accumulators,
        processing configuration, response tables, memory statistics, and
        runtime state.
*/