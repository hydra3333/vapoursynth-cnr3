#include "cnr3_cache_core_selftest.h"

#include "cnr3_cache_core.h"

#include "cnr3_diagnostics.h"

#include <cstdio>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

    struct Cnr3CacheCoreSelftestVsApiState {
        int add_frame_ref_count = 0;
        int free_frame_count = 0;
        const VSFrame* last_add_ref_frame = nullptr;
        const VSFrame* last_freed_frame = nullptr;

        const VSFrame* tracked_release_frames[4] = {};
        int tracked_release_counts[4] = {};
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

            for (int tracked_index = 0; tracked_index < 4; ++tracked_index) {
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
