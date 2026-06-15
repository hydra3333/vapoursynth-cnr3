#pragma once

/*
    CNR3 cache-core scaffold.

    CMS07-B.2.8 aligns this module as the future CMS07 cache-manager core
    boundary. It intentionally introduces no cache data structures or behaviour.

    Future responsibilities of this module include:
        - cache slot ownership;
        - ordered frame-number index;
        - non-checkpoint and checkpoint pools;
        - consumer pins and per-invocation pin-lists;
        - hot-zone state;
        - bounded recovery planning;
        - store / lookup / remove helpers;
        - prune / eviction;
        - integrity validation;
        - teardown / clear discipline.

    This module must not contain:
        - pixel loops;
        - response-table construction;
        - VapourSynth getFrame request/retrieve lifecycle code;
        - VSMap parsing;
        - D-SUM formatting or printing;
        - old strict-streaming authority;
        - CMS06 output-cache-manager state.

    VapourSynth frame-reference ownership may appear here later only where the
    cache core is explicitly responsible for retaining or releasing a cached
    VSFrame reference. That later implementation must obey the CMS07 ownership,
    pin, checkpoint, hot-zone, prune, and atomic-scope rules exactly.

    CMS07-B.2.8 does not expose a public cache API yet. Public cache functions
    will be introduced only when CMS07-C creates the first real cache data model.
*/