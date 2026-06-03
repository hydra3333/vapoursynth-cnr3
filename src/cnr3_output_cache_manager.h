#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <unordered_map>

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "cnr3_build_config.h"

// -----------------------------------------------------------------------------
// CNR3 output cache manager - CMS05 design structures
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
//          - cnr3_output_cache_* helper functions
//          - cnr3_get_frame() code that directly touches cache-manager state
//          - pruning code
//          - checkpoint promotion code
//          - pin_count handling
//          - cache index updates
//          - future debug/statistics counters stored inside the cache manager
//
//      All non-static cnr3_output_cache_* functions MUST be thread-safe
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
//              cnr3_output_cache_find_frame_and_add_ref()
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
// CMS06 output-frame cache manager.
//
// Store/prune proving is live: output_cache stores and prunes real produced
// frames. CMS02-F direct cache-hit reuse is wired through caller-owned lookup
// references.
//
// The old strict-streaming path remains the cache-miss and newly-computed-frame
// path for now. Recovery walks and bounded warm-up recovery are not yet wired.
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// CNR3 output cache manager tunable constants
//
// These are deliberately compile-time constants for the first implementation.
// Public VapourSynth parameters can be considered later after behaviour and
// memory use are measured.
// -----------------------------------------------------------------------------

// --- Soft pruning targets ---

static constexpr int CNR3_OUTPUT_CACHE_CAPACITY = 100;
static constexpr double CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1;

// --- Hard ceiling (byte-budget based) ---

/*
    Nominal output-cache byte budget used to derive active_ceiling.

    The runtime cache limit remains a simple frame count. This byte budget is
    used only at filter creation to choose a format-aware frame-count ceiling.

    The 1 GiB value is intentional: CNR3's bounded recursive recovery needs
    enough cached frame history to remain useful under VapourSynth request
    jitter and modest seeks.
*/
static constexpr int64_t CNR3_CACHE_BYTE_BUDGET = 1024LL * 1024LL * 1024LL;
static constexpr int CNR3_CACHE_MIN_HARD_CEILING = 150;
static constexpr int CNR3_CACHE_MAX_HARD_CEILING = 1000;

// --- Checkpoints ---

static constexpr int CNR3_CHECKPOINT_INTERVAL = 10;
static constexpr int CNR3_CHECKPOINT_MAX_RETAIN = 32;
static constexpr int CNR3_CHECKPOINT_MIN_RETAIN = 10;

// --- Hot zones ---

static constexpr int CNR3_HOT_ZONE_FORWARD_RADIUS = 10;
static constexpr int CNR3_HOT_ZONE_BACK_RADIUS = 50;
static constexpr int CNR3_MAX_HOT_ZONES = 5;
static constexpr int CNR3_HOT_ZONE_JUMP_THRESHOLD =
CNR3_HOT_ZONE_FORWARD_RADIUS + CNR3_HOT_ZONE_BACK_RADIUS + 1;

// -----------------------------------------------------------------------------
// CNR3 output cache manager development diagnostics
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

/*
    Defaults to CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS, but may be set independently
    if post-mutation validation needs to be enabled or disabled separately from
    other output-cache development diagnostics.
*/
constexpr bool CNR3_OUTPUT_CACHE_VALIDATE_AFTER_MUTATION =
CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS;

// -----------------------------------------------------------------------------
// CNR3 output cache manager statistics
//
// These counters are used to assess cache-manager behaviour and detect problems
// such as missed unpin paths, failed pin/unpin attempts, and cache thrashing.
//
// Statistics are mutable cache-manager state. Any helper that reads or writes
// these counters must follow the same cache_mutex locking rules as the cache
// pools and cache index.
//
// Future diagnostic note:
//     A separate verbose/full stats dump may be added later. The current
//     vapoursynth-Cnr3.cpp summary intentionally prints only selected headline
//     counters plus post-validation failures.
// -----------------------------------------------------------------------------

struct Cnr3OutputCacheStats {
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

    /*
        Number of checkpoint pins currently held by callers.

        This is updated with pin_count mutations and must match the sum of all
        checkpoint_pool slot pin_count values. It makes missing unpin cleanup
        visible even after cache clear removes the checkpoint slots.
    */
    int64_t checkpoint_active_pin_total = 0;
    int64_t checkpoint_pin_balance_errors = 0;

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
    int64_t cache_validation_ref_balance_errors = 0;

    int64_t cache_store_attempts = 0;
    int64_t cache_store_successes = 0;
    int64_t cache_store_failures = 0;
    int64_t cache_store_duplicate_rejections = 0;
    int64_t cache_store_invalid_input_errors = 0;
    int64_t cache_store_add_ref_failures = 0;
    int64_t cache_store_pool_inconsistency_errors = 0;
    int64_t cache_store_index_inconsistency_errors = 0;
    int64_t cache_store_post_validation_failures = 0;

    int64_t cache_ceiling_hard_aborts = 0;

    // CMS05 duplicate-store diagnostics.
    int64_t store_skipped_already_cached = 0;
    int64_t duplicate_store_computed_but_discarded = 0;

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
    int64_t cache_remove_post_validation_failures = 0;

    int64_t non_checkpoint_remove_successes = 0;
    int64_t checkpoint_remove_successes = 0;

    int64_t cache_addframeref_total = 0;
    int64_t cache_freeframe_total = 0;

    // CMS02-F cache-hit lookup diagnostics.
    int64_t cache_hits_at_arAllFramesReady = 0;
    int64_t cache_misses = 0;
    int64_t lookup_owned_ref_acquired_total = 0;
    int64_t lookup_owned_ref_released_total = 0;
    int64_t lookup_owned_ref_transferred_total = 0;
    int64_t cache_lookup_attempts = 0;
    int64_t cache_lookup_failures = 0;
    int64_t cache_lookup_invalid_input_errors = 0;
    int64_t cache_lookup_pool_inconsistency_errors = 0;
    int64_t cache_lookup_index_inconsistency_errors = 0;
    int64_t cache_lookup_null_frame_errors = 0;

    int64_t non_checkpoint_prune_attempts = 0;
    int64_t non_checkpoint_prune_runs = 0;
    int64_t non_checkpoint_prune_skipped_below_overflow = 0;
    int64_t non_checkpoint_prune_removed_frames = 0;
    int64_t non_checkpoint_prune_remove_failures = 0;
    int64_t non_checkpoint_prune_post_validation_failures = 0;

    int64_t non_checkpoint_prune_skipped_in_hot_zone = 0;
    int64_t checkpoint_prune_skipped_in_hot_zone = 0;
    int64_t prune_no_candidate_exists = 0;

    int64_t checkpoint_prune_attempts = 0;
    int64_t checkpoint_prune_runs = 0;
    int64_t checkpoint_prune_skipped_below_max_retain = 0;
    int64_t checkpoint_prune_removed_frames = 0;
    int64_t checkpoint_prune_remove_failures = 0;
    int64_t checkpoint_prune_skipped_frame_zero = 0;
    int64_t checkpoint_prune_skipped_pinned = 0;
    int64_t checkpoint_prune_no_eligible_frames = 0;
    int64_t checkpoint_prune_post_validation_failures = 0;

    int64_t prune_after_store_attempts = 0;
    int64_t prune_after_store_successes = 0;
    int64_t prune_after_store_failures = 0;
    int64_t prune_after_store_non_checkpoint_failures = 0;
    int64_t prune_after_store_checkpoint_failures = 0;
    int64_t prune_after_store_post_validation_failures = 0;

    // CMS05 hot-zone diagnostics.
    int64_t hot_zone_allocations = 0;
    int64_t hot_zone_slides = 0;
    int64_t hot_zone_hits = 0;
    int64_t hot_zone_merges = 0;
    int64_t hot_zone_retirements = 0;
    int64_t hot_zone_new_zone_requests = 0;
    int64_t hot_zone_max_active_observed = 0;
    int64_t hot_zone_updates_at_arInitial = 0;

    // Last hot-zone update, for one-line debug trace output.
    int hot_zone_last_event_kind = 0;
    int hot_zone_last_event_frame = -1;
    int hot_zone_last_event_zone_index = -1;
    int hot_zone_last_event_old_low = -1;
    int hot_zone_last_event_old_high = -1;
    int hot_zone_last_event_new_low = -1;
    int hot_zone_last_event_new_high = -1;
    int hot_zone_last_event_active_count = 0;
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
// CNR3 output cache hot zone
//
// CMS05 hot zones are sliding frame-number ranges used to protect likely-active
// output-cache regions from pruning.
// -----------------------------------------------------------------------------

struct Cnr3HotZone {
    bool active = false;
    int low = -1;
    int high = -1;
    int last_observed_frame = -1;

    int64_t hit_count = 0;
    int64_t slide_count = 0;
    int64_t merge_count = 0;
    int64_t retirement_count = 0;
    int64_t prune_protection_count = 0;
};

// -----------------------------------------------------------------------------
// CNR3 output cache scheduling mode
//
// Used only where cache-manager behaviour must distinguish the current
// VapourSynth scheduling policy. CMS05 uses the same sliding hot-zone update
// in both modes; retirement is mode-specific.
// -----------------------------------------------------------------------------

enum class Cnr3CacheSchedulingMode {
    FmUnordered,
    FmParallelRequests
};

// -----------------------------------------------------------------------------
// CNR3 output cache manager
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

struct Cnr3OutputCacheManager {
    std::map<int, const VSFrame*> non_checkpoint_pool;
    std::map<int, Cnr3CheckpointSlot> checkpoint_pool;
    std::unordered_map<int, const VSFrame*> cache_index;

    std::mutex cache_mutex;

    int highest_cached_frame_number = -1;

    int active_ceiling = CNR3_CACHE_MIN_HARD_CEILING;

    std::array<Cnr3HotZone, CNR3_MAX_HOT_ZONES> hot_zones{};

    Cnr3OutputCacheStats stats;
};

// -----------------------------------------------------------------------------
// CNR3 output cache manager debug snapshot
//
// This structure is a passive diagnostic snapshot of CMS05 output-cache state.
//
// It is intended for debug/status output. It does not own frame references and
// must not store VSFrame pointers.
//
// A debug snapshot should be collected by one cache-manager helper while holding
// cache.cache_mutex once, so all fields describe one coherent point-in-time view.
// -----------------------------------------------------------------------------

struct Cnr3OutputCacheDebugSnapshot {
    std::size_t non_checkpoint_count = 0;
    std::size_t checkpoint_count = 0;
    std::size_t total_cached_frame_count = 0;

    bool has_pinned_checkpoints = false;
    int64_t total_pin_count = 0;

    int highest_cached_frame_number = -1;
    int active_ceiling = CNR3_CACHE_MIN_HARD_CEILING;

    /*
        Passive invariant result.

        This is calculated without incrementing validation counters. It is meant
        for diagnostic summary output, not for recording validation activity.
    */
    bool invariants_ok = false;

    Cnr3OutputCacheStats stats;
};

// -----------------------------------------------------------------------------
// CNR3 output cache manager helper functions - Phase 2A
//
// These helpers are intentionally limited to safe cache state inspection and
// teardown/release support.
//
// They do not change current CNR3 runtime behaviour until the CMS05 output cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

int cnr3_output_cache_get_non_checkpoint_overflow_limit();

bool cnr3_output_cache_is_empty(
    Cnr3OutputCacheManager& cache
);

std::size_t cnr3_output_cache_get_non_checkpoint_count(
    Cnr3OutputCacheManager& cache
);

std::size_t cnr3_output_cache_get_checkpoint_count(
    Cnr3OutputCacheManager& cache
);

std::size_t cnr3_output_cache_get_total_cached_frame_count(
    Cnr3OutputCacheManager& cache
);

bool cnr3_output_cache_contains_frame(
    Cnr3OutputCacheManager& cache,
    int frame_number
);

const VSFrame* cnr3_output_cache_find_frame_and_add_ref(
    Cnr3OutputCacheManager& cache,
    int frame_number,
    const VSAPI* vsapi
);

void cnr3_output_cache_note_lookup_ref_released(
    Cnr3OutputCacheManager& cache
);

void cnr3_output_cache_note_lookup_ref_transferred(
    Cnr3OutputCacheManager& cache
);

bool cnr3_output_cache_clear(
    Cnr3OutputCacheManager& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 output cache active-ceiling helpers - CMS05-2E
//
// The runtime hard ceiling is a simple frame-count limit. This helper derives
// that frame-count limit from clip geometry, bit depth, and the nominal byte
// budget.
//
// Thread safety:
//     Locks cache.cache_mutex internally.
// -----------------------------------------------------------------------------

void cnr3_output_cache_set_ceiling(
    Cnr3OutputCacheManager& cache,
    const VSVideoInfo* vi
);

// -----------------------------------------------------------------------------
// CNR3 output cache hot-zone helpers - CMS05-2A
// -----------------------------------------------------------------------------

bool cnr3_output_cache_is_frame_in_hot_zone_externally_locked(
    const Cnr3OutputCacheManager& cache,
    int frame_number
);

void cnr3_output_cache_update_hot_zones(
    Cnr3OutputCacheManager& cache,
    int frame_number
);

void cnr3_output_cache_retire_cold_hot_zones_externally_locked(
    Cnr3OutputCacheManager& cache,
    Cnr3CacheSchedulingMode mode
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager lookup helpers - Phase 2B
//
// These helpers perform frame-number ordered lookups only.
//
// They do not change current CNR3 runtime behaviour until the CMS05 output cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

/*
    Find the nearest prior checkpoint for requested_frame_number.

    Critical CNR3 output cache manager ordering rule:
        "nearest prior checkpoint" means the checkpoint with the highest
        frame number that is strictly less than requested_frame_number.

    It does not mean most recently inserted, most recently used, most recently
    written, or nearest by any container/insertion/cache-recency order.

    This public helper returns only the checkpoint frame number. It deliberately
    does not return a raw cached VSFrame pointer.

    The selected checkpoint is the greatest checkpoint frame number less than or
    equal to requested_frame_number.

    Thread safety:
        Locks cache.cache_mutex internally.
        Caller must not already hold cache.cache_mutex.
*/
bool cnr3_output_cache_find_nearest_checkpoint_at_or_before(
    Cnr3OutputCacheManager& cache,
    int requested_frame_number,
    int& checkpoint_frame_number
);

bool cnr3_output_cache_should_promote_checkpoint(
    int frame_number
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager statistics helpers - Phase 2C.1
//
// These helpers manage the CNR3 output cache manager statistics counters.
//
// reset_stats() resets the resettable diagnostic counters only. It does not
// clear cached output frames, checkpoint slots, cache indexes, pin counts, or
// frame references.
//
// Integrity/error counters are part of the resettable statistics block. A later
// lifetime/non-resettable error counter set can be added if runtime testing
// shows that persistent integrity history is useful.
//
// These helpers do not change current CNR3 runtime behaviour until the
// CNR3 output cache manager is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

void cnr3_output_cache_reset_stats(
    Cnr3OutputCacheManager& cache
);

Cnr3OutputCacheStats cnr3_output_cache_get_stats_snapshot(
    Cnr3OutputCacheManager& cache
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager debug snapshot helpers - Phase 3C.1
//
// These helpers collect passive diagnostic snapshots of CMS05 output-cache state.
//
// The debug snapshot helper locks cache.cache_mutex once and copies all summary
// fields from one coherent point-in-time view.
//
// Unlike cnr3_output_cache_validate_invariants(), this helper's invariant check
// is passive and does not increment validation counters.
//
// This helper is intended for summary/status output. It is not a substitute for
// the mutating development validation helper used after cache mutations.
// -----------------------------------------------------------------------------

bool cnr3_output_cache_get_debug_snapshot(
    Cnr3OutputCacheManager& cache,
    Cnr3OutputCacheDebugSnapshot& snapshot
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager validation helpers - Phase 2G.1
//
// These helpers validate internal cache-manager invariants.
//
// They are intended for development, maintenance, debug diagnostics, and future
// runtime sanity checks.
//
// They do not change current CNR3 runtime behaviour until the CMS05 output cache manager
// is later wired into Cnr3Data and cnr3_get_frame().
// -----------------------------------------------------------------------------

bool cnr3_output_cache_validate_invariants(
    Cnr3OutputCacheManager& cache
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager checkpoint pin helpers - Phase 2C.2
//
// These helpers manage checkpoint pin_count values.
//
// pin_count protects a checkpoint_pool slot from pruning while an in-flight
// invocation depends on that checkpoint.
//
// pin_count is not a VapourSynth frame reference. It does not call addFrameRef()
// and it does not call freeFrame().
// 
// CMS02-G.1 adds atomic find-and-pin support so future recovery code does not
// perform an unsafe find/unlock/pin sequence.
//
// The selected checkpoint is the greatest checkpoint frame number less than or
// equal to requested_frame_number. On success, checkpoint_frame_number receives
// that frame number and the checkpoint remains pinned until the caller unpins it.
// -----------------------------------------------------------------------------

bool cnr3_output_cache_find_and_pin_nearest_checkpoint_at_or_before(
    Cnr3OutputCacheManager& cache,
    int requested_frame_number,
    int& checkpoint_frame_number
);

bool cnr3_output_cache_pin_checkpoint(
    Cnr3OutputCacheManager& cache,
    int checkpoint_frame_number
);

bool cnr3_output_cache_unpin_checkpoint(
    Cnr3OutputCacheManager& cache,
    int checkpoint_frame_number
);

bool cnr3_output_cache_has_pinned_checkpoints(
    Cnr3OutputCacheManager& cache
);

int64_t cnr3_output_cache_get_total_pin_count(
    Cnr3OutputCacheManager& cache
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager store helpers - Phase 2D.1
//
// These helpers insert output frames into the CMS05 output cache manager.
//
// Store helpers take cache-owned VSFrame references with vsapi->addFrameRef().
// Those references must later be released exactly once by pruning, clearing, or
// teardown using vsapi->freeFrame().
//
// No pruning is performed in Phase 2D.1.
// -----------------------------------------------------------------------------

bool cnr3_output_cache_store_frame(
    Cnr3OutputCacheManager& cache,
    int frame_number,
    const VSFrame* output_frame,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 output cache remove helpers - Phase 2D.2
//
// These helpers remove output frames from the CMS05 output cache manager.
//
// Remove helpers release cache-owned VSFrame references with vsapi->freeFrame().
// They must remove the non-owning cache_index alias and exactly one owning pool
// entry.
//
// Pinned checkpoints must not be removed.
// -----------------------------------------------------------------------------

bool cnr3_output_cache_remove_frame(
    Cnr3OutputCacheManager& cache,
    int frame_number,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager non-checkpoint pruning helpers - Phase 2E.1b
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

bool cnr3_output_cache_prune_non_checkpoint_pool(
    Cnr3OutputCacheManager& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager checkpoint pruning helpers - Phase 2E.2
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
// Skip-counter semantics:
//     checkpoint_prune_skipped_frame_zero and
//     checkpoint_prune_skipped_pinned are skip observations, not unique
//     checkpoint counts. Because checkpoint pruning may restart from begin()
//     after each successful removal, the same surviving checkpoint may be
//     observed and counted more than once across a prune run.
//
//     checkpoint_prune_no_eligible_frames means no eligible removable checkpoint
//     was found in that pass. It should not be used for remove-failure cases;
//     those are represented by checkpoint_prune_remove_failures.
//
// Actual frame removal is delegated to the shared remove helper so that
// cache_index erasure, owning-pool erasure, and freeFrame() release remain
// centralised.
// -----------------------------------------------------------------------------

bool cnr3_output_cache_prune_checkpoint_pool(
    Cnr3OutputCacheManager& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// CNR3 output cache manager combined pruning helpers - Phase 2E.3b
//
// These helpers run the standard post-store pruning pass.
//
// The combined prune-after-store helper exists so later runtime code can call
// one public helper after storing output frames, while the cache manager keeps
// the detailed pruning sequence and locking policy internal.
// -----------------------------------------------------------------------------

bool cnr3_output_cache_prune_after_store(
    Cnr3OutputCacheManager& cache,
    const VSAPI* vsapi
);

// -----------------------------------------------------------------------------
// END CNR3 output cache manager - CMS05 design structures
// -----------------------------------------------------------------------------
