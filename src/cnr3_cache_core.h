#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "cnr3_build_config.h"
#include "cnr3_cache_diagnostics.h"
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
    Cache profile identity marker.

    This is keyed off the same CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY macro
    that gates the cache policy constants below, so the marker cannot drift
    from the compiled constants. It is for human diagnostics, selftest
    assertions, and future D-SUM summary headers only. Do not branch cache
    behaviour on this string; the preprocessor macro is the only feature gate.
*/
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr const char* CNR3_CACHE_PROFILE_NAME = "tiny-100";
#else
inline constexpr const char* CNR3_CACHE_PROFILE_NAME = "normal";
#endif

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
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES = 40U;   // TINY-100 diagnostic profile.
#else
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES = 150U;  // NORMAL production profile.
#endif

#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 100U;   // TINY-100 diagnostic profile.
#else
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 1000U;  // NORMAL production profile.
#endif

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
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr int CNR3_CACHE_CHECKPOINT_INTERVAL = 3;  // TINY-100 diagnostic profile.
#else
inline constexpr int CNR3_CACHE_CHECKPOINT_INTERVAL = 10; // NORMAL production profile.
#endif

#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_MIN_RETAIN = 4U;   // TINY-100 diagnostic profile.
#else
inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_MIN_RETAIN = 10U;  // NORMAL production profile.
#endif

#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_MAX_RETAIN = 12U;  // TINY-100 diagnostic profile.
#else
inline constexpr std::size_t CNR3_CACHE_CHECKPOINT_MAX_RETAIN = 48U;  // NORMAL production profile.
#endif

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
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr int CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS = 3;   // TINY-100 diagnostic profile.
#else
inline constexpr int CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS = 10;  // NORMAL production profile.
#endif

#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr int CNR3_CACHE_HOT_ZONE_BACK_RADIUS = 15;  // TINY-100 diagnostic profile.
#else
inline constexpr int CNR3_CACHE_HOT_ZONE_BACK_RADIUS = 50;  // NORMAL production profile.
#endif
inline constexpr int CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS =
    CNR3_CACHE_HOT_ZONE_BACK_RADIUS;

// Recency bound for the prune-rechurn counter, in EVICTION-COUNT units (not frame numbers).
// A re-fetched frame counts as "recently evicted" only if <= this many evictions occurred since it was dropped.
// Proof data under TINY-100 showed evict-then-refetch events clustering into two populations:
// recovery-local events at <= 50 and far-revisit events at >= 101, with 51-100 empirically empty across
// S9/S9c/S9d/S9e. Three times BACK_RADIUS gives 45 under TINY-100 and 150 under NORMAL, sitting in that
// valley: it catches recovery-local refetches while rejecting far-revisit noise. Expressed from BACK_RADIUS
// so the bound profiles automatically.
inline constexpr std::uint64_t CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP =
    static_cast<std::uint64_t>(CNR3_CACHE_HOT_ZONE_BACK_RADIUS) * 3U;

/*
    Hot-zone count. MAX_HOT_ZONES scales with concurrent distinct access
    regions, not thread count.
*/
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_CACHE_MAX_HOT_ZONES = 2U;  // TINY-100 diagnostic profile.
#else
inline constexpr std::size_t CNR3_CACHE_MAX_HOT_ZONES = 5U;  // NORMAL production profile.
#endif

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
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr int CNR3_CACHE_HOT_ZONE_DECAY_MARGIN = 6;   // TINY-100 diagnostic profile.
#else
inline constexpr int CNR3_CACHE_HOT_ZONE_DECAY_MARGIN = 20;  // NORMAL production profile.
#endif

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
    Ordered prune-candidate distance entry.

    CMS07-G.8A uses this only to prove victim ordering among candidates that
    are already eligible by the narrow non-checkpoint / unpinned / outside-hot-
    zone predicate. It is not the final CMS07 prune policy and does not decide
    when pruning should run.
*/
struct Cnr3PruneCandidateDistanceOrderEntry {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    int nearest_hot_zone_distance = 0;
    bool is_checkpoint = false;
};

[[nodiscard]] constexpr bool cnr3_prune_candidate_distance_order_entry_is_valid(
    Cnr3PruneCandidateDistanceOrderEntry entry
) noexcept {
    return
        cnr3_frame_number_is_valid(entry.frame_number) &&
        entry.nearest_hot_zone_distance >= 0;
}

/*
    Active-ceiling and checkpoint-retention prune trigger decision.

    prune_is_required remains the CMS07 section 7.2 capacity trigger only.
    checkpoint_prune_is_required is the independent CMS07.14 section 7.4
    checkpoint-retention trigger. Later AS5 execution may run when either
    trigger is active, but selection/detach/free remain separate steps.
*/
struct Cnr3CachePruneTriggerDecision {
    std::uint64_t frame_byte_count = 0U;
    std::size_t active_ceiling_frame_count = 0U;
    std::size_t overflow_trigger_frame_count = 0U;
    std::size_t current_slot_count = 0U;
    bool prune_is_required = false;
    std::size_t target_slot_count_after_prune = 0U;
    std::size_t target_remove_count = 0U;
    std::size_t current_checkpoint_count = 0U;
    std::size_t checkpoint_retain_target_count = 0U;
    bool checkpoint_prune_is_required = false;
    std::size_t checkpoint_target_count_after_prune = 0U;
    std::size_t checkpoint_target_remove_count = 0U;
};

/*
    Bounded AS5 prune execution summary.

    CMS07-G.11A reports what the AS5 decide/detach/free proof did without
    exposing cache internals. It records the trigger decision, the externally
    supplied bounded-remove cap, the effective bounded remove limit, how many
    candidates were selected under the cache lock, and how many slots were
    actually detached under that same lock. Detached frame references are
    released after the lock is released by the public AS5 helper.
*/
struct Cnr3CachePruneExecutionSummary {
    Cnr3CachePruneTriggerDecision trigger_decision{};
    std::size_t retain_checkpoint_count = 0U;
    std::size_t max_remove_count = 0U;
    std::size_t bounded_remove_limit = 0U;
    std::size_t selected_candidate_count = 0U;
    std::size_t detached_count = 0U;
};

/*
    Combined AS2 store/pin-record summary.

    CMS07-G.12A reports the result of one store/adopt/pin/record/checkpoint
    atomic. Duplicate stores preserve the first frame data; checkpoint-eligible
    duplicates may only raise checkpoint state monotonically.
*/
struct Cnr3CacheAs2StoreRecordSummary {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    bool requested_checkpoint = false;
    bool inserted_new_slot = false;
    bool duplicate_existing_slot = false;
    bool checkpoint_promoted = false;
    bool resulting_slot_is_checkpoint = false;
    bool pin_recorded = false;
    bool incoming_frame_consumed = false;
    bool incoming_frame_rejected = false;
};

/*
    W.3 combined live store-and-prune store kind.

    The valid kinds make production-vs-AS2 pin behaviour explicit. Invalid is a
    sentinel so an early-return summary cannot look like a valid store outcome.
*/
enum class Cnr3CacheStoreKind : std::uint8_t {
    Invalid = 0,
    ProductionNonCheckpoint,
    ProductionCheckpoint,
    As2ConsumerNonCheckpoint,
    As2ConsumerCheckpoint
};

[[nodiscard]] constexpr const char* cnr3_cache_store_kind_name(
    Cnr3CacheStoreKind store_kind
) noexcept {
    switch (store_kind) {
    case Cnr3CacheStoreKind::Invalid:
        return "invalid";
    case Cnr3CacheStoreKind::ProductionNonCheckpoint:
        return "production_noncheckpoint";
    case Cnr3CacheStoreKind::ProductionCheckpoint:
        return "production_checkpoint";
    case Cnr3CacheStoreKind::As2ConsumerNonCheckpoint:
        return "as2_consumer_noncheckpoint";
    case Cnr3CacheStoreKind::As2ConsumerCheckpoint:
        return "as2_consumer_checkpoint";
    }

    return "unknown";
}

/*
    W.3 combined store/prune summary.

    The wrapper return value is the overall hard status. The original store
    outcome remains visible here so production duplicate and AS2 duplicate
    semantics are not flattened into a single ok/fail result.
*/
struct Cnr3CombinedStoreAndPruneSummary {
    Cnr3CacheStoreKind store_kind = Cnr3CacheStoreKind::Invalid;
    int stored_frame_number = CNR3_INVALID_FRAME_NUMBER;
    int activation_target_frame = CNR3_INVALID_FRAME_NUMBER;
    Cnr3Status store_status = Cnr3Status::invariant_violation;
    Cnr3Status retire_status = Cnr3Status::invariant_violation;
    Cnr3Status prune_status = Cnr3Status::invariant_violation;
    Cnr3CacheAs2StoreRecordSummary as2_summary{};
    Cnr3CachePruneExecutionSummary prune_summary{};
};

/*
    Bounded recovery search plan.

    CMS07-H.1A proves the read-only AS1 recovery search scaffold only. The
    search descends from requested_frame - 1 within the inclusive lower-bound
    recovery window, ignores checkpoint classification for anchor eligibility,
    and records missing frames between the selected anchor and requested frame.

    The requested frame itself is the later repair target and is deliberately
    not included in hole_frame_numbers. Read-only planning leaves
    anchor_pin_recorded false. CMS07-H.2A records true only when the bounded
    recovery anchor is pinned and recorded by the combined AS1 helper.
*/
struct Cnr3CacheRecoverySearchPlan {
    int requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int max_back_radius = 0;
    int search_lower_frame = CNR3_INVALID_FRAME_NUMBER;
    int search_upper_frame = CNR3_INVALID_FRAME_NUMBER;
    bool search_interval_has_frames = false;
    bool anchor_found = false;
    int anchor_frame_number = CNR3_INVALID_FRAME_NUMBER;
    bool anchor_is_checkpoint = false;
    bool anchor_pin_recorded = false;
    bool requested_frame_is_repair_target = false;
    bool requested_frame_is_in_hole_catalogue = false;
    std::vector<int> hole_frame_numbers{};
};

/*
    Keystone request-plan branch.

    CMS07-K.1A defines only the plan representation used by later getFrame
    integration phases. It does not request, retrieve, compute, recover, store,
    return, or call the pixel path.

    The hard-status branch is a carrier for statuses produced by existing
    guards in later phases, especially the C.13B recovery-plan guard. Defining
    this carrier here must not be read as adding a second contiguity validator.
*/
enum class Cnr3KeystoneRequestPlanBranch : std::uint8_t {
    invalid = 0,
    direct_cached_output_return,
    frame0_fresh_start,
    predecessor_present,
    bounded_recovery_exact_anchor,
    bounded_recovery_floor_fresh_start,
    hard_status
};

[[nodiscard]] constexpr const char* cnr3_keystone_request_plan_branch_name(
    Cnr3KeystoneRequestPlanBranch branch
) noexcept {
    switch (branch) {
    case Cnr3KeystoneRequestPlanBranch::direct_cached_output_return:
        return "CACHE-HIT";
    case Cnr3KeystoneRequestPlanBranch::frame0_fresh_start:
        return "FRAME0-FRESH";
    case Cnr3KeystoneRequestPlanBranch::predecessor_present:
        return "PRED-PRESENT";
    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_exact_anchor:
        return "RECOVER";
    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_floor_fresh_start:
        return "RECOVER";
    case Cnr3KeystoneRequestPlanBranch::hard_status:
        return "HARD-STATUS";
    case Cnr3KeystoneRequestPlanBranch::invalid:
        break;
    }

    return "INVALID";
}

struct Cnr3KeystoneRequestPlan {
    Cnr3KeystoneRequestPlanBranch branch =
        Cnr3KeystoneRequestPlanBranch::invalid;
    int requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int floor_frame = CNR3_INVALID_FRAME_NUMBER;
    int start_point_frame = CNR3_INVALID_FRAME_NUMBER;
    int predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
    bool floor_fresh_start_approximation = false;
    Cnr3Status hard_status = Cnr3Status::ok;

    /*
        Output holes are explicit frame numbers, not an implied span.
        Later getFrame phases derive source requests from this list so the
        request set remains holes-only even if a future planner becomes sparse.

        For the floor-fresh-start branch, the floor frame itself is included in
        this list because it is an absent output that must be produced as the
        fresh-start base before walking forward.
    */
    std::vector<int> hole_frame_numbers{};
    std::vector<int> source_request_frame_numbers{};
};

#if defined(CNR3_KEYSTONE_DEV_TRACE)
struct Cnr3KeystoneDevTraceSummary {
    int total_plan_count = 0;
    int direct_cached_output_return_count = 0;
    int frame0_fresh_start_count = 0;
    int predecessor_present_count = 0;
    int bounded_recovery_exact_anchor_count = 0;
    int bounded_recovery_floor_fresh_start_count = 0;
    int hard_status_count = 0;
    int max_recovery_span = 0;
    bool floor_fresh_start_approximation_seen = false;
};
#endif

void cnr3_keystone_request_plan_reset(
    Cnr3KeystoneRequestPlan& plan
) noexcept;

[[nodiscard]] Cnr3Status cnr3_keystone_request_plan_rebuild_source_request_set(
    Cnr3KeystoneRequestPlan& plan
);

#if defined(CNR3_KEYSTONE_DEV_TRACE)
void cnr3_keystone_dev_trace_summary_observe_plan(
    const Cnr3KeystoneRequestPlan& plan,
    Cnr3KeystoneDevTraceSummary& summary
) noexcept;

[[nodiscard]] Cnr3Status cnr3_keystone_format_dev_trace_line(
    const Cnr3KeystoneRequestPlan& plan,
    char* out_buffer,
    std::size_t out_buffer_size
) noexcept;

[[nodiscard]] Cnr3Status cnr3_keystone_format_dev_trace_summary(
    const Cnr3KeystoneDevTraceSummary& summary,
    char* out_buffer,
    std::size_t out_buffer_size
) noexcept;
#endif

/*
    Calculate the CMS07 section 7.2 active-ceiling / overflow-factor trigger
    and the CMS07.14 section 7.4 checkpoint-retention trigger.

    prune_is_required fires only when current_slot_count is strictly greater
    than active_ceiling * OVERFLOW_FACTOR. checkpoint_prune_is_required fires
    independently when the checkpoint-flagged count exceeds MAX_RETAIN.
*/
[[nodiscard]] Cnr3Status cnr3_calculate_cache_prune_trigger_decision(
    std::uint64_t frame_byte_count,
    std::size_t current_slot_count,
    std::size_t current_checkpoint_count,
    std::size_t retain_checkpoint_count,
    Cnr3CachePruneTriggerDecision& out_decision
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
    Cnr3OutputCacheCore();
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
    [[nodiscard]] Cnr3CacheHotZoneDiagnosticStats hot_zone_diagnostic_stats() const;
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    [[nodiscard]] Cnr3CacheOwnershipDiagnosticStats ownership_diagnostic_stats() const;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    [[nodiscard]] Cnr3CacheIntegrityDiagnosticStats cache_integrity_diagnostic_stats() const;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    [[nodiscard]] Cnr3CacheStoreDiagnosticStats cache_store_diagnostic_stats() const;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    [[nodiscard]] Cnr3CachePruneDiagnosticStats prune_diagnostic_stats() const;
#endif

    /*
        Lock-owning observer for hot-zone membership.

        This is read-only hot-zone visibility for tests and later prune policy.
        It does not decide eviction and does not make a frame live; consumer pins
        remain the only active-frame liveness mechanism.
    */
    [[nodiscard]] bool frame_is_inside_hot_zone(
        int frame_number
    ) const;

    /*
        Lock-owning hot-zone observation update.

        CMS07-G.3A/G.4A proves the slide/spawn/merge part of the hot-zone
        model. This records activity for frame_number by sliding the nearest
        active zone within CNR3_CACHE_JUMP_THRESHOLD, by spawning a new zone
        when capacity permits, or by conservatively merging the two closest
        active zones before spawning when the hot-zone vector is full.
        Retirement/decay, prune use, and final AS1 integration remain
        deferred.
    */
    [[nodiscard]] Cnr3Status record_hot_zone_observation(
        int frame_number
    );

    /*
        Lock-owning hot-zone retirement update.

        CMS07-G.5A proves only the decay/retirement predicate. A hot zone may
        be retired once CNR3_CACHE_HOT_ZONE_DECAY_MARGIN frames have elapsed
        since its last observation and no currently pinned cache slot lies in
        the zone range. Checkpoints do not keep a hot zone alive. This does not
        apply zones to prune policy or remove cache slots.
    */
    [[nodiscard]] Cnr3Status retire_decay_eligible_hot_zones(
        int current_frame
    );

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
        Lock-owning combined AS2 store/adopt/pin-record operation.

        This is the CMS07-G.12A AS2 primitive for one computed output frame. The
        caller supplies an owned frame reference and a per-invocation pin list.
        The helper pre-reserves pin-list capacity before acquiring cache_mutex_,
        then under one cache-lock acquisition it stores or adopts the existing
        first-in-best-dressed winner, applies monotonic checkpoint promotion,
        increments the slot pin count, and records the pin without allocation.

        Duplicate stores never overwrite existing frame data. A checkpoint-
        eligible duplicate may promote an existing non-checkpoint slot to
        checkpoint; a non-checkpoint duplicate never demotes an existing
        checkpoint. Rejected duplicate loser frames remain in the public wrapper
        and are released only after cache_mutex_ is unlocked.

        This helper does not request/retrieve source frames, compute pixels,
        prune, recover, or wire D-SUM counters.
    */
    [[nodiscard]] Cnr3Status store_owned_frame_and_record_pin(
        int frame_number,
        Cnr3OwnedFrameRef frame,
        bool is_checkpoint,
        Cnr3CachePinList& pin_list,
        Cnr3CacheAs2StoreRecordSummary& out_summary
    );

    /*
        Recovery planned-hole AS2 consumer.

        CMS07-H.3A uses the already-proven AS2 helper to consume one hole from
        a bounded recovery plan. This wrapper validates that hole_frame_number
        is one of the plan's genuine hole entries and is not the requested
        repair target, then delegates to store_owned_frame_and_record_pin().

        This is not a second AS2 primitive and must not grow a split pin path.
        The G.12A AS2 helper remains responsible for store/adopt, checkpoint
        promotion, pin, pin-list record, and duplicate-loser release after the
        cache lock. H.3A proves the recovery consumer accounts for the AS2 pin
        produced by every planned-hole store/adopt call.

        This helper first re-validates the current minimal recovery-plan
        contiguity contract before any AS2 delegation. That guard is ordinary
        hard-status logic, not a diagnostic gate.

        This helper does not request/retrieve source frames, compute pixels,
        prune, return frames, wire D-SUM counters, emit stderr, or connect to
        getFrame.
    */
    [[nodiscard]] Cnr3Status store_recovery_plan_hole_owned_frame_and_record_pin(
        const Cnr3CacheRecoverySearchPlan& recovery_plan,
        int hole_frame_number,
        Cnr3OwnedFrameRef frame,
        bool is_checkpoint,
        Cnr3CachePinList& pin_list,
        Cnr3CacheAs2StoreRecordSummary& out_summary
    );

    /*
        W.3 lock-owning combined live store-and-prune wrappers.

        Production stores never pin. AS2 stores must pin and record exactly one
        consumer pin before prune is allowed to run. The recovery-hole wrapper
        preserves the H.3A plan/hole guard before any reservation, lock, or
        mutation. All wrappers take ownership of frame by value so duplicate or
        rejected losers release after cache_mutex_ is unlocked.
    */
    [[nodiscard]] Cnr3Status store_production_output_and_prune(
        int stored_frame_number,
        int activation_target_frame,
        Cnr3OwnedFrameRef frame,
        bool is_checkpoint,
        std::uint64_t frame_byte_count,
        Cnr3CombinedStoreAndPruneSummary& out_summary
    );

    [[nodiscard]] Cnr3Status store_as2_floor_and_prune(
        int stored_frame_number,
        int activation_target_frame,
        Cnr3OwnedFrameRef frame,
        bool is_checkpoint,
        std::uint64_t frame_byte_count,
        Cnr3CachePinList& pin_list,
        Cnr3CombinedStoreAndPruneSummary& out_summary
    );

    [[nodiscard]] Cnr3Status store_recovery_hole_and_prune(
        const Cnr3CacheRecoverySearchPlan& recovery_plan,
        int hole_frame_number,
        int activation_target_frame,
        Cnr3OwnedFrameRef frame,
        bool is_checkpoint,
        std::uint64_t frame_byte_count,
        Cnr3CachePinList& pin_list,
        Cnr3CombinedStoreAndPruneSummary& out_summary
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
        Lock-owning bounded hot-zone-protected non-checkpoint selection/detach
        operation.

        CMS07-G.7A proves the hot-zone exclusion clause for future AS5 prune
        selection: only unpinned non-checkpoint slots outside every active hot
        zone may be selected and detached. The selection and detach happen under
        one cache-lock acquisition, and detached frame references release only
        after cache_mutex_ is unlocked.

        This is still not final prune policy. It deliberately does not assemble
        the full CMS07 section 7.1 predicate, apply greatest-hot-zone-distance
        victim ordering, evaluate checkpoint-retention limits, trigger on active
        ceiling / overflow-factor pressure, wire D-SUM prune counters, or
        perform recovery/getFrame work.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
        std::size_t max_remove_count,
        std::size_t& out_removed_count
    );

    /*
        Lock-owning bounded prune-candidate distance-order selector.

        CMS07-G.8A proves ordering only. The helper returns up to
        max_select_count candidates that are already eligible by the narrow
        G.7A predicate, ordered by greatest distance from the nearest active
        hot-zone boundary, with lower frame number as the deterministic tie
        breaker. It does not detach slots, trigger pruning, apply active-ceiling
        pressure, apply checkpoint-retention policy, wire D-SUM counters, or
        assemble the final CMS07 section 7.1 predicate.
    */
    [[nodiscard]] Cnr3Status select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded(
        std::size_t max_select_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
    ) const;

    /*
        Lock-owning bounded composite prune-candidate selector.

        CMS07-G.9A assembles the already-proven predicate clauses for
        selection only: unpinned, outside every active hot zone, checkpoint
        retention permitted for checkpoints, and externally-supplied capacity
        permission for non-checkpoints. Victims are ordered by greatest distance
        from the nearest active hot-zone boundary.

        This helper does not compute the CMS07 section 7.2 active-ceiling /
        overflow-factor trigger, detach slots, release frames, wire D-SUM
        counters, or perform production AS5 prune integration.
    */
    [[nodiscard]] Cnr3Status select_composite_prune_candidates_bounded(
        bool noncheckpoint_capacity_permits,
        std::size_t retain_checkpoint_count,
        std::size_t max_select_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
    ) const;

    /*
        Lock-owning bounded AS5 prune execution helper.

        CMS07-G.11A composes the proven trigger decision and composite
        selector into one bounded decide/detach pass: under the cache lock it
        evaluates the section 7.2 trigger from the current slot count, selects
        victims through the G.9A composite selector, and detaches selected slots
        through the central remove helper. Detached frame references release
        only after cache_mutex_ is unlocked.

        This is still not full production getFrame integration. It does not
        wire D-SUM-11 prune-rejection counters, perform recovery planning,
        request or retrieve source frames, or touch pixel behaviour.
    */
    [[nodiscard]] Cnr3Status execute_bounded_prune_pass(
        std::uint64_t frame_byte_count,
        std::size_t retain_checkpoint_count,
        std::size_t max_remove_count,
        Cnr3CachePruneExecutionSummary& out_summary
    );

    /*
        Lock-owning bounded AS1 recovery search scaffold.

        CMS07-H.1A proves only the read-only recovery planning search: descend
        from requested_frame - 1, stop at the inclusive lower bound
        requested_frame - max_back_radius clamped to frame 0, choose the nearest
        present cached output regardless of checkpoint classification, and build
        the hole catalogue from anchor + 1 through requested_frame - 1.

        The requested frame itself is not a hole-catalogue entry; it is the
        later repair target handled by a later recovery execution phase. This
        helper does not pin, store, prune, request source frames, recompute,
        return frames, or touch pixel behaviour.
    */
    [[nodiscard]] Cnr3Status plan_bounded_recovery_search(
        int requested_frame,
        int max_back_radius,
        Cnr3CacheRecoverySearchPlan& out_plan
    ) const;

    /*
        Lock-owning AS1 bounded recovery anchor pin-record operation.

        CMS07-H.2A composes the H.1A bounded recovery search with the D.3A-style
        pin-list discipline. The public helper reserves hole-catalogue storage
        and one pin-list entry before acquiring cache_mutex_. Under one cache
        lock it plans the bounded recovery search, then pins and records exactly
        the selected anchor when an anchor exists. No-anchor and requested-frame-
        only cases record no pin.

        This helper does not call addFrameRef(), freeFrame(), AS2, source
        request/retrieve, recompute, return-transfer, prune, getFrame, or pixel
        processing.
    */
    [[nodiscard]] Cnr3Status plan_bounded_recovery_search_and_record_anchor_pin(
        int requested_frame,
        int max_back_radius,
        Cnr3CachePinList& pin_list,
        Cnr3CacheRecoverySearchPlan& out_plan
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
        Lock-owning AS4 whole-list discharge operation.

        This is the CMS07-Recovery-Step-0 implementation of AS4 final
        unpin: one cache_mutex_ acquisition for the whole pin-list. The
        helper walks every still-valid token through unpin_frame_locked(),
        so it never re-enters the public lock-owning unpin_frame() path.

        The observable discharge contract is unchanged from
        Cnr3CachePinList::discharge_all(): attempt every still-valid token,
        remember the first failure, and report it only after the walk. A
        clean discharge invalidates all entries and leaves the list empty; a
        second discharge is a no-op success.
    */
    [[nodiscard]] Cnr3Status discharge_pin_list(
        Cnr3CachePinList& pin_list
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
    [[nodiscard]] Cnr3CacheHotZoneDiagnosticStats hot_zone_diagnostic_stats_locked() const noexcept;
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    [[nodiscard]] Cnr3CacheOwnershipDiagnosticStats ownership_diagnostic_stats_locked() const noexcept;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    [[nodiscard]] Cnr3CacheIntegrityDiagnosticStats cache_integrity_diagnostic_stats_locked() const noexcept;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    [[nodiscard]] Cnr3CacheStoreDiagnosticStats cache_store_diagnostic_stats_locked() const noexcept;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    [[nodiscard]] Cnr3CachePruneDiagnosticStats prune_diagnostic_stats_locked() const;
#endif
    [[nodiscard]] bool frame_is_inside_hot_zone_locked(
        int frame_number
    ) const noexcept;
    [[nodiscard]] int nearest_active_hot_zone_boundary_distance_locked(
        int frame_number
    ) const noexcept;
    [[nodiscard]] bool hot_zone_has_pinned_frame_in_range_locked(
        const Cnr3CacheHotZone& hot_zone
    ) const noexcept;
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
        Lock-protected hot-zone helpers.

        These helpers assume the caller already holds cache_mutex_. They must
        not acquire cache_mutex_ themselves. They update only hot-zone policy
        hints and must not pin, unpin, store, remove, prune, recover, request
        source frames, or touch pixel data.
    */
    [[nodiscard]] Cnr3Status merge_closest_active_hot_zones_locked();

    [[nodiscard]] Cnr3Status record_hot_zone_observation_locked(
        int frame_number
    );

    [[nodiscard]] Cnr3Status retire_decay_eligible_hot_zones_locked(
        int current_frame
    );

    /*
        Lock-protected D-SUM-11 hot-zone counter helpers.

        These helpers assume the caller already holds cache_mutex_. They must
        not acquire cache_mutex_ themselves. They update only diagnostic
        counters and must not format, print, allocate, affect control flow, or
        change cache behaviour.
    */
    void observe_hot_zone_create_locked() noexcept;
    void observe_hot_zone_slide_locked() noexcept;
    void observe_hot_zone_merge_locked() noexcept;
    void observe_hot_zone_decay_locked() noexcept;
    void observe_hot_zone_expiry_locked() noexcept;
    void observe_hot_zone_state_sample_locked() noexcept;
    void observe_hot_zone_prune_rejections_locked(
        std::size_t rejected_frame_count
    ) noexcept;
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    void observe_pin_acquired_locked() noexcept;
    void observe_pin_released_locked() noexcept;
    void observe_lookup_ref_acquired_locked() const noexcept;
    void observe_lookup_ref_released_by_cache_core() const noexcept;
    void observe_lookup_ref_transferred() const noexcept;
    void observe_ownership_error() const noexcept;
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    void observe_cache_invariant_check_started_locked() const noexcept;
    [[nodiscard]] bool observe_cache_invariant_failure_locked(
        const char* site
    ) const noexcept;
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    void observe_store_outcome_locked(
        const Cnr3CombinedStoreAndPruneSummary& summary
    ) noexcept;
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    void observe_prune_execution_locked(
        std::uint64_t frame_byte_count,
        const std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order,
        const std::vector<int>& selected_frame_numbers,
        const Cnr3CachePruneExecutionSummary& prune_summary,
        std::size_t hot_zone_prune_rejection_count
    ) noexcept;
    void observe_lookup_miss_rechurn_locked(
        int frame_number
    ) const noexcept;
#endif

    [[nodiscard]] std::size_t count_prune_candidates_rejected_by_hot_zone_locked(
        bool noncheckpoint_capacity_permits,
        std::size_t retain_checkpoint_count
    ) const noexcept;

    [[nodiscard]] Cnr3Status store_owned_frame_and_prune_impl(
        int stored_frame_number,
        int activation_target_frame,
        Cnr3OwnedFrameRef& frame,
        bool is_checkpoint,
        Cnr3CacheStoreKind store_kind,
        std::uint64_t frame_byte_count,
        Cnr3CachePinList* pin_list,
        Cnr3CombinedStoreAndPruneSummary& out_summary
    );

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
        Lock-protected combined AS2 store/adopt/pin-record helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. pin_list must have enough pre-reserved
        storage for one more token before this helper is called.
    */
    [[nodiscard]] Cnr3Status store_owned_frame_and_record_pin_locked(
        int frame_number,
        Cnr3OwnedFrameRef& frame,
        bool is_checkpoint,
        Cnr3CachePinList& pin_list,
        Cnr3CacheAs2StoreRecordSummary& out_summary
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
        Lock-protected bounded hot-zone-protected non-checkpoint
        selection/detach helper.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. detached_slots must have enough reserved
        capacity before entry so the in-lock detach loop does not allocate.

        This helper selects only slots with pin_count == 0, is_checkpoint ==
        false, and frame_number outside every active hot zone. It proves the
        hot-zone exclusion clause only, not the full CMS07 prune predicate.
    */
    [[nodiscard]] Cnr3Status remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded_locked(
        std::size_t max_remove_count,
        std::vector<Cnr3CacheSlot>& detached_slots,
        std::size_t& out_removed_count
    );

    /*
        Lock-protected bounded distance-order selector.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. out_candidate_order must have enough
        reserved capacity before entry so selection does not allocate while the
        cache lock is held.

        Ordering is by distance to the nearest active hot-zone boundary, not by
        distance to any one selected zone. A frame adjacent to one live zone is
        therefore considered near even if it is far from another zone.
    */
    [[nodiscard]] Cnr3Status select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded_locked(
        std::size_t max_select_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
    ) const;


    /*
        Lock-protected bounded composite prune-candidate selector.

        This helper assumes the caller already holds cache_mutex_. It must not
        acquire cache_mutex_ itself. Both vectors must have enough reserved
        capacity before entry so selection does not allocate while the cache
        lock is held.

        noncheckpoint_capacity_permits is supplied by the caller because
        CMS07-G.9A does not implement the section 7.2 active-ceiling trigger.
    */
    [[nodiscard]] Cnr3Status select_composite_prune_candidates_bounded_locked(
        bool noncheckpoint_capacity_permits,
        std::size_t retain_checkpoint_count,
        std::size_t max_select_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& checkpoint_candidate_order
    ) const;

    /*
        Lock-protected bounded AS5 prune execution helper.

        This helper assumes the caller already holds cache_mutex_. All vectors
        must have enough reserved capacity before entry so the in-lock
        decide/select/detach pass does not allocate. Detached slots are moved
        into detached_slots and must be released only after the public helper
        exits the cache-lock scope.
    */
    [[nodiscard]] Cnr3Status execute_bounded_prune_pass_locked(
        std::uint64_t frame_byte_count,
        std::size_t retain_checkpoint_count,
        std::size_t max_remove_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& checkpoint_candidate_order,
        std::vector<int>& selected_frame_numbers,
        std::vector<Cnr3CacheSlot>& detached_slots,
        Cnr3CachePruneExecutionSummary& out_summary
    );


    /*
        Lock-protected bounded recovery search scaffold.

        Caller must hold cache_mutex_ and must pre-reserve hole_frame_numbers so
        catalogue construction is allocation-free while the cache lock is held.
        The helper validates the current minimal nearest-anchor + contiguous-
        hole postcondition before returning ok. The guard is a bounded pure scan
        over the just-built catalogue and does not allocate, print, touch
        frame refs, or handle sparse/AS3 plans.
    */
    [[nodiscard]] Cnr3Status plan_bounded_recovery_search_locked(
        int requested_frame,
        int max_back_radius,
        Cnr3CacheRecoverySearchPlan& out_plan
    ) const;

    /*
        Lock-protected AS1 bounded recovery anchor pin-record helper.

        Caller must hold cache_mutex_, must pre-reserve hole_frame_numbers, and
        must pre-reserve one pin-list entry. This helper must not allocate while
        the cache lock is held.
    */
    [[nodiscard]] Cnr3Status plan_bounded_recovery_search_and_record_anchor_pin_locked(
        int requested_frame,
        int max_back_radius,
        Cnr3CachePinList& pin_list,
        Cnr3CacheRecoverySearchPlan& out_plan
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
    Cnr3CacheHotZoneDiagnosticStats hot_zone_diag_stats_{};
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    mutable Cnr3CacheOwnershipDiagnosticStats ownership_diag_stats_{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    mutable Cnr3CacheIntegrityDiagnosticStats cache_integrity_diag_stats_{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    mutable Cnr3CacheStoreDiagnosticStats cache_store_diag_stats_{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    mutable Cnr3CachePruneDiagnosticStats prune_diag_stats_{};
#endif
    Cnr3CacheSlotIdSource slot_id_source_{};
};

/*
    Per-invocation cache pin list.

    This list owns slot-pin tokens recorded during one cache-core activation.
    Recording a token consumes the caller token by resetting it after the list
    has stored its own copy. Discharge delegates to
    Cnr3OutputCacheCore::discharge_pin_list(), which releases the whole
    recorded list under one AS4 cache-lock acquisition.

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

        This public pin-list entry point is deliberately kept stable for
        getFrame cleanup code. Internally it delegates to the cache core's
        AS4 batch-discharge method, so the whole list is discharged under one
        cache-lock acquisition rather than one lock per token.

        A clean discharge invalidates all recorded entries and leaves the list
        empty, so a second discharge is a no-op success. If any unpin fails, the
        first failure status is returned after all entries have been attempted.
    */
    [[nodiscard]] Cnr3Status discharge_all(
        Cnr3OutputCacheCore& cache
    );

private:
    friend class Cnr3OutputCacheCore;
    friend struct Cnr3CachePinListSelftestAccess;

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
