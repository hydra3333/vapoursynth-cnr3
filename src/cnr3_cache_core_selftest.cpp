#include "cnr3_cache_core_selftest.h"

#include "cnr3_cache_core.h"

#include "cnr3_diagnostics.h"

#include "cnr3_frame_processing.h"

#include "cnr3_response_tables.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

    constexpr int CNR3_CACHE_CORE_SELFTEST_TRACKED_RELEASE_FRAME_COUNT = 8;
    constexpr int CNR3_CACHE_CORE_SELFTEST_FAKE_PLANE_COUNT = 3;
    constexpr int CNR3_CACHE_CORE_SELFTEST_FAKE_FRAME_SLOT_COUNT = 3;

    struct Cnr3CacheCoreSelftestVsFramePlaneState {
        int width = 0;
        int height = 0;
        ptrdiff_t stride = 0;
        const std::uint8_t* read_ptr = nullptr;
        std::uint8_t* write_ptr = nullptr;
    };

    struct Cnr3CacheCoreSelftestVsFrameState {
        const VSFrame* frame = nullptr;
        Cnr3CacheCoreSelftestVsFramePlaneState planes[
            CNR3_CACHE_CORE_SELFTEST_FAKE_PLANE_COUNT
        ] = {};
    };

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

        const VSFrame* fake_plane_frame = nullptr;
        Cnr3CacheCoreSelftestVsFramePlaneState fake_planes[
            CNR3_CACHE_CORE_SELFTEST_FAKE_PLANE_COUNT
        ] = {};
        Cnr3CacheCoreSelftestVsFrameState fake_frames[
            CNR3_CACHE_CORE_SELFTEST_FAKE_FRAME_SLOT_COUNT
        ] = {};

        int get_stride_count = 0;
        int get_read_ptr_count = 0;
        int get_write_ptr_count = 0;
        int get_frame_width_count = 0;
        int get_frame_height_count = 0;
        int last_plane = -1;
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

    [[nodiscard]] Cnr3CacheCoreSelftestVsFramePlaneState* cnr3_cache_core_selftest_find_fake_plane(
        const VSFrame* frame,
        int plane
    ) noexcept {
        if (
            g_cnr3_cache_core_selftest_vsapi_state == nullptr ||
            frame == nullptr ||
            plane < 0 ||
            plane >= CNR3_CACHE_CORE_SELFTEST_FAKE_PLANE_COUNT
            ) {
            return nullptr;
        }

        if (frame == g_cnr3_cache_core_selftest_vsapi_state->fake_plane_frame) {
            return &g_cnr3_cache_core_selftest_vsapi_state->fake_planes[plane];
        }

        for (Cnr3CacheCoreSelftestVsFrameState& fake_frame :
            g_cnr3_cache_core_selftest_vsapi_state->fake_frames) {
            if (frame == fake_frame.frame) {
                return &fake_frame.planes[plane];
            }
        }

        return nullptr;
    }

    ptrdiff_t VS_CC cnr3_cache_core_selftest_get_stride(
        const VSFrame* frame,
        int plane
    ) noexcept {
        const Cnr3CacheCoreSelftestVsFramePlaneState* plane_state =
            cnr3_cache_core_selftest_find_fake_plane(frame, plane);

        if (plane_state == nullptr) {
            return 0;
        }

        ++g_cnr3_cache_core_selftest_vsapi_state->get_stride_count;
        g_cnr3_cache_core_selftest_vsapi_state->last_plane = plane;
        return plane_state->stride;
    }

    const std::uint8_t* VS_CC cnr3_cache_core_selftest_get_read_ptr(
        const VSFrame* frame,
        int plane
    ) noexcept {
        const Cnr3CacheCoreSelftestVsFramePlaneState* plane_state =
            cnr3_cache_core_selftest_find_fake_plane(frame, plane);

        if (plane_state == nullptr) {
            return nullptr;
        }

        ++g_cnr3_cache_core_selftest_vsapi_state->get_read_ptr_count;
        g_cnr3_cache_core_selftest_vsapi_state->last_plane = plane;
        return plane_state->read_ptr;
    }

    std::uint8_t* VS_CC cnr3_cache_core_selftest_get_write_ptr(
        VSFrame* frame,
        int plane
    ) noexcept {
        const Cnr3CacheCoreSelftestVsFramePlaneState* plane_state =
            cnr3_cache_core_selftest_find_fake_plane(frame, plane);

        if (plane_state == nullptr) {
            return nullptr;
        }

        ++g_cnr3_cache_core_selftest_vsapi_state->get_write_ptr_count;
        g_cnr3_cache_core_selftest_vsapi_state->last_plane = plane;
        return plane_state->write_ptr;
    }

    int VS_CC cnr3_cache_core_selftest_get_frame_width(
        const VSFrame* frame,
        int plane
    ) noexcept {
        const Cnr3CacheCoreSelftestVsFramePlaneState* plane_state =
            cnr3_cache_core_selftest_find_fake_plane(frame, plane);

        if (plane_state == nullptr) {
            return 0;
        }

        ++g_cnr3_cache_core_selftest_vsapi_state->get_frame_width_count;
        g_cnr3_cache_core_selftest_vsapi_state->last_plane = plane;
        return plane_state->width;
    }

    int VS_CC cnr3_cache_core_selftest_get_frame_height(
        const VSFrame* frame,
        int plane
    ) noexcept {
        const Cnr3CacheCoreSelftestVsFramePlaneState* plane_state =
            cnr3_cache_core_selftest_find_fake_plane(frame, plane);

        if (plane_state == nullptr) {
            return 0;
        }

        ++g_cnr3_cache_core_selftest_vsapi_state->get_frame_height_count;
        g_cnr3_cache_core_selftest_vsapi_state->last_plane = plane;
        return plane_state->height;
    }

    VSAPI cnr3_cache_core_selftest_make_vsapi() noexcept {
        VSAPI vsapi{};

        vsapi.addFrameRef = cnr3_cache_core_selftest_add_frame_ref;
        vsapi.freeFrame = cnr3_cache_core_selftest_free_frame;
        vsapi.getStride = cnr3_cache_core_selftest_get_stride;
        vsapi.getReadPtr = cnr3_cache_core_selftest_get_read_ptr;
        vsapi.getWritePtr = cnr3_cache_core_selftest_get_write_ptr;
        vsapi.getFrameWidth = cnr3_cache_core_selftest_get_frame_width;
        vsapi.getFrameHeight = cnr3_cache_core_selftest_get_frame_height;

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

    void cnr3_cache_core_selftest_write_u16_sample(
        std::uint8_t* storage,
        int offset,
        std::uint16_t value
    ) noexcept {
        std::memcpy(storage + offset, &value, sizeof(value));
    }

} // namespace

struct Cnr3CachePinListSelftestAccess {
    [[nodiscard]] static bool has_used_token_at(
        const Cnr3CachePinList& pin_list,
        std::size_t pin_index
    ) noexcept {
        return pin_index < pin_list.used_pin_count_;
    }

    [[nodiscard]] static Cnr3CacheSlotPinToken token_at(
        const Cnr3CachePinList& pin_list,
        std::size_t pin_index
    ) noexcept {
        if (pin_index >= pin_list.used_pin_count_) {
            return {};
        }

        return pin_list.pin_tokens_[pin_index];
    }

    static void replace_token_at(
        Cnr3CachePinList& pin_list,
        std::size_t pin_index,
        const Cnr3CacheSlotPinToken& pin_token
    ) noexcept {
        if (pin_index >= pin_list.used_pin_count_) {
            return;
        }

        pin_list.pin_tokens_[pin_index] = pin_token;
    }
};

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

Cnr3Status cnr3_cache_core_selftest_d4_present_frame_adopt_skip_primitive() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int present_frame_storage = 1;
    const VSFrame* present_frame =
        reinterpret_cast<const VSFrame*>(&present_frame_storage);

    vsapi_state.tracked_release_frames[0] = present_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList consumer_pin_list{};
        Cnr3OwnedFrameRef present_owned_frame{};

        if (
            present_owned_frame.reset_to_owned_frame(
                present_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                40,
                std::move(present_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (present_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.total_pin_count() != 0 ||
            consumer_pin_list.pin_count() != 0U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0 || vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        /*
            D.4 proves the context-free primitive that live floor and hole
            adopt-skip paths call after a race outcome has made the frame
            present. The selftest calls lookup directly, so no compute or store
            path is invoked by construction; live scheduling that reaches this
            outcome is deferred to fmParallel validation.
        */
        if (cache.lookup_frame_and_record_pin(40, consumer_pin_list) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.total_pin_count() != 1 ||
            consumer_pin_list.pin_count() != 1U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.add_frame_ref_count != 0 || vsapi_state.free_frame_count != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.remove_unpinned_frame(40) != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (consumer_pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.total_pin_count() != 0 ||
            consumer_pin_list.pin_count() != 0U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::ok) {
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
    }

    if (
        vsapi_state.add_frame_ref_count != 0 ||
        vsapi_state.free_frame_count != 1 ||
        vsapi_state.tracked_release_counts[0] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "D.4 present-frame adopt-skip primitive scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    pre-present frame 40 is adopted by lookup_frame_and_record_pin"
    );
    cnr3_cache_core_selftest_trace_line(
        "    no addFrameRef/freeFrame side effects occur during pin adoption"
    );
    cnr3_cache_core_selftest_trace_line(
        "    AS4 discharge balances the adopted pin and releases clear normally"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_d4_first_in_best_dressed_duplicate_primitive() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int winner_frame_storage = 1;
    int loser_frame_storage = 2;

    const VSFrame* winner_frame =
        reinterpret_cast<const VSFrame*>(&winner_frame_storage);
    const VSFrame* loser_frame =
        reinterpret_cast<const VSFrame*>(&loser_frame_storage);

    vsapi_state.tracked_release_frames[0] = winner_frame;
    vsapi_state.tracked_release_frames[1] = loser_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList consumer_pin_list{};
        Cnr3CacheAs2StoreRecordSummary summary{};
        Cnr3OwnedFrameRef winner_owned_frame{};

        if (
            winner_owned_frame.reset_to_owned_frame(
                winner_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_noncheckpoint_owned_frame(
                41,
                std::move(winner_owned_frame)
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (winner_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.total_pin_count() != 0 ||
            consumer_pin_list.pin_count() != 0U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef loser_owned_frame{};

        if (
            loser_owned_frame.reset_to_owned_frame(
                loser_frame,
                &vsapi
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.store_owned_frame_and_record_pin(
                41,
                std::move(loser_owned_frame),
                false,
                consumer_pin_list,
                summary
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (loser_owned_frame.has_frame()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            summary.frame_number != 41 ||
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
            cache.slot_count() != 1U ||
            cache.index_count() != 1U ||
            cache.total_pin_count() != 1 ||
            consumer_pin_list.pin_count() != 1U
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[1] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (vsapi_state.tracked_release_counts[0] != 0) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3OwnedFrameRef winner_lookup{};

        if (
            cache.lookup_frame_and_add_ref(
                41,
                &vsapi,
                winner_lookup
            ) != Cnr3Status::ok
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (winner_lookup.get() != winner_frame) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        winner_lookup.reset();

        if (
            vsapi_state.add_frame_ref_count != 1 ||
            vsapi_state.tracked_release_counts[0] != 1
            ) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (consumer_pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (
            cache.total_pin_count() != 0 ||
            consumer_pin_list.pin_count() != 0U
            ) {
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

        if (!cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    if (
        vsapi_state.add_frame_ref_count != 1 ||
        vsapi_state.free_frame_count != 3 ||
        vsapi_state.tracked_release_counts[0] != 2 ||
        vsapi_state.tracked_release_counts[1] != 1
        ) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    cnr3_cache_core_selftest_trace_line(
        "D.4 first-in-best-dressed duplicate scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    duplicate store for frame 41 preserves the existing winner"
    );
    cnr3_cache_core_selftest_trace_line(
        "    incoming loser is released once after the cache lock"
    );
    cnr3_cache_core_selftest_trace_line(
        "    duplicate_existing_slot reports the adopted-post-compute-loser outcome"
    );

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    return Cnr3Status::ok;
}

Cnr3Status cnr3_cache_core_selftest_d5_recovery_pin_survives_bounded_prune_pass() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    constexpr int checkpoint_frame_number = 0;
    constexpr int hot_zone_observation_frame = 100;
    constexpr int foundation_frame_number = 165;
    constexpr int expected_protected_victim_frame_number = 164;
    constexpr int requested_frame = foundation_frame_number + 1;
    constexpr int recovery_back_radius = 1;
    constexpr int stored_last_frame_number = foundation_frame_number;
    constexpr std::size_t stored_frame_count =
        static_cast<std::size_t>(stored_last_frame_number + 1);
    constexpr std::uint64_t prune_frame_byte_count =
        CNR3_CACHE_BYTE_BUDGET_BYTES + 1ULL;
    constexpr std::size_t retain_checkpoint_count = 1U;
    constexpr std::size_t max_remove_count = 1U;

    auto fail = []() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    auto store_frame = [](Cnr3OutputCacheCore& cache,
        int frame_number,
        bool is_checkpoint,
        const VSFrame* frame,
        const VSAPI* vsapi
    ) noexcept -> Cnr3Status {
        Cnr3OwnedFrameRef owned_frame{};

        const Cnr3Status adopt_status =
            owned_frame.reset_to_owned_frame(frame, vsapi);

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

    auto build_prune_state = [store_frame](Cnr3OutputCacheCore& cache,
        const std::vector<int>& frame_storage,
        const VSAPI* vsapi
    ) noexcept -> Cnr3Status {
        if (
            frame_storage.size() < stored_frame_count ||
            cache.record_hot_zone_observation(hot_zone_observation_frame) !=
                Cnr3Status::ok
            ) {
            return Cnr3Status::invariant_violation;
        }

        for (int frame_number = checkpoint_frame_number;
            frame_number <= stored_last_frame_number;
            ++frame_number) {
            const VSFrame* frame =
                reinterpret_cast<const VSFrame*>(&frame_storage[frame_number]);
            const bool is_checkpoint = (frame_number == checkpoint_frame_number);

            const Cnr3Status store_status =
                store_frame(cache, frame_number, is_checkpoint, frame, vsapi);

            if (!cnr3_status_is_ok(store_status)) {
                return store_status;
            }
        }

        if (
            cache.slot_count() != stored_frame_count ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 0
            ) {
            return Cnr3Status::invariant_violation;
        }

        return Cnr3Status::ok;
    };

    std::vector<int> control_frame_storage(stored_frame_count);
    std::vector<int> protected_frame_storage(stored_frame_count);

    if (
        control_frame_storage.size() != stored_frame_count ||
        protected_frame_storage.size() != stored_frame_count
        ) {
        return fail();
    }

    for (std::size_t i = 0; i < stored_frame_count; ++i) {
        control_frame_storage[i] = static_cast<int>(1000U + i);
        protected_frame_storage[i] = static_cast<int>(2000U + i);
    }

    const VSFrame* control_foundation_frame =
        reinterpret_cast<const VSFrame*>(
            &control_frame_storage[foundation_frame_number]
        );
    const VSFrame* control_next_victim_frame =
        reinterpret_cast<const VSFrame*>(
            &control_frame_storage[expected_protected_victim_frame_number]
        );
    const VSFrame* control_checkpoint_frame =
        reinterpret_cast<const VSFrame*>(
            &control_frame_storage[checkpoint_frame_number]
        );
    const VSFrame* protected_foundation_frame =
        reinterpret_cast<const VSFrame*>(
            &protected_frame_storage[foundation_frame_number]
        );
    const VSFrame* protected_next_victim_frame =
        reinterpret_cast<const VSFrame*>(
            &protected_frame_storage[expected_protected_victim_frame_number]
        );
    const VSFrame* protected_checkpoint_frame =
        reinterpret_cast<const VSFrame*>(
            &protected_frame_storage[checkpoint_frame_number]
        );
    const VSFrame* protected_hot_zone_frame =
        reinterpret_cast<const VSFrame*>(
            &protected_frame_storage[hot_zone_observation_frame]
        );

    vsapi_state.tracked_release_frames[0] = control_foundation_frame;
    vsapi_state.tracked_release_frames[1] = control_next_victim_frame;
    vsapi_state.tracked_release_frames[2] = control_checkpoint_frame;
    vsapi_state.tracked_release_frames[3] = protected_foundation_frame;
    vsapi_state.tracked_release_frames[4] = protected_next_victim_frame;
    vsapi_state.tracked_release_frames[5] = protected_checkpoint_frame;
    vsapi_state.tracked_release_frames[6] = protected_hot_zone_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        if (
            build_prune_state(cache, control_frame_storage, &vsapi) !=
            Cnr3Status::ok
            ) {
            return fail();
        }

        Cnr3CachePruneExecutionSummary prune_summary{};
        const int free_count_before_prune = vsapi_state.free_frame_count;

        if (
            cache.execute_bounded_prune_pass(
                prune_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                prune_summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !prune_summary.trigger_decision.prune_is_required ||
            prune_summary.trigger_decision.current_slot_count !=
                stored_frame_count ||
            prune_summary.bounded_remove_limit != max_remove_count ||
            prune_summary.selected_candidate_count != 1U ||
            prune_summary.detached_count != 1U ||
            cache.slot_count() != stored_frame_count - 1U ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 0 ||
            vsapi_state.free_frame_count != free_count_before_prune + 1 ||
            vsapi_state.tracked_release_counts[0] != 1 ||
            vsapi_state.tracked_release_counts[1] != 0 ||
            vsapi_state.tracked_release_counts[2] != 0
            ) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }

        if (!cache.empty() || cache.total_pin_count() != 0) {
            return fail();
        }

        if (
            vsapi_state.tracked_release_counts[0] != 1 ||
            vsapi_state.tracked_release_counts[1] != 1 ||
            vsapi_state.tracked_release_counts[2] != 1
            ) {
            return fail();
        }
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};
        Cnr3CacheRecoverySearchPlan plan{};

        if (
            build_prune_state(cache, protected_frame_storage, &vsapi) !=
            Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            cache.plan_bounded_recovery_search_and_record_anchor_pin(
                requested_frame,
                recovery_back_radius,
                pin_list,
                plan
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !plan.anchor_found ||
            plan.anchor_frame_number != foundation_frame_number ||
            plan.anchor_is_checkpoint ||
            !plan.anchor_pin_recorded ||
            !plan.hole_frame_numbers.empty() ||
            pin_list.pin_count() != 1U ||
            cache.total_pin_count() != 1
            ) {
            return fail();
        }

        Cnr3CachePruneExecutionSummary prune_summary{};
        const int free_count_before_prune = vsapi_state.free_frame_count;

        if (
            cache.execute_bounded_prune_pass(
                prune_frame_byte_count,
                retain_checkpoint_count,
                max_remove_count,
                prune_summary
            ) != Cnr3Status::ok
            ) {
            return fail();
        }

        if (
            !prune_summary.trigger_decision.prune_is_required ||
            prune_summary.trigger_decision.current_slot_count !=
                stored_frame_count ||
            prune_summary.bounded_remove_limit != max_remove_count ||
            prune_summary.selected_candidate_count != 1U ||
            prune_summary.detached_count != 1U ||
            cache.slot_count() != stored_frame_count - 1U ||
            cache.checkpoint_count() != 1U ||
            cache.total_pin_count() != 1 ||
            pin_list.pin_count() != 1U ||
            vsapi_state.free_frame_count != free_count_before_prune + 1 ||
            vsapi_state.tracked_release_counts[3] != 0 ||
            vsapi_state.tracked_release_counts[4] != 1 ||
            vsapi_state.tracked_release_counts[5] != 0 ||
            vsapi_state.tracked_release_counts[6] != 0
            ) {
            return fail();
        }

        Cnr3OwnedFrameRef survivor_lookup{};

        if (
            cache.lookup_frame_and_add_ref(
                foundation_frame_number,
                &vsapi,
                survivor_lookup
            ) != Cnr3Status::ok ||
            !survivor_lookup.has_frame() ||
            survivor_lookup.get() != protected_foundation_frame
            ) {
            survivor_lookup.reset();
            return fail();
        }

        if (vsapi_state.add_frame_ref_count != 1) {
            survivor_lookup.reset();
            return fail();
        }

        survivor_lookup.reset();

        if (vsapi_state.tracked_release_counts[3] != 1) {
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

        if (!cache.empty() || cache.total_pin_count() != 0) {
            return fail();
        }

        if (
            vsapi_state.tracked_release_counts[3] != 2 ||
            vsapi_state.tracked_release_counts[4] != 1 ||
            vsapi_state.tracked_release_counts[5] != 1 ||
            vsapi_state.tracked_release_counts[6] != 1
            ) {
            return fail();
        }
    }

    if (vsapi_state.add_frame_ref_count != 1) {
        return fail();
    }

    cnr3_cache_core_selftest_trace_line(
        "D.5 recovery pin survives bounded prune-pass scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    control: unpinned foundation frame 165 is the single prune victim"
    );
    cnr3_cache_core_selftest_trace_line(
        "    protected: recovery-pinned frame 165 survives the same prune pressure"
    );
    cnr3_cache_core_selftest_trace_line(
        "    protected: frame 164 is detached instead and AS4 discharge balances"
    );
    cnr3_cache_core_selftest_trace_line(
        "    survivor remains lookup-addref usable as the original predecessor"
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

Cnr3Status cnr3_cache_core_selftest_as4_single_lock_batch_discharge_proof() noexcept {
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int frame_storage[6] = { 10, 11, 12, 20, 21, 22 };
    const VSFrame* frames[6] = {
        reinterpret_cast<const VSFrame*>(&frame_storage[0]),
        reinterpret_cast<const VSFrame*>(&frame_storage[1]),
        reinterpret_cast<const VSFrame*>(&frame_storage[2]),
        reinterpret_cast<const VSFrame*>(&frame_storage[3]),
        reinterpret_cast<const VSFrame*>(&frame_storage[4]),
        reinterpret_cast<const VSFrame*>(&frame_storage[5])
    };

    for (int frame_index = 0; frame_index < 6; ++frame_index) {
        vsapi_state.tracked_release_frames[frame_index] = frames[frame_index];
    }

    const auto store_frame = [](
        Cnr3OutputCacheCore& cache,
        VSAPI& vsapi,
        int frame_number,
        const VSFrame* frame
    ) -> Cnr3Status {
        Cnr3OwnedFrameRef owned_frame{};

        const Cnr3Status adopt_status = owned_frame.reset_to_owned_frame(
            frame,
            &vsapi
        );

        if (!cnr3_status_is_ok(adopt_status)) {
            return adopt_status;
        }

        return cache.store_noncheckpoint_owned_frame(
            frame_number,
            std::move(owned_frame)
        );
    };

    struct Cnr3As4SelftestFrameDataHolder {
        Cnr3CachePinList pin_list{};
    };

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3As4SelftestFrameDataHolder* holder =
            new (std::nothrow) Cnr3As4SelftestFrameDataHolder{};

        if (holder == nullptr) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::allocation_failed;
        }

        for (int frame_offset = 0; frame_offset < 3; ++frame_offset) {
            if (
                store_frame(
                    cache,
                    vsapi,
                    100 + frame_offset,
                    frames[frame_offset]
                ) != Cnr3Status::ok
                ) {
                delete holder;
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (
                cache.lookup_frame_and_record_pin(
                    100 + frame_offset,
                    holder->pin_list
                ) != Cnr3Status::ok
                ) {
                delete holder;
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (cache.total_pin_count() != 3) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (holder->pin_list.pin_count() != 3U) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        std::size_t removed_count = 99U;
        const std::vector<int> pinned_candidates{ 100, 101, 102 };

        if (
            cache.remove_selected_unpinned_frames_bounded(
                pinned_candidates,
                pinned_candidates.size(),
                removed_count
            ) != Cnr3Status::lifecycle_violation
            ) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (removed_count != 0U || cache.total_pin_count() != 3) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.clear() != Cnr3Status::lifecycle_violation) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (holder->pin_list.discharge_all(cache) != Cnr3Status::ok) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0 || !holder->pin_list.empty()) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (holder->pin_list.discharge_all(cache) != Cnr3Status::ok) {
            delete holder;
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        delete holder;

        if (cache.clear() != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0 || !cache.empty()) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};
        Cnr3CachePinList pin_list{};

        for (int frame_offset = 0; frame_offset < 3; ++frame_offset) {
            if (
                store_frame(
                    cache,
                    vsapi,
                    200 + frame_offset,
                    frames[3 + frame_offset]
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }

            if (
                cache.lookup_frame_and_record_pin(
                    200 + frame_offset,
                    pin_list
                ) != Cnr3Status::ok
                ) {
                g_cnr3_cache_core_selftest_vsapi_state = nullptr;
                return Cnr3Status::invariant_violation;
            }
        }

        if (cache.total_pin_count() != 3 || pin_list.pin_count() != 3U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (!Cnr3CachePinListSelftestAccess::has_used_token_at(pin_list, 2U)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        const Cnr3CacheSlotPinToken original_middle_token =
            Cnr3CachePinListSelftestAccess::token_at(pin_list, 1U);

        if (!cnr3_cache_slot_pin_token_is_valid(original_middle_token)) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheSlotPinToken mismatched_middle_token = original_middle_token;
        ++mismatched_middle_token.slot_id.value;

        Cnr3CachePinListSelftestAccess::replace_token_at(
            pin_list,
            1U,
            mismatched_middle_token
        );

        if (pin_list.discharge_all(cache) != Cnr3Status::lifecycle_violation) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        /*
            The first and third tokens must have been attempted and released
            even though the middle token failed. If the batch walk aborted on
            first failure, two pins would remain instead of one.
        */
        if (cache.total_pin_count() != 1 || pin_list.pin_count() != 1U) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        Cnr3CachePinListSelftestAccess::replace_token_at(
            pin_list,
            1U,
            original_middle_token
        );

        if (pin_list.discharge_all(cache) != Cnr3Status::ok) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }

        if (cache.total_pin_count() != 0 || !pin_list.empty()) {
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

    if (vsapi_state.add_frame_ref_count != 0) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    if (vsapi_state.free_frame_count != 6) {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    }

    for (int frame_index = 0; frame_index < 6; ++frame_index) {
        if (vsapi_state.tracked_release_counts[frame_index] != 1) {
            g_cnr3_cache_core_selftest_vsapi_state = nullptr;
            return Cnr3Status::invariant_violation;
        }
    }

    cnr3_cache_core_selftest_trace_line(
        "Recovery-Step-0 AS4 single-lock batch discharge scenario"
    );
    cnr3_cache_core_selftest_trace_line(
        "    public discharge_all call site is stable and delegates to cache.discharge_pin_list"
    );
    cnr3_cache_core_selftest_trace_line(
        "    batch path uses one AS4 cache-lock acquisition and unpin_frame_locked worker"
    );
    cnr3_cache_core_selftest_trace_line(
        "    multi-pin discharge releases every token and double-discharge no-ops"
    );
    cnr3_cache_core_selftest_trace_line(
        "    partial invalid-token proof attempts all entries and returns first failure"
    );
    cnr3_cache_core_selftest_trace_line(
        "    public clear/remove cannot invalidate pinned tokens before discharge"
    );

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


Cnr3Status cnr3_cache_core_selftest_vapoursynth_plane_view_adapter_proof() noexcept {
    /*
        P.10A is the real-frame-memory adapter trigger, but still not getFrame
        lifecycle integration. It proves only VSAPI plane pointer/stride metadata
        conversion into the P.8A native byte-plane views.
    */
    constexpr int bits_8 = 8;
    constexpr int bits_10 = 10;
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr int stride_10bit = 8;

    int fake_frame_storage = 0;
    VSFrame* fake_frame = reinterpret_cast<VSFrame*>(&fake_frame_storage);
    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    vsapi_state.fake_plane_frame = fake_frame;
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto fail = [&]() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    const auto summary_matches = [](
        const Cnr3VapourSynthPlaneByteViewSummary& summary,
        int expected_plane,
        int expected_width,
        int expected_height,
        int expected_stride_bytes,
        int expected_bits_per_sample,
        int expected_storage_bytes,
        bool expected_read,
        bool expected_write
    ) noexcept -> bool {
        return summary.plane == expected_plane &&
            summary.width == expected_width &&
            summary.height == expected_height &&
            summary.stride_bytes == expected_stride_bytes &&
            summary.bits_per_sample == expected_bits_per_sample &&
            summary.storage_bytes == expected_storage_bytes &&
            summary.read_view_created == expected_read &&
            summary.write_view_created == expected_write;
    };

    std::vector<std::uint8_t> read_storage(static_cast<std::size_t>(stride_10bit * height), 0xEEU);
    cnr3_cache_core_selftest_write_u16_sample(read_storage.data(), 0, 0);
    cnr3_cache_core_selftest_write_u16_sample(read_storage.data(), 2, 1);
    cnr3_cache_core_selftest_write_u16_sample(read_storage.data(), 4, 2);
    cnr3_cache_core_selftest_write_u16_sample(read_storage.data(), 8, 100);
    cnr3_cache_core_selftest_write_u16_sample(read_storage.data(), 10, 101);
    cnr3_cache_core_selftest_write_u16_sample(read_storage.data(), 12, 102);

    vsapi_state.fake_planes[0].width = width;
    vsapi_state.fake_planes[0].height = height;
    vsapi_state.fake_planes[0].stride = stride_10bit;
    vsapi_state.fake_planes[0].read_ptr = read_storage.data();

    Cnr3ConstNativePlaneByteView read_view{};
    Cnr3VapourSynthPlaneByteViewSummary read_summary{};
    int read_sample = -1;

    if (
        cnr3_make_vapoursynth_read_plane_byte_view(
            fake_frame,
            &vsapi,
            0,
            bits_10,
            read_view,
            read_summary
        ) != Cnr3Status::ok ||
        read_view.data != read_storage.data() ||
        read_view.width != width ||
        read_view.height != height ||
        read_view.stride_bytes != stride_10bit ||
        read_view.bits_per_sample != bits_10 ||
        !summary_matches(read_summary, 0, width, height, stride_10bit, bits_10, 2, true, false) ||
        cnr3_load_native_plane_sample(read_view, 2, 1, read_sample) != Cnr3Status::ok ||
        read_sample != 102 ||
        vsapi_state.get_frame_width_count != 1 ||
        vsapi_state.get_frame_height_count != 1 ||
        vsapi_state.get_stride_count != 1 ||
        vsapi_state.get_read_ptr_count != 1 ||
        vsapi_state.get_write_ptr_count != 0
        ) {
        return fail();
    }

    std::vector<std::uint8_t> write_storage(static_cast<std::size_t>(stride_10bit * height), 0xAAU);
    vsapi_state.fake_planes[1].width = width;
    vsapi_state.fake_planes[1].height = height;
    vsapi_state.fake_planes[1].stride = stride_10bit;
    vsapi_state.fake_planes[1].write_ptr = write_storage.data();

    Cnr3MutableNativePlaneByteView write_view{};
    Cnr3VapourSynthPlaneByteViewSummary write_summary{};

    if (
        cnr3_make_vapoursynth_write_plane_byte_view(
            fake_frame,
            &vsapi,
            1,
            bits_10,
            write_view,
            write_summary
        ) != Cnr3Status::ok ||
        write_view.data != write_storage.data() ||
        write_view.width != width ||
        write_view.height != height ||
        write_view.stride_bytes != stride_10bit ||
        write_view.bits_per_sample != bits_10 ||
        !summary_matches(write_summary, 1, width, height, stride_10bit, bits_10, 2, false, true) ||
        vsapi_state.get_read_ptr_count != 1 ||
        vsapi_state.get_write_ptr_count != 1
        ) {
        return fail();
    }

    if (cnr3_store_native_plane_sample(write_view, 2, 1, 777) != Cnr3Status::ok) {
        return fail();
    }

    const Cnr3ConstNativePlaneByteView written_const_view{
        write_storage.data(),
        width,
        height,
        stride_10bit,
        bits_10
    };
    int written_sample = -1;

    if (
        cnr3_load_native_plane_sample(written_const_view, 2, 1, written_sample) != Cnr3Status::ok ||
        written_sample != 777 ||
        write_storage[6] != 0xAAU ||
        write_storage[7] != 0xAAU ||
        write_storage[14] != 0xAAU ||
        write_storage[15] != 0xAAU
        ) {
        return fail();
    }

    std::vector<std::uint8_t> read_8bit_storage{ 3U, 4U, 5U, 0xCCU, 6U, 7U, 8U, 0xCCU };
    vsapi_state.fake_planes[2].width = width;
    vsapi_state.fake_planes[2].height = height;
    vsapi_state.fake_planes[2].stride = 4;
    vsapi_state.fake_planes[2].read_ptr = read_8bit_storage.data();

    Cnr3ConstNativePlaneByteView read_8bit_view{};
    Cnr3VapourSynthPlaneByteViewSummary read_8bit_summary{};
    int read_8bit_sample = -1;

    if (
        cnr3_make_vapoursynth_read_plane_byte_view(
            fake_frame,
            &vsapi,
            2,
            bits_8,
            read_8bit_view,
            read_8bit_summary
        ) != Cnr3Status::ok ||
        !summary_matches(read_8bit_summary, 2, width, height, 4, bits_8, 1, true, false) ||
        cnr3_load_native_plane_sample(read_8bit_view, 1, 1, read_8bit_sample) != Cnr3Status::ok ||
        read_8bit_sample != 7
        ) {
        return fail();
    }

    {
        vsapi_state.fake_planes[2].stride = 7;
        Cnr3ConstNativePlaneByteView rejected_view{
            reinterpret_cast<const void*>(0x1),
            9,
            8,
            7,
            bits_10
        };
        Cnr3VapourSynthPlaneByteViewSummary rejected_summary{};
        rejected_summary.plane = 99;

        if (
            cnr3_make_vapoursynth_read_plane_byte_view(
                fake_frame,
                &vsapi,
                2,
                bits_10,
                rejected_view,
                rejected_summary
            ) != Cnr3Status::invalid_argument ||
            rejected_view.data != nullptr ||
            rejected_summary.plane != -1
            ) {
            return fail();
        }
    }

    {
        VSAPI missing_read_api = vsapi;
        missing_read_api.getReadPtr = nullptr;
        Cnr3ConstNativePlaneByteView rejected_view{};
        Cnr3VapourSynthPlaneByteViewSummary rejected_summary{};

        if (
            cnr3_make_vapoursynth_read_plane_byte_view(
                fake_frame,
                &missing_read_api,
                0,
                bits_10,
                rejected_view,
                rejected_summary
            ) != Cnr3Status::invalid_argument
            ) {
            return fail();
        }
    }

    {
        Cnr3MutableNativePlaneByteView rejected_write_view{};
        Cnr3VapourSynthPlaneByteViewSummary rejected_summary{};

        if (
            cnr3_make_vapoursynth_write_plane_byte_view(
                fake_frame,
                &vsapi,
                3,
                bits_10,
                rejected_write_view,
                rejected_summary
            ) != Cnr3Status::invalid_argument ||
            rejected_write_view.data != nullptr ||
            rejected_summary.plane != -1
            ) {
            return fail();
        }
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    cnr3_cache_core_selftest_trace_line("P.10A VapourSynth plane-view adapter proof scenario");
    cnr3_cache_core_selftest_trace_line("    VSAPI getFrameWidth/getFrameHeight/getStride/getReadPtr build read native byte views");
    cnr3_cache_core_selftest_trace_line("    VSAPI getWritePtr builds write native byte views without calling getReadPtr again");
    cnr3_cache_core_selftest_trace_line("    10-bit read/write vectors prove VS stride and x*storage_bytes composition");
    cnr3_cache_core_selftest_trace_line("    two-byte VS strides must be storage-byte aligned before a view is published");
    cnr3_cache_core_selftest_trace_line("    source-frame lifecycle, predecessor acquisition, scene-change, and getFrame/cache integration remain deferred");

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_caller_supplied_frame_triplet_view_proof() noexcept {
    /*
        P.11A validates caller-supplied frame triplets only. It must not decide
        source-frame lifecycle, predecessor acquisition, cache lookup, or getFrame
        scheduling.
    */
    constexpr int bits_10 = 10;
    constexpr int sub_sampling_w = 1;
    constexpr int sub_sampling_h = 1;
    constexpr int luma_width = 4;
    constexpr int luma_height = 2;
    constexpr int chroma_width = 2;
    constexpr int chroma_height = 1;
    constexpr int luma_stride = 10;
    constexpr int chroma_stride = 6;

    int current_source_storage = 10;
    int previous_filtered_storage = 20;
    int destination_storage = 30;
    const VSFrame* current_source_frame =
        reinterpret_cast<const VSFrame*>(&current_source_storage);
    const VSFrame* previous_filtered_frame =
        reinterpret_cast<const VSFrame*>(&previous_filtered_storage);
    VSFrame* destination_frame = reinterpret_cast<VSFrame*>(&destination_storage);

    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    vsapi_state.fake_frames[0].frame = current_source_frame;
    vsapi_state.fake_frames[1].frame = previous_filtered_frame;
    vsapi_state.fake_frames[2].frame = destination_frame;
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto fail = [&]() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    const auto summary_matches = [](
        const Cnr3VapourSynthFrameTripletViewSummary& summary,
        int expected_bits,
        int expected_storage_bytes,
        int expected_subw,
        int expected_subh,
        int expected_luma_width,
        int expected_luma_height,
        int expected_chroma_width,
        int expected_chroma_height
    ) noexcept -> bool {
        return summary.bits_per_sample == expected_bits &&
            summary.storage_bytes == expected_storage_bytes &&
            summary.sub_sampling_w == expected_subw &&
            summary.sub_sampling_h == expected_subh &&
            summary.luma_width == expected_luma_width &&
            summary.luma_height == expected_luma_height &&
            summary.chroma_width == expected_chroma_width &&
            summary.chroma_height == expected_chroma_height &&
            summary.current_source_views_created &&
            summary.previous_filtered_views_created &&
            summary.destination_views_created &&
            summary.triplet_views_created;
    };

    const auto triplet_views_are_clear = [](
        const Cnr3VapourSynthFrameTripletNativeViews& views
    ) noexcept -> bool {
        return views.current_source_y.data == nullptr &&
            views.current_source_u.data == nullptr &&
            views.current_source_v.data == nullptr &&
            views.previous_filtered_y.data == nullptr &&
            views.previous_filtered_u.data == nullptr &&
            views.previous_filtered_v.data == nullptr &&
            views.destination_y.data == nullptr &&
            views.destination_u.data == nullptr &&
            views.destination_v.data == nullptr;
    };

    const auto summary_is_clear = [](
        const Cnr3VapourSynthFrameTripletViewSummary& summary
    ) noexcept -> bool {
        return summary.bits_per_sample == 0 &&
            summary.storage_bytes == 0 &&
            summary.sub_sampling_w == -1 &&
            summary.sub_sampling_h == -1 &&
            summary.luma_width == 0 &&
            summary.luma_height == 0 &&
            summary.chroma_width == 0 &&
            summary.chroma_height == 0 &&
            !summary.current_source_views_created &&
            !summary.previous_filtered_views_created &&
            !summary.destination_views_created &&
            !summary.triplet_views_created;
    };

    std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA0U);
    std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xA1U);
    std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xA2U);
    std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xB0U);
    std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB1U);
    std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB2U);
    std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xC0U);
    std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC1U);
    std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC2U);

    cnr3_cache_core_selftest_write_u16_sample(current_y.data(), 12, 111U);
    cnr3_cache_core_selftest_write_u16_sample(previous_u.data(), 2, 222U);

    const auto configure_read_frame = [&](
        Cnr3CacheCoreSelftestVsFrameState& frame_state,
        const std::vector<std::uint8_t>& y_plane,
        const std::vector<std::uint8_t>& u_plane,
        const std::vector<std::uint8_t>& v_plane
    ) noexcept {
        frame_state.planes[0].width = luma_width;
        frame_state.planes[0].height = luma_height;
        frame_state.planes[0].stride = luma_stride;
        frame_state.planes[0].read_ptr = y_plane.data();

        frame_state.planes[1].width = chroma_width;
        frame_state.planes[1].height = chroma_height;
        frame_state.planes[1].stride = chroma_stride;
        frame_state.planes[1].read_ptr = u_plane.data();

        frame_state.planes[2].width = chroma_width;
        frame_state.planes[2].height = chroma_height;
        frame_state.planes[2].stride = chroma_stride;
        frame_state.planes[2].read_ptr = v_plane.data();
    };

    const auto configure_write_frame = [&](
        Cnr3CacheCoreSelftestVsFrameState& frame_state,
        std::vector<std::uint8_t>& y_plane,
        std::vector<std::uint8_t>& u_plane,
        std::vector<std::uint8_t>& v_plane
    ) noexcept {
        frame_state.planes[0].width = luma_width;
        frame_state.planes[0].height = luma_height;
        frame_state.planes[0].stride = luma_stride;
        frame_state.planes[0].write_ptr = y_plane.data();

        frame_state.planes[1].width = chroma_width;
        frame_state.planes[1].height = chroma_height;
        frame_state.planes[1].stride = chroma_stride;
        frame_state.planes[1].write_ptr = u_plane.data();

        frame_state.planes[2].width = chroma_width;
        frame_state.planes[2].height = chroma_height;
        frame_state.planes[2].stride = chroma_stride;
        frame_state.planes[2].write_ptr = v_plane.data();
    };

    configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v);
    configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v);
    configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v);

    Cnr3VapourSynthFrameTripletNativeViews views{};
    Cnr3VapourSynthFrameTripletViewSummary summary{};

    if (
        cnr3_make_caller_supplied_vapoursynth_frame_triplet_views(
            current_source_frame,
            previous_filtered_frame,
            destination_frame,
            &vsapi,
            bits_10,
            sub_sampling_w,
            sub_sampling_h,
            views,
            summary
        ) != Cnr3Status::ok ||
        views.current_source_y.data != current_y.data() ||
        views.previous_filtered_u.data != previous_u.data() ||
        views.destination_v.data != destination_v.data() ||
        !summary_matches(
            summary,
            bits_10,
            2,
            sub_sampling_w,
            sub_sampling_h,
            luma_width,
            luma_height,
            chroma_width,
            chroma_height
        )
        ) {
        return fail();
    }

    int current_y_sample = -1;
    int previous_u_sample = -1;
    const Cnr3ConstNativePlaneByteView destination_v_readback{
        destination_v.data(),
        chroma_width,
        chroma_height,
        chroma_stride,
        bits_10
    };
    int destination_v_sample = -1;

    if (
        cnr3_load_native_plane_sample(
            views.current_source_y,
            1,
            1,
            current_y_sample
        ) != Cnr3Status::ok ||
        current_y_sample != 111 ||
        cnr3_load_native_plane_sample(
            views.previous_filtered_u,
            1,
            0,
            previous_u_sample
        ) != Cnr3Status::ok ||
        previous_u_sample != 222 ||
        cnr3_store_native_plane_sample(
            views.destination_v,
            1,
            0,
            333
        ) != Cnr3Status::ok ||
        cnr3_load_native_plane_sample(
            destination_v_readback,
            1,
            0,
            destination_v_sample
        ) != Cnr3Status::ok ||
        destination_v_sample != 333 ||
        vsapi_state.get_read_ptr_count != 6 ||
        vsapi_state.get_write_ptr_count != 3
        ) {
        return fail();
    }

    {
        Cnr3VapourSynthFrameTripletNativeViews rejected_views{};
        rejected_views.current_source_y.data = reinterpret_cast<const void*>(0x1);
        rejected_views.previous_filtered_y.data = reinterpret_cast<const void*>(0x2);
        rejected_views.destination_y.data = reinterpret_cast<void*>(0x3);
        Cnr3VapourSynthFrameTripletViewSummary rejected_summary{};
        rejected_summary.bits_per_sample = 99;
        rejected_summary.sub_sampling_w = 9;

        vsapi_state.fake_frames[1].planes[2].height = 2;

        if (
            cnr3_make_caller_supplied_vapoursynth_frame_triplet_views(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_10,
                sub_sampling_w,
                sub_sampling_h,
                rejected_views,
                rejected_summary
            ) != Cnr3Status::invalid_argument ||
            !triplet_views_are_clear(rejected_views) ||
            !summary_is_clear(rejected_summary)
            ) {
            return fail();
        }

        vsapi_state.fake_frames[1].planes[2].height = chroma_height;
    }

    {
        Cnr3VapourSynthFrameTripletNativeViews rejected_views{};
        Cnr3VapourSynthFrameTripletViewSummary rejected_summary{};

        vsapi_state.fake_frames[0].planes[0].width = 5;

        if (
            cnr3_make_caller_supplied_vapoursynth_frame_triplet_views(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_10,
                sub_sampling_w,
                sub_sampling_h,
                rejected_views,
                rejected_summary
            ) != Cnr3Status::invalid_argument ||
            !triplet_views_are_clear(rejected_views) ||
            !summary_is_clear(rejected_summary)
            ) {
            return fail();
        }

        vsapi_state.fake_frames[0].planes[0].width = luma_width;
    }

    {
        Cnr3VapourSynthFrameTripletNativeViews rejected_views{};
        Cnr3VapourSynthFrameTripletViewSummary rejected_summary{};

        if (
            cnr3_make_caller_supplied_vapoursynth_frame_triplet_views(
                current_source_frame,
                nullptr,
                destination_frame,
                &vsapi,
                bits_10,
                sub_sampling_w,
                sub_sampling_h,
                rejected_views,
                rejected_summary
            ) != Cnr3Status::invalid_argument ||
            !triplet_views_are_clear(rejected_views) ||
            !summary_is_clear(rejected_summary)
            ) {
            return fail();
        }
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    cnr3_cache_core_selftest_trace_line("P.11A caller-supplied frame-triplet view proof scenario");
    cnr3_cache_core_selftest_trace_line("    current source, previous filtered output, and destination VSFrames are caller supplied");
    cnr3_cache_core_selftest_trace_line("    triplet validation composes P.10A read/write plane adapters without lifecycle decisions");
    cnr3_cache_core_selftest_trace_line("    4:2:0 real-frame dimensions require exact luma divisibility and matching U/V triplets");
    cnr3_cache_core_selftest_trace_line("    stale triplet views are cleared on plane mismatch or invalid real-frame dimensions");
    cnr3_cache_core_selftest_trace_line("    source-frame lifecycle, predecessor sourcing, scene-change, pixel composition, and getFrame/cache integration remain deferred");

    return Cnr3Status::ok;
}


Cnr3Status cnr3_cache_core_selftest_caller_supplied_real_frame_pixel_composition_proof() noexcept {
    /*
        P.11B proves real-frame pixel composition only for frames supplied by
        the caller. It must not request, retrieve, acquire, cache, recover, or
        return frames, and it must write destination Y/U/V only after all staging
        succeeds.
    */
    constexpr int bits_8 = 8;
    constexpr int bits_10 = 10;
    constexpr int sub_sampling_w = 1;
    constexpr int sub_sampling_h = 1;
    constexpr int luma_width = 4;
    constexpr int luma_height = 4;
    constexpr int chroma_width = 2;
    constexpr int chroma_height = 2;

    int current_source_storage = 11;
    int previous_filtered_storage = 12;
    int destination_storage = 13;

    const VSFrame* current_source_frame = reinterpret_cast<const VSFrame*>(&current_source_storage);
    const VSFrame* previous_filtered_frame = reinterpret_cast<const VSFrame*>(&previous_filtered_storage);
    VSFrame* destination_frame = reinterpret_cast<VSFrame*>(&destination_storage);

    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    vsapi_state.fake_frames[0].frame = current_source_frame;
    vsapi_state.fake_frames[1].frame = previous_filtered_frame;
    vsapi_state.fake_frames[2].frame = destination_frame;
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto fail = [&]() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    const auto make_tables = [](
        int bits_per_sample,
        int y_signed_diff,
        int u_signed_diff,
        int v_signed_diff
    ) noexcept -> Cnr3ResponseTables {
        const int sample_peak = (1 << bits_per_sample) - 1;
        Cnr3ResponseTables tables{};
        tables.sample_peak = sample_peak;

        if (
            cnr3_response_table_geometry_for_sample_peak(
                sample_peak,
                tables.table_offset,
                tables.table_size
            ) != Cnr3Status::ok
            ) {
            return tables;
        }

        try {
            tables.y.assign(static_cast<std::size_t>(tables.table_size), 0);
            tables.u.assign(static_cast<std::size_t>(tables.table_size), 0);
            tables.v.assign(static_cast<std::size_t>(tables.table_size), 0);
        } catch (...) {
            return Cnr3ResponseTables{};
        }

        tables.y[static_cast<std::size_t>(tables.table_offset + y_signed_diff)] =
            sample_peak;
        tables.u[static_cast<std::size_t>(tables.table_offset + u_signed_diff)] =
            sample_peak;
        tables.v[static_cast<std::size_t>(tables.table_offset + v_signed_diff)] =
            sample_peak;
        return tables;
    };

    const auto expected_blend = [](
        int bits_per_sample,
        int current_sample,
        int previous_sample
    ) noexcept -> int {
        const int sample_peak = (1 << bits_per_sample) - 1;
        const std::int64_t weight =
            static_cast<std::int64_t>(sample_peak) * static_cast<std::int64_t>(sample_peak);
        const int shift2 = bits_per_sample << 1;
        const std::int64_t shift = std::int64_t{1} << shift2;
        const std::int64_t shift1 = shift >> 1;

        return static_cast<int>(
            (
                (weight * static_cast<std::int64_t>(previous_sample)) +
                ((shift - weight) * static_cast<std::int64_t>(current_sample)) +
                shift1
            ) >> shift2
        );
    };

    const auto configure_read_frame = [](
        Cnr3CacheCoreSelftestVsFrameState& frame_state,
        const std::vector<std::uint8_t>& y_plane,
        const std::vector<std::uint8_t>& u_plane,
        const std::vector<std::uint8_t>& v_plane,
        int luma_stride,
        int chroma_stride
    ) noexcept {
        frame_state.planes[0].width = luma_width;
        frame_state.planes[0].height = luma_height;
        frame_state.planes[0].stride = luma_stride;
        frame_state.planes[0].read_ptr = y_plane.data();

        frame_state.planes[1].width = chroma_width;
        frame_state.planes[1].height = chroma_height;
        frame_state.planes[1].stride = chroma_stride;
        frame_state.planes[1].read_ptr = u_plane.data();

        frame_state.planes[2].width = chroma_width;
        frame_state.planes[2].height = chroma_height;
        frame_state.planes[2].stride = chroma_stride;
        frame_state.planes[2].read_ptr = v_plane.data();
    };

    const auto configure_write_frame = [](
        Cnr3CacheCoreSelftestVsFrameState& frame_state,
        std::vector<std::uint8_t>& y_plane,
        std::vector<std::uint8_t>& u_plane,
        std::vector<std::uint8_t>& v_plane,
        int luma_stride,
        int chroma_stride
    ) noexcept {
        frame_state.planes[0].width = luma_width;
        frame_state.planes[0].height = luma_height;
        frame_state.planes[0].stride = luma_stride;
        frame_state.planes[0].write_ptr = y_plane.data();

        frame_state.planes[1].width = chroma_width;
        frame_state.planes[1].height = chroma_height;
        frame_state.planes[1].stride = chroma_stride;
        frame_state.planes[1].write_ptr = u_plane.data();

        frame_state.planes[2].width = chroma_width;
        frame_state.planes[2].height = chroma_height;
        frame_state.planes[2].stride = chroma_stride;
        frame_state.planes[2].write_ptr = v_plane.data();
    };

    const auto active_padding_is = [](
        const std::vector<std::uint8_t>& bytes,
        int width,
        int height,
        int stride_bytes,
        int storage_bytes,
        std::uint8_t sentinel
    ) noexcept -> bool {
        const int active_row_bytes = width * storage_bytes;

        for (int y = 0; y < height; ++y) {
            for (int byte_index = active_row_bytes; byte_index < stride_bytes; ++byte_index) {
                if (bytes[static_cast<std::size_t>((y * stride_bytes) + byte_index)] != sentinel) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto all_bytes_are = [](
        const std::vector<std::uint8_t>& bytes,
        std::uint8_t value
    ) noexcept -> bool {
        for (std::uint8_t byte : bytes) {
            if (byte != value) {
                return false;
            }
        }

        return true;
    };

    const auto fill_u8_active = [](
        std::vector<std::uint8_t>& bytes,
        int width,
        int height,
        int stride_bytes,
        std::uint8_t value
    ) noexcept {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bytes[static_cast<std::size_t>((y * stride_bytes) + x)] = value;
            }
        }
    };

    const auto fill_u8_luma = [](
        std::vector<std::uint8_t>& bytes,
        int stride_bytes
    ) noexcept {
        for (int y = 0; y < luma_height; ++y) {
            for (int x = 0; x < luma_width; ++x) {
                bytes[static_cast<std::size_t>((y * stride_bytes) + x)] =
                    static_cast<std::uint8_t>((y * 10) + x);
            }
        }
    };

    const auto put_u16_active = [](
        std::vector<std::uint8_t>& bytes,
        int stride_bytes,
        int width,
        int height,
        std::uint16_t value
    ) noexcept {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_bytes)) +
                    (static_cast<std::size_t>(x) * 2U);
                cnr3_cache_core_selftest_write_u16_sample(bytes.data(), static_cast<int>(offset), value);
            }
        }
    };

    const auto get_u16_active = [](
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

    const auto fill_u16_luma = [&put_u16_active](
        std::vector<std::uint8_t>& bytes,
        int stride_bytes
    ) noexcept {
        for (int y = 0; y < luma_height; ++y) {
            for (int x = 0; x < luma_width; ++x) {
                const int value = (y * 10) + x;
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_bytes)) +
                    (static_cast<std::size_t>(x) * 2U);
                cnr3_cache_core_selftest_write_u16_sample(
                    bytes.data(),
                    static_cast<int>(offset),
                    static_cast<std::uint16_t>(value)
                );
            }
        }
    };

    {
        constexpr int luma_stride = 6;
        constexpr int chroma_stride = 4;
        constexpr int current_chroma_u = 120;
        constexpr int filtered_chroma_u = 20;
        constexpr int decoy_source_previous_u = 220;
        constexpr int current_chroma_v = 130;
        constexpr int filtered_chroma_v = 30;
        constexpr int decoy_source_previous_v = 230;
        constexpr int expected_u = 21;
        constexpr int expected_v = 31;
        constexpr int decoy_u = 219;
        constexpr int decoy_v = 229;

        static_assert(expected_u == 21);
        static_assert(expected_v == 31);
        static_assert(decoy_u == 219);
        static_assert(decoy_v == 229);

        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEEU);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEDU);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xECU);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEBU);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEAU);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE9U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA5U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB5U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC5U);

        fill_u8_luma(current_y, luma_stride);
        fill_u8_luma(previous_y, luma_stride);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, current_chroma_u);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, current_chroma_v);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, filtered_chroma_u);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, filtered_chroma_v);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v, luma_stride, chroma_stride);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v, luma_stride, chroma_stride);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v, luma_stride, chroma_stride);

        const Cnr3ResponseTables tables = make_tables(bits_8, 0, 100, 100);
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                summary
            ) != Cnr3Status::ok ||
            summary.luma_samples_copied != 16 ||
            summary.chroma_u_samples_processed != 4 ||
            summary.chroma_v_samples_processed != 4 ||
            summary.first_u_output_sample != expected_u ||
            summary.last_u_output_sample != expected_u ||
            summary.first_v_output_sample != expected_v ||
            summary.last_v_output_sample != expected_v ||
            !summary.memcpy_byte_view_path_used ||
            !summary.typed_row_pointer_optimization_deferred ||
            !summary.frame_processed ||
            expected_blend(bits_8, current_chroma_u, filtered_chroma_u) != expected_u ||
            expected_blend(bits_8, current_chroma_u, decoy_source_previous_u) != decoy_u ||
            expected_u == decoy_u ||
            expected_blend(bits_8, current_chroma_v, filtered_chroma_v) != expected_v ||
            expected_blend(bits_8, current_chroma_v, decoy_source_previous_v) != decoy_v ||
            expected_v == decoy_v
            ) {
            return fail();
        }

        for (int y = 0; y < luma_height; ++y) {
            for (int x = 0; x < luma_width; ++x) {
                const std::uint8_t expected = static_cast<std::uint8_t>((y * 10) + x);
                if (destination_y[static_cast<std::size_t>((y * luma_stride) + x)] != expected) {
                    return fail();
                }
            }
        }

        for (int y = 0; y < chroma_height; ++y) {
            for (int x = 0; x < chroma_width; ++x) {
                if (
                    destination_u[static_cast<std::size_t>((y * chroma_stride) + x)] != expected_u ||
                    destination_v[static_cast<std::size_t>((y * chroma_stride) + x)] != expected_v
                    ) {
                    return fail();
                }
            }
        }

        if (
            !active_padding_is(destination_y, luma_width, luma_height, luma_stride, 1, 0xA5U) ||
            !active_padding_is(destination_u, chroma_width, chroma_height, chroma_stride, 1, 0xB5U) ||
            !active_padding_is(destination_v, chroma_width, chroma_height, chroma_stride, 1, 0xC5U)
            ) {
            return fail();
        }
    }

    {
        constexpr int luma_stride = 12;
        constexpr int chroma_stride = 6;
        constexpr int current_chroma_u = 120;
        constexpr int filtered_chroma_u = 20;
        constexpr int current_chroma_v = 130;
        constexpr int filtered_chroma_v = 30;
        constexpr int expected_u = 20;
        constexpr int expected_v = 30;

        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEEU);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEDU);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xECU);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEBU);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEAU);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE9U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA6U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB6U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC6U);

        fill_u16_luma(current_y, luma_stride);
        fill_u16_luma(previous_y, luma_stride);
        put_u16_active(current_u, chroma_stride, chroma_width, chroma_height, current_chroma_u);
        put_u16_active(current_v, chroma_stride, chroma_width, chroma_height, current_chroma_v);
        put_u16_active(previous_u, chroma_stride, chroma_width, chroma_height, filtered_chroma_u);
        put_u16_active(previous_v, chroma_stride, chroma_width, chroma_height, filtered_chroma_v);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v, luma_stride, chroma_stride);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v, luma_stride, chroma_stride);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v, luma_stride, chroma_stride);

        const Cnr3ResponseTables tables = make_tables(bits_10, 0, 100, 100);
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_10,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                summary
            ) != Cnr3Status::ok ||
            summary.storage_bytes != 2 ||
            summary.first_u_output_sample != expected_u ||
            summary.first_v_output_sample != expected_v ||
            get_u16_active(destination_y, luma_stride, 3, 2) != 23 ||
            get_u16_active(destination_u, chroma_stride, 0, 0) != expected_u ||
            get_u16_active(destination_u, chroma_stride, 1, 1) != expected_u ||
            get_u16_active(destination_v, chroma_stride, 0, 0) != expected_v ||
            get_u16_active(destination_v, chroma_stride, 1, 1) != expected_v ||
            !active_padding_is(destination_y, luma_width, luma_height, luma_stride, 2, 0xA6U) ||
            !active_padding_is(destination_u, chroma_width, chroma_height, chroma_stride, 2, 0xB6U) ||
            !active_padding_is(destination_v, chroma_width, chroma_height, chroma_stride, 2, 0xC6U)
            ) {
            return fail();
        }
    }

    {
        constexpr int luma_stride = 12;
        constexpr int chroma_stride = 6;
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEEU);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEDU);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xECU);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEBU);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEAU);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE9U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA7U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB7U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC7U);

        fill_u16_luma(current_y, luma_stride);
        fill_u16_luma(previous_y, luma_stride);
        put_u16_active(current_u, chroma_stride, chroma_width, chroma_height, 120U);
        put_u16_active(current_v, chroma_stride, chroma_width, chroma_height, 130U);
        put_u16_active(previous_u, chroma_stride, chroma_width, chroma_height, 20U);
        put_u16_active(previous_v, chroma_stride, chroma_width, chroma_height, 30U);
        cnr3_cache_core_selftest_write_u16_sample(current_u.data(), 8, 2048U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v, luma_stride, chroma_stride);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v, luma_stride, chroma_stride);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v, luma_stride, chroma_stride);

        const Cnr3ResponseTables tables = make_tables(bits_10, 0, 100, 100);
        Cnr3CallerSuppliedFrameProcessSummary summary{};
        summary.frame_processed = true;
        summary.luma_samples_copied = 99;

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_10,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                summary
            ) != Cnr3Status::invalid_argument ||
            !all_bytes_are(destination_y, 0xA7U) ||
            !all_bytes_are(destination_u, 0xB7U) ||
            !all_bytes_are(destination_v, 0xC7U) ||
            summary.frame_processed ||
            summary.luma_samples_copied != 0
            ) {
            return fail();
        }
    }

    {
        constexpr int luma_stride = 6;
        constexpr int chroma_stride = 4;
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEEU);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEDU);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xECU);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEBU);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEAU);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE9U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA8U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB8U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC8U);

        fill_u8_luma(current_y, luma_stride);
        fill_u8_luma(previous_y, luma_stride);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, 120U);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, 130U);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, 20U);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, 30U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v, luma_stride, chroma_stride);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v, luma_stride, chroma_stride);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v, luma_stride, chroma_stride);

        Cnr3ResponseTables bad_tables = make_tables(bits_8, 0, 100, 100);
        bad_tables.table_offset = 0;
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                bad_tables,
                summary
            ) != Cnr3Status::invalid_argument ||
            !all_bytes_are(destination_y, 0xA8U) ||
            !all_bytes_are(destination_u, 0xB8U) ||
            !all_bytes_are(destination_v, 0xC8U) ||
            summary.frame_processed
            ) {
            return fail();
        }
    }

    {
        constexpr int luma_stride = 6;
        constexpr int chroma_stride = 4;
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEEU);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xEDU);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xECU);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEBU);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xEAU);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE9U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA9U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB9U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC9U);

        fill_u8_luma(current_y, luma_stride);
        fill_u8_luma(previous_y, luma_stride);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, 120U);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, 130U);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, 20U);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, 30U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v, luma_stride, chroma_stride);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v, luma_stride, chroma_stride);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v, luma_stride, chroma_stride);
        vsapi_state.fake_frames[1].planes[1].width = 3;

        const Cnr3ResponseTables tables = make_tables(bits_8, 0, 100, 100);
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                summary
            ) != Cnr3Status::invalid_argument ||
            !all_bytes_are(destination_y, 0xA9U) ||
            !all_bytes_are(destination_u, 0xB9U) ||
            !all_bytes_are(destination_v, 0xC9U) ||
            summary.frame_processed
            ) {
            return fail();
        }

        vsapi_state.fake_frames[1].planes[1].width = chroma_width;
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    cnr3_cache_core_selftest_trace_line("P.11B caller-supplied real-frame pixel-composition proof scenario");
    cnr3_cache_core_selftest_trace_line("    caller-supplied frame triplet composes P.11A views with the proven pixel pipeline");
    cnr3_cache_core_selftest_trace_line("    output luma is staged and copied unchanged from current source luma");
    cnr3_cache_core_selftest_trace_line("    chroma U/V are blended against previous filtered output frame planes");
    cnr3_cache_core_selftest_trace_line("    predecessor semantics vector proves previous filtered output is used, not source[N-1]");
    cnr3_cache_core_selftest_trace_line("    two-byte samples retain the P.8A memcpy path; typed row-pointer optimization is deferred to fmParallel measurement");
    cnr3_cache_core_selftest_trace_line("    invalid late pixel/response-table proofs publish no partial destination frame");
    cnr3_cache_core_selftest_trace_line("    source-frame lifecycle, predecessor sourcing, scene-change, and getFrame/cache integration remain deferred");

    return Cnr3Status::ok;
}



Cnr3Status cnr3_cache_core_selftest_caller_supplied_scene_change_reset_proof() noexcept {
    /*
        P.11C proves scene-change/reset on the caller-supplied pixel path only.
        The integer threshold is supplied by the caller; instance-option parsing
        and getFrame/cache consequences remain later integration work.
    */
    constexpr int bits_8 = 8;
    constexpr int sub_sampling_w = 1;
    constexpr int sub_sampling_h = 1;
    constexpr int luma_width = 4;
    constexpr int luma_height = 4;
    constexpr int chroma_width = 2;
    constexpr int chroma_height = 2;
    constexpr int luma_stride = 6;
    constexpr int chroma_stride = 4;

    int current_source_storage = 21;
    int previous_filtered_storage = 22;
    int destination_storage = 23;

    const VSFrame* current_source_frame = reinterpret_cast<const VSFrame*>(&current_source_storage);
    const VSFrame* previous_filtered_frame = reinterpret_cast<const VSFrame*>(&previous_filtered_storage);
    VSFrame* destination_frame = reinterpret_cast<VSFrame*>(&destination_storage);

    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    vsapi_state.fake_frames[0].frame = current_source_frame;
    vsapi_state.fake_frames[1].frame = previous_filtered_frame;
    vsapi_state.fake_frames[2].frame = destination_frame;
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    const auto fail = [&]() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    const auto make_tables = [](
        int bits_per_sample,
        int y_signed_diff,
        int u_signed_diff,
        int v_signed_diff
    ) noexcept -> Cnr3ResponseTables {
        const int sample_peak = (1 << bits_per_sample) - 1;
        Cnr3ResponseTables tables{};
        tables.sample_peak = sample_peak;

        if (
            cnr3_response_table_geometry_for_sample_peak(
                sample_peak,
                tables.table_offset,
                tables.table_size
            ) != Cnr3Status::ok
            ) {
            return tables;
        }

        try {
            tables.y.assign(static_cast<std::size_t>(tables.table_size), 0);
            tables.u.assign(static_cast<std::size_t>(tables.table_size), 0);
            tables.v.assign(static_cast<std::size_t>(tables.table_size), 0);
        } catch (...) {
            return Cnr3ResponseTables{};
        }

        tables.y[static_cast<std::size_t>(tables.table_offset + y_signed_diff)] =
            sample_peak;
        tables.u[static_cast<std::size_t>(tables.table_offset + u_signed_diff)] =
            sample_peak;
        tables.v[static_cast<std::size_t>(tables.table_offset + v_signed_diff)] =
            sample_peak;
        return tables;
    };

    const auto expected_blend = [](
        int bits_per_sample,
        int current_sample,
        int previous_sample
    ) noexcept -> int {
        const int sample_peak = (1 << bits_per_sample) - 1;
        const std::int64_t weight =
            static_cast<std::int64_t>(sample_peak) * static_cast<std::int64_t>(sample_peak);
        const int shift2 = bits_per_sample << 1;
        const std::int64_t shift = std::int64_t{1} << shift2;
        const std::int64_t shift1 = shift >> 1;

        return static_cast<int>(
            (
                (weight * static_cast<std::int64_t>(previous_sample)) +
                ((shift - weight) * static_cast<std::int64_t>(current_sample)) +
                shift1
            ) >> shift2
        );
    };

    const auto configure_read_frame = [](
        Cnr3CacheCoreSelftestVsFrameState& frame_state,
        const std::vector<std::uint8_t>& y_plane,
        const std::vector<std::uint8_t>& u_plane,
        const std::vector<std::uint8_t>& v_plane
    ) noexcept {
        frame_state.planes[0].width = luma_width;
        frame_state.planes[0].height = luma_height;
        frame_state.planes[0].stride = luma_stride;
        frame_state.planes[0].read_ptr = y_plane.data();

        frame_state.planes[1].width = chroma_width;
        frame_state.planes[1].height = chroma_height;
        frame_state.planes[1].stride = chroma_stride;
        frame_state.planes[1].read_ptr = u_plane.data();

        frame_state.planes[2].width = chroma_width;
        frame_state.planes[2].height = chroma_height;
        frame_state.planes[2].stride = chroma_stride;
        frame_state.planes[2].read_ptr = v_plane.data();
    };

    const auto configure_write_frame = [](
        Cnr3CacheCoreSelftestVsFrameState& frame_state,
        std::vector<std::uint8_t>& y_plane,
        std::vector<std::uint8_t>& u_plane,
        std::vector<std::uint8_t>& v_plane
    ) noexcept {
        frame_state.planes[0].width = luma_width;
        frame_state.planes[0].height = luma_height;
        frame_state.planes[0].stride = luma_stride;
        frame_state.planes[0].write_ptr = y_plane.data();

        frame_state.planes[1].width = chroma_width;
        frame_state.planes[1].height = chroma_height;
        frame_state.planes[1].stride = chroma_stride;
        frame_state.planes[1].write_ptr = u_plane.data();

        frame_state.planes[2].width = chroma_width;
        frame_state.planes[2].height = chroma_height;
        frame_state.planes[2].stride = chroma_stride;
        frame_state.planes[2].write_ptr = v_plane.data();
    };

    const auto fill_u8_active = [](
        std::vector<std::uint8_t>& plane,
        int width,
        int height,
        int stride,
        std::uint8_t value
    ) noexcept {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                plane[static_cast<std::size_t>((y * stride) + x)] = value;
            }
        }
    };

    const auto active_u8_sample = [](
        const std::vector<std::uint8_t>& plane,
        int stride,
        int x,
        int y
    ) noexcept -> int {
        return static_cast<int>(plane[static_cast<std::size_t>((y * stride) + x)]);
    };

    const auto padding_is = [](
        const std::vector<std::uint8_t>& plane,
        int width,
        int height,
        int stride,
        std::uint8_t expected
    ) noexcept -> bool {
        for (int y = 0; y < height; ++y) {
            for (int x = width; x < stride; ++x) {
                if (plane[static_cast<std::size_t>((y * stride) + x)] != expected) {
                    return false;
                }
            }
        }

        return true;
    };

    const auto all_bytes_are = [](
        const std::vector<std::uint8_t>& plane,
        std::uint8_t expected
    ) noexcept -> bool {
        for (const std::uint8_t value : plane) {
            if (value != expected) {
                return false;
            }
        }

        return true;
    };

    {
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE1U);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE2U);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE3U);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE4U);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE5U);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE6U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA1U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB1U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC1U);

        fill_u8_active(current_y, luma_width, luma_height, luma_stride, 100U);
        fill_u8_active(previous_y, luma_width, luma_height, luma_stride, 50U);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, 120U);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, 130U);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, 20U);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, 30U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v);

        const Cnr3ResponseTables tables = make_tables(bits_8, 50, 100, 100);
        const Cnr3SceneChangeConfig scene_config{199, false};
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                scene_config,
                summary
            ) != Cnr3Status::ok ||
            !summary.frame_processed ||
            !summary.scene_change_detection_used ||
            summary.scene_chroma_used ||
            !summary.scene_change_detected ||
            !summary.scene_change_reset_output_used ||
            summary.recursive_chroma_blend_used ||
            summary.scene_change_threshold != 199 ||
            summary.scene_change_diff_total != 200 ||
            summary.scene_change_samples_examined != 1 ||
            active_u8_sample(destination_y, luma_stride, 0, 0) != 100 ||
            active_u8_sample(destination_u, chroma_stride, 0, 0) != 120 ||
            active_u8_sample(destination_v, chroma_stride, 0, 0) != 130 ||
            active_u8_sample(destination_u, chroma_stride, 1, 1) != 120 ||
            active_u8_sample(destination_v, chroma_stride, 1, 1) != 130 ||
            !padding_is(destination_y, luma_width, luma_height, luma_stride, 0xA1U) ||
            !padding_is(destination_u, chroma_width, chroma_height, chroma_stride, 0xB1U) ||
            !padding_is(destination_v, chroma_width, chroma_height, chroma_stride, 0xC1U)
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE1U);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE2U);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE3U);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE4U);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE5U);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE6U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA2U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB2U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC2U);

        fill_u8_active(current_y, luma_width, luma_height, luma_stride, 100U);
        fill_u8_active(previous_y, luma_width, luma_height, luma_stride, 50U);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, 120U);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, 130U);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, 20U);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, 30U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v);

        const Cnr3ResponseTables tables = make_tables(bits_8, 50, 100, 100);
        const Cnr3SceneChangeConfig scene_config{800, false};
        Cnr3CallerSuppliedFrameProcessSummary summary{};
        const int expected_u = expected_blend(bits_8, 120, 20);
        const int expected_v = expected_blend(bits_8, 130, 30);

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                scene_config,
                summary
            ) != Cnr3Status::ok ||
            !summary.frame_processed ||
            !summary.scene_change_detection_used ||
            summary.scene_change_detected ||
            summary.scene_change_reset_output_used ||
            !summary.recursive_chroma_blend_used ||
            summary.scene_change_diff_total != 800 ||
            summary.scene_change_samples_examined != 4 ||
            active_u8_sample(destination_u, chroma_stride, 0, 0) != expected_u ||
            active_u8_sample(destination_v, chroma_stride, 0, 0) != expected_v
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE1U);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE2U);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE3U);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE4U);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE5U);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE6U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA3U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB3U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC3U);

        fill_u8_active(current_y, luma_width, luma_height, luma_stride, 90U);
        fill_u8_active(previous_y, luma_width, luma_height, luma_stride, 90U);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, 180U);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, 170U);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, 40U);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, 30U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v);

        const Cnr3ResponseTables tables = make_tables(bits_8, 0, 140, 140);
        const Cnr3SceneChangeConfig scene_config{139, true};
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                scene_config,
                summary
            ) != Cnr3Status::ok ||
            !summary.scene_chroma_used ||
            !summary.scene_change_detected ||
            summary.scene_change_diff_total != 280 ||
            summary.scene_change_samples_examined != 1 ||
            active_u8_sample(destination_u, chroma_stride, 0, 0) != 180 ||
            active_u8_sample(destination_v, chroma_stride, 0, 0) != 170
            ) {
            return fail();
        }
    }

    {
        std::vector<std::uint8_t> current_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE1U);
        std::vector<std::uint8_t> previous_y(static_cast<std::size_t>(luma_stride * luma_height), 0xE2U);
        std::vector<std::uint8_t> current_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE3U);
        std::vector<std::uint8_t> current_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE4U);
        std::vector<std::uint8_t> previous_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE5U);
        std::vector<std::uint8_t> previous_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xE6U);
        std::vector<std::uint8_t> destination_y(static_cast<std::size_t>(luma_stride * luma_height), 0xA4U);
        std::vector<std::uint8_t> destination_u(static_cast<std::size_t>(chroma_stride * chroma_height), 0xB4U);
        std::vector<std::uint8_t> destination_v(static_cast<std::size_t>(chroma_stride * chroma_height), 0xC4U);

        fill_u8_active(current_y, luma_width, luma_height, luma_stride, 100U);
        fill_u8_active(previous_y, luma_width, luma_height, luma_stride, 50U);
        fill_u8_active(current_u, chroma_width, chroma_height, chroma_stride, 120U);
        fill_u8_active(current_v, chroma_width, chroma_height, chroma_stride, 130U);
        fill_u8_active(previous_u, chroma_width, chroma_height, chroma_stride, 20U);
        fill_u8_active(previous_v, chroma_width, chroma_height, chroma_stride, 30U);

        configure_read_frame(vsapi_state.fake_frames[0], current_y, current_u, current_v);
        configure_read_frame(vsapi_state.fake_frames[1], previous_y, previous_u, previous_v);
        configure_write_frame(vsapi_state.fake_frames[2], destination_y, destination_u, destination_v);

        const Cnr3ResponseTables tables = make_tables(bits_8, 50, 100, 100);
        const Cnr3SceneChangeConfig scene_config{-1, false};
        Cnr3CallerSuppliedFrameProcessSummary summary{};

        if (
            cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
                current_source_frame,
                previous_filtered_frame,
                destination_frame,
                &vsapi,
                bits_8,
                sub_sampling_w,
                sub_sampling_h,
                tables,
                scene_config,
                summary
            ) != Cnr3Status::invalid_argument ||
            !all_bytes_are(destination_y, 0xA4U) ||
            !all_bytes_are(destination_u, 0xB4U) ||
            !all_bytes_are(destination_v, 0xC4U) ||
            summary.frame_processed
            ) {
            return fail();
        }
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    cnr3_cache_core_selftest_trace_line("P.11C caller-supplied scene-change/reset proof scenario");
    cnr3_cache_core_selftest_trace_line("    scene-change uses caller-supplied frames and an already-computed integer threshold");
    cnr3_cache_core_selftest_trace_line("    diff_total accumulates scaled downsampled-luma differences and optional chroma differences");
    cnr3_cache_core_selftest_trace_line("    strict diff_total > threshold fires reset; equality keeps recursive chroma blend active");
    cnr3_cache_core_selftest_trace_line("    scene reset stages current source chroma unchanged with all-or-nothing destination commit");
    cnr3_cache_core_selftest_trace_line("    source-frame lifecycle, threshold calculation, checkpoint promotion, and getFrame/cache integration remain deferred");

    return Cnr3Status::ok;
}



Cnr3Status cnr3_cache_core_selftest_keystone_request_plan_dev_trace_proof() noexcept {
    /*
        K.1A proves request-plan shape and temporary KDT formatting only.
        It does not request/retrieve frames, call the pixel path, prove
        predecessor pixel correctness, or connect any functional getFrame path.
    */
#if !defined(CNR3_KEYSTONE_DEV_TRACE)
    return Cnr3Status::lifecycle_violation;
#else
    const auto fail = []() noexcept -> Cnr3Status {
        return Cnr3Status::invariant_violation;
    };

    const auto text_is = [](
        const char* actual,
        const char* expected
    ) noexcept -> bool {
        return actual != nullptr &&
            expected != nullptr &&
            std::strcmp(actual, expected) == 0;
    };

    const auto vector_is = [](
        const std::vector<int>& actual,
        std::initializer_list<int> expected
    ) noexcept -> bool {
        if (actual.size() != expected.size()) {
            return false;
        }

        std::size_t index = 0U;
        for (int expected_value : expected) {
            if (actual[index] != expected_value) {
                return false;
            }
            ++index;
        }

        return true;
    };

    const auto expect_trace_line = [=](
        const Cnr3KeystoneRequestPlan& plan,
        const char* expected_line
    ) noexcept -> bool {
        char line[192] = {};
        return cnr3_keystone_format_dev_trace_line(
            plan,
            line,
            sizeof(line)
        ) == Cnr3Status::ok &&
            text_is(line, expected_line);
    };

    Cnr3KeystoneDevTraceSummary trace_summary{};
    Cnr3KeystoneRequestPlan plan{};

    plan.branch = Cnr3KeystoneRequestPlanBranch::direct_cached_output_return;
    plan.requested_frame = 58;
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(plan) != Cnr3Status::ok ||
        !plan.source_request_frame_numbers.empty() ||
        !expect_trace_line(plan, "[KDT] N=58 CACHE-HIT")
        ) {
        return fail();
    }
    cnr3_keystone_dev_trace_summary_observe_plan(plan, trace_summary);

    cnr3_keystone_request_plan_reset(plan);
    plan.branch = Cnr3KeystoneRequestPlanBranch::frame0_fresh_start;
    plan.requested_frame = 0;
    plan.floor_frame = 0;
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(plan) != Cnr3Status::ok ||
        !vector_is(plan.source_request_frame_numbers, {0}) ||
        !expect_trace_line(plan, "[KDT] N=0 FRAME0-FRESH src=1")
        ) {
        return fail();
    }
    cnr3_keystone_dev_trace_summary_observe_plan(plan, trace_summary);

    cnr3_keystone_request_plan_reset(plan);
    plan.branch = Cnr3KeystoneRequestPlanBranch::predecessor_present;
    plan.requested_frame = 43;
    plan.predecessor_frame = 42;
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(plan) != Cnr3Status::ok ||
        !vector_is(plan.source_request_frame_numbers, {43}) ||
        !expect_trace_line(plan, "[KDT] N=43 PRED-PRESENT pred=42 src=1")
        ) {
        return fail();
    }
    cnr3_keystone_dev_trace_summary_observe_plan(plan, trace_summary);

    cnr3_keystone_request_plan_reset(plan);
    plan.branch = Cnr3KeystoneRequestPlanBranch::bounded_recovery_exact_anchor;
    plan.requested_frame = 64;
    plan.floor_frame = 14;
    plan.start_point_frame = 59;
    plan.hole_frame_numbers = {60, 61, 62, 63};
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(plan) != Cnr3Status::ok ||
        !vector_is(plan.hole_frame_numbers, {60, 61, 62, 63}) ||
        !vector_is(plan.source_request_frame_numbers, {60, 61, 62, 63, 64}) ||
        !expect_trace_line(plan, "[KDT] N=64 RECOVER floor=14 anchor=59 holes=4 src=5")
        ) {
        return fail();
    }
    cnr3_keystone_dev_trace_summary_observe_plan(plan, trace_summary);

    cnr3_keystone_request_plan_reset(plan);
    plan.branch = Cnr3KeystoneRequestPlanBranch::bounded_recovery_floor_fresh_start;
    plan.requested_frame = 2000;
    plan.floor_frame = 1950;
    plan.floor_fresh_start_approximation = true;
    for (int frame_number = 1950; frame_number <= 1999; ++frame_number) {
        plan.hole_frame_numbers.push_back(frame_number);
    }
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(plan) != Cnr3Status::ok ||
        plan.hole_frame_numbers.size() != 50U ||
        plan.source_request_frame_numbers.size() != 51U ||
        plan.source_request_frame_numbers.front() != 1950 ||
        plan.source_request_frame_numbers.back() != 2000 ||
        !expect_trace_line(
            plan,
            "[KDT] N=2000 RECOVER floor=1950 anchor=FLOOR holes=50 src=51 flag=APPROX"
        )
        ) {
        return fail();
    }
    cnr3_keystone_dev_trace_summary_observe_plan(plan, trace_summary);

    cnr3_keystone_request_plan_reset(plan);
    plan.branch = Cnr3KeystoneRequestPlanBranch::hard_status;
    plan.requested_frame = 77;
    plan.hard_status = Cnr3Status::invariant_violation;
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(plan) != Cnr3Status::ok ||
        !plan.source_request_frame_numbers.empty() ||
        !expect_trace_line(plan, "[KDT] N=77 HARD-STATUS status=invariant_violation")
        ) {
        return fail();
    }
    cnr3_keystone_dev_trace_summary_observe_plan(plan, trace_summary);

    char summary_line[256] = {};
    if (
        cnr3_keystone_format_dev_trace_summary(
            trace_summary,
            summary_line,
            sizeof(summary_line)
        ) != Cnr3Status::ok ||
        !text_is(
            summary_line,
            "[KDT-SUMMARY] total=6 cache_hit=1 frame0=1 pred_present=1 exact_recovery=1 floor_reset=1 hard=1 max_span=50 floor_approx=yes"
        )
        ) {
        return fail();
    }

    cnr3_cache_core_selftest_trace_line("K.1A keystone request-plan and temporary KDT trace proof scenario");
    cnr3_cache_core_selftest_trace_line("    synthetic vectors prove plan branch shape, not predecessor pixel correctness");
    cnr3_cache_core_selftest_trace_line("    recovery request representation is holes-list/source-set, never a blanket span");
    cnr3_cache_core_selftest_trace_line("    hard-status branch is a carrier for existing C.13B guard results, not a new validator");
    cnr3_cache_core_selftest_trace_line("    [KDT] and [KDT-SUMMARY] formatting is driven by the plan structure");
    cnr3_cache_core_selftest_trace_line("    no getFrame wiring, source lifecycle, pixel-path call, cache semantic change, or VS header edit is introduced");

    return Cnr3Status::ok;

#endif
}


Cnr3Status cnr3_cache_core_selftest_keystone_direct_cached_output_return_proof() noexcept {
    /*
        K.1B proves direct cached-output return ownership accounting only.
        It uses the real cache lookup/addref operation and the real
        Cnr3OwnedFrameRef release/transfer operations. The final return sink is
        synthetic and represents VapourSynth receiving the getFrame return.
        Real VSFrame return integration remains owed by a later keystone phase.
    */
#if !defined(CNR3_KEYSTONE_DEV_TRACE)
    return Cnr3Status::lifecycle_violation;
#else
    struct Cnr3CacheCoreSelftestKeystoneReturnAccounting {
        int lookup_refs_acquired = 0;
        int lookup_refs_released = 0;
        int lookup_refs_transferred = 0;

        [[nodiscard]] int balance() const noexcept {
            return lookup_refs_acquired - lookup_refs_released - lookup_refs_transferred;
        }
    };

    struct Cnr3CacheCoreSelftestGetFrameReturnSink {
        const VSFrame* returned_frame = nullptr;

        [[nodiscard]] Cnr3Status accept_getframe_return(
            const VSFrame* frame
        ) noexcept {
            if (frame == nullptr || returned_frame != nullptr) {
                return Cnr3Status::invalid_argument;
            }

            returned_frame = frame;
            return Cnr3Status::ok;
        }

        void release_as_vapoursynth_owner(
            const VSAPI& vsapi
        ) noexcept {
            const VSFrame* frame_to_release = returned_frame;
            returned_frame = nullptr;

            if (frame_to_release != nullptr && vsapi.freeFrame != nullptr) {
                vsapi.freeFrame(frame_to_release);
            }
        }
    };

    const auto fail = []() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    const auto text_is = [](
        const char* actual,
        const char* expected
    ) noexcept -> bool {
        return actual != nullptr &&
            expected != nullptr &&
            std::strcmp(actual, expected) == 0;
    };

    const auto reset_vsapi_state = [](
        Cnr3CacheCoreSelftestVsApiState& vsapi_state
    ) noexcept {
        vsapi_state = Cnr3CacheCoreSelftestVsApiState{};
        g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;
    };

    const auto store_cached_frame = [](
        Cnr3OutputCacheCore& cache,
        int frame_number,
        const VSFrame* frame,
        const VSAPI& vsapi
    ) noexcept -> Cnr3Status {
        Cnr3OwnedFrameRef cached_owned_frame{};

        const Cnr3Status adopt_status = cached_owned_frame.reset_to_owned_frame(
            frame,
            &vsapi
        );

        if (!cnr3_status_is_ok(adopt_status)) {
            return adopt_status;
        }

        return cache.store_noncheckpoint_owned_frame(
            frame_number,
            std::move(cached_owned_frame)
        );
    };

    const auto observe_acquired_lookup_ref = [](
        const Cnr3OwnedFrameRef& owned_frame,
        Cnr3CacheCoreSelftestKeystoneReturnAccounting& accounting
    ) noexcept -> Cnr3Status {
        if (!owned_frame.has_frame()) {
            return Cnr3Status::ownership_violation;
        }

        ++accounting.lookup_refs_acquired;
        return Cnr3Status::ok;
    };

    const auto release_lookup_ref = [](
        Cnr3OwnedFrameRef& owned_frame,
        Cnr3CacheCoreSelftestKeystoneReturnAccounting& accounting
    ) noexcept -> Cnr3Status {
        if (!owned_frame.has_frame()) {
            return Cnr3Status::ownership_violation;
        }

        owned_frame.reset();
        ++accounting.lookup_refs_released;
        return Cnr3Status::ok;
    };

    const auto transfer_lookup_ref_to_getframe_sink = [](
        Cnr3OwnedFrameRef& owned_frame,
        Cnr3CacheCoreSelftestGetFrameReturnSink& return_sink,
        Cnr3CacheCoreSelftestKeystoneReturnAccounting& accounting
    ) noexcept -> Cnr3Status {
        if (!owned_frame.has_frame()) {
            return Cnr3Status::ownership_violation;
        }

        const VSFrame* frame_for_vapoursynth = owned_frame.transfer_to_caller();

        const Cnr3Status sink_status =
            return_sink.accept_getframe_return(frame_for_vapoursynth);

        if (!cnr3_status_is_ok(sink_status)) {
            return sink_status;
        }

        ++accounting.lookup_refs_transferred;
        return Cnr3Status::ok;
    };

    const auto direct_return_balance_is_clean = [](
        const Cnr3CacheCoreSelftestKeystoneReturnAccounting& accounting
    ) noexcept -> bool {
        return accounting.balance() == 0;
    };

    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;
    VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();

    Cnr3KeystoneRequestPlan direct_return_plan{};
    direct_return_plan.branch = Cnr3KeystoneRequestPlanBranch::direct_cached_output_return;
    direct_return_plan.requested_frame = 58;

    char trace_line[192] = {};
    if (
        cnr3_keystone_request_plan_rebuild_source_request_set(direct_return_plan) != Cnr3Status::ok ||
        !direct_return_plan.hole_frame_numbers.empty() ||
        !direct_return_plan.source_request_frame_numbers.empty() ||
        cnr3_keystone_format_dev_trace_line(
            direct_return_plan,
            trace_line,
            sizeof(trace_line)
        ) != Cnr3Status::ok ||
        !text_is(trace_line, "[KDT] N=58 CACHE-HIT")
        ) {
        return fail();
    }

    {
        reset_vsapi_state(vsapi_state);

        int cached_frame_storage = 58;
        const VSFrame* cached_frame =
            reinterpret_cast<const VSFrame*>(&cached_frame_storage);

        Cnr3OutputCacheCore cache{};

        if (store_cached_frame(cache, 58, cached_frame, vsapi) != Cnr3Status::ok) {
            return fail();
        }

        Cnr3OwnedFrameRef lookup_owned_frame{};
        Cnr3CacheCoreSelftestKeystoneReturnAccounting accounting{};
        Cnr3CacheCoreSelftestGetFrameReturnSink getframe_return_sink{};

        if (
            cache.lookup_frame_and_add_ref(58, &vsapi, lookup_owned_frame) != Cnr3Status::ok ||
            observe_acquired_lookup_ref(lookup_owned_frame, accounting) != Cnr3Status::ok ||
            vsapi_state.add_frame_ref_count != 1 ||
            vsapi_state.last_add_ref_frame != cached_frame ||
            vsapi_state.free_frame_count != 0
            ) {
            return fail();
        }

        if (
            transfer_lookup_ref_to_getframe_sink(
                lookup_owned_frame,
                getframe_return_sink,
                accounting
            ) != Cnr3Status::ok ||
            lookup_owned_frame.has_frame() ||
            getframe_return_sink.returned_frame != cached_frame ||
            !direct_return_balance_is_clean(accounting) ||
            accounting.lookup_refs_acquired != 1 ||
            accounting.lookup_refs_released != 0 ||
            accounting.lookup_refs_transferred != 1 ||
            vsapi_state.free_frame_count != 0
            ) {
            return fail();
        }

        getframe_return_sink.release_as_vapoursynth_owner(vsapi);

        if (
            vsapi_state.free_frame_count != 1 ||
            vsapi_state.last_freed_frame != cached_frame
            ) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }

        if (vsapi_state.free_frame_count != 2) {
            return fail();
        }
    }

    {
        reset_vsapi_state(vsapi_state);

        int cached_frame_storage = 104;
        const VSFrame* cached_frame =
            reinterpret_cast<const VSFrame*>(&cached_frame_storage);

        Cnr3OutputCacheCore cache{};

        if (store_cached_frame(cache, 104, cached_frame, vsapi) != Cnr3Status::ok) {
            return fail();
        }

        Cnr3OwnedFrameRef lookup_owned_frame{};
        Cnr3CacheCoreSelftestKeystoneReturnAccounting accounting{};

        if (
            cache.lookup_frame_and_add_ref(104, &vsapi, lookup_owned_frame) != Cnr3Status::ok ||
            observe_acquired_lookup_ref(lookup_owned_frame, accounting) != Cnr3Status::ok ||
            vsapi_state.add_frame_ref_count != 1 ||
            vsapi_state.free_frame_count != 0
            ) {
            return fail();
        }

        if (
            release_lookup_ref(lookup_owned_frame, accounting) != Cnr3Status::ok ||
            lookup_owned_frame.has_frame() ||
            !direct_return_balance_is_clean(accounting) ||
            accounting.lookup_refs_acquired != 1 ||
            accounting.lookup_refs_released != 1 ||
            accounting.lookup_refs_transferred != 0 ||
            vsapi_state.free_frame_count != 1 ||
            vsapi_state.last_freed_frame != cached_frame
            ) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }

        if (vsapi_state.free_frame_count != 2) {
            return fail();
        }
    }

    {
        reset_vsapi_state(vsapi_state);

        Cnr3OutputCacheCore cache{};
        Cnr3OwnedFrameRef lookup_owned_frame{};
        Cnr3CacheCoreSelftestKeystoneReturnAccounting accounting{};

        if (
            cache.lookup_frame_and_add_ref(999, &vsapi, lookup_owned_frame) != Cnr3Status::not_found ||
            lookup_owned_frame.has_frame() ||
            !direct_return_balance_is_clean(accounting) ||
            accounting.lookup_refs_acquired != 0 ||
            accounting.lookup_refs_released != 0 ||
            accounting.lookup_refs_transferred != 0 ||
            vsapi_state.add_frame_ref_count != 0 ||
            vsapi_state.free_frame_count != 0
            ) {
            return fail();
        }
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    cnr3_cache_core_selftest_trace_line("K.1B direct cached-output return ownership proof scenario");
    cnr3_cache_core_selftest_trace_line("    synthetic proof uses real Cnr3OwnedFrameRef and real cache lookup/addref operations");
    cnr3_cache_core_selftest_trace_line("    success path transfers lookup ref once to a getFrame-return sink and releases zero");
    cnr3_cache_core_selftest_trace_line("    cleanup-before-transfer path releases the lookup ref once and transfers zero");
    cnr3_cache_core_selftest_trace_line("    no-acquire miss path acquires, releases, and transfers zero lookup refs");
    cnr3_cache_core_selftest_trace_line("    real VSFrame return-to-VapourSynth integration remains owed by a later keystone wiring step");

    return Cnr3Status::ok;
#endif
}


Cnr3Status cnr3_cache_core_selftest_k1e1_frame_data_pin_gap_synthetic_proof() noexcept {
    /*
        CMS07-K.1E.1 proves the actual gap-carriage mechanism needed by
        K.1E before any live getFrame frame-1 code depends on it.

        The proof deliberately carries a caller-owned pin-list and predecessor
        frame number in a frameData-shaped heap holder. It carries no
        predecessor VSFrame reference across the arInitial -> arAllFramesReady
        gap. The pin protects slot liveness only; discharge releases the pin
        before the holder is deleted on both the normal and abandoned/free
        paths.
    */
    struct Cnr3CacheCoreSelftestK1EFrameDataHolder {
        int requested_frame = CNR3_INVALID_FRAME_NUMBER;
        int predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
        bool predecessor_pin_taken = false;
        bool predecessor_pin_discharged = false;
        Cnr3CachePinList pin_list{};
    };

    const auto fail = []() noexcept -> Cnr3Status {
        g_cnr3_cache_core_selftest_vsapi_state = nullptr;
        return Cnr3Status::invariant_violation;
    };

    const auto seed_output_zero = [](
        Cnr3OutputCacheCore& cache,
        const VSFrame* frame,
        const VSAPI* vsapi
    ) noexcept -> Cnr3Status {
        if (frame == nullptr || vsapi == nullptr) {
            return Cnr3Status::invalid_argument;
        }

        Cnr3OwnedFrameRef owned_frame{};
        const Cnr3Status adopt_status = owned_frame.reset_to_owned_frame(
            frame,
            vsapi
        );

        if (!cnr3_status_is_ok(adopt_status)) {
            return adopt_status;
        }

        return cache.store_checkpoint_owned_frame(0, std::move(owned_frame));
    };

    const auto seed_noncheckpoint_frame = [](
        Cnr3OutputCacheCore& cache,
        int frame_number,
        const VSFrame* frame,
        const VSAPI* vsapi
    ) noexcept -> Cnr3Status {
        if (!cnr3_frame_number_is_valid(frame_number) || frame == nullptr || vsapi == nullptr) {
            return Cnr3Status::invalid_argument;
        }

        Cnr3OwnedFrameRef owned_frame{};
        const Cnr3Status adopt_status = owned_frame.reset_to_owned_frame(
            frame,
            vsapi
        );

        if (!cnr3_status_is_ok(adopt_status)) {
            return adopt_status;
        }

        return cache.store_noncheckpoint_owned_frame(frame_number, std::move(owned_frame));
    };

    const auto discard_holder = [](
        Cnr3OutputCacheCore& cache,
        Cnr3CacheCoreSelftestK1EFrameDataHolder*& holder
    ) noexcept -> Cnr3Status {
        if (holder == nullptr) {
            return Cnr3Status::ok;
        }

        Cnr3Status status = holder->pin_list.discharge_all(cache);

        if (cnr3_status_is_ok(status)) {
            holder->predecessor_pin_discharged = true;

            if (!holder->pin_list.empty() || holder->pin_list.pin_count() != 0U) {
                status = Cnr3Status::invariant_violation;
            }
        }

        delete holder;
        holder = nullptr;

        return status;
    };

    Cnr3CacheCoreSelftestVsApiState vsapi_state{};
    g_cnr3_cache_core_selftest_vsapi_state = &vsapi_state;

    int normal_frame_storage = 101;
    int abandoned_frame_storage = 202;

    const VSFrame* normal_frame =
        reinterpret_cast<const VSFrame*>(&normal_frame_storage);
    const VSFrame* abandoned_frame =
        reinterpret_cast<const VSFrame*>(&abandoned_frame_storage);

    vsapi_state.tracked_release_frames[0] = normal_frame;
    vsapi_state.tracked_release_frames[1] = abandoned_frame;

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        if (seed_output_zero(cache, normal_frame, &vsapi) != Cnr3Status::ok) {
            return fail();
        }

        if (cache.total_pin_count() != 0) {
            return fail();
        }

        Cnr3CacheCoreSelftestK1EFrameDataHolder* holder =
            new (std::nothrow) Cnr3CacheCoreSelftestK1EFrameDataHolder;

        if (holder == nullptr) {
            return fail();
        }

        holder->requested_frame = 1;
        holder->predecessor_frame = 0;

        if (
            cache.lookup_frame_and_record_pin(
                holder->predecessor_frame,
                holder->pin_list
            ) != Cnr3Status::ok
            ) {
            delete holder;
            return fail();
        }

        holder->predecessor_pin_taken = true;

        if (
            !holder->predecessor_pin_taken ||
            holder->predecessor_pin_discharged ||
            holder->requested_frame != 1 ||
            holder->predecessor_frame != 0 ||
            holder->pin_list.pin_count() != 1U ||
            cache.total_pin_count() != 1 ||
            vsapi_state.add_frame_ref_count != 0 ||
            vsapi_state.free_frame_count != 0
            ) {
            delete holder;
            return fail();
        }

        if (discard_holder(cache, holder) != Cnr3Status::ok || holder != nullptr) {
            return fail();
        }

        if (cache.total_pin_count() != 0) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }

        if (!cache.empty() || cache.total_pin_count() != 0) {
            return fail();
        }
    }

    if (
        vsapi_state.free_frame_count != 1 ||
        vsapi_state.tracked_release_counts[0] != 1 ||
        vsapi_state.tracked_release_counts[1] != 0
        ) {
        return fail();
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        if (seed_output_zero(cache, abandoned_frame, &vsapi) != Cnr3Status::ok) {
            return fail();
        }

        Cnr3CacheCoreSelftestK1EFrameDataHolder* holder =
            new (std::nothrow) Cnr3CacheCoreSelftestK1EFrameDataHolder;

        if (holder == nullptr) {
            return fail();
        }

        holder->requested_frame = 1;
        holder->predecessor_frame = 0;

        if (
            cache.lookup_frame_and_record_pin(
                holder->predecessor_frame,
                holder->pin_list
            ) != Cnr3Status::ok
            ) {
            delete holder;
            return fail();
        }

        holder->predecessor_pin_taken = true;

        if (holder->pin_list.pin_count() != 1U || cache.total_pin_count() != 1) {
            delete holder;
            return fail();
        }

        if (discard_holder(cache, holder) != Cnr3Status::ok || holder != nullptr) {
            return fail();
        }

        if (cache.total_pin_count() != 0) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }

        if (!cache.empty() || cache.total_pin_count() != 0) {
            return fail();
        }
    }

    if (
        vsapi_state.add_frame_ref_count != 0 ||
        vsapi_state.free_frame_count != 2 ||
        vsapi_state.tracked_release_counts[0] != 1 ||
        vsapi_state.tracked_release_counts[1] != 1
        ) {
        return fail();
    }

    {
        VSAPI vsapi = cnr3_cache_core_selftest_make_vsapi();
        Cnr3OutputCacheCore cache{};

        int protected_frame_storage = 303;
        const VSFrame* protected_frame =
            reinterpret_cast<const VSFrame*>(&protected_frame_storage);
        vsapi_state.tracked_release_frames[2] = protected_frame;

        if (seed_output_zero(cache, protected_frame, &vsapi) != Cnr3Status::ok) {
            return fail();
        }

        constexpr std::size_t prune_pressure_frame_count =
            CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES +
            (CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES / 10U) +
            CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS +
            2U;

        std::vector<int> prune_pressure_frame_storage(prune_pressure_frame_count);

        for (std::size_t i = 0; i < prune_pressure_frame_storage.size(); ++i) {
            prune_pressure_frame_storage[i] = static_cast<int>(1000U + i);
            const VSFrame* prune_pressure_frame =
                reinterpret_cast<const VSFrame*>(&prune_pressure_frame_storage[i]);
            const int frame_number = static_cast<int>(i + 1U);

            if (
                seed_noncheckpoint_frame(
                    cache,
                    frame_number,
                    prune_pressure_frame,
                    &vsapi
                ) != Cnr3Status::ok
                ) {
                return fail();
            }
        }

        Cnr3CacheCoreSelftestK1EFrameDataHolder* holder =
            new (std::nothrow) Cnr3CacheCoreSelftestK1EFrameDataHolder;

        if (holder == nullptr) {
            return fail();
        }

        holder->requested_frame = 1;
        holder->predecessor_frame = 0;

        if (
            cache.lookup_frame_and_record_pin(
                holder->predecessor_frame,
                holder->pin_list
            ) != Cnr3Status::ok
            ) {
            delete holder;
            return fail();
        }

        holder->predecessor_pin_taken = true;

        if (holder->pin_list.pin_count() != 1U || cache.total_pin_count() != 1) {
            delete holder;
            return fail();
        }

        Cnr3CachePruneExecutionSummary prune_summary{};
        if (
            cache.execute_bounded_prune_pass(
                CNR3_CACHE_BYTE_BUDGET_BYTES,
                CNR3_CACHE_CHECKPOINT_MIN_RETAIN,
                CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS,
                prune_summary
            ) != Cnr3Status::ok
            ) {
            delete holder;
            return fail();
        }

        if (
            !prune_summary.trigger_decision.prune_is_required ||
            prune_summary.detached_count == 0U ||
            prune_summary.detached_count > CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS ||
            holder->pin_list.pin_count() != 1U ||
            cache.total_pin_count() != 1
            ) {
            delete holder;
            return fail();
        }

        Cnr3OwnedFrameRef protected_lookup_ref{};
        if (
            cache.lookup_frame_and_add_ref(
                holder->predecessor_frame,
                &vsapi,
                protected_lookup_ref
            ) != Cnr3Status::ok ||
            !protected_lookup_ref.has_frame() ||
            protected_lookup_ref.get() != protected_frame
            ) {
            protected_lookup_ref.reset();
            delete holder;
            return fail();
        }

        if (vsapi_state.add_frame_ref_count != 1) {
            protected_lookup_ref.reset();
            delete holder;
            return fail();
        }

        protected_lookup_ref.reset();

        if (vsapi_state.tracked_release_counts[2] != 1) {
            delete holder;
            return fail();
        }

        if (discard_holder(cache, holder) != Cnr3Status::ok || holder != nullptr) {
            return fail();
        }

        if (cache.total_pin_count() != 0) {
            return fail();
        }

        if (cache.clear() != Cnr3Status::ok) {
            return fail();
        }

        if (!cache.empty() || cache.total_pin_count() != 0) {
            return fail();
        }
    }

    if (
        vsapi_state.add_frame_ref_count != 1 ||
        vsapi_state.tracked_release_counts[2] != 2
        ) {
        return fail();
    }

    g_cnr3_cache_core_selftest_vsapi_state = nullptr;

    cnr3_cache_core_selftest_trace_line("K.1E.1 frameData pin-gap synthetic proof scenario");
    cnr3_cache_core_selftest_trace_line("    frameData-shaped holder carries predecessor frame number and caller-owned pin-list only");
    cnr3_cache_core_selftest_trace_line("    no predecessor VSFrame ref is carried across the arInitial/arAllFramesReady gap");
    cnr3_cache_core_selftest_trace_line("    normal path discharges the pin-list before deleting the holder");
    cnr3_cache_core_selftest_trace_line("    abandoned/free path uses the same discharge-before-delete helper");
    cnr3_cache_core_selftest_trace_line("    intervening prune during the gap leaves the pinned predecessor present and retrievable");

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
            "d4_present_frame_adopt_skip_primitive",
            cnr3_cache_core_selftest_d4_present_frame_adopt_skip_primitive
        },
        {
            "d4_first_in_best_dressed_duplicate_primitive",
            cnr3_cache_core_selftest_d4_first_in_best_dressed_duplicate_primitive
        },
        {
            "d5_recovery_pin_survives_bounded_prune_pass",
            cnr3_cache_core_selftest_d5_recovery_pin_survives_bounded_prune_pass
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
            "vapoursynth_plane_view_adapter_proof",
            cnr3_cache_core_selftest_vapoursynth_plane_view_adapter_proof
        },
        {
            "caller_supplied_frame_triplet_view_proof",
            cnr3_cache_core_selftest_caller_supplied_frame_triplet_view_proof
        },
        {
            "caller_supplied_real_frame_pixel_composition_proof",
            cnr3_cache_core_selftest_caller_supplied_real_frame_pixel_composition_proof
        },
        {
            "caller_supplied_scene_change_reset_proof",
            cnr3_cache_core_selftest_caller_supplied_scene_change_reset_proof
        },
        {
            "keystone_request_plan_dev_trace_proof",
            cnr3_cache_core_selftest_keystone_request_plan_dev_trace_proof
        },
        {
            "keystone_direct_cached_output_return_proof",
            cnr3_cache_core_selftest_keystone_direct_cached_output_return_proof
        },
        {
            "k1e1_frame_data_pin_gap_synthetic_proof",
            cnr3_cache_core_selftest_k1e1_frame_data_pin_gap_synthetic_proof
        },
        {
            "as4_single_lock_batch_discharge_proof",
            cnr3_cache_core_selftest_as4_single_lock_batch_discharge_proof
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
