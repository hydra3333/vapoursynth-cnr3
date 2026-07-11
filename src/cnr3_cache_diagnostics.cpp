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

    void cnr3_cache_diag_write_int64_row(
        Cnr3InstanceId instance_id,
        const char* component,
        const char* dsum_name,
        const char* label,
        std::int64_t value
    ) noexcept {
        char message[224] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] %s %-44s %lld",
            dsum_name != nullptr ? dsum_name : "D-SUM-??",
            label != nullptr ? label : "(null)",
            static_cast<long long>(value)
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


#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)

    [[nodiscard]] std::uint64_t cnr3_cache_diag_lookup_misses(
        std::uint64_t looks,
        std::uint64_t hits
    ) noexcept {
        return looks >= hits ? (looks - hits) : 0U;
    }

    void cnr3_cache_diag_format_delta(
        char* buffer,
        std::size_t buffer_size,
        std::uint64_t sum,
        std::uint64_t total
    ) noexcept {
        if (buffer == nullptr || buffer_size == 0U) {
            return;
        }

        const char* sign = "+";
        std::uint64_t magnitude = 0;

        if (sum >= total) {
            magnitude = sum - total;
        }
        else {
            sign = "-";
            magnitude = total - sum;
        }

        const int written = std::snprintf(
            buffer,
            buffer_size,
            "%s%llu",
            sign,
            static_cast<unsigned long long>(magnitude)
        );

        if (written < 0) {
            buffer[0] = '\0';
            return;
        }

        buffer[buffer_size - 1U] = '\0';
    }

    void cnr3_cache_diag_write_lookup_site_row(
        Cnr3InstanceId instance_id,
        const char* label,
        const Cnr3CacheLookupSiteDiagnosticStats& stats,
        bool excluded_from_totals,
        const char* purpose
    ) noexcept {
        char message[512] = {};

        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-04 %-38s invocations=%llu looks=%llu hits=%llu misses=%llu%s",
            label != nullptr ? label : "(null)",
            static_cast<unsigned long long>(stats.invocations),
            static_cast<unsigned long long>(stats.looks_counted),
            static_cast<unsigned long long>(stats.hits_counted),
            static_cast<unsigned long long>(stats.misses_counted),
            excluded_from_totals ? " (excluded from totals)" : ""
        );

        if (written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                "D-SUM-04",
                "[DSUM-SUMMARY] D-SUM-04 lookup-site row formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        message[sizeof(message) - 1U] = '\0';
        cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", message);

        char purpose_message[640] = {};
        const int purpose_written = std::snprintf(
            purpose_message,
            sizeof(purpose_message),
            "[DSUM-SUMMARY] D-SUM-04    -> %s",
            purpose != nullptr ? purpose : "no purpose text supplied."
        );

        if (purpose_written < 0) {
            cnr3_diag_write_line(
                instance_id,
                Cnr3DiagnosticLevel::error,
                "D-SUM-04",
                "[DSUM-SUMMARY] D-SUM-04 lookup-site purpose formatting_error",
                Cnr3StderrFlushPolicy::no_flush
            );
            return;
        }

        purpose_message[sizeof(purpose_message) - 1U] = '\0';
        cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", purpose_message);
    }

    void cnr3_cache_diag_accumulate_lookup_site(
        const Cnr3CacheLookupSiteDiagnosticStats& stats,
        std::uint64_t& looks,
        std::uint64_t& hits,
        std::uint64_t& misses
    ) noexcept {
        cnr3_cache_diag_saturating_add(looks, stats.looks_counted);
        cnr3_cache_diag_saturating_add(hits, stats.hits_counted);
        cnr3_cache_diag_saturating_add(misses, stats.misses_counted);
    }

    [[nodiscard]] std::uint64_t cnr3_cache_diag_lifecycle_bucket_sum(
        const Cnr3FrameLifecycleOriginDiagnosticStats& stats
    ) noexcept {
        std::uint64_t total = 0;
        cnr3_cache_diag_saturating_add(total, stats.frame0_fresh_start);
        cnr3_cache_diag_saturating_add(total, stats.floor_fresh_start);
        cnr3_cache_diag_saturating_add(total, stats.ordinary_target);
        cnr3_cache_diag_saturating_add(total, stats.recovery_hole);
        cnr3_cache_diag_saturating_add(total, stats.recovery_target);
        return total;
    }

    void cnr3_cache_diag_write_lifecycle_value_line(
        Cnr3InstanceId instance_id,
        const char* label,
        std::uint64_t value,
        const char* suffix
    ) noexcept {
        char message[640] = {};
        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-04     %-50s = %llu%s",
            label != nullptr ? label : "(null)",
            static_cast<unsigned long long>(value),
            suffix != nullptr ? suffix : ""
        );

        if (written >= 0) {
            message[sizeof(message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", message);
        }
    }

    void cnr3_cache_diag_write_lifecycle_event(
        Cnr3InstanceId instance_id,
        const char* label,
        const Cnr3FrameLifecycleOriginDiagnosticStats& stats,
        const char* purpose,
        bool a_event,
        bool x_event
    ) noexcept {
        char message[640] = {};
        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-04 %-62s total=%llu",
            label != nullptr ? label : "(null)",
            static_cast<unsigned long long>(stats.total)
        );

        if (written >= 0) {
            message[sizeof(message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", message);
        }

        const char* impossible_for_a = "   (cannot occur for this event; printed to prove it)";
        const char* impossible_for_x = "   (cannot occur for this event unless a new path is discovered)";

        cnr3_cache_diag_write_lifecycle_value_line(
            instance_id,
            "of which frame0 fresh-start",
            stats.frame0_fresh_start,
            a_event ? impossible_for_a : ""
        );
        cnr3_cache_diag_write_lifecycle_value_line(
            instance_id,
            "of which floor fresh-start",
            stats.floor_fresh_start,
            x_event ? impossible_for_x : ""
        );
        cnr3_cache_diag_write_lifecycle_value_line(
            instance_id,
            "of which ordinary target",
            stats.ordinary_target,
            (a_event || x_event) ? (a_event ? impossible_for_a : impossible_for_x) : ""
        );
        cnr3_cache_diag_write_lifecycle_value_line(
            instance_id,
            "of which recovery hole",
            stats.recovery_hole,
            x_event ? impossible_for_x : ""
        );
        cnr3_cache_diag_write_lifecycle_value_line(
            instance_id,
            "of which recovery target",
            stats.recovery_target,
            (a_event || x_event) ? (a_event ? impossible_for_a : impossible_for_x) : ""
        );

        char purpose_message[768] = {};
        const int purpose_written = std::snprintf(
            purpose_message,
            sizeof(purpose_message),
            "[DSUM-SUMMARY] D-SUM-04    -> %s",
            purpose != nullptr ? purpose : "no purpose text supplied."
        );

        if (purpose_written >= 0) {
            purpose_message[sizeof(purpose_message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", purpose_message);
        }
    }

    void cnr3_cache_diag_write_lifecycle_check(
        Cnr3InstanceId instance_id,
        const char* label,
        bool ok,
        std::uint64_t left,
        std::uint64_t right
    ) noexcept {
        char message[768] = {};
        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-04 lifecycle self-check: %s (%llu vs %llu) -> %s",
            label != nullptr ? label : "unnamed check",
            static_cast<unsigned long long>(left),
            static_cast<unsigned long long>(right),
            ok ? "OK" : "*** MISMATCH ***"
        );

        if (written >= 0) {
            message[sizeof(message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", message);
        }
    }

    void cnr3_cache_diag_write_lifecycle_expectation(
        Cnr3InstanceId instance_id,
        bool ok,
        std::uint64_t computed_total,
        std::uint64_t temporary_outputs_created
    ) noexcept {
        char message[768] = {};
        const int written = std::snprintf(
            message,
            sizeof(message),
            "[DSUM-SUMMARY] D-SUM-04 lifecycle expectation: frames_computed <= D-SUM-07 temporary_outputs_created (%llu <= %llu) -> %s%s",
            static_cast<unsigned long long>(computed_total),
            static_cast<unsigned long long>(temporary_outputs_created),
            ok ? "OK" : "processing failures occurred",
            computed_total == temporary_outputs_created ? "; equality on this clean run" : ""
        );

        if (written >= 0) {
            message[sizeof(message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", message);
        }
    }

    void cnr3_cache_diag_write_lifecycle_summary(
        Cnr3InstanceId instance_id,
        const Cnr3CacheOwnershipDiagnosticStats& stats
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        , const Cnr3DiagDsum07TempOutputLifecycleStats* dsum07_temp_output_lifecycle
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
        , const Cnr3CacheStoreDiagnosticStats* dsum08_cache_store
#endif
    ) noexcept {
        const Cnr3FrameLifecycleOriginDiagnosticStats& a =
            stats.lifecycle_bailed_before_compute;
        const Cnr3FrameLifecycleOriginDiagnosticStats& b =
            stats.lifecycle_frames_computed;
        const Cnr3FrameLifecycleOriginDiagnosticStats& e =
            stats.lifecycle_bailed_after_compute_duplicate;
        const Cnr3FrameLifecycleOriginDiagnosticStats& x =
            stats.lifecycle_computed_but_returned_after_duplicate_store;
        const Cnr3FrameLifecycleOriginDiagnosticStats& f =
            stats.lifecycle_frames_computed_and_stored;

        cnr3_cache_diag_write_text_line(
            instance_id,
            "D-SUM-04",
            "[DSUM-SUMMARY] D-SUM-04 frame lifecycle summary (each number is counted where the event happens; nothing is derived):"
        );
        cnr3_cache_diag_write_text_line(
            instance_id,
            "D-SUM-04",
            "[DSUM-SUMMARY] D-SUM-04   computed = this activation produced the output frame itself, by full pixel processing or fresh-start copy; adopted frames are not computed"
        );

        cnr3_cache_diag_write_lifecycle_event(
            instance_id,
            "bailed_before_compute_since_already_in_cache",
            a,
            "before computing a recovery floor or hole, checked whether another activation had already produced it; when found, adopted it and skipped the work.",
            true,
            false
        );
        cnr3_cache_diag_write_lifecycle_event(
            instance_id,
            "frames_computed",
            b,
            "frames this activation actually produced, including full pixel outputs and copy-only fresh starts; later duplicate losers are still counted here.",
            false,
            false
        );
        cnr3_cache_diag_write_lifecycle_event(
            instance_id,
            "bailed_after_compute_because_another_activation_stored_it_first",
            e,
            "after this activation produced a frame, the store-time duplicate check found another activation's winner; this activation's copy was discarded.",
            false,
            false
        );
        cnr3_cache_diag_write_lifecycle_event(
            instance_id,
            "computed_but_returned_after_duplicate_store",
            x,
            "frame 0 produced a copy, store found an existing winner, and this activation returned its own copy rather than discarding it.",
            false,
            true
        );
        cnr3_cache_diag_write_lifecycle_event(
            instance_id,
            "frames_computed_and_stored",
            f,
            "frames this activation produced and successfully inserted into the cache as the stored winner.",
            false,
            false
        );

        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "bailed-before total equals its five origin buckets",
            a.total == cnr3_cache_diag_lifecycle_bucket_sum(a),
            a.total,
            cnr3_cache_diag_lifecycle_bucket_sum(a)
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "frames-computed total equals its five origin buckets",
            b.total == cnr3_cache_diag_lifecycle_bucket_sum(b),
            b.total,
            cnr3_cache_diag_lifecycle_bucket_sum(b)
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "discard-after-compute total equals its five origin buckets",
            e.total == cnr3_cache_diag_lifecycle_bucket_sum(e),
            e.total,
            cnr3_cache_diag_lifecycle_bucket_sum(e)
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "returned-after-duplicate total equals its five origin buckets",
            x.total == cnr3_cache_diag_lifecycle_bucket_sum(x),
            x.total,
            cnr3_cache_diag_lifecycle_bucket_sum(x)
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "computed-and-stored total equals its five origin buckets",
            f.total == cnr3_cache_diag_lifecycle_bucket_sum(f),
            f.total,
            cnr3_cache_diag_lifecycle_bucket_sum(f)
        );

        const std::uint64_t terminal_total = f.total + e.total + x.total;
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "computed == stored + discarded + returned-after-duplicate",
            b.total == terminal_total,
            b.total,
            terminal_total
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "frame0 computed == stored + discarded + returned-after-duplicate",
            b.frame0_fresh_start ==
                (f.frame0_fresh_start + e.frame0_fresh_start + x.frame0_fresh_start),
            b.frame0_fresh_start,
            f.frame0_fresh_start + e.frame0_fresh_start + x.frame0_fresh_start
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "floor computed == stored + discarded + returned-after-duplicate",
            b.floor_fresh_start ==
                (f.floor_fresh_start + e.floor_fresh_start + x.floor_fresh_start),
            b.floor_fresh_start,
            f.floor_fresh_start + e.floor_fresh_start + x.floor_fresh_start
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "ordinary computed == stored + discarded + returned-after-duplicate",
            b.ordinary_target ==
                (f.ordinary_target + e.ordinary_target + x.ordinary_target),
            b.ordinary_target,
            f.ordinary_target + e.ordinary_target + x.ordinary_target
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "recovery-hole computed == stored + discarded + returned-after-duplicate",
            b.recovery_hole ==
                (f.recovery_hole + e.recovery_hole + x.recovery_hole),
            b.recovery_hole,
            f.recovery_hole + e.recovery_hole + x.recovery_hole
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "recovery-target computed == stored + discarded + returned-after-duplicate",
            b.recovery_target ==
                (f.recovery_target + e.recovery_target + x.recovery_target),
            b.recovery_target,
            f.recovery_target + e.recovery_target + x.recovery_target
        );

        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "bail-before total equals site7a + site7b hits",
            a.total ==
                (stats.site7a_floor_adopt_bail_early.hits_counted +
                    stats.site7b_hole_adopt_bail_early.hits_counted),
            a.total,
            stats.site7a_floor_adopt_bail_early.hits_counted +
                stats.site7b_hole_adopt_bail_early.hits_counted
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "floor bail-before equals site7a hits",
            a.floor_fresh_start == stats.site7a_floor_adopt_bail_early.hits_counted,
            a.floor_fresh_start,
            stats.site7a_floor_adopt_bail_early.hits_counted
        );
        cnr3_cache_diag_write_lifecycle_check(
            instance_id,
            "recovery-hole bail-before equals site7b hits",
            a.recovery_hole == stats.site7b_hole_adopt_bail_early.hits_counted,
            a.recovery_hole,
            stats.site7b_hole_adopt_bail_early.hits_counted
        );

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        if (dsum07_temp_output_lifecycle != nullptr) {
            const Cnr3DiagDsum07TempOutputLifecycleSnapshot dsum07 =
                cnr3_diag_dsum07_snapshot_temp_output_lifecycle(
                    *dsum07_temp_output_lifecycle
                );
            cnr3_cache_diag_write_lifecycle_check(
                instance_id,
                "ordinary+recovery-target discard equals D-SUM-07 duplicate discard",
                (e.ordinary_target + e.recovery_target) ==
                    dsum07.duplicate_computed_but_discarded,
                e.ordinary_target + e.recovery_target,
                dsum07.duplicate_computed_but_discarded
            );
            cnr3_cache_diag_write_lifecycle_expectation(
                instance_id,
                b.total <= dsum07.temporary_outputs_created,
                b.total,
                dsum07.temporary_outputs_created
            );
        }
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
        if (dsum08_cache_store != nullptr) {
            const std::uint64_t production_stores =
                dsum08_cache_store->stores_by_kind[0] +
                dsum08_cache_store->stores_by_kind[1];
            const std::uint64_t as2_stores =
                dsum08_cache_store->stores_by_kind[2] +
                dsum08_cache_store->stores_by_kind[3];
            cnr3_cache_diag_write_lifecycle_check(
                instance_id,
                "stored frame0+ordinary+recovery-target equals D-SUM-08 production stores",
                (f.frame0_fresh_start + f.ordinary_target + f.recovery_target) ==
                    production_stores,
                f.frame0_fresh_start + f.ordinary_target + f.recovery_target,
                production_stores
            );
            cnr3_cache_diag_write_lifecycle_check(
                instance_id,
                "stored floor+recovery-hole equals D-SUM-08 AS2 stores",
                (f.floor_fresh_start + f.recovery_hole) == as2_stores,
                f.floor_fresh_start + f.recovery_hole,
                as2_stores
            );
        }
#endif
    }

#endif

} // namespace

#if defined(CNR3_DIAG_PRINT_DSUM04_OWNERSHIP_BALANCE)

void cnr3_cache_ownership_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheOwnershipDiagnosticStats& stats
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    , const Cnr3DiagDsum07TempOutputLifecycleStats* dsum07_temp_output_lifecycle
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    , const Cnr3CacheStoreDiagnosticStats* dsum08_cache_store
#endif
) noexcept {
    const std::uint64_t lookup_refs_released_or_transferred =
        stats.lookup_refs_released_by_cache_core + stats.lookup_refs_transferred;
    const std::int64_t pin_balance =
        stats.pins_acquired >= stats.pins_released
        ? static_cast<std::int64_t>(stats.pins_acquired - stats.pins_released)
        : -static_cast<std::int64_t>(stats.pins_released - stats.pins_acquired);
    const std::int64_t lookup_ref_balance =
        stats.lookup_refs_acquired >= lookup_refs_released_or_transferred
        ? static_cast<std::int64_t>(stats.lookup_refs_acquired - lookup_refs_released_or_transferred)
        : -static_cast<std::int64_t>(lookup_refs_released_or_transferred - stats.lookup_refs_acquired);
    const std::uint64_t cache_lookup_misses =
        cnr3_cache_diag_lookup_misses(stats.cache_lookup_queries_total, stats.cache_lookup_hits);

    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04 ownership-balance summary"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04 interpretation: pin_balance and lookup_ref_balance must be 0 after drain; non-zero = leak or missed handoff"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04 note: pin-list record/discharge fields are consciously narrowed out in DIAG.2b"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04 note: cache_lookup_* counts intent-counted probes only; lookup_refs_acquired counts add-ref ownership only"
    );

    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "pins_acquired", stats.pins_acquired);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "pins_released", stats.pins_released);
    cnr3_cache_diag_write_int64_row(instance_id, "D-SUM-04", "D-SUM-04", "pin_balance", pin_balance);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-04", "D-SUM-04", "total_pin_count_crosscheck", stats.total_pin_count_crosscheck);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "cache_lookup_queries_total", stats.cache_lookup_queries_total);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "cache_lookup_hits", stats.cache_lookup_hits);
    cnr3_cache_diag_write_uint64_row(
        instance_id, "D-SUM-04", "D-SUM-04", "cache_lookup_misses",
        cache_lookup_misses);   // derived; else-branch means a policy-counted hit was not paired with a query.

    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04 lookup-site breakdown legend:"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04   a lookup is the plugin asking the cache: is this frame present?"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04   invocations = how many times the code reached this location and asked"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04   looks       = of those, how many were COUNTED toward the cache_lookup totals"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04   hits        = of the counted looks, how many found the frame present"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04   misses      = of the counted looks, how many found it absent"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-04",
        "[DSUM-SUMMARY] D-SUM-04   excluded from totals = this location deliberately never counts; invocations prove it participated"
    );

    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site1_requested_frame_check",
        stats.site1_requested_frame_check,
        false,
        "when a frame was first requested, checked if it was already finished in the cache; counted only when found, which means out-of-order work produced it early."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site2_predecessor_fastpath",
        stats.site2_predecessor_fastpath,
        false,
        "after the requested frame was absent, checked whether the previous output frame was present so this frame could be built directly; a miss routes to recovery."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site3_recovery_walk",
        stats.site3_recovery_walk,
        false,
        "during recovery, walked backward until the nearest present anchor frame was found; N-1 is counted only if it appears between two lock holds, deeper probes count normally."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site4_hole_catalogue_scan",
        stats.site4_hole_catalogue_scan,
        true,
        "after an anchor was found, scanned the frames between anchor and target to list holes for the plan; this is bookkeeping, not a lookup decision."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site5_anchor_repin",
        stats.site5_anchor_repin,
        true,
        "re-pinned the anchor frame that the recovery walk had just found under the same cache lock; guaranteed present, so deliberately not counted."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site6_reacquire_already_pinned",
        stats.site6_reacquire_already_pinned,
        true,
        "re-fetched a frame this request had already found and pinned earlier; guaranteed present, so deliberately not counted."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site7a_floor_adopt_bail_early",
        stats.site7a_floor_adopt_bail_early,
        false,
        "before computing a floor-recovery frame, checked whether another activation had already produced it; counted only when found and adopted."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site7b_hole_adopt_bail_early",
        stats.site7b_hole_adopt_bail_early,
        false,
        "before computing a recovery hole, checked whether another activation had already produced it; counted only when found and adopted."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site8a_plain_store_duplicate_check",
        stats.site8a_plain_store_duplicate_check,
        false,
        "after computing a normal output frame, checked whether another activation had already stored it; counted only when found, then this duplicate is discarded."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site8b_as2_store_duplicate_check",
        stats.site8b_as2_store_duplicate_check,
        false,
        "after computing an AS2 recovery frame, checked whether another activation had already stored it; counted only when found, then this duplicate is discarded."
    );
    cnr3_cache_diag_write_lookup_site_row(
        instance_id,
        "site9_duplicate_winner_reacquire",
        stats.site9_duplicate_winner_reacquire,
        true,
        "after losing a store race, fetched the other activation's winning frame; duplicate status guarantees it is present, so deliberately not counted."
    );

    std::uint64_t site_looks_sum = 0;
    std::uint64_t site_hits_sum = 0;
    std::uint64_t site_misses_sum = 0;
    cnr3_cache_diag_accumulate_lookup_site(stats.site1_requested_frame_check, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site2_predecessor_fastpath, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site3_recovery_walk, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site4_hole_catalogue_scan, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site5_anchor_repin, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site6_reacquire_already_pinned, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site7a_floor_adopt_bail_early, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site7b_hole_adopt_bail_early, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site8a_plain_store_duplicate_check, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site8b_as2_store_duplicate_check, site_looks_sum, site_hits_sum, site_misses_sum);
    cnr3_cache_diag_accumulate_lookup_site(stats.site9_duplicate_winner_reacquire, site_looks_sum, site_hits_sum, site_misses_sum);

    char selfcheck_message[640] = {};
    if (
        site_looks_sum == stats.cache_lookup_queries_total &&
        site_hits_sum == stats.cache_lookup_hits &&
        site_misses_sum == cache_lookup_misses
        ) {
        const int written = std::snprintf(
            selfcheck_message,
            sizeof(selfcheck_message),
            "[DSUM-SUMMARY] D-SUM-04 breakdown self-check: per-site looks/hits/misses add up to the cache_lookup totals (%llu/%llu/%llu) -> OK",
            static_cast<unsigned long long>(stats.cache_lookup_queries_total),
            static_cast<unsigned long long>(stats.cache_lookup_hits),
            static_cast<unsigned long long>(cache_lookup_misses)
        );

        if (written >= 0) {
            selfcheck_message[sizeof(selfcheck_message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", selfcheck_message);
        }
    }
    else {
        char looks_delta[32] = {};
        char hits_delta[32] = {};
        char misses_delta[32] = {};
        cnr3_cache_diag_format_delta(
            looks_delta,
            sizeof(looks_delta),
            site_looks_sum,
            stats.cache_lookup_queries_total
        );
        cnr3_cache_diag_format_delta(
            hits_delta,
            sizeof(hits_delta),
            site_hits_sum,
            stats.cache_lookup_hits
        );
        cnr3_cache_diag_format_delta(
            misses_delta,
            sizeof(misses_delta),
            site_misses_sum,
            cache_lookup_misses
        );

        const int written = std::snprintf(
            selfcheck_message,
            sizeof(selfcheck_message),
            "[DSUM-SUMMARY] D-SUM-04 breakdown self-check: *** MISMATCH *** per-site sums (%llu/%llu/%llu) vs cache_lookup totals (%llu/%llu/%llu); delta looks=%s hits=%s misses=%s",
            static_cast<unsigned long long>(site_looks_sum),
            static_cast<unsigned long long>(site_hits_sum),
            static_cast<unsigned long long>(site_misses_sum),
            static_cast<unsigned long long>(stats.cache_lookup_queries_total),
            static_cast<unsigned long long>(stats.cache_lookup_hits),
            static_cast<unsigned long long>(cache_lookup_misses),
            looks_delta,
            hits_delta,
            misses_delta
        );

        if (written >= 0) {
            selfcheck_message[sizeof(selfcheck_message) - 1U] = '\0';
            cnr3_cache_diag_write_text_line(instance_id, "D-SUM-04", selfcheck_message);
        }
    }

    cnr3_cache_diag_write_lifecycle_summary(
        instance_id,
        stats
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        , dsum07_temp_output_lifecycle
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
        , dsum08_cache_store
#endif
    );

    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "lookup_refs_acquired", stats.lookup_refs_acquired);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "lookup_refs_released_by_cache_core", stats.lookup_refs_released_by_cache_core);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "lookup_refs_transferred", stats.lookup_refs_transferred);
    cnr3_cache_diag_write_int64_row(instance_id, "D-SUM-04", "D-SUM-04", "lookup_ref_balance", lookup_ref_balance);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-04", "D-SUM-04", "ownership_errors", stats.ownership_errors);

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM05_CACHE_INTEGRITY)

void cnr3_cache_integrity_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheIntegrityDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-05",
        "[DSUM-SUMMARY] D-SUM-05 cache-integrity summary"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-05",
        "[DSUM-SUMMARY] D-SUM-05 interpretation: violations should be 0; non-zero = structural-invariant breach"
    );

    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-05", "D-SUM-05", "invariant_checks_performed", stats.invariant_checks_performed);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-05", "D-SUM-05", "invariant_violations_detected", stats.invariant_violations_detected);

    char site_message[256] = {};
    std::snprintf(
        site_message,
        sizeof(site_message),
        "[DSUM-SUMMARY] D-SUM-05 %-44s %s",
        "first_violation_site",
        stats.first_violation_site != nullptr ? stats.first_violation_site : "<none>"
    );
    site_message[sizeof(site_message) - 1U] = '\0';
    cnr3_cache_diag_write_text_line(instance_id, "D-SUM-05", site_message);

    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "slot_count_min", stats.have_structural_sample ? stats.slot_count_min : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "slot_count_max", stats.have_structural_sample ? stats.slot_count_max : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "checkpoint_count_min", stats.have_structural_sample ? stats.checkpoint_count_min : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "checkpoint_count_max", stats.have_structural_sample ? stats.checkpoint_count_max : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "non_checkpoint_count_min", stats.have_structural_sample ? stats.non_checkpoint_count_min : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "non_checkpoint_count_max", stats.have_structural_sample ? stats.non_checkpoint_count_max : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "checkpoint_retain_headroom_min", stats.have_structural_sample ? stats.checkpoint_retain_headroom_min : 0U);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-05", "D-SUM-05", "total_pin_count_min", stats.have_structural_sample ? stats.total_pin_count_min : 0);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-05", "D-SUM-05", "total_pin_count_max", stats.have_structural_sample ? stats.total_pin_count_max : 0);

    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "summary_slot_count", stats.have_summary_sample ? stats.summary_slot_count : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "summary_checkpoint_count", stats.have_summary_sample ? stats.summary_checkpoint_count : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "summary_non_checkpoint_count", stats.have_summary_sample ? stats.summary_non_checkpoint_count : 0U);
    cnr3_cache_diag_write_size_row(instance_id, "D-SUM-05", "D-SUM-05", "summary_checkpoint_retain_headroom", stats.have_summary_sample ? stats.summary_checkpoint_retain_headroom : 0U);
    cnr3_cache_diag_write_int_row(instance_id, "D-SUM-05", "D-SUM-05", "summary_total_pin_count", stats.have_summary_sample ? stats.summary_total_pin_count : 0);

    cnr3_diag_flush_stderr();
}

#endif

#if defined(CNR3_DIAG_PRINT_DSUM08_CACHE_STORE)

void cnr3_cache_store_diagnostic_write_summary(
    Cnr3InstanceId instance_id,
    const Cnr3CacheStoreDiagnosticStats& stats
) noexcept {
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-08",
        "[DSUM-SUMMARY] D-SUM-08 cache-store summary"
    );
    cnr3_cache_diag_write_text_line(
        instance_id,
        "D-SUM-08",
        "[DSUM-SUMMARY] D-SUM-08 interpretation: duplicates + rejected count first-in-best-dressed races; as2_checkpoint_promotions are monotonic AS2 upgrades; store_failures should be 0"
    );

    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "stores_total", stats.stores_total);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "stores_production_checkpoint", stats.stores_by_kind[0]);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "stores_production_noncheckpoint", stats.stores_by_kind[1]);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "stores_as2_consumer_checkpoint", stats.stores_by_kind[2]);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "stores_as2_consumer_noncheckpoint", stats.stores_by_kind[3]);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "duplicates_seen", stats.duplicates_seen);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "incoming_rejected", stats.incoming_rejected);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "as2_checkpoint_promotions", stats.as2_checkpoint_promotions);
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-08", "D-SUM-08", "store_failures", stats.store_failures);

    cnr3_diag_flush_stderr();
}

#endif

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
    cnr3_cache_diag_write_uint64_row(instance_id, "D-SUM-10", "D-SUM-10", "frames_recently_evicted_then_re_requested", stats.frames_recently_evicted_then_re_requested);
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
