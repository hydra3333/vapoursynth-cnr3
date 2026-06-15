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
    slot model, but does not yet call addFrameRef(), freeFrame(), lookup, store,
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

    CMS07-C.1 introduces the fields only. It does not implement store, lookup,
    pin, unpin, checkpoint promotion, prune, or teardown behaviour.
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
