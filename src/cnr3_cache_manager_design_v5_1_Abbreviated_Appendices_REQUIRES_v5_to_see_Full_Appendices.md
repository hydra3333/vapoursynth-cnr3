# CNR3 Cache Manager — Revised Design Specification CMS05.1
## Sliding Hot-Zone Pruning with Reference-Count Discipline (Non-Checkpoint Pinning Deferred)

**Date:** 2026-06-02
**Version:** CMS05.1
**Status:** Design specification — ready for coding
**Supersedes:** CMS05 (cnr3_cache_manager_design_v5.md), CMS04, CMS03, CMS02, CMS01
**Also supersedes:** Bounded-recovery policy in handover snapshot v0.14 section 0.9A
**Companion documents:**
- CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)
- cnr3_code_review_plan_v5_1.md (code review and simulation plan)

---

## Changelog

### CMS05.1 — 2026-06-02

**CMS05.1-1 — `CNR3_CACHE_BYTE_BUDGET` raised from 512 MiB to 1 GiB.**
The 512 MiB budget was conservative for modern systems. 1 GiB gives
more headroom for large-format or high-bit-depth clips while remaining
well within the available RAM on typical VHS restoration workstations.
Changed in: Section 5 (constants), Section 4.6.1 (formula and worked
examples table).

**CMS05.1-2 — Ceiling clarification note added for 720×576 vs
1440×576.**
During implementation verification a log entry showed
`active_ceiling=1000` for a clip described as 1440×576 YUV420P8. This
appeared inconsistent with the formula. Investigation confirmed the clip
was actually 720×576 (standard PAL VHS resolution), for which the 1 GiB
budget yields candidate_ceiling ≈ 1,726, correctly clamped to 1000. For
genuine 1440×576 YUV420P8 the correct result is 863 (not clamped). No
code error exists. A clarifying note has been added to Section 4.6.1 to
prevent future confusion.
Changed in: Section 4.6.1 (note below worked examples table).

**CMS05.1-3 — Appendix D added: Code Review and Simulation Plan.**
A companion document (`cnr3_code_review_plan_v5_1.md`) has been created
covering the planned static analysis, Monte Carlo simulations, and
design-compliance review to be carried out after implementation. Appendix
D is a reference pointer to that document.
Changed in: Appendix D (new section).

---

## Change Summary CMS04 → CMS05

*(Unchanged from CMS05. Retained for reference.)*

1. First-in-best-dressed store idempotency.
2. Final-frame ownership transfer in rolling predecessor pattern.
3. Lookup-owned reference balance counters.
4. Version-independent naming.
5. Design-Compliance Review process.
6. Additional store-related diagnostic counters.
7. Appendix C added.

---

## 1. Problem Statement

CNR3 is a recursive temporal chroma stabiliser. Every output frame depends
on the previous output frame as a predecessor. The cache manager must
retain enough predecessor frames to satisfy any in-flight computation,
prune old frames to keep memory bounded, and maintain strict VSFrame
reference-count discipline so that no frame is ever leaked or freed
prematurely.

The existing v005 cache manager infrastructure (pools, checkpoint
pinning, mutex model, store/remove/validate helpers) is sound. What is
not yet designed is the pruning policy and the structural protections
needed to make that policy safe under three VapourSynth execution modes:

- **fmUnordered** — one request in flight at a time, mostly sequential
- **fmParallelRequests** — multiple concurrent requests, serialised writer
- **fmParallel** — fully concurrent readers and writers (out of scope for
  this iteration)

The primary failure modes without the new design are:

**FM1 — Prune destroys in-flight predecessor.**
**FM2 — Prune destroys a checkpoint needed by a recovery chain.**
**FM3 — Jump recovery burst exceeds pool capacity.**
**FM4 — Prune eviction key is wrong (lowest-first).**
**FM5 — VSFrame reference leak.**
**FM6 — VSFrame use-after-free.**
**FM7 — Duplicate store overwrites or double-references existing frame.**

---

## 2. Design Goals

1. **Prevent pruning of frames likely to be needed by active computation
   using hot-zone protection.** If diagnostics show that hot-zone
   protection is insufficient, promote non-checkpoint pinning to a
   mandatory implementation step (Section 4.4).

2. Make pruning decisions based on frame-number proximity to active work,
   not on insertion order or frame-number magnitude.

3. Support up to `CNR3_MAX_HOT_ZONES` simultaneous active working ranges,
   covering concurrent pre-jump and post-jump activity in
   fmParallelRequests.

4. Fill holes only — never recompute a frame that is already cached.
   Stores are idempotent by frame number (Section 2.1, Section 4.9).

5. **Reference-Count Invariant.** For every VSFrame held by the cache,
   the cache contributes exactly one `addFrameRef` while it holds the
   slot, balanced by exactly one `freeFrame` when the slot is removed.
   No VSFrame is ever leaked. No VSFrame is ever freed while still in
   use outside the cache mutex (Section 2.2, Section 4.7).

6. Bound memory use with a hard ceiling computed from actual frame
   geometry and a configurable byte budget. Tune after empirical runtime
   memory data (Section 4.6).

7. Hard-abort cleanly when the ceiling is hit with nothing prunable. The
   filter must remain in a valid state after the abort (Section 4.6,
   Section 9.5).

8. Remain compatible with the existing mutex model, pool structures, and
   `_externally_locked` helper pattern.

9. Require no changes to the VapourSynth API interaction model.

10. Keep the first implementation understandable and empirically provable
    before adding further complexity.

### 2.1 Fill-Holes-Only Principle (with Reference-Ownership Rule)

The cache manager must never blindly recompute a complete chain when the
required output frames are already cached.

Recovery and chain-filling operate as follows:

- Recovery proceeds in ascending frame-number order only.
- Before computing any frame K in a recovery or chain-fill walk, check
  whether output[K] already exists in the cache.
- If output[K] is present, reuse it as the predecessor for output[K+1]
  without recomputing it.
- If output[K] is absent (a hole), compute it from the best available
  predecessor, store it, and continue.
- If another concurrent request has already filled part of the chain,
  the later walk skips already-filled frames.
- If a concurrent walk loses a store race (both computed the same frame,
  the other walk's store landed first), the losing walk's store helper
  returns success without taking ownership of the losing walk's frame.
  The losing walk releases its computed frame normally (Section 4.9).

**Reference-ownership rule:**

When output[K] is found in cache and used as the predecessor for
output[K+1] outside the cache mutex, the lookup helper takes
`addFrameRef()` on the found frame while still holding the cache mutex
(making the find-and-ref atomic), then returns that caller-owned
reference. The caller must `freeFrame()` it on every exit path —
success, error, early return. This is implemented by
`cnr3_output_cache_find_frame_and_add_ref()` (Section 9.2.H). Callers
never see a raw borrowed `VSFrame*` after the mutex is released.

Hot-zone protection and `addFrameRef` are independent mechanisms with
different roles (Section 4.7).

### 2.2 Reference-Count Discipline

VapourSynth manages frame memory by reference counting. Every VSFrame
has an integer reference count. `addFrameRef` increments it; `freeFrame`
decrements it. When the count reaches zero, the frame's pixel data is
released.

**Hard rules:**

**RC1 — Single store helper.** All cache-owned `addFrameRef` calls
occur in `cnr3_output_cache_store_frame()`. No other code path takes a
cache-owned reference.

**RC2 — Single remove helper.** All cache-owned `freeFrame` calls
occur in `cnr3_output_cache_remove_frame_externally_locked()`. No code
path may `pool.erase()` or `cache_index.erase()` directly without going
through the remove helper.

**RC3 — Store error paths must rebalance.** If a path inside the store
helper takes `addFrameRef` and then fails to complete insertion, it must
execute a matching `freeFrame` before returning.

**RC4 — Lookup error paths must rebalance.** If
`find_frame_and_add_ref` takes `addFrameRef` and then fails before
returning to the caller, it must execute a matching `freeFrame` before
returning nullptr.

**RC5 — Caller exit paths must `freeFrame`.** Every code path that
receives a caller-owned reference from a lookup helper must `freeFrame`
on every exit path: success, error, early return, exception, hard abort.

**RC6 — Shutdown must clear.** Destruction of `Cnr3OutputCacheManager`
must iterate all slots in both pools, `freeFrame` each, and log a
warning for any slot with `pin_count > 0`. See Section 9.5.

**RC7 — Validation enforces balance.**`validate_invariants` includes
a ref-balance check: at quiescence,
`cache_addframeref_total - cache_freeframe_total ==
 total_live_slots_across_both_pools`.

**RC8 — First-in-best-dressed store idempotency.** The store helper is
idempotent by frame number. If `output[N]` already exists in
`cache_index` at store time, the store returns success without taking
`addFrameRef`, without modifying either pool, and without disturbing
the existing slot's classification. The cache manager has not taken
ownership of the caller's supplied frame in this case. See Section 4.9.

**Recommended pattern — RAII wrapper:**

```cpp
struct Cnr3OwnedFrameRef {
    const VSFrame* frame = nullptr;
    const VSAPI* vsapi = nullptr;
    Cnr3OwnedFrameRef() = default;
    Cnr3OwnedFrameRef(const VSFrame* f, const VSAPI* api)
        : frame(f), vsapi(api) {}
    ~Cnr3OwnedFrameRef() {
        if (frame && vsapi) vsapi->freeFrame(frame);
    }
    Cnr3OwnedFrameRef(const Cnr3OwnedFrameRef&) = delete;
    Cnr3OwnedFrameRef& operator=(const Cnr3OwnedFrameRef&) = delete;
    Cnr3OwnedFrameRef(Cnr3OwnedFrameRef&& other) noexcept;
    Cnr3OwnedFrameRef& operator=(Cnr3OwnedFrameRef&& other) noexcept;
    const VSFrame* release() noexcept {
        auto f = frame; frame = nullptr; return f;
    }
};
```

**Caller-side diagnostic balance (development only):**

Three development counters track caller-owned reference lifecycle:

- `lookup_owned_ref_acquired_total`
- `lookup_owned_ref_released_total`
- `lookup_owned_ref_transferred_total`

Caller-side diagnostic invariant at quiescent points:
```
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

---

## 3. Simulation Results — Linear Encoding Jitter

A Monte Carlo simulation (200 runs, 100 frames, varied jitter) was run to
characterise realistic frame arrival patterns for the primary linear
encoding use case (vspipe / ffmpeg pipe with BestSource source plugin).

Model: each frame is dispatched at time = `frame_number +
uniform_random(0, jitter_max)`. Frames arrive at `cnr3_get_frame` in
delivery-time order.

**Key results (jitter sweep, 500 runs each):**

| Jitter range | Avg max gap | p95 max gap | p99 max gap | Abs max gap |
|---|---|---|---|---|
| 0..2  | 2.7  | 3  | 3  | 3  |
| 0..4  | 4.7  | 6  | 7  | 7  |
| 0..6  | 6.5  | 8  | 9  | 9  |
| 0..8  | 8.2  | 10 | 11 | 12 |
| 0..10 | 10.0 | 12 | 12 | 14 |
| 0..12 | 11.6 | 14 | 15 | 16 |
| 0..16 | 15.2 | 18 | 19 | 20 |
| 0..24 | 22.0 | 24 | 26 | 29 |
| 0..32 | 28.9 | 32 | 34 | 34 |

For observed BestSource jitter of 4–6 frames, the worst-case reorder
window is approximately 9 frames at p99. `CNR3_HOT_ZONE_FORWARD_RADIUS
= 10` covers this with headroom.

The existing `CNR3_REORDER_WINDOW = 32` is adequate for jitter up to
approximately `jitter_max=24` at p99.

---

## 4. Algorithm Overview

### 4.1 Hot Zone Tracking

The cache manager maintains a small fixed-size array of hot zones. Each
hot zone represents a contiguous range of frame numbers currently active.

A hot zone has:
- An **active** flag
- A **low** frame number boundary (inclusive)
- A **high** frame number boundary (inclusive)
- A **last_observed_frame**
- Diagnostic counters: `hit_count`, `slide_count`, `merge_count`,
  `retirement_count`, `prune_protection_count`

### 4.2 Hot Zone Lifecycle — Sliding (Both Modes)

#### 4.2.1 Sliding rule (both modes)

For an arriving frame `F`:

1. Find the nearest active zone Z such that `F` is within
   `CNR3_HOT_ZONE_JUMP_THRESHOLD` of Z's range.

2. If such a Z is found, slide it:
   ```
   Z.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   Z.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   Z.last_observed_frame = F
   ```
   Increment `slide_count` if bounds moved, or `hit_count` if not.

3. If no Z is within threshold, allocate a new zone (Section 4.2.3).

#### 4.2.2 Why sliding is safe in both modes

Safety relies on:
- `addFrameRef` discipline (Section 4.7) — a walk holding an owned
  reference keeps the underlying VSFrame alive even if the cache slot
  is evicted.
- Rolling predecessor reference pattern (Section 4.5.1) — walks hold
  an owned reference to the current predecessor without a zero-reference
  window during transitions.

Full discussion in Appendix A.

#### 4.2.3 New zone allocation

When `F` is outside all active zones by more than `JUMP_THRESHOLD`:

1. Look for a free (inactive) zone slot.
2. If found:
   ```
   slot.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   slot.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   slot.last_observed_frame = F
   slot.active = true
   ```
   Increment `hot_zone_allocations`.
3. If no free slot: attempt retirement (4.2.4), then merge (4.2.5).

#### 4.2.4 Retirement (mode-specific)

**fmUnordered (eager):** New `arInitial` proves previous request
complete. Stale zones retired immediately before new zone allocation.
Typically at most one active zone in normal linear operation.

**fmParallelRequests (lazy):** Zone eligible for retirement only when:
- No live frames remain in either pool within `[low, high]`, AND
- No pinned checkpoint exists within its range.

Attempted only when a new zone allocation needs a free slot.

**fmParallel:** out of scope.

#### 4.2.5 Zone merge

When all slots full and no zone eligible for retirement:

1. Find two zones with smallest gap between boundaries.
2. Merge:
   ```
   merged.low  = min(Z1.low,  Z2.low)
   merged.high = max(Z1.high, Z2.high)
   merged.last_observed_frame = max(Z1.last, Z2.last)
   ```
3. Mark one inactive, store merged in the other.
4. Increment `merge_count`. Use freed slot for new zone.

### 4.3 Pruning Policy — Hot Zone Aware (Phase-Guarded)

#### 4.3.1 Non-Checkpoint Pool Pruning

**Phase A — before non-checkpoint pinning exists:**

1. Call `retire_cold_hot_zones_externally_locked`.
2. Collect non-checkpoint frames outside every active hot zone's
   `[low, high]` range. These are eviction candidates.
3. Evict the candidate with greatest minimum distance from any hot zone
   boundary (via the single remove helper).
4. Repeat until pool ≤ `CNR3_OUTPUT_CACHE_CAPACITY` or no candidates.
5. If no candidates remain and pool exceeds overflow limit, do not
   evict further.

**Phase B — after non-checkpoint pinning added (Phase CMS02-I only
if promotion criteria met):** Identical to Phase A except step 2 also
requires `pin_count == 0`.

#### 4.3.2 Checkpoint Pool Pruning

1. Candidate if: frame number ≠ 0, `pin_count == 0`, AND outside every
   active hot zone.
2. Evict the candidate furthest from any hot zone boundary first.
3. Continue until pool ≤ `CNR3_CHECKPOINT_MIN_RETAIN` or no candidates.

Checkpoints within hot zones or pinned are retained regardless of
`CNR3_CHECKPOINT_MAX_RETAIN`.

### 4.4 Non-Checkpoint Frame Pinning — Deferred

Deferred to Phase CMS02-I. Mandatory promotion if any of:

- `predecessor_missing_when_expected` is non-zero in any realistic test.
- Recovery repeatedly recomputes recently cached frames.
- fmParallelRequests reveals an uncovered race.
- Required hot-zone settings become so broad they defeat pruning.

**Structural change if added:** Change `non_checkpoint_pool` from
`std::map<int, const VSFrame*>` to `std::map<int, Cnr3NonCheckpointSlot>`
where `Cnr3NonCheckpointSlot = { const VSFrame* frame; int pin_count=0; }`.

### 4.5 Bounded Warm-Up Recovery — No-Prior-Checkpoint Case

When no cached output[N-1] exists and no usable checkpoint exists:

```
start = max(0, N - CNR3_OUTPUT_CACHE_CAPACITY)
output[start] = source-copy initialisation (no predecessor blend,
   deliberate approximation)
output[start+1..N] computed using fill-holes-only + rolling predecessor
```

#### 4.5.1 Rolling Predecessor Reference Pattern

```text
prev_ref = nullptr

if cache hit at start:
    prev_ref = find_frame_and_add_ref(start)
else:
    new_frame = initialise_from_source(start)
    store_frame(start, new_frame)
    prev_ref = new_frame   // walk retains original computed ref

for K in start+1 .. N:
    cached = find_frame_and_add_ref(K)
    if cached != nullptr:
        freeFrame(prev_ref)
        prev_ref = cached
        continue

    new_frame = compute_blend(prev_ref, source[K])
    store_frame(K, new_frame)
    freeFrame(prev_ref)
    prev_ref = new_frame

// end of loop: see Section 4.5.2 for final-frame handling
```

Key properties:
- Walk always holds exactly one owned reference to current predecessor.
- New predecessor acquired before old one released — no zero-ref window.
- Concurrent prune evicting the cache slot cannot damage the walk.

#### 4.5.2 Final-Frame Ownership Transfer

At the end of a recovery walk, `prev_ref` owns the last frame
computed or found. Two cases:

**Case A — intermediate predecessor only:** `freeFrame(prev_ref)`.

**Case B — `prev_ref` is the requested output frame N being returned
to VapourSynth:** Use `release()` to transfer ownership. Do NOT call
`freeFrame` after transfer — that would be a double-free.

```cpp
if (prev_ref_is_requested_output) {
    return prev_ref.release();  // ownership transfers to VS
} // else: RAII destructor calls freeFrame automatically
```

#### 4.5.3 Error Path Discipline

On any failure inside the walk, `freeFrame(prev_ref)` and
`freeFrame(new_frame)` (if held) before returning. The
`Cnr3OwnedFrameRef` RAII wrapper handles this automatically.

### 4.6 Hard Ceiling and Abort Policy — Byte-Budget Based

#### 4.6.1 Ceiling Calculation

**[CHANGED CMS05.1 — `CNR3_CACHE_BYTE_BUDGET` raised from 512 MiB to
1 GiB. See changelog entry CMS05.1-1.]**

At `cnr3_create()` time:

```cpp
static int cnr3_subsampled_dimension(int full_size, int subsampling_shift)
{
    return (full_size + ((1 << subsampling_shift) - 1)) >> subsampling_shift;
}

int64_t estimated_frame_bytes = 0;
const int bytes_per_sample = (vi->format.bitsPerSample + 7) / 8;
const int sub_w = vi->format.subSamplingW;
const int sub_h = vi->format.subSamplingH;
for (int p = 0; p < vi->format.numPlanes; ++p) {
    const int pw = (p == 0) ? vi->width
                             : cnr3_subsampled_dimension(vi->width,  sub_w);
    const int ph = (p == 0) ? vi->height
                             : cnr3_subsampled_dimension(vi->height, sub_h);
    estimated_frame_bytes += (int64_t)pw * ph * bytes_per_sample;
}

const int candidate_ceiling =
    (int)(CNR3_CACHE_BYTE_BUDGET / estimated_frame_bytes);
const int active_ceiling = std::clamp(candidate_ceiling,
                                       CNR3_CACHE_MIN_HARD_CEILING,
                                       CNR3_CACHE_MAX_HARD_CEILING);
```

**Worked examples (with `CNR3_CACHE_BYTE_BUDGET = 1 GiB`):**

| Format                | bytes/frame | candidate | active (clamped) |
|---|---|---|---|
| 4:2:0 8-bit  720x576  | 622,080     | 1,726     | **1000** (MAX)   |
| 4:2:2 8-bit  720x576  | 829,440     | 1,294     | **1000** (MAX)   |
| 4:2:0 16-bit 720x576  | 1,244,160   | 863       | 863              |
| 4:2:2 16-bit 720x576  | 1,658,880   | 647       | 647              |
| 4:2:0 8-bit  1920x1080| 3,110,400   | 344       | 344              |
| 4:2:0 16-bit 1920x1080| 6,220,800   | 172       | 172              |
| 4:2:0 8-bit  1440x576 | 1,244,160   | 863       | 863              |

**[NOTE CMS05.1-2 — 720×576 vs 1440×576 disambiguation:]**
Standard PAL VHS source resolution is 720×576 (not 1440×576). At 1 GiB
budget, 720×576 YUV420P8 yields candidate_ceiling ≈ 1,726, correctly
clamped to 1000. A genuine 1440×576 YUV420P8 clip yields
candidate_ceiling ≈ 863, not clamped. If a log entry shows
`active_ceiling=1000` for a clip described as 1440×576, confirm the
actual clip dimensions — it is likely 720×576. No code error exists in
the ceiling calculation.

#### 4.6.2 Abort Policy

A store is allowed if `total_live_refs_after_store <= active_ceiling`.
Rejected if it would cause `total_live_refs_after_store > active_ceiling`
AND prune cannot free any frame.

When rejected:
1. Store returns `false` without taking `addFrameRef` (RC3).
2. Increment `cache_ceiling_hard_aborts`.
3. Calling `getFrame` path executes cleanup (4.6.3) then returns VS
   filter error: *"CNR3: cache ceiling reached ([N] frames). CNR3 is
   designed for near-linear access…"*

`cnr3_output_cache_would_exceed_ceiling_externally_locked()` returns
true iff a subsequent store would cause
`total_live_refs_after_store > active_ceiling`.

#### 4.6.3 Cleanup Discipline on Ceiling Abort

- Pinned checkpoints: unpin exactly once.
- Caller-owned references (including `prev_ref`): `freeFrame` once.
- Source frames obtained from VS: release.
- Destination frames allocated but not returned: release.
- Hot zone state: no rollback.

After cleanup, reference counters remain balanced.

### 4.7 addFrameRef and Pinning — Separate Concerns

| Mechanism | Protects against | Holder | Lifetime |
|---|---|---|---|
| **Hot zone membership** | Slot eviction by prune | Cache manager | Until zone slides/retires past frame |
| **Checkpoint `pin_count`** | Slot eviction (checkpoints) | Recovery walks | Pin/unpin pair |
| **`addFrameRef`** | Underlying VSFrame being freed | Caller | Until `freeFrame` |

`addFrameRef` protects a frame *already found*. It does not make a
missing predecessor appear. The rolling predecessor pattern (Section
4.5.1) ensures the walk always holds a ref to its current predecessor
before it would be needed as input.

### 4.8 arInitial vs. Cache-Hit Hot Zone Update

Hot zone updates fire at `arInitial` for every arriving frame request,
regardless of whether the request later results in a cache hit or
computation.

Diagnostic counters distinguish:
- `hot_zone_updates_at_arInitial`
- `cache_hits_at_arAllFramesReady`
- `recoveries_started_at_arAllFramesReady`

### 4.9 First-In-Best-Dressed Store Idempotency

`cnr3_output_cache_store_frame()` is idempotent by frame number.

When storing `output[N]`:

1. Lock the cache mutex.
2. Check `cache_index` for frame number `N`.
3. **If already exists** (duplicate-store):
   - Do not replace, do not take `addFrameRef`, do not mutate pools.
   - Increment `store_skipped_already_cached`.
   - Increment `duplicate_store_computed_but_discarded`.
   - Return success. Cache has not taken ownership of caller's frame.
4. **If does not exist** (normal):
   - Check ceiling. If would exceed AND prune cannot free: return false.
   - `addFrameRef(frame)`. Increment `cache_addframeref_total`.
   - Insert into pool. Update `cache_index`.
   - If insertion fails: `freeFrame(frame)` (RC3). Return false.
   - Run `prune_after_store`.
   - Return success.

**Caller responsibility on duplicate:** The cache has not taken
ownership. The caller still owns its computed frame and must release
or transfer it normally.

---

## 5. Constants

**[CHANGED CMS05.1 — `CNR3_CACHE_BYTE_BUDGET` raised from 512 MiB to
1 GiB. See changelog entry CMS05.1-1.]**

```
// --- Soft pruning targets ---

CNR3_OUTPUT_CACHE_CAPACITY        = 100
CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Overflow threshold = 110. Prune fires at pool size 111+ (strict ">").

// --- Hard ceiling (byte-budget based) ---

CNR3_CACHE_BYTE_BUDGET            = 1024 * 1024 * 1024  // 1 GiB
CNR3_CACHE_MIN_HARD_CEILING       = 150 frames
CNR3_CACHE_MAX_HARD_CEILING       = 1000 frames

// --- Checkpoints ---

CNR3_CHECKPOINT_INTERVAL          = 10
CNR3_CHECKPOINT_MAX_RETAIN        = 32
CNR3_CHECKPOINT_MIN_RETAIN        = 10

// --- Hot zones ---

CNR3_HOT_ZONE_FORWARD_RADIUS      = 10
CNR3_HOT_ZONE_BACK_RADIUS         = 50
CNR3_MAX_HOT_ZONES                = 5
CNR3_HOT_ZONE_JUMP_THRESHOLD      = FORWARD + BACK + 1  (= 61)
```

---

## 6. Diagnostics — Definitive Counter Specification

All counters are `int64_t`.

**Hot zone counters:**
- `hot_zone_allocations`
- `hot_zone_slides`
- `hot_zone_hits`
- `hot_zone_merges`
- `hot_zone_retirements`
- `hot_zone_new_zone_requests`
- `hot_zone_max_active_observed`
- `hot_zone_updates_at_arInitial`

**Pruning counters:**
- `non_checkpoint_prune_skipped_in_hot_zone`
- `non_checkpoint_prune_skipped_pinned` (RESERVED for Phase CMS02-I)
- `checkpoint_prune_skipped_in_hot_zone`
- `prune_no_candidate_exists`
- `cache_ceiling_hard_aborts`

**Recovery counters:**
- `bounded_warmup_recovery_count`
- `bounded_warmup_recovery_last_start_frame`
- `bounded_warmup_recovery_last_length`
- `recovery_frames_computed`
- `recovery_frames_skipped_already_cached`
- `holes_filled`
- `duplicate_computation_avoided`
- `nearest_checkpoint_recovery_count`
- `no_prior_checkpoint_recovery_count`
- `max_recovery_chain_length_observed`

**Cache operation counters:**
- `cache_hits_at_arAllFramesReady`
- `cache_misses`
- `recoveries_started_at_arAllFramesReady`
- `predecessor_missing_when_expected` *** CRITICAL — Section 4.4 ***
- `checkpoint_missing_when_expected`
- `max_live_cached_frames_observed`
- `max_non_checkpoint_pool_size_observed`
- `max_checkpoint_pool_size_observed`

**Store/duplicate counters:**
- `store_skipped_already_cached`
- `duplicate_store_computed_but_discarded`

**Cache-side reference-count counters:**
- `cache_addframeref_total`
- `cache_freeframe_total`
- Invariant: `cache_addframeref_total - cache_freeframe_total ==
  non_checkpoint_pool.size() + checkpoint_pool.size()`

**Caller-side reference-count counters (development diagnostic):**
- `lookup_owned_ref_acquired_total`
- `lookup_owned_ref_released_total`
- `lookup_owned_ref_transferred_total`
- Invariant: `acquired == released + transferred` at quiescent points

**Debug output policy:**
- Default: headline counters at `cnr3_free`.
- Dev diagnostics: all counters plus hot zone state.
- Per-event: guarded behind `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`.
- **No diagnostic output to stdout under any circumstances.**
- `predecessor_missing_when_expected > 0` → prominent warning always.
- Ref-balance failure → prominent warning always.

---

## 7. Worked Examples

### Example A — Linear Encoding, No Jumps (sliding)

| Arrival | Action | Zone 0 after | Pool size |
|---|---|---|---|
| F=0   | Allocate zone 0 | low=0, high=10 | 1 |
| F=50  | Slide | low=0, high=60 | ~50 |
| F=60  | Slide | low=10, high=70 | ~60 |
| F=100 | Slide | low=50, high=110 | ~100 |
| F=110 | Slide | low=60, high=120 | ~110 |
| F=111 | Slide; **prune fires** | low=61, high=121 | back to ~100 |

Prune candidates: frames outside `[61, 121]`. Evict furthest-first via
single remove helper (RC2).

### Example B — Small Forward Jump Within JUMP_THRESHOLD

At F=80, zone 0 = `low=30, high=90`. F=95 arrives. Distance 5 ≤ 61.
Slide: `low=45, high=105`. Cache lookup proceeds normally.

### Example C — Large Forward Jump (fmParallelRequests, sliding)

F=600 arrives while zone 0 = `low=152, high=212`. Distance 388 > 61.
Jump. Allocate zone 1: `low=550, high=610`. Pre-jump walks continue
protected by zone 0 and their own `prev_ref` ownership. Bounded
recovery for F=600. Zone 0 lazy-retired when range goes cold.

### Example D — No-Prior-Checkpoint Recovery (cold seek)

F=800. No active zones. Allocate zone 0: `low=750, high=810`. No
checkpoint found. Bounded warm-up: `start=700`. Source-copy init.
Rolling-predecessor walk 701..800. At end of walk, `prev_ref` holds
output[800]. Use `release()` to transfer ownership to VapourSynth.

### Example E — Hard Ceiling Abort

16-bit 4:2:2. `active_ceiling=647`. 7 rapid large seeks, each needing
~101 frames. After ~647 stores, next store exceeds ceiling, prune finds
no candidates. Store returns false without `addFrameRef`. Cleanup.
VS error returned. Filter remains valid.

### Example F — Fill-Holes-Only Avoiding Redundant Compute

output[150] already cached. Recovery walk reaches K=150.
`find_frame_and_add_ref(150)` returns cached frame. Skip computation.
Transition: `freeFrame(prev_ref)`, `prev_ref = cached_150`. Continue.

### Example G — Duplicate Store Race

Walk A stores output[200] first. Walk B reaches `store_frame(200, frameB)`:
finds output[200] already in `cache_index`. Returns success without
`addFrameRef`. Walk B then `freeFrame(frameB)` normally. frameB
destroyed. frameA remains in cache with one balanced cache-side ref.
Ref counts balanced throughout.

---

## 8. Phased Implementation Sequence

#### Phase CMS02-A — Documentation, constants, renames
Rename types, add constants, add `Cnr3HotZone`, add
`Cnr3CacheSchedulingMode`, add `Cnr3OwnedFrameRef`, set `CNR3_CACHE_BYTE_BUDGET
= 1024 * 1024 * 1024`. No runtime behaviour change.
**End-of-phase: Design Compliance Review (Section 12).**

#### Phase CMS02-B — Hot zone structures and passive diagnostics
Add hot zone array, helper declarations, passive snapshot fields, ref-count
counter instrumentation, duplicate-store counter instrumentation.
Extend `validate_invariants` with cache-side ref-balance check.
**End-of-phase: Design Compliance Review (may be batched with CMS02-A).**

#### Phase CMS02-C — Hot zone update helpers
Implement sliding update, is-in-hot-zone, retire helpers. Hard-wire
`FmUnordered`. Instrument all zone lifecycle events.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-D — Hot zone aware prune candidate selection
Replace prune inner loops with hot-zone-aware selection. Add ceiling
check and hard abort to store. Implement first-in-best-dressed
idempotency with duplicate-store counters.
**End-of-phase: Design Compliance Review (mandatory before CMS02-E).**

#### Phase CMS02-E — Store/prune-only runtime proving
Store produced frames into `output_cache` after existing path completes.
Do not yet use cached frames for output. Prove ref balance, pool sizes,
prune behaviour, no stdout output, ceiling counters zero in normal
linear encode.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-F — Cache-hit reuse under fmUnordered
Implement `find_frame_and_add_ref` with defensive contract. Wire cache
hit check at arAllFramesReady. Instrument lookup-owned-ref counters.
**End-of-phase: Design Compliance Review (mandatory before CMS02-G).**

#### Phase CMS02-G — Checkpoint recovery and hole filling under fmUnordered
Implement rolling predecessor pattern and final-frame ownership transfer.
Fill-holes-only walk. First-in-best-dressed store handles race safety.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-H — Bounded warm-up recovery under fmUnordered
No-prior-checkpoint warm-up using rolling predecessor and final-frame
transfer. Instrument all warm-up counters.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-I — Empirical review: non-checkpoint pinning decision
Inspect all counters. Promote pinning if mandatory criteria met.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-J — fmParallelRequests wiring and proving
Same sliding update helper. Retirement helper invoked with
`FmParallelRequests`. Test concurrent scenarios. Validate retirement
proxy.
**End-of-phase: Design Compliance Review.**

#### Full fmParallel — explicitly out of scope for this iteration.

---

## 9. Structural Changes Required to Uploaded Code

### 9.1 `cnr3_cache_manager.h` — Structure additions and renames

**A.** Rename `Cnr3CacheManagerV005` → `Cnr3OutputCacheManager`.
Transitional alias permitted; final code must remove it.

**B.** Rename `Cnr3CacheManager` → `Cnr3StrictStreamCache`.

**C.** Add `Cnr3HotZone` struct:
```cpp
struct Cnr3HotZone {
    bool active = false;
    int low = -1;
    int high = -1;
    int last_observed_frame = -1;
    // int64_t: hit_count, slide_count, merge_count,
    //          retirement_count, prune_protection_count
};
```

**D.** Add hot zone array to `Cnr3OutputCacheManager` (size
`CNR3_MAX_HOT_ZONES`, all inactive). Mutex-protected.

**E.** Add `active_ceiling` int field to `Cnr3OutputCacheManager`.

**F.** Add `Cnr3CacheSchedulingMode` enum:
```cpp
enum class Cnr3CacheSchedulingMode { FmUnordered, FmParallelRequests };
```

**G.** Add new constants (Section 5). Remove obsolete ceiling and
extension-margin constants.

**H.** Add new statistics counters (Section 6).

**I.** Add `Cnr3OwnedFrameRef` RAII wrapper (Section 2.2).

**J.** Update `Cnr3Data` members:
```cpp
Cnr3StrictStreamCache  strict_cache;  // formerly: cache
Cnr3OutputCacheManager output_cache;  // formerly: cache_manager_v005
```

### 9.2 `cnr3_cache_manager.h` — New helper declarations

- `cnr3_output_cache_update_hot_zones(cache, frame_number)` — public,
  locks, sliding rule both modes.
- `cnr3_output_cache_is_frame_in_hot_zone_externally_locked(cache, frame_number)`
- `cnr3_output_cache_retire_cold_hot_zones_externally_locked(cache, mode)`
- `cnr3_output_cache_set_ceiling(cache, vi)` — called once at create.
- `cnr3_output_cache_would_exceed_ceiling_externally_locked(cache)`

**H. Lookup helper — defensive contract:**

`cnr3_output_cache_find_frame_and_add_ref(cache, frame_number, vsapi)`:

1. Lock mutex.
2. Find in `cache_index`. If not found: increment `cache_misses`,
   return nullptr.
3. Verify owner pool contains frame_number. If not: increment
   `cache_index_inconsistency_detected`, log warning, return nullptr.
4. Verify stored `VSFrame*` non-null. If null: same as step 3.
5. `vsapi->addFrameRef(frame)`. Increment
   `lookup_owned_ref_acquired_total`.
6. Increment `cache_hits`.
7. Unlock. Return caller-owned `VSFrame*`.

### 9.3 `cnr3_cache_manager.cpp` — Logic changes

**K.** `prune_non_checkpoint_pool_externally_locked`: hot-zone-aware
candidate selection per Section 4.3.1 Phase A.

**L.** `prune_checkpoint_pool_externally_locked`: add hot-zone candidate
filtering.

**M.** `store_frame`: first-in-best-dressed idempotency per Section 4.9.

**N.** `validate_invariants_externally_locked`: add hot zone validity,
total frames ≤ ceiling, cache-side ref-balance check (RC7), and
(development mode) caller-side ref-balance check.

### 9.4 `vapoursynth-Cnr3.cpp` — Wiring changes (future phases)

**O.** `cnr3_create()`:
```cpp
cnr3_output_cache_set_ceiling(d->output_cache, d->vi);
```

**P.** `cnr3_get_frame()` arInitial:
```cpp
cnr3_output_cache_update_hot_zones(cache, frame_number);
```

**Q.** `cnr3_free` debug output: add hot zone state, Section 6 counters,
ref-balance check results. Prominent warning if any balance fails.

### 9.5 Failure-Path and Shutdown Discipline

**Failure paths:** unpin checkpoints, `freeFrame` owned references,
release source and destination frames, no hot-zone rollback, increment
diagnostic counters.

**Shutdown `clear()`:**
1. Acquire mutex.
2. Iterate `non_checkpoint_pool`: `freeFrame` each, erase.
3. Iterate `checkpoint_pool`: warn if `pin_count > 0`, `freeFrame`
   each, erase.
4. Clear `cache_index`. Reset hot zones. Run `validate_invariants`.

---

## 10. Known Hazard Addressed by Hot-Zone Lifecycle Rules

**Hazard:** A concurrent forward jump slides the hot zone past a frame
actively needed as a predecessor by another in-flight walk.

**Resolution:**
- fmUnordered: previous request has completed before new `arInitial`.
- fmParallelRequests: lazy retirement prevents zone retirement while
  recovery walks are active within the zone. The rolling predecessor
  pattern (Section 4.5.1) ensures the walk's `addFrameRef` keeps the
  VSFrame alive even if the cache slot is pruned.
- Runtime verification: `predecessor_missing_when_expected` detects
  any failure. Non-zero value triggers mandatory promotion to
  non-checkpoint pinning (Section 4.4).

---

## 11. Items to Confirm Empirically

**EI1** — Lazy retirement proxy under fmParallelRequests.
**EI2** — `BACK_RADIUS = 50` empirical sufficiency.
**EI3** — Checkpoint retain values `MAX=32`, `MIN=10`.
**EI4** — Ceiling abort frequency (should be zero in normal encoding).
**EI5** — `prune_no_candidate_exists` behaviour.
**EI6** — Non-checkpoint pinning promotion decision (Phase CMS02-I).
**EI7** — Cache-side ref-balance at quiescence and zero at shutdown.
**EI8** — Caller-side ref-balance: `acquired == released + transferred`.
**EI9** — Duplicate-store frequency as indicator of overlapping walks.

---

## 12. Design Compliance Review

### 12.1 Required coding rule

After completing each phase or coherent block of phases, perform a
design-compliance review of all changed code paths and all unchanged
helper functions invoked by those changed paths. Verify that resulting
execution paths follow CMS05.1, not older v005 assumptions.

### 12.2 Verification checklist

1. **Mutex ownership** is correct.
2. **`_externally_locked` helpers** called only while mutex held.
3. **Public helpers** do not deadlock by calling other locking helpers.
4. **No old prune logic** remains in the active path.
5. **No direct `pool.erase()`** bypasses the remove helper (RC2).
6. **No direct cache-owned `addFrameRef`** bypasses the store helper (RC1).
7. **Store collision** follows first-in-best-dressed semantics (RC8).
8. **Store failure after `addFrameRef`** rebalances (RC3).
9. **Lookup helper** takes `addFrameRef` atomically under mutex.
10. **Caller-owned lookup references** freed or transferred exactly once (RC5).
11. **Checkpoint pins** unpinned on every exit path.
12. **Hot-zone state** not rolled back on frame failure.
13. **Cache-side ref-balance invariant** holds (RC7).
14. **Caller-side lookup-ref diagnostics** balance in development mode.
15. **Shutdown `clear()`** releases every cache-owned reference (RC6).
16. **No diagnostics write to stdout.**

### 12.3 Review timing

- CMS02-A and CMS02-B may be reviewed together.
- **CMS02-D must be reviewed before CMS02-E** (first runtime use of
  new store/prune).
- **CMS02-F must be reviewed before CMS02-G** (lookup-owned references
  and rolling predecessor correctness).
- All other phases require their own review.

### 12.4 Outcome

Each review produces: **Pass**, **Pass with notes**, or **Fail**
(with specific findings). Brief but specific — what was found and
what was done.

---

## Appendix A — Sliding vs. Extend-Only: Discussion and Decision

*(Unchanged from CMS05. See that document for full text.)*

Key points:
- CMS02 "never shrink" rule contradicted linear pruning examples.
- CMS03 mode-specific split was an over-correction.
- CMS04/CMS05 adopted sliding in both modes — one algorithm, one
  mental model, consistent with the use case.
- Safety preserved by rolling predecessor pattern + `addFrameRef`.
- Pathological stall case produces more recompute, not bad output.
- Detectable via `predecessor_missing_when_expected`.
- Non-checkpoint pinning is the escalation path if needed.

---

## Appendix B — Reference-Count Discipline: Discussion and Decision

*(Unchanged from CMS05. See that document for full text.)*

Key points:
- Concern raised: dangling `addFrameRef` calls causing runaway VS
  memory use after cache slots are evicted.
- Analysis confirmed: design is robust if RC1–RC8 discipline is
  followed. Five specific leak classes identified and closed.
- Sliding does not worsen this — it exercises the remove path more,
  which finds discipline bugs faster.
- Promoted to first-class Goal 5 with named Section 2.2.
- RAII wrapper, diagnostic counters, and shutdown protocol are the
  supporting mechanisms.

---

## Appendix C — Implementation Process Notes and Rationale

*(Unchanged from CMS05. See that document for full text.)*

Subsections:
- C.1: First-in-best-dressed store race narrative
- C.2: RAII wrapper instrumentation strategy
- C.3: Naming history and rationale
- C.4: Why design-compliance review matters
- C.5: Phase timing examples
- C.6: Guiding principle (maintainer quote)

---

## Appendix D — Code Review and Simulation Plan

**[ADDED CMS05.1 — see changelog entry CMS05.1-3.]**

A companion document covers the planned post-implementation code review
and simulation programme:

**Document:** `cnr3_code_review_plan_v5_1.md`
**Version:** matches CMS05.1

The companion document specifies:

- **Static analysis** — 15 review items covering spec compliance, RC
  rule compliance per function, error path completeness, mutex
  correctness, hot zone lifecycle, first-in-best-dressed store,
  final-frame ownership transfer, rolling predecessor pattern,
  pruning correctness, shutdown completeness, instrumentation
  coverage, `validate_invariants` coverage, data structure usage,
  fmUnordered/fmParallelRequests transition safety, and RAII wrapper
  correctness.

- **Monte Carlo simulations** — Three sub-scenarios (fmUnordered,
  fmParallelRequests, fmParallel informational), each comprising 50
  short runs (200 frames, 6 threads, 0–6 frame jitter) plus one
  long run (10,000 frames). CSV output for visualisation in Excel.
  Metrics verified: pool sizes, hot zone behaviour, pruning
  correctness, ref-count balance, predecessor miss rate, duplicate
  store frequency.

- **Design-compliance review** per Section 12 of this spec.

- **Session structure** — two sessions recommended (analysis then
  decision/remediation).

- **Model recommendation** — Opus 4.7 for both sessions.

Refer to `cnr3_code_review_plan_v5_1.md` for the full specification
of each item.

