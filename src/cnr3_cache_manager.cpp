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
            cache.checkpoint_pool. Writes diagnostic statistics if corrupt
            checkpoint state is detected.
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

        This implementation scans checkpoint_pool in reverse frame-number order.
        checkpoint_pool is std::map<int, Cnr3CheckpointSlot>, so reverse
        iteration visits the highest frame numbers first.

        The first checkpoint whose frame number is strictly less than
        requested_frame_number is therefore the nearest prior checkpoint.

        This is intentionally chosen for maintainability and clarity. The
        checkpoint pool is small, so the cost of reverse scanning is negligible.

        This public helper returns the checkpoint frame number only. It does not
        return a raw checkpoint frame pointer, because returning a non-retained
        cached pointer after unlocking would be unsafe.
    */

    checkpoint_frame_number = -1;

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    for (
        auto found = cache.checkpoint_pool.rbegin();
        found != cache.checkpoint_pool.rend();
        ++found
        ) {
        if (found->first >= requested_frame_number) {
            continue;
        }

        if (found->second.frame == nullptr) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.checkpoint_null_frame_errors;
            return false;
        }

        checkpoint_frame_number = found->first;
        return true;
    }

    return false;
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

bool cnr3_cache_manager_pin_checkpoint(
    Cnr3CacheManagerV005& cache,
    int checkpoint_frame_number
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state: cache.checkpoint_pool and cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    ++cache.stats.checkpoint_pin_attempts;

    auto found = cache.checkpoint_pool.find(checkpoint_frame_number);

    if (found == cache.checkpoint_pool.end()) {
        ++cache.stats.checkpoint_pin_failures;
        return false;
    }

    Cnr3CheckpointSlot& slot = found->second;

    if (slot.frame == nullptr) {
        ++cache.stats.checkpoint_pin_failures;
        return false;
    }

    ++slot.pin_count;
    ++cache.stats.checkpoint_pin_successes;

    return true;
}

bool cnr3_cache_manager_unpin_checkpoint(
    Cnr3CacheManagerV005& cache,
    int checkpoint_frame_number
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state: cache.checkpoint_pool and cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    ++cache.stats.checkpoint_unpin_attempts;

    auto found = cache.checkpoint_pool.find(checkpoint_frame_number);

    if (found == cache.checkpoint_pool.end()) {
        ++cache.stats.checkpoint_unpin_failures;
        return false;
    }

    Cnr3CheckpointSlot& slot = found->second;

    if (slot.frame == nullptr) {
        ++cache.stats.checkpoint_unpin_failures;
        return false;
    }

    if (slot.pin_count <= 0) {
        ++cache.stats.checkpoint_unpin_failures;
        ++cache.stats.checkpoint_unpin_underflow_errors;
        return false;
    }

    --slot.pin_count;
    ++cache.stats.checkpoint_unpin_successes;

    return true;
}

bool cnr3_cache_manager_has_pinned_checkpoints(
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

    for (const auto& entry : cache.checkpoint_pool) {
        const Cnr3CheckpointSlot& slot = entry.second;

        if (slot.pin_count > 0) {
            return true;
        }
    }

    return false;
}

int64_t cnr3_cache_manager_get_total_pin_count(
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

    int64_t total_pin_count = 0;

    for (const auto& entry : cache.checkpoint_pool) {
        const Cnr3CheckpointSlot& slot = entry.second;

        if (slot.pin_count > 0) {
            total_pin_count += slot.pin_count;
        }
    }

    return total_pin_count;
}

bool cnr3_cache_manager_store_output_frame(
    Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSFrame* output_frame,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state: cache.non_checkpoint_pool, cache.checkpoint_pool,
            cache.cache_index, cache.highest_cached_frame_number, and cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    /*
        Store one output frame in the v005 cache manager.

        Ownership rule:
            On successful insertion, this function takes one cache-owned
            reference using vsapi->addFrameRef(output_frame).

            That cache-owned reference is owned by exactly one of:
                cache.non_checkpoint_pool
                cache.checkpoint_pool

            cache.cache_index does not own the reference. It only aliases the
            owning pool's frame pointer.

            The cache-owned reference must later be released exactly once by
            pruning, clearing, or teardown.

        Promotion rule:
            cnr3_cache_manager_should_promote_checkpoint(frame_number) decides
            whether the frame is stored in checkpoint_pool or non_checkpoint_pool.

        Duplicate rule:
            If frame_number is already present in cache_index, no addFrameRef()
            is taken and the function returns false.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    ++cache.stats.cache_store_attempts;

    if (frame_number < 0 || output_frame == nullptr || vsapi == nullptr) {
        ++cache.stats.cache_store_failures;
        ++cache.stats.cache_store_invalid_input_errors;
        return false;
    }

    if (cache.cache_index.find(frame_number) != cache.cache_index.end()) {
        ++cache.stats.cache_store_failures;
        ++cache.stats.cache_store_duplicate_rejections;
        return false;
    }

    const bool should_promote_to_checkpoint =
        cnr3_cache_manager_should_promote_checkpoint(frame_number);

    const VSFrame* retained_frame = vsapi->addFrameRef(output_frame);

    if (retained_frame == nullptr) {
        ++cache.stats.cache_store_failures;
        ++cache.stats.cache_store_add_ref_failures;
        return false;
    }

    if (should_promote_to_checkpoint) {
        Cnr3CheckpointSlot slot;
        slot.frame = retained_frame;
        slot.pin_count = 0;

        const auto pool_insert_result =
            cache.checkpoint_pool.emplace(frame_number, slot);

        if (!pool_insert_result.second) {
            /*
                This should be impossible because cache_index said the frame was
                not present. Treat it as cache-manager corruption and release
                the retained reference immediately to avoid a leak.
            */
            vsapi->freeFrame(retained_frame);

            ++cache.stats.cache_store_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_store_pool_inconsistency_errors;
            return false;
        }
    }
    else {
        const auto pool_insert_result =
            cache.non_checkpoint_pool.emplace(frame_number, retained_frame);

        if (!pool_insert_result.second) {
            /*
                This should be impossible because cache_index said the frame was
                not present. Treat it as cache-manager corruption and release
                the retained reference immediately to avoid a leak.
            */
            vsapi->freeFrame(retained_frame);

            ++cache.stats.cache_store_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_store_pool_inconsistency_errors;
            return false;
        }
    }

    const auto index_insert_result =
        cache.cache_index.emplace(frame_number, retained_frame);

    if (!index_insert_result.second) {
        /*
            This should be impossible because cache_index was checked before
            insertion. Remove the owning pool entry and release the retained
            reference to keep ownership balanced.
        */
        if (should_promote_to_checkpoint) {
            cache.checkpoint_pool.erase(frame_number);
        }
        else {
            cache.non_checkpoint_pool.erase(frame_number);
        }

        vsapi->freeFrame(retained_frame);

        ++cache.stats.cache_store_failures;
        ++cache.stats.cache_integrity_errors;
        ++cache.stats.cache_store_index_inconsistency_errors;
        return false;
    }

    if (frame_number > cache.highest_cached_frame_number) {
        cache.highest_cached_frame_number = frame_number;
    }

    ++cache.stats.cache_store_successes;

    if (should_promote_to_checkpoint) {
        ++cache.stats.checkpoint_store_successes;
    }
    else {
        ++cache.stats.non_checkpoint_store_successes;
    }

    return true;
}

bool cnr3_cache_manager_remove_output_frame(
    Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state: cache.non_checkpoint_pool, cache.checkpoint_pool,
            cache.cache_index, cache.highest_cached_frame_number, and cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    /*
        Remove one cached output frame from the v005 cache manager.

        Ownership rule:
            A cached output frame must be owned by exactly one of:
                cache.non_checkpoint_pool
                cache.checkpoint_pool

            cache.cache_index does not own the frame reference. It only aliases
            the owning pool's frame pointer.

            On successful removal, this function removes the cache_index alias,
            erases exactly one owning pool entry, and releases exactly one
            cache-owned reference with vsapi->freeFrame().

        Pinned checkpoint rule:
            A checkpoint with pin_count > 0 must not be removed. The in-flight
            invocation that pinned it still depends on that checkpoint slot.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    ++cache.stats.cache_remove_attempts;

    if (frame_number < 0 || vsapi == nullptr) {
        ++cache.stats.cache_remove_failures;
        ++cache.stats.cache_remove_invalid_input_errors;
        return false;
    }

    const auto index_found = cache.cache_index.find(frame_number);

    if (index_found == cache.cache_index.end()) {
        ++cache.stats.cache_remove_failures;
        ++cache.stats.cache_remove_not_found_failures;
        return false;
    }

    const VSFrame* indexed_frame = index_found->second;

    if (indexed_frame == nullptr) {
        ++cache.stats.cache_remove_failures;
        ++cache.stats.cache_integrity_errors;
        ++cache.stats.cache_remove_index_inconsistency_errors;
        return false;
    }

    auto non_checkpoint_found = cache.non_checkpoint_pool.find(frame_number);
    auto checkpoint_found = cache.checkpoint_pool.find(frame_number);

    const bool in_non_checkpoint_pool =
        (non_checkpoint_found != cache.non_checkpoint_pool.end());

    const bool in_checkpoint_pool =
        (checkpoint_found != cache.checkpoint_pool.end());

    if (in_non_checkpoint_pool == in_checkpoint_pool) {
        /*
            Either neither owning pool contains the indexed frame, or both pools
            contain the same frame number. Both states violate the ownership
            invariant.
        */
        ++cache.stats.cache_remove_failures;
        ++cache.stats.cache_integrity_errors;
        ++cache.stats.cache_remove_pool_inconsistency_errors;
        return false;
    }

    const VSFrame* owned_frame = nullptr;

    if (in_checkpoint_pool) {
        Cnr3CheckpointSlot& slot = checkpoint_found->second;

        if (slot.pin_count > 0) {
            ++cache.stats.cache_remove_failures;
            ++cache.stats.cache_remove_pinned_checkpoint_rejections;
            return false;
        }

        owned_frame = slot.frame;

        if (owned_frame == nullptr) {
            ++cache.stats.cache_remove_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.checkpoint_null_frame_errors;
            ++cache.stats.cache_remove_pool_inconsistency_errors;
            return false;
        }

        if (owned_frame != indexed_frame) {
            ++cache.stats.cache_remove_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_remove_index_inconsistency_errors;
            return false;
        }

        cache.cache_index.erase(index_found);
        cache.checkpoint_pool.erase(checkpoint_found);
        vsapi->freeFrame(owned_frame);

        ++cache.stats.cache_remove_successes;
        ++cache.stats.checkpoint_remove_successes;
    }
    else {
        owned_frame = non_checkpoint_found->second;

        if (owned_frame == nullptr) {
            ++cache.stats.cache_remove_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_remove_pool_inconsistency_errors;
            return false;
        }

        if (owned_frame != indexed_frame) {
            ++cache.stats.cache_remove_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_remove_index_inconsistency_errors;
            return false;
        }

        cache.cache_index.erase(index_found);
        cache.non_checkpoint_pool.erase(non_checkpoint_found);
        vsapi->freeFrame(owned_frame);

        ++cache.stats.cache_remove_successes;
        ++cache.stats.non_checkpoint_remove_successes;
    }

    if (frame_number == cache.highest_cached_frame_number) {
        cache.highest_cached_frame_number = -1;

        for (const auto& entry : cache.non_checkpoint_pool) {
            if (entry.first > cache.highest_cached_frame_number) {
                cache.highest_cached_frame_number = entry.first;
            }
        }

        for (const auto& entry : cache.checkpoint_pool) {
            if (entry.first > cache.highest_cached_frame_number) {
                cache.highest_cached_frame_number = entry.first;
            }
        }
    }

    return true;
}