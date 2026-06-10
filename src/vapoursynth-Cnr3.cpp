/*
    CNR3 - VapourSynth API4 chroma stabiliser, based on the venerable CNR2/VSCNR2

    CNR3 is a redevelopment intended to closely follow the Cnr2/vscnr2 recursive
    temporal chroma-stabilisation model while using VapourSynth API4 only.

    Recursive processing and VapourSynth scheduling:
        The Cnr2/vscnr2 algorithm is inherently temporal and recursive, which
        requires in-order serial frame processing.

        Processing of SOURCE frame N into OUTPUT N requires access to the already
        filtered OUTPUT arising from previously processed SOURCE frame N - 1:
            output[N] depends on both SOURCE[N] and OUTPUT[N - 1]

        That makes the CNR2/vscnr2 algorithm naturally "serial".

        Older VapourSynth-era recursive filters could sometimes rely on
        compatibility-style scheduling parameters and assumptions. In
        particular, 'fmFrameState' meant only one thread would call a filter's
        getframe function at a time and only one frame would be processed at a
        time.

        However, VapourSynth API4 documentation says 'fmFrameState' is
        for compatibility only and MUST NOT BE USED IN NEW FILTERS.

        CNR3 therefore uses 'fmUnordered'.

        In 'fmUnordered', only one thread can call this filter's getframe function
        at a time, which protects CNR3's internal recursive state from
        concurrent entry. HOWEVER, 'fmUnordered' does not guarantee in-order frame
        processing. VapourSynth may STILL call CNR3's getframe for frames in a
        NON-SERIAL ORDER, which effectively defeats a recursive output[N - 1]
        algorithm without special measures.

        INITIAL REDEVELOPMENT APPROACH:
        CNR3 implementation currently uses a strict streaming cache policy:
            - frame 0 initialises the previous-output state
            - frame N requires output[N - 1] to have already been produced
            - out-of-order frame requests are rejected with a clear error
        During testing, use:
            vspipe -r 1
        This is a deliberate correctness-first API4 bridge.
        An upcoming cache manager will relax the strict ordering requirement
        by adding reorder, seek, checkpoint, or recomputation support.
        Until then, CNR3 must be treated as a serial recursive filter.

    Diagnostic output rule:
        CNR3 must never write to stdout, debug/status messages must go to stderr.
        VapourSynth errors must use mapSetError() or setFilterError().

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "cnr3_build_config.h"
#include "cnr3_output_cache_manager.h"
#include "cnr3_memory_diagnostics.h"
#include "cnr3_common.h"
#include "cnr3_response_tables.h"
#include "cnr3_frame_internal_processing.h"

// -----------------------------------------------------------------------------
//  API policy:
//      CNR3 is an API4-only VapourSynth plugin.
//      Do not include legacy VapourSynth.h / VSHelper.h.
//      Do not use API3-era types or functions.
// -----------------------------------------------------------------------------
#ifndef VAPOURSYNTH_API_VERSION
#error "CNR3 requires VapourSynth API4 headers. VAPOURSYNTH_API_VERSION is not defined."
#endif
#ifndef VS_EXTERNAL_API
#error "CNR3 requires VapourSynth API4-compatible headers. VS_EXTERNAL_API is not defined."
#endif
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Help identify and track instances.
// i.e. which source stream is being processed and thus which cache to use.
// Interlaced sources are usually have fields separated and processed separately
// before re-interlacing or deinterlacing - which means 2 instances of this plugin.
//
static std::atomic<int> g_cnr3_next_instance_id{ 1 };
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// HELPER functions
// -----------------------------------------------------------------------------

static void cnr3_vfprintf_stderr(
    const char* format,
    va_list args
) {
    /*
        CNR3 diagnostic convention:
            - never write plugin diagnostics to stdout
            - debug/status output goes to stderr
            - VapourSynth user-facing errors use mapSetError/setFilterError

        stdout may be used by vspipe for video/data output, so plugin code
        must not write anything there.

        This helper receives an already-started va_list from a printf-style
        wrapper function and writes the formatted message to stderr.
    */
    if (format == nullptr) {
        return;
    }

    std::vfprintf(stderr, format, args);
    std::fflush(stderr);
}

static void cnr3_debug_printf(
    bool debug_enabled,
    const char* format,
    ...
) {
    /*
        This is a small printf-style helper.

        The "..." is the C/C++ varargs syntax. It allows calls such as:

            cnr3_debug_printf(debug, "frame=%d value=%d\n", n, value);

        The named arguments are:
            debug_enabled
            format

        Everything after format is captured by va_start() into args and passed
        to std::vfprintf().

        Use this only for CNR3 diagnostics. Do not use stdout.
    */
    if (!debug_enabled || format == nullptr) {
        return;
    }

    va_list args;
    va_start(args, format);
    cnr3_vfprintf_stderr(format, args);
    va_end(args);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static void cnr3_debug_print_cache_state(
    const Cnr3Data* d,
    const char* where,
    int requested_frame
) {
    if (d == nullptr || !d->debug) {
        return;
    }

    const int next_needed = d->old_strict_cache.next_needed;
    const int gap = requested_frame - next_needed;

    cnr3_debug_printf(
        d->debug,
        "CNR3 debug: instance=%d, %s: requested=%d, next_needed=%d, gap=%d, prev_output=%s\n",
        d->instance_id,
        where,
        requested_frame,
        next_needed,
        gap,
        d->old_strict_cache.prev_output != nullptr ? "yes" : "no"
    );
}

static const char* cnr3_hot_zone_event_name(
    int event_kind
) {
    switch (event_kind) {
    case 1:
        return "HOT-ZONE-HIT";
    case 2:
        return "HOT-ZONE-SLIDE";
    case 3:
        return "HOT-ZONE-NEW-ALLOCATE";
    case 4:
        return "HOT-ZONE-RETIRE";
    case 5:
        return "HOT-ZONE-MERGE";
    default:
        return "HOT-ZONE-NONE";
    }
}

static void cnr3_debug_print_output_cache_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Print one scan-friendly output-cache summary line.

        The field set intentionally preserves the earlier multi-line summary
        counters. Keep this as a single stderr line so long logs remain human
        searchable while cache/refcount safety is being proven.
    */

    if (d == nullptr || !d->debug || where == nullptr) {
        return;
    }

    Cnr3OutputCacheManager& cache =
        const_cast<Cnr3OutputCacheManager&>(d->output_cache);

    Cnr3OutputCacheDebugSnapshot snapshot;

    if (!cnr3_output_cache_get_debug_snapshot(cache, snapshot)) {
        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_debug_print_output_cache_summary # SNAPSHOT-UNAVAILABLE # instance=%d # where=\"%s\"\n",
            d->instance_id,
            where
        );

        return;
    }

    const Cnr3OutputCacheStats& stats = snapshot.stats;

    const int64_t cache_ref_balance =
        stats.cache_addframeref_total -
        stats.cache_freeframe_total;

    const int64_t lookup_ref_balance =
        stats.lookup_owned_ref_acquired_total -
        stats.lookup_owned_ref_released_total -
        stats.lookup_owned_ref_transferred_total;

    cnr3_debug_printf(
        d->debug,
        "output-cache # cnr3_debug_print_output_cache_summary # SUMMARY # "
        "instance=%d # where=\"%s\" # cms_phase=CMS06 # proving_store_prune_active=1 # "
        "output_authoritative=0 # active_ceiling=%d # non_checkpoint_count=%llu # "
        "checkpoint_count=%llu # total_cached_frame_count=%llu # highest_cached_frame_number=%d # "
        "has_pinned_checkpoints=%d # total_pin_count=%lld # invariants_ok=%d # "
        "integrity_errors=%lld # validation_attempts=%lld # validation_successes=%lld # "
        "validation_failures=%lld # ref_balance_errors=%lld # addframeref_total=%lld # "
        "freeframe_total=%lld # balance=%lld # cache_hits_at_arAllFramesReady=%lld # "
        "cache_misses=%lld # lookup_ref_acquired=%lld # lookup_ref_released=%lld # "
        "lookup_ref_transferred=%lld # lookup_ref_balance=%lld # cache_lookup_attempts=%lld # "
        "cache_lookup_failures=%lld # cache_lookup_invalid_input_errors=%lld # "
        "cache_lookup_pool_inconsistency_errors=%lld # cache_lookup_index_inconsistency_errors=%lld # "
        "cache_lookup_null_frame_errors=%lld # store_attempts=%lld # store_successes=%lld # "
        "store_failures=%lld # non_checkpoint_store_successes=%lld # checkpoint_store_successes=%lld # "
        "store_invalid_input_errors=%lld # store_add_ref_failures=%lld # "
        "store_pool_inconsistency_errors=%lld # store_index_inconsistency_errors=%lld # "
        "store_post_validation_failures=%lld # duplicate_skipped_already_cached=%lld # "
        "duplicate_computed_but_discarded=%lld # legacy_duplicate_rejections=%lld # "
        "ceiling_hard_aborts=%lld # remove_attempts=%lld # remove_successes=%lld # "
        "remove_failures=%lld # non_checkpoint_remove_successes=%lld # checkpoint_remove_successes=%lld # "
        "remove_not_found_failures=%lld # remove_invalid_input_errors=%lld # "
        "remove_pinned_checkpoint_rejections=%lld # remove_pool_inconsistency_errors=%lld # "
        "remove_index_inconsistency_errors=%lld # remove_post_validation_failures=%lld # "
        "clear_attempts=%lld # clear_successes=%lld # clear_failures=%lld # "
        "clear_null_vsapi_failures=%lld # prune_after_store_attempts=%lld # "
        "prune_after_store_successes=%lld # prune_after_store_failures=%lld # "
        "prune_after_store_non_checkpoint_failures=%lld # prune_after_store_checkpoint_failures=%lld # "
        "prune_after_store_post_validation_failures=%lld # non_checkpoint_prune_attempts=%lld # "
        "non_checkpoint_prune_runs=%lld # non_checkpoint_prune_skipped_below_overflow=%lld # "
        "non_checkpoint_prune_skipped_in_hot_zone=%lld # non_checkpoint_prune_removed_frames=%lld # "
        "non_checkpoint_prune_remove_failures=%lld # non_checkpoint_prune_post_validation_failures=%lld # "
        "checkpoint_prune_attempts=%lld # checkpoint_prune_runs=%lld # "
        "checkpoint_prune_skipped_below_max_retain=%lld # checkpoint_prune_skipped_frame_zero=%lld # "
        "checkpoint_prune_skipped_pinned=%lld # checkpoint_prune_skipped_in_hot_zone=%lld # "
        "checkpoint_prune_no_eligible_frames=%lld # checkpoint_prune_removed_frames=%lld # "
        "checkpoint_prune_remove_failures=%lld # checkpoint_prune_post_validation_failures=%lld # "
        "prune_no_candidate_exists=%lld # checkpoint_pin_attempts=%lld # "
        "checkpoint_pin_successes=%lld # checkpoint_pin_failures=%lld # "
        "checkpoint_find_and_pin_attempts=%lld # checkpoint_find_and_pin_successes=%lld # "
        "checkpoint_find_and_pin_failures=%lld # checkpoint_find_and_pin_no_prior_checkpoint_failures=%lld # "
        "checkpoint_find_and_pin_null_frame_failures=%lld # checkpoint_unpin_attempts=%lld # "
        "checkpoint_unpin_successes=%lld # checkpoint_unpin_failures=%lld # "
        "checkpoint_unpin_underflow_errors=%lld # hot_zone_updates_at_arInitial=%lld # "
        "hot_zone_new_zone_requests=%lld # hot_zone_allocations=%lld # hot_zone_hits=%lld # "
        "hot_zone_slides=%lld # hot_zone_merges=%lld # hot_zone_retirements=%lld # "
        "hot_zone_max_active_observed=%lld # last_hot_zone_action=%s # "
        "last_hot_zone_frame=%d # last_hot_zone_index=%d # last_hot_zone_before=[%d..%d] # "
        "last_hot_zone_after=[%d..%d] # last_hot_zone_active_count=%d\n",
        d->instance_id,
        where,
        snapshot.active_ceiling,
        static_cast<unsigned long long>(snapshot.non_checkpoint_count),
        static_cast<unsigned long long>(snapshot.checkpoint_count),
        static_cast<unsigned long long>(snapshot.total_cached_frame_count),
        snapshot.highest_cached_frame_number,
        snapshot.has_pinned_checkpoints ? 1 : 0,
        static_cast<long long>(snapshot.total_pin_count),
        snapshot.invariants_ok ? 1 : 0,
        static_cast<long long>(stats.cache_integrity_errors),
        static_cast<long long>(stats.cache_validation_attempts),
        static_cast<long long>(stats.cache_validation_successes),
        static_cast<long long>(stats.cache_validation_failures),
        static_cast<long long>(stats.cache_validation_ref_balance_errors),
        static_cast<long long>(stats.cache_addframeref_total),
        static_cast<long long>(stats.cache_freeframe_total),
        static_cast<long long>(cache_ref_balance),
        static_cast<long long>(stats.cache_hits_at_arAllFramesReady),
        static_cast<long long>(stats.cache_misses),
        static_cast<long long>(stats.lookup_owned_ref_acquired_total),
        static_cast<long long>(stats.lookup_owned_ref_released_total),
        static_cast<long long>(stats.lookup_owned_ref_transferred_total),
        static_cast<long long>(lookup_ref_balance),
        static_cast<long long>(stats.cache_lookup_attempts),
        static_cast<long long>(stats.cache_lookup_failures),
        static_cast<long long>(stats.cache_lookup_invalid_input_errors),
        static_cast<long long>(stats.cache_lookup_pool_inconsistency_errors),
        static_cast<long long>(stats.cache_lookup_index_inconsistency_errors),
        static_cast<long long>(stats.cache_lookup_null_frame_errors),
        static_cast<long long>(stats.cache_store_attempts),
        static_cast<long long>(stats.cache_store_successes),
        static_cast<long long>(stats.cache_store_failures),
        static_cast<long long>(stats.non_checkpoint_store_successes),
        static_cast<long long>(stats.checkpoint_store_successes),
        static_cast<long long>(stats.cache_store_invalid_input_errors),
        static_cast<long long>(stats.cache_store_add_ref_failures),
        static_cast<long long>(stats.cache_store_pool_inconsistency_errors),
        static_cast<long long>(stats.cache_store_index_inconsistency_errors),
        static_cast<long long>(stats.cache_store_post_validation_failures),
        static_cast<long long>(stats.store_skipped_already_cached),
        static_cast<long long>(stats.duplicate_store_computed_but_discarded),
        static_cast<long long>(stats.cache_store_duplicate_rejections),
        static_cast<long long>(stats.cache_ceiling_hard_aborts),
        static_cast<long long>(stats.cache_remove_attempts),
        static_cast<long long>(stats.cache_remove_successes),
        static_cast<long long>(stats.cache_remove_failures),
        static_cast<long long>(stats.non_checkpoint_remove_successes),
        static_cast<long long>(stats.checkpoint_remove_successes),
        static_cast<long long>(stats.cache_remove_not_found_failures),
        static_cast<long long>(stats.cache_remove_invalid_input_errors),
        static_cast<long long>(stats.cache_remove_pinned_checkpoint_rejections),
        static_cast<long long>(stats.cache_remove_pool_inconsistency_errors),
        static_cast<long long>(stats.cache_remove_index_inconsistency_errors),
        static_cast<long long>(stats.cache_remove_post_validation_failures),
        static_cast<long long>(stats.cache_clear_attempts),
        static_cast<long long>(stats.cache_clear_successes),
        static_cast<long long>(stats.cache_clear_failures),
        static_cast<long long>(stats.cache_clear_null_vsapi_failures),
        static_cast<long long>(stats.prune_after_store_attempts),
        static_cast<long long>(stats.prune_after_store_successes),
        static_cast<long long>(stats.prune_after_store_failures),
        static_cast<long long>(stats.prune_after_store_non_checkpoint_failures),
        static_cast<long long>(stats.prune_after_store_checkpoint_failures),
        static_cast<long long>(stats.prune_after_store_post_validation_failures),
        static_cast<long long>(stats.non_checkpoint_prune_attempts),
        static_cast<long long>(stats.non_checkpoint_prune_runs),
        static_cast<long long>(stats.non_checkpoint_prune_skipped_below_overflow),
        static_cast<long long>(stats.non_checkpoint_prune_skipped_in_hot_zone),
        static_cast<long long>(stats.non_checkpoint_prune_removed_frames),
        static_cast<long long>(stats.non_checkpoint_prune_remove_failures),
        static_cast<long long>(stats.non_checkpoint_prune_post_validation_failures),
        static_cast<long long>(stats.checkpoint_prune_attempts),
        static_cast<long long>(stats.checkpoint_prune_runs),
        static_cast<long long>(stats.checkpoint_prune_skipped_below_max_retain),
        static_cast<long long>(stats.checkpoint_prune_skipped_frame_zero),
        static_cast<long long>(stats.checkpoint_prune_skipped_pinned),
        static_cast<long long>(stats.checkpoint_prune_skipped_in_hot_zone),
        static_cast<long long>(stats.checkpoint_prune_no_eligible_frames),
        static_cast<long long>(stats.checkpoint_prune_removed_frames),
        static_cast<long long>(stats.checkpoint_prune_remove_failures),
        static_cast<long long>(stats.checkpoint_prune_post_validation_failures),
        static_cast<long long>(stats.prune_no_candidate_exists),
        static_cast<long long>(stats.checkpoint_pin_attempts),
        static_cast<long long>(stats.checkpoint_pin_successes),
        static_cast<long long>(stats.checkpoint_pin_failures),
        static_cast<long long>(stats.checkpoint_find_and_pin_attempts),
        static_cast<long long>(stats.checkpoint_find_and_pin_successes),
        static_cast<long long>(stats.checkpoint_find_and_pin_failures),
        static_cast<long long>(stats.checkpoint_find_and_pin_no_prior_checkpoint_failures),
        static_cast<long long>(stats.checkpoint_find_and_pin_null_frame_failures),
        static_cast<long long>(stats.checkpoint_unpin_attempts),
        static_cast<long long>(stats.checkpoint_unpin_successes),
        static_cast<long long>(stats.checkpoint_unpin_failures),
        static_cast<long long>(stats.checkpoint_unpin_underflow_errors),
        static_cast<long long>(stats.hot_zone_updates_at_arInitial),
        static_cast<long long>(stats.hot_zone_new_zone_requests),
        static_cast<long long>(stats.hot_zone_allocations),
        static_cast<long long>(stats.hot_zone_hits),
        static_cast<long long>(stats.hot_zone_slides),
        static_cast<long long>(stats.hot_zone_merges),
        static_cast<long long>(stats.hot_zone_retirements),
        static_cast<long long>(stats.hot_zone_max_active_observed),
        cnr3_hot_zone_event_name(stats.hot_zone_last_event_kind),
        stats.hot_zone_last_event_frame,
        stats.hot_zone_last_event_zone_index,
        stats.hot_zone_last_event_old_low,
        stats.hot_zone_last_event_old_high,
        stats.hot_zone_last_event_new_low,
        stats.hot_zone_last_event_new_high,
        stats.hot_zone_last_event_active_count
    );
}

static void cnr3_debug_print_output_cache_hot_zone_trace(
    const Cnr3Data* d,
    int frame_number
) {
    if (d == nullptr || !d->debug || frame_number < 0) {
        return;
    }

    Cnr3OutputCacheManager& cache =
        const_cast<Cnr3OutputCacheManager&>(d->output_cache);

    Cnr3OutputCacheDebugSnapshot snapshot;

    if (!cnr3_output_cache_get_debug_snapshot(cache, snapshot)) {
        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_output_cache_update_hot_zones # SNAPSHOT-UNAVAILABLE # instance=%d # frame=%d\n",
            d->instance_id,
            frame_number
        );

        return;
    }

    const Cnr3OutputCacheStats& stats = snapshot.stats;

    cnr3_debug_printf(
        d->debug,
        "output-cache # cnr3_output_cache_update_hot_zones # %s # instance=%d # "
        "frame=%d # zone=%d # before=[%d..%d] # after=[%d..%d] # "
        "active_zones=%d # updates_at_arInitial=%lld # new_zone_requests=%lld # "
        "allocations=%lld # hits=%lld # slides=%lld # merges=%lld # retirements=%lld # "
        "max_active_observed=%lld\n",
        cnr3_hot_zone_event_name(stats.hot_zone_last_event_kind),
        d->instance_id,
        frame_number,
        stats.hot_zone_last_event_zone_index,
        stats.hot_zone_last_event_old_low,
        stats.hot_zone_last_event_old_high,
        stats.hot_zone_last_event_new_low,
        stats.hot_zone_last_event_new_high,
        stats.hot_zone_last_event_active_count,
        static_cast<long long>(stats.hot_zone_updates_at_arInitial),
        static_cast<long long>(stats.hot_zone_new_zone_requests),
        static_cast<long long>(stats.hot_zone_allocations),
        static_cast<long long>(stats.hot_zone_hits),
        static_cast<long long>(stats.hot_zone_slides),
        static_cast<long long>(stats.hot_zone_merges),
        static_cast<long long>(stats.hot_zone_retirements),
        static_cast<long long>(stats.hot_zone_max_active_observed)
    );
}

static bool cnr3_should_print_frame_output_cache_summary(
    const Cnr3Data* d,
    int frame_number
) {
    /*
        Throttle full per-frame output-cache summaries. Compact frame traces
        still print every frame while CMS05 store/prune proving is active.
    */

    if (d == nullptr || frame_number < 0) {
        return false;
    }

    if (frame_number <= 1) {
        return true;
    }

    if ((frame_number % 100) == 0) {
        return true;
    }

    if (d->vi != nullptr) {
        const int final_frame = d->vi->numFrames - 1;

        if (
            frame_number == final_frame ||
            frame_number == final_frame - 1
            ) {
            return true;
        }
    }

    return false;
}

static void cnr3_debug_print_output_cache_frame_trace(
    const Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok,
    bool output_cache_prune_ok
) {
    /*
        Compact per-frame trace for long runs. Keep this to one stderr line so
        cache/refcount failures are grep-friendly and human-scannable.
    */

    if (d == nullptr || !d->debug || frame_number < 0) {
        return;
    }

    Cnr3OutputCacheManager& cache =
        const_cast<Cnr3OutputCacheManager&>(d->output_cache);

    Cnr3OutputCacheDebugSnapshot snapshot;

    if (!cnr3_output_cache_get_debug_snapshot(cache, snapshot)) {
        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_debug_print_output_cache_frame_trace # SNAPSHOT-UNAVAILABLE # instance=%d # frame=%d\n",
            d->instance_id,
            frame_number
        );

        return;
    }

    const Cnr3OutputCacheStats& stats = snapshot.stats;

    const int64_t cache_ref_balance =
        stats.cache_addframeref_total -
        stats.cache_freeframe_total;

    const int64_t lookup_ref_balance =
        stats.lookup_owned_ref_acquired_total -
        stats.lookup_owned_ref_released_total -
        stats.lookup_owned_ref_transferred_total;

    cnr3_debug_printf(
        d->debug,
        "output-cache # cnr3_debug_print_output_cache_frame_trace # AFTER-STORE-PRUNE # "
        "instance=%d # frame=%d # store_ok=%d # prune_ok=%d # cached=%llu # "
        "non_checkpoint=%llu # checkpoint=%llu # highest=%d # addframeref_total=%lld # "
        "freeframe_total=%lld # balance=%lld # cache_hits_at_arAllFramesReady=%lld # "
        "cache_misses=%lld # lookup_ref_acquired=%lld # lookup_ref_released=%lld # "
        "lookup_ref_transferred=%lld # lookup_ref_balance=%lld # validation_attempts=%lld # "
        "validation_successes=%lld # validation_failures=%lld # integrity_errors=%lld # "
        "ref_balance_errors=%lld # store_failures=%lld # prune_after_store_failures=%lld # "
        "hot_zone_updates_at_arInitial=%lld # hot_zone_slides=%lld # "
        "hot_zone_max_active_observed=%lld # last_hot_zone_action=%s # "
        "last_hot_zone_before=[%d..%d] # last_hot_zone_after=[%d..%d]\n",
        d->instance_id,
        frame_number,
        output_cache_store_ok ? 1 : 0,
        output_cache_prune_ok ? 1 : 0,
        static_cast<unsigned long long>(snapshot.total_cached_frame_count),
        static_cast<unsigned long long>(snapshot.non_checkpoint_count),
        static_cast<unsigned long long>(snapshot.checkpoint_count),
        snapshot.highest_cached_frame_number,
        static_cast<long long>(stats.cache_addframeref_total),
        static_cast<long long>(stats.cache_freeframe_total),
        static_cast<long long>(cache_ref_balance),
        static_cast<long long>(stats.cache_hits_at_arAllFramesReady),
        static_cast<long long>(stats.cache_misses),
        static_cast<long long>(stats.lookup_owned_ref_acquired_total),
        static_cast<long long>(stats.lookup_owned_ref_released_total),
        static_cast<long long>(stats.lookup_owned_ref_transferred_total),
        static_cast<long long>(lookup_ref_balance),
        static_cast<long long>(stats.cache_validation_attempts),
        static_cast<long long>(stats.cache_validation_successes),
        static_cast<long long>(stats.cache_validation_failures),
        static_cast<long long>(stats.cache_integrity_errors),
        static_cast<long long>(stats.cache_validation_ref_balance_errors),
        static_cast<long long>(stats.cache_store_failures),
        static_cast<long long>(stats.prune_after_store_failures),
        static_cast<long long>(stats.hot_zone_updates_at_arInitial),
        static_cast<long long>(stats.hot_zone_slides),
        static_cast<long long>(stats.hot_zone_max_active_observed),
        cnr3_hot_zone_event_name(stats.hot_zone_last_event_kind),
        stats.hot_zone_last_event_old_low,
        stats.hot_zone_last_event_old_high,
        stats.hot_zone_last_event_new_low,
        stats.hot_zone_last_event_new_high
    );
}

struct Cnr3ForDebugOnlyFrameDifferenceStats {
    int compared_planes = 0;
    int compared_rows = 0;
    int64_t samples_compared = 0;
    int64_t samples_different = 0;
    int64_t sum_abs_sample_diff = 0;
    int max_abs_sample_diff = 0;
};

struct Cnr3ForDebugOnlyRecoveryDifferenceSummary {
    int64_t frames_checked = 0;
    int64_t frames_measured = 0;
    int64_t frames_skipped_no_cached_output = 0;
    int64_t frames_exact_match = 0;
    int64_t frames_with_differences = 0;
    int64_t structural_failures = 0;
    int64_t lookup_refs_released = 0;
    int64_t samples_compared = 0;
    int64_t samples_different = 0;
    int64_t sum_abs_sample_diff = 0;
    int max_abs_sample_diff = 0;
};

static std::mutex g_cnr3_for_debug_only_recovery_difference_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyRecoveryDifferenceSummary
> g_cnr3_for_debug_only_recovery_difference_summaries;

static void cnr3_for_debug_only_record_recovery_difference_summary(
    const Cnr3Data* d,
    bool lookup_ok,
    bool measurement_ok,
    const Cnr3ForDebugOnlyFrameDifferenceStats& frame_stats
) {
    /*
        Temporary CMS02-G.10D.6 proof summary.

        This accumulates per-instance difference-measurement totals for the
        current proof run. It is diagnostic-only and does not affect cache
        ownership, output authority, or returned frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_STORE_DIFFERENCE_MEASUREMENT_PROOF) {
        (void)d;
        (void)lookup_ok;
        (void)measurement_ok;
        (void)frame_stats;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_recovery_difference_summary_mutex
        );

        Cnr3ForDebugOnlyRecoveryDifferenceSummary& summary =
            g_cnr3_for_debug_only_recovery_difference_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (!lookup_ok) {
            ++summary.frames_skipped_no_cached_output;
            return;
        }

        ++summary.lookup_refs_released;

        if (!measurement_ok) {
            ++summary.structural_failures;
            return;
        }

        ++summary.frames_measured;

        if (frame_stats.samples_different == 0) {
            ++summary.frames_exact_match;
        }
        else {
            ++summary.frames_with_differences;
        }

        summary.samples_compared += frame_stats.samples_compared;
        summary.samples_different += frame_stats.samples_different;
        summary.sum_abs_sample_diff += frame_stats.sum_abs_sample_diff;

        if (frame_stats.max_abs_sample_diff > summary.max_abs_sample_diff) {
            summary.max_abs_sample_diff = frame_stats.max_abs_sample_diff;
        }
    }
}

static void cnr3_for_debug_only_print_recovery_difference_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-G.10D.6 proof summary print.

        Print one final scan-friendly line so the enabled proof can be audited
        without counting per-frame measurement lines by hand.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_STORE_DIFFERENCE_MEASUREMENT_PROOF) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyRecoveryDifferenceSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_recovery_difference_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_recovery_difference_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_recovery_difference_summaries.end()
                ) {
                summary = found->second;
            }
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_recovery_difference_summary # FOR-DEBUG-ONLY-RECOVERY-STORE-DIFFERENCE-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # frames_measured=%lld # frames_skipped_no_cached_output=%lld # frames_exact_match=%lld # frames_with_differences=%lld # structural_failures=%lld # lookup_refs_released=%lld # samples_compared=%lld # samples_different=%lld # max_abs_sample_diff=%d # sum_abs_sample_diff=%lld # output_authoritative=0 # would_return_recovered_output=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.frames_measured),
            static_cast<long long>(summary.frames_skipped_no_cached_output),
            static_cast<long long>(summary.frames_exact_match),
            static_cast<long long>(summary.frames_with_differences),
            static_cast<long long>(summary.structural_failures),
            static_cast<long long>(summary.lookup_refs_released),
            static_cast<long long>(summary.samples_compared),
            static_cast<long long>(summary.samples_different),
            summary.max_abs_sample_diff,
            static_cast<long long>(summary.sum_abs_sample_diff)
        );
    }
}

static void cnr3_for_debug_only_erase_recovery_difference_summary(
    const Cnr3Data* d
) {
    /*
        Remove the per-instance proof summary when the filter instance is freed.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_STORE_DIFFERENCE_MEASUREMENT_PROOF) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_recovery_difference_summary_mutex
        );

        g_cnr3_for_debug_only_recovery_difference_summaries.erase(
            d->instance_id
        );
    }
}

struct Cnr3ForDebugOnlyRecoveryReturnDecisionSummary {
    int64_t frames_checked = 0;
    int64_t candidates_found = 0;
    int64_t frames_skipped_no_candidate = 0;
    int64_t would_be_returnable = 0;
    int64_t lookup_refs_released = 0;
    int64_t lookup_refs_transferred = 0;
    int64_t lookup_failures = 0;
    int64_t actual_recovered_returns = 0;
};

static std::mutex g_cnr3_for_debug_only_recovery_return_decision_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyRecoveryReturnDecisionSummary
> g_cnr3_for_debug_only_recovery_return_decision_summaries;

static void cnr3_for_debug_only_record_recovery_return_decision_summary(
    const Cnr3Data* d,
    bool candidate_found,
    bool lookup_ref_released,
    bool lookup_ref_transferred
) {
    /*
        Temporary CMS02-G.10D.7/G10D.9 proof summary.

        This records whether a recovery-stored cached output was available at
        the future return point, and whether the caller-owned lookup reference
        was released locally or transferred to VapourSynth.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_TRANSFER_PROOF
        ) {
        (void)d;
        (void)candidate_found;
        (void)lookup_ref_released;
        (void)lookup_ref_transferred;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_recovery_return_decision_summary_mutex
        );

        Cnr3ForDebugOnlyRecoveryReturnDecisionSummary& summary =
            g_cnr3_for_debug_only_recovery_return_decision_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (!candidate_found) {
            ++summary.frames_skipped_no_candidate;
            return;
        }

        ++summary.candidates_found;
        ++summary.would_be_returnable;

        if (lookup_ref_transferred) {
            ++summary.lookup_refs_transferred;
            ++summary.actual_recovered_returns;
        }
        else if (lookup_ref_released) {
            ++summary.lookup_refs_released;
        }
        else {
            ++summary.lookup_failures;
        }
    }
}

static void cnr3_for_debug_only_print_recovery_return_decision_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-G.10D.7/G10D.9 proof summary print.

        Print one final scan-friendly line so the enabled return proof can be
        audited without counting per-frame decision or transfer lines by hand.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_TRANSFER_PROOF
        ) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyRecoveryReturnDecisionSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_recovery_return_decision_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_recovery_return_decision_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_recovery_return_decision_summaries.end()
                ) {
                summary = found->second;
            }
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_recovery_return_decision_summary # FOR-DEBUG-ONLY-RECOVERY-RETURN-DECISION-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # candidates_found=%lld # frames_skipped_no_candidate=%lld # would_be_returnable=%lld # lookup_refs_released=%lld # lookup_refs_transferred=%lld # lookup_failures=%lld # actual_recovered_returns=%lld # output_authoritative=0 # proof_only_recovery_return_transfer=%d\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.candidates_found),
            static_cast<long long>(summary.frames_skipped_no_candidate),
            static_cast<long long>(summary.would_be_returnable),
            static_cast<long long>(summary.lookup_refs_released),
            static_cast<long long>(summary.lookup_refs_transferred),
            static_cast<long long>(summary.lookup_failures),
            static_cast<long long>(summary.actual_recovered_returns),
            CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_TRANSFER_PROOF ? 1 : 0
        );
    }
}

static void cnr3_for_debug_only_erase_recovery_return_decision_summary(
    const Cnr3Data* d
) {
    /*
        Remove the per-instance proof summary when the filter instance is freed.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_TRANSFER_PROOF
        ) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_recovery_return_decision_summary_mutex
        );

        g_cnr3_for_debug_only_recovery_return_decision_summaries.erase(
            d->instance_id
        );
    }
}

static bool cnr3_for_debug_only_probe_recovery_return_decision_dry_run(
    Cnr3Data* d,
    int frame_number,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.10D.7 proof helper.

        Look up the recovery-stored cached output that a future
        output-authoritative path could return. This dry-run releases the
        caller-owned lookup reference immediately and always lets the normal
        strict-path output continue.

        Frame 0 and any frame with no recovery-stored output yet are skipped as
        non-failures.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_DECISION_DRY_RUN) {
        (void)d;
        (void)frame_number;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            vsapi == nullptr ||
            frame_number < 0
            ) {
            return true;
        }

        const VSFrame* candidate_frame =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                frame_number,
                vsapi
            );

        const bool candidate_found =
            (candidate_frame != nullptr);

        bool lookup_ref_released = false;

        if (candidate_frame != nullptr) {
            vsapi->freeFrame(candidate_frame);
            cnr3_output_cache_note_lookup_ref_released(
                d->output_cache
            );

            candidate_frame = nullptr;
            lookup_ref_released = true;
        }

        const bool would_be_returnable =
            candidate_found &&
            lookup_ref_released;

        cnr3_for_debug_only_record_recovery_return_decision_summary(
            d,
            candidate_found,
            lookup_ref_released,
            false
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_return_decision_dry_run # FOR-DEBUG-ONLY-RECOVERY-RETURN-DECISION # instance=%d # frame=%d # candidate_lookup_ok=%d # would_be_returnable=%d # released_lookup_ref=%d # no_cached_recovery_output_is_ok=%d # actual_returned_recovered_output=0 # returned_normal_strict_output=1 # output_authoritative=0 # would_transfer_lookup_ref_to_vapoursynth=0 # mutates_old_strict=0 # proof_ok=1\n",
            d->instance_id,
            frame_number,
            candidate_found ? 1 : 0,
            would_be_returnable ? 1 : 0,
            lookup_ref_released ? 1 : 0,
            candidate_found ? 0 : 1
        );

        return true;
    }
}

static const VSFrame* cnr3_for_debug_only_probe_recovery_return_transfer_proof(
    Cnr3Data* d,
    int frame_number,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.10D.9 proof helper.

        Look up a recovery-stored cached output and transfer the caller-owned
        lookup reference to VapourSynth as the returned frame.

        This is proof-only. It does not make recovery output generally
        authoritative and must remain disabled outside a dedicated proof run.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_TRANSFER_PROOF) {
        (void)d;
        (void)frame_number;
        (void)vsapi;
        return nullptr;
    }
    else {
        if (
            d == nullptr ||
            vsapi == nullptr ||
            frame_number < 0
            ) {
            return nullptr;
        }

        const VSFrame* candidate_frame =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                frame_number,
                vsapi
            );

        const bool candidate_found =
            (candidate_frame != nullptr);

        if (candidate_frame == nullptr) {
            cnr3_for_debug_only_record_recovery_return_decision_summary(
                d,
                false,
                false,
                false
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_return_transfer_proof # FOR-DEBUG-ONLY-RECOVERY-RETURN-TRANSFER # instance=%d # frame=%d # candidate_lookup_ok=0 # transferred_lookup_ref=0 # no_cached_recovery_output_is_ok=1 # actual_returned_recovered_output=0 # returned_normal_strict_output=1 # output_authoritative=0 # proof_only_recovery_return_transfer=1 # mutates_old_strict=0 # proof_ok=1\n",
                d->instance_id,
                frame_number
            );

            return nullptr;
        }

        cnr3_output_cache_note_lookup_ref_transferred(
            d->output_cache
        );

        cnr3_for_debug_only_record_recovery_return_decision_summary(
            d,
            true,
            false,
            true
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_return_transfer_proof # FOR-DEBUG-ONLY-RECOVERY-RETURN-TRANSFER # instance=%d # frame=%d # candidate_lookup_ok=1 # transferred_lookup_ref=1 # no_cached_recovery_output_is_ok=0 # actual_returned_recovered_output=1 # returned_normal_strict_output=0 # output_authoritative=0 # proof_only_recovery_return_transfer=1 # mutates_old_strict=0 # proof_ok=1\n",
            d->instance_id,
            frame_number
        );

        return candidate_frame;
    }
}

static bool cnr3_for_debug_only_measure_frame_sample_differences(
    const Cnr3Data* d,
    int frame_number,
    const VSFrame* cached_frame,
    const VSFrame* strict_frame,
    Cnr3ForDebugOnlyFrameDifferenceStats& total_stats,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.10D.6 proof helper.

        Measure sample differences between a recovery-stored cached output and
        the normal strict-path output for the same frame. Differences are
        diagnostic data, not proof failures. Structural mismatches still fail.

        This helper does not own either frame reference.
    */

    if (
        d == nullptr ||
        d->vi == nullptr ||
        cached_frame == nullptr ||
        strict_frame == nullptr ||
        vsapi == nullptr
        ) {
        return false;
    }

    const int bytes_per_sample =
        (d->vi->format.bitsPerSample + 7) / 8;

    if (bytes_per_sample != 1 && bytes_per_sample != 2) {
        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_measure_frame_sample_differences # FOR-DEBUG-ONLY-RECOVERY-STORE-DIFFERENCE-STRUCTURAL-FAILURE # instance=%d # frame=%d # reason=unsupported-bytes-per-sample # bytes_per_sample=%d\n",
            d->instance_id,
            frame_number,
            bytes_per_sample
        );

        return false;
    }

    for (int plane = 0; plane < d->vi->format.numPlanes; ++plane) {
        const int cached_width =
            vsapi->getFrameWidth(cached_frame, plane);

        const int strict_width =
            vsapi->getFrameWidth(strict_frame, plane);

        const int cached_height =
            vsapi->getFrameHeight(cached_frame, plane);

        const int strict_height =
            vsapi->getFrameHeight(strict_frame, plane);

        if (
            cached_width != strict_width ||
            cached_height != strict_height
            ) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_measure_frame_sample_differences # FOR-DEBUG-ONLY-RECOVERY-STORE-DIFFERENCE-STRUCTURAL-FAILURE # instance=%d # frame=%d # reason=dimension-mismatch # plane=%d # cached_width=%d # strict_width=%d # cached_height=%d # strict_height=%d\n",
                d->instance_id,
                frame_number,
                plane,
                cached_width,
                strict_width,
                cached_height,
                strict_height
            );

            return false;
        }

        const uint8_t* cached_base =
            vsapi->getReadPtr(cached_frame, plane);

        const uint8_t* strict_base =
            vsapi->getReadPtr(strict_frame, plane);

        const ptrdiff_t cached_stride =
            vsapi->getStride(cached_frame, plane);

        const ptrdiff_t strict_stride =
            vsapi->getStride(strict_frame, plane);

        int64_t plane_samples_compared = 0;
        int64_t plane_samples_different = 0;
        int64_t plane_sum_abs_sample_diff = 0;
        int plane_max_abs_sample_diff = 0;
        int plane_rows_with_differences = 0;

        for (int y = 0; y < cached_height; ++y) {
            const uint8_t* cached_row =
                cached_base + static_cast<ptrdiff_t>(y) * cached_stride;

            const uint8_t* strict_row =
                strict_base + static_cast<ptrdiff_t>(y) * strict_stride;

            bool row_has_difference = false;

            for (int x = 0; x < cached_width; ++x) {
                int cached_sample = 0;
                int strict_sample = 0;

                if (bytes_per_sample == 1) {
                    cached_sample =
                        static_cast<int>(cached_row[x]);

                    strict_sample =
                        static_cast<int>(strict_row[x]);
                }
                else {
                    const uint16_t* cached_row_u16 =
                        reinterpret_cast<const uint16_t*>(cached_row);

                    const uint16_t* strict_row_u16 =
                        reinterpret_cast<const uint16_t*>(strict_row);

                    cached_sample =
                        static_cast<int>(cached_row_u16[x]);

                    strict_sample =
                        static_cast<int>(strict_row_u16[x]);
                }

                const int abs_diff =
                    cached_sample >= strict_sample
                    ? cached_sample - strict_sample
                    : strict_sample - cached_sample;

                ++plane_samples_compared;

                if (abs_diff > 0) {
                    ++plane_samples_different;
                    plane_sum_abs_sample_diff += abs_diff;
                    row_has_difference = true;

                    if (abs_diff > plane_max_abs_sample_diff) {
                        plane_max_abs_sample_diff = abs_diff;
                    }
                }
            }

            if (row_has_difference) {
                ++plane_rows_with_differences;
            }
        }

        ++total_stats.compared_planes;
        total_stats.compared_rows += cached_height;
        total_stats.samples_compared += plane_samples_compared;
        total_stats.samples_different += plane_samples_different;
        total_stats.sum_abs_sample_diff += plane_sum_abs_sample_diff;

        if (plane_max_abs_sample_diff > total_stats.max_abs_sample_diff) {
            total_stats.max_abs_sample_diff = plane_max_abs_sample_diff;
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_measure_frame_sample_differences # FOR-DEBUG-ONLY-RECOVERY-STORE-DIFFERENCE-PLANE # instance=%d # frame=%d # plane=%d # width=%d # height=%d # samples_compared=%lld # samples_different=%lld # rows_with_differences=%d # max_abs_sample_diff=%d # sum_abs_sample_diff=%lld # exact_match=%d\n",
            d->instance_id,
            frame_number,
            plane,
            cached_width,
            cached_height,
            static_cast<long long>(plane_samples_compared),
            static_cast<long long>(plane_samples_different),
            plane_rows_with_differences,
            plane_max_abs_sample_diff,
            static_cast<long long>(plane_sum_abs_sample_diff),
            plane_samples_different == 0 ? 1 : 0
        );
    }

    cnr3_debug_printf(
        d->debug,
        "output-cache # cnr3_for_debug_only_measure_frame_sample_differences # FOR-DEBUG-ONLY-RECOVERY-STORE-DIFFERENCE-MEASURED # instance=%d # frame=%d # compared_planes=%d # compared_rows=%d # samples_compared=%lld # samples_different=%lld # max_abs_sample_diff=%d # sum_abs_sample_diff=%lld # exact_match=%d # output_authoritative=0 # would_return_recovered_output=0\n",
        d->instance_id,
        frame_number,
        total_stats.compared_planes,
        total_stats.compared_rows,
        static_cast<long long>(total_stats.samples_compared),
        static_cast<long long>(total_stats.samples_different),
        total_stats.max_abs_sample_diff,
        static_cast<long long>(total_stats.sum_abs_sample_diff),
        total_stats.samples_different == 0 ? 1 : 0
    );

    return true;
}

static bool cnr3_for_debug_only_probe_recovery_store_difference_measurement(
    Cnr3Data* d,
    int frame_number,
    const VSFrame* strict_frame,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.10D.6 proof helper.

        Look up the recovery-stored cached frame for frame_number and measure
        sample differences from the normal strict-path output frame. The lookup
        reference is caller-owned and must be released on every path.

        Sample differences are expected to be possible once recovery starts from
        a checkpoint rather than the full recursive history. They are measured,
        not treated as proof failures.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_STORE_DIFFERENCE_MEASUREMENT_PROOF) {
        (void)d;
        (void)frame_number;
        (void)strict_frame;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            strict_frame == nullptr ||
            vsapi == nullptr ||
            frame_number < 0
            ) {
            return true;
        }

        const VSFrame* cached_frame =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                frame_number,
                vsapi
            );

        const bool lookup_ok =
            (cached_frame != nullptr);

        Cnr3ForDebugOnlyFrameDifferenceStats total_stats;
        bool measurement_ok = false;

        if (cached_frame != nullptr) {
            measurement_ok =
                cnr3_for_debug_only_measure_frame_sample_differences(
                    d,
                    frame_number,
                    cached_frame,
                    strict_frame,
                    total_stats,
                    vsapi
                );

            vsapi->freeFrame(cached_frame);
            cnr3_output_cache_note_lookup_ref_released(
                d->output_cache
            );

            cached_frame = nullptr;
        }

        const bool exact_match =
            (
                measurement_ok &&
                total_stats.samples_different == 0
                );

        /*
            Frame 0 and any frame not recovery-stored before this point have no
            cached recovery output to measure. That is not a proof failure. The
            proof only measures frames for which a recovery-stored cached output
            is already present before the normal strict-path duplicate store.
        */
        const bool proof_ok =
            (
                !lookup_ok ||
                measurement_ok
                );

        cnr3_for_debug_only_record_recovery_difference_summary(
            d,
            lookup_ok,
            measurement_ok,
            total_stats
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_store_difference_measurement # FOR-DEBUG-ONLY-RECOVERY-STORE-DIFFERENCE-END # instance=%d # frame=%d # lookup_ok=%d # measurement_ok=%d # measured=%d # exact_match=%d # samples_compared=%lld # samples_different=%lld # max_abs_sample_diff=%d # sum_abs_sample_diff=%lld # released_lookup_ref=%d # no_cached_recovery_output_is_ok=%d # would_return_recovered_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d->instance_id,
            frame_number,
            lookup_ok ? 1 : 0,
            measurement_ok ? 1 : 0,
            lookup_ok ? 1 : 0,
            exact_match ? 1 : 0,
            static_cast<long long>(total_stats.samples_compared),
            static_cast<long long>(total_stats.samples_different),
            total_stats.max_abs_sample_diff,
            static_cast<long long>(total_stats.sum_abs_sample_diff),
            lookup_ok ? 1 : 0,
            lookup_ok ? 0 : 1,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

static int64_t get_optional_int(
    const VSMap* in,
    const VSAPI* vsapi,
    const char* name,
    int64_t default_value
) {
    int err = 0;
    const int64_t value = vsapi->mapGetInt(in, name, 0, &err);
    return err ? default_value : value;
}

static double get_optional_float(
    const VSMap* in,
    const VSAPI* vsapi,
    const char* name,
    double default_value
) {
    int err = 0;
    const double value = vsapi->mapGetFloat(in, name, 0, &err);
    return err ? default_value : value;
}

static std::string get_optional_data_string(
    const VSMap* in,
    const VSAPI* vsapi,
    const char* name,
    const char* default_value
) {
    int err = 0;
    const char* value = vsapi->mapGetData(in, name, 0, &err);
    if (err || value == nullptr) {
        return std::string(default_value);
    }

    return std::string(value);
}

static bool validate_cnr3_format(
    const VSVideoInfo* vi,
    VSMap* out,
    const VSAPI* vsapi
) {
    if (vi == nullptr) {
        vsapi->mapSetError(out, "CNR3: internal error: video info is null.");
        return false;
    }

    /*
        API4 note:
        Do not rely on helper functions such as isConstantVideoFormat()
        being available in every vendored header set. Check the fields
        directly instead.

        In VapourSynth, variable/unknown format clips have cfUndefined.
        Variable/unknown dimensions are represented by non-positive
        width/height.
    */
    if (vi->format.colorFamily == cfUndefined) {
        vsapi->mapSetError(out, "CNR3: only constant-format video clips are supported.");
        return false;
    }

    if (vi->width <= 0 || vi->height <= 0) {
        vsapi->mapSetError(out, "CNR3: only constant-dimension video clips are supported.");
        return false;
    }

    if (vi->format.colorFamily != cfYUV) {
        vsapi->mapSetError(out, "CNR3: only YUV clips are supported.");
        return false;
    }

    if (vi->format.sampleType != stInteger) {
        vsapi->mapSetError(out, "CNR3: only integer sample clips are supported.");
        return false;
    }

    if (vi->format.bitsPerSample < 8 || vi->format.bitsPerSample > 16) {
        vsapi->mapSetError(out, "CNR3: only 8-bit to 16-bit integer clips are supported.");
        return false;
    }

    if (vi->format.numPlanes != 3) {
        vsapi->mapSetError(out, "CNR3: only 3-plane YUV clips are supported.");
        return false;
    }

    if (vi->format.subSamplingW < 0 || vi->format.subSamplingW > 1) {
        vsapi->mapSetError(out, "CNR3: unsupported horizontal chroma subsampling.");
        return false;
    }

    if (vi->format.subSamplingH < 0 || vi->format.subSamplingH > 1) {
        vsapi->mapSetError(out, "CNR3: unsupported vertical chroma subsampling.");
        return false;
    }

    return true;
}

static int scale_8bit_parameter_to_bit_depth(
    int value_8bit,
    int bits_per_sample
) {
    /*
        Public CNR3 threshold parameters use the historical 8-bit Cnr2/vscnr2
        scale. Internally, integer clips above 8-bit use proportionally scaled
        thresholds.

        Examples:
            8-bit:   35 -> 35
            10-bit:  35 -> approximately 140
            16-bit:  35 -> approximately 8995
    */
    const int peak = (1 << bits_per_sample) - 1;

    return static_cast<int>(
        (static_cast<int64_t>(value_8bit) * peak + 127) / 255
        );
}

// Frame-internal pixel/plane processing lives in
// cnr3_frame_internal_processing.cpp. vapoursynth-Cnr3.cpp keeps plugin
// lifecycle, parameter parsing, scheduling, cache orchestration, and teardown.

// -----------------------------------------------------------------------------
static void cnr3_for_debug_only_print_bounded_warmup_decision_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_warmup_decision_summary(
    const Cnr3Data* d
);

static void cnr3_for_debug_only_print_bounded_checkpoint_search_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_checkpoint_search_summary(
    const Cnr3Data* d
);

static void cnr3_for_debug_only_print_bounded_warmup_source_request_plan_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_warmup_source_request_plan_summary(
    const Cnr3Data* d
);

static void cnr3_for_debug_only_print_bounded_warmup_source_frame_set_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_warmup_source_frame_set_summary(
    const Cnr3Data* d
);

static void cnr3_for_debug_only_print_bounded_warmup_local_compute_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_warmup_local_compute_summary(
    const Cnr3Data* d
);

static void cnr3_for_debug_only_print_bounded_warmup_store_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_warmup_store_summary(
    const Cnr3Data* d
);

static void cnr3_for_debug_only_print_bounded_warmup_return_decision_summary(
    const Cnr3Data* d,
    const char* where
);

static void cnr3_for_debug_only_erase_bounded_warmup_return_decision_summary(
    const Cnr3Data* d
);

// -----------------------------------------------------------------------------
// CNR3 cache manager
// -----------------------------------------------------------------------------
static void VS_CC cnr3_free(
    void* instanceData,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    Cnr3Data* d = static_cast<Cnr3Data*>(instanceData);

    if (d != nullptr) {
        if (d->node != nullptr) {
            vsapi->freeNode(d->node);
            d->node = nullptr;
        }

        cnr3_debug_print_output_cache_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_recovery_difference_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_recovery_return_decision_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_warmup_decision_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_checkpoint_search_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_warmup_source_request_plan_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_warmup_source_frame_set_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_warmup_local_compute_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_warmup_store_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_for_debug_only_print_bounded_warmup_return_decision_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_memory_record_and_print_snapshot(
            d->memory_stats,
            d->debug,
            d->instance_id,
            "before cnr3_free cleanup"
        );

        old_cnr3_strict_cache_clear(d->old_strict_cache, vsapi);

        if (!cnr3_output_cache_clear(d->output_cache, vsapi)) {
            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, output_cache clear failed during cnr3_free.\n",
                d->instance_id
            );
        }

        cnr3_debug_print_output_cache_summary(
            d,
            "after cnr3_free output_cache clear"
        );

        cnr3_memory_record_and_print_snapshot(
            d->memory_stats,
            d->debug,
            d->instance_id,
            "after cnr3_free cache cleanup"
        );
        cnr3_memory_print_summary(
            d->memory_stats,
            d->debug,
            d->instance_id,
            "before Cnr3Data delete"
        );

        cnr3_for_debug_only_erase_recovery_difference_summary(d);
        cnr3_for_debug_only_erase_recovery_return_decision_summary(d);
        cnr3_for_debug_only_erase_bounded_warmup_decision_summary(d);
        cnr3_for_debug_only_erase_bounded_checkpoint_search_summary(d);
        cnr3_for_debug_only_erase_bounded_warmup_source_request_plan_summary(d);
        cnr3_for_debug_only_erase_bounded_warmup_source_frame_set_summary(d);
        cnr3_for_debug_only_erase_bounded_warmup_local_compute_summary(d);
        cnr3_for_debug_only_erase_bounded_warmup_store_summary(d);
        cnr3_for_debug_only_erase_bounded_warmup_return_decision_summary(d);

        delete d;
    }
}

static void cnr3_for_debug_only_force_cache_lookup_probe(
    Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-F proof hook.

        This deliberately calls the real CMS02-F find-and-addref helper after a
        successful store, then immediately releases the caller-owned lookup
        reference. It proves the atomic lookup/addFrameRef path without relying
        on VapourSynth to request the same frame twice.

        Remove or disable CNR3_FOR_DEBUG_ONLY_FORCE_CACHE_LOOKUP_PROBE after the
        CMS02-F lookup/reference path is proven.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_FORCE_CACHE_LOOKUP_PROBE) {
        (void)d;
        (void)frame_number;
        (void)output_cache_store_ok;
        (void)vsapi;
        return;
    }
    else {
        if (
            d == nullptr ||
            vsapi == nullptr ||
            frame_number < 0 ||
            !output_cache_store_ok
            ) {
            return;
        }

        const VSFrame* probe_frame =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                frame_number,
                vsapi
            );

        if (probe_frame == nullptr) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_force_cache_lookup_probe # FOR-DEBUG-ONLY-FORCE-CACHE-LOOKUP-MISS # instance=%d # frame=%d\n",
                d->instance_id,
                frame_number
            );

            return;
        }

        vsapi->freeFrame(probe_frame);

        cnr3_output_cache_note_lookup_ref_released(
            d->output_cache
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_force_cache_lookup_probe # FOR-DEBUG-ONLY-FORCE-CACHE-LOOKUP-HIT-RELEASED # instance=%d # frame=%d\n",
            d->instance_id,
            frame_number
        );
    }
}

struct Cnr3ForDebugOnlyBoundedWarmupDecisionSummary {
    int64_t frames_checked = 0;
    int64_t prior_checkpoint_available = 0;
    int64_t no_prior_checkpoint_detected = 0;
    int64_t warmup_plans_created = 0;
    int64_t warmup_start_at_zero = 0;
    int64_t warmup_start_bounded_nonzero = 0;
    int64_t warmup_forward_distance_total = 0;
    int64_t warmup_forward_distance_max = 0;
    int64_t warmup_source_range_invalid = 0;
    int64_t warmup_proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_warmup_decision_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedWarmupDecisionSummary
> g_cnr3_for_debug_only_bounded_warmup_decision_summaries;

static void cnr3_for_debug_only_record_bounded_warmup_decision_summary(
    const Cnr3Data* d,
    bool prior_checkpoint_available,
    bool warmup_plan_created,
    bool warmup_start_at_zero,
    int warmup_forward_distance,
    bool source_range_valid,
    bool proof_ok
) {
    /*
        Temporary CMS02-H.2 proof summary.

        This records bounded warm-up decision/range diagnostics only. It must
        not affect cache ownership, output authority, source requests, or
        returned frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_DECISION_SCAFFOLD) {
        (void)d;
        (void)prior_checkpoint_available;
        (void)warmup_plan_created;
        (void)warmup_start_at_zero;
        (void)warmup_forward_distance;
        (void)source_range_valid;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_decision_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedWarmupDecisionSummary& summary =
            g_cnr3_for_debug_only_bounded_warmup_decision_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (prior_checkpoint_available) {
            ++summary.prior_checkpoint_available;
            return;
        }

        ++summary.no_prior_checkpoint_detected;

        if (warmup_plan_created) {
            ++summary.warmup_plans_created;
        }

        if (warmup_start_at_zero) {
            ++summary.warmup_start_at_zero;
        }
        else {
            ++summary.warmup_start_bounded_nonzero;
        }

        summary.warmup_forward_distance_total += warmup_forward_distance;

        if (warmup_forward_distance > summary.warmup_forward_distance_max) {
            summary.warmup_forward_distance_max = warmup_forward_distance;
        }

        if (!source_range_valid) {
            ++summary.warmup_source_range_invalid;
        }

        if (!proof_ok) {
            ++summary.warmup_proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_warmup_decision_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H.2 proof summary print.

        Print one scan-friendly line so the enabled no-prior-checkpoint warm-up
        decision proof can be audited without counting per-frame lines by hand.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_DECISION_SCAFFOLD) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedWarmupDecisionSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_warmup_decision_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_warmup_decision_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_warmup_decision_summaries.end()
                ) {
                summary = found->second;
            }
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_warmup_decision_summary # FOR-DEBUG-ONLY-BOUNDED-WARMUP-DECISION-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # prior_checkpoint_available=%lld # no_prior_checkpoint_detected=%lld # warmup_plans_created=%lld # warmup_start_at_zero=%lld # warmup_start_bounded_nonzero=%lld # warmup_forward_distance_total=%lld # warmup_forward_distance_max=%lld # warmup_source_range_invalid=%lld # warmup_proof_failures=%lld # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.prior_checkpoint_available),
            static_cast<long long>(summary.no_prior_checkpoint_detected),
            static_cast<long long>(summary.warmup_plans_created),
            static_cast<long long>(summary.warmup_start_at_zero),
            static_cast<long long>(summary.warmup_start_bounded_nonzero),
            static_cast<long long>(summary.warmup_forward_distance_total),
            static_cast<long long>(summary.warmup_forward_distance_max),
            static_cast<long long>(summary.warmup_source_range_invalid),
            static_cast<long long>(summary.warmup_proof_failures)
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_warmup_decision_summary(
    const Cnr3Data* d
) {
    /*
        Remove the per-instance proof summary when the filter instance is freed.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_DECISION_SCAFFOLD) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_decision_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_warmup_decision_summaries.erase(
            d->instance_id
        );
    }
}

static void cnr3_for_debug_only_probe_bounded_warmup_decision(
    Cnr3Data* d,
    int frame_number
) {
    /*
        Temporary CMS02-H.2 proof helper.

        Detect the no-prior-checkpoint case and log the bounded warm-up range
        that a later recovery path would need.

        This does not request, retrieve, compute, store, or return frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_DECISION_SCAFFOLD) {
        (void)d;
        (void)frame_number;
        return;
    }
    else {
        if (d == nullptr || frame_number < 0) {
            return;
        }

        int checkpoint_frame_number = -1;

        const bool prior_checkpoint_available =
            cnr3_output_cache_find_nearest_checkpoint_at_or_before(
                d->output_cache,
                frame_number,
                checkpoint_frame_number
            );

        if (prior_checkpoint_available) {
            cnr3_for_debug_only_record_bounded_warmup_decision_summary(
                d,
                true,
                false,
                false,
                0,
                true,
                true
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_bounded_warmup_decision # FOR-DEBUG-ONLY-BOUNDED-WARMUP-DECISION # instance=%d # requested=%d # prior_checkpoint_available=1 # checkpoint=%d # warmup_needed=0 # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=1\n",
                d->instance_id,
                frame_number,
                checkpoint_frame_number
            );

            return;
        }

        const int warmup_start =
            std::max(
                0,
                frame_number - CNR3_RECOVERY_MAX_FORWARD_FRAMES
            );

        const int warmup_end = frame_number;
        const int warmup_forward_distance = warmup_end - warmup_start;

        const bool source_range_valid =
            (
                warmup_start >= 0 &&
                warmup_end >= warmup_start &&
                warmup_forward_distance >= 0 &&
                warmup_forward_distance <= CNR3_RECOVERY_MAX_FORWARD_FRAMES
                );

        const bool proof_ok = source_range_valid;

        cnr3_for_debug_only_record_bounded_warmup_decision_summary(
            d,
            false,
            source_range_valid,
            warmup_start == 0,
            warmup_forward_distance,
            source_range_valid,
            proof_ok
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_bounded_warmup_decision # FOR-DEBUG-ONLY-BOUNDED-WARMUP-DECISION # instance=%d # requested=%d # prior_checkpoint_available=0 # warmup_start=%d # warmup_end=%d # warmup_forward_distance=%d # max_forward=%d # bounded_by_limit=%d # would_request_source_first=%d # would_request_source_last=%d # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d->instance_id,
            frame_number,
            warmup_start,
            warmup_end,
            warmup_forward_distance,
            CNR3_RECOVERY_MAX_FORWARD_FRAMES,
            warmup_start > 0 ? 1 : 0,
            warmup_start,
            warmup_end,
            proof_ok ? 1 : 0
        );
    }
}

struct Cnr3ForDebugOnlyBoundedCheckpointSearchSummary {
    int64_t frames_checked = 0;
    int64_t frames_skipped_store_or_prune_failure = 0;
    int64_t bounded_plans_available = 0;
    int64_t bounded_warmup_needed = 0;
    int64_t unpins_attempted = 0;
    int64_t unpins_succeeded = 0;
    int64_t unpins_failed = 0;
    int64_t pin_cleanup_failures = 0;
    int64_t proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_checkpoint_search_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedCheckpointSearchSummary
> g_cnr3_for_debug_only_bounded_checkpoint_search_summaries;

static void cnr3_for_debug_only_record_bounded_checkpoint_search_summary(
    const Cnr3Data* d,
    bool skipped_store_or_prune_failure,
    bool plan_available,
    bool unpin_attempted,
    bool unpin_ok,
    bool pin_cleanup_ok,
    bool proof_ok
) {
    /*
        Temporary CMS02-H.2B proof summary.

        This records post-store bounded checkpoint-search proof diagnostics only.
        It must not affect cache ownership, output authority, source requests, or
        returned frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_CHECKPOINT_SEARCH_PROOF) {
        (void)d;
        (void)skipped_store_or_prune_failure;
        (void)plan_available;
        (void)unpin_attempted;
        (void)unpin_ok;
        (void)pin_cleanup_ok;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_checkpoint_search_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedCheckpointSearchSummary& summary =
            g_cnr3_for_debug_only_bounded_checkpoint_search_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (skipped_store_or_prune_failure) {
            ++summary.frames_skipped_store_or_prune_failure;
        }

        if (plan_available) {
            ++summary.bounded_plans_available;
        }
        else if (!skipped_store_or_prune_failure) {
            ++summary.bounded_warmup_needed;
        }

        if (unpin_attempted) {
            ++summary.unpins_attempted;

            if (unpin_ok) {
                ++summary.unpins_succeeded;
            }
            else {
                ++summary.unpins_failed;
            }
        }

        if (!pin_cleanup_ok) {
            ++summary.pin_cleanup_failures;
        }

        if (!proof_ok) {
            ++summary.proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_checkpoint_search_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H.2B proof summary print.

        Print one scan-friendly line so the enabled bounded checkpoint-search
        proof can be audited without counting per-frame lines by hand.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_CHECKPOINT_SEARCH_PROOF) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedCheckpointSearchSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_checkpoint_search_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_checkpoint_search_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_checkpoint_search_summaries.end()
                ) {
                summary = found->second;
            }
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_checkpoint_search_summary # FOR-DEBUG-ONLY-BOUNDED-CHECKPOINT-SEARCH-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # frames_skipped_store_or_prune_failure=%lld # bounded_plans_available=%lld # bounded_warmup_needed=%lld # unpins_attempted=%lld # unpins_succeeded=%lld # unpins_failed=%lld # pin_cleanup_failures=%lld # proof_failures=%lld # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.frames_skipped_store_or_prune_failure),
            static_cast<long long>(summary.bounded_plans_available),
            static_cast<long long>(summary.bounded_warmup_needed),
            static_cast<long long>(summary.unpins_attempted),
            static_cast<long long>(summary.unpins_succeeded),
            static_cast<long long>(summary.unpins_failed),
            static_cast<long long>(summary.pin_cleanup_failures),
            static_cast<long long>(summary.proof_failures)
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_checkpoint_search_summary(
    const Cnr3Data* d
) {
    /*
        Remove the per-instance proof summary when the filter instance is freed.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_CHECKPOINT_SEARCH_PROOF) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_checkpoint_search_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_checkpoint_search_summaries.erase(
            d->instance_id
        );
    }
}

static void cnr3_for_debug_only_probe_bounded_checkpoint_search(
    Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok,
    bool output_cache_prune_ok
) {
    /*
        Temporary CMS02-H.2B proof helper.

        Run after the normal output-cache store/prune path and prove that the
        bounded recovery-plan helper searches only inside the bounded checkpoint
        interval before pinning.

        This does not request, retrieve, compute, store, or return frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_CHECKPOINT_SEARCH_PROOF) {
        (void)d;
        (void)frame_number;
        (void)output_cache_store_ok;
        (void)output_cache_prune_ok;
        return;
    }
    else {
        if (d == nullptr || frame_number < 0) {
            return;
        }

        if (!output_cache_store_ok || !output_cache_prune_ok) {
            cnr3_for_debug_only_record_bounded_checkpoint_search_summary(
                d,
                true,
                false,
                false,
                true,
                true,
                false
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_bounded_checkpoint_search # FOR-DEBUG-ONLY-BOUNDED-CHECKPOINT-SEARCH # instance=%d # requested=%d # skipped_store_or_prune_failure=1 # store_ok=%d # prune_ok=%d # proof_ok=0\n",
                d->instance_id,
                frame_number,
                output_cache_store_ok ? 1 : 0,
                output_cache_prune_ok ? 1 : 0
            );

            return;
        }

        const int proof_bound =
            CNR3_FOR_DEBUG_ONLY_BOUNDED_CHECKPOINT_SEARCH_PROOF_BOUND;

        const int lower_bound =
            std::max(
                0,
                frame_number - proof_bound
            );

        const int upper_bound = frame_number;

        const int64_t pin_count_before =
            cnr3_output_cache_get_total_pin_count(d->output_cache);

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool plan_ok =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                proof_bound,
                recovery_plan
            );

        const int64_t pin_count_after_prepare =
            cnr3_output_cache_get_total_pin_count(d->output_cache);

        bool unpin_attempted = false;
        bool unpin_ok = true;

        if (plan_ok && recovery_plan.checkpoint_pinned) {
            unpin_attempted = true;
            unpin_ok =
                cnr3_output_cache_unpin_checkpoint(
                    d->output_cache,
                    recovery_plan.checkpoint_frame_number
                );
        }

        const int64_t pin_count_after_cleanup =
            cnr3_output_cache_get_total_pin_count(d->output_cache);

        const bool plan_postconditions_ok =
            (
                !plan_ok ||
                (
                    recovery_plan.valid &&
                    recovery_plan.checkpoint_pinned &&
                    recovery_plan.checkpoint_frame_number >= lower_bound &&
                    recovery_plan.checkpoint_frame_number <= upper_bound &&
                    recovery_plan.forward_frame_count >= 0 &&
                    recovery_plan.forward_frame_count <= proof_bound &&
                    pin_count_after_prepare == pin_count_before + 1
                    )
                );

        const bool no_plan_postconditions_ok =
            (
                plan_ok ||
                pin_count_after_prepare == pin_count_before
                );

        const bool cleanup_ok =
            (pin_count_after_cleanup == pin_count_before);

        const bool proof_ok =
            (
                plan_postconditions_ok &&
                no_plan_postconditions_ok &&
                cleanup_ok &&
                (!unpin_attempted || unpin_ok)
                );

        cnr3_for_debug_only_record_bounded_checkpoint_search_summary(
            d,
            false,
            plan_ok,
            unpin_attempted,
            unpin_ok,
            cleanup_ok,
            proof_ok
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_bounded_checkpoint_search # FOR-DEBUG-ONLY-BOUNDED-CHECKPOINT-SEARCH # instance=%d # requested=%d # lower_bound=%d # upper_bound=%d # proof_bound=%d # plan_available=%d # checkpoint=%d # forward=%d # bounded_warmup_needed=%d # pin_count_before=%lld # pin_count_after_prepare=%lld # unpin_attempted=%d # unpin_ok=%d # pin_count_after_cleanup=%lld # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d->instance_id,
            frame_number,
            lower_bound,
            upper_bound,
            proof_bound,
            plan_ok ? 1 : 0,
            plan_ok ? recovery_plan.checkpoint_frame_number : -1,
            plan_ok ? recovery_plan.forward_frame_count : -1,
            plan_ok ? 0 : 1,
            static_cast<long long>(pin_count_before),
            static_cast<long long>(pin_count_after_prepare),
            unpin_attempted ? 1 : 0,
            unpin_ok ? 1 : 0,
            static_cast<long long>(pin_count_after_cleanup),
            proof_ok ? 1 : 0
        );
    }
}

struct Cnr3ForDebugOnlyBoundedWarmupSourceRequestPlanSummary {
    int64_t frames_checked = 0;
    int64_t frames_skipped_store_or_prune_failure = 0;
    int64_t checkpoint_plans_available = 0;
    int64_t warmup_source_request_plans_created = 0;
    int64_t warmup_start_at_zero = 0;
    int64_t warmup_start_bounded_nonzero = 0;
    int64_t source_request_frame_count_total = 0;
    int64_t source_request_frame_count_max = 0;
    int64_t unpins_attempted = 0;
    int64_t unpins_succeeded = 0;
    int64_t unpins_failed = 0;
    int64_t pin_cleanup_failures = 0;
    int64_t source_range_invalid = 0;
    int64_t proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedWarmupSourceRequestPlanSummary
> g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summaries;

static void cnr3_for_debug_only_record_bounded_warmup_source_request_plan_summary(
    const Cnr3Data* d,
    bool skipped_store_or_prune_failure,
    bool checkpoint_plan_available,
    bool source_request_plan_created,
    bool warmup_start_at_zero,
    int source_request_frame_count,
    bool unpin_attempted,
    bool unpin_ok,
    bool pin_cleanup_ok,
    bool source_range_valid,
    bool proof_ok
) {
    /*
        Temporary CMS02-H.3 proof summary.

        This records bounded warm-up source-request-plan diagnostics only. It
        must not affect cache ownership, source requests, frame retrieval,
        computation, output authority, or returned frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_SCAFFOLD) {
        (void)d;
        (void)skipped_store_or_prune_failure;
        (void)checkpoint_plan_available;
        (void)source_request_plan_created;
        (void)warmup_start_at_zero;
        (void)source_request_frame_count;
        (void)unpin_attempted;
        (void)unpin_ok;
        (void)pin_cleanup_ok;
        (void)source_range_valid;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedWarmupSourceRequestPlanSummary& summary =
            g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (skipped_store_or_prune_failure) {
            ++summary.frames_skipped_store_or_prune_failure;
        }

        if (checkpoint_plan_available) {
            ++summary.checkpoint_plans_available;
        }

        if (source_request_plan_created) {
            ++summary.warmup_source_request_plans_created;

            if (warmup_start_at_zero) {
                ++summary.warmup_start_at_zero;
            }
            else {
                ++summary.warmup_start_bounded_nonzero;
            }

            summary.source_request_frame_count_total += source_request_frame_count;

            if (source_request_frame_count > summary.source_request_frame_count_max) {
                summary.source_request_frame_count_max = source_request_frame_count;
            }
        }

        if (unpin_attempted) {
            ++summary.unpins_attempted;

            if (unpin_ok) {
                ++summary.unpins_succeeded;
            }
            else {
                ++summary.unpins_failed;
            }
        }

        if (!pin_cleanup_ok) {
            ++summary.pin_cleanup_failures;
        }

        if (!source_range_valid) {
            ++summary.source_range_invalid;
        }

        if (!proof_ok) {
            ++summary.proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_warmup_source_request_plan_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H.3 proof summary print.

        Print one scan-friendly line so the enabled source-request-plan scaffold
        can be audited without counting per-frame lines by hand.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_SCAFFOLD) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourceRequestPlanSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summaries.end()
                ) {
                summary = found->second;
            }
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_warmup_source_request_plan_summary # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-REQUEST-PLAN-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # frames_skipped_store_or_prune_failure=%lld # checkpoint_plans_available=%lld # warmup_source_request_plans_created=%lld # warmup_start_at_zero=%lld # warmup_start_bounded_nonzero=%lld # source_request_frame_count_total=%lld # source_request_frame_count_max=%lld # unpins_attempted=%lld # unpins_succeeded=%lld # unpins_failed=%lld # pin_cleanup_failures=%lld # source_range_invalid=%lld # proof_failures=%lld # would_request_source_frames=0 # would_retrieve_source_frames=0 # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.frames_skipped_store_or_prune_failure),
            static_cast<long long>(summary.checkpoint_plans_available),
            static_cast<long long>(summary.warmup_source_request_plans_created),
            static_cast<long long>(summary.warmup_start_at_zero),
            static_cast<long long>(summary.warmup_start_bounded_nonzero),
            static_cast<long long>(summary.source_request_frame_count_total),
            static_cast<long long>(summary.source_request_frame_count_max),
            static_cast<long long>(summary.unpins_attempted),
            static_cast<long long>(summary.unpins_succeeded),
            static_cast<long long>(summary.unpins_failed),
            static_cast<long long>(summary.pin_cleanup_failures),
            static_cast<long long>(summary.source_range_invalid),
            static_cast<long long>(summary.proof_failures)
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_warmup_source_request_plan_summary(
    const Cnr3Data* d
) {
    /*
        Remove the per-instance proof summary when the filter instance is freed.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_SCAFFOLD) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_warmup_source_request_plan_summaries.erase(
            d->instance_id
        );
    }
}

static void cnr3_for_debug_only_probe_bounded_warmup_source_request_plan(
    Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok,
    bool output_cache_prune_ok
) {
    /*
        Temporary CMS02-H.3 proof helper.

        Run after the normal output-cache store/prune path. If the interval-
        bounded checkpoint-start plan is unavailable, derive the source-frame
        range a future bounded warm-up recovery path would need.

        This does not request, retrieve, hold, release, compute, store, or return
        frames. It only logs and counts the plan range.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_SCAFFOLD) {
        (void)d;
        (void)frame_number;
        (void)output_cache_store_ok;
        (void)output_cache_prune_ok;
        return;
    }
    else {
        if (d == nullptr || frame_number < 0) {
            return;
        }

        if (!output_cache_store_ok || !output_cache_prune_ok) {
            cnr3_for_debug_only_record_bounded_warmup_source_request_plan_summary(
                d,
                true,
                false,
                false,
                false,
                0,
                false,
                true,
                true,
                true,
                false
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_bounded_warmup_source_request_plan # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-REQUEST-PLAN # instance=%d # requested=%d # skipped_store_or_prune_failure=1 # store_ok=%d # prune_ok=%d # proof_ok=0\n",
                d->instance_id,
                frame_number,
                output_cache_store_ok ? 1 : 0,
                output_cache_prune_ok ? 1 : 0
            );

            return;
        }

        const int proof_bound =
            CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_PROOF_BOUND;

        const int warmup_start =
            std::max(
                0,
                frame_number - proof_bound
            );

        const int warmup_end = frame_number;

        const int source_request_first = warmup_start;
        const int source_request_last = warmup_end;
        const int source_request_frame_count =
            source_request_last - source_request_first + 1;

        const bool source_range_valid =
            (
                proof_bound >= 0 &&
                warmup_start >= 0 &&
                warmup_end >= warmup_start &&
                source_request_first == warmup_start &&
                source_request_last == warmup_end &&
                source_request_frame_count >= 1 &&
                source_request_frame_count <= proof_bound + 1
                );

        const int64_t pin_count_before =
            cnr3_output_cache_get_total_pin_count(d->output_cache);

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool checkpoint_plan_available =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                proof_bound,
                recovery_plan
            );

        const int64_t pin_count_after_prepare =
            cnr3_output_cache_get_total_pin_count(d->output_cache);

        bool unpin_attempted = false;
        bool unpin_ok = true;

        if (checkpoint_plan_available && recovery_plan.checkpoint_pinned) {
            unpin_attempted = true;
            unpin_ok =
                cnr3_output_cache_unpin_checkpoint(
                    d->output_cache,
                    recovery_plan.checkpoint_frame_number
                );
        }

        const int64_t pin_count_after_cleanup =
            cnr3_output_cache_get_total_pin_count(d->output_cache);

        const bool checkpoint_plan_postconditions_ok =
            (
                !checkpoint_plan_available ||
                (
                    recovery_plan.valid &&
                    recovery_plan.checkpoint_pinned &&
                    recovery_plan.forward_frame_count >= 0 &&
                    recovery_plan.forward_frame_count <= proof_bound &&
                    pin_count_after_prepare == pin_count_before + 1
                    )
                );

        const bool no_checkpoint_plan_postconditions_ok =
            (
                checkpoint_plan_available ||
                pin_count_after_prepare == pin_count_before
                );

        const bool pin_cleanup_ok =
            (pin_count_after_cleanup == pin_count_before);

        const bool source_request_plan_created =
            (
                !checkpoint_plan_available &&
                source_range_valid
                );

        const bool proof_ok =
            (
                checkpoint_plan_postconditions_ok &&
                no_checkpoint_plan_postconditions_ok &&
                pin_cleanup_ok &&
                (!unpin_attempted || unpin_ok) &&
                source_range_valid
                );

        cnr3_for_debug_only_record_bounded_warmup_source_request_plan_summary(
            d,
            false,
            checkpoint_plan_available,
            source_request_plan_created,
            warmup_start == 0,
            source_request_frame_count,
            unpin_attempted,
            unpin_ok,
            pin_cleanup_ok,
            source_range_valid,
            proof_ok
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_bounded_warmup_source_request_plan # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-REQUEST-PLAN # instance=%d # requested=%d # proof_bound=%d # checkpoint_plan_available=%d # checkpoint=%d # forward=%d # bounded_warmup_needed=%d # warmup_start=%d # warmup_end=%d # source_request_first=%d # source_request_last=%d # source_request_frame_count=%d # would_request_source_frames=0 # would_retrieve_source_frames=0 # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # pin_count_before=%lld # pin_count_after_prepare=%lld # unpin_attempted=%d # unpin_ok=%d # pin_count_after_cleanup=%lld # source_range_valid=%d # proof_ok=%d\n",
            d->instance_id,
            frame_number,
            proof_bound,
            checkpoint_plan_available ? 1 : 0,
            checkpoint_plan_available ? recovery_plan.checkpoint_frame_number : -1,
            checkpoint_plan_available ? recovery_plan.forward_frame_count : -1,
            checkpoint_plan_available ? 0 : 1,
            source_request_plan_created ? warmup_start : -1,
            source_request_plan_created ? warmup_end : -1,
            source_request_plan_created ? source_request_first : -1,
            source_request_plan_created ? source_request_last : -1,
            source_request_plan_created ? source_request_frame_count : 0,
            static_cast<long long>(pin_count_before),
            static_cast<long long>(pin_count_after_prepare),
            unpin_attempted ? 1 : 0,
            unpin_ok ? 1 : 0,
            static_cast<long long>(pin_count_after_cleanup),
            source_range_valid ? 1 : 0,
            proof_ok ? 1 : 0
        );
    }
}

static void cnr3_for_debug_only_probe_recovery_plan(
    Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok
) {
    /*
        Temporary CMS02-G.4 proof hook.

        This calls the real bounded recovery-plan helper after a successful
        store/prune path, then immediately unpins the selected checkpoint. It
        proves checkpoint find/pin/unpin balance without performing recovery,
        recomputation, frame generation, or output-frame replacement.

        Disable CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON after the
        recovery-plan pin/unpin path is proven.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON) {
        (void)d;
        (void)frame_number;
        (void)output_cache_store_ok;
        return;
    }
    else {
        if (
            d == nullptr ||
            frame_number < 0 ||
            !output_cache_store_ok
            ) {
            return;
        }

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool plan_ok =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES,
                recovery_plan
            );

        if (!plan_ok) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_plan # FOR-DEBUG-ONLY-RECOVERY-PLAN-NOT-AVAILABLE # instance=%d # frame=%d # max_forward=%d\n",
                d->instance_id,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES
            );

            return;
        }

        const bool unpin_ok =
            cnr3_output_cache_unpin_checkpoint(
                d->output_cache,
                recovery_plan.checkpoint_frame_number
            );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_plan # FOR-DEBUG-ONLY-RECOVERY-PLAN-PIN-UNPIN # instance=%d # requested=%d # checkpoint=%d # forward=%d # max_forward=%d # unpin_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            CNR3_RECOVERY_MAX_FORWARD_FRAMES,
            unpin_ok ? 1 : 0
        );
    }
}

static void cnr3_for_debug_only_probe_recovery_walk_skeleton(
    Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok
) {
    /*
        Temporary CMS02-G.5 proof hook.

        This prepares a bounded recovery plan and reports the frame-number range
        that a future recovery walk would process. It deliberately does not
        request source frames, recompute outputs, store recovered outputs, or
        return recovered frames.

        If a plan is obtained, the selected checkpoint must be unpinned exactly
        once before returning.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON ||
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_WALK_SKELETON
        ) {
        (void)d;
        (void)frame_number;
        (void)output_cache_store_ok;
        return;
    }
    else {
        if (
            d == nullptr ||
            frame_number < 0 ||
            !output_cache_store_ok
            ) {
            return;
        }

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool plan_ok =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES,
                recovery_plan
            );

        if (!plan_ok) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_walk_skeleton # FOR-DEBUG-ONLY-RECOVERY-WALK-NOT-AVAILABLE # instance=%d # frame=%d # max_forward=%d\n",
                d->instance_id,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES
            );

            return;
        }

        const bool has_frames_to_walk =
            (recovery_plan.forward_frame_count > 0);

        const int first_recovery_frame =
            has_frames_to_walk
            ? recovery_plan.checkpoint_frame_number + 1
            : -1;

        const int last_recovery_frame =
            has_frames_to_walk
            ? recovery_plan.requested_frame_number
            : -1;

        const bool unpin_ok =
            cnr3_output_cache_unpin_checkpoint(
                d->output_cache,
                recovery_plan.checkpoint_frame_number
            );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_walk_skeleton # FOR-DEBUG-ONLY-RECOVERY-WALK-SKELETON # instance=%d # requested=%d # checkpoint=%d # forward=%d # max_forward=%d # has_walk=%d # first_recovery_frame=%d # last_recovery_frame=%d # unpin_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            CNR3_RECOVERY_MAX_FORWARD_FRAMES,
            has_frames_to_walk ? 1 : 0,
            first_recovery_frame,
            last_recovery_frame,
            unpin_ok ? 1 : 0
        );
    }
}

static void cnr3_for_debug_only_probe_recovery_start_ref_skeleton(
    Cnr3Data* d,
    int frame_number,
    bool output_cache_store_ok,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.6 proof hook.

        This prepares a bounded recovery plan, obtains a caller-owned reference
        to the selected checkpoint output frame using the existing atomic
        lookup/addref helper, then releases that reference.

        The checkpoint pin protects the cache slot from pruning while the plan
        is active. The caller-owned addref proves the future recovery path can
        safely hold a usable checkpoint frame reference beyond the cache lookup.

        This deliberately does not request source frames, recompute outputs,
        store recovered outputs, or return recovered frames.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON ||
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_START_REF_SKELETON
        ) {
        (void)d;
        (void)frame_number;
        (void)output_cache_store_ok;
        (void)vsapi;
        return;
    }
    else {
        if (
            d == nullptr ||
            frame_number < 0 ||
            !output_cache_store_ok ||
            vsapi == nullptr
            ) {
            return;
        }

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool plan_ok =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES,
                recovery_plan
            );

        if (!plan_ok) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_start_ref_skeleton # FOR-DEBUG-ONLY-RECOVERY-START-REF-NOT-AVAILABLE # instance=%d # frame=%d # max_forward=%d\n",
                d->instance_id,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES
            );

            return;
        }

        const VSFrame* checkpoint_ref =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                recovery_plan.checkpoint_frame_number,
                vsapi
            );

        const bool checkpoint_ref_ok =
            (checkpoint_ref != nullptr);

        if (checkpoint_ref != nullptr) {
            vsapi->freeFrame(checkpoint_ref);
            cnr3_output_cache_note_lookup_ref_released(d->output_cache);
            checkpoint_ref = nullptr;
        }

        const bool unpin_ok =
            cnr3_output_cache_unpin_checkpoint(
                d->output_cache,
                recovery_plan.checkpoint_frame_number
            );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_start_ref_skeleton # FOR-DEBUG-ONLY-RECOVERY-START-REF-SKELETON # instance=%d # requested=%d # checkpoint=%d # forward=%d # max_forward=%d # checkpoint_ref_ok=%d # released=1 # unpin_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            CNR3_RECOVERY_MAX_FORWARD_FRAMES,
            checkpoint_ref_ok ? 1 : 0,
            unpin_ok ? 1 : 0
        );
    }
}

struct Cnr3ForDebugOnlyRecoverySourceRequestPlan {
    /*
        Temporary CMS02-G.7A frameData payload.

        This is per-invocation state carried from arInitial to arAllFramesReady.
        It is deliberately separate from Cnr3Data so future fmParallelRequests
        and fmParallel work does not depend on shared "current request" state.

        This skeleton records the source-frame range that arInitial requested
        for this invocation. Later phases may widen the range for recovery; this
        disabled first pass must not change runtime behaviour.
    */
    int requested_frame_number = -1;
    int first_source_frame_number = -1;
    int last_source_frame_number = -1;
    int source_frame_count = 0;
};

static Cnr3ForDebugOnlyRecoverySourceRequestPlan*
cnr3_for_debug_only_create_recovery_source_request_plan(
    Cnr3Data* d,
    int frame_number
) {
    /*
        Temporary CMS02-G.7 source-request proof hook.

        When disabled, this returns nullptr and runtime behaviour is unchanged.

        When enabled, this creates a per-invocation frameData plan. G.7C widens
        the requested source range backwards by a small compile-time bounded
        amount, then proves that arInitial requests that range and
        arAllFramesReady retrieves only frames from that same plan.

        This must not recompute outputs, store recovered outputs, return
        recovered outputs, or change output authority.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON) {
        (void)d;
        (void)frame_number;
        return nullptr;
    }
    else {
        if (d == nullptr || frame_number < 0) {
            return nullptr;
        }

        Cnr3ForDebugOnlyRecoverySourceRequestPlan* plan =
            new Cnr3ForDebugOnlyRecoverySourceRequestPlan;

        const int bounded_back_frames =
            std::max(
                0,
                CNR3_FOR_DEBUG_ONLY_RECOVERY_SOURCE_REQUEST_BACK_FRAMES
            );

        plan->requested_frame_number = frame_number;
        plan->first_source_frame_number =
            std::max(
                0,
                frame_number - bounded_back_frames
            );
        plan->last_source_frame_number = frame_number;
        plan->source_frame_count =
            plan->last_source_frame_number -
            plan->first_source_frame_number +
            1;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_create_recovery_source_request_plan # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-CREATED # instance=%d # requested=%d # first_source=%d # last_source=%d # count=%d\n",
            d->instance_id,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count
        );

        return plan;
    }
}

static void cnr3_for_debug_only_request_recovery_source_request_plan_frames(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* plan,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.7C proof helper.

        Request every source frame described by the per-invocation frameData
        plan. arAllFramesReady must later retrieve only frames that were
        requested by this same invocation's arInitial.

        This is request/retrieve scaffolding only. It must not perform recovery
        or change output authority.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)vsapi;
        return;
    }
    else {
        if (
            d == nullptr ||
            d->node == nullptr ||
            plan == nullptr ||
            frameCtx == nullptr ||
            vsapi == nullptr ||
            plan->first_source_frame_number < 0 ||
            plan->last_source_frame_number < plan->first_source_frame_number
            ) {
            return;
        }

        for (
            int source_frame_number = plan->first_source_frame_number;
            source_frame_number <= plan->last_source_frame_number;
            ++source_frame_number
            ) {
            vsapi->requestFrameFilter(
                source_frame_number,
                d->node,
                frameCtx
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_request_recovery_source_request_plan_frames # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-REQUESTED # instance=%d # requested=%d # source=%d # first_source=%d # last_source=%d # count=%d\n",
                d->instance_id,
                plan->requested_frame_number,
                source_frame_number,
                plan->first_source_frame_number,
                plan->last_source_frame_number,
                plan->source_frame_count
            );
        }
    }
}

static void cnr3_for_debug_only_destroy_recovery_source_request_plan(
    Cnr3ForDebugOnlyRecoverySourceRequestPlan*& plan
) {
    /*
        Destroy and null the per-invocation frameData plan.

        This helper deliberately takes the pointer by reference so every cleanup
        path leaves the caller's local pointer in a known null state.
    */

    delete plan;
    plan = nullptr;
}

static void cnr3_for_debug_only_trace_recovery_source_request_plan_consumed(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* plan
) {
    /*
        Temporary CMS02-G.7B proof trace.

        This proves that arAllFramesReady received the same per-invocation
        frameData plan that arInitial created. It does not change the requested
        source-frame range or runtime output behaviour.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON) {
        (void)d;
        (void)plan;
        return;
    }
    else {
        if (d == nullptr || plan == nullptr) {
            return;
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_get_frame # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-CONSUMED # instance=%d # requested=%d # first_source=%d # last_source=%d # count=%d\n",
            d->instance_id,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count
        );
    }
}

static bool cnr3_for_debug_only_retrieve_extra_source_request_plan_frames(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* plan,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.7C proof helper.

        Retrieve and immediately release source frames in the plan, excluding
        the normal requested source frame N. The normal path still retrieves N
        exactly where it did before this proof patch.

        This proves widened source-frame request/retrieve discipline without
        performing recovery or changing output authority.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            d->node == nullptr ||
            plan == nullptr ||
            frameCtx == nullptr ||
            vsapi == nullptr ||
            plan->first_source_frame_number < 0 ||
            plan->last_source_frame_number < plan->first_source_frame_number
            ) {
            return true;
        }

        for (
            int source_frame_number = plan->first_source_frame_number;
            source_frame_number <= plan->last_source_frame_number;
            ++source_frame_number
            ) {
            if (source_frame_number == plan->requested_frame_number) {
                continue;
            }

            const VSFrame* extra_source =
                vsapi->getFrameFilter(
                    source_frame_number,
                    d->node,
                    frameCtx
                );

            if (extra_source == nullptr) {
                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_retrieve_extra_source_request_plan_frames # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-EXTRA-RETRIEVE-FAILED # instance=%d # requested=%d # source=%d # first_source=%d # last_source=%d # count=%d\n",
                    d->instance_id,
                    plan->requested_frame_number,
                    source_frame_number,
                    plan->first_source_frame_number,
                    plan->last_source_frame_number,
                    plan->source_frame_count
                );

                return false;
            }

            vsapi->freeFrame(extra_source);

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_retrieve_extra_source_request_plan_frames # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-EXTRA-RETRIEVED-RELEASED # instance=%d # requested=%d # source=%d # first_source=%d # last_source=%d # count=%d\n",
                d->instance_id,
                plan->requested_frame_number,
                source_frame_number,
                plan->first_source_frame_number,
                plan->last_source_frame_number,
                plan->source_frame_count
            );
        }

        return true;
    }
}

static bool cnr3_for_debug_only_source_request_plan_covers_frame(
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* plan,
    int frame_number
) {
    /*
        Temporary CMS02-G.8 helper.

        Return whether the per-invocation frameData source-request plan covers
        frame_number. This is used only by recovery decision/walk diagnostics;
        it must not request, retrieve, compute, store, or return frames.
    */

    if (plan == nullptr || frame_number < 0) {
        return false;
    }

    return (
        frame_number >= plan->first_source_frame_number &&
        frame_number <= plan->last_source_frame_number
        );
}

static void cnr3_for_debug_only_probe_recovery_decision_walk_skeleton(
    Cnr3Data* d,
    int frame_number,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan,
    bool output_cache_store_ok,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.8A recovery decision/walk skeleton.

        This is diagnostic scaffolding only. When enabled later, it will prepare
        a bounded recovery plan, obtain/release a caller-owned checkpoint-start
        reference, log the future walk range, and log whether each frame would
        use cache or require computation.

        It must not recompute outputs, store recovered outputs, return recovered
        outputs, change output authority, or enable any parallel VapourSynth
        mode.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_DECISION_WALK_SKELETON) {
        (void)d;
        (void)frame_number;
        (void)source_request_plan;
        (void)output_cache_store_ok;
        (void)vsapi;
        return;
    }
    else {
        if (
            d == nullptr ||
            frame_number < 0 ||
            !output_cache_store_ok ||
            vsapi == nullptr
            ) {
            return;
        }

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool plan_ok =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES,
                recovery_plan
            );

        if (!plan_ok) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_decision_walk_skeleton # FOR-DEBUG-ONLY-RECOVERY-DECISION-WALK-NOT-AVAILABLE # instance=%d # requested=%d # max_forward=%d\n",
                d->instance_id,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES
            );

            return;
        }

        const VSFrame* checkpoint_ref =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                recovery_plan.checkpoint_frame_number,
                vsapi
            );

        const bool checkpoint_ref_ok =
            (checkpoint_ref != nullptr);

        if (checkpoint_ref != nullptr) {
            vsapi->freeFrame(checkpoint_ref);
            cnr3_output_cache_note_lookup_ref_released(
                d->output_cache
            );
            checkpoint_ref = nullptr;
        }

        const bool has_walk =
            (recovery_plan.forward_frame_count > 0);

        const int first_walk_frame =
            has_walk
            ? recovery_plan.checkpoint_frame_number + 1
            : -1;

        const int last_walk_frame =
            has_walk
            ? recovery_plan.requested_frame_number
            : -1;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_decision_walk_skeleton # FOR-DEBUG-ONLY-RECOVERY-DECISION-WALK-START # instance=%d # requested=%d # checkpoint=%d # forward=%d # first_walk=%d # last_walk=%d # checkpoint_ref_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            first_walk_frame,
            last_walk_frame,
            checkpoint_ref_ok ? 1 : 0
        );

        if (has_walk) {
            for (
                int walk_frame = first_walk_frame;
                walk_frame <= last_walk_frame;
                ++walk_frame
                ) {
                const bool already_cached =
                    cnr3_output_cache_contains_frame(
                        d->output_cache,
                        walk_frame
                    );

                const bool source_covered =
                    cnr3_for_debug_only_source_request_plan_covers_frame(
                        source_request_plan,
                        walk_frame
                    );

                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_probe_recovery_decision_walk_skeleton # FOR-DEBUG-ONLY-RECOVERY-DECISION-WALK-STEP # instance=%d # requested=%d # checkpoint=%d # walk_frame=%d # already_cached=%d # would_compute=%d # source_covered_by_plan=%d\n",
                    d->instance_id,
                    recovery_plan.requested_frame_number,
                    recovery_plan.checkpoint_frame_number,
                    walk_frame,
                    already_cached ? 1 : 0,
                    already_cached ? 0 : 1,
                    source_covered ? 1 : 0
                );
            }
        }

        const bool unpin_ok =
            cnr3_output_cache_unpin_checkpoint(
                d->output_cache,
                recovery_plan.checkpoint_frame_number
            );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_decision_walk_skeleton # FOR-DEBUG-ONLY-RECOVERY-DECISION-WALK-END # instance=%d # requested=%d # checkpoint=%d # checkpoint_ref_released=%d # unpin_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            checkpoint_ref_ok ? 1 : 0,
            unpin_ok ? 1 : 0
        );
    }
}

struct Cnr3ForDebugOnlyRecoverySourceFrameSetEntry {
    int frame_number = -1;
    const VSFrame* frame = nullptr;
};

struct Cnr3ForDebugOnlyRecoverySourceFrameSet {
    int requested_frame_number = -1;
    int checkpoint_frame_number = -1;
    int first_walk_frame = -1;
    int last_walk_frame = -1;
    std::vector<Cnr3ForDebugOnlyRecoverySourceFrameSetEntry> entries;
};

static void cnr3_for_debug_only_release_recovery_source_frame_set(
    const Cnr3Data* d,
    Cnr3ForDebugOnlyRecoverySourceFrameSet& source_frame_set,
    const char* reason,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.9 proof helper.

        Release all source frames held by the local per-invocation recovery
        source-frame set. This helper must be called on both normal and partial
        failure paths before returning from the proof helper.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_FRAME_SET_SKELETON) {
        (void)d;
        (void)source_frame_set;
        (void)reason;
        (void)vsapi;
        return;
    }
    else {
        if (vsapi == nullptr) {
            return;
        }

        for (
            Cnr3ForDebugOnlyRecoverySourceFrameSetEntry& entry :
            source_frame_set.entries
            ) {
            if (entry.frame == nullptr) {
                continue;
            }

            vsapi->freeFrame(entry.frame);

            cnr3_debug_printf(
                d != nullptr ? d->debug : false,
                "output-cache # cnr3_for_debug_only_release_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-RELEASED # instance=%d # reason=%s # requested=%d # checkpoint=%d # source=%d\n",
                d != nullptr ? d->instance_id : -1,
                reason != nullptr ? reason : "unknown",
                source_frame_set.requested_frame_number,
                source_frame_set.checkpoint_frame_number,
                entry.frame_number
            );

            entry.frame = nullptr;
        }

        source_frame_set.entries.clear();
    }
}

static bool cnr3_for_debug_only_source_frame_set_holds_frame(
    const Cnr3ForDebugOnlyRecoverySourceFrameSet& source_frame_set,
    int frame_number
) {
    for (
        const Cnr3ForDebugOnlyRecoverySourceFrameSetEntry& entry :
        source_frame_set.entries
        ) {
        if (entry.frame_number == frame_number && entry.frame != nullptr) {
            return true;
        }
    }

    return false;
}

static const VSFrame* cnr3_for_debug_only_find_source_frame_in_set(
    const Cnr3ForDebugOnlyRecoverySourceFrameSet& source_frame_set,
    int frame_number
) {
    for (
        const Cnr3ForDebugOnlyRecoverySourceFrameSetEntry& entry :
        source_frame_set.entries
        ) {
        if (entry.frame_number == frame_number) {
            return entry.frame;
        }
    }

    return nullptr;
}

static bool cnr3_for_debug_only_probe_recovery_compute_dry_run(
    Cnr3Data* d,
    const Cnr3OutputCacheRecoveryPlan& recovery_plan,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan,
    const Cnr3ForDebugOnlyRecoverySourceFrameSet& source_frame_set
) {
    /*
        Temporary CMS02-G.10ABC dry-run helper.

        This proves the future recovery compute orchestration shape while the
        local source-frame set is still held. It logs what recovery would need
        and what it would compute, but deliberately performs no pixel work and
        does not mutate any output-authority or old strict-streaming state.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_COMPUTE_DRY_RUN_SKELETON) {
        (void)d;
        (void)recovery_plan;
        (void)source_request_plan;
        (void)source_frame_set;
        return true;
    }
    else {
        if (
            d == nullptr ||
            source_request_plan == nullptr ||
            !recovery_plan.valid ||
            !recovery_plan.checkpoint_pinned
            ) {
            return true;
        }

        const bool has_walk =
            (recovery_plan.forward_frame_count > 0);

        const int first_walk_frame =
            has_walk
            ? recovery_plan.checkpoint_frame_number + 1
            : -1;

        const int last_walk_frame =
            has_walk
            ? recovery_plan.requested_frame_number
            : -1;

        bool proof_ok = true;
        int dry_run_step_count = 0;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_compute_dry_run # FOR-DEBUG-ONLY-RECOVERY-COMPUTE-DRY-RUN-START # instance=%d # requested=%d # checkpoint=%d # forward=%d # first_walk=%d # last_walk=%d # actual_compute=0 # output_authoritative=0 # mutates_old_strict=0\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            first_walk_frame,
            last_walk_frame
        );

        if (has_walk) {
            for (
                int walk_frame = first_walk_frame;
                walk_frame <= last_walk_frame;
                ++walk_frame
                ) {
                const bool already_cached =
                    cnr3_output_cache_contains_frame(
                        d->output_cache,
                        walk_frame
                    );

                const bool source_covered =
                    cnr3_for_debug_only_source_request_plan_covers_frame(
                        source_request_plan,
                        walk_frame
                    );

                const bool source_held =
                    cnr3_for_debug_only_source_frame_set_holds_frame(
                        source_frame_set,
                        walk_frame
                    );

                const int predecessor_frame =
                    walk_frame - 1;

                const bool predecessor_is_checkpoint =
                    (predecessor_frame == recovery_plan.checkpoint_frame_number);

                const bool predecessor_is_prior_walk_output =
                    (
                        predecessor_frame >= first_walk_frame &&
                        predecessor_frame < walk_frame
                        );

                const bool would_compute =
                    !already_cached;

                const bool step_ok =
                    source_covered &&
                    source_held &&
                    predecessor_frame >= recovery_plan.checkpoint_frame_number;

                if (!step_ok) {
                    proof_ok = false;
                }

                ++dry_run_step_count;

                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_probe_recovery_compute_dry_run # FOR-DEBUG-ONLY-RECOVERY-COMPUTE-DRY-RUN-STEP # instance=%d # requested=%d # checkpoint=%d # walk_frame=%d # predecessor=%d # predecessor_is_checkpoint=%d # predecessor_is_prior_walk_output=%d # already_cached=%d # would_compute=%d # would_need_prev_output=1 # source_covered_by_plan=%d # source_held=%d # would_allocate_output=0 # would_call_process_cnr3_frame=0 # would_store_recovered_output=0 # step_ok=%d\n",
                    d->instance_id,
                    recovery_plan.requested_frame_number,
                    recovery_plan.checkpoint_frame_number,
                    walk_frame,
                    predecessor_frame,
                    predecessor_is_checkpoint ? 1 : 0,
                    predecessor_is_prior_walk_output ? 1 : 0,
                    already_cached ? 1 : 0,
                    would_compute ? 1 : 0,
                    source_covered ? 1 : 0,
                    source_held ? 1 : 0,
                    step_ok ? 1 : 0
                );
            }
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_compute_dry_run # FOR-DEBUG-ONLY-RECOVERY-COMPUTE-DRY-RUN-END # instance=%d # requested=%d # checkpoint=%d # steps=%d # proof_ok=%d # actual_compute=0 # output_authoritative=0 # mutates_old_strict=0\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            dry_run_step_count,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

static bool cnr3_for_debug_only_probe_recovery_local_single_compute(
    Cnr3Data* d,
    const Cnr3OutputCacheRecoveryPlan& recovery_plan,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan,
    const Cnr3ForDebugOnlyRecoverySourceFrameSet& source_frame_set,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.10D.1 proof helper.

        This proves one actual local recovery computation only when the selected
        checkpoint is the immediate predecessor of the requested frame. The
        computed frame is released immediately. It is not stored, returned, or
        made output-authoritative.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_LOCAL_SINGLE_COMPUTE_PROOF) {
        (void)d;
        (void)recovery_plan;
        (void)source_request_plan;
        (void)source_frame_set;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            d->vi == nullptr ||
            source_request_plan == nullptr ||
            frameCtx == nullptr ||
            core == nullptr ||
            vsapi == nullptr ||
            !recovery_plan.valid ||
            !recovery_plan.checkpoint_pinned
            ) {
            return true;
        }

        const bool immediate_predecessor_case =
            (
                recovery_plan.forward_frame_count == 1 &&
                recovery_plan.requested_frame_number ==
                recovery_plan.checkpoint_frame_number + 1
                );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_local_single_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-SINGLE-COMPUTE-START # instance=%d # requested=%d # checkpoint=%d # forward=%d # immediate_predecessor_case=%d # output_authoritative=0 # mutates_old_strict=0\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            immediate_predecessor_case ? 1 : 0
        );

        if (!immediate_predecessor_case) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_local_single_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-SINGLE-COMPUTE-SKIP # instance=%d # requested=%d # checkpoint=%d # reason=not-immediate-predecessor-case # actual_compute=0\n",
                d->instance_id,
                recovery_plan.requested_frame_number,
                recovery_plan.checkpoint_frame_number
            );

            return true;
        }

        const bool source_covered =
            cnr3_for_debug_only_source_request_plan_covers_frame(
                source_request_plan,
                recovery_plan.requested_frame_number
            );

        const VSFrame* source_frame =
            cnr3_for_debug_only_find_source_frame_in_set(
                source_frame_set,
                recovery_plan.requested_frame_number
            );

        const bool source_held =
            (source_frame != nullptr);

        const VSFrame* checkpoint_ref =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                recovery_plan.checkpoint_frame_number,
                vsapi
            );

        const bool checkpoint_ref_ok =
            (checkpoint_ref != nullptr);

        bool proof_ok =
            source_covered &&
            source_held &&
            checkpoint_ref_ok;

        VSFrame* recovered_frame = nullptr;

        bool recovered_frame_allocated = false;

        if (proof_ok) {
            recovered_frame = vsapi->newVideoFrame(
                &d->vi->format,
                d->vi->width,
                d->vi->height,
                source_frame,
                core
            );

            recovered_frame_allocated =
                (recovered_frame != nullptr);

            if (recovered_frame == nullptr) {
                proof_ok = false;
            }
        }

        bool process_ok = false;

        if (proof_ok) {
            process_ok =
                process_cnr3_frame_with_explicit_previous_output(
                    d,
                    recovery_plan.requested_frame_number,
                    source_frame,
                    checkpoint_ref,
                    recovered_frame,
                    frameCtx,
                    vsapi
                );

            proof_ok = process_ok;
        }

        if (recovered_frame != nullptr) {
            vsapi->freeFrame(recovered_frame);
            recovered_frame = nullptr;
        }

        if (checkpoint_ref != nullptr) {
            vsapi->freeFrame(checkpoint_ref);
            cnr3_output_cache_note_lookup_ref_released(
                d->output_cache
            );
            checkpoint_ref = nullptr;
        }

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_local_single_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-SINGLE-COMPUTE-END # instance=%d # requested=%d # checkpoint=%d # source_covered=%d # source_held=%d # checkpoint_ref_ok=%d # allocated_output=%d # process_ok=%d # released_output=1 # released_checkpoint_ref=%d # would_store_recovered_output=0 # would_return_recovered_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            source_covered ? 1 : 0,
            source_held ? 1 : 0,
            checkpoint_ref_ok ? 1 : 0,
            recovered_frame_allocated ? 1 : 0,
            process_ok ? 1 : 0,
            checkpoint_ref_ok ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

static bool cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute(
    Cnr3Data* d,
    const Cnr3OutputCacheRecoveryPlan& recovery_plan,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan,
    const Cnr3ForDebugOnlyRecoverySourceFrameSet& source_frame_set,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.10D.2/G.10D.5 proof helper.

        This proves the local checkpoint-to-request recovery walk shape with
        rolling predecessor ownership. Cached walk outputs are reused through
        caller-owned lookup references. Missing walk outputs are computed
        locally through the explicit-predecessor processing boundary.

        When the G.10D.5 store proof gate is enabled, locally computed outputs
        are stored in output_cache after successful computation. They are still
        never returned or made output-authoritative in this phase.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_LOCAL_BOUNDED_WALK_COMPUTE_PROOF) {
        (void)d;
        (void)recovery_plan;
        (void)source_request_plan;
        (void)source_frame_set;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            d->vi == nullptr ||
            source_request_plan == nullptr ||
            frameCtx == nullptr ||
            core == nullptr ||
            vsapi == nullptr ||
            !recovery_plan.valid ||
            !recovery_plan.checkpoint_pinned
            ) {
            return true;
        }

        const bool has_walk =
            (recovery_plan.forward_frame_count > 0);

        const int first_walk_frame =
            has_walk
            ? recovery_plan.checkpoint_frame_number + 1
            : -1;

        const int last_walk_frame =
            has_walk
            ? recovery_plan.requested_frame_number
            : -1;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-BOUNDED-WALK-COMPUTE-START # instance=%d # requested=%d # checkpoint=%d # forward=%d # first_walk=%d # last_walk=%d # output_authoritative=0 # mutates_old_strict=0\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            first_walk_frame,
            last_walk_frame
        );

        if (!has_walk) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-BOUNDED-WALK-COMPUTE-END # instance=%d # requested=%d # checkpoint=%d # steps=0 # cached_steps=0 # computed_steps=0 # proof_ok=1 # output_authoritative=0 # mutates_old_strict=0\n",
                d->instance_id,
                recovery_plan.requested_frame_number,
                recovery_plan.checkpoint_frame_number
            );

            return true;
        }

        const VSFrame* predecessor_ref =
            cnr3_output_cache_find_frame_and_add_ref(
                d->output_cache,
                recovery_plan.checkpoint_frame_number,
                vsapi
            );

        bool predecessor_ref_is_lookup_ref =
            (predecessor_ref != nullptr);

        int predecessor_frame_number =
            recovery_plan.checkpoint_frame_number;

        int lookup_ref_released_count = 0;
        int local_output_released_count = 0;

        const auto release_predecessor_ref = [&]() {
            if (predecessor_ref == nullptr) {
                return;
            }

            const bool released_lookup_ref =
                predecessor_ref_is_lookup_ref;

            vsapi->freeFrame(predecessor_ref);

            if (released_lookup_ref) {
                cnr3_output_cache_note_lookup_ref_released(
                    d->output_cache
                );

                ++lookup_ref_released_count;
            }
            else {
                ++local_output_released_count;
            }

            predecessor_ref = nullptr;
            predecessor_ref_is_lookup_ref = false;
            };

        const bool checkpoint_ref_ok =
            (predecessor_ref != nullptr);

        bool proof_ok =
            checkpoint_ref_ok;

        int step_count = 0;
        int cached_step_count = 0;
        int computed_step_count = 0;
        int local_output_allocated_count = 0;
        int process_success_count = 0;
        int lookup_ref_acquired_count = checkpoint_ref_ok ? 1 : 0;
        int recovery_store_attempt_count = 0;
        int recovery_store_success_count = 0;
        int recovery_store_failure_count = 0;
        int recovery_store_duplicate_before_count = 0;
        int recovery_prune_attempt_count = 0;
        int recovery_prune_success_count = 0;
        int recovery_prune_failure_count = 0;

        for (
            int walk_frame = first_walk_frame;
            proof_ok && walk_frame <= last_walk_frame;
            ++walk_frame
            ) {
            ++step_count;

            const bool source_covered =
                cnr3_for_debug_only_source_request_plan_covers_frame(
                    source_request_plan,
                    walk_frame
                );

            const VSFrame* source_frame =
                cnr3_for_debug_only_find_source_frame_in_set(
                    source_frame_set,
                    walk_frame
                );

            const bool source_held =
                (source_frame != nullptr);

            const VSFrame* cached_walk_ref =
                cnr3_output_cache_find_frame_and_add_ref(
                    d->output_cache,
                    walk_frame,
                    vsapi
                );

            const bool cached_walk_ref_ok =
                (cached_walk_ref != nullptr);

            if (cached_walk_ref_ok) {
                ++lookup_ref_acquired_count;

                release_predecessor_ref();

                predecessor_ref = cached_walk_ref;
                predecessor_ref_is_lookup_ref = true;
                predecessor_frame_number = walk_frame;
                cached_walk_ref = nullptr;
                ++cached_step_count;
            }
            else {
                VSFrame* recovered_frame = nullptr;
                bool recovered_frame_allocated = false;
                bool process_ok = false;

                const bool can_compute =
                    source_covered &&
                    source_held &&
                    predecessor_ref != nullptr &&
                    predecessor_frame_number == walk_frame - 1;

                if (can_compute) {
                    recovered_frame = vsapi->newVideoFrame(
                        &d->vi->format,
                        d->vi->width,
                        d->vi->height,
                        source_frame,
                        core
                    );

                    recovered_frame_allocated =
                        (recovered_frame != nullptr);

                    if (recovered_frame_allocated) {
                        ++local_output_allocated_count;

                        process_ok =
                            process_cnr3_frame_with_explicit_previous_output(
                                d,
                                walk_frame,
                                source_frame,
                                predecessor_ref,
                                recovered_frame,
                                frameCtx,
                                vsapi
                            );
                    }
                }

                bool recovery_store_attempted = false;
                bool recovery_store_ok = true;
                bool recovery_prune_ok = true;
                bool recovery_store_duplicate_before = false;

                if (process_ok) {
                    ++process_success_count;

                    if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_LOCAL_BOUNDED_WALK_STORE_PROOF) {
                        recovery_store_attempted = true;
                        ++recovery_store_attempt_count;

                        recovery_store_duplicate_before =
                            cnr3_output_cache_contains_frame(
                                d->output_cache,
                                walk_frame
                            );

                        if (recovery_store_duplicate_before) {
                            ++recovery_store_duplicate_before_count;
                        }

                        recovery_store_ok =
                            cnr3_output_cache_store_frame(
                                d->output_cache,
                                walk_frame,
                                recovered_frame,
                                vsapi
                            );

                        if (recovery_store_ok) {
                            ++recovery_store_success_count;

                            ++recovery_prune_attempt_count;
                            recovery_prune_ok =
                                cnr3_output_cache_prune_after_store(
                                    d->output_cache,
                                    vsapi
                                );

                            if (recovery_prune_ok) {
                                ++recovery_prune_success_count;
                            }
                            else {
                                ++recovery_prune_failure_count;
                            }
                        }
                        else {
                            ++recovery_store_failure_count;
                        }

                        process_ok =
                            process_ok &&
                            recovery_store_ok &&
                            recovery_prune_ok;
                    }

                    if (process_ok) {
                        release_predecessor_ref();

                        predecessor_ref = recovered_frame;
                        predecessor_ref_is_lookup_ref = false;
                        predecessor_frame_number = walk_frame;
                        recovered_frame = nullptr;
                        ++computed_step_count;
                    }
                    else {
                        if (recovered_frame != nullptr) {
                            vsapi->freeFrame(recovered_frame);
                            recovered_frame = nullptr;
                            ++local_output_released_count;
                        }

                        proof_ok = false;
                    }
                }
                else {
                    if (recovered_frame != nullptr) {
                        vsapi->freeFrame(recovered_frame);
                        recovered_frame = nullptr;
                        ++local_output_released_count;
                    }

                    proof_ok = false;
                }

                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-BOUNDED-WALK-COMPUTE-STEP # instance=%d # requested=%d # checkpoint=%d # walk_frame=%d # predecessor=%d # cached_step=0 # computed_step=%d # source_covered=%d # source_held=%d # allocated_output=%d # process_ok=%d # recovery_store_attempted=%d # recovery_store_ok=%d # recovery_store_duplicate_before=%d # recovery_prune_ok=%d # would_return_recovered_output=0 # output_authoritative=0 # mutates_old_strict=0 # step_ok=%d\n",
                    d->instance_id,
                    recovery_plan.requested_frame_number,
                    recovery_plan.checkpoint_frame_number,
                    walk_frame,
                    walk_frame - 1,
                    process_ok ? 1 : 0,
                    source_covered ? 1 : 0,
                    source_held ? 1 : 0,
                    recovered_frame_allocated ? 1 : 0,
                    process_ok ? 1 : 0,
                    recovery_store_attempted ? 1 : 0,
                    recovery_store_ok ? 1 : 0,
                    recovery_store_duplicate_before ? 1 : 0,
                    recovery_prune_ok ? 1 : 0,
                    proof_ok ? 1 : 0
                );
            }

            if (cached_walk_ref_ok) {
                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-BOUNDED-WALK-COMPUTE-STEP # instance=%d # requested=%d # checkpoint=%d # walk_frame=%d # predecessor=%d # cached_step=1 # computed_step=0 # source_covered=%d # source_held=%d # allocated_output=0 # process_ok=0 # recovery_store_attempted=0 # recovery_store_ok=1 # recovery_store_duplicate_before=0 # recovery_prune_ok=1 # would_return_recovered_output=0 # output_authoritative=0 # mutates_old_strict=0 # step_ok=1\n",
                    d->instance_id,
                    recovery_plan.requested_frame_number,
                    recovery_plan.checkpoint_frame_number,
                    walk_frame,
                    walk_frame - 1,
                    source_covered ? 1 : 0,
                    source_held ? 1 : 0
                );
            }
        }

        release_predecessor_ref();

        const bool local_cleanup_ok =
            (
                lookup_ref_released_count == lookup_ref_acquired_count &&
                local_output_released_count == local_output_allocated_count
                );

        proof_ok =
            proof_ok &&
            local_cleanup_ok;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute # FOR-DEBUG-ONLY-RECOVERY-LOCAL-BOUNDED-WALK-COMPUTE-END # instance=%d # requested=%d # checkpoint=%d # steps=%d # cached_steps=%d # computed_steps=%d # local_outputs_allocated=%d # local_outputs_released=%d # process_successes=%d # lookup_refs_acquired=%d # lookup_refs_released=%d # local_cleanup_ok=%d # recovery_store_attempts=%d # recovery_store_successes=%d # recovery_store_failures=%d # recovery_store_duplicate_before=%d # recovery_prune_attempts=%d # recovery_prune_successes=%d # recovery_prune_failures=%d # checkpoint_ref_ok=%d # would_return_recovered_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            step_count,
            cached_step_count,
            computed_step_count,
            local_output_allocated_count,
            local_output_released_count,
            process_success_count,
            lookup_ref_acquired_count,
            lookup_ref_released_count,
            local_cleanup_ok ? 1 : 0,
            recovery_store_attempt_count,
            recovery_store_success_count,
            recovery_store_failure_count,
            recovery_store_duplicate_before_count,
            recovery_prune_attempt_count,
            recovery_prune_success_count,
            recovery_prune_failure_count,
            checkpoint_ref_ok ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

static bool cnr3_for_debug_only_probe_recovery_source_frame_set(
    Cnr3Data* d,
    int frame_number,
    const Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-G.9AB proof helper.

        This proves that a future recovery path can retrieve, hold, and release
        all source frames needed for the checkpoint-to-request walk.

        The source frames are held only in a local per-invocation structure.
        They are not stored in Cnr3Data, not shared between invocations, and not
        used to recompute outputs in this proof.

        This must not recompute outputs, store recovered outputs, return
        recovered outputs, change output authority, or enable any parallel
        VapourSynth mode.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_FRAME_SET_SKELETON) {
        (void)d;
        (void)frame_number;
        (void)source_request_plan;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            frame_number < 0 ||
            source_request_plan == nullptr ||
            frameCtx == nullptr ||
            vsapi == nullptr
            ) {
            return true;
        }

        Cnr3OutputCacheRecoveryPlan recovery_plan;

        const bool plan_ok =
            cnr3_output_cache_prepare_bounded_recovery_plan(
                d->output_cache,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES,
                recovery_plan
            );

        if (!plan_ok) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_probe_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-NOT-AVAILABLE # instance=%d # requested=%d # max_forward=%d\n",
                d->instance_id,
                frame_number,
                CNR3_RECOVERY_MAX_FORWARD_FRAMES
            );

            return true;
        }

        Cnr3ForDebugOnlyRecoverySourceFrameSet source_frame_set;

        source_frame_set.requested_frame_number =
            recovery_plan.requested_frame_number;
        source_frame_set.checkpoint_frame_number =
            recovery_plan.checkpoint_frame_number;

        const bool has_walk =
            (recovery_plan.forward_frame_count > 0);

        source_frame_set.first_walk_frame =
            has_walk
            ? recovery_plan.checkpoint_frame_number + 1
            : -1;

        source_frame_set.last_walk_frame =
            has_walk
            ? recovery_plan.requested_frame_number
            : -1;

        bool proof_ok = true;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-START # instance=%d # requested=%d # checkpoint=%d # forward=%d # first_walk=%d # last_walk=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            recovery_plan.forward_frame_count,
            source_frame_set.first_walk_frame,
            source_frame_set.last_walk_frame
        );

        if (has_walk) {
            for (
                int walk_frame = source_frame_set.first_walk_frame;
                walk_frame <= source_frame_set.last_walk_frame;
                ++walk_frame
                ) {
                const bool source_covered =
                    cnr3_for_debug_only_source_request_plan_covers_frame(
                        source_request_plan,
                        walk_frame
                    );

                if (!source_covered) {
                    cnr3_debug_printf(
                        d->debug,
                        "output-cache # cnr3_for_debug_only_probe_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-SOURCE-NOT-COVERED # instance=%d # requested=%d # checkpoint=%d # source=%d # first_source=%d # last_source=%d\n",
                        d->instance_id,
                        recovery_plan.requested_frame_number,
                        recovery_plan.checkpoint_frame_number,
                        walk_frame,
                        source_request_plan->first_source_frame_number,
                        source_request_plan->last_source_frame_number
                    );

                    proof_ok = false;
                    break;
                }

                const VSFrame* source_frame =
                    vsapi->getFrameFilter(
                        walk_frame,
                        d->node,
                        frameCtx
                    );

                if (source_frame == nullptr) {
                    cnr3_debug_printf(
                        d->debug,
                        "output-cache # cnr3_for_debug_only_probe_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-ACQUIRE-FAILED # instance=%d # requested=%d # checkpoint=%d # source=%d\n",
                        d->instance_id,
                        recovery_plan.requested_frame_number,
                        recovery_plan.checkpoint_frame_number,
                        walk_frame
                    );

                    proof_ok = false;
                    break;
                }

                source_frame_set.entries.push_back(
                    Cnr3ForDebugOnlyRecoverySourceFrameSetEntry{
                        walk_frame,
                        source_frame
                    }
                );

                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_probe_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-ACQUIRED # instance=%d # requested=%d # checkpoint=%d # source=%d # held=%llu\n",
                    d->instance_id,
                    recovery_plan.requested_frame_number,
                    recovery_plan.checkpoint_frame_number,
                    walk_frame,
                    static_cast<unsigned long long>(source_frame_set.entries.size())
                );
            }
        }

        const size_t acquired_count =
            source_frame_set.entries.size();

        if (
            proof_ok &&
            !cnr3_for_debug_only_probe_recovery_compute_dry_run(
                d,
                recovery_plan,
                source_request_plan,
                source_frame_set
            )
            ) {
            proof_ok = false;
        }

        if (
            proof_ok &&
            !cnr3_for_debug_only_probe_recovery_local_single_compute(
                d,
                recovery_plan,
                source_request_plan,
                source_frame_set,
                frameCtx,
                core,
                vsapi
            )
            ) {
            proof_ok = false;
        }

        if (
            proof_ok &&
            !cnr3_for_debug_only_probe_recovery_local_bounded_walk_compute(
                d,
                recovery_plan,
                source_request_plan,
                source_frame_set,
                frameCtx,
                core,
                vsapi
            )
            ) {
            proof_ok = false;
        }

        cnr3_for_debug_only_release_recovery_source_frame_set(
            d,
            source_frame_set,
            proof_ok ? "normal-proof-release" : "failure-release",
            vsapi
        );

        const bool unpin_ok =
            cnr3_output_cache_unpin_checkpoint(
                d->output_cache,
                recovery_plan.checkpoint_frame_number
            );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_probe_recovery_source_frame_set # FOR-DEBUG-ONLY-RECOVERY-SOURCE-FRAME-SET-END # instance=%d # requested=%d # checkpoint=%d # acquired=%llu # released=%llu # unpin_ok=%d # proof_ok=%d\n",
            d->instance_id,
            recovery_plan.requested_frame_number,
            recovery_plan.checkpoint_frame_number,
            static_cast<unsigned long long>(acquired_count),
            static_cast<unsigned long long>(acquired_count),
            unpin_ok ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok && unpin_ok;
    }
}

static void cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
    const Cnr3Data* d,
    Cnr3ForDebugOnlyRecoverySourceRequestPlan*& plan,
    const char* reason
) {
    /*
        Temporary CMS02-G.7B proof trace.

        This logs destruction of the per-invocation frameData plan before
        deleting it. The delete/null action is still performed even when the
        proof flag is false, so cleanup remains safe if the helper is reused.
    */

    if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON) {
        if (d != nullptr && plan != nullptr) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_get_frame # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-DESTROYED # instance=%d # reason=%s # requested=%d # first_source=%d # last_source=%d # count=%d\n",
                d->instance_id,
                reason != nullptr ? reason : "unknown",
                plan->requested_frame_number,
                plan->first_source_frame_number,
                plan->last_source_frame_number,
                plan->source_frame_count
            );
        }
    }
    else {
        (void)d;
        (void)reason;
    }

    cnr3_for_debug_only_destroy_recovery_source_request_plan(
        plan
    );
}

struct Cnr3ForDebugOnlyBoundedWarmupSourcePlan {
    /*
        Temporary CMS02-H.4 frameData payload.

        This is dedicated H4 per-invocation state. It deliberately does not
        reuse the G-phase recovery source-request-plan structure because H4
        proves a conservative bounded warm-up source window, not a checkpoint-
        anchored recovery source request plan.

        The lifecycle shape is the same proven arInitial -> frameData ->
        arAllFramesReady -> cleanup pattern.
    */
    int requested_frame_number = -1;
    int first_source_frame_number = -1;
    int last_source_frame_number = -1;
    int source_frame_count = 0;
};

struct Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetEntry {
    int frame_number = -1;
    const VSFrame* frame = nullptr;
};

struct Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet {
    int requested_frame_number = -1;
    int first_source_frame_number = -1;
    int last_source_frame_number = -1;
    std::vector<Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetEntry> entries;
};

struct Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetSummary {
    int64_t frames_checked = 0;
    int64_t plans_created = 0;
    int64_t plans_destroyed = 0;
    int64_t source_frames_requested_total = 0;
    int64_t source_frames_retrieved_total = 0;
    int64_t source_frames_released_total = 0;
    int64_t source_frame_count_max = 0;
    int64_t partial_acquire_failures = 0;
    int64_t source_frame_release_balance_errors = 0;
    int64_t proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetSummary
> g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summaries;

static void cnr3_for_debug_only_record_bounded_warmup_source_frame_set_summary(
    const Cnr3Data* d,
    bool plan_created,
    bool plan_destroyed,
    int source_frames_requested,
    int source_frames_retrieved,
    int source_frames_released,
    bool partial_acquire_failure,
    bool release_balance_ok,
    bool proof_ok
) {
    /*
        Temporary CMS02-H.4 proof summary.

        This records source-frame request/retrieve/release ownership diagnostics
        only. It must not affect output-cache ownership, output authority,
        computation, storage, or returned frames.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF) {
        (void)d;
        (void)plan_created;
        (void)plan_destroyed;
        (void)source_frames_requested;
        (void)source_frames_retrieved;
        (void)source_frames_released;
        (void)partial_acquire_failure;
        (void)release_balance_ok;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetSummary& summary =
            g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summaries[
                d->instance_id
            ];

        if (source_frames_requested > 0) {
            ++summary.frames_checked;
        }

        if (plan_created) {
            ++summary.plans_created;
        }

        if (plan_destroyed) {
            ++summary.plans_destroyed;
        }

        summary.source_frames_requested_total += source_frames_requested;
        summary.source_frames_retrieved_total += source_frames_retrieved;
        summary.source_frames_released_total += source_frames_released;

        if (source_frames_requested > summary.source_frame_count_max) {
            summary.source_frame_count_max = source_frames_requested;
        }

        if (partial_acquire_failure) {
            ++summary.partial_acquire_failures;
        }

        if (!release_balance_ok) {
            ++summary.source_frame_release_balance_errors;
        }

        if (!proof_ok) {
            ++summary.proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_warmup_source_frame_set_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H.4 proof summary print.

        Print one scan-friendly line so the source-frame ownership proof can be
        audited without counting per-frame request/retrieve/release lines by
        hand.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summaries.end()
                ) {
                summary = found->second;
            }
        }

        const int64_t source_frame_release_balance =
            summary.source_frames_retrieved_total -
            summary.source_frames_released_total;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_warmup_source_frame_set_summary # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-FRAME-SET-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # plans_created=%lld # plans_destroyed=%lld # source_frames_requested_total=%lld # source_frames_retrieved_total=%lld # source_frames_released_total=%lld # source_frame_release_balance=%lld # source_frame_count_max=%lld # partial_acquire_failures=%lld # source_frame_release_balance_errors=%lld # proof_failures=%lld # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.plans_created),
            static_cast<long long>(summary.plans_destroyed),
            static_cast<long long>(summary.source_frames_requested_total),
            static_cast<long long>(summary.source_frames_retrieved_total),
            static_cast<long long>(summary.source_frames_released_total),
            static_cast<long long>(source_frame_release_balance),
            static_cast<long long>(summary.source_frame_count_max),
            static_cast<long long>(summary.partial_acquire_failures),
            static_cast<long long>(summary.source_frame_release_balance_errors),
            static_cast<long long>(summary.proof_failures)
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_warmup_source_frame_set_summary(
    const Cnr3Data* d
) {
    /*
        Remove the per-instance proof summary when the filter instance is freed.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_warmup_source_frame_set_summaries.erase(
            d->instance_id
        );
    }
}

static constexpr bool cnr3_for_debug_only_bounded_warmup_source_plan_gate_enabled() {
    return
        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF ||
        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF ||
        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF;
}

static Cnr3ForDebugOnlyBoundedWarmupSourcePlan*
cnr3_for_debug_only_create_bounded_warmup_source_plan(
    Cnr3Data* d,
    int frame_number
) {
    /*
        Temporary CMS02-H arInitial plan helper.

        Create the conservative bounded warm-up source-frame request window:
            [max(0, requested - proof_bound), requested]

        H4 uses this plan to prove request/retrieve/release ownership.
        H5 uses the same source window to prove local compute.
        H6 uses a separate compute-and-store helper over the same source window.
        H7 reuses the H6 compute/store path, then performs a dry-run return decision.
        H8 reuses the same path, then performs the first proof-gated return transfer.
        H5/H6/H7 remain proof-only and must not return warm-up output.
        H8 may return only the proof-gated lookup reference and must not mutate old strict state.
    */

    if constexpr (!cnr3_for_debug_only_bounded_warmup_source_plan_gate_enabled()) {
        (void)d;
        (void)frame_number;
        return nullptr;
    }
    else {
        if (d == nullptr || frame_number < 0) {
            return nullptr;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan =
            new Cnr3ForDebugOnlyBoundedWarmupSourcePlan;

        int proof_bound =
            CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF_BOUND;

        if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF) {
            proof_bound =
                CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF_BOUND;
        }

        if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF) {
            proof_bound =
                CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_STORE_PROOF_BOUND;
        }

        if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN) {
            proof_bound =
                CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN_BOUND;
        }

        if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF) {
            proof_bound =
                CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF_BOUND;
        }

        if constexpr (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF) {
            proof_bound =
                CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF_BOUND;
        }

        proof_bound = std::max(0, proof_bound);

        plan->requested_frame_number = frame_number;
        plan->first_source_frame_number =
            std::max(
                0,
                frame_number - proof_bound
            );
        plan->last_source_frame_number = frame_number;
        plan->source_frame_count =
            plan->last_source_frame_number -
            plan->first_source_frame_number +
            1;

        cnr3_for_debug_only_record_bounded_warmup_source_frame_set_summary(
            d,
            true,
            false,
            0,
            0,
            0,
            false,
            true,
            true
        );

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_create_bounded_warmup_source_plan # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-PLAN-CREATED # instance=%d # requested=%d # first_source=%d # last_source=%d # count=%d # would_compute_warmup_outputs=%d # would_store_warmup_outputs=%d # would_return_warmup_output=0 # output_authoritative=0\n",
            d->instance_id,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count,
            (
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ) ? 1 : 0,
            (
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ) ? 1 : 0
        );

        return plan;
    }
}

static void cnr3_for_debug_only_request_bounded_warmup_source_plan_frames(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-H arInitial request helper.

        Request every source frame in the bounded warm-up source window. Any
        matching getFrameFilter() call must happen later in arAllFramesReady for
        the same callback activation.
    */

    if constexpr (!cnr3_for_debug_only_bounded_warmup_source_plan_gate_enabled()) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)vsapi;
        return;
    }
    else {
        if (
            d == nullptr ||
            d->node == nullptr ||
            plan == nullptr ||
            frameCtx == nullptr ||
            vsapi == nullptr ||
            plan->first_source_frame_number < 0 ||
            plan->last_source_frame_number < plan->first_source_frame_number
            ) {
            return;
        }

        for (
            int source_frame_number = plan->first_source_frame_number;
            source_frame_number <= plan->last_source_frame_number;
            ++source_frame_number
            ) {
            vsapi->requestFrameFilter(
                source_frame_number,
                d->node,
                frameCtx
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_request_bounded_warmup_source_plan_frames # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-REQUESTED # instance=%d # requested=%d # source=%d # first_source=%d # last_source=%d # count=%d\n",
                d->instance_id,
                plan->requested_frame_number,
                source_frame_number,
                plan->first_source_frame_number,
                plan->last_source_frame_number,
                plan->source_frame_count
            );
        }
    }
}

static void cnr3_for_debug_only_destroy_bounded_warmup_source_plan(
    Cnr3ForDebugOnlyBoundedWarmupSourcePlan*& plan
) {
    delete plan;
    plan = nullptr;
}

static void cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
    const Cnr3Data* d,
    Cnr3ForDebugOnlyBoundedWarmupSourcePlan*& plan,
    const char* reason
) {
    /*
        Destroy and null the per-invocation bounded warm-up source plan.
    */

    if constexpr (cnr3_for_debug_only_bounded_warmup_source_plan_gate_enabled()) {
        if (plan != nullptr) {
            cnr3_debug_printf(
                d != nullptr ? d->debug : false,
                "output-cache # cnr3_get_frame # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-PLAN-DESTROYED # instance=%d # reason=%s # requested=%d # first_source=%d # last_source=%d # count=%d\n",
                d != nullptr ? d->instance_id : -1,
                reason != nullptr ? reason : "unknown",
                plan->requested_frame_number,
                plan->first_source_frame_number,
                plan->last_source_frame_number,
                plan->source_frame_count
            );

            cnr3_for_debug_only_record_bounded_warmup_source_frame_set_summary(
                d,
                false,
                true,
                0,
                0,
                0,
                false,
                true,
                true
            );
        }
    }
    else {
        (void)d;
        (void)reason;
    }

    cnr3_for_debug_only_destroy_bounded_warmup_source_plan(plan);
}

static void cnr3_for_debug_only_release_bounded_warmup_source_frame_set(
    const Cnr3Data* d,
    Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet& source_frame_set,
    const char* reason,
    const VSAPI* vsapi,
    int& released_count
) {
    /*
        Temporary CMS02-H proof helper.

        Release all source frames held by the local bounded warm-up source-frame
        set. This helper must be called on both normal and failure paths before
        returning from arAllFramesReady.
    */

    released_count = 0;

    if constexpr (!cnr3_for_debug_only_bounded_warmup_source_plan_gate_enabled()) {
        (void)d;
        (void)source_frame_set;
        (void)reason;
        (void)vsapi;
        return;
    }
    else {
        if (vsapi == nullptr) {
            return;
        }

        for (
            Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetEntry& entry :
            source_frame_set.entries
            ) {
            if (entry.frame == nullptr) {
                continue;
            }

            vsapi->freeFrame(entry.frame);
            ++released_count;

            cnr3_debug_printf(
                d != nullptr ? d->debug : false,
                "output-cache # cnr3_for_debug_only_release_bounded_warmup_source_frame_set # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-FRAME-RELEASED # instance=%d # reason=%s # requested=%d # source=%d\n",
                d != nullptr ? d->instance_id : -1,
                reason != nullptr ? reason : "unknown",
                source_frame_set.requested_frame_number,
                entry.frame_number
            );

            entry.frame = nullptr;
        }

        source_frame_set.entries.clear();
    }
}

static bool cnr3_for_debug_only_retrieve_bounded_warmup_source_frames(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi,
    Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet& source_frame_set,
    int& retrieved_count,
    bool& partial_acquire_failure
) {
    /*
        Retrieve every source frame that was requested in arInitial and hold the
        frames in a local per-invocation set. The caller owns cleanup through
        cnr3_for_debug_only_release_bounded_warmup_source_frame_set().
    */

    retrieved_count = 0;
    partial_acquire_failure = false;

    if constexpr (!cnr3_for_debug_only_bounded_warmup_source_plan_gate_enabled()) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)vsapi;
        (void)source_frame_set;
        return true;
    }
    else {
        if (
            d == nullptr ||
            d->node == nullptr ||
            plan == nullptr ||
            frameCtx == nullptr ||
            vsapi == nullptr ||
            plan->first_source_frame_number < 0 ||
            plan->last_source_frame_number < plan->first_source_frame_number ||
            plan->source_frame_count <= 0
            ) {
            return true;
        }

        source_frame_set.requested_frame_number = plan->requested_frame_number;
        source_frame_set.first_source_frame_number = plan->first_source_frame_number;
        source_frame_set.last_source_frame_number = plan->last_source_frame_number;

        for (
            int source_frame_number = plan->first_source_frame_number;
            source_frame_number <= plan->last_source_frame_number;
            ++source_frame_number
            ) {
            const VSFrame* source_frame =
                vsapi->getFrameFilter(
                    source_frame_number,
                    d->node,
                    frameCtx
                );

            if (source_frame == nullptr) {
                partial_acquire_failure = true;

                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_for_debug_only_retrieve_bounded_warmup_source_frames # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-FRAME-ACQUIRE-FAILED # instance=%d # requested=%d # source=%d # first_source=%d # last_source=%d # retrieved_so_far=%d\n",
                    d->instance_id,
                    plan->requested_frame_number,
                    source_frame_number,
                    plan->first_source_frame_number,
                    plan->last_source_frame_number,
                    retrieved_count
                );

                return false;
            }

            source_frame_set.entries.push_back(
                Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetEntry{
                    source_frame_number,
                    source_frame
                }
            );

            ++retrieved_count;

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_retrieve_bounded_warmup_source_frames # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-FRAME-ACQUIRED # instance=%d # requested=%d # source=%d # held=%llu # first_source=%d # last_source=%d\n",
                d->instance_id,
                plan->requested_frame_number,
                source_frame_number,
                static_cast<unsigned long long>(source_frame_set.entries.size()),
                plan->first_source_frame_number,
                plan->last_source_frame_number
            );
        }

        return true;
    }
}

static bool cnr3_for_debug_only_retrieve_hold_release_bounded_warmup_source_frames(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-H.4 arAllFramesReady ownership proof.

        Retrieve every source frame that H4 requested in arInitial, hold the
        frames in a local per-invocation set, and release every acquired source
        frame before returning.

        This must not compute, store, or return warm-up outputs.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)vsapi;
        return true;
    }
    else {
        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet source_frame_set;
        int retrieved_count = 0;
        bool partial_acquire_failure = false;

        bool proof_ok =
            cnr3_for_debug_only_retrieve_bounded_warmup_source_frames(
                d,
                plan,
                frameCtx,
                vsapi,
                source_frame_set,
                retrieved_count,
                partial_acquire_failure
            );

        int released_count = 0;

        cnr3_for_debug_only_release_bounded_warmup_source_frame_set(
            d,
            source_frame_set,
            proof_ok ? "normal-proof-release" : "partial-failure-release",
            vsapi,
            released_count
        );

        const bool release_balance_ok =
            (retrieved_count == released_count);

        const bool all_requested_retrieved =
            (
                plan != nullptr &&
                retrieved_count == plan->source_frame_count
                );

        proof_ok =
            (
                proof_ok &&
                all_requested_retrieved &&
                release_balance_ok
                );

        cnr3_for_debug_only_record_bounded_warmup_source_frame_set_summary(
            d,
            false,
            false,
            plan != nullptr ? plan->source_frame_count : 0,
            retrieved_count,
            released_count,
            partial_acquire_failure,
            release_balance_ok,
            proof_ok
        );

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # cnr3_for_debug_only_retrieve_hold_release_bounded_warmup_source_frames # FOR-DEBUG-ONLY-BOUNDED-WARMUP-SOURCE-FRAME-SET-END # instance=%d # requested=%d # first_source=%d # last_source=%d # source_count=%d # retrieved=%d # released=%d # release_balance=%d # partial_acquire_failure=%d # would_compute_warmup_outputs=0 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d != nullptr ? d->instance_id : -1,
            plan != nullptr ? plan->requested_frame_number : -1,
            plan != nullptr ? plan->first_source_frame_number : -1,
            plan != nullptr ? plan->last_source_frame_number : -1,
            plan != nullptr ? plan->source_frame_count : 0,
            retrieved_count,
            released_count,
            retrieved_count - released_count,
            partial_acquire_failure ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

struct Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry {
    int frame_number = -1;
    const VSFrame* frame = nullptr;
};

struct Cnr3ForDebugOnlyBoundedWarmupLocalComputeSummary {
    int64_t frames_checked = 0;
    int64_t plans_seen = 0;
    int64_t source_frames_retrieved_total = 0;
    int64_t source_frames_released_total = 0;
    int64_t source_frame_release_balance_errors = 0;
    int64_t start_frame_zero_count = 0;
    int64_t start_frame_nonzero_count = 0;
    int64_t local_start_reset_copies = 0;
    int64_t local_recursive_computes = 0;
    int64_t local_outputs_allocated = 0;
    int64_t local_outputs_released = 0;
    int64_t local_output_release_balance_errors = 0;
    int64_t partial_acquire_failures = 0;
    int64_t compute_failures = 0;
    int64_t proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_warmup_local_compute_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedWarmupLocalComputeSummary
> g_cnr3_for_debug_only_bounded_warmup_local_compute_summaries;

static void cnr3_for_debug_only_record_bounded_warmup_local_compute_summary(
    const Cnr3Data* d,
    bool plan_seen,
    int source_frames_retrieved,
    int source_frames_released,
    bool source_release_balance_ok,
    bool start_frame_zero,
    bool start_frame_nonzero,
    int local_start_reset_copies,
    int local_recursive_computes,
    int local_outputs_allocated,
    int local_outputs_released,
    bool local_output_release_balance_ok,
    bool partial_acquire_failure,
    bool compute_failure,
    bool proof_ok
) {
    /*
        Temporary CMS02-H5 proof summary.

        This records local bounded warm-up compute diagnostics only. H5 remains
        proof-only: it must not store computed frames, return them, or change
        output authority.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF) {
        (void)d;
        (void)plan_seen;
        (void)source_frames_retrieved;
        (void)source_frames_released;
        (void)source_release_balance_ok;
        (void)start_frame_zero;
        (void)start_frame_nonzero;
        (void)local_start_reset_copies;
        (void)local_recursive_computes;
        (void)local_outputs_allocated;
        (void)local_outputs_released;
        (void)local_output_release_balance_ok;
        (void)partial_acquire_failure;
        (void)compute_failure;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_local_compute_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedWarmupLocalComputeSummary& summary =
            g_cnr3_for_debug_only_bounded_warmup_local_compute_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (plan_seen) {
            ++summary.plans_seen;
        }

        summary.source_frames_retrieved_total += source_frames_retrieved;
        summary.source_frames_released_total += source_frames_released;

        if (!source_release_balance_ok) {
            ++summary.source_frame_release_balance_errors;
        }

        if (start_frame_zero) {
            ++summary.start_frame_zero_count;
        }

        if (start_frame_nonzero) {
            ++summary.start_frame_nonzero_count;
        }

        summary.local_start_reset_copies += local_start_reset_copies;
        summary.local_recursive_computes += local_recursive_computes;
        summary.local_outputs_allocated += local_outputs_allocated;
        summary.local_outputs_released += local_outputs_released;

        if (!local_output_release_balance_ok) {
            ++summary.local_output_release_balance_errors;
        }

        if (partial_acquire_failure) {
            ++summary.partial_acquire_failures;
        }

        if (compute_failure) {
            ++summary.compute_failures;
        }

        if (!proof_ok) {
            ++summary.proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_warmup_local_compute_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H5 proof summary print.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedWarmupLocalComputeSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_warmup_local_compute_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_warmup_local_compute_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_warmup_local_compute_summaries.end()
                ) {
                summary = found->second;
            }
        }

        const int64_t source_frame_release_balance =
            summary.source_frames_retrieved_total -
            summary.source_frames_released_total;

        const int64_t local_output_release_balance =
            summary.local_outputs_allocated -
            summary.local_outputs_released;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_warmup_local_compute_summary # FOR-DEBUG-ONLY-BOUNDED-WARMUP-LOCAL-COMPUTE-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # plans_seen=%lld # source_frames_retrieved_total=%lld # source_frames_released_total=%lld # source_frame_release_balance=%lld # source_frame_release_balance_errors=%lld # start_frame_zero_count=%lld # start_frame_nonzero_count=%lld # local_start_reset_copies=%lld # local_recursive_computes=%lld # local_outputs_allocated=%lld # local_outputs_released=%lld # local_output_release_balance=%lld # local_output_release_balance_errors=%lld # partial_acquire_failures=%lld # compute_failures=%lld # proof_failures=%lld # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.plans_seen),
            static_cast<long long>(summary.source_frames_retrieved_total),
            static_cast<long long>(summary.source_frames_released_total),
            static_cast<long long>(source_frame_release_balance),
            static_cast<long long>(summary.source_frame_release_balance_errors),
            static_cast<long long>(summary.start_frame_zero_count),
            static_cast<long long>(summary.start_frame_nonzero_count),
            static_cast<long long>(summary.local_start_reset_copies),
            static_cast<long long>(summary.local_recursive_computes),
            static_cast<long long>(summary.local_outputs_allocated),
            static_cast<long long>(summary.local_outputs_released),
            static_cast<long long>(local_output_release_balance),
            static_cast<long long>(summary.local_output_release_balance_errors),
            static_cast<long long>(summary.partial_acquire_failures),
            static_cast<long long>(summary.compute_failures),
            static_cast<long long>(summary.proof_failures)
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_warmup_local_compute_summary(
    const Cnr3Data* d
) {
    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_local_compute_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_warmup_local_compute_summaries.erase(
            d->instance_id
        );
    }
}

static const VSFrame* cnr3_for_debug_only_find_bounded_warmup_source_frame(
    const Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet& source_frame_set,
    int frame_number
) {
    for (const Cnr3ForDebugOnlyBoundedWarmupSourceFrameSetEntry& entry :
        source_frame_set.entries) {
        if (entry.frame_number == frame_number) {
            return entry.frame;
        }
    }

    return nullptr;
}

static void cnr3_for_debug_only_release_bounded_warmup_local_outputs(
    const Cnr3Data* d,
    std::vector<Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry>& local_outputs,
    const char* reason,
    const VSAPI* vsapi,
    int& released_count
) {
    released_count = 0;

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
        ) {
        (void)d;
        (void)local_outputs;
        (void)reason;
        (void)vsapi;
        return;
    }
    else {
        if (vsapi == nullptr) {
            return;
        }

        for (Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry& entry :
            local_outputs) {
            if (entry.frame == nullptr) {
                continue;
            }

            vsapi->freeFrame(entry.frame);
            ++released_count;

            cnr3_debug_printf(
                d != nullptr ? d->debug : false,
                "output-cache # cnr3_for_debug_only_release_bounded_warmup_local_outputs # FOR-DEBUG-ONLY-BOUNDED-WARMUP-LOCAL-OUTPUT-RELEASED # instance=%d # reason=%s # frame=%d\n",
                d != nullptr ? d->instance_id : -1,
                reason != nullptr ? reason : "unknown",
                entry.frame_number
            );

            entry.frame = nullptr;
        }

        local_outputs.clear();
    }
}

static bool cnr3_for_debug_only_compute_bounded_warmup_local_outputs(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    const Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet& source_frame_set,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi,
    int& local_start_reset_copies,
    int& local_recursive_computes,
    int& local_outputs_allocated,
    int& local_outputs_released,
    bool& compute_failure,
    bool& local_output_release_balance_ok
) {
    /*
        Temporary CMS02-H5 local compute proof.

        The start frame is an explicit bounded warm-up reset/copy. Recursive
        steps after the start use process_cnr3_frame_with_explicit_previous_output()
        with the previous local output. This does not call process_cnr3_frame(),
        does not touch old strict state, and does not implement any new pixel
        algorithm.
    */

    local_start_reset_copies = 0;
    local_recursive_computes = 0;
    local_outputs_allocated = 0;
    local_outputs_released = 0;
    compute_failure = false;
    local_output_release_balance_ok = true;

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF) {
        (void)d;
        (void)plan;
        (void)source_frame_set;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            d->vi == nullptr ||
            plan == nullptr ||
            frameCtx == nullptr ||
            core == nullptr ||
            vsapi == nullptr
            ) {
            return true;
        }

        std::vector<Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry> local_outputs;
        bool proof_ok = true;

        for (
            int frame_number = plan->first_source_frame_number;
            frame_number <= plan->last_source_frame_number;
            ++frame_number
            ) {
            const VSFrame* source_frame =
                cnr3_for_debug_only_find_bounded_warmup_source_frame(
                    source_frame_set,
                    frame_number
                );

            if (source_frame == nullptr) {
                compute_failure = true;
                proof_ok = false;
                break;
            }

            VSFrame* local_output =
                vsapi->newVideoFrame(
                    &d->vi->format,
                    d->vi->width,
                    d->vi->height,
                    source_frame,
                    core
                );

            if (local_output == nullptr) {
                compute_failure = true;
                proof_ok = false;
                break;
            }

            ++local_outputs_allocated;

            const bool is_start_frame =
                (frame_number == plan->first_source_frame_number);

            const VSFrame* previous_local_output =
                local_outputs.empty() ? nullptr : local_outputs.back().frame;

            const int processing_frame_number =
                is_start_frame ? 0 : frame_number;

            const int predecessor_frame_number =
                is_start_frame ? -1 : frame_number - 1;

            const VSFrame* explicit_previous_output =
                is_start_frame ? nullptr : previous_local_output;

            if (
                !process_cnr3_frame_with_explicit_previous_output(
                    d,
                    processing_frame_number,
                    source_frame,
                    explicit_previous_output,
                    local_output,
                    frameCtx,
                    vsapi
                )
                ) {
                vsapi->freeFrame(local_output);
                local_output = nullptr;
                ++local_outputs_released;
                compute_failure = true;
                proof_ok = false;
                break;
            }

            if (is_start_frame) {
                ++local_start_reset_copies;
            }
            else {
                ++local_recursive_computes;
            }

            local_outputs.push_back(
                Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry{
                    frame_number,
                    local_output
                }
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_compute_bounded_warmup_local_outputs # FOR-DEBUG-ONLY-BOUNDED-WARMUP-LOCAL-COMPUTE-STEP # instance=%d # requested=%d # actual_source_frame=%d # warmup_start_frame=%d # processing_frame_number=%d # predecessor_frame_number=%d # bounded_start_uses_frame0_reset_path=%d # recursive_compute=%d # uses_existing_explicit_previous_output_helper=1 # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0\n",
                d->instance_id,
                plan->requested_frame_number,
                frame_number,
                plan->first_source_frame_number,
                processing_frame_number,
                predecessor_frame_number,
                is_start_frame ? 1 : 0,
                is_start_frame ? 0 : 1
            );
        }

        int released_by_helper = 0;

        cnr3_for_debug_only_release_bounded_warmup_local_outputs(
            d,
            local_outputs,
            proof_ok ? "normal-proof-release" : "compute-failure-release",
            vsapi,
            released_by_helper
        );

        local_outputs_released += released_by_helper;

        local_output_release_balance_ok =
            (local_outputs_allocated == local_outputs_released);

        proof_ok =
            (
                proof_ok &&
                local_output_release_balance_ok
                );

        return proof_ok;
    }
}

static bool cnr3_for_debug_only_probe_bounded_warmup_local_compute(
    const Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-H5 arAllFramesReady proof.

        Retrieve the bounded warm-up source-frame set requested in arInitial,
        compute local outputs from S..N, and release all source and local output
        frames. The computed outputs are never stored or returned.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (plan == nullptr) {
            return true;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet source_frame_set;
        int retrieved_count = 0;
        bool partial_acquire_failure = false;

        bool proof_ok =
            cnr3_for_debug_only_retrieve_bounded_warmup_source_frames(
                d,
                plan,
                frameCtx,
                vsapi,
                source_frame_set,
                retrieved_count,
                partial_acquire_failure
            );

        int local_start_reset_copies = 0;
        int local_recursive_computes = 0;
        int local_outputs_allocated = 0;
        int local_outputs_released = 0;
        bool compute_failure = false;
        bool local_output_release_balance_ok = true;

        if (proof_ok) {
            proof_ok =
                cnr3_for_debug_only_compute_bounded_warmup_local_outputs(
                    d,
                    plan,
                    source_frame_set,
                    frameCtx,
                    core,
                    vsapi,
                    local_start_reset_copies,
                    local_recursive_computes,
                    local_outputs_allocated,
                    local_outputs_released,
                    compute_failure,
                    local_output_release_balance_ok
                );
        }

        int source_frames_released = 0;

        cnr3_for_debug_only_release_bounded_warmup_source_frame_set(
            d,
            source_frame_set,
            proof_ok ? "normal-proof-release" : "h5-proof-failure-release",
            vsapi,
            source_frames_released
        );

        const bool source_release_balance_ok =
            (retrieved_count == source_frames_released);

        proof_ok =
            (
                proof_ok &&
                source_release_balance_ok &&
                !partial_acquire_failure &&
                !compute_failure &&
                local_output_release_balance_ok
                );

        cnr3_for_debug_only_record_bounded_warmup_local_compute_summary(
            d,
            true,
            retrieved_count,
            source_frames_released,
            source_release_balance_ok,
            plan->first_source_frame_number == 0,
            plan->first_source_frame_number > 0,
            local_start_reset_copies,
            local_recursive_computes,
            local_outputs_allocated,
            local_outputs_released,
            local_output_release_balance_ok,
            partial_acquire_failure,
            compute_failure,
            proof_ok
        );

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # cnr3_for_debug_only_probe_bounded_warmup_local_compute # FOR-DEBUG-ONLY-BOUNDED-WARMUP-LOCAL-COMPUTE-END # instance=%d # requested=%d # first_source=%d # last_source=%d # source_count=%d # retrieved=%d # source_released=%d # source_release_balance=%d # start_frame_zero=%d # start_frame_nonzero=%d # local_start_reset_copies=%d # local_recursive_computes=%d # local_outputs_allocated=%d # local_outputs_released=%d # local_output_release_balance=%d # partial_acquire_failure=%d # compute_failure=%d # would_store_warmup_outputs=0 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d != nullptr ? d->instance_id : -1,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count,
            retrieved_count,
            source_frames_released,
            retrieved_count - source_frames_released,
            plan->first_source_frame_number == 0 ? 1 : 0,
            plan->first_source_frame_number > 0 ? 1 : 0,
            local_start_reset_copies,
            local_recursive_computes,
            local_outputs_allocated,
            local_outputs_released,
            local_outputs_allocated - local_outputs_released,
            partial_acquire_failure ? 1 : 0,
            compute_failure ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

struct Cnr3ForDebugOnlyBoundedWarmupStoreSummary {
    int64_t frames_checked = 0;
    int64_t plans_seen = 0;
    int64_t source_frames_retrieved_total = 0;
    int64_t source_frames_released_total = 0;
    int64_t source_frame_release_balance_errors = 0;
    int64_t local_start_reset_copies = 0;
    int64_t local_recursive_computes = 0;
    int64_t local_outputs_available_for_store = 0;
    int64_t store_attempts = 0;
    int64_t store_successes = 0;
    int64_t store_failures = 0;
    int64_t duplicate_skipped_already_cached = 0;
    int64_t duplicate_computed_but_discarded = 0;
    int64_t local_outputs_released = 0;
    int64_t local_output_release_balance_errors = 0;
    int64_t partial_acquire_failures = 0;
    int64_t compute_failures = 0;
    int64_t proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_warmup_store_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedWarmupStoreSummary
> g_cnr3_for_debug_only_bounded_warmup_store_summaries;

static void cnr3_for_debug_only_record_bounded_warmup_store_summary(
    const Cnr3Data* d,
    bool plan_seen,
    int source_frames_retrieved,
    int source_frames_released,
    bool source_release_balance_ok,
    int local_start_reset_copies,
    int local_recursive_computes,
    int local_outputs_available_for_store,
    int store_attempts,
    int store_successes,
    int store_failures,
    int64_t duplicate_skipped_already_cached,
    int64_t duplicate_computed_but_discarded,
    int local_outputs_released,
    bool local_output_release_balance_ok,
    bool partial_acquire_failure,
    bool compute_failure,
    bool proof_ok
) {
    /*
        Temporary CMS02-H6 proof summary.

        H6 proves store orchestration and ownership only. It must not return
        warm-up outputs or make output_cache authoritative for this proof path.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
        ) {
        (void)d;
        (void)plan_seen;
        (void)source_frames_retrieved;
        (void)source_frames_released;
        (void)source_release_balance_ok;
        (void)local_start_reset_copies;
        (void)local_recursive_computes;
        (void)local_outputs_available_for_store;
        (void)store_attempts;
        (void)store_successes;
        (void)store_failures;
        (void)duplicate_skipped_already_cached;
        (void)duplicate_computed_but_discarded;
        (void)local_outputs_released;
        (void)local_output_release_balance_ok;
        (void)partial_acquire_failure;
        (void)compute_failure;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_store_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedWarmupStoreSummary& summary =
            g_cnr3_for_debug_only_bounded_warmup_store_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (plan_seen) {
            ++summary.plans_seen;
        }

        summary.source_frames_retrieved_total += source_frames_retrieved;
        summary.source_frames_released_total += source_frames_released;

        if (!source_release_balance_ok) {
            ++summary.source_frame_release_balance_errors;
        }

        summary.local_start_reset_copies += local_start_reset_copies;
        summary.local_recursive_computes += local_recursive_computes;
        summary.local_outputs_available_for_store += local_outputs_available_for_store;
        summary.store_attempts += store_attempts;
        summary.store_successes += store_successes;
        summary.store_failures += store_failures;
        summary.duplicate_skipped_already_cached += duplicate_skipped_already_cached;
        summary.duplicate_computed_but_discarded += duplicate_computed_but_discarded;
        summary.local_outputs_released += local_outputs_released;

        if (!local_output_release_balance_ok) {
            ++summary.local_output_release_balance_errors;
        }

        if (partial_acquire_failure) {
            ++summary.partial_acquire_failures;
        }

        if (compute_failure) {
            ++summary.compute_failures;
        }

        if (!proof_ok) {
            ++summary.proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_warmup_store_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H6 proof summary print.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedWarmupStoreSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_warmup_store_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_warmup_store_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_warmup_store_summaries.end()
                ) {
                summary = found->second;
            }
        }

        const int64_t source_frame_release_balance =
            summary.source_frames_retrieved_total -
            summary.source_frames_released_total;

        const int64_t local_output_release_balance =
            summary.local_outputs_available_for_store -
            summary.local_outputs_released;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_warmup_store_summary # FOR-DEBUG-ONLY-BOUNDED-WARMUP-STORE-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # plans_seen=%lld # source_frames_retrieved_total=%lld # source_frames_released_total=%lld # source_frame_release_balance=%lld # source_frame_release_balance_errors=%lld # local_start_reset_copies=%lld # local_recursive_computes=%lld # local_outputs_available_for_store=%lld # store_attempts=%lld # store_successes=%lld # store_failures=%lld # duplicate_skipped_already_cached=%lld # duplicate_computed_but_discarded=%lld # local_outputs_released=%lld # local_output_release_balance=%lld # local_output_release_balance_errors=%lld # partial_acquire_failures=%lld # compute_failures=%lld # proof_failures=%lld # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.plans_seen),
            static_cast<long long>(summary.source_frames_retrieved_total),
            static_cast<long long>(summary.source_frames_released_total),
            static_cast<long long>(source_frame_release_balance),
            static_cast<long long>(summary.source_frame_release_balance_errors),
            static_cast<long long>(summary.local_start_reset_copies),
            static_cast<long long>(summary.local_recursive_computes),
            static_cast<long long>(summary.local_outputs_available_for_store),
            static_cast<long long>(summary.store_attempts),
            static_cast<long long>(summary.store_successes),
            static_cast<long long>(summary.store_failures),
            static_cast<long long>(summary.duplicate_skipped_already_cached),
            static_cast<long long>(summary.duplicate_computed_but_discarded),
            static_cast<long long>(summary.local_outputs_released),
            static_cast<long long>(local_output_release_balance),
            static_cast<long long>(summary.local_output_release_balance_errors),
            static_cast<long long>(summary.partial_acquire_failures),
            static_cast<long long>(summary.compute_failures),
            static_cast<long long>(summary.proof_failures)
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_warmup_store_summary(
    const Cnr3Data* d
) {
    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_store_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_warmup_store_summaries.erase(
            d->instance_id
        );
    }
}

static bool cnr3_for_debug_only_compute_and_store_bounded_warmup_outputs(
    Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    const Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet& source_frame_set,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi,
    int& local_start_reset_copies,
    int& local_recursive_computes,
    int& local_outputs_available_for_store,
    int& store_attempts,
    int& store_successes,
    int& store_failures,
    int64_t& duplicate_skipped_already_cached,
    int64_t& duplicate_computed_but_discarded,
    int& local_outputs_released,
    bool& compute_failure,
    bool& local_output_release_balance_ok
) {
    /*
        Temporary CMS02-H6 compute-and-store proof.

        This deliberately uses a new helper instead of changing the proven H5
        local-compute helper. It reuses the same explicit-predecessor processing
        boundary and adds only store orchestration plus ownership diagnostics.
    */

    local_start_reset_copies = 0;
    local_recursive_computes = 0;
    local_outputs_available_for_store = 0;
    store_attempts = 0;
    store_successes = 0;
    store_failures = 0;
    duplicate_skipped_already_cached = 0;
    duplicate_computed_but_discarded = 0;
    local_outputs_released = 0;
    compute_failure = false;
    local_output_release_balance_ok = true;

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
        ) {
        (void)d;
        (void)plan;
        (void)source_frame_set;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (
            d == nullptr ||
            d->vi == nullptr ||
            plan == nullptr ||
            frameCtx == nullptr ||
            core == nullptr ||
            vsapi == nullptr
            ) {
            return true;
        }

        Cnr3OutputCacheDebugSnapshot before_snapshot;
        const bool before_snapshot_ok =
            cnr3_output_cache_get_debug_snapshot(
                d->output_cache,
                before_snapshot
            );

        std::vector<Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry> local_outputs;
        bool proof_ok = true;

        for (
            int frame_number = plan->first_source_frame_number;
            frame_number <= plan->last_source_frame_number;
            ++frame_number
            ) {
            const VSFrame* source_frame =
                cnr3_for_debug_only_find_bounded_warmup_source_frame(
                    source_frame_set,
                    frame_number
                );

            if (source_frame == nullptr) {
                compute_failure = true;
                proof_ok = false;
                break;
            }

            VSFrame* local_output =
                vsapi->newVideoFrame(
                    &d->vi->format,
                    d->vi->width,
                    d->vi->height,
                    source_frame,
                    core
                );

            if (local_output == nullptr) {
                compute_failure = true;
                proof_ok = false;
                break;
            }

            ++local_outputs_available_for_store;

            const bool is_start_frame =
                (frame_number == plan->first_source_frame_number);

            const VSFrame* previous_local_output =
                local_outputs.empty() ? nullptr : local_outputs.back().frame;

            const int processing_frame_number =
                is_start_frame ? 0 : frame_number;

            const int predecessor_frame_number =
                is_start_frame ? -1 : frame_number - 1;

            const VSFrame* explicit_previous_output =
                is_start_frame ? nullptr : previous_local_output;

            if (
                !process_cnr3_frame_with_explicit_previous_output(
                    d,
                    processing_frame_number,
                    source_frame,
                    explicit_previous_output,
                    local_output,
                    frameCtx,
                    vsapi
                )
                ) {
                vsapi->freeFrame(local_output);
                local_output = nullptr;
                ++local_outputs_released;
                compute_failure = true;
                proof_ok = false;
                break;
            }

            if (is_start_frame) {
                ++local_start_reset_copies;
            }
            else {
                ++local_recursive_computes;
            }

            ++store_attempts;

            const bool store_ok =
                cnr3_output_cache_store_frame(
                    d->output_cache,
                    frame_number,
                    local_output,
                    vsapi
                );

            if (store_ok) {
                ++store_successes;
            }
            else {
                ++store_failures;
                proof_ok = false;
            }

            local_outputs.push_back(
                Cnr3ForDebugOnlyBoundedWarmupLocalOutputEntry{
                    frame_number,
                    local_output
                }
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_for_debug_only_compute_and_store_bounded_warmup_outputs # FOR-DEBUG-ONLY-BOUNDED-WARMUP-STORE-STEP # instance=%d # requested=%d # actual_source_frame=%d # warmup_start_frame=%d # processing_frame_number=%d # predecessor_frame_number=%d # bounded_start_uses_frame0_reset_path=%d # recursive_compute=%d # store_attempted=1 # store_ok=%d # caller_still_owns_local_output=1 # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0\n",
                d->instance_id,
                plan->requested_frame_number,
                frame_number,
                plan->first_source_frame_number,
                processing_frame_number,
                predecessor_frame_number,
                is_start_frame ? 1 : 0,
                is_start_frame ? 0 : 1,
                store_ok ? 1 : 0
            );

            if (!store_ok) {
                break;
            }
        }

        int released_by_helper = 0;

        const char* local_output_release_reason =
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN
            ? (proof_ok ? "h7-return-decision-local-output-release" : "h7-return-decision-local-output-failure-release")
            : (proof_ok ? "h6-normal-store-proof-release" : "h6-store-proof-failure-release");

        cnr3_for_debug_only_release_bounded_warmup_local_outputs(
            d,
            local_outputs,
            local_output_release_reason,
            vsapi,
            released_by_helper
        );

        local_outputs_released += released_by_helper;

        Cnr3OutputCacheDebugSnapshot after_snapshot;
        const bool after_snapshot_ok =
            cnr3_output_cache_get_debug_snapshot(
                d->output_cache,
                after_snapshot
            );

        if (before_snapshot_ok && after_snapshot_ok) {
            duplicate_skipped_already_cached =
                after_snapshot.stats.store_skipped_already_cached -
                before_snapshot.stats.store_skipped_already_cached;

            duplicate_computed_but_discarded =
                after_snapshot.stats.duplicate_store_computed_but_discarded -
                before_snapshot.stats.duplicate_store_computed_but_discarded;
        }

        local_output_release_balance_ok =
            (local_outputs_available_for_store == local_outputs_released);

        proof_ok =
            (
                proof_ok &&
                local_output_release_balance_ok &&
                store_failures == 0
                );

        return proof_ok;
    }
}

static bool cnr3_for_debug_only_probe_bounded_warmup_store(
    Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-H6 arAllFramesReady proof.

        Retrieve the bounded warm-up source-frame set requested in arInitial,
        compute local outputs from S..N, store them into output_cache, and
        release every local output. H6 never returns the stored warm-up outputs.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (plan == nullptr) {
            return true;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet source_frame_set;
        int retrieved_count = 0;
        bool partial_acquire_failure = false;

        bool proof_ok =
            cnr3_for_debug_only_retrieve_bounded_warmup_source_frames(
                d,
                plan,
                frameCtx,
                vsapi,
                source_frame_set,
                retrieved_count,
                partial_acquire_failure
            );

        int local_start_reset_copies = 0;
        int local_recursive_computes = 0;
        int local_outputs_available_for_store = 0;
        int store_attempts = 0;
        int store_successes = 0;
        int store_failures = 0;
        int64_t duplicate_skipped_already_cached = 0;
        int64_t duplicate_computed_but_discarded = 0;
        int local_outputs_released = 0;
        bool compute_failure = false;
        bool local_output_release_balance_ok = true;

        if (proof_ok) {
            proof_ok =
                cnr3_for_debug_only_compute_and_store_bounded_warmup_outputs(
                    d,
                    plan,
                    source_frame_set,
                    frameCtx,
                    core,
                    vsapi,
                    local_start_reset_copies,
                    local_recursive_computes,
                    local_outputs_available_for_store,
                    store_attempts,
                    store_successes,
                    store_failures,
                    duplicate_skipped_already_cached,
                    duplicate_computed_but_discarded,
                    local_outputs_released,
                    compute_failure,
                    local_output_release_balance_ok
                );
        }

        int source_frames_released = 0;

        cnr3_for_debug_only_release_bounded_warmup_source_frame_set(
            d,
            source_frame_set,
            proof_ok ? "h6-normal-store-proof-release" : "h6-proof-failure-release",
            vsapi,
            source_frames_released
        );

        const bool source_release_balance_ok =
            (retrieved_count == source_frames_released);

        proof_ok =
            (
                proof_ok &&
                source_release_balance_ok &&
                !partial_acquire_failure &&
                !compute_failure &&
                store_failures == 0 &&
                local_output_release_balance_ok
                );

        cnr3_for_debug_only_record_bounded_warmup_store_summary(
            d,
            true,
            retrieved_count,
            source_frames_released,
            source_release_balance_ok,
            local_start_reset_copies,
            local_recursive_computes,
            local_outputs_available_for_store,
            store_attempts,
            store_successes,
            store_failures,
            duplicate_skipped_already_cached,
            duplicate_computed_but_discarded,
            local_outputs_released,
            local_output_release_balance_ok,
            partial_acquire_failure,
            compute_failure,
            proof_ok
        );

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # cnr3_for_debug_only_probe_bounded_warmup_store # FOR-DEBUG-ONLY-BOUNDED-WARMUP-STORE-END # instance=%d # requested=%d # first_source=%d # last_source=%d # source_count=%d # retrieved=%d # source_released=%d # source_release_balance=%d # local_start_reset_copies=%d # local_recursive_computes=%d # local_outputs_available_for_store=%d # store_attempts=%d # store_successes=%d # store_failures=%d # duplicate_skipped_already_cached=%lld # duplicate_computed_but_discarded=%lld # local_outputs_released=%d # local_output_release_balance=%d # partial_acquire_failure=%d # compute_failure=%d # would_return_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d != nullptr ? d->instance_id : -1,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count,
            retrieved_count,
            source_frames_released,
            retrieved_count - source_frames_released,
            local_start_reset_copies,
            local_recursive_computes,
            local_outputs_available_for_store,
            store_attempts,
            store_successes,
            store_failures,
            static_cast<long long>(duplicate_skipped_already_cached),
            static_cast<long long>(duplicate_computed_but_discarded),
            local_outputs_released,
            local_outputs_available_for_store - local_outputs_released,
            partial_acquire_failure ? 1 : 0,
            compute_failure ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

struct Cnr3ForDebugOnlyBoundedWarmupReturnDecisionSummary {
    int64_t frames_checked = 0;
    int64_t plans_seen = 0;
    int64_t source_frames_retrieved_total = 0;
    int64_t source_frames_released_total = 0;
    int64_t source_frame_release_balance_errors = 0;
    int64_t local_outputs_available_for_store = 0;
    int64_t store_attempts = 0;
    int64_t store_successes = 0;
    int64_t store_failures = 0;
    int64_t duplicate_skipped_already_cached = 0;
    int64_t duplicate_computed_but_discarded = 0;
    int64_t local_outputs_released = 0;
    int64_t local_output_release_balance_errors = 0;
    int64_t return_decisions_checked = 0;
    int64_t candidate_lookup_attempts = 0;
    int64_t candidate_lookup_successes = 0;
    int64_t candidate_lookup_failures = 0;
    int64_t lookup_refs_released = 0;
    int64_t lookup_refs_transferred = 0;
    int64_t lookup_ref_release_balance_errors = 0;
    int64_t would_return_warmup_output_count = 0;
    int64_t actual_returned_warmup_output_count = 0;
    int64_t partial_acquire_failures = 0;
    int64_t compute_failures = 0;
    int64_t proof_failures = 0;
};

static std::mutex g_cnr3_for_debug_only_bounded_warmup_return_decision_summary_mutex;

static std::unordered_map<
    int,
    Cnr3ForDebugOnlyBoundedWarmupReturnDecisionSummary
> g_cnr3_for_debug_only_bounded_warmup_return_decision_summaries;

static void cnr3_for_debug_only_record_bounded_warmup_return_decision_summary(
    const Cnr3Data* d,
    bool plan_seen,
    int source_frames_retrieved,
    int source_frames_released,
    bool source_release_balance_ok,
    int local_outputs_available_for_store,
    int store_attempts,
    int store_successes,
    int store_failures,
    int64_t duplicate_skipped_already_cached,
    int64_t duplicate_computed_but_discarded,
    int local_outputs_released,
    bool local_output_release_balance_ok,
    bool return_decision_checked,
    bool candidate_lookup_attempted,
    bool candidate_lookup_success,
    bool lookup_ref_released,
    bool lookup_ref_transferred,
    bool would_return_warmup_output,
    bool actual_returned_warmup_output,
    bool partial_acquire_failure,
    bool compute_failure,
    bool proof_ok
) {
    /*
        Temporary CMS02-H7 proof summary.

        H7 proves return-decision conditions only. It may look up and release
        a caller-owned candidate reference, but it must not return that frame or
        make output_cache authoritative for this proof path.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF) {
        (void)d;
        (void)plan_seen;
        (void)source_frames_retrieved;
        (void)source_frames_released;
        (void)source_release_balance_ok;
        (void)local_outputs_available_for_store;
        (void)store_attempts;
        (void)store_successes;
        (void)store_failures;
        (void)duplicate_skipped_already_cached;
        (void)duplicate_computed_but_discarded;
        (void)local_outputs_released;
        (void)local_output_release_balance_ok;
        (void)return_decision_checked;
        (void)candidate_lookup_attempted;
        (void)candidate_lookup_success;
        (void)lookup_ref_released;
        (void)lookup_ref_transferred;
        (void)would_return_warmup_output;
        (void)actual_returned_warmup_output;
        (void)partial_acquire_failure;
        (void)compute_failure;
        (void)proof_ok;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_return_decision_summary_mutex
        );

        Cnr3ForDebugOnlyBoundedWarmupReturnDecisionSummary& summary =
            g_cnr3_for_debug_only_bounded_warmup_return_decision_summaries[
                d->instance_id
            ];

        ++summary.frames_checked;

        if (plan_seen) {
            ++summary.plans_seen;
        }

        summary.source_frames_retrieved_total += source_frames_retrieved;
        summary.source_frames_released_total += source_frames_released;

        if (!source_release_balance_ok) {
            ++summary.source_frame_release_balance_errors;
        }

        summary.local_outputs_available_for_store += local_outputs_available_for_store;
        summary.store_attempts += store_attempts;
        summary.store_successes += store_successes;
        summary.store_failures += store_failures;
        summary.duplicate_skipped_already_cached += duplicate_skipped_already_cached;
        summary.duplicate_computed_but_discarded += duplicate_computed_but_discarded;
        summary.local_outputs_released += local_outputs_released;

        if (!local_output_release_balance_ok) {
            ++summary.local_output_release_balance_errors;
        }

        if (return_decision_checked) {
            ++summary.return_decisions_checked;
        }

        if (candidate_lookup_attempted) {
            ++summary.candidate_lookup_attempts;
        }

        if (candidate_lookup_success) {
            ++summary.candidate_lookup_successes;
        }
        else if (candidate_lookup_attempted) {
            ++summary.candidate_lookup_failures;
        }

        if (lookup_ref_released) {
            ++summary.lookup_refs_released;
        }

        if (lookup_ref_transferred) {
            ++summary.lookup_refs_transferred;
        }

        if (candidate_lookup_success && !lookup_ref_released && !lookup_ref_transferred) {
            ++summary.lookup_ref_release_balance_errors;
        }

        if (would_return_warmup_output) {
            ++summary.would_return_warmup_output_count;
        }

        if (actual_returned_warmup_output) {
            ++summary.actual_returned_warmup_output_count;
        }

        if (partial_acquire_failure) {
            ++summary.partial_acquire_failures;
        }

        if (compute_failure) {
            ++summary.compute_failures;
        }

        if (!proof_ok) {
            ++summary.proof_failures;
        }
    }
}

static void cnr3_for_debug_only_print_bounded_warmup_return_decision_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Temporary CMS02-H7 proof summary print.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF) {
        (void)d;
        (void)where;
        return;
    }
    else {
        if (d == nullptr || !d->debug) {
            return;
        }

        Cnr3ForDebugOnlyBoundedWarmupReturnDecisionSummary summary;

        {
            std::lock_guard<std::mutex> lock(
                g_cnr3_for_debug_only_bounded_warmup_return_decision_summary_mutex
            );

            const auto found =
                g_cnr3_for_debug_only_bounded_warmup_return_decision_summaries.find(
                    d->instance_id
                );

            if (
                found !=
                g_cnr3_for_debug_only_bounded_warmup_return_decision_summaries.end()
                ) {
                summary = found->second;
            }
        }

        const int64_t source_frame_release_balance =
            summary.source_frames_retrieved_total -
            summary.source_frames_released_total;

        const int64_t local_output_release_balance =
            summary.local_outputs_available_for_store -
            summary.local_outputs_released;

        const int64_t lookup_ref_ownership_balance =
            summary.candidate_lookup_successes -
            summary.lookup_refs_released -
            summary.lookup_refs_transferred;

        cnr3_debug_printf(
            d->debug,
            "output-cache # cnr3_for_debug_only_print_bounded_warmup_return_decision_summary # FOR-DEBUG-ONLY-BOUNDED-WARMUP-RETURN-DECISION-SUMMARY # instance=%d # where=\"%s\" # frames_checked=%lld # plans_seen=%lld # source_frames_retrieved_total=%lld # source_frames_released_total=%lld # source_frame_release_balance=%lld # source_frame_release_balance_errors=%lld # local_outputs_available_for_store=%lld # store_attempts=%lld # store_successes=%lld # store_failures=%lld # duplicate_skipped_already_cached=%lld # duplicate_computed_but_discarded=%lld # local_outputs_released=%lld # local_output_release_balance=%lld # local_output_release_balance_errors=%lld # return_decisions_checked=%lld # candidate_lookup_attempts=%lld # candidate_lookup_successes=%lld # candidate_lookup_failures=%lld # lookup_refs_released=%lld # lookup_refs_transferred=%lld # lookup_ref_ownership_balance=%lld # lookup_ref_release_balance_errors=%lld # would_return_warmup_output_count=%lld # actual_returned_warmup_output_count=%lld # partial_acquire_failures=%lld # compute_failures=%lld # proof_failures=%lld # output_authoritative=%d # mutates_old_strict=0\n",
            d->instance_id,
            where != nullptr ? where : "unknown",
            static_cast<long long>(summary.frames_checked),
            static_cast<long long>(summary.plans_seen),
            static_cast<long long>(summary.source_frames_retrieved_total),
            static_cast<long long>(summary.source_frames_released_total),
            static_cast<long long>(source_frame_release_balance),
            static_cast<long long>(summary.source_frame_release_balance_errors),
            static_cast<long long>(summary.local_outputs_available_for_store),
            static_cast<long long>(summary.store_attempts),
            static_cast<long long>(summary.store_successes),
            static_cast<long long>(summary.store_failures),
            static_cast<long long>(summary.duplicate_skipped_already_cached),
            static_cast<long long>(summary.duplicate_computed_but_discarded),
            static_cast<long long>(summary.local_outputs_released),
            static_cast<long long>(local_output_release_balance),
            static_cast<long long>(summary.local_output_release_balance_errors),
            static_cast<long long>(summary.return_decisions_checked),
            static_cast<long long>(summary.candidate_lookup_attempts),
            static_cast<long long>(summary.candidate_lookup_successes),
            static_cast<long long>(summary.candidate_lookup_failures),
            static_cast<long long>(summary.lookup_refs_released),
            static_cast<long long>(summary.lookup_refs_transferred),
            static_cast<long long>(lookup_ref_ownership_balance),
            static_cast<long long>(summary.lookup_ref_release_balance_errors),
            static_cast<long long>(summary.would_return_warmup_output_count),
            static_cast<long long>(summary.actual_returned_warmup_output_count),
            static_cast<long long>(summary.partial_acquire_failures),
            static_cast<long long>(summary.compute_failures),
            static_cast<long long>(summary.proof_failures),
            (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF || CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF) ? 1 : 0
        );
    }
}

static void cnr3_for_debug_only_erase_bounded_warmup_return_decision_summary(
    const Cnr3Data* d
) {
    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF) {
        (void)d;
        return;
    }
    else {
        if (d == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(
            g_cnr3_for_debug_only_bounded_warmup_return_decision_summary_mutex
        );

        g_cnr3_for_debug_only_bounded_warmup_return_decision_summaries.erase(
            d->instance_id
        );
    }
}

static bool cnr3_for_debug_only_probe_bounded_warmup_return_decision_dry_run(
    Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    /*
        Temporary CMS02-H7 arAllFramesReady proof.

        H7 reuses the H6 compute-and-store path, then performs only a dry-run
        return decision for output[N]. A caller-owned lookup reference may be
        acquired to prove candidate availability, but it is released immediately
        and never returned to VapourSynth.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        return true;
    }
    else {
        if (plan == nullptr) {
            return true;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet source_frame_set;
        int retrieved_count = 0;
        bool partial_acquire_failure = false;

        bool proof_ok =
            cnr3_for_debug_only_retrieve_bounded_warmup_source_frames(
                d,
                plan,
                frameCtx,
                vsapi,
                source_frame_set,
                retrieved_count,
                partial_acquire_failure
            );

        int local_start_reset_copies = 0;
        int local_recursive_computes = 0;
        int local_outputs_available_for_store = 0;
        int store_attempts = 0;
        int store_successes = 0;
        int store_failures = 0;
        int64_t duplicate_skipped_already_cached = 0;
        int64_t duplicate_computed_but_discarded = 0;
        int local_outputs_released = 0;
        bool compute_failure = false;
        bool local_output_release_balance_ok = true;

        if (proof_ok) {
            proof_ok =
                cnr3_for_debug_only_compute_and_store_bounded_warmup_outputs(
                    d,
                    plan,
                    source_frame_set,
                    frameCtx,
                    core,
                    vsapi,
                    local_start_reset_copies,
                    local_recursive_computes,
                    local_outputs_available_for_store,
                    store_attempts,
                    store_successes,
                    store_failures,
                    duplicate_skipped_already_cached,
                    duplicate_computed_but_discarded,
                    local_outputs_released,
                    compute_failure,
                    local_output_release_balance_ok
                );
        }

        bool return_decision_checked = false;
        bool candidate_lookup_attempted = false;
        bool candidate_lookup_success = false;
        bool lookup_ref_released = false;
        bool would_return_warmup_output = false;
        bool actual_returned_warmup_output = false;

        if (proof_ok) {
            return_decision_checked = true;
            candidate_lookup_attempted = true;

            const VSFrame* candidate_output =
                cnr3_output_cache_find_frame_and_add_ref(
                    d->output_cache,
                    plan->requested_frame_number,
                    vsapi
                );

            if (candidate_output != nullptr) {
                candidate_lookup_success = true;
                would_return_warmup_output = true;

                vsapi->freeFrame(candidate_output);
                cnr3_output_cache_note_lookup_ref_released(d->output_cache);
                lookup_ref_released = true;
            }
            else {
                proof_ok = false;
            }
        }

        int source_frames_released = 0;

        cnr3_for_debug_only_release_bounded_warmup_source_frame_set(
            d,
            source_frame_set,
            proof_ok ? "h7-normal-return-decision-dry-run-release" : "h7-proof-failure-release",
            vsapi,
            source_frames_released
        );

        const bool source_release_balance_ok =
            (retrieved_count == source_frames_released);

        const bool lookup_ref_release_balance_ok =
            (!candidate_lookup_success || lookup_ref_released);

        proof_ok =
            (
                proof_ok &&
                source_release_balance_ok &&
                !partial_acquire_failure &&
                !compute_failure &&
                store_failures == 0 &&
                local_output_release_balance_ok &&
                return_decision_checked &&
                candidate_lookup_success &&
                lookup_ref_release_balance_ok &&
                would_return_warmup_output &&
                !actual_returned_warmup_output
                );

        cnr3_for_debug_only_record_bounded_warmup_return_decision_summary(
            d,
            true,
            retrieved_count,
            source_frames_released,
            source_release_balance_ok,
            local_outputs_available_for_store,
            store_attempts,
            store_successes,
            store_failures,
            duplicate_skipped_already_cached,
            duplicate_computed_but_discarded,
            local_outputs_released,
            local_output_release_balance_ok,
            return_decision_checked,
            candidate_lookup_attempted,
            candidate_lookup_success,
            lookup_ref_released,
            false,
            would_return_warmup_output,
            actual_returned_warmup_output,
            partial_acquire_failure,
            compute_failure,
            proof_ok
        );

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # cnr3_for_debug_only_probe_bounded_warmup_return_decision_dry_run # FOR-DEBUG-ONLY-BOUNDED-WARMUP-RETURN-DECISION-END # instance=%d # requested=%d # first_source=%d # last_source=%d # source_count=%d # retrieved=%d # source_released=%d # source_release_balance=%d # local_outputs_available_for_store=%d # store_attempts=%d # store_successes=%d # store_failures=%d # duplicate_skipped_already_cached=%lld # duplicate_computed_but_discarded=%lld # local_outputs_released=%d # local_output_release_balance=%d # return_decision_checked=%d # candidate_lookup_attempted=%d # candidate_lookup_success=%d # lookup_ref_released=%d # lookup_ref_release_balance=%d # would_return_warmup_output=%d # actual_returned_warmup_output=0 # output_authoritative=0 # mutates_old_strict=0 # proof_ok=%d\n",
            d != nullptr ? d->instance_id : -1,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count,
            retrieved_count,
            source_frames_released,
            retrieved_count - source_frames_released,
            local_outputs_available_for_store,
            store_attempts,
            store_successes,
            store_failures,
            static_cast<long long>(duplicate_skipped_already_cached),
            static_cast<long long>(duplicate_computed_but_discarded),
            local_outputs_released,
            local_outputs_available_for_store - local_outputs_released,
            return_decision_checked ? 1 : 0,
            candidate_lookup_attempted ? 1 : 0,
            candidate_lookup_success ? 1 : 0,
            lookup_ref_released ? 1 : 0,
            lookup_ref_release_balance_ok ? 0 : 1,
            would_return_warmup_output ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}


static bool cnr3_for_debug_only_probe_bounded_warmup_return_transfer_proof(
    Cnr3Data* d,
    const Cnr3ForDebugOnlyBoundedWarmupSourcePlan* plan,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi,
    const VSFrame** returned_frame
) {
    /*
        Temporary CMS02-H8/H9 arAllFramesReady proof.

        H8 proves the first proof-gated return transfer for output[N]. H9 reuses
        the same transfer mechanics as the candidate output-authoritative path,
        while still proving that old strict-streaming state is not mutated. The
        caller-owned lookup reference is transferred to VapourSynth by returning
        it from cnr3_get_frame(); it must not also be released locally.
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF) {
        (void)d;
        (void)plan;
        (void)frameCtx;
        (void)core;
        (void)vsapi;
        (void)returned_frame;
        return true;
    }
    else {
        if (returned_frame != nullptr) {
            *returned_frame = nullptr;
        }

        if (plan == nullptr || returned_frame == nullptr) {
            return true;
        }

        Cnr3ForDebugOnlyBoundedWarmupSourceFrameSet source_frame_set;
        int retrieved_count = 0;
        bool partial_acquire_failure = false;

        bool proof_ok =
            cnr3_for_debug_only_retrieve_bounded_warmup_source_frames(
                d,
                plan,
                frameCtx,
                vsapi,
                source_frame_set,
                retrieved_count,
                partial_acquire_failure
            );

        int local_start_reset_copies = 0;
        int local_recursive_computes = 0;
        int local_outputs_available_for_store = 0;
        int store_attempts = 0;
        int store_successes = 0;
        int store_failures = 0;
        int64_t duplicate_skipped_already_cached = 0;
        int64_t duplicate_computed_but_discarded = 0;
        int local_outputs_released = 0;
        bool compute_failure = false;
        bool local_output_release_balance_ok = true;

        if (proof_ok) {
            proof_ok =
                cnr3_for_debug_only_compute_and_store_bounded_warmup_outputs(
                    d,
                    plan,
                    source_frame_set,
                    frameCtx,
                    core,
                    vsapi,
                    local_start_reset_copies,
                    local_recursive_computes,
                    local_outputs_available_for_store,
                    store_attempts,
                    store_successes,
                    store_failures,
                    duplicate_skipped_already_cached,
                    duplicate_computed_but_discarded,
                    local_outputs_released,
                    compute_failure,
                    local_output_release_balance_ok
                );
        }

        bool return_decision_checked = false;
        bool candidate_lookup_attempted = false;
        bool candidate_lookup_success = false;
        bool lookup_ref_released = false;
        bool lookup_ref_transferred = false;
        bool would_return_warmup_output = false;
        bool actual_returned_warmup_output = false;

        if (proof_ok) {
            return_decision_checked = true;
            candidate_lookup_attempted = true;

            const VSFrame* candidate_output =
                cnr3_output_cache_find_frame_and_add_ref(
                    d->output_cache,
                    plan->requested_frame_number,
                    vsapi
                );

            if (candidate_output != nullptr) {
                candidate_lookup_success = true;
                would_return_warmup_output = true;

                cnr3_output_cache_note_lookup_ref_transferred(d->output_cache);
                lookup_ref_transferred = true;
                actual_returned_warmup_output = true;
                *returned_frame = candidate_output;
            }
            else {
                proof_ok = false;
            }
        }

        int source_frames_released = 0;

        cnr3_for_debug_only_release_bounded_warmup_source_frame_set(
            d,
            source_frame_set,
            proof_ok
            ? (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "h9-authority-integration-proof-release"
                : "h8-normal-return-transfer-proof-release")
            : (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "h9-proof-failure-release"
                : "h8-proof-failure-release"),
            vsapi,
            source_frames_released
        );

        const bool source_release_balance_ok =
            (retrieved_count == source_frames_released);

        const bool lookup_ref_release_balance_ok =
            (!candidate_lookup_success || lookup_ref_released || lookup_ref_transferred);

        proof_ok =
            (
                proof_ok &&
                source_release_balance_ok &&
                !partial_acquire_failure &&
                !compute_failure &&
                store_failures == 0 &&
                local_output_release_balance_ok &&
                return_decision_checked &&
                candidate_lookup_success &&
                lookup_ref_release_balance_ok &&
                would_return_warmup_output &&
                actual_returned_warmup_output &&
                lookup_ref_transferred &&
                !lookup_ref_released
                );

        cnr3_for_debug_only_record_bounded_warmup_return_decision_summary(
            d,
            true,
            retrieved_count,
            source_frames_released,
            source_release_balance_ok,
            local_outputs_available_for_store,
            store_attempts,
            store_successes,
            store_failures,
            duplicate_skipped_already_cached,
            duplicate_computed_but_discarded,
            local_outputs_released,
            local_output_release_balance_ok,
            return_decision_checked,
            candidate_lookup_attempted,
            candidate_lookup_success,
            lookup_ref_released,
            lookup_ref_transferred,
            would_return_warmup_output,
            actual_returned_warmup_output,
            partial_acquire_failure,
            compute_failure,
            proof_ok
        );

        const char* proof_function_name =
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
            ? "cnr3_for_debug_only_probe_bounded_warmup_authority_integration_proof"
            : "cnr3_for_debug_only_probe_bounded_warmup_return_transfer_proof";
        const char* proof_event_name =
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
            ? "FOR-DEBUG-ONLY-BOUNDED-WARMUP-AUTHORITY-INTEGRATION-END"
            : "FOR-DEBUG-ONLY-BOUNDED-WARMUP-RETURN-TRANSFER-END";

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # %s # %s # "
            "instance=%d # requested=%d # first_source=%d # last_source=%d # "
            "source_count=%d # retrieved=%d # source_released=%d # "
            "source_release_balance=%d # local_outputs_available_for_store=%d # "
            "store_attempts=%d # store_successes=%d # store_failures=%d # "
            "duplicate_skipped_already_cached=%lld # "
            "duplicate_computed_but_discarded=%lld # local_outputs_released=%d # "
            "local_output_release_balance=%d # return_decision_checked=%d # "
            "candidate_lookup_attempted=%d # candidate_lookup_success=%d # "
            "lookup_ref_released=%d # lookup_ref_release_balance=%d # "
            "would_return_warmup_output=%d # lookup_ref_transferred=%d # "
            "actual_returned_warmup_output=%d # output_authoritative=1 # "
            "mutates_old_strict=0 # proof_ok=%d\n",
            proof_function_name,
            proof_event_name,
            d != nullptr ? d->instance_id : -1,
            plan->requested_frame_number,
            plan->first_source_frame_number,
            plan->last_source_frame_number,
            plan->source_frame_count,
            retrieved_count,
            source_frames_released,
            retrieved_count - source_frames_released,
            local_outputs_available_for_store,
            store_attempts,
            store_successes,
            store_failures,
            static_cast<long long>(duplicate_skipped_already_cached),
            static_cast<long long>(duplicate_computed_but_discarded),
            local_outputs_released,
            local_outputs_available_for_store - local_outputs_released,
            return_decision_checked ? 1 : 0,
            candidate_lookup_attempted ? 1 : 0,
            candidate_lookup_success ? 1 : 0,
            lookup_ref_released ? 1 : 0,
            lookup_ref_release_balance_ok ? 0 : 1,
            would_return_warmup_output ? 1 : 0,
            lookup_ref_transferred ? 1 : 0,
            actual_returned_warmup_output ? 1 : 0,
            proof_ok ? 1 : 0
        );

        return proof_ok;
    }
}

static void cnr3_for_debug_only_destroy_unexpected_frame_data_source_request_plan(
    const Cnr3Data* d,
    void** frameData,
    int activationReason
) {
    /*
        Defensive cleanup for unexpected activation reasons.

        Normal VapourSynth flow should deliver the per-invocation frameData plan
        to arAllFramesReady, where the normal cleanup paths handle it. This
        helper prevents a future enabled proof run from leaking that plan if an
        unexpected activation reason reaches the function fallback path.

        H4 uses a dedicated frameData plan type. The H4 gate and G-phase gate
        must not be enabled together in a proof run.
    */

    if constexpr (
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF &&
        !CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON
        ) {
        (void)d;
        (void)frameData;
        (void)activationReason;
        return;
    }
    else {
        if (frameData == nullptr || *frameData == nullptr) {
            return;
        }

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # cnr3_get_frame # FOR-DEBUG-ONLY-FRAMEDATA-UNEXPECTED-FALLBACK # instance=%d # activation_reason=%d\n",
            d != nullptr ? d->instance_id : -1,
            activationReason
        );

        if constexpr (
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
            ) {
            Cnr3ForDebugOnlyBoundedWarmupSourcePlan* h4_source_plan =
                static_cast<Cnr3ForDebugOnlyBoundedWarmupSourcePlan*>(*frameData);

            *frameData = nullptr;

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "unexpected-activation-fallback"
            );
        }
        else {
            Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan =
                static_cast<Cnr3ForDebugOnlyRecoverySourceRequestPlan*>(*frameData);

            *frameData = nullptr;

            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "unexpected-activation-fallback"
            );
        }
    }
}

static const VSFrame* VS_CC cnr3_get_frame(
    int n,
    int activationReason,
    void* instanceData,
    void** frameData,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    Cnr3Data* d = static_cast<Cnr3Data*>(instanceData);

    if (activationReason == arInitial) {
        /*
            Normally too noisy for routine debugging.

            Enable temporarily only when investigating VapourSynth scheduling
            or unexpected frame request order.
        */
        /*
        cnr3_debug_print_cache_state(
            d,
            "arInitial/request source frame",
            n
        );
        */

        cnr3_output_cache_update_hot_zones(
            d->output_cache,
            n
        );

        cnr3_debug_print_output_cache_hot_zone_trace(
            d,
            n
        );

        Cnr3ForDebugOnlyBoundedWarmupSourcePlan* h4_source_plan =
            nullptr;

        Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan =
            nullptr;

        if (frameData != nullptr) {
            if constexpr (
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ) {
                h4_source_plan =
                    cnr3_for_debug_only_create_bounded_warmup_source_plan(
                        d,
                        n
                    );

                *frameData = h4_source_plan;
            }
            else {
                source_request_plan =
                    cnr3_for_debug_only_create_recovery_source_request_plan(
                        d,
                        n
                    );

                *frameData = source_request_plan;
            }
        }

        if (h4_source_plan != nullptr) {
            cnr3_for_debug_only_request_bounded_warmup_source_plan_frames(
                d,
                h4_source_plan,
                frameCtx,
                vsapi
            );
        }
        else if (source_request_plan != nullptr) {
            cnr3_for_debug_only_request_recovery_source_request_plan_frames(
                d,
                source_request_plan,
                frameCtx,
                vsapi
            );
        }
        else {
            vsapi->requestFrameFilter(n, d->node, frameCtx);
        }

        return nullptr;
    }

    if (activationReason == arAllFramesReady) {
        /*
            Normally too noisy for routine debugging.

            Enable temporarily only when investigating VapourSynth scheduling
            or unexpected frame request order.
        */
        /*
        cnr3_debug_print_cache_state(
            d,
            "arAllFramesReady/entry",
            n
        );
        */

        Cnr3ForDebugOnlyBoundedWarmupSourcePlan* h4_source_plan =
            nullptr;

        Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan =
            nullptr;

        if (frameData != nullptr) {
            if constexpr (
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ) {
                h4_source_plan =
                    static_cast<Cnr3ForDebugOnlyBoundedWarmupSourcePlan*>(*frameData);
            }
            else {
                source_request_plan =
                    static_cast<Cnr3ForDebugOnlyRecoverySourceRequestPlan*>(*frameData);
            }

            *frameData = nullptr;
        }

        if (
            !cnr3_for_debug_only_retrieve_hold_release_bounded_warmup_source_frames(
                d,
                h4_source_plan,
                frameCtx,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "h4-source-frame-set-proof-failure"
            );

            vsapi->setFilterError(
                "CNR3: debug-only bounded warm-up source-frame-set proof failed.",
                frameCtx
            );

            return nullptr;
        }

        if (
            !cnr3_for_debug_only_probe_bounded_warmup_local_compute(
                d,
                h4_source_plan,
                frameCtx,
                core,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "h5-local-compute-proof-failure"
            );

            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "h5-local-compute-proof-failure"
            );

            vsapi->setFilterError(
                "CNR3: debug-only bounded warm-up local-compute proof failed.",
                frameCtx
            );

            return nullptr;
        }

        if (
            !cnr3_for_debug_only_probe_bounded_warmup_store(
                d,
                h4_source_plan,
                frameCtx,
                core,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "h6-store-proof-failure"
            );

            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "h6-store-proof-failure"
            );

            vsapi->setFilterError(
                "CNR3: debug-only bounded warm-up store proof failed.",
                frameCtx
            );

            return nullptr;
        }

        if (
            !cnr3_for_debug_only_probe_bounded_warmup_return_decision_dry_run(
                d,
                h4_source_plan,
                frameCtx,
                core,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "h7-return-decision-dry-run-failure"
            );

            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "h7-return-decision-dry-run-failure"
            );

            vsapi->setFilterError(
                "CNR3: debug-only bounded warm-up return-decision dry-run failed.",
                frameCtx
            );

            return nullptr;
        }

        const VSFrame* h8_or_h9_returned_frame = nullptr;

        if (
            !cnr3_for_debug_only_probe_bounded_warmup_return_transfer_proof(
                d,
                h4_source_plan,
                frameCtx,
                core,
                vsapi,
                &h8_or_h9_returned_frame
            )
            ) {
            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "h8-return-transfer-proof-failure"
            );

            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "h8-return-transfer-proof-failure"
            );

            vsapi->setFilterError(
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "CNR3: debug-only bounded warm-up authority-integration proof failed."
                : "CNR3: debug-only bounded warm-up return-transfer proof failed.",
                frameCtx
            );

            return nullptr;
        }

        if (h8_or_h9_returned_frame != nullptr) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "h9-authority-integration-proof-return"
                : "h8-return-transfer-proof-return"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "h9-authority-integration-proof-return"
                : "h8-return-transfer-proof-return"
            );

            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_get_frame # %s # "
                "instance=%d # frame=%d # transferred_lookup_ref=1 # "
                "returned_bounded_warmup_output=1 # output_authoritative=1 # "
                "mutates_old_strict=0\n",
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "FOR-DEBUG-ONLY-BOUNDED-WARMUP-AUTHORITY-INTEGRATION-RETURN"
                : "FOR-DEBUG-ONLY-BOUNDED-WARMUP-RETURN-TRANSFER",
                d->instance_id,
                n
            );

            return h8_or_h9_returned_frame;
        }

        cnr3_for_debug_only_trace_recovery_source_request_plan_consumed(
            d,
            source_request_plan
        );

        if (
            !cnr3_for_debug_only_retrieve_extra_source_request_plan_frames(
                d,
                source_request_plan,
                frameCtx,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "extra-source-retrieval-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "extra-source-retrieval-failure"
            );

            vsapi->setFilterError(
                "CNR3: debug-only widened source-request proof failed to retrieve an extra source frame.",
                frameCtx
            );

            return nullptr;
        }

        if constexpr (
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ||
            CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
            ) {
            cnr3_debug_printf(
                d->debug,
                "output-cache # cnr3_get_frame # FOR-DEBUG-ONLY-BOUNDED-WARMUP-CACHE-HIT-RETURN-BYPASS # instance=%d # frame=%d # reason=%s # output_authoritative=0\n",
                d->instance_id,
                n,
                CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF
                ? "h9-authority-integration-proof-uses-explicit-transfer-path"
                : (
                    CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF
                    ? "h8-return-transfer-proof-uses-explicit-transfer-path"
                    : (
                        CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN
                        ? "h7-return-decision-dry-run-must-not-return"
                        : "h6-stored-proof-frames-must-not-be-returned"
                        )
                    )
            );
        }
        else {
            const VSFrame* cached_output =
                cnr3_output_cache_find_frame_and_add_ref(
                    d->output_cache,
                    n,
                    vsapi
                );

            if (cached_output != nullptr) {
                cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                    d,
                    source_request_plan,
                    "cache-hit-return"
                );

                cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                    d,
                    h4_source_plan,
                    "cache-hit-return"
                );

                cnr3_output_cache_note_lookup_ref_transferred(
                    d->output_cache
                );

                cnr3_debug_printf(
                    d->debug,
                    "output-cache # cnr3_get_frame # CACHE-HIT-RETURN # instance=%d # frame=%d\n",
                    d->instance_id,
                    n
                );

                cnr3_debug_print_output_cache_summary(
                    d,
                    "after CMS02-F output_cache cache-hit return"
                );

                return cached_output;
            }
        }

        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);

        if (src == nullptr) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "source-retrieval-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "source-retrieval-failure"
            );

            vsapi->setFilterError("CNR3: failed to retrieve source frame.", frameCtx);
            return nullptr;
        }

        /*
            Initial recursive Policy A.

            The real recursive algorithm uses d->old_strict_cache.prev_output when producing frame n.

            Later, this can be replaced by a seek-safe Policy C using recomputation
            or checkpoints.
        */

        if (n != d->old_strict_cache.next_needed) {
            const int requested_frame = n;
            const int next_needed = d->old_strict_cache.next_needed;
            const int gap = requested_frame - next_needed;

            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, out-of-order frame request: requested=%d, next_needed=%d, gap=%d, prev_output=%s\n",
                d->instance_id,
                requested_frame,
                next_needed,
                gap,
                d->old_strict_cache.prev_output != nullptr ? "yes" : "no"
            );

            char error_message[384];

            std::snprintf(
                error_message,
                sizeof(error_message),
                "CNR3: recursive streaming mode currently requires strictly increasing frame requests. "
                "instance=%d, requested=%d, next_needed=%d, gap=%d, prev_output=%s.",
                d->instance_id,
                requested_frame,
                next_needed,
                gap,
                d->old_strict_cache.prev_output != nullptr ? "yes" : "no"
            );

            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "out-of-order-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "out-of-order-failure"
            );

            vsapi->freeFrame(src);
            vsapi->setFilterError(error_message, frameCtx);
            return nullptr;
        }
        /*
            Normally too noisy for routine debugging.

            The out-of-order debug above is more useful because it captures
            the failure condition directly.
        */
        /*
        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, in-order frame accepted: requested=%d, next_needed=%d, prev_output=%s\n",
            d->instance_id,
            n,
            d->old_strict_cache.next_needed,
            d->old_strict_cache.prev_output != nullptr ? "yes" : "no"
        );
        */

        VSFrame* dst = vsapi->newVideoFrame(
            &d->vi->format,
            d->vi->width,
            d->vi->height,
            src,
            core
        );

        if (dst == nullptr) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "destination-allocation-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "destination-allocation-failure"
            );

            vsapi->freeFrame(src);
            vsapi->setFilterError("CNR3: failed to allocate output frame.", frameCtx);
            return nullptr;
        }

        if (!process_cnr3_frame(
            d,
            n,
            src,
            dst,
            frameCtx,
            vsapi
        )) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "process-frame-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "process-frame-failure"
            );

            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            return nullptr;
        }

        old_cnr3_strict_cache_store_output_frame(
            d->old_strict_cache,
            dst,
            n,
            vsapi
        );

        /*
            Verbose normal-path cache diagnostic. Keep disabled unless debugging
            strict streaming, cache ownership, or future cache-manager behaviour.

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processed frame: frame=%d, new_next_needed=%d, stored_prev_output=%s\n",
            d->instance_id,
            n,
            d->old_strict_cache.next_needed,
            d->old_strict_cache.prev_output != nullptr ? "yes" : "no"
        );
        */

        vsapi->freeFrame(src);

        cnr3_for_debug_only_probe_bounded_warmup_decision(
            d,
            n
        );

        cnr3_for_debug_only_probe_recovery_decision_walk_skeleton(
            d,
            n,
            source_request_plan,
            true,
            vsapi
        );

        if (
            !cnr3_for_debug_only_probe_recovery_source_frame_set(
                d,
                n,
                source_request_plan,
                frameCtx,
                core,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "source-frame-set-proof-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "source-frame-set-proof-failure"
            );

            vsapi->freeFrame(dst);

            vsapi->setFilterError(
                "CNR3: debug-only recovery source-frame-set proof failed.",
                frameCtx
            );

            return nullptr;
        }

        if (
            !cnr3_for_debug_only_probe_recovery_store_difference_measurement(
                d,
                n,
                dst,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "recovery-store-difference-measurement-proof-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "recovery-store-difference-measurement-proof-failure"
            );

            vsapi->freeFrame(dst);

            vsapi->setFilterError(
                "CNR3: debug-only recovery-store difference measurement proof failed.",
                frameCtx
            );

            return nullptr;
        }

        if (
            !cnr3_for_debug_only_probe_recovery_return_decision_dry_run(
                d,
                n,
                vsapi
            )
            ) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "recovery-return-decision-dry-run-failure"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "recovery-return-decision-dry-run-failure"
            );

            vsapi->freeFrame(dst);

            vsapi->setFilterError(
                "CNR3: debug-only recovery-return decision dry-run failed.",
                frameCtx
            );

            return nullptr;
        }

        const VSFrame* recovered_output_for_debug_return =
            cnr3_for_debug_only_probe_recovery_return_transfer_proof(
                d,
                n,
                vsapi
            );

        if (recovered_output_for_debug_return != nullptr) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
                "recovery-return-transfer-proof"
            );

            cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
                d,
                h4_source_plan,
                "recovery-return-transfer-proof"
            );

            vsapi->freeFrame(dst);

            cnr3_debug_print_output_cache_summary(
                d,
                "after CMS02-G10D9 recovery return transfer proof"
            );

            return recovered_output_for_debug_return;
        }

        /*
            CMS05-3A store/prune-only runtime proving.

            The CMS05 output cache is not output-authoritative in this phase.
            The already-produced dst frame remains the frame returned to
            VapourSynth.

            Purpose:                - store the produced output frame in output_cache;
                - exercise addFrameRef/freeFrame ownership accounting;
                - exercise active_ceiling, hot zones, and pruning on real frames;
                - collect diagnostics before any future output-cache read path is
                  allowed to affect recursive output generation.

            Important:
                Failure to store or prune the diagnostic/proving cache must not
                change the returned frame in CMS05-3A. The old strict-streaming
                path remains the source of output truth.
        */

        const bool output_cache_store_ok =
            cnr3_output_cache_store_frame(
                d->output_cache,
                n,
                dst,
                vsapi
            );

        bool output_cache_prune_ok = true;

        if (!output_cache_store_ok) {
            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, frame=%d, CMS05-3A output_cache store failed; returning strict-path output.\n",
                d->instance_id,
                n
            );
        }
        else {
            output_cache_prune_ok =
                cnr3_output_cache_prune_after_store(
                    d->output_cache,
                    vsapi
                );

            if (!output_cache_prune_ok) {
                cnr3_debug_printf(
                    d->debug,
                    "CNR3 debug: instance=%d, frame=%d, CMS05-3A output_cache prune_after_store failed; returning strict-path output.\n",
                    d->instance_id,
                    n
                );
            }
        }

        cnr3_for_debug_only_probe_bounded_checkpoint_search(
            d,
            n,
            output_cache_store_ok,
            output_cache_prune_ok
        );

        cnr3_for_debug_only_probe_bounded_warmup_source_request_plan(
            d,
            n,
            output_cache_store_ok,
            output_cache_prune_ok
        );

        cnr3_for_debug_only_force_cache_lookup_probe(
            d,
            n,
            output_cache_store_ok,
            vsapi
        );

        cnr3_for_debug_only_probe_recovery_plan(
            d,
            n,
            output_cache_store_ok
        );

        cnr3_for_debug_only_probe_recovery_walk_skeleton(
            d,
            n,
            output_cache_store_ok
        );

        cnr3_for_debug_only_probe_recovery_start_ref_skeleton(
            d,
            n,
            output_cache_store_ok,
            vsapi
        );

        cnr3_debug_print_output_cache_frame_trace(
            d,
            n,
            output_cache_store_ok,
            output_cache_prune_ok
        );

        if (
            !output_cache_store_ok ||
            !output_cache_prune_ok ||
            cnr3_should_print_frame_output_cache_summary(d, n)
            ) {
            cnr3_debug_print_output_cache_summary(
                d,
                "after CMS05-3A output_cache store/prune proving"
            );
        }

        if (
            CNR3_MEMORY_DIAG_FRAME_INTERVAL > 0 &&
            n > 0 &&
            (n % CNR3_MEMORY_DIAG_FRAME_INTERVAL) == 0
            ) {
            char memory_label[64];
            std::snprintf(
                memory_label,
                sizeof(memory_label),
                "frame=%d",
                n
            );

            cnr3_memory_record_and_print_snapshot(
                d->memory_stats,
                d->debug,
                d->instance_id,
                memory_label
            );
        }

        cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
            d,
            source_request_plan,
            "computed-frame-return"
        );

        cnr3_for_debug_only_destroy_bounded_warmup_source_plan_with_trace(
            d,
            h4_source_plan,
            "computed-frame-return"
        );

        return dst;
    }

    cnr3_for_debug_only_destroy_unexpected_frame_data_source_request_plan(
        d,
        frameData,
        activationReason
    );

    return nullptr;
}

// -----------------------------------------------------------------------------
// END CNR3 cache manager
// -----------------------------------------------------------------------------

static void VS_CC cnr3_create(
    const VSMap* in,
    VSMap* out,
    void* userData,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)userData;

    Cnr3Data* data = new Cnr3Data();
    Cnr3Data& local = *data;

    // an ID to identify and track instances
    local.instance_id = g_cnr3_next_instance_id.fetch_add(1);

    int err = 0;
    local.node = vsapi->mapGetNode(in, "clip", 0, &err);

    if (err || local.node == nullptr) {
        vsapi->mapSetError(out, "CNR3: clip is required.");
        delete data;
        return;
    }

    local.vi = vsapi->getVideoInfo(local.node);

    if (local.vi == nullptr) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: failed to get video info.");
        delete data;
        return;
    }

    if (!validate_cnr3_format(local.vi, out, vsapi)) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        delete data;
        return;
    }

    local.mode = get_optional_data_string(in, vsapi, "mode", "oxx");

    local.ln = static_cast<int>(get_optional_int(in, vsapi, "ln", 35));
    local.lm = static_cast<int>(get_optional_int(in, vsapi, "lm", 192));
    local.un = static_cast<int>(get_optional_int(in, vsapi, "un", 47));
    local.um = static_cast<int>(get_optional_int(in, vsapi, "um", 255));
    local.vn = static_cast<int>(get_optional_int(in, vsapi, "vn", 47));
    local.vm = static_cast<int>(get_optional_int(in, vsapi, "vm", 255));

    local.scdthr = get_optional_float(in, vsapi, "scdthr", 10.0);
    local.scene_chroma = get_optional_int(in, vsapi, "scene_chroma", 0) != 0;

    /*
        Development/maintenance option.

        Default to recursive Cnr2-style chroma blending enabled.

        blend=false remains available for maintenance/testing because it keeps
        the diagnostic read/table paths active while forcing chroma output to
        pass through unchanged.
    */
    local.blend = get_optional_int(in, vsapi, "blend", 1) != 0;

    local.debug = get_optional_int(in, vsapi, "debug", 0) != 0;

    if (local.mode.size() != 3) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: mode must be a 3-character string, for example \"oxx\".");
        delete data;
        return;
    }

    for (const char c : local.mode) {
        if (c != 'o' && c != 'x') {
            vsapi->freeNode(local.node);
            local.node = nullptr;
            vsapi->mapSetError(out, "CNR3: mode may contain only 'o' and 'x' characters.");
            delete data;
            return;
        }
    }

    if (
        local.ln < 0 ||
        local.lm < 0 ||
        local.un < 0 ||
        local.um < 0 ||
        local.vn < 0 ||
        local.vm < 0
        ) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: threshold parameters must be non-negative.");
        delete data;
        return;
    }

    if (local.scdthr < 0.0) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: scdthr must be non-negative.");
        delete data;
        return;
    }

    local.bits_per_sample = local.vi->format.bitsPerSample;
    local.sample_peak = (1 << local.bits_per_sample) - 1;

    /*
        Signed-difference table geometry.

        sample_peak:
            maximum legal sample value, for example 255 or 65535.

        table_offset:
            one greater than sample_peak, used to map signed differences into
            positive vector indexes.

        table_size:
            enough entries for all possible signed differences from
            -sample_peak through +sample_peak, plus the offset slot.

        Example for 8-bit:
            sample_peak  = 255
            table_offset = 256
            table_size   = 513
    */
    local.table_offset = local.sample_peak + 1;
    local.table_size = local.table_offset * 2 + 1;

    local.ln_scaled = scale_8bit_parameter_to_bit_depth(local.ln, local.bits_per_sample);
    local.lm_scaled = scale_8bit_parameter_to_bit_depth(local.lm, local.bits_per_sample);
    local.un_scaled = scale_8bit_parameter_to_bit_depth(local.un, local.bits_per_sample);
    local.um_scaled = scale_8bit_parameter_to_bit_depth(local.um, local.bits_per_sample);
    local.vn_scaled = scale_8bit_parameter_to_bit_depth(local.vn, local.bits_per_sample);
    local.vm_scaled = scale_8bit_parameter_to_bit_depth(local.vm, local.bits_per_sample);

    /*
        vscnr2-style scene-change threshold.

        vscnr2 uses:
            max_pixel_diff = scene_chroma
                ? (219 + 224 * 2) >> (subsw + subsh)
                : 219

            diff_max = (
                scdthr * width * height * max_pixel_diff / 100.0
            ) << (depth - 8)

        CNR3 keeps that model. The threshold is compared against a per-frame
        accumulated diff_total during process_cnr3_frame().
    */
    const int subsampling_shift =
        local.vi->format.subSamplingW +
        local.vi->format.subSamplingH;

    const int max_pixel_diff =
        (!local.scene_chroma)
        ? 219
        : ((219 + (224 * 2)) >> subsampling_shift);

    local.scene_change_threshold =
        static_cast<int64_t>(
            (
                local.scdthr *
                static_cast<double>(local.vi->width) *
                static_cast<double>(local.vi->height) *
                static_cast<double>(max_pixel_diff)
                ) /
            100.0
            ) << (local.bits_per_sample - 8);

    if (!build_cnr3_lookup_tables(local, out, vsapi)) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        delete data;
        return;
    }

    if (local.debug) {
        cnr3_debug_printf(
            local.debug,
            "CNR3 debug: instance=%d, edit_version=%s\n",
            local.instance_id,
            CNR3_EDIT_VERSION
        );

        const int y_mid = local.ln_scaled / 2;
        const int u_mid = local.un_scaled / 2;
        const int v_mid = local.vn_scaled / 2;

        cnr3_debug_printf(
            local.debug,
            "CNR3 debug: instance=%d, format=%d-bit YUV, peak=%d, "
            "table_offset=%d, table_size=%d, "
            "ln=%d->%d, lm=%d->%d, "
            "un=%d->%d, um=%d->%d, "
            "vn=%d->%d, vm=%d->%d, "
            "mode=%s, scdthr=%f, scene_chroma=%d, scene_change_threshold=%lld, blend=%d\n",
            local.instance_id,
            local.bits_per_sample,
            local.sample_peak,
            local.table_offset,
            local.table_size,
            local.ln,
            local.ln_scaled,
            local.lm,
            local.lm_scaled,
            local.un,
            local.un_scaled,
            local.um,
            local.um_scaled,
            local.vn,
            local.vn_scaled,
            local.vm,
            local.vm_scaled,
            local.mode.c_str(),
            local.scdthr,
            local.scene_chroma ? 1 : 0,
            static_cast<long long>(local.scene_change_threshold),
            local.blend ? 1 : 0
        );

        cnr3_debug_printf(
            local.debug,
            "CNR3 debug: instance=%d, table samples by signed diff: "
            "Y[0]=%d, Y[%d]=%d, Y[%d]=%d, Y[%d]=%d; "
            "U[0]=%d, U[%d]=%d, U[%d]=%d, U[%d]=%d; "
            "V[0]=%d, V[%d]=%d, V[%d]=%d, V[%d]=%d\n",
            local.instance_id,

            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                0
            ),
            y_mid,
            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                y_mid
            ),
            local.ln_scaled,
            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                local.ln_scaled
            ),
            local.sample_peak,
            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                local.sample_peak
            ),

            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                0
            ),
            u_mid,
            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                u_mid
            ),
            local.un_scaled,
            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                local.un_scaled
            ),
            local.sample_peak,
            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                local.sample_peak
            ),

            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                0
            ),
            v_mid,
            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                v_mid
            ),
            local.vn_scaled,
            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                local.vn_scaled
            ),
            local.sample_peak,
            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                local.sample_peak
            )
        );
    }

    cnr3_output_cache_set_ceiling(
        data->output_cache,
        data->vi
    );

    cnr3_debug_print_output_cache_summary(
        data,
        "after cnr3_create configuration before createVideoFilter"
    );

    cnr3_memory_record_and_print_snapshot(
        data->memory_stats,
        data->debug,
        data->instance_id,
        "at cnr3_create (baseline)",
        true
    );

    VSFilterDependency deps[] = {
        {data->node, rpGeneral}
    };

    vsapi->createVideoFilter(
        out,
        "CNR3",
        data->vi,
        cnr3_get_frame,
        cnr3_free,
        fmUnordered,
        deps,
        1,
        data,
        core
    );
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(
    VSPlugin* plugin,
    const VSPLUGINAPI* vspapi
) {
    vspapi->configPlugin(
        "org.vapoursynth.cnr3",
        "cnr3",
        "CNR3 recursive chroma stabiliser",
        VS_MAKE_VERSION(0, 1),
        VAPOURSYNTH_API_VERSION,
        0,
        plugin
    );

    vspapi->registerFunction(
        "CNR3",
        "clip:vnode;"
        "mode:data:opt;"
        "ln:int:opt;"
        "lm:int:opt;"
        "un:int:opt;"
        "um:int:opt;"
        "vn:int:opt;"
        "vm:int:opt;"
        "scdthr:float:opt;"
        "scene_chroma:int:opt;"
        "blend:int:opt;"
        "debug:int:opt;",
        "clip:vnode;",
        cnr3_create,
        nullptr,
        plugin
    );
}
