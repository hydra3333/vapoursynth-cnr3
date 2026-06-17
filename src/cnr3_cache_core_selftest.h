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

/*
    Run the CMS07-C.3 slot-ID source selftest.

    This checks only the isolated ID source. It does not create cache slots,
    insert into the frame index, store frames, or mutate Cnr3OutputCacheCore.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_slot_id_source() noexcept;

/*
    Run the CMS07-C.4 invalid-store selftest.

    This verifies that the store mutator rejects an empty owned-frame wrapper
    without mutating the cache. It does not create a real VSFrame and does not
    exercise successful frame storage.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_store_rejects_empty_owned_frame() noexcept;

/*
    Run the CMS07-C.4A / CMS07-E.1A successful-store and duplicate-store
    selftest.

    This uses a synthetic VSAPI freeFrame stub and fake opaque VSFrame pointers.
    It proves first-in-best-dressed store behaviour and duplicate-loser release
    with per-frame release counters, so a leaked or double-freed loser fails the
    test. It does not allocate real VapourSynth frames, call addFrameRef(), or
    introduce AS2 store-and-pin-record behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_store_success_and_duplicate() noexcept;

/*
    Run the CMS07-E.3A checkpoint-store flag lifecycle selftest.

    This verifies that checkpoint store sets checkpoint classification during
    the store operation, that checkpoint status is not a pin, that clear can
    detach an unpinned checkpoint, and that duplicate stores apply monotonic
    checkpoint classification: checkpoint-eligible duplicates promote an
    existing non-checkpoint slot, while non-checkpoint duplicates never demote an
    existing checkpoint. Per-frame release counts make loser leaks and
    double-frees fail the proof. It does not introduce AS2 store/adopt/pin/record,
    prune, recovery, or getFrame behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_checkpoint_store_flag_lifecycle() noexcept;

/*
    Run the CMS07-F.1A central remove helper selftest.

    This verifies that the central remove helper rejects pinned slots, detaches
    unpinned slots from the frame index and checkpoint-position list, updates
    compacted slot positions, and releases detached frames outside the cache
    lock. It does not introduce prune policy, hot zones, recovery, AS2, or
    getFrame behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_central_remove_helper_lifecycle() noexcept;

/*
    Run the CMS07-C.5A lookup/addref selftest.

    This uses a synthetic VSAPI addFrameRef/freeFrame stub pair and fake opaque
    VSFrame pointers. It does not allocate real VapourSynth frames and does not
    introduce pinning, checkpoints, prune, recovery, or getFrame wiring.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_lookup_addref_hit_and_miss() noexcept;

/*
    Run the CMS07-C.6A clear/teardown selftest.

    This verifies that clear() detaches retained cached frames, leaves the
    cache empty and invariant-clean, and releases each retained frame reference
    exactly once through the synthetic VSAPI freeFrame stub.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_clear_teardown_releases_cached_frames_once() noexcept;

/*
    Run the CMS07-C.7 slot pin/unpin selftest.

    This verifies that slot pins are balanced liveness reservations only. It
    checks hit/miss pin behaviour, clear rejection while pinned, unpin balance,
    token invalidation, and absence of addFrameRef/freeFrame side effects during
    pin/unpin.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_slot_pin_unpin_lifecycle() noexcept;

/*
    Run the CMS07-C.8 lookup-pin reservation selftest.

    This verifies the lookup-pin reservation lifecycle through the AS1 combined
    helper. A public lookup-pin-without-record helper must not be reintroduced.
    No addFrameRef(), freeFrame(), frame transfer, checkpoint, prune, recovery,
    or getFrame behaviour is introduced.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_lookup_pin_reservation_lifecycle() noexcept;

/*
    Run the CMS07-D.1 per-invocation pin-list selftest.

    This verifies that a pin list consumes recorded pin tokens, discharges them
    exactly once through Cnr3OutputCacheCore::unpin_frame(), and remains safe to
    discharge repeatedly after a clean discharge. It does not introduce
    getFrame wiring, source lifecycle handling, pixel behaviour, prune,
    checkpoints, hot zones, recovery, or D-SUM production counters.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_per_invocation_pin_list_lifecycle() noexcept;

/*
    Run the CMS07-D.3A / CMS07-E.2A AS1 lookup-pin-record atomicity selftest.

    This verifies that the AS1 combined helper records a pin through the
    cache-core operation without exposing a public caller-owned transient token
    gap. CMS07-E.2A reconciles the original E.2 lookup-pin-record helper
    obligation to this existing combined helper and proof.
    It does not introduce AS2 store/adopt/pin/record, checkpoints, prune,
    recovery, getFrame wiring, or D-SUM production counters.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_as1_lookup_pin_record_atomicity() noexcept;

/*
    Permanent cache-core selftest runner result.

    This is test infrastructure, not production diagnostics. It is intentionally
    independent of the later C.14 D-SUM counters so C.14A can still prove that
    production diagnostic counters are observe-only.
*/
struct Cnr3CacheCoreSelftestRunResult {
    int total_count = 0;
    int passed_count = 0;
    int failed_count = 0;
    const char* first_failed_test_name = nullptr;
    Cnr3Status first_failed_status = Cnr3Status::ok;
};

/*
    Run all cache-core selftests currently implemented.

    The runner records all pass/fail counts and preserves the first failing
    selftest name/status for the C.6C console harness. This function performs
    no printing and does not depend on plugin registration or VapourSynth
    getFrame scheduling.
*/
[[nodiscard]] Cnr3CacheCoreSelftestRunResult cnr3_cache_core_selftest_run_all() noexcept;

[[nodiscard]] bool cnr3_cache_core_selftest_run_result_passed(
    const Cnr3CacheCoreSelftestRunResult& result
) noexcept;