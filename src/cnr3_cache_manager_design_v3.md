# CNR3 Cache Manager — Revised Design Specification CMS03
## Jump-Safe Hot-Zone Pruning with Non-Checkpoint Pinning (Deferred)

**Date:** 2026-06-01
**Status:** Design specification — pre-implementation, ready for coding
**Supersedes:** CMS02 (cnr3_cache_manager_design_v2.md), CMS01
**Also supersedes:** Bounded-recovery policy in handover snapshot v0.14 section 0.9A
**Companion documents:** CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)

---

## Change Summary CMS02 → CMS03

Each change is annotated in-place in the spec with **[CHANGED CMS03 — reason]**
or **[ADDED CMS03 — reason]**. The complete list:

1. **Major correction — hot zones must slide, not only extend.** CMS02
   contained a contradiction between "zones only extend, never shrink" and
   the linear-pruning examples that required the back boundary to move
   forward. Hot zone lifecycle is now explicitly mode-specific: sliding in
   fmUnordered, extend-only in fmParallelRequests. See Section 4.2.

2. **Phase-guarded non-checkpoint `pin_count` references.** CMS02 referenced
   `pin_count == 0` as a prune candidate filter even though non-checkpoint
   pinning is deferred. Pruning rules now split into pre-pinning and
   post-pinning phases. See Section 4.3.

3. **Reference ownership in fill-holes-only recovery.** The lookup helper
   takes `addFrameRef()` while still holding the mutex; the caller receives
   a caller-owned reference and must `freeFrame()` on every exit path.
   `addFrameRef` controls VSFrame lifetime; hot-zone protection controls
   pruning. These are distinct responsibilities. See Sections 2.1 and 4.7.

4. **Design Goal 1 reworded.** Removed the "guarantee … to the extent
   achievable" awkwardness. The wording now accurately reflects the
   empirical, hot-zone-first position. See Section 2.

5. **Byte-budget ceiling adopted now.** OQ3 from CMS02 is resolved as
   "implement now in Phase CMS02-A." Frame-count ceilings are replaced
   by a byte budget divided by per-frame bytes computed from actual plane
   geometry. See Sections 4.6 and 5.

6. **Checkpoint retain constants increased.** OQ4 from CMS02 is resolved:
   `MAX_RETAIN = 32`, `MIN_RETAIN = 10`. See Section 5.

7. **`BACK_RADIUS` increased to 50.** Aligns with earlier recommendation;
   covers 5 checkpoint intervals of backward history. Subject to empirical
   tuning after Phase CMS02-E. See Section 5.

8. **Extension margin scoped to non-sliding phases only.** With sliding
   zones in fmUnordered, the extension margin is unnecessary. It remains
   useful only for fmParallelRequests where zones extend rather than slide.
   See Section 4.2 and Section 5.

9. **Phase labels renamed from CMS01-* to CMS02-***. CMS02 retained the
   CMS01 phase names; CMS03 renames them to avoid handover confusion.
   See Section 8.

10. **Ceiling-abort cleanup discipline made explicit.** A failed store at
    the ceiling must not leak references or leave pinned checkpoints. See
    Section 4.6 and Section 9 (failure-path rules).

11. **Section 10 renamed.** "Rejected/Handle-Carefully" → "Known Hazard
    Addressed by Hot-Zone Lifecycle Rules." Same content, neutral framing.

12. **arInitial vs. cache-hit hot zone update clarified.** Hot zone updates
    fire at arInitial regardless of whether the request later results in a
    cache hit. New diagnostics distinguish the cases. See Section 4.8.

13. **OQ1–OQ5 finalized as decisions.** No outstanding open questions
    block implementation. A small set of post-implementation review items
    is retained in Section 11 (renamed "Items to Confirm Empirically").

---

## 1. Problem Statement

CNR3 is a recursive temporal chroma stabiliser. Every output frame depends on
the previous output frame as a predecessor. The cache manager must retain
enough predecessor frames to satisfy any in-flight computation, while pruning
old frames to keep memory bounded.

The existing v005 cache manager infrastructure (pools, checkpoint pinning,
mutex model, store/remove/validate helpers) is sound. What is not yet
designed is the pruning policy and the structural protections needed to make
that policy safe under three VapourSynth execution modes:

- **fmUnordered** — one request in flight at a time, mostly sequential
- **fmParallelRequests** — multiple concurrent requests, serialised writer
- **fmParallel** — fully concurrent readers and writers (out of scope for
  this iteration)

The primary failure modes without the new design are:

**FM1 — Prune destroys in-flight predecessor.** A walk is computing
output[450] using output[449] as predecessor. Prune evicts output[449]
because a forward jump has pushed new frames into the cache. Result:
corrupt output or crash.

**FM2 — Prune destroys a checkpoint needed by a recovery chain.** A
recovery walk pins checkpoint[400] and fills frames 401..500. A concurrent
prune evicts checkpoint[380] which another concurrent request needed as
its anchor. That request falls back to the expensive no-prior-checkpoint
path unnecessarily.

**FM3 — Jump recovery burst exceeds pool capacity.** A forward jump
triggers bounded warm-up recovery generating up to CAPACITY new frames.
Simultaneously, pre-jump in-flight requests hold frames in the pool. The
combined total exceeds the overflow limit; stores fail and the filter
aborts unnecessarily.

**FM4 — Prune eviction key is wrong.** Current pruning evicts the lowest
frame number first. After a forward jump, the lowest frame numbers are
exactly the pre-jump in-flight frames most critically needed.

---

## 2. Design Goals

**[CHANGED CMS03 — Goal 1 reworded per review item 3 to remove
"guarantee … to the extent achievable" contradiction.]**

1. **Prevent pruning of frames likely to be needed by active computation
   using hot-zone protection** in the first implementation. If diagnostics
   show that hot-zone protection is insufficient, promote non-checkpoint
   pinning to a mandatory implementation step before proceeding (see
   Section 4.4).

2. Make pruning decisions based on frame-number proximity to active work,
   not on insertion order or frame-number magnitude alone.

3. Support up to `CNR3_MAX_HOT_ZONES` simultaneous active working ranges
   (covering concurrent pre-jump and post-jump activity in
   fmParallelRequests).

4. Fill holes only — never recompute a frame that is already cached
   (Section 2.1).

5. Maintain `addFrameRef`/`freeFrame` discipline for every cached frame
   used outside the cache mutex (Section 2.1, Section 4.7).

6. Bound memory use with a hard ceiling computed from actual frame
   geometry and a configurable byte budget. Tune after empirical runtime
   memory data (Section 4.6).

7. Hard-abort cleanly when the ceiling is hit with nothing prunable. The
   filter must remain in a valid state after the abort, with no leaked
   references or stuck pinned checkpoints (Section 4.6, Section 9).

8. Remain compatible with the existing mutex model, pool structures, and
   `_externally_locked` helper pattern.

9. Require no changes to the VapourSynth API interaction model.

10. Keep the first implementation understandable and empirically provable
    before adding further complexity.

### 2.1 Fill-Holes-Only Principle (with Reference-Ownership Rule)

**[ADDED CMS02; refined CMS03 to include reference-ownership rule per
review item 4 and the user's clarification that the helper, not the
caller, makes the `addFrameRef` call.]**

The cache manager must never blindly recompute a complete chain when the
required output frames are already cached.

Recovery and chain-filling must operate as follows:

- Recovery proceeds in ascending frame-number order only.
- Before computing any frame K in a recovery or chain-fill walk, check
  whether output[K] already exists in the cache.
- If output[K] is present, reuse it as the predecessor for output[K+1]
  without recomputing it.
- If output[K] is absent (a hole), compute it from the best available
  predecessor, store it, and continue.
- If another concurrent request has already filled part of the chain, the
  later walk skips already-filled frames.

**Reference-ownership rule (added CMS03):**

When output[K] is found in cache and used as the predecessor for
output[K+1] *outside* the cache mutex, the lookup helper takes
`addFrameRef()` on the found frame *while still holding the cache mutex*
(making the find-and-ref atomic), then returns that caller-owned
reference. The caller must `freeFrame()` it on every exit path —
success, error, early return.

This is implemented by `cnr3_cache_manager_find_output_frame_and_add_ref()`
(Section 9.2.H). Callers never see a raw borrowed `VSFrame*` after the
mutex is released.

**Why this matters even with hot-zone protection:**

Hot-zone protection prevents pruning from *evicting the slot* during the
walk. But VapourSynth's reference-count contract requires that anyone
using a `VSFrame*` outside the structure that owns it holds an explicit
reference. Even if pruning is fully prevented, returning a raw borrowed
pointer would violate this contract. The two responsibilities are
separate:

- **Hot-zone protection** — prevents the cache from pruning the slot.
- **`addFrameRef`** — ensures the VSFrame's pixel data remains valid
  during use outside the mutex, independent of the cache's own
  bookkeeping.

The lookup helper combines both: under the mutex, it confirms the slot
exists and takes a reference; the caller then uses the reference safely
outside the mutex and frees it.

**Boundary case:** for paths that use a frame only *while holding the
mutex* (e.g., validation or internal bookkeeping), no `addFrameRef` is
needed because the mutex prevents concurrent modification. These paths
must not retain the pointer after releasing the mutex.

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
approximately 9 frames at p99. A forward hot zone radius of 10 frames
covers this with headroom.

**Implication for hot zone forward radius:**
`CNR3_HOT_ZONE_FORWARD_RADIUS = 10` is well-supported for realistic
BestSource jitter up to approximately `jitter_max=8`.

**Implication for reorder buffer:**
Existing `CNR3_REORDER_WINDOW = 32` is adequate for jitter up to
approximately `jitter_max=24` at p99. No change needed.

---

## 4. Algorithm Overview

### 4.1 Hot Zone Tracking

The cache manager maintains a small fixed-size array of hot zones. Each
hot zone represents a contiguous range of frame numbers that is currently
active (one or more in-flight computations work within that range).

A hot zone has:

- An **active** flag
- A **low** frame number boundary (inclusive)
- A **high** frame number boundary (inclusive)
- A **last_observed_frame** — the most recently arrived frame number that
  fell within or caused this zone
- Diagnostic counters: `hit_count`, `extension_count`, `slide_count`,
  `merge_count`, `retirement_count`, `prune_protection_count`

**Why discrete hot zones, not a single watermark:** A single
lowest-protected-frame-number watermark cannot represent two simultaneous
active ranges. After a forward jump, pre-jump in-flight requests cluster
at low frame numbers while post-jump recovery clusters at high frame
numbers. A watermark set to protect the lower range allows pruning of the
upper range and vice versa. Discrete zones represent both ranges
independently. (Note: under fmUnordered, two zones can briefly coexist
during the arInitial of a jump; see Section 4.2.)

**Why not a rolling median:** A single rolling median across both active
ranges smears to a meaningless midpoint between them, protecting a dead
zone while exposing the edges of both active ranges. Discrete zones
avoid this.

### 4.2 Hot Zone Lifecycle — Mode-Specific Rules

**[MAJOR CHANGE CMS03 — review item 1 identified a contradiction in
CMS02: zones could not both "never shrink" and slide forward to allow
old frames to be pruned. CMS03 resolves this by making the lifecycle
explicitly mode-specific. fmUnordered uses **sliding** zones (the zone
moves with the working position); fmParallelRequests uses **extend-only**
zones (zones grow but do not shrink while active work may exist).]**

#### 4.2.1 fmUnordered: Sliding Hot Zones

In fmUnordered, only one request is in flight at any time. By the time a
new `arInitial` fires, the previous `arAllFramesReady` (or `arError`) has
completed. Therefore, when a new request arrives, any zone associated
with previous work is definitively cold.

**Sliding rule for in-zone or near-zone arrivals:**

For an arriving frame `F`:

1. Find any active zone Z such that `F` is within
   `CNR3_HOT_ZONE_JUMP_THRESHOLD` of Z's range
   (i.e., `Z.low - JUMP_THRESHOLD ≤ F ≤ Z.high + JUMP_THRESHOLD`).

2. If such a Z exists, **slide Z to be centred on F**:
   ```
   Z.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   Z.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   Z.last_observed_frame = F
   ```
   Increment `slide_count` if `F` was already inside Z (or
   `extension_count` if `F` was just outside but within threshold). This
   is a single deterministic assignment — the zone's bounds are
   recomputed from F using the radii. There is no extension margin.

3. If no such Z exists (i.e., `F` is far outside all zones), allocate a
   new zone (see 4.2.3).

**Pre-allocation retirement (fmUnordered only):**

Because fmUnordered guarantees the previous request has completed, the
arInitial handler may retire any zone whose `last_observed_frame` is not
the current `F` before allocating a new zone. In practice this means
fmUnordered typically operates with **at most one active zone**, which
slides forward in normal operation and jumps to a new position on
discontinuity.

**Why sliding is safe in fmUnordered:**

- The walk for the previous request completed before this `arInitial`
  fired. No predecessor frames from the previous walk are still in
  active use.
- The current walk holds its own `addFrameRef` references on any cached
  frames it uses as predecessors (Section 4.7). Even if a slide moves
  the zone past those frames and a subsequent prune evicts them, the
  walk's owned reference keeps the underlying VSFrame valid.
- Frames that fall outside the slid zone are simply prunable. This is
  the correct behaviour — they are no longer in the working set.

#### 4.2.2 fmParallelRequests: Extend-Only Hot Zones

In fmParallelRequests, multiple requests may be in flight simultaneously.
A zone may represent the working range of more than one in-flight
request. Sliding a zone forward could remove protection from frames
still needed by an in-flight request whose target is at the back of the
zone.

**Extend-only rule for in-zone or near-zone arrivals:**

For an arriving frame `F`:

1. Find any active zone Z such that `F` is within
   `CNR3_HOT_ZONE_JUMP_THRESHOLD` of Z's range.

2. If `F` is inside Z's range (`Z.low ≤ F ≤ Z.high`):
   - Update `Z.last_observed_frame = F`
   - Do not move boundaries. (They already cover F.)
   - Increment `hit_count`.

3. If `F` is just outside Z's range, within `JUMP_THRESHOLD`:
   - **Extend only outward**:
     ```
     Z.low  = min(Z.low,  max(0, F - CNR3_HOT_ZONE_BACK_RADIUS))
     Z.high = max(Z.high, F + CNR3_HOT_ZONE_FORWARD_RADIUS)
     Z.last_observed_frame = F
     ```
   - Increment `extension_count`.
   - The `CNR3_HOT_ZONE_EXTENSION_MARGIN` constant is **not used in
     fmParallelRequests** for in-zone updates either — extension happens
     whenever F is just outside the zone. The margin concept was
     specific to CMS02's centre-shift model and is dropped (Section 5).

4. If no Z is within threshold, allocate a new zone (4.2.3).

**Retirement (fmParallelRequests):**

Zones are retired only lazily — when a new zone allocation is needed and
all slots are full. A zone is eligible for lazy retirement if:

- No live frames remain in either pool within its `[low, high]` range,
  AND
- No pinned checkpoint exists within its range.

The pinned-checkpoint check is a conservative proxy for "no recovery
walk is in progress within this zone." It may retain zones slightly
longer than strictly necessary, which is safe.

If neither criterion is met for any zone and all slots are full, two
zones must be merged (4.2.4).

#### 4.2.3 New Zone Allocation (both modes)

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
   - In **fmUnordered**: retire the existing zone (its work is complete)
     and use that slot.
   - In **fmParallelRequests**: attempt lazy retirement (4.2.2). If that
     fails, merge (4.2.4).

#### 4.2.4 Zone Merge (fmParallelRequests only)

When all slots are full and no zone is eligible for lazy retirement:

1. Find the two zones whose boundaries are closest (smallest gap between
   one zone's `high` and another zone's `low`).
2. Merge them:
   ```
   merged.low  = min(Z1.low,  Z2.low)
   merged.high = max(Z1.high, Z2.high)
   merged.last_observed_frame = max(Z1.last_observed_frame,
                                     Z2.last_observed_frame)
   ```
3. Mark one slot inactive, store merged in the other.
4. Increment `merge_count`.
5. Use the freed slot for the new zone allocation.

Merging is conservative: the merged zone protects everything either
original protected. No frames lose protection.

Log the merge action (zones involved, frame ranges) for diagnostics.

### 4.3 Pruning Policy — Hot Zone Aware (Phase-Guarded)

**[CHANGED CMS03 per review item 2: CMS02 referenced
`pin_count == 0` as a non-checkpoint prune candidate filter even though
non-checkpoint pinning is deferred. CMS03 splits the rule into
pre-pinning and post-pinning phases.]**

#### 4.3.1 Non-Checkpoint Pool Pruning

**Phase A — before non-checkpoint pinning exists (Phase CMS02-D
through CMS02-H):**

1. Call `retire_cold_hot_zones_externally_locked` (lazy retirement
   pass).
2. Collect non-checkpoint frames whose frame number falls outside every
   active hot zone's `[low, high]` range. These are eviction candidates.
3. Among candidates, find the one with the greatest minimum distance
   from any active hot zone boundary. Evict it.
4. Repeat step 3 until `non_checkpoint_pool.size() <=
   CNR3_OUTPUT_CACHE_CAPACITY` or no candidates remain.
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

Apply hot-zone-aware filtering with the existing pin_count check:

1. A checkpoint is a candidate if:
   - Frame number is not zero (frame 0 is never pruned), AND
   - `pin_count == 0` (existing rule), AND
   - Frame number falls outside every active hot zone's `[low, high]`.
2. Evict the candidate with the greatest distance from any hot zone
   boundary first.
3. Continue until `checkpoint_pool.size() <= CNR3_CHECKPOINT_MIN_RETAIN`
   or no candidates remain.

Checkpoints within hot zones or pinned are retained regardless of
`CNR3_CHECKPOINT_MAX_RETAIN`. The retain limits are soft triggers, not
hard caps.

**Rationale for retaining checkpoints longer:** Checkpoints anchor
recovery chains. A checkpoint inside or near an active hot zone may be
needed as a recovery anchor for in-flight or soon-to-arrive requests.
Evicting it forces more expensive no-prior-checkpoint warm-up recovery.
With `BACK_RADIUS=50` and `CHECKPOINT_INTERVAL=10`, a hot zone holds at
most 5 checkpoints. With 5 active zones, up to 25 checkpoints could be
protected — well within the increased `MAX_RETAIN=32`.

### 4.4 Non-Checkpoint Frame Pinning — Deferred

Non-checkpoint pinning remains the deterministic solution to FM1 if
hot-zone-aware pruning is shown to be insufficient. The first
implementation defers non-checkpoint pinning and instead relies on
conservative hot zones, fill-holes-only recovery, the reference-ownership
rule (Section 2.1), and detailed diagnostics.

**Mandatory promotion criterion (unchanged from CMS02):**

Non-checkpoint pinning becomes mandatory — not optional, not deferred
further — if any of the following are observed during realistic test
runs:

- The diagnostic counter `predecessor_missing_when_expected` is non-zero
  during any realistic VHS/VHS-C encode test.
- Recovery repeatedly recomputes frames that were recently cached and
  should have been retained.
- fmParallelRequests testing reveals a race between prune and active
  predecessor use that hot zones do not cover.
- Hot-zone settings required to prevent the above become so broad that
  they defeat pruning (e.g., back radius must exceed 200 to be safe).

**Structural change if added later (unchanged from CMS02):**

Change `non_checkpoint_pool` from `std::map<int, const VSFrame*>` to
`std::map<int, Cnr3NonCheckpointSlot>` where `Cnr3NonCheckpointSlot =
{ const VSFrame* frame; int pin_count = 0; }`. Mirror the existing
checkpoint pin/unpin pattern.

### 4.5 Bounded Warm-Up Recovery — No-Prior-Checkpoint Case

When a request arrives for frame N and no cached output[N-1] exists and
no usable nearest-prior checkpoint exists:

```
start = max(0, N - CNR3_OUTPUT_CACHE_CAPACITY)
output[start] initialised from source[start] using source-copy semantics
   (no predecessor blend — deliberate approximation, documented)
output[start+1..N] computed using fill-holes-only:
    for K in start+1..N:
        if output[K] in cache: use as predecessor, skip computation
        else: compute from output[K-1], store, continue
```

Before beginning the walk, the hot zone for frame N has already been
allocated by `arInitial` (Section 4.2). The walk frames `[start,
start+BACK_RADIUS-1]` will be **outside** the hot zone (since the zone
only covers `[N-BACK_RADIUS, N+FORWARD_RADIUS]`).

**Safety of out-of-zone walk frames:**

In fmUnordered, no concurrent thread can prune during the walk. Stores
during the walk may trigger prune, but the walk holds a caller-owned
`addFrameRef` reference on each predecessor (Section 2.1), so even if
prune evicts the cache slot, the underlying VSFrame remains alive while
the walk uses it. The walk releases its reference after consuming the
predecessor.

In fmParallelRequests, the walk relies on the same `addFrameRef`
discipline. Concurrent pruning may evict slots from the walk's range,
but the walk's own reference keeps the underlying VSFrame valid for the
duration of its use.

If the `addFrameRef` discipline proves insufficient under
fmParallelRequests (evidenced by `predecessor_missing_when_expected > 0`),
non-checkpoint pinning is promoted per Section 4.4.

### 4.6 Hard Ceiling and Abort Policy — Byte-Budget Based

**[CHANGED CMS03 per review item 5 and OQ3 resolution: frame-count
ceilings (1000/500) replaced by byte-budget calculation using actual
plane geometry.]**

#### 4.6.1 Ceiling Calculation

At `cnr3_create()` time, after `Cnr3Data` construction:

```
estimated_frame_bytes = 0
for each plane p in 0..num_planes-1:
    plane_width  = (vi->width  >> (p > 0 ? sub_w : 0))
    plane_height = (vi->height >> (p > 0 ? sub_h : 0))
    estimated_frame_bytes += plane_width * plane_height * bytes_per_sample
```

where `sub_w` and `sub_h` are the chroma subsampling shifts and
`bytes_per_sample = (bits_per_sample + 7) / 8`.

This formula uses VapourSynth's actual plane dimensions and works
cleanly for 4:2:0, 4:2:2, 4:4:4, and 4:0:0 (grey).

```
candidate_ceiling = CNR3_CACHE_BYTE_BUDGET / estimated_frame_bytes
active_ceiling = clamp(candidate_ceiling,
                       CNR3_CACHE_MIN_HARD_CEILING,
                       CNR3_CACHE_MAX_HARD_CEILING)
```

Worked examples (with `CNR3_CACHE_BYTE_BUDGET = 512 MiB`):

| Format                | bytes/frame | candidate | active (clamped) |
|---|---|---|---|
| 4:2:0 8-bit  720x576  | 622,080     | 863       | 863              |
| 4:2:2 8-bit  720x576  | 829,440     | 647       | 647              |
| 4:2:0 16-bit 720x576  | 1,244,160   | 431       | 431              |
| 4:2:2 16-bit 720x576  | 1,658,880   | 323       | 323              |
| 4:2:0 8-bit  1920x1080| 3,110,400   | 172       | 172              |
| 4:2:0 16-bit 1920x1080| 6,220,800   | 86        | **150** (clamped to MIN) |

The clamp ensures the cache remains usable for very large frames and
caps unbounded growth for very small frames.

#### 4.6.2 Abort Policy

If a store would cause total live VSFrame references (both pools
combined) to reach or exceed `active_ceiling`, AND prune cannot free any
frame (everything within hot zones or pinned):

1. The store returns `false`.
2. Increment `cache_ceiling_hard_aborts`.
3. The calling `getFrame` path returns a VapourSynth filter error for
   that frame:

   *"CNR3: cache ceiling reached ([N] frames). CNR3 is designed for
   near-linear access. Large random seeks in rapid succession may
   exceed cache capacity. Reduce seek frequency or use a near-linear
   workflow."*

#### 4.6.3 Cleanup Discipline on Ceiling Abort

**[ADDED CMS03 per review item 9.]**

A ceiling abort may leave already-successfully-stored frames in the
cache (those frames are valid outputs). However, the failure path must
not leave any temporary runtime state unreleased:

- Any pinned checkpoint pinned during the failed recovery must be
  unpinned exactly once.
- Any caller-owned `VSFrame*` reference held by the recovery walk must
  be `freeFrame`d.
- Any source frame obtained during the recovery walk must be released.
- Any destination frame allocated for the failed frame must be released
  if not returned to VapourSynth.
- Hot zone state must remain consistent (the failed frame's `arInitial`
  may have already extended or allocated a zone — that is acceptable;
  the zone will be retired naturally when its range becomes cold).

After cleanup, the filter remains in a valid state. Subsequent
near-linear requests continue to work normally.

### 4.7 addFrameRef and Pinning — Separate Concerns

**[ADDED CMS03 to make explicit per review item 4 and user
clarification.]**

The cache manager has two distinct protection mechanisms with different
roles:

| Mechanism | Protects against | Holder | Lifetime |
|---|---|---|---|
| **Hot zone membership** | Slot eviction by prune | Cache manager | Until zone slides/retires |
| **Checkpoint `pin_count`** | Slot eviction by prune (checkpoints) | Recovery walks | Pin/unpin pair on every path |
| **`addFrameRef`** | Underlying VSFrame being `freeFrame`d | Caller of lookup helper | Until `freeFrame` by caller |

Hot-zone protection and `pin_count` are both prune-side mechanisms. They
prevent the cache from evicting the slot. `addFrameRef` is independent
— it ensures the VSFrame's pixel data remains valid for the holder even
if the cache decides to evict (the VSFrame's reference count stays > 0
as long as someone holds a ref).

The **lookup helper** (`cnr3_cache_manager_find_output_frame_and_add_ref`)
combines find-and-ref atomically under the mutex:

1. Acquire cache mutex.
2. Look up the frame number in `cache_index`.
3. If found, call `vsapi->addFrameRef(slot.frame)`.
4. Release cache mutex.
5. Return the now-ref'd frame to the caller.

The caller must `vsapi->freeFrame()` on every exit path. This is the
load-bearing safety mechanism for the deferred-pinning regime: even if
prune evicts the cache slot between the lookup and the caller's use, the
caller's reference keeps the VSFrame alive.

### 4.8 arInitial vs. Cache-Hit Hot Zone Update

**[ADDED CMS03 per review item 11.]**

Hot zone updates fire at `arInitial` for every arriving frame request,
regardless of whether the request later results in a cache hit
(no computation) or computation (recovery walk).

This is deliberate: a cache hit still represents activity near that
frame number, and the hot zone should reflect the request stream's
working position.

**Diagnostic counters distinguish these cases:**

- `hot_zone_updates_at_arInitial` — total `arInitial` calls that
  updated a hot zone (slide, extension, hit, or new allocation).
- `cache_hits_at_arAllFramesReady` — requests served from cache without
  computation.
- `recoveries_started_at_arAllFramesReady` — requests that required a
  recovery walk.

If `cache_hits_at_arAllFramesReady` is much larger than
`recoveries_started_at_arAllFramesReady`, the hot zone may appear
active due to cache-hit-only access. This is acceptable behaviour but
useful to understand when interpreting diagnostics.

---

## 5. Constants

**[CHANGED CMS03 — ceiling now byte-budget-based per OQ3 resolution;
`BACK_RADIUS` increased to 50 per user decision and earlier
recommendation; `CHECKPOINT_MAX_RETAIN`/`MIN_RETAIN` increased per OQ4
resolution; `CNR3_HOT_ZONE_EXTENSION_MARGIN` removed per OQ2 resolution
and review item 7 since sliding zones in fmUnordered and extend-on-near-
miss in fmParallelRequests do not need it.]**

```
// --- Soft pruning targets ---

CNR3_OUTPUT_CACHE_CAPACITY        = 100
    Soft pruning target.

CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Prune triggers when non_checkpoint_pool > capacity * factor (= 110).

// --- Hard ceiling (byte-budget based) ---

CNR3_CACHE_BYTE_BUDGET            = 512 * 1024 * 1024  // 512 MiB
    Byte budget for the active_ceiling computation.

CNR3_CACHE_MIN_HARD_CEILING       = 150
    Lower bound on active_ceiling.

CNR3_CACHE_MAX_HARD_CEILING       = 1000
    Upper bound on active_ceiling.

// --- Checkpoints ---

CNR3_CHECKPOINT_INTERVAL          = 10
    Promote every 10th frame to checkpoint pool.

CNR3_CHECKPOINT_MAX_RETAIN        = 32       [INCREASED from 16]
    Soft trigger threshold; checkpoints in hot zones retained regardless.

CNR3_CHECKPOINT_MIN_RETAIN        = 10       [INCREASED from 6]
    Prune back to this count when eligible candidates exist.

// --- Hot zones ---

CNR3_HOT_ZONE_FORWARD_RADIUS      = 10
    Frames ahead of hot zone high boundary protected from pruning.
    Supported by simulation: covers p99 BestSource jitter to ~jitter_max=8.

CNR3_HOT_ZONE_BACK_RADIUS         = 50       [INCREASED from 30]
    Frames behind hot zone low boundary protected from pruning.
    Covers 5 checkpoint intervals of backward history.
    Subject to empirical tuning after Phase CMS02-E.

CNR3_MAX_HOT_ZONES                = 5
    Maximum simultaneous active hot zones.
    Covers fmParallelRequests with up to 5 concurrent jump zones.

CNR3_HOT_ZONE_JUMP_THRESHOLD      = CNR3_HOT_ZONE_FORWARD_RADIUS +
                                    CNR3_HOT_ZONE_BACK_RADIUS + 1  (= 61)
    A new frame request allocates a new hot zone if it falls outside all
    existing zones by more than this threshold. Within this distance,
    the nearest zone is slid (fmUnordered) or extended (fmParallelRequests).
```

**Removed constant:** `CNR3_HOT_ZONE_EXTENSION_MARGIN` — no longer
needed. fmUnordered uses pure sliding (deterministic recompute of zone
bounds from F); fmParallelRequests uses extend-only outward (the zone
already covers F when F is inside, so no margin check is needed).

---

## 6. Diagnostics — Definitive Counter Specification

**[Carried over from CMS02 Section 6, with additions per CMS03 review
items 11 and corresponding new behaviours.]**

All counters are `int64_t`. Placement (in `Cnr3CacheManagerStats` or a
new struct) is an implementation choice at coding time.

**Hot zone counters:**

- `hot_zone_allocations`
- `hot_zone_extensions` (fmParallelRequests extend-outward)
- `hot_zone_slides` (fmUnordered slide events) — NEW CMS03
- `hot_zone_hits` (request inside existing zone, no slide/extend needed)
- `hot_zone_merges`
- `hot_zone_retirements`
- `hot_zone_new_zone_requests`
- `hot_zone_max_active_observed`
- `hot_zone_updates_at_arInitial` — NEW CMS03

**Pruning counters (additions to existing):**

- `non_checkpoint_prune_skipped_in_hot_zone`
- `non_checkpoint_prune_skipped_pinned`
  (RESERVED FOR PHASE CMS02-I if non-checkpoint pinning is promoted)
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

- `cache_hits_at_arAllFramesReady` — NEW CMS03
- `cache_misses` (existing)
- `recoveries_started_at_arAllFramesReady` — NEW CMS03
- `predecessor_missing_when_expected` *** CRITICAL — see Section 4.4 ***
- `checkpoint_missing_when_expected`
- `max_live_cached_frames_observed`
- `max_non_checkpoint_pool_size_observed`
- `max_checkpoint_pool_size_observed`

**Debug output policy:**

- Default: headline counters at `cnr3_free` time.
- Development diagnostics enabled: all counters plus hot zone state.
- Per-event verbose output guarded behind
  `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`.
- **No diagnostic output to stdout under any circumstances.**
- `predecessor_missing_when_expected > 0` prints a prominent warning
  regardless of diagnostic level.

---

## 7. Worked Examples

**[Updated CMS03 for BACK_RADIUS=50 and sliding-zone behaviour.]**

### Example A — Linear Encoding, No Jumps (fmUnordered, sliding)

Setup: 32-thread encode, BestSource jitter up to 6 frames. Cache starts
empty.

| Arrival | Action | Zone 0 after action |
|---|---|---|
| F=0   | Allocate zone 0 | low=0, high=10 |
| F=1   | Inside zone, slide | low=0 (max(0,1-50)=0), high=11 |
| F=10  | Inside zone, slide | low=0, high=20 |
| F=50  | Inside zone, slide | low=0, high=60 |
| F=51  | Inside zone, slide | low=1, high=61 (low starts moving) |
| F=100 | Inside zone, slide | low=50, high=110 |
| F=110 | Inside zone, slide | low=60, high=120 |

By F=110, the non_checkpoint_pool holds ~110 frames. Overflow triggered
(110 > 110 * 1.1 = 121 — not yet, but at F=121 it will be).

At F=121, zone is `low=71, high=131`. Pool exceeds overflow. Prune:
candidates are frames outside `[71, 131]` with `pin_count == 0`. Frames
0–70 are candidates. Evict furthest-from-zone first (F=0, then F=1,
etc.) until pool size returns to 100.

Predecessor for current computation is always within zone (e.g., F=120
needs output[119], well inside the zone). No predecessor failures.

### Example B — Small Forward Jump Within JUMP_THRESHOLD

Setup: at F=80, zone 0 = `low=30, high=90`.

F=95 arrives. Distance from zone high (90): 5 frames. 5 ≤
JUMP_THRESHOLD (61). Within threshold.

**fmUnordered:** Slide zone 0:
`low = max(0, 95-50) = 45`, `high = 95+10 = 105`.
Single deterministic recompute of bounds.

**fmParallelRequests:** Extend zone 0 outward:
`low = min(30, max(0, 95-50)) = min(30, 45) = 30`,
`high = max(90, 95+10) = 105`.
Zone now `[30, 105]` — both old and new ranges covered.

Cache lookup: output[94] may be present. If yes, cache hit, return
immediately. If no, nearest checkpoint (say checkpoint[90]) found and
pinned, fill-holes-only walk fills 91..95, checkpoint unpinned.

### Example C — Large Forward Jump (fmParallelRequests)

Setup: in fmParallelRequests, sequential encoding at F=200 with three
in-flight requests (200, 201, 202). Zone 0 = `low=150, high=210`.

F=600 arrives. Distance from zone 0 high (210): 390 frames. 390 >
JUMP_THRESHOLD (61). Jump detected.

Allocate zone 1: `low=550, high=610`.

Zone 0 remains active (in-flight requests 200, 201, 202 are within
[150, 210]). It cannot be retired until those complete and the pinned-
checkpoint check (if any are pinned within its range) clears.

Bounded recovery for F=600: no checkpoint above 200 exists.
`start = max(0, 600-100) = 500`.
Recovery walk uses fill-holes-only: walk output[500..600] computing each
frame from its predecessor.

For each predecessor look-up in the walk, the helper takes
`addFrameRef`. Even if a concurrent prune (triggered by another
request's store) evicts output[K-1] between when the walk grabbed it and
when it finishes computing output[K], the walk's `addFrameRef` keeps
the VSFrame alive.

Frames 500–549 are stored but outside both zones (zone 0 covers
150–210, zone 1 covers 550–610). They are prunable. The walk has
already used each as a predecessor by the time it stores the next, so
prune evicting them does not corrupt the in-progress walk.

Pre-jump frames 198, 199, 200 complete. Zone 0 has no live frames
remaining in [150, 210] (they were pruned as zone 1 grew). Zone 0
becomes eligible for lazy retirement next time a slot is needed.

Steady state: one active zone around F=600. Cache holds ~100 frames in
the 550–610 range plus retained checkpoints.

### Example D — No-Prior-Checkpoint Recovery (fmUnordered, cold seek)

Setup: fresh instance, user seeks to F=800. Cache empty. No checkpoints.

F=800 arrives. No active zones. Allocate zone 0: `low=750, high=810`.

`find_and_pin_nearest_prior_checkpoint` returns false. Increment
`no_prior_checkpoint_recovery_count`.

Bounded warm-up:
- `start = max(0, 800-100) = 700`
- output[700] = source-copy initialisation (no predecessor — approximation)
- For K in 701..800: compute from output[K-1] (held via `addFrameRef`),
  store. Each store may trigger prune.
- Checkpoints promoted at 700, 710, …, 800.

After recovery: prune candidates are frames 700..749 (below zone low=750)
with `pin_count == 0`. Evicted furthest-first. Pool settles to ~50
frames.

Checkpoints 700..800 retained by checkpoint pruning rules
(MIN_RETAIN=10, hot zone protection for the few inside the zone).

Subsequent seeks to 700..800 range use checkpoints as anchors.
Cold-start approximation is confined to output[700].

### Example E — Hard Ceiling Abort

Setup: 16-bit 4:2:2 input. `estimated_frame_bytes ≈ 1.66 MB`.
`active_ceiling = 512 MiB / 1.66 MB ≈ 323` (clamped within [150, 1000]).

User makes 5 rapid large seeks before any recovery completes. Five
concurrent recovery walks each generating ~101 frames = ~505 total
demand. Ceiling = 323.

After ~323 stores, next store: ceiling check fires. Prune: no
candidates (all frames within one of the 5 active hot zones).

Store returns false. Increment `cache_ceiling_hard_aborts`. Cleanup
discipline (Section 4.6.3): unpin any pinned checkpoint for this walk,
free any held source/dst frame references, free any caller-owned cache
references.

`getFrame` returns VS error: *"CNR3: cache ceiling reached (323 frames).
CNR3 is designed for near-linear access…"*

Filter remains valid. Subsequent near-linear requests succeed.

### Example F — Fill-Holes-Only Avoiding Redundant Compute

Setup: F=150 was computed and stored. Concurrent request for F=155
triggers a fill-holes-only walk from checkpoint[150] for 151..155.

At step K=150 in the walk: lookup output[150] in cache (via
`find_output_frame_and_add_ref`). Found. Use as predecessor for
output[151]. Increment `recovery_frames_skipped_already_cached`. Walk
continues without recomputing 150. After consuming output[150] for the
computation of output[151], `freeFrame` the reference.

In a scenario where multiple requests overlap recovery ranges, multiple
frames are skipped.

---

## 8. Phased Implementation Sequence

**[CHANGED CMS03 per review item 8 — phases renamed from CMS01-* to
CMS02-* to match the spec they came from.]**

#### Phase CMS02-A — Documentation and constants only

- Mark CMS03 as the current Phase 4 cache-policy direction.
- Add all new constants (Section 5) to `cnr3_cache_manager.h`.
- Add all new diagnostic counter declarations (Section 6).
- Add `active_ceiling` field to `Cnr3CacheManagerV005`.
- Implement byte-budget ceiling computation in
  `cnr3_cache_manager_set_ceiling()`.
- Add `Cnr3HotZone` struct declaration.
- Do not change runtime behaviour.

#### Phase CMS02-B — Hot zone structures and passive diagnostics

- Add hot zone array to `Cnr3CacheManagerV005`.
- Add hot zone helper declarations.
- Add passive debug snapshot fields for hot zone state.
- Do not use hot zones for pruning yet.

#### Phase CMS02-C — Hot zone update helpers

- Implement `cnr3_cache_manager_update_hot_zones()` with mode-specific
  logic per Section 4.2 (sliding for fmUnordered, extend-only for
  fmParallelRequests). For Phase CMS02-C, only the fmUnordered path
  is wired; fmParallelRequests is implemented but not wired.
- Implement `cnr3_cache_manager_is_frame_in_hot_zone_externally_locked()`.
- Implement `cnr3_cache_manager_retire_cold_hot_zones_externally_locked()`
  with fmUnordered eager-retirement policy.
- Instrument all zone lifecycle events.

#### Phase CMS02-D — Hot zone aware prune candidate selection

- Replace `prune_non_checkpoint_pool_externally_locked` inner loop with
  hot-zone-aware candidate selection (Section 4.3.1 Phase A — without
  pin_count check, since non-checkpoint pinning is deferred).
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
- Prove: `addFrameRef`/`freeFrame` balance, pool sizes, prune behaviour,
  hot zone allocation/retirement, no stdout output, ceiling counters
  zero during normal linear encode.

#### Phase CMS02-F — Cache-hit reuse under fmUnordered

- Implement `cnr3_cache_manager_find_output_frame_and_add_ref()` per
  Section 2.1 and Section 9.2.H.
- At the start of `cnr3_get_frame` arAllFramesReady, check v005 cache
  hit. If hit: return cached frame via caller-owned ref. If miss:
  proceed with normal computation.
- Instrument `cache_hits_at_arAllFramesReady`.

#### Phase CMS02-G — Checkpoint recovery and hole filling under fmUnordered

- `find_and_pin_nearest_prior_checkpoint` for out-of-order requests.
- Fill missing frames ascending using fill-holes-only rule.
- Skip frames already present in cache (using
  `find_output_frame_and_add_ref`).
- Store newly generated frames; `prune_after_store`.
- Unpin checkpoint on every exit path.
- Instrument `holes_filled`,
  `recovery_frames_skipped_already_cached`,
  `nearest_checkpoint_recovery_count`,
  `predecessor_missing_when_expected`.

#### Phase CMS02-H — Bounded warm-up recovery under fmUnordered

- Implement no-prior-checkpoint warm-up per Section 4.5.
- Instrument `bounded_warmup_recovery_count`, start frame, length,
  frames computed vs. skipped.

#### Phase CMS02-I — Empirical review: non-checkpoint pinning decision

After realistic VHS/VHS-C encode tests and synthetic jump tests:

- Inspect all Section 6 counters, especially
  `predecessor_missing_when_expected`.
- If mandatory promotion criteria (Section 4.4) are met: implement
  non-checkpoint pinning before proceeding.
- If criteria are not met: document findings and proceed to CMS02-J.

#### Phase CMS02-J — fmParallelRequests wiring and proving

Only after CMS02-H proven under fmUnordered and CMS02-I decision made.

- Wire fmParallelRequests path through hot-zone update (already
  implemented in CMS02-C using extend-only logic).
- Test concurrent jump scenarios.
- Validate retirement policy with pinned-checkpoint proxy.
- Decide on `active_request_count` per zone if retirement proves too
  conservative.

#### Full fmParallel — explicitly out of scope for this iteration.

---

## 9. Structural Changes Required to Uploaded Code

### 9.1 `cnr3_cache_manager.h` — Structure changes

**A. Add `Cnr3HotZone` struct:**

```cpp
struct Cnr3HotZone {
    bool active = false;
    int low = -1;
    int high = -1;
    int last_observed_frame = -1;
    // diagnostic counters (int64_t):
    //   hit_count, extension_count, slide_count, merge_count,
    //   retirement_count, prune_protection_count
};
```

**B. Add hot zone array to `Cnr3CacheManagerV005`:**

A fixed-size array of `Cnr3HotZone`, size `CNR3_MAX_HOT_ZONES`. All
elements initialised inactive. Mutable cache-manager state — access
only while holding `cache_mutex`.

**C. Add `active_ceiling` to `Cnr3CacheManagerV005`:** `int` field.
Set once at `cnr3_create()` time via byte-budget computation (Section
4.6.1). Not changed thereafter.

**D. Add new constants** per Section 5. Remove obsolete frame-count
ceiling constants (`CNR3_OUTPUT_CACHE_CEILING_8BIT`,
`CNR3_OUTPUT_CACHE_CEILING_16BIT`) and obsolete
`CNR3_HOT_ZONE_EXTENSION_MARGIN`.

**E. Add new statistics counters** per Section 6.

**NOTE:** Non-checkpoint pinning structural change (changing
`non_checkpoint_pool` value type to `Cnr3NonCheckpointSlot`) is
**deferred to Phase CMS02-I**. Do not change the pool type now.

### 9.2 `cnr3_cache_manager.h` — New helper declarations

**F. Hot zone helpers:**

- `cnr3_cache_manager_update_hot_zones(cache, frame_number, mode)` —
  public, locks. `mode` selects sliding (fmUnordered) vs. extend-only
  (fmParallelRequests) behaviour. The mode parameter is plumbed from
  `cnr3_get_frame` based on VapourSynth's request mode for the
  instance.

- `cnr3_cache_manager_is_frame_in_hot_zone_externally_locked(cache, frame_number)`
  — bool, caller holds mutex.

- `cnr3_cache_manager_retire_cold_hot_zones_externally_locked(cache, mode)`
  — caller holds mutex. fmUnordered uses eager retirement;
  fmParallelRequests uses lazy retirement with pinned-checkpoint proxy.

- `cnr3_cache_manager_set_ceiling(cache, vi)` — public, called once at
  `cnr3_create`. Uses VapourSynth video info for byte-budget
  computation.

**G. Ceiling check:**

- `cnr3_cache_manager_would_exceed_ceiling_externally_locked(cache)` —
  bool.

**H. Cache hit lookup (Section 2.1, Section 4.7):**

- `cnr3_cache_manager_find_output_frame_and_add_ref(cache, frame_number, vsapi)`
  — public, locks internally. Returns caller-owned `VSFrame*` (with
  `addFrameRef` taken inside, under mutex), or `nullptr` if not found.
  Caller must `freeFrame` on every exit path.

### 9.3 `cnr3_cache_manager.cpp` — Logic changes

**I. `prune_non_checkpoint_pool_externally_locked`:** Replace
`while { evict begin() }` with hot-zone-aware candidate selection per
Section 4.3.1 Phase A. Call `retire_cold_hot_zones` first.

**J. `prune_checkpoint_pool_externally_locked`:** Add hot-zone
candidate filtering before existing frame-zero and pin_count checks
(Section 4.3.2).

**K. `store_output_frame` (public):** Before `addFrameRef` and pool
insertion, call `would_exceed_ceiling`. If ceiling would be exceeded
and no prune candidates exist, increment `cache_ceiling_hard_aborts`
and return false. Caller (the failed `cnr3_get_frame` path) executes
cleanup discipline (4.6.3) before returning a VS error.

**L. `validate_invariants_externally_locked`:** Add:

- All hot zone slots with `active==true` have `low >= 0`,
  `high >= low`, `last_observed_frame >= 0`.
- `last_observed_frame ∈ [low, high]` is **not** required because a
  zone may legitimately briefly contain a `last_observed_frame` at the
  exact boundary or have been slid such that the boundary semantics
  hold without requiring boundary inclusion.
- Total live frames ≤ `active_ceiling`.

No functional change to existing frame ownership checks.

### 9.4 `vapoursynth-Cnr3.cpp` — Wiring changes (future phases)

**M. `cnr3_create()`:**

After `Cnr3Data` construction:

```cpp
cnr3_cache_manager_set_ceiling(d->cache_manager_v005, d->vi);
```

**N. `cnr3_get_frame()` arInitial:**

```cpp
cnr3_cache_manager_update_hot_zones(cache, frame_number, request_mode);
```

Called for every arriving frame request before any cache lookup.

**O. Debug output at `cnr3_free`:**

Add hot zone state summary and new Section 6 counters to existing
debug summary. `predecessor_missing_when_expected > 0` prints a
prominent warning regardless of diagnostic level.

### 9.5 Failure-Path Discipline (cross-cutting)

**[ADDED CMS03 per review item 9.]**

Every failure path that returns a VapourSynth error must execute
cleanup before returning:

1. Any checkpoint pinned during this operation: `unpin_checkpoint` once
   per `pin_and_find` call.
2. Any caller-owned VSFrame references obtained via
   `find_output_frame_and_add_ref` or
   `find_and_pin_nearest_prior_checkpoint`'s returned-reference path:
   `freeFrame` once.
3. Any source frame references obtained from VapourSynth:
   `freeFrame`.
4. Any destination frame allocated but not being returned as output:
   `freeFrame`.
5. Hot zone state: no rollback needed. A zone allocated for a failed
   request is not invalid — it will be retired naturally when its
   range becomes cold. Do not retroactively undo zone updates.
6. Diagnostic counters: still incremented as appropriate
   (`cache_ceiling_hard_aborts`, `predecessor_missing_when_expected`,
   etc.).

The handover document already specifies the unpin-exactly-once rule
for checkpoint paths. CMS03 extends it to all caller-owned references.

---

## 10. Known Hazard Addressed by Hot-Zone Lifecycle Rules

**[RENAMED CMS03 from "Rejected/Handle-Carefully" per review item 10.
Same content, neutral framing.]**

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

**Resolution by CMS03 design:**

This scenario is the canonical test case for hot-zone lifecycle
correctness. The design closes it as follows:

- **Step 5 prevention:** In fmUnordered, request A's walk has completed
  before any jump arInitial fires, so its predecessor needs do not
  persist. In fmParallelRequests, the zone covering request A's range
  may extend (not slide) and may not be retired while any in-flight
  request is within its range (pinned-checkpoint proxy in CMS02-J;
  `active_request_count` if proxy proves insufficient).

- **Step 6 prevention (in fmUnordered):** Even if the zone slides past
  output[449], request A's walk holds a caller-owned `addFrameRef`
  reference on output[449] for the duration of computing output[450]
  (Section 2.1, Section 4.7). Prune may remove the cache slot, but the
  walk's reference keeps the underlying VSFrame alive.

- **Step 6 prevention (in fmParallelRequests):** Same `addFrameRef`
  protection. Additionally, extend-only zone behaviour means the zone
  covering output[449] does not contract while in use.

- **Runtime verification:** `predecessor_missing_when_expected` is the
  empirical detector. If it ever increments, the design has failed
  somewhere and non-checkpoint pinning becomes mandatory before
  further work (Section 4.4).

The hazard is acknowledged, addressed by mode-specific lifecycle rules
plus `addFrameRef` discipline, and continuously monitored at runtime.

---

## 11. Items to Confirm Empirically

**[RENAMED CMS03 from "Open Questions Requiring Decision" — OQ1
through OQ5 are now resolved. This section contains the residual
empirical items to confirm during Phases CMS02-E onward.]**

**EI1 — Conservative retirement proxy under fmParallelRequests.**

Resolution: accept the pinned-checkpoint proxy for first
fmParallelRequests implementation. Spurious retention is safe;
spurious retirement could cause FM1.

To confirm empirically (Phase CMS02-J): if the proxy proves too
conservative (zones never retire, slots fill, merges happen too
frequently), add `active_request_count` per zone — incremented at
`arInitial`, decremented at `arAllFramesReady` / `arError` — for exact
retirement eligibility.

**EI2 — `BACK_RADIUS = 50` empirical sufficiency.**

Resolution: starting value 50. Covers 5 checkpoint intervals.

To confirm empirically (Phase CMS02-E onward): observe
`recovery_frames_computed` and `cache_hits_at_arAllFramesReady`.
If hot zone proves too narrow (frequent recomputation of recently-
computed frames near the boundary), increase to 75 or 100. If too
wide (cache stays near ceiling under normal linear encode), decrease.

**EI3 — Checkpoint retain values `MAX=32`, `MIN=10`.**

Resolution: starting values 32/10.

To confirm empirically: observe `max_checkpoint_pool_size_observed`.
If checkpoints frequently hit the soft trigger (32) under normal
linear encoding, the values may need increasing. If checkpoints rarely
exceed `MIN_RETAIN`, the soft trigger value can be lowered.

**EI4 — Ceiling abort frequency under realistic workloads.**

Resolution: byte-budget based, clamped to [150, 1000].

To confirm empirically: `cache_ceiling_hard_aborts` should be zero
during normal linear encoding. If it increments at all during realistic
VHS encode tests, the byte budget or the maximum-zones constant needs
review.

**EI5 — `prune_no_candidate_exists` behaviour.**

Resolution: accept "do not evict protected frames" initially. The
ceiling and hard abort are the safety net.

To confirm empirically: if `prune_no_candidate_exists` increments
significantly during realistic tests, reduce `BACK_RADIUS` or
`MAX_HOT_ZONES`, or add a fallback eviction policy. Recommended only
after Phase CMS02-E memory data is available.

**EI6 — Non-checkpoint pinning promotion decision (Phase CMS02-I).**

Resolution: deferred. Mandatory promotion criteria are in Section 4.4.

To confirm empirically: at the end of Phase CMS02-H, inspect
`predecessor_missing_when_expected` and related counters. Decide.
This is the most important empirical decision in the implementation
sequence.
