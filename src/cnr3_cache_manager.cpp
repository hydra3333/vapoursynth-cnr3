#include "cnr3_cache_manager.h"

// -----------------------------------------------------------------------------
// CNR3 cache manager - v005 helper functions
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

bool cnr3_cache_manager_find_output_frame(
    const Cnr3CacheManagerV005& cache,
    int frame_number,
    const VSFrame*& output_frame
) {
    /*
        Find any cached output frame by frame number.

        This searches cache_index, which is the fast lookup table across both:
            non_checkpoint_pool
            checkpoint_pool

        cache_index does not own frame references. It only aliases the owning
        references held in the two ordered pools.
    */

    output_frame = nullptr;

    const auto found = cache.cache_index.find(frame_number);

    if (found == cache.cache_index.end()) {
        return false;
    }

    output_frame = found->second;
    return (output_frame != nullptr);
}

bool cnr3_cache_manager_find_nearest_prior_checkpoint(
    const Cnr3CacheManagerV005& cache,
    int requested_frame_number,
    int& checkpoint_frame_number,
    const VSFrame*& checkpoint_frame
) {
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
    */

    checkpoint_frame_number = -1;
    checkpoint_frame = nullptr;

    if (cache.checkpoint_pool.empty()) {
        return false;
    }

    auto found = cache.checkpoint_pool.lower_bound(requested_frame_number);

    if (found == cache.checkpoint_pool.begin()) {
        return false;
    }

    --found;

    checkpoint_frame_number = found->first;
    checkpoint_frame = found->second.frame;

    return (checkpoint_frame != nullptr);
}

bool cnr3_cache_manager_should_promote_checkpoint(
    int frame_number
) {
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