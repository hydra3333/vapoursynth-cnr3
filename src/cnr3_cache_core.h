#pragma once

/*
    CNR3 CMS07 cache core.

    CMS07-A.2 skeleton only.

    This module will own the CMS07 cache-manager core:
        - output frame reference slots;
        - ordered frame-number index;
        - non-checkpoint and checkpoint pools;
        - consumer-held pins;
        - per-invocation pin-lists;
        - checkpoint flags;
        - hot zones;
        - prune policy;
        - recovery planning;
        - validation;
        - cache diagnostics counters.

    This module must not contain:
        - pixel loops;
        - response-table construction;
        - memory-diagnostic internals;
        - VapourSynth getFrame orchestration;
        - parameter parsing.

    The exact CMS07 AS1-AS7 lock scopes are designer-owned and inviolable.
    They must be implemented exactly as CMS07.0 section 8.7 defines them.

    If comments in this file ever diverge from CMS07.0 section 8.7, CMS07.0
    wins and the comments must be corrected.
*/