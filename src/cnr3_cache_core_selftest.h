#pragma once

/*
    CNR3 cache-core selftest scaffold.

    CMS07-B.2.9 reserves this module as the future isolated selftest boundary
    for cache-core behaviour.

    The first real selftests must remain isolated from VapourSynth getFrame
    scheduling and pixel processing. Their job is to exercise cache-core
    ownership, slot/index state, pins, checkpoint retention, hot-zone policy,
    prune decisions, recovery planning, and teardown discipline before those
    mechanisms are connected to VapourSynth frame processing.

    Future responsibilities of this module may include:
        - synthetic request-order drivers;
        - cache slot/index invariant tests;
        - pin-list lifecycle tests;
        - lookup-reference balance tests;
        - first-in-best-dressed store tests;
        - checkpoint-retention tests;
        - hot-zone update/merge/retire tests;
        - prune candidate/victim-selection tests;
        - bounded recovery-planning tests;
        - teardown/clear balance tests.

    This module must not contain:
        - VapourSynth getFrame wiring;
        - source-frame request/retrieve lifecycle code;
        - pixel loops;
        - response-table implementation;
        - production cache authority hidden behind test code;
        - behaviour required for production correctness.

    A selftest may call public cache-core APIs after CMS07-C introduces them.
    It must not reach through private cache internals unless that access is
    introduced deliberately for a named selftest phase.

    CMS07-B.2.9 intentionally introduces no public selftest entry point. The
    entry point and first executable selftest belong in a later explicit phase,
    after the cache-core data model exists.
*/
