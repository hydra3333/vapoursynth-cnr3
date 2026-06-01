# CNR3 Cache Manager — Revised Design Specification
## Jump-Safe Hot-Zone Pruning with Non-Checkpoint Pinning

**Date:** 2026-06-01  
**Status:** Design specification — pre-implementation  
**Supersedes:** Bounded-recovery policy described in handover snapshot v0.14 section 0.9A  
**Companion documents:** CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)

---

## 1. Problem Statement

CNR3 is a recursive temporal chroma stabiliser. Every output frame depends on
the previous output frame as a predecessor. The cache manager must retain enough
predecessor frames to satisfy any in-flight computation, while pruning old frames
to keep memory bounded.

The existing v005 cache manager infrastructure (pools, checkpoint pinning,
mutex model, store/remove/validate helpers) is sound. What is not yet designed
is the pruning policy and the structural protections needed to make that policy
safe under three VapourSynth execution modes:

- **fmUnordered** — one request in flight at a time, mostly sequential
- **fmParallelRequests** — multiple concurrent requests, serialised writer
- **fmParallel** — fully concurrent readers and writers

The primary failure modes without the new design are:

**FM1 — Prune destroys in-flight predecessor.**  
Thread A is computing output[450] and needs output[449] as its predecessor.
The prune logic evicts output[449] (lowest frame number, currently unpinned)
because a forward jump has pushed new frames into the cache. Thread A now has
a missing predecessor. Result: corrupt output or crash.

**FM2 — Prune destroys a checkpoint needed by a recovery chain.**  
A recovery walk pins checkpoint[400] and fills frames 401..500. A concurrent
prune evicts checkpoint[380] which another concurrent request needed as its
anchor. That request falls back to the expensive no-prior-checkpoint path
unnecessarily.

**FM3 — Jump recovery burst exceeds pool capacity.**  
A forward jump to frame N triggers bounded warm-up recovery generating up to
CNR3_OUTPUT_CACHE_CAPACITY new frames. Simultaneously, pre-jump in-flight
requests hold frames in the pool. The combined total exceeds the overflow limit,
stores fail, and the filter aborts unnecessarily.

**FM4 — Prune eviction key is wrong.**  
Current pruning evicts the lowest frame number first. After a forward jump, the
lowest frame numbers are exactly the pre-jump in-flight frames most critically
needed. Pruning them first is the worst possible choice.

---

## 2. Design Goals

1. Guarantee that no frame needed by any active computation is ever pruned.
2. Make pruning decisions based on frame-number proximity to active work,
   not on insertion order or frame-number magnitude alone.
3. Support up to CNR3_MAX_HOT_ZONES simultaneous active working ranges
   (covering concurrent pre-jump and post-jump activity).
4. Bound memory use with a hard ceiling derived from bit depth.
5. Hard-abort cleanly when the ceiling is hit with nothing prunable, with a
   clear user-facing error message.
6. Remain compatible with the existing mutex model, pool structures, and
   externally-locked helper pattern.
7. Require no changes to the VapourSynth API interaction model.

---

## 3. Simulation Results — Linear Encoding Jitter

A Monte Carlo simulation (200 runs, 100 frames, varied jitter) was run to
characterise realistic frame arrival patterns for the primary linear encoding
use case (vspipe / ffmpeg pipe with BestSource source plugin).

Model: each frame is dispatched at time = frame_number + uniform_random(0, jitter_max).
Frames arrive at cnr3_get_frame in delivery-time order, producing a jittered
but near-sequential arrival stream.

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

The p99 max gap tracks approximately jitter_max + 3 across the range tested.
For observed BestSource jitter of 4-6 frames, the worst-case reorder window
needed is approximately 9 frames at p99. A forward hot zone radius of 10 frames
covers this with headroom.

The "max predecessor distance" (how many positions late a predecessor can arrive
relative to the frame that needs it) follows the same pattern: max 8 at jitter=6,
max 15 at jitter=12.

**Implication for hot zone forward radius:**  
CNR3_HOT_ZONE_FORWARD_RADIUS = 10 is well-supported by the simulation data
for realistic BestSource jitter up to approximately jitter=8. Increase if
empirically higher jitter is observed.

**Implication for reorder buffer:**  
The existing CNR3_REORDER_WINDOW = 32 is more than adequate for linear encoding
jitter up to approximately jitter_max=24 at p99. It is not threatened by
realistic BestSource jitter and does not need to change.

**Hot zone protection rate in linear encoding:**  
With back=50, forward=10, the hot zone covers the predecessor for approximately
83% of frame arrivals under jitter=6. The remaining 17% are cases where the
predecessor is in the reorder buffer awaiting delivery, not at risk of pruning.
Hot zone protection is not the critical mechanism for linear encoding — it
becomes critical only under jump scenarios.

---

## 4. Algorithm Overview

### 4.1 Hot Zone Tracking

The cache manager maintains a small fixed-size array of **hot zones**. Each hot
zone represents a contiguous range of frame numbers that is currently "active"
— meaning one or more in-flight computations are working within that range.

A hot zone has:
- A **centre** frame number (updated as requests arrive)
- A **low boundary** = centre - CNR3_HOT_ZONE_BACK_RADIUS
- A **high boundary** = centre + CNR3_HOT_ZONE_FORWARD_RADIUS
- An **active** flag

Hot zones are updated under the cache mutex at arInitial time, when a frame
request arrives. Hot zones are not time-based — they are frame-number-based.

**Why hot zones, not a single watermark:**  
A single "lowest protected frame number" watermark cannot represent two
simultaneous active ranges (pre-jump and post-jump). After a forward jump,
pre-jump in-flight requests cluster around frame numbers in the low hundreds
while post-jump recovery clusters around frame numbers in the high hundreds.
A watermark set to protect the lower range allows pruning of the upper range
and vice versa. Discrete hot zones represent both ranges independently.

**Why not a rolling median:**  
A single rolling median of all recent arrivals smears across both active ranges,
producing a meaningless midpoint. For example, with pre-jump activity at frames
380-420 and post-jump activity at frames 780-820, the median would be ~600 —
protecting a dead zone while potentially exposing the edges of both active
ranges to pruning. Discrete zones avoid this.

### 4.2 Hot Zone Lifecycle

**Zone allocation (at arInitial):**  
When a frame request arrives with frame number F:

1. Check whether F falls within the range [low, high] of any existing active
   hot zone. If yes, update that zone's centre toward F (shift by a small
   fraction, or simply record F as the new observed maximum in the zone) and
   extend the zone boundaries if F is near the edge. No new zone needed.

2. If F is outside all existing active zones by more than
   CNR3_HOT_ZONE_JUMP_THRESHOLD (see constants), allocate a new hot zone
   centred on F.

3. If no zone slot is free and a new zone is needed, merge the two
   nearest existing zones (combine their ranges) to free a slot, then
   allocate the new zone. Alternatively, evict the zone whose high boundary
   is furthest below the current minimum live frame number in the cache
   (i.e. the zone whose range has been entirely superseded by pruning).

**Zone retirement:**  
A hot zone is retired (marked inactive) when:
- Its entire frame range has been pruned from the cache (no live frames
  remain within [low, high]), AND
- No in-flight request is known to be within that range.

In practice, zone retirement can be deferred — an inactive zone that protects
no live frames costs nothing because prune will find no eviction candidates
within it anyway. Zones can be retired lazily at the point a new zone needs
to be allocated and no free slot exists.

### 4.3 Pruning Policy — Hot Zone Aware

The revised pruning policy replaces the current "evict lowest frame number
first" rule with:

**Non-checkpoint pool pruning:**

1. Collect all non-checkpoint frames whose frame number falls outside every
   active hot zone's [low, high] range AND whose pin_count is zero
   (see SP1 below). These are candidates for eviction.

2. Among candidates, sort by distance from the nearest hot zone boundary,
   descending. Evict the furthest-from-any-hot-zone candidate first.

3. Continue evicting candidates until non_checkpoint_pool.size() is back
   at or below CNR3_OUTPUT_CACHE_CAPACITY, or no more candidates exist.

4. If no candidates exist (everything is pinned or within a hot zone) and
   the pool exceeds the overflow limit, do not evict. The pool temporarily
   exceeds the soft target. This is acceptable up to the hard ceiling.

**Checkpoint pool pruning:**

Apply the same hot zone awareness to checkpoint eviction candidates:

1. A checkpoint is a candidate for eviction if:
   - Its frame number is not zero (frame 0 is never pruned), AND
   - Its pin_count is zero, AND
   - Its frame number falls outside every active hot zone's [low, high] range.

2. Among candidates, evict the one furthest from any hot zone boundary first,
   same as non-checkpoint pruning.

3. Continue until checkpoint_pool.size() is back at or below
   CNR3_CHECKPOINT_MIN_RETAIN, or no more candidates exist.

4. Checkpoints within hot zones or pinned are retained regardless of
   CNR3_CHECKPOINT_MAX_RETAIN. The retain limits apply only to eligible
   candidates. This means CNR3_CHECKPOINT_MAX_RETAIN is a soft trigger
   threshold, not a hard eviction cap.

**Rationale for checkpoint retention:**  
Checkpoints exist to anchor recovery chains. A checkpoint inside or near an
active hot zone may be needed as a recovery anchor for in-flight or soon-to-
arrive requests within that zone. Evicting it forces a more expensive no-prior-
checkpoint warm-up recovery. Retaining checkpoints longer than non-checkpoint
frames is cheap — at CNR3_CHECKPOINT_INTERVAL=10, 50 checkpoints costs 50
VSFrame references, approximately 30MB at 4:2:0 8-bit 720x576.

### 4.4 Non-Checkpoint Frame Pinning (SP1)

The existing checkpoint_pool uses per-slot pin_count to protect checkpoints
from pruning while an in-flight recovery chain depends on them. The
non_checkpoint_pool has no equivalent protection. This is the root cause of FM1.

The fix: non_checkpoint_pool entries gain a pin_count field, identical in
semantics to checkpoint pin_count. Any frame in non_checkpoint_pool that is
currently serving as a predecessor in an active computation must be pinned
before that computation begins and unpinned on every exit path (success,
error, early return).

Pinning a non-checkpoint frame: caller increments pin_count under the mutex
via a new helper cnr3_cache_manager_pin_non_checkpoint() before beginning any
computation that depends on that frame as a predecessor.

Unpinning: caller calls cnr3_cache_manager_unpin_non_checkpoint() on every
exit path, same discipline as checkpoint unpinning.

Prune skips any non-checkpoint frame with pin_count > 0, same rule as
checkpoints.

This closes FM1 completely. A frame that is actively being used as a
predecessor cannot be evicted regardless of its position relative to hot zones.

### 4.5 Bounded Warm-Up Recovery (No-Prior-Checkpoint Case)

When a request arrives for frame N and no cached output[N-1] exists and no
usable nearest-prior checkpoint exists, the filter must warm-start from an
earlier frame. The policy:

    start = max(0, N - CNR3_OUTPUT_CACHE_CAPACITY)
    request source[start .. N]
    initialise recovery chain at start using source-copy semantics
    (output[start] = source[start], no predecessor blend)
    compute output[start+1] through output[N] recursively

This requests at most CNR3_OUTPUT_CACHE_CAPACITY + 1 source frames (at most
101 with current defaults). This is deliberate approximation — output[start]
is not mathematically identical to the true filtered output[start] because
no predecessor is available. The chroma stabilisation algorithm is expected
to settle within a bounded number of frames. This trade-off is explicit and
documented.

The bounded recovery window must allocate a hot zone centred on N with its
full back and forward radii before beginning the recovery store loop. This
protects all recovery-generated frames from concurrent pruning during the
recovery walk.

### 4.6 Hard Ceiling and Abort Policy

The soft target (CNR3_OUTPUT_CACHE_CAPACITY) is the level pruning aims for
under normal operation. The overflow limit (capacity × overflow factor) is
the point at which prune is triggered after a store. The hard ceiling is the
absolute maximum live VSFrame references the cache will hold.

If a store would cause total live VSFrame references (non_checkpoint_pool.size()
+ checkpoint_pool.size()) to exceed the hard ceiling, AND prune cannot free
any frame (everything within hot zones or pinned), the store returns false and
the calling getFrame path returns a VapourSynth filter error for that frame.

The error message must clearly state that CNR3 is designed for near-linear
access and that large random seeks may exceed cache capacity. This is the
documented limitation of the filter, not a bug.

The hard ceiling is bit-depth-dependent and set at cnr3_create() time:
- 8-bit input:  CNR3_OUTPUT_CACHE_CEILING_8BIT  = 1000 frames (~608MB at 4:2:0)
- 16-bit input: CNR3_OUTPUT_CACHE_CEILING_16BIT = 500 frames  (~596MB at 4:2:0)

These ceilings provide comfortable headroom above normal operation (100-frame
soft target) while remaining within the memory budget of a 16-32GB system
running VapourSynth alongside other tools.

---

## 5. Constants Summary

The following constants replace or supplement those currently defined in
cnr3_cache_manager.h:

```
CNR3_OUTPUT_CACHE_CAPACITY        = 100
    Soft pruning target. Prune tries to keep non_checkpoint_pool at or below
    this size during normal operation. Unchanged from current.

CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Prune is triggered when non_checkpoint_pool exceeds
    capacity * overflow_factor (= 110 frames). Unchanged from current.

CNR3_OUTPUT_CACHE_CEILING_8BIT    = 1000
    Hard ceiling for 8-bit input. Abort if total live refs exceed this
    and nothing is prunable. NEW.

CNR3_OUTPUT_CACHE_CEILING_16BIT   = 500
    Hard ceiling for 16-bit input. NEW.

CNR3_CHECKPOINT_INTERVAL          = 10
    Promote every 10th frame to checkpoint pool. Unchanged.

CNR3_CHECKPOINT_MAX_RETAIN        = 16
    Soft trigger: checkpoint prune runs when checkpoint_pool exceeds this.
    Now soft only — checkpoints in hot zones are retained regardless.
    May need increasing; to be confirmed after runtime proving. Unchanged
    for now but flagged for review.

CNR3_CHECKPOINT_MIN_RETAIN        = 6
    Prune checkpoint pool back to this count when prune runs and eligible
    candidates exist. Unchanged for now; flagged for review.

CNR3_HOT_ZONE_FORWARD_RADIUS      = 10
    Frames ahead of hot zone centre protected from pruning.
    Supported by simulation: covers p99 BestSource jitter up to ~jitter_max=8.
    NEW.

CNR3_HOT_ZONE_BACK_RADIUS         = 30
    Frames behind hot zone centre protected from pruning.
    Covers 3 checkpoint intervals of backward history before needing
    recompute. Conservative starting point; increase if pruning is too
    aggressive in practice. NEW.

CNR3_MAX_HOT_ZONES                = 5
    Maximum simultaneous active hot zones.
    Covers: 1 linear encoding zone + up to 4 concurrent jump zones.
    NEW.

CNR3_HOT_ZONE_JUMP_THRESHOLD      = CNR3_HOT_ZONE_FORWARD_RADIUS +
                                     CNR3_HOT_ZONE_BACK_RADIUS + 1  (= 41)
    A new frame request is classified as a jump (new hot zone allocation)
    if it falls outside all existing hot zones by more than this threshold.
    This means normal jitter (within 10 frames ahead or 30 frames behind
    the zone centre) extends the existing zone rather than creating a new one.
    NEW.
```

---

## 6. Worked Examples

### Example A — Linear Encoding, No Jumps

Setup: 32-thread encode, BestSource jitter up to 6 frames. Frames arrive in
near-sequential order. Cache starts empty.

Frames 0..5 arrive in order 0,1,2,3,4,5. Each is computed and stored. After
frame 5 arrives, hot zone 0 is allocated: centre=3, low=max(0,3-30)=0,
high=3+10=13.

Frames 6..15 arrive slightly out of order (e.g. 7,6,9,8,11,10,12,13,14,15).
Each arrival updates hot zone 0's centre toward the arriving frame number.
Hot zone 0 expands: by frame 15, centre≈11, low=0, high=21.

No pruning occurs yet (pool size = 16, well below overflow = 110).

By frame 50, hot zone 0: centre≈45, low=15, high=55. Pool has ~50 frames.
Still below overflow. No pruning needed.

By frame 110, pool has 100+ frames, pruning triggers. Candidates: frames
outside hot zone [~85, ~115]. Frames 0..84 are candidates. Evict frames
0..10 (furthest from zone low boundary=85). Pool returns to 100. Frames
85..115 remain intact. Predecessor chain for any in-flight request is
protected because all needed predecessors are within the hot zone.

Result: smooth linear encoding, no predecessor failures, modest memory use.

---

### Example B — Small Forward Jump Within Hot Zone

Setup: encoding at frame 80. User seeks to frame 95. Jump size = 15 frames.
Hot zone 0: centre=80, low=50, high=90.

Frame 95 arrives. Is 95 outside all hot zones by more than 41 frames? No —
it is only 5 frames outside hot zone 0's high boundary of 90. So frame 95
extends hot zone 0: new high = 95+10 = 105, new centre shifts toward 95.

No new hot zone allocated. No special jump handling. Cache lookup: frame 94
may be in cache (if 95 was requested after a recent sequential run through
94). If not, nearest-prior checkpoint (say checkpoint[90]) is found and
pinned, recovery fills 91..95, checkpoint unpinned. Normal operation resumes.

Result: small jump handled transparently within existing hot zone. No policy
mode change.

---

### Example C — Large Forward Jump, Two Active Ranges

Setup: sequential encoding at frame 200. User jumps to frame 600. Jump size
= 400 frames. Pre-jump in-flight requests: frames 198, 199, 200 are still
computing when the jump fires.

**Step 1 — Jump detection at arInitial for frame 600:**
Frame 600 is outside all existing hot zones (hot zone 0: centre=198,
low=168, high=208) by 392 frames, which exceeds jump threshold of 41.
Allocate hot zone 1: centre=600, low=570, high=610.

**Step 2 — Pre-jump protection:**
Hot zone 0 still active: low=168, high=208. Frames 198, 199, 200 are within
this zone and cannot be pruned. Their non-checkpoint pin_counts are also
incremented by their active computations (SP1), giving double protection.

**Step 3 — Bounded warm-up recovery for frame 600:**
No cached output[599], no checkpoint above frame 200 (none have been stored
yet for the 200-600 gap).
start = max(0, 600 - 100) = 500.
Recovery requests source[500..600], computes output[500..600].
All 101 recovery frames stored into cache. Hot zone 1 (low=570, high=610)
protects frames 570-610 from pruning during the walk.
Frames 500-569 are outside hot zone 1 but also outside hot zone 0 — they
are prune candidates once the recovery walk completes.

**Step 4 — Pruning during and after recovery:**
After storing frame 510, prune triggers (pool > 110). Candidates: frames
outside all hot zones (outside [168,208] and outside [570,610]) with
pin_count=0. Frames 0-167 and 209-569 are candidates. Evict furthest first
(frame 0, frame 1, ... frame 167, then frame 209, 210, ...). Frames 168-208
(pre-jump hot zone) and 570-610 (jump hot zone) are untouched.

**Step 5 — Pre-jump in-flight completes:**
Frames 198, 199, 200 complete computation. Their pin_counts are decremented.
Hot zone 0 begins to go cold — no new requests arrive in the 168-208 range.
On next prune cycle, frames 168-208 become candidates. They are evicted
(furthest from hot zone 1's boundary first). Hot zone 0 is retired.

**Step 6 — Steady state at frame 600:**
Cache contains: checkpoint[600] (if 600 is a checkpoint multiple) and
frames 598-610. Hot zone 1 is now the only active zone. Normal operation
resumes from frame 600.

Result: clean isolation of pre-jump and post-jump ranges. No predecessor
failures. Memory peak during recovery: ~200-250 frames (well within ceiling).

---

### Example D — No-Prior-Checkpoint Recovery (Cold Seek)

Setup: fresh filter instance, user seeks immediately to frame 800. Cache
is empty. No checkpoints exist.

Frame 800 arrives. No hot zone exists yet. Allocate hot zone 0: centre=800,
low=770, high=810. No checkpoint found (checkpoint_find_and_pin returns
false). Bounded warm-up recovery:
start = max(0, 800 - 100) = 700.
output[700] initialised from source[700] with source-copy semantics
(no predecessor blend — this is the deliberate approximation).
output[701..800] computed recursively using each prior output as predecessor.
All 101 frames stored. Checkpoints promoted at 700, 710, 720, ... 800.

After recovery completes, prune: frames 700-770 are outside hot zone 0
(low=770), pin_count=0, eviction candidates. Frames 770-810 remain. Pool
settles to ~40 frames. Checkpoints 700-800 retained per checkpoint retain
rules.

Frame 801, 802, ... arrive normally. Hot zone 0 advances. Subsequent seeks
within the 700-800 range can use checkpoints as anchors rather than
cold-starting from frame 700 again.

Result: cold seek handled with bounded recompute cost (101 frames maximum),
settled cache, useful checkpoints for follow-up seeks.

---

### Example E — Hard Ceiling Abort

Setup: 16-bit 4:2:2 input (ceiling = 500 frames). User rapidly seeks:
frame 1000, frame 2000, frame 3000, frame 4000, frame 5000 before any
recovery completes. Five concurrent recovery walks each trying to store
101 frames = 505 frames total demand. Ceiling = 500.

After 500 stores, the next store attempt checks: total live refs = 500,
ceiling = 500, nothing prunable (all within hot zones). Store returns false.
The getFrame path for that frame returns a VapourSynth filter error:

"CNR3: cache ceiling reached (500 frames, 16-bit mode). CNR3 is designed
for near-linear access. Large random seeks in rapid succession exceed the
cache capacity. Reduce seek frequency or use a player that buffers seeks."

All other in-flight frames that can complete do so. The errored frame is
the only failure. The filter remains in a valid state.

Result: clean abort, informative error, filter remains usable for
subsequent near-linear access.

---

## 7. Structural Changes Required to Uploaded Code

This section describes what must change in the uploaded source files.
No new files are required. All changes are additions or modifications
to existing structures and helpers.

### 7.1 cnr3_cache_manager.h — Structure changes

**A. Add Cnr3NonCheckpointSlot:**
The non_checkpoint_pool currently maps int -> const VSFrame*.
It must be changed to map int -> Cnr3NonCheckpointSlot, where
Cnr3NonCheckpointSlot contains:
- frame: const VSFrame* (same as current map value)
- pin_count: int, initialised to 0

This mirrors the existing Cnr3CheckpointSlot structure exactly.
All existing code that reads non_checkpoint_pool entries by dereferencing
the map value must be updated to read .frame instead.

**B. Add Cnr3HotZone:**
A new struct Cnr3HotZone containing:
- centre: int, the current centre frame number of the zone (-1 = inactive)
- low: int, centre - CNR3_HOT_ZONE_BACK_RADIUS (clamped to 0)
- high: int, centre + CNR3_HOT_ZONE_FORWARD_RADIUS
- active: bool

**C. Add hot zone array to Cnr3CacheManagerV005:**
A fixed-size array of Cnr3HotZone, size CNR3_MAX_HOT_ZONES.
All elements initialised inactive.
This array is mutable cache-manager state and must be accessed only
while holding cache_mutex.

**D. Add active_ceiling to Cnr3CacheManagerV005:**
An int field storing the bit-depth-selected ceiling (1000 or 500).
Set once at cnr3_create() time via a new helper
cnr3_cache_manager_set_ceiling(cache, bits_per_sample).
Not changed after initialisation.

**E. Add new constants:**
CNR3_HOT_ZONE_FORWARD_RADIUS, CNR3_HOT_ZONE_BACK_RADIUS,
CNR3_MAX_HOT_ZONES, CNR3_HOT_ZONE_JUMP_THRESHOLD,
CNR3_OUTPUT_CACHE_CEILING_8BIT, CNR3_OUTPUT_CACHE_CEILING_16BIT.
Values as specified in Section 5.

**F. Add new statistics counters to Cnr3CacheManagerStats:**
- hot_zone_allocations: int64_t
- hot_zone_retirements: int64_t
- hot_zone_merges: int64_t
- non_checkpoint_prune_skipped_pinned: int64_t
  (mirrors checkpoint_prune_skipped_pinned)
- non_checkpoint_prune_skipped_in_hot_zone: int64_t
- checkpoint_prune_skipped_in_hot_zone: int64_t
- cache_ceiling_hard_aborts: int64_t

### 7.2 cnr3_cache_manager.h — New helper declarations

**G. Hot zone helpers (all lock internally unless _externally_locked):**
- cnr3_cache_manager_update_hot_zones(cache, frame_number)
  Called at arInitial for every arriving frame request.
  Finds the matching zone and updates it, or allocates a new zone,
  or merges zones if all slots are full.
  Returns void — hot zone update is best-effort; failure does not abort.

- cnr3_cache_manager_is_frame_in_hot_zone_externally_locked(cache, frame_number)
  Returns bool. Used internally by prune helpers to test eviction
  candidates. _externally_locked because prune helpers already hold
  the mutex.

- cnr3_cache_manager_retire_cold_hot_zones_externally_locked(cache)
  Marks inactive any hot zone whose entire [low, high] range has no
  live frames in either pool. Called at the start of each prune pass.

- cnr3_cache_manager_set_ceiling(cache, bits_per_sample)
  Sets cache.active_ceiling based on bits_per_sample.
  Called once in cnr3_create().

**H. Non-checkpoint pin helpers:**
- cnr3_cache_manager_pin_non_checkpoint(cache, frame_number)
  Increments pin_count on a non_checkpoint_pool entry. Locks internally.
  Returns false if frame not found or frame is in checkpoint_pool.

- cnr3_cache_manager_unpin_non_checkpoint(cache, frame_number)
  Decrements pin_count. Guards against underflow. Locks internally.
  Returns false if frame not found, not in non_checkpoint_pool, or
  pin_count already zero.

**I. Ceiling check helper:**
- cnr3_cache_manager_would_exceed_ceiling_externally_locked(cache)
  Returns bool. Checks whether total live refs (both pools combined)
  is at or above active_ceiling. Called inside store helper before
  attempting insertion. _externally_locked because store already holds
  the mutex.

### 7.3 cnr3_cache_manager.cpp — Logic changes

**J. prune_non_checkpoint_pool_externally_locked:**
Replace the current while(pool.size() > capacity) { evict begin() } loop
with the hot-zone-aware eviction algorithm described in Section 4.3:
1. Call retire_cold_hot_zones_externally_locked.
2. Collect eviction candidates (outside all hot zones, pin_count == 0).
3. Sort candidates by distance from nearest hot zone boundary, descending.
4. Evict in that order until pool.size() <= CNR3_OUTPUT_CACHE_CAPACITY
   or no candidates remain.

The sort over candidates is a new operation. For typical pool sizes
(100-200 frames) a simple linear scan to find the maximum-distance
candidate on each iteration is acceptable without a formal sort.

**K. prune_checkpoint_pool_externally_locked:**
Apply the same hot-zone candidate filtering before the existing
frame-zero and pin_count checks. A checkpoint inside a hot zone is
added to the skip list alongside frame-zero and pinned checkpoints.
Otherwise the existing oldest-first eviction order is preserved among
eligible candidates.

**L. remove_output_frame_externally_locked:**
Update to read .frame from Cnr3NonCheckpointSlot instead of the raw
map value. Add a check: if the non_checkpoint_pool entry has pin_count > 0,
return false and increment a new counter
cache_remove_pinned_non_checkpoint_rejections. This mirrors the existing
pinned checkpoint rejection behaviour.

**M. store_output_frame (public, locks internally):**
Before the existing addFrameRef and pool insertion logic, add a ceiling
check: if would_exceed_ceiling AND no prune candidates exist, increment
cache_ceiling_hard_aborts and return false immediately without storing.

**N. validate_invariants_externally_locked:**
Add validation for Cnr3NonCheckpointSlot:
- pin_count >= 0 for all non-checkpoint slots.
- frame pointer non-null for all non-checkpoint slots.
These mirror the existing checkpoint validation checks.

### 7.4 vapoursynth-Cnr3.cpp — Wiring changes (future phases)

These are not yet implemented but are called out here so the wiring
plan is coherent with the new structures:

**O. cnr3_create():**
After constructing Cnr3Data, call:
  cnr3_cache_manager_set_ceiling(d->cache_manager_v005, d->bits_per_sample)
This sets the bit-depth-appropriate hard ceiling before the first frame
request arrives.

**P. cnr3_get_frame() — arInitial stage:**
Before any cache lookup or request scheduling, call:
  cnr3_cache_manager_update_hot_zones(cache, frame_number)
This registers the arriving frame number and updates or allocates a hot zone.
This call must happen for every frame request, including cache hits.

**Q. cnr3_get_frame() — predecessor pinning:**
When a predecessor frame (output[N-1]) is found in non_checkpoint_pool
and will be used as input to the blend computation, call:
  cnr3_cache_manager_pin_non_checkpoint(cache, N-1)
before beginning the computation. Call:
  cnr3_cache_manager_unpin_non_checkpoint(cache, N-1)
on every exit path from that computation (success, error, early return).

**R. Debug summary in cnr3_free() / debug output paths:**
Add reporting of hot zone state (active zone count, zone ranges) and
new counters (ceiling aborts, hot zone allocations, non-checkpoint
prune skipped pinned/in-hot-zone) to the existing debug summary output.

---

## 8. What Does Not Change

The following aspects of the current implementation are sound and require
no modification:

- The mutex model and _externally_locked naming convention.
- The cache_index unordered_map structure and aliasing semantics.
- The checkpoint_pool map structure and Cnr3CheckpointSlot (beyond pin_count
  semantics which are already correct).
- The addFrameRef / freeFrame ownership rules.
- The find_and_pin_nearest_prior_checkpoint atomic helper.
- The validate_invariants framework (additions only in 7.3.N).
- The post-mutation validation compile-time switch pattern.
- The statistics reset and snapshot helpers.
- The prune_after_store combined entry point (internal logic changes in J/K,
  public interface unchanged).
- The highest_cached_frame_number tracking.
- The existing Cnr3CacheManager (old strict-streaming cache) — untouched.
- The VapourSynth API interaction model and fmUnordered runtime path.

---

## 9. Implementation Order Recommendation

These changes should be implemented in the following order to keep each
step independently verifiable:

1. SP1 first: Add Cnr3NonCheckpointSlot, update pool type, update all
   readers of non_checkpoint_pool, add pin/unpin helpers, update prune
   to skip pinned non-checkpoint frames, update validate_invariants.
   Verify: existing tests pass, new pin/unpin counters behave correctly,
   validation catches negative pin_count.

2. SP4 next: Add ceiling constants, add active_ceiling field, add
   set_ceiling helper, add ceiling check in store, add hard abort path.
   Verify: store fails cleanly when ceiling is simulated by setting a low
   ceiling in a test scenario.

3. SP2/SP3 last: Add Cnr3HotZone, add hot zone array to cache manager,
   implement update_hot_zones, implement is_frame_in_hot_zone, implement
   retire_cold_hot_zones, update prune_non_checkpoint to use hot zone
   candidate filtering, update prune_checkpoint to use hot zone candidate
   filtering.
   Verify: hot zone allocation/retirement counters increment correctly,
   prune skips hot-zone frames, validation still passes.

4. Wiring (separate phase): cnr3_create ceiling init, arInitial hot zone
   update, predecessor pin/unpin, debug output additions.

