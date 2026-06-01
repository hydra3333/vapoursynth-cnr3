# CNR3 Cache Manager — Revised Design Specification CMS04
## Sliding Hot-Zone Pruning with Reference-Count Discipline (Non-Checkpoint Pinning Deferred)

**Date:** 2026-06-01
**Status:** Design specification — ready for coding
**Supersedes:** CMS03 (cnr3_cache_manager_design_v3.md), CMS02, CMS01
**Also supersedes:** Bounded-recovery policy in handover snapshot v0.14 section 0.9A
**Companion documents:** CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)

---

## Change Summary CMS03 → CMS04

Each change is annotated in-place with **[CHANGED CMS04 — reason]** or
**[ADDED CMS04 — reason]**. Appendices A and B record the discussion and
rationale behind the two main decisions for future maintainers. The complete list:

1. **Major change — sliding everywhere.** CMS03 used sliding zones in
   fmUnordered but extend-only zones in fmParallelRequests. The
   mode-specific split was an over-correction to the CMS02 contradiction.
   Sliding is safe in both modes because of the `addFrameRef` discipline
   (Section 4.7) and the rolling predecessor reference pattern (Section
   4.5.1). The only mode-specific behaviour is retirement: eager in
   fmUnordered, lazy in fmParallelRequests. See Section 4.2 and **Appendix
   A** for the full discussion.

2. **Reference-Count Discipline made an explicit first-class goal.** CMS03
   implicitly assumed ref-count correctness but did not state it as a
   top-level invariant. CMS04 adds Goal 5 (Reference-Count Invariant) and
   a new Section 2.2 (Reference-Count Discipline) spelling out the rules.
   See **Appendix B** for the discussion.

3. **Rolling predecessor reference pattern specified.** Walks must
   maintain an owned reference to their current predecessor and transition
   ownership to the newly computed frame before releasing the previous
   one. This is the load-bearing implementation pattern that makes
   `addFrameRef`-only protection sufficient. See Section 4.5.1.

4. **Reference-count diagnostic counters added.** `cache_addframeref_total`
   and `cache_freeframe_total` are tracked per cache instance, with a
   `validate_invariants` check that confirms balance at quiescence and
   shutdown. See Section 6.

5. **RAII wrapper recommended for caller-owned references.** Reduces the
   risk of exit-path leaks at call sites. See Section 2.2.

6. **Shutdown protocol made explicit.** The cache manager's destruction
   must iterate all slots, `freeFrame` each, and log a warning for any
   slot with `pin_count > 0`. See Section 9.5.

7. **Example A overflow arithmetic fixed.** With `CAPACITY = 100` and
   `OVERFLOW_FACTOR = 1.1`, the overflow threshold is 110, and prune fires
   at 111 frames (using strict `>` semantics). CMS03 incorrectly stated
   121. See Section 7 Example A.

8. **`active_ceiling` semantics clarified.** A store is allowed if
   `total_live_refs_after_store <= active_ceiling`; rejected if
   `> active_ceiling`. Helper name `would_exceed_ceiling` aligns with
   this. See Section 4.6.2.

9. **Ceil-style subsampled plane dimensions.** Byte-budget ceiling
   computation now uses
   `(full_size + ((1 << shift) - 1)) >> shift` for chroma plane
   dimensions, ensuring no underestimation on odd dimensions. See Section
   4.6.1.

10. **Grey/4:0:0 wording removed.** The handover specifies YUV with three
    planes. CMS04 no longer implies grey support. See Section 4.6.1.

11. **Scheduling mode enum specified.** A `Cnr3CacheSchedulingMode` enum
    replaces an ad-hoc int/string parameter, and is used only by the
    retirement helper. Hard-wired to `FmUnordered` for the first
    implementation. See Section 9.

12. **`find_output_frame_and_add_ref` defensive contract.** Helper must
    verify the cache_index entry, pool consistency, and non-null frame
    pointer before taking `addFrameRef`, and must increment counters. See
    Section 9.2.H.

13. **Appendix A added.** Records the sliding vs. extend-only discussion
    and decision rationale.

14. **Appendix B added.** Records the reference-count discipline
    discussion and decision rationale.

---

## 1. Problem Statement

CNR3 is a recursive temporal chroma stabiliser. Every output frame depends
on the previous output frame as a predecessor. The cache manager must
retain enough predecessor frames to satisfy any in-flight computation,
prune old frames to keep memory bounded, and maintain strict VSFrame
reference-count discipline so that no frame is ever leaked or freed
prematurely.

The existing v005 cache manager infrastructure (pools, checkpoint pinning,
mutex model, store/remove/validate helpers) is sound. What is not yet
designed is the pruning policy and the structural protections needed to
make that policy safe under three VapourSynth execution modes:

- **fmUnordered** — one request in flight at a time, mostly sequential
- **fmParallelRequests** — multiple concurrent requests, serialised writer
- **fmParallel** — fully concurrent readers and writers (out of scope for
  this iteration)

The primary failure modes without the new design are:

**FM1 — Prune destroys in-flight predecessor.** A walk is computing
output[450] using output[449] as predecessor. Prune evicts output[449]
because a forward jump has pushed new frames into the cache.

**FM2 — Prune destroys a checkpoint needed by a recovery chain.** A
recovery walk pins checkpoint[400] and fills frames 401..500. A concurrent
prune evicts checkpoint[380] which another concurrent request needed as
its anchor.

**FM3 — Jump recovery burst exceeds pool capacity.** A forward jump
triggers bounded warm-up recovery generating up to CAPACITY new frames.
Simultaneously, pre-jump in-flight requests hold frames in the pool. The
combined total exceeds the overflow limit; stores fail.

**FM4 — Prune eviction key is wrong.** Current pruning evicts the lowest
frame number first. After a forward jump, the lowest frame numbers are
exactly the pre-jump in-flight frames most critically needed.

**FM5 — VSFrame reference leak.** A code path takes `addFrameRef` (e.g.
via store or lookup) but does not execute a matching `freeFrame` on every
exit. Over time, the process accumulates VapourSynth frame buffers that
the cache no longer tracks, causing runaway memory consumption.

**FM6 — VSFrame use-after-free.** A code path uses a `VSFrame*` after its
reference count has dropped to zero (e.g., a raw borrowed pointer
returned by a lookup helper while another thread evicts the slot).

---

## 2. Design Goals

**[CHANGED CMS04 — added Goal 5 (Reference-Count Invariant). Goal 1
wording finalised from CMS03 reword.]**

1. **Prevent pruning of frames likely to be needed by active computation
   using hot-zone protection.** If diagnostics show that hot-zone
   protection is insufficient, promote non-checkpoint pinning to a
   mandatory implementation step (Section 4.4).

2. Make pruning decisions based on frame-number proximity to active work,
   not on insertion order or frame-number magnitude.

3. Support up to `CNR3_MAX_HOT_ZONES` simultaneous active working ranges,
   covering concurrent pre-jump and post-jump activity in
   fmParallelRequests.

4. Fill holes only — never recompute a frame that is already cached
   (Section 2.1).

5. **Reference-Count Invariant.** For every VSFrame held by the cache,
   the cache contributes exactly one `addFrameRef` while it holds the
   slot, balanced by exactly one `freeFrame` when the slot is removed.
   No VSFrame is ever leaked. No VSFrame is ever freed while still in
   use outside the cache mutex (Section 2.2, Section 4.7).

6. Bound memory use with a hard ceiling computed from actual frame
   geometry and a configurable byte budget. Tune after empirical runtime
   memory data (Section 4.6).

7. Hard-abort cleanly when the ceiling is hit with nothing prunable. The
   filter must remain in a valid state after the abort, with no leaked
   references or stuck pinned checkpoints (Section 4.6, Section 9.5).

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

**Reference-ownership rule:**

When output[K] is found in cache and used as the predecessor for
output[K+1] *outside* the cache mutex, the lookup helper takes
`addFrameRef()` on the found frame *while still holding the cache mutex*
(making the find-and-ref atomic), then returns that caller-owned
reference. The caller must `freeFrame()` it on every exit path —
success, error, early return. This is implemented by
`cnr3_cache_manager_find_output_frame_and_add_ref()` (Section 9.2.H).
Callers never see a raw borrowed `VSFrame*` after the mutex is released.

Hot-zone protection and `addFrameRef` are independent mechanisms with
different roles (Section 4.7).

### 2.2 Reference-Count Discipline

**[ADDED CMS04 per the discussion in Appendix B. Promoted to a named
discipline section because the addFrameRef regime is now load-bearing
while non-checkpoint pinning is deferred, and ref-count correctness must
be explicit.]**

VapourSynth manages frame memory by reference counting. Every VSFrame has
an integer reference count. `addFrameRef` increments it; `freeFrame`
decrements it. When the count reaches zero, the frame's pixel data is
released.

The cache manager interacts with this reference count whenever it stores,
looks up, removes, or destroys a slot. The Reference-Count Invariant
(Goal 5) requires that all such interactions remain balanced — no orphan
references, no premature releases.

**Hard rules:**

**RC1 — Single store helper.** All `addFrameRef` calls made by the cache
for storage occur in `cnr3_cache_manager_store_output_frame()`. No other
code path takes a cache-owned reference.

**RC2 — Single remove helper.** All `freeFrame` calls made by the cache
on cache-owned references occur in
`cnr3_cache_manager_remove_output_frame_externally_locked()` (and its
internal callers within prune). No code path may `pool.erase()` or
`cache_index.erase()` directly without going through the remove helper.

**RC3 — Store error paths must rebalance.** If a path inside the store
helper takes `addFrameRef` and then fails to complete insertion (e.g.,
validation rejection, allocation failure), it must execute a matching
`freeFrame` before returning. The store helper is the single point of
truth for this rebalancing — callers do not retry-balance.

**RC4 — Lookup error paths must rebalance.** If
`find_output_frame_and_add_ref` takes `addFrameRef` and then fails before
returning to the caller (e.g., validation check rejects the result), it
must execute a matching `freeFrame` before returning nullptr.

**RC5 — Caller exit paths must `freeFrame`.** Every code path that
receives a caller-owned reference from a lookup helper must `freeFrame`
on every exit path: success, error, early return, exception, hard abort.
Forgetting one exit path leaks one reference per invocation.

**RC6 — Shutdown must clear.** Destruction of `Cnr3CacheManagerV005` (or
its explicit `clear()` called before destruction) must iterate all slots
in both pools, `freeFrame` each, and log a warning for any slot with
`pin_count > 0` (logical leak, indicates a missed unpin). See Section
9.5.

**RC7 — Validation enforces balance.** `validate_invariants` includes a
ref-balance check: at quiescence, `cache_addframeref_total -
cache_freeframe_total == total_live_slots_across_both_pools`. The
counters are updated under the cache mutex at each store/remove.

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
    // Release ownership (e.g., when transferring to next iteration)
    const VSFrame* release() noexcept { auto f = frame; frame = nullptr; return f; }
};
```

Use is straightforward:

```cpp
Cnr3OwnedFrameRef pred(
    cnr3_cache_manager_find_output_frame_and_add_ref(cache, K-1, vsapi),
    vsapi
);
if (!pred.frame) { /* cache miss path */ }
// ... use pred.frame ... ref is released automatically at scope exit
```

The wrapper is a recommendation, not a hard requirement. Direct
`addFrameRef`/`freeFrame` is acceptable if RC5 discipline is followed.

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

The p99 max gap tracks approximately `jitter_max + 3`. For observed
BestSource jitter of 4–6 frames, the worst-case reorder window is
approximately 9 frames at p99. `CNR3_HOT_ZONE_FORWARD_RADIUS = 10`
covers this with headroom.

The existing `CNR3_REORDER_WINDOW = 32` is adequate for jitter up to
approximately `jitter_max=24` at p99.

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
- A **last_observed_frame** — the most recently arrived frame number
  that fell within or caused this zone
- Diagnostic counters: `hit_count`, `slide_count`, `merge_count`,
  `retirement_count`, `prune_protection_count`

Discrete hot zones (rather than a single watermark or rolling median)
allow multiple simultaneous active ranges to be represented
independently. A jump produces two zones that coexist until the old
zone goes cold; sliding handles the steady-state working position
within each zone.

### 4.2 Hot Zone Lifecycle — Sliding (Both Modes)

**[MAJOR CHANGE CMS04 — CMS03 had mode-specific sliding (fmUnordered)
vs. extend-only (fmParallelRequests). CMS04 uses sliding in both modes.
The only mode-specific behaviour is retirement. See Appendix A for the
full discussion and rationale.]**

#### 4.2.1 Sliding rule (both modes)

For an arriving frame `F`:

1. **Find a candidate zone.** Scan active zones. Find the nearest active
   zone Z such that `F` is within `CNR3_HOT_ZONE_JUMP_THRESHOLD` of Z's
   range:
   ```
   Z.low - JUMP_THRESHOLD ≤ F ≤ Z.high + JUMP_THRESHOLD
   ```
   If multiple zones satisfy this, pick the one with the smallest
   absolute distance from F to the nearest boundary.

2. **Slide the candidate zone.** If such a Z is found, recompute its
   bounds deterministically:
   ```
   Z.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   Z.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   Z.last_observed_frame = F
   ```
   Increment `slide_count` if the bounds actually moved, or `hit_count`
   if they did not. The semantics are the same in either case — the zone
   ends up centred on F.

3. **Allocate a new zone.** If no Z is within threshold, this is a jump
   event. See 4.2.3.

#### 4.2.2 Why sliding is safe in both modes

The safety argument relies on two mechanisms:

- **`addFrameRef` discipline (Section 4.7).** A walk that has already
  looked up a predecessor holds a caller-owned reference. Even if a
  slide moves the zone past that predecessor's frame number and a
  subsequent prune evicts the cache slot, the walk's owned reference
  keeps the underlying VSFrame alive until the walk releases it.

- **Rolling predecessor reference pattern (Section 4.5.1).** Walks hold
  an owned reference to their current predecessor and transition
  ownership across iterations. The predecessor reference is never
  released until after the next predecessor (the newly computed and
  stored frame) is in hand.

Under normal sequential linear encoding in fmParallelRequests, the
working spread of concurrent requests is much smaller than
`BACK_RADIUS=50`. With 16–32 threads and jitter ~6, the spread is
~22–38 frames. The slid zone always covers all in-flight predecessor
needs.

The pathological case — a walk stalls while concurrent walks slide the
zone forward — produces extra recovery work (the stalled walk's
predecessor lookup misses; it falls back to checkpoint recovery), not
incorrect output. The `predecessor_missing_when_expected` diagnostic
counter detects this empirically and triggers promotion to
non-checkpoint pinning if it occurs.

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
3. If no free slot exists:
   - Attempt retirement of a cold zone (4.2.4).
   - If no zone is eligible for retirement, merge the two closest zones
     (4.2.5).

#### 4.2.4 Retirement (mode-specific)

**[MODE-SPECIFIC BEHAVIOUR — the only mode-dependent part of the
lifecycle.]**

**fmUnordered (eager retirement):**

When a new `arInitial` fires in fmUnordered, the previous
`arAllFramesReady` (or `arError`) has completed. Therefore, any zone
whose `last_observed_frame` is not the current `F` represents work
that is no longer in flight. Before allocating a new zone, all such
"stale" zones can be retired.

In practice, fmUnordered typically operates with **at most one active
zone**, which slides forward in normal operation and is replaced
(retired + new allocation) on a jump.

**fmParallelRequests (lazy retirement):**

Multiple requests may be in flight simultaneously. A zone cannot be
retired purely because its `last_observed_frame` is stale, because
in-flight requests may still be using frames within its range.

A zone is eligible for lazy retirement only when:

- No live frames remain in either pool within its `[low, high]` range,
  AND
- No pinned checkpoint exists within its range (conservative proxy for
  "no recovery walk is in progress within this zone").

Retirement is attempted only when a new zone allocation needs a free
slot and none is available. Spurious retention is safe; the cost is a
modest increase in zone-slot pressure.

If the lazy proxy proves too conservative in practice, an
`active_request_count` per zone (incremented at `arInitial`,
decremented at `arAllFramesReady`/`arError`) can be added for exact
retirement eligibility.

**fmParallel:** out of scope for this iteration.

#### 4.2.5 Zone merge

When all slots are full and no zone is eligible for retirement:

1. Find the two zones whose boundaries are closest (smallest gap
   between one zone's `high` and another zone's `low`).
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

Merging is conservative — the merged zone protects everything either
original protected. No frames lose protection. Log the merge action
for diagnostics.

### 4.3 Pruning Policy — Hot Zone Aware (Phase-Guarded)

#### 4.3.1 Non-Checkpoint Pool Pruning

**Phase A — before non-checkpoint pinning exists (Phase CMS02-D
through CMS02-H):**

1. Call `retire_cold_hot_zones_externally_locked` (lazy retirement
   pass, mode-aware).
2. Collect non-checkpoint frames whose frame number falls outside every
   active hot zone's `[low, high]` range. These are eviction candidates.
3. Among candidates, find the one with the greatest minimum distance
   from any active hot zone boundary. Evict it (via the single remove
   helper, which executes `freeFrame` per RC2).
4. Repeat step 3 until
   `non_checkpoint_pool.size() <= CNR3_OUTPUT_CACHE_CAPACITY` or no
   candidates remain.
5. If no candidates remain and the pool exceeds the overflow limit, do
   not evict further. The pool may temporarily exceed the soft target,
   up to the hard ceiling.

**Phase B — after non-checkpoint pinning is added (Phase CMS02-I or
later, only if promotion criteria are met):**

Identical to Phase A except step 2 also requires `pin_count == 0`. The
diagnostic counter `non_checkpoint_prune_skipped_pinned` is then
reserved for use.

For typical pool sizes (100–300 frames), a linear scan to find the
maximum-distance candidate on each iteration is acceptable. No formal
sort structure is required.

#### 4.3.2 Checkpoint Pool Pruning

Apply hot-zone-aware filtering with the existing `pin_count` check:

1. A checkpoint is a candidate if:
   - Frame number is not zero (frame 0 is never pruned), AND
   - `pin_count == 0`, AND
   - Frame number falls outside every active hot zone's `[low, high]`.
2. Evict the candidate with the greatest distance from any hot zone
   boundary first (via the single remove helper).
3. Continue until
   `checkpoint_pool.size() <= CNR3_CHECKPOINT_MIN_RETAIN` or no
   candidates remain.

Checkpoints within hot zones or pinned are retained regardless of
`CNR3_CHECKPOINT_MAX_RETAIN`. The retain limits are soft triggers, not
hard caps.

### 4.4 Non-Checkpoint Frame Pinning — Deferred

Non-checkpoint pinning remains the deterministic solution to FM1 if
hot-zone-aware pruning is shown to be insufficient. The first
implementation defers non-checkpoint pinning and instead relies on
conservative hot zones, fill-holes-only recovery, the reference-ownership
rule (Section 2.1), the rolling predecessor reference pattern (Section
4.5.1), and detailed diagnostics.

**Mandatory promotion criterion:**

Non-checkpoint pinning becomes mandatory if any of the following are
observed during realistic test runs:

- The diagnostic counter `predecessor_missing_when_expected` is non-zero
  during any realistic VHS/VHS-C encode test.
- Recovery repeatedly recomputes frames that were recently cached.
- fmParallelRequests testing reveals a race between prune and active
  predecessor use that hot zones and `addFrameRef` do not cover.
- Hot-zone settings required to prevent the above become so broad that
  they defeat pruning (e.g., back radius must exceed 200 to be safe).

**Structural change if added later:**

Change `non_checkpoint_pool` from `std::map<int, const VSFrame*>` to
`std::map<int, Cnr3NonCheckpointSlot>` where
`Cnr3NonCheckpointSlot = { const VSFrame* frame; int pin_count = 0; }`.
Mirror the existing checkpoint pin/unpin pattern.

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

**[ADDED CMS04 — this is the load-bearing implementation pattern that
makes the deferred-pinning regime safe under concurrent pruning.
Specified here so implementers cannot accidentally introduce a
"release-then-lookup" gap that exposes the walk to FM1.]**

A recovery walk maintains an owned reference to the current
predecessor through each iteration. The reference is transitioned (not
released-then-reacquired) when moving from iteration K to K+1.

```text
prev_ref = nullptr  // walk-owned reference to current predecessor

if start == 0 or start has a known cache hit:
    prev_ref = find_output_frame_and_add_ref(start)
       // or initialise from frame 0 if cache miss

else:
    // bounded warmup start with no predecessor available
    new_frame = initialise_from_source(start)  // source-copy semantics
    store_output_frame(start, new_frame)
       // cache takes its own addFrameRef
    prev_ref = new_frame
       // walk retains its original computed ref - no addFrameRef needed,
       // no freeFrame yet

for K in start+1 .. N:
    cached = find_output_frame_and_add_ref(K)
    if cached != nullptr:
        // Fill-holes-only: cache hit, skip computation
        freeFrame(prev_ref)
        prev_ref = cached
        continue

    // Cache miss: compute output[K] from prev_ref
    new_frame = compute_blend(prev_ref, source[K])
       // new_frame has its own ref (count=1, owned by walk)
    store_output_frame(K, new_frame)
       // cache takes an independent addFrameRef on new_frame
    freeFrame(prev_ref)
       // release our old predecessor; cache may or may not still own it
    prev_ref = new_frame
       // walk now owns the only walk-side ref to output[K]

// End of loop:
if prev_ref != nullptr:
    freeFrame(prev_ref)
```

**Key properties:**

- At every point during the walk, the walk holds exactly one owned
  reference (`prev_ref`) to the current predecessor.
- The transition from K to K+1 *first* acquires the new predecessor
  ref, *then* releases the old one. There is no window in which the
  walk holds zero references to its current predecessor.
- If a concurrent prune evicts the cache slot for `prev_ref` between
  iterations, the walk's owned reference keeps the underlying VSFrame
  alive until the walk releases it.
- The pattern works identically in fmUnordered (where no concurrent
  prune can fire mid-walk) and fmParallelRequests (where it can).

**On error paths:** if any step inside the loop fails (`store_output_frame`
returns false at the ceiling, blend computation fails, etc.), the walk
must `freeFrame(prev_ref)` and `freeFrame(new_frame)` (if held) before
returning. The Cnr3OwnedFrameRef RAII wrapper from Section 2.2 handles
this automatically at scope exit.

### 4.6 Hard Ceiling and Abort Policy — Byte-Budget Based

#### 4.6.1 Ceiling Calculation

**[CHANGED CMS04 per review item 3 — ceil-style chroma plane dimension
arithmetic; per review item 4 — grey/4:0:0 mention removed.]**

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
```

The ceil-style chroma dimension formula ensures the byte estimate is
conservative on odd dimensions. This formula works for accepted planar
YUV formats such as 4:2:0, 4:2:2, 4:4:0, and 4:4:4. CNR3 accepts only
three-plane YUV per the handover specification; grey/4:0:0 is not in
scope for this iteration.

```cpp
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

#### 4.6.2 Abort Policy

**[CLARIFIED CMS04 per review item 2 — exceed semantics.]**

A store is allowed if `total_live_refs_after_store <= active_ceiling`.
A store is rejected if it would cause
`total_live_refs_after_store > active_ceiling` AND prune cannot free
any frame (everything within hot zones or pinned).

When rejected:

1. The store returns `false` without taking `addFrameRef` (RC3 — no
   imbalance introduced).
2. Increment `cache_ceiling_hard_aborts`.
3. The calling `getFrame` path executes cleanup discipline (4.6.3)
   then returns a VapourSynth filter error for that frame:

   *"CNR3: cache ceiling reached ([N] frames). CNR3 is designed for
   near-linear access. Large random seeks in rapid succession may
   exceed cache capacity. Reduce seek frequency or use a near-linear
   workflow."*

The helper `cnr3_cache_manager_would_exceed_ceiling_externally_locked()`
returns true iff a subsequent store would cause
`total_live_refs_after_store > active_ceiling`.

#### 4.6.3 Cleanup Discipline on Ceiling Abort

A ceiling abort may leave already-successfully-stored frames in the
cache (those frames are valid outputs). However, the failure path must
not leave any temporary runtime state unreleased:

- Any checkpoint pinned during the failed recovery: unpin exactly once.
- Any caller-owned `VSFrame*` reference held by the recovery walk
  (including the rolling `prev_ref`): `freeFrame` exactly once.
- Any source frame obtained during the recovery walk: release.
- Any destination frame allocated for the failed frame: release if not
  returned to VapourSynth.
- Hot zone state: no rollback. A zone allocated for the failed request
  is not invalid — it will be retired naturally when its range becomes
  cold.

After cleanup, the filter remains in a valid state. Reference counters
(`cache_addframeref_total`, `cache_freeframe_total`) remain balanced.

### 4.7 addFrameRef and Pinning — Separate Concerns

The cache manager has three distinct protection mechanisms with
different roles:

| Mechanism | Protects against | Holder | Lifetime |
|---|---|---|---|
| **Hot zone membership** | Slot eviction by prune | Cache manager | Until zone slides/retires past frame |
| **Checkpoint `pin_count`** | Slot eviction by prune (checkpoints) | Recovery walks | Pin/unpin pair on every path |
| **`addFrameRef`** | Underlying VSFrame being freed | Caller of lookup helper | Until `freeFrame` by caller |

Hot-zone protection and `pin_count` are both *prune-side* mechanisms.
They prevent the cache from evicting the slot. `addFrameRef` is
independent — it ensures the VSFrame's pixel data remains valid for
the holder even if the cache decides to evict.

The reviewer's framing is the cleanest summary:

> `addFrameRef` protects a frame *already found*. It does not make a
> missing predecessor appear.

This means `addFrameRef` cannot rescue a walk whose predecessor was
pruned *before* the walk's lookup. The rolling predecessor reference
pattern (Section 4.5.1) addresses this by ensuring the walk always
holds a ref to its current predecessor *before* it would be needed.

### 4.8 arInitial vs. Cache-Hit Hot Zone Update

Hot zone updates fire at `arInitial` for every arriving frame request,
regardless of whether the request later results in a cache hit or
computation. This is deliberate: a cache hit still represents activity
near that frame number, and the hot zone should reflect the request
stream's working position.

**Diagnostic counters distinguish these cases:**

- `hot_zone_updates_at_arInitial` — total `arInitial` calls that
  updated a hot zone (slide, hit, or new allocation).
- `cache_hits_at_arAllFramesReady` — requests served from cache
  without computation.
- `recoveries_started_at_arAllFramesReady` — requests requiring a
  recovery walk.

---

## 5. Constants

```
// --- Soft pruning targets ---

CNR3_OUTPUT_CACHE_CAPACITY        = 100
    Soft pruning target.

CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Prune triggers when non_checkpoint_pool > capacity * factor (= 110).
    Strict ">" semantics: prune fires when pool size is 111+.

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
    Supported by simulation: covers p99 BestSource jitter to ~jitter_max=8.

CNR3_HOT_ZONE_BACK_RADIUS         = 50
    Covers 5 checkpoint intervals of backward history.
    Subject to empirical tuning after Phase CMS02-E.

CNR3_MAX_HOT_ZONES                = 5

CNR3_HOT_ZONE_JUMP_THRESHOLD      = FORWARD_RADIUS + BACK_RADIUS + 1  (= 61)
```

**Removed constants** (relative to CMS02): `CNR3_OUTPUT_CACHE_CEILING_8BIT`,
`CNR3_OUTPUT_CACHE_CEILING_16BIT` (replaced by byte-budget computation),
`CNR3_HOT_ZONE_EXTENSION_MARGIN` (no longer needed under sliding).

---

## 6. Diagnostics — Definitive Counter Specification

All counters are `int64_t`. Placement (in `Cnr3CacheManagerStats` or a
new struct) is an implementation choice at coding time.

**Hot zone counters:**

- `hot_zone_allocations`
- `hot_zone_slides` (bounds actually moved on update)
- `hot_zone_hits` (bounds already covered F; no movement needed)
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

**Reference-count counters (ADDED CMS04 per Section 2.2 RC7):**

- `cache_addframeref_total` (cumulative, incremented inside store and any
  other cache-side `addFrameRef` site)
- `cache_freeframe_total` (cumulative, incremented inside remove and any
  other cache-side `freeFrame` site)
- `cache_addframeref_lookup_total` (cumulative `addFrameRef` taken by
  `find_output_frame_and_add_ref`; not counted in the cache-side balance
  check — these are caller-owned)
- Invariant check at quiescence and shutdown:
  `cache_addframeref_total - cache_freeframe_total ==
   non_checkpoint_pool.size() + checkpoint_pool.size()`

**Debug output policy:**

- Default: headline counters at `cnr3_free`.
- Dev diagnostics enabled: all counters plus hot zone state.
- Per-event verbose output guarded behind
  `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`.
- **No diagnostic output to stdout under any circumstances.**
- `predecessor_missing_when_expected > 0` prints a prominent warning
  regardless of diagnostic level.
- Ref-balance failure at shutdown prints a prominent warning regardless
  of diagnostic level.

---

## 7. Worked Examples

### Example A — Linear Encoding, No Jumps (sliding)

Setup: 32-thread encode, BestSource jitter up to 6 frames. Cache starts
empty.

| Arrival | Action | Zone 0 after action | Pool size |
|---|---|---|---|
| F=0   | Allocate zone 0 | low=0, high=10 | 1 |
| F=10  | Inside zone, slide | low=0, high=20 | ~10 |
| F=50  | Inside zone, slide | low=0, high=60 | ~50 |
| F=60  | Slide | low=10, high=70 (low starts moving) | ~60 |
| F=100 | Slide | low=50, high=110 | ~100 |
| F=110 | Slide | low=60, high=120 | ~110 |
| F=111 | Slide | low=61, high=121; **prune fires** (111 > 110) | back to ~100 |

**[CORRECTED CMS04 per review item 1 — pool size hits overflow
threshold at 111, not 121. 100 × 1.1 = 110, and strict ">" means prune
fires at 111.]**

At F=111, prune candidates are frames outside `[61, 121]`. Frames 0–60
are candidates. Evict furthest-from-zone first (F=0, then F=1, …) until
pool returns to ≤ 100. Each eviction goes through the single remove
helper (RC2), which `freeFrame`s the cache-owned ref.

Predecessor for current computation is always within zone (F=111 needs
output[110], well inside `[61, 121]`). No predecessor failures.

### Example B — Small Forward Jump Within JUMP_THRESHOLD

Setup: at F=80, zone 0 = `low=30, high=90`.

F=95 arrives. Distance from zone high (90): 5 frames. 5 ≤ 61. Within
threshold. Slide zone 0: `low = max(0, 95-50) = 45`, `high = 95+10 = 105`.
Zone now centred on 95.

Cache lookup: output[94] may be present (cache hit) or absent (cache
miss). On hit, the walk uses it directly via `find_output_frame_and_add_ref`.
On miss, nearest checkpoint (say checkpoint[90]) is found and pinned,
the rolling-predecessor walk fills 91..95, checkpoint is unpinned.

### Example C — Large Forward Jump (fmParallelRequests, sliding)

**[REVISED CMS04 for sliding-everywhere and the rolling predecessor
pattern.]**

Setup: sequential encoding at F=200 with three in-flight walks (200,
201, 202). Zone 0 was last slid to F=202: `low=152, high=212`.

F=600 arrives. Distance from zone 0 high (212): 388 frames. 388 > 61.
Jump detected.

Allocate zone 1: `low=550, high=610`.

Zone 0 remains active. It still represents the working range of the
in-flight walks for 200/201/202. Their predecessors (199, 200, 201) are
within `[152, 212]`. Each walk holds a rolling `prev_ref` for its
current predecessor (Section 4.5.1), so even if a concurrent prune
fires, the walks' owned references keep their predecessors alive.

Bounded recovery for F=600: no checkpoint above 200 exists.
`start = max(0, 600-100) = 500`. The walk follows the rolling
predecessor pattern: source-copy at 500, blend 501..600 using each prior
output as predecessor.

Frames 500–549 are stored but outside zone 0 and outside zone 1.
They are prunable. Pruning may fire after stores in this range. The
walk has either:

- Already used each as predecessor and transitioned `prev_ref` to the
  next frame (no longer needed), or
- Holds the current predecessor in `prev_ref` (kept alive regardless of
  prune).

Pre-jump walks for 200/201/202 complete. Zone 0 has no live frames in
[152, 212] (pruned as part of normal prune cycles, since they're now
outside both zones). On the next zone-slot demand, zone 0 is lazy-
retired via the pinned-checkpoint proxy (no pinned checkpoints remain in
its range).

Steady state: one active zone around F=600. Cache holds ~100 frames in
the 550–610 range plus retained checkpoints.

### Example D — No-Prior-Checkpoint Recovery (cold seek)

Setup: fresh instance, user seeks to F=800. Cache empty. No
checkpoints.

F=800 arrives. No active zones. Allocate zone 0: `low=750, high=810`.

`find_and_pin_nearest_prior_checkpoint` returns false. Increment
`no_prior_checkpoint_recovery_count`.

Bounded warm-up using the rolling predecessor pattern:

- `start = 700`. Source-copy initialise output[700]. `prev_ref` =
  the newly computed output[700].
- For K = 701..800:
  - Look up output[K] in cache (`find_output_frame_and_add_ref`).
    Miss expected here on first walk.
  - Compute `new_frame = blend(prev_ref, source[K])`.
  - Store new_frame (cache `addFrameRef`).
  - `freeFrame(prev_ref)`. Set `prev_ref = new_frame`.

Checkpoints promoted at 700, 710, …, 800.

After recovery: `freeFrame(prev_ref)` at end of loop. Prune candidates
are frames 700..749 (below zone low=750) with `pin_count == 0`. Evicted
furthest-first. Pool settles to ~50 frames. Checkpoints retained per
checkpoint rules.

### Example E — Hard Ceiling Abort

Setup: 16-bit 4:2:2 input. `estimated_frame_bytes ≈ 1.66 MB`.
`active_ceiling = floor(512 MiB / 1.66 MB) ≈ 323`, within `[150, 1000]`.

User makes 5 rapid large seeks before any recovery completes. Five
concurrent recovery walks each generating ~101 frames = ~505 total
demand. Ceiling = 323.

After ~323 stores, the next store would cause
`total_live_refs_after_store > 323`. Ceiling check fires. Prune: no
candidates (all frames within one of the 5 active hot zones).

Store returns false without taking `addFrameRef` (RC3 — no imbalance).
Increment `cache_ceiling_hard_aborts`. Cleanup discipline (Section
4.6.3): the walk `freeFrame`s its `prev_ref` and any other held
references, unpins any pinned checkpoint, releases any source/dst
frames.

`getFrame` returns the VS ceiling error. Filter remains valid.
Reference counters remain balanced.

### Example F — Fill-Holes-Only Avoiding Redundant Compute

Setup: output[150] was computed and stored by a previous walk. A new
walk reaches K=150 during a recovery from checkpoint[145] toward
target N=155.

At K=150 the walk calls `find_output_frame_and_add_ref(150)`. Cache hit.
Increment `recovery_frames_skipped_already_cached`. The walk
transitions: `freeFrame(prev_ref)` (which held output[149]), then
`prev_ref = cached_150`. Skip computation. Continue loop.

At K=151, the walk uses output[150] (held in `prev_ref`) as predecessor.
Computes, stores, transitions.

Result: one frame of redundant computation avoided. Reference counts
remain balanced throughout.

---

## 8. Phased Implementation Sequence

#### Phase CMS02-A — Documentation and constants only
- Mark CMS04 as the current Phase 4 cache-policy direction.
- Add all new constants (Section 5) to `cnr3_cache_manager.h`.
- Add all new diagnostic counter declarations (Section 6, including
  the new ref-count counters).
- Add `active_ceiling` field to `Cnr3CacheManagerV005`.
- Implement byte-budget ceiling computation in
  `cnr3_cache_manager_set_ceiling()` using the ceil-style chroma
  formula.
- Add `Cnr3HotZone` struct declaration.
- Add `Cnr3CacheSchedulingMode` enum.
- Add `Cnr3OwnedFrameRef` RAII wrapper (recommended; optional adoption
  in later phases).
- Do not change runtime behaviour.

#### Phase CMS02-B — Hot zone structures and passive diagnostics
- Add hot zone array to `Cnr3CacheManagerV005`.
- Add hot zone helper declarations.
- Add passive debug snapshot fields for hot zone state.
- Add `cache_addframeref_total`, `cache_freeframe_total` instrumentation
  to existing store and remove helpers (passive — no new behaviour, just
  counting).
- Extend `validate_invariants` with the ref-balance check (RC7).
- Do not use hot zones for pruning yet.

#### Phase CMS02-C — Hot zone update helpers
- Implement `cnr3_cache_manager_update_hot_zones()` using sliding rule
  (Section 4.2.1), with new-zone allocation and merge (Section 4.2.3,
  4.2.5).
- Implement `cnr3_cache_manager_is_frame_in_hot_zone_externally_locked()`.
- Implement `cnr3_cache_manager_retire_cold_hot_zones_externally_locked()`
  with mode-aware retirement (Section 4.2.4). Hard-wire
  `FmUnordered` for first implementation.
- Instrument all zone lifecycle events.
- Do not use hot zones in pruning yet.

#### Phase CMS02-D — Hot zone aware prune candidate selection
- Replace `prune_non_checkpoint_pool_externally_locked` inner loop with
  hot-zone-aware candidate selection (Section 4.3.1 Phase A).
- Replace `prune_checkpoint_pool_externally_locked` similarly
  (Section 4.3.2).
- Add `cnr3_cache_manager_would_exceed_ceiling_externally_locked()`.
- Add ceiling check and hard abort to `store_output_frame`, including
  cleanup discipline (Section 4.6.3).
- Instrument all prune decisions.

#### Phase CMS02-E — Store/prune-only runtime proving
- In the existing strict-streaming output path, store produced frames
  into `cache_manager_v005` after the existing path completes.
- Call `prune_after_store`.
- Do not yet use v005 frames for output generation.
- Enable memory diagnostics.
- Prove: `addFrameRef`/`freeFrame` balance at end of every operation,
  ref-balance counter check at shutdown, pool sizes, prune behaviour,
  hot zone allocation/retirement, no stdout output, ceiling counters
  zero during normal linear encode.

#### Phase CMS02-F — Cache-hit reuse under fmUnordered
- Implement `cnr3_cache_manager_find_output_frame_and_add_ref()` with
  the defensive contract (Section 9.2.H).
- At the start of `cnr3_get_frame` arAllFramesReady, check v005 cache
  hit. If hit: return cached frame via caller-owned ref using
  `Cnr3OwnedFrameRef` RAII wrapper. If miss: proceed with normal
  computation.
- Instrument `cache_hits_at_arAllFramesReady`.

#### Phase CMS02-G — Checkpoint recovery and hole filling under fmUnordered
- `find_and_pin_nearest_prior_checkpoint` for out-of-order requests.
- Implement rolling predecessor reference pattern (Section 4.5.1).
- Fill missing frames ascending using fill-holes-only rule.
- Skip frames already present in cache.
- Store newly generated frames; `prune_after_store`.
- Unpin checkpoint on every exit path.
- Instrument `holes_filled`,
  `recovery_frames_skipped_already_cached`,
  `nearest_checkpoint_recovery_count`,
  `predecessor_missing_when_expected`.

#### Phase CMS02-H — Bounded warm-up recovery under fmUnordered
- Implement no-prior-checkpoint warm-up per Section 4.5 using the
  rolling predecessor pattern.
- Instrument `bounded_warmup_recovery_count`, start frame, length,
  frames computed vs. skipped.

#### Phase CMS02-I — Empirical review: non-checkpoint pinning decision
After realistic VHS/VHS-C encode tests and synthetic jump tests:
- Inspect all Section 6 counters, especially
  `predecessor_missing_when_expected` and ref-balance counters.
- If mandatory promotion criteria (Section 4.4) are met: implement
  non-checkpoint pinning before proceeding.
- If criteria are not met: document findings and proceed to CMS02-J.

#### Phase CMS02-J — fmParallelRequests wiring and proving
Only after CMS02-H proven and CMS02-I decision made.
- Wire fmParallelRequests path: same sliding update helper
  (implementation already complete from CMS02-C), retirement helper
  invoked with `FmParallelRequests` mode.
- Test concurrent jump scenarios.
- Validate retirement with pinned-checkpoint proxy.
- Decide on `active_request_count` per zone if retirement proves too
  conservative.

#### Full fmParallel — explicitly out of scope for this iteration.

---

## 9. Structural Changes Required to Uploaded Code

### 9.1 `cnr3_cache_manager.h` — Structure additions

**A. Add `Cnr3HotZone` struct:**

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

**B. Add hot zone array to `Cnr3CacheManagerV005`:** Fixed-size array of
`Cnr3HotZone`, size `CNR3_MAX_HOT_ZONES`, all initially inactive.
Mutable state — access only while holding `cache_mutex`.

**C. Add `active_ceiling` field to `Cnr3CacheManagerV005`:** int, set
once at `cnr3_create()` via byte-budget computation.

**D. Add `Cnr3CacheSchedulingMode` enum:**

```cpp
enum class Cnr3CacheSchedulingMode {
    FmUnordered,
    FmParallelRequests
};
```

**E. Add new constants per Section 5.** Remove obsolete frame-count
ceiling constants and `CNR3_HOT_ZONE_EXTENSION_MARGIN`.

**F. Add new statistics counters per Section 6**, including ref-count
counters.

**G. Add `Cnr3OwnedFrameRef` RAII wrapper per Section 2.2.** Recommended
for use at call sites.

**NOTE:** Non-checkpoint pinning structural change (changing
`non_checkpoint_pool` value type) is deferred to Phase CMS02-I.

### 9.2 `cnr3_cache_manager.h` — New helper declarations

**Hot zone helpers:**

- `cnr3_cache_manager_update_hot_zones(cache, frame_number)` — public,
  locks. Uses sliding rule from Section 4.2.1 in both modes. No mode
  parameter.

- `cnr3_cache_manager_is_frame_in_hot_zone_externally_locked(cache,
  frame_number)` — bool, caller holds mutex.

- `cnr3_cache_manager_retire_cold_hot_zones_externally_locked(cache,
  mode)` — caller holds mutex. Takes `Cnr3CacheSchedulingMode` to
  select eager (fmUnordered) or lazy (fmParallelRequests) retirement.

- `cnr3_cache_manager_set_ceiling(cache, vi)` — public, called once at
  `cnr3_create`.

**Ceiling check:**

- `cnr3_cache_manager_would_exceed_ceiling_externally_locked(cache)` —
  bool. Returns true iff a subsequent store would cause
  `total_live_refs_after_store > active_ceiling`.

**H. Cache hit lookup (Section 2.1, Section 4.7) — defensive contract:**

`cnr3_cache_manager_find_output_frame_and_add_ref(cache, frame_number,
vsapi)` must:

1. Lock `cache_mutex`.
2. Find `frame_number` in `cache_index`.
3. If not found, increment `cache_misses`, unlock, return nullptr.
4. Verify the indexed owner pool actually contains `frame_number`. If
   not, this is an internal inconsistency — increment a new counter
   `cache_index_inconsistency_detected`, log a warning, unlock, return
   nullptr.
5. Verify the stored `VSFrame*` is non-null. If null, same handling as
   step 4.
6. Call `vsapi->addFrameRef(frame)`. Increment
   `cache_addframeref_lookup_total`.
7. Increment `cache_hits` (or `cache_hits_at_arAllFramesReady` if called
   from that path; counter discipline at implementation time).
8. Unlock `cache_mutex`.
9. Return the caller-owned `VSFrame*`.

The caller must `freeFrame` the returned reference on every exit path
(RC5). Use of `Cnr3OwnedFrameRef` at the call site is recommended.

### 9.3 `cnr3_cache_manager.cpp` — Logic changes

**I. `prune_non_checkpoint_pool_externally_locked`:** Replace
`while { evict begin() }` with hot-zone-aware candidate selection per
Section 4.3.1 Phase A. Call `retire_cold_hot_zones` first. Every
eviction goes through the single remove helper (RC2).

**J. `prune_checkpoint_pool_externally_locked`:** Add hot-zone candidate
filtering before existing frame-zero and pin_count checks (Section
4.3.2). Same remove discipline.

**K. `store_output_frame` (public):** Before `addFrameRef` and pool
insertion, call `would_exceed_ceiling`. If exceeded AND prune cannot
free anything, return false without taking `addFrameRef` (RC3 — no
imbalance) and increment `cache_ceiling_hard_aborts`.

If `addFrameRef` is taken and insertion subsequently fails, balance
with `freeFrame` before returning (RC3).

**L. `validate_invariants_externally_locked`:** Add:

- All hot zone slots with `active==true` have `low >= 0`,
  `high >= low`, `last_observed_frame >= 0`.
- Total live frames ≤ `active_ceiling`.
- Ref-balance check (RC7):
  `cache_addframeref_total - cache_freeframe_total ==
   non_checkpoint_pool.size() + checkpoint_pool.size()`.
  This check is for cache-side balance only; caller-owned references
  taken via `find_output_frame_and_add_ref` are tracked separately in
  `cache_addframeref_lookup_total` and are not part of the cache
  invariant (they are the caller's responsibility per RC5).

### 9.4 `vapoursynth-Cnr3.cpp` — Wiring changes (future phases)

**M. `cnr3_create()`:**

```cpp
cnr3_cache_manager_set_ceiling(d->cache_manager_v005, d->vi);
```

**N. `cnr3_get_frame()` arInitial:**

```cpp
cnr3_cache_manager_update_hot_zones(cache, frame_number);
```

Called for every arriving frame request before any cache lookup.

**O. Debug output at `cnr3_free`:**

Add hot zone state summary, new Section 6 counters, and ref-balance
check result to existing debug summary.
`predecessor_missing_when_expected > 0` or ref-balance failure prints a
prominent warning regardless of diagnostic level.

### 9.5 Failure-Path and Shutdown Discipline

**[EXPANDED CMS04 per Section 2.2 and Appendix B discussion.]**

Every failure path that returns a VapourSynth error must execute cleanup
before returning:

1. Any checkpoint pinned during this operation: `unpin_checkpoint` once
   per `pin_and_find` call.
2. Any caller-owned VSFrame references obtained via the lookup helper
   or other paths: `freeFrame` once. Use `Cnr3OwnedFrameRef` RAII to
   make this automatic.
3. Any source frame references obtained from VapourSynth: `freeFrame`.
4. Any destination frame allocated but not being returned as output:
   `freeFrame`.
5. Hot zone state: no rollback needed.
6. Diagnostic counters: still incremented as appropriate.

**Shutdown protocol (`Cnr3CacheManagerV005` destruction):**

A `clear()` method (callable explicitly or invoked from the destructor)
must:

1. Acquire `cache_mutex` for the duration.
2. Iterate `non_checkpoint_pool`: for each slot, `freeFrame(frame)`,
   increment `cache_freeframe_total`. Erase slot.
3. Iterate `checkpoint_pool`: for each slot:
   - If `pin_count > 0`, log a warning citing the frame number and
     `pin_count` value (indicates a missed unpin somewhere in the
     code — logical leak, not a memory leak, since we are freeing the
     frame regardless).
   - `freeFrame(frame)`, increment `cache_freeframe_total`. Erase slot.
4. Clear `cache_index`.
5. Reset hot zone slots to inactive.
6. Run `validate_invariants` to confirm ref-balance.

After clear, the cache manager is empty and ready for either reuse or
destruction.

---

## 10. Known Hazard Addressed by Hot-Zone Lifecycle Rules

**Hazard scenario (from CMS01 review Section 4, retained as a test
case):**

> 1. Request A is computing output[450].
> 2. It needs output[449] as the predecessor.
> 3. output[449] is present in the non-checkpoint pool.
> 4. A forward jump creates a new hot zone around output[800].
> 5. The old hot zone around output[450] is shifted, merged, retired
>    too early, or becomes too narrow.
> 6. Pruning sees output[449] as outside all hot zones and removes it.
> 7. Request A no longer has the predecessor it expected.

**Resolution by CMS04 design:**

This scenario is the canonical test case for hot-zone lifecycle
correctness. The design closes it as follows:

- **Step 5 in fmUnordered:** The previous request has completed before
  this `arInitial` fires (eager retirement is safe). Request A is not
  in flight at the time of the jump.

- **Step 5 in fmParallelRequests:** Sliding can occur, moving the zone
  past output[449]. This is by design — the rolling predecessor
  reference pattern (Section 4.5.1) ensures Request A's walk already
  holds an owned reference to output[449] before any concurrent
  pruning could fire. Lazy retirement (pinned-checkpoint proxy) also
  prevents premature retirement of zones associated with active
  recovery walks.

- **Step 6:** Even if a slide moves the zone past output[449] and a
  subsequent prune evicts the cache slot, the walk's owned
  `addFrameRef` reference keeps the underlying VSFrame alive until the
  walk releases it. Pruning removes the cache slot; the VSFrame is
  destroyed only when its reference count reaches zero, which the
  walk's reference prevents.

- **Runtime verification:** `predecessor_missing_when_expected` is the
  empirical detector. If it ever increments, the design has failed
  somewhere and non-checkpoint pinning becomes mandatory before
  further work (Section 4.4).

The hazard is acknowledged, addressed by the combination of hot-zone
lifecycle rules, the rolling predecessor reference pattern, and
`addFrameRef` discipline, and continuously monitored at runtime.

---

## 11. Items to Confirm Empirically

**EI1 — Lazy retirement proxy under fmParallelRequests.**
Resolution: accept the pinned-checkpoint proxy for first
fmParallelRequests implementation. To confirm: if zones never retire,
add `active_request_count` per zone.

**EI2 — `BACK_RADIUS = 50` empirical sufficiency.** Starting value 50.
To confirm: observe `recovery_frames_computed` and
`cache_hits_at_arAllFramesReady`.

**EI3 — Checkpoint retain values `MAX=32`, `MIN=10`.** Starting values.
To confirm: observe `max_checkpoint_pool_size_observed`.

**EI4 — Ceiling abort frequency.** `cache_ceiling_hard_aborts` should
be zero during normal linear encoding.

**EI5 — `prune_no_candidate_exists` behaviour.** Accept "do not evict
protected frames" initially. To confirm: monitor and reduce
`BACK_RADIUS` or `MAX_HOT_ZONES` if it increments significantly.

**EI6 — Non-checkpoint pinning promotion decision (Phase CMS02-I).**
Mandatory criteria in Section 4.4. The most important empirical
decision in the implementation sequence.

**EI7 — Reference-count balance.** `cache_addframeref_total -
cache_freeframe_total` must equal total live slots at quiescence and
zero at shutdown after `clear()`. Any deviation is a discipline
failure and must be diagnosed before the implementation is considered
correct.

---

## Appendix A — Sliding vs. Extend-Only: Discussion and Decision

**[ADDED CMS04 to record the design conversation behind the
sliding-everywhere decision. Future maintainers should read this to
understand why the lifecycle is the way it is.]**

### Background

CMS02 attempted to use "extend-only, never shrink" zones in all modes.
This was found in the CMS03 review to contain a fatal contradiction:
linear encoding examples required the zone's back boundary to advance
forward (so old frames could be pruned), but the rule said zones never
shrink. The two were incompatible.

CMS03 resolved this by introducing **mode-specific** lifecycles:

- **fmUnordered:** sliding (zone bounds recomputed from `F` on every
  in-zone arrival, including `low` moving forward).
- **fmParallelRequests:** extend-only outward (zone bounds only grow,
  never contract, on the assumption that contracting could remove
  protection from in-flight predecessors of concurrent walks).

The reasoning for the split was conservatism: in fmParallelRequests,
multiple walks may concurrently rely on predecessors within the zone,
and sliding forward could remove protection from a walk that hadn't
yet looked up its predecessor.

### The Concern Raised

During CMS03 review, the user observed that the working pattern in
both modes is conceptually similar: a hot area sliding along the video
timeline, with concurrent threads bubbling around in that hot area. A
different algorithm for fmParallelRequests felt inconsistent with the
fundamental use case.

The user's intuition: sliding might be correct for both modes, with
the right safety mechanisms.

### Analysis

**Sequential linear encoding under sliding in fmParallelRequests:**

Suppose 16 concurrent walks compute frames 100–115. Sequential linear
encoding, BestSource jitter ~6 frames.

- When frame 100's `arInitial` fires, zone becomes `[50, 110]`.
- When frame 115's `arInitial` fires later, zone slides to `[65, 125]`.
- The 16 walks compute frames 100–115 with predecessors 99–114.
- Predecessors 99–114 all lie inside the slid zone `[65, 125]`.
- No predecessor loses protection.

The zone slides at the rate of the leading edge of arrivals. With
`BACK_RADIUS=50` and typical concurrency spread of 22–38 frames
(16–32 threads with jitter ~6), sliding never strips protection from
in-flight predecessors under normal sequential operation.

**The pathological case:**

A walk for frame 100 stalls (scheduling delay) while concurrent walks
complete frames 101–200, sliding the zone to `[150, 210]`. The stalled
walk eventually proceeds and calls
`find_output_frame_and_add_ref(99)`. Frame 99 has been pruned. Lookup
returns nullptr.

Crucially, **this is not a crash and not incorrect output**. The walk
falls back to checkpoint recovery: find nearest prior checkpoint, fill
holes. Checkpoints are retained much longer than non-checkpoint frames
(`MAX_RETAIN=32` plus hot-zone protection), so a checkpoint near
frame 99 almost certainly still exists. The walk takes a slower path
but produces correct output.

The pathological case produces **more recompute work**, not bad data.
And the diagnostic counter `predecessor_missing_when_expected` detects
it directly. If it increments, the design promotes to non-checkpoint
pinning (Section 4.4).

**The role of `addFrameRef` and the rolling predecessor pattern:**

When a walk has *already begun* using a predecessor (it called
`find_output_frame_and_add_ref` and holds the owned reference), the
underlying VSFrame stays alive even if the cache slot is pruned out
from under it. The slide doesn't damage walks that are mid-computation.

The rolling predecessor reference pattern (Section 4.5.1) ensures that
a walk's transition from iteration K to K+1 never has a window of
zero references. The walk acquires the new predecessor reference
*before* releasing the old one. This is the load-bearing pattern.

### Comparison of Approaches

**Sliding everywhere (CMS04 decision):**
- One algorithm to understand, debug, validate.
- Memory-bounded zones (always BACK + FORWARD + 1 = 61 frames wide).
- Aggressive pruning of frames behind the working position.
- Stalled walks pay a recovery cost (checkpoint recovery) rather than
  getting free protection.
- Detectable via `predecessor_missing_when_expected`.

**Extend-only in fmParallelRequests (CMS03 approach):**
- Different algorithm for different modes.
- Zones can grow indefinitely under sustained activity, accumulating
  history.
- More frames protected — fewer recovery walks.
- Larger memory footprint under sustained pressure.
- Mental model harder: "how big is this zone right now?" depends on
  history.

### Decision

**Use sliding in both modes.** The only mode-specific behaviour is
**retirement**: eager in fmUnordered (a new `arInitial` proves the
previous request has completed), lazy with pinned-checkpoint proxy in
fmParallelRequests (in-flight requests may still need zone frames).

The `update_hot_zones` helper does not need a mode parameter. Only the
retirement helper does.

This decision was made because:

1. It is conceptually simpler — one algorithm, one mental model.
2. It is consistent with the use case (a sliding hot area in a moving
   timeline) regardless of mode.
3. The safety property is preserved by the rolling predecessor pattern
   plus `addFrameRef`, not by zones being conservative.
4. The pathological case produces correct output (just more compute),
   and is directly detectable via diagnostics.
5. If diagnostics reveal real problems, non-checkpoint pinning is the
   well-understood escalation path.

### What Future Maintainers Should Watch For

- If `predecessor_missing_when_expected` ever increments in production,
  the sliding decision needs to be revisited. Either promote to
  non-checkpoint pinning, or reintroduce extend-only zones in
  fmParallelRequests.
- If the rolling predecessor pattern is broken (a walk releases
  `prev_ref` before acquiring the next), all of the safety argument
  collapses. The walk implementation is load-bearing.
- The decision is not "sliding is always best." It is "sliding is good
  enough given the other safety mechanisms in place." If those
  mechanisms are removed or weakened, sliding alone is not sufficient.

---

## Appendix B — Reference-Count Discipline: Discussion and Decision

**[ADDED CMS04 to record why ref-count discipline was promoted to a
first-class concern. Future maintainers should read this to understand
the rationale for the explicit invariant and the supporting
mechanisms.]**

### Background

CMS03 introduced `find_output_frame_and_add_ref` and described
`addFrameRef` as "load-bearing while non-checkpoint pinning is
deferred." However, CMS03 did not state the reference-count invariant
as a first-class design goal. It assumed the discipline implicitly.

### The Concern Raised

During CMS03 review, the user raised a critical concern:

> "the cache slot is pruned out from under it" and the consequential
> potential risk of having dangling Addrefs and runaway memory
> consumption by VS hanging onto forgotten frame YUV data (i.e. had not
> had a matching release) which no longer has slots to remember the
> refs.

The user asked: is the design 100% robust in ensuring there will never
be dangling frames or dangling slots? Is that explicitly a design goal?

### Analysis

**Reference accounting for each VSFrame the cache manages:**

The cache contributes exactly one `addFrameRef` per slot at store
time, and exactly one `freeFrame` at slot removal. Each cache-side
contribution is balanced.

Callers using the lookup helper take additional `addFrameRef` (one per
lookup) and balance with `freeFrame` (one per exit path). Each caller
contribution is balanced independently.

**Where leaks could happen:**

1. **Failed insert after addFrameRef.** Store helper takes
   `addFrameRef` but pool insertion subsequently fails (allocation,
   validation). Without RC3, the ref is orphaned.

2. **Erase without freeFrame.** Any direct `pool.erase()` or
   `cache_index.erase()` bypassing the remove helper leaks the cache's
   ref to the erased frame.

3. **Caller forgets freeFrame.** A caller exit path (success, error,
   exception, early return) doesn't `freeFrame` a caller-owned ref.

4. **Shutdown without clear.** Destruction of the cache manager
   without freeing all slots leaks all live refs.

5. **Lingering pinned checkpoint.** A failed recovery that pinned but
   did not unpin. This is a logical leak (slot becomes un-prunable),
   not a memory leak (cache still owns the ref and frees at shutdown).

**Does sliding make this worse?**

No. Sliding causes pruning to fire more frequently, which exercises
the remove-with-freeFrame path more often. If the discipline is
correct, more exercise is fine. If the discipline has a bug, sliding
finds it faster — arguably a feature.

The one subtlety: sliding can cause a caller's lookup to find a frame,
take an owned ref, and then have the cache slot evicted while the
caller is still using its ref. The VSFrame stays alive (ref count from
caller keeps it), and is destroyed when the caller releases. **No
leak.** This is the load-bearing safety property of the owned-ref
design — the cache's ref and the caller's ref are independent.

### Decision

**Promote the reference-count invariant to an explicit first-class
design goal (Goal 5), and add Section 2.2 as a named discipline
section.**

The discipline rules (RC1–RC7) make explicit what was previously
implicit:

- Single store helper (RC1)
- Single remove helper (RC2)
- Store error paths rebalance (RC3)
- Lookup error paths rebalance (RC4)
- Caller exit paths freeFrame (RC5)
- Shutdown clears (RC6)
- Validation enforces balance (RC7)

The discipline is supported by:

- **Diagnostic counters** (`cache_addframeref_total`,
  `cache_freeframe_total`, `cache_addframeref_lookup_total`) for
  runtime balance checks.
- **Recommended RAII wrapper** (`Cnr3OwnedFrameRef`) to reduce the
  surface area for RC5 violations at call sites.
- **Explicit shutdown protocol** (Section 9.5) that iterates all
  slots, freeFrames each, and warns on lingering pinned checkpoints.

### What Future Maintainers Should Watch For

- Any new code path that calls `addFrameRef` or `freeFrame` outside
  the single store and remove helpers is a discipline violation.
- Any new path that uses raw `pool.erase()` or `cache_index.erase()`
  is a discipline violation.
- Ref-balance failure at shutdown (counter difference ≠ 0 after
  `clear()`) is a real bug, not a tolerable approximation. Diagnose
  before declaring the implementation correct.
- New callers of `find_output_frame_and_add_ref` should use the
  `Cnr3OwnedFrameRef` wrapper if possible. Hand-written
  `addFrameRef`/`freeFrame` is acceptable but requires careful
  exit-path discipline.
- If non-checkpoint pinning is added in Phase CMS02-I, the same
  discipline applies to the new pin/unpin helpers — single pin
  helper, single unpin helper, balanced on every exit path.

