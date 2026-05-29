#pragma once

#include <cstddef>
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
    const Cnr3CacheManagerV005& cache
);

std::size_t cnr3_cache_manager_get_non_checkpoint_count(
    const Cnr3CacheManagerV005& cache
);

std::size_t cnr3_cache_manager_get_checkpoint_count(
    const Cnr3CacheManagerV005& cache
);

std::size_t cnr3_cache_manager_get_total_cached_frame_count(
    const Cnr3CacheManagerV005& cache
);

bool cnr3_cache_manager_contains_output_frame(
    const Cnr3CacheManagerV005& cache,
    int frame_number
);

void cnr3_cache_manager_clear(
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

bool cnr3_cache_manager_find_output_frame(
    const Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSFrame*& output_frame
);

/*
    Find the nearest prior checkpoint for requested_frame_number.

    CRITICAL v005 cache-manager ordering rule:
        "nearest prior checkpoint" means the checkpoint with the highest
        frame number that is strictly less than requested_frame_number.

    It does not mean most recently inserted, most recently used, most recently
    written, or nearest by any container/insertion/cache-recency order.

    Returns true if such a checkpoint exists.

    On success:
        checkpoint_frame_number receives the checkpoint frame number.
        checkpoint_frame receives the cached checkpoint frame pointer.

    On failure:
        checkpoint_frame_number is set to -1.
        checkpoint_frame is set to nullptr.
*/
bool cnr3_cache_manager_find_nearest_prior_checkpoint(
    const Cnr3CacheManagerV005& cache,
    int requested_frame_number,
    int& checkpoint_frame_number,
    const VSFrame*& checkpoint_frame
);

bool cnr3_cache_manager_should_promote_checkpoint(
    int frame_number
);

// -----------------------------------------------------------------------------
// END CNR3 cache manager - v005 design structures
// -----------------------------------------------------------------------------