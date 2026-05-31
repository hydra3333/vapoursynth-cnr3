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

static bool cnr3_cache_manager_check_invariants_externally_locked(
    Cnr3CacheManagerV005& cache
);

static bool cnr3_cache_manager_prune_non_checkpoint_pool_externally_locked(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
);

static bool cnr3_cache_manager_prune_checkpoint_pool_externally_locked(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
);

static bool cnr3_cache_manager_validate_invariants_externally_locked(
    Cnr3CacheManagerV005& cache
);

static bool cnr3_cache_manager_remove_output_frame_externally_locked(
    Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Does not lock internally.
        Caller requirement:
            Requires a lock to be applied by the caller before calling this
            function; i.e. the caller MUST already hold cache.cache_mutex.
        Expected callers:
            cnr3_cache_manager_remove_output_frame().
            Future prune helpers that need to remove multiple frames while
            holding one cache-manager critical section.
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

bool cnr3_cache_manager_clear(
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

    ++cache.stats.cache_clear_attempts;

    /*
        Release every VS frame reference owned by the v005 cache manager.

        Important ownership rule:
            Frames stored in non_checkpoint_pool and checkpoint_pool are owned
            references previously retained with addFrameRef().

        cache_index contains non-owning aliases only. Those pointers must not be
        released separately.

        Safety rule:
            If the cache owns any frame references, vsapi must be available so
            those references can be released with freeFrame(). Clearing non-empty
            pools without freeFrame() would leak the cache-owned references.
    */

    const bool cache_has_owned_frames =
        (
            !cache.non_checkpoint_pool.empty() ||
            !cache.checkpoint_pool.empty()
            );

    if (vsapi == nullptr && cache_has_owned_frames) {
        ++cache.stats.cache_clear_failures;
        ++cache.stats.cache_clear_null_vsapi_failures;
        ++cache.stats.cache_integrity_errors;
        return false;
    }

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

    ++cache.stats.cache_clear_successes;
    return true;
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

bool cnr3_cache_manager_get_debug_snapshot(
    Cnr3CacheManagerV005& cache,
    Cnr3CacheManagerDebugSnapshot& snapshot
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads mutable cache-manager
            state and copies a coherent diagnostic snapshot.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    /*
        Collect one passive debug snapshot while holding cache.cache_mutex once.

        This avoids a mixed-time summary where counts, pin state, validation
        result, and statistics might come from different moments.

        The invariant check used here is passive. It does not increment
        validation counters or integrity counters.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    snapshot = Cnr3CacheManagerDebugSnapshot{};

    snapshot.non_checkpoint_count = cache.non_checkpoint_pool.size();
    snapshot.checkpoint_count = cache.checkpoint_pool.size();
    snapshot.total_cached_frame_count =
        snapshot.non_checkpoint_count + snapshot.checkpoint_count;

    snapshot.highest_cached_frame_number =
        cache.highest_cached_frame_number;

    int64_t total_pin_count = 0;
    bool has_pinned_checkpoints = false;

    for (const auto& entry : cache.checkpoint_pool) {
        const Cnr3CheckpointSlot& slot = entry.second;

        if (slot.pin_count > 0) {
            has_pinned_checkpoints = true;
            total_pin_count += static_cast<int64_t>(slot.pin_count);
        }
    }

    snapshot.has_pinned_checkpoints = has_pinned_checkpoints;
    snapshot.total_pin_count = total_pin_count;

    snapshot.invariants_ok =
        cnr3_cache_manager_check_invariants_externally_locked(cache);

    snapshot.stats = cache.stats;

    return true;
}

bool cnr3_cache_manager_validate_invariants(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state through
            cnr3_cache_manager_validate_invariants_externally_locked().
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return cnr3_cache_manager_validate_invariants_externally_locked(
        cache
    );
}

static bool cnr3_cache_manager_validate_invariants_externally_locked(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Does not lock internally.
        Caller requirement:
            Requires a lock to be applied by the caller before calling this
            function; i.e. the caller MUST already hold cache.cache_mutex.
        Expected callers:
            cnr3_cache_manager_validate_invariants().
            Future compound cache-manager helpers that need to validate
            invariants while holding one cache-manager critical section.
    */

    /*
        Validate core cache-manager invariants.

        Ownership/index invariants:
            Every frame number in cache_index must exist in exactly one owning pool.

            Every frame number in non_checkpoint_pool must exist in cache_index.

            Every frame number in checkpoint_pool must exist in cache_index.

            No frame number may exist in both owning pools.

            cache_index is non-owning. It must alias the VSFrame pointer owned by
            the corresponding owning pool.

        Frame-reference invariants:
            Owning pool frame pointers must not be nullptr.

            checkpoint pin_count must never be negative.

        Highest-frame invariant:
            highest_cached_frame_number must match the actual highest frame
            number present in either owning pool, or -1 if both pools are empty.
    */

    ++cache.stats.cache_validation_attempts;

    bool valid = true;

    int actual_highest_cached_frame_number = -1;

    for (const auto& entry : cache.non_checkpoint_pool) {
        const int frame_number = entry.first;
        const VSFrame* pool_frame = entry.second;

        if (frame_number > actual_highest_cached_frame_number) {
            actual_highest_cached_frame_number = frame_number;
        }

        if (pool_frame == nullptr) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_null_frame_errors;
            valid = false;
        }

        if (cache.checkpoint_pool.find(frame_number) != cache.checkpoint_pool.end()) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_dual_pool_ownership_errors;
            valid = false;
        }

        const auto index_found = cache.cache_index.find(frame_number);

        if (index_found == cache.cache_index.end()) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_pool_missing_cache_index_errors;
            valid = false;
        }
        else if (index_found->second != pool_frame) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_pool_missing_cache_index_errors;
            valid = false;
        }
    }

    for (const auto& entry : cache.checkpoint_pool) {
        const int frame_number = entry.first;
        const Cnr3CheckpointSlot& slot = entry.second;

        if (frame_number > actual_highest_cached_frame_number) {
            actual_highest_cached_frame_number = frame_number;
        }

        if (slot.frame == nullptr) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_null_frame_errors;
            ++cache.stats.checkpoint_null_frame_errors;
            valid = false;
        }

        if (slot.pin_count < 0) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_negative_pin_count_errors;
            valid = false;
        }

        if (cache.non_checkpoint_pool.find(frame_number) != cache.non_checkpoint_pool.end()) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_dual_pool_ownership_errors;
            valid = false;
        }

        const auto index_found = cache.cache_index.find(frame_number);

        if (index_found == cache.cache_index.end()) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_pool_missing_cache_index_errors;
            valid = false;
        }
        else if (index_found->second != slot.frame) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_pool_missing_cache_index_errors;
            valid = false;
        }
    }

    for (const auto& entry : cache.cache_index) {
        const int frame_number = entry.first;
        const VSFrame* indexed_frame = entry.second;

        const auto non_checkpoint_found =
            cache.non_checkpoint_pool.find(frame_number);

        const auto checkpoint_found =
            cache.checkpoint_pool.find(frame_number);

        const bool in_non_checkpoint_pool =
            (non_checkpoint_found != cache.non_checkpoint_pool.end());

        const bool in_checkpoint_pool =
            (checkpoint_found != cache.checkpoint_pool.end());

        if (indexed_frame == nullptr) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_null_frame_errors;
            valid = false;
        }

        if (in_non_checkpoint_pool == in_checkpoint_pool) {
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.cache_validation_cache_index_missing_pool_entry_errors;
            valid = false;
            continue;
        }

        if (in_non_checkpoint_pool) {
            if (non_checkpoint_found->second != indexed_frame) {
                ++cache.stats.cache_integrity_errors;
                ++cache.stats.cache_validation_cache_index_missing_pool_entry_errors;
                valid = false;
            }
        }
        else {
            if (checkpoint_found->second.frame != indexed_frame) {
                ++cache.stats.cache_integrity_errors;
                ++cache.stats.cache_validation_cache_index_missing_pool_entry_errors;
                valid = false;
            }
        }
    }

    if (cache.highest_cached_frame_number != actual_highest_cached_frame_number) {
        ++cache.stats.cache_integrity_errors;
        ++cache.stats.cache_validation_highest_frame_number_errors;
        valid = false;
    }

    if (valid) {
        ++cache.stats.cache_validation_successes;
    }
    else {
        ++cache.stats.cache_validation_failures;
    }

    return valid;
}

static bool cnr3_cache_manager_check_invariants_externally_locked(
    Cnr3CacheManagerV005& cache
) {
    /*
        Thread safety:
            Does not lock internally.
        Caller requirement:
            Requires a lock to be applied by the caller before calling this
            function; i.e. the caller MUST already hold cache.cache_mutex.
        Expected callers:
            cnr3_cache_manager_get_debug_snapshot().
    */

    /*
        Passively check core cache-manager invariants.

        This helper intentionally does not update validation counters,
        integrity counters, or any other statistics. It is used when diagnostic
        code needs to know whether the current cache state is coherent without
        changing the statistics it is about to report.

        The checked invariants mirror the mutating validation helper:
            - every cache_index entry exists in exactly one owning pool
            - every owning pool entry exists in cache_index
            - no frame number exists in both owning pools
            - no stored frame pointer is nullptr
            - checkpoint pin_count is never negative
            - highest_cached_frame_number matches the actual highest cached
              frame number, or -1 if both owning pools are empty
    */

    int actual_highest_cached_frame_number = -1;

    for (const auto& entry : cache.non_checkpoint_pool) {
        const int frame_number = entry.first;
        const VSFrame* pool_frame = entry.second;

        if (frame_number > actual_highest_cached_frame_number) {
            actual_highest_cached_frame_number = frame_number;
        }

        if (pool_frame == nullptr) {
            return false;
        }

        if (cache.checkpoint_pool.find(frame_number) != cache.checkpoint_pool.end()) {
            return false;
        }

        const auto index_found = cache.cache_index.find(frame_number);

        if (index_found == cache.cache_index.end()) {
            return false;
        }

        if (index_found->second != pool_frame) {
            return false;
        }
    }

    for (const auto& entry : cache.checkpoint_pool) {
        const int frame_number = entry.first;
        const Cnr3CheckpointSlot& slot = entry.second;

        if (frame_number > actual_highest_cached_frame_number) {
            actual_highest_cached_frame_number = frame_number;
        }

        if (slot.frame == nullptr) {
            return false;
        }

        if (slot.pin_count < 0) {
            return false;
        }

        if (cache.non_checkpoint_pool.find(frame_number) != cache.non_checkpoint_pool.end()) {
            return false;
        }

        const auto index_found = cache.cache_index.find(frame_number);

        if (index_found == cache.cache_index.end()) {
            return false;
        }

        if (index_found->second != slot.frame) {
            return false;
        }
    }

    for (const auto& entry : cache.cache_index) {
        const int frame_number = entry.first;
        const VSFrame* indexed_frame = entry.second;

        if (indexed_frame == nullptr) {
            return false;
        }

        const auto non_checkpoint_found =
            cache.non_checkpoint_pool.find(frame_number);

        const auto checkpoint_found =
            cache.checkpoint_pool.find(frame_number);

        const bool in_non_checkpoint_pool =
            (non_checkpoint_found != cache.non_checkpoint_pool.end());

        const bool in_checkpoint_pool =
            (checkpoint_found != cache.checkpoint_pool.end());

        if (in_non_checkpoint_pool == in_checkpoint_pool) {
            return false;
        }

        if (in_non_checkpoint_pool) {
            if (non_checkpoint_found->second != indexed_frame) {
                return false;
            }
        }
        else {
            if (checkpoint_found->second.frame != indexed_frame) {
                return false;
            }
        }
    }

    return (
        cache.highest_cached_frame_number ==
        actual_highest_cached_frame_number
        );
}

bool cnr3_cache_manager_find_and_pin_nearest_prior_checkpoint(
    Cnr3CacheManagerV005& cache,
    int requested_frame_number,
    int& checkpoint_frame_number
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state: cache.checkpoint_pool and cache.stats.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    /*
        Atomically find and pin the nearest prior checkpoint.

        Critical safety rule:
            Future runtime code must not perform this as separate public calls:

                find nearest prior checkpoint
                unlock
                later pin checkpoint

            That sequence is unsafe because another cache operation could prune
            the selected checkpoint between the find and the pin.

        This helper performs both operations while holding cache.cache_mutex:
            1. Find the nearest prior checkpoint by strict frame-number order.
            2. Verify that the checkpoint slot has a valid frame pointer.
            3. Increment that checkpoint slot's pin_count.
            4. Validate the mutation in development builds.
            5. Return only the checkpoint frame number.

        If development validation fails after pin_count has been incremented,
        this helper rolls the pin_count change back before returning false.

        Therefore:
            return true  means the checkpoint was found and remains pinned.
            return false means no new pin remains held by this helper.

        This helper does not return a raw VSFrame pointer. The checkpoint slot is
        protected from pruning by pin_count, not by an extra addFrameRef().
    */

    checkpoint_frame_number = -1;

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    ++cache.stats.checkpoint_find_and_pin_attempts;

    for (
        auto found = cache.checkpoint_pool.rbegin();
        found != cache.checkpoint_pool.rend();
        ++found
        ) {
        if (found->first >= requested_frame_number) {
            continue;
        }

        Cnr3CheckpointSlot& slot = found->second;

        if (slot.frame == nullptr) {
            ++cache.stats.checkpoint_find_and_pin_failures;
            ++cache.stats.checkpoint_find_and_pin_null_frame_failures;
            ++cache.stats.cache_integrity_errors;
            ++cache.stats.checkpoint_null_frame_errors;
            return false;
        }

        ++cache.stats.checkpoint_pin_attempts;

        ++slot.pin_count;
        checkpoint_frame_number = found->first;

        if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
            if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
                /*
                    The pin has not been published to the caller as a success,
                    so roll it back before returning false. This prevents a
                    future caller from missing the matching unpin cleanup.
                */
                --slot.pin_count;
                checkpoint_frame_number = -1;

                ++cache.stats.checkpoint_find_and_pin_failures;
                ++cache.stats.checkpoint_pin_failures;

                return false;
            }
        }

        ++cache.stats.checkpoint_find_and_pin_successes;
        ++cache.stats.checkpoint_pin_successes;

        return true;
    }

    ++cache.stats.checkpoint_find_and_pin_failures;
    ++cache.stats.checkpoint_find_and_pin_no_prior_checkpoint_failures;

    return false;
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

    /*
        Pin one existing checkpoint.

        A successful return means pin_count was incremented and remains
        incremented.

        If development validation fails after pin_count has been incremented,
        this helper rolls the increment back before returning false.
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
        ++cache.stats.cache_integrity_errors;
        ++cache.stats.checkpoint_null_frame_errors;
        return false;
    }

    ++slot.pin_count;

    if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
        if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
            /*
                The pin is not being reported as a success, so restore the
                caller-visible pin state before returning false.
            */
            --slot.pin_count;

            ++cache.stats.checkpoint_pin_failures;
            return false;
        }
    }

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

    /*
        Unpin one existing checkpoint.

        A successful return means pin_count was decremented and remains
        decremented.

        If development validation fails after pin_count has been decremented,
        this helper rolls the decrement back before returning false.
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
        ++cache.stats.cache_integrity_errors;
        ++cache.stats.checkpoint_null_frame_errors;
        return false;
    }

    if (slot.pin_count <= 0) {
        ++cache.stats.checkpoint_unpin_failures;
        ++cache.stats.checkpoint_unpin_underflow_errors;
        return false;
    }

    --slot.pin_count;

    if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
        if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
            /*
                The unpin is not being reported as a success, so restore the
                caller-visible pin state before returning false.
            */
            ++slot.pin_count;

            ++cache.stats.checkpoint_unpin_failures;
            return false;
        }
    }

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

    if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
        if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
            ++cache.stats.cache_store_post_validation_failures;
            return false;
        }
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
            cache-manager state through
            cnr3_cache_manager_remove_output_frame_externally_locked().
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    const bool removed =
        cnr3_cache_manager_remove_output_frame_externally_locked(
            cache,
            frame_number,
            vsapi
        );

    if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
        if (
            removed &&
            !cnr3_cache_manager_validate_invariants_externally_locked(cache)
            ) {
            ++cache.stats.cache_remove_post_validation_failures;
            return false;
        }
    }

    return removed;
}

bool cnr3_cache_manager_prune_non_checkpoint_pool(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state through
            cnr3_cache_manager_prune_non_checkpoint_pool_externally_locked().
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return cnr3_cache_manager_prune_non_checkpoint_pool_externally_locked(
        cache,
        vsapi
    );
}

bool cnr3_cache_manager_prune_checkpoint_pool(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state through
            cnr3_cache_manager_prune_checkpoint_pool_externally_locked().
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    return cnr3_cache_manager_prune_checkpoint_pool_externally_locked(
        cache,
        vsapi
    );
}

bool cnr3_cache_manager_prune_after_store(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Locks cache.cache_mutex internally. Reads and writes mutable
            cache-manager state through the non-checkpoint and checkpoint
            prune helpers' _externally_locked implementations.
        Caller requirement:
            Caller must not already hold cache.cache_mutex.
    */

    /*
        Run the standard post-store pruning pass.

        Pruning order:
            1. Prune non_checkpoint_pool using the Option B overflow policy.
            2. Prune checkpoint_pool using checkpoint retain/pin rules.

        Both pruning steps run under one cache_mutex critical section. This keeps
        the combined post-store pruning pass internally consistent and avoids
        deadlocking on public helpers that would otherwise lock internally.

        Actual frame removal is delegated to
        cnr3_cache_manager_remove_output_frame_externally_locked(), so
        cache_index erasure, owning-pool erasure, and freeFrame() release remain
        centralised.
    */

    std::lock_guard<std::mutex> lock(cache.cache_mutex);

    ++cache.stats.prune_after_store_attempts;

    const bool non_checkpoint_prune_ok =
        cnr3_cache_manager_prune_non_checkpoint_pool_externally_locked(
            cache,
            vsapi
        );

    const bool checkpoint_prune_ok =
        cnr3_cache_manager_prune_checkpoint_pool_externally_locked(
            cache,
            vsapi
        );

    if (!non_checkpoint_prune_ok) {
        ++cache.stats.prune_after_store_non_checkpoint_failures;
    }

    if (!checkpoint_prune_ok) {
        ++cache.stats.prune_after_store_checkpoint_failures;
    }

    if (!non_checkpoint_prune_ok || !checkpoint_prune_ok) {
        ++cache.stats.prune_after_store_failures;
        return false;
    }

    if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
        if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
            ++cache.stats.prune_after_store_post_validation_failures;
            return false;
        }
    }

    ++cache.stats.prune_after_store_successes;
    return true;
}

static bool cnr3_cache_manager_prune_non_checkpoint_pool_externally_locked(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Does not lock internally.
        Caller requirement:
            Requires a lock to be applied by the caller before calling this
            function; i.e. the caller MUST already hold cache.cache_mutex.
        Expected callers:
            cnr3_cache_manager_prune_non_checkpoint_pool().
            Combined prune helpers that need to prune multiple pools while
            holding one cache-manager critical section.
    */

    /*
        Prune only the non-checkpoint output-frame pool.

        Option B pruning policy:
            Let non_checkpoint_pool grow up to the overflow limit.

            If non_checkpoint_pool.size() exceeds the overflow limit, prune the
            oldest non-checkpoint frames first until non_checkpoint_pool.size()
            is back to CNR3_OUTPUT_CACHE_CAPACITY.

        Ordering rule:
            non_checkpoint_pool is std::map<int, const VSFrame *>, ordered by
            frame number. begin() is therefore the lowest/oldest cached
            non-checkpoint frame number.

        Ownership rule:
            Frame removal is delegated to
            cnr3_cache_manager_remove_output_frame_externally_locked(), which
            removes the cache_index alias, erases exactly one owning pool entry,
            and releases exactly one cache-owned VSFrame reference with
            vsapi->freeFrame().

        Checkpoint rule:
            This helper does not choose or prune checkpoints.
    */

    ++cache.stats.non_checkpoint_prune_attempts;

    if (vsapi == nullptr) {
        ++cache.stats.non_checkpoint_prune_remove_failures;
        return false;
    }

    const int overflow_limit =
        cnr3_cache_manager_get_non_checkpoint_overflow_limit();

    if (
        cache.non_checkpoint_pool.size() <=
        static_cast<std::size_t>(overflow_limit)
        ) {
        ++cache.stats.non_checkpoint_prune_skipped_below_overflow;
        return true;
    }

    ++cache.stats.non_checkpoint_prune_runs;

    bool all_removes_succeeded = true;

    while (
        cache.non_checkpoint_pool.size() >
        static_cast<std::size_t>(CNR3_OUTPUT_CACHE_CAPACITY)
        ) {
        if (cache.non_checkpoint_pool.empty()) {
            break;
        }

        const int frame_number_to_remove =
            cache.non_checkpoint_pool.begin()->first;

        const bool removed =
            cnr3_cache_manager_remove_output_frame_externally_locked(
                cache,
                frame_number_to_remove,
                vsapi
            );

        if (!removed) {
            ++cache.stats.non_checkpoint_prune_remove_failures;
            all_removes_succeeded = false;
            break;
        }

        ++cache.stats.non_checkpoint_prune_removed_frames;

        if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
            if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
                ++cache.stats.non_checkpoint_prune_post_validation_failures;
                return false;
            }
        }
    }

    return all_removes_succeeded;
}

static bool cnr3_cache_manager_prune_checkpoint_pool_externally_locked(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
    /*
        Thread safety:
            Does not lock internally.
        Caller requirement:
            Requires a lock to be applied by the caller before calling this
            function; i.e. the caller MUST already hold cache.cache_mutex.
        Expected callers:
            cnr3_cache_manager_prune_checkpoint_pool().
            Combined prune helpers that need to prune multiple pools while
            holding one cache-manager critical section.
    */

    /*
        Prune only the checkpoint output-frame pool.

        Checkpoint pruning policy:
            If checkpoint_pool.size() exceeds CNR3_CHECKPOINT_MAX_RETAIN, prune
            the oldest eligible checkpoints first until checkpoint_pool.size()
            is back to CNR3_CHECKPOINT_MIN_RETAIN.

        Eligibility rules:
            Frame 0 is never pruned because it is the last-resort checkpoint.

            A checkpoint with pin_count > 0 is never pruned because an in-flight
            invocation still depends on that checkpoint slot.

        Ordering rule:
            checkpoint_pool is std::map<int, Cnr3CheckpointSlot>, ordered by
            frame number. begin() is therefore the lowest/oldest checkpoint
            frame number.

        Ownership rule:
            Frame removal is delegated to
            cnr3_cache_manager_remove_output_frame_externally_locked(), which
            removes the cache_index alias, erases exactly one owning pool entry,
            and releases exactly one cache-owned VSFrame reference with
            vsapi->freeFrame().
    */

    ++cache.stats.checkpoint_prune_attempts;

    if (vsapi == nullptr) {
        ++cache.stats.checkpoint_prune_remove_failures;
        return false;
    }

    if (
        cache.checkpoint_pool.size() <=
        static_cast<std::size_t>(CNR3_CHECKPOINT_MAX_RETAIN)
        ) {
        ++cache.stats.checkpoint_prune_skipped_below_max_retain;
        return true;
    }

    ++cache.stats.checkpoint_prune_runs;

    bool all_removes_succeeded = true;

    while (
        cache.checkpoint_pool.size() >
        static_cast<std::size_t>(CNR3_CHECKPOINT_MIN_RETAIN)
        ) {
        bool removed_one_checkpoint = false;

        for (
            auto found = cache.checkpoint_pool.begin();
            found != cache.checkpoint_pool.end();
            ++found
            ) {
            const int frame_number_to_remove = found->first;
            const Cnr3CheckpointSlot& slot = found->second;

            if (frame_number_to_remove == 0) {
                ++cache.stats.checkpoint_prune_skipped_frame_zero;
                continue;
            }

            if (slot.pin_count > 0) {
                ++cache.stats.checkpoint_prune_skipped_pinned;
                continue;
            }

            const bool removed =
                cnr3_cache_manager_remove_output_frame_externally_locked(
                    cache,
                    frame_number_to_remove,
                    vsapi
                );

            if (!removed) {
                ++cache.stats.checkpoint_prune_remove_failures;
                all_removes_succeeded = false;
            }
            else {
                ++cache.stats.checkpoint_prune_removed_frames;
                removed_one_checkpoint = true;

                if constexpr (CNR3_CACHE_MANAGER_VALIDATE_AFTER_MUTATION) {
                    if (!cnr3_cache_manager_validate_invariants_externally_locked(cache)) {
                        ++cache.stats.checkpoint_prune_post_validation_failures;
                        return false;
                    }
                }
            }

            /*
                The remove helper erases from checkpoint_pool, invalidating the
                iterator used by this loop. Break and restart from begin().
            */
            break;
        }

        if (!removed_one_checkpoint) {
            /*
                The pool still exceeds the target size, but every remaining
                checkpoint is either frame 0 or pinned, or a removal failed.
                Stop rather than looping forever.
            */
            ++cache.stats.checkpoint_prune_no_eligible_frames;
            break;
        }

        if (!all_removes_succeeded) {
            break;
        }
    }

    return all_removes_succeeded;
}
