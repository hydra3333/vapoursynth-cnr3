#include "cnr3_cache_core_selftest.h"

#include "cnr3_cache_core.h"

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

} // namespace

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
