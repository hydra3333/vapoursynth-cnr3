#include "cnr3_cache_core.h"

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
    std::is_nothrow_move_assignable_v<Cnr3OwnedFrameRef>,
    "Cnr3OwnedFrameRef must be nothrow move-assignable for cache slot adoption."
    );

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

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_pin(
    int frame_number,
    Cnr3CacheSlotPinToken& out_pin_token
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (cnr3_cache_slot_pin_token_is_valid(out_pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return lookup_frame_and_pin_locked(frame_number, out_pin_token);
}

Cnr3Status Cnr3OutputCacheCore::pin_frame(
    int frame_number,
    Cnr3CacheSlotPinToken& out_pin_token
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (cnr3_cache_slot_pin_token_is_valid(out_pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return pin_frame_locked(frame_number, out_pin_token);
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
        checkpoint_slot_positions_.empty();
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

Cnr3Status Cnr3OutputCacheCore::store_noncheckpoint_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame
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

    if (frame_index_.find(frame_number) != frame_index_.end()) {
        return Cnr3Status::duplicate;
    }

    const std::size_t slot_position = slots_.size();

    if (slot_position >= slots_.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        slots_.reserve(slot_position + 1U);

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
    slot.is_checkpoint = false;
    slot.pin_count = 0;

    slots_.push_back(std::move(slot));

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

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_pin_locked(
    int frame_number,
    Cnr3CacheSlotPinToken& out_pin_token
) {
    return pin_frame_locked(frame_number, out_pin_token);
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

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

bool Cnr3OutputCacheCore::cache_state_invariants_hold_locked() const noexcept {
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

    for (const std::size_t checkpoint_slot_position : checkpoint_slot_positions_) {
        if (checkpoint_slot_position >= slots_.size()) {
            return false;
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

/*
    CMS07 cache-core implementation placeholder.

    Mutating cache-core functions start in later CMS07-C subphases.

    CMS07-C.3C introduced the single non-recursive std::mutex skeleton before
    mutating functions exist. CMS07-C.3D split public read-only observers from
    private lock-protected observer helpers. CMS07-C.3E added a structural
    invariant observer so future mutating phases have a concrete cache-state
    consistency check to call. CMS07-C.4 introduced the first real mutator:
    storing one owned, non-checkpoint output frame as a slot/index unit.
    
    CMS07-C.5 introduced immediate lookup/addref for caller-owned lookup
    results without introducing slot pins. CMS07-C.6 introduced explicit
    clear/teardown detach: cached slots are detached under cache_mutex_, then
    retained frame references are released outside the lock by Cnr3OwnedFrameRef
    destruction. CMS07-C.7 introduces balanced slot pin/unpin mechanics without
    introducing lookup-pin reservation, prune, checkpoint promotion, recovery,
    or getFrame wiring.

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
        the frame-reference bump. A future find-and-pin operation, for example,
        is a single cache-lock operation because the find and the add-ref/pin
        record must not be separated by a prune window.

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
    CMS07 AS1 - arInitial plan-and-pin.

    What must happen inside the lock:
        - Phase-1 descending bounded search [max(0, N - B), N].
        - Pin the start point and every present reused frame.
        - Catalogue output holes.
        - Append every pin to the frameData pin-list.
        - Update/slide hot zone(s) for N.

    What must not happen inside the lock:
        - VapourSynth source requests.
        - VapourSynth source retrieval.
        - Pixel compute.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition, indivisible.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS2 - arAllFramesReady per-hole store-and-pin.

    What must happen inside the lock:
        - First-in-best-dressed check.
        - Store computed output or adopt the existing winner.
        - Pin the stored/adopted output.
        - Append the pin to the frameData pin-list.
        - If the stored frame is a grid checkpoint or detected-cut checkpoint,
          establish checkpoint state in this same store scope.

    What must not happen inside the lock:
        - Pixel compute.
        - Source requests or retrieval.
        - Batch freeFrame.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition per hole. Compute happens outside before this
        scope. AS6 checkpoint establishment is folded into this store scope, not
        implemented as a separate lock.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS3 - reused-frame pin during ascending fill.

    What must happen inside the lock:
        - Confirm output[K] is present.
        - Add/retain the frame reference under the cache lock.
        - Append the pin to the frameData pin-list.

    What must not happen inside the lock:
        - Pixel compute.
        - Source requests or retrieval.
        - Diagnostic formatting or printing.

    Lock rule:
        Find-and-pin is one indivisible unit. Do not separate the lookup from
        the add-ref/pin record.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS4 - final unpin.

    What must happen inside the lock:
        - For every entry on the frameData pin-list, unpin/decrement exactly
          once.
        - Discharge the pin-list entries so cleanup cannot unpin them again.

    What must not happen inside the lock:
        - Pixel compute.
        - Source requests or retrieval.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition for the whole pin-list at the end of
        arAllFramesReady.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS5 - bounded prune decide-and-detach.

    What must happen inside the lock:
        - Evaluate the composite eviction predicate.
        - Select up to K victims, greatest-distance-first.
        - Detach each victim slot from the index using the central remove
          helper.
        - Collect freed VSFrame references into a local list for later release.

    What must not happen inside the lock:
        - Batch freeFrame.
        - Pixel compute.
        - Source requests or retrieval.
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        One lock acquisition for decide-and-detach. The batch freeFrame work is
        outside this scope.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/

/*
    CMS07 AS6 - checkpoint establish.

    What must happen inside the lock:
        - On store of a grid frame or detected-cut frame, set is_checkpoint.
        - Insert the frame into the checkpoint pool / ordered checkpoint index.

    Lock rule:
        AS6 is not a separate lock. It is folded into the relevant AS2
        store-and-pin scope using the same lock.

    Do not split AS6 out of AS2 without explicit CMS update / user approval.
*/

/*
    CMS07 AS7 - zone retirement / merge.

    What must happen inside the lock:
        - Test no-pins-in-range plus decay margin.
        - Mark a zone inactive/retired or merge zones as required.

    What must not happen inside the lock:
        - Diagnostic formatting or printing.
        - Heap-heavy summary construction.

    Lock rule:
        Performed under the same lock during AS1 or the prune pass. Never split
        into an ad-hoc separate lock.

    Do not split, merge, reorder, or reinterpret this scope without explicit
    CMS update / user approval.
*/
