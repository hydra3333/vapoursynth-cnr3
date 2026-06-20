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
    Run the CMS07-G.12A AS2 store-pin-record checkpoint atomicity selftest.

    This verifies that the combined AS2 helper stores or adopts a frame, applies
    monotonic checkpoint promotion, pins the resulting slot, and records the pin
    in one public operation. It proves duplicate loser frames are released once,
    first-in-best-dressed frame data survives duplicates, checkpoint-eligible
    duplicates promote existing non-checkpoints, and non-checkpoint duplicates
    never demote existing checkpoints. It does not add recovery, getFrame wiring,
    source lifecycle handling, prune policy, or pixel behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_as2_store_record_monotonic_checkpoint() noexcept;

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
    Run the CMS07-F.2A bounded selected-detach selftest.

    This verifies that a bounded batch detach removes at most the configured
    number of already-selected unpinned candidates in one lock-owning operation,
    releases detached frames after the lock, preserves pinned slots, and keeps
    index/checkpoint invariants clean. It proves the AS5 batch-detach shape
    without adding final prune policy, hot zones, recovery, AS2, or getFrame
    behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_bounded_selected_detach_lifecycle() noexcept;

/*
    Run the CMS07-F.3A unpinned non-checkpoint selection/detach selftest.

    This verifies that the narrow AS5 candidate-selection layer detaches only
    unpinned non-checkpoint slots, bounds one lock-owning pass, leaves pinned
    and checkpoint slots cached, and releases detached frames after the lock. It
    does not add final prune policy, hot zones, recovery, AS2, or getFrame
    behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_unpinned_noncheckpoint_selection_lifecycle() noexcept;

/*
    Run the CMS07-F.4A checkpoint retention-boundary selection selftest.

    This verifies that the checkpoint-side bounded selection helper detaches
    only unpinned checkpoint slots above a caller-provided retain floor, never
    removes frame 0, never removes pinned checkpoints, never removes
    non-checkpoints, and releases detached frame references after the cache lock
    exits. It does not introduce hot-zone exclusion, distance ordering, active
    ceiling policy, recovery, getFrame wiring, or D-SUM production counters.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_checkpoint_retention_boundary_lifecycle() noexcept;

/*
    Run the CMS07-G.1A cache policy constants selftest.

    This verifies the CMS07.1 cache policy numbers and coherence relationships
    without adding active-ceiling calculation, hot-zone state, prune ordering,
    recovery, AS2, getFrame wiring, or D-SUM production counters.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_cache_policy_constants() noexcept;

/*
    Run the CMS07-G.2A hot-zone data-model selftest.

    This verifies only the hot-zone storage shape and invariants. It does not
    slide, spawn, merge, retire, apply zones to prune policy, perform recovery,
    or connect to VapourSynth getFrame scheduling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_hot_zone_data_model() noexcept;

/*
    Run the CMS07-G.3A hot-zone slide/spawn selftest.

    This verifies only the first hot-zone update behaviour: slide the nearest
    active zone within the jump threshold, or spawn a new zone when no active
    zone is close enough and capacity permits. It does not merge, retire, apply
    zones to prune policy, perform recovery, or connect to VapourSynth getFrame
    scheduling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_hot_zone_slide_spawn_lifecycle() noexcept;

/*
    Run the CMS07-G.4A hot-zone capacity-merge selftest.

    This verifies only the full-capacity merge fallback: when a distant
    observation arrives and all hot-zone slots are active, the two closest
    existing zones merge conservatively and the new observation spawns a zone in
    the freed slot. It does not retire, apply zones to prune policy, perform
    recovery, or connect to VapourSynth getFrame scheduling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_hot_zone_capacity_merge_lifecycle() noexcept;

/*
    Run the CMS07-G.5A hot-zone retirement/decay selftest.

    This verifies only the hot-zone retirement predicate: decay margin elapsed
    since the last observation and no pinned cache slot inside the zone range.
    It does not apply zones to prune policy, remove cache slots, perform
    recovery, or connect to VapourSynth getFrame scheduling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_hot_zone_retirement_decay_lifecycle() noexcept;

/*
    Run the CMS07-G.6A D-SUM-11 hot-zone counter model selftest.

    This proves counter updates for create, slide, merge, decay, expiry, zone
    count sampling, protected-range sampling, and the zero baseline for prune
    rejection before AS5 prune execution. It does not format or print the
    D-SUM-11 summary.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_hot_zone_dsum11_counter_model() noexcept;

/*
    Run the CMS07-G.7A hot-zone prune-protection selection selftest.

    This proves the hot-zone exclusion clause for future prune selection: the
    bounded selector may detach only unpinned non-checkpoint slots outside every
    active hot zone. It does not assemble the full CMS07 section 7.1 predicate,
    apply greatest-distance ordering, implement the section 7.2 capacity
    trigger, wire D-SUM prune counters, perform recovery, or connect to
    VapourSynth getFrame scheduling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_hot_zone_prune_protection_selection_lifecycle() noexcept;

/*
    Run the CMS07-G.8A prune-victim distance-ordering selftest.

    This verifies ordering among already-eligible prune candidates by greatest
    distance from the nearest active hot-zone boundary. It proves ordering only;
    it does not detach slots, implement the full CMS07 section 7.1 predicate,
    apply the section 7.2 trigger, or wire D-SUM counters.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_prune_victim_distance_ordering() noexcept;

/*
    Run the CMS07-G.9A composite prune-candidate selection selftest.

    This proves selection-only assembly of the already-proven predicate clauses:
    unpinned, outside every active hot zone, checkpoint-retention permitted for
    checkpoints, and externally supplied capacity permission for non-checkpoints.
    It does not detach, free, trigger pruning, or implement production AS5.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_composite_prune_candidate_selection() noexcept;

/*
    Run the CMS07-G.10A prune-trigger decision selftest.

    This proves the CMS07 section 7.2 active-ceiling / overflow-factor trigger
    arithmetic only. It verifies that pruning fires only past the overflow
    threshold and targets active_ceiling, not the trigger threshold or empty.
    It does not select victims, detach, free, or implement production AS5.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_prune_trigger_decision_hysteresis() noexcept;

/*
    Run the CMS07-G.11A AS5 prune execution selftest.

    This composes the proven trigger decision and composite candidate selector
    into one bounded decide/detach/free pass. It verifies no-prune boundary
    behaviour, bounded prune execution, selected victim release after the public
    helper returns, survivor preservation, pin preservation, D-SUM-11 hot-zone
    prune-rejection counter wiring, and cache invariant cleanliness. It does
    not perform recovery, connect to getFrame, or touch pixel behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_as5_prune_execution_decide_detach_free() noexcept;

/*
    Run the CMS07-H.1A AS1 bounded recovery search scaffold selftest.

    This proves only read-only recovery planning: descending bounded search from
    requested_frame - 1, inclusive lower-bound stopping, checkpoint-flag
    irrelevance for anchor selection, requested-frame exclusion from the hole
    catalogue, and safe lower-bound clamping near frame 0. It does not pin,
    store via AS2, recompute, request source frames, return frames, prune, or
    touch pixel behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_as1_bounded_recovery_search_scaffold() noexcept;

/*
    Run the CMS07-H.2A AS1 recovery anchor pin-record selftest.

    This composes the bounded recovery search with anchor pin-recording under one
    cache-lock acquisition. It proves anchor-found pin recording, no-anchor and
    requested-frame-only no-pin cases, and the ordered clear-refused-while-
    pinned / discharge / clear-succeeds sequence. It does not call AS2, request
    source frames, recompute, return frames, prune, wire getFrame, change D-SUM
    gates, or touch pixel behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_as1_recovery_anchor_pin_record() noexcept;

/*
    Run the CMS07-C.13B recovery-plan contiguity guard selftest.

    This verifies the production hard-status guard for the current minimal
    nearest-anchor + contiguous-hole recovery model. It proves reachable planner
    output satisfies the contiguity contract and that a hand-constructed
    non-contiguous plan is rejected before AS2 delegation. It does not implement
    AS3, sparse planning, D-SUM counters, stderr emission, getFrame wiring,
    source lifecycle handling, or pixel behaviour.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_recovery_plan_contiguity_guard() noexcept;

/*
    Run the CMS07-C.14A aggregate cache-core workload selftest.

    This capstone proof composes AS2 store/adopt, checkpoint monotonicity,
    hot-zone/prune execution, recovery anchor pin-record, recovery AS2 hole
    consumption, and the C.13B contiguity guard in one labelled aggregate
    workload. Behavioural assertions must not read D-SUM counters; the same
    test must pass when CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE is compiled out.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_aggregate_cache_core_workload() noexcept;

/*
    Run the CMS07-P.1A response-table vector proof.

    This is the first pixel-number proof after the C.14A cache-core milestone.
    It temporarily uses the existing selftest runner so the four-way harness,
    count discipline, and forced-fail machinery remain unchanged. It proves
    exact integer vscnr2-compatible response-table values and does not touch
    cache-core behaviour, VapourSynth getFrame lifecycle, Cnr3Data/VSMap
    parsing, or predecessor/recovery logic.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_response_table_vector_proof() noexcept;

/*
    Run the CMS07-P.2A response-table configuration surface proof.

    This is the second pixel-number proof after the C.14A cache-core milestone.
    It temporarily uses the existing selftest runner so the four-way harness,
    count discipline, and forced-fail machinery remain unchanged. It proves
    explicit Y/U/V response-table configuration and native-bit-depth scaling.
    The Y table is a luma-difference response for the later chroma blend, not
    a luma filter. Native subsampling traversal, downSampleLuma, and the int64
    blend remain deliberately deferred to P.3A/P.4A/P.5A.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_response_table_config_surface_proof() noexcept;

/*
    Run the CMS07-P.3A weighted chroma blend vector proof.

    This is the third pixel-number proof after the C.14A cache-core milestone.
    It temporarily uses the existing selftest runner so the four-way harness,
    count discipline, and forced-fail machinery remain unchanged. It proves
    the source-confirmed vsCnr2 weighted chroma blend arithmetic with an
    int64 accumulator, including the max-response convex-combination boundary.
    P.2A native parameter scaling remains intentionally unchanged.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_weighted_chroma_blend_vector_proof() noexcept;

/*
    Run the CMS07-P.4A downsampled-luma vector proof.

    This is the fourth pixel-number proof after the C.14A cache-core milestone.
    It proves the source-confirmed downSampleLuma scalar shape, tap-coordinate
    mapping, degenerate 4:2:2/4:4:0/4:4:4 behaviour, right/bottom edge clamping,
    and invalid-input preservation without adding frame traversal or VapourSynth
    frame access.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_downsampled_luma_vector_proof() noexcept;

/*
    Run the CMS07-P.5A signed-difference/table-lookup blend proof.

    This is the fifth pixel-number proof after the C.14A cache-core milestone.
    It composes signed current-minus-previous differences, the P.1A total
    response-table lookup, the P.2A table geometry convention, and the P.3A
    weighted chroma blend helper without adding frame traversal, scene-change,
    or VapourSynth frame access.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_signed_difference_table_lookup_blend_proof() noexcept;

/*
    Run the CMS07-P.6A chroma-plane traversal vector proof.

    This composes P.5A over matching scalar sample buffers for one chroma
    plane. It proves row/column traversal, stride handling, padding preservation,
    and no-partial-publish invalid-input behaviour without VapourSynth frame
    access, source-frame lifecycle, cache authority, or scene-change handling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_chroma_plane_traversal_vector_proof() noexcept;

/*
    Run the CMS07-P.7A source-luma downsample plane traversal proof.

    This applies the P.4A scalar downsampled-luma tap and sample helpers across
    scalar source-luma buffers to produce the downsampled-luma planes consumed
    by P.6A. It proves expected chroma-grid dimensions, stride handling, edge
    clamping, padding preservation, and no-partial-publish invalid-input
    behaviour without real frame memory, VapourSynth frame access, cache
    authority, or scene-change handling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_source_luma_downsample_plane_traversal_proof() noexcept;

/*
    Run the CMS07-P.8A native byte-plane access vector proof.

    This proves synthetic native byte-stride sample load/store and whole-plane
    scalar/native copy discipline for 8-bit and 9..16-bit integer storage. It
    verifies that multi-byte column addressing uses x * storage_bytes and that
    invalid late samples publish no partial destination. It does not use
    VapourSynth frame pointers, source-frame lifecycle, cache authority, or
    scene-change handling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_native_byte_plane_access_vector_proof() noexcept;

/*
    Run the CMS07-P.9A native luma downsample bridge proof.

    This composes the P.8A synthetic native byte-plane access helpers with the
    P.7A scalar source-luma downsample traversal. It proves that native 8-bit
    and 10-bit source-luma byte buffers can produce the downsampled-luma scalar
    planes consumed by P.6A without using VapourSynth frame pointers,
    source-frame lifecycle, cache authority, or scene-change handling.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_native_luma_downsample_bridge_proof() noexcept;

/*
    Run the CMS07-H.3A AS2 recovery store-consumer selftest.

    This verifies that genuine holes from a bounded recovery plan are consumed
    through the existing AS2 store/adopt/pin-record helper, and that every AS2
    call contributes one recorded pin to the per-invocation pin-list. The proof
    includes the duplicate/adopt case: a racing winner remains first-in-best-
    dressed, the incoming loser is released once, and the existing winner is
    pinned and recorded for later discharge. It does not introduce AS3 reused-
    frame pinning, source lifecycle handling, getFrame wiring, pixel behaviour,
    prune changes, or D-SUM diagnostics.
*/
[[nodiscard]] Cnr3Status cnr3_cache_core_selftest_as2_recovery_store_consumer() noexcept;

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
    Run all registered selftests currently implemented.

    The runner records all pass/fail counts and preserves the first failing
    selftest name/status for the C.6C console harness. This function performs
    no printing and does not depend on plugin registration or VapourSynth
    getFrame scheduling.
*/
void cnr3_cache_core_selftest_set_verbose(bool verbose) noexcept;

[[nodiscard]] Cnr3CacheCoreSelftestRunResult cnr3_cache_core_selftest_run_all() noexcept;

[[nodiscard]] bool cnr3_cache_core_selftest_run_result_passed(
    const Cnr3CacheCoreSelftestRunResult& result
) noexcept;