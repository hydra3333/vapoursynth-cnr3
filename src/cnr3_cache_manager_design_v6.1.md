# CNR3 Cache Manager — Revised Design Specification CMS06.1
## Sliding Hot-Zone Pruning with Reference-Count Discipline (Non-Checkpoint Pinning Deferred)

**Date:** 2026-06-05
**Version:** CMS06.1
**Status:** Design specification — ready for coding
**Supersedes:** CMS06, CMS05.2b, CMS05.2, CMS05.1, CMS05, CMS04, CMS03, CMS02, CMS01
**Also supersedes:** Bounded-recovery policy in handover snapshot v0.14 section 0.9A
**Companion documents:**
- CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)
- cnr3_code_review_plan_v5_1.md (code review and simulation plan)

---

## Changelog

### CMS06.1 — 2026-06-05

Clarifified that the final target is operating safely fmParallel and that interim
steps of fmUnorderd and fmParallelRequests may be valid interim steps but design
and coding must lean toward safe operation under fmParallel.

### CMS06 — 2026-06-02

Clean version incorporating all CMS05.x revisions. No new design
decisions. Changes from CMS05.1:

**CMS06-1 — Actual code file naming confirmed and spec updated.**
Code review of uploaded source files confirmed the current active naming.
Section 9 updated to match code reality. Two sets of files were in play:
disk-uploaded (pre-rename) and inline documents (current active). The
inline documents are authoritative. See Sections 9 and 14.

**CMS06-2 — `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS` resolved.**
`cnr3_output_cache_manager.h` referenced `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS`
which was missing from `cnr3_build_config.h`. The updated build config
now defines it, replacing the old `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`.
If any file still references the old name, it must be updated. See
Section 13.1.

**CMS06-3 — Stale phase comment in `cnr3_output_cache_manager.h` noted.**
The comment "Phase 1 intentionally does not change current runtime
behaviour" is stale — Phase 3A store/prune wiring is complete. See
Section 13.2.

**CMS06-4 — Section 14 added: Current Implementation State.**
Ground truth derived from reading the uploaded source files. Records
phase completion, rename completion, what is wired and live, one spec
deviation (hot zone update placement), and what is not yet implemented.

**CMS06-5 — Section 13 added: Pending Code Issues.**
Records known issues requiring resolution before or during next coding
session.

### CMS05.1 — 2026-06-02

- `CNR3_CACHE_BYTE_BUDGET` raised from 512 MiB to 1 GiB.
- Ceiling clarification note added for 720×576 vs 1440×576.
- Appendix D added: Code Review and Simulation Plan reference.

### CMS05 — 2026-06-01

Seven changes from CMS04: first-in-best-dressed store idempotency;
final-frame ownership transfer; lookup-owned reference balance counters;
version-independent naming; Design-Compliance Review process; additional
store-related diagnostic counters; Appendix C added.

---

## 1. Problem Statement

CNR3 is a recursive temporal chroma stabiliser. Every output frame depends
on the previous output frame as a predecessor. The cache manager must
retain enough predecessor frames to satisfy any in-flight computation,
prune old frames to keep memory bounded, and maintain strict VSFrame
reference-count discipline so that no frame is ever leaked or freed
prematurely.

The existing cache manager infrastructure (pools, checkpoint pinning,
mutex model, store/remove/validate helpers) is sound. What is not yet
designed is the pruning policy and structural protections needed to make
that policy safe under three VapourSynth execution modes:

- **fmUnordered** — one request in flight at a time, mostly sequential
- **fmParallelRequests** — multiple concurrent requests, serialised writer
- **fmParallel** — fully concurrent readers and writers (final target)
- Important Notes (repeated in another section):
    To be very clear, **fmParallel** is specifically the final operational target,
    hence design and coding should lead toward running safe under fmParallel although
    iterative design/development may pass through fmUnordered and fmParallelRequests
    as interim stepping stones.
    It is conjectured that the cache design is, with correct mutexes in the correct 
    places, orders and depths, compatible with fmParallel; alignment with this will
    be subject to later review.

The primary failure modes without the new design:

**FM1 — Prune destroys in-flight predecessor.**
**FM2 — Prune destroys a checkpoint needed by a recovery chain.**
**FM3 — Jump recovery burst exceeds pool capacity.**
**FM4 — Prune eviction key is wrong (lowest-first).**
**FM5 — VSFrame reference leak.**
**FM6 — VSFrame use-after-free.**
**FM7 — Duplicate store overwrites or double-references existing frame.**

---

## 2. Design Goals

1. Prevent pruning of frames likely to be needed by active computation
   using hot-zone protection. If diagnostics show hot-zone protection is
   insufficient, promote non-checkpoint pinning to a mandatory
   implementation step (Section 4.4).

2. Make pruning decisions based on frame-number proximity to active work,
   not on insertion order or frame-number magnitude.

3. Support up to `CNR3_MAX_HOT_ZONES` simultaneous active working ranges.

4. Fill holes only — never recompute a frame that is already cached.
   Stores are idempotent by frame number (Section 2.1, Section 4.9).

5. **Reference-Count Invariant.** For every VSFrame held by the cache,
   the cache contributes exactly one `addFrameRef` while it holds the
   slot, balanced by exactly one `freeFrame` when the slot is removed.
   No VSFrame is ever leaked. No VSFrame is ever freed while still in
   use outside the cache mutex (Section 2.2, Section 4.7).

6. Bound memory use with a hard ceiling computed from actual frame
   geometry and a configurable byte budget (Section 4.6).

7. Hard-abort cleanly when the ceiling is hit with nothing prunable.
   Filter must remain in a valid state after the abort.

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
- Before computing any frame K, check whether output[K] already exists.
- If present, reuse it without recomputing.
- If absent (a hole), compute from the best available predecessor, store,
  and continue.
- If a concurrent walk loses a store race, the losing walk's store helper
  returns success without taking ownership of the losing walk's frame.
  The losing walk releases its computed frame normally (Section 4.9).

**Reference-ownership rule:**

When output[K] is found in cache and used as the predecessor for
output[K+1] outside the cache mutex, the lookup helper takes
`addFrameRef()` while still holding the cache mutex (atomic find-and-ref),
then returns that caller-owned reference. The caller must `freeFrame()`
on every exit path — success, error, early return. Implemented by
`cnr3_output_cache_find_frame_and_add_ref()` (Section 9.2.H).

Hot-zone protection and `addFrameRef` are independent mechanisms with
different roles (Section 4.7).

### 2.2 Reference-Count Discipline

VapourSynth manages frame memory by reference counting. `addFrameRef`
increments; `freeFrame` decrements. At zero the frame's pixel data is
released.

**Hard rules:**

**RC1 — Single store helper.** All cache-owned `addFrameRef` calls occur
only in `cnr3_output_cache_store_frame()`.

**RC2 — Single remove helper.** All cache-owned `freeFrame` calls occur
only in `cnr3_output_cache_remove_frame_externally_locked()`. No direct
`pool.erase()` or `cache_index.erase()` outside the remove helper.

**RC3 — Store error paths must rebalance.** If store takes `addFrameRef`
then fails to complete insertion, it must `freeFrame` before returning.

**RC4 — Lookup error paths must rebalance.** If
`find_frame_and_add_ref` takes `addFrameRef` then fails before returning
to the caller, it must `freeFrame` before returning nullptr.

**RC5 — Caller exit paths must `freeFrame`.** Every code path receiving
a caller-owned reference from a lookup helper must `freeFrame` on every
exit path: success, error, early return, exception, hard abort.

**RC6 — Shutdown must clear.** Destruction of `Cnr3OutputCacheManager`
must iterate all slots in both pools, `freeFrame` each, and log a
warning for any slot with `pin_count > 0`.

**RC7 — Validation enforces balance.** `validate_invariants` includes:
`cache_addframeref_total - cache_freeframe_total == total_live_slots`.

**RC8 — First-in-best-dressed store idempotency.** Store is idempotent
by frame number. If `output[N]` already exists at store time, return
success without taking `addFrameRef`, without modifying either pool, and
without disturbing the existing slot's classification. The cache has not
taken ownership of the caller's supplied frame. See Section 4.9.

**Recommended RAII wrapper — `Cnr3OwnedFrameRef`:**

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
    Cnr3OwnedFrameRef(Cnr3OwnedFrameRef&&) noexcept;
    Cnr3OwnedFrameRef& operator=(Cnr3OwnedFrameRef&&) noexcept;
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

Diagnostic invariant at quiescent points:
`acquired == released + transferred`

---

## 3. Simulation Results — Linear Encoding Jitter

A Monte Carlo simulation (200 runs, 100 frames, varied jitter) characterised
realistic frame arrival patterns for the primary linear encoding use case
(vspipe / ffmpeg pipe with BestSource source plugin).

Model: each frame dispatched at time = `frame_number + uniform_random(0,
jitter_max)`. Frames arrive at `cnr3_get_frame` in delivery-time order.

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

For observed BestSource jitter of 4–6 frames, worst-case reorder window
is approximately 9 frames at p99. `CNR3_HOT_ZONE_FORWARD_RADIUS = 10`
covers this with headroom. `CNR3_REORDER_WINDOW = 32` is adequate for
jitter up to approximately `jitter_max=24` at p99.

---

## 4. Algorithm Overview

### 4.1 Hot Zone Tracking

The cache manager maintains a fixed-size array of hot zones. Each hot
zone represents a contiguous range of frame numbers currently active
(one or more in-flight computations work within that range).

A hot zone has:
- **active** flag
- **low** frame number boundary (inclusive)
- **high** frame number boundary (inclusive)
- **last_observed_frame** — most recently arrived frame number that
  fell within or caused this zone
- Diagnostic counters: `hit_count`, `slide_count`, `merge_count`,
  `retirement_count`, `prune_protection_count`

Discrete hot zones allow multiple simultaneous active ranges to be
represented independently. A jump produces two zones that coexist until
the old zone goes cold.

### 4.2 Hot Zone Lifecycle — Sliding (Both Modes)

#### 4.2.1 Sliding rule (both modes)

For an arriving frame `F`:

1. Find the nearest active zone Z such that `F` is within
   `CNR3_HOT_ZONE_JUMP_THRESHOLD` of Z's range. If multiple qualify,
   pick the one with smallest absolute distance from F to the nearest
   boundary.

2. If found, slide Z:
   ```
   Z.low  = max(0, F - CNR3_HOT_ZONE_BACK_RADIUS)
   Z.high = F + CNR3_HOT_ZONE_FORWARD_RADIUS
   Z.last_observed_frame = F
   ```
   Increment `slide_count` if bounds moved, or `hit_count` if not.

3. If no Z within threshold, this is a jump event. Allocate a new zone
   (Section 4.2.3).

#### 4.2.2 Why sliding is safe in both modes

Safety relies on two mechanisms working together:

- **`addFrameRef` discipline (Section 4.7)** — a walk that has already
  looked up a predecessor holds a caller-owned reference. Even if a slide
  moves the zone past that predecessor and a prune evicts the cache slot,
  the walk's owned reference keeps the underlying VSFrame alive.

- **Rolling predecessor reference pattern (Section 4.5.1)** — walks hold
  an owned reference to the current predecessor and transition ownership
  across iterations without ever holding zero references to the current
  predecessor.

Under normal sequential linear encoding in fmParallelRequests, the working
spread of concurrent requests is much smaller than `BACK_RADIUS=50`. With
16–32 threads and jitter ~6, the spread is ~22–38 frames. The slid zone
always covers all in-flight predecessor needs.

The pathological case — a walk stalls while concurrent walks slide the
zone forward — produces extra recovery work (checkpoint recovery), not
incorrect output. The diagnostic counter `predecessor_missing_when_expected`
detects this empirically. Non-zero triggers mandatory non-checkpoint
pinning (Section 4.4).

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
3. If no free slot: attempt retirement (4.2.4). If retirement fails,
   merge the two closest zones (4.2.5).

#### 4.2.4 Retirement (mode-specific)

**fmUnordered (eager retirement):**

When a new `arInitial` fires, the previous `arAllFramesReady` has
completed. All stale zones can be retired immediately before allocating
a new zone. In practice fmUnordered typically operates with at most one
active zone, which slides forward in normal linear operation and is
replaced (retire + new alloc) on a jump.

**fmParallelRequests (lazy retirement):**

Multiple requests may be in flight. A zone is eligible for lazy
retirement only when:
- No live frames remain in either pool within its `[low, high]` range,
  AND
- No pinned checkpoint exists within its range (conservative proxy for
  "no recovery walk is in progress within this zone").

Retirement is attempted only when a new zone allocation needs a free
slot and none is available. Spurious retention is safe; the cost is
modest increase in zone-slot pressure.

If the lazy proxy proves too conservative (zones never retire, slots
fill, merges happen too frequently), add `active_request_count` per zone
(incremented at `arInitial`, decremented at `arAllFramesReady`/`arError`)
for exact retirement eligibility.


**fmParallel (full multithreading):**

fmParallel is only potentially out of scope for early to late iteratative development
in terms of coding.
    
Important Notes (repeated in another section):
    To be very clear, **fmParallel** is specifically the final operational target,
    hence design and coding should lead toward running safe under fmParallel although
    iterative design/development may pass through fmUnordered and fmParallelRequests
    as interim stepping stones.
    It is conjectured that the cache design is, with correct mutexes in the correct 
    places, orders and depths, compatible with fmParallel; alignment with this will
    be subject to later review.
  
#### 4.2.5 Zone merge

When all slots are full and no zone is eligible for retirement:

1. Find the two zones with the smallest gap between boundaries.
2. Merge:
   ```
   merged.low  = min(Z1.low,  Z2.low)
   merged.high = max(Z1.high, Z2.high)
   merged.last_observed_frame = max(Z1.last_observed_frame,
                                     Z2.last_observed_frame)
   ```
3. Mark one slot inactive; store merged in the other.
4. Increment `merge_count`.
5. Use the freed slot for the new zone allocation.

Merging is conservative — no frames lose protection. Log the merge
action including zone frame ranges for diagnostics.

### 4.3 Pruning Policy — Hot Zone Aware (Phase-Guarded)

#### 4.3.1 Non-Checkpoint Pool Pruning

**Phase A — before non-checkpoint pinning exists (Phases CMS02-D
through CMS02-H):**

1. Call `retire_cold_hot_zones_externally_locked` (lazy retirement
   pass, mode-aware).
2. Collect non-checkpoint frames whose frame number falls outside every
   active hot zone's `[low, high]` range. These are eviction candidates.
3. Among candidates, find the one with the greatest minimum distance
   from any active hot zone boundary. Evict it via the single remove
   helper (RC2).
4. Repeat until `non_checkpoint_pool.size() <= CNR3_OUTPUT_CACHE_CAPACITY`
   or no candidates remain.
5. If no candidates remain and the pool exceeds the overflow limit, do
   not evict further. The pool may temporarily exceed the soft target,
   up to the hard ceiling.

**Phase B — after non-checkpoint pinning (Phase CMS02-I or later,
only if promotion criteria are met):**
Identical to Phase A except step 2 also requires `pin_count == 0`.

For typical pool sizes (100–300 frames), a linear scan to find the
maximum-distance candidate on each iteration is acceptable.

#### 4.3.2 Checkpoint Pool Pruning

Apply hot-zone-aware filtering with the existing `pin_count` check:

1. A checkpoint is a candidate if: frame number ≠ 0 (frame 0 is never
   pruned), `pin_count == 0`, AND frame number falls outside every
   active hot zone's `[low, high]`.
2. Evict the candidate with the greatest distance from any hot zone
   boundary first (via the single remove helper).
3. Continue until `checkpoint_pool.size() <= CNR3_CHECKPOINT_MIN_RETAIN`
   or no candidates remain.

Checkpoints within hot zones or pinned are retained regardless of
`CNR3_CHECKPOINT_MAX_RETAIN`. The retain limits are soft triggers for
when prune runs, not hard caps on what can be retained.

**Why checkpoints are retained longer:** Checkpoints anchor recovery
chains. A checkpoint inside or near an active hot zone may be needed as
a recovery anchor. With `BACK_RADIUS=50` and `CHECKPOINT_INTERVAL=10`,
a hot zone holds at most 5 checkpoints. With 5 active zones, up to 25
checkpoints could be protected — well within `MAX_RETAIN=32`.

### 4.4 Non-Checkpoint Frame Pinning — Deferred

Non-checkpoint pinning remains the deterministic solution to FM1 if
hot-zone-aware pruning is shown to be insufficient. The first
implementation defers non-checkpoint pinning.

**Mandatory promotion criterion:**

Non-checkpoint pinning becomes mandatory — not optional, not deferred
further — if any of the following are observed during realistic test runs:

- `predecessor_missing_when_expected` is non-zero in any realistic
  VHS/VHS-C encode test.
- Recovery repeatedly recomputes frames that were recently cached.
- fmParallelRequests testing reveals a race between prune and active
  predecessor use that hot zones and `addFrameRef` do not cover.
- Hot-zone settings required to prevent the above become so broad they
  defeat pruning (e.g., back radius must exceed 200 to be safe).

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
   deliberate approximation — documented trade-off)
output[start+1..N] computed using fill-holes-only + rolling predecessor
```

#### 4.5.1 Rolling Predecessor Reference Pattern

A recovery walk maintains an owned reference to the current predecessor
through each iteration. The reference is transitioned (not
released-then-reacquired) when moving from iteration K to K+1.

```text
prev_ref = nullptr

if cache hit at start:
    prev_ref = find_frame_and_add_ref(start)
else:
    new_frame = initialise_from_source(start)  // source-copy semantics
    store_frame(start, new_frame)
       // cache takes its own addFrameRef
    prev_ref = new_frame
       // walk retains original computed ref

for K in start+1 .. N:
    cached = find_frame_and_add_ref(K)
    if cached != nullptr:
        // Fill-holes-only: cache hit, skip computation
        freeFrame(prev_ref)
        prev_ref = cached
        continue

    // Cache miss: compute output[K] from prev_ref
    new_frame = compute_blend(prev_ref, source[K])
    store_frame(K, new_frame)
       // cache may take independent addFrameRef (first-in-best-dressed:
       // if already cached by concurrent walk, store returns success
       // without taking addFrameRef)
    freeFrame(prev_ref)
    prev_ref = new_frame
       // walk retains its own computed ref

// end of loop: see Section 4.5.2 for final-frame handling
```

**Key properties:**
- Walk always holds exactly one owned reference to the current
  predecessor.
- New predecessor acquired before old one released — no zero-ref window.
- Concurrent prune evicting a cache slot cannot damage the walk.
- Pattern works identically in fmUnordered and fmParallelRequests.

**On error paths:** if any step fails (`store_frame` returns false at
ceiling, blend fails, etc.), the walk must `freeFrame(prev_ref)` and
`freeFrame(new_frame)` (if held) before returning. The
`Cnr3OwnedFrameRef` RAII wrapper handles this automatically.

#### 4.5.2 Final-Frame Ownership Transfer

At the end of a recovery walk, `prev_ref` normally owns the last output
frame computed or found by the walk. Two cases must be distinguished:

**Case A — `prev_ref` is an intermediate predecessor only:** The walk
has computed output[N] and stored it. `prev_ref` holds the last
predecessor in the rolling sequence. Call `freeFrame(prev_ref)`.

**Case B — `prev_ref` is the requested output frame N being returned
to VapourSynth via `getFrame`:** Ownership transfers when the walk
returns the VSFrame pointer. The walk must NOT call `freeFrame(prev_ref)`
after the transfer — that would be a double-free.

Required idiom using `Cnr3OwnedFrameRef::release()`:

```cpp
if (prev_ref_is_requested_output_frame) {
    // Transfer ownership to VapourSynth.
    // Wrapper destructor will NOT freeFrame.
    return prev_ref.release();
}
// else: RAII destructor calls freeFrame at scope exit.
```

This idiom makes the transfer auditable and prevents accidental
double-free.

### 4.6 Hard Ceiling and Abort Policy — Byte-Budget Based

#### 4.6.1 Ceiling Calculation

At `cnr3_create()` time, after `Cnr3Data` construction:

```cpp
static int cnr3_output_cache_subsampled_dimension(int full_size,
                                                   int subsampling_shift)
{
    // ceil(full_size / (1 << subsampling_shift))
    // Conservative for odd dimensions; never underestimates.
    return (full_size + ((1 << subsampling_shift) - 1)) >> subsampling_shift;
}

int64_t estimated_frame_bytes = 0;
const int bytes_per_sample = (vi->format.bitsPerSample + 7) / 8;
const int sub_w = vi->format.subSamplingW;
const int sub_h = vi->format.subSamplingH;
for (int p = 0; p < vi->format.numPlanes; ++p) {
    const int pw = (p == 0) ? vi->width
                             : cnr3_output_cache_subsampled_dimension(
                                   vi->width, sub_w);
    const int ph = (p == 0) ? vi->height
                             : cnr3_output_cache_subsampled_dimension(
                                   vi->height, sub_h);
    estimated_frame_bytes += (int64_t)pw * ph * bytes_per_sample;
}
const int candidate_ceiling =
    (int)(CNR3_CACHE_BYTE_BUDGET / estimated_frame_bytes);
const int active_ceiling = std::clamp(candidate_ceiling,
                                       CNR3_CACHE_MIN_HARD_CEILING,
                                       CNR3_CACHE_MAX_HARD_CEILING);
```

This formula works for accepted planar YUV formats (4:2:0, 4:2:2, 4:4:0,
4:4:4). CNR3 accepts three-plane YUV only; grey is not in scope.

**Important disambiguation:** Standard PAL VHS source is 720×576, not
1440×576. At 1 GiB budget, 720×576 YUV420P8 gives candidate_ceiling ≈
1,726, correctly clamped to 1000. A genuine 1440×576 YUV420P8 clip gives
≈ 863, not clamped. If a log shows `active_ceiling=1000` for a clip
described as 1440×576, confirm the actual clip dimensions — it is likely
720×576. No code error exists in the ceiling calculation.

**Worked examples (`CNR3_CACHE_BYTE_BUDGET = 1 GiB`):**

| Format                | bytes/frame | candidate | active (clamped) |
|---|---|---|---|
| 4:2:0 8-bit  720×576  | 622,080     | 1,726     | **1000** (MAX)   |
| 4:2:2 8-bit  720×576  | 829,440     | 1,294     | **1000** (MAX)   |
| 4:2:0 16-bit 720×576  | 1,244,160   | 863       | 863              |
| 4:2:2 16-bit 720×576  | 1,658,880   | 647       | 647              |
| 4:2:0 8-bit  1920×1080| 3,110,400   | 344       | 344              |
| 4:2:0 16-bit 1920×1080| 6,220,800   | 172       | 172              |
| 4:2:0 8-bit  1440×576 | 1,244,160   | 863       | 863              |

#### 4.6.2 Abort Policy

A store is allowed if `total_live_refs_after_store <= active_ceiling`.
Rejected if it would cause `total_live_refs_after_store > active_ceiling`
AND prune cannot free any frame.

When rejected:
1. Store returns `false` without taking `addFrameRef` (RC3 preserved).
2. Increment `cache_ceiling_hard_aborts`.
3. The calling `getFrame` path executes cleanup discipline (4.6.3) then
   returns a VS filter error:
   *"CNR3: cache ceiling reached ([N] frames). CNR3 is designed for
   near-linear access. Large random seeks in rapid succession may exceed
   cache capacity."*

`cnr3_output_cache_would_exceed_ceiling_externally_locked()` returns true
iff a subsequent store would cause
`total_live_refs_after_store > active_ceiling`.

#### 4.6.3 Cleanup Discipline on Ceiling Abort

A ceiling abort may leave already-successfully-stored frames in the cache
(those frames are valid outputs). The failure path must not leave any
temporary runtime state unreleased:

- Any checkpoint pinned during the failed recovery: unpin exactly once.
- Any caller-owned VSFrame references held by the recovery walk
  (including `prev_ref`): `freeFrame` exactly once.
- Any source frame obtained from VapourSynth: release.
- Any destination frame allocated but not returned: release.
- Hot zone state: no rollback. A zone allocated for a failed request will
  be retired naturally when its range becomes cold.

After cleanup, reference counters remain balanced.

### 4.7 addFrameRef and Pinning — Separate Concerns

The cache manager has three distinct protection mechanisms:

| Mechanism | Protects against | Holder | Lifetime |
|---|---|---|---|
| **Hot zone membership** | Slot eviction by prune | Cache manager | Until zone slides/retires past frame |
| **Checkpoint `pin_count`** | Slot eviction by prune (checkpoints) | Recovery walks | Pin/unpin pair on every path |
| **`addFrameRef`** | Underlying VSFrame being freed | Caller of lookup helper | Until `freeFrame` by caller |

Hot-zone protection and `pin_count` are prune-side mechanisms. They
prevent the cache from evicting the slot. `addFrameRef` is independent
— it ensures the VSFrame's pixel data remains valid for the holder even
if the cache decides to evict.

Key insight:
> `addFrameRef` protects a frame *already found*. It does not make a
> missing predecessor appear.

The rolling predecessor reference pattern (Section 4.5.1) addresses this
by ensuring the walk always holds a ref to its current predecessor before
it would be needed as input.

### 4.8 arInitial vs. Cache-Hit Hot Zone Update

Hot zone updates fire at `arInitial` for every arriving frame request,
regardless of whether the request later results in a cache hit or
computation. This is deliberate: a cache hit still represents activity
near that frame number, and the hot zone should reflect the request
stream's working position.

**Note:** The current code updates hot zones in `arAllFramesReady`
rather than `arInitial`. Under fmUnordered this makes no difference to
correctness or zone behaviour. The call must be moved to `arInitial`
before wiring fmParallelRequests. See Section 14.4.

Diagnostic counters distinguish:
- `hot_zone_updates_at_arInitial` — total `arInitial` calls that
  updated a hot zone.
- `cache_hits_at_arAllFramesReady` — requests served from cache without
  computation.
- `recoveries_started_at_arAllFramesReady` — requests requiring a
  recovery walk.

### 4.9 First-In-Best-Dressed Store Idempotency

`cnr3_output_cache_store_frame()` is idempotent by frame number.

When storing `output[N]`:

1. Lock the cache mutex.
2. Check `cache_index` for frame number `N`.
3. **If already exists (duplicate-store case):**
   - Do not replace the existing cached frame.
   - Do not take an additional cache-owned `addFrameRef`.
   - Do not mutate either pool.
   - Do not disturb the existing checkpoint/non-checkpoint classification.
   - Increment `store_skipped_already_cached`.
   - Increment `duplicate_store_computed_but_discarded`.
   - Return success. Cache has not taken ownership of caller's frame.
   - Caller still owns its computed frame and must release or transfer
     it according to normal caller-side ownership rules.
4. **If does not exist (normal case):**
   - Check ceiling (Section 4.6.2). If would exceed AND prune cannot
     free: increment `cache_ceiling_hard_aborts`, return false without
     taking `addFrameRef` (RC3 preserved).
   - `addFrameRef(frame)`. Increment `cache_addframeref_total`.
   - Insert into the appropriate pool. Update `cache_index`.
   - If insertion fails: `freeFrame(frame)`, increment
     `cache_freeframe_total` (RC3 rebalance). Return false.
   - Run `prune_after_store`.
   - Return success.

**Why idempotency matters:** Without it, two overlapping recovery walks
both computing `output[N]` and racing to store would either overwrite
(leaking the first frame's cache-side ref) or double-reference the same
slot. Either breaks the ref-count invariant (FM7). With idempotency,
the first store wins; the second is a no-op from the cache's perspective.

**Diagnostic interpretation:** High `duplicate_store_computed_but_discarded`
indicates overlapping recovery walks are common. This is a candidate for
later duplicate-work suppression. Not implemented in this iteration.

---

## 5. Constants

```
// --- Soft pruning targets ---

CNR3_OUTPUT_CACHE_CAPACITY        = 100
CNR3_OUTPUT_CACHE_OVERFLOW_FACTOR = 1.1
    Overflow threshold = 110 frames.
    Prune fires when pool size strictly exceeds 110 (i.e. at 111+).

// --- Hard ceiling (byte-budget based) ---

CNR3_CACHE_BYTE_BUDGET            = 1024 * 1024 * 1024  // 1 GiB
    Nominal byte budget used only at filter creation to derive
    active_ceiling. Runtime cache limit is a simple frame count.
    The 1 GiB value provides headroom for recursive recovery history
    under VapourSynth request jitter and modest seeks.

CNR3_CACHE_MIN_HARD_CEILING       = 150 frames
    Lower bound on active_ceiling. Intentional even if large formats
    exceed the nominal byte budget, because the recursive recovery
    design needs enough frame history to be useful and safe.

CNR3_CACHE_MAX_HARD_CEILING       = 1000 frames

// --- Checkpoints ---

CNR3_CHECKPOINT_INTERVAL          = 10
    Promote every 10th frame (and frame 0) to checkpoint pool.

CNR3_CHECKPOINT_MAX_RETAIN        = 32
    Soft trigger: checkpoint prune runs when pool exceeds this.
    Checkpoints in hot zones are retained regardless of this limit.

CNR3_CHECKPOINT_MIN_RETAIN        = 10
    Prune back to this count when eligible candidates exist.

// --- Hot zones ---

CNR3_HOT_ZONE_FORWARD_RADIUS      = 10
    Frames ahead of hot zone high boundary protected from pruning.
    Supported by simulation: covers p99 BestSource jitter up to
    approximately jitter_max=8.

CNR3_HOT_ZONE_BACK_RADIUS         = 50
    Frames behind hot zone low boundary protected from pruning.
    Covers 5 checkpoint intervals of backward history.
    Subject to empirical tuning after Phase CMS02-E.

CNR3_MAX_HOT_ZONES                = 5
    Maximum simultaneous active hot zones. Covers 1 linear encoding
    zone plus up to 4 concurrent jump zones.

CNR3_HOT_ZONE_JUMP_THRESHOLD      = FORWARD_RADIUS + BACK_RADIUS + 1
                                  = 61
    A new frame request allocates a new hot zone if it falls outside
    all existing zones by more than this threshold. Within this
    distance, the nearest zone is slid instead.
```

---

## 6. Diagnostics — Definitive Counter Specification

All counters are `int64_t` members of `Cnr3OutputCacheStats`.

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
- `non_checkpoint_prune_skipped_pinned`
  (RESERVED — only meaningful after Phase CMS02-I if pinning added)
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
- `predecessor_missing_when_expected`
  *** CRITICAL — non-zero triggers mandatory non-checkpoint pinning
  promotion (Section 4.4). Prominent warning regardless of gate setting.
- `checkpoint_missing_when_expected`
- `max_live_cached_frames_observed`
- `max_non_checkpoint_pool_size_observed`
- `max_checkpoint_pool_size_observed`

**Store/duplicate counters:**
- `store_skipped_already_cached`
- `duplicate_store_computed_but_discarded`
  (verbose name deliberate — makes clear computation was wasted even
  though cache state remained correct)

**Cache-side reference-count counters:**
- `cache_addframeref_total`
- `cache_freeframe_total`
- Invariant check at quiescence and shutdown:
  `cache_addframeref_total - cache_freeframe_total ==
   non_checkpoint_pool.size() + checkpoint_pool.size()`

**Caller-side reference-count counters (development diagnostic):**
- `lookup_owned_ref_acquired_total`
  (incremented inside `find_frame_and_add_ref` on success)
- `lookup_owned_ref_released_total`
  (incremented when a caller-owned lookup reference is `freeFrame`d)
- `lookup_owned_ref_transferred_total`
  (incremented when a caller-owned lookup reference is transferred to
  VapourSynth or another owner via `release()`)
- Caller-side diagnostic invariant at quiescent points:
  `acquired == released + transferred`
  Not part of the cache-side invariant; caller-side discipline only.

**Debug output policy:**
- Default: headline counters at `cnr3_free`.
- Dev diagnostics enabled: all counters plus hot zone state.
- Per-event verbose output guarded behind
  `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS`.
- **No diagnostic output to stdout under any circumstances.**
- `predecessor_missing_when_expected > 0` prints prominent warning
  regardless of diagnostic gate setting.
- Ref-balance failure at shutdown prints prominent warning regardless
  of diagnostic gate setting.

---

## 7. Worked Examples

### Example A — Linear Encoding, No Jumps (fmUnordered, sliding)

Setup: 32-thread encode, BestSource jitter up to 6 frames. Cache empty.

| Arrival | Action | Zone 0 after | Pool size |
|---|---|---|---|
| F=0   | Allocate zone 0 | low=0, high=10 | 1 |
| F=50  | Slide | low=0, high=60 | ~50 |
| F=60  | Slide | low=10, high=70 | ~60 |
| F=100 | Slide | low=50, high=110 | ~100 |
| F=110 | Slide | low=60, high=120 | ~110 |
| F=111 | Slide; **prune fires** (111 > 110) | low=61, high=121 | back to ~100 |

Prune candidates: frames outside `[61, 121]`. Evict furthest-from-zone
first via single remove helper (RC2).

Predecessor for current computation always within zone. No predecessor
failures.

### Example B — Small Forward Jump Within JUMP_THRESHOLD

Setup: at F=80, zone 0 = `low=30, high=90`. F=95 arrives.

Distance from zone high (90): 5 frames. 5 ≤ 61. Within threshold.
Slide zone 0: `low = max(0, 95-50) = 45`, `high = 95+10 = 105`.

Cache lookup: output[94] may be present (hit) or absent (miss). On hit,
walk uses it via `find_frame_and_add_ref`. On miss, nearest checkpoint
found and pinned, rolling-predecessor walk fills holes, checkpoint
unpinned on every exit path.

### Example C — Large Forward Jump (fmParallelRequests, sliding)

Setup: sequential encoding at F=200 with three in-flight walks (200,
201, 202). Zone 0 last slid to F=202: `low=152, high=212`.

F=600 arrives. Distance from zone 0 high (212): 388 > 61. Jump detected.
Allocate zone 1: `low=550, high=610`.

Zone 0 remains active. In-flight walks for 200/201/202 have predecessors
within `[152, 212]`. Each walk holds `prev_ref` for its current
predecessor (Section 4.5.1). Even if prune fires and evicts a cache
slot, the walk's owned reference keeps the underlying VSFrame alive.

Bounded recovery for F=600: `start = max(0, 600-100) = 500`. Walk uses
rolling predecessor pattern. Frames 500–549 stored but outside both
zones — prunable. Walk has already transitioned `prev_ref` past those
frames; prune evicting them does not corrupt the walk.

Pre-jump walks complete. Zone 0 lazy-retired when no live frames remain
in its range and no pinned checkpoint is within it. Steady state: one
active zone around F=600.

### Example D — No-Prior-Checkpoint Recovery (fmUnordered, cold seek)

Setup: fresh instance, user seeks to F=800. Cache empty.

F=800 arrives. No active zones. Allocate zone 0: `low=750, high=810`.

`find_and_pin_nearest_prior_checkpoint` returns false. Increment
`no_prior_checkpoint_recovery_count`.

Bounded warm-up:
- `start = max(0, 800-100) = 700`
- output[700] = source-copy initialisation (deliberate approximation;
  no predecessor available)
- `prev_ref` = the walk's own ref to output[700]
- For K = 701..800: fill-holes-only walk using rolling predecessor
- At K=800: `prev_ref` holds the requested output frame. Return via
  `prev_ref.release()` — ownership transfers to VapourSynth.

Subsequent seeks to 700..800 range use checkpoints as anchors.
Cold-start approximation is confined to output[700].

### Example E — Hard Ceiling Abort

Setup: 16-bit 4:2:2 input. `estimated_frame_bytes ≈ 1.66 MB`.
`active_ceiling = floor(1 GiB / 1.66 MB) ≈ 647` (clamped to `[150,
1000]`).

User makes 7 rapid large seeks before any recovery completes. Seven
concurrent recovery walks each generating ~101 frames = ~707 total
demand. Ceiling = 647.

After ~647 stores, the next store would cause
`total_live_refs_after_store > 647`. Ceiling check fires. Prune: no
candidates (all frames within active hot zones). Store returns false
without taking `addFrameRef` (RC3). Increment `cache_ceiling_hard_aborts`.

Cleanup (Section 4.6.3): walk `freeFrame`s `prev_ref` and any other
held references, unpins any pinned checkpoint, releases any
source/destination frames. VS filter error returned. Filter remains
valid. Reference counters balanced.

### Example F — Fill-Holes-Only Avoiding Redundant Compute

Setup: output[150] already cached. Recovery walk from checkpoint[145]
toward target N=155 reaches K=150.

`find_frame_and_add_ref(150)`: cache hit. Increment
`recovery_frames_skipped_already_cached` and
`lookup_owned_ref_acquired_total`. Walk transitions: `freeFrame(prev_ref)`
(which held output[149]; increment `lookup_owned_ref_released_total`),
then `prev_ref = cached_150`. Skip computation. Continue loop to K=151.

### Example G — Duplicate Store Race (first-in-best-dressed)

Setup: two concurrent recovery walks A and B both compute output[200]
and race to store.

Walk A's `store_frame(200, frameA)` acquires mutex first:
- `cache_index[200]` does not exist → normal store path.
- `addFrameRef(frameA)`. Insert into pool. Update index.
- Increment `cache_addframeref_total`. Return success.
- Walk A then `freeFrame(frameA)` (its walk-side ref released).
- Cache holds the only ref to frameA.

Walk B's `store_frame(200, frameB)` acquires mutex:
- `cache_index[200]` now exists (frameA is the source of truth).
- First-in-best-dressed: return success without taking `addFrameRef`.
- Increment `store_skipped_already_cached` and
  `duplicate_store_computed_but_discarded`.
- Walk B then `freeFrame(frameB)` (its walk-side ref released).
- frameB's underlying VSFrame is destroyed. frameA remains in cache.

Reference counters balanced throughout. No leak, no double-free.

---

## 8. Phased Implementation Sequence

**Current state:** Phases CMS02-A through CMS02-E are complete. See
Section 14 for exact completion status and rename status.

#### Phase CMS02-A — Documentation, constants, renames
**Status: Partially complete.** See Section 14.2 for what remains.
Remaining work: update stale comment in `cnr3_output_cache_manager.h`
(Section 13.2).

#### Phase CMS02-B — Hot zone structures and passive diagnostics
**Status: Complete.**

#### Phase CMS02-C — Hot zone update helpers
**Status: Complete.** Hard-wired `FmUnordered`. All zone lifecycle
events instrumented.

#### Phase CMS02-D — Hot zone aware prune candidate selection
**Status: Complete.** Hot-zone-aware candidate selection in prune loops.
Ceiling check and hard abort in store. First-in-best-dressed idempotency.
**Design Compliance Review: required before CMS02-E was started.**

#### Phase CMS02-E — Store/prune-only runtime proving
**Status: Complete.** Frames stored into `output_cache` after existing
strict-streaming path. `prune_after_store` called. Output cache not yet
output-authoritative. Ref-balance counters confirmed working.
**Design Compliance Review: complete.**

#### Phase CMS02-F — Cache-hit reuse under fmUnordered
**Status: Not started.**
- Implement `cnr3_output_cache_find_frame_and_add_ref()` per Section
  9.2.H.
- At the start of `cnr3_get_frame` arAllFramesReady, check cache hit.
  If hit: return cached frame via caller-owned ref using
  `Cnr3OwnedFrameRef`. If miss: proceed with normal computation.
- Instrument `cache_hits_at_arAllFramesReady` and caller-side lookup-ref
  counters at call sites.
**End-of-phase: Design Compliance Review (mandatory before CMS02-G).**

#### Phase CMS02-G — Checkpoint recovery and hole filling (fmUnordered)
**Status: Not started.**
- `find_and_pin_nearest_prior_checkpoint` for out-of-order requests.
- Implement rolling predecessor reference pattern (Section 4.5.1).
- Implement final-frame ownership transfer (Section 4.5.2).
- Fill missing frames ascending using fill-holes-only rule.
- Skip frames already present via first-in-best-dressed store.
- Unpin checkpoint on every exit path.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-H — Bounded warm-up recovery (fmUnordered)
**Status: Not started.**
- Implement no-prior-checkpoint warm-up per Section 4.5.
- Instrument all warm-up counters.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-I — Empirical review: non-checkpoint pinning decision
After realistic VHS/VHS-C encode tests and synthetic jump tests:
- Inspect all Section 6 counters, especially
  `predecessor_missing_when_expected` and ref-balance counters.
- If mandatory promotion criteria (Section 4.4) are met: implement
  non-checkpoint pinning before proceeding.
- If criteria are not met: document findings and proceed to CMS02-J.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-J — fmParallelRequests wiring and proving
Only after CMS02-H proven and CMS02-I decision made.
- Move `cnr3_output_cache_update_hot_zones()` call to `arInitial`
  (prerequisite — see Section 14.4).
- Wire fmParallelRequests path: same sliding update helper (already
  implemented), retirement helper invoked with `FmParallelRequests`.
- Test concurrent jump scenarios.
- Validate retirement with pinned-checkpoint proxy.
- Decide on `active_request_count` per zone if retirement proves too
  conservative.
**End-of-phase: Design Compliance Review.**

#### Full fmParallel — explicitly out of scope for this iteration.

---

## 9. Structural Changes — Current Naming

### 9.1 Current Active Naming

**[CONFIRMED CMS06 — derived from reading uploaded source files. The
inline document versions (vapoursynth-Cnr3.cpp, cnr3_output_cache_manager
.h/.cpp, cnr3_common.h) are the authoritative current state. The
disk-uploaded cnr3_cache_manager.h/.cpp and cnr3_common.h are pre-rename
versions and are superseded.]**

**Types:**

```cpp
Cnr3OutputCacheManager      // formerly Cnr3CacheManagerV005
OldCnr3StrictStreamCache    // formerly Cnr3CacheManager
Cnr3OutputCacheStats        // in cnr3_output_cache_manager.h
Cnr3CheckpointSlot          // unchanged
Cnr3HotZone                 // CMS02-B addition
Cnr3CacheSchedulingMode     // CMS02-C addition
Cnr3OwnedFrameRef           // recommended RAII wrapper, not yet in code
```

**`Cnr3Data` members:**

```cpp
OldCnr3StrictStreamCache old_strict_cache;  // formerly: cache
Cnr3OutputCacheManager   output_cache;      // formerly: cache_manager_v005
```

**Helper function prefixes:**

```cpp
cnr3_output_cache_*     // active prefix for output cache helpers
old_cnr3_strict_cache_* // active prefix for old strict cache helpers
```

**Files:**

```
cnr3_output_cache_manager.h    // formerly cnr3_cache_manager.h
cnr3_output_cache_manager.cpp  // formerly cnr3_cache_manager.cpp
old_cnr3_strict_cache.h        // formerly cnr3_cache.h (approx)
cnr3_common.h                  // updated to use new names
vapoursynth-Cnr3.cpp           // updated to use new names
cnr3_build_config.h            // CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS now defined
```

**Old naming — do not use in new code:**

```cpp
Cnr3CacheManagerV005          // use Cnr3OutputCacheManager
Cnr3CacheManager              // use OldCnr3StrictStreamCache
cache_manager_v005            // use output_cache
cache (in Cnr3Data)           // use old_strict_cache
cnr3_cache_manager_*          // use cnr3_output_cache_*
CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS  // use CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS
v005 in comments              // use CMS05 or current phase reference
```

### 9.2 New Helpers Required (Phases CMS02-F onward)

**H. Cache hit lookup — defensive contract:**

`cnr3_output_cache_find_frame_and_add_ref(cache, frame_number, vsapi)`:

1. Lock `cache_mutex`.
2. Find `frame_number` in `cache_index`. If not found: increment
   `cache_misses`, unlock, return nullptr.
3. Verify the indexed owner pool actually contains `frame_number`. If
   not: increment `cache_index_inconsistency_detected`, log warning,
   unlock, return nullptr.
4. Verify the stored `VSFrame*` is non-null. If null: same as step 3.
5. Call `vsapi->addFrameRef(frame)`. Increment
   `lookup_owned_ref_acquired_total`.
6. Increment `cache_hits`.
7. Unlock `cache_mutex`.
8. Return the caller-owned `VSFrame*`.

Caller must `freeFrame` the returned reference on every exit path (RC5).
Use of `Cnr3OwnedFrameRef` at the call site is strongly recommended.

### 9.3 Failure-Path and Shutdown Discipline

Every failure path returning a VS error must execute cleanup:

1. Any checkpoint pinned during this operation: `unpin_checkpoint` once.
2. Any caller-owned VSFrame references: `freeFrame` once. Use
   `Cnr3OwnedFrameRef` RAII to make this automatic.
3. Any source frame references obtained from VapourSynth: `freeFrame`.
4. Any destination frame allocated but not returned: `freeFrame`.
5. Hot zone state: no rollback needed.
6. Diagnostic counters: still incremented as appropriate.

**Shutdown `clear()` (already implemented and verified):**

1. Acquire `cache_mutex`.
2. Iterate `non_checkpoint_pool`: `freeFrame` each frame, increment
   `cache_freeframe_total`, erase slot.
3. Iterate `checkpoint_pool`: if `pin_count > 0`, log warning (logical
   leak indicator); `freeFrame` each frame, increment
   `cache_freeframe_total`, erase slot.
4. Clear `cache_index`. Reset hot zones to inactive.
5. Run `validate_invariants` to confirm ref-balance.

---

## 10. Known Hazard Addressed by Hot-Zone Lifecycle Rules

**Hazard scenario (retained as canonical test case):**

> 1. Request A is computing output[450].
> 2. It needs output[449] as the predecessor.
> 3. output[449] is present in the non-checkpoint pool.
> 4. A forward jump creates a new hot zone around output[800].
> 5. The old hot zone around output[450] is shifted, merged, or retired.
> 6. Pruning sees output[449] as outside all hot zones and removes it.
> 7. Request A no longer has the predecessor it expected.

**Resolution by CMS06.1-or-later design:**

- **Step 5 in fmUnordered:** The previous request has completed before
  this `arInitial` fires. Request A is not in flight at the time of the
  jump.

- **Step 5 in fmParallelRequests:** Lazy retirement (pinned-checkpoint
  proxy) prevents premature retirement of zones associated with active
  recovery walks. Sliding can occur, but the rolling predecessor
  reference pattern (Section 4.5.1) ensures Request A's walk already
  holds an owned reference to output[449] before any concurrent pruning
  could fire.

- **Step 6:** Even if a slide moves the zone past output[449] and a
  subsequent prune evicts the cache slot, the walk's owned `addFrameRef`
  reference keeps the underlying VSFrame alive until the walk releases
  it. Pruning removes the cache slot; the VSFrame is destroyed only when
  its reference count reaches zero, which the walk's reference prevents.

- **Runtime verification:** `predecessor_missing_when_expected` is the
  empirical detector. If it ever increments, non-checkpoint pinning
  becomes mandatory before further work (Section 4.4).

---

## 11. Items to Confirm Empirically

**EI1 — Lazy retirement proxy under fmParallelRequests.**
Accept the pinned-checkpoint proxy initially. If zones never retire, add
`active_request_count` per zone for exact retirement eligibility.

**EI2 — `BACK_RADIUS = 50` empirical sufficiency.**
Observe `recovery_frames_computed` and `cache_hits_at_arAllFramesReady`.
Increase if pruning proves too aggressive; decrease if pool stays near
ceiling under normal linear encoding.

**EI3 — Checkpoint retain values `MAX=32`, `MIN=10`.**
Observe `max_checkpoint_pool_size_observed`. Adjust if frequently hitting
the soft trigger during normal linear encoding.

**EI4 — Ceiling abort frequency.**
`cache_ceiling_hard_aborts` should be zero during normal linear encoding.
Any non-zero value during realistic VHS encode tests warrants review of
the byte budget or `CNR3_MAX_HOT_ZONES` constant.

**EI5 — `prune_no_candidate_exists` behaviour.**
Accept "do not evict protected frames" initially. If it increments
significantly during realistic tests, reduce `BACK_RADIUS` or
`CNR3_MAX_HOT_ZONES`, or add a fallback eviction policy.

**EI6 — Non-checkpoint pinning promotion decision (Phase CMS02-I).**
The most important empirical decision in the implementation sequence.
Mandatory criteria in Section 4.4.

**EI7 — Cache-side reference-count balance.**
`cache_addframeref_total - cache_freeframe_total` must equal total live
slots at quiescence and zero at shutdown after `clear()`. Deviation is
a real bug, not a tolerable approximation.

**EI8 — Caller-side reference-count balance.**
`lookup_owned_ref_acquired_total == released + transferred` at quiescent
points. Essential while caller-owned refs are load-bearing under the
deferred-pinning regime.

**EI9 — Duplicate-store frequency.**
`duplicate_store_computed_but_discarded` indicates overlapping recovery
walk computation waste. If high, future active-computation tracking or
duplicate-work suppression may be justified.

---

## 12. Design Compliance Review

### 12.1 Required coding rule

After completing each implementation phase (Section 8) or a coherent
block of phases, perform a design-compliance review of all changed code
paths and all unchanged helper functions invoked by those changed paths.

The review must verify that the resulting execution paths follow CMS06.1-or-later,
not older pre-rename or pre-CMS05 assumptions.

### 12.2 Verification checklist

1. **Mutex ownership** is correct at every access to mutable
   cache-manager state.
2. **`_externally_locked` helpers** called only while mutex held.
3. **Public helpers** that lock internally do not call other public
   locking helpers (deadlock risk).
4. **No old prune logic** remains in the active path.
5. **No direct `pool.erase()`** bypasses the single remove helper (RC2).
6. **No direct cache-owned `addFrameRef`** bypasses the single store
   helper (RC1).
7. **Store collision** follows first-in-best-dressed semantics (RC8).
8. **Store failure after `addFrameRef`** rebalances correctly (RC3).
9. **Lookup helper** takes `addFrameRef` atomically under mutex.
10. **Caller-owned lookup references** freed or transferred exactly once
    on every exit path (RC5).
11. **Checkpoint pins** unpinned on every exit path.
12. **Hot-zone state** not rolled back on frame failure.
13. **Cache-side ref-balance invariant** holds (RC7).
14. **Caller-side lookup-ref diagnostics** balance in development mode.
15. **Shutdown `clear()`** releases every cache-owned frame reference
    and clears all indexes (RC6).
16. **No diagnostics write to stdout.**

### 12.3 Review timing

- CMS02-A and CMS02-B may be reviewed together (structures, counters,
  passive diagnostics only).
- **CMS02-D must be reviewed before CMS02-E** (first runtime use of
  new store/prune behaviour).
- **CMS02-F must be reviewed before CMS02-G** (lookup-owned references
  and rolling predecessor correctness).
- All other phases require their own review.

### 12.4 Outcome

Each review produces: **Pass**, **Pass with notes**, or **Fail** (with
specific findings and what was done about them). Brief but specific.

---

## 13. Pending Code Issues

### 13.1 `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS` in `cnr3_build_config.h`

**Status: RESOLVED.**

`cnr3_output_cache_manager.h` references `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS`
(used in `CNR3_OUTPUT_CACHE_VALIDATE_AFTER_MUTATION`). The updated
`cnr3_build_config.h` now defines:

```cpp
constexpr bool CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS =
    CNR3_DEV_DIAGNOSTICS;
```

The old constant `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS` has been removed
from `cnr3_build_config.h` and replaced by
`CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS`.

**Action required:** grep the entire source tree for
`CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS`. If any file still references the
old name, update it to `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS`.

### 13.2 Stale phase comment in `cnr3_output_cache_manager.h`

**Status: Pending — comment only, no correctness impact.**

Near the bottom of `cnr3_output_cache_manager.h`:

```cpp
// This file contains only the data structures and constants for the future
// CMS05 output-frame cache manager.
//
// Phase 1 intentionally does not change current runtime behaviour.
// The existing strict-streaming cache remains active until later phases wire
// these structures into cnr3_get_frame().
```

This is stale. Phase 3A store/prune wiring is complete; the output cache
is live (non-output-authoritative). Suggested replacement:

```cpp
// CMS06 output-frame cache manager.
//
// Phase CMS02-E complete: output cache stores and prunes real produced
// frames for proving (non-output-authoritative).
// The strict-streaming cache remains the source of returned output.
// Cache-hit reuse, recovery walks, and bounded warm-up recovery are
// not yet wired (Phases CMS02-F through CMS02-H).
```

---

## 14. Current Implementation State

**[Ground truth derived from reading uploaded source files. Update this
section at every session boundary.]**

### 14.1 Phase Completion Status

| Phase | Status | Notes |
|---|---|---|
| CMS02-A (renames, constants) | Partially complete | See Section 14.2 |
| CMS02-B (hot zone structures, passive diagnostics) | Complete | |
| CMS02-C (hot zone update helpers) | Complete | Hard-wired FmUnordered |
| CMS02-D (hot-zone-aware prune, ceiling) | Complete | |
| CMS02-E (store/prune-only runtime proving) | Complete | |
| CMS02-F (cache-hit reuse) | Not started | |
| CMS02-G (checkpoint recovery, hole filling) | Not started | |
| CMS02-H (bounded warm-up recovery) | Not started | |
| CMS02-I (pinning decision) | Not started | |
| CMS02-J (fmParallelRequests) | Not started | |

### 14.2 Rename and Build Config Completion Status

| Item | Status | Notes |
|---|---|---|
| `Cnr3CacheManagerV005` → `Cnr3OutputCacheManager` | Complete | In active files |
| `Cnr3CacheManager` → `OldCnr3StrictStreamCache` | Complete | In active files |
| `cache_manager_v005` → `output_cache` in Cnr3Data | Complete | In active files |
| `cache` → `old_strict_cache` in Cnr3Data | Complete | In active files |
| `cnr3_cache_manager_*` → `cnr3_output_cache_*` helpers | Complete | |
| `old_cnr3_strict_cache_*` prefix | In use | |
| `cnr3_cache_manager.h/.cpp` → `cnr3_output_cache_manager.h/.cpp` | Complete | |
| `cnr3_build_config.h` — `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS` | Complete | Old `CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS` removed |
| Stale phase comment in `cnr3_output_cache_manager.h` | **Pending** | Comment only — see Section 13.2 |
| Disk-uploaded `cnr3_common.h` | Old version | Uses old names; superseded by current active version |
| Disk-uploaded `cnr3_cache_manager.h/.cpp` | Old version | Superseded by `cnr3_output_cache_manager.h/.cpp` |

### 14.3 What Is Implemented and Wired (Live)

- 8/16-bit recursive chroma blend with scene-change detection.
- Luma downsampling and sharing between U/V planes.
- `OldCnr3StrictStreamCache` — active, returns output frames to
  VapourSynth. This is the current source of output authority.
- `Cnr3OutputCacheManager` — owned by `Cnr3Data`, store/prune wired
  in `cnr3_get_frame()` after the strict-streaming path produces each
  frame.
- `cnr3_output_cache_update_hot_zones()` called in `arAllFramesReady`
  (see Section 14.4 for spec deviation note).
- `cnr3_output_cache_store_frame()` and
  `cnr3_output_cache_prune_after_store()` called after each successful
  frame production.
- `cnr3_output_cache_set_ceiling()` called in `cnr3_create()`.
- Debug snapshot printed after each store/prune (gated on `d->debug`).
- `cnr3_output_cache_clear()` called in `cnr3_free()`.
- Memory diagnostics wired at create/free points.
- `validate_invariants` called inside store/remove/prune helpers when
  `CNR3_OUTPUT_CACHE_VALIDATE_AFTER_MUTATION` is true.

### 14.4 Spec Deviation: Hot Zone Update Placement

**The spec (Section 4.8) says:** hot zone updates fire at `arInitial`
for every arriving frame request.

**The code does:** calls `cnr3_output_cache_update_hot_zones()` in the
`arAllFramesReady` block, not in `arInitial`.

**Impact under fmUnordered:** None. Only one request is in flight at a
time. The hot zone is updated before the store and prune, which is what
matters for pruning decisions.

**Required before fmParallelRequests (Phase CMS02-J):** Move the call
to the `arInitial` block. Under fmParallelRequests, each request's zone
must be registered as early as possible so that concurrent pruning or
stores cannot race with zone registration.

This move should be treated as a prerequisite task for Phase CMS02-J,
not as an urgent change for Phase CMS02-F or CMS02-G.

### 14.5 What Is Not Yet Implemented

- Output cache read/cache-hit reuse (output cache not yet
  output-authoritative).
- `cnr3_output_cache_find_frame_and_add_ref()` lookup helper
  (Phase CMS02-F).
- Checkpoint recovery and hole-filling walks (Phase CMS02-G).
- Bounded warm-up recovery (Phase CMS02-H).
- Non-checkpoint pinning (Phase CMS02-I, deferred).
- fmParallelRequests wiring (Phase CMS02-J).
- `Cnr3OwnedFrameRef` RAII wrapper (specified, not yet in code).
- Caller-side lookup ref balance counters (not yet needed — no lookups
  exist yet).

### 14.6 Output Authority

The **old strict-streaming cache** (`old_strict_cache`) remains the
source of truth for returned output frames. The output cache stores and
prunes for proving only.

This changes when Phase CMS02-F is complete and the output cache begins
serving cache hits. That phase requires the Design Compliance Review of
Phase CMS02-F before any cache hit can return output.

---

## Appendix A — Sliding vs. Extend-Only: Discussion and Decision

*(Full discussion in CMS05 Appendix A. Summary retained here.)*

**Background:** CMS02 "never shrink" rule contradicted linear pruning
examples. CMS03 introduced mode-specific split (sliding in fmUnordered,
extend-only in fmParallelRequests) as an overcorrection. CMS04/CMS05
adopted sliding in both modes.

**Key reasoning:** In fmParallelRequests with 16–32 threads and jitter
~6, concurrent request spread is ~22–38 frames. `BACK_RADIUS=50` always
covers all in-flight predecessor needs under normal sequential operation.
The pathological stall case produces more recompute (checkpoint recovery),
not incorrect output. Detectable via `predecessor_missing_when_expected`.

**Decision:** Sliding in both modes. Only retirement is mode-specific.
The `update_hot_zones` helper needs no mode parameter.

**Maintainer caveats:**
- If `predecessor_missing_when_expected` ever increments in production,
  revisit: promote to non-checkpoint pinning, or reintroduce extend-only
  zones in fmParallelRequests.
- The rolling predecessor pattern is load-bearing. If broken, all safety
  arguments collapse.
- Sliding is "good enough given other mechanisms," not "always best."

---

## Appendix B — Reference-Count Discipline: Discussion and Decision

*(Full discussion in CMS05 Appendix B. Summary retained here.)*

**Background:** Concern raised that cache slot eviction could leave
dangling `addFrameRef` calls causing runaway VS memory consumption.

**Analysis:** Design is robust if RC1–RC8 are followed. Five specific
leak classes identified and closed by the rules. Sliding does not worsen
this — it exercises the remove path more, which finds discipline bugs
faster.

**Key insight:** When a walk holds a caller-owned ref and the cache slot
is evicted, the VSFrame stays alive (caller's ref keeps it). The slot
is removed; the VSFrame is destroyed only when all refs reach zero.
No leak. This is the load-bearing safety property.

**Decision:** Promoted to first-class Goal 5 with named Section 2.2.
Rules RC1–RC8, diagnostic counters, RAII wrapper, and shutdown protocol
are the supporting mechanisms.

**Maintainer caveats:**
- Any new code path calling `addFrameRef`/`freeFrame` outside the single
  store/remove helpers is a discipline violation.
- Any `pool.erase()` or `cache_index.erase()` outside the remove helper
  is a violation.
- Ref-balance failure at shutdown is a real bug.
- If non-checkpoint pinning is added, same discipline applies.

---

## Appendix C — Implementation Process Notes and Rationale

*(Full content in CMS05 Appendix C. Subsection list retained here.)*

- **C.1:** First-in-best-dressed store race — narrative and rationale.
- **C.2:** RAII wrapper instrumentation strategy — three options, Option
  3 (explicit call-site instrumentation) recommended for first
  implementation.
- **C.3:** Naming history and rationale — why v005 was replaced, what
  the names mean.
- **C.4:** Why design-compliance review matters — the highest risk is an
  old helper remaining in the call path with old assumptions.
- **C.5:** Phase timing examples — which phases can be batched, which
  cannot.
- **C.6:** Guiding principle — "prefer the safest, clearest, most
  maintainable implementation."

---

## Appendix D — Code Review and Simulation Plan

See companion document `cnr3_code_review_plan_v5_1.md` (version CMS05.1).

The companion document specifies:

- **Static analysis** — 15 review items covering spec compliance, RC
  rule compliance per function, error path completeness, mutex
  correctness, hot zone lifecycle, first-in-best-dressed store,
  final-frame ownership transfer, rolling predecessor pattern, pruning
  correctness, shutdown completeness, instrumentation coverage,
  `validate_invariants` coverage, data structure usage,
  fmUnordered/fmParallelRequests transition safety, and RAII wrapper
  correctness.

- **Monte Carlo simulations** — Three sub-scenarios (fmUnordered,
  fmParallelRequests, fmParallel informational), each comprising 50
  short runs (200 frames, 6 threads, 0–6 frame jitter) plus one long
  run (10,000 frames). CSV output for visualisation in Excel.
  Metrics verified: pool sizes, hot zone behaviour, pruning correctness,
  ref-count balance, predecessor miss rate, duplicate store frequency.

- **Design-compliance review** per Section 12 of this spec.

- **Session structure** — two sessions recommended (analysis then
  decision/remediation).

- **Model recommendation** — Opus 4.7 for both sessions.

