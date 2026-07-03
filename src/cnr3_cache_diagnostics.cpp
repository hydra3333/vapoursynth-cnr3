#include "cnr3_cache_diagnostics.h"

#include "cnr3_diagnostics.h"

#include <algorithm>
#include <cstdio>
#include <limits>

/*
    Cache-specific D-SUM formatting lives here. Callers must hand this module a
    by-value snapshot or call only after taking a snapshot outside any stderr
    write path. No function in this file may inspect or mutate cache policy.
*/

namespace {

    void cnr3_cache_diag_write_uint64_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_name,
        const char* label,
        std::uint64_t value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %llu",
            dsum_name != nullptr ? dsum_name : "D-SUM-??",
            label != nullptr ? label : "(null)",
            static_cast<unsigned long long>(value)
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';

        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_cache_diag_write_size_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_name,
        const char* label,
        std::size_t value
    ) noexcept {
        cnr3_cache_diag_write_uint64_row(
            instance_id,
            component,
            dsum_name,
            label,
            static_cast<std::uint64_t>(value)
        );
    }

    void cnr3_cache_diag_write_int_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_name,
        const char* label,
        int value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %d",
            dsum_name != nullptr ? dsum_name : "D-SUM-??",
            label != nullptr ? label : "(null)",
            value
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';

        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_cache_diag_write_text_line(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* message
    ) noexcept {
        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_cache_diag_write_int_series_line(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* tag,
        const char* label,
        const std::vector<int>& values,
        std::size_t start,
        std::size_t count
    ) noexcept {
        char message[768] = {};
        std::size_t used = 0U;

        int written = std::snprintf(
            message,
            sizeof(message),
            "%s %s [%llu..%llu):",
            tag != nullptr ? tag : "[DSUM10-RING]",
            label != nullptr ? label : "entries",
            static_cast<unsigned long long>(start),
            static_cast<unsigned long long>(start + count)
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                component,
                "[DSUM-SUMMARY] formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        used = static_cast<std::size_t>(written);
        if (used >= sizeof(message)) {
            used = sizeof(message) - 1U;
        }

        for (std::size_t i = 0U; i < count && (start + i) < values.size(); ++i) {
            written = std::snprintf(
                message + used,
                sizeof(message) - used,
                " %d",
                values[start + i]
            );

            if (written < 0) {
                break;
            }

            const std::size_t add = static_cast<std::size_t>(written);
            if (add >= (sizeof(message) - used)) {
                used = sizeof(message) - 1U;
                break;
            }

            used += add;
        }

        message[sizeof(message) - 1U] = '\0';

        cnr3_diag_write_line(
            instance_id,
            Cnr3DiagnosticLevel::info,
            component,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

    [[nodiscard]] std::size_t cnr3_cache_diag_dsum10_safe_capacity(
        int checkpoint_search_bound_B,
        std::size_t active_ceiling
    ) noexcept {
        const std::size_t positive_B =
            checkpoint_search_bound_B > 0
            ? static_cast<std::size_t>(checkpoint_search_bound_B)
            : 0U;
        const std::size_t base = std::max(positive_B, active_ceiling);
        const std::size_t k = CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_MULTIPLIER;

        std::size_t derived = CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_FLOOR;

        if (base != 0U) {
            if (base > (std::numeric_limits<std::size_t>::max() / k)) {
                derived = std::numeric_limits<std::size_t>::max();
            }
            else {
                derived = base * k;
            }
        }

        return std::max(derived, CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_FLOOR);
    }

#endif

} // namespace

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

void cnr3_cache_prune_diagnostic_configure(
    Cnr3CachePruneDiagnosticStats& stats,
    int checkpoint_search_bound_B,
    std::size_t active_ceiling
) {
    const std::size_t ring_capacity = cnr3_cache_diag_dsum10_safe_capacity(
        checkpoint_search_bound_B,
        active_ceiling
    );
    // const std::size_t ring_capacity = 65536U; // MANUAL OVERRIDE: uncomment to force a fixed D-SUM-10 ring size.

    stats = Cnr3CachePruneDiagnosticStats{};
    stats.checkpoint_search_bound_B = checkpoint_search_bound_B;
    stats.active_ceiling = active_ceiling;
    stats.capacity_multiplier_k = CNR3_CACHE_DIAG_DSUM10_RING_CAPACITY_MULTIPLIER;
    stats.ring_capacity = ring_capacity;
    stats.recently_evicted_ring.assign(ring_capacity, Cnr3CachePruneDiagnosticRingEntry{});

#if defined(CNR3_DIAG_DSUM10_RING_WINDOW_DUMP)
    stats.ring_window_dump_entries.assign(
        static_cast<std::size_t>(CNR3_DIAG_DSUM10_RING_WINDOW_SIZE) *
            static_cast<std::size_t>(CNR3_DIAG_DSUM10_RING_WINDOW_MAX_DUMPS),
        CNR3_INVALID_FRAME_NUMBER
    );
#endif

#if defined(CNR3_DIAG_DSUM10_RING_FULL_DUMP)
    stats.ring_full_dump_entries.assign(
        ring_capacity * static_cast<std::size_t>(CNR3_DIAG_DSUM10_RING_FULL_MAX_DUMPS),
        CNR3_INVALID_FRAME_NUMBER
    );
#endif
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM10_PRUNE_EVICTION)

void cnr3_cache_prune_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CachePruneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-10",
        "[DSUM-SUMMARY] D-SUM-10 prune/eviction and re-churn summary"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-10",
        "[DSUM-SUMMARY] D-SUM-10 interpretation: small-gap re-churn suggests tunable over-eviction; broad large-gap churn suggests arrival disorder"
    );

    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "prune_invocations", stats.prune_invocations);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "prune_events_triggered", stats.prune_events_triggered);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "frames_evicted", stats.frames_evicted);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "bytes_evicted", stats.bytes_evicted);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "checkpoint_prunes", stats.checkpoint_prunes);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "hot_zone_rejected", stats.hot_zone_rejected);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "frames_evicted_then_re_requested", stats.frames_evicted_then_re_requested);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "frames_re_requested_repeatedly", stats.frames_re_requested_repeatedly);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-10", "D-SUM-10", "ring_capacity", stats.ring_capacity);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-10", "D-SUM-10", "checkpoint_search_bound_B", stats.checkpoint_search_bound_B);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-10", "D-SUM-10", "active_ceiling", stats.active_ceiling);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-10", "D-SUM-10", "capacity_multiplier_k", stats.capacity_multiplier_k);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "ring_wrap_count", stats.ring_wrap_count);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "total_evicted_records", stats.total_evicted_records);

    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-10",
        stats.ring_saturated
            ? "[DSUM-SUMMARY] D-SUM-10 re-churn >= reported value; ring saturated, count is a lower bound; consider larger capacity"
            : "[DSUM-SUMMARY] D-SUM-10 ring_saturated false; re-churn count is not saturation-limited"
    );

    char message[384] = {};
    std::snprintf(
        message,
        sizeof(message),
        "[DSUM10-GAP-HISTO] 1-10=%llu 11-50=%llu 51-100=%llu 101-500=%llu 501-1000=%llu 1001-5000=%llu 5000+=%llu",
        static_cast<unsigned long long>(stats.gap_histogram[0]),
        static_cast<unsigned long long>(stats.gap_histogram[1]),
        static_cast<unsigned long long>(stats.gap_histogram[2]),
        static_cast<unsigned long long>(stats.gap_histogram[3]),
        static_cast<unsigned long long>(stats.gap_histogram[4]),
        static_cast<unsigned long long>(stats.gap_histogram[5]),
        static_cast<unsigned long long>(stats.gap_histogram[6])
    );
    message[sizeof(message) - 1U] = '\0';
    cnr3_cache_diag_write_text_line(instance_id, "D-SUM-10", message);

    char top_message[512] = {};
    std::size_t used = 0U;
    int written = std::snprintf(
        top_message,
        sizeof(top_message),
        "[DSUM10-TOP-THRASH] count=%llu",
        static_cast<unsigned long long>(stats.top_thrasher_count)
    );
    if (written > 0) {
        used = static_cast<std::size_t>(written);
        if (used >= sizeof(top_message)) {
            used = sizeof(top_message) - 1U;
        }

        for (std::size_t i = 0U; i < stats.top_thrasher_count && i < stats.top_thrashers.size(); ++i) {
            const Cnr3CachePruneDiagnosticTopThrashEntry& entry = stats.top_thrashers[i];
            if (!cnr3_frame_number_is_valid(entry.frame_number) || entry.re_churn_count == 0U) {
                continue;
            }

            written = std::snprintf(
                top_message + used,
                sizeof(top_message) - used,
                " frame=%d:%llux",
                entry.frame_number,
                static_cast<unsigned long long>(entry.re_churn_count)
            );

            if (written < 0) {
                break;
            }

            const std::size_t add = static_cast<std::size_t>(written);
            if (add >= (sizeof(top_message) - used)) {
                used = sizeof(top_message) - 1U;
                break;
            }
            used += add;
        }
    }
    top_message[sizeof(top_message) - 1U] = '\0';
    cnr3_cache_diag_write_text_line(instance_id, "D-SUM-10", top_message);

#if defined(CNR3_DIAG_DSUM10_RING_WINDOW_DUMP)
    if (stats.ring_window_dump_entry_count == 0U) {
        cnr3_cache_diag_write_text_line(
            instance_id,
            "D-SUM-10",
            "[DSUM10-RING-WINDOW] entries=0 fired=0"
        );
    }
    else {
        const std::size_t chunk_size = static_cast<std::size_t>(CNR3_DIAG_DSUM10_RING_WINDOW_SIZE);
        const std::size_t entry_count = std::min(
            stats.ring_window_dump_entry_count,
            stats.ring_window_dump_entries.size()
        );
        for (std::size_t start = 0U; start < entry_count; start += chunk_size) {
            const std::size_t count = std::min(chunk_size, entry_count - start);
            cnr3_cache_diag_write_int_series_line(
                instance_id,
                "D-SUM-10",
                "[DSUM10-RING-WINDOW]",
                "entries",
                stats.ring_window_dump_entries,
                start,
                count
            );
        }
    }
#endif

#if defined(CNR3_DIAG_DSUM10_RING_FULL_DUMP)
    if (stats.ring_full_dump_entry_count > 0U) {
        const std::size_t chunk_size = 100U;
        const std::size_t entry_count = std::min(
            stats.ring_full_dump_entry_count,
            stats.ring_full_dump_entries.size()
        );
        for (std::size_t start = 0U; start < entry_count; start += chunk_size) {
            const std::size_t count = std::min(chunk_size, entry_count - start);
            cnr3_cache_diag_write_int_series_line(
                instance_id,
                "D-SUM-10",
                "[DSUM10-RING-FULL]",
                "entries",
                stats.ring_full_dump_entries,
                start,
                count
            );
        }
    }
#endif

#if defined(CNR3_DIAG_DSUM10_RING_FINAL_DUMP)
    const std::size_t final_limit = static_cast<std::size_t>(CNR3_DIAG_DSUM10_RING_FINAL_COUNT);
    const std::size_t live_count = std::min(stats.ring_live_count, stats.recently_evicted_ring.size());
    const std::size_t final_count = std::min(final_limit, live_count);
    const std::size_t oldest_index =
        (stats.ring_live_count < stats.ring_capacity) ? 0U : stats.ring_head;
    const std::size_t skip_count = live_count - final_count;

    char final_header[256] = {};
    std::snprintf(
        final_header,
        sizeof(final_header),
        "[DSUM10-RING-FINAL] entries=%llu saturated=%s (showing last %llu of total_evicted=%llu)",
        static_cast<unsigned long long>(final_count),
        stats.ring_saturated ? "yes" : "no",
        static_cast<unsigned long long>(final_count),
        static_cast<unsigned long long>(stats.total_evicted_records)
    );
    final_header[sizeof(final_header) - 1U] = '\0';
    cnr3_cache_diag_write_text_line(instance_id, "D-SUM-10", final_header);

    constexpr std::size_t final_chunk_size = 50U;
    for (std::size_t chunk_start = 0U; chunk_start < final_count; chunk_start += final_chunk_size) {
        const std::size_t chunk_count = std::min(final_chunk_size, final_count - chunk_start);
        char final_line[768] = {};
        std::size_t used = 0U;

        int written = std::snprintf(
            final_line,
            sizeof(final_line),
            "[DSUM10-RING-FINAL] entries [%llu..%llu):",
            static_cast<unsigned long long>(chunk_start),
            static_cast<unsigned long long>(chunk_start + chunk_count)
        );

        if (written < 0) {
            continue;
        }

        used = static_cast<std::size_t>(written);
        if (used >= sizeof(final_line)) {
            used = sizeof(final_line) - 1U;
        }

        for (std::size_t i = 0U; i < chunk_count; ++i) {
            const std::size_t logical_index = skip_count + chunk_start + i;
            const std::size_t ring_index =
                stats.ring_capacity != 0U
                ? ((oldest_index + logical_index) % stats.ring_capacity)
                : 0U;
            const int frame_number =
                ring_index < stats.recently_evicted_ring.size()
                ? stats.recently_evicted_ring[ring_index].frame_number
                : CNR3_INVALID_FRAME_NUMBER;

            written = std::snprintf(
                final_line + used,
                sizeof(final_line) - used,
                " %d",
                frame_number
            );

            if (written < 0) {
                break;
            }

            const std::size_t add = static_cast<std::size_t>(written);
            if (add >= (sizeof(final_line) - used)) {
                used = sizeof(final_line) - 1U;
                break;
            }

            used += add;
        }

        final_line[sizeof(final_line) - 1U] = '\0';
        cnr3_cache_diag_write_text_line(instance_id, "D-SUM-10", final_line);
    }
#endif

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM11_HOT_ZONE)

void cnr3_cache_hot_zone_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheHotZoneDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-11",
        "[DSUM-SUMMARY] D-SUM-11 hot-zone operation summary"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-11",
        "[DSUM-SUMMARY] D-SUM-11 interpretation: hot zones are prune-policy hints only; pins prove active liveness"
    );

    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "hot_zone_updates", stats.hot_zone_updates);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zones_created", stats.zones_created);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zones_slid", stats.zones_slid);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zones_merged", stats.zones_merged);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zones_decayed", stats.zones_decayed);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zones_expired", stats.zones_expired);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-11", "D-SUM-11", "zone_count_min", stats.have_zone_count_sample ? stats.zone_count_min : 0U);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zone_count_sum", stats.zone_count_sum);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "zone_count_samples", stats.zone_count_samples);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-11", "D-SUM-11", "zone_count_mean", stats.zone_count_samples != 0U ? static_cast<std::size_t>(stats.zone_count_sum / stats.zone_count_samples) : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-11", "D-SUM-11", "zone_count_max", stats.have_zone_count_sample ? stats.zone_count_max : 0U);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-11", "D-SUM-11", "protected_range_min", stats.have_protected_range_sample ? stats.protected_range_min : CNR3_INVALID_FRAME_NUMBER);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-11", "D-SUM-11", "protected_range_max", stats.have_protected_range_sample ? stats.protected_range_max : CNR3_INVALID_FRAME_NUMBER);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-11", "D-SUM-11", "frames_rejected_from_prune_due_to_hot_zone", stats.frames_rejected_from_prune_due_to_hot_zone);

    cnr3_diag_flush_stderr();
}

#endif
