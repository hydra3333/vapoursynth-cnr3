#include "cnr3_cache_core.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

static_assert(
    std::is_nothrow_move_constructible_v<Cnr3CacheSlot>,
    "Cnr3CacheSlot must be nothrow move-constructible for cache vector insertion."
    );

static_assert(
    std::is_nothrow_move_assignable_v<Cnr3CacheSlot>,
    "Cnr3CacheSlot must be nothrow move-assignable for cache slot removal."
    );

static_assert(
    std::is_nothrow_move_assignable_v<Cnr3OwnedFrameRef>,
    "Cnr3OwnedFrameRef must be nothrow move-assignable for cache slot adoption."
    );

namespace {

    [[nodiscard]] bool cnr3_prune_candidate_distance_order_before(
        Cnr3PruneCandidateDistanceOrderEntry left,
        Cnr3PruneCandidateDistanceOrderEntry right
    ) noexcept {
        if (left.nearest_hot_zone_distance != right.nearest_hot_zone_distance) {
            return left.nearest_hot_zone_distance > right.nearest_hot_zone_distance;
        }

        return left.frame_number < right.frame_number;
    }

    [[nodiscard]] int cnr3_frame_distance_to_hot_zone_boundary(
        int frame_number,
        const Cnr3CacheHotZone& hot_zone
    ) noexcept {
        if (!hot_zone.is_active || !cnr3_frame_number_is_valid(frame_number)) {
            return std::numeric_limits<int>::max();
        }

        if (frame_number < hot_zone.low_frame) {
            return hot_zone.low_frame - frame_number;
        }

        if (frame_number > hot_zone.high_frame) {
            return frame_number - hot_zone.high_frame;
        }

        return 0;
    }

    [[nodiscard]] Cnr3Status cnr3_consider_prune_candidate_bounded(
        Cnr3PruneCandidateDistanceOrderEntry candidate,
        std::size_t max_select_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order
    ) {
        if (max_select_count == 0U) {
            return Cnr3Status::ok;
        }

        if (candidate_order.capacity() < max_select_count) {
            return Cnr3Status::invalid_argument;
        }

        if (!cnr3_prune_candidate_distance_order_entry_is_valid(candidate)) {
            return Cnr3Status::invariant_violation;
        }

        if (candidate_order.size() < max_select_count) {
            candidate_order.push_back(candidate);
            return Cnr3Status::ok;
        }

        auto worst_selected = std::max_element(
            candidate_order.begin(),
            candidate_order.end(),
            cnr3_prune_candidate_distance_order_before
        );

        if (
            worst_selected != candidate_order.end() &&
            cnr3_prune_candidate_distance_order_before(candidate, *worst_selected)
            ) {
            *worst_selected = candidate;
        }

        return Cnr3Status::ok;
    }

} // namespace

Cnr3CacheSlotId Cnr3CacheSlotIdSource::allocate() noexcept {
    const std::uint64_t value_to_return = (next_value_ != 0U) ? next_value_ : 1U;

    next_value_ =
        (value_to_return < std::numeric_limits<std::uint64_t>::max())
        ? (value_to_return + 1U)
        : 1U;

    return Cnr3CacheSlotId{ value_to_return };
}

std::uint64_t Cnr3CacheSlotIdSource::next_value_for_diagnostics() const noexcept {
    return next_value_;
}

bool cnr3_cache_slot_has_frame(
    const Cnr3CacheSlot& slot
) noexcept {
    return slot.frame.has_frame();
}

bool cnr3_cache_slot_is_indexable(
    const Cnr3CacheSlot& slot
) noexcept {
    return cnr3_cache_slot_id_is_valid(slot.slot_id) &&
        cnr3_frame_number_is_valid(slot.frame_number) &&
        slot.frame.has_frame();
}

bool cnr3_cache_hot_zone_is_valid(
    const Cnr3CacheHotZone& hot_zone
) noexcept {
    if (!hot_zone.is_active) {
        return
            hot_zone.low_frame == CNR3_INVALID_FRAME_NUMBER &&
            hot_zone.high_frame == CNR3_INVALID_FRAME_NUMBER &&
            hot_zone.last_observed_frame == CNR3_INVALID_FRAME_NUMBER;
    }

    if (!cnr3_frame_number_is_valid(hot_zone.low_frame)) {
        return false;
    }

    if (!cnr3_frame_number_is_valid(hot_zone.high_frame)) {
        return false;
    }

    if (!cnr3_frame_number_is_valid(hot_zone.last_observed_frame)) {
        return false;
    }

    if (hot_zone.low_frame > hot_zone.high_frame) {
        return false;
    }

    if (hot_zone.last_observed_frame < hot_zone.low_frame) {
        return false;
    }

    if (hot_zone.last_observed_frame > hot_zone.high_frame) {
        return false;
    }

    return true;
}

bool cnr3_cache_hot_zone_model_invariants_hold(
    const std::vector<Cnr3CacheHotZone>& hot_zones
) noexcept {
    if (hot_zones.size() > CNR3_CACHE_MAX_HOT_ZONES) {
        return false;
    }

    for (const Cnr3CacheHotZone& hot_zone : hot_zones) {
        if (!cnr3_cache_hot_zone_is_valid(hot_zone)) {
            return false;
        }
    }

    return true;
}

bool Cnr3OutputCacheCore::empty() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return empty_locked();
}

std::size_t Cnr3OutputCacheCore::slot_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return slot_count_locked();
}

std::size_t Cnr3OutputCacheCore::index_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return index_count_locked();
}

std::size_t Cnr3OutputCacheCore::checkpoint_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return checkpoint_count_locked();
}

std::size_t Cnr3OutputCacheCore::hot_zone_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return hot_zone_count_locked();
}

Cnr3CacheHotZoneDiagnosticStats Cnr3OutputCacheCore::hot_zone_diagnostic_stats() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return hot_zone_diagnostic_stats_locked();
}

bool Cnr3OutputCacheCore::frame_is_inside_hot_zone(
    int frame_number
) const {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return false;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return frame_is_inside_hot_zone_locked(frame_number);
}

Cnr3Status Cnr3OutputCacheCore::record_hot_zone_observation(
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return record_hot_zone_observation_locked(frame_number);
}

Cnr3Status Cnr3OutputCacheCore::retire_decay_eligible_hot_zones(
    int current_frame
) {
    if (!cnr3_frame_number_is_valid(current_frame)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return retire_decay_eligible_hot_zones_locked(current_frame);
}

int Cnr3OutputCacheCore::total_pin_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return total_pin_count_locked();
}

bool Cnr3OutputCacheCore::cache_state_invariants_hold() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return cache_state_invariants_hold_locked();
}

Cnr3Status Cnr3OutputCacheCore::store_noncheckpoint_owned_frame(
    int frame_number,
    Cnr3OwnedFrameRef frame
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so a rejected owned frame is released after
        cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = store_noncheckpoint_owned_frame_locked(frame_number, frame);
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::store_checkpoint_owned_frame(
    int frame_number,
    Cnr3OwnedFrameRef frame
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so a rejected owned frame is released after
        cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = store_checkpoint_owned_frame_locked(frame_number, frame);
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_frame(
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3CacheSlot detached_slot{};
    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slot releases its owned
        frame after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_frame_locked(frame_number, detached_slot);
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_selected_unpinned_frames_bounded(
    const std::vector<int>& candidate_frame_numbers,
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U || candidate_frame_numbers.empty()) {
        return Cnr3Status::ok;
    }

    for (const int frame_number : candidate_frame_numbers) {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return Cnr3Status::invalid_argument;
        }
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_selected_unpinned_frames_bounded_locked(
            candidate_frame_numbers,
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_bounded(
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_noncheckpoint_frames_bounded_locked(
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded_locked(
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded(
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
) const {
    out_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (max_select_count > out_candidate_order.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded_locked(
        max_select_count,
        out_candidate_order
    );
}

Cnr3Status Cnr3OutputCacheCore::select_composite_prune_candidates_bounded(
    bool noncheckpoint_capacity_permits,
    std::size_t retain_checkpoint_count,
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
) const {
    out_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (max_select_count > out_candidate_order.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    std::vector<Cnr3PruneCandidateDistanceOrderEntry> checkpoint_candidate_order{};

    if (max_select_count > checkpoint_candidate_order.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        checkpoint_candidate_order.reserve(max_select_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return select_composite_prune_candidates_bounded_locked(
        noncheckpoint_capacity_permits,
        retain_checkpoint_count,
        max_select_count,
        out_candidate_order,
        checkpoint_candidate_order
    );
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_checkpoints_above_retain_count_bounded(
    std::size_t retain_checkpoint_count,
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_checkpoints_above_retain_count_bounded_locked(
            retain_checkpoint_count,
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_add_ref(
    int frame_number,
    const VSAPI* vsapi,
    Cnr3OwnedFrameRef& out_frame
) const {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (vsapi == nullptr || vsapi->addFrameRef == nullptr || vsapi->freeFrame == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    if (out_frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    const VSFrame* acquired_frame = nullptr;
    Cnr3Status status = Cnr3Status::invariant_violation;

    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = lookup_frame_and_add_ref_locked(
            frame_number,
            vsapi,
            &acquired_frame
        );
    }

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    if (acquired_frame == nullptr) {
        return Cnr3Status::vapoursynth_error;
    }

    const Cnr3Status adopt_status =
        out_frame.reset_to_owned_frame(acquired_frame, vsapi);

    if (!cnr3_status_is_ok(adopt_status)) {
        /*
            This should be unreachable because vsapi was validated before the
            lookup. If it ever happens, rebalance the acquired lookup reference
            outside cache_mutex_ before reporting the ownership error.
        */
        vsapi->freeFrame(acquired_frame);

        return adopt_status;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_record_pin(
    int frame_number,
    Cnr3CachePinList& pin_list
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status reserve_status = pin_list.reserve_for_additional_pins(1U);

    if (!cnr3_status_is_ok(reserve_status)) {
        return reserve_status;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return lookup_frame_and_record_pin_locked(frame_number, pin_list);
}

Cnr3Status Cnr3OutputCacheCore::unpin_frame(
    Cnr3CacheSlotPinToken& pin_token
) {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return unpin_frame_locked(pin_token);
}

Cnr3Status Cnr3OutputCacheCore::clear() {
    std::vector<Cnr3CacheSlot> detached_slots{};
    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so detached Cnr3OwnedFrameRef objects are
        destroyed only after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = clear_locked(detached_slots);
    }

    return status;
}

bool Cnr3OutputCacheCore::empty_locked() const noexcept {
    return slots_.empty() &&
        frame_index_.empty() &&
        checkpoint_slot_positions_.empty() &&
        hot_zones_.empty();
}

std::size_t Cnr3OutputCacheCore::slot_count_locked() const noexcept {
    return slots_.size();
}

std::size_t Cnr3OutputCacheCore::index_count_locked() const noexcept {
    return frame_index_.size();
}

std::size_t Cnr3OutputCacheCore::checkpoint_count_locked() const noexcept {
    return checkpoint_slot_positions_.size();
}

std::size_t Cnr3OutputCacheCore::hot_zone_count_locked() const noexcept {
    return hot_zones_.size();
}

Cnr3CacheHotZoneDiagnosticStats Cnr3OutputCacheCore::hot_zone_diagnostic_stats_locked() const noexcept {
    return hot_zone_diag_stats_;
}

bool Cnr3OutputCacheCore::frame_is_inside_hot_zone_locked(
    int frame_number
) const noexcept {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return false;
    }

    for (const Cnr3CacheHotZone& hot_zone : hot_zones_) {
        if (!hot_zone.is_active) {
            continue;
        }

        if (
            frame_number >= hot_zone.low_frame &&
            frame_number <= hot_zone.high_frame
            ) {
            return true;
        }
    }

    return false;
}

int Cnr3OutputCacheCore::nearest_active_hot_zone_boundary_distance_locked(
    int frame_number
) const noexcept {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return 0;
    }

    int nearest_distance = std::numeric_limits<int>::max();

    for (const Cnr3CacheHotZone& hot_zone : hot_zones_) {
        if (!hot_zone.is_active) {
            continue;
        }

        const int distance =
            cnr3_frame_distance_to_hot_zone_boundary(frame_number, hot_zone);

        if (distance < nearest_distance) {
            nearest_distance = distance;
        }
    }

    return nearest_distance;
}

bool Cnr3OutputCacheCore::hot_zone_has_pinned_frame_in_range_locked(
    const Cnr3CacheHotZone& hot_zone
) const noexcept {
    if (!cnr3_cache_hot_zone_is_valid(hot_zone) || !hot_zone.is_active) {
        return false;
    }

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_has_frame(slot)) {
            continue;
        }

        if (slot.pin_count <= 0) {
            continue;
        }

        if (
            slot.frame_number >= hot_zone.low_frame &&
            slot.frame_number <= hot_zone.high_frame
            ) {
            return true;
        }
    }

    return false;
}

int Cnr3OutputCacheCore::total_pin_count_locked() const noexcept {
    int total_pin_count = 0;

    for (const Cnr3CacheSlot& slot : slots_) {
        if (slot.pin_count < 0) {
            return -1;
        }

        if (slot.pin_count > (std::numeric_limits<int>::max() - total_pin_count)) {
            return -1;
        }

        total_pin_count += slot.pin_count;
    }

    return total_pin_count;
}

void Cnr3OutputCacheCore::observe_hot_zone_create_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_create(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_slide_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_slide(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_merge_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_merge(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_decay_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_decay(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_expiry_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_expiry(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_state_sample_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_zone_count_sample(
        hot_zone_diag_stats_,
        hot_zones_.size()
    );

    for (const Cnr3CacheHotZone& hot_zone : hot_zones_) {
        if (!hot_zone.is_active) {
            continue;
        }

        if (hot_zone.high_frame < hot_zone.low_frame) {
            continue;
        }

        cnr3_cache_hot_zone_diagnostic_observe_protected_range_sample(
            hot_zone_diag_stats_,
            hot_zone.high_frame - hot_zone.low_frame + 1
        );
    }
#endif
}

Cnr3Status Cnr3OutputCacheCore::merge_closest_active_hot_zones_locked() {
    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    if (hot_zones_.size() < 2U) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t first_merge_position = hot_zones_.size();
    std::size_t second_merge_position = hot_zones_.size();
    int best_distance = std::numeric_limits<int>::max();

    for (std::size_t first_position = 0U;
        first_position < hot_zones_.size();
        ++first_position
        ) {
        const Cnr3CacheHotZone& first_zone = hot_zones_[first_position];

        if (!first_zone.is_active) {
            continue;
        }

        for (std::size_t second_position = first_position + 1U;
            second_position < hot_zones_.size();
            ++second_position
            ) {
            const Cnr3CacheHotZone& second_zone = hot_zones_[second_position];

            if (!second_zone.is_active) {
                continue;
            }

            int distance = 0;

            if (first_zone.high_frame < second_zone.low_frame) {
                distance = second_zone.low_frame - first_zone.high_frame;
            }
            else if (second_zone.high_frame < first_zone.low_frame) {
                distance = first_zone.low_frame - second_zone.high_frame;
            }

            if (distance < best_distance) {
                best_distance = distance;
                first_merge_position = first_position;
                second_merge_position = second_position;
            }
        }
    }

    if (
        first_merge_position >= hot_zones_.size() ||
        second_merge_position >= hot_zones_.size()
        ) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone& first_zone = hot_zones_[first_merge_position];
    const Cnr3CacheHotZone& second_zone = hot_zones_[second_merge_position];

    if (second_zone.low_frame < first_zone.low_frame) {
        first_zone.low_frame = second_zone.low_frame;
    }

    if (second_zone.high_frame > first_zone.high_frame) {
        first_zone.high_frame = second_zone.high_frame;
    }

    if (second_zone.last_observed_frame > first_zone.last_observed_frame) {
        first_zone.last_observed_frame = second_zone.last_observed_frame;
    }

    hot_zones_.erase(hot_zones_.begin() + second_merge_position);
    observe_hot_zone_merge_locked();

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::record_hot_zone_observation_locked(
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (frame_number >
        (std::numeric_limits<int>::max() - CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS)
        ) {
        return Cnr3Status::capacity_exceeded;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const int observed_low_frame =
        (frame_number > CNR3_CACHE_HOT_ZONE_BACK_RADIUS) ?
        (frame_number - CNR3_CACHE_HOT_ZONE_BACK_RADIUS) :
        0;
    const int observed_high_frame =
        frame_number + CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS;

    std::size_t best_zone_position = hot_zones_.size();
    int best_distance = std::numeric_limits<int>::max();

    for (std::size_t zone_position = 0U;
        zone_position < hot_zones_.size();
        ++zone_position
        ) {
        const Cnr3CacheHotZone& hot_zone = hot_zones_[zone_position];

        if (!hot_zone.is_active) {
            continue;
        }

        int distance = 0;

        if (frame_number < hot_zone.low_frame) {
            distance = hot_zone.low_frame - frame_number;
        }
        else if (frame_number > hot_zone.high_frame) {
            distance = frame_number - hot_zone.high_frame;
        }

        if (
            distance <= CNR3_CACHE_JUMP_THRESHOLD &&
            distance < best_distance
            ) {
            best_distance = distance;
            best_zone_position = zone_position;
        }
    }

    if (best_zone_position < hot_zones_.size()) {
        Cnr3CacheHotZone& hot_zone = hot_zones_[best_zone_position];

        hot_zone.is_active = true;
        hot_zone.low_frame = observed_low_frame;
        hot_zone.high_frame = observed_high_frame;
        hot_zone.last_observed_frame = frame_number;
        observe_hot_zone_slide_locked();
    }
    else {
        if (hot_zones_.size() >= CNR3_CACHE_MAX_HOT_ZONES) {
            const Cnr3Status merge_status = merge_closest_active_hot_zones_locked();

            if (!cnr3_status_is_ok(merge_status)) {
                return merge_status;
            }
        }

        if (hot_zones_.size() >= CNR3_CACHE_MAX_HOT_ZONES) {
            return Cnr3Status::capacity_exceeded;
        }

        Cnr3CacheHotZone hot_zone{};
        hot_zone.is_active = true;
        hot_zone.low_frame = observed_low_frame;
        hot_zone.high_frame = observed_high_frame;
        hot_zone.last_observed_frame = frame_number;

        try {
            hot_zones_.push_back(hot_zone);
        }
        catch (const std::bad_alloc&) {
            return Cnr3Status::allocation_failed;
        }

        observe_hot_zone_create_locked();
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::retire_decay_eligible_hot_zones_locked(
    int current_frame
) {
    if (!cnr3_frame_number_is_valid(current_frame)) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (std::size_t zone_position = 0U; zone_position < hot_zones_.size();) {
        const Cnr3CacheHotZone& hot_zone = hot_zones_[zone_position];

        if (!hot_zone.is_active) {
            hot_zones_.erase(hot_zones_.begin() + zone_position);
            continue;
        }

        const bool decay_margin_elapsed =
            current_frame >= hot_zone.last_observed_frame &&
            (current_frame - hot_zone.last_observed_frame) >=
            CNR3_CACHE_HOT_ZONE_DECAY_MARGIN;

        if (decay_margin_elapsed) {
            observe_hot_zone_decay_locked();
        }

        if (
            decay_margin_elapsed &&
            !hot_zone_has_pinned_frame_in_range_locked(hot_zone)
            ) {
            hot_zones_.erase(hot_zones_.begin() + zone_position);
            observe_hot_zone_expiry_locked();
            continue;
        }

        ++zone_position;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::store_noncheckpoint_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame
) {
    return store_owned_frame_locked(frame_number, frame, false);
}

Cnr3Status Cnr3OutputCacheCore::store_checkpoint_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame
) {
    return store_owned_frame_locked(frame_number, frame, true);
}

Cnr3Status Cnr3OutputCacheCore::store_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame,
    bool is_checkpoint
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto existing_frame_index_it = frame_index_.find(frame_number);

    if (existing_frame_index_it != frame_index_.end()) {
        const std::size_t existing_slot_position = existing_frame_index_it->second;

        if (existing_slot_position >= slots_.size()) {
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheSlot& existing_slot = slots_[existing_slot_position];

        if (!cnr3_cache_slot_is_indexable(existing_slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (existing_slot.frame_number != frame_number) {
            return Cnr3Status::invariant_violation;
        }

        if (is_checkpoint && !existing_slot.is_checkpoint) {
            if (
                checkpoint_slot_positions_.size() >=
                checkpoint_slot_positions_.max_size()
                ) {
                return Cnr3Status::capacity_exceeded;
            }

            try {
                checkpoint_slot_positions_.reserve(
                    checkpoint_slot_positions_.size() + 1U
                );
            }
            catch (const std::bad_alloc&) {
                return Cnr3Status::allocation_failed;
            }

            existing_slot.is_checkpoint = true;
            checkpoint_slot_positions_.push_back(existing_slot_position);

            if (!cache_state_invariants_hold_locked()) {
                return Cnr3Status::invariant_violation;
            }
        }

        return Cnr3Status::duplicate;
    }

    const std::size_t slot_position = slots_.size();

    if (slot_position >= slots_.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    if (
        is_checkpoint &&
        checkpoint_slot_positions_.size() >= checkpoint_slot_positions_.max_size()
        ) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        slots_.reserve(slot_position + 1U);

        if (is_checkpoint) {
            checkpoint_slot_positions_.reserve(
                checkpoint_slot_positions_.size() + 1U
            );
        }

        const auto insert_result = frame_index_.emplace(frame_number, slot_position);

        if (!insert_result.second) {
            return Cnr3Status::duplicate;
        }
    }
    catch (const std::bad_alloc&) {
        frame_index_.erase(frame_number);

        return Cnr3Status::allocation_failed;
    }

    Cnr3CacheSlot slot{};
    slot.slot_id = slot_id_source_.allocate();
    slot.frame_number = frame_number;
    slot.frame = std::move(frame);
    slot.is_checkpoint = is_checkpoint;
    slot.pin_count = 0;

    slots_.push_back(std::move(slot));

    if (is_checkpoint) {
        checkpoint_slot_positions_.push_back(slot_position);
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_frame_locked(
    int frame_number,
    Cnr3CacheSlot& detached_slot
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (
        cnr3_cache_slot_has_frame(detached_slot) ||
        cnr3_cache_slot_id_is_valid(detached_slot.slot_id) ||
        cnr3_frame_number_is_valid(detached_slot.frame_number) ||
        detached_slot.is_checkpoint ||
        detached_slot.pin_count != 0
        ) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::not_found;
    }

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.pin_count != 0) {
        return Cnr3Status::lifecycle_violation;
    }

    const bool removed_checkpoint = slot.is_checkpoint;
    const std::size_t last_slot_position = slots_.size() - 1U;

    bool removed_checkpoint_position = false;

    if (removed_checkpoint) {
        for (std::size_t checkpoint_index = 0;
            checkpoint_index < checkpoint_slot_positions_.size();
            ++checkpoint_index
            ) {
            if (checkpoint_slot_positions_[checkpoint_index] == slot_position) {
                checkpoint_slot_positions_[checkpoint_index] =
                    checkpoint_slot_positions_.back();
                checkpoint_slot_positions_.pop_back();
                removed_checkpoint_position = true;
                break;
            }
        }

        if (!removed_checkpoint_position) {
            return Cnr3Status::invariant_violation;
        }
    }

    frame_index_.erase(frame_index_it);
    detached_slot = std::move(slots_[slot_position]);

    if (slot_position != last_slot_position) {
        Cnr3CacheSlot& moved_slot = slots_[last_slot_position];

        if (!cnr3_cache_slot_is_indexable(moved_slot)) {
            return Cnr3Status::invariant_violation;
        }

        const int moved_frame_number = moved_slot.frame_number;
        auto moved_frame_index_it = frame_index_.find(moved_frame_number);

        if (moved_frame_index_it == frame_index_.end()) {
            return Cnr3Status::invariant_violation;
        }

        slots_[slot_position] = std::move(moved_slot);
        moved_frame_index_it->second = slot_position;

        for (std::size_t& checkpoint_slot_position : checkpoint_slot_positions_) {
            if (checkpoint_slot_position == last_slot_position) {
                checkpoint_slot_position = slot_position;
            }
        }
    }

    slots_.pop_back();

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_selected_unpinned_frames_bounded_locked(
    const std::vector<int>& candidate_frame_numbers,
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U || candidate_frame_numbers.empty()) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (const int frame_number : candidate_frame_numbers) {
        if (out_removed_count >= max_remove_count) {
            break;
        }

        if (!cnr3_frame_number_is_valid(frame_number)) {
            return Cnr3Status::invalid_argument;
        }

        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_bounded_locked(
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t slot_position = 0U;

    while (slot_position < slots_.size() && out_removed_count < max_remove_count) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (slot.pin_count != 0 || slot.is_checkpoint) {
            ++slot_position;
            continue;
        }

        const int frame_number = slot.frame_number;
        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;

        /*
            Do not increment slot_position here. The central remove helper may
            move the previous last slot into this position, and that moved slot
            still needs to be evaluated by this bounded selection pass.
        */
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded_locked(
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t slot_position = 0U;

    while (slot_position < slots_.size() && out_removed_count < max_remove_count) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (
            slot.pin_count != 0 ||
            slot.is_checkpoint ||
            frame_is_inside_hot_zone_locked(slot.frame_number)
            ) {
            ++slot_position;
            continue;
        }

        const int frame_number = slot.frame_number;
        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;

        /*
            Do not increment slot_position here. The central remove helper may
            move the previous last slot into this position, and that moved slot
            still needs to be evaluated by this bounded selection pass.
        */
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded_locked(
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
) const {
    out_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (
            slot.pin_count != 0 ||
            slot.is_checkpoint ||
            frame_is_inside_hot_zone_locked(slot.frame_number)
            ) {
            continue;
        }

        const Cnr3PruneCandidateDistanceOrderEntry candidate{
            slot.frame_number,
            nearest_active_hot_zone_boundary_distance_locked(slot.frame_number)
        };

        if (!cnr3_prune_candidate_distance_order_entry_is_valid(candidate)) {
            return Cnr3Status::invariant_violation;
        }

        if (out_candidate_order.size() < max_select_count) {
            out_candidate_order.push_back(candidate);
            continue;
        }

        auto worst_selected = std::max_element(
            out_candidate_order.begin(),
            out_candidate_order.end(),
            cnr3_prune_candidate_distance_order_before
        );

        if (
            worst_selected != out_candidate_order.end() &&
            cnr3_prune_candidate_distance_order_before(candidate, *worst_selected)
            ) {
            *worst_selected = candidate;
        }
    }

    std::sort(
        out_candidate_order.begin(),
        out_candidate_order.end(),
        cnr3_prune_candidate_distance_order_before
    );

    return Cnr3Status::ok;
}


Cnr3Status Cnr3OutputCacheCore::select_composite_prune_candidates_bounded_locked(
    bool noncheckpoint_capacity_permits,
    std::size_t retain_checkpoint_count,
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& checkpoint_candidate_order
) const {
    out_candidate_order.clear();
    checkpoint_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    if (checkpoint_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const std::size_t checkpoint_count = checkpoint_count_locked();
    const std::size_t checkpoint_select_budget =
        (checkpoint_count > retain_checkpoint_count)
        ? std::min(checkpoint_count - retain_checkpoint_count, max_select_count)
        : 0U;

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (slot.pin_count != 0 || frame_is_inside_hot_zone_locked(slot.frame_number)) {
            continue;
        }

        if (slot.is_checkpoint) {
            if (slot.frame_number == 0 || checkpoint_select_budget == 0U) {
                continue;
            }

            const Cnr3PruneCandidateDistanceOrderEntry candidate{
                slot.frame_number,
                nearest_active_hot_zone_boundary_distance_locked(slot.frame_number),
                true
            };

            const Cnr3Status consider_status = cnr3_consider_prune_candidate_bounded(
                candidate,
                checkpoint_select_budget,
                checkpoint_candidate_order
            );

            if (!cnr3_status_is_ok(consider_status)) {
                return consider_status;
            }

            continue;
        }

        if (!noncheckpoint_capacity_permits) {
            continue;
        }

        const Cnr3PruneCandidateDistanceOrderEntry candidate{
            slot.frame_number,
            nearest_active_hot_zone_boundary_distance_locked(slot.frame_number),
            false
        };

        const Cnr3Status consider_status = cnr3_consider_prune_candidate_bounded(
            candidate,
            max_select_count,
            out_candidate_order
        );

        if (!cnr3_status_is_ok(consider_status)) {
            return consider_status;
        }
    }

    for (const Cnr3PruneCandidateDistanceOrderEntry& checkpoint_candidate : checkpoint_candidate_order) {
        const Cnr3Status consider_status = cnr3_consider_prune_candidate_bounded(
            checkpoint_candidate,
            max_select_count,
            out_candidate_order
        );

        if (!cnr3_status_is_ok(consider_status)) {
            return consider_status;
        }
    }

    std::sort(
        out_candidate_order.begin(),
        out_candidate_order.end(),
        cnr3_prune_candidate_distance_order_before
    );

    return Cnr3Status::ok;
}


Cnr3Status Cnr3OutputCacheCore::remove_unpinned_checkpoints_above_retain_count_bounded_locked(
    std::size_t retain_checkpoint_count,
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t slot_position = 0U;

    while (
        slot_position < slots_.size() &&
        out_removed_count < max_remove_count &&
        checkpoint_count_locked() > retain_checkpoint_count
        ) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (
            !slot.is_checkpoint ||
            slot.pin_count != 0 ||
            slot.frame_number == 0
            ) {
            ++slot_position;
            continue;
        }

        const int frame_number = slot.frame_number;
        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;

        /*
            Do not increment slot_position here. The central remove helper may
            move the previous last slot into this position, and that moved slot
            still needs to be evaluated by this bounded selection pass.
        */
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_add_ref_locked(
    int frame_number,
    const VSAPI* vsapi,
    const VSFrame** out_acquired_frame
) const {
    if (out_acquired_frame == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    *out_acquired_frame = nullptr;

    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (vsapi == nullptr || vsapi->addFrameRef == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::not_found;
    }

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    const VSFrame* cached_frame = slot.frame.get();

    if (cached_frame == nullptr) {
        return Cnr3Status::invariant_violation;
    }

    const VSFrame* acquired_frame = vsapi->addFrameRef(cached_frame);

    if (acquired_frame == nullptr) {
        return Cnr3Status::vapoursynth_error;
    }

    *out_acquired_frame = acquired_frame;

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_record_pin_locked(
    int frame_number,
    Cnr3CachePinList& pin_list
) {
    Cnr3CacheSlotPinToken pin_token{};

    const Cnr3Status pin_status = pin_frame_locked(frame_number, pin_token);

    if (!cnr3_status_is_ok(pin_status)) {
        return pin_status;
    }

    const Cnr3Status record_status =
        pin_list.record_pin_without_allocation(pin_token);

    if (!cnr3_status_is_ok(record_status)) {
        const Cnr3Status unpin_status = unpin_frame_locked(pin_token);

        if (!cnr3_status_is_ok(unpin_status)) {
            return unpin_status;
        }

        return record_status;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::pin_frame_locked(
    int frame_number,
    Cnr3CacheSlotPinToken& out_pin_token
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (cnr3_cache_slot_pin_token_is_valid(out_pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::not_found;
    }

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.pin_count == std::numeric_limits<int>::max()) {
        return Cnr3Status::capacity_exceeded;
    }

    ++slot.pin_count;

    out_pin_token.slot_id = slot.slot_id;
    out_pin_token.frame_number = slot.frame_number;

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::unpin_frame_locked(
    Cnr3CacheSlotPinToken& pin_token
) {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto frame_index_it = frame_index_.find(pin_token.frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::lifecycle_violation;
    }

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != pin_token.frame_number) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.slot_id.value != pin_token.slot_id.value) {
        return Cnr3Status::lifecycle_violation;
    }

    if (slot.pin_count <= 0) {
        return Cnr3Status::lifecycle_violation;
    }

    --slot.pin_count;
    cnr3_cache_slot_pin_token_reset(pin_token);

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::clear_locked(
    std::vector<Cnr3CacheSlot>& detached_slots
) {
    if (!detached_slots.empty()) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (const Cnr3CacheSlot& slot : slots_) {
        if (slot.pin_count != 0) {
            return Cnr3Status::lifecycle_violation;
        }
    }

    detached_slots.swap(slots_);
    frame_index_.clear();
    checkpoint_slot_positions_.clear();
    hot_zones_.clear();

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

bool Cnr3OutputCacheCore::cache_state_invariants_hold_locked() const noexcept {
    if (!cnr3_cache_hot_zone_model_invariants_hold(hot_zones_)) {
        return false;
    }

    for (const auto& frame_index_entry : frame_index_) {
        const int frame_number = frame_index_entry.first;
        const std::size_t slot_position = frame_index_entry.second;

        if (slot_position >= slots_.size()) {
            return false;
        }

        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return false;
        }

        if (slot.frame_number != frame_number) {
            return false;
        }
    }

    for (std::size_t slot_position = 0; slot_position < slots_.size(); ++slot_position) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (slot.pin_count < 0) {
            return false;
        }

        if (!cnr3_cache_slot_has_frame(slot)) {
            if (cnr3_cache_slot_id_is_valid(slot.slot_id)) {
                return false;
            }

            if (cnr3_frame_number_is_valid(slot.frame_number)) {
                return false;
            }

            if (slot.is_checkpoint) {
                return false;
            }

            if (slot.pin_count != 0) {
                return false;
            }

            continue;
        }

        if (!cnr3_cache_slot_id_is_valid(slot.slot_id)) {
            return false;
        }

        if (!cnr3_frame_number_is_valid(slot.frame_number)) {
            return false;
        }

        const auto frame_index_it = frame_index_.find(slot.frame_number);

        if (frame_index_it == frame_index_.end()) {
            return false;
        }

        if (frame_index_it->second != slot_position) {
            return false;
        }
    }

    std::size_t checkpoint_slot_count = 0;

    for (std::size_t slot_position = 0; slot_position < slots_.size(); ++slot_position) {
        if (slots_[slot_position].is_checkpoint) {
            ++checkpoint_slot_count;
        }
    }

    if (checkpoint_slot_count != checkpoint_slot_positions_.size()) {
        return false;
    }

    for (std::size_t checkpoint_index = 0;
        checkpoint_index < checkpoint_slot_positions_.size();
        ++checkpoint_index
        ) {
        const std::size_t checkpoint_slot_position =
            checkpoint_slot_positions_[checkpoint_index];

        if (checkpoint_slot_position >= slots_.size()) {
            return false;
        }

        for (std::size_t compare_index = checkpoint_index + 1U;
            compare_index < checkpoint_slot_positions_.size();
            ++compare_index
            ) {
            if (checkpoint_slot_positions_[compare_index] == checkpoint_slot_position) {
                return false;
            }
        }

        const Cnr3CacheSlot& slot = slots_[checkpoint_slot_position];

        if (!slot.is_checkpoint) {
            return false;
        }

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return false;
        }
    }

    return true;
}

bool Cnr3CachePinList::empty() const noexcept {
    return pin_count() == 0U;
}

std::size_t Cnr3CachePinList::pin_count() const noexcept {
    std::size_t valid_pin_count = 0;

    for (std::size_t pin_index = 0; pin_index < used_pin_count_; ++pin_index) {
        if (cnr3_cache_slot_pin_token_is_valid(pin_tokens_[pin_index])) {
            ++valid_pin_count;
        }
    }

    return valid_pin_count;
}

Cnr3Status Cnr3CachePinList::reserve_for_additional_pins(
    std::size_t additional_pin_count
) {
    if (additional_pin_count == 0U) {
        return Cnr3Status::ok;
    }

    if (additional_pin_count > (pin_tokens_.max_size() - used_pin_count_)) {
        return Cnr3Status::capacity_exceeded;
    }

    const std::size_t required_slot_count = used_pin_count_ + additional_pin_count;

    if (pin_tokens_.size() >= required_slot_count) {
        return Cnr3Status::ok;
    }

    try {
        pin_tokens_.resize(required_slot_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3CachePinList::record_pin(
    Cnr3CacheSlotPinToken& pin_token
) {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status reserve_status = reserve_for_additional_pins(1U);

    if (!cnr3_status_is_ok(reserve_status)) {
        return reserve_status;
    }

    return record_pin_without_allocation(pin_token);
}

Cnr3Status Cnr3CachePinList::record_pin_without_allocation(
    Cnr3CacheSlotPinToken& pin_token
) noexcept {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    if (used_pin_count_ >= pin_tokens_.size()) {
        return Cnr3Status::capacity_exceeded;
    }

    pin_tokens_[used_pin_count_] = pin_token;
    ++used_pin_count_;

    cnr3_cache_slot_pin_token_reset(pin_token);

    return Cnr3Status::ok;
}

Cnr3Status Cnr3CachePinList::discharge_all(
    Cnr3OutputCacheCore& cache
) {
    Cnr3Status first_failure_status = Cnr3Status::ok;

    for (std::size_t pin_index = 0; pin_index < used_pin_count_; ++pin_index) {
        Cnr3CacheSlotPinToken& pin_token = pin_tokens_[pin_index];

        if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
            continue;
        }

        const Cnr3Status unpin_status = cache.unpin_frame(pin_token);

        if (
            !cnr3_status_is_ok(unpin_status) &&
            cnr3_status_is_ok(first_failure_status)
            ) {
            first_failure_status = unpin_status;
        }
    }

    if (cnr3_status_is_ok(first_failure_status)) {
        for (std::size_t pin_index = 0; pin_index < used_pin_count_; ++pin_index) {
            cnr3_cache_slot_pin_token_reset(pin_tokens_[pin_index]);
        }

        used_pin_count_ = 0U;
    }

    return first_failure_status;
}

/*
    CMS07 cache-core implementation notes.

    CMS07-C.3C introduced the single non-recursive std::mutex skeleton.
    CMS07-C.3D split public read-only observers from private lock-protected
    observer helpers. CMS07-C.3E added a structural invariant observer.
    CMS07-C.4 introduced non-checkpoint store. CMS07-C.5 introduced
    immediate lookup/addref. CMS07-C.6 introduced clear/teardown detach.
    CMS07-C.7 introduced balanced slot pin/unpin mechanics. CMS07-C.8
    introduced explicit lookup-pin reservation. CMS07-D.1 introduced
    per-invocation pin-list record/discharge. CMS07-D.2A aligned AS comments
    with the CMS07 register. CMS07-D.3A introduced the AS1-compliant combined
    lookup-pin-record helper. CMS07-E.1A strengthened the isolated store helper
    proof. CMS07-E.2A reconciled the original E.2 lookup-pin-record obligation
    as satisfied by the D.3A combined helper; no second lookup-pin-record helper
    is required. CMS07-F.1A introduced the central single-slot remove helper.
    CMS07-F.2A introduced a bounded selected-detach proof for the AS5 batch
    shape. CMS07-F.3A introduced a narrow unpinned non-checkpoint selection
    proof. CMS07-F.4A introduced a checkpoint retention-boundary proof.
    CMS07-G.1A introduced cache policy constants. CMS07-G.2A introduced the
    hot-zone data model. CMS07-G.3A introduces hot-zone slide/spawn update
    behaviour; merge, retirement, and final prune-policy use remain deferred.

    The current public read-only observers acquire the mutex once at their outer
    boundary using RAII scoped guards.

    Future lock-protected helpers that are called from AS implementations must
    assume the caller already holds the mutex and must not acquire it
    themselves. Manual lock()/unlock() calls are forbidden. This is required to
    keep std::mutex non-recursive, to avoid AS self-deadlock, and to make
    accidental AS-boundary splitting structurally hard to write.

    The comments below are intentionally placed in this source file because AS
    implementation will live here initially. Keeping AS functions close to the
    state they protect reduces the risk of hiding or weakening the lock-scope
    contract.

    V5 firewall:
        VapourSynth's internal frame-reference count atomicity protects only a
        single addFrameRef/freeFrame operation. It does not make any CMS07 cache
        critical section optional, smaller, splittable, reorderable, or movable.

        The protected operation is the whole cache-state decision, not merely
        the frame-reference bump. A find-and-pin-record operation, for example,
        is a single cache-lock operation because the find, pin-count increment,
        and pin-list record must not be separated by a prune window.

    Atomic-scope rule:
        CMS07 AS1-AS7 are designer-owned, mandatory, indivisible cache-lock
        scopes. Do not shrink, split, merge, reorder, or reinterpret them
        without explicit CMS update / user approval.

    Slow work rule:
        Source requests, source retrieval, pixel compute, batch freeFrame after
        detach, diagnostic formatting/printing, and heap-heavy summary
        construction must happen outside CMS07 cache atomic/locked scopes.
*/

/*
    CMS07 8.7 atomic-scope register copy.

    This block is the source-code copy of the CMS07.0 8.7 AS1-AS7 register.
    It is intentionally not paraphrased; only C++ comment indentation and
    ASCII punctuation are normalised for source style. If this block and
    CMS07.0 differ, update this block from the CMS rather than editing the
    rule locally.

    Atomic-scope register - DESIGNER-OWNED, MANDATORY, boundaries inviolable.

    Each entry is one indivisible cache-lock critical section. The coder
    IMPLEMENTS these exactly; the coder may NOT shrink, split, merge, or
    reorder the contents of a scope without explicit designer agreement. All
    slow work (pixel compute, VS source requests, the batch freeFrame) is
    OUTSIDE these scopes by mandate.

    AS1  arInitial plan-and-pin (Section 9.1):
           { Phase-1 descending bounded search [max(0,N-B), N];
             pin the start point and every present reused frame;
             catalogue the output holes;
             append every pin to frameData pin-list;
             update/slide hot zone(s) for N }
           - one lock acquisition, indivisible.

    AS2  arAllFramesReady per-hole store-and-pin (Section 9.2), repeated per hole:
           { first-in-best-dressed check;
             store computed output (or adopt existing winner);
             pin it; append to pin-list }
           - one lock acquisition PER hole; compute happens OUTSIDE before this.

    AS3  reused-frame pin during ascending fill (Section 9.2/9.5), as encountered:
           { confirm output[K] present; addFrameRef under lock; append to pin-list }
           - find-and-pin is one indivisible unit (the find-and-add-ref primitive, D06).

    AS4  final unpin (Section 4.5):
           { for every entry on the pin-list: unpin (decrement) }
           - one lock acquisition for the whole list at end of arAllFramesReady.

    AS5  bounded prune decide+detach (Section 7.3 a+b):
           { evaluate composite eviction predicate;
             select up to K victims (greatest-distance-first);
             detach each victim slot from the index (central remove helper, D09);
             collect freed VSFrame* refs into a local list }
           - one lock acquisition; the batch freeFrame (c) is OUTSIDE this scope.

    AS6  checkpoint establish (Sections 6.3/6.4):
           { on store of a grid frame or a detected-cut frame, set is_checkpoint;
             insert into checkpoint pool / ordered index }
           - folded into the relevant AS2 store scope (same lock), not a separate lock.

    AS7  zone retirement / merge (Sections 5.5/5.6):
           { test no-pins-in-range + decay-margin; mark zone inactive / merge }
           - performed under the same lock during AS1 or the prune pass; never split.

    The register is the single authority on critical-section boundaries. If an
    operation needed in practice is not covered here, it is raised to the
    designer - NOT improvised with an ad-hoc smaller lock.
*/
