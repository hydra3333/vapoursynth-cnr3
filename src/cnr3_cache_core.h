#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "cnr3_common.h"
#include "cnr3_owned_frame_ref.h"

/*
    CNR3 cache-core data model skeleton.

    CMS07-C.1 introduces the first real cache-core data types. It does not
    introduce cache behaviour.

    This module is the CMS07 cache-manager core boundary.

    Future responsibilities of this module include:
        - cache slot ownership;
        - ordered frame-number index;
        - non-checkpoint and checkpoint pools;
        - consumer pins and per-invocation pin-lists;
        - hot-zone state;
        - bounded recovery planning;
        - store / lookup / remove helpers;
        - prune / eviction;
        - integrity validation;
        - teardown / clear discipline.

    This module must not contain:
        - pixel loops;
        - response-table construction;
        - VapourSynth getFrame request/retrieve lifecycle code;
        - VSMap parsing;
        - D-SUM formatting or printing;
        - old strict-streaming authority;
        - CMS06 output-cache-manager state.

    VapourSynth frame-reference ownership may appear here only through the
    Cnr3OwnedFrameRef boundary. CMS07-C.1 stores the ownership wrapper in the
    slot model, and must not call freeFrame() while cache_mutex_ is held,
    prune, or return-transfer logic.

    Position B reminder:
        A cache slot and its owned frame reference are managed as one unit. The
        cache slot is the liveness/index unit for active cached output frames.
        Do not split frame ownership away from the slot unless a later explicit
        CMS update approves an exception.
*/

/*
    Stable slot identifiers are introduced as a named type now so future
    diagnostics and selftests can distinguish slot identity from frame number
    and vector position.

    CMS07-C.1 does not yet allocate reusable slot IDs. The field exists so the
    data model has a clear place for that invariant when slot creation begins.
*/
struct Cnr3CacheSlotId {
    std::uint64_t value = 0;
};

[[nodiscard]] constexpr bool cnr3_cache_slot_id_is_valid(
    Cnr3CacheSlotId slot_id
) noexcept {
    return slot_id.value != 0U;
}

/*
    Slot pin token.

    A pin protects cache-slot liveness only. It is not a VapourSynth frame
    reference and does not call addFrameRef(), freeFrame(), or transfer a frame.

    The token binds the caller-held pin to the slot identity observed at pin
    time. Later unpin must match both frame number and slot ID, so stale or
    mismatched tokens cannot silently decrement the wrong slot.
*/
struct Cnr3CacheSlotPinToken {
    Cnr3CacheSlotId slot_id{};
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
};

[[nodiscard]] constexpr bool cnr3_cache_slot_pin_token_is_valid(
    Cnr3CacheSlotPinToken pin_token
) noexcept {
    return cnr3_cache_slot_id_is_valid(pin_token.slot_id) &&
        cnr3_frame_number_is_valid(pin_token.frame_number);
}

constexpr void cnr3_cache_slot_pin_token_reset(
    Cnr3CacheSlotPinToken& pin_token
) noexcept {
    pin_token = Cnr3CacheSlotPinToken{};
}

/*
    Slot-ID source.

    CMS07-C.3 introduces only the ID source used by future cache-slot creation.
    It does not create slots and does not insert anything into the cache index.

    IDs are diagnostic/stability aids for slot identity. They are distinct from:
        - frame number;
        - vector position;
        - checkpoint index position.

    The source uses std::uint64_t to avoid signed-overflow undefined behaviour
    and to make realistic long-run slot churn effectively irrelevant. The
    source wraps back to 1 after UINT64_MAX only as a theoretical fallback.
    Future live-slot collision checks belong in the slot creation/store phase,
    not in this isolated ID source.

    Threading rule:
        Cnr3CacheSlotIdSource is not independently thread-safe.

        In production cache code, allocate() must be called only while holding
        the Cnr3OutputCacheCore mutex as part of the larger CMS07 atomic scope
        that creates/inserts the slot. Making this counter atomic would not be
        sufficient, because slot identity, slot storage, frame-number index
        insertion, checkpoint state, and pin visibility must be updated as one
        protected cache-state operation.

        The isolated selftest may instantiate a local Cnr3CacheSlotIdSource
        without a mutex because that object is not shared between threads.
*/
class Cnr3CacheSlotIdSource {
public:
    Cnr3CacheSlotIdSource() noexcept = default;

    [[nodiscard]] Cnr3CacheSlotId allocate() noexcept;

    /*
        Diagnostic/test visibility only.

        This does not reserve an ID.
    */
    [[nodiscard]] std::uint64_t next_value_for_diagnostics() const noexcept;

private:
    std::uint64_t next_value_ = 1;
};

/*
    A cache slot is the unit that binds:
        - output frame number;
        - owned VapourSynth frame reference;
        - checkpoint protection state;
        - active consumer pin count.

    CMS07-C.1 introduced the fields only. CMS07-C.7 adds balanced pin/unpin
    operations without adding checkpoint promotion, prune, or teardown policy.
*/
struct Cnr3CacheSlot {
    Cnr3CacheSlotId slot_id{};
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    Cnr3OwnedFrameRef frame{};
    bool is_checkpoint = false;
    int pin_count = 0;
};

[[nodiscard]] bool cnr3_cache_slot_has_frame(
    const Cnr3CacheSlot& slot
) noexcept;

[[nodiscard]] bool cnr3_cache_slot_is_indexable(
    const Cnr3CacheSlot& slot
) noexcept;

/*
    Ordered frame-number index.

    The value is the vector position of the slot in Cnr3OutputCacheCore::slots_.
    This intentionally remains an internal implementation detail. Future phases
    may replace the storage strategy without changing cache semantics.

    The index is ordered by frame number so future bounded descending recovery
    search can use ordered traversal rather than global nearest-then-reject
    behaviour.
*/
using Cnr3CacheFrameIndex = std::map<int, std::size_t>;

class Cnr3CachePinList;

/*
    CMS07 output cache core.

    CMS07-C.1 introduced the initial data containers. CMS07-C.3C introduces the
    single cache-core mutex skeleton before the first real mutating cache
    operation.

    Mutex model:
        - exactly one cache-core mutex protects cache state;
        - the mutex type is std::mutex;
        - std::mutex is intentionally non-recursive;
        - no AS scope may re-enter the cache lock;
        - public lock-owning operations acquire the mutex once at their outer
          boundary;
        - lock-protected internal helpers must assume the caller already holds
          the mutex and must not acquire it themselves;
        - all cache-core lock acquisition must use an RAII scoped guard such as
          std::lock_guard or std::scoped_lock;
        - manual lock()/unlock() calls are forbidden.

    This keeps a plain non-recursive std::mutex viable and prevents accidental
    self-deadlock. It also avoids the false safety of making isolated fields
    atomic while leaving compound cache invariants unprotected.

    The current read-only query helpers acquire the mutex themselves because
    they are public lock-owning observers. Future internal AS helpers must not
    call these public observers while already holding the cache lock.

    No mutating cache operation is introduced in CMS07-C.3C.
*/
class Cnr3OutputCacheCore {
public:
    Cnr3OutputCacheCore() = default;
    ~Cnr3OutputCacheCore() = default;

    Cnr3OutputCacheCore(const Cnr3OutputCacheCore&) = delete;
    Cnr3OutputCacheCore& operator=(const Cnr3OutputCacheCore&) = delete;

    /*
        The cache core is intentionally not movable.

        Moving a live cache core would also move/replace the mutex and all
        cache-state containers, which is not a supported runtime operation.
        Cache lifetime is per filter instance.
    */
    Cnr3OutputCacheCore(Cnr3OutputCacheCore&&) = delete;
    Cnr3OutputCacheCore& operator=(Cnr3OutputCacheCore&&) = delete;

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t slot_count() const;
    [[nodiscard]] std::size_t index_count() const;
    [[nodiscard]] std::size_t checkpoint_count() const;

    /*
        Lock-owning diagnostic/test observer for active slot pins.

        Pins are cache-slot liveness reservations only. They are not frame
        references and must not be confused with lookup addrefs or checkpoints.
    */
    [[nodiscard]] int total_pin_count() const;

    /*
        Lock-owning invariant observer.

        This is a read-only public observer. It acquires cache_mutex_ once at
        its outer boundary and then calls the lock-protected invariant helper.

        Future AS/mutating code that already holds cache_mutex_ must call
        cache_state_invariants_hold_locked() directly, not this public observer.
    */
    [[nodiscard]] bool cache_state_invariants_hold() const;

    /*
        Lock-owning non-checkpoint store operation.

        The caller passes an already-owned frame reference. The cache core does
        not call addFrameRef(). On successful store, this operation consumes the
        Cnr3OwnedFrameRef and the cache owns the retained frame.

        If the store is rejected, the incoming owned frame remains in the public
        wrapper and is released after cache_mutex_ is unlocked. This preserves
        the CMS07 rule that slow frame release work is not performed inside the
        cache atomic scope.

        Duplicate frame numbers preserve first-in-best-dressed behaviour: the
        existing cache slot remains authoritative and the rejected incoming
        frame is released by the caller-side wrapper after the lock scope exits.

        This is an isolated store primitive, not the complete AS2
        store-and-pin-record operation. Future AS2 consumer code must use a
        combined AS2 helper that performs store/adopt, pin, pin-list record, and
        checkpoint establishment inside one per-hole cache-lock scope.
    */
    [[nodiscard]] Cnr3Status store_noncheckpoint_owned_frame(
        int frame_number,
        Cnr3OwnedFrameRef frame
    );

    /*
        Lock-owning immediate lookup/addref operation.

        This operation is for immediate returned-frame ownership only. It does
        not pin the cache slot and does not reserve slot liveness for a future
        consumer.

        On hit, the cache core calls VSAPI::addFrameRef() while holding
        cache_mutex_ so the frame pointer and cache index cannot be invalidated
        between lookup and reference acquisition. The acquired reference is then
        adopted by out_frame after cache_mutex_ has been released.

        out_frame must be empty on entry. Replacing an already-owned caller
        reference is deliberately rejected so this operation never releases a
        caller-owned frame while holding cache_mutex_.

        On miss, no reference is acquired and out_frame remains empty.
    */
    [[nodiscard]] Cnr3Status lookup_frame_and_add_ref(
        int frame_number,
        const VSAPI* vsapi,
        Cnr3OwnedFrameRef& out_frame
    ) const;

    /*
        Lock-owning AS1 lookup-pin-record operation.

        This is the CMS07-D.3A compliant AS1 primitive for cached-frame reuse.
        It reserves pin-list capacity before acquiring cache_mutex_, then under
        one cache-lock acquisition it finds the frame-number index entry,
        validates the slot, increments the matching slot pin count, and records
        the pin in pin_list without allocation.

        This operation does not call addFrameRef(), freeFrame(), or transfer a
        VSFrame. It reserves slot liveness only.
    */
    [[nodiscard]] Cnr3Status lookup_frame_and_record_pin(
        int frame_number,
        Cnr3CachePinList& pin_list
    );

    /*
        Lock-owning slot unpin operation.

        The token must have been recorded by a cache-core AS helper and later
        supplied by Cnr3CachePinList::discharge_all(). On success, the slot pin
        count is decremented and the token is invalidated.

        A token mismatch is a lifecycle violation because decrementing the wrong
        slot would break cache liveness accounting.
    */
    [[nodiscard]] Cnr3Status unpin_frame(
        Cnr3CacheSlotPinToken& pin_token
    );

    /*
        Lock-owning clear operation.

        This operation detaches cached slots and clears frame-index/checkpoint
        metadata while holding cache_mutex_. Detached Cnr3OwnedFrameRef objects
        are then destroyed after cache_mutex_ has been released.

        This preserves the CMS07 §8.2 rule: no VSAPI::freeFrame work is done
        inside the cache atomic scope.
    */
    [[nodiscard]] Cnr3Status clear();

private:
    /*
        Lock-protected observer helpers.

        These helpers assume the caller already holds cache_mutex_. They must
        not acquire cache_mutex_ themselves.

        Public observers may acquire the mutex once at their outer boundary and
        then call these helpers. Future AS/mutating code that already holds the
        mutex must call these helpers directly, not the public observers, to
        avoid re-entering the non-recursive mutex.
    */
    [[nodiscard]] bool empty_locked() const noexcept;
    [[nodiscard]] std::size_t slot_count_locked() const noexcept;
    [[nodiscard]] std::size_t index_count_locked() const noexcept;
    [[nodiscard]] std::size_t checkpoint_count_locked() const noexcept;
    [[nodiscard]] int total_pin_count_locked() const noexcept;

    /*
        Lock-protected cache-state invariant helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        The invariant checked here is structural only. It verifies that the
        slot vector, frame index, checkpoint-position list, live slot metadata,
        and basic pin counts are mutually consistent. It does not perform
        VapourSynth reference-count checks, ownership accounting, recovery
        planning, pruning, or pixel validation.
    */
    [[nodiscard]] bool cache_state_invariants_hold_locked() const noexcept;

    /*
        Lock-protected non-checkpoint store helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        The helper may move from frame only on successful insertion. On rejected
        paths, frame remains owned by the public wrapper so it can be released
        after cache_mutex_ is unlocked.
    */
    [[nodiscard]] Cnr3Status store_noncheckpoint_owned_frame_locked(
        int frame_number,
        Cnr3OwnedFrameRef& frame
    );

    /*
        Lock-protected immediate lookup/addref helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        On hit, this helper calls VSAPI::addFrameRef() while cache_mutex_ is
        held and writes the acquired raw reference to out_acquired_frame. The
        public wrapper adopts that acquired reference into Cnr3OwnedFrameRef
        after the lock scope exits.

        This helper does not pin the slot. AS1 lookup-pin-record reservation is
        handled by lookup_frame_and_record_pin_locked().
    */
    [[nodiscard]] Cnr3Status lookup_frame_and_add_ref_locked(
        int frame_number,
        const VSAPI* vsapi,
        const VSFrame** out_acquired_frame
    ) const;

    /*
        Lock-protected AS1 lookup-pin-record helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. pin_list must have enough pre-reserved
        storage for one more token before this helper is called. The in-lock
        record operation must not allocate.
    */
    [[nodiscard]] Cnr3Status lookup_frame_and_record_pin_locked(
        int frame_number,
        Cnr3CachePinList& pin_list
    );

    /*
        Lock-protected slot pin helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        The helper increments only slot.pin_count. It must not call
        VSAPI::addFrameRef(), VSAPI::freeFrame(), or move a Cnr3OwnedFrameRef.
    */
    [[nodiscard]] Cnr3Status pin_frame_locked(
        int frame_number,
        Cnr3CacheSlotPinToken& out_pin_token
    );

    /*
        Lock-protected slot unpin helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        The token must match the current slot identity before pin_count is
        decremented.
    */
    [[nodiscard]] Cnr3Status unpin_frame_locked(
        Cnr3CacheSlotPinToken& pin_token
    );

    /*
        Lock-protected clear helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        The helper swaps the slot vector into detached_slots and clears cache
        index/checkpoint metadata. The detached slots must be destroyed by the
        public wrapper after cache_mutex_ has been released, so retained
        Cnr3OwnedFrameRef objects release their VSFrame references outside the
        cache atomic scope.

        detached_slots must be empty on entry.
    */
    [[nodiscard]] Cnr3Status clear_locked(
        std::vector<Cnr3CacheSlot>& detached_slots
    );

    /*
        Single CMS07 cache-core mutex.

        This is a non-recursive std::mutex.

        Lock acquisition rule:
            All cache-core lock acquisition must use an RAII scoped guard, such
            as std::lock_guard or std::scoped_lock.

            Manual lock()/unlock() calls are forbidden. They make it too easy to
            split a CMS07 atomic scope accidentally, leak a held lock through an
            early return, or reorder a protected operation without noticing.

        AS re-entry rule:
            No AS scope may re-enter the cache lock. Public lock-owning
            operations acquire the mutex once at the outer boundary.

        Lock-protected helper rule:
            Internal helpers that require lock protection must assume the caller
            already holds cache_mutex_. They must not acquire cache_mutex_
            themselves.

        Do not change this to std::recursive_mutex. A recursive mutex would hide
        accidental AS re-entry and weaken the single-lock design discipline.
    */
    mutable std::mutex cache_mutex_{};

    std::vector<Cnr3CacheSlot> slots_{};
    Cnr3CacheFrameIndex frame_index_{};
    std::vector<std::size_t> checkpoint_slot_positions_{};
    Cnr3CacheSlotIdSource slot_id_source_{};
};

/*
    Per-invocation cache pin list.

    This list owns slot-pin tokens recorded during one cache-core activation.
    Recording a token consumes the caller token by resetting it after the list
    has stored its own copy. Discharge walks the recorded tokens and releases
    each still-valid token through Cnr3OutputCacheCore::unpin_frame().

    The list is deliberately not copyable or movable. A copied pin list would
    duplicate pin-token ownership and could cause double-unpin attempts.

    This is cache-core lifecycle infrastructure only. It does not call
    addFrameRef(), freeFrame(), transfer a VSFrame, perform getFrame work,
    compute pixels, prune, create checkpoints, or perform recovery.
*/
class Cnr3CachePinList {
public:
    Cnr3CachePinList() = default;
    ~Cnr3CachePinList() = default;

    Cnr3CachePinList(const Cnr3CachePinList&) = delete;
    Cnr3CachePinList& operator=(const Cnr3CachePinList&) = delete;

    Cnr3CachePinList(Cnr3CachePinList&&) = delete;
    Cnr3CachePinList& operator=(Cnr3CachePinList&&) = delete;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t pin_count() const noexcept;

    /*
        Pre-reserve storage for additional pin records.

        This operation may allocate and must be called before entering a CMS07
        AS cache-lock scope. The AS1 combined helper relies on this to make the
        in-lock append bounded, deterministic, and allocation-free.
    */
    [[nodiscard]] Cnr3Status reserve_for_additional_pins(
        std::size_t additional_pin_count
    );

    /*
        Record one already-acquired slot pin.

        pin_token must be valid on entry. On success, this function stores its
        own copy and resets pin_token so the caller no longer owns that pin
        token. If storage allocation fails, pin_token remains valid and the
        caller remains responsible for releasing it.

        This separate call is not AS1/AS2 compliant when paired with a cache
        pin operation. Use Cnr3OutputCacheCore::lookup_frame_and_record_pin()
        for AS1 cached-frame reuse.
    */
    [[nodiscard]] Cnr3Status record_pin(
        Cnr3CacheSlotPinToken& pin_token
    );

    /*
        Release every still-valid token recorded in this list.

        A clean discharge invalidates all recorded entries and leaves the list
        empty, so a second discharge is a no-op success. If any unpin fails, the
        first failure status is returned after all entries have been attempted.
    */
    [[nodiscard]] Cnr3Status discharge_all(
        Cnr3OutputCacheCore& cache
    );

private:
    friend class Cnr3OutputCacheCore;

    /*
        Record one pin into already-reserved storage.

        This must not allocate. It exists so a cache-core AS helper can append
        the pin-list record inside the same cache-lock acquisition as the slot
        pin-count increment.
    */
    [[nodiscard]] Cnr3Status record_pin_without_allocation(
        Cnr3CacheSlotPinToken& pin_token
    ) noexcept;

    std::vector<Cnr3CacheSlotPinToken> pin_tokens_{};
    std::size_t used_pin_count_ = 0;
};
