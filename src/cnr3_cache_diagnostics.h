#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cnr3_common.h"

/*
    CNR3 cache diagnostics scaffold.

    CMS07-B.2.4 keeps this module cache-specific.

    Generic stderr output belongs in cnr3_diagnostics.*. This module must not
    own generic print/flush helpers because memory diagnostics, VapourSynth
    integration, future proof summaries, and other modules also need diagnostic
    output without depending on cache diagnostics.

    This module is reserved for cache-specific diagnostic state and D-SUM
    support, for example:
        - cache integrity summaries;
        - ownership / pin / lookup-ref balance summaries;
        - store / duplicate-store summaries;
        - prune / eviction summaries;
        - hot-zone summaries;
        - recovery-search and recovery-plan summaries.

    CMS07-G.6A introduces only the D-SUM-11 hot-zone counter model. It does not
    format or print summaries, inspect cache internals, write stderr, or change
    cache behaviour.
*/

/*
    D-SUM-11 hot-zone diagnostic counter snapshot.

    This is a counter model only. It deliberately contains no formatting,
    printing, heap-owned strings, cache mutation authority, frame ownership, or
    control-flow decisions. Counter updates may occur inside CMS07 cache-lock
    scopes when the update is a minimal observation of the mutation already
    being performed. Human-readable summary formatting must be implemented in a
    later phase outside all cache locks.
*/
struct Cnr3CacheHotZoneDiagnosticStats {
    std::uint64_t hot_zone_updates = 0;
    std::uint64_t zones_created = 0;
    std::uint64_t zones_slid = 0;
    std::uint64_t zones_merged = 0;
    std::uint64_t zones_decayed = 0;
    std::uint64_t zones_expired = 0;

    bool have_zone_count_sample = false;
    std::size_t zone_count_min = 0;
    std::uint64_t zone_count_sum = 0;
    std::size_t zone_count_max = 0;
    std::uint64_t zone_count_samples = 0;

    bool have_protected_range_sample = false;
    int protected_range_min = CNR3_INVALID_FRAME_NUMBER;
    int protected_range_max = CNR3_INVALID_FRAME_NUMBER;

    std::uint64_t frames_rejected_from_prune_due_to_hot_zone = 0;
};


#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

inline constexpr std::size_t CNR3_CACHE_DIAG_DSUM10_GAP_HISTOGRAM_BIN_COUNT = 7U;
inline constexpr std::size_t CNR3_CACHE_DIAG_DSUM10_TOP_THRASH_CAPACITY = 16U;
inline constexpr std::size_t CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_MULTIPLIER = 16U;
inline constexpr std::size_t CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_FLOOR = 1024U;

struct Cnr3CachePruneDiagnosticRingEntry {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    std::uint64_t eviction_sequence = 0;
};

struct Cnr3CachePruneDiagnosticTopThrashEntry {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    std::uint64_t re_churn_count = 0;
};

/*
    D-SUM-10 prune/eviction diagnostic state.

    This is observe-only telemetry. It may be updated while the cache mutex is
    already held, but it must not decide prune candidates, pinning, lookup
    results, or return behaviour. Formatting and stderr output must use a
    by-value snapshot outside the cache lock.
*/
struct Cnr3CachePruneDiagnosticStats {
    std::uint64_t prune_invocations = 0;
    std::uint64_t prune_events_triggered = 0;
    std::uint64_t frames_evicted = 0;
    std::uint64_t bytes_evicted = 0;
    std::uint64_t checkpoint_prunes = 0;
    std::uint64_t hot_zone_rejected = 0;

    std::uint64_t frames_evicted_then_re_requested = 0;
    std::uint64_t frames_re_requested_repeatedly = 0;

    std::vector<Cnr3CachePruneDiagnosticRingEntry> recently_evicted_ring{};
    std::size_t ring_head = 0;
    std::size_t ring_live_count = 0;
    std::size_t ring_capacity = 0;
    int checkpoint_search_bound_B = 0;
    std::size_t active_ceiling = 0;
    std::size_t capacity_multiplier_k = CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_MULTIPLIER;
    std::uint64_t ring_wrap_count = 0;
    bool ring_saturated = false;
    std::uint64_t total_evicted_records = 0;

    std::uint64_t gap_histogram[CNR3_CACHE_DIAG_DSUM10_GAP_HISTOGRAM_BIN_COUNT] = {};
    std::array<
        Cnr3CachePruneDiagnosticTopThrashEntry,
        CNR3_CACHE_DIAG_DSUM10_TOP_THRASH_CAPACITY
    > top_thrashers{};
    std::size_t top_thrasher_count = 0;

    std::uint64_t window_dumps_emitted = 0;
    std::uint64_t full_dumps_emitted = 0;

#if defined(CNR3_DIAG_DSUM10_RING_WINDOW_DUMP)
    std::vector<int> ring_window_dump_entries{};
    std::size_t ring_window_dump_entry_count = 0;
#endif

#if defined(CNR3_DIAG_DSUM10_RING_FULL_DUMP)
    std::vector<int> ring_full_dump_entries{};
    std::size_t ring_full_dump_entry_count = 0;
#endif
};

void cnr3_cache_prune_diagnostic_configure(
    Cnr3CachePruneDiagnosticStats& stats,
    int checkpoint_search_bound_B,
    std::size_t active_ceiling
);

#endif

inline void cnr3_cache_diag_saturating_increment(
    std::uint64_t& value
) noexcept {
    if (value < UINT64_MAX) {
        ++value;
    }
}

inline void cnr3_cache_diag_saturating_add(
    std::uint64_t& value,
    std::uint64_t increment
) noexcept {
    if (increment == 0U) {
        return;
    }

    if (value <= (UINT64_MAX - increment)) {
        value += increment;
    }
    else {
        value = UINT64_MAX;
    }
}

inline void cnr3_cache_hot_zone_diagnostic_observe_create(
    Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.hot_zone_updates);
    cnr3_cache_diag_saturating_increment(stats.zones_created);
}

inline void cnr3_cache_hot_zone_diagnostic_observe_slide(
    Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.hot_zone_updates);
    cnr3_cache_diag_saturating_increment(stats.zones_slid);
}

inline void cnr3_cache_hot_zone_diagnostic_observe_merge(
    Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.hot_zone_updates);
    cnr3_cache_diag_saturating_increment(stats.zones_merged);
}

inline void cnr3_cache_hot_zone_diagnostic_observe_decay(
    Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.hot_zone_updates);
    cnr3_cache_diag_saturating_increment(stats.zones_decayed);
}

inline void cnr3_cache_hot_zone_diagnostic_observe_expiry(
    Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.hot_zone_updates);
    cnr3_cache_diag_saturating_increment(stats.zones_expired);
}

inline void cnr3_cache_hot_zone_diagnostic_observe_zone_count_sample(
    Cnr3CacheHotZoneDiagnosticStats& stats,
    std::size_t zone_count
) noexcept {
    if (!stats.have_zone_count_sample) {
        stats.have_zone_count_sample = true;
        stats.zone_count_min = zone_count;
        stats.zone_count_max = zone_count;
    }
    else {
        if (zone_count < stats.zone_count_min) {
            stats.zone_count_min = zone_count;
        }

        if (zone_count > stats.zone_count_max) {
            stats.zone_count_max = zone_count;
        }
    }

    if (stats.zone_count_sum <= (UINT64_MAX - zone_count)) {
        stats.zone_count_sum += zone_count;
    }
    else {
        stats.zone_count_sum = UINT64_MAX;
    }

    cnr3_cache_diag_saturating_increment(stats.zone_count_samples);
}

inline void cnr3_cache_hot_zone_diagnostic_observe_protected_range_sample(
    Cnr3CacheHotZoneDiagnosticStats& stats,
    int protected_range
) noexcept {
    if (protected_range < 0) {
        return;
    }

    if (!stats.have_protected_range_sample) {
        stats.have_protected_range_sample = true;
        stats.protected_range_min = protected_range;
        stats.protected_range_max = protected_range;
        return;
    }

    if (protected_range < stats.protected_range_min) {
        stats.protected_range_min = protected_range;
    }

    if (protected_range > stats.protected_range_max) {
        stats.protected_range_max = protected_range;
    }
}

inline void cnr3_cache_hot_zone_diagnostic_observe_prune_rejections(
    Cnr3CacheHotZoneDiagnosticStats& stats,
    std::uint64_t rejected_frame_count
) noexcept {
    cnr3_cache_diag_saturating_add(
        stats.frames_rejected_from_prune_due_to_hot_zone,
        rejected_frame_count
    );
}

#if defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION)

void cnr3_cache_prune_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CachePruneDiagnosticStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM11_HOT_ZONE)

void cnr3_cache_hot_zone_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept;

#endif
