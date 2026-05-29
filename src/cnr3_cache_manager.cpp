#include "cnr3_cache_manager.h"

// -----------------------------------------------------------------------------
// CNR3 cache manager - v005 helper functions
//
// Phase 2A adds only safe cache state inspection and teardown/release helpers.
//
// These helpers are not wired into current runtime behaviour yet.
// The existing strict-streaming cache remains active.
// -----------------------------------------------------------------------------

int cnr3_cache_manager_get_non_checkpoint_overflow_limit() {
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
    const Cnr3CacheManagerV005& cache
) {
    return (
        cache.non_checkpoint_pool.empty() &&
        cache.checkpoint_pool.empty() &&
        cache.cache_index.empty()
        );
}

std::size_t cnr3_cache_manager_get_non_checkpoint_count(
    const Cnr3CacheManagerV005& cache
) {
    return cache.non_checkpoint_pool.size();
}

std::size_t cnr3_cache_manager_get_checkpoint_count(
    const Cnr3CacheManagerV005& cache
) {
    return cache.checkpoint_pool.size();
}

std::size_t cnr3_cache_manager_get_total_cached_frame_count(
    const Cnr3CacheManagerV005& cache
) {
    return (
        cache.non_checkpoint_pool.size() +
        cache.checkpoint_pool.size()
        );
}

bool cnr3_cache_manager_contains_output_frame(
    const Cnr3CacheManagerV005& cache,
    int frame_number
) {
    return (cache.cache_index.find(frame_number) != cache.cache_index.end());
}

void cnr3_cache_manager_clear(
    Cnr3CacheManagerV005& cache,
    const VSAPI* vsapi
) {
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
