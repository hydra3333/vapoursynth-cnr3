#pragma once

#include "cnr3_common.h"

/*
    CNR3 cache-core selftest scaffold.

    CMS07-C.2 introduces the first compiled isolated selftest entry point.

    The selftest added in this phase checks only the empty cache-core data model
    introduced by CMS07-C.1. It does not mutate cache state and does not exercise
    store, lookup, remove, prune, pins, checkpoints, hot zones, recovery, or
    teardown.

    Selftests in this module must remain isolated from VapourSynth getFrame
    scheduling and pixel processing. Their job is to exercise cache-core
    ownership, slot/index state, pins, checkpoint retention, hot-zone policy,
    prune decisions, recovery planning, and teardown discipline before those
    mechanisms are connected to VapourSynth frame processing.

    This module must not contain:
        - VapourSynth getFrame wiring;
        - source-frame request/retrieve lifecycle code;
        - pixel loops;
        - response-table implementation;
        - production cache authority hidden behind test code;
        - behaviour required for production correctness.

    A selftest may call public cache-core APIs after those APIs exist. It must
    not reach through private cache internals unless that access is introduced
    deliberately for a named selftest phase.
*/

/*
    Run the CMS07-C.2 empty cache-core model selftest.

    Return Cnr3Status::ok only when a default-constructed Cnr3OutputCacheCore
    reports the expected empty state.

    This function does not print. A later test-runner phase may decide how to
    call selftests and how to report failures.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_empty_model() noexcept;
