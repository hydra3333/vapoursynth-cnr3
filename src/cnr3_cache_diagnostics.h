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

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)

/*
    D-SUM-04 ownership-balance and lookup-rate diagnostic state.

    DIAG.2b deliberately observes narrow cache-core balances: slot pins
    and lookup-ref handoff. The lookup query/hit counters are diagnostic
    rates only; they do not claim a global VSFrame ownership balance.
*/
struct Cnr3CacheLookupSiteDiagnosticStats {
    std::uint64_t invocations = 0;
    std::uint64_t looks_counted = 0;
    std::uint64_t hits_counted = 0;
    std::uint64_t misses_counted = 0;
};

struct Cnr3CacheOwnershipDiagnosticStats {
    std::uint64_t pins_acquired = 0;
    std::uint64_t pins_released = 0;
    int total_pin_count_crosscheck = 0;

    std::uint64_t cache_lookup_queries_total = 0;
    std::uint64_t cache_lookup_hits = 0;

    Cnr3CacheLookupSiteDiagnosticStats site1_requested_frame_check{};
    Cnr3CacheLookupSiteDiagnosticStats site2_predecessor_fastpath{};
    Cnr3CacheLookupSiteDiagnosticStats site3_recovery_walk{};
    Cnr3CacheLookupSiteDiagnosticStats site4_hole_catalogue_scan{};
    Cnr3CacheLookupSiteDiagnosticStats site5_anchor_repin{};
    Cnr3CacheLookupSiteDiagnosticStats site6_reacquire_already_pinned{};
    Cnr3CacheLookupSiteDiagnosticStats site7a_floor_adopt_bail_early{};
    Cnr3CacheLookupSiteDiagnosticStats site7b_hole_adopt_bail_early{};
    Cnr3CacheLookupSiteDiagnosticStats site8a_plain_store_duplicate_check{};
    Cnr3CacheLookupSiteDiagnosticStats site8b_as2_store_duplicate_check{};
    Cnr3CacheLookupSiteDiagnosticStats site9_duplicate_winner_reacquire{};

    std::uint64_t lookup_refs_acquired = 0;
    std::uint64_t lookup_refs_released_by_cache_core = 0;
    std::uint64_t lookup_refs_transferred = 0;
    std::uint64_t ownership_errors = 0;
};

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)

/*
    D-SUM-05 cache-integrity diagnostic state.

    This observes the existing central cache-state invariant predicate. It
    must not add guards, change predicate results, or acquire locks from the
    predicate's lock-protected context.
*/
struct Cnr3CacheIntegrityDiagnosticStats {
    std::uint64_t invariant_checks_performed = 0;
    std::uint64_t invariant_violations_detected = 0;
    const char* first_violation_site = nullptr;

    bool have_structural_sample = false;
    std::size_t slot_count_min = 0;
    std::size_t slot_count_max = 0;
    std::size_t checkpoint_count_min = 0;
    std::size_t checkpoint_count_max = 0;
    std::size_t non_checkpoint_count_min = 0;
    std::size_t non_checkpoint_count_max = 0;
    std::size_t checkpoint_retain_headroom_min = 0;
    int total_pin_count_min = 0;
    int total_pin_count_max = 0;

    bool have_summary_sample = false;
    std::size_t summary_slot_count = 0;
    std::size_t summary_checkpoint_count = 0;
    std::size_t summary_non_checkpoint_count = 0;
    std::size_t summary_checkpoint_retain_headroom = 0;
    int summary_total_pin_count = 0;
};

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)

inline constexpr std::size_t CNR3_CACHE_DIAG_DSUM08_STORE_KIND_COUNT = 4U;

/*
    D-SUM-08 cache-store diagnostic state.

    This is wrapper-level store outcome telemetry. Duplicate is a healthy
    first-in-best-dressed outcome and is not counted as store failure.
*/
struct Cnr3CacheStoreDiagnosticStats {
    std::uint64_t stores_total = 0;
    std::uint64_t stores_by_kind[CNR3_CACHE_DIAG_DSUM08_STORE_KIND_COUNT] = {};
    std::uint64_t duplicates_seen = 0;
    std::uint64_t incoming_rejected = 0;
    std::uint64_t as2_checkpoint_promotions = 0;
    std::uint64_t store_failures = 0;
};

#endif



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

    std::uint64_t frames_recently_evicted_then_re_requested = 0;
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

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)

inline void cnr3_cache_ownership_diagnostic_observe_pin_acquired(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.pins_acquired);
}

inline void cnr3_cache_ownership_diagnostic_observe_pin_released(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.pins_released);
}

inline void cnr3_cache_ownership_diagnostic_observe_cache_lookup_query(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.cache_lookup_queries_total);
}

inline void cnr3_cache_ownership_diagnostic_observe_cache_lookup_hit(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.cache_lookup_hits);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_site_invocation(
    Cnr3CacheLookupSiteDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.invocations);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_site_look(
    Cnr3CacheLookupSiteDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.looks_counted);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_site_hit(
    Cnr3CacheLookupSiteDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.hits_counted);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_site_miss(
    Cnr3CacheLookupSiteDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.misses_counted);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_ref_acquired(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.lookup_refs_acquired);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_ref_released_by_cache_core(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.lookup_refs_released_by_cache_core);
}

inline void cnr3_cache_ownership_diagnostic_observe_lookup_ref_transferred(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.lookup_refs_transferred);
}

inline void cnr3_cache_ownership_diagnostic_observe_ownership_error(
    Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.ownership_errors);
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)

inline void cnr3_cache_integrity_diagnostic_observe_structural_sample(
    Cnr3CacheIntegrityDiagnosticStats& stats,
    std::size_t slot_count,
    std::size_t checkpoint_count,
    std::size_t checkpoint_retain_headroom,
    int total_pin_count
) noexcept {
    const std::size_t non_checkpoint_count =
        slot_count >= checkpoint_count ? slot_count - checkpoint_count : 0U;

    if (!stats.have_structural_sample) {
        stats.have_structural_sample = true;
        stats.slot_count_min = slot_count;
        stats.slot_count_max = slot_count;
        stats.checkpoint_count_min = checkpoint_count;
        stats.checkpoint_count_max = checkpoint_count;
        stats.non_checkpoint_count_min = non_checkpoint_count;
        stats.non_checkpoint_count_max = non_checkpoint_count;
        stats.checkpoint_retain_headroom_min = checkpoint_retain_headroom;
        stats.total_pin_count_min = total_pin_count;
        stats.total_pin_count_max = total_pin_count;
        return;
    }

    if (slot_count < stats.slot_count_min) {
        stats.slot_count_min = slot_count;
    }
    if (slot_count > stats.slot_count_max) {
        stats.slot_count_max = slot_count;
    }

    if (checkpoint_count < stats.checkpoint_count_min) {
        stats.checkpoint_count_min = checkpoint_count;
    }
    if (checkpoint_count > stats.checkpoint_count_max) {
        stats.checkpoint_count_max = checkpoint_count;
    }

    if (non_checkpoint_count < stats.non_checkpoint_count_min) {
        stats.non_checkpoint_count_min = non_checkpoint_count;
    }
    if (non_checkpoint_count > stats.non_checkpoint_count_max) {
        stats.non_checkpoint_count_max = non_checkpoint_count;
    }

    if (checkpoint_retain_headroom < stats.checkpoint_retain_headroom_min) {
        stats.checkpoint_retain_headroom_min = checkpoint_retain_headroom;
    }

    if (total_pin_count < stats.total_pin_count_min) {
        stats.total_pin_count_min = total_pin_count;
    }
    if (total_pin_count > stats.total_pin_count_max) {
        stats.total_pin_count_max = total_pin_count;
    }
}

inline void cnr3_cache_integrity_diagnostic_set_summary_sample(
    Cnr3CacheIntegrityDiagnosticStats& stats,
    std::size_t slot_count,
    std::size_t checkpoint_count,
    std::size_t checkpoint_retain_headroom,
    int total_pin_count
) noexcept {
    stats.have_summary_sample = true;
    stats.summary_slot_count = slot_count;
    stats.summary_checkpoint_count = checkpoint_count;
    stats.summary_non_checkpoint_count =
        slot_count >= checkpoint_count ? slot_count - checkpoint_count : 0U;
    stats.summary_checkpoint_retain_headroom = checkpoint_retain_headroom;
    stats.summary_total_pin_count = total_pin_count;
}

inline void cnr3_cache_integrity_diagnostic_observe_check(
    Cnr3CacheIntegrityDiagnosticStats& stats,
    std::size_t slot_count,
    std::size_t checkpoint_count,
    std::size_t checkpoint_retain_headroom,
    int total_pin_count
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.invariant_checks_performed);
    cnr3_cache_integrity_diagnostic_observe_structural_sample(
        stats,
        slot_count,
        checkpoint_count,
        checkpoint_retain_headroom,
        total_pin_count
    );
}

inline void cnr3_cache_integrity_diagnostic_observe_failure(
    Cnr3CacheIntegrityDiagnosticStats& stats,
    const char* site
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.invariant_violations_detected);

    if (stats.first_violation_site == nullptr) {
        stats.first_violation_site = site;
    }
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)

inline void cnr3_cache_store_diagnostic_observe_store(
    Cnr3CacheStoreDiagnosticStats& stats,
    std::size_t store_kind_index,
    bool duplicate_seen,
    bool incoming_rejected,
    bool as2_checkpoint_promoted,
    bool store_failed
) noexcept {
    cnr3_cache_diag_saturating_increment(stats.stores_total);

    if (store_kind_index < CNR3_CACHE_DIAG_DSUM08_STORE_KIND_COUNT) {
        cnr3_cache_diag_saturating_increment(stats.stores_by_kind[store_kind_index]);
    }

    if (duplicate_seen) {
        cnr3_cache_diag_saturating_increment(stats.duplicates_seen);
    }

    if (incoming_rejected) {
        cnr3_cache_diag_saturating_increment(stats.incoming_rejected);
    }

    if (as2_checkpoint_promoted) {
        cnr3_cache_diag_saturating_increment(stats.as2_checkpoint_promotions);
    }

    if (store_failed) {
        cnr3_cache_diag_saturating_increment(stats.store_failures);
    }
}

#endif


#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)

void cnr3_cache_ownership_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheOwnershipDiagnosticStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY)

void cnr3_cache_integrity_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheIntegrityDiagnosticStats& stats
) noexcept;

#endif

#if defined(CNR3_DIAG_PRINT_DSUM08_CACHE_STORE)

void cnr3_cache_store_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheStoreDiagnosticStats& stats
) noexcept;

#endif


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
