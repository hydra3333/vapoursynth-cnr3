#include "cnr3_cache_manager.h"

// -----------------------------------------------------------------------------
// CNR3 cache manager - v005
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
// Phase 2A adds only safe cache state inspection and teardown/release helpers.
//
// These helpers are not wired into current runtime behaviour yet.
// The existing strict-streaming cache remains active.
// -----------------------------------------------------------------------------

int cnr3_cache_manager_get_non_checkpoint_overflow_limit() {
    /*
        Thread safety:
            Does not lock. Reads only compile-time constants and does not read
            or write mutable cache-manager state.
        Caller requirement:
            None.
    */

    /*
        The v005 design has a nominal non-checkpoint capacity plus a small
        overflow allowance. The overflow allowance gives the cache room to
        absorb bursts during out-of-order request patterns before pruning.

        With the current constants:
            100 * 1.1 = 110
    */
    return static_cast<int>(
        static_cast<double>(CNR3_OUTPUT_CACHE_CAPACITY) *
        CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR
        );
}

bool cnr3_cache_manager_is_empty(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager state.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return (
        cache.non_checkpoint_pool.empty() &&
        cache.checkpoint_pool.empty() &&
        cache.cache_index.empty()
        );
}

std::size_t cnr3_cache_manager_get_non_checkpoint_count(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager state:
            cache.non_checkpoint_pool.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return cache.non_checkpoint_pool.size();
}

std::size_t cnr3_cache_manager_get_checkpoint_count(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager state:
            cache.checkpoint_pool.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return cache.checkpoint_pool.size();
}

std::size_t cnr3_cache_manager_get_total_cached_frame_count(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager state:
            cache.non_checkpoint_pool and cache.checkpoint_pool.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return (
        cache.non_checkpoint_pool.size() +
        cache.checkpoint_pool.size()
        );
}

bool cnr3_cache_manager_contains_output_frame(
    Cnr3CacheManagerV005& cache,
    int frame_number
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager state:
            cache.cache_index.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return (cache.cache_index.find(frame_number) != cache.cache_index.end());
}

void cnr3_cache_manager_clear(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Writes mutable cache-manager state
            and releases all cache-owned VS frame references.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    /*
        Release every VS frame reference owned by the v005 cache manager.

        Important ownership rule:
            Frames stored in non_checkpoint_pool and checkpoint_pool are owned
            references previously retained with addFrameRef().

        cache_index contains non-owning aliases only. Those pointers must not be
        released separately.
    */

    if (vsapi != nullptr) {
        for (const auto& entry : cache.non_checkpoint_pool) {
            const VSFrame* frame = entry.second;

            if (frame != nullptr) {
                vsapi->freeFrame(frame);
            }
        }

        for (const auto& entry : cache.checkpoint_pool) {
            const Cnr3CheckpointSlot& slot = entry.second;

            if (slot.frame != nullptr) {
                vsapi->freeFrame(slot.frame);
            }
        }
    }

    cache.non_checkpoint_pool.clear();
    cache.checkpoint_pool.clear();
    cache.cache_index.clear();

    cache.highest_cached_frame_number = -1;
}

bool cnr3_cache_manager_find_nearest_prior_checkpoint(
    Cnr3CacheManagerV005& cache,
    int requested_frame_number,
    int& checkpoint_frame_number
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager state:
            cache.checkpoint_pool.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    /*
        Find the nearest prior checkpoint for requested_frame_number.

        Critical v005 cache-manager ordering rule:
            "nearest prior checkpoint" means the checkpoint with the highest
            frame number that is strictly less than requested_frame_number.

        It does not mean most recently inserted, most recently used, most
        recently written, or nearest by any container/insertion/cache-recency
        order.

        std::map is ordered by frame number. lower_bound(requested_frame_number)
        finds the first checkpoint whose frame number is not less than the
        requested frame. Stepping one entry backward therefore gives the
        highest checkpoint frame number strictly less than requested_frame_number.

        This public helper returns the checkpoint frame number only. It does not
        return a raw checkpoint frame pointer, because returning a non-retained
        cached pointer after unlocking would be unsafe.
    */

    checkpoint_frame_number = -1;

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    if (cache.checkpoint_pool.empty()) {
        return false;
    }

    auto found = cache.checkpoint_pool.lower_bound(requested_frame_number);

    if (found == cache.checkpoint_pool.begin()) {
        return false;
    }

    --found;

    if (found->second.frame == nullptr) {
        return false;
    }

    checkpoint_frame_number = found->first;

    return true;
}

bool cnr3_cache_manager_should_promote_checkpoint(
    int frame_number
) {
    /*
        Thread safety:
            Does not lock. Reads only function parameters and compile-time
            constants. Does not read or write mutable cache-manager state.
        Caller requirement:
            None.
    */

    /*
        v005 checkpoint promotion rule.

        Frame 0 is always a checkpoint.

        Every frame number divisible by CNR3_CHECKPOINT_INTERVAL is also
        promoted to checkpoint when written.
    */

    if (frame_number < 0) {
        return false;
    }

    if (frame_number == 0) {
        return true;
    }

    return ((frame_number % CNR3_CHECKPOINT_INTERVAL) == 0);
}

void cnr3_cache_manager_reset_stats(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Writes mutable cache-manager
            state: cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    cache.stats = Cnr3CacheManagerStats{};
}

Cnr3CacheManagerStats cnr3_cache_manager_get_stats_snapshot(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager
            state: cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return cache.stats;
}