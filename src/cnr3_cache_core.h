#pragma once

#include <cstddef>
#include <map>
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
    int value = 0;
};

[[nodiscard]] constexpr bool cnr3_cache_slot_id_is_valid(
    Cnr3CacheSlotId slot_id
) noexcept {
    return slot_id.value > 0;
}

/*
    Slot-ID source.

    CMS07-C.3 introduces only the ID source used by future cache-slot creation.
    It does not create slots and does not insert anything into the cache index.

    IDs are diagnostic/stability aids for slot identity. They are distinct from:
        - frame number;
        - vector position;
        - checkpoint index position.

    The source wraps back to 1 after INT_MAX. That is acceptable for this
    single-process diagnostic identity source because slot IDs are not used as
    long-term persistent identifiers. Future live-slot collision checks belong
    in the slot creation/store phase, not in this isolated ID source.
*/
class Cnr3CacheSlotIdSource {
public:
    Cnr3CacheSlotIdSource() noexcept = default;

    [[nodiscard]] Cnr3CacheSlotId allocate() noexcept;

    /*
        Diagnostic/test visibility only.

        This does not reserve an ID.
    */
    [[nodiscard]] int next_value_for_diagnostics() const noexcept;

private:
    int next_value_ = 1;
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

    CMS07-C.1 owns the data containers only. It provides read-only mechanical
    queries so selftests and later phases can verify the empty model compiles
    and has the expected initial state.

    No lock is introduced in CMS07-C.1. Mutex/atomic-scope implementation starts
    in a later explicit phase, when the first mutating cache operation is added.
*/
class Cnr3OutputCacheCore {
public:
    Cnr3OutputCacheCore() = default;
    ~Cnr3OutputCacheCore() = default;

    Cnr3OutputCacheCore(const Cnr3OutputCacheCore&) = delete;
    Cnr3OutputCacheCore& operator=(const Cnr3OutputCacheCore&) = delete;

    Cnr3OutputCacheCore(Cnr3OutputCacheCore&&) noexcept = default;
    Cnr3OutputCacheCore& operator=(Cnr3OutputCacheCore&&) noexcept = default;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t slot_count() const noexcept;
    [[nodiscard]] std::size_t index_count() const noexcept;
    [[nodiscard]] std::size_t checkpoint_count() const noexcept;

private:
    std::vector<Cnr3CacheSlot> slots_{};
    Cnr3CacheFrameIndex frame_index_{};
    std::vector<std::size_t> checkpoint_slot_positions_{};
    Cnr3CacheSlotIdSource slot_id_source_{};
};
