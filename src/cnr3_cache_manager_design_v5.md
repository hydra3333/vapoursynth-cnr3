# CNR3 Cache Manager — Revised Design Specification CMS05
## Sliding Hot-Zone Pruning with Reference-Count Discipline (Non-Checkpoint Pinning Deferred)

**Date:** 2026-06-01
**Status:** Design specification — ready for coding
**Supersedes:** CMS04 (cnr3_cache_manager_design_v4.md), CMS03, CMS02, CMS01
**Also supersedes:** Bounded-recovery policy in handover snapshot v0.14 section 0.9A
**Companion documents:** CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)

---

## Change Summary CMS04 → CMS05

Each change is annotated in-place with **[CHANGED CMS05 — reason]** or
**[ADDED CMS05 — reason]**. Appendix C records the non-spec rationale and
implementation process notes from the design conversations. The complete
list:

1. **First-in-best-dressed store idempotency.** `store_output_frame` is
   idempotent by frame number — if the requested frame already exists in
   `cache_index`, the store returns success without modifying state and
   without taking `addFrameRef`. Added as RC8 in Section 2.2 and as a new
   Section 4.9. Race-condition rationale in Appendix C.1.

2. **Final-frame ownership transfer in rolling predecessor pattern.**
   When a recovery walk's final `prev_ref` is the requested output frame
   being returned to VapourSynth, ownership transfers via `release()`
   rather than `freeFrame`. Section 4.5.1 updated.

3. **Lookup-owned reference balance counters.** Three new development
   counters track the lifecycle of caller-owned references obtained from
   the lookup helper: acquired, released, transferred. Caller-side
   diagnostic invariant added. The CMS04 `cache_addframeref_lookup_total`
   counter is renamed to `lookup_owned_ref_acquired_total`. Section 6
   updated. RAII wrapper instrumentation strategy in Appendix C.2.

4. **Version-independent naming.** `Cnr3CacheManagerV005` →
   `Cnr3OutputCacheManager`. `Cnr3CacheManager` → `Cnr3StrictStreamCache`.
   `Cnr3Data` members renamed accordingly. Helper function names may
   optionally be renamed to `cnr3_output_cache_*` but this is not
   required for CMS05 (the existing `cnr3_cache_manager_*` helpers are
   generic and need not change). Section 9 updated. Naming history in
   Appendix C.3.

5. **Design-Compliance Review process.** A required review step at the
   end of each implementation phase (or coherent block of phases),
   verifying that the resulting execution paths follow CMS05 rather than
   older v005 assumptions. New Section 12. Rationale in Appendix C.4.

6. **Additional store-related diagnostic counters.**
   `store_skipped_already_cached` (headline) and
   `duplicate_store_computed_but_discarded` (development) added to
   Section 6.

7. **Appendix C added.** Records implementation process notes,
   non-directly-spec rationale, and design history that supports the
   spec but does not belong in the main body.

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
   *(Added CMS05 — concurrent overlapping recovery walks may both
   compute output[N] and attempt to store it. Without idempotency, the
   second store could either overwrite (leaking the first frame's
   cache-side ref) or take a second `addFrameRef` on the same slot.
   Either breaks the reference-count invariant.)*

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
- If another concurrent request has already filled part of the chain, the
  later walk skips already-filled frames.
- **[CHANGED CMS05]** If a concurrent walk loses a store race (both
  computed the same frame, the other walk's store landed first), the
  losing walk's store helper returns success without taking ownership
  of the losing walk's frame. The losing walk releases its computed
  frame normally (Section 4.9).

**Reference-ownership rule:**

When output[K] is found in cache and used as the predecessor for
output[K+1] *outside* the cache mutex, the lookup helper takes
`addFrameRef()` on the found frame *while still holding the cache mutex*
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

The cache manager interacts with this reference count whenever it
stores, looks up, removes, or destroys a slot. The Reference-Count
Invariant (Goal 5) requires that all such interactions remain balanced
— no orphan references, no premature releases.

**Hard rules:**

**RC1 — Single store helper.** All cache-owned `addFrameRef` calls
occur in `cnr3_output_cache_store_frame()`. No other code path takes a
cache-owned reference.

**RC2 — Single remove helper.** All cache-owned `freeFrame` calls
occur in `cnr3_output_cache_remove_frame_externally_locked()` (and its
internal callers within prune). No code path may `pool.erase()` or
`cache_index.erase()` directly without going through the remove helper.

**RC3 — Store error paths must rebalance.** If a path inside the store
helper takes `addFrameRef` and then fails to complete insertion (e.g.,
validation rejection, allocation failure), it must execute a matching
`freeFrame` before returning. The store helper is the single point of
truth for this rebalancing — callers do not retry-balance.

**RC4 — Lookup error paths must rebalance.** If
`find_frame_and_add_ref` takes `addFrameRef` and then fails before
returning to the caller, it must execute a matching `freeFrame` before
returning nullptr.

**RC5 — Caller exit paths must `freeFrame`.** Every code path that
receives a caller-owned reference from a lookup helper must `freeFrame`
on every exit path: success, error, early return, exception, hard
abort. Forgetting one exit path leaks one reference per invocation.

**RC6 — Shutdown must clear.** Destruction of `Cnr3OutputCacheManager`
(or its explicit `clear()` called before destruction) must iterate all
slots in both pools, `freeFrame` each, and log a warning for any slot
with `pin_count > 0` (logical leak, indicates a missed unpin). See
Section 9.5.

**RC7 — Validation enforces balance.** `validate_invariants` includes
a ref-balance check: at quiescence,
`cache_addframeref_total - cache_freeframe_total ==
 total_live_slots_across_both_pools`. The counters are updated under
the cache mutex at each store/remove.

**RC8 — First-in-best-dressed store idempotency.** **[ADDED CMS05]**
The store helper is idempotent by frame number. If `output[N]` already
exists in `cache_index` at store time, the store returns success
without taking `addFrameRef`, without modifying either pool, and
without disturbing the existing slot's checkpoint/non-checkpoint
classification. The cache manager has not taken ownership of the
caller's supplied frame in this case; the caller continues to release
or transfer its own frame reference according to normal caller-side
ownership rules. See Section 4.9 for full semantics.

**Recommended pattern — RAII wrapper:**

To reduce the surface area for RC5 violations, callers should use a
small RAII wrapper at call sites:

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
    // Disable copy, enable move
    Cnr3OwnedFrameRef(const Cnr3OwnedFrameRef&) = delete;
    Cnr3OwnedFrameRef& operator=(const Cnr3OwnedFrameRef&) = delete;
    Cnr3OwnedFrameRef(Cnr3OwnedFrameRef&& other) noexcept;
    Cnr3OwnedFrameRef& operator=(Cnr3OwnedFrameRef&& other) noexcept;
    // Release ownership (e.g., when transferring to VapourSynth)
    const VSFrame* release() noexcept { auto f = frame; frame = nullptr; return f; }
};
```

Use:

```cpp
Cnr3OwnedFrameRef pred(
    cnr3_output_cache_find_frame_and_add_ref(cache, K-1, vsapi),
    vsapi
);
if (!pred.frame) { /* cache miss path */ }
// ... use pred.frame ... ref is released automatically at scope exit

// When transferring ownership (e.g., returning to VS):
return pred.release();  // wrapper will NOT freeFrame
```

The wrapper is recommended, not mandatory. Direct
`addFrameRef`/`freeFrame` is acceptable if RC5 discipline is followed.

**Caller-side diagnostic balance (development only):**

**[ADDED CMS05 — see Appendix C.2 for RAII instrumentation strategy.]**

Three development counters track caller-owned reference lifecycle:

- `lookup_owned_ref_acquired_total` — incremented inside
  `find_frame_and_add_ref` when it successfully takes `addFrameRef`.
- `lookup_owned_ref_released_total` — incremented when a caller-owned
  lookup reference is `freeFrame`d.
- `lookup_owned_ref_transferred_total` — incremented when a
  caller-owned lookup reference is transferred to VapourSynth or
  another owner via `release()` (i.e., not released by the holder).

The caller-side diagnostic invariant, checked at suitable quiescent
points in development diagnostics:

```
lookup_owned_ref_acquired_total ==
    lookup_owned_ref_released_total + lookup_owned_ref_transferred_total
```

This is **not** a cache-side invariant. It is a caller-side diagnostic
invariant intended to detect leaks or transfer mistakes in code using
the lookup helper. It is essential while non-checkpoint pinning is
deferred and caller-owned refs are load-bearing.

The simplest implementation is explicit call-site instrumentation
rather than embedding counters in the RAII wrapper (see Appendix C.2).

---

## 3. Simulation Results — Linear Encoding Jitter

A Monte Carlo simulation (200 runs, 100 frames, varied jitter)
characterised realistic frame arrival patterns for the primary linear
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

---

## 4. Algorithm Overview

### 4.1 Hot Zone Tracking

The cache manager maintains a small fixed-size array of hot zones. Each
hot zone represents a contiguous range of frame numbers currently active
(one or more in-flight computations work within that range).

A hot zone has:
- An **active** flag
- A **low** frame number boundary (inclusive)
- A **high** frame number boundary (inclusive)
- A **last_observed_frame** — most recently arrived frame number that
  fell within or caused this zone
- Diagnostic counters: `hit_count`, `slide_count`, `merge_count`,
  `retirement_count`, `prune_protection_count`

Discrete hot zones (rather than a single watermark or rolling median)
allow multiple simultaneous active ranges to be represented
independently.

### 4.2 Hot Zone Lifecycle — Sliding (Both Modes)

#### 4.2.1 Sliding rule (both modes)

For an arriving frame `F`:

1. **Find a candidate zone.** Scan active zones. Find the nearest active
   zone Z such that `F` is within `CNR3_HOT_ZONE_JUMP_THRESHOLD` of Z's
   range. If multiple zones satisfy this, pick the one with smallest
   absolute distance from F to the nearest boundary.

2. **Slide the candidate zone.** If such a Z is found:
   ```
   Z.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   Z.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   Z.last_observed_frame = F
   ```
   Increment `slide_count` if bounds moved, or `hit_count` if not.

3. **Allocate a new zone.** If no Z is within threshold, this is a jump
   event. See 4.2.3.

#### 4.2.2 Why sliding is safe in both modes

The safety argument relies on:

- **`addFrameRef` discipline** (Section 4.7) — a walk that has already
  looked up a predecessor holds a caller-owned reference. Even if a
  slide moves the zone past that predecessor's frame and a subsequent
  prune evicts the cache slot, the walk's owned reference keeps the
  underlying VSFrame alive.

- **Rolling predecessor reference pattern** (Section 4.5.1) — walks
  hold an owned reference to the current predecessor and transition
  ownership across iterations without ever holding zero references.

Full discussion in Appendix A.

#### 4.2.3 New zone allocation

When `F` is outside all active zones by more than `JUMP_THRESHOLD`:

1. Look for a free (inactive) zone slot.
2. If found, initialise:
   ```
   slot.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   slot.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   slot.last_observed_frame = F
   slot.active = true
   ```
   Increment `hot_zone_allocations`.
3. If no free slot:
   - Attempt retirement of a cold zone (4.2.4).
   - If no zone is eligible for retirement, merge the two closest
     zones (4.2.5).

#### 4.2.4 Retirement (mode-specific)

**fmUnordered (eager retirement):** When a new `arInitial` fires, the
previous request has completed. Zones whose `last_observed_frame` is
not the current `F` can be retired immediately. In practice fmUnordered
operates with at most one active zone, sliding forward on linear and
being replaced (retire + new alloc) on a jump.

**fmParallelRequests (lazy retirement):** Multiple requests may be in
flight. A zone is eligible for lazy retirement only when:
- No live frames remain in either pool within its `[low, high]` range,
  AND
- No pinned checkpoint exists within its range (conservative proxy).

Retirement attempted only when a new zone allocation needs a free slot
and none is available.

**fmParallel:** out of scope for this iteration.

#### 4.2.5 Zone merge

When all slots are full and no zone is eligible for retirement:

1. Find the two zones with smallest gap between boundaries.
2. Merge them:
   ```
   merged.low  = min(Z1.low,  Z2.low)
   merged.high = max(Z1.high, Z2.high)
   merged.last_observed_frame = max(Z1.last_observed_frame,
                                     Z2.last_observed_frame)
   ```
3. Mark one slot inactive; store merged in the other.
4. Increment `merge_count`.
5. Use the freed slot for the new zone allocation.

Merging is conservative — no frames lose protection.

### 4.3 Pruning Policy — Hot Zone Aware (Phase-Guarded)

#### 4.3.1 Non-Checkpoint Pool Pruning

**Phase A — before non-checkpoint pinning exists:**

1. Call `retire_cold_hot_zones_externally_locked` (lazy retirement
   pass, mode-aware).
2. Collect non-checkpoint frames whose frame number falls outside every
   active hot zone's `[low, high]` range. These are eviction candidates.
3. Among candidates, find the one with greatest minimum distance from
   any active hot zone boundary. Evict it (via the single remove helper,
   which executes `freeFrame` per RC2).
4. Repeat step 3 until
   `non_checkpoint_pool.size() <= CNR3_OUTPUT_CACHE_CAPACITY` or no
   candidates remain.
5. If no candidates remain and the pool exceeds the overflow limit, do
   not evict further. The pool may temporarily exceed the soft target,
   up to the hard ceiling.

**Phase B — after non-checkpoint pinning is added (Phase CMS02-I or
later, only if promotion criteria are met):** Identical to Phase A
except step 2 also requires `pin_count == 0`.

#### 4.3.2 Checkpoint Pool Pruning

Apply hot-zone-aware filtering with the existing `pin_count` check:

1. A checkpoint is a candidate if:
   - Frame number is not zero, AND
   - `pin_count == 0`, AND
   - Frame number falls outside every active hot zone's `[low, high]`.
2. Evict the candidate with greatest distance from any hot zone
   boundary first (via the single remove helper).
3. Continue until
   `checkpoint_pool.size() <= CNR3_CHECKPOINT_MIN_RETAIN` or no
   candidates remain.

Checkpoints within hot zones or pinned are retained regardless of
`CNR3_CHECKPOINT_MAX_RETAIN` (the retain limits are soft triggers).

### 4.4 Non-Checkpoint Frame Pinning — Deferred

Non-checkpoint pinning remains the deterministic solution to FM1 if
hot-zone-aware pruning is shown to be insufficient. The first
implementation defers non-checkpoint pinning and relies on conservative
hot zones, fill-holes-only recovery, the reference-ownership rule, the
rolling predecessor reference pattern, and detailed diagnostics.

**Mandatory promotion criterion:**

Non-checkpoint pinning becomes mandatory if any of the following are
observed during realistic test runs:

- The diagnostic counter `predecessor_missing_when_expected` is non-zero
  during any realistic VHS/VHS-C encode test.
- Recovery repeatedly recomputes frames that were recently cached.
- fmParallelRequests testing reveals a race between prune and active
  predecessor use that hot zones and `addFrameRef` do not cover.
- Hot-zone settings required to prevent the above become so broad that
  they defeat pruning.

**Structural change if added later:**

Change `non_checkpoint_pool` from `std::map<int, const VSFrame*>` to
`std::map<int, Cnr3NonCheckpointSlot>` where
`Cnr3NonCheckpointSlot = { const VSFrame* frame; int pin_count = 0; }`.

### 4.5 Bounded Warm-Up Recovery — No-Prior-Checkpoint Case

When a request arrives for frame N and no cached output[N-1] exists and
no usable nearest-prior checkpoint exists:

```
start = max(0, N - CNR3_OUTPUT_CACHE_CAPACITY)
output[start] = source-copy initialisation (no predecessor blend,
   deliberate approximation)
output[start+1..N] computed using fill-holes-only + rolling predecessor
```

#### 4.5.1 Rolling Predecessor Reference Pattern

A recovery walk maintains an owned reference to the current predecessor
through each iteration. The reference is transitioned (not
released-then-reacquired) when moving from iteration K to K+1.

```text
prev_ref = nullptr  // walk-owned reference to current predecessor

if start == 0 or start has a known cache hit:
    prev_ref = find_frame_and_add_ref(start)
       // or initialise from frame 0 if cache miss

else:
    // bounded warmup start with no predecessor available
    new_frame = initialise_from_source(start)  // source-copy semantics
    store_frame(start, new_frame)
       // cache takes its own addFrameRef
    prev_ref = new_frame
       // walk retains its original computed ref - no addFrameRef needed

for K in start+1 .. N:
    cached = find_frame_and_add_ref(K)
    if cached != nullptr:
        // Fill-holes-only: cache hit, skip computation
        freeFrame(prev_ref)
        prev_ref = cached
        continue

    // Cache miss: compute output[K] from prev_ref
    new_frame = compute_blend(prev_ref, source[K])
    store_result = store_frame(K, new_frame)
       // cache may have taken addFrameRef (first-in-best-dressed:
       // if N was already cached by a concurrent walk, store_frame
       // returns success without taking addFrameRef)
    freeFrame(prev_ref)
       // release our old predecessor
    prev_ref = new_frame
       // walk retains its own computed ref regardless of whether the
       // cache took its own addFrameRef
```

#### 4.5.2 Final-Frame Ownership Transfer

**[ADDED CMS05 — closes a real double-free risk identified in CMS04
review item 2.]**

At the end of a recovery walk, `prev_ref` normally owns the last output
frame computed or found by the walk. Two cases must be distinguished:

**Case A — `prev_ref` is an intermediate predecessor only.** The walk
has computed `output[N]` and stored it; `prev_ref` may still point to
the last frame in the rolling sequence, but the requested output is
something else (e.g., the walk overshot to populate cache). In this
case the walk must `freeFrame(prev_ref)` before returning.

**Case B — `prev_ref` is the requested output frame N being returned
to VapourSynth via `getFrame`.** Ownership transfers to VapourSynth
when the walk returns the VSFrame pointer. The walk must **not**
`freeFrame(prev_ref)` after the transfer — that would be a double-free.

**Required idiom:**

Use `Cnr3OwnedFrameRef::release()` to make the ownership transfer
explicit:

```cpp
// At end of recovery walk:
if (prev_ref_wraps_requested_output_frame) {
    // Transfer ownership to VapourSynth.
    // Wrapper destructor will NOT freeFrame.
    increment lookup_owned_ref_transferred_total or
              cache-side equivalent counter as appropriate
    return prev_ref.release();
} else {
    // Intermediate predecessor — release normally.
    // (No explicit action needed if using RAII wrapper —
    //  destructor handles it at scope exit.)
}
```

This idiom makes the transfer auditable and prevents accidental
double-free or leak. Without it, a maintainer might naively add
`freeFrame(prev_ref)` at end of loop and break the invariant.

#### 4.5.3 Error path discipline

If any step inside the loop fails (`store_frame` returns false at the
ceiling, blend computation fails, etc.), the walk must `freeFrame(prev_ref)`
and `freeFrame(new_frame)` (if held) before returning. The
`Cnr3OwnedFrameRef` RAII wrapper handles this automatically at scope
exit if used consistently.

### 4.6 Hard Ceiling and Abort Policy — Byte-Budget Based

#### 4.6.1 Ceiling Calculation

At `cnr3_create()` time, after `Cnr3Data` construction:

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
    const int plane_width  = (p == 0) ? vi->width
                                      : cnr3_subsampled_dimension(vi->width, sub_w);
    const int plane_height = (p == 0) ? vi->height
                                      : cnr3_subsampled_dimension(vi->height, sub_h);
    estimated_frame_bytes += (int64_t)plane_width * plane_height * bytes_per_sample;
}

const int candidate_ceiling =
    (int)(CNR3_CACHE_BYTE_BUDGET / estimated_frame_bytes);
const int active_ceiling = std::clamp(candidate_ceiling,
                                       CNR3_CACHE_MIN_HARD_CEILING,
                                       CNR3_CACHE_MAX_HARD_CEILING);
```

Worked examples (with `CNR3_CACHE_BYTE_BUDGET = 512 MiB`):

| Format                | bytes/frame | candidate | active (clamped) |
|---|---|---|---|
| 4:2:0 8-bit  720x576  | 622,080     | 863       | 863              |
| 4:2:2 8-bit  720x576  | 829,440     | 647       | 647              |
| 4:2:0 16-bit 720x576  | 1,244,160   | 431       | 431              |
| 4:2:2 16-bit 720x576  | 1,658,880   | 323       | 323              |
| 4:2:0 8-bit  1920x1080| 3,110,400   | 172       | 172              |
| 4:2:0 16-bit 1920x1080| 6,220,800   | 86        | **150** (clamp)  |

This formula works for accepted planar YUV formats (4:2:0, 4:2:2,
4:4:0, 4:4:4). CNR3 accepts three-plane YUV per the handover; grey is
not in scope.

#### 4.6.2 Abort Policy

A store is allowed if `total_live_refs_after_store <= active_ceiling`.
Rejected if it would cause `total_live_refs_after_store > active_ceiling`
AND prune cannot free any frame.

When rejected:

1. The store returns `false` without taking `addFrameRef` (RC3).
2. Increment `cache_ceiling_hard_aborts`.
3. The calling `getFrame` path executes cleanup discipline (4.6.3)
   then returns a VS filter error:

   *"CNR3: cache ceiling reached ([N] frames). CNR3 is designed for
   near-linear access. Large random seeks in rapid succession may
   exceed cache capacity. Reduce seek frequency or use a near-linear
   workflow."*

The helper `cnr3_output_cache_would_exceed_ceiling_externally_locked()`
returns true iff a subsequent store would cause
`total_live_refs_after_store > active_ceiling`.

#### 4.6.3 Cleanup Discipline on Ceiling Abort

A ceiling abort may leave already-successfully-stored frames in the
cache (valid outputs). The failure path must not leave any temporary
runtime state unreleased:

- Any checkpoint pinned during the failed recovery: unpin exactly once.
- Any caller-owned `VSFrame*` reference held by the recovery walk
  (including `prev_ref`): `freeFrame` exactly once.
- Any source frame obtained during the recovery walk: release.
- Any destination frame allocated for the failed frame: release if not
  returned to VapourSynth.
- Hot zone state: no rollback. A zone allocated for the failed request
  will be retired naturally when its range becomes cold.

After cleanup, reference counters remain balanced.

### 4.7 addFrameRef and Pinning — Separate Concerns

| Mechanism | Protects against | Holder | Lifetime |
|---|---|---|---|
| **Hot zone membership** | Slot eviction by prune | Cache manager | Until zone slides/retires past frame |
| **Checkpoint `pin_count`** | Slot eviction by prune (checkpoints) | Recovery walks | Pin/unpin pair on every path |
| **`addFrameRef`** | Underlying VSFrame being freed | Caller of lookup helper | Until `freeFrame` by caller |

Hot-zone protection and `pin_count` are *prune-side* mechanisms; they
prevent the cache from evicting the slot. `addFrameRef` is independent
— it ensures the VSFrame's pixel data remains valid even if the cache
evicts the slot.

`addFrameRef` protects a frame *already found*. It does not make a
missing predecessor appear. The rolling predecessor reference pattern
(Section 4.5.1) addresses this by ensuring the walk always holds a ref
to its current predecessor *before* it would be needed.

### 4.8 arInitial vs. Cache-Hit Hot Zone Update

Hot zone updates fire at `arInitial` for every arriving frame request,
regardless of whether the request later results in a cache hit or
computation.

Diagnostic counters distinguish:

- `hot_zone_updates_at_arInitial` — total `arInitial` calls that
  updated a hot zone.
- `cache_hits_at_arAllFramesReady` — requests served from cache
  without computation.
- `recoveries_started_at_arAllFramesReady` — requests requiring a
  recovery walk.

### 4.9 First-In-Best-Dressed Store Idempotency

**[ADDED CMS05 — formalises the existing v005 store behaviour and
makes RC8 (Section 2.2) operative.]**

`cnr3_output_cache_store_frame()` is idempotent by frame number. The
operative semantics:

When storing `output[N]`:

1. Lock the cache mutex.
2. Check `cache_index` for frame number `N`.
3. **If `output[N]` already exists** (duplicate-store case):
   - Do not replace the existing cached frame.
   - Do not take an additional cache-owned `addFrameRef`.
   - Do not mutate either pool.
   - Do not disturb the existing checkpoint / non-checkpoint
     ownership classification.
   - Increment `store_skipped_already_cached` (headline counter).
   - Increment `duplicate_store_computed_but_discarded` (development
     counter — explicitly named to make clear that the caller's
     computation was wasted even though the cache state is correct).
   - Return success to the caller.
4. **If `output[N]` does not exist** (normal case):
   - Check ceiling (4.6.2). If would exceed and prune cannot free
     candidates, return false without `addFrameRef`.
   - `addFrameRef(frame)` (RC1).
   - Insert into the appropriate pool and update `cache_index`.
   - If insertion fails after `addFrameRef`, balance with `freeFrame`
     (RC3) and return false.
   - Trigger `prune_after_store`.
   - Increment `cache_addframeref_total`.
   - Return success.

**Caller-side responsibility for the duplicate case:**

If `store_frame()` returns success because output[N] was already
cached, the cache manager has not taken ownership of the caller's
supplied frame. The caller must continue to release or transfer its
own frame reference according to normal caller-side ownership rules.
The cache manager must not `freeFrame` a caller-owned frame it did not
retain.

**Why idempotency matters:**

Without it, two overlapping recovery walks both computing `output[N]`
and racing to store would either overwrite (leaking the first frame's
cache-side ref) or double-reference the same slot. Either breaks the
ref-count invariant. With idempotency, the first store wins; the
second is a no-op from the cache's perspective. Full rationale in
Appendix C.1.

**Diagnostic interpretation:**

If `duplicate_store_computed_but_discarded` is high in production,
overlapping recovery walks are common. This is a candidate for later
duplicate-work suppression (e.g., active-computation tracking that
allows a walk to wait for another walk's in-progress result rather
than recomputing). Not implemented in this iteration; the counter
identifies whether it would be worth doing.

---

## 5. Constants

```
// --- Soft pruning targets ---

CNR3_OUTPUT_CACHE_CAPACITY        = 100
CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Prune triggers when non_checkpoint_pool > 110 (strict ">" semantics:
    prune fires at pool size 111+).

// --- Hard ceiling (byte-budget based) ---

CNR3_CACHE_BYTE_BUDGET            = 512 * 1024 * 1024  // 512 MiB
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

All counters are `int64_t`. Placement in `Cnr3OutputCacheStats` (or
equivalent struct) is an implementation choice at coding time.

**Hot zone counters:**

- `hot_zone_allocations`
- `hot_zone_slides` (bounds actually moved)
- `hot_zone_hits` (bounds already covered F; no movement)
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

**Store/duplicate counters (ADDED CMS05):**

- `store_skipped_already_cached` (headline — first-in-best-dressed
  hits)
- `duplicate_store_computed_but_discarded` (development — verbose name
  deliberately chosen to make clear that the caller wasted computation
  even though the cache state remained correct)

**Cache-side reference-count counters:**

- `cache_addframeref_total` (cumulative `addFrameRef` calls inside
  store)
- `cache_freeframe_total` (cumulative `freeFrame` calls inside remove)
- Invariant check at quiescence and shutdown:
  `cache_addframeref_total - cache_freeframe_total ==
   non_checkpoint_pool.size() + checkpoint_pool.size()`

**Caller-side reference-count counters (ADDED CMS05, development
diagnostic):**

- `lookup_owned_ref_acquired_total` (renamed from CMS04's
  `cache_addframeref_lookup_total`; incremented inside
  `find_frame_and_add_ref` on success)
- `lookup_owned_ref_released_total` (incremented when a caller-owned
  lookup reference is `freeFrame`d — call-site instrumentation or RAII
  destructor)
- `lookup_owned_ref_transferred_total` (incremented when a
  caller-owned lookup reference is transferred to another owner via
  `release()`)
- Caller-side diagnostic invariant at quiescent points:
  `lookup_owned_ref_acquired_total ==
   lookup_owned_ref_released_total + lookup_owned_ref_transferred_total`

These caller-side counters are not part of the cache-side invariant;
they are diagnostics for caller discipline. See Appendix C.2 for
instrumentation strategy.

**Debug output policy:**

- Default: headline counters at `cnr3_free`.
- Dev diagnostics enabled: all counters plus hot zone state.
- Per-event verbose output guarded behind
  `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`.
- **No diagnostic output to stdout under any circumstances.**
- `predecessor_missing_when_expected > 0` prints prominent warning
  regardless of diagnostic level.
- Ref-balance failure (cache-side or caller-side) at shutdown prints
  prominent warning regardless of diagnostic level.

---

## 7. Worked Examples

### Example A — Linear Encoding, No Jumps (sliding)

Setup: 32-thread encode, BestSource jitter up to 6 frames. Cache starts
empty.

| Arrival | Action | Zone 0 after | Pool size |
|---|---|---|---|
| F=0   | Allocate zone 0 | low=0, high=10 | 1 |
| F=50  | Slide | low=0, high=60 | ~50 |
| F=60  | Slide | low=10, high=70 | ~60 |
| F=100 | Slide | low=50, high=110 | ~100 |
| F=110 | Slide | low=60, high=120 | ~110 |
| F=111 | Slide; **prune fires** (111 > 110) | low=61, high=121 | back to ~100 |

At F=111, prune candidates are frames outside `[61, 121]`. Frames 0–60
are candidates. Evict furthest-first via the single remove helper (RC2).

Predecessor for current computation is always within zone. No predecessor
failures.

### Example B — Small Forward Jump Within JUMP_THRESHOLD

Setup: at F=80, zone 0 = `low=30, high=90`.

F=95 arrives. Distance from high (90): 5 ≤ 61. Within threshold. Slide
zone 0: `low = max(0, 95-50) = 45`, `high = 95+10 = 105`.

Cache lookup: output[94] hit or miss. On hit, walk uses it via
`find_frame_and_add_ref`. On miss, nearest checkpoint pinned, rolling-
predecessor walk fills holes, checkpoint unpinned.

### Example C — Large Forward Jump (fmParallelRequests, sliding)

Setup: sequential encoding at F=200 with three in-flight walks. Zone 0
slid to F=202: `low=152, high=212`.

F=600 arrives. Distance from zone 0 high: 388 > 61. Jump detected.

Allocate zone 1: `low=550, high=610`.

Bounded recovery for F=600: `start = 500`. Walk uses rolling predecessor
pattern. Frames 500–549 stored but outside both zones — prunable. Walk
already has each as predecessor in `prev_ref`, so prune evicting them
does not damage the walk.

Pre-jump walks complete. Zone 0 lazy-retired via pinned-checkpoint
proxy. Steady state: one active zone around F=600.

### Example D — No-Prior-Checkpoint Recovery (cold seek)

Setup: fresh instance, user seeks to F=800. Cache empty.

F=800 arrives. No active zones. Allocate zone 0: `low=750, high=810`.
No checkpoint found.

Bounded warm-up: `start = 700`. Source-copy initialise output[700].
Rolling-predecessor walk for K=701..800.

**[CHANGED CMS05]** At end of walk, `prev_ref` holds output[800]
(the requested frame). Use `release()` to transfer ownership to
VapourSynth via the return statement. Increment
`lookup_owned_ref_transferred_total`.

Subsequent seeks to 700..800 range use checkpoints as anchors.

### Example E — Hard Ceiling Abort

Setup: 16-bit 4:2:2 input. `active_ceiling = 323`.

5 rapid large seeks before recovery completes. Five concurrent recovery
walks generating ~101 frames each = ~505 total. Ceiling = 323.

After ~323 stores, next store would exceed. Prune: no candidates (all
within hot zones). Store returns false without `addFrameRef`. Increment
`cache_ceiling_hard_aborts`. Cleanup (4.6.3) releases all walk-held
references. VS error returned. Filter remains valid.

### Example F — Fill-Holes-Only Avoiding Redundant Compute

Setup: output[150] already cached. Recovery from checkpoint[145] toward
N=155 reaches K=150.

`find_frame_and_add_ref(150)` returns cached frame. Increment
`recovery_frames_skipped_already_cached` and
`lookup_owned_ref_acquired_total`. Walk transitions: `freeFrame(prev_ref)`
(holding output[149]; increment `lookup_owned_ref_released_total`), then
`prev_ref = cached_150`. Skip computation. Continue.

### Example G — Duplicate Store Race (ADDED CMS05)

Setup: two concurrent recovery walks A and B both reach K=200 and both
attempt to store output[200]. Both have computed independently.

Walk A's `store_frame(200, frameA)` acquires mutex first:
- `cache_index[200]` does not exist.
- Take `addFrameRef(frameA)`.
- Insert into pool, update index.
- `cache_addframeref_total` += 1.
- Release mutex. Return success.

Walk A then `freeFrame(frameA)` (its computed ref). Cache holds the
only ref to frameA's underlying VSFrame.

Walk B's `store_frame(200, frameB)` acquires mutex:
- `cache_index[200]` now exists (frameA).
- First-in-best-dressed: do not take `addFrameRef(frameB)`. Do not
  replace frameA with frameB. Do not mutate either pool.
- Increment `store_skipped_already_cached`.
- Increment `duplicate_store_computed_but_discarded`.
- Release mutex. Return success.

Walk B then `freeFrame(frameB)` (its computed ref). frameB's underlying
VSFrame is destroyed (its ref count reaches zero). frameA remains in
the cache. Both walks proceed normally using output[200] for
predecessor of output[201].

Reference counters remain balanced throughout. No leak, no double-free.

---

## 8. Phased Implementation Sequence

**Note on naming:** Phases produce code that uses the CMS05 names
(`Cnr3OutputCacheManager`, `output_cache`). The reverse rename
(removing `Cnr3CacheManagerV005`) happens in Phase CMS02-A.

#### Phase CMS02-A — Documentation, constants, renames
- Mark CMS05 as the current Phase 4 cache-policy direction.
- Rename `Cnr3CacheManagerV005` → `Cnr3OutputCacheManager`.
- Rename `Cnr3CacheManager` → `Cnr3StrictStreamCache`.
- Rename `Cnr3Data` members: `cache` → `strict_cache`,
  `cache_manager_v005` → `output_cache`.
- (Optional) rename helper functions from `cnr3_cache_manager_*` to
  `cnr3_output_cache_*`. May be deferred to a separate cleanup if
  preferred.
- Add all new constants (Section 5).
- Add all new diagnostic counter declarations (Section 6).
- Add `active_ceiling` field to `Cnr3OutputCacheManager`.
- Implement byte-budget ceiling computation using ceil-style chroma
  formula.
- Add `Cnr3HotZone` struct declaration.
- Add `Cnr3CacheSchedulingMode` enum.
- Add `Cnr3OwnedFrameRef` RAII wrapper.
- Do not change runtime behaviour.
- **End-of-phase: Design Compliance Review (Section 12).**

#### Phase CMS02-B — Hot zone structures and passive diagnostics
- Add hot zone array to `Cnr3OutputCacheManager`.
- Add hot zone helper declarations.
- Add passive debug snapshot fields for hot zone state.
- Add `cache_addframeref_total`, `cache_freeframe_total` instrumentation
  to existing store and remove helpers.
- Add `store_skipped_already_cached` and
  `duplicate_store_computed_but_discarded` to the existing duplicate
  detection (the v005 store already had this check; instrument it
  with the new counters).
- Add `lookup_owned_ref_*` counter declarations.
- Extend `validate_invariants` with cache-side ref-balance check (RC7).
- Do not use hot zones for pruning yet.
- **End-of-phase: Design Compliance Review.** (Phases A and B may be
  reviewed together if they only add structures, counters, and passive
  diagnostics.)

#### Phase CMS02-C — Hot zone update helpers
- Implement `cnr3_output_cache_update_hot_zones()` using sliding rule.
- Implement
  `cnr3_output_cache_is_frame_in_hot_zone_externally_locked()`.
- Implement
  `cnr3_output_cache_retire_cold_hot_zones_externally_locked()` with
  mode-aware retirement. Hard-wire `FmUnordered` for first
  implementation.
- Instrument all zone lifecycle events.
- Do not use hot zones in pruning yet.
- **End-of-phase: Design Compliance Review.**

#### Phase CMS02-D — Hot zone aware prune candidate selection
- Replace `prune_non_checkpoint_pool_externally_locked` inner loop with
  hot-zone-aware candidate selection (Section 4.3.1 Phase A).
- Replace `prune_checkpoint_pool_externally_locked` similarly.
- Add `cnr3_output_cache_would_exceed_ceiling_externally_locked()`.
- Add ceiling check and hard abort to `store_frame`, including
  cleanup discipline.
- Instrument all prune decisions.
- **End-of-phase: Design Compliance Review (mandatory before CMS02-E,
  which begins runtime use of the new prune logic).**

#### Phase CMS02-E — Store/prune-only runtime proving
- In the existing strict-streaming output path, store produced frames
  into `output_cache` after the existing path completes.
- Call `prune_after_store`.
- Do not yet use cached frames for output generation.
- Enable memory diagnostics.
- Prove: cache-side ref balance at end of every operation, ref-balance
  counter check at shutdown, pool sizes, prune behaviour, hot zone
  allocation/retirement, no stdout output, ceiling counters zero
  during normal linear encode.
- **End-of-phase: Design Compliance Review.**

#### Phase CMS02-F — Cache-hit reuse under fmUnordered
- Implement `cnr3_output_cache_find_frame_and_add_ref()` with the
  defensive contract (Section 9.2.H).
- At the start of `cnr3_get_frame` arAllFramesReady, check cache hit.
  If hit, return cached frame via caller-owned ref using
  `Cnr3OwnedFrameRef`. If miss, proceed with normal computation.
- Instrument `cache_hits_at_arAllFramesReady` and lookup-owned-ref
  counters at call sites.
- **End-of-phase: Design Compliance Review (mandatory before CMS02-G,
  which depends on lookup-owned references and rolling predecessor
  correctness).**

#### Phase CMS02-G — Checkpoint recovery and hole filling under fmUnordered
- `find_and_pin_nearest_prior_checkpoint` for out-of-order requests.
- Implement rolling predecessor reference pattern (Section 4.5.1).
- Implement final-frame ownership transfer (Section 4.5.2).
- Fill missing frames ascending using fill-holes-only rule.
- Skip frames already present in cache. Rely on first-in-best-dressed
  store idempotency (Section 4.9) for race safety.
- Store newly generated frames; `prune_after_store`.
- Unpin checkpoint on every exit path.
- Instrument all relevant counters including the new lookup-owned
  ones.
- **End-of-phase: Design Compliance Review.**

#### Phase CMS02-H — Bounded warm-up recovery under fmUnordered
- Implement no-prior-checkpoint warm-up per Section 4.5 using the
  rolling predecessor pattern and final-frame ownership transfer.
- Instrument all warm-up counters.
- **End-of-phase: Design Compliance Review.**

#### Phase CMS02-I — Empirical review: non-checkpoint pinning decision
After realistic VHS/VHS-C encode tests and synthetic jump tests:
- Inspect all Section 6 counters, especially
  `predecessor_missing_when_expected`, ref-balance counters, and
  lookup-owned-ref counters.
- If mandatory promotion criteria (Section 4.4) are met, implement
  non-checkpoint pinning before proceeding.
- If criteria are not met, document findings and proceed to CMS02-J.
- **End-of-phase: Design Compliance Review.**

#### Phase CMS02-J — fmParallelRequests wiring and proving
Only after CMS02-H proven and CMS02-I decision made.
- Wire fmParallelRequests path: same sliding update helper, retirement
  helper invoked with `FmParallelRequests` mode.
- Test concurrent jump scenarios. Verify
  `store_skipped_already_cached` increments appropriately under
  overlapping walks.
- Validate retirement with pinned-checkpoint proxy.
- Decide on `active_request_count` per zone if retirement proves too
  conservative.
- **End-of-phase: Design Compliance Review.**

#### Full fmParallel — explicitly out of scope for this iteration.

---

## 9. Structural Changes Required to Uploaded Code

### 9.1 `cnr3_cache_manager.h` — Structure additions and renames

**A. Rename type:**

```cpp
// Old:
struct Cnr3CacheManagerV005 { ... };
// New:
struct Cnr3OutputCacheManager { ... };
```

A transitional alias may be used during a multi-patch transition:

```cpp
using Cnr3CacheManagerV005 = Cnr3OutputCacheManager;
```

but the final code must remove the alias and the `v005` name entirely.

**B. Rename associated old type (strict-streaming cache):**

```cpp
// Old:
struct Cnr3CacheManager { ... };
// New:
struct Cnr3StrictStreamCache { ... };
```

Same transitional-alias strategy is acceptable.

**C. Add `Cnr3HotZone` struct:**

```cpp
struct Cnr3HotZone {
    bool active = false;
    int low = -1;
    int high = -1;
    int last_observed_frame = -1;
    // diagnostic counters (int64_t):
    //   hit_count, slide_count, merge_count, retirement_count,
    //   prune_protection_count
};
```

**D. Add hot zone array to `Cnr3OutputCacheManager`:** Fixed-size array
of `Cnr3HotZone`, size `CNR3_MAX_HOT_ZONES`. Mutable state — access
only while holding `cache_mutex`.

**E. Add `active_ceiling` field to `Cnr3OutputCacheManager`:** int, set
once at `cnr3_create()` via byte-budget computation.

**F. Add `Cnr3CacheSchedulingMode` enum:**

```cpp
enum class Cnr3CacheSchedulingMode {
    FmUnordered,
    FmParallelRequests
};
```

**G. Add new constants per Section 5.** Remove obsolete frame-count
ceiling constants and `CNR3_HOT_ZONE_EXTENSION_MARGIN`.

**H. Add new statistics counters per Section 6,** including ref-count
counters (cache-side and caller-side).

**I. Add `Cnr3OwnedFrameRef` RAII wrapper per Section 2.2.**

**J. Update `Cnr3Data` members:**

```cpp
struct Cnr3Data {
    ...
    Cnr3StrictStreamCache strict_cache;    // formerly: cache
    Cnr3OutputCacheManager output_cache;   // formerly: cache_manager_v005
    ...
};
```

**NOTE:** Non-checkpoint pinning structural change is deferred to
Phase CMS02-I.

### 9.2 `cnr3_cache_manager.h` — New helper declarations

Helper names use the existing `cnr3_cache_manager_*` prefix or the new
`cnr3_output_cache_*` prefix at the implementer's choice. CMS05 uses
the `cnr3_output_cache_*` form for clarity but does not require
renaming existing helpers.

**Hot zone helpers:**

- `cnr3_output_cache_update_hot_zones(cache, frame_number)` — public,
  locks. Uses sliding rule from Section 4.2.1 in both modes. No mode
  parameter.

- `cnr3_output_cache_is_frame_in_hot_zone_externally_locked(cache,
  frame_number)` — bool, caller holds mutex.

- `cnr3_output_cache_retire_cold_hot_zones_externally_locked(cache,
  mode)` — caller holds mutex. Takes `Cnr3CacheSchedulingMode`.

- `cnr3_output_cache_set_ceiling(cache, vi)` — public, called once.

**Ceiling check:**

- `cnr3_output_cache_would_exceed_ceiling_externally_locked(cache)` —
  bool. Returns true iff `total_live_refs_after_store >
  active_ceiling`.

**H. Cache hit lookup — defensive contract:**

`cnr3_output_cache_find_frame_and_add_ref(cache, frame_number, vsapi)`
must:

1. Lock `cache_mutex`.
2. Find `frame_number` in `cache_index`.
3. If not found, increment `cache_misses`, unlock, return nullptr.
4. Verify the indexed owner pool actually contains `frame_number`. If
   not, this is an internal inconsistency — increment
   `cache_index_inconsistency_detected`, log warning, unlock, return
   nullptr.
5. Verify the stored `VSFrame*` is non-null. If null, same handling.
6. Call `vsapi->addFrameRef(frame)`. Increment
   `lookup_owned_ref_acquired_total`.
7. Increment `cache_hits` (or context-specific counter).
8. Unlock `cache_mutex`.
9. Return the caller-owned `VSFrame*`.

The caller must `freeFrame` the returned reference on every exit path
(RC5). The `Cnr3OwnedFrameRef` wrapper handles this at scope exit.

### 9.3 `cnr3_cache_manager.cpp` — Logic changes

**K. `prune_non_checkpoint_pool_externally_locked`:** Replace
`while { evict begin() }` with hot-zone-aware candidate selection per
Section 4.3.1 Phase A. Call `retire_cold_hot_zones` first. Every
eviction goes through the single remove helper (RC2).

**L. `prune_checkpoint_pool_externally_locked`:** Add hot-zone
candidate filtering before existing frame-zero and `pin_count` checks.

**M. `store_frame` (public)** — **[CHANGED CMS05 per Section 4.9 to
implement first-in-best-dressed idempotency]:**

```text
1. Lock cache_mutex.
2. Check cache_index for frame_number.
3. If present:
   - Increment store_skipped_already_cached.
   - Increment duplicate_store_computed_but_discarded.
   - Unlock. Return success (no addFrameRef taken).
4. Call would_exceed_ceiling.
5. If exceeded AND prune cannot free anything:
   - Increment cache_ceiling_hard_aborts.
   - Unlock. Return false (no addFrameRef taken — RC3 preserved).
6. addFrameRef(frame). Increment cache_addframeref_total.
7. Insert into pool. Update cache_index.
8. If insertion failed:
   - freeFrame(frame). Increment cache_freeframe_total. (RC3 rebalance.)
   - Unlock. Return false.
9. Run prune_after_store (which uses the single remove helper for any
   evictions, preserving RC2 and ref-balance).
10. Unlock. Return success.
```

The caller-side responsibility for the duplicate case (Section 4.9) is
the caller's contract: if `store_frame` returns success and the cache
did not take the frame, the caller still owns its computed frame and
must release it.

**N. `validate_invariants_externally_locked`:** Add:

- All hot zone slots with `active==true` have `low >= 0`,
  `high >= low`, `last_observed_frame >= 0`.
- Total live frames ≤ `active_ceiling`.
- Cache-side ref-balance check (RC7):
  `cache_addframeref_total - cache_freeframe_total ==
   non_checkpoint_pool.size() + checkpoint_pool.size()`.
- (Development mode) caller-side ref-balance check at quiescence:
  `lookup_owned_ref_acquired_total ==
   lookup_owned_ref_released_total + lookup_owned_ref_transferred_total`.

### 9.4 `vapoursynth-Cnr3.cpp` — Wiring changes (future phases)

**O. `cnr3_create()`:**

```cpp
cnr3_output_cache_set_ceiling(d->output_cache, d->vi);
```

**P. `cnr3_get_frame()` arInitial:**

```cpp
cnr3_output_cache_update_hot_zones(cache, frame_number);
```

Called for every arriving frame request before any cache lookup.

**Q. Debug output at `cnr3_free`:**

Add hot zone state summary, new Section 6 counters, both ref-balance
check results to existing debug summary. Ref-balance failure (cache or
caller side) prints prominent warning regardless of diagnostic level.

### 9.5 Failure-Path and Shutdown Discipline

Every failure path that returns a VapourSynth error must execute
cleanup before returning:

1. Any checkpoint pinned during this operation: `unpin_checkpoint` once.
2. Any caller-owned VSFrame references obtained via the lookup helper:
   `freeFrame` once. Use `Cnr3OwnedFrameRef` RAII or explicit call-site
   increments of `lookup_owned_ref_released_total`.
3. Any source frame references obtained from VapourSynth: `freeFrame`.
4. Any destination frame allocated but not returned: `freeFrame`.
5. Hot zone state: no rollback needed.
6. Diagnostic counters: still incremented as appropriate.

**Shutdown protocol (`Cnr3OutputCacheManager` destruction):**

A `clear()` method (callable explicitly or invoked from destructor)
must:

1. Acquire `cache_mutex` for the duration.
2. Iterate `non_checkpoint_pool`: for each slot, `freeFrame(frame)`,
   increment `cache_freeframe_total`. Erase slot.
3. Iterate `checkpoint_pool`: for each slot:
   - If `pin_count > 0`, log warning citing frame number and
     `pin_count` value (logical leak indicator).
   - `freeFrame(frame)`, increment `cache_freeframe_total`. Erase.
4. Clear `cache_index`.
5. Reset hot zone slots to inactive.
6. Run `validate_invariants` to confirm balance.

---

## 10. Known Hazard Addressed by Hot-Zone Lifecycle Rules

**Hazard scenario (test case):**

> 1. Request A is computing output[450].
> 2. It needs output[449] as the predecessor.
> 3. output[449] is present in the non-checkpoint pool.
> 4. A forward jump creates a new hot zone around output[800].
> 5. The old hot zone is shifted, merged, or retired.
> 6. Pruning sees output[449] as outside all hot zones and removes it.
> 7. Request A no longer has the predecessor it expected.

**Resolution by CMS05 design:**

- **Step 5 in fmUnordered:** Previous request has completed before this
  `arInitial` fires.
- **Step 5 in fmParallelRequests:** Lazy retirement (pinned-checkpoint
  proxy) prevents premature retirement of zones with active recovery.
- **Step 6:** Even if a slide moves the zone past output[449] and a
  prune evicts the cache slot, the walk's owned `addFrameRef` (held
  through the rolling predecessor pattern, Section 4.5.1) keeps the
  underlying VSFrame alive. The cache slot is removed; the VSFrame
  itself is destroyed only when its ref count reaches zero, which the
  walk's reference prevents.
- **Runtime verification:** `predecessor_missing_when_expected` is the
  empirical detector. If it ever increments, non-checkpoint pinning
  becomes mandatory (Section 4.4).

The hazard is addressed by hot-zone lifecycle rules + rolling
predecessor pattern + `addFrameRef` discipline + first-in-best-dressed
store idempotency, and continuously monitored at runtime.

---

## 11. Items to Confirm Empirically

**EI1 — Lazy retirement proxy under fmParallelRequests.** Accept the
pinned-checkpoint proxy initially. If too conservative, add
`active_request_count` per zone.

**EI2 — `BACK_RADIUS = 50` empirical sufficiency.** Observe
`recovery_frames_computed` and `cache_hits_at_arAllFramesReady`.

**EI3 — Checkpoint retain values.** Starting `MAX=32`, `MIN=10`.
Observe `max_checkpoint_pool_size_observed`.

**EI4 — Ceiling abort frequency.** `cache_ceiling_hard_aborts` should
be zero during normal linear encoding.

**EI5 — `prune_no_candidate_exists` behaviour.** Accept "do not evict
protected frames" initially.

**EI6 — Non-checkpoint pinning promotion decision (Phase CMS02-I).**
The most important empirical decision.

**EI7 — Cache-side reference-count balance.**
`cache_addframeref_total - cache_freeframe_total` must equal total
live slots at quiescence and zero at shutdown.

**EI8 — Caller-side reference-count balance** (ADDED CMS05).
`lookup_owned_ref_acquired_total == released + transferred` at
quiescence. Particularly important during Phases CMS02-F through
CMS02-H where caller-owned refs are exercised.

**EI9 — Duplicate-store frequency** (ADDED CMS05).
`duplicate_store_computed_but_discarded` indicates how much
computation is wasted by overlapping recovery walks. If high, future
work on duplicate-work suppression may be justified.

---

## 12. Design Compliance Review

**[ADDED CMS05 — required process step at the end of each phase or
coherent block of phases, per CMS04 review item 5. Rationale in
Appendix C.4.]**

### 12.1 Required coding rule

After completing each implementation phase (Section 8) or a coherent
block of phases, perform a design-compliance review of all changed
code paths and all unchanged helper functions invoked by those changed
paths.

The review must verify that the resulting execution paths follow
CMS05, not older v005 assumptions.

### 12.2 Verification checklist

For every affected path, verify:

1. **Mutex ownership** is correct.
2. **`_externally_locked` helpers** are called only while the mutex is
   held.
3. **Public helpers** that lock internally do not call other public
   locking helpers in a way that can deadlock.
4. **No old prune logic** remains in the active path.
5. **No direct `pool.erase()`** bypasses the single remove helper
   (RC2).
6. **No direct cache-owned `addFrameRef`** bypasses the single store
   helper (RC1).
7. **Store collision** follows first-in-best-dressed semantics
   (Section 4.9, RC8).
8. **Store failure after `addFrameRef`** rebalances correctly (RC3).
9. **Lookup helper** takes `addFrameRef` atomically under the mutex.
10. **Caller-owned lookup references** are freed or transferred
    exactly once (RC5).
11. **Checkpoint pins** are unpinned on every exit path.
12. **Hot-zone state** does not need rollback on frame failure.
13. **Cache-side ref-balance invariant** holds (RC7).
14. **Caller-side lookup-ref diagnostics** balance in development
    mode.
15. **Shutdown `clear()`** releases every cache-owned frame reference
    and clears indexes (RC6).
16. **No diagnostics write to stdout.**

### 12.3 Review timing

Preferably, the review happens at the end of each phase.

For tightly coupled work, deferral to the end of a small block of
phases is acceptable, but the review must not be omitted.

Mandatory single-phase reviews (cannot be batched with subsequent
phases):

- **CMS02-D must be reviewed before CMS02-E**, because CMS02-E begins
  runtime use of the new store/prune behaviour.
- **CMS02-F must be reviewed before CMS02-G**, because CMS02-G depends
  on lookup-owned references and rolling predecessor correctness.

Phases that may reasonably be batched (only if they consist purely of
declarations, structs, counters, and passive diagnostics):

- CMS02-A and CMS02-B may be reviewed together.

All other phases require their own review.

### 12.4 Outcome

Each design-compliance review produces a written outcome:

- **Pass** — all checklist items verified. Phase complete. Proceed to
  next phase.
- **Pass with notes** — all checklist items verified, but observations
  recorded for follow-up (e.g., a refactoring opportunity, a counter
  that warrants closer monitoring).
- **Fail** — one or more checklist items not verified. Phase not
  complete. Address findings before proceeding.

The written outcome should be brief but specific — for each non-pass
item, what was found and what was done about it.

---

## Appendix A — Sliding vs. Extend-Only: Discussion and Decision

[Content unchanged from CMS04 Appendix A. Retained verbatim for
maintainer reference.]

### Background

CMS02 attempted "extend-only, never shrink" zones in all modes. This
contained a fatal contradiction: linear encoding examples required the
zone's back boundary to advance forward, but the rule said zones never
shrink. CMS03 resolved by introducing mode-specific lifecycles.

### The Concern Raised

During CMS03 review, the user observed that the working pattern in
both modes is conceptually similar: a hot area sliding along the
timeline with concurrent threads bubbling in it. A different algorithm
for fmParallelRequests felt inconsistent.

### Analysis

**Sequential linear encoding under sliding in fmParallelRequests:**

16 concurrent walks computing frames 100–115. When frame 100's
`arInitial` fires, zone = `[50, 110]`. When frame 115's `arInitial`
fires, zone slides to `[65, 125]`. Predecessors 99–114 all lie inside
`[65, 125]`. No predecessor loses protection.

With `BACK_RADIUS=50` and typical concurrency spread of 22–38 frames,
sliding never strips protection from in-flight predecessors under
normal sequential operation.

**The pathological case:** A walk for frame 100 stalls while
concurrent walks slide the zone to `[150, 210]`. The stalled walk
calls `find_frame_and_add_ref(99)`. Frame 99 has been pruned.

This is **not a crash and not incorrect output**. The walk falls back
to checkpoint recovery; checkpoints are retained longer
(`MAX_RETAIN=32` plus hot-zone protection). The pathological case
produces **more recompute work**, not bad data, and is detectable via
`predecessor_missing_when_expected`.

**Role of `addFrameRef` and the rolling predecessor pattern:**

A walk that has already begun using a predecessor holds the owned
reference. The slide does not damage walks mid-computation. The
rolling predecessor pattern ensures a walk's K→K+1 transition never
has a window of zero references — the walk acquires the new
predecessor *before* releasing the old one.

### Decision

**Use sliding in both modes.** The only mode-specific behaviour is
**retirement**.

Decision rationale:
1. Conceptually simpler — one algorithm, one mental model.
2. Consistent with the use case (a sliding hot area).
3. Safety preserved by rolling predecessor + `addFrameRef`.
4. Pathological case produces correct output (just more compute).
5. Diagnostics detect it.
6. Non-checkpoint pinning is the well-understood escalation path.

### Maintainer Caveats

- If `predecessor_missing_when_expected` ever increments in production,
  revisit. Either promote to non-checkpoint pinning, or reintroduce
  extend-only in fmParallelRequests.
- The rolling predecessor pattern is load-bearing. If broken, all
  safety arguments collapse.
- Sliding is "good enough given other mechanisms," not "always best."

---

## Appendix B — Reference-Count Discipline: Discussion and Decision

[Content unchanged from CMS04 Appendix B. Retained verbatim for
maintainer reference.]

### Background

CMS03 introduced `find_output_frame_and_add_ref` and described
`addFrameRef` as "load-bearing while non-checkpoint pinning is
deferred." However, CMS03 did not state the reference-count invariant
as a first-class design goal.

### The Concern Raised

During CMS03 review, the user raised a critical concern:

> "the cache slot is pruned out from under it" and the consequential
> potential risk of having dangling Addrefs and runaway memory
> consumption by VS hanging onto forgotten frame YUV data which no
> longer has slots to remember the refs.

### Analysis

**Reference accounting:** The cache contributes exactly one
`addFrameRef` per slot at store, balanced by exactly one `freeFrame`
at removal. Callers using the lookup helper take additional
`addFrameRef` (one per lookup) balanced by `freeFrame` (one per exit).

**Where leaks could happen:**
1. Failed insert after `addFrameRef` (RC3 prevents).
2. Erase without `freeFrame` (RC2 prevents).
3. Caller forgets `freeFrame` (RC5 + RAII prevent).
4. Shutdown without clear (RC6 prevents).
5. Lingering pinned checkpoint (logical leak, not memory leak).

**Sliding doesn't make this worse.** Sliding exercises remove paths
more, which finds bugs faster.

**Subtlety:** Sliding can cause a caller's lookup to find a frame,
take an owned ref, and then have the cache slot evicted while the
caller is still using its ref. The VSFrame stays alive (caller's ref
keeps it). **No leak.** This is the load-bearing safety property.

### Decision

Promoted the reference-count invariant to explicit first-class goal
(Goal 5) and added Section 2.2 as the named discipline section. Rules
RC1–RC7 (CMS04) and RC8 (CMS05) make the discipline explicit.

### Maintainer Caveats

- Any new code path calling `addFrameRef`/`freeFrame` outside the
  single store/remove helpers is a discipline violation.
- Any `pool.erase()` or `cache_index.erase()` outside the remove
  helper is a violation.
- Ref-balance failure at shutdown is a real bug.
- Use `Cnr3OwnedFrameRef` wrapper for new lookup-helper callers.
- If non-checkpoint pinning is added, same discipline applies to new
  pin/unpin helpers.

---

## Appendix C — Implementation Process Notes and Rationale

**[ADDED CMS05 — collects non-directly-spec rationale, race-condition
narratives, design history, and process guidance from the CMS04
review and prior conversations. Preserved here so future maintainers
have full context without bloating the main spec.]**

### C.1 First-In-Best-Dressed Store Race — Narrative

The first-in-best-dressed store rule (Section 4.9, RC8) closes a
specific race that becomes possible once overlapping recovery walks
exist.

**Concrete scenario:**

In fmParallelRequests, a user makes a forward seek that triggers
recovery for frame 500. Walk A starts, identifies that output[200]
is a hole (cache miss for that frame), and begins computing it from
checkpoint[190].

Concurrently, a second request arrives for frame 250 (still within
the recovery target range or its zone). Walk B starts, also
identifies output[200] as missing, and begins computing it.

Both walks complete their computation of output[200] independently
(they may have used different checkpoints, but the algorithm's
convergence assumption means both results are approximately
correct).

Walk A reaches `store_frame(200, frameA)` first, acquires the mutex,
inserts. Walk B reaches `store_frame(200, frameB)` second, acquires
the mutex, finds output[200] already present.

**Without first-in-best-dressed:**

- Walk B might overwrite frameA with frameB. The cache's
  `addFrameRef` on frameA is now unbalanced (frameA's slot is gone,
  but freeFrame was never called on the cache's frameA ref). FM5 —
  memory leak.
- Or Walk B might take a second `addFrameRef` on the same slot
  (incrementing the slot's bookkeeping incorrectly). The cache
  thinks it has one ref but actually contributes two. On eventual
  eviction, only one is released. FM5 — memory leak.

**With first-in-best-dressed:**

- Walk B's store helper detects the existing entry, returns success
  without taking `addFrameRef` and without disturbing frameA.
- frameB's underlying VSFrame still has Walk B's own ref. Walk B
  releases this normally on its exit path (`freeFrame(frameB)`).
- frameB's ref count reaches zero. VS destroys frameB's pixel data.
- frameA remains in the cache, with its single cache-side ref intact.

**Why the verbose counter name:**

`duplicate_store_computed_but_discarded` is deliberately longer than
strictly necessary. The verbose name makes clear at a glance that
the caller wasted computation even though the cache state remained
correct. If the counter were named just `duplicate_store_avoided`,
a future maintainer reading "duplicate stores were avoided" might
mistakenly conclude that no duplicate work happened. The verbose
name prevents that misreading.

### C.2 RAII Wrapper Instrumentation Strategy

The lookup-owned reference counters
(`lookup_owned_ref_acquired_total`, `_released_total`,
`_transferred_total`) need to be incremented at three points:

- **Acquired:** inside `find_frame_and_add_ref` when it successfully
  takes `addFrameRef`. This is centralised — one counter increment in
  one helper. Easy.

- **Released:** when a caller-owned ref is `freeFrame`d. This could be
  at many call sites.

- **Transferred:** when ownership passes to VS or another owner via
  `release()`. This is less common but also at multiple sites.

**Option 1: Make `Cnr3OwnedFrameRef` clever.** Have the wrapper's
destructor increment `lookup_owned_ref_released_total`, and `release()`
increment `lookup_owned_ref_transferred_total`. This automates
release/transfer counter increments at call sites.

Problem: the wrapper is generic. If it is used for non-lookup-owned
references (e.g., wrapping a computed `new_frame` rather than a
lookup-acquired frame), the destructor would falsely increment
`lookup_owned_ref_released_total`.

To make Option 1 work, the wrapper needs an ownership-origin flag —
e.g., `Cnr3OwnedFrameRef::from_lookup` vs.
`Cnr3OwnedFrameRef::from_computation` — and the destructor/release
methods check the flag before incrementing.

**Option 2: Use separate wrappers.** A `Cnr3LookupOwnedFrameRef`
wrapper increments the lookup counters; a `Cnr3ComputedFrameRef`
wrapper does not. Slightly more code but cleaner semantics.

**Option 3: Explicit call-site instrumentation.** Use one generic
`Cnr3OwnedFrameRef` wrapper that just calls `freeFrame` on
destruction. Counters are incremented manually at the call sites:

```cpp
{
    Cnr3OwnedFrameRef pred(find_frame_and_add_ref(K-1, vsapi), vsapi);
    increment lookup_owned_ref_acquired_total;
       // ^ actually, this is already incremented inside the find helper.

    if (!pred.frame) {
        // cache miss handling
        return error;
    }

    // ... use pred ...

    // On scope exit, wrapper destructor calls freeFrame.
    // Increment release counter manually before scope exit (or just
    // before the explicit release/transfer points).
    increment lookup_owned_ref_released_total;
}
```

**Recommended for first implementation: Option 3 (explicit call-site
instrumentation).**

Reasoning:
- The clever-wrapper options (1 and 2) trade implementation simplicity
  for slightly easier call sites.
- The number of call sites that use `find_frame_and_add_ref` is small
  in the first implementation (mostly in the recovery walks).
- Explicit instrumentation is easier to debug if the caller-side
  invariant fails.
- If later the call-site count grows large, the codebase can be
  refactored to use Option 2 (separate wrappers) without losing
  history.

If Option 1 is chosen, the ownership-origin flag should be passed
explicitly at construction so it cannot be forgotten. No default value.

### C.3 Naming History and Rationale

The original v005 cache manager was developed as a draft alongside the
existing strict-streaming cache (`Cnr3CacheManager`). The "v005"
suffix referred to the fifth design draft in a development series.

Once the design stabilised into the form documented in CMS04, the
"v005" name no longer reflected anything meaningful — it was just a
historical artifact. A future maintainer reading the code should not
need to know about the design draft history to understand what
`Cnr3CacheManagerV005` is.

**The CMS05 rename:**

- `Cnr3CacheManagerV005` → `Cnr3OutputCacheManager` — describes what
  the type manages (the output cache).
- `Cnr3CacheManager` → `Cnr3StrictStreamCache` — describes the policy
  the type implements (strict-streaming).
- Members: `cache` → `strict_cache`, `cache_manager_v005` →
  `output_cache` — descriptive, no version suffixes.

Helper functions (e.g., `cnr3_cache_manager_store_output_frame`) were
already generic in their naming and do not strictly require renaming.
For internal consistency, renaming them to `cnr3_output_cache_*` is
recommended but not blocking.

**Transitional aliasing:**

A `using Cnr3OutputCacheManager = Cnr3CacheManagerV005;` alias may be
used during a multi-patch transition to reduce churn in any single
patch. The final code must remove the alias and the `v005` name
entirely.

### C.4 Why Design-Compliance Review Matters

The highest risk during implementation is not that a newly written
helper is obviously wrong. New code is usually examined carefully by
the implementer; obvious bugs are caught.

The highest risk is that an **older helper remains in the call path**
with assumptions from the previous design — mutex placement, ownership
semantics, pruning behaviour, reference-count handling. The new code
calls the old helper expecting CMS05 semantics, but the old helper
silently provides v005 semantics. The mismatch is invisible because
both pieces "look correct" in isolation.

**Example failure mode this prevents:**

Suppose CMS02-D refactors `prune_non_checkpoint_pool_externally_locked`
to use hot-zone-aware candidate selection. The implementer carefully
writes the new logic. CMS02-E begins runtime use.

But suppose somewhere else in the code base — perhaps in
`cnr3_get_frame` or in an obscure error path — there is a leftover
direct call to `pool.erase()` (bypassing the remove helper). The new
prune logic doesn't touch this code; the old code still works as it
always did. Result: the leftover erase leaks a ref every time it
fires, but only the new code's design-compliance review would catch
it — because the review checks the full execution path, including
unchanged helpers invoked by changed code.

**Checklist items most likely to catch this kind of issue:**

- Item 5 (no direct `pool.erase()`)
- Item 6 (no direct `addFrameRef` outside store helper)
- Item 13 (cache-side ref-balance invariant holds)

The checklist is not a substitute for code review or testing. It is a
**discipline of asking the right questions** at each phase boundary.

### C.5 Phase Timing Examples (Process Detail)

The Section 12 review timing rules can be interpreted as:

**Acceptable batching:**

- Phases CMS02-A and CMS02-B reviewed together (both add only
  structures, counters, passive diagnostics — no runtime behaviour
  change).

**Mandatory single-phase reviews:**

- CMS02-D reviewed alone, before CMS02-E.
  CMS02-E begins runtime use of new store/prune behaviour. The review
  must confirm CMS02-D's new logic is correct before letting it
  affect runtime.
- CMS02-F reviewed alone, before CMS02-G.
  CMS02-G depends on lookup-owned references and rolling predecessor
  correctness from CMS02-F. The review confirms CMS02-F's helpers and
  RAII discipline are sound before CMS02-G builds on them.

**Standard reviews (one per phase):**

- CMS02-C alone (hot zone helpers, no runtime use yet).
- CMS02-E alone (first runtime use of new store/prune).
- CMS02-G alone (recovery walks introduced).
- CMS02-H alone (warm-up recovery introduced).
- CMS02-I alone (empirical decision point — not just code review, but
  decision on whether to promote non-checkpoint pinning).
- CMS02-J alone (fmParallelRequests proving).

### C.6 Guiding Principle (Maintainer Quote)

From the CMS04 review:

> Prefer the safest, clearest, most maintainable implementation. Reuse
> existing code where it remains correct and readable. Rewrite
> individual helpers where old assumptions would make the new design
> hard to reason about.

This is the operating principle behind the design-compliance review.
It is not a directive to rewrite everything; it is a directive to be
honest about which old code remains correct and which old code carries
silent assumptions that have changed.

