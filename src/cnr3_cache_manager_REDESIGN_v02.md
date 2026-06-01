# CNR3 Cache Manager — Revised Design Specification CMS02
## Jump-Safe Hot-Zone Pruning with Non-Checkpoint Pinning (Deferred)

**Date:** 2026-06-01
**Status:** Design specification — pre-implementation
**Supersedes:** CMS01 (cnr3_cache_manager_design_v1.md)
**Also supersedes:** Bounded-recovery policy described in handover snapshot
    v0.14 section 0.9A
**Companion documents:** CNR3_Handover_Snapshot_v0.14 (for infrastructure
    already built)

**Change summary CMS01 → CMS02:**
- Non-checkpoint pinning deferred to Phase CMS01-I with explicit mandatory
  promotion criterion (Section 4.4, Section 7.3.I).
- Fill-holes-only added as a named design principle (Section 2.1).
- Hot zone lifecycle made deterministic; vague centre-shift language replaced
  with explicit low/high extension rules (Section 4.2).
- CMS01 review section 4 (hot zones are heuristic) retained verbatim, marked
  REJECT/HANDLE-CAREFULLY, with rebuttal and corrective design rule (Section 10).
- Phased implementation sequence CMS01-A through CMS01-I adopted (Section 8).
- Ceiling constants flagged for byte-budget replacement after runtime proving
  (Section 5).
- Zone retirement policy for fmUnordered phases made explicit (Section 4.2,
  Section 11).
- Diagnostics list from CMS01 review adopted as definitive counter
  specification (Section 6).
- Open questions requiring decision added (Section 11).

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

1. Guarantee that no frame needed by any active computation is ever pruned,
   to the extent achievable with hot-zone protection. If hot zones prove
   insufficient, non-checkpoint pinning is added (see Section 4.4).
2. Make pruning decisions based on frame-number proximity to active work,
   not on insertion order or frame-number magnitude alone.
3. Support up to CNR3_MAX_HOT_ZONES simultaneous active working ranges
   (covering concurrent pre-jump and post-jump activity).
4. Fill holes only — never recompute a frame that is already cached
   (see Section 2.1).
5. Bound memory use with a hard ceiling derived from bit depth and tuned
   by runtime memory diagnostics.
6. Hard-abort cleanly when the ceiling is hit with nothing prunable, with a
   clear user-facing error message.
7. Remain compatible with the existing mutex model, pool structures, and
   externally-locked helper pattern.
8. Require no changes to the VapourSynth API interaction model.
9. Keep the first implementation understandable and empirically provable
   before adding further complexity.

### 2.1 Fill-Holes-Only Principle

**[ADDED CMS02: This principle was implicit in CMS01 but not named or
explicitly stated. The CMS01 review (Section 2) correctly identified it as
a critical design principle that must be preserved. It is promoted here to
a named first-class design rule.]**

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
- If another concurrent request has already filled part of the chain,
  the later walk must skip those already-filled frames.

This principle minimises redundant computation in normal out-of-order and
mildly overlapping request scenarios. It is the primary reason the design
can remain practical without immediately requiring active-computation locks,
condition variables, or duplicate-work suppression machinery.

Note: this principle does not by itself prove safety under full concurrent
computation. It reduces unnecessary recomputation but does not prevent a frame
from being pruned between the time it is checked and the time it is needed.
That risk is managed in the first implementation by hot-zone protection and
conservative pruning. If testing shows unsafe churn, non-checkpoint pinning
is added (Section 4.4).

---

## 3. Simulation Results — Linear Encoding Jitter

A Monte Carlo simulation (200 runs, 100 frames, varied jitter) was run to
characterise realistic frame arrival patterns for the primary linear encoding
use case (vspipe / ffmpeg pipe with BestSource source plugin).

Model: each frame is dispatched at time = frame_number + uniform_random(0,
jitter_max). Frames arrive at cnr3_get_frame in delivery-time order,
producing a jittered but near-sequential arrival stream.

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
relative to the frame that needs it) follows the same pattern: max 8 at
jitter=6, max 15 at jitter=12.

**Implication for hot zone forward radius:**
CNR3_HOT_ZONE_FORWARD_RADIUS = 10 is well-supported by the simulation data
for realistic BestSource jitter up to approximately jitter_max=8. Increase if
empirically higher jitter is observed.

**Implication for reorder buffer:**
The existing CNR3_REORDER_WINDOW = 32 is more than adequate for linear encoding
jitter up to approximately jitter_max=24 at p99. It does not need to change.

**Hot zone protection rate in linear encoding:**
With back=30, forward=10, the hot zone covers the predecessor for the majority
of frame arrivals under jitter=6. The remaining misses are cases where the
predecessor is in the reorder buffer awaiting delivery, not at risk of pruning.
Hot zone protection is not the critical mechanism for linear encoding — it
becomes critical only under jump scenarios.

---

## 4. Algorithm Overview

### 4.1 Hot Zone Tracking

The cache manager maintains a small fixed-size array of hot zones. Each hot
zone represents a contiguous range of frame numbers that is currently active
— meaning one or more in-flight computations are working within that range.

A hot zone has:
- An **active** flag
- A **low** frame number boundary
- A **high** frame number boundary
- A **last_observed_frame** — the most recently arrived frame number that
  fell within or caused this zone
- Optional diagnostic counters: hit_count, extension_count, merge_count,
  retirement_count, prune_protection_count

**[CHANGED FROM CMS01: CMS01 described a "centre" field and a vague
"shift centre toward F" update rule. This was too implicit for safe
implementation. The CMS01 review (Section 7) correctly identified that
a deterministic low/high extension rule is cleaner, more predictable,
and easier to debug. The centre concept is replaced by low/high boundaries
and last_observed_frame. See lifecycle rules below.]**

Hot zones are updated under the cache mutex at arInitial time, when a frame
request arrives. Hot zones are not time-based — they are frame-number-based.

**Why discrete hot zones, not a single watermark:**
A single lowest-protected-frame-number watermark cannot represent two
simultaneous active ranges. After a forward jump, pre-jump in-flight requests
cluster at low frame numbers while post-jump recovery clusters at high frame
numbers. A watermark set to protect the lower range allows pruning of the upper
range and vice versa. Discrete zones represent both ranges independently.

**Why not a rolling median:**
A single rolling median of all recent arrivals smears across both active ranges,
producing a meaningless midpoint. With pre-jump activity at frames 380-420 and
post-jump activity at frames 780-820, the median would be approximately 600 —
protecting a dead zone while potentially exposing the edges of both active
ranges. Discrete zones avoid this.

### 4.2 Hot Zone Lifecycle — Deterministic Rules

**[CHANGED FROM CMS01: CMS01 described zone lifecycle with a moving-centre
heuristic. Replaced here with explicit deterministic rules per CMS01 review
Section 7. The rules below are the authoritative implementation specification.]**

#### Zone update at arInitial (called for every arriving frame request F):

1. **Check existing zones.**
   Scan all active hot zones. If F falls within the [low, high] range of any
   active zone:
   - Update that zone's last_observed_frame = F.
   - If F < zone.low + CNR3_HOT_ZONE_EXTENSION_MARGIN, extend:
     zone.low = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS).
   - If F > zone.high - CNR3_HOT_ZONE_EXTENSION_MARGIN, extend:
     zone.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS.
   - Increment zone hit_count diagnostic counter.
   - Return. No further action needed.

   CNR3_HOT_ZONE_EXTENSION_MARGIN is a small constant (suggested: 5 frames)
   that determines how close to a zone boundary an arrival must be before the
   zone is extended. Arrivals near the centre of the zone do not extend it.

2. **Check zone jump threshold.**
   If F is outside all active zones but within CNR3_HOT_ZONE_JUMP_THRESHOLD
   frames of any active zone's nearest boundary:
   - Find the nearest active zone.
   - Extend it: adjust low or high to encompass F plus the appropriate radius.
   - Update last_observed_frame.
   - Increment extension_count.
   - Return.

3. **Allocate new zone.**
   If F is outside all active zones by more than CNR3_HOT_ZONE_JUMP_THRESHOLD:
   - This is a jump event.
   - Find a free (inactive) zone slot.
   - If a free slot exists: initialise it with
     low = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS),
     high = F + CNR3_HOT_ZONE_FORWARD_RADIUS,
     last_observed_frame = F, active = true.
   - Increment hot_zone_allocations diagnostic counter.
   - If no free slot exists: execute merge or retirement (see below).

4. **Merge or retire to free a slot (only when all slots are full and a new
   zone is needed):**
   - Attempt lazy retirement first: scan all active zones. If any zone has
     no live frames in either pool within its [low, high] range, mark it
     inactive. Use that slot for the new zone. Increment retirement_count.
   - If no zone can be retired by the above check: merge the two zones
     whose boundaries are closest to each other (smallest gap between one
     zone's high and another zone's low). Set the merged zone's low to
     the minimum of the two lows, high to the maximum of the two highs,
     last_observed_frame to the most recent of the two. Increment
     merge_count. Use the freed slot for the new zone.
   - Log the merge or retirement action including the zone frame ranges
     involved. This is important diagnostic information.

#### Zone retirement policy:

**[ADDED CMS02: CMS01 review Section 7 said "do not retire a zone merely
because it looks old if there may still be in-flight work." The review did
not specify how to determine whether in-flight work remains.
This section makes the retirement policy explicit for each execution mode.]**

For fmUnordered phases (CMS01-E through CMS01-H):
- Only attempt zone retirement lazily — when a new zone allocation is needed
  and all slots are full.
- Do not retire proactively during normal operation.
- A zone is eligible for lazy retirement if no live frames remain in either
  pool within its [low, high] range.
- This is safe for fmUnordered because only one request is in flight at a
  time. When a new arInitial fires, the previous arAllFramesReady is complete,
  so any zone associated with previous work is definitively cold.

For fmParallelRequests (CMS01-F onwards):
- Lazy retirement as above, but retirement eligibility must additionally
  check that no pinned checkpoint exists within the zone's range.
- A zone with a pinned checkpoint in range is not eligible for retirement
  even if no live non-checkpoint frames remain, because the pinned checkpoint
  indicates an active recovery chain is still in progress.
- This check is conservative: it may retain zones slightly longer than
  strictly necessary, which is safe (a spurious zone costs nothing if its
  range has no live frames to protect).

For fmParallel: out of scope until fmParallelRequests is proven. Retirement
policy to be designed when fmParallel is targeted.

### 4.3 Pruning Policy — Hot Zone Aware

The revised pruning policy replaces the current "evict lowest frame number
first" rule.

**Non-checkpoint pool pruning:**

1. Call retire_cold_hot_zones_externally_locked (lazy retirement pass).
2. Collect all non-checkpoint frames whose frame number falls outside every
   active hot zone's [low, high] range AND whose pin_count is zero.
   These are eviction candidates.
3. Among candidates, find the one with the greatest minimum distance from
   any active hot zone boundary. Evict it.
4. Repeat step 3 until non_checkpoint_pool.size() <= CNR3_OUTPUT_CACHE_CAPACITY
   or no candidates remain.
5. If no candidates remain and the pool exceeds the overflow limit, do not
   evict further. The pool temporarily exceeds the soft target. This is
   acceptable up to the hard ceiling.

For step 3, a simple linear scan to find the maximum-distance candidate on
each iteration is acceptable for typical pool sizes (100-300 frames) without
a formal sort structure.

**Checkpoint pool pruning:**

Apply the same hot-zone candidate filtering:

1. A checkpoint is a candidate if: frame number is not zero, pin_count is
   zero, AND frame number falls outside every active hot zone's [low, high].
2. Among candidates, evict the one with the greatest distance from any hot
   zone boundary first.
3. Continue until checkpoint_pool.size() <= CNR3_CHECKPOINT_MIN_RETAIN or
   no candidates remain.

Checkpoints within hot zones or pinned are retained regardless of
CNR3_CHECKPOINT_MAX_RETAIN. The retain limits are soft triggers for when
prune runs, not hard caps on what can be retained.

**Rationale for retaining checkpoints longer:**
Checkpoints anchor recovery chains. A checkpoint inside or near an active hot
zone may be needed as a recovery anchor for in-flight or soon-to-arrive
requests within that zone. Evicting it forces a more expensive no-prior-
checkpoint warm-up recovery. Retaining checkpoints inside hot zones is cheap
— at CNR3_CHECKPOINT_INTERVAL=10 and BACK_RADIUS=30, a hot zone contains at
most 3 checkpoints. Retaining all of them costs 3 VSFrame references.

### 4.4 Non-Checkpoint Frame Pinning — Deferred

**[CHANGED FROM CMS01: CMS01 presented non-checkpoint pinning as the complete
and immediate fix for FM1. The CMS01 review (Sections 3, 5) correctly noted
that non-checkpoint pinning may add unnecessary complexity to the first
implementation if hot zones prove sufficient. CMS02 defers pinning to Phase
CMS01-I and makes it conditional on empirical evidence.]**

**Original CMS01 text (retained for reference):**
"The fix: non_checkpoint_pool entries gain a pin_count field, identical in
semantics to checkpoint pin_count. Any frame in non_checkpoint_pool that is
currently serving as a predecessor in an active computation must be pinned
before that computation begins and unpinned on every exit path."

**CMS02 position:**
Non-checkpoint pinning remains the deterministic solution to FM1 if hot-zone-
aware pruning is shown to be insufficient. The first implementation defers
non-checkpoint pinning and instead relies on conservative hot zones, fill-
holes-only recovery, and detailed diagnostics.

**Mandatory promotion criterion:**
Non-checkpoint pinning becomes mandatory — not optional, not deferred further
— if any of the following are observed during realistic test runs:

- The diagnostic counter `predecessor_missing_when_expected` is non-zero
  during any realistic VHS/VHS-C encode test.
- Recovery repeatedly recomputes frames that were recently cached and should
  have been retained (evidenced by high `recovery_frames_computed` with high
  `holes_filled` but also high `duplicate_computation_avoided` being zero).
- fmParallelRequests testing reveals a race between prune and active
  predecessor use that hot zones do not cover.
- Hot-zone settings required to prevent the above become so broad that they
  defeat the purpose of pruning (e.g. back radius must exceed 200 to be safe).

If any of these conditions are met, stop, add non-checkpoint pinning, and
re-prove before proceeding.

**If non-checkpoint pinning is later added:**
The structural change is: change non_checkpoint_pool from
std::map<int, const VSFrame*> to std::map<int, Cnr3NonCheckpointSlot> where
Cnr3NonCheckpointSlot = { const VSFrame* frame; int pin_count = 0; }.
This mirrors the existing Cnr3CheckpointSlot exactly. All pool readers must
be updated to read .frame. New helpers cnr3_cache_manager_pin_non_checkpoint()
and cnr3_cache_manager_unpin_non_checkpoint() follow the existing checkpoint
pin/unpin pattern. Prune skips non-checkpoint frames with pin_count > 0.

### 4.5 Bounded Warm-Up Recovery — No-Prior-Checkpoint Case

When a request arrives for frame N and no cached output[N-1] exists and no
usable nearest-prior checkpoint exists:

    start = max(0, N - CNR3_OUTPUT_CACHE_CAPACITY)
    output[start] initialised from source[start] using source-copy semantics
    (no predecessor blend — deliberate approximation, documented trade-off)
    output[start+1] through output[N] computed using fill-holes-only rule:
        for K in start+1 .. N:
            if output[K] already in cache: skip (use as predecessor for K+1)
            else: compute from output[K-1], store, continue

Before beginning this walk, allocate or extend a hot zone centred on N with
the standard back/forward radii. This protects recovery-generated frames from
concurrent pruning during the walk.

The approximation at output[start] (source-copy initialisation) is deliberate.
The chroma stabilisation algorithm settles within a bounded number of frames.
This trade-off is accepted and documented. It is not a bug.

### 4.6 Hard Ceiling and Abort Policy

The soft target (CNR3_OUTPUT_CACHE_CAPACITY) is the level pruning aims for.
The overflow limit (capacity × overflow factor) is the trigger for prune.
The hard ceiling is the absolute maximum live VSFrame references the cache
will hold.

If a store would cause total live VSFrame references (both pools combined)
to reach or exceed the hard ceiling, AND prune cannot free any frame
(everything within hot zones or pinned), the store returns false.

The calling getFrame path then returns a VapourSynth filter error for that
frame with a message such as:

"CNR3: cache ceiling reached ([N] frames, [8/16]-bit mode). CNR3 is designed
for near-linear access. Large random seeks in rapid succession may exceed
cache capacity. Reduce seek frequency or use a near-linear workflow."

The filter remains in a valid state after a ceiling abort. Subsequent near-
linear requests continue normally.

---

## 5. Constants

**[CHANGED FROM CMS01: The CMS01 review (Section 8) correctly noted that
frame-count ceilings are partly based on SD 4:2:0 assumptions and may be
inaccurate for other formats CNR3 supports. The frame-count ceilings remain
for the first implementation but are explicitly flagged for replacement with
a byte-budget calculation after runtime memory diagnostics are available.
The byte-budget calculation should derive bytes_per_frame from width, height,
subsampling, bits_per_sample, and number of planes, then set the ceiling
from a configurable byte budget (e.g. 512MB or 1GB) divided by
bytes_per_frame. This replacement is deferred to after Phase CMS01-E when
real memory data is available.]**

```
CNR3_OUTPUT_CACHE_CAPACITY        = 100
    Soft pruning target. Prune tries to keep non_checkpoint_pool at or below
    this size during normal operation.

CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Prune triggers when non_checkpoint_pool exceeds capacity * factor (= 110).

CNR3_OUTPUT_CACHE_CEILING_8BIT    = 1000
    Hard ceiling for 8-bit input (~608MB at 4:2:0 720x576).
    SUBJECT TO TUNING after Phase CMS01-E memory diagnostics.
    Planned replacement: byte-budget ceiling calculated from actual frame
    geometry. See note above.

CNR3_OUTPUT_CACHE_CEILING_16BIT   = 500
    Hard ceiling for 16-bit input (~596MB at 4:2:0 720x576).
    Same tuning note as above.

CNR3_CHECKPOINT_INTERVAL          = 10
    Promote every 10th frame to checkpoint pool.

CNR3_CHECKPOINT_MAX_RETAIN        = 16
    Soft trigger: checkpoint prune runs when checkpoint_pool exceeds this.
    Checkpoints inside hot zones are retained regardless of this limit.
    Flagged for review after runtime proving.

CNR3_CHECKPOINT_MIN_RETAIN        = 6
    Prune back to this count when eligible candidates exist.
    Flagged for review after runtime proving.

CNR3_HOT_ZONE_FORWARD_RADIUS      = 10
    Frames ahead of hot zone high boundary protected from pruning.
    Supported by simulation: covers p99 BestSource jitter to jitter_max~8.

CNR3_HOT_ZONE_BACK_RADIUS         = 30
    Frames behind hot zone low boundary protected from pruning.
    Covers 3 checkpoint intervals of backward history. Conservative starting
    point; increase if pruning proves too aggressive in practice.

CNR3_HOT_ZONE_EXTENSION_MARGIN    = 5
    An arrival within this many frames of a zone boundary triggers a zone
    extension rather than just a hit record. Tunable.

CNR3_MAX_HOT_ZONES                = 5
    Maximum simultaneous active hot zones. Covers 1 linear encoding zone
    plus up to 4 concurrent jump zones.

CNR3_HOT_ZONE_JUMP_THRESHOLD      = CNR3_HOT_ZONE_FORWARD_RADIUS +
                                     CNR3_HOT_ZONE_BACK_RADIUS + 1 (= 41)
    A new frame request allocates a new hot zone if it falls outside all
    existing zones by more than this threshold. Within this distance,
    the nearest zone is extended instead.
```

---

## 6. Diagnostics — Definitive Counter Specification

**[ADDED CMS02: The CMS01 review (Section 9) provided a comprehensive
diagnostics list. This list is adopted as the definitive counter specification
for the cache manager. It supersedes the partial list in CMS01 Section 7.1.F.
These counters are the primary evidence base for the decision in Section 4.4
on whether non-checkpoint pinning is required.]**

All counters are int64_t. All are members of Cnr3CacheManagerStats or a
new Cnr3CacheManagerRuntimeStats struct (to be decided at implementation time
based on whether it is cleaner to extend the existing stats struct or add a
second one for runtime-only counters).

**Hot zone counters:**
- hot_zone_allocations
- hot_zone_extensions
- hot_zone_merges
- hot_zone_retirements
- hot_zone_hits (requests falling inside an existing zone)
- hot_zone_new_zone_requests (requests causing a new zone allocation)
- hot_zone_max_active_observed (high-water mark of simultaneous active zones)

**Pruning counters (additions to existing):**
- non_checkpoint_prune_skipped_in_hot_zone
- non_checkpoint_prune_skipped_pinned (mirrors existing checkpoint counter)
- checkpoint_prune_skipped_in_hot_zone
- prune_no_candidate_exists (pool exceeds limit but all frames protected)
- cache_ceiling_hard_aborts

**Recovery counters:**
- bounded_warmup_recovery_count
- bounded_warmup_recovery_start_frame (last observed, or log all)
- bounded_warmup_recovery_length (last observed)
- recovery_frames_computed
- recovery_frames_skipped_already_cached
- holes_filled
- duplicate_computation_avoided
- nearest_checkpoint_recovery_count
- no_prior_checkpoint_recovery_count
- max_recovery_chain_length_observed

**Cache operation counters:**
- cache_hits
- cache_misses
- predecessor_missing_when_expected  *** CRITICAL — see Section 4.4 ***
- checkpoint_missing_when_expected
- max_live_cached_frames_observed
- max_non_checkpoint_pool_size_observed
- max_checkpoint_pool_size_observed

**Debug summary output policy:**
- Default: print headline counters only at cnr3_free time.
- Development diagnostics enabled: print all counters plus hot zone state.
- Per-event verbose output: guarded behind CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS.
- No diagnostic output to stdout under any circumstances.
- predecessor_missing_when_expected > 0 should print a prominent warning
  regardless of diagnostic level, because it indicates a design assumption
  has been violated.

---

## 7. Worked Examples

### Example A — Linear Encoding, No Jumps

Setup: 32-thread encode, BestSource jitter up to 6 frames. Cache starts empty.

Frames 0..5 arrive approximately in order. On the first arrival (frame 0),
hot zone 0 is allocated: low=0, high=10, last_observed=0.

As frames 1..15 arrive (some out of order due to jitter), they fall within
hot zone 0's range. Zone 0 extends gradually: by frame 15, high=25.

No pruning occurs (pool size well below overflow = 110).

By frame 80, hot zone 0: low=50, high=90. Pool has ~80 frames. Still below
overflow.

By frame 110, pool has ~110 frames, overflow triggered. Prune runs.
Candidates: frames outside hot zone [50, 90] with pin_count=0. Frames 0..49
are candidates. Evict in order of greatest distance from zone boundary (frame 0
first, then 1, etc.) until pool = 100. Frames 50..110 remain intact.

Predecessor chain is always within the hot zone. No predecessor failures.

---

### Example B — Small Forward Jump Within Extension Threshold

Setup: encoding at frame 80. User seeks to frame 95. Hot zone 0: low=50,
high=90.

Frame 95 arrives. Distance from hot zone 0's high boundary: 5 frames.
5 <= CNR3_HOT_ZONE_JUMP_THRESHOLD (41). Zone 0 is extended: high = 95+10=105.
No new zone allocated.

Cache lookup: output[94] may be present. If present (cache hit), return
immediately. If not, nearest checkpoint (say checkpoint[90]) found, pinned,
recovery fills 91..95, checkpoint unpinned.

Normal operation resumes. One zone, no policy change.

---

### Example C — Large Forward Jump, Two Active Ranges

Setup: sequential encoding at frame 200. Hot zone 0: low=170, high=210.
Pre-jump in-flight: frames 198, 199, 200 still computing.

Frame 600 arrives at arInitial. Distance from zone 0 high boundary (210):
390 frames. 390 > jump threshold (41). Jump detected. Allocate hot zone 1:
low=570, high=610.

Hot zone 0 still active: frames 198, 199, 200 are within [170, 210].
They cannot be pruned.

Bounded recovery for frame 600: no checkpoint above 200.
start = max(0, 600 - 100) = 500.
Recovery walk: output[500] = source-copy. output[501..600] computed using
fill-holes-only (skip any already cached). 101 frames stored.

Pruning during recovery walk: candidates are frames outside both zones
([170,210] and [570,610]) with pin_count=0. Frames 0..169 and 211..569 are
candidates. Evict furthest first: frame 0, 1, ... Pool stays manageable.
Frames 170..210 (pre-jump zone) and 570..610 (jump zone) untouched.

Pre-jump frames 198, 199, 200 complete. Hot zone 0 goes cold — no new
arrivals in [170, 210]. At next zone slot demand, zone 0 is lazy-retired
(no live frames remain in [170, 210] after normal pruning clears them).

Steady state: one active zone around frame 600. Cache holds frames 580..610
plus recent recovery frames. Normal operation resumes.

---

### Example D — No-Prior-Checkpoint Recovery, Cold Seek

Setup: fresh instance, user seeks to frame 800. Cache empty.

Frame 800 arrives. No active zones. Allocate hot zone 0: low=770, high=810.

find_and_pin_nearest_prior_checkpoint: no checkpoints exist, returns false.
no_prior_checkpoint_recovery_count incremented.

Bounded warm-up:
start = max(0, 800 - 100) = 700.
output[700] = source-copy initialisation (no predecessor, approximation).
Fill-holes-only walk: output[701..800] computed. Checkpoints promoted at
700, 710, 720, ... 800.

After recovery: prune candidates are frames 700..769 (below hot zone low=770),
pin_count=0. Evict furthest first. Pool settles to ~40 frames.
Checkpoints 700..800 retained per checkpoint pruning rules.

Subsequent seeks to the 700-800 range use checkpoints as anchors. Cold-start
approximation is confined to output[700].

---

### Example E — Hard Ceiling Abort, Non-Linear Use

Setup: 16-bit 4:2:2 input. User makes 5 rapid large seeks before any recovery
completes. Five concurrent recovery walks each generating ~101 frames = ~505
total. Ceiling = 500.

After 500 stores, next store: ceiling check fires. Prune: no candidates
(all frames within one of the 5 active hot zones). Store returns false.
cache_ceiling_hard_aborts incremented.

getFrame for that frame returns VapourSynth error:
"CNR3: cache ceiling reached (500 frames, 16-bit mode). CNR3 is designed for
near-linear access. Large random seeks in rapid succession exceed cache
capacity."

Filter remains valid. Subsequent near-linear requests succeed.

---

### Example F — Fill-Holes-Only Avoiding Redundant Compute

Setup: frame 150 was computed and stored. Concurrent request for frame 155
triggers recovery walk from checkpoint[150], walking 151..155. On arrival
at step K=150 in the walk: output[150] found in cache. Skip computation.
Use output[150] as predecessor for output[151]. recovery_frames_skipped_
already_cached incremented. Walk continues from 151 without recomputing 150.

Result: one frame of redundant computation avoided. In a scenario where
multiple requests overlap recovery ranges, multiple frames are skipped.

---

## 8. Phased Implementation Sequence

**[ADDED CMS02: Adopts the CMS01-A through CMS01-I phasing from the CMS01
review (Section 10) as the authoritative implementation sequence. This
supersedes the implementation order in CMS01 Section 9.]**

#### Phase CMS01-A — Documentation and constants only
- Mark CMS02 as the current Phase 4 cache-policy direction.
- Add all new constants (Section 5) to cnr3_cache_manager.h.
- Add all new diagnostic counter declarations (Section 6).
- Add ceiling constants and active_ceiling field to Cnr3CacheManagerV005.
- Add Cnr3HotZone struct declaration.
- Do not change runtime behaviour.

#### Phase CMS01-B — Hot zone structures and passive diagnostics
- Add hot zone array to Cnr3CacheManagerV005.
- Add hot zone helper declarations to cnr3_cache_manager.h.
- Add passive debug snapshot fields for hot zone state.
- Add all Section 6 counter fields to stats structs.
- Do not use hot zones for pruning yet.
- Do not change frame output behaviour.

#### Phase CMS01-C — Hot zone update helpers
- Implement cnr3_cache_manager_update_hot_zones() per Section 4.2 rules.
- Implement cnr3_cache_manager_is_frame_in_hot_zone_externally_locked().
- Implement cnr3_cache_manager_retire_cold_hot_zones_externally_locked()
  using fmUnordered lazy retirement policy (Section 4.2).
- Instrument all zone lifecycle events with Section 6 counters.
- Do not use hot zones in pruning yet.

#### Phase CMS01-D — Hot zone aware prune candidate selection
- Replace prune_non_checkpoint_pool_externally_locked inner loop with
  hot-zone-aware candidate selection (Section 4.3).
- Replace prune_checkpoint_pool_externally_locked similarly.
- Add cnr3_cache_manager_would_exceed_ceiling_externally_locked().
- Add ceiling check and hard abort to store_output_frame.
- Instrument all prune decisions with Section 6 counters.

#### Phase CMS01-E — Store/prune-only runtime proving
- In the existing strict-streaming output path, store produced frames into
  cache_manager_v005 after the existing path completes.
- Call prune_after_store with the new hot-zone-aware prune.
- Do not yet use v005 frames for output generation.
- Enable memory diagnostics around store/prune events.
- Prove: addFrameRef/freeFrame balance, pool sizes, prune behaviour,
  hot zone allocation/retirement, no stdout output.
- Prove: ceiling counters remain zero during normal linear encode.

#### Phase CMS01-F — Cache-hit reuse under fmUnordered
- Implement cnr3_cache_manager_find_output_frame_and_add_ref() returning
  a caller-owned reference (never a raw borrowed pointer after unlock).
- At the start of cnr3_get_frame arAllFramesReady, check v005 cache hit.
- If hit: return cached frame via caller-owned ref, increment cache_hits.
- If miss: proceed with normal computation, increment cache_misses.

#### Phase CMS01-G — Checkpoint recovery and hole filling under fmUnordered
- find_and_pin_nearest_prior_checkpoint for out-of-order requests.
- Fill missing frames ascending using fill-holes-only rule.
- Skip frames already present in cache.
- Store newly generated frames, prune after store.
- Unpin checkpoint on every exit path.
- Instrument holes_filled, recovery_frames_skipped_already_cached,
  nearest_checkpoint_recovery_count, predecessor_missing_when_expected.

#### Phase CMS01-H — Bounded warm-up recovery under fmUnordered
- Implement no-prior-checkpoint warm-up recovery per Section 4.5.
- Instrument bounded_warmup_recovery_count, start frame, length, frames
  computed vs skipped.

#### Phase CMS01-I — Empirical review: non-checkpoint pinning decision
After realistic VHS/VHS-C encode tests and synthetic jump tests:
- Inspect all Section 6 counters, especially predecessor_missing_when_expected.
- If mandatory promotion criteria (Section 4.4) are met: add non-checkpoint
  pinning before proceeding.
- If criteria are not met: document findings and proceed to fmParallelRequests.

#### Phase CMS01-J (formerly next) — fmParallelRequests
Only after CMS01-H is proven under fmUnordered and CMS01-I decision is made.

#### Full fmParallel — explicitly out of scope until fmParallelRequests proven.

---

## 9. Structural Changes Required to Uploaded Code

### 9.1 cnr3_cache_manager.h — Structure changes

**A. Add Cnr3HotZone struct:**
```
struct Cnr3HotZone {
    bool active = false;
    int low = -1;
    int high = -1;
    int last_observed_frame = -1;
    // diagnostic counters (int64_t):
    //   hit_count, extension_count, merge_count,
    //   retirement_count, prune_protection_count
};
```

**B. Add hot zone array to Cnr3CacheManagerV005:**
A fixed-size array of Cnr3HotZone, size CNR3_MAX_HOT_ZONES.
All elements initialised inactive. This is mutable cache-manager state
and must be accessed only while holding cache_mutex.

**C. Add active_ceiling to Cnr3CacheManagerV005:**
An int field. Set once at cnr3_create() time. Not changed after init.

**D. Add new constants** per Section 5.

**E. Add new statistics counters** per Section 6 to Cnr3CacheManagerStats
or a new Cnr3CacheManagerRuntimeStats struct.

**[NOTE: Non-checkpoint pinning structural change (changing non_checkpoint_pool
value type) is deferred to Phase CMS01-I. Do not change the pool type now.]**

### 9.2 cnr3_cache_manager.h — New helper declarations

**F. Hot zone helpers:**
- cnr3_cache_manager_update_hot_zones(cache, frame_number) — public, locks
- cnr3_cache_manager_is_frame_in_hot_zone_externally_locked(cache, frame_number)
  — returns bool, caller holds mutex
- cnr3_cache_manager_retire_cold_hot_zones_externally_locked(cache)
  — lazy retirement pass, caller holds mutex
- cnr3_cache_manager_set_ceiling(cache, bits_per_sample) — public, called once

**G. Ceiling check:**
- cnr3_cache_manager_would_exceed_ceiling_externally_locked(cache) — bool

**H. Cache hit lookup:**
- cnr3_cache_manager_find_output_frame_and_add_ref(cache, frame_number, vsapi)
  — returns caller-owned VSFrame* (addFrameRef taken inside, under mutex),
    or nullptr if not found. Caller must freeFrame on every path.

### 9.3 cnr3_cache_manager.cpp — Logic changes

**I. prune_non_checkpoint_pool_externally_locked:**
Replace while { evict begin() } with hot-zone-aware candidate selection
per Section 4.3. Call retire_cold_hot_zones first.

**J. prune_checkpoint_pool_externally_locked:**
Add hot-zone candidate filtering before existing frame-zero and pin_count
checks. Evict furthest-from-hot-zone eligible candidate first.

**K. store_output_frame (public):**
Before addFrameRef and pool insertion, call would_exceed_ceiling. If ceiling
would be exceeded and no prune candidates exist, increment
cache_ceiling_hard_aborts and return false.

**L. validate_invariants_externally_locked:**
Add: all hot zone slots with active=true have low >= 0, high >= low,
last_observed_frame >= 0. No functional change to frame ownership checks.

### 9.4 vapoursynth-Cnr3.cpp — Wiring (future phases, not yet)

**M. cnr3_create():**
After Cnr3Data construction:
  cnr3_cache_manager_set_ceiling(d->cache_manager_v005, d->bits_per_sample)

**N. cnr3_get_frame() arInitial:**
  cnr3_cache_manager_update_hot_zones(cache, frame_number)
Called for every arriving frame request before any cache lookup.

**O. Debug output at cnr3_free:**
Add hot zone state summary and new Section 6 counters to existing
debug summary. predecessor_missing_when_expected > 0 prints a prominent
warning regardless of diagnostic level.

---

## 10. Rejected/Handle-Carefully: CMS01 Review Section 4

**[REJECT / HANDLE CAREFULLY — see rebuttal and corrective rule below]**

**Original CMS01 review text (Section 4), retained verbatim:**

"The concern with hot zones is not that they are wrong. The concern is that
they are a heuristic protection layer.

A hot zone protects a frame because its frame number is near active work.
That should be effective for normal linear encoding, modest jitter, and
bounded jump recovery. However, it is not as absolute as pinning a specific
frame that is known to be in active use.

A specific failure pattern to keep in mind:

1. Request A is computing output[450].
2. It needs output[449] as the predecessor.
3. output[449] is present in the non-checkpoint pool.
4. A forward jump creates a new hot zone around output[800].
5. The old hot zone around output[450] is shifted, merged, retired too early,
   or becomes too narrow.
6. Pruning sees output[449] as outside all hot zones and removes it.
7. Request A now no longer has the predecessor it expected."

**Why this is rejected as a design objection (but retained as a test case):**

The failure scenario described is real. However, it is not an argument against
hot zones as a mechanism. It is a precise specification of what the hot zone
lifecycle rules must prevent.

Step 5 is the load-bearing failure: "the old hot zone is shifted, merged,
retired too early, or becomes too narrow." If the hot zone lifecycle rules
in Section 4.2 are correctly implemented, step 5 cannot occur:

- A zone is not retired while any in-flight request has a frame number within
  its range (fmUnordered: trivially safe as only one request is in flight;
  fmParallelRequests: checked via pinned checkpoint presence in range).
- A zone is not merged unless no live frames remain in its range AND the
  zone is eligible for retirement.
- A zone is not narrowed — zones only ever extend, never shrink.
- A request for frame 450 will have allocated or extended a hot zone that
  covers [420, 460] (back=30, forward=10). Frame 449 is inside that zone.
  As long as the zone lifecycle rules prevent premature retirement of that
  zone, frame 449 cannot be pruned.

Therefore: the failure scenario is the correct test case for the lifecycle
rules, not a counterargument to hot zones.

**Corrective design rule added to CMS02:**
Hot zones MUST NOT be retired while any request whose arInitial fired within
the zone's [low, high] range has not yet completed its arAllFramesReady or
arError path. For fmUnordered this is trivially enforced by the single-request
constraint. For fmParallelRequests this is approximated conservatively by the
pinned-checkpoint check in Section 4.2. The diagnostic counter
predecessor_missing_when_expected is the runtime verification that this rule
is holding. If it is ever non-zero, the lifecycle rules have failed and
non-checkpoint pinning becomes mandatory.

---

## 11. Open Questions Requiring Decision

These questions were not addressed by the CMS01 review. They must be resolved
before or during the phases described in Section 8. Each has a proposed answer
or recommendation.

---

**OQ1 — How is zone retirement safety verified under fmParallelRequests?**

The retirement eligibility check for fmParallelRequests (Section 4.2) uses
"no pinned checkpoint exists within the zone's range" as a proxy for "no
active computation is in progress within this zone." This is conservative but
not exact — a computation could be active in a zone that currently has no
pinned checkpoint (e.g. a cache-hit computation that needed no recovery and
therefore pinned no checkpoint).

*Proposed answer:*
Accept the conservative proxy for the first fmParallelRequests implementation.
A spuriously retained zone costs nothing (its frames are already being retained
by the hot zone protection anyway). A spuriously retired zone could cause FM1.
Conservative retention is the safe default.

If this proxy proves too conservative (zones never retire, zone slots fill up,
merges happen too frequently), the correct escalation is to add a per-zone
active_request_count field, incremented at arInitial and decremented at
arAllFramesReady/arError. This gives exact retirement eligibility.

*Decision needed:* Accept this answer, or specify a different proxy for phase
CMS01-J (fmParallelRequests). Recommended: accept the conservative proxy now,
add active_request_count when fmParallelRequests testing shows it is needed.

---

**OQ2 — What is CNR3_HOT_ZONE_EXTENSION_MARGIN and should it exist?**

Section 4.2 introduces a CNR3_HOT_ZONE_EXTENSION_MARGIN constant (suggested: 5)
that determines how close to a zone boundary an arrival must be before the zone
is extended. Arrivals near the centre of the zone do not trigger an extension.

This is a refinement to avoid unnecessary zone boundary creep during normal
linear operation (where every frame arrives roughly in order near the zone
centre). Without it, every arrival would potentially extend the zone, causing
it to grow unboundedly.

*Proposed answer:*
Keep the extension margin concept. Set it to 5 frames initially. If the zone
grows too aggressively in practice (observed via hot_zone_extensions counter
being very high), increase the margin. If the zone proves too slow to expand
(observed via zones being too narrow to protect needed predecessors), decrease it
or remove it.

*Decision needed:* Accept extension margin = 5, or remove the concept and
always extend on arrival near the boundary. Recommended: accept margin = 5.

---

**OQ3 — Should the byte-budget ceiling replacement be specified now or after
Phase CMS01-E?**

Section 5 defers the byte-budget ceiling calculation to after Phase CMS01-E
memory diagnostics are available. However, the calculation is straightforward
and could be specified now.

Approximate bytes per frame:
  bytes_per_frame = width * height * bytes_per_sample *
                    (1.0 + 2.0 * chroma_fraction)
  where chroma_fraction = 0.25 for 4:2:0, 0.5 for 4:2:2, 1.0 for 4:4:4
  and bytes_per_sample = bits_per_sample / 8

Ceiling frame count = byte_budget / bytes_per_frame
  where byte_budget = e.g. 512MB or 1GB, compile-time constant.

*Proposed answer:*
Specify the formula now, implement it in Phase CMS01-A as the ceiling
calculation in cnr3_cache_manager_set_ceiling(). Use a compile-time
CNR3_CACHE_BYTE_BUDGET constant (suggested: 512 * 1024 * 1024 = 512MB).
The resulting ceiling frame count is then format-aware rather than a
hardcoded 1000/500. The 8-bit/16-bit constants become fallbacks if the
geometry is not available at ceiling-set time.

*Decision needed:* Specify and implement byte-budget ceiling in Phase
CMS01-A, or keep frame-count ceilings and defer byte-budget to after
CMS01-E. Recommended: implement byte-budget ceiling in CMS01-A — it is
simple, format-correct, and eliminates the need to revisit the constants.

---

**OQ4 — Should CNR3_CHECKPOINT_MAX_RETAIN and MIN_RETAIN be increased now?**

Both constants were sized before hot-zone-aware pruning was designed. Under
the new design, checkpoints inside hot zones are retained regardless of
MAX_RETAIN. The retain limits are now soft triggers, not hard caps.

With CNR3_HOT_ZONE_BACK_RADIUS = 30 and CNR3_CHECKPOINT_INTERVAL = 10, a
single hot zone can hold at most 3 checkpoints that are protected from pruning
regardless of MAX_RETAIN. With 5 hot zones active simultaneously, up to 15
checkpoints could be protected. If MAX_RETAIN = 16 and 15 slots are protected,
only 1 checkpoint is eligible for pruning, which may be insufficient to bring
the pool back to MIN_RETAIN = 6 when a large jump fills many new checkpoints.

*Proposed answer:*
Increase CNR3_CHECKPOINT_MAX_RETAIN to 32 and CNR3_CHECKPOINT_MIN_RETAIN to
10. This gives more room for protected checkpoints without exhausting the
checkpoint pool. The checkpoint pool at 32 entries costs at most 32 VSFrame
references (~19MB at 4:2:0 8-bit 720x576) — entirely affordable.

*Decision needed:* Accept MAX_RETAIN=32, MIN_RETAIN=10, or keep current
values and revisit after Phase CMS01-E. Recommended: increase now, since
the analysis shows the current values are likely too small under the new
hot-zone-aware design.

---

**OQ5 — How should the prune-no-candidate-exists case be handled in practice?**

Section 4.3 says if no eviction candidates exist (everything in hot zones or
pinned) and the pool exceeds the overflow limit, do not evict — the pool
temporarily exceeds the soft target up to the ceiling.

Under normal linear encoding this should never occur (only one hot zone active,
plenty of frames outside it). Under a large jump scenario with multiple hot
zones, the pool could grow significantly before candidates appear.

If the pool grows toward the ceiling and `prune_no_candidate_exists` counter is
incrementing rapidly, it indicates the hot zones are too broad or too many are
active simultaneously.

*Proposed answer:*
Accept the "do not evict" behaviour as correct for now. Track
prune_no_candidate_exists. If it is incrementing significantly during realistic
tests (not just extreme random-seek abuse), reduce CNR3_HOT_ZONE_BACK_RADIUS
or CNR3_MAX_HOT_ZONES. The ceiling and hard abort remain the safety net.

*Decision needed:* Accept this position, or specify a fallback eviction
policy (e.g. after N consecutive prune_no_candidate_exists increments, evict
the frame furthest from all zone centres regardless of hot zone membership).
Recommended: accept the "do not evict" behaviour initially; add the fallback
only if memory diagnostics show the ceiling is being approached during
realistic workloads.

