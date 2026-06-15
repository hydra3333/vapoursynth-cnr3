#pragma once

/*
    CNR3 per-instance configuration and identity.

    CMS07-A.2 skeleton only.

    This layer will later hold immutable or mostly immutable per-instance
    configuration created from VapourSynth user parameters and validated source
    format information.

    Intended responsibilities:
        - human-readable instance identity;
        - user parameter storage after parsing;
        - validated format configuration;
        - scaled threshold / processing configuration;
        - separation between plugin parameter parsing and cache internals.

    This layer must not own:
        - cache slots;
        - pins;
        - recovery plans;
        - frameData lifecycle state;
        - VapourSynth source request/retrieve state;
        - pixel loops.
*/