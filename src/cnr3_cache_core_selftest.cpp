#include "cnr3_cache_core_selftest.h"

#include "cnr3_cache_core.h"

#include "cnr3_diagnostics.h"

#include "cnr3_frame_processing.h"

#include "cnr3_response_tables.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

    constexpr int CNR3_CACHE_CORE_SELFTEST_TRACKED_RELEASE_FRAME_COUNT = 8;

    struct Cnr3CacheCoreSelftestVsApiState {
        int add_frame_ref_count = 0;
        int free_frame_count = 0;
        const VSFrame* last_add_ref_frame = nullptr;
        const VSFrame* last_freed_frame = nullptr;

        const VSFrame* tracked_release_frames[
            CNR3_CACHE_CORE_SELFTEST_TRACKED_RELEASE_FRAME_COUNT
        ] = {};
        int tracked_release_counts[
            CNR3_CACHE_CORE_SELFTEST_TRACKED_RELEASE_FRAME_COUNT
        ] = {};
    };

    /*
        VSAPI::addFrameRef() and VSAPI::freeFrame() have no user-data pointer,
        so this narrow selftest uses a file-local active state pointer. The
        selftest is isolated and single-threaded. Production cache code must not
        use this pattern.
    */
    Cnr3CacheCoreSelftestVsApiState* g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    const VSFrame* VS_CC cnr3_cache_core_selftest_add_frame_ref(
        const VSFrame* frame
    ) noexcept {
        if (g_cnr3_cache_core_selftest_vsapi_state != nullptr) {
            ++g_cnr3_cache_core_selftest_vsapi_state->add_frame_ref_count;
            g_cnr3_cache_core_selftest_vsapi_state->last_add_ref_frame = frame;
        }

        return frame;
    }

    void VS_CC cnr3_cache_core_selftest_free_frame(
        const VSFrame* frame
    ) noexcept {
        if (g_cnr3_cache_core_selftest_vsapi_state != nullptr) {
            ++g_cnr3_cache_core_selftest_vsapi_state->free_frame_count;
            g_cnr3_cache_core_selftest_vsapi_state->last_freed_frame = frame;

            for (int tracked_index = 0; tracked_index < CNR3_CACHE_CORE_SELFTEST_TRACKED_RELEASE_FRAME_COUNT; ++tracked_index) {
                if (
                    g_cnr3_cache_core_selftest_vsapi_state->
                    tracked_release_frames[tracked_index] != nullptr &&
                    g_cnr3_cache_core_selftest_vsapi_state->
                    tracked_release_frames[tracked_index] == frame
                    ) {
                    ++g_cnr3_cache_core_selftest_vsapi_state->
                        tracked_release_counts[tracked_index];
                }
            }
        }
    }

    VSAPI cnr3_cache_core_selftest_make_vsapi() noexcept {
        VSAPI vsapi{};

        vsapi.addFrameRef = cnr3_cache_core_selftest_add_frame_ref;
        vsapi.freeFrame = cnr3_cache_core_selftest_free_frame;

        return vsapi;
    }

    template <typename CacheType, typename = void>
    struct Cnr3CacheCoreSelftestHasPublicPinFrame : std::false_type {};

    template <typename CacheType>
    struct Cnr3CacheCoreSelftestHasPublicPinFrame<
        CacheType,
        std::void_t<decltype(
            std::declval<CacheType&>().pin_frame(
                0,
                std::declval<Cnr3CacheSlotPinToken&>()
            )
        )>
    > : std::true_type {};

    template <typename CacheType, typename = void>
    struct Cnr3CacheCoreSelftestHasPublicLookupFrameAndPin : std::false_type {};

    template <typename CacheType>
    struct Cnr3CacheCoreSelftestHasPublicLookupFrameAndPin<
        CacheType,
        std::void_t<decltype(
            std::declval<CacheType&>().lookup_frame_and_pin(
                0,
                std::declval<Cnr3CacheSlotPinToken&>()
            )
        )>
    > : std::true_type {};

    static_assert(
        !Cnr3CacheCoreSelftestHasPublicPinFrame<Cnr3OutputCacheCore>::value,
        "Cnr3OutputCacheCore must not expose public pin_frame(); AS paths must pin and record atomically."
        );

    static_assert(
        !Cnr3CacheCoreSelftestHasPublicLookupFrameAndPin<Cnr3OutputCacheCore>::value,
        "Cnr3OutputCacheCore must not expose public lookup_frame_and_pin(); AS1 must use lookup_frame_and_record_pin()."
        );

    static_assert(
        std::is_same_v<
            decltype(
                std::declval<Cnr3OutputCacheCore&>().lookup_frame_and_record_pin(
                    0,
                    std::declval<Cnr3CachePinList&>()
                )
            ),
            Cnr3Status
        >,
        "lookup_frame_and_record_pin() must not expose a caller-owned pin token."
        );

    bool g_cnr3_cache_core_selftest_verbose = false;

    constexpr Cnr3InstanceId CNR3_SELFTEST_TRACE_INSTANCE_ID{};
    constexpr const char* CNR3_SELFTEST_TRACE_COMPONENT = "cache_core_selftest_trace";

    void cnr3_cache_core_selftest_trace_line(
        const char* message
    ) noexcept {
        if (!g_cnr3_cache_core_selftest_verbose) {
            return;
        }

        cnr3_diag_write_line(
            CNR3_SELFTEST_TRACE_INSTANCE_ID,
            Cnr3DiagnosticLevel::info,
            CNR3_SELFTEST_TRACE_COMPONENT,
            message,
            Cnr3StderrFlushPolicy::no_flush
        );
    }

    void cnr3_cache_core_selftest_trace_candidate_order(
        const std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order
    ) noexcept {
        if (!g_cnr3_cache_core_selftest_verbose) {
            return;
        }

        for (const Cnr3PruneCandidateDistanceOrderEntry& candidate : candidate_order) {
            char message[160] = {};

            const int written = std::snprintf(
                message,
                sizeof(message),
                "    frame=%d nearest_zone_distance=%d",
                candidate.frame_number,
                candidate.nearest_hot_zone_distance
            );

            if (written < 0) {
                cnr3_cache_core_selftest_trace_line("    formatting_error");
                continue;
            }

            message[sizeof(message) - 1U] = '\0';
            cnr3_cache_core_selftest_trace_line(message);
        }
    }

} // namespace

void cnr3_cache_core_selftest_set_verbose(bool verbose) noexcept {
    g_cnr3_cache_core_selftest_verbose = verbose;
}

Cnr3Status cnr3_cache_core_selftest_empty_model() noexcept {
    const Cnr3OutputCacheCore cache{};

    if (!cache.empty()) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.slot_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.index_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.checkpoint_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_slot_id_source() noexcept {
    Cnr3CacheSlotIdSource source{};

    if (source.next_value_for_diagnostics() != 1U) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheSlotId first = source.allocate();

    if (!cnr3_cache_slot_id_is_valid(first)) {
        return Cnr3Status::invariant_violation;
    }

    if (first.value != 1U) {
        return Cnr3Status::invariant_violation;
    }

    if (source.next_value_for_diagnostics() != 2U) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheSlotId second = source.allocate();

    if (!cnr3_cache_slot_id_is_valid(second)) {
        return Cnr3Status::invariant_violation;
    }

    if (second.value != 2U) {
        return Cnr3Status::invariant_violation;
    }

    if (first.value == second.value) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_store_rejects_empty_owned_frame() noexcept {
    Cnr3OutputCacheCore cache{};

    if (
        cache.store_noncheckpoint_owned_frame(0, Cnr3OwnedFrameRef{}) !=
        Cnr3Status::invalid_argument
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.empty()) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.slot_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.index_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.checkpoint_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_store_success_and_duplicate() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int first_frame_storage = 1;
    int duplicate_frame_storage = 2;

    const VSFrame* first_frame =
        reinterpret_cast<const VSFrame*>(&first_frame_storage);
    const VSFrame* duplicate_frame =
        reinterpret_cast<const VSFrame*>(&duplicate_frame_storage);

    vsapi_state.tracked_release_frames[0] = first_frame;
    vsapi_state.tracked_release_frames[1] = duplicate_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        Cnr3OwnedFrameRef first_owned_frame{};

        if (
            first_owned_frame.reset_to_owned_frame(first_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!first_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(first_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (first_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef duplicate_owned_frame{};

        if (
            duplicate_owned_frame.reset_to_owned_frame(duplicate_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(duplicate_owned_frame)) !=
            Cnr3Status::duplicate
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (duplicate_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.last_freed_frame != duplicate_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 2) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[0] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[1] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.last_freed_frame != first_frame) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_checkpoint_store_flag_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int noncheckpoint_winner_storage = 1;
    int checkpoint_duplicate_loser_storage = 2;
    int checkpoint_winner_storage = 3;
    int noncheckpoint_duplicate_loser_storage = 4;

    const VSFrame* noncheckpoint_winner_frame =
        reinterpret_cast<const VSFrame*>(&noncheckpoint_winner_storage);
    const VSFrame* checkpoint_duplicate_loser_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_duplicate_loser_storage);
    const VSFrame* checkpoint_winner_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_winner_storage);
    const VSFrame* noncheckpoint_duplicate_loser_frame =
        reinterpret_cast<const VSFrame*>(&noncheckpoint_duplicate_loser_storage);

    vsapi_state.tracked_release_frames[0] = noncheckpoint_winner_frame;
    vsapi_state.tracked_release_frames[1] = checkpoint_duplicate_loser_frame;
    vsapi_state.tracked_release_frames[2] = checkpoint_winner_frame;
    vsapi_state.tracked_release_frames[3] = noncheckpoint_duplicate_loser_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        if (
            cache.store_checkpoint_owned_frame(0, Cnr3OwnedFrameRef{}) !=
            Cnr3Status::invalid_argument
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef noncheckpoint_winner{};
        Cnr3OwnedFrameRef checkpoint_duplicate_loser{};

        if (
            noncheckpoint_winner.reset_to_owned_frame(
                noncheckpoint_winner_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                1,
                std::move(noncheckpoint_winner)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (noncheckpoint_winner.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            checkpoint_duplicate_loser.reset_to_owned_frame(
                checkpoint_duplicate_loser_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(
                1,
                std::move(checkpoint_duplicate_loser)
            ) != Cnr3Status::duplicate
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (checkpoint_duplicate_loser.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef promoted_lookup{};

        if (
            cache.lookup_frame_and_add_ref(1, &vsapi, promoted_lookup) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (promoted_lookup.get() != noncheckpoint_winner_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        promoted_lookup.reset();

        if (promoted_lookup.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 3) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        Cnr3OwnedFrameRef checkpoint_winner{};
        Cnr3OwnedFrameRef noncheckpoint_duplicate_loser{};

        if (
            checkpoint_winner.reset_to_owned_frame(
                checkpoint_winner_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(
                10,
                std::move(checkpoint_winner)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (checkpoint_winner.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            noncheckpoint_duplicate_loser.reset_to_owned_frame(
                noncheckpoint_duplicate_loser_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                10,
                std::move(noncheckpoint_duplicate_loser)
            ) != Cnr3Status::duplicate
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (noncheckpoint_duplicate_loser.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 5) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[0] != 2) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[1] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[2] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[3] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_as2_store_record_monotonic_checkpoint() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int noncheckpoint_winner_storage = 1;
    int checkpoint_promote_loser_storage = 2;
    int checkpoint_winner_storage = 3;
    int noncheckpoint_no_demote_loser_storage = 4;

    const VSFrame* noncheckpoint_winner_frame =
        reinterpret_cast<const VSFrame*>(&noncheckpoint_winner_storage);
    const VSFrame* checkpoint_promote_loser_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_promote_loser_storage);
    const VSFrame* checkpoint_winner_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_winner_storage);
    const VSFrame* noncheckpoint_no_demote_loser_frame =
        reinterpret_cast<const VSFrame*>(&noncheckpoint_no_demote_loser_storage);

    vsapi_state.tracked_release_frames[0] = noncheckpoint_winner_frame;
    vsapi_state.tracked_release_frames[1] = checkpoint_promote_loser_frame;
    vsapi_state.tracked_release_frames[2] = checkpoint_winner_frame;
    vsapi_state.tracked_release_frames[3] = noncheckpoint_no_demote_loser_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheAs2StoreRecordSummary summary{};

        if (
            cache.store_owned_frame_and_record_pin(
                10,
                Cnr3OwnedFrameRef{},
                false,
                pin_list,
                summary
            ) != Cnr3Status::invalid_argument
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef noncheckpoint_winner{};

        if (
            noncheckpoint_winner.reset_to_owned_frame(
                noncheckpoint_winner_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_owned_frame_and_record_pin(
                10,
                std::move(noncheckpoint_winner),
                false,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (noncheckpoint_winner.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            summary.frame_number != 10 ||
            summary.requested_checkpoint ||
            !summary.inserted_new_slot ||
            summary.duplicate_existing_slot ||
            summary.checkpoint_promoted ||
            summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            !summary.incoming_frame_consumed ||
            summary.incoming_frame_rejected
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.checkpoint_count() != 0U ||
            cache.total_pin_count() != 1 ||
            pin_list.pin_count() != 1U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef checkpoint_promote_loser{};

        if (
            checkpoint_promote_loser.reset_to_owned_frame(
                checkpoint_promote_loser_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_owned_frame_and_record_pin(
                10,
                std::move(checkpoint_promote_loser),
                true,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (checkpoint_promote_loser.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            summary.frame_number != 10 ||
            !summary.requested_checkpoint ||
            summary.inserted_new_slot ||
            !summary.duplicate_existing_slot ||
            !summary.checkpoint_promoted ||
            !summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            summary.incoming_frame_consumed ||
            !summary.incoming_frame_rejected
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 2 ||
            pin_list.pin_count() != 2U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef promoted_lookup{};

        if (
            cache.lookup_frame_and_add_ref(10, &vsapi, promoted_lookup) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (promoted_lookup.get() != noncheckpoint_winner_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        promoted_lookup.reset();

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0 || pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheAs2StoreRecordSummary summary{};

        Cnr3OwnedFrameRef checkpoint_winner{};

        if (
            checkpoint_winner.reset_to_owned_frame(
                checkpoint_winner_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_owned_frame_and_record_pin(
                20,
                std::move(checkpoint_winner),
                true,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            summary.frame_number != 20 ||
            !summary.requested_checkpoint ||
            !summary.inserted_new_slot ||
            summary.duplicate_existing_slot ||
            summary.checkpoint_promoted ||
            !summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            !summary.incoming_frame_consumed ||
            summary.incoming_frame_rejected
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 1 ||
            pin_list.pin_count() != 1U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef noncheckpoint_no_demote_loser{};

        if (
            noncheckpoint_no_demote_loser.reset_to_owned_frame(
                noncheckpoint_no_demote_loser_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_owned_frame_and_record_pin(
                20,
                std::move(noncheckpoint_no_demote_loser),
                false,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (noncheckpoint_no_demote_loser.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            summary.frame_number != 20 ||
            summary.requested_checkpoint ||
            summary.inserted_new_slot ||
            !summary.duplicate_existing_slot ||
            summary.checkpoint_promoted ||
            !summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            summary.incoming_frame_consumed ||
            !summary.incoming_frame_rejected
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 2 ||
            pin_list.pin_count() != 2U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef no_demote_lookup{};

        if (
            cache.lookup_frame_and_add_ref(20, &vsapi, no_demote_lookup) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (no_demote_lookup.get() != checkpoint_winner_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        no_demote_lookup.reset();

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0 || pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 2) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 6) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        vsapi_state.tracked_release_counts[0] != 2 ||
        vsapi_state.tracked_release_counts[1] != 1 ||
        vsapi_state.tracked_release_counts[2] != 2 ||
        vsapi_state.tracked_release_counts[3] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "G.12A AS2 store-record monotonic checkpoint scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    duplicate checkpoint promotes existing non-checkpoint frame 10"
    );
    cnr3_cache_core_selftest_trace_line(
        "    duplicate non-checkpoint does not demote checkpoint frame 20"
    );
    cnr3_cache_core_selftest_trace_line(
        "    loser frames freed once after lock; winners remain first-in-best-dressed"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_central_remove_helper_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int first_frame_storage = 1;
    int middle_frame_storage = 2;
    int checkpoint_last_frame_storage = 3;
    int pinned_frame_storage = 4;

    const VSFrame* first_frame =
        reinterpret_cast<const VSFrame*>(&first_frame_storage);
    const VSFrame* middle_frame =
        reinterpret_cast<const VSFrame*>(&middle_frame_storage);
    const VSFrame* checkpoint_last_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_last_frame_storage);
    const VSFrame* pinned_frame =
        reinterpret_cast<const VSFrame*>(&pinned_frame_storage);

    vsapi_state.tracked_release_frames[0] = first_frame;
    vsapi_state.tracked_release_frames[1] = middle_frame;
    vsapi_state.tracked_release_frames[2] = checkpoint_last_frame;
    vsapi_state.tracked_release_frames[3] = pinned_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        if (cache.remove_unpinned_frame(-1) != Cnr3Status::invalid_argument) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.remove_unpinned_frame(99) != Cnr3Status::not_found) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef first_owned_frame{};
        Cnr3OwnedFrameRef middle_owned_frame{};
        Cnr3OwnedFrameRef checkpoint_last_owned_frame{};

        if (
            first_owned_frame.reset_to_owned_frame(first_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            middle_owned_frame.reset_to_owned_frame(middle_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            checkpoint_last_owned_frame.reset_to_owned_frame(
                checkpoint_last_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                1,
                std::move(first_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                2,
                std::move(middle_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(
                3,
                std::move(checkpoint_last_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.remove_unpinned_frame(2) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef moved_checkpoint_lookup{};

        if (
            cache.lookup_frame_and_add_ref(
                3,
                &vsapi,
                moved_checkpoint_lookup
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (moved_checkpoint_lookup.get() != checkpoint_last_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        moved_checkpoint_lookup.reset();

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.remove_unpinned_frame(3) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef missing_lookup{};

        if (
            cache.lookup_frame_and_add_ref(3, &vsapi, missing_lookup) !=
            Cnr3Status::not_found
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (missing_lookup.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3OwnedFrameRef pinned_owned_frame{};

        if (
            pinned_owned_frame.reset_to_owned_frame(pinned_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                4,
                std::move(pinned_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(4, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.remove_unpinned_frame(4) != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.remove_unpinned_frame(4) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 5) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.add_frame_ref_count != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[0] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[1] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[2] != 2) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[3] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_bounded_selected_detach_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int pinned_frame_storage = 1;
    int first_removed_frame_storage = 2;
    int checkpoint_removed_frame_storage = 3;
    int survivor_frame_storage = 4;

    const VSFrame* pinned_frame =
        reinterpret_cast<const VSFrame*>(&pinned_frame_storage);
    const VSFrame* first_removed_frame =
        reinterpret_cast<const VSFrame*>(&first_removed_frame_storage);
    const VSFrame* checkpoint_removed_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_removed_frame_storage);
    const VSFrame* survivor_frame =
        reinterpret_cast<const VSFrame*>(&survivor_frame_storage);

    vsapi_state.tracked_release_frames[0] = pinned_frame;
    vsapi_state.tracked_release_frames[1] = first_removed_frame;
    vsapi_state.tracked_release_frames[2] = checkpoint_removed_frame;
    vsapi_state.tracked_release_frames[3] = survivor_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        Cnr3OwnedFrameRef pinned_owned_frame{};
        Cnr3OwnedFrameRef first_removed_owned_frame{};
        Cnr3OwnedFrameRef checkpoint_removed_owned_frame{};
        Cnr3OwnedFrameRef survivor_owned_frame{};

        if (
            pinned_owned_frame.reset_to_owned_frame(pinned_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            first_removed_owned_frame.reset_to_owned_frame(
                first_removed_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            checkpoint_removed_owned_frame.reset_to_owned_frame(
                checkpoint_removed_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            survivor_owned_frame.reset_to_owned_frame(survivor_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                1,
                std::move(pinned_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                2,
                std::move(first_removed_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(
                3,
                std::move(checkpoint_removed_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                4,
                std::move(survivor_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(1, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::size_t removed_count = 999U;
        const std::vector<int> empty_candidates{};

        if (
            cache.remove_selected_unpinned_frames_bounded(
                empty_candidates,
                8U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> invalid_candidates{ -1 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                invalid_candidates,
                8U,
                removed_count
            ) != Cnr3Status::invalid_argument
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> pinned_candidates{ 1 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                pinned_candidates,
                1U,
                removed_count
            ) != Cnr3Status::lifecycle_violation
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> bounded_candidates{ 2, 3, 4 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                bounded_candidates,
                2U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> missing_candidates{ 2 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                missing_candidates,
                1U,
                removed_count
            ) != Cnr3Status::not_found
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> survivor_candidates{ 4 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                survivor_candidates,
                4U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> formerly_pinned_candidates{ 1 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                formerly_pinned_candidates,
                1U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 4) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    for (int tracked_index = 0; tracked_index < 4; ++tracked_index) {
        if (vsapi_state.tracked_release_counts[tracked_index] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_unpinned_noncheckpoint_selection_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int frame_storage[5] = { 1, 2, 3, 4, 5 };
    const VSFrame* frames[5] = {
        reinterpret_cast<const VSFrame*>(&frame_storage[0]),
        reinterpret_cast<const VSFrame*>(&frame_storage[1]),
        reinterpret_cast<const VSFrame*>(&frame_storage[2]),
        reinterpret_cast<const VSFrame*>(&frame_storage[3]),
        reinterpret_cast<const VSFrame*>(&frame_storage[4])
    };

    for (int tracked_index = 0; tracked_index < 4; ++tracked_index) {
        vsapi_state.tracked_release_frames[tracked_index] = frames[tracked_index];
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3OwnedFrameRef owned_frames[5] = {};

        for (int frame_index = 0; frame_index < 5; ++frame_index) {
            if (
                owned_frames[frame_index].reset_to_owned_frame(
                    frames[frame_index],
                    &vsapi
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(owned_frames[0])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(2, std::move(owned_frames[1])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(3, std::move(owned_frames[2])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(4, std::move(owned_frames[3])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(5, std::move(owned_frames[4])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(2, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::size_t removed_count = 999U;

        if (
            cache.remove_unpinned_noncheckpoint_frames_bounded(
                0U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_bounded(
                2U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 4) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 5) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    for (int tracked_index = 0; tracked_index < 4; ++tracked_index) {
        if (vsapi_state.tracked_release_counts[tracked_index] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_cache_policy_constants() noexcept {
    if (CNR3_CACHE_BYTE_BUDGET_BYTES != 1073741824ULL) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES != 150U) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES != 1000U) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_OVERFLOW_FACTOR_NUMERATOR != 11U) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR != 10U) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_CHECKPOINT_INTERVAL != 10) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_CHECKPOINT_MIN_RETAIN != 10U) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_CHECKPOINT_MAX_RETAIN != 48U) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS != 10) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_HOT_ZONE_BACK_RADIUS != 50) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS !=
        CNR3_CACHE_HOT_ZONE_BACK_RADIUS
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_MAX_HOT_ZONES != 5U) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_JUMP_THRESHOLD !=
        (CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS +
         CNR3_CACHE_HOT_ZONE_BACK_RADIUS +
         1)
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_JUMP_THRESHOLD != 61) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_HOT_ZONE_DECAY_MARGIN != 20) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS != 8U) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR == 0U ||
        CNR3_CACHE_OVERFLOW_FACTOR_NUMERATOR <=
        CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_HOT_ZONE_BACK_RADIUS !=
        (5 * CNR3_CACHE_CHECKPOINT_INTERVAL)
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES <
        (2U * CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE)
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE != 348U) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_CHECKPOINT_MAX_RETAIN <
        CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE != 25U) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS >
        CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        CNR3_CACHE_HOT_ZONE_DECAY_MARGIN >
        CNR3_CACHE_HOT_ZONE_BACK_RADIUS
        ) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_hot_zone_data_model() noexcept {
    const Cnr3OutputCacheCore cache{};

    if (cache.hot_zone_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheHotZone inactive_zone{};

    if (!cnr3_cache_hot_zone_is_valid(inactive_zone)) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone malformed_inactive_zone{};
    malformed_inactive_zone.low_frame = 0;

    if (cnr3_cache_hot_zone_is_valid(malformed_inactive_zone)) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone active_zone{};
    active_zone.is_active = true;
    active_zone.low_frame = 10;
    active_zone.high_frame = 20;
    active_zone.last_observed_frame = 15;

    if (!cnr3_cache_hot_zone_is_valid(active_zone)) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone reversed_zone = active_zone;
    reversed_zone.low_frame = 20;
    reversed_zone.high_frame = 10;

    if (cnr3_cache_hot_zone_is_valid(reversed_zone)) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone low_observation_zone = active_zone;
    low_observation_zone.last_observed_frame = 9;

    if (cnr3_cache_hot_zone_is_valid(low_observation_zone)) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone high_observation_zone = active_zone;
    high_observation_zone.last_observed_frame = 21;

    if (cnr3_cache_hot_zone_is_valid(high_observation_zone)) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone invalid_low_zone = active_zone;
    invalid_low_zone.low_frame = CNR3_INVALID_FRAME_NUMBER;

    if (cnr3_cache_hot_zone_is_valid(invalid_low_zone)) {
        return Cnr3Status::invariant_violation;
    }

    std::vector<Cnr3CacheHotZone> hot_zones{};

    if (!cnr3_cache_hot_zone_model_invariants_hold(hot_zones)) {
        return Cnr3Status::invariant_violation;
    }

    for (std::size_t zone_index = 0;
        zone_index < CNR3_CACHE_MAX_HOT_ZONES;
        ++zone_index
        ) {
        Cnr3CacheHotZone zone{};
        zone.is_active = true;
        zone.low_frame = static_cast<int>(zone_index * 100U);
        zone.high_frame = zone.low_frame + CNR3_CACHE_HOT_ZONE_BACK_RADIUS;
        zone.last_observed_frame = zone.low_frame;
        hot_zones.push_back(zone);
    }

    if (!cnr3_cache_hot_zone_model_invariants_hold(hot_zones)) {
        return Cnr3Status::invariant_violation;
    }

    hot_zones.push_back(active_zone);

    if (cnr3_cache_hot_zone_model_invariants_hold(hot_zones)) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_hot_zone_slide_spawn_lifecycle() noexcept {
    Cnr3OutputCacheCore cache{};

    if (
        cache.record_hot_zone_observation(CNR3_INVALID_FRAME_NUMBER) !=
        Cnr3Status::invalid_argument
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(100)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 1U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(50)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(100)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(110)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(49)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(111)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(120) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 1U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(50)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(70)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(130)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(131)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(200) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 2U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(150)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(210)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(260) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 2U) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(150)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(210)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(270)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(500) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(700) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(900) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != CNR3_CACHE_MAX_HOT_ZONES) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.clear() != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.empty()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_hot_zone_capacity_merge_lifecycle() noexcept {
    Cnr3OutputCacheCore cache{};

    const int observations[CNR3_CACHE_MAX_HOT_ZONES] = {
        100,
        300,
        600,
        1000,
        1500
    };

    for (std::size_t observation_index = 0U;
        observation_index < CNR3_CACHE_MAX_HOT_ZONES;
        ++observation_index
        ) {
        if (
            cache.record_hot_zone_observation(observations[observation_index]) !=
            Cnr3Status::ok
            ) {
            return Cnr3Status::invariant_violation;
        }
    }

    if (cache.hot_zone_count() != CNR3_CACHE_MAX_HOT_ZONES) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(180)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(2450)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(2500) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != CNR3_CACHE_MAX_HOT_ZONES) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(50)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(180)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(310)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(600)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(1000)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(1500)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(2450)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(2500)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(2510)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(2449)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(2511)) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(2550) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != CNR3_CACHE_MAX_HOT_ZONES) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.frame_is_inside_hot_zone(2450)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(2500)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.frame_is_inside_hot_zone(2560)) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_hot_zone_retirement_decay_lifecycle() noexcept {
    {
        Cnr3OutputCacheCore cache{};

        if (
            cache.retire_decay_eligible_hot_zones(CNR3_INVALID_FRAME_NUMBER) !=
            Cnr3Status::invalid_argument
            ) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 0U) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.retire_decay_eligible_hot_zones(99) != Cnr3Status::ok) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 1U) {
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.retire_decay_eligible_hot_zones(
                100 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN - 1
            ) != Cnr3Status::ok
            ) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 1U) {
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.retire_decay_eligible_hot_zones(
                100 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
            ) != Cnr3Status::ok
            ) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 0U) {
            return Cnr3Status::invariant_violation;
        }

        if (cache.frame_is_inside_hot_zone(100)) {
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3CacheCoreSelftestVsApiState vsapi_state{};
        g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

        int pinned_frame_storage = 1;

        const VSFrame* pinned_frame =
            reinterpret_cast<const VSFrame*>(&pinned_frame_storage);

        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3OwnedFrameRef pinned_owned_frame{};

        if (
            pinned_owned_frame.reset_to_owned_frame(pinned_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(90, std::move(pinned_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.lookup_frame_and_record_pin(90, pin_list) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.retire_decay_eligible_hot_zones(
                100 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.frame_is_inside_hot_zone(100)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.retire_decay_eligible_hot_zones(
                100 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
    }

    {
        Cnr3CacheCoreSelftestVsApiState vsapi_state{};
        g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

        int checkpoint_frame_storage = 2;

        const VSFrame* checkpoint_frame =
            reinterpret_cast<const VSFrame*>(&checkpoint_frame_storage);

        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3OwnedFrameRef checkpoint_owned_frame{};

        if (
            checkpoint_owned_frame.reset_to_owned_frame(checkpoint_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(90, std::move(checkpoint_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.retire_decay_eligible_hot_zones(
                100 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.frame_is_inside_hot_zone(100)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_lookup_addref_hit_and_miss() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int cached_frame_storage = 1;

    const VSFrame* cached_frame =
        reinterpret_cast<const VSFrame*>(&cached_frame_storage);

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        Cnr3OwnedFrameRef cached_owned_frame{};

        if (
            cached_owned_frame.reset_to_owned_frame(cached_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(cached_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef miss_lookup_frame{};

        if (
            cache.lookup_frame_and_add_ref(2, &vsapi, miss_lookup_frame) !=
            Cnr3Status::not_found
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (miss_lookup_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef hit_lookup_frame{};

        if (
            cache.lookup_frame_and_add_ref(1, &vsapi, hit_lookup_frame) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!hit_lookup_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (hit_lookup_frame.get() != cached_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.last_add_ref_frame != cached_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        hit_lookup_frame.reset();

        if (hit_lookup_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.last_freed_frame != cached_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 2) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.last_freed_frame != cached_frame) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_clear_teardown_releases_cached_frames_once() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int first_frame_storage = 1;
    int second_frame_storage = 2;
    int third_frame_storage = 3;

    const VSFrame* first_frame =
        reinterpret_cast<const VSFrame*>(&first_frame_storage);
    const VSFrame* second_frame =
        reinterpret_cast<const VSFrame*>(&second_frame_storage);
    const VSFrame* third_frame =
        reinterpret_cast<const VSFrame*>(&third_frame_storage);

    vsapi_state.tracked_release_frames[0] = first_frame;
    vsapi_state.tracked_release_frames[1] = second_frame;
    vsapi_state.tracked_release_frames[2] = third_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        Cnr3OwnedFrameRef first_owned_frame{};
        Cnr3OwnedFrameRef second_owned_frame{};
        Cnr3OwnedFrameRef third_owned_frame{};

        if (
            first_owned_frame.reset_to_owned_frame(first_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            second_owned_frame.reset_to_owned_frame(second_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            third_owned_frame.reset_to_owned_frame(third_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(first_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(2, std::move(second_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(3, std::move(third_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (first_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (second_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (third_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 3) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[0] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[1] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[2] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_slot_pin_unpin_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int cached_frame_storage = 1;

    const VSFrame* cached_frame =
        reinterpret_cast<const VSFrame*>(&cached_frame_storage);

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        Cnr3OwnedFrameRef cached_owned_frame{};

        if (
            cached_owned_frame.reset_to_owned_frame(cached_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(cached_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(2, pin_list) != Cnr3Status::not_found) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty() || pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(1, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.empty() || pin_list.pin_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(1, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty() || pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.last_freed_frame != cached_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_lookup_pin_reservation_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int cached_frame_storage = 1;

    const VSFrame* cached_frame =
        reinterpret_cast<const VSFrame*>(&cached_frame_storage);

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        Cnr3OwnedFrameRef cached_owned_frame{};

        if (
            cached_owned_frame.reset_to_owned_frame(cached_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(cached_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.lookup_frame_and_record_pin(2, pin_list) !=
            Cnr3Status::not_found
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty() || pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.lookup_frame_and_record_pin(1, pin_list) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_per_invocation_pin_list_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int cached_frame_storage = 1;

    const VSFrame* cached_frame =
        reinterpret_cast<const VSFrame*>(&cached_frame_storage);

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        if (!pin_list.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheSlotPinToken invalid_pin{};

        if (pin_list.record_pin(invalid_pin) != Cnr3Status::invalid_argument) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef cached_owned_frame{};

        if (
            cached_owned_frame.reset_to_owned_frame(cached_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(cached_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(1, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(1, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.last_freed_frame != cached_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_as1_lookup_pin_record_atomicity() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int cached_frame_storage = 1;

    const VSFrame* cached_frame =
        reinterpret_cast<const VSFrame*>(&cached_frame_storage);

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        Cnr3OwnedFrameRef cached_owned_frame{};

        if (
            cached_owned_frame.reset_to_owned_frame(cached_frame, &vsapi) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(1, std::move(cached_owned_frame)) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.lookup_frame_and_record_pin(2, pin_list) !=
            Cnr3Status::not_found
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.lookup_frame_and_record_pin(1, pin_list) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        /*
            Structural AS1 proof: the only public cache operation used here is
            the combined helper. The helper does not expose a caller-owned pin
            token, so a caller cannot observe a state where cache.total_pin_count()
            increased but pin_list.pin_count() did not also increase through the
            same public operation.
        */
        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.add_frame_ref_count != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_checkpoint_retention_boundary_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int frame_storage[5] = { 1, 2, 3, 4, 5 };
    const VSFrame* frames[5] = {
        reinterpret_cast<const VSFrame*>(&frame_storage[0]),
        reinterpret_cast<const VSFrame*>(&frame_storage[1]),
        reinterpret_cast<const VSFrame*>(&frame_storage[2]),
        reinterpret_cast<const VSFrame*>(&frame_storage[3]),
        reinterpret_cast<const VSFrame*>(&frame_storage[4])
    };

    for (int tracked_index = 0; tracked_index < 4; ++tracked_index) {
        vsapi_state.tracked_release_frames[tracked_index] = frames[tracked_index];
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3OwnedFrameRef owned_frames[5] = {};

        for (int frame_index = 0; frame_index < 5; ++frame_index) {
            if (
                owned_frames[frame_index].reset_to_owned_frame(
                    frames[frame_index],
                    &vsapi
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (
            cache.store_checkpoint_owned_frame(0, std::move(owned_frames[0])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(10, std::move(owned_frames[1])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(20, std::move(owned_frames[2])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(30, std::move(owned_frames[3])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(40, std::move(owned_frames[4])) !=
            Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(20, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 5U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.index_count() != 5U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 4U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::size_t removed_count = 999U;

        if (
            cache.remove_unpinned_checkpoints_above_retain_count_bounded(
                1U,
                0U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_checkpoints_above_retain_count_bounded(
                1U,
                1U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 4U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_checkpoints_above_retain_count_bounded(
                1U,
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_checkpoints_above_retain_count_bounded(
                1U,
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_checkpoints_above_retain_count_bounded(
                1U,
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_checkpoints_above_retain_count_bounded(
                0U,
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 5) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 5) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_hot_zone_dsum11_counter_model() noexcept {
#if !defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    return Cnr3Status::ok;
#else
    Cnr3OutputCacheCore cache{};

    Cnr3CacheHotZoneDiagnosticStats stats = cache.hot_zone_diagnostic_stats();

    if (stats.hot_zone_updates != 0U || stats.zones_created != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (stats.have_zone_count_sample || stats.have_protected_range_sample) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    stats = cache.hot_zone_diagnostic_stats();

    if (
        stats.hot_zone_updates != 1U ||
        stats.zones_created != 1U ||
        stats.zones_slid != 0U ||
        stats.zones_merged != 0U ||
        stats.zones_decayed != 0U ||
        stats.zones_expired != 0U
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        !stats.have_zone_count_sample ||
        stats.zone_count_min != 1U ||
        stats.zone_count_max != 1U ||
        stats.zone_count_sum != 1U ||
        stats.zone_count_samples != 1U
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        !stats.have_protected_range_sample ||
        stats.protected_range_min != 61 ||
        stats.protected_range_max != 61
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(120) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    stats = cache.hot_zone_diagnostic_stats();

    if (
        stats.hot_zone_updates != 2U ||
        stats.zones_created != 1U ||
        stats.zones_slid != 1U ||
        stats.zone_count_min != 1U ||
        stats.zone_count_max != 1U ||
        stats.zone_count_sum != 2U ||
        stats.zone_count_samples != 2U
        ) {
        return Cnr3Status::invariant_violation;
    }

    const int observations[CNR3_CACHE_MAX_HOT_ZONES - 1U] = {
        300,
        600,
        1000,
        1500
    };

    for (std::size_t observation_index = 0U;
        observation_index < (CNR3_CACHE_MAX_HOT_ZONES - 1U);
        ++observation_index
        ) {
        if (
            cache.record_hot_zone_observation(observations[observation_index]) !=
            Cnr3Status::ok
            ) {
            return Cnr3Status::invariant_violation;
        }
    }

    stats = cache.hot_zone_diagnostic_stats();

    if (
        stats.hot_zone_updates != 6U ||
        stats.zones_created != 5U ||
        stats.zones_slid != 1U ||
        stats.zones_merged != 0U ||
        stats.zone_count_max != CNR3_CACHE_MAX_HOT_ZONES ||
        stats.zone_count_samples != 6U
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.record_hot_zone_observation(2500) != Cnr3Status::ok) {
        return Cnr3Status::invariant_violation;
    }

    stats = cache.hot_zone_diagnostic_stats();

    if (
        stats.hot_zone_updates != 8U ||
        stats.zones_created != 6U ||
        stats.zones_slid != 1U ||
        stats.zones_merged != 1U ||
        stats.zone_count_min != 1U ||
        stats.zone_count_max != CNR3_CACHE_MAX_HOT_ZONES ||
        stats.zone_count_samples != 8U
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (stats.protected_range_max < stats.protected_range_min) {
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.retire_decay_eligible_hot_zones(
            2500 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    stats = cache.hot_zone_diagnostic_stats();

    if (
        stats.zones_decayed != 5U ||
        stats.zones_expired != 5U ||
        stats.hot_zone_updates != 18U ||
        stats.zone_count_min != 0U ||
        stats.zone_count_max != CNR3_CACHE_MAX_HOT_ZONES ||
        stats.zone_count_samples != 18U
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (cache.hot_zone_count() != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (stats.frames_rejected_from_prune_due_to_hot_zone != 0U) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache.cache_state_invariants_hold()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
#endif
}

Cnr3Status cnr3_cache_core_selftest_hot_zone_prune_protection_selection_lifecycle() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int protected_low_storage = 1;
    int protected_mid_storage = 2;
    int outside_low_storage = 3;
    int outside_high_storage = 4;
    int pinned_outside_storage = 5;
    int checkpoint_outside_storage = 6;

    const VSFrame* protected_low_frame =
        reinterpret_cast<const VSFrame*>(&protected_low_storage);
    const VSFrame* protected_mid_frame =
        reinterpret_cast<const VSFrame*>(&protected_mid_storage);
    const VSFrame* outside_low_frame =
        reinterpret_cast<const VSFrame*>(&outside_low_storage);
    const VSFrame* outside_high_frame =
        reinterpret_cast<const VSFrame*>(&outside_high_storage);
    const VSFrame* pinned_outside_frame =
        reinterpret_cast<const VSFrame*>(&pinned_outside_storage);
    const VSFrame* checkpoint_outside_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_outside_storage);

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        const auto store_noncheckpoint = [
            &cache,
            &vsapi,
            &vsapi_state
        ](
            int frame_number,
            const VSFrame* frame
        ) noexcept -> Cnr3Status {
            Cnr3OwnedFrameRef owned_frame{};

            const Cnr3Status adopt_status =
                owned_frame.reset_to_owned_frame(frame, &vsapi);

            if (!cnr3_status_is_ok(adopt_status)) {
                return adopt_status;
            }

            const Cnr3Status store_status =
                cache.store_noncheckpoint_owned_frame(
                    frame_number,
                    std::move(owned_frame)
                );

            if (!cnr3_status_is_ok(store_status)) {
                return store_status;
            }

            if (owned_frame.has_frame()) {
                return Cnr3Status::ownership_violation;
            }

            if (vsapi_state.free_frame_count != 0) {
                return Cnr3Status::ownership_violation;
            }

            return Cnr3Status::ok;
        };

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.frame_is_inside_hot_zone(60)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.frame_is_inside_hot_zone(100)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.frame_is_inside_hot_zone(40)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.frame_is_inside_hot_zone(111)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_noncheckpoint(40, outside_low_frame) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_noncheckpoint(60, protected_low_frame) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_noncheckpoint(100, protected_mid_frame) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_noncheckpoint(111, outside_high_frame) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_noncheckpoint(200, pinned_outside_frame) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef checkpoint_owned_frame{};

        if (
            checkpoint_owned_frame.reset_to_owned_frame(
                checkpoint_outside_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_checkpoint_owned_frame(
                300,
                std::move(checkpoint_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (checkpoint_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::ownership_violation;
        }

        Cnr3CachePinList pin_list{};

        if (pin_list.reserve_for_additional_pins(1U) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(200, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 6U || cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::size_t removed_count = 999U;

        if (
            cache.remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
                0U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U || vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 2U || vsapi_state.free_frame_count != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 4U || cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U || vsapi_state.free_frame_count != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
                1U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 1U || vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 3U || cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.retire_decay_eligible_hot_zones(
                100 + CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.hot_zone_count() != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
                10U,
                removed_count
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 2U || vsapi_state.free_frame_count != 5) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 1U || cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const Cnr3CacheHotZoneDiagnosticStats stats =
            cache.hot_zone_diagnostic_stats();

        if (stats.frames_rejected_from_prune_due_to_hot_zone != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 6) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 6) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_prune_victim_distance_ordering() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int frame_storage[6] = {};
    const int frame_numbers[6] = {
        40,
        112,
        200,
        220,
        249,
        400
    };

    const int expected_frame_order[6] = {
        400,
        200,
        220,
        40,
        112,
        249
    };

    const int expected_distance_order[6] = {
        90,
        50,
        30,
        10,
        2,
        1
    };

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        const auto store_noncheckpoint = [
            &cache,
            &vsapi
        ](
            int frame_number,
            const VSFrame* frame
        ) noexcept -> Cnr3Status {
            Cnr3OwnedFrameRef owned_frame{};

            const Cnr3Status adopt_status =
                owned_frame.reset_to_owned_frame(frame, &vsapi);

            if (!cnr3_status_is_ok(adopt_status)) {
                return adopt_status;
            }

            const Cnr3Status store_status =
                cache.store_noncheckpoint_owned_frame(
                    frame_number,
                    std::move(owned_frame)
                );

            if (!cnr3_status_is_ok(store_status)) {
                return store_status;
            }

            if (owned_frame.has_frame()) {
                return Cnr3Status::ownership_violation;
            }

            return Cnr3Status::ok;
        };

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.record_hot_zone_observation(300) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.frame_is_inside_hot_zone(50)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.frame_is_inside_hot_zone(250)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.frame_is_inside_hot_zone(249)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        for (int index = 0; index < 6; ++index) {
            frame_storage[index] = index + 1;
            const VSFrame* frame =
                reinterpret_cast<const VSFrame*>(&frame_storage[index]);

            if (
                store_noncheckpoint(frame_numbers[index], frame) !=
                Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> candidate_order{};
        candidate_order.reserve(6U);

        if (
            cache.select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded(
                6U,
                candidate_order
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (candidate_order.size() != 6U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        cnr3_cache_core_selftest_trace_line(
            "G.8A multi-zone prune-victim ordering scenario"
        );
        cnr3_cache_core_selftest_trace_line(
            "    active hot zones: [50-110], [250-310]"
        );
        cnr3_cache_core_selftest_trace_line(
            "    eligible cached frames: 40,112,200,220,249,400"
        );
        cnr3_cache_core_selftest_trace_line(
            "    returned order and nearest-zone distances:"
        );
        cnr3_cache_core_selftest_trace_candidate_order(candidate_order);
        cnr3_cache_core_selftest_trace_line(
            "    expected: 400(90), 200(50), 220(30), 40(10), 112(2), 249(1)"
        );

        for (std::size_t index = 0U; index < candidate_order.size(); ++index) {
            if (candidate_order[index].frame_number != expected_frame_order[index]) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (
                candidate_order[index].nearest_hot_zone_distance !=
                expected_distance_order[index]
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        /*
            This is the key multi-zone correctness check: frame 249 is adjacent
            to the second zone, even though it is far from the first zone. It
            must therefore rank colder than the genuinely isolated frame 200.
        */
        if (candidate_order[1].frame_number != 200) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (candidate_order[5].frame_number != 249) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> bounded_order{};
        bounded_order.reserve(3U);

        if (
            cache.select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded(
                3U,
                bounded_order
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (bounded_order.size() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        for (std::size_t index = 0U; index < bounded_order.size(); ++index) {
            if (bounded_order[index].frame_number != expected_frame_order[index]) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> no_capacity_order{};

        if (
            cache.select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded(
                1U,
                no_capacity_order
            ) != Cnr3Status::invalid_argument
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 6U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 6) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 6) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_composite_prune_candidate_selection() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    struct FrameSpec {
        int frame_number = CNR3_INVALID_FRAME_NUMBER;
        bool is_checkpoint = false;
        bool should_pin = false;
    };

    const FrameSpec frame_specs[] = {
        { 0, true, false },
        { 40, false, false },
        { 60, false, false },
        { 112, false, false },
        { 200, false, false },
        { 220, false, true },
        { 249, true, false },
        { 260, false, false },
        { 265, true, false },
        { 320, true, false },
        { 400, false, false },
        { 520, true, false }
    };

    const int expected_composite_frames[] = {
        520,
        400,
        200,
        40,
        320,
        112
    };

    const int expected_composite_distances[] = {
        210,
        90,
        50,
        10,
        10,
        2
    };

    const bool expected_composite_checkpoint_flags[] = {
        true,
        false,
        false,
        false,
        true,
        false
    };

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        const auto store_frame = [
            &cache,
            &vsapi
        ](
            int frame_number,
            bool is_checkpoint,
            const VSFrame* frame
        ) noexcept -> Cnr3Status {
            Cnr3OwnedFrameRef owned_frame{};

            const Cnr3Status adopt_status =
                owned_frame.reset_to_owned_frame(frame, &vsapi);

            if (!cnr3_status_is_ok(adopt_status)) {
                return adopt_status;
            }

            const Cnr3Status store_status =
                is_checkpoint
                ? cache.store_checkpoint_owned_frame(frame_number, std::move(owned_frame))
                : cache.store_noncheckpoint_owned_frame(frame_number, std::move(owned_frame));

            if (!cnr3_status_is_ok(store_status)) {
                return store_status;
            }

            if (owned_frame.has_frame()) {
                return Cnr3Status::ownership_violation;
            }

            return Cnr3Status::ok;
        };

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.record_hot_zone_observation(300) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        int frame_storage[static_cast<int>(sizeof(frame_specs) / sizeof(frame_specs[0]))] = {};

        for (int index = 0; index < static_cast<int>(sizeof(frame_specs) / sizeof(frame_specs[0])); ++index) {
            frame_storage[index] = index + 1;
            const VSFrame* frame =
                reinterpret_cast<const VSFrame*>(&frame_storage[index]);

            if (
                store_frame(
                    frame_specs[index].frame_number,
                    frame_specs[index].is_checkpoint,
                    frame
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (cache.checkpoint_count() != 5U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.reserve_for_additional_pins(1U) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(220, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> composite_order{};
        composite_order.reserve(8U);

        if (
            cache.select_composite_prune_candidates_bounded(
                true,
                3U,
                8U,
                composite_order
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (composite_order.size() != 6U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        for (std::size_t index = 0U; index < composite_order.size(); ++index) {
            if (composite_order[index].frame_number != expected_composite_frames[index]) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (
                composite_order[index].nearest_hot_zone_distance !=
                expected_composite_distances[index]
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (
                composite_order[index].is_checkpoint !=
                expected_composite_checkpoint_flags[index]
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        /*
            Checkpoint 249 is outside hot zones, but the retention budget allows
            only the two coldest checkpoint candidates. It must not outrank or
            displace colder checkpoint candidates 520 and 320.
        */
        for (const Cnr3PruneCandidateDistanceOrderEntry& entry : composite_order) {
            if (entry.frame_number == 249) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> checkpoint_only_order{};
        checkpoint_only_order.reserve(8U);

        if (
            cache.select_composite_prune_candidates_bounded(
                false,
                3U,
                8U,
                checkpoint_only_order
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (checkpoint_only_order.size() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            checkpoint_only_order[0].frame_number != 520 ||
            checkpoint_only_order[1].frame_number != 320
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> noncheckpoint_only_order{};
        noncheckpoint_only_order.reserve(8U);

        if (
            cache.select_composite_prune_candidates_bounded(
                true,
                cache.checkpoint_count(),
                8U,
                noncheckpoint_only_order
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (noncheckpoint_only_order.size() != 4U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const int expected_noncheckpoint_frames[] = {
            400,
            200,
            40,
            112
        };

        for (std::size_t index = 0U; index < noncheckpoint_only_order.size(); ++index) {
            if (noncheckpoint_only_order[index].frame_number != expected_noncheckpoint_frames[index]) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (noncheckpoint_only_order[index].is_checkpoint) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> bounded_order{};
        bounded_order.reserve(3U);

        if (
            cache.select_composite_prune_candidates_bounded(
                true,
                3U,
                3U,
                bounded_order
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (bounded_order.size() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        for (std::size_t index = 0U; index < bounded_order.size(); ++index) {
            if (bounded_order[index].frame_number != expected_composite_frames[index]) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> cross_pool_order{};
        cross_pool_order.reserve(3U);

        {
            Cnr3CacheCoreSelftestVsApiState cross_pool_vsapi_state{};
            g_cnr3_cache_core_selftest_vsapi_state = &cross_pool_vsapi_state;

            VSAPI cross_pool_vsapi = cnr3_cache_core_selftest_make_vsapi();
            Cnr3OutputCacheCore cross_pool_cache{};

            const auto store_cross_pool_frame = [
                &cross_pool_cache,
                &cross_pool_vsapi
            ](
                int frame_number,
                bool is_checkpoint,
                const VSFrame* frame
            ) noexcept -> Cnr3Status {
                Cnr3OwnedFrameRef owned_frame{};

                const Cnr3Status adopt_status =
                    owned_frame.reset_to_owned_frame(frame, &cross_pool_vsapi);

                if (!cnr3_status_is_ok(adopt_status)) {
                    return adopt_status;
                }

                const Cnr3Status store_status =
                    is_checkpoint
                    ? cross_pool_cache.store_checkpoint_owned_frame(
                        frame_number,
                        std::move(owned_frame)
                    )
                    : cross_pool_cache.store_noncheckpoint_owned_frame(
                        frame_number,
                        std::move(owned_frame)
                    );

                if (!cnr3_status_is_ok(store_status)) {
                    return store_status;
                }

                if (owned_frame.has_frame()) {
                    return Cnr3Status::ownership_violation;
                }

                return Cnr3Status::ok;
            };

            if (cross_pool_cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            int cross_pool_frame_storage[6] = {};

            const struct CrossPoolFrameSpec {
                int frame_number = CNR3_INVALID_FRAME_NUMBER;
                bool is_checkpoint = false;
            } cross_pool_specs[] = {
                { 0, true },
                { 150, true },
                { 160, false },
                { 170, false },
                { 180, true },
                { 200, false }
            };

            for (int index = 0; index < static_cast<int>(sizeof(cross_pool_specs) / sizeof(cross_pool_specs[0])); ++index) {
                cross_pool_frame_storage[index] = 100 + index;

                const VSFrame* frame =
                    reinterpret_cast<const VSFrame*>(&cross_pool_frame_storage[index]);

                if (
                    store_cross_pool_frame(
                        cross_pool_specs[index].frame_number,
                        cross_pool_specs[index].is_checkpoint,
                        frame
                    ) != Cnr3Status::ok
                    ) {
                    g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                    return Cnr3Status::invariant_violation;
                }
            }

            if (cross_pool_cache.checkpoint_count() != 3U) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (
                cross_pool_cache.select_composite_prune_candidates_bounded(
                    true,
                    1U,
                    3U,
                    cross_pool_order
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (cross_pool_order.size() != 3U) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            const int expected_cross_pool_frames[] = {
                200,
                180,
                170
            };

            const int expected_cross_pool_distances[] = {
                90,
                70,
                60
            };

            const bool expected_cross_pool_checkpoint_flags[] = {
                false,
                true,
                false
            };

            for (std::size_t index = 0U; index < cross_pool_order.size(); ++index) {
                if (
                    cross_pool_order[index].frame_number !=
                    expected_cross_pool_frames[index]
                    ) {
                    g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                    return Cnr3Status::invariant_violation;
                }

                if (
                    cross_pool_order[index].nearest_hot_zone_distance !=
                    expected_cross_pool_distances[index]
                    ) {
                    g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                    return Cnr3Status::invariant_violation;
                }

                if (
                    cross_pool_order[index].is_checkpoint !=
                    expected_cross_pool_checkpoint_flags[index]
                    ) {
                    g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                    return Cnr3Status::invariant_violation;
                }
            }

            for (const Cnr3PruneCandidateDistanceOrderEntry& entry : cross_pool_order) {
                if (entry.frame_number == 150 || entry.frame_number == 160) {
                    g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                    return Cnr3Status::invariant_violation;
                }
            }

            cnr3_cache_core_selftest_trace_line(
                "G.9A cross-pool bounded top-K scenario"
            );
            cnr3_cache_core_selftest_trace_line(
                "    active hot zones: [50-110]"
            );
            cnr3_cache_core_selftest_trace_line(
                "    eligible checkpoints: 150(40),180(70); frame 0 excluded"
            );
            cnr3_cache_core_selftest_trace_line(
                "    eligible non-checkpoints: 160(50),170(60),200(90)"
            );
            cnr3_cache_core_selftest_trace_line(
                "    returned global top-3 and nearest-zone distances:"
            );
            cnr3_cache_core_selftest_trace_candidate_order(cross_pool_order);
            cnr3_cache_core_selftest_trace_line(
                "    expected: 200 noncheckpoint(90), 180 checkpoint(70), 170 noncheckpoint(60)"
            );
            cnr3_cache_core_selftest_trace_line(
                "    absent by assertion: 160 noncheckpoint(50), 150 checkpoint(40)"
            );

            if (cross_pool_cache.clear() != Cnr3Status::ok) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (!cross_pool_cache.empty()) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (cross_pool_vsapi_state.free_frame_count != 6) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;
        }

        std::vector<Cnr3PruneCandidateDistanceOrderEntry> no_capacity_order{};

        if (
            cache.select_composite_prune_candidates_bounded(
                true,
                3U,
                1U,
                no_capacity_order
            ) != Cnr3Status::invalid_argument
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != (sizeof(frame_specs) / sizeof(frame_specs[0]))) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != static_cast<int>(sizeof(frame_specs) / sizeof(frame_specs[0]))) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != static_cast<int>(sizeof(frame_specs) / sizeof(frame_specs[0]))) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_prune_trigger_decision_hysteresis() noexcept {
    Cnr3CachePruneTriggerDecision decision{};

    if (
        cnr3_calculate_cache_prune_trigger_decision(
            0U,
            0U,
            decision
        ) != Cnr3Status::invalid_argument
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        decision.frame_byte_count != 0U ||
        decision.active_ceiling_frame_count != 0U ||
        decision.overflow_trigger_frame_count != 0U ||
        decision.current_slot_count != 0U ||
        decision.prune_is_required ||
        decision.target_slot_count_after_prune != 0U ||
        decision.target_remove_count != 0U
        ) {
        return Cnr3Status::invariant_violation;
    }

    const auto expect_decision = [](
        std::uint64_t frame_byte_count,
        std::size_t current_slot_count,
        std::size_t expected_active_ceiling,
        std::size_t expected_trigger,
        bool expected_prune_required,
        std::size_t expected_target_after_prune,
        std::size_t expected_remove_count
    ) noexcept -> Cnr3Status {
        Cnr3CachePruneTriggerDecision observed{};

        const Cnr3Status status = cnr3_calculate_cache_prune_trigger_decision(
            frame_byte_count,
            current_slot_count,
            observed
        );

        if (!cnr3_status_is_ok(status)) {
            return status;
        }

        if (observed.frame_byte_count != frame_byte_count) {
            return Cnr3Status::invariant_violation;
        }

        if (observed.current_slot_count != current_slot_count) {
            return Cnr3Status::invariant_violation;
        }

        if (observed.active_ceiling_frame_count != expected_active_ceiling) {
            return Cnr3Status::invariant_violation;
        }

        if (observed.overflow_trigger_frame_count != expected_trigger) {
            return Cnr3Status::invariant_violation;
        }

        if (observed.prune_is_required != expected_prune_required) {
            return Cnr3Status::invariant_violation;
        }

        if (observed.target_slot_count_after_prune != expected_target_after_prune) {
            return Cnr3Status::invariant_violation;
        }

        if (observed.target_remove_count != expected_remove_count) {
            return Cnr3Status::invariant_violation;
        }

        return Cnr3Status::ok;
    };

    const std::uint64_t one_mebibyte = 1024ULL * 1024ULL;
    const std::uint64_t two_mebibytes = 2ULL * 1024ULL * 1024ULL;

    if (
        expect_decision(
            one_mebibyte,
            1100U,
            1000U,
            1100U,
            false,
            1100U,
            0U
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        expect_decision(
            one_mebibyte,
            1101U,
            1000U,
            1100U,
            true,
            1000U,
            101U
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        expect_decision(
            two_mebibytes,
            563U,
            512U,
            563U,
            false,
            563U,
            0U
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        expect_decision(
            two_mebibytes,
            564U,
            512U,
            563U,
            true,
            512U,
            52U
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        expect_decision(
            CNR3_CACHE_BYTE_BUDGET_BYTES + 1ULL,
            165U,
            150U,
            165U,
            false,
            165U,
            0U
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    if (
        expect_decision(
            CNR3_CACHE_BYTE_BUDGET_BYTES + 1ULL,
            166U,
            150U,
            165U,
            true,
            150U,
            16U
        ) != Cnr3Status::ok
        ) {
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "G.10A prune-trigger decision hysteresis scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    1 MiB frame -> active_ceiling 1000, trigger 1100"
    );
    cnr3_cache_core_selftest_trace_line(
        "    slot_count 1100 -> no prune; slot_count 1101 -> prune target 1000, remove 101"
    );
    cnr3_cache_core_selftest_trace_line(
        "    2 MiB frame -> active_ceiling 512, trigger 563"
    );
    cnr3_cache_core_selftest_trace_line(
        "    slot_count 563 -> no prune; slot_count 564 -> prune target 512, remove 52"
    );
    cnr3_cache_core_selftest_trace_line(
        "    oversize frame -> active_ceiling 150, trigger 165"
    );
    cnr3_cache_core_selftest_trace_line(
        "    slot_count 165 -> no prune; slot_count 166 -> prune target 150, remove 16"
    );

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_as5_prune_execution_decide_detach_free() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr std::uint64_t oversize_frame_byte_count =
        CNR3_CACHE_BYTE_BUDGET_BYTES + 1ULL;
    constexpr std::size_t retain_checkpoint_count = 1U;
    constexpr std::size_t max_remove_count = 3U;
    constexpr std::size_t total_frame_count = 166U;

    int frame_storage[total_frame_count] = {};

    for (std::size_t index = 0U; index < total_frame_count; ++index) {
        frame_storage[index] = static_cast<int>(1000U + index);
    }

    const VSFrame* removed_checkpoint_frame =
        reinterpret_cast<const VSFrame*>(&frame_storage[165]);
    const VSFrame* removed_first_noncheckpoint_frame =
        reinterpret_cast<const VSFrame*>(&frame_storage[163]);
    const VSFrame* removed_second_noncheckpoint_frame =
        reinterpret_cast<const VSFrame*>(&frame_storage[162]);
    const VSFrame* pinned_survivor_frame =
        reinterpret_cast<const VSFrame*>(&frame_storage[164]);

    vsapi_state.tracked_release_frames[0] = removed_checkpoint_frame;
    vsapi_state.tracked_release_frames[1] = removed_first_noncheckpoint_frame;
    vsapi_state.tracked_release_frames[2] = removed_second_noncheckpoint_frame;
    vsapi_state.tracked_release_frames[3] = pinned_survivor_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const auto store_frame = [
            &cache,
            &vsapi,
            &frame_storage
        ](
            int frame_number,
            bool is_checkpoint
        ) noexcept -> Cnr3Status {
            Cnr3OwnedFrameRef owned_frame{};

            const VSFrame* frame =
                reinterpret_cast<const VSFrame*>(&frame_storage[frame_number]);

            const Cnr3Status adopt_status =
                owned_frame.reset_to_owned_frame(frame, &vsapi);

            if (!cnr3_status_is_ok(adopt_status)) {
                return adopt_status;
            }

            const Cnr3Status store_status =
                is_checkpoint
                ? cache.store_checkpoint_owned_frame(
                    frame_number,
                    std::move(owned_frame)
                )
                : cache.store_noncheckpoint_owned_frame(
                    frame_number,
                    std::move(owned_frame)
                );

            if (!cnr3_status_is_ok(store_status)) {
                return store_status;
            }

            if (owned_frame.has_frame()) {
                return Cnr3Status::ownership_violation;
            }

            return Cnr3Status::ok;
        };

        for (int frame_number = 0; frame_number <= 164; ++frame_number) {
            const bool is_checkpoint = (frame_number == 0);

            if (store_frame(frame_number, is_checkpoint) != Cnr3Status::ok) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (cache.slot_count() != 165U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CachePruneExecutionSummary no_prune_summary{};

        if (
            cache.execute_bounded_prune_pass(
                oversize_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                no_prune_summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (no_prune_summary.trigger_decision.current_slot_count != 165U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (no_prune_summary.trigger_decision.prune_is_required) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            no_prune_summary.selected_candidate_count != 0U ||
            no_prune_summary.detached_count != 0U ||
            vsapi_state.free_frame_count != 0
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
        Cnr3CacheHotZoneDiagnosticStats stats =
            cache.hot_zone_diagnostic_stats();

        if (stats.frames_rejected_from_prune_due_to_hot_zone != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
#endif

        if (store_frame(165, true) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 166U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 2U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.lookup_frame_and_record_pin(164, pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const int hot_zone_pinned_frames[] = {55, 60, 65};

        for (const int pinned_frame_number : hot_zone_pinned_frames) {
            if (
                cache.lookup_frame_and_record_pin(
                    pinned_frame_number,
                    pin_list
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (cache.total_pin_count() != 4) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CachePruneExecutionSummary prune_summary{};

        if (
            cache.execute_bounded_prune_pass(
                oversize_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                prune_summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!prune_summary.trigger_decision.prune_is_required) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.trigger_decision.active_ceiling_frame_count != 150U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.trigger_decision.overflow_trigger_frame_count != 165U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.trigger_decision.current_slot_count != 166U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.trigger_decision.target_remove_count != 16U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.bounded_remove_limit != max_remove_count) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.selected_candidate_count != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (prune_summary.detached_count != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.slot_count() != 163U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.checkpoint_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 4) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[2] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[3] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
        constexpr std::uint64_t expected_hot_zone_prune_rejections = 58U;

        static_assert(
            expected_hot_zone_prune_rejections !=
            static_cast<std::uint64_t>(
                CNR3_CACHE_HOT_ZONE_BACK_RADIUS +
                CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS +
                1
            ),
            "G.13A must prove cached-state counting, not hot-zone width."
        );

        stats = cache.hot_zone_diagnostic_stats();

        if (
            stats.frames_rejected_from_prune_due_to_hot_zone !=
            expected_hot_zone_prune_rejections
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
#endif

        Cnr3CachePruneExecutionSummary post_prune_summary{};

        if (
            cache.execute_bounded_prune_pass(
                oversize_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                post_prune_summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (post_prune_summary.trigger_decision.prune_is_required) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (post_prune_summary.detached_count != 0U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.free_frame_count != 3) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
        stats = cache.hot_zone_diagnostic_stats();

        if (
            stats.frames_rejected_from_prune_due_to_hot_zone !=
            expected_hot_zone_prune_rejections
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
#endif

        cnr3_cache_core_selftest_trace_line(
            "G.11A AS5 prune execution decide/detach/free scenario"
        );
        cnr3_cache_core_selftest_trace_line(
            "    hot zone: [50-110]; frame 164 pinned; frame 0 retained checkpoint"
        );
        cnr3_cache_core_selftest_trace_line(
            "    hot-zone pins for D-SUM discrimination: 55, 60, 65"
        );
        cnr3_cache_core_selftest_trace_line(
            "    slot_count 165 -> no prune; slot_count 166 -> prune required"
        );
        cnr3_cache_core_selftest_trace_line(
            "    target remove 16, bounded remove limit 3"
        );
        cnr3_cache_core_selftest_trace_line(
            "    detached/free after lock: 165 checkpoint, 163 noncheckpoint, 162 noncheckpoint"
        );
        cnr3_cache_core_selftest_trace_line(
            "    preserved: 164 pinned noncheckpoint, frame 0 retained checkpoint"
        );
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
        cnr3_cache_core_selftest_trace_line(
            "G.13A D-SUM-11 hot-zone prune-rejection counter scenario"
        );
        cnr3_cache_core_selftest_trace_line(
            "    no-prune boundary leaves rejected-frame counter at 0"
        );
        cnr3_cache_core_selftest_trace_line(
            "    prune pass counts 58 cached, unpinned, prunable frames in hot zone [50-110]"
        );
        cnr3_cache_core_selftest_trace_line(
            "    test would fail if counter returned the 61-frame hot-zone width"
        );
        cnr3_cache_core_selftest_trace_line(
            "    idempotent post-prune pass does not increment counter again"
        );
#endif

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.cache_state_invariants_hold()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != static_cast<int>(total_frame_count)) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[3] != 1) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_as1_bounded_recovery_search_scaffold() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr int requested_frame = 100;
    constexpr int recovery_back_radius = CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS;

    static_assert(
        recovery_back_radius == CNR3_CACHE_HOT_ZONE_BACK_RADIUS,
        "H.1A recovery search window must track the CMS07 hot-zone back radius."
    );
    static_assert(recovery_back_radius == 50);

    int frame_storage[220] = {};

    for (int index = 0; index < static_cast<int>(sizeof(frame_storage) / sizeof(frame_storage[0])); ++index) {
        frame_storage[index] = 3000 + index;
    }

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto store_frame = [
        &vsapi,
        &frame_storage
    ](
        Cnr3OutputCacheCore& cache,
        int frame_number,
        bool is_checkpoint
    ) noexcept -> Cnr3Status {
        Cnr3OwnedFrameRef owned_frame{};

        const VSFrame* frame =
            reinterpret_cast<const VSFrame*>(&frame_storage[frame_number]);

        const Cnr3Status adopt_status =
            owned_frame.reset_to_owned_frame(frame, &vsapi);

        if (!cnr3_status_is_ok(adopt_status)) {
            return adopt_status;
        }

        const Cnr3Status store_status =
            is_checkpoint
            ? cache.store_checkpoint_owned_frame(frame_number, std::move(owned_frame))
            : cache.store_noncheckpoint_owned_frame(frame_number, std::move(owned_frame));

        if (!cnr3_status_is_ok(store_status)) {
            return store_status;
        }

        if (owned_frame.has_frame()) {
            return Cnr3Status::ownership_violation;
        }

        return Cnr3Status::ok;
    };

    const auto run_plan = [](
        const Cnr3OutputCacheCore& cache,
        int request,
        Cnr3CacheRecoverySearchPlan& plan
    ) noexcept -> Cnr3Status {
        return cache.plan_bounded_recovery_search(
            request,
            CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
            plan
        );
    };

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, requested_frame, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_frame(cache, requested_frame - 1, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, requested_frame, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.search_lower_frame != 50 || plan.search_upper_frame != 99) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_found || plan.anchor_frame_number != 99) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.anchor_frame_number == requested_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.requested_frame_is_repair_target || plan.requested_frame_is_in_hole_catalogue) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.hole_frame_numbers.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, 49, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_frame(cache, 50, true) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, requested_frame, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_found || plan.anchor_frame_number != 50) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_is_checkpoint) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, 49, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, requested_frame, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.anchor_found || cnr3_frame_number_is_valid(plan.anchor_frame_number)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.hole_frame_numbers.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, 90, true) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_frame(cache, 95, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, requested_frame, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_found || plan.anchor_frame_number != 95 || plan.anchor_is_checkpoint) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, 90, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_frame(cache, 95, true) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, requested_frame, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_found || plan.anchor_frame_number != 95 || !plan.anchor_is_checkpoint) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, 90, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (store_frame(cache, requested_frame, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, requested_frame, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> expected_holes = {
            91, 92, 93, 94, 95, 96, 97, 98, 99
        };

        if (!plan.anchor_found || plan.anchor_frame_number != 90) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.hole_frame_numbers != expected_holes) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        for (const int hole_frame : plan.hole_frame_numbers) {
            if (hole_frame == plan.anchor_frame_number || hole_frame == requested_frame) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (!plan.requested_frame_is_repair_target || plan.requested_frame_is_in_hole_catalogue) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};

        if (store_frame(cache, 0, true) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheRecoverySearchPlan plan{};

        if (run_plan(cache, 3, plan) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> expected_holes = {1, 2};

        if (plan.search_lower_frame != 0 || plan.search_upper_frame != 2) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_found || plan.anchor_frame_number != 0 || !plan.anchor_is_checkpoint) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.hole_frame_numbers != expected_holes) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "H.1A AS1 bounded recovery search scaffold scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    recovery window B = 50; search interval is [requested-B, requested-1]"
    );
    cnr3_cache_core_selftest_trace_line(
        "    lower bound is inclusive: candidate at requested-B is found"
    );
    cnr3_cache_core_selftest_trace_line(
        "    below-bound candidate at requested-B-1 is not found"
    );
    cnr3_cache_core_selftest_trace_line(
        "    checkpoint flag is irrelevant: nearer non-checkpoint and nearer checkpoint both win"
    );
    cnr3_cache_core_selftest_trace_line(
        "    requested frame is the later repair target, not a hole-catalogue entry"
    );
    cnr3_cache_core_selftest_trace_line(
        "    underflow edge clamps lower bound to frame 0"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_as1_recovery_anchor_pin_record() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr int requested_frame = 100;
    constexpr int recovery_back_radius = CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS;

    static_assert(recovery_back_radius == CNR3_CACHE_HOT_ZONE_BACK_RADIUS);
    static_assert(recovery_back_radius == 50);

    int frame_storage[120] = {};

    for (int index = 0; index < static_cast<int>(sizeof(frame_storage) / sizeof(frame_storage[0])); ++index) {
        frame_storage[index] = 4000 + index;
    }

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto store_frame = [
        &vsapi,
        &frame_storage
    ](
        Cnr3OutputCacheCore& cache,
        int frame_number,
        bool is_checkpoint
    ) noexcept -> Cnr3Status {
        Cnr3OwnedFrameRef owned_frame{};

        const VSFrame* frame =
            reinterpret_cast<const VSFrame*>(&frame_storage[frame_number]);

        const Cnr3Status adopt_status =
            owned_frame.reset_to_owned_frame(frame, &vsapi);

        if (!cnr3_status_is_ok(adopt_status)) {
            return adopt_status;
        }

        const Cnr3Status store_status =
            is_checkpoint
            ? cache.store_checkpoint_owned_frame(frame_number, std::move(owned_frame))
            : cache.store_noncheckpoint_owned_frame(frame_number, std::move(owned_frame));

        if (!cnr3_status_is_ok(store_status)) {
            return store_status;
        }

        if (owned_frame.has_frame()) {
            return Cnr3Status::ownership_violation;
        }

        return Cnr3Status::ok;
    };

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheRecoverySearchPlan plan{};

        vsapi_state.tracked_release_frames[0] =
            reinterpret_cast<const VSFrame*>(&frame_storage[90]);

        if (store_frame(cache, 90, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.plan_bounded_recovery_search_and_record_anchor_pin(
                requested_frame,
                recovery_back_radius,
                pin_list,
                plan
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const std::vector<int> expected_holes = {
            91, 92, 93, 94, 95, 96, 97, 98, 99
        };

        if (!plan.anchor_found || plan.anchor_frame_number != 90) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.anchor_is_checkpoint || !plan.anchor_pin_recorded) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.hole_frame_numbers != expected_holes) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.pin_count() != 1U || cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.empty() || cache.total_pin_count() != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty() || cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheRecoverySearchPlan plan{};

        if (store_frame(cache, 49, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.plan_bounded_recovery_search_and_record_anchor_pin(
                requested_frame,
                recovery_back_radius,
                pin_list,
                plan
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.anchor_found || plan.anchor_pin_recorded) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty() || cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheRecoverySearchPlan plan{};

        if (store_frame(cache, requested_frame, false) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.plan_bounded_recovery_search_and_record_anchor_pin(
                requested_frame,
                recovery_back_radius,
                pin_list,
                plan
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (plan.anchor_found || plan.anchor_pin_recorded) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!pin_list.empty() || cache.total_pin_count() != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (vsapi_state.free_frame_count != 3) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "H.2A AS1 recovery anchor pin-record scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    bounded search, anchor pin, and pin-list record occur under one cache lock"
    );
    cnr3_cache_core_selftest_trace_line(
        "    no-anchor and requested-frame-only cases record no pin"
    );
    cnr3_cache_core_selftest_trace_line(
        "    ordered proof: clear refused while pinned, discharge, then clear succeeds"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_as2_recovery_store_consumer() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr int anchor_frame_number = 10;
    constexpr int first_hole_frame_number = 11;
    constexpr int duplicate_hole_frame_number = 12;
    constexpr int requested_frame = 13;
    constexpr int recovery_back_radius = CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS;

    constexpr std::size_t expected_anchor_pin_count = 1U;
    constexpr std::size_t expected_first_hole_as2_pin_count = 1U;
    constexpr std::size_t expected_duplicate_hole_as2_pin_count = 1U;
    constexpr std::size_t expected_final_pin_list_count =
        expected_anchor_pin_count +
        expected_first_hole_as2_pin_count +
        expected_duplicate_hole_as2_pin_count;

    static_assert(expected_final_pin_list_count == 3U);

    int anchor_frame_storage = 5100;
    int first_hole_frame_storage = 5110;
    int duplicate_winner_frame_storage = 5120;
    int duplicate_loser_frame_storage = 5121;
    int requested_reject_frame_storage = 5130;

    const VSFrame* anchor_frame =
        reinterpret_cast<const VSFrame*>(&anchor_frame_storage);
    const VSFrame* first_hole_frame =
        reinterpret_cast<const VSFrame*>(&first_hole_frame_storage);
    const VSFrame* duplicate_winner_frame =
        reinterpret_cast<const VSFrame*>(&duplicate_winner_frame_storage);
    const VSFrame* duplicate_loser_frame =
        reinterpret_cast<const VSFrame*>(&duplicate_loser_frame_storage);
    const VSFrame* requested_reject_frame =
        reinterpret_cast<const VSFrame*>(&requested_reject_frame_storage);

    vsapi_state.tracked_release_frames[0] = anchor_frame;
    vsapi_state.tracked_release_frames[1] = first_hole_frame;
    vsapi_state.tracked_release_frames[2] = duplicate_winner_frame;
    vsapi_state.tracked_release_frames[3] = duplicate_loser_frame;
    vsapi_state.tracked_release_frames[4] = requested_reject_frame;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
    Cnr3OutputCacheCore cache{};
    Cnr3CachePinList pin_list{};
    Cnr3CacheRecoverySearchPlan plan{};
    Cnr3CacheAs2StoreRecordSummary summary{};

    const auto make_owned_frame = [
        &vsapi
    ](
        const VSFrame* frame,
        Cnr3OwnedFrameRef& owned_frame
    ) noexcept -> Cnr3Status {
        return owned_frame.reset_to_owned_frame(frame, &vsapi);
    };

    Cnr3OwnedFrameRef anchor_owned{};

    if (make_owned_frame(anchor_frame, anchor_owned) != Cnr3Status::ok) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.store_noncheckpoint_owned_frame(
            anchor_frame_number,
            std::move(anchor_owned)
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (anchor_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.plan_bounded_recovery_search_and_record_anchor_pin(
            requested_frame,
            recovery_back_radius,
            pin_list,
            plan
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    const std::vector<int> expected_holes = {
        first_hole_frame_number,
        duplicate_hole_frame_number
    };

    if (
        !plan.anchor_found ||
        plan.anchor_frame_number != anchor_frame_number ||
        !plan.anchor_pin_recorded ||
        !plan.requested_frame_is_repair_target ||
        plan.requested_frame_is_in_hole_catalogue ||
        plan.hole_frame_numbers != expected_holes
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        pin_list.pin_count() != expected_anchor_pin_count ||
        cache.total_pin_count() != static_cast<int>(expected_anchor_pin_count)
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    Cnr3OwnedFrameRef requested_reject_owned{};

    if (
        make_owned_frame(requested_reject_frame, requested_reject_owned) !=
        Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.store_recovery_plan_hole_owned_frame_and_record_pin(
            plan,
            requested_frame,
            std::move(requested_reject_owned),
            false,
            pin_list,
            summary
        ) != Cnr3Status::invalid_argument
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (requested_reject_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        summary.pin_recorded ||
        pin_list.pin_count() != expected_anchor_pin_count ||
        cache.total_pin_count() != static_cast<int>(expected_anchor_pin_count) ||
        vsapi_state.tracked_release_counts[4] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    Cnr3OwnedFrameRef first_hole_owned{};

    if (make_owned_frame(first_hole_frame, first_hole_owned) != Cnr3Status::ok) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.store_recovery_plan_hole_owned_frame_and_record_pin(
            plan,
            first_hole_frame_number,
            std::move(first_hole_owned),
            false,
            pin_list,
            summary
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (first_hole_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        summary.frame_number != first_hole_frame_number ||
        summary.requested_checkpoint ||
        !summary.inserted_new_slot ||
        summary.duplicate_existing_slot ||
        summary.checkpoint_promoted ||
        summary.resulting_slot_is_checkpoint ||
        !summary.pin_recorded ||
        !summary.incoming_frame_consumed ||
        summary.incoming_frame_rejected
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    const std::size_t expected_after_first_hole_pin_count =
        expected_anchor_pin_count + expected_first_hole_as2_pin_count;

    if (
        pin_list.pin_count() != expected_after_first_hole_pin_count ||
        cache.total_pin_count() != static_cast<int>(expected_after_first_hole_pin_count)
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.tracked_release_counts[1] != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    Cnr3OwnedFrameRef duplicate_winner_owned{};

    if (
        make_owned_frame(duplicate_winner_frame, duplicate_winner_owned) !=
        Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.store_noncheckpoint_owned_frame(
            duplicate_hole_frame_number,
            std::move(duplicate_winner_owned)
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (duplicate_winner_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        pin_list.pin_count() != expected_after_first_hole_pin_count ||
        cache.total_pin_count() != static_cast<int>(expected_after_first_hole_pin_count) ||
        vsapi_state.tracked_release_counts[2] != 0
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    Cnr3OwnedFrameRef duplicate_loser_owned{};

    if (
        make_owned_frame(duplicate_loser_frame, duplicate_loser_owned) !=
        Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        cache.store_recovery_plan_hole_owned_frame_and_record_pin(
            plan,
            duplicate_hole_frame_number,
            std::move(duplicate_loser_owned),
            false,
            pin_list,
            summary
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (duplicate_loser_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        summary.frame_number != duplicate_hole_frame_number ||
        summary.requested_checkpoint ||
        summary.inserted_new_slot ||
        !summary.duplicate_existing_slot ||
        summary.checkpoint_promoted ||
        summary.resulting_slot_is_checkpoint ||
        !summary.pin_recorded ||
        summary.incoming_frame_consumed ||
        !summary.incoming_frame_rejected
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        pin_list.pin_count() != expected_final_pin_list_count ||
        cache.total_pin_count() != static_cast<int>(expected_final_pin_list_count)
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        vsapi_state.tracked_release_counts[2] != 0 ||
        vsapi_state.tracked_release_counts[3] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (cache.clear() != Cnr3Status::lifecycle_violation) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        pin_list.pin_count() != expected_final_pin_list_count ||
        cache.total_pin_count() != static_cast<int>(expected_final_pin_list_count)
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (!pin_list.empty() || cache.total_pin_count() != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (cache.clear() != Cnr3Status::ok) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (!cache.empty() || !cache.cache_state_invariants_hold()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        vsapi_state.tracked_release_counts[0] != 1 ||
        vsapi_state.tracked_release_counts[1] != 1 ||
        vsapi_state.tracked_release_counts[2] != 1 ||
        vsapi_state.tracked_release_counts[3] != 1 ||
        vsapi_state.tracked_release_counts[4] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "H.3A AS2 recovery store-consumer scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    planned holes 11 and 12 are consumed through the existing AS2 helper"
    );
    cnr3_cache_core_selftest_trace_line(
        "    duplicate race: existing winner 12 survives, incoming loser is released once"
    );
    cnr3_cache_core_selftest_trace_line(
        "    expected pins: anchor 1 + inserted-hole AS2 1 + duplicate-hole AS2 1 = 3"
    );
    cnr3_cache_core_selftest_trace_line(
        "    G.12A owns freeFrame-outside-lock timing; H.3A proves recovery pin accounting"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_recovery_plan_contiguity_guard() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr int anchor_frame_number = 10;
    constexpr int first_hole_frame_number = 11;
    constexpr int second_hole_frame_number = 12;
    constexpr int corrupt_extra_hole_frame_number = 13;
    constexpr int normal_requested_frame = 13;
    constexpr int corrupt_requested_frame = 14;
    constexpr int recovery_back_radius = CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS;

    constexpr std::size_t expected_normal_hole_count = 2U;
    static_assert(expected_normal_hole_count == 2U);

    int normal_anchor_storage = 5200;
    int corrupt_incoming_storage = 5210;

    const VSFrame* normal_anchor_frame =
        reinterpret_cast<const VSFrame*>(&normal_anchor_storage);
    const VSFrame* corrupt_incoming_frame =
        reinterpret_cast<const VSFrame*>(&corrupt_incoming_storage);

    vsapi_state.tracked_release_frames[0] = normal_anchor_frame;
    vsapi_state.tracked_release_frames[1] = corrupt_incoming_frame;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto make_owned_frame = [
        &vsapi
    ](
        const VSFrame* frame,
        Cnr3OwnedFrameRef& owned_frame
    ) noexcept -> Cnr3Status {
        return owned_frame.reset_to_owned_frame(frame, &vsapi);
    };

    Cnr3OutputCacheCore normal_cache{};
    Cnr3OwnedFrameRef normal_anchor_owned{};

    if (
        make_owned_frame(normal_anchor_frame, normal_anchor_owned) !=
        Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        normal_cache.store_noncheckpoint_owned_frame(
            anchor_frame_number,
            std::move(normal_anchor_owned)
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (normal_anchor_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheRecoverySearchPlan normal_plan{};

    if (
        normal_cache.plan_bounded_recovery_search(
            normal_requested_frame,
            recovery_back_radius,
            normal_plan
        ) != Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    const std::vector<int> expected_normal_holes = {
        first_hole_frame_number,
        second_hole_frame_number
    };

    if (
        !normal_plan.anchor_found ||
        normal_plan.anchor_frame_number != anchor_frame_number ||
        normal_plan.anchor_pin_recorded ||
        !normal_plan.requested_frame_is_repair_target ||
        normal_plan.requested_frame_is_in_hole_catalogue ||
        normal_plan.hole_frame_numbers.size() != expected_normal_hole_count ||
        normal_plan.hole_frame_numbers != expected_normal_holes
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (normal_cache.clear() != Cnr3Status::ok) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        vsapi_state.tracked_release_counts[0] != 1 ||
        normal_cache.total_pin_count() != 0
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    Cnr3OutputCacheCore corrupt_cache{};
    Cnr3CachePinList corrupt_pin_list{};
    Cnr3CacheAs2StoreRecordSummary corrupt_summary{};
    Cnr3CacheRecoverySearchPlan corrupt_plan{};

    corrupt_plan.requested_frame = corrupt_requested_frame;
    corrupt_plan.max_back_radius = recovery_back_radius;
    corrupt_plan.search_lower_frame = 0;
    corrupt_plan.search_upper_frame = corrupt_requested_frame - 1;
    corrupt_plan.search_interval_has_frames = true;
    corrupt_plan.anchor_found = true;
    corrupt_plan.anchor_frame_number = anchor_frame_number;
    corrupt_plan.anchor_is_checkpoint = false;
    corrupt_plan.anchor_pin_recorded = false;
    corrupt_plan.requested_frame_is_repair_target = true;
    corrupt_plan.requested_frame_is_in_hole_catalogue = false;
    corrupt_plan.hole_frame_numbers = {
        first_hole_frame_number,
        corrupt_extra_hole_frame_number
    };

    Cnr3OwnedFrameRef corrupt_incoming_owned{};

    if (
        make_owned_frame(corrupt_incoming_frame, corrupt_incoming_owned) !=
        Cnr3Status::ok
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    const int free_count_before_corrupt_call = vsapi_state.free_frame_count;

    if (
        corrupt_cache.store_recovery_plan_hole_owned_frame_and_record_pin(
            corrupt_plan,
            corrupt_extra_hole_frame_number,
            std::move(corrupt_incoming_owned),
            false,
            corrupt_pin_list,
            corrupt_summary
        ) != Cnr3Status::invariant_violation
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (corrupt_incoming_owned.has_frame()) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (
        corrupt_summary.pin_recorded ||
        corrupt_summary.inserted_new_slot ||
        corrupt_summary.duplicate_existing_slot ||
        corrupt_summary.incoming_frame_consumed ||
        corrupt_summary.incoming_frame_rejected ||
        !corrupt_pin_list.empty() ||
        corrupt_cache.total_pin_count() != 0 ||
        corrupt_cache.slot_count() != 0U ||
        vsapi_state.free_frame_count != free_count_before_corrupt_call + 1 ||
        vsapi_state.tracked_release_counts[1] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "C.13B recovery-plan contiguity guard scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    source guard accepts reachable nearest-anchor + contiguous-hole plan 10 -> 13"
    );
    cnr3_cache_core_selftest_trace_line(
        "    consumer guard rejects corrupt non-contiguous holes {11,13} for requested 14"
    );
    cnr3_cache_core_selftest_trace_line(
        "    rejection returns invariant_violation before AS2 delegation, with no pins recorded"
    );
    cnr3_cache_core_selftest_trace_line(
        "    cache core emits no stderr; future getFrame integration owns developer-alert text"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_aggregate_cache_core_workload() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr int checkpoint_promote_frame_number = 10;
    constexpr int checkpoint_no_demote_frame_number = 20;
    constexpr std::size_t expected_as2_pin_per_call = 1U;
    constexpr std::size_t expected_sub1_as2_call_count = 4U;
    constexpr std::size_t expected_sub1_pin_count =
        expected_sub1_as2_call_count * expected_as2_pin_per_call;
    static_assert(expected_sub1_pin_count == 4U);

    constexpr std::uint64_t oversize_frame_byte_count =
        CNR3_CACHE_BYTE_BUDGET_BYTES + 1ULL;
    constexpr std::size_t retain_checkpoint_count = 1U;
    constexpr std::size_t max_remove_count = 3U;
    constexpr std::size_t total_prune_frame_count = 166U;
    constexpr std::size_t expected_active_ceiling = 150U;
    constexpr std::size_t expected_overflow_trigger = 165U;
    constexpr std::size_t expected_target_remove_count = 16U;
    constexpr std::size_t expected_prune_detached_count = 3U;
    static_assert(expected_prune_detached_count == max_remove_count);

    constexpr int recovery_anchor_frame_number = 30;
    constexpr int first_recovery_hole_frame_number = 31;
    constexpr int duplicate_recovery_hole_frame_number = 32;
    constexpr int recovery_requested_frame = 33;
    constexpr std::size_t expected_recovery_hole_count = 2U;
    constexpr std::size_t expected_anchor_pin_count = 1U;
    constexpr std::size_t expected_inserted_hole_as2_pin_count = 1U;
    constexpr std::size_t expected_duplicate_hole_as2_pin_count = 1U;
    constexpr std::size_t expected_recovery_pin_count =
        expected_anchor_pin_count +
        expected_inserted_hole_as2_pin_count +
        expected_duplicate_hole_as2_pin_count;
    static_assert(expected_recovery_hole_count == 2U);
    static_assert(expected_recovery_pin_count == 3U);

    constexpr int malformed_anchor_frame_number = 10;
    constexpr int malformed_requested_frame = 14;
    constexpr int malformed_first_hole_frame_number = 11;
    constexpr int malformed_noncontiguous_hole_frame_number = 13;
    constexpr std::size_t expected_guard_pin_delta = 0U;
    static_assert(expected_guard_pin_delta == 0U);

    int checkpoint_promote_winner_storage = 6000;
    int checkpoint_promote_loser_storage = 6001;
    int checkpoint_no_demote_winner_storage = 6002;
    int checkpoint_no_demote_loser_storage = 6003;
    int recovery_anchor_storage = 6004;
    int recovery_first_hole_storage = 6005;
    int recovery_duplicate_winner_storage = 6006;
    int recovery_duplicate_loser_storage = 6007;
    int malformed_incoming_storage = 6008;

    int prune_frame_storage[total_prune_frame_count] = {};

    for (std::size_t index = 0U; index < total_prune_frame_count; ++index) {
        prune_frame_storage[index] = static_cast<int>(6100U + index);
    }

    const VSFrame* checkpoint_promote_winner_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_promote_winner_storage);
    const VSFrame* checkpoint_promote_loser_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_promote_loser_storage);
    const VSFrame* checkpoint_no_demote_winner_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_no_demote_winner_storage);
    const VSFrame* checkpoint_no_demote_loser_frame =
        reinterpret_cast<const VSFrame*>(&checkpoint_no_demote_loser_storage);
    const VSFrame* recovery_anchor_frame =
        reinterpret_cast<const VSFrame*>(&recovery_anchor_storage);
    const VSFrame* recovery_first_hole_frame =
        reinterpret_cast<const VSFrame*>(&recovery_first_hole_storage);
    const VSFrame* recovery_duplicate_winner_frame =
        reinterpret_cast<const VSFrame*>(&recovery_duplicate_winner_storage);
    const VSFrame* recovery_duplicate_loser_frame =
        reinterpret_cast<const VSFrame*>(&recovery_duplicate_loser_storage);
    const VSFrame* malformed_incoming_frame =
        reinterpret_cast<const VSFrame*>(&malformed_incoming_storage);

    const VSFrame* removed_checkpoint_frame =
        reinterpret_cast<const VSFrame*>(&prune_frame_storage[165]);
    const VSFrame* removed_first_noncheckpoint_frame =
        reinterpret_cast<const VSFrame*>(&prune_frame_storage[163]);
    const VSFrame* removed_second_noncheckpoint_frame =
        reinterpret_cast<const VSFrame*>(&prune_frame_storage[162]);

    vsapi_state.tracked_release_frames[0] = checkpoint_promote_loser_frame;
    vsapi_state.tracked_release_frames[1] = checkpoint_no_demote_loser_frame;
    vsapi_state.tracked_release_frames[2] = removed_checkpoint_frame;
    vsapi_state.tracked_release_frames[3] = removed_first_noncheckpoint_frame;
    vsapi_state.tracked_release_frames[4] = removed_second_noncheckpoint_frame;
    vsapi_state.tracked_release_frames[5] = recovery_duplicate_loser_frame;
    vsapi_state.tracked_release_frames[6] = malformed_incoming_frame;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto make_owned_frame = [
        &vsapi
    ](
        const VSFrame* frame,
        Cnr3OwnedFrameRef& owned_frame
    ) noexcept -> Cnr3Status {
        return owned_frame.reset_to_owned_frame(frame, &vsapi);
    };

    const auto fail = []() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheAs2StoreRecordSummary summary{};

        Cnr3OwnedFrameRef checkpoint_promote_winner{};

        if (
            make_owned_frame(
                checkpoint_promote_winner_frame,
                checkpoint_promote_winner
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_owned_frame_and_record_pin(
                checkpoint_promote_frame_number,
                std::move(checkpoint_promote_winner),
                false,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            summary.frame_number != checkpoint_promote_frame_number ||
            summary.requested_checkpoint ||
            !summary.inserted_new_slot ||
            summary.duplicate_existing_slot ||
            summary.checkpoint_promoted ||
            summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            !summary.incoming_frame_consumed ||
            summary.incoming_frame_rejected
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef checkpoint_promote_loser{};

        if (
            make_owned_frame(
                checkpoint_promote_loser_frame,
                checkpoint_promote_loser
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_owned_frame_and_record_pin(
                checkpoint_promote_frame_number,
                std::move(checkpoint_promote_loser),
                true,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            summary.frame_number != checkpoint_promote_frame_number ||
            !summary.requested_checkpoint ||
            summary.inserted_new_slot ||
            !summary.duplicate_existing_slot ||
            !summary.checkpoint_promoted ||
            !summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            summary.incoming_frame_consumed ||
            !summary.incoming_frame_rejected ||
            vsapi_state.tracked_release_counts[0] != 1
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef checkpoint_no_demote_winner{};

        if (
            make_owned_frame(
                checkpoint_no_demote_winner_frame,
                checkpoint_no_demote_winner
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_owned_frame_and_record_pin(
                checkpoint_no_demote_frame_number,
                std::move(checkpoint_no_demote_winner),
                true,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef checkpoint_no_demote_loser{};

        if (
            make_owned_frame(
                checkpoint_no_demote_loser_frame,
                checkpoint_no_demote_loser
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_owned_frame_and_record_pin(
                checkpoint_no_demote_frame_number,
                std::move(checkpoint_no_demote_loser),
                false,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            summary.frame_number != checkpoint_no_demote_frame_number ||
            summary.requested_checkpoint ||
            summary.inserted_new_slot ||
            !summary.duplicate_existing_slot ||
            summary.checkpoint_promoted ||
            !summary.resulting_slot_is_checkpoint ||
            !summary.pin_recorded ||
            summary.incoming_frame_consumed ||
            !summary.incoming_frame_rejected ||
            vsapi_state.tracked_release_counts[1] != 1
            ) {
            return fail();
        }

        if (
            pin_list.pin_count() != expected_sub1_pin_count ||
            cache.total_pin_count() != static_cast<int>(expected_sub1_pin_count) ||
            cache.slot_count() != 2U ||
            cache.checkpoint_count() != 2U
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef lookup_frame{};
        const int add_ref_count_before_lookup = vsapi_state.add_frame_ref_count;
        const int free_count_before_lookup = vsapi_state.free_frame_count;

        if (
            cache.lookup_frame_and_add_ref(
                checkpoint_promote_frame_number,
                &vsapi,
                lookup_frame
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            lookup_frame.get() != checkpoint_promote_winner_frame ||
            vsapi_state.add_frame_ref_count != add_ref_count_before_lookup + 1
            ) {
            return fail();
        }

        lookup_frame.reset();

        if (vsapi_state.free_frame_count != free_count_before_lookup + 1) {
            return fail();
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            return fail();
        }

        if (!pin_list.empty() || cache.total_pin_count() != 0) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }
    }

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        if (cache.record_hot_zone_observation(100) != Cnr3Status::ok) {
            return fail();
        }

        const auto store_prune_frame = [
            &cache,
            &vsapi,
            &prune_frame_storage
        ](
            int frame_number,
            bool is_checkpoint
        ) noexcept -> Cnr3Status {
            Cnr3OwnedFrameRef owned_frame{};
            const VSFrame* frame =
                reinterpret_cast<const VSFrame*>(&prune_frame_storage[frame_number]);

            const Cnr3Status adopt_status =
                owned_frame.reset_to_owned_frame(frame, &vsapi);

            if (!cnr3_status_is_ok(adopt_status)) {
                return adopt_status;
            }

            const Cnr3Status store_status =
                is_checkpoint
                ? cache.store_checkpoint_owned_frame(
                    frame_number,
                    std::move(owned_frame)
                )
                : cache.store_noncheckpoint_owned_frame(
                    frame_number,
                    std::move(owned_frame)
                );

            if (!cnr3_status_is_ok(store_status)) {
                return store_status;
            }

            if (owned_frame.has_frame()) {
                return Cnr3Status::ownership_violation;
            }

            return Cnr3Status::ok;
        };

        for (int frame_number = 0; frame_number <= 164; ++frame_number) {
            const bool is_checkpoint = (frame_number == 0);

            if (store_prune_frame(frame_number, is_checkpoint) != Cnr3Status::ok) {
                return fail();
            }
        }

        Cnr3CachePruneExecutionSummary no_prune_summary{};

        if (
            cache.execute_bounded_prune_pass(
                oversize_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                no_prune_summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            no_prune_summary.trigger_decision.current_slot_count != 165U ||
            no_prune_summary.trigger_decision.prune_is_required ||
            no_prune_summary.selected_candidate_count != 0U ||
            no_prune_summary.detached_count != 0U
            ) {
            return fail();
        }

        if (store_prune_frame(165, true) != Cnr3Status::ok) {
            return fail();
        }

        if (
            cache.lookup_frame_and_record_pin(164, pin_list) !=
            Cnr3Status::ok
            ) {
            return fail();
        }

        const int hot_zone_pinned_frames[] = {55, 60, 65};

        for (const int pinned_frame_number : hot_zone_pinned_frames) {
            if (
                cache.lookup_frame_and_record_pin(
                    pinned_frame_number,
                    pin_list
                ) != Cnr3Status::ok
                ) {
                return fail();
            }
        }

        if (cache.total_pin_count() != 4 || pin_list.pin_count() != 4U) {
            return fail();
        }

        Cnr3CachePruneExecutionSummary prune_summary{};
        const int free_count_before_prune = vsapi_state.free_frame_count;

        if (
            cache.execute_bounded_prune_pass(
                oversize_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                prune_summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !prune_summary.trigger_decision.prune_is_required ||
            prune_summary.trigger_decision.active_ceiling_frame_count !=
                expected_active_ceiling ||
            prune_summary.trigger_decision.overflow_trigger_frame_count !=
                expected_overflow_trigger ||
            prune_summary.trigger_decision.current_slot_count !=
                total_prune_frame_count ||
            prune_summary.trigger_decision.target_remove_count !=
                expected_target_remove_count ||
            prune_summary.bounded_remove_limit != max_remove_count ||
            prune_summary.selected_candidate_count != expected_prune_detached_count ||
            prune_summary.detached_count != expected_prune_detached_count ||
            cache.slot_count() !=
                (total_prune_frame_count - expected_prune_detached_count) ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 4 ||
            vsapi_state.free_frame_count !=
                free_count_before_prune +
                static_cast<int>(expected_prune_detached_count) ||
            vsapi_state.tracked_release_counts[2] != 1 ||
            vsapi_state.tracked_release_counts[3] != 1 ||
            vsapi_state.tracked_release_counts[4] != 1
            ) {
            return fail();
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            return fail();
        }

        if (!pin_list.empty() || cache.total_pin_count() != 0) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }
    }

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheRecoverySearchPlan plan{};
        Cnr3CacheAs2StoreRecordSummary summary{};

        Cnr3OwnedFrameRef anchor_owned{};

        if (make_owned_frame(recovery_anchor_frame, anchor_owned) != Cnr3Status::ok) {
            return fail();
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                recovery_anchor_frame_number,
                std::move(anchor_owned)
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.plan_bounded_recovery_search_and_record_anchor_pin(
                recovery_requested_frame,
                CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
                pin_list,
                plan
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        const std::vector<int> expected_recovery_holes = {
            first_recovery_hole_frame_number,
            duplicate_recovery_hole_frame_number
        };

        if (
            !plan.anchor_found ||
            plan.anchor_frame_number != recovery_anchor_frame_number ||
            !plan.anchor_pin_recorded ||
            !plan.requested_frame_is_repair_target ||
            plan.requested_frame_is_in_hole_catalogue ||
            plan.hole_frame_numbers.size() != expected_recovery_hole_count ||
            plan.hole_frame_numbers != expected_recovery_holes ||
            pin_list.pin_count() != expected_anchor_pin_count ||
            cache.total_pin_count() != static_cast<int>(expected_anchor_pin_count)
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef first_hole_owned{};

        if (
            make_owned_frame(recovery_first_hole_frame, first_hole_owned) !=
            Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_recovery_plan_hole_owned_frame_and_record_pin(
                plan,
                first_recovery_hole_frame_number,
                std::move(first_hole_owned),
                false,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            summary.frame_number != first_recovery_hole_frame_number ||
            !summary.inserted_new_slot ||
            summary.duplicate_existing_slot ||
            !summary.pin_recorded ||
            !summary.incoming_frame_consumed ||
            summary.incoming_frame_rejected
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef duplicate_winner_owned{};

        if (
            make_owned_frame(
                recovery_duplicate_winner_frame,
                duplicate_winner_owned
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                duplicate_recovery_hole_frame_number,
                std::move(duplicate_winner_owned)
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef duplicate_loser_owned{};

        if (
            make_owned_frame(
                recovery_duplicate_loser_frame,
                duplicate_loser_owned
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.store_recovery_plan_hole_owned_frame_and_record_pin(
                plan,
                duplicate_recovery_hole_frame_number,
                std::move(duplicate_loser_owned),
                false,
                pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            summary.frame_number != duplicate_recovery_hole_frame_number ||
            summary.inserted_new_slot ||
            !summary.duplicate_existing_slot ||
            !summary.pin_recorded ||
            summary.incoming_frame_consumed ||
            !summary.incoming_frame_rejected ||
            vsapi_state.tracked_release_counts[5] != 1 ||
            pin_list.pin_count() != expected_recovery_pin_count ||
            cache.total_pin_count() != static_cast<int>(expected_recovery_pin_count)
            ) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            return fail();
        }

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            return fail();
        }

        if (!pin_list.empty() || cache.total_pin_count() != 0) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }
    }

    {
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheAs2StoreRecordSummary summary{};
        Cnr3CacheRecoverySearchPlan malformed_plan{};

        malformed_plan.requested_frame = malformed_requested_frame;
        malformed_plan.max_back_radius = CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS;
        malformed_plan.search_lower_frame = 0;
        malformed_plan.search_upper_frame = malformed_requested_frame - 1;
        malformed_plan.search_interval_has_frames = true;
        malformed_plan.anchor_found = true;
        malformed_plan.anchor_frame_number = malformed_anchor_frame_number;
        malformed_plan.anchor_is_checkpoint = false;
        malformed_plan.anchor_pin_recorded = false;
        malformed_plan.requested_frame_is_repair_target = true;
        malformed_plan.requested_frame_is_in_hole_catalogue = false;
        malformed_plan.hole_frame_numbers = {
            malformed_first_hole_frame_number,
            malformed_noncontiguous_hole_frame_number
        };

        Cnr3OwnedFrameRef malformed_incoming_owned{};

        if (
            make_owned_frame(malformed_incoming_frame, malformed_incoming_owned) !=
            Cnr3Status::ok
            ) {
            return fail();
        }

        const int free_count_before_guard = vsapi_state.free_frame_count;

        if (
            cache.store_recovery_plan_hole_owned_frame_and_record_pin(
                malformed_plan,
                malformed_noncontiguous_hole_frame_number,
                std::move(malformed_incoming_owned),
                false,
                pin_list,
                summary
            ) != Cnr3Status::invariant_violation
            ) {
            return fail();
        }

        if (
            summary.pin_recorded ||
            summary.inserted_new_slot ||
            summary.duplicate_existing_slot ||
            summary.incoming_frame_consumed ||
            summary.incoming_frame_rejected ||
            !pin_list.empty() ||
            cache.total_pin_count() != 0 ||
            cache.slot_count() != 0U ||
            vsapi_state.free_frame_count != free_count_before_guard + 1 ||
            vsapi_state.tracked_release_counts[6] != 1
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "C.14A aggregate cache-core workload scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    sub-1 AS2 duplicate/adopt proves checkpoint promote and no-demote with 4 pins"
    );
    cnr3_cache_core_selftest_trace_line(
        "    sub-2 hot-zone/prune executes bounded AS5 detach/free with D-SUM-11 observe-only"
    );
    cnr3_cache_core_selftest_trace_line(
        "    sub-3 recovery pins anchor 30 and fills holes 31,32 through AS2 duplicate/adopt"
    );
    cnr3_cache_core_selftest_trace_line(
        "    sub-4 C.13B guard rejects non-contiguous holes {11,13} by hard status only"
    );
    cnr3_cache_core_selftest_trace_line(
        "    behavioural assertions do not read D-SUM counters; macro-off must also pass"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_response_table_vector_proof() noexcept {
    /*
        P.1A temporarily hosts this first pixel-number proof in the established
        selftest runner so the four-way count and forced-fail machinery remain
        unchanged. The active response-table code stays separate from cache
        state and from VapourSynth instance/getFrame lifecycle.
    */
    constexpr int table_offset_8bit = 255;
    constexpr int table_size_8bit = 511;
    constexpr int sample_peak_8bit = 255;

    constexpr int expected_peak_strength_255 = 254;
    constexpr int expected_narrow_255_t10_d5 = 127;
    constexpr int expected_wide_255_t10_d5 = 216;
    constexpr int expected_narrow_200_t20_d7 = 145;
    constexpr int expected_wide_200_t20_d7 = 192;

    static_assert(table_size_8bit == (table_offset_8bit * 2) + 1);
    static_assert(expected_peak_strength_255 == 254);
    static_assert(expected_narrow_255_t10_d5 == 127);
    static_assert(expected_wide_255_t10_d5 == 216);
    static_assert(expected_narrow_200_t20_d7 == 145);
    static_assert(expected_wide_200_t20_d7 == 192);

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto expect_table_value = [](
        const std::vector<int>& table,
        int table_offset,
        int signed_diff,
        int expected_value
    ) noexcept -> bool {
        return get_cnr3_table_value_for_signed_diff(
            table,
            table_offset,
            signed_diff
        ) == expected_value;
    };

    {
        const std::vector<int> table = {
            0,
            10,
            20,
            30,
            40
        };
        constexpr int lookup_offset = 2;

        if (
            !expect_table_value(table, lookup_offset, -2, 0) ||
            !expect_table_value(table, lookup_offset, -1, 10) ||
            !expect_table_value(table, lookup_offset, 0, 20) ||
            !expect_table_value(table, lookup_offset, 1, 30) ||
            !expect_table_value(table, lookup_offset, 2, 40) ||
            !expect_table_value(table, lookup_offset, -3, 0) ||
            !expect_table_value(table, lookup_offset, 3, 0)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> table{};

        if (
            build_cnr3_weight_table(
                table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                0,
                200,
                false
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            table.size() != static_cast<std::size_t>(table_size_8bit) ||
            !expect_table_value(table, table_offset_8bit, 0, 200) ||
            !expect_table_value(table, table_offset_8bit, -1, 0) ||
            !expect_table_value(table, table_offset_8bit, 1, 0)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> table{};

        if (
            build_cnr3_weight_table(
                table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                10,
                255,
                false
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !expect_table_value(table, table_offset_8bit, 0, expected_peak_strength_255) ||
            !expect_table_value(table, table_offset_8bit, -5, expected_narrow_255_t10_d5) ||
            !expect_table_value(table, table_offset_8bit, 5, expected_narrow_255_t10_d5) ||
            !expect_table_value(table, table_offset_8bit, -10, 0) ||
            !expect_table_value(table, table_offset_8bit, 10, 0) ||
            !expect_table_value(table, table_offset_8bit, -11, 0) ||
            !expect_table_value(table, table_offset_8bit, 11, 0)
            ) {
            return fail();
        }

        if (
            get_cnr3_table_value_for_signed_diff(table, table_offset_8bit, -5) !=
            get_cnr3_table_value_for_signed_diff(table, table_offset_8bit, 5)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> table{};

        if (
            build_cnr3_weight_table(
                table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                10,
                255,
                true
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !expect_table_value(table, table_offset_8bit, 0, expected_peak_strength_255) ||
            !expect_table_value(table, table_offset_8bit, -5, expected_wide_255_t10_d5) ||
            !expect_table_value(table, table_offset_8bit, 5, expected_wide_255_t10_d5) ||
            !expect_table_value(table, table_offset_8bit, -10, 0) ||
            !expect_table_value(table, table_offset_8bit, 10, 0)
            ) {
            return fail();
        }

        if (
            get_cnr3_table_value_for_signed_diff(table, table_offset_8bit, -5) !=
            get_cnr3_table_value_for_signed_diff(table, table_offset_8bit, 5)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> narrow_table{};
        std::vector<int> wide_table{};

        if (
            build_cnr3_weight_table(
                narrow_table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                20,
                200,
                false
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            build_cnr3_weight_table(
                wide_table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                20,
                200,
                true
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !expect_table_value(narrow_table, table_offset_8bit, 0, 200) ||
            !expect_table_value(narrow_table, table_offset_8bit, -7, expected_narrow_200_t20_d7) ||
            !expect_table_value(narrow_table, table_offset_8bit, 7, expected_narrow_200_t20_d7) ||
            !expect_table_value(narrow_table, table_offset_8bit, -20, 0) ||
            !expect_table_value(narrow_table, table_offset_8bit, 20, 0) ||
            !expect_table_value(wide_table, table_offset_8bit, 0, 200) ||
            !expect_table_value(wide_table, table_offset_8bit, -7, expected_wide_200_t20_d7) ||
            !expect_table_value(wide_table, table_offset_8bit, 7, expected_wide_200_t20_d7) ||
            !expect_table_value(wide_table, table_offset_8bit, -20, 0) ||
            !expect_table_value(wide_table, table_offset_8bit, 20, 0)
            ) {
            return fail();
        }

        if (
            get_cnr3_table_value_for_signed_diff(narrow_table, table_offset_8bit, -7) !=
            get_cnr3_table_value_for_signed_diff(narrow_table, table_offset_8bit, 7)
            ) {
            return fail();
        }

        if (
            get_cnr3_table_value_for_signed_diff(wide_table, table_offset_8bit, -7) !=
            get_cnr3_table_value_for_signed_diff(wide_table, table_offset_8bit, 7)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> table{};

        if (
            build_cnr3_weight_table(
                table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                -5,
                300,
                false
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !expect_table_value(table, table_offset_8bit, 0, 255) ||
            !expect_table_value(table, table_offset_8bit, 1, 0)
            ) {
            return fail();
        }

        if (
            build_cnr3_weight_table(
                table,
                table_offset_8bit,
                table_size_8bit,
                sample_peak_8bit,
                300,
                -1,
                true
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !expect_table_value(table, table_offset_8bit, 0, 0) ||
            !expect_table_value(table, table_offset_8bit, 1, 0)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> table{};

        if (
            build_cnr3_weight_table(
                table,
                table_offset_8bit,
                0,
                sample_peak_8bit,
                10,
                255,
                false
            ) != Cnr3Status::invalid_argument
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "P.1A response-table vector proof scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    safe signed lookup proves table_offset indexing and out-of-range zero"
    );
    cnr3_cache_core_selftest_trace_line(
        "    threshold-zero proof sets only the centre entry"
    );
    cnr3_cache_core_selftest_trace_line(
        "    narrow 255/10 proves diff 5 -> 127 and peak 254 integer-division quirk"
    );
    cnr3_cache_core_selftest_trace_line(
        "    wide 255/10 proves diff 5 -> 216 through squared response curve"
    );
    cnr3_cache_core_selftest_trace_line(
        "    second 200/20 family proves narrow diff 7 -> 145 and wide diff 7 -> 192"
    );
    cnr3_cache_core_selftest_trace_line(
        "    clamp vectors prove threshold/strength are clamped before table generation"
    );

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_response_table_config_surface_proof() noexcept {
    /*
        P.2A still hosts the pixel-number proof in the established runner so
        the four-way count and forced-fail machinery remain unchanged. Reassess
        a separate pixel selftest module at P.3A, when project-file wiring can
        be reviewed deliberately.
    */
    constexpr int expected_peak_8bit = 255;
    constexpr int expected_offset_8bit = 255;
    constexpr int expected_size_8bit = 511;

    constexpr int expected_peak_10bit = 1023;
    constexpr int expected_offset_10bit = 1023;
    constexpr int expected_size_10bit = 2047;

    constexpr int expected_peak_16bit = 65535;
    constexpr int expected_offset_16bit = 65535;
    constexpr int expected_size_16bit = 131071;

    constexpr int max_sample_peak_without_geometry_overflow =
        (std::numeric_limits<int>::max() - 1) / 2;
    constexpr int first_overflowing_sample_peak =
        max_sample_peak_without_geometry_overflow + 1;

    constexpr int expected_scale_10_to_16bit = 2570;
    constexpr int expected_scale_20_to_16bit = 5140;
    constexpr int expected_scale_200_to_16bit = 51400;
    constexpr int expected_scale_128_to_10bit = 514;

    constexpr int expected_p1_peak_strength_255 = 254;
    constexpr int expected_p1_narrow_255_t10_d5 = 127;
    constexpr int expected_p1_wide_255_t10_d5 = 216;
    constexpr int expected_p1_wide_200_t20_d7 = 192;

    constexpr int expected_y16_peak_strength = 65534;
    constexpr int expected_y16_narrow_mid = 32767;
    constexpr int expected_u16_peak_strength = 51400;
    constexpr int expected_u16_wide_mid = 43872;

    static_assert(expected_size_8bit == (expected_offset_8bit * 2) + 1);
    static_assert(expected_size_10bit == (expected_offset_10bit * 2) + 1);
    static_assert(expected_size_16bit == (expected_offset_16bit * 2) + 1);
    static_assert(
        (max_sample_peak_without_geometry_overflow * 2) + 1 ==
        std::numeric_limits<int>::max()
    );

    static_assert(expected_scale_10_to_16bit == 2570);
    static_assert(expected_scale_20_to_16bit == 5140);
    static_assert(expected_scale_200_to_16bit == 51400);
    static_assert(expected_scale_128_to_10bit == 514);

    static_assert(expected_p1_peak_strength_255 == 254);
    static_assert(expected_p1_narrow_255_t10_d5 == 127);
    static_assert(expected_p1_wide_255_t10_d5 == 216);
    static_assert(expected_p1_wide_200_t20_d7 == 192);

    static_assert(expected_y16_peak_strength == 65534);
    static_assert(expected_y16_narrow_mid == 32767);
    static_assert(expected_u16_peak_strength == 51400);
    static_assert(expected_u16_wide_mid == 43872);

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto expect_table_value = [](
        const std::vector<int>& table,
        int table_offset,
        int signed_diff,
        int expected_value
    ) noexcept -> bool {
        return get_cnr3_table_value_for_signed_diff(
            table,
            table_offset,
            signed_diff
        ) == expected_value;
    };

    {
        int table_offset = 0;
        int table_size = 0;

        if (
            cnr3_response_table_geometry_for_sample_peak(
                expected_peak_8bit,
                table_offset,
                table_size
            ) != Cnr3Status::ok ||
            table_offset != expected_offset_8bit ||
            table_size != expected_size_8bit
            ) {
            return fail();
        }

        if (
            cnr3_response_table_geometry_for_sample_peak(
                expected_peak_10bit,
                table_offset,
                table_size
            ) != Cnr3Status::ok ||
            table_offset != expected_offset_10bit ||
            table_size != expected_size_10bit
            ) {
            return fail();
        }

        if (
            cnr3_response_table_geometry_for_sample_peak(
                0,
                table_offset,
                table_size
            ) != Cnr3Status::invalid_argument ||
            table_offset != 0 ||
            table_size != 0
            ) {
            return fail();
        }

        if (
            cnr3_response_table_geometry_for_sample_peak(
                -1,
                table_offset,
                table_size
            ) != Cnr3Status::invalid_argument ||
            table_offset != 0 ||
            table_size != 0
            ) {
            return fail();
        }

        if (
            cnr3_response_table_geometry_for_sample_peak(
                max_sample_peak_without_geometry_overflow,
                table_offset,
                table_size
            ) != Cnr3Status::ok ||
            table_offset != max_sample_peak_without_geometry_overflow ||
            table_size != std::numeric_limits<int>::max()
            ) {
            return fail();
        }

        if (
            cnr3_response_table_geometry_for_sample_peak(
                first_overflowing_sample_peak,
                table_offset,
                table_size
            ) != Cnr3Status::invalid_argument ||
            table_offset != 0 ||
            table_size != 0
            ) {
            return fail();
        }
    }

    if (
        cnr3_scale_8bit_parameter_to_sample_peak(-5, expected_peak_8bit) != 0 ||
        cnr3_scale_8bit_parameter_to_sample_peak(0, expected_peak_8bit) != 0 ||
        cnr3_scale_8bit_parameter_to_sample_peak(10, expected_peak_8bit) != 10 ||
        cnr3_scale_8bit_parameter_to_sample_peak(255, expected_peak_8bit) != 255 ||
        cnr3_scale_8bit_parameter_to_sample_peak(300, expected_peak_8bit) != 255 ||
        cnr3_scale_8bit_parameter_to_sample_peak(10, expected_peak_16bit) != expected_scale_10_to_16bit ||
        cnr3_scale_8bit_parameter_to_sample_peak(20, expected_peak_16bit) != expected_scale_20_to_16bit ||
        cnr3_scale_8bit_parameter_to_sample_peak(200, expected_peak_16bit) != expected_scale_200_to_16bit ||
        cnr3_scale_8bit_parameter_to_sample_peak(255, expected_peak_16bit) != expected_peak_16bit ||
        cnr3_scale_8bit_parameter_to_sample_peak(128, expected_peak_10bit) != expected_scale_128_to_10bit
        ) {
        return fail();
    }

    {
        Cnr3ResponseTableConfig config{};
        config.sample_peak = expected_peak_8bit;
        config.y.threshold_8bit = 10;
        config.y.strength_8bit = 255;
        config.y.curve = Cnr3ResponseCurveKind::wide;
        config.u.threshold_8bit = 10;
        config.u.strength_8bit = 255;
        config.u.curve = Cnr3ResponseCurveKind::narrow;
        config.v.threshold_8bit = 20;
        config.v.strength_8bit = 200;
        config.v.curve = Cnr3ResponseCurveKind::wide;

        Cnr3ResponseTables tables{};

        if (build_cnr3_response_tables(config, tables) != Cnr3Status::ok) {
            return fail();
        }

        if (
            tables.sample_peak != expected_peak_8bit ||
            tables.table_offset != expected_offset_8bit ||
            tables.table_size != expected_size_8bit ||
            tables.y.size() != static_cast<std::size_t>(expected_size_8bit) ||
            tables.u.size() != static_cast<std::size_t>(expected_size_8bit) ||
            tables.v.size() != static_cast<std::size_t>(expected_size_8bit)
            ) {
            return fail();
        }

        if (
            !expect_table_value(tables.y, tables.table_offset, 0, expected_p1_peak_strength_255) ||
            !expect_table_value(tables.y, tables.table_offset, 5, expected_p1_wide_255_t10_d5) ||
            !expect_table_value(tables.u, tables.table_offset, 0, expected_p1_peak_strength_255) ||
            !expect_table_value(tables.u, tables.table_offset, 5, expected_p1_narrow_255_t10_d5) ||
            !expect_table_value(tables.v, tables.table_offset, 0, 200) ||
            !expect_table_value(tables.v, tables.table_offset, 7, expected_p1_wide_200_t20_d7)
            ) {
            return fail();
        }
    }

    {
        Cnr3ResponseTableConfig config{};
        config.sample_peak = expected_peak_16bit;
        config.y.threshold_8bit = 10;
        config.y.strength_8bit = 255;
        config.y.curve = Cnr3ResponseCurveKind::narrow;
        config.u.threshold_8bit = 20;
        config.u.strength_8bit = 200;
        config.u.curve = Cnr3ResponseCurveKind::wide;
        config.v.threshold_8bit = 0;
        config.v.strength_8bit = 300;
        config.v.curve = Cnr3ResponseCurveKind::narrow;

        Cnr3ResponseTables tables{};

        if (build_cnr3_response_tables(config, tables) != Cnr3Status::ok) {
            return fail();
        }

        if (
            tables.sample_peak != expected_peak_16bit ||
            tables.table_offset != expected_offset_16bit ||
            tables.table_size != expected_size_16bit ||
            tables.y.size() != static_cast<std::size_t>(expected_size_16bit) ||
            tables.u.size() != static_cast<std::size_t>(expected_size_16bit) ||
            tables.v.size() != static_cast<std::size_t>(expected_size_16bit)
            ) {
            return fail();
        }

        if (
            !expect_table_value(tables.y, tables.table_offset, 0, expected_y16_peak_strength) ||
            !expect_table_value(tables.y, tables.table_offset, 1285, expected_y16_narrow_mid) ||
            !expect_table_value(tables.y, tables.table_offset, 2570, 0) ||
            !expect_table_value(tables.u, tables.table_offset, 0, expected_u16_peak_strength) ||
            !expect_table_value(tables.u, tables.table_offset, 2570, expected_u16_wide_mid) ||
            !expect_table_value(tables.u, tables.table_offset, 5140, 0) ||
            !expect_table_value(tables.v, tables.table_offset, 0, expected_peak_16bit) ||
            !expect_table_value(tables.v, tables.table_offset, 1, 0)
            ) {
            return fail();
        }
    }

    {
        Cnr3ResponseTables tables{};
        tables.sample_peak = expected_peak_8bit;
        tables.table_offset = expected_offset_8bit;
        tables.table_size = expected_size_8bit;
        tables.y = { 1 };
        tables.u = { 2 };
        tables.v = { 3 };

        Cnr3ResponseTableConfig invalid_config{};
        invalid_config.sample_peak = -1;

        if (
            build_cnr3_response_tables(invalid_config, tables) !=
            Cnr3Status::invalid_argument
            ) {
            return fail();
        }

        if (
            tables.sample_peak != expected_peak_8bit ||
            tables.table_offset != expected_offset_8bit ||
            tables.table_size != expected_size_8bit ||
            tables.y.size() != 1U ||
            tables.u.size() != 1U ||
            tables.v.size() != 1U ||
            tables.y[0] != 1 ||
            tables.u[0] != 2 ||
            tables.v[0] != 3
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "P.2A response-table config surface proof scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    geometry proof maps sample_peak to signed table offset and size"
    );
    cnr3_cache_core_selftest_trace_line(
        "    scaling proof codifies round-to-nearest 8-bit-domain native scaling"
    );
    cnr3_cache_core_selftest_trace_line(
        "    8-bit Y/U/V build proves per-plane narrow/wide configs do not cross"
    );
    cnr3_cache_core_selftest_trace_line(
        "    16-bit build proves scaled thresholds/strengths and native table geometry"
    );
    cnr3_cache_core_selftest_trace_line(
        "    Y table is a luma-difference response for chroma blend, not a luma filter"
    );
    cnr3_cache_core_selftest_trace_line(
        "    subsampling traversal, downSampleLuma, and int64 blend are deferred to P.3A/P.4A/P.5A"
    );
    cnr3_cache_core_selftest_trace_line(
        "    invalid config proof preserves prior output tables without partial publish"
    );
    cnr3_cache_core_selftest_trace_line(
        "    reassess separate pixel selftest module at P.3A with project wiring review"
    );

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_weighted_chroma_blend_vector_proof() noexcept {
    /*
        P.3A continues to host the pixel-number proof in the established runner.
        A separate pixel selftest module remains deferred until VS project-file
        wiring can be reviewed deliberately before P.4A/P.5A.
    */
    constexpr int bits_8 = 8;
    constexpr int bits_16 = 16;

    constexpr std::int64_t expected_shift_8bit = 65536;
    constexpr std::int64_t expected_shift_16bit = 4294967296LL;

    constexpr std::int64_t expected_weight_8bit_max_from_tables = 64516;
    constexpr int expected_blend_8bit_max_cur10_prev200 = 197;
    constexpr int expected_blend_8bit_narrow_mid = 104;
    constexpr int expected_blend_8bit_wide_mid = 169;
    constexpr int expected_blend_8bit_200_20_family = 142;

    constexpr std::int64_t expected_weight_8bit_validation_max = 65025;
    constexpr std::int64_t expected_shift_minus_weight_8bit_validation_max = 511;
    constexpr int expected_blend_8bit_validation_max = 199;

    constexpr int expected_blend_8bit_prev_high = 251;
    constexpr int expected_blend_8bit_cur_high = 4;
    constexpr int expected_blend_8bit_half_point = 1;

    constexpr std::int64_t expected_weight_16bit_max_from_tables = 4294705156LL;
    constexpr int expected_blend_16bit_max_from_tables = 49997;
    constexpr int expected_blend_16bit_mid = 25499;
    constexpr std::int64_t expected_weight_16bit_scaled_family = 2255020800LL;
    constexpr int expected_blend_16bit_scaled_family = 26727;

    constexpr std::int64_t expected_weight_16bit_validation_max = 4294836225LL;
    constexpr std::int64_t expected_shift_minus_weight_16bit_validation_max = 131071;
    constexpr int expected_blend_16bit_validation_max = 49999;

    static_assert(expected_shift_8bit == (1LL << (bits_8 << 1)));
    static_assert(expected_shift_16bit == (1LL << (bits_16 << 1)));

    static_assert(expected_weight_8bit_max_from_tables == 64516);
    static_assert(expected_blend_8bit_max_cur10_prev200 == 197);
    static_assert(expected_blend_8bit_narrow_mid == 104);
    static_assert(expected_blend_8bit_wide_mid == 169);
    static_assert(expected_blend_8bit_200_20_family == 142);

    static_assert(expected_weight_8bit_validation_max == 65025);
    static_assert(expected_shift_minus_weight_8bit_validation_max == 511);
    static_assert(expected_blend_8bit_validation_max == 199);

    static_assert(expected_blend_8bit_prev_high == 251);
    static_assert(expected_blend_8bit_cur_high == 4);
    static_assert(expected_blend_8bit_half_point == 1);

    static_assert(expected_weight_16bit_max_from_tables == 4294705156LL);
    static_assert(expected_blend_16bit_max_from_tables == 49997);
    static_assert(expected_blend_16bit_mid == 25499);
    static_assert(expected_weight_16bit_scaled_family == 2255020800LL);
    static_assert(expected_blend_16bit_scaled_family == 26727);

    static_assert(expected_weight_16bit_validation_max == 4294836225LL);
    static_assert(expected_shift_minus_weight_16bit_validation_max == 131071);
    static_assert(expected_blend_16bit_validation_max == 49999);

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto expect_blend = [](
        int bits_per_sample,
        int current_source_sample,
        int previous_filtered_sample,
        int y_response,
        int chroma_response,
        int expected_output
    ) noexcept -> bool {
        int output_sample = -1;

        if (
            cnr3_blend_chroma_sample(
                current_source_sample,
                previous_filtered_sample,
                y_response,
                chroma_response,
                bits_per_sample,
                output_sample
            ) != Cnr3Status::ok
            ) {
            return false;
        }

        return output_sample == expected_output;
    };

    {
        std::int64_t blend_scale = 0;

        if (
            cnr3_blend_scale_for_bit_depth(bits_8, blend_scale) != Cnr3Status::ok ||
            blend_scale != expected_shift_8bit
            ) {
            return fail();
        }

        if (
            cnr3_blend_scale_for_bit_depth(bits_16, blend_scale) != Cnr3Status::ok ||
            blend_scale != expected_shift_16bit
            ) {
            return fail();
        }

        if (
            cnr3_blend_scale_for_bit_depth(7, blend_scale) != Cnr3Status::invalid_argument ||
            blend_scale != 0
            ) {
            return fail();
        }

        if (
            cnr3_blend_scale_for_bit_depth(17, blend_scale) != Cnr3Status::invalid_argument ||
            blend_scale != 0
            ) {
            return fail();
        }
    }

    if (
        cnr3_calculate_combined_blend_weight(254, 254) !=
            expected_weight_8bit_max_from_tables ||
        cnr3_calculate_combined_blend_weight(255, 255) !=
            expected_weight_8bit_validation_max ||
        cnr3_calculate_combined_blend_weight(65535, 65535) !=
            expected_weight_16bit_validation_max
        ) {
        return fail();
    }

    if (
        (expected_shift_8bit - expected_weight_8bit_validation_max) !=
            expected_shift_minus_weight_8bit_validation_max ||
        (expected_shift_16bit - expected_weight_16bit_validation_max) !=
            expected_shift_minus_weight_16bit_validation_max
        ) {
        return fail();
    }

    if (
        !expect_blend(bits_8, 100, 200, 0, 254, 100) ||
        !expect_blend(bits_8, 100, 200, 254, 0, 100)
        ) {
        return fail();
    }

    if (
        !expect_blend(bits_8, 10, 200, 254, 254, expected_blend_8bit_max_cur10_prev200) ||
        !expect_blend(bits_8, 10, 200, 127, 254, expected_blend_8bit_narrow_mid) ||
        !expect_blend(bits_8, 10, 200, 216, 254, expected_blend_8bit_wide_mid)
        ) {
        return fail();
    }

    if (
        !expect_blend(bits_8, 100, 200, 145, 192, expected_blend_8bit_200_20_family)
        ) {
        return fail();
    }

    if (
        !expect_blend(bits_8, 100, 200, 255, 255, expected_blend_8bit_validation_max) ||
        !expect_blend(bits_16, 1000, 50000, 65535, 65535, expected_blend_16bit_validation_max)
        ) {
        return fail();
    }

    if (
        !expect_blend(bits_8, 0, 255, 254, 254, expected_blend_8bit_prev_high) ||
        !expect_blend(bits_8, 255, 0, 254, 254, expected_blend_8bit_cur_high)
        ) {
        return fail();
    }

    if (!expect_blend(bits_8, 0, 2, 128, 128, expected_blend_8bit_half_point)) {
        return fail();
    }

    if (
        !expect_blend(bits_16, 1000, 50000, 65534, 65534, expected_blend_16bit_max_from_tables) ||
        !expect_blend(bits_16, 1000, 50000, 32767, 65534, expected_blend_16bit_mid) ||
        !expect_blend(bits_16, 1000, 50000, 43872, 51400, expected_blend_16bit_scaled_family)
        ) {
        return fail();
    }

    {
        int output_sample = 12345;

        if (
            cnr3_blend_chroma_sample(
                256,
                0,
                0,
                0,
                bits_8,
                output_sample
            ) != Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }

        if (
            cnr3_blend_chroma_sample(
                0,
                -1,
                0,
                0,
                bits_8,
                output_sample
            ) != Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }

        if (
            cnr3_blend_chroma_sample(
                0,
                0,
                -1,
                0,
                bits_8,
                output_sample
            ) != Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }

        if (
            cnr3_blend_chroma_sample(
                0,
                0,
                0,
                256,
                bits_8,
                output_sample
            ) != Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "P.3A weighted chroma blend vector proof scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    source check confirms shift2=depth<<1, shift=1LL<<shift2, shift1=shift>>1"
    );
    cnr3_cache_core_selftest_trace_line(
        "    compatibility claim is bit-exact blend/curve maths plus improved native parameter scaling"
    );
    cnr3_cache_core_selftest_trace_line(
        "    zero-weight proof returns current source chroma"
    );
    cnr3_cache_core_selftest_trace_line(
        "    max-weight proof keeps shift-weight positive at 8-bit and 16-bit bounds"
    );
    cnr3_cache_core_selftest_trace_line(
        "    half-point proof preserves vsCnr2 shift1 round-half-up behaviour"
    );
    cnr3_cache_core_selftest_trace_line(
        "    8-bit vectors prove response multiplication, shift1 rounding, and shift2 depth"
    );
    cnr3_cache_core_selftest_trace_line(
        "    directionality proof distinguishes current source from previous filtered output"
    );
    cnr3_cache_core_selftest_trace_line(
        "    16-bit vectors prove int64 accumulator safety for high-bit-depth chroma blend"
    );
    cnr3_cache_core_selftest_trace_line(
        "    invalid input proof preserves output sentinel without partial publish"
    );
    cnr3_cache_core_selftest_trace_line(
        "    signed-difference/table-lookup and downSampleLuma rules are carried forward to P.4A/P.5A"
    );

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_downsampled_luma_vector_proof() noexcept {
    /*
        P.4A still uses the established selftest runner. It proves the scalar
        downSampleLuma shape and coordinate mapping only; frame traversal and
        VapourSynth frame access remain deferred.
    */
    constexpr int bits_8 = 8;
    constexpr int bits_16 = 16;

    constexpr int expected_sample_8bit_box = 25;
    constexpr int expected_sample_8bit_half_point = 1;
    constexpr int expected_sample_8bit_quarter_point = 0;
    constexpr int expected_sample_16bit_peak = 65535;

    constexpr int expected_420_x0 = 4;
    constexpr int expected_420_x1 = 5;
    constexpr int expected_420_y0 = 2;
    constexpr int expected_420_y1 = 3;
    constexpr int expected_420_sample = 255;

    constexpr int expected_422_x0 = 4;
    constexpr int expected_422_x1 = 5;
    constexpr int expected_422_y0 = 3;
    constexpr int expected_422_y1 = 3;
    constexpr int expected_422_sample = 305;

    constexpr int expected_440_x0 = 4;
    constexpr int expected_440_x1 = 5;
    constexpr int expected_440_y0 = 4;
    constexpr int expected_440_y1 = 5;
    constexpr int expected_440_sample = 455;

    constexpr int expected_444_x0 = 4;
    constexpr int expected_444_x1 = 5;
    constexpr int expected_444_y0 = 3;
    constexpr int expected_444_y1 = 3;
    constexpr int expected_444_sample = 305;

    constexpr int expected_444_right_edge_x0 = 5;
    constexpr int expected_444_right_edge_x1 = 5;
    constexpr int expected_444_right_edge_y0 = 2;
    constexpr int expected_444_right_edge_y1 = 2;
    constexpr int expected_444_right_edge_sample = 205;

    constexpr int expected_420_bottom_right_x0 = 4;
    constexpr int expected_420_bottom_right_x1 = 4;
    constexpr int expected_420_bottom_right_y0 = 4;
    constexpr int expected_420_bottom_right_y1 = 4;
    constexpr int expected_420_bottom_right_sample = 404;

    static_assert(expected_sample_8bit_box == ((10 + 20 + 30 + 40 + 2) >> 2));
    static_assert(expected_sample_8bit_half_point == ((0 + 0 + 0 + 2 + 2) >> 2));
    static_assert(expected_sample_8bit_quarter_point == ((0 + 0 + 0 + 1 + 2) >> 2));
    static_assert(expected_sample_16bit_peak == ((65535 + 65535 + 65535 + 65535 + 2) >> 2));

    static_assert(expected_420_sample == ((204 + 205 + 304 + 305 + 2) >> 2));
    static_assert(expected_422_sample == ((304 + 305 + 304 + 305 + 2) >> 2));
    static_assert(expected_440_sample == ((404 + 405 + 504 + 505 + 2) >> 2));
    static_assert(expected_444_sample == ((304 + 305 + 304 + 305 + 2) >> 2));
    static_assert(expected_444_right_edge_sample == ((205 + 205 + 205 + 205 + 2) >> 2));
    static_assert(expected_420_bottom_right_sample == ((404 + 404 + 404 + 404 + 2) >> 2));

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto sample_value = [](int x, int y) noexcept -> int {
        return y * 100 + x;
    };

    const auto coordinates_match = [](
        const Cnr3DownsampledLumaTapCoordinates& coordinates,
        int x0,
        int x1,
        int y0,
        int y1
    ) noexcept -> bool {
        return coordinates.x0 == x0 &&
            coordinates.x1 == x1 &&
            coordinates.y0 == y0 &&
            coordinates.y1 == y1;
    };

    const auto expect_coordinates = [coordinates_match](
        int chroma_x,
        int chroma_y,
        int luma_width,
        int luma_height,
        int sub_sampling_w,
        int sub_sampling_h,
        int expected_x0,
        int expected_x1,
        int expected_y0,
        int expected_y1
    ) noexcept -> bool {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(
                chroma_x,
                chroma_y,
                luma_width,
                luma_height,
                sub_sampling_w,
                sub_sampling_h,
                coordinates
            ) != Cnr3Status::ok
            ) {
            return false;
        }

        return coordinates_match(
            coordinates,
            expected_x0,
            expected_x1,
            expected_y0,
            expected_y1
        );
    };

    const auto expect_downsample = [](
        int top_left_sample,
        int top_right_sample,
        int bottom_left_sample,
        int bottom_right_sample,
        int bits_per_sample,
        int expected_output
    ) noexcept -> bool {
        int output_sample = -1;

        if (
            cnr3_downsample_luma_sample(
                top_left_sample,
                top_right_sample,
                bottom_left_sample,
                bottom_right_sample,
                bits_per_sample,
                output_sample
            ) != Cnr3Status::ok
            ) {
            return false;
        }

        return output_sample == expected_output;
    };

    const auto expect_coordinate_sample = [expect_downsample, sample_value](
        int bits_per_sample,
        const Cnr3DownsampledLumaTapCoordinates& coordinates,
        int expected_output
    ) noexcept -> bool {
        return expect_downsample(
            sample_value(coordinates.x0, coordinates.y0),
            sample_value(coordinates.x1, coordinates.y0),
            sample_value(coordinates.x0, coordinates.y1),
            sample_value(coordinates.x1, coordinates.y1),
            bits_per_sample,
            expected_output
        );
    };

    if (
        !expect_downsample(10, 20, 30, 40, bits_8, expected_sample_8bit_box) ||
        !expect_downsample(0, 0, 0, 2, bits_8, expected_sample_8bit_half_point) ||
        !expect_downsample(0, 0, 0, 1, bits_8, expected_sample_8bit_quarter_point) ||
        !expect_downsample(65535, 65535, 65535, 65535, bits_16, expected_sample_16bit_peak)
        ) {
        return fail();
    }

    if (
        !expect_coordinates(
            2,
            1,
            8,
            6,
            1,
            1,
            expected_420_x0,
            expected_420_x1,
            expected_420_y0,
            expected_420_y1
        )
        ) {
        return fail();
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(2, 1, 8, 6, 1, 1, coordinates) !=
                Cnr3Status::ok ||
            !coordinates_match(
                coordinates,
                expected_420_x0,
                expected_420_x1,
                expected_420_y0,
                expected_420_y1
            ) ||
            !expect_coordinate_sample(bits_16, coordinates, expected_420_sample)
            ) {
            return fail();
        }
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(2, 3, 8, 6, 1, 0, coordinates) !=
                Cnr3Status::ok ||
            !coordinates_match(
                coordinates,
                expected_422_x0,
                expected_422_x1,
                expected_422_y0,
                expected_422_y1
            ) ||
            !expect_coordinate_sample(bits_16, coordinates, expected_422_sample)
            ) {
            return fail();
        }
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(4, 2, 8, 6, 0, 1, coordinates) !=
                Cnr3Status::ok ||
            !coordinates_match(
                coordinates,
                expected_440_x0,
                expected_440_x1,
                expected_440_y0,
                expected_440_y1
            ) ||
            !expect_coordinate_sample(bits_16, coordinates, expected_440_sample)
            ) {
            return fail();
        }
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(4, 3, 8, 6, 0, 0, coordinates) !=
                Cnr3Status::ok ||
            !coordinates_match(
                coordinates,
                expected_444_x0,
                expected_444_x1,
                expected_444_y0,
                expected_444_y1
            ) ||
            !expect_coordinate_sample(bits_16, coordinates, expected_444_sample)
            ) {
            return fail();
        }
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(5, 2, 6, 4, 0, 0, coordinates) !=
                Cnr3Status::ok ||
            !coordinates_match(
                coordinates,
                expected_444_right_edge_x0,
                expected_444_right_edge_x1,
                expected_444_right_edge_y0,
                expected_444_right_edge_y1
            ) ||
            !expect_coordinate_sample(bits_16, coordinates, expected_444_right_edge_sample)
            ) {
            return fail();
        }
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};

        if (
            cnr3_downsample_luma_tap_coordinates(2, 2, 5, 5, 1, 1, coordinates) !=
                Cnr3Status::ok ||
            !coordinates_match(
                coordinates,
                expected_420_bottom_right_x0,
                expected_420_bottom_right_x1,
                expected_420_bottom_right_y0,
                expected_420_bottom_right_y1
            ) ||
            !expect_coordinate_sample(bits_16, coordinates, expected_420_bottom_right_sample)
            ) {
            return fail();
        }
    }

    {
        Cnr3DownsampledLumaTapCoordinates coordinates{};
        coordinates.x0 = 7;
        coordinates.x1 = 8;
        coordinates.y0 = 9;
        coordinates.y1 = 10;

        if (
            cnr3_downsample_luma_tap_coordinates(-1, 0, 8, 6, 0, 0, coordinates) !=
                Cnr3Status::invalid_argument ||
            !coordinates_match(coordinates, 7, 8, 9, 10)
            ) {
            return fail();
        }

        if (
            cnr3_downsample_luma_tap_coordinates(0, 0, 0, 6, 0, 0, coordinates) !=
                Cnr3Status::invalid_argument ||
            !coordinates_match(coordinates, 7, 8, 9, 10)
            ) {
            return fail();
        }

        if (
            cnr3_downsample_luma_tap_coordinates(0, 0, 8, 6, 2, 0, coordinates) !=
                Cnr3Status::invalid_argument ||
            !coordinates_match(coordinates, 7, 8, 9, 10)
            ) {
            return fail();
        }

        if (
            cnr3_downsample_luma_tap_coordinates(3, 0, 4, 6, 1, 0, coordinates) !=
                Cnr3Status::invalid_argument ||
            !coordinates_match(coordinates, 7, 8, 9, 10)
            ) {
            return fail();
        }
    }

    {
        int output_sample = 12345;

        if (
            cnr3_downsample_luma_sample(0, 0, 0, 0, 7, output_sample) !=
                Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }

        if (
            cnr3_downsample_luma_sample(0, 0, 0, 0, 17, output_sample) !=
                Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }

        if (
            cnr3_downsample_luma_sample(-1, 0, 0, 0, bits_8, output_sample) !=
                Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }

        if (
            cnr3_downsample_luma_sample(256, 0, 0, 0, bits_8, output_sample) !=
                Cnr3Status::invalid_argument ||
            output_sample != 12345
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "P.4A downsampled-luma vector proof scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    source check confirms x0=x<<subw, x1=x0+1, y0=y<<subh, y1=y0+subh"
    );
    cnr3_cache_core_selftest_trace_line(
        "    four-tap proof preserves (a+b+c+d+2)>>2 round-to-nearest behaviour"
    );
    cnr3_cache_core_selftest_trace_line(
        "    4:2:0 proof maps chroma samples to the expected 2x2 luma box"
    );
    cnr3_cache_core_selftest_trace_line(
        "    4:2:2, 4:4:0, and 4:4:4 proofs preserve vsCnr2 degenerate averages"
    );
    cnr3_cache_core_selftest_trace_line(
        "    right/bottom edge proof clamps taps instead of relying on frame padding"
    );
    cnr3_cache_core_selftest_trace_line(
        "    invalid coordinate and sample proofs preserve sentinels without partial publish"
    );
    cnr3_cache_core_selftest_trace_line(
        "    frame traversal, signed differences, table lookup, and scene-change remain deferred"
    );

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_signed_difference_table_lookup_blend_proof() noexcept {
    /*
        P.5A composes the already-proven scalar pixel helpers only. It proves
        signed current-minus-previous differences, P.1A total lookup, P.2A table
        geometry, and P.3A blend integration without walking frame buffers.
    */
    constexpr int bits_8 = 8;
    constexpr int table_offset_8 = 255;
    constexpr int table_size_8 = 511;

    constexpr int bits_16 = 16;
    constexpr int table_offset_16 = 65535;
    constexpr int table_size_16 = 131071;

    constexpr int expected_positive_luma_diff = 5;
    constexpr int expected_negative_chroma_diff = -10;
    constexpr int expected_positive_luma_response = 216;
    constexpr int expected_negative_chroma_response = 254;
    constexpr std::int64_t expected_positive_negative_weight = 54864;
    constexpr int expected_positive_negative_output = 48;

    constexpr int expected_negative_luma_diff = -10;
    constexpr int expected_positive_chroma_diff = 10;
    constexpr int expected_negative_luma_response = 127;
    constexpr int expected_positive_chroma_response = 145;
    constexpr std::int64_t expected_negative_positive_weight = 18415;
    constexpr int expected_negative_positive_output = 57;

    constexpr int expected_extreme_luma_diff = 255;
    constexpr int expected_extreme_chroma_diff = -255;
    constexpr std::int64_t expected_extreme_weight = 65025;
    constexpr int expected_extreme_output = 253;

    constexpr int expected_zero_response_output = 40;

    constexpr int expected_16bit_luma_diff = 7000;
    constexpr int expected_16bit_chroma_diff = -49000;
    constexpr int expected_16bit_luma_response = 43872;
    constexpr int expected_16bit_chroma_response = 51400;
    constexpr std::int64_t expected_16bit_weight = 2255020800LL;
    constexpr int expected_16bit_output = 26727;

    static_assert(table_size_8 == (table_offset_8 * 2) + 1);
    static_assert(table_size_16 == (table_offset_16 * 2) + 1);
    static_assert(expected_positive_negative_weight == 216LL * 254LL);
    static_assert(expected_negative_positive_weight == 127LL * 145LL);
    static_assert(expected_extreme_weight == 255LL * 255LL);
    static_assert(expected_16bit_weight == 43872LL * 51400LL);

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto result_matches = [](
        const Cnr3ChromaBlendSampleResult& result,
        int luma_signed_diff,
        int chroma_signed_diff,
        int y_response,
        int chroma_response,
        int output_sample
    ) noexcept -> bool {
        return result.luma_signed_diff == luma_signed_diff &&
            result.chroma_signed_diff == chroma_signed_diff &&
            result.y_response == y_response &&
            result.chroma_response == chroma_response &&
            result.output_sample == output_sample;
    };

    const auto sentinel_matches = [result_matches](
        const Cnr3ChromaBlendSampleResult& result
    ) noexcept -> bool {
        return result_matches(result, 1, 2, 3, 4, 5);
    };

    const auto set_table_value = [](
        std::vector<int>& table,
        int table_offset,
        int signed_diff,
        int value
    ) noexcept {
        table[static_cast<std::size_t>(table_offset + signed_diff)] = value;
    };

    std::vector<int> y_table_8(static_cast<std::size_t>(table_size_8), 0);
    std::vector<int> chroma_table_8(static_cast<std::size_t>(table_size_8), 0);

    set_table_value(
        y_table_8,
        table_offset_8,
        expected_positive_luma_diff,
        expected_positive_luma_response
    );
    set_table_value(
        chroma_table_8,
        table_offset_8,
        expected_negative_chroma_diff,
        expected_negative_chroma_response
    );
    set_table_value(
        y_table_8,
        table_offset_8,
        expected_negative_luma_diff,
        expected_negative_luma_response
    );
    set_table_value(
        chroma_table_8,
        table_offset_8,
        expected_positive_chroma_diff,
        expected_positive_chroma_response
    );
    set_table_value(y_table_8, table_offset_8, expected_extreme_luma_diff, 255);
    set_table_value(chroma_table_8, table_offset_8, expected_extreme_chroma_diff, 255);

    {
        Cnr3ChromaBlendSampleResult result{};

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                105,
                100,
                40,
                50,
                y_table_8,
                chroma_table_8,
                table_offset_8,
                bits_8,
                result
            ) != Cnr3Status::ok ||
            !result_matches(
                result,
                expected_positive_luma_diff,
                expected_negative_chroma_diff,
                expected_positive_luma_response,
                expected_negative_chroma_response,
                expected_positive_negative_output
            ) ||
            cnr3_calculate_combined_blend_weight(result.y_response, result.chroma_response) !=
                expected_positive_negative_weight
            ) {
            return fail();
        }
    }

    {
        Cnr3ChromaBlendSampleResult result{};

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                90,
                100,
                60,
                50,
                y_table_8,
                chroma_table_8,
                table_offset_8,
                bits_8,
                result
            ) != Cnr3Status::ok ||
            !result_matches(
                result,
                expected_negative_luma_diff,
                expected_positive_chroma_diff,
                expected_negative_luma_response,
                expected_positive_chroma_response,
                expected_negative_positive_output
            ) ||
            cnr3_calculate_combined_blend_weight(result.y_response, result.chroma_response) !=
                expected_negative_positive_weight
            ) {
            return fail();
        }
    }

    {
        Cnr3ChromaBlendSampleResult result{};

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                255,
                0,
                0,
                255,
                y_table_8,
                chroma_table_8,
                table_offset_8,
                bits_8,
                result
            ) != Cnr3Status::ok ||
            !result_matches(
                result,
                expected_extreme_luma_diff,
                expected_extreme_chroma_diff,
                255,
                255,
                expected_extreme_output
            ) ||
            cnr3_calculate_combined_blend_weight(result.y_response, result.chroma_response) !=
                expected_extreme_weight
            ) {
            return fail();
        }
    }

    {
        Cnr3ChromaBlendSampleResult result{};

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                120,
                120,
                40,
                50,
                y_table_8,
                chroma_table_8,
                table_offset_8,
                bits_8,
                result
            ) != Cnr3Status::ok ||
            !result_matches(
                result,
                0,
                expected_negative_chroma_diff,
                0,
                expected_negative_chroma_response,
                expected_zero_response_output
            )
            ) {
            return fail();
        }
    }

    {
        std::vector<int> y_table_16(static_cast<std::size_t>(table_size_16), 0);
        std::vector<int> chroma_table_16(static_cast<std::size_t>(table_size_16), 0);
        Cnr3ChromaBlendSampleResult result{};

        set_table_value(
            y_table_16,
            table_offset_16,
            expected_16bit_luma_diff,
            expected_16bit_luma_response
        );
        set_table_value(
            chroma_table_16,
            table_offset_16,
            expected_16bit_chroma_diff,
            expected_16bit_chroma_response
        );

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                40000,
                33000,
                1000,
                50000,
                y_table_16,
                chroma_table_16,
                table_offset_16,
                bits_16,
                result
            ) != Cnr3Status::ok ||
            !result_matches(
                result,
                expected_16bit_luma_diff,
                expected_16bit_chroma_diff,
                expected_16bit_luma_response,
                expected_16bit_chroma_response,
                expected_16bit_output
            ) ||
            cnr3_calculate_combined_blend_weight(result.y_response, result.chroma_response) !=
                expected_16bit_weight
            ) {
            return fail();
        }
    }

    {
        Cnr3ChromaBlendSampleResult result{};
        result.luma_signed_diff = 1;
        result.chroma_signed_diff = 2;
        result.y_response = 3;
        result.chroma_response = 4;
        result.output_sample = 5;

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                105,
                100,
                40,
                50,
                y_table_8,
                chroma_table_8,
                table_offset_8 + 1,
                bits_8,
                result
            ) != Cnr3Status::invalid_argument ||
            !sentinel_matches(result)
            ) {
            return fail();
        }

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                105,
                100,
                40,
                50,
                std::vector<int>(static_cast<std::size_t>(table_size_8 - 1), 0),
                chroma_table_8,
                table_offset_8,
                bits_8,
                result
            ) != Cnr3Status::invalid_argument ||
            !sentinel_matches(result)
            ) {
            return fail();
        }

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                256,
                100,
                40,
                50,
                y_table_8,
                chroma_table_8,
                table_offset_8,
                bits_8,
                result
            ) != Cnr3Status::invalid_argument ||
            !sentinel_matches(result)
            ) {
            return fail();
        }

        if (
            cnr3_blend_chroma_sample_from_response_tables(
                105,
                100,
                40,
                50,
                y_table_8,
                chroma_table_8,
                table_offset_8,
                7,
                result
            ) != Cnr3Status::invalid_argument ||
            !sentinel_matches(result)
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "P.5A signed-difference/table-lookup blend proof scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    signed current-minus-previous luma/chroma diffs feed P.1A total lookup"
    );
    cnr3_cache_core_selftest_trace_line(
        "    P.2A geometry proof requires table_offset=sample_peak and full signed-diff table size"
    );
    cnr3_cache_core_selftest_trace_line(
        "    positive luma plus negative chroma diff proves no unsigned wrap on table path"
    );
    cnr3_cache_core_selftest_trace_line(
        "    negative luma plus positive chroma diff proves signed lookup before blending"
    );
    cnr3_cache_core_selftest_trace_line(
        "    zero-response proof returns current source chroma after table lookup"
    );
    cnr3_cache_core_selftest_trace_line(
        "    16-bit vector proves signed lookup and P.3A int64 blend remain composed"
    );
    cnr3_cache_core_selftest_trace_line(
        "    invalid geometry/sample proofs preserve result sentinel without partial publish"
    );
    cnr3_cache_core_selftest_trace_line(
        "    frame traversal, source-frame access, and scene-change remain deferred to P.6A"
    );

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_chroma_plane_traversal_vector_proof() noexcept {
    /*
        P.6A walks matching scalar sample buffers for one chroma plane. It proves
        traversal and publication discipline only; source luma downsample traversal,
        VapourSynth frame access, previous-output acquisition, and scene-change are
        still deferred.
    */
    constexpr int bits_8 = 8;
    constexpr int table_offset_8 = 255;
    constexpr int table_size_8 = 511;
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr int stride = 5;

    constexpr int expected_first_output = 48;
    constexpr int expected_second_output = 57;
    constexpr int expected_zero_response_output = 40;
    constexpr int expected_extreme_output = 253;
    constexpr int expected_half_point_output = 1;
    constexpr int expected_current_high_output = 2;
    constexpr int expected_samples_processed = width * height;

    static_assert(table_size_8 == (table_offset_8 * 2) + 1);
    static_assert(expected_samples_processed == 6);

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto set_table_value = [](
        std::vector<int>& table,
        int table_offset,
        int signed_diff,
        int value
    ) noexcept {
        table[static_cast<std::size_t>(table_offset + signed_diff)] = value;
    };

    const auto all_output_padding_is = [](
        const std::vector<int>& output,
        int sentinel
    ) noexcept -> bool {
        return output[3] == sentinel &&
            output[4] == sentinel &&
            output[8] == sentinel &&
            output[9] == sentinel;
    };

    const auto summary_matches = [](
        const Cnr3ChromaPlaneProcessSummary& summary,
        int expected_width,
        int expected_height,
        int expected_count,
        int expected_first,
        int expected_last
    ) noexcept -> bool {
        return summary.width == expected_width &&
            summary.height == expected_height &&
            summary.samples_processed == expected_count &&
            summary.first_output_sample == expected_first &&
            summary.last_output_sample == expected_last;
    };

    std::vector<int> y_table(static_cast<std::size_t>(table_size_8), 0);
    std::vector<int> chroma_table(static_cast<std::size_t>(table_size_8), 0);

    set_table_value(y_table, table_offset_8, 5, 216);
    set_table_value(chroma_table, table_offset_8, -10, 254);
    set_table_value(y_table, table_offset_8, -10, 127);
    set_table_value(chroma_table, table_offset_8, 10, 145);
    set_table_value(y_table, table_offset_8, 255, 255);
    set_table_value(chroma_table, table_offset_8, -255, 255);
    set_table_value(y_table, table_offset_8, 0, 128);
    set_table_value(chroma_table, table_offset_8, -2, 128);
    set_table_value(y_table, table_offset_8, 1, 255);
    set_table_value(chroma_table, table_offset_8, 255, 255);

    const std::vector<int> current_downsampled_luma = {
        105, 90, 120, -1, -1,
        255, 0, 1, -1, -1
    };
    const std::vector<int> previous_downsampled_luma = {
        100, 100, 100, -1, -1,
        0, 0, 0, -1, -1
    };
    const std::vector<int> current_source_chroma = {
        40, 60, 40, -1, -1,
        0, 0, 255, -1, -1
    };
    const std::vector<int> previous_filtered_chroma = {
        50, 50, 200, -1, -1,
        255, 2, 0, -1, -1
    };

    const Cnr3ConstPlaneBufferView current_downsampled_luma_plane{
        current_downsampled_luma.data(), width, height, stride
    };
    const Cnr3ConstPlaneBufferView previous_downsampled_luma_plane{
        previous_downsampled_luma.data(), width, height, stride
    };
    const Cnr3ConstPlaneBufferView current_source_chroma_plane{
        current_source_chroma.data(), width, height, stride
    };
    const Cnr3ConstPlaneBufferView previous_filtered_chroma_plane{
        previous_filtered_chroma.data(), width, height, stride
    };

    {
        std::vector<int> output_chroma(static_cast<std::size_t>(stride * height), -700);
        Cnr3MutablePlaneBufferView output_chroma_plane{
            output_chroma.data(), width, height, stride
        };
        Cnr3ChromaPlaneProcessSummary summary{};

        if (
            cnr3_process_chroma_plane_from_downsampled_luma(
                current_downsampled_luma_plane,
                previous_downsampled_luma_plane,
                current_source_chroma_plane,
                previous_filtered_chroma_plane,
                y_table,
                chroma_table,
                table_offset_8,
                bits_8,
                output_chroma_plane,
                summary
            ) != Cnr3Status::ok ||
            output_chroma[0] != expected_first_output ||
            output_chroma[1] != expected_second_output ||
            output_chroma[2] != expected_zero_response_output ||
            output_chroma[5] != expected_extreme_output ||
            output_chroma[6] != expected_half_point_output ||
            output_chroma[7] != expected_current_high_output ||
            !all_output_padding_is(output_chroma, -700) ||
            !summary_matches(
                summary,
                width,
                height,
                expected_samples_processed,
                expected_first_output,
                expected_current_high_output
            )
            ) {
            return fail();
        }
    }

    {
        std::vector<int> bad_current_source_chroma = current_source_chroma;
        bad_current_source_chroma[7] = 300;

        const Cnr3ConstPlaneBufferView bad_current_source_chroma_plane{
            bad_current_source_chroma.data(), width, height, stride
        };

        std::vector<int> output_chroma(static_cast<std::size_t>(stride * height), -900);
        Cnr3MutablePlaneBufferView output_chroma_plane{
            output_chroma.data(), width, height, stride
        };
        Cnr3ChromaPlaneProcessSummary summary{};
        summary.width = 9;
        summary.height = 8;
        summary.samples_processed = 7;
        summary.first_output_sample = 6;
        summary.last_output_sample = 5;

        if (
            cnr3_process_chroma_plane_from_downsampled_luma(
                current_downsampled_luma_plane,
                previous_downsampled_luma_plane,
                bad_current_source_chroma_plane,
                previous_filtered_chroma_plane,
                y_table,
                chroma_table,
                table_offset_8,
                bits_8,
                output_chroma_plane,
                summary
            ) != Cnr3Status::invalid_argument ||
            output_chroma[0] != -900 ||
            output_chroma[1] != -900 ||
            output_chroma[2] != -900 ||
            output_chroma[5] != -900 ||
            output_chroma[6] != -900 ||
            output_chroma[7] != -900 ||
            !all_output_padding_is(output_chroma, -900) ||
            !summary_matches(summary, 9, 8, 7, 6, 5)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> output_chroma(static_cast<std::size_t>(stride * height), -910);
        Cnr3MutablePlaneBufferView output_chroma_plane{
            output_chroma.data(), width, height, stride
        };
        Cnr3ChromaPlaneProcessSummary summary{};
        summary.width = 1;
        summary.height = 2;
        summary.samples_processed = 3;
        summary.first_output_sample = 4;
        summary.last_output_sample = 5;

        Cnr3ConstPlaneBufferView mismatched_luma_plane = current_downsampled_luma_plane;
        mismatched_luma_plane.width = width - 1;

        if (
            cnr3_process_chroma_plane_from_downsampled_luma(
                mismatched_luma_plane,
                previous_downsampled_luma_plane,
                current_source_chroma_plane,
                previous_filtered_chroma_plane,
                y_table,
                chroma_table,
                table_offset_8,
                bits_8,
                output_chroma_plane,
                summary
            ) != Cnr3Status::invalid_argument ||
            output_chroma[0] != -910 ||
            output_chroma[7] != -910 ||
            !summary_matches(summary, 1, 2, 3, 4, 5)
            ) {
            return fail();
        }

        Cnr3ConstPlaneBufferView null_luma_plane = current_downsampled_luma_plane;
        null_luma_plane.samples = nullptr;

        if (
            cnr3_process_chroma_plane_from_downsampled_luma(
                null_luma_plane,
                previous_downsampled_luma_plane,
                current_source_chroma_plane,
                previous_filtered_chroma_plane,
                y_table,
                chroma_table,
                table_offset_8,
                bits_8,
                output_chroma_plane,
                summary
            ) != Cnr3Status::invalid_argument ||
            output_chroma[0] != -910 ||
            output_chroma[7] != -910 ||
            !summary_matches(summary, 1, 2, 3, 4, 5)
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "P.6A chroma-plane traversal vector proof scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    matching scalar planes are traversed row-major with explicit strides"
    );
    cnr3_cache_core_selftest_trace_line(
        "    each chroma sample composes P.5A signed lookup and P.3A int64 blend"
    );
    cnr3_cache_core_selftest_trace_line(
        "    positive/negative/zero/extreme/half-point paths are proven across one plane"
    );
    cnr3_cache_core_selftest_trace_line(
        "    row padding is preserved and not treated as active chroma width"
    );
    cnr3_cache_core_selftest_trace_line(
        "    invalid late-sample and geometry proofs publish no partial output"
    );
    cnr3_cache_core_selftest_trace_line(
        "    source luma downsample traversal, frame access, and scene-change remain deferred"
    );

    return Cnr3Status::ok;
}



Cnr3Status cnr3_cache_core_selftest_source_luma_downsample_plane_traversal_proof() noexcept {
    /*
        P.7A traverses scalar source-luma buffers and produces downsampled-luma
        scalar buffers for the P.6A chroma-plane traversal. It still does not
        access VapourSynth frames or real byte-strided frame memory.
    */
    constexpr int bits_10 = 10;
    constexpr int source_width = 5;
    constexpr int source_height = 5;
    constexpr int source_stride = 7;
    constexpr int output_width_420 = 3;
    constexpr int output_height_420 = 3;
    constexpr int output_stride_420 = 5;

    constexpr int expected_420_first = 51;
    constexpr int expected_420_mid = 253;
    constexpr int expected_420_right_edge = 54;
    constexpr int expected_420_bottom_edge = 401;
    constexpr int expected_420_last = 404;
    constexpr int expected_420_count = output_width_420 * output_height_420;

    static_assert(expected_420_count == 9);
    static_assert(expected_420_first == ((0 + 1 + 100 + 101 + 2) >> 2));
    static_assert(expected_420_mid == ((202 + 203 + 302 + 303 + 2) >> 2));
    static_assert(expected_420_right_edge == ((4 + 4 + 104 + 104 + 2) >> 2));
    static_assert(expected_420_bottom_edge == ((400 + 401 + 400 + 401 + 2) >> 2));
    static_assert(expected_420_last == ((404 + 404 + 404 + 404 + 2) >> 2));

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto make_source_plane = [](
        int width,
        int height,
        int stride
    ) noexcept -> std::vector<int> {
        std::vector<int> samples(static_cast<std::size_t>(stride * height), -1);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                samples[static_cast<std::size_t>((y * stride) + x)] = (y * 100) + x;
            }
        }

        return samples;
    };

    const auto output_padding_is = [](
        const std::vector<int>& output,
        int width,
        int height,
        int stride,
        int sentinel
    ) noexcept -> bool {
        for (int y = 0; y < height; ++y) {
            for (int x = width; x < stride; ++x) {
                if (output[static_cast<std::size_t>((y * stride) + x)] != sentinel) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto summary_matches = [](
        const Cnr3DownsampledLumaPlaneProcessSummary& summary,
        int expected_source_width,
        int expected_source_height,
        int expected_output_width,
        int expected_output_height,
        int expected_count,
        int expected_first,
        int expected_last
    ) noexcept -> bool {
        return summary.source_width == expected_source_width &&
            summary.source_height == expected_source_height &&
            summary.output_width == expected_output_width &&
            summary.output_height == expected_output_height &&
            summary.samples_processed == expected_count &&
            summary.first_output_sample == expected_first &&
            summary.last_output_sample == expected_last;
    };

    {
        const std::vector<int> source_luma = make_source_plane(source_width, source_height, source_stride);
        const Cnr3ConstPlaneBufferView source_luma_plane{source_luma.data(), source_width, source_height, source_stride};
        std::vector<int> output_luma(static_cast<std::size_t>(output_stride_420 * output_height_420), -700);
        Cnr3MutablePlaneBufferView output_luma_plane{output_luma.data(), output_width_420, output_height_420, output_stride_420};
        Cnr3DownsampledLumaPlaneProcessSummary summary{};

        if (
            cnr3_downsample_luma_plane_to_chroma_grid(source_luma_plane, 1, 1, bits_10, output_luma_plane, summary) != Cnr3Status::ok ||
            output_luma[0] != expected_420_first ||
            output_luma[1] != 53 ||
            output_luma[2] != expected_420_right_edge ||
            output_luma[5] != 251 ||
            output_luma[6] != expected_420_mid ||
            output_luma[7] != 254 ||
            output_luma[10] != expected_420_bottom_edge ||
            output_luma[11] != 403 ||
            output_luma[12] != expected_420_last ||
            !output_padding_is(output_luma, output_width_420, output_height_420, output_stride_420, -700) ||
            !summary_matches(summary, source_width, source_height, output_width_420, output_height_420, expected_420_count, expected_420_first, expected_420_last)
            ) {
            return fail();
        }
    }

    {
        constexpr int width_422 = 5;
        constexpr int height_422 = 2;
        constexpr int stride_422 = 6;
        constexpr int output_width_422 = 3;
        constexpr int output_height_422 = 2;
        constexpr int output_stride_422 = 4;
        const std::vector<int> source_luma = make_source_plane(width_422, height_422, stride_422);
        const Cnr3ConstPlaneBufferView source_luma_plane{source_luma.data(), width_422, height_422, stride_422};
        std::vector<int> output_luma(static_cast<std::size_t>(output_stride_422 * output_height_422), -710);
        Cnr3MutablePlaneBufferView output_luma_plane{output_luma.data(), output_width_422, output_height_422, output_stride_422};
        Cnr3DownsampledLumaPlaneProcessSummary summary{};

        if (
            cnr3_downsample_luma_plane_to_chroma_grid(source_luma_plane, 1, 0, bits_10, output_luma_plane, summary) != Cnr3Status::ok ||
            output_luma[0] != 1 || output_luma[1] != 3 || output_luma[2] != 4 ||
            output_luma[4] != 101 || output_luma[5] != 103 || output_luma[6] != 104 ||
            !output_padding_is(output_luma, output_width_422, output_height_422, output_stride_422, -710) ||
            !summary_matches(summary, width_422, height_422, 3, 2, 6, 1, 104)
            ) {
            return fail();
        }
    }

    {
        constexpr int width_440 = 4;
        constexpr int height_440 = 5;
        constexpr int stride_440 = 6;
        constexpr int output_width_440 = 4;
        constexpr int output_height_440 = 3;
        constexpr int output_stride_440 = 5;
        const std::vector<int> source_luma = make_source_plane(width_440, height_440, stride_440);
        const Cnr3ConstPlaneBufferView source_luma_plane{source_luma.data(), width_440, height_440, stride_440};
        std::vector<int> output_luma(static_cast<std::size_t>(output_stride_440 * output_height_440), -720);
        Cnr3MutablePlaneBufferView output_luma_plane{output_luma.data(), output_width_440, output_height_440, output_stride_440};
        Cnr3DownsampledLumaPlaneProcessSummary summary{};

        if (
            cnr3_downsample_luma_plane_to_chroma_grid(source_luma_plane, 0, 1, bits_10, output_luma_plane, summary) != Cnr3Status::ok ||
            output_luma[0] != 51 || output_luma[3] != 53 ||
            output_luma[5] != 251 || output_luma[8] != 253 ||
            output_luma[10] != 401 || output_luma[13] != 403 ||
            !output_padding_is(output_luma, output_width_440, output_height_440, output_stride_440, -720) ||
            !summary_matches(summary, width_440, height_440, 4, 3, 12, 51, 403)
            ) {
            return fail();
        }
    }

    {
        constexpr int width_444 = 4;
        constexpr int height_444 = 2;
        constexpr int stride_444 = 6;
        constexpr int output_width_444 = 4;
        constexpr int output_height_444 = 2;
        constexpr int output_stride_444 = 5;
        const std::vector<int> source_luma = make_source_plane(width_444, height_444, stride_444);
        const Cnr3ConstPlaneBufferView source_luma_plane{source_luma.data(), width_444, height_444, stride_444};
        std::vector<int> output_luma(static_cast<std::size_t>(output_stride_444 * output_height_444), -730);
        Cnr3MutablePlaneBufferView output_luma_plane{output_luma.data(), output_width_444, output_height_444, output_stride_444};
        Cnr3DownsampledLumaPlaneProcessSummary summary{};

        if (
            cnr3_downsample_luma_plane_to_chroma_grid(source_luma_plane, 0, 0, bits_10, output_luma_plane, summary) != Cnr3Status::ok ||
            output_luma[0] != 1 || output_luma[1] != 2 || output_luma[3] != 3 ||
            output_luma[5] != 101 || output_luma[8] != 103 ||
            !output_padding_is(output_luma, output_width_444, output_height_444, output_stride_444, -730) ||
            !summary_matches(summary, width_444, height_444, 4, 2, 8, 1, 103)
            ) {
            return fail();
        }
    }

    {
        std::vector<int> source_luma = make_source_plane(source_width, source_height, source_stride);
        source_luma[static_cast<std::size_t>((4 * source_stride) + 4)] = 2000;
        const Cnr3ConstPlaneBufferView source_luma_plane{source_luma.data(), source_width, source_height, source_stride};
        std::vector<int> output_luma(static_cast<std::size_t>(output_stride_420 * output_height_420), -800);
        Cnr3MutablePlaneBufferView output_luma_plane{output_luma.data(), output_width_420, output_height_420, output_stride_420};
        Cnr3DownsampledLumaPlaneProcessSummary summary{};
        summary.source_width = 1;
        summary.source_height = 2;
        summary.output_width = 3;
        summary.output_height = 4;
        summary.samples_processed = 5;
        summary.first_output_sample = 6;
        summary.last_output_sample = 7;

        if (
            cnr3_downsample_luma_plane_to_chroma_grid(source_luma_plane, 1, 1, bits_10, output_luma_plane, summary) != Cnr3Status::invalid_argument ||
            output_luma[0] != -800 || output_luma[1] != -800 || output_luma[2] != -800 ||
            output_luma[10] != -800 || output_luma[11] != -800 || output_luma[12] != -800 ||
            !output_padding_is(output_luma, output_width_420, output_height_420, output_stride_420, -800) ||
            !summary_matches(summary, 1, 2, 3, 4, 5, 6, 7)
            ) {
            return fail();
        }
    }

    {
        const std::vector<int> source_luma = make_source_plane(source_width, source_height, source_stride);
        const Cnr3ConstPlaneBufferView source_luma_plane{source_luma.data(), source_width, source_height, source_stride};
        std::vector<int> output_luma(static_cast<std::size_t>(output_stride_420 * output_height_420), -810);
        Cnr3MutablePlaneBufferView bad_output_luma_plane{output_luma.data(), output_width_420 - 1, output_height_420, output_stride_420};
        Cnr3DownsampledLumaPlaneProcessSummary summary{};
        summary.source_width = 9;
        summary.source_height = 8;
        summary.output_width = 7;
        summary.output_height = 6;
        summary.samples_processed = 5;
        summary.first_output_sample = 4;
        summary.last_output_sample = 3;

        if (
            cnr3_downsample_luma_plane_to_chroma_grid(source_luma_plane, 1, 1, bits_10, bad_output_luma_plane, summary) != Cnr3Status::invalid_argument ||
            output_luma[0] != -810 || output_luma[12] != -810 ||
            !summary_matches(summary, 9, 8, 7, 6, 5, 4, 3)
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line("P.7A source-luma downsample plane traversal proof scenario");
    cnr3_cache_core_selftest_trace_line("    scalar source-luma buffers are traversed with explicit strides");
    cnr3_cache_core_selftest_trace_line("    expected chroma-grid dimensions are derived from luma size and subsampling");
    cnr3_cache_core_selftest_trace_line("    4:2:0, 4:2:2, 4:4:0, and 4:4:4 traversal shapes are proven");
    cnr3_cache_core_selftest_trace_line("    right/bottom edge taps are clamped during full-plane traversal");
    cnr3_cache_core_selftest_trace_line("    output padding is preserved and invalid late-sample failure publishes no partial plane");
    cnr3_cache_core_selftest_trace_line("    real frame-memory access, byte-stride reinterpretation, and scene-change remain deferred");

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_native_byte_plane_access_vector_proof() noexcept {
    /*
        P.8A proves native byte-stride access over synthetic byte buffers only.
        It does not call VapourSynth frame APIs or make getFrame lifecycle
        decisions. Multi-byte column offsets must be x * storage_bytes.
    */
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr int scalar_stride = 4;
    constexpr int stride_8bit = 5;
    constexpr int stride_10bit = 8;
    constexpr int bits_8 = 8;
    constexpr int bits_10 = 10;

    constexpr int expected_10bit_first = 0x0123;
    constexpr int expected_10bit_second = 0x02AB;
    constexpr int expected_10bit_third = 0x03FF;
    constexpr int expected_10bit_count = width * height;

    static_assert(expected_10bit_first == 291);
    static_assert(expected_10bit_second == 683);
    static_assert(expected_10bit_third == 1023);
    static_assert(expected_10bit_count == 6);

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto scalar_padding_is = [](
        const std::vector<int>& samples,
        int active_width,
        int active_height,
        int stride,
        int sentinel
    ) noexcept -> bool {
        for (int y = 0; y < active_height; ++y) {
            for (int x = active_width; x < stride; ++x) {
                if (samples[static_cast<std::size_t>((y * stride) + x)] != sentinel) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto native_padding_is = [](
        const std::vector<std::uint8_t>& bytes,
        int active_width,
        int active_height,
        int stride_bytes,
        int storage_bytes,
        std::uint8_t sentinel
    ) noexcept -> bool {
        const int active_row_bytes = active_width * storage_bytes;

        for (int y = 0; y < active_height; ++y) {
            for (int byte_index = active_row_bytes; byte_index < stride_bytes; ++byte_index) {
                if (
                    bytes[
                        static_cast<std::size_t>((y * stride_bytes) + byte_index)
                    ] != sentinel
                    ) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto put_native_u16 = [](
        std::vector<std::uint8_t>& bytes,
        int stride_bytes,
        int x,
        int y,
        std::uint16_t value
    ) noexcept {
        const std::size_t offset =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_bytes)) +
            (static_cast<std::size_t>(x) * 2U);
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };

    const auto get_native_u16 = [](
        const std::vector<std::uint8_t>& bytes,
        int stride_bytes,
        int x,
        int y
    ) noexcept -> int {
        const std::size_t offset =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_bytes)) +
            (static_cast<std::size_t>(x) * 2U);
        std::uint16_t value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return static_cast<int>(value);
    };

    {
        int storage_bytes = -1;

        if (
            cnr3_native_storage_bytes_for_bit_depth(8, storage_bytes) != Cnr3Status::ok ||
            storage_bytes != 1 ||
            cnr3_native_storage_bytes_for_bit_depth(9, storage_bytes) != Cnr3Status::ok ||
            storage_bytes != 2 ||
            cnr3_native_storage_bytes_for_bit_depth(10, storage_bytes) != Cnr3Status::ok ||
            storage_bytes != 2 ||
            cnr3_native_storage_bytes_for_bit_depth(16, storage_bytes) != Cnr3Status::ok ||
            storage_bytes != 2
            ) {
            return fail();
        }

        storage_bytes = 1234;

        if (
            cnr3_native_storage_bytes_for_bit_depth(7, storage_bytes) != Cnr3Status::invalid_argument ||
            storage_bytes != 0 ||
            cnr3_native_storage_bytes_for_bit_depth(17, storage_bytes) != Cnr3Status::invalid_argument ||
            storage_bytes != 0
            ) {
            return fail();
        }
    }

    {
        const std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_8bit * height), 0U);
        const Cnr3ConstNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_8bit,
            bits_8
        };
        int sample = 777;

        if (
            cnr3_load_native_plane_sample(native_plane, -1, 0, sample) != Cnr3Status::invalid_argument ||
            sample != 777
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_8bit * height), 0xEEU);
        native_bytes[0] = 10;
        native_bytes[1] = 20;
        native_bytes[2] = 30;
        native_bytes[5] = 110;
        native_bytes[6] = 120;
        native_bytes[7] = 130;

        const Cnr3ConstNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_8bit,
            bits_8
        };
        std::vector<int> scalar(static_cast<std::size_t>(scalar_stride * height), -100);
        Cnr3MutablePlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };

        if (
            cnr3_copy_native_plane_to_scalar_buffer(native_plane, scalar_plane) != Cnr3Status::ok ||
            scalar[0] != 10 ||
            scalar[1] != 20 ||
            scalar[2] != 30 ||
            scalar[4] != 110 ||
            scalar[5] != 120 ||
            scalar[6] != 130 ||
            !scalar_padding_is(scalar, width, height, scalar_stride, -100)
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_10bit * height), 0xEEU);
        put_native_u16(native_bytes, stride_10bit, 0, 0, static_cast<std::uint16_t>(expected_10bit_first));
        put_native_u16(native_bytes, stride_10bit, 1, 0, static_cast<std::uint16_t>(expected_10bit_second));
        put_native_u16(native_bytes, stride_10bit, 2, 0, static_cast<std::uint16_t>(expected_10bit_third));
        put_native_u16(native_bytes, stride_10bit, 0, 1, 100);
        put_native_u16(native_bytes, stride_10bit, 1, 1, 512);
        put_native_u16(native_bytes, stride_10bit, 2, 1, 900);

        const Cnr3ConstNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_10bit,
            bits_10
        };
        std::vector<int> scalar(static_cast<std::size_t>(scalar_stride * height), -110);
        Cnr3MutablePlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };

        if (
            cnr3_copy_native_plane_to_scalar_buffer(native_plane, scalar_plane) != Cnr3Status::ok ||
            scalar[0] != expected_10bit_first ||
            scalar[1] != expected_10bit_second ||
            scalar[2] != expected_10bit_third ||
            scalar[4] != 100 ||
            scalar[5] != 512 ||
            scalar[6] != 900 ||
            !scalar_padding_is(scalar, width, height, scalar_stride, -110)
            ) {
            return fail();
        }
    }

    {
        const std::vector<int> scalar{
            10, 20, 30, -1,
            110, 120, 130, -1
        };
        const Cnr3ConstPlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_8bit * height), 0xDDU);
        Cnr3MutableNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_8bit,
            bits_8
        };

        if (
            cnr3_copy_scalar_buffer_to_native_plane(scalar_plane, native_plane) != Cnr3Status::ok ||
            native_bytes[0] != 10 ||
            native_bytes[1] != 20 ||
            native_bytes[2] != 30 ||
            native_bytes[5] != 110 ||
            native_bytes[6] != 120 ||
            native_bytes[7] != 130 ||
            !native_padding_is(native_bytes, width, height, stride_8bit, 1, 0xDDU)
            ) {
            return fail();
        }
    }

    {
        const std::vector<int> scalar{
            expected_10bit_first, expected_10bit_second, expected_10bit_third, -1,
            1, 258, 515, -1
        };
        const Cnr3ConstPlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_10bit * height), 0xCCU);
        Cnr3MutableNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_10bit,
            bits_10
        };

        if (
            cnr3_copy_scalar_buffer_to_native_plane(scalar_plane, native_plane) != Cnr3Status::ok ||
            get_native_u16(native_bytes, stride_10bit, 0, 0) != expected_10bit_first ||
            get_native_u16(native_bytes, stride_10bit, 1, 0) != expected_10bit_second ||
            get_native_u16(native_bytes, stride_10bit, 2, 0) != expected_10bit_third ||
            get_native_u16(native_bytes, stride_10bit, 0, 1) != 1 ||
            get_native_u16(native_bytes, stride_10bit, 1, 1) != 258 ||
            get_native_u16(native_bytes, stride_10bit, 2, 1) != 515 ||
            !native_padding_is(native_bytes, width, height, stride_10bit, 2, 0xCCU)
            ) {
            return fail();
        }

        int sample = -1;
        const Cnr3ConstNativePlaneByteView round_trip_plane{
            native_bytes.data(),
            width,
            height,
            stride_10bit,
            bits_10
        };

        if (
            cnr3_load_native_plane_sample(round_trip_plane, 1, 0, sample) != Cnr3Status::ok ||
            sample != expected_10bit_second
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_10bit * height), 0U);
        Cnr3MutableNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_10bit,
            bits_10
        };

        if (cnr3_store_native_plane_sample(native_plane, 1, 0, 321) != Cnr3Status::ok) {
            return fail();
        }

        if (
            cnr3_store_native_plane_sample(native_plane, 1, 0, 1024) != Cnr3Status::invalid_argument ||
            get_native_u16(native_bytes, stride_10bit, 1, 0) != 321
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_10bit * height), 0xEEU);
        put_native_u16(native_bytes, stride_10bit, 0, 0, 1);
        put_native_u16(native_bytes, stride_10bit, 1, 0, 2);
        put_native_u16(native_bytes, stride_10bit, 2, 0, 3);
        put_native_u16(native_bytes, stride_10bit, 0, 1, 4);
        put_native_u16(native_bytes, stride_10bit, 1, 1, 5);
        put_native_u16(native_bytes, stride_10bit, 2, 1, 1024);

        const Cnr3ConstNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_10bit,
            bits_10
        };
        std::vector<int> scalar(static_cast<std::size_t>(scalar_stride * height), -900);
        Cnr3MutablePlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };

        if (
            cnr3_copy_native_plane_to_scalar_buffer(native_plane, scalar_plane) != Cnr3Status::invalid_argument ||
            scalar[0] != -900 ||
            scalar[1] != -900 ||
            scalar[2] != -900 ||
            scalar[4] != -900 ||
            scalar[5] != -900 ||
            scalar[6] != -900 ||
            !scalar_padding_is(scalar, width, height, scalar_stride, -900)
            ) {
            return fail();
        }
    }

    {
        const std::vector<int> scalar{
            1, 2, 3, -1,
            4, 5, 1024, -1
        };
        const Cnr3ConstPlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_10bit * height), 0xAAU);
        Cnr3MutableNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            stride_10bit,
            bits_10
        };

        if (cnr3_copy_scalar_buffer_to_native_plane(scalar_plane, native_plane) != Cnr3Status::invalid_argument) {
            return fail();
        }

        for (std::uint8_t byte : native_bytes) {
            if (byte != 0xAAU) {
                return fail();
            }
        }
    }

    {
        std::vector<std::uint8_t> native_bytes(static_cast<std::size_t>(stride_10bit * height), 0U);
        const Cnr3ConstNativePlaneByteView native_plane{
            native_bytes.data(),
            width,
            height,
            5,
            bits_10
        };
        std::vector<int> scalar(static_cast<std::size_t>(scalar_stride * height), -910);
        Cnr3MutablePlaneBufferView scalar_plane{
            scalar.data(),
            width,
            height,
            scalar_stride
        };

        if (
            cnr3_copy_native_plane_to_scalar_buffer(native_plane, scalar_plane) != Cnr3Status::invalid_argument ||
            scalar[0] != -910 ||
            scalar[1] != -910 ||
            scalar[2] != -910 ||
            scalar[4] != -910 ||
            scalar[5] != -910 ||
            scalar[6] != -910
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line("P.8A native byte-plane access vector proof scenario");
    cnr3_cache_core_selftest_trace_line("    bit-depth proof maps 8-bit to one-byte storage and 9..16-bit to two-byte storage");
    cnr3_cache_core_selftest_trace_line("    native byte-stride loads convert 8-bit and 10-bit rows into scalar int planes");
    cnr3_cache_core_selftest_trace_line("    10-bit vectors prove column byte offsets use x*storage_bytes");
    cnr3_cache_core_selftest_trace_line("    scalar-to-native stores preserve active samples and native padding bytes");
    cnr3_cache_core_selftest_trace_line("    invalid late native/scalar samples publish no partial destination plane");
    cnr3_cache_core_selftest_trace_line("    VapourSynth frame ownership, source-frame lifecycle, and scene-change remain deferred");

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_native_luma_downsample_bridge_proof() noexcept {
    /*
        P.9A composes the P.8A native byte-buffer access proof with the P.7A
        scalar source-luma downsample traversal. It still does not access
        VapourSynth frames or source-frame lifecycle state.
    */
    constexpr int bits_8 = 8;
    constexpr int bits_10 = 10;

    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto scalar_padding_is = [](
        const std::vector<int>& output,
        int width,
        int height,
        int stride,
        int sentinel
    ) noexcept -> bool {
        for (int y = 0; y < height; ++y) {
            for (int x = width; x < stride; ++x) {
                if (output[static_cast<std::size_t>((y * stride) + x)] != sentinel) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto scalar_plane_is = [](
        const std::vector<int>& output,
        int width,
        int height,
        int stride,
        int sentinel
    ) noexcept -> bool {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < stride; ++x) {
                if (output[static_cast<std::size_t>((y * stride) + x)] != sentinel) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto summary_matches = [](
        const Cnr3DownsampledLumaPlaneProcessSummary& summary,
        int expected_source_width,
        int expected_source_height,
        int expected_output_width,
        int expected_output_height,
        int expected_count,
        int expected_first,
        int expected_last
    ) noexcept -> bool {
        return summary.source_width == expected_source_width &&
            summary.source_height == expected_source_height &&
            summary.output_width == expected_output_width &&
            summary.output_height == expected_output_height &&
            summary.samples_processed == expected_count &&
            summary.first_output_sample == expected_first &&
            summary.last_output_sample == expected_last;
    };

    const auto fill_native_plane = [](
        Cnr3MutableNativePlaneByteView& native_plane,
        int multiplier
    ) noexcept -> Cnr3Status {
        for (int y = 0; y < native_plane.height; ++y) {
            for (int x = 0; x < native_plane.width; ++x) {
                const int sample = (y * multiplier) + x;
                const Cnr3Status status = cnr3_store_native_plane_sample(
                    native_plane,
                    x,
                    y,
                    sample
                );

                if (status != Cnr3Status::ok) {
                    return status;
                }
            }
        }

        return Cnr3Status::ok;
    };

    {
        constexpr int width = 5;
        constexpr int height = 5;
        constexpr int stride_bytes = 14;
        constexpr int output_width = 3;
        constexpr int output_height = 3;
        constexpr int output_stride = 5;
        constexpr int expected_first = 51;
        constexpr int expected_mid = 253;
        constexpr int expected_right_edge = 54;
        constexpr int expected_bottom_edge = 401;
        constexpr int expected_last = 404;

        static_assert(expected_first == ((0 + 1 + 100 + 101 + 2) >> 2));
        static_assert(expected_mid == ((202 + 203 + 302 + 303 + 2) >> 2));
        static_assert(expected_right_edge == ((4 + 4 + 104 + 104 + 2) >> 2));
        static_assert(expected_bottom_edge == ((400 + 401 + 400 + 401 + 2) >> 2));
        static_assert(expected_last == ((404 + 404 + 404 + 404 + 2) >> 2));

        std::vector<std::uint8_t> native_bytes(
            static_cast<std::size_t>(stride_bytes * height),
            0xEEU
        );
        Cnr3MutableNativePlaneByteView native_luma_mutable{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_10
        };

        if (fill_native_plane(native_luma_mutable, 100) != Cnr3Status::ok) {
            return fail();
        }

        const Cnr3ConstNativePlaneByteView native_luma{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_10
        };
        std::vector<int> output_luma(
            static_cast<std::size_t>(output_stride * output_height),
            -800
        );
        Cnr3MutablePlaneBufferView output_luma_plane{
            output_luma.data(),
            output_width,
            output_height,
            output_stride
        };
        Cnr3DownsampledLumaPlaneProcessSummary summary{};

        if (
            cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
                native_luma,
                1,
                1,
                output_luma_plane,
                summary
            ) != Cnr3Status::ok ||
            output_luma[0] != expected_first ||
            output_luma[1] != 53 ||
            output_luma[2] != expected_right_edge ||
            output_luma[5] != 251 ||
            output_luma[6] != expected_mid ||
            output_luma[7] != 254 ||
            output_luma[10] != expected_bottom_edge ||
            output_luma[11] != 403 ||
            output_luma[12] != expected_last ||
            !scalar_padding_is(output_luma, output_width, output_height, output_stride, -800) ||
            !summary_matches(
                summary,
                width,
                height,
                output_width,
                output_height,
                output_width * output_height,
                expected_first,
                expected_last
            )
            ) {
            return fail();
        }
    }

    {
        constexpr int width = 4;
        constexpr int height = 2;
        constexpr int stride_bytes = 6;
        constexpr int output_width = 4;
        constexpr int output_height = 2;
        constexpr int output_stride = 5;

        std::vector<std::uint8_t> native_bytes(
            static_cast<std::size_t>(stride_bytes * height),
            0xCCU
        );
        Cnr3MutableNativePlaneByteView native_luma_mutable{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_8
        };

        if (fill_native_plane(native_luma_mutable, 20) != Cnr3Status::ok) {
            return fail();
        }

        const Cnr3ConstNativePlaneByteView native_luma{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_8
        };
        std::vector<int> output_luma(
            static_cast<std::size_t>(output_stride * output_height),
            -810
        );
        Cnr3MutablePlaneBufferView output_luma_plane{
            output_luma.data(),
            output_width,
            output_height,
            output_stride
        };
        Cnr3DownsampledLumaPlaneProcessSummary summary{};

        if (
            cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
                native_luma,
                0,
                0,
                output_luma_plane,
                summary
            ) != Cnr3Status::ok ||
            output_luma[0] != 1 ||
            output_luma[1] != 2 ||
            output_luma[2] != 3 ||
            output_luma[3] != 3 ||
            output_luma[5] != 21 ||
            output_luma[6] != 22 ||
            output_luma[7] != 23 ||
            output_luma[8] != 23 ||
            !scalar_padding_is(output_luma, output_width, output_height, output_stride, -810) ||
            !summary_matches(summary, width, height, output_width, output_height, 8, 1, 23)
            ) {
            return fail();
        }
    }

    {
        constexpr int width = 5;
        constexpr int height = 2;
        constexpr int stride_bytes = 12;
        constexpr int output_width = 3;
        constexpr int output_height = 2;
        constexpr int output_stride = 4;

        std::vector<std::uint8_t> native_bytes(
            static_cast<std::size_t>(stride_bytes * height),
            0U
        );
        Cnr3MutableNativePlaneByteView native_luma_mutable{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_10
        };

        if (fill_native_plane(native_luma_mutable, 100) != Cnr3Status::ok) {
            return fail();
        }

        const std::uint16_t invalid_sample = 2048U;
        const std::size_t invalid_offset =
            static_cast<std::size_t>(1 * stride_bytes) +
            (static_cast<std::size_t>(3) * static_cast<std::size_t>(2));
        std::memcpy(native_bytes.data() + invalid_offset, &invalid_sample, sizeof(invalid_sample));

        const Cnr3ConstNativePlaneByteView native_luma{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_10
        };
        std::vector<int> output_luma(
            static_cast<std::size_t>(output_stride * output_height),
            -820
        );
        Cnr3MutablePlaneBufferView output_luma_plane{
            output_luma.data(),
            output_width,
            output_height,
            output_stride
        };
        Cnr3DownsampledLumaPlaneProcessSummary summary{};
        summary.source_width = 9;
        summary.source_height = 8;
        summary.output_width = 7;
        summary.output_height = 6;
        summary.samples_processed = 5;
        summary.first_output_sample = 4;
        summary.last_output_sample = 3;

        if (
            cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
                native_luma,
                1,
                0,
                output_luma_plane,
                summary
            ) != Cnr3Status::invalid_argument ||
            !scalar_plane_is(output_luma, output_width, output_height, output_stride, -820) ||
            !summary_matches(summary, 9, 8, 7, 6, 5, 4, 3)
            ) {
            return fail();
        }
    }

    {
        constexpr int width = 5;
        constexpr int height = 2;
        constexpr int stride_bytes = 12;
        constexpr int output_width = 2;
        constexpr int output_height = 2;
        constexpr int output_stride = 3;

        std::vector<std::uint8_t> native_bytes(
            static_cast<std::size_t>(stride_bytes * height),
            0U
        );
        Cnr3MutableNativePlaneByteView native_luma_mutable{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_10
        };

        if (fill_native_plane(native_luma_mutable, 100) != Cnr3Status::ok) {
            return fail();
        }

        const Cnr3ConstNativePlaneByteView native_luma{
            native_bytes.data(),
            width,
            height,
            stride_bytes,
            bits_10
        };
        std::vector<int> output_luma(
            static_cast<std::size_t>(output_stride * output_height),
            -830
        );
        Cnr3MutablePlaneBufferView output_luma_plane{
            output_luma.data(),
            output_width,
            output_height,
            output_stride
        };
        Cnr3DownsampledLumaPlaneProcessSummary summary{};
        summary.source_width = 1;
        summary.source_height = 2;
        summary.output_width = 3;
        summary.output_height = 4;
        summary.samples_processed = 5;
        summary.first_output_sample = 6;
        summary.last_output_sample = 7;

        if (
            cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
                native_luma,
                1,
                0,
                output_luma_plane,
                summary
            ) != Cnr3Status::invalid_argument ||
            !scalar_plane_is(output_luma, output_width, output_height, output_stride, -830) ||
            !summary_matches(summary, 1, 2, 3, 4, 5, 6, 7)
            ) {
            return fail();
        }
    }

    {
        constexpr int width = 5;
        constexpr int height = 2;
        constexpr int bad_stride_bytes = 9;
        constexpr int output_width = 3;
        constexpr int output_height = 2;
        constexpr int output_stride = 4;

        std::vector<std::uint8_t> native_bytes(
            static_cast<std::size_t>(10 * height),
            0U
        );
        const Cnr3ConstNativePlaneByteView native_luma{
            native_bytes.data(),
            width,
            height,
            bad_stride_bytes,
            bits_10
        };
        std::vector<int> output_luma(
            static_cast<std::size_t>(output_stride * output_height),
            -840
        );
        Cnr3MutablePlaneBufferView output_luma_plane{
            output_luma.data(),
            output_width,
            output_height,
            output_stride
        };
        Cnr3DownsampledLumaPlaneProcessSummary summary{};
        summary.source_width = 4;
        summary.source_height = 3;
        summary.output_width = 2;
        summary.output_height = 1;
        summary.samples_processed = 9;
        summary.first_output_sample = 8;
        summary.last_output_sample = 7;

        if (
            cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
                native_luma,
                1,
                0,
                output_luma_plane,
                summary
            ) != Cnr3Status::invalid_argument ||
            !scalar_plane_is(output_luma, output_width, output_height, output_stride, -840) ||
            !summary_matches(summary, 4, 3, 2, 1, 9, 8, 7)
            ) {
            return fail();
        }
    }

    cnr3_cache_core_selftest_trace_line("P.9A native luma downsample bridge proof scenario");
    cnr3_cache_core_selftest_trace_line("    native source-luma byte buffers are copied through P.8A before P.7A downsample traversal");
    cnr3_cache_core_selftest_trace_line("    10-bit native luma vector proves x*storage_bytes survives the composed downsample path");
    cnr3_cache_core_selftest_trace_line("    8-bit native luma vector proves one-byte source-luma input path");
    cnr3_cache_core_selftest_trace_line("    invalid late native sample and output geometry proofs publish no partial scalar plane");
    cnr3_cache_core_selftest_trace_line("    VapourSynth frame ownership, source-frame lifecycle, scene-change, and getFrame/cache integration remain deferred");

    return Cnr3Status::ok;
}


Cnr3CacheCoreSelftestRunResult cnr3_cache_core_selftest_run_all() noexcept {
    using Cnr3CacheCoreSelftestFunction = Cnr3Status(*)() noexcept;

    struct Cnr3CacheCoreSelftestEntry {
        const char* name = nullptr;
        Cnr3CacheCoreSelftestFunction function = nullptr;
    };

    const Cnr3CacheCoreSelftestEntry selftests[] = {
        {
            "empty_model",
            cnr3_cache_core_selftest_empty_model
        },
        {
            "slot_id_source",
            cnr3_cache_core_selftest_slot_id_source
        },
        {
            "store_rejects_empty_owned_frame",
            cnr3_cache_core_selftest_store_rejects_empty_owned_frame
        },
        {
            "store_success_and_duplicate",
            cnr3_cache_core_selftest_store_success_and_duplicate
        },
        {
            "checkpoint_store_flag_lifecycle",
            cnr3_cache_core_selftest_checkpoint_store_flag_lifecycle
        },
        {
            "as2_store_record_monotonic_checkpoint",
            cnr3_cache_core_selftest_as2_store_record_monotonic_checkpoint
        },
        {
            "central_remove_helper_lifecycle",
            cnr3_cache_core_selftest_central_remove_helper_lifecycle
        },
        {
            "bounded_selected_detach_lifecycle",
            cnr3_cache_core_selftest_bounded_selected_detach_lifecycle
        },
        {
            "unpinned_noncheckpoint_selection_lifecycle",
            cnr3_cache_core_selftest_unpinned_noncheckpoint_selection_lifecycle
        },
        {
            "checkpoint_retention_boundary_lifecycle",
            cnr3_cache_core_selftest_checkpoint_retention_boundary_lifecycle
        },
        {
            "cache_policy_constants",
            cnr3_cache_core_selftest_cache_policy_constants
        },
        {
            "hot_zone_data_model",
            cnr3_cache_core_selftest_hot_zone_data_model
        },
        {
            "hot_zone_slide_spawn_lifecycle",
            cnr3_cache_core_selftest_hot_zone_slide_spawn_lifecycle
        },
        {
            "hot_zone_capacity_merge_lifecycle",
            cnr3_cache_core_selftest_hot_zone_capacity_merge_lifecycle
        },
        {
            "hot_zone_retirement_decay_lifecycle",
            cnr3_cache_core_selftest_hot_zone_retirement_decay_lifecycle
        },
        {
            "hot_zone_dsum11_counter_model",
            cnr3_cache_core_selftest_hot_zone_dsum11_counter_model
        },
        {
            "hot_zone_prune_protection_selection_lifecycle",
            cnr3_cache_core_selftest_hot_zone_prune_protection_selection_lifecycle
        },
        {
            "prune_victim_distance_ordering",
            cnr3_cache_core_selftest_prune_victim_distance_ordering
        },
        {
            "composite_prune_candidate_selection",
            cnr3_cache_core_selftest_composite_prune_candidate_selection
        },
        {
            "prune_trigger_decision_hysteresis",
            cnr3_cache_core_selftest_prune_trigger_decision_hysteresis
        },
        {
            "as5_prune_execution_decide_detach_free",
            cnr3_cache_core_selftest_as5_prune_execution_decide_detach_free
        },
        {
            "as1_bounded_recovery_search_scaffold",
            cnr3_cache_core_selftest_as1_bounded_recovery_search_scaffold
        },
        {
            "as1_recovery_anchor_pin_record",
            cnr3_cache_core_selftest_as1_recovery_anchor_pin_record
        },
        {
            "as2_recovery_store_consumer",
            cnr3_cache_core_selftest_as2_recovery_store_consumer
        },
        {
            "recovery_plan_contiguity_guard",
            cnr3_cache_core_selftest_recovery_plan_contiguity_guard
        },
        {
            "aggregate_cache_core_workload",
            cnr3_cache_core_selftest_aggregate_cache_core_workload
        },
        {
            "response_table_vector_proof",
            cnr3_cache_core_selftest_response_table_vector_proof
        },
        {
            "response_table_config_surface_proof",
            cnr3_cache_core_selftest_response_table_config_surface_proof
        },
        {
            "weighted_chroma_blend_vector_proof",
            cnr3_cache_core_selftest_weighted_chroma_blend_vector_proof
        },
        {
            "downsampled_luma_vector_proof",
            cnr3_cache_core_selftest_downsampled_luma_vector_proof
        },
        {
            "signed_difference_table_lookup_blend_proof",
            cnr3_cache_core_selftest_signed_difference_table_lookup_blend_proof
        },
        {
            "chroma_plane_traversal_vector_proof",
            cnr3_cache_core_selftest_chroma_plane_traversal_vector_proof
        },
        {
            "source_luma_downsample_plane_traversal_proof",
            cnr3_cache_core_selftest_source_luma_downsample_plane_traversal_proof
        },
        {
            "native_byte_plane_access_vector_proof",
            cnr3_cache_core_selftest_native_byte_plane_access_vector_proof
        },
        {
            "native_luma_downsample_bridge_proof",
            cnr3_cache_core_selftest_native_luma_downsample_bridge_proof
        },
        {
            "lookup_addref_hit_and_miss",
            cnr3_cache_core_selftest_lookup_addref_hit_and_miss
        },
        {
            "clear_teardown_releases_cached_frames_once",
            cnr3_cache_core_selftest_clear_teardown_releases_cached_frames_once
        },
        {
            "slot_pin_unpin_lifecycle",
            cnr3_cache_core_selftest_slot_pin_unpin_lifecycle
        },
        {
            "lookup_pin_reservation_lifecycle",
            cnr3_cache_core_selftest_lookup_pin_reservation_lifecycle
        },
        {
            "per_invocation_pin_list_lifecycle",
            cnr3_cache_core_selftest_per_invocation_pin_list_lifecycle
        },
        {
            "as1_lookup_pin_record_atomicity",
            cnr3_cache_core_selftest_as1_lookup_pin_record_atomicity
        }
    };

    Cnr3CacheCoreSelftestRunResult result{};

    for (const Cnr3CacheCoreSelftestEntry& selftest : selftests) {
        ++result.total_count;

        if (selftest.name == nullptr || selftest.function == nullptr) {
            ++result.failed_count;

            if (result.first_failed_test_name == nullptr) {
                result.first_failed_test_name = "invalid_selftest_entry";
                result.first_failed_status = Cnr3Status::invariant_violation;
            }

            continue;
        }

        const Cnr3Status status = selftest.function();

        if (cnr3_status_is_ok(status)) {
            ++result.passed_count;
            continue;
        }

        ++result.failed_count;

        if (result.first_failed_test_name == nullptr) {
            result.first_failed_test_name = selftest.name;
            result.first_failed_status = status;
        }
    }

    return result;
}

bool cnr3_cache_core_selftest_run_result_passed(
    const Cnr3CacheCoreSelftestRunResult& result
) noexcept {
    return
        result.total_count > 0 &&
        result.failed_count == 0 &&
        result.passed_count == result.total_count &&
        result.first_failed_test_name == nullptr &&
        cnr3_status_is_ok(result.first_failed_status);
}

/*
    CMS07-G.3A cache-core selftest/audit note.

    The current isolated selftest suite proves cache-core data structure,
    ownership, pin, checkpoint, remove/detach, policy-constant boundaries, and
    initial hot-zone slide/spawn behaviour before those mechanisms are connected
    to VapourSynth getFrame scheduling.

    CMS07-G.3A adds only hot-zone observation slide/spawn behaviour. It does not
    introduce hot-zone merge, retirement/decay transition, active-ceiling
    calculation, prune distance ordering, recovery planning, AS2
    store/adopt/pin/record/checkpoint logic, source lifecycle handling, pixel
    behaviour, or D-SUM production counters.

    Future selftests must continue to verify ownership and lifecycle properties
    before behaviour is trusted. In particular, tests must prove that:
        - pin/unpin operations balance;
        - lookup-reference acquire/release/transfer operations balance;
        - first-in-best-dressed store behaviour preserves the first winner;
        - duplicate-store losers are released or transferred through the
          documented ownership path;
        - prune never evicts pinned, checkpoint-protected, or hot-zone-protected
          frames;
        - bounded recovery planning searches only within its allowed bounds;
        - teardown/clear releases all retained references and reports unsafe
          non-zero pin state.

    Selftest scaffolds must not become production fallback code.
*/
