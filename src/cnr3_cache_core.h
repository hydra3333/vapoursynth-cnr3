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
    CMS07 cache policy constants.

    CMS07-G.1A defines the policy numbers and their coherence relationships
    only. These constants do not by themselves implement active-ceiling
    calculation, hot-zone state, prune candidate ordering, recovery, AS2, or
    VapourSynth getFrame wiring.
*/

/*
    The active cache ceiling is derived later from this nominal byte budget and
    the actual output frame byte size, then clamped to the hard min/max below.
*/
inline constexpr std::uint64_t CNR3_CACHE_BYTE_BUDGET_BYTES =
    1024ULL * 1024ULL * 1024ULL;

/*
    Active-ceiling clamp bounds.

    CR4 active_ceiling >= ~2 x max-protected set, where max-protected ~=
    MAX_HOT_ZONES x (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool ~=
    5 x 60 + 48 = ~348; 1000 >> 348. If pruning can never reach target,
    this rule is violated.
*/
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES = 150U;
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 1000U;

/*
    Non-checkpoint prune hysteresis factor, represented as an exact rational to
    avoid introducing floating-point policy into cache-core integer accounting.
*/
inline constexpr std::size_t CNR3_CACHE_OVERFLOW_FACTOR_NUMERATOR = 11U;
inline constexpr std::size_t CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR = 10U;

/*
    Checkpoint establishment and retention. Grid checkpoints are every interval
    frames plus frame 0; cut checkpoints add irregular density on top.

    CR5 CHECKPOINT_MAX_RETAIN >= MAX_HOT_ZONES x
    (BACK_RADIUS / INTERVAL) = 25 grid checkpoints, treated as a density FLOOR
    because cuts add irregular checkpoints on top. MAX_RETAIN raised 32 -> 48:
    25 grid floor + ~23 headroom for scene-cut checkpoints within the
    ~300-frame protected span. Content-dependent starting value; if cut-heavy
    material keeps the pool pinned at 48 with prune unable to reduce, raise it.
    MIN_RETAIN unchanged at 10.
*/
inline constexpr int CNR3_CACHE_CHECKPOINT_INTERVAL = 10;
inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_MIN_RETAIN = 10U;
inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_MAX_RETAIN = 48U;

/*
    Hot-zone radii.

    CR2 BACK_RADIUS >= bounded recovery search window B; ideally = B
    (currently both 50). AND B must exceed the effective settling length of the
    recursive chroma blend, so the floor-approximation fresh-start in Section
    9.5 is invisible at N. If B were shrunk for memory, confirm it still
    exceeds the blend settling length, or the approximate start would show at
    the output.

    CR3 BACK_RADIUS ~= 5 x CHECKPOINT_INTERVAL; a zone covers about five grid
    anchors. 50 = 5 x 10.
*/
inline constexpr int CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS = 10;
inline constexpr int CNR3_CACHE_HOT_ZONE_BACK_RADIUS = 50;
inline constexpr int CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS =
    CNR3_CACHE_HOT_ZONE_BACK_RADIUS;

/*
    Hot-zone count. MAX_HOT_ZONES scales with concurrent distinct access
    regions, not thread count.
*/
inline constexpr std::size_t CNR3_CACHE_MAX_HOT_ZONES = 5U;

/*
    CR1 JUMP_THRESHOLD is DERIVED = FORWARD_RADIUS + BACK_RADIUS + 1; never set
    independently.
*/
inline constexpr int CNR3_CACHE_JUMP_THRESHOLD =
    CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS +
    CNR3_CACHE_HOT_ZONE_BACK_RADIUS +
    1;

/*
    decay_margin bound: FORWARD_RADIUS <= decay_margin <= BACK_RADIUS
    (10 <= 20 <= 50); far below active_ceiling.
*/
inline constexpr int CNR3_CACHE_HOT_ZONE_DECAY_MARGIN = 20;

/*
    Bounded-prune victim cap. This bounds one AS5 decide/detach pass; the later
    full prune policy decides when and which candidates to offer.
*/
inline constexpr std::size_t CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS = 8U;

inline constexpr std::size_t CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE =
    CNR3_CACHE_MAX_HOT_ZONES *
    static_cast<std::size_t>(
        CNR3_CACHE_HOT_ZONE_BACK_RADIUS +
        CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS
    ) +
    CNR3_CACHE_CHECKPOINT_MAX_RETAIN;

inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE =
    CNR3_CACHE_MAX_HOT_ZONES *
    static_cast<std::size_t>(
        CNR3_CACHE_HOT_ZONE_BACK_RADIUS /
        CNR3_CACHE_CHECKPOINT_INTERVAL
    );

static_assert(CNR3_CACHE_BYTE_BUDGET_BYTES == 1073741824ULL);
static_assert(CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES > 0U);
static_assert(
    CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES <=
    CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES
    );
static_assert(CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR != 0U);
static_assert(
    CNR3_CACHE_OVERFLOW_FACTOR_NUMERATOR >
    CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR
    );
static_assert(CNR3_CACHE_CHECKPOINT_INTERVAL > 0);
static_assert(
    CNR3_CACHE_CHECKPOINT_MIN_RETAIN <=
    CNR3_CACHE_CHECKPOINT_MAX_RETAIN
    );
static_assert(
    CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS <=
    CNR3_CACHE_HOT_ZONE_BACK_RADIUS
    );
static_assert(
    CNR3_CACHE_HOT_ZONE_BACK_RADIUS ==
    (5 * CNR3_CACHE_CHECKPOINT_INTERVAL)
    );
static_assert(
    CNR3_CACHE_JUMP_THRESHOLD ==
    (CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS +
     CNR3_CACHE_HOT_ZONE_BACK_RADIUS +
     1)
    );
static_assert(
    CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES >=
    (2U * CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE)
    );
static_assert(
    CNR3_CACHE_CHECKPOINT_MAX_RETAIN >=
    CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE
    );
static_assert(
    CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS <=
    CNR3_CACHE_HOT_ZONE_DECAY_MARGIN
    );
static_assert(
    CNR3_CACHE_HOT_ZONE_DECAY_MARGIN <=
    CNR3_CACHE_HOT_ZONE_BACK_RADIUS
    );
static_assert(CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS > 0U);

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
    Hot-zone data model.

    CMS07-G.2A introduces only the hot-zone storage shape and invariants. It
    does not slide, spawn, merge, retire, or apply zones to prune policy.

    A live hot zone is a prune-policy hint for anticipated or recently observed
    access, not a correctness/liveness guarantee. Consumer pins remain the only
    active-frame liveness mechanism.
*/
struct Cnr3CacheHotZone {
    bool is_active = false;
    int low_frame = CNR3_INVALID_FRAME_NUMBER;
    int high_frame = CNR3_INVALID_FRAME_NUMBER;
    int last_observed_frame = CNR3_INVALID_FRAME_NUMBER;
};

[[nodiscard]] bool cnr3_cache_hot_zone_is_valid(
    const Cnr3CacheHotZone& hot_zone
) noexcept;

[[nodiscard]] bool cnr3_cache_hot_zone_model_invariants_hold(
    const std::vector<Cnr3CacheHotZone>& hot_zones
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
    [[nodiscard]] std::size_t hot_zone_count() const;

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

        Duplicate frame numbers preserve first-in-best-dressed frame data: the
        existing cache slot's pixels remain authoritative and the rejected
        incoming frame is released by the caller-side wrapper after the lock
        scope exits. A duplicate non-checkpoint store never clears an existing
        checkpoint flag.

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
        Lock-owning checkpoint store operation.

        This is the CMS07-E.3A isolated checkpoint-store primitive. It has the
        same ownership rules as store_noncheckpoint_owned_frame(), but successful
        insertion sets the cache slot checkpoint flag and records the slot
        position in the checkpoint-position list inside the same cache-lock
        acquisition.

        On duplicate, first-in-best-dressed still preserves the existing frame
        data. Under the CMS07.1 monotonic checkpoint rule, if the existing slot
        is not yet a checkpoint, this operation promotes only its checkpoint
        flag and records it in the checkpoint index inside the same cache-lock
        acquisition. It never overwrites frame data.

        Checkpoint status is an eviction-protection classification only. It is
        not a slot pin, does not increment pin_count, does not addFrameRef(),
        and does not reserve consumer liveness. Future AS2 consumer code must
        still use a combined AS2 helper for store/adopt, pin, pin-list record,
        and checkpoint establishment.
    */
    [[nodiscard]] Cnr3Status store_checkpoint_owned_frame(
        int frame_number,
        Cnr3OwnedFrameRef frame
    );

    /*
        Lock-owning central single-slot remove operation.

        This is the CMS07-F.1A low-level detach primitive for future prune and
        teardown policy. It removes one unpinned slot from the frame index,
        checkpoint-position list, and slot vector inside one cache-lock
        acquisition, then releases the detached frame after cache_mutex_ is
        unlocked.

        This helper enforces the no-pinned-frame rule, but it does not decide
        prune eligibility. Future prune code must evaluate the full CMS07
        eviction predicate before calling the locked remove helper.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_frame(
        int frame_number
    );

    /*
        Lock-owning bounded selected-detach operation.

        CMS07-F.2A proves the AS5 detach shape before final prune policy is
        available. The caller supplies already-selected candidate frame numbers
        and max_remove_count bounds the work done during one cache-lock
        acquisition. Detached slots release their owned frame references after
        cache_mutex_ is unlocked.

        This is not final prune policy. It does not evaluate hot-zone distance,
        checkpoint-retention limits, active ceiling pressure, or recovery state.
        Future AS5 prune code must perform candidate selection and detach under
        the same cache-lock scope using the complete CMS07 eviction predicate.
    */
    [[nodiscard]] Cnr3Status remove_selected_unpinned_frames_bounded(
        const std::vector<int>& candidate_frame_numbers,
        std::size_t max_remove_count,
        std::size_t& out_removed_count
    );

    /*
        Lock-owning bounded unpinned non-checkpoint selection/detach operation.

        CMS07-F.3A proves the first narrow candidate-selection layer for AS5:
        only unpinned non-checkpoint slots may be selected and detached. The
        selection and detach happen under one cache-lock acquisition, and
        detached frame references release only after cache_mutex_ is unlocked.

        This is still not final prune policy. It deliberately does not evaluate
        hot-zone distance, checkpoint-retention limits, active ceiling pressure,
        or recovery state. Future AS5 prune code must extend this shape to the
        complete CMS07 eviction predicate.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_noncheckpoint_frames_bounded(
        std::size_t max_remove_count,
        std::size_t& out_removed_count
    );

    /*
        Lock-owning bounded checkpoint retention-boundary selection/detach
        operation.

        CMS07-F.4A proves the checkpoint side of the future AS5 predicate before
        hot-zone exclusion and distance ordering exist. The helper detaches only
        unpinned checkpoint slots above retain_checkpoint_count, never detaches
        frame 0, and releases detached frame references only after cache_mutex_
        is unlocked.

        This is still not final checkpoint-retention prune policy. It deliberately
        does not evaluate hot-zone protection, greatest-distance ordering, active
        ceiling pressure, or recovery state. Future AS5 prune code must extend
        this shape to the complete CMS07 eviction predicate.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_checkpoints_above_retain_count_bounded(
        std::size_t retain_checkpoint_count,
        std::size_t max_remove_count,
        std::size_t& out_removed_count
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

        This is the CMS07-D.3A compliant AS1 primitive for cached-frame reuse
        and the CMS07-E.2A reconciled implementation of the original E.2
        lookup-pin-record helper obligation. It reserves pin-list capacity
        before acquiring cache_mutex_, then under one cache-lock acquisition it
        finds the frame-number index entry, validates the slot, increments the
        matching slot pin count, and records the pin in pin_list without
        allocation.

        This operation does not call addFrameRef(), freeFrame(), or transfer a
        VSFrame. It reserves slot liveness only. Do not add a second public
        lookup-pin-record helper unless a later CMS update changes this AS1
        boundary.
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
    [[nodiscard]] std::size_t hot_zone_count_locked() const noexcept;
    [[nodiscard]] int total_pin_count_locked() const noexcept;

    /*
        Lock-protected cache-state invariant helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself.

        The invariant checked here is structural only. It verifies that the
        slot vector, frame index, checkpoint-position list, hot-zone model, live
        slot metadata, and basic pin counts are mutually consistent. It does not
        perform
        VapourSynth reference-count checks, ownership accounting, recovery
        planning, pruning, or pixel validation.
    */
    [[nodiscard]] bool cache_state_invariants_hold_locked() const noexcept;

    /*
        Lock-protected store helpers.

        These helpers assume the caller already holds cache_mutex_. They must
        not acquire cache_mutex_ themselves.

        A store helper may move from frame only on successful insertion. On
        rejected paths, frame remains owned by the public wrapper so it can be
        released after cache_mutex_ is unlocked.

        Checkpoint insertion or duplicate-store promotion updates the slot flag
        and checkpoint-position list inside the same lock-protected store
        operation. Checkpoint status is not a pin and must not change pin_count.
    */
    [[nodiscard]] Cnr3Status store_noncheckpoint_owned_frame_locked(
        int frame_number,
        Cnr3OwnedFrameRef& frame
    );

    [[nodiscard]] Cnr3Status store_checkpoint_owned_frame_locked(
        int frame_number,
        Cnr3OwnedFrameRef& frame
    );

    [[nodiscard]] Cnr3Status store_owned_frame_locked(
        int frame_number,
        Cnr3OwnedFrameRef& frame,
        bool is_checkpoint
    );

    /*
        Lock-protected central single-slot remove helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. The slot must be unpinned. The detached
        slot is moved into detached_slot so its owned frame reference can be
        released after cache_mutex_ is unlocked.

        This helper is policy-free: future prune code must select candidates by
        the composite eviction predicate before calling it.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_frame_locked(
        int frame_number,
        Cnr3CacheSlot& detached_slot
    );

    /*
        Lock-protected bounded selected-detach helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. detached_slots must have enough reserved
        capacity before entry so the in-lock detach loop does not allocate.

        The helper mechanically removes up to max_remove_count candidates by
        calling the central single-slot remove helper. It is policy-free and is
        not the final AS5 candidate-selection predicate.
    */
    [[nodiscard]] Cnr3Status remove_selected_unpinned_frames_bounded_locked(
        const std::vector<int>& candidate_frame_numbers,
        std::size_t max_remove_count,
        std::vector<Cnr3CacheSlot>& detached_slots,
        std::size_t& out_removed_count
    );

    /*
        Lock-protected bounded unpinned non-checkpoint selection/detach helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. detached_slots must have enough reserved
        capacity before entry so the in-lock detach loop does not allocate.

        This helper selects only slots with pin_count == 0 and is_checkpoint ==
        false, then detaches them through the central remove helper. It is a
        narrow F.3A safety proof, not the final CMS07 prune predicate.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_noncheckpoint_frames_bounded_locked(
        std::size_t max_remove_count,
        std::vector<Cnr3CacheSlot>& detached_slots,
        std::size_t& out_removed_count
    );

    /*
        Lock-protected bounded checkpoint retention-boundary selection/detach
        helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. detached_slots must have enough reserved
        capacity before entry so the in-lock detach loop does not allocate.

        This helper selects only checkpoint slots with pin_count == 0, frame
        number other than 0, and checkpoint_count above retain_checkpoint_count.
        It is a narrow F.4A safety proof, not the final CMS07 checkpoint prune
        predicate.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_checkpoints_above_retain_count_bounded_locked(
        std::size_t retain_checkpoint_count,
        std::size_t max_remove_count,
        std::vector<Cnr3CacheSlot>& detached_slots,
        std::size_t& out_removed_count
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

        CMS07-E.2A confirms this helper is the single AS1 lookup-pin-record
        primitive. It is not a staging half for another public lookup-pin API.
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
    std::vector<Cnr3CacheHotZone> hot_zones_{};
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
