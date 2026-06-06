/*
    CNR3 - VapourSynth API4 chroma stabiliser, based on the venerable CNR2/VSCNR2.

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
    */

    if constexpr (!CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON) {
        (void)d;
        (void)frameData;
        (void)activationReason;
        return;
    }
    else {
        if (frameData == nullptr || *frameData == nullptr) {
            return;
        }

        Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan =
            static_cast<Cnr3ForDebugOnlyRecoverySourceRequestPlan*>(*frameData);

        *frameData = nullptr;

        cnr3_debug_printf(
            d != nullptr ? d->debug : false,
            "output-cache # cnr3_get_frame # FOR-DEBUG-ONLY-SOURCE-REQUEST-PLAN-UNEXPECTED-FALLBACK # instance=%d # activation_reason=%d\n",
            d != nullptr ? d->instance_id : -1,
            activationReason
        );

        cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
            d,
            source_request_plan,
            "unexpected-activation-fallback"
        );
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

        Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan =
            nullptr;

        if (frameData != nullptr) {
            source_request_plan =
                cnr3_for_debug_only_create_recovery_source_request_plan(
                    d,
                    n
                );

            *frameData = source_request_plan;
        }

        if (source_request_plan != nullptr) {
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

        Cnr3ForDebugOnlyRecoverySourceRequestPlan* source_request_plan =
            nullptr;

        if (frameData != nullptr) {
            source_request_plan =
                static_cast<Cnr3ForDebugOnlyRecoverySourceRequestPlan*>(*frameData);
            *frameData = nullptr;
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

            vsapi->setFilterError(
                "CNR3: debug-only widened source-request proof failed to retrieve an extra source frame.",
                frameCtx
            );

            return nullptr;
        }

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

        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);

        if (src == nullptr) {
            cnr3_for_debug_only_destroy_recovery_source_request_plan_with_trace(
                d,
                source_request_plan,
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

            vsapi->freeFrame(src);
            vsapi->setFilterError("CNR3: failed to allocate destination frame.", frameCtx);
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

            vsapi->freeFrame(dst);

            vsapi->setFilterError(
                "CNR3: debug-only recovery source-frame-set proof failed.",
                frameCtx
            );

            return nullptr;
        }

        /*
            CMS05-3A store/prune-only runtime proving.

            The CMS05 output cache is not output-authoritative in this phase.
            The already-produced dst frame remains the frame returned to
            VapourSynth.

            Purpose:
                - store the produced output frame in output_cache;
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
