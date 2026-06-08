# CNR3 Cache Manager — Revised Design Specification CMS06
## Sliding Hot-Zone Pruning with Reference-Count Discipline (Non-Checkpoint Pinning Deferred)

**Date:** 2026-06-08
**Version:** CMS06.4
**Status:** Design specification — ready for coding
**Supersedes:** CMS06.4 supersedes CMS06.3, CMS06.2, CMS06.1, CMS06, CMS05.2b, CMS05.2, CMS05.1, CMS05, CMS04, CMS03, CMS02, CMS01
**Also supersedes:** Bounded-recovery policy in handover snapshot v0.14 section 0.9A
**Companion documents:**
- CNR3_Handover_Snapshot_v0.14 (for infrastructure already built)
- cnr3_code_review_plan_v5_1.md (code review and simulation plan)

---

## Changelog

### CMS06.4 — 2026-06-08

**CMS06.4-1 — Bounded checkpoint search contract added (Section 4.5.3).**
Bounded recovery planning must bound the checkpoint search interval itself,
not merely find the global nearest prior checkpoint and reject it afterward.
The search interval is `[max(0, N - B), N]`. If no checkpoint exists within
that interval, return false without pinning. Do not search outside the
interval. This is a design contract correction affecting how
`prepare_bounded_recovery_plan()` must behave. See Section 4.5.3 and
Section 13.10.

**CMS06.4-2 — Section 9.2 updated: bounded vs unbounded helpers
distinguished.**
The existing `cnr3_output_cache_find_and_pin_nearest_prior_checkpoint()`
helper is unbounded and must not be used directly inside
`prepare_bounded_recovery_plan()`. A bounded variant that searches only
within `[max(0, N - B), N]` is required. See Section 9.2.

**CMS06.4-3 — CMS02-H sub-phases H2A and H2B inserted.**
Two new sub-phases inserted before H3 in the CMS02-H sequence:
`CMS02-H / SubPhase H2A / bounded-checkpoint-search-contract-review` and
`CMS02-H / SubPhase H2B / bounded-checkpoint-search-helper-proof`.
H3 and later sub-phases must not proceed until H2B is proven. See Section 8.

**CMS06.4-4 — Named item 13.10 added: bounded checkpoint search contract.**
Records the settled design decision with rejected alternative
(search-then-reject) and reason. Prevents future chats from silently
using the unbounded helper for bounded recovery planning. See Section 13.10.

### CMS06.3 — 2026-06-07

**CMS06.3-1 — Phase/subphase naming standard adopted.**
All sub-phase references now use the expanded form
`CMS02-G / SubPhase G10D.N / short-intent-name` in prose and handover
documents. Compact form `CMS02-G.10D.N` retained only in tables, log
markers, and commit titles. Prevents confusion between sub-phases and
major CMS02 phases. See handover pack spec v1.3 Section 3.7.

**CMS06.3-2 — Implementation state updated through CMS02-G / SubPhase G10D.8.**
Phase completion table in Section 14.1 updated. CMS02-G / SubPhase G10ABC
and G10D.1 through G10D.8 recorded as complete. Latest committed phase
label: `Complete CMS02-G.10D.7 recovery-return-decision-dry-run`.
Latest observed proof edit marker:
`CMS02-G10D7-recovery-return-decision-dry-run-v1`.

**CMS06.3-3 — CMS02-F status corrected from "not started" to
"substantially implemented."**
In the source snapshots reviewed: `cnr3_output_cache_find_frame_and_add_ref()`,
`arAllFramesReady` cache-hit lookup, `cnr3_output_cache_note_lookup_ref_transferred()`,
and CACHE-HIT-RETURN return path are implemented. Cache misses still fall
through to strict-streaming computation. Full output-cache authority is not
yet complete. See Section 8 (CMS02-F) and Section 14.3.

**CMS06.3-4 — exact_match named constraint added.**
`exact_match` is a diagnostic label/value in the recovery
difference-measurement proof path. It must not be used as a
bounded-recovery return condition. See Section 13.8.

**CMS06.3-5 — Duplicate/recompute waste summary added to Section 6.**
Raw counters already exist. A required human-readable duplicate/recompute
waste summary block (counts and percentages) is specified as a near-term
required diagnostic output format. Not yet implemented as a formatted block.
See Section 6 and Section 13.9.

**CMS06.3-6 — fmParallel warning strengthened.**
`old_strict_cache.next_needed` and `old_strict_cache.prev_output` are
explicitly named as incompatible with final fmParallel output authority.
Design and coding must not introduce ordering assumptions tied to these
fields. See Sections 1, 14.6.

**CMS06.3-7 — Cnr3OwnedFrameRef RAII wrapper status clarified.**
Not yet implemented. Recommended but not mandatory while explicit ref
handling remains clean and the caller-side invariant holds. Trigger
condition for when it becomes mandatory is now stated. See Section 2.2
and Section 9.1.

**CMS06.3-8 — Section 14.3 stale bullet corrected.**
Removed stale bullet saying `cnr3_output_cache_update_hot_zones()` was
called in `arAllFramesReady`. It is called in `arInitial`. Section 14.3
updated to reflect actual current wiring including cache-hit return path.

**CMS06.3-9 — Section 14.5 and 14.6 updated to match current state.**
Several items previously listed as "not implemented" are now implemented
(cache-hit lookup, transfer accounting). Output authority description
updated to reflect partial shift: cache hits served from output_cache,
cache misses still strict-streaming.

### CMS06.2 — 2026-06-06

**CMS06.2-1 — Section 14.4 hot-zone update placement: RESOLVED.**
The CMS06.1 deviation note saying hot-zone updates were in `arAllFramesReady`
is superseded. The current committed code calls
`cnr3_output_cache_update_hot_zones()` from `arInitial`, before
`requestFrameFilter()`. This is correct and must be preserved. See
Section 14.4 and named item G-PAR-HZ-ARINITIAL-01 (Section 13.3).

**CMS06.2-2 — Open design requirement added before Phase CMS02-G.10D.**
Actual recovery computation must not depend on or mutate
`d->old_strict_cache.prev_output` or `d->old_strict_cache.next_needed`.
A safe processing boundary with explicit predecessor input must exist before
any real recovered-frame computation begins. See Section 8 (G.10D) and
Section 13.4.

**CMS06.2-3 — Implementation state updated through CMS02-G.9AB.**
Phase completion table in Section 14.1 updated. CMS02-G sub-phases G.7A
through G.9AB recorded as complete. All proof paths remain disabled in the
normal committed state. Current `CNR3_EDIT_VERSION`:
`CMS02-G9AB-source-frame-set-proof-disabled-v1`.

**CMS06.2-4 — Phase CMS02-G.10ABC added to Section 8.**
Dry-run compute orchestration skeleton phase. Explicit not-allowed list
prevents premature recovered-frame computation or output-authority changes.
Cross-reference to the G.10D precondition requirement added.

**CMS06.2-5 — Named deferred items added (Section 13).**
Three named items formalised: G-PAR-HZ-ARINITIAL-01 (resolved),
G-DIAG-RECALC-HIST-01 (deferred), G-DIAG-LOG-VOLUME-01 (deferred).

**CMS06.2-6 — Section 4.8 stale note removed.**
The note in Section 4.8 saying hot zones were updated in `arAllFramesReady`
is replaced with a resolved-status note matching Section 14.4.

**CMS06.2-7 — Comment/label drift: do not mix with G.10ABC.**
Noted in Section 13.5 that CMS05/CMS06 comment wording cleanup must not
be combined with G.10ABC. G.10ABC is a safety-critical proof phase.

### CMS06.1 — 2026-06-04

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
    **fmParallel warning:** `old_strict_cache.next_needed` and
    `old_strict_cache.prev_output` are not final fmParallel output authority.
    Do not introduce design or code assumptions tied to these fields that would
    block migration to full fmParallel authority.

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

**RAII wrapper — `Cnr3OwnedFrameRef` (recommended, not yet implemented):**

This wrapper is specified and recommended but is not yet in the current
committed code. Explicit ref handling (`cnr3_output_cache_note_lookup_ref_released`,
`cnr3_output_cache_note_lookup_ref_transferred`, and manual `freeFrame` on
every exit path) is the current acceptable interim approach, provided the
caller-side invariant (`acquired == released + transferred`) remains clean
at all quiescent points.

**Trigger condition for mandatory implementation:** If explicit ref handling
ever produces a balance error, or if a code review finds that exit paths are
not reliably covered, implementing `Cnr3OwnedFrameRef` becomes a corrective
action required before further development. No phase is currently blocked on
it while the invariant holds clean.

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
    **fmParallel warning:** `old_strict_cache.next_needed` and
    `old_strict_cache.prev_output` are not final fmParallel output authority.
    Do not introduce design or code assumptions tied to these fields that would
    block migration to full fmParallel authority.
  
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
`Cnr3OwnedFrameRef` RAII wrapper handles this automatically when implemented.
Until then, explicit `freeFrame` on every exit path is required.

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

Required idiom when `Cnr3OwnedFrameRef` is implemented (`release()`);
until then, equivalent explicit handling is required on every exit path:

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

#### 4.5.3 Bounded Checkpoint Search Contract

**Settled design decision (CMS06.4). Must not be changed without explicit
review. See also Section 13.10.**

When a bounded recovery plan is being prepared for frame N with recovery
bound B (maximum forward frame count), the checkpoint search must be
bounded by the recovery interval itself — not merely by post-search
plan acceptance.

**Required search contract:**

```text
requested_frame    = N
max_forward_count  = B
lower_bound        = max(0, N - B)

Search for the greatest checkpoint C such that:
    lower_bound <= C <= N

If such C exists:
    pin C and return a bounded checkpoint-start recovery plan.

If no such C exists:
    return false without pinning any checkpoint.
    caller must treat the request as needing bounded warm-up.
```

**Worked examples (B = 2, checkpoints = {0, 10}):**

```text
requested_frame = 15  search interval [13, 15]
    no checkpoint in [13, 15]
    -> bounded plan: false; warm-up needed; no pin taken

requested_frame = 12  search interval [10, 12]
    checkpoint 10 is within [10, 12]
    -> bounded plan: true; selected checkpoint = 10; pin 10

requested_frame = 11  search interval [9, 11]
    checkpoint 10 is within [9, 11]
    -> bounded plan: true; selected checkpoint = 10; pin 10

requested_frame = 10  search interval [8, 10]
    checkpoint 10 is within [8, 10]
    -> bounded plan: true; selected checkpoint = 10; pin 10

requested_frame = 9   search interval [7, 9]
    no checkpoint in [7, 9]
    (checkpoint 10 is above N; checkpoint 0 is below lower_bound)
    -> bounded plan: false; warm-up needed; no pin taken
```

**Why not search-then-reject:**

The rejected alternative is to call the existing unbounded
`find_and_pin_nearest_prior_checkpoint()` helper, then check whether the
found checkpoint falls within the recovery interval, and unpin if not.

This is not acceptable because:
- It creates unnecessary pin/unpin churn for checkpoints already known
  to be out of scope before pinning.
- It may wander into older retained regions or other hot zones, creating
  surprising pin side-effects.
- It obscures diagnostics: a pin followed immediately by an unpin
  appears as spurious checkpoint activity.
- Under fmParallelRequests and fmParallel, out-of-scope pin/unpin churn
  creates unnecessary contention.

**Implementation consequence:**

`prepare_bounded_recovery_plan()` must use the bounded helper
`cnr3_output_cache_find_and_pin_checkpoint_in_interval()` (Section 9.2.G2).
The unbounded `cnr3_output_cache_find_and_pin_nearest_prior_checkpoint()`
must not be called directly from bounded recovery planning.

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

**[CMS06.2 — RESOLVED]** The current committed code calls
`cnr3_output_cache_update_hot_zones()` from `arInitial`, before
`requestFrameFilter()`. This is correct. Do not move hot-zone updates
back to `arAllFramesReady`. The `arInitial` placement is a safety
prerequisite for fmParallelRequests and fmParallel. See Section 14.4
and Section 13.3.

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

**Duplicate/recompute waste summary (near-term required implementation):**

The raw counters already exist and are printed in the machine-readable
output-cache summary:
- `store_attempts`
- `store_successes`
- `store_skipped_already_cached` (duplicate_skipped_already_cached)
- `duplicate_store_computed_but_discarded`

A required human-readable formatted block must be added as a near-term
implementation task (see Section 13.9). The block should appear at shutdown
in `cnr3_free` when `d->debug` is active, and must show:
- Frames computed exactly once (unique computations).
- Frames computed more than once (wasted duplicate work).
- Total duplicate computation count.
- Percentage of total computations that were wasted.

This block is diagnostic only. It must not affect output authority or
frame scheduling.

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

**Current state:** Phases CMS02-A through CMS02-E are complete. CMS02-F
cache-hit return is substantially implemented. CMS02-G proof sub-phases
through `CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review`
are complete (all proof paths disabled in normal committed state).
See Section 14 for exact completion status.
Latest committed phase label: `Complete CMS02-G.10D.7 recovery-return-decision-dry-run`
Latest observed proof edit marker: `CMS02-G10D7-recovery-return-decision-dry-run-v1`

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
**Status: Substantially implemented. Verify against current committed source.**

In source snapshots reviewed, the following CMS02-F mechanisms are implemented:
- `cnr3_output_cache_find_frame_and_add_ref()` — implemented.
- `arAllFramesReady` cache-hit lookup — implemented; first check before
  source-frame retrieval.
- `cnr3_output_cache_note_lookup_ref_transferred()` — implemented; called
  before returning cached frame.
- `CACHE-HIT-RETURN` log label and summary — implemented.
- Cache misses still fall through to source-frame retrieval and
  strict-streaming/new-computation path.

Still open or unverified:
- Full output-cache authority for cache misses — not yet transferred;
  misses still use strict-streaming computation.
- `Cnr3OwnedFrameRef` RAII wrapper — not implemented; explicit ref
  handling is the current acceptable approach (Section 2.2).
- Proof evidence in logs — verify `cache_hits_at_arAllFramesReady` and
  caller-side ref-balance counters in current committed test run.

CMS02-F is a prerequisite before CMS02-G becomes output-authoritative.
**End-of-phase: Design Compliance Review (mandatory before CMS02-G).**

#### Phase CMS02-G — Checkpoint recovery and hole filling (fmUnordered)
**Status: Proof scaffolding in progress. Output authority unchanged.**

Sub-phases completed (all proof paths disabled in normal committed state).
Expanded naming: `CMS02-G / SubPhase G10D.N / short-intent-name`. Compact
form `CMS02-G.10D.N` used in table for readability.

| Compact label | Expanded sub-phase name | Status | Notes |
|---|---|---|---|
| G.7A | CMS02-G / SubPhase G7A / source-request-plan-skeleton | Complete | Proof disabled in committed state |
| G.7B | CMS02-G / SubPhase G7B / lifecycle-proof | Complete | Proof disabled in committed state |
| G.7C | CMS02-G / SubPhase G7C / widened-source-request-proof | Complete | Proof disabled in committed state |
| G.8A | CMS02-G / SubPhase G8A / decision-walk-skeleton | Complete | Proof disabled in committed state |
| G.8B | CMS02-G / SubPhase G8B / checkpoint-selection-ref-rollover | Complete | Checkpoint rollover observed |
| G.8C | CMS02-G / SubPhase G8C / pre-store-probe | Complete | Probe moved pre-store; disabled state clean |
| G.8D | CMS02-G / SubPhase G8D / would-compute-detection | Complete | `would_compute=1` proven; disabled |
| G.9AB | CMS02-G / SubPhase G9AB / source-frame-set-acquire-release | Complete | Proof disabled in committed state |
| G.10ABC | CMS02-G / SubPhase G10ABC / dry-run-compute-orchestration | Complete | No real frames; no output authority change |
| G.10D.1 | CMS02-G / SubPhase G10D.1 / local-single-frame-compute-proof | Complete | Proof disabled in committed state |
| G.10D.2 | CMS02-G / SubPhase G10D.2 / local-bounded-walk-compute-proof | Complete | Proof disabled in committed state |
| G.10D.3 | CMS02-G / SubPhase G10D.3 / diagnostics-cleanup-proof | Complete | Proof disabled in committed state |
| G.10D.4 | CMS02-G / SubPhase G10D.4 / proof-scaffold-cleanup-review | Complete | Proof disabled in committed state |
| G.10D.5 | CMS02-G / SubPhase G10D.5 / local-bounded-walk-store-proof | Complete | Proof disabled in committed state |
| G.10D.6 | CMS02-G / SubPhase G10D.6 / recovery-store-sample-difference-measurement | Complete | `exact_match` diagnostic proven |
| G.10D.7 | CMS02-G / SubPhase G10D.7 / recovery-return-decision-dry-run | Complete | `actual_returned_recovered_output=0`; dry-run only |
| G.10D.8 | CMS02-G / SubPhase G10D.8 / output-authority-transition-readiness-review | Complete | Readiness reviewed; not yet transferred |
| G.10D.9+ | CMS02-G / SubPhase G10D.9+ / (next sub-phase) | **Next** | First actual recovered-frame return to VapourSynth |

Full implementation goals (when output authority is transferred):
- `find_and_pin_nearest_prior_checkpoint` for out-of-order requests.
- Implement rolling predecessor reference pattern (Section 4.5.1).
- Implement final-frame ownership transfer (Section 4.5.2).
- Fill missing frames ascending using fill-holes-only rule.
- Skip frames already present via first-in-best-dressed store.
- Unpin checkpoint on every exit path.
**End-of-phase: Design Compliance Review.**

#### Phase CMS02-G / SubPhase G10ABC / dry-run-compute-orchestration
**Status: Complete.** (Was: "Next recommended phase after CMS02-G.9AB.")

Purpose: prove the future recovery compute orchestration shape without
actual recovered-frame computation.

**Allowed in G.10ABC:**
- Prepare bounded recovery plan.
- Identify checkpoint and walk range.
- Inspect source-frame-set availability.
- Log would-compute steps and predecessor requirements.
- Prove cleanup paths.
- Disable all proof gates before normal commit.

**Not allowed in G.10ABC:**
- Allocate recovered output frames.
- Call `process_cnr3_frame()` for recovery computation.
- Store recovered outputs in the output cache.
- Return recovered outputs to VapourSynth.
- Mutate `d->old_strict_cache.prev_output`.
- Mutate `d->old_strict_cache.next_needed`.
- Change output authority.
- Enable fmParallelRequests or fmParallel.

Cross-reference: the G.10D precondition (Section 13.4) must be satisfied
before G.10ABC transitions to G.10D.

**End-of-phase: Design Compliance Review.**

#### Phase CMS02-G / SubPhase G10D.1–G10D.8 — Local recovery compute proofs
**Status: Complete through G10D.8.** All proof paths disabled in committed state.

G10D.1 through G10D.8 have progressively proven: local single-frame
computation, bounded-walk computation, diagnostics/cleanup, proof scaffold
review, bounded-walk store, recovery-store sample-difference measurement
(`exact_match` diagnostic), recovery-return decision dry-run
(`actual_returned_recovered_output=0`, `output_authoritative=0`), and
output-authority-transition readiness review.

The G-PAR-PRED-BOUNDARY-01 precondition (Section 13.4) was satisfied
during G10D work. The safe processing boundary with explicit predecessor
input is implemented.

#### Phase CMS02-G / SubPhase G10D.9+ — First actual recovered-frame return
**Status: Next. Preconditions satisfied.**

Purpose: return a locally computed recovered frame to VapourSynth from the
output cache for the first time, with full output-authority transfer for
recovered frames proven.

**Still required before full output authority transfers:**
- Recovered outputs stored as authoritative in output cache.
- Recovered outputs returned to VapourSynth (not just dry-run candidates).
- Output authority formally transferred from strict-streaming path.
- Design Compliance Review completed.

**End-of-phase: Design Compliance Review.**

#### Phase CMS02-H — Bounded warm-up recovery (fmUnordered)
**Status: Not started. H2A and H2B must precede H3 and later.**

The CMS02-H sequence includes the following sub-phases. H2A and H2B
are inserted before H3 following a design review that identified the
bounded checkpoint search contract gap (see Section 4.5.3).

#### CMS02-H / SubPhase H2A / bounded-checkpoint-search-contract-review
**Status: Not started.**

Purpose: review and confirm the bounded checkpoint search contract
(Section 4.5.3) before any implementation. Design confirmation only —
no code changes in this sub-phase.

Confirm:
- The search interval is `[max(0, N - B), N]`.
- The helper finds the greatest C in that interval.
- No checkpoint outside the interval is pinned.
- If no checkpoint in interval, return false with no pin.
- The unbounded helper must not be used inside
  `prepare_bounded_recovery_plan()`.

**End-of-sub-phase: written confirmation that contract is understood.**

#### CMS02-H / SubPhase H2B / bounded-checkpoint-search-helper-proof
**Status: Not started. Requires H2A confirmation.**

Purpose: implement `cnr3_output_cache_find_and_pin_checkpoint_in_interval()`
(Section 9.2.G2) and prove its behaviour with a small diagnostic bound.

**Proof placement — post-store, not pre-store:**
The H2B proof must run after `cnr3_output_cache_store_frame()` and
`cnr3_output_cache_prune_after_store()` for the current frame, not before.
This is distinct from the existing H2 pre-store diagnostic probe:

```text
H2 pre-store probe (existing, do not alter):
    frame 0: fires before store; checkpoint 0 not yet in cache
             -> no prior checkpoint; no plan; warm-up needed

H2B post-store proof (new):
    frame 0: fires after store; checkpoint 0 now in cache
             -> checkpoint 0 found in [0,0]; plan available
```

This is a timing distinction, not a contradiction. The two probes coexist.
Do not alter the existing H2 pre-store diagnostic scaffold to accommodate
H2B. Add H2B proof placement after store/prune independently.

Proof expectations (20-frame sequential test, bound B = 2,
checkpoints at every 10th frame: {0, 10}, proof runs post-store):

```text
frame 0:  search [0,0]  -> checkpoint 0 in interval -> plan available
frame 1:  search [0,1]  -> checkpoint 0 in interval -> plan available
frame 2:  search [0,2]  -> checkpoint 0 in interval -> plan available
frame 3:  search [1,3]  -> no checkpoint in [1,3]   -> warm-up needed
...
frame 9:  search [7,9]  -> no checkpoint in [7,9]   -> warm-up needed
frame 10: search [8,10] -> checkpoint 10 in interval -> plan available
frame 11: search [9,11] -> checkpoint 10 in interval -> plan available
frame 12: search [10,12]-> checkpoint 10 in interval -> plan available
frame 13: search [11,13]-> no checkpoint in [11,13] -> warm-up needed
...
frame 19: search [17,19]-> no checkpoint in [17,19] -> warm-up needed
```

Proof must confirm:
- Out-of-window checkpoints are never pinned.
- Successful bounded plans are unpinned on proof cleanup.
- Failed bounded plans leave no pins behind.
- Pin counts and cache/ref diagnostics remain clean throughout.
- Disable all proof gates before normal commit.

**End-of-sub-phase: Design Compliance Review.**

#### CMS02-H / SubPhase H3+ / bounded-warm-up-source-and-compute
**Status: Not started. Requires H2B proven.**

- Implement no-prior-checkpoint warm-up per Sections 4.5 and 4.5.3.
- Use `find_and_pin_checkpoint_in_interval()` for bounded plan decisions.
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
- `cnr3_output_cache_update_hot_zones()` is already called from `arInitial`
  (confirmed in current code — this prerequisite is satisfied).
- Wire fmParallelRequests path: same sliding update helper (already
  implemented), retirement helper invoked with `FmParallelRequests`.
- Test concurrent jump scenarios.
- Validate retirement with pinned-checkpoint proxy.
- Decide on `active_request_count` per zone if retirement proves too
  conservative.
**End-of-phase: Design Compliance Review.**

#### Full fmParallel — final operational target; deferred pending fmParallelRequests proving.
Design and coding must not introduce shared current-request state or
ordering assumptions that would block future fmParallel compatibility.

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
Cnr3OwnedFrameRef           // recommended RAII wrapper, not yet implemented;
                               //   explicit ref handling is current acceptable approach
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

**G2. Bounded checkpoint search helper (required for CMS02-H):**

`cnr3_output_cache_find_and_pin_checkpoint_in_interval(cache, N, B,
    checkpoint_frame_number)`:

Required before `CMS02-H / SubPhase H2B`. This helper must:

1. Lock `cache_mutex`.
2. Compute `lower_bound = max(0, N - B)`.
3. Scan `checkpoint_pool` in reverse frame-number order (highest first).
4. Find the first checkpoint C such that `lower_bound <= C <= N`.
5. If found: `addFrameRef` is NOT taken (pin_count only); increment
   `slot.pin_count`; set `checkpoint_frame_number = C`; unlock; return true.
6. If not found: unlock; return false without touching any checkpoint.

The existing unbounded helper
`cnr3_output_cache_find_and_pin_nearest_prior_checkpoint()` searches
backward from N with no lower bound. It must not be used inside
`prepare_bounded_recovery_plan()`. The two helpers serve different
purposes:

| Helper | Bounded? | Use case |
|---|---|---|
| `find_and_pin_nearest_prior_checkpoint` | No | General recovery; caller applies own acceptance logic |
| `find_and_pin_checkpoint_in_interval` | Yes | Bounded recovery planning; no out-of-scope pinning |

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

**Resolution by CMS06 design:**

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

The review must verify that the resulting execution paths follow CMS06.4,
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

### 13.3 G-PAR-HZ-ARINITIAL-01 — Hot-zone update at arInitial

**Status: RESOLVED.**

Hot-zone updates must occur at `arInitial`, not `arAllFramesReady`, before
fmParallelRequests or fmParallel work. Under concurrent request modes,
deferring hot-zone update to `arAllFramesReady` can allow pruning decisions
to ignore active request intent.

The current committed code calls `cnr3_output_cache_update_hot_zones()`
from the `arInitial` branch of `cnr3_get_frame()`, before
`requestFrameFilter()`. This is correct and must be preserved.

**Implementation consequence:** Do not move hot-zone updates back to
`arAllFramesReady`. If future scheduling modes require changes to
`arInitial` handling, any replacement must register request activity at
least as early as the current `arInitial` call.

### 13.4 G-PAR-PRED-BOUNDARY-01 — Safe processing boundary before G.10D

**Status: RESOLVED. Satisfied during CMS02-G / SubPhase G10D work.**

`process_cnr3_frame()` currently obtains the recursive predecessor from:

```cpp
const VSFrame* prev_output = d->old_strict_cache.prev_output;
```

That is correct for the strict-streaming path, but unsuitable for actual
local recovery computation. Recovery must compute `output[K]` from an
explicit local predecessor without mutating strict-streaming state.

**Hard requirement:** Before CMS02-G.10D begins, a safe processing boundary
must exist that allows recovery code to compute `output[K]` from:
- `source[K]` (the source frame for K)
- an explicit previous filtered output frame for K−1
- local per-invocation recovery state

without modifying:
- `d->old_strict_cache.prev_output`
- `d->old_strict_cache.next_needed`
- normal strict-streaming output authority

**Preferred approach (not mandated):** Refactor or add a processing entry
point that accepts an explicit predecessor `VSFrame*` parameter, making
recovery computation reusable without touching strict-streaming state. The
coder may choose the exact implementation shape, but any chosen approach
must satisfy the non-mutation and explicit-predecessor requirements before
actual recovered-frame computation begins.

**Implementation consequence:** G.10ABC dry-run work (Section 8) should
design toward this boundary even while not yet computing real frames.
G.10D must not begin until this requirement is explicitly satisfied and
reviewed.

### 13.5 G-DIAG-RECALC-HIST-01 — Recalculation histogram (deferred)

**Status: Deferred — implement after recovery can compute/duplicate work.**

Add a compile-time-only per-instance recalculation histogram showing how
many output frames were computed exactly once, twice, three times, etc.
Count true computation work, not cache returns. This is useful once
recovery is operating under first-in-best-dressed store idempotency, where
duplicate work can occur under concurrent walks.

Not to be combined with G.10ABC implementation work.

### 13.6 G-DIAG-LOG-VOLUME-01 — Long-run diagnostic throttling (deferred)

**Status: Deferred — implement when long-run tests become routine.**

Add compile-time diagnostic verbosity controls so that 50+ and 100+ frame
test runs remain readable in normal operation while detailed proof logs
remain available for the currently active change. A separate interval
constant analogous to `CNR3_MEMORY_DIAG_FRAME_INTERVAL` is the expected
approach.

Not to be combined with G.10ABC implementation work.

### 13.7 Comment/label drift — do not mix with active proof phases

Several code comments and diagnostic labels still use CMS05/CMS05-3A
wording. This is documentation drift only and carries no functional defect.

**Rule:** Do not combine broad CMS05/CMS06 wording cleanup with active
safety-critical proof phases. Cleanup-only wording changes make patch review
harder and increase the chance of accidental logic changes. Handle wording
cleanup as a separate dedicated pass between proof phases.

### 13.8 exact_match — Named Constraint (diagnostic only)

**Status: Active constraint. Must not be used as a return condition.**

`exact_match` is a diagnostic label/value in the recovery
difference-measurement proof path (introduced in
`CMS02-G / SubPhase G10D.6 / recovery-store-sample-difference-measurement`).

Per plane per frame, it evaluates as true (`exact_match=1`) when
`samples_different == 0` — meaning the locally computed recovery output
matches the cached/strict-streaming output exactly.

**Named constraint:** `exact_match` must not be used as a bounded-recovery
return condition. It is a diagnostic observation only. Whether a recovered
frame is returned to VapourSynth must be determined by output-authority
rules, not by sample-level equality measurements, unless a later explicit
quality/tolerance policy decision explicitly changes this.

**Reason:** Using `exact_match` as a return gate would couple output
authority to pixel-level comparison results, which:
- makes the return decision format-dependent;
- would silently break for near-identical but not bit-exact recovery;
- bypasses the intended output-authority transition design.

### 13.9 Duplicate/recompute waste summary — Required near-term implementation

**Status: Near-term required. Raw counters exist; formatted block not yet
implemented.**

The machine-readable output-cache summary already includes:
- `store_attempts`
- `store_successes`
- `store_skipped_already_cached`
- `duplicate_store_computed_but_discarded`

A human-readable formatted duplicate/recompute waste summary block must be
added as a near-term implementation task. The block should appear at shutdown
in `cnr3_free` when `d->debug` is active.

Required content:
- Frames computed exactly once (unique computations): count.
- Frames computed more than once (wasted work): count.
- Total duplicate computation instances: count.
- Percentage of total computations that were wasted: percentage.

This block is diagnostic only. It must not affect output authority or
frame scheduling. It must not be gated behind `CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS`
alone — it should be visible whenever `d->debug` is active, since it directly
informs cache efficiency decisions.

### 13.10 Bounded checkpoint search contract — Named design decision

**Status: Settled design decision (CMS06.4). Pending implementation in
CMS02-H / SubPhase H2B.**

**Decision:** For bounded recovery planning, the checkpoint search must be
bounded by the recovery interval `[max(0, N - B), N]`. The helper must
not search outside this interval before pinning.

**Rejected alternative:** Search globally for the nearest prior checkpoint
using the unbounded helper, then check whether the found checkpoint falls
within the recovery interval, and unpin if not (search-then-reject).

**Reason rejected:**
- Creates unnecessary pin/unpin churn on checkpoints known to be out of
  scope before pinning begins.
- May wander into older retained regions or other hot zones.
- Obscures diagnostics with spurious pin activity.
- Creates unnecessary contention under fmParallelRequests and fmParallel.

**Implementation consequence:**
`prepare_bounded_recovery_plan()` must use
`cnr3_output_cache_find_and_pin_checkpoint_in_interval()` (Section 9.2.G2),
not the unbounded `find_and_pin_nearest_prior_checkpoint()` (Section 9.2).
The unbounded helper remains valid for other purposes.

**Cross-references:** Section 4.5.3 (design contract), Section 9.2.G2
(helper specification), Section 8 (CMS02-H / SubPhase H2A and H2B).

---

## 14. Current Implementation State

**[Ground truth derived from reading uploaded source files. Update this
section at every session boundary.]**

### 14.1 Phase Completion Status

Latest committed phase label: `Complete CMS02-G.10D.7 recovery-return-decision-dry-run`
Latest observed proof edit marker: `CMS02-G10D7-recovery-return-decision-dry-run-v1`
(Final disabled-state CNR3_EDIT_VERSION string not confirmed — verify against
current `cnr3_build_config.h`.)

| Phase / Sub-phase (compact) | Expanded name | Status | Notes |
|---|---|---|---|
| CMS02-A | renames, constants | Partially complete | See Section 14.2 |
| CMS02-B | hot zone structures, passive diagnostics | Complete | |
| CMS02-C | hot zone update helpers | Complete | Hard-wired FmUnordered |
| CMS02-D | hot-zone-aware prune, ceiling | Complete | |
| CMS02-E | store/prune-only runtime proving | Complete | |
| CMS02-F | cache-hit reuse | Substantially implemented | Cache-hit return path implemented; cache misses still strict-streaming; see Section 8 |
| G.7A | source-request-plan-skeleton | Complete | Proof disabled in committed state |
| G.7B | lifecycle-proof | Complete | Proof disabled in committed state |
| G.7C | widened-source-request-proof | Complete | Proof disabled in committed state |
| G.8A | decision-walk-skeleton | Complete | Proof disabled in committed state |
| G.8B | checkpoint-selection-ref-rollover | Complete | Proof disabled in committed state |
| G.8C | pre-store-probe | Complete | Proof disabled in committed state |
| G.8D | would-compute-detection | Complete | Proof disabled in committed state |
| G.9AB | source-frame-set-acquire-release | Complete | Proof disabled in committed state |
| G.10ABC | dry-run-compute-orchestration | Complete | No real frames; no output authority change |
| G.10D.1 | local-single-frame-compute-proof | Complete | Proof disabled in committed state |
| G.10D.2 | local-bounded-walk-compute-proof | Complete | Proof disabled in committed state |
| G.10D.3 | diagnostics-cleanup-proof | Complete | Proof disabled in committed state |
| G.10D.4 | proof-scaffold-cleanup-review | Complete | Proof disabled in committed state |
| G.10D.5 | local-bounded-walk-store-proof | Complete | Proof disabled in committed state |
| G.10D.6 | recovery-store-sample-difference-measurement | Complete | `exact_match` diagnostic proven |
| G.10D.7 | recovery-return-decision-dry-run | Complete | `actual_returned_recovered_output=0`; dry-run only |
| G.10D.8 | output-authority-transition-readiness-review | Complete | Readiness reviewed; not yet transferred |
| G.10D.9+ | (next sub-phase) | **Next** | First actual recovered-frame return to VapourSynth |
| CMS02-H | bounded warm-up recovery | Not started | |
| CMS02-I | pinning decision | Not started | |
| CMS02-J | fmParallelRequests | Not started | arInitial hot-zone prerequisite already satisfied |

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

**Output authority (partial shift — cache hits only):**
- Cache hits: `arAllFramesReady` first checks `output_cache` via
  `cnr3_output_cache_find_frame_and_add_ref()`. If found, calls
  `cnr3_output_cache_note_lookup_ref_transferred()`, logs `CACHE-HIT-RETURN`,
  and returns the cached frame to VapourSynth.
- Cache misses: fall through to source-frame retrieval and the
  strict-streaming/new-computation path, including the
  `old_strict_cache.next_needed` check.
- Recovered outputs: not yet returned from output_cache; recovery
  scaffolding through G10D.8 is proof-only and disabled in normal
  committed state.

Do not describe full output-cache authority as achieved until recovered
outputs are computed, stored, returned, and proven in a future sub-phase.

- 8/16-bit recursive chroma blend with scene-change detection.
- Luma downsampling and sharing between U/V planes.
- `OldCnr3StrictStreamCache` — active, returns output frames to
  VapourSynth. This is the current source of output authority.
- `Cnr3OutputCacheManager` — owned by `Cnr3Data`, store/prune wired
  in `cnr3_get_frame()` after the strict-streaming path produces each
  frame.
- `cnr3_output_cache_update_hot_zones()` called in `arInitial` (correct;
  see Section 14.4 — resolved).
- `cnr3_output_cache_find_frame_and_add_ref()` called at start of `arAllFramesReady`;
  cache hits returned via `cnr3_output_cache_note_lookup_ref_transferred()`.
- `cnr3_output_cache_store_frame()` and
  `cnr3_output_cache_prune_after_store()` called after each successful
  frame production.
- `cnr3_output_cache_set_ceiling()` called in `cnr3_create()`.
- Debug snapshot printed after each store/prune (gated on `d->debug`).
- `cnr3_output_cache_clear()` called in `cnr3_free()`.
- Memory diagnostics wired at create/free points.
- `validate_invariants` called inside store/remove/prune helpers when
  `CNR3_OUTPUT_CACHE_VALIDATE_AFTER_MUTATION` is true.

### 14.4 Hot Zone Update Placement

**Status: RESOLVED. Earlier CMS06.1 deviation note is superseded.**

The current committed code calls `cnr3_output_cache_update_hot_zones()`
from the `arInitial` branch of `cnr3_get_frame()`, before
`requestFrameFilter()` and source-request-plan handling. This is correct.

**Do not move hot-zone updates back to `arAllFramesReady`.** The `arInitial`
placement is a hard safety prerequisite for fmParallelRequests and fmParallel:
under concurrent request modes, deferring zone registration to
`arAllFramesReady` can allow pruning to ignore active request intent.

The CMS02-J prerequisite for moving this call is therefore already satisfied.
See named item G-PAR-HZ-ARINITIAL-01 (Section 13.3).
See also G-PAR-PRED-BOUNDARY-01 (Section 13.4) — resolved during G10D work.

### 14.5 What Is Not Yet Implemented or Complete

- Full output-cache authority for cache misses — misses still use
  strict-streaming computation path.
- Recovered-frame return to VapourSynth (Phase CMS02-G / SubPhase G10D.9+).
  Proof scaffolding through G10D.8 is complete but all proof paths disabled;
  `actual_returned_recovered_output=0` in dry-run.
- Bounded warm-up recovery (Phase CMS02-H).
- Non-checkpoint pinning (Phase CMS02-I, deferred).
- fmParallelRequests wiring (Phase CMS02-J).
- `Cnr3OwnedFrameRef` RAII wrapper (recommended, not yet implemented;
  explicit ref handling is acceptable interim while invariant is clean).
- Human-readable duplicate/recompute waste summary block (Section 13.9 —
  near-term required; raw counters exist).
- Recalculation histogram (Section 13.5 — deferred).
- Long-run diagnostic throttling (Section 13.6 — deferred).

### 14.6 Output Authority

**Current state (partial shift):**
- Cache hits are now served from `output_cache` via the CACHE-HIT-RETURN
  path. Output authority has partially shifted for frames already cached.
- Cache misses still use the strict-streaming computation path including
  `old_strict_cache.next_needed` and `old_strict_cache.prev_output`.
- Recovered outputs are not yet returned from `output_cache`; the
  recovery scaffolding through G10D.8 is proof-only and disabled.

**fmParallel warning:** `old_strict_cache.next_needed` and
`old_strict_cache.prev_output` are not final fmParallel output authority.
Any design or code that introduces ordering assumptions tied to these
fields blocks future fmParallel migration. They must eventually be
replaced by fully output-cache-authoritative mechanisms.

**Next authority transition:** Completing
`CMS02-G / SubPhase G10D.9+ / first-actual-recovered-frame-return`
and its Design Compliance Review will transfer output authority for
recovered frames from the strict-streaming path to `output_cache`.

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
Note: the review plan predates CMS02-G proof work; it remains valid as
a post-implementation review target but will need updating to cover
G.10ABC and G.10D+ phases when those are complete.

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

