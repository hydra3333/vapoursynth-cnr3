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
    lookup-pin-record helper.

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
