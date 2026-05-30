#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <unordered_map>

#include "VapourSynth4.h"
#include "VSHelper4.h"

// -----------------------------------------------------------------------------
// CNR3 cache manager - v005 design structures
//
// CRITICAL DESIGN RULE:
//      The cache-manager specification has the critical rule that
//      ALL CACHES MUST ALWAYS BE STRICTLY ORDERED BY FRAME NUMBER and
//      all cache-related operations must STRICTLY use only frame-number ordering.
//      Everything touching frame/checkpoint cache state MUST COMPLY with this rule
//      at all times, specifically including: every operation touching
//      output frame cache state, checkpoint cache state, cache indexes,
//      checkpoint lookup, pruning, promotion, and pin_count handling.
//
// THREAD SAFETY:
//      Any code path that reads or writes mutable cache-manager state may run
//      while another thread is also interacting with the same CNR3 filter instance.
//
//      Therefore, any code path that reads or writes mutable cache-manager state
//      MUST hold the cache manager's per-instance cache_mutex while doing so.
//
//      This applies to at least:
//          - cnr3_cache_manager_* helper functions
//          - cnr3_get_frame() code that directly touches cache-manager state
//          - pruning code
//          - checkpoint promotion code
//          - pin_count handling
//          - cache index updates
//          - future debug/statistics counters stored inside the cache manager
//
//      All non-static cnr3_cache_manager_* functions MUST be thread-safe
//      and lock internally if/as appropriate, unless their name ends in _externally_locked.
//
//      A helper whose name ends in _externally_locked MUST be called only while
//      the caller already holds the cache manager's per-instance cache_mutex.
//
//      All cache-manager code must be designed around this policy:
//          VapourSynth mode: fmUnordered:
//              Safe, even though only one callback enters at a time.
//          VapourSynth mode: fmParallelRequests:
//              Safe with concurrent arInitial readers/pinners and serial
//              arAllFramesReady writer.
//          VapourSynth mode: fmParallel:
//              Direct cache-manager metadata access and cache-manager helpers
//              remain thread-safe under concurrent arInitial/arAllFramesReady
//              calls, provided all mutable cache-manager state is accessed only
//              while holding the per-instance cache_mutex.
//              Full fmParallel algorithm correctness still needs later
//              condition variables and active-computation state.
//
// EXTERNAL FRAME REFERENCE SAFETY:
//      The cache manager stores VSFrame pointers that are owned by VapourSynth
//      and protected by VapourSynth reference counting.
//
//      There are two distinct reference-ownership cases:
//
//      1. Cache-owned frame references:
//          Every VSFrame pointer stored in non_checkpoint_pool or checkpoint_pool
//          MUST be a cache-owned reference obtained with vsapi->addFrameRef().
//          This ensures that VapourSynth does not dispose of the frame while it
//          is still referenced by the pointer in our cache.
//
//          Every cache-owned frame reference MUST be released exactly once with
//          vsapi->freeFrame() when that cache slot is pruned, when the cache is
//          cleared, or when the owning CNR3 filter instance is destroyed.
//
//          To be clear:
//              cache_index does not own frame references.
//              It only aliases frame pointers owned by either
//              non_checkpoint_pool or checkpoint_pool.
//
//      2. Caller-owned temporary frame references:
//          If a public cache-manager helper returns a cached VSFrame pointer
//          after releasing cache.cache_mutex, it MUST return a caller-owned
//          temporary reference obtained with vsapi->addFrameRef().
//          This ensures that VapourSynth does not dispose of the frame while
//          the caller-owned temporary reference is still held by the caller.
//
//          The helper name MUST make this explicit, for example:
//              cnr3_cache_manager_find_output_frame_and_add_ref()
//
//          The caller MUST release that caller-owned temporary reference exactly
//          once with vsapi->freeFrame() on every success, error, and early-exit
//          path.
//
//      A helper that returns a raw borrowed cached VSFrame pointer without taking
//      addFrameRef() MUST have a name ending in _externally_locked.
//
//      A raw borrowed pointer returned by an _externally_locked helper is valid
//      only while the caller already holds cache.cache_mutex and only while the
//      relevant cache slot remains protected from change or pruning. It must not
//      be stored or used after the caller releases cache.cache_mutex unless the
//      caller first takes its own addFrameRef().
//
//      Special note on pin_count:
//          pin_count is not a VapourSynth frame reference.
//          It does not call addFrameRef() and it does not call freeFrame().
//          It only prevents a checkpoint_pool slot from being pruned while
//          an in-flight invocation depends on that checkpoint.
// 
// This file contains only the data structures and constants for the future
// v005 output-frame cache manager.
//
// Phase 1 intentionally does not change current runtime behaviour.
// The existing strict-streaming cache remains active until later phases wire
// these structures into cnr3_get_frame().
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// CNR3 cache manager tunable constants
//
// These are deliberately compile-time constants for the first implementation.
// Public VapourSynth parameters can be considered later after behaviour and
// memory use are measured.
// -----------------------------------------------------------------------------

constexpr int CNR3_OUTPUT_CACHE_CAPACITY = 100;
constexpr double CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1;

constexpr int CNR3_CHECKPOINT_INTERVAL = 10;
constexpr int CNR3_CHECKPOINT_MAX_RETAIN = 16;
constexpr int CNR3_CHECKPOINT_MIN_RETAIN = 6;

// -----------------------------------------------------------------------------
// CNR3 cache manager development diagnostics
//
// These switches are compile-time controls for extra validation and diagnostics
// intended for development and maintenance.
//
// Code guarded by if constexpr using these constants is compiled out of the
// generated executable code when the relevant constant is false.
//
// These diagnostics must not be required for correctness. Correctness must come
// from the cache-manager ownership, locking, pruning, and index/pool invariants.
// -----------------------------------------------------------------------------

constexpr bool CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS = true;

/*
    Defaults to CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS, but may be set independently
    if post-mutation validation needs to be enabled or disabled separately from
    other development diagnostics.
*/
constexpr bool CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION =
                      CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS;

// -----------------------------------------------------------------------------
// CNR3 cache manager statistics
//
// These counters are used to assess cache-manager behaviour and detect problems
// such as missed unpin paths, failed pin/unpin attempts, and cache thrashing.
//
// Statistics are mutable cache-manager state. Any helper that reads or writes
// these counters must follow the same cache_mutex locking rules as the cache
// pools and cache index.
// -----------------------------------------------------------------------------

struct Cnr3CacheManagerStats {
    int64_t checkpoint_pin_attempts = 0;
    int64_t checkpoint_pin_successes = 0;
    int64_t checkpoint_pin_failures = 0;

    int64_t checkpoint_find_and_pin_attempts = 0;
    int64_t checkpoint_find_and_pin_successes = 0;
    int64_t checkpoint_find_and_pin_failures = 0;
    int64_t checkpoint_find_and_pin_no_prior_checkpoint_failures = 0;
    int64_t checkpoint_find_and_pin_null_frame_failures = 0;

    int64_t checkpoint_unpin_attempts = 0;
    int64_t checkpoint_unpin_successes = 0;
    int64_t checkpoint_unpin_failures = 0;
    int64_t checkpoint_unpin_underflow_errors = 0;

    int64_t cache_integrity_errors = 0;
    int64_t checkpoint_null_frame_errors = 0;

    int64_t cache_clear_attempts = 0;
    int64_t cache_clear_successes = 0;
    int64_t cache_clear_failures = 0;
    int64_t cache_clear_null_vsapi_failures = 0;

    int64_t cache_validation_attempts = 0;
    int64_t cache_validation_successes = 0;
    int64_t cache_validation_failures = 0;
    int64_t cache_validation_cache_index_missing_pool_entry_errors = 0;
    int64_t cache_validation_pool_missing_cache_index_errors = 0;
    int64_t cache_validation_dual_pool_ownership_errors = 0;
    int64_t cache_validation_null_frame_errors = 0;
    int64_t cache_validation_negative_pin_count_errors = 0;
    int64_t cache_validation_highest_frame_number_errors = 0;

    int64_t cache_store_attempts = 0;
    int64_t cache_store_successes = 0;
    int64_t cache_store_failures = 0;
    int64_t cache_store_duplicate_rejections = 0;
    int64_t cache_store_invalid_input_errors = 0;
    int64_t cache_store_add_ref_failures = 0;
    int64_t cache_store_pool_inconsistency_errors = 0;
    int64_t cache_store_index_inconsistency_errors = 0;

    int64_t non_checkpoint_store_successes = 0;
    int64_t checkpoint_store_successes = 0;

    int64_t cache_remove_attempts = 0;
    int64_t cache_remove_successes = 0;
    int64_t cache_remove_failures = 0;
    int64_t cache_remove_not_found_failures = 0;
    int64_t cache_remove_invalid_input_errors = 0;
    int64_t cache_remove_pinned_checkpoint_rejections = 0;
    int64_t cache_remove_pool_inconsistency_errors = 0;
    int64_t cache_remove_index_inconsistency_errors = 0;

    int64_t non_checkpoint_remove_successes = 0;
    int64_t checkpoint_remove_successes = 0;

    int64_t non_checkpoint_prune_attempts = 0;
    int64_t non_checkpoint_prune_runs = 0;
    int64_t non_checkpoint_prune_skipped_below_overflow = 0;
    int64_t non_checkpoint_prune_removed_frames = 0;
    int64_t non_checkpoint_prune_remove_failures = 0;

    int64_t checkpoint_prune_attempts = 0;
    int64_t checkpoint_prune_runs = 0;
    int64_t checkpoint_prune_skipped_below_max_retain = 0;
    int64_t checkpoint_prune_removed_frames = 0;
    int64_t checkpoint_prune_remove_failures = 0;
    int64_t checkpoint_prune_skipped_frame_zero = 0;
    int64_t checkpoint_prune_skipped_pinned = 0;
    int64_t checkpoint_prune_no_eligible_frames = 0;

    int64_t prune_after_store_attempts = 0;
    int64_t prune_after_store_successes = 0;
    int64_t prune_after_store_failures = 0;
    int64_t prune_after_store_non_checkpoint_failures = 0;
    int64_t prune_after_store_checkpoint_failures = 0;
};

// -----------------------------------------------------------------------------
// CNR3 checkpoint slot
//
// A checkpoint slot owns one retained VS frame reference.
//
// frame:
//     Pointer returned by vsapi->addFrameRef().
//     The pointer must be released with vsapi->freeFrame() when the slot is
//     evicted or the owning CNR3 instance is destroyed.
//
// pin_count:
//     Number of in-flight frame invocations that selected this checkpoint in
//     arInitial and have not yet completed their matching arAllFramesReady
//     cleanup path.
//
//     A checkpoint with pin_count > 0 must not be pruned.
// -----------------------------------------------------------------------------

struct Cnr3CheckpointSlot {
    const VSFrame* frame = nullptr;
    int pin_count = 0;
};

// -----------------------------------------------------------------------------
// CNR3 cache manager
//
// This structure is intended to be owned by Cnr3Data, making the cache strictly
// per-instance. It must never be global or static.
//
// non_checkpoint_pool:
//     Ordered by frame number.
//     Holds normal cached output frames.
//
// checkpoint_pool:
//     Ordered by frame number.
//     Holds checkpoint output frames and their pin counts.
//
// cache_index:
//     Fast lookup across both pools.
//     The frame pointers in this index are non-owning aliases of the owning
//     pointers stored in non_checkpoint_pool or checkpoint_pool.
//
// cache_mutex:
//     Per-instance mutex. Under fmParallelRequests this protects concurrent
//     arInitial cache reads and checkpoint pinning from the single serialised
//     arAllFramesReady writer/pruner.
//
// highest_cached_frame_number:
//     Highest frame number currently present in either pool.
//     -1 means the cache is empty.
// -----------------------------------------------------------------------------

struct Cnr3CacheManagerV005 {
    std::map<int, const VSFrame*> non_checkpoint_pool;
    std::map<int, Cnr3CheckpointSlot> checkpoint_pool;
    std::unordered_map<int, const VSFrame*> cache_index;

    std::mutex cache_mutex;

    int highest_cached_frame_number = -1;

    Cnr3CacheManagerStats stats;
};

// -----------------------------------------------------------------------------
// CNR3 cache manager helper functions - Phase 2A
//
// These helpers are intentionally limited to safe cache state inspection and
// teardown/release support.
//
// They do not change current CNR3 runtime behaviour until the v005 cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

int cnr3_cache_manager_get_non_checkpoint_overflow_limit();

bool cnr3_cache_manager_is_empty(
    Cnr3CacheManagerV005& cache
);

std::size_t cnr3_cache_manager_get_non_checkpoint_count(
    Cnr3CacheManagerV005& cache
);

std::size_t cnr3_cache_manager_get_checkpoint_count(
    Cnr3CacheManagerV005& cache
);

std::size_t cnr3_cache_manager_get_total_cached_frame_count(
    Cnr3CacheManagerV005& cache
);

bool cnr3_cache_manager_contains_output_frame(
    Cnr3CacheManagerV005& cache,
    int frame_number
);

bool cnr3_cache_manager_clear(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 cache manager lookup helpers - Phase 2B
//
// These helpers perform frame-number ordered lookups only.
//
// They do not change current CNR3 runtime behaviour until the v005 cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

/*
    Find the nearest prior checkpoint for requested_frame_number.

    Critical v005 cache-manager ordering rule:
        "nearest prior checkpoint" means the checkpoint with the highest
        frame number that is strictly less than requested_frame_number.

    It does not mean most recently inserted, most recently used, most recently
    written, or nearest by any container/insertion/cache-recency order.

    This public helper returns only the checkpoint frame number. It deliberately
    does not return a raw cached VSFrame pointer.

    Thread safety:
        Locks cache.cache_mutex internally.
        Caller must not already hold cache.cache_mutex.
*/
bool cnr3_cache_manager_find_nearest_prior_checkpoint(
    Cnr3CacheManagerV005& cache,
    int requested_frame_number,
    int& checkpoint_frame_number
);

bool cnr3_cache_manager_should_promote_checkpoint(
    int frame_number
);

// -----------------------------------------------------------------------------
// CNR3 cache manager statistics helpers - Phase 2C.1
//
// These helpers manage the v005 cache-manager statistics counters.
//
// They do not change current CNR3 runtime behaviour until the v005 cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

void cnr3_cache_manager_reset_stats(
    Cnr3CacheManagerV005& cache
);

Cnr3CacheManagerStats cnr3_cache_manager_get_stats_snapshot(
    Cnr3CacheManagerV005& cache
);

// -----------------------------------------------------------------------------
// CNR3 cache manager validation helpers - Phase 2G.1
//
// These helpers validate internal cache-manager invariants.
//
// They are intended for development, maintenance, debug diagnostics, and future
// runtime sanity checks.
//
// They do not change current CNR3 runtime behaviour until the v005 cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_validate_invariants(
    Cnr3CacheManagerV005& cache
);

// -----------------------------------------------------------------------------
// CNR3 cache manager checkpoint pin helpers - Phase 2C.2
//
// These helpers manage checkpoint pin_count values.
//
// pin_count protects a checkpoint_pool slot from pruning while an in-flight
// invocation depends on that checkpoint.
//
// pin_count is not a VapourSynth frame reference. It does not call addFrameRef()
// and it does not call freeFrame().
// 
// Phase 2H.1 adds atomic find-and-pin support so future runtime code does not
// perform an unsafe find/unlock/pin sequence.
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_find_and_pin_nearest_prior_checkpoint(
    Cnr3CacheManagerV005& cache,
    int requested_frame_number,
    int& checkpoint_frame_number
);

bool cnr3_cache_manager_pin_checkpoint(
    Cnr3CacheManagerV005& cache,
    int checkpoint_frame_number
);

bool cnr3_cache_manager_unpin_checkpoint(
    Cnr3CacheManagerV005& cache,
    int checkpoint_frame_number
);

bool cnr3_cache_manager_has_pinned_checkpoints(
    Cnr3CacheManagerV005& cache
);

int64_t cnr3_cache_manager_get_total_pin_count(
    Cnr3CacheManagerV005& cache
);

// -----------------------------------------------------------------------------
// CNR3 cache manager store helpers - Phase 2D.1
//
// These helpers insert output frames into the v005 cache manager.
//
// Store helpers take cache-owned VSFrame references with vsapi->addFrameRef().
// Those references must later be released exactly once by pruning, clearing, or
// teardown using vsapi->freeFrame().
//
// No pruning is performed in Phase 2D.1.
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_store_output_frame(
    Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSFrame* output_frame,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 cache manager remove helpers - Phase 2D.2
//
// These helpers remove output frames from the v005 cache manager.
//
// Remove helpers release cache-owned VSFrame references with vsapi->freeFrame().
// They must remove the non-owning cache_index alias and exactly one owning pool
// entry.
//
// Pinned checkpoints must not be removed.
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_remove_output_frame(
    Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 cache manager non-checkpoint pruning helpers - Phase 2E.1b
//
// These helpers prune only non_checkpoint_pool.
//
// Option B policy:
//     Let non_checkpoint_pool grow up to the overflow limit.
//     If it exceeds the overflow limit, prune oldest non-checkpoint frames first
//     until non_checkpoint_pool.size() is back to CNR3_OUTPUT_CACHE_CAPACITY.
//
// Checkpoints are not pruned by these helpers.
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_prune_non_checkpoint_pool(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 cache manager checkpoint pruning helpers - Phase 2E.2
//
// These helpers prune only checkpoint_pool.
//
// Checkpoint pruning policy:
//     If checkpoint_pool.size() exceeds CNR3_CHECKPOINT_MAX_RETAIN, prune the
//     oldest eligible checkpoints first until checkpoint_pool.size() is back to
//     CNR3_CHECKPOINT_MIN_RETAIN.
//
//     Frame 0 is never pruned.
//     Checkpoints with pin_count > 0 are never pruned.
//
// Actual frame removal is delegated to the shared remove helper so that
// cache_index erasure, owning-pool erasure, and freeFrame() release remain
// centralised.
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_prune_checkpoint_pool(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 cache manager combined pruning helpers - Phase 2E.3b
//
// These helpers run the standard post-store pruning pass.
//
// The combined prune-after-store helper exists so later runtime code can call
// one public helper after storing output frames, while the cache manager keeps
// the detailed pruning sequence and locking policy internal.
// -----------------------------------------------------------------------------

bool cnr3_cache_manager_prune_after_store(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// END CNR3 cache manager - v005 design structures
// -----------------------------------------------------------------------------