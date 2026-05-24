# CNR3 Cache Manager — Detailed Design Specification

**Project:** `vapoursynth-cnr3`  
**Scope:** Frame ordering, reorder buffer, output cache, checkpoint store,
speculative rejection window, large-jump recovery  
**Status:** Design / pre-implementation  
**Depends on:** `vapoursynth-cnr3-interaction-model.md`

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Root Cause Analysis](#2-root-cause-analysis)
3. [Design Goals](#3-design-goals)
4. [Assumed Environment](#4-assumed-environment)
5. [Tunable Constants](#5-tunable-constants)
6. [Data Structures](#6-data-structures)
7. [Initialisation](#7-initialisation)
8. [Thread Safety](#8-thread-safety)
9. [State Machine](#9-state-machine)
10. [The getFrame Lifecycle Under Cache Management](#10-the-getframe-lifecycle-under-cache-management)
11. [Per-Mode Behaviour](#11-per-mode-behaviour)
12. [Cache-Hit Fast Path](#12-cache-hit-fast-path)
13. [The lookup_predecessor Function](#13-the-lookup_predecessor-function)
14. [Jump Size Detection and Speculate Window Scaling](#14-jump-size-detection-and-speculate-window-scaling)
15. [Recovery Strategy](#15-recovery-strategy)
16. [The frameData Parameter](#16-the-framedata-parameter)
17. [Checkpoint Management](#17-checkpoint-management)
18. [Scene Change Interaction](#18-scene-change-interaction)
19. [Reorder Buffer Overflow Handling](#19-reorder-buffer-overflow-handling)
20. [BestSource Integration Notes](#20-bestsource-integration-notes)
21. [Reference Counting Obligations](#21-reference-counting-obligations)
22. [Filter Destroy / Cleanup](#22-filter-destroy--cleanup)
23. [Exposed User Parameters](#23-exposed-user-parameters)
24. [Implementation Phases](#24-implementation-phases)
25. [Appendix A — Linear Encode Timeline (PAL, 3900X)](#appendix-a--linear-encode-timeline-pal-3900x)
26. [Appendix B — Warm Seek Recovery Timeline](#appendix-b--warm-seek-recovery-timeline)
27. [Appendix C — Cold Seek Recovery Timeline](#appendix-c--cold-seek-recovery-timeline)
28. [Appendix D — Proportional Speculate Window Examples](#appendix-d--proportional-speculate-window-examples)
29. [Appendix E — Cascade Drain Worked Example](#appendix-e--cascade-drain-worked-example)
30. [Appendix F — Cache-Hit Fast Path Worked Example](#appendix-f--cache-hit-fast-path-worked-example)

---

## 1. Problem Statement

CNR3 is a recursive temporal chroma filter. Its core recurrence is:

```
output[0] = source[0]                         (scene start / no history)
output[N] = f(source[N], output[N-1])         (all subsequent frames)
```

This creates a **strict serial data dependency**: frame N cannot be computed
until frame N-1 has been fully computed and its output stored.

VapourSynth's scheduler, running on a multi-core machine, may deliver
`arAllFramesReady` callbacks to CNR3's `getFrame` in an order that does
**not** match the ascending frame number sequence that the recurrence requires.

Two distinct causes produce out-of-order delivery:

### Cause 1 — Thread jitter during linear encode

With a thread pool of T threads and a request depth of R frames,
up to `min(T, R)` source frames may be in flight upstream simultaneously.
Those upstream fetches complete in non-deterministic order, so
`arAllFramesReady` for frame N+3 may arrive before `arAllFramesReady`
for frame N+1, even during a perfectly sequential `vspipe` encode.

```
vspipe -r 24, core.num_threads = 24 (Ryzen 3900X default):
  Up to 24 frames in flight simultaneously.
  Jitter depth at CNR3's door: typically 4–16 frames in practice
  (decoder is usually the bottleneck and limits actual concurrency).
  Worst-case theoretical: 24 frames.
```

### Cause 2 — Non-linear access during preview / seeking

A preview tool (VapourSynth Editor, vsedit, Vapoursynth Preview) requests
frames by arbitrary number on user interaction. The user may seek from
frame 0 to frame 500, then back to frame 100, then forward to frame 501.
There is no guarantee that any predecessor frame was ever computed.

---

## 2. Root Cause Analysis

The fundamental tension is:

```
VapourSynth's scheduler optimises for throughput:
  → deliver arAllFramesReady as soon as the source is ready
  → do not impose ordering on a filter unless it declares fmFrameState
    (which disables caching and is discouraged)

CNR3's algorithm requires ordering:
  → output[N] is undefined until output[N-1] exists

These two requirements are in direct conflict.
```

`fmUnordered` (the correct filter mode for CNR3) guarantees that
CNR3's `getFrame` is never called concurrently from two threads,
but makes **no promise about the order** in which frame numbers arrive.

The cache manager exists to bridge this gap: absorb out-of-order deliveries,
hold them until they can be processed in order, and recover gracefully when
the gap is too large to be explained by jitter alone.

### The single-return constraint

A critical constraint that shapes the entire design:

**Every `arAllFramesReady` call to `getFrame(n)` MUST return a valid
`VSFrame*` before returning. It cannot block, defer, or return null.**

This means:
- CNR3 cannot simply wait for frame N-1 to be computed — doing so would
  deadlock, because VS cannot dispatch getFrame(N-1) while getFrame(N) is
  blocking (fmUnordered serialises all calls).
- When a frame arrives out of order and its predecessor is not yet computed,
  CNR3 must either return a pre-computed result from cache, or trigger an
  inline recompute, or return an error frame.
- The cascade drain model works because pre-computed intermediate frames
  are stored in `recent_cache` and returned immediately (cache-hit fast path)
  when VS later calls `arAllFramesReady` for those frame numbers.

---

## 3. Design Goals

| Priority | Goal |
|---|---|
| P0 | Correct recursive output for linear `vspipe` encode |
| P0 | Never produce a frame computed from the wrong predecessor |
| P1 | Absorb thread jitter without recomputation during linear encode |
| P1 | Recover correctly (not silently wrongly) from large jumps |
| P2 | Make recovery cost bounded and independent of clip length |
| P2 | Support preview/seek workflows with acceptable latency |
| P3 | All buffer sizes configurable via named compile-time constants |
| P3 | Degrade gracefully on low-memory systems |

### Non-goals (initial implementation)

- Real-time preview with zero recompute on arbitrary seeks
- Parallel recomputation during recovery
- Persistent checkpoint storage across process restarts

---

## 4. Assumed Environment

### Hardware baseline

```
Primary development target:
  AMD Ryzen 3900X  — 12 cores / 24 threads (SMT)
  32 GB RAM
  Windows x64

Secondary target:
  Intel i4670  — 4 cores / 8 threads (HT)
  8 GB RAM
  Windows x64
```

### VapourSynth configuration

```
core.num_threads:  defaults to logical CPU count
                   3900X → 24    i4670 → 8

vspipe -r N:       request depth, often equals num_threads
                   3900X → up to 24 frames in flight
                   i4670 → up to 8 frames in flight
```

### Source plugin

**BestSource** (primary target). BestSource is a modern VapourSynth source
plugin using FFmpeg for decoding. It is architecturally similar to ffms2 and
lsmas for the purposes of this design:

- It decodes roughly sequentially during linear playback.
- It supports random access but pays a seek penalty for large jumps.
- It may decode a small number of frames ahead of the last requested frame
  (its own internal lookahead), contributing to the jitter CNR3 must absorb.
- It is the upstream filter whose `arAllFramesReady` completion order
  determines when CNR3 sees its source frames.

For this design, BestSource is treated as an opaque upstream clip.
CNR3 makes no assumptions about its internal behaviour beyond what
the VS API guarantees.

### Source material

```
PAL VHS / VHS-C:   720 × 576i,  25 fps (or 25p)
NTSC VHS / VHS-C:  720 × 480i,  29.97 fps
HD (edge case):    1920 × 1080,  25 or 29.97 fps

2 seconds of frames:
  PAL:   50 frames   ← used as the recompute lookback baseline
  NTSC:  ~60 frames
  HD:    50 or 60 frames (same count, larger per-frame memory)
```

---

## 5. Tunable Constants

All constants are defined in a single header section for easy adjustment.
Names are chosen to be self-documenting.

```cpp
// ── CNR3 Cache Manager — Tunable Constants ────────────────────────────────
//
// Sizes are in frames unless noted.
// Defaults are calibrated for PAL 720x576 on a Ryzen 3900X / 32 GB system.
// Reduce CNR3_RECENT_CACHE_SIZE and CNR3_CHECKPOINT_COUNT for low-memory
// systems (see memory notes in Section 4).

// Reorder buffer — maximum number of out-of-order source frames held
// while waiting for their predecessors. Must be >= VS thread count to
// absorb worst-case jitter without triggering speculative mode.
// 32 covers the 24-thread 3900X with headroom.
// If VS thread count exceeds this value, normal jitter may be misclassified
// as a seek. Increase if running with more than 32 threads.
static constexpr int CNR3_REORDER_WINDOW       = 32;

// Rolling output cache — number of most-recently-computed output frames
// to retain. Used for:
//   (a) the cache-hit fast path (see Section 12)
//   (b) predecessor lookup during cascade drain (see Section 13)
//   (c) backward scrub support up to CNR3_RECENT_CACHE_SIZE frames
// ~2 seconds PAL (50 frames), ~1.67 seconds NTSC.
static constexpr int CNR3_RECENT_CACHE_SIZE    = 50;

// Checkpoint interval — store a permanent output frame snapshot every
// N frames. These survive beyond the recent cache window and allow
// recovery recompute cost to be bounded regardless of clip length.
// Set equal to CNR3_RECENT_CACHE_SIZE so that the cache and checkpoint
// grid are aligned: the oldest frame that may fall out of recent cache
// is always covered by a checkpoint.
// 50 frames = 2 seconds PAL = 1.67 seconds NTSC.
static constexpr int CNR3_CHECKPOINT_INTERVAL  = 50;

// Maximum number of checkpoints to retain in memory at any one time.
// Oldest checkpoint is evicted (FIFO) when this limit is reached.
// 20 checkpoints × 50 frame interval = 1000 frames = 40 seconds PAL.
// For a 2-hour VHS tape (180000 PAL frames), this covers only the most
// recent 40 seconds; older checkpoints are evicted. Recovery for very
// old frames not covered by a checkpoint falls back to the nearest
// available checkpoint and walks forward from there.
static constexpr int CNR3_CHECKPOINT_COUNT     = 20;

// Speculate window base — the number of subsequent frame arrivals to
// wait, observing whether the gap fills, before declaring a confirmed
// large jump and entering recovery. Applied at full value when
// gap <= CNR3_REORDER_WINDOW. Tapered to zero at
// CNR3_LARGE_JUMP_THRESHOLD. See Section 14 for the full formula.
// Set equal to CNR3_REORDER_WINDOW so that the speculation budget
// matches the maximum expected jitter depth.
static constexpr int CNR3_SPECULATE_BASE       = 32;

// Large jump threshold — gap sizes strictly above this value trigger
// immediate recovery with zero speculation wait. Gap sizes between
// CNR3_REORDER_WINDOW and this value trigger proportionally tapered
// speculation. Must be strictly greater than CNR3_REORDER_WINDOW.
// Default: 5 × CNR3_REORDER_WINDOW = 160 frames = 6.4 seconds PAL.
// No plausible thread-jitter scenario produces a gap this large;
// any such gap is definitively a user seek.
static constexpr int CNR3_LARGE_JUMP_THRESHOLD = 160;   // must be > CNR3_REORDER_WINDOW

// Recompute lookback — on a confirmed large jump to target frame N,
// recompute this many frames before N to warm the recursive state
// before returning output[N]. This ensures the filter has seen
// approximately 2 seconds of context before the target frame,
// producing good chroma stabilisation quality from the first output
// frame after a seek. Must satisfy:
//   CNR3_RECOMPUTE_LOOKBACK <= CNR3_CHECKPOINT_INTERVAL
// so that the nearest checkpoint always covers the lookback window.
// If this constraint is violated, recovery may cold-start from frame 0
// even when checkpoints exist.
static constexpr int CNR3_RECOMPUTE_LOOKBACK   = 50;

// Maximum speculate window cap — the proportional scaling formula
// (Section 14) is clamped to this value as an upper bound. In practice
// the formula never exceeds CNR3_SPECULATE_BASE, so this is a safety
// ceiling. Set to 2 × CNR3_SPECULATE_BASE.
static constexpr int CNR3_SPECULATE_MAX        = 64;
```

### Constraint relationships between constants

The following relationships must hold for the design to behave correctly.
Violating them does not cause crashes, but does degrade correctness or
produce unexpected costs:

```
CNR3_LARGE_JUMP_THRESHOLD  >  CNR3_REORDER_WINDOW
  Required: otherwise the taper formula has a zero denominator.
  Default: 160 > 32 ✓

CNR3_RECOMPUTE_LOOKBACK  <=  CNR3_CHECKPOINT_INTERVAL
  Required: ensures the nearest checkpoint always covers the lookback.
  Default: 50 <= 50 ✓

CNR3_REORDER_WINDOW  >=  core.num_threads  (runtime check recommended)
  Recommended: otherwise normal jitter may fill the reorder buffer
  and be misclassified as a seek. Not a hard compile-time constraint
  since core.num_threads is not known at compile time.
  Recommend logging a warning at filter creation if
  core.num_threads > CNR3_REORDER_WINDOW.
```

---

## 6. Data Structures

```cpp
// ── Per-instance state ────────────────────────────────────────────────────

enum CNR3Mode {
    MODE_STREAMING,    // normal operation; reorder buffer absorbing jitter
    MODE_SPECULATING,  // large gap detected; waiting to see if gap fills
    MODE_RECOVERING,   // genuine large jump confirmed; recomputing from checkpoint
};

// ── Recent output cache ───────────────────────────────────────────────────
//
// Ring buffer of the last CNR3_RECENT_CACHE_SIZE completed output frames.
// Slot assignment: slot index = frame_number % CNR3_RECENT_CACHE_SIZE.
// A slot is valid only when slot.frame_number == the frame number being
// looked up. If the slot holds a different frame number, it is stale.
// All frame_numbers initialised to -1 (empty sentinel).

struct CNR3RecentCacheSlot {
    int             frame_number;   // -1 = empty
    const VSFrame*  frame;          // cloneFrameRef held; null when frame_number == -1
};

struct CNR3RecentCache {
    CNR3RecentCacheSlot slots[CNR3_RECENT_CACHE_SIZE];
};

// ── Checkpoint store ──────────────────────────────────────────────────────
//
// Circular buffer of sparse permanent output frame snapshots.
// Stored every CNR3_CHECKPOINT_INTERVAL frames.
// Eviction is FIFO: next_slot always points to the oldest entry,
// which is overwritten on the next checkpoint store.

struct CNR3Checkpoint {
    int             frame_number;   // -1 = empty slot
    const VSFrame*  output_frame;   // cloneFrameRef held; null when frame_number == -1
};

struct CNR3CheckpointStore {
    CNR3Checkpoint  slots[CNR3_CHECKPOINT_COUNT];
    int             count;      // number of valid (non-empty) entries; 0..CNR3_CHECKPOINT_COUNT
    int             next_slot;  // index of the slot to write next (oldest slot when full)
};

// ── Reorder buffer ────────────────────────────────────────────────────────
//
// Holds source frames that have arrived via arAllFramesReady but whose
// predecessor output frame has not yet been computed. Each stored frame
// is a cloneFrameRef of the source frame retrieved via getFrameFilter.
//
// Implementation note: use std::map<int, const VSFrame*> for correctness
// and simplicity. The map is bounded to CNR3_REORDER_WINDOW entries by
// the overflow policy (see Section 19). A fixed-size array indexed by
// (frame_number % CNR3_REORDER_WINDOW) is also viable but requires
// careful validity checking to handle frame numbers that alias to the
// same slot; the map is preferred for initial implementation.

struct CNR3ReorderBuffer {
    std::map<int, const VSFrame*>  pending;
    // pending.size() is bounded to CNR3_REORDER_WINDOW by the overflow policy.
};

// ── Per-frame state passed between arInitial and arAllFramesReady ─────────
//
// VS provides a void** frameData parameter in getFrame that is unique per
// (frame number, filter instance) invocation. The arInitial pass may write
// a pointer or value into *frameData; the arAllFramesReady pass reads it.
// CNR3 uses this to communicate the recovery plan computed during arInitial
// to the arAllFramesReady pass. See Section 16.

struct CNR3FrameState {
    bool    recovery_needed;    // arInitial detected a large jump
    int     recovery_start;     // recompute from this frame number
    int     recovery_target;    // recompute up to and including this frame
};
// Allocated with new in arInitial, deleted in arAllFramesReady.

// ── Cache manager aggregate ───────────────────────────────────────────────

struct CNR3CacheManager {
    CNR3ReorderBuffer    reorder;
    CNR3RecentCache      recent;
    CNR3CheckpointStore  checkpoints;

    CNR3Mode             mode;
    int                  next_needed;         // next frame number not yet computed
    const VSFrame*       prev_output;         // most recently computed output frame
                                              // (cloneFrameRef held; null before frame 0)
    // Speculation state (valid only in MODE_SPECULATING)
    int                  speculate_target;    // frame number that triggered speculation
    int                  speculate_gap;       // gap size that triggered speculation
    int                  speculate_countdown; // frame arrivals remaining before recovery
};

// ── Top-level filter instance data ───────────────────────────────────────

struct CNR3Data {
    VSNode*              node;              // source clip
    VSVideoInfo          vi;                // clip info (format, dimensions, fps)

    // Filter parameters (stored in 8-bit public units; scaled to clip
    // bit depth in internal lookup tables at filter creation time)
    int                  mode_flags;        // parsed from mode= string ("oxx" etc.)
    int                  ln, lm;            // luma noise/mask thresholds (8-bit units)
    int                  un, um;            // U chroma thresholds (8-bit units)
    int                  vn, vm;            // V chroma thresholds (8-bit units)
    float                scdthr;            // scene change detection threshold
    bool                 scene_chroma;      // reset chroma recursive state on scene change
    bool                 debug;             // overlay debug information on output frames

    // Seek mode (set from seek_mode= parameter at filter creation)
    // CNR3_SEEK_STRICT:   error on any out-of-order frame
    // CNR3_SEEK_AUTO:     full cache manager (default)
    // CNR3_SEEK_RECOVER:  skip speculation, recover immediately on any gap
    int                  seek_mode;

    CNR3CacheManager     cache;             // all frame ordering and caching state
};
```

---

## 7. Initialisation

`CNR3CacheManager` must be fully initialised in the filter creation function
(`CNR3Create` or equivalent) before any `getFrame` calls arrive.

```cpp
void cnr3_cache_init(CNR3CacheManager* cache) {

    // Reorder buffer — empty map, no initialisation needed beyond construction
    // (std::map default-constructs to empty)

    // Recent cache — mark all slots as empty
    for (int i = 0; i < CNR3_RECENT_CACHE_SIZE; i++) {
        cache->recent.slots[i].frame_number = -1;
        cache->recent.slots[i].frame        = nullptr;
    }

    // Checkpoint store — mark all slots as empty
    for (int i = 0; i < CNR3_CHECKPOINT_COUNT; i++) {
        cache->checkpoints.slots[i].frame_number  = -1;
        cache->checkpoints.slots[i].output_frame  = nullptr;
    }
    cache->checkpoints.count     = 0;
    cache->checkpoints.next_slot = 0;

    // State
    cache->mode         = MODE_STREAMING;
    cache->next_needed  = 0;
    cache->prev_output  = nullptr;   // null = no predecessor (first frame is scene start)

    // Speculation state — undefined until mode == MODE_SPECULATING
    cache->speculate_target    = -1;
    cache->speculate_gap       = 0;
    cache->speculate_countdown = 0;
}
```

This function is called once per filter instance, during `CNR3Create`,
immediately after allocating the `CNR3Data` struct.

---

## 8. Thread Safety

**No mutexes or other synchronisation primitives are needed in the
CNR3 cache manager.**

CNR3 declares `fmUnordered` as its filter mode. The VapourSynth scheduler
guarantees that `getFrame` is never called from two threads simultaneously
for the same filter instance. All calls to `getFrame` — regardless of frame
number — are serialised by VS before reaching CNR3's callback.

This means:
- All reads and writes to `CNR3CacheManager` fields occur on a single thread
  at any given moment.
- There are no concurrent accesses to `prev_output`, `next_needed`, `mode`,
  the reorder buffer map, the recent cache ring, or the checkpoint store.
- Do not add mutexes. They would add overhead and false safety.

The only threading consideration is that the VS scheduler may call
`getFrame` on CNR3 from *different* threads across successive frames
(thread A handles frame 0, thread B handles frame 1, etc.). This is fine
because fmUnordered ensures these are strictly sequential in time — thread A
finishes before thread B starts.

---

## 9. State Machine

```
                        ┌──────────────────────────────────────────────────┐
                        │                                                  │
               gap fills (cascade advances next_needed)        cascade fills gap
                        │                                                  │
           ┌────────────▼────────────┐                      ┌─────────────┴───────────┐
           │     MODE_STREAMING      │                      │     MODE_SPECULATING    │
           │                         │                      │                         │
           │  Normal operation.      │──gap >               │  Large gap arrived.     │
           │  Reorder buffer         │  REORDER_WINDOW ────▶│  Waiting speculate_    │
           │  absorbs jitter.        │  AND window > 0      │  countdown frames to    │
           │  Cascade drain keeps    │                      │  see if catchup occurs. │
           │  next_needed advancing. │◀─────────────────────│                         │
           └─────────────────────────┘  catchup occurred    └─────────────┬───────────┘
                        ▲                                                 │
                        │                                 speculate_countdown == 0
                        │                                 (no catchup, genuine jump)
                        │                                                 │
                        │                                   ┌─────────────▼───────────┐
                        │                                   │     MODE_RECOVERING     │
                        │                                   │                         │
                        └───────────────────────────────────│  Find nearest           │
                             recovery complete               │  checkpoint ≤ target.   │
                                                            │  Recompute lookback     │
                                                            │  frames to target.      │
                                                            │  Drain reorder buffer.  │
                                                            └─────────────────────────┘
```

Additionally, for `seek_mode = CNR3_SEEK_RECOVER`:

```
Any gap > CNR3_REORDER_WINDOW bypasses MODE_SPECULATING entirely
and transitions directly to MODE_RECOVERING.
```

### State transition triggers

| From | To | Trigger |
|---|---|---|
| STREAMING | SPECULATING | `gap > CNR3_REORDER_WINDOW` AND `compute_speculate_window(gap) > 0` AND `seek_mode != CNR3_SEEK_RECOVER` |
| STREAMING | RECOVERING | `gap > CNR3_REORDER_WINDOW` AND `compute_speculate_window(gap) == 0` (gap >= threshold) OR `seek_mode == CNR3_SEEK_RECOVER` |
| SPECULATING | STREAMING | `cascade_drain()` advances `next_needed` at least once (gap is filling) |
| SPECULATING | RECOVERING | `speculate_countdown` reaches 0 with gap still open |
| RECOVERING | STREAMING | Recovery recompute complete; reorder buffer drained |

### seek_mode = CNR3_SEEK_STRICT behaviour

When `seek_mode == CNR3_SEEK_STRICT`, the state machine is bypassed entirely.
Any frame arrival where `n != next_needed` (and `n` is not already in
`recent_cache`) immediately sets an error via `vsapi->setFilterError` and
returns `nullptr`. No reorder buffer, no speculation, no recovery.
This mode is correct and zero-overhead for `vspipe -r 1` sequential encode.

---

## 10. The getFrame Lifecycle Under Cache Management

Understanding where the cache manager fits within the VS `getFrame`
two-pass lifecycle is essential.

### arInitial pass

```cpp
if (activationReason == arInitial) {

    // Strict mode: just request the one source frame and return.
    if (d->seek_mode == CNR3_SEEK_STRICT) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        return nullptr;
    }

    // Auto/recover mode: check if a large jump is likely.
    // We do not have the source frame yet, but we know n and next_needed.
    int gap = n - d->cache.next_needed;

    if (gap > CNR3_REORDER_WINDOW) {
        // Likely a seek. Compute recovery range.
        int recovery_start = cnr3_find_recompute_start(&d->cache, n);

        // Request ALL source frames from recovery_start+1 to n.
        // VS will not call arAllFramesReady until all of them are ready.
        for (int f = recovery_start + 1; f <= n; f++) {
            vsapi->requestFrameFilter(f, d->node, frameCtx);
        }

        // Pass the recovery plan to arAllFramesReady via frameData.
        CNR3FrameState* fs = new CNR3FrameState();
        fs->recovery_needed  = true;
        fs->recovery_start   = recovery_start;
        fs->recovery_target  = n;
        *frameData = fs;

    } else {
        // Normal case: request only source frame n.
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        *frameData = nullptr;   // no recovery plan
    }

    return nullptr;
}
```

### arAllFramesReady pass

```cpp
if (activationReason == arAllFramesReady) {

    CNR3FrameState* fs = (CNR3FrameState*)*frameData;
    bool recovery_planned = (fs != nullptr && fs->recovery_needed);

    // ── Cache-hit fast path (see Section 12) ──────────────────────
    // If output[n] was pre-computed by a prior cascade drain, return
    // it directly from recent_cache without any pixel work.
    const VSFrame* cached = cnr3_recent_cache_get(&d->cache.recent, n);
    if (cached != nullptr) {
        if (fs) delete fs;
        // Return a clone — VS takes ownership of the returned frame,
        // and recent_cache retains its own reference.
        return vsapi->cloneFrameRef(cached);
    }

    // ── Strict mode ───────────────────────────────────────────────
    if (d->seek_mode == CNR3_SEEK_STRICT) {
        if (n != d->cache.next_needed) {
            vsapi->setFilterError("CNR3: out-of-order frame request. "
                "Only sequential access is supported (seek_mode=strict).",
                frameCtx);
            if (fs) delete fs;
            return nullptr;
        }
        // Fall through to normal processing below.
    }

    // ── Recovery path ─────────────────────────────────────────────
    if (recovery_planned) {
        cnr3_execute_recovery(d, fs->recovery_start, fs->recovery_target,
                              frameCtx, core, vsapi);
        delete fs;
        // After recovery, output[n] is now in recent_cache.
        // Return it via the cache-hit path.
        const VSFrame* recovered = cnr3_recent_cache_get(&d->cache.recent, n);
        // recovered must not be null here — recovery guarantees it.
        return vsapi->cloneFrameRef(recovered);
    }

    if (fs) delete fs;

    // ── Normal path: retrieve source frame, run cache manager ─────
    const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);
    VSFrame* dst = cnr3_process_frame(d, n, src, frameCtx, core, vsapi);
    vsapi->freeFrame(src);
    return dst;
}
```

### cnr3_process_frame

This function handles the arAllFramesReady logic for a single source frame
arriving in the streaming/speculating path:

```cpp
VSFrame* cnr3_process_frame(CNR3Data* d, int n, const VSFrame* src,
                             VSFrameContext* frameCtx, VSCore* core,
                             const VSAPI* vsapi) {
    int gap = n - d->cache.next_needed;

    // Store source frame in reorder buffer (cloneFrameRef acquired).
    cnr3_reorder_insert(&d->cache.reorder, n, src, vsapi);

    // Update state machine.
    switch (d->cache.mode) {

        case MODE_STREAMING:
            if (gap > CNR3_REORDER_WINDOW) {
                cnr3_enter_speculating(&d->cache, n, gap);
                // Cannot produce output yet — return passthrough of src
                // as a placeholder. VS will re-request if needed.
                // See Section 15, Option B discussion.
                // For initial implementation: set error instead.
                vsapi->setFilterError("CNR3: gap exceeded reorder window "
                    "before speculation resolved.", frameCtx);
                return nullptr;
            }
            return cnr3_cascade_drain(d, frameCtx, core, vsapi);

        case MODE_SPECULATING:
            d->cache.speculate_countdown--;
            {
                VSFrame* result = cnr3_cascade_drain(d, frameCtx, core, vsapi);
                if (d->cache.mode == MODE_STREAMING) {
                    // Cascade advanced next_needed — gap was filling, not a seek.
                    return result;
                }
                if (d->cache.speculate_countdown == 0) {
                    // Speculation expired without catchup. Recovery was already
                    // planned in arInitial; the recovery_planned path above
                    // handles it. This branch should not be reached in normal
                    // operation if arInitial correctly detected the large gap.
                }
                // Still speculating — no output yet for frame n.
                // Same issue as above: must return something.
                // For initial implementation: error.
                vsapi->setFilterError("CNR3: speculation still pending, "
                    "cannot return frame.", frameCtx);
                return nullptr;
            }

        case MODE_RECOVERING:
            // Recovery is handled entirely in the arInitial path and the
            // recovery_planned branch above. If we reach here, something
            // went wrong with state tracking.
            vsapi->setFilterError("CNR3: unexpected MODE_RECOVERING in "
                "cnr3_process_frame.", frameCtx);
            return nullptr;
    }
    return nullptr; // unreachable
}
```

### Important note on the single-return constraint in speculation

The above pseudocode returns errors during active speculation for frame
numbers that are out of order and whose predecessor is not yet available.
This is intentional and conservative for the initial implementation.
The correct full solution — where speculating frames are held and returned
once the cascade resolves — requires the `arInitial` pre-fetch approach
described in Section 16, and is part of the Phase 4 implementation.

---

## 11. Per-Mode Behaviour

### cascade_drain

This function is the core of the normal-path processing. It is called
from `arAllFramesReady` (via `cnr3_process_frame`) each time a source frame
arrives. It drains as many pending frames as possible in order.

```cpp
VSFrame* cnr3_cascade_drain(CNR3Data* d, VSFrameContext* frameCtx,
                             VSCore* core, const VSAPI* vsapi) {
    VSFrame* result_for_caller = nullptr;

    while (d->cache.reorder.pending.count(d->cache.next_needed) > 0) {

        int f = d->cache.next_needed;
        const VSFrame* src_f = d->cache.reorder.pending[f];

        // Look up predecessor output frame.
        const VSFrame* prev = cnr3_lookup_predecessor(&d->cache, f, vsapi);
        if (prev == nullptr && f > 0) {
            // Predecessor not available — cannot proceed.
            // This should not happen in normal STREAMING mode if
            // CNR3_REORDER_WINDOW >= VS thread count.
            break;
        }

        // Check for scene change. If detected and scene_chroma is true,
        // treat this frame as a scene start (prev = nullptr).
        // See Section 18.
        bool is_scene_start = (f == 0) || (prev == nullptr);
        if (!is_scene_start && d->scene_chroma) {
            is_scene_start = cnr3_detect_scene_change(src_f, prev, d, vsapi);
            if (is_scene_start) {
                cnr3_invalidate_recursive_state(&d->cache, vsapi);
                prev = nullptr;
            }
        }

        // Allocate destination frame.
        VSFrame* dst = vsapi->newVideoFrame(
            vsapi->getVideoFrameFormat(src_f),
            vsapi->getFrameWidth(src_f, 0),
            vsapi->getFrameHeight(src_f, 0),
            src_f, core);

        // Apply filter.
        if (is_scene_start) {
            cnr3_copy_all_planes(src_f, dst, vsapi);   // no history: passthrough
        } else {
            cnr3_copy_plane(src_f, dst, 0, vsapi);     // Y: always unchanged
            cnr3_filter_chroma(src_f, prev, dst, 1, d, vsapi);  // U
            cnr3_filter_chroma(src_f, prev, dst, 2, d, vsapi);  // V
        }

        // Store output in recent cache (acquires cloneFrameRef).
        cnr3_recent_cache_put(&d->cache.recent, f, dst, vsapi);

        // Store checkpoint if on interval.
        cnr3_maybe_store_checkpoint(&d->cache, f, dst, vsapi);

        // Update rolling prev_output reference.
        if (d->cache.prev_output)
            vsapi->freeFrame(d->cache.prev_output);
        d->cache.prev_output = vsapi->cloneFrameRef(dst);

        // Advance state.
        d->cache.next_needed = f + 1;
        vsapi->freeFrame(d->cache.reorder.pending[f]);  // release reorder ref
        d->cache.reorder.pending.erase(f);

        // Restore STREAMING mode if we were speculating and gap filled.
        if (d->cache.mode == MODE_SPECULATING) {
            d->cache.mode = MODE_STREAMING;
        }

        // dst is the output for frame f. If this is the frame the original
        // getFrame(f) call is waiting to return, keep it.
        // Note: for all OTHER frames processed in this cascade (frames that
        // were pre-computed as side effects), their output is in recent_cache.
        // VS will serve those via the cache-hit fast path (Section 12) when
        // it later calls getFrame for those frame numbers.
        //
        // The caller's frame number is the one passed to the outer
        // arAllFramesReady call. The cascade may compute frames before AND
        // equal to that number. We return the dst for the originally
        // requested frame; all others are retained only in recent_cache.
        //
        // Implementation note: the caller must pass the originally-requested
        // frame number 'n' into cascade_drain so it knows which dst to return.
        // The pseudocode here uses result_for_caller as that slot.
        if (result_for_caller == nullptr) {
            // First frame processed — check if this is the originally
            // requested frame. (In the common case, the cascade processes
            // frames in order up to and including n.)
            // Store all computed frames in recent_cache; return the one for n.
            result_for_caller = vsapi->cloneFrameRef(dst);
            // (dst itself is now owned by recent_cache via its cloneFrameRef;
            //  result_for_caller is a separate ref that will be returned to VS)
        }
        vsapi->freeFrame(dst);  // release our local ref; recent_cache and
                                // result_for_caller (if applicable) retain theirs
    }

    return result_for_caller;
}
```

**Clarification on returning from cascade_drain:**

The cascade may process multiple frames (e.g. frames 0, 1, 2, 3, 4) when
the originally-requested frame was frame 0. Each processed frame has its
output stored in `recent_cache`. The return value of `cascade_drain` should
be the output frame for the frame number originally requested in the outer
`arAllFramesReady` call. All other frames computed in the cascade are
"pre-computed side effects" stored in `recent_cache`, to be served via
the cache-hit fast path when VS later calls `getFrame` for those numbers.

See Appendix E (Cascade Drain Worked Example) and Appendix F (Cache-Hit
Fast Path Worked Example) for concrete illustrations.

### MODE_SPECULATING entry

```cpp
void cnr3_enter_speculating(CNR3CacheManager* cache, int trigger_frame, int gap) {
    cache->mode                 = MODE_SPECULATING;
    cache->speculate_target     = trigger_frame;
    cache->speculate_gap        = gap;
    cache->speculate_countdown  = compute_speculate_window(gap);
}
```

---

## 12. Cache-Hit Fast Path

When the cascade drain (Section 11) pre-computes output frames as side
effects, those frames are stored in `recent_cache`. When VS subsequently
calls `getFrame(n, arAllFramesReady)` for one of those pre-computed frames,
CNR3 must return the cached result immediately without recomputation.

**This path is mandatory for correctness.** Without it, CNR3 would recompute
output[n] using a potentially different `prev_output` than was used during
the cascade, producing wrong output or crashes.

```cpp
// Lookup: returns the cached VSFrame* for frame_number, or nullptr if not cached.
// Does NOT acquire an additional reference — the caller must cloneFrameRef
// if it needs to retain the frame beyond the cache's lifetime.
const VSFrame* cnr3_recent_cache_get(const CNR3RecentCache* cache, int frame_number) {
    if (frame_number < 0) return nullptr;
    int slot = frame_number % CNR3_RECENT_CACHE_SIZE;
    if (cache->slots[slot].frame_number == frame_number) {
        return cache->slots[slot].frame;
    }
    return nullptr;  // slot is empty or holds a different (stale) frame number
}

// Store: saves output frame for frame_number in the ring buffer.
// Acquires a cloneFrameRef on the stored frame.
// Evicts (freeFrame) the previous occupant of the slot if non-empty.
void cnr3_recent_cache_put(CNR3RecentCache* cache, int frame_number,
                            const VSFrame* frame, const VSAPI* vsapi) {
    int slot = frame_number % CNR3_RECENT_CACHE_SIZE;
    // Evict old occupant
    if (cache->slots[slot].frame_number >= 0 && cache->slots[slot].frame != nullptr) {
        vsapi->freeFrame(cache->slots[slot].frame);
    }
    cache->slots[slot].frame_number = frame_number;
    cache->slots[slot].frame        = vsapi->cloneFrameRef(frame);
}
```

### arAllFramesReady cache-hit check

The cache-hit check must be the **first** thing done in `arAllFramesReady`,
before any other logic:

```cpp
// At the top of the arAllFramesReady branch:
const VSFrame* cached = cnr3_recent_cache_get(&d->cache.recent, n);
if (cached != nullptr) {
    // Return a new reference — VS takes ownership of the returned frame,
    // but recent_cache retains its own reference for future predecessor lookups.
    return vsapi->cloneFrameRef(cached);
}
```

### Ring buffer aliasing

The ring buffer uses `frame_number % CNR3_RECENT_CACHE_SIZE` as the slot
index. Two frame numbers that differ by exactly `CNR3_RECENT_CACHE_SIZE`
alias to the same slot. The validity check (`slot.frame_number == frame_number`)
prevents stale hits: if frame 50 is in slot 0, a lookup for frame 100 (which
also maps to slot 0 for size=50) will find frame_number=50 ≠ 100 and return
nullptr correctly.

---

## 13. The lookup_predecessor Function

`lookup_predecessor(frame_number)` retrieves the output frame for
`frame_number` from the cache, used to find the predecessor `output[N-1]`
before computing `output[N]`. It is called inside `cascade_drain`.

```cpp
// Returns a borrowed pointer to the predecessor output frame, or nullptr.
// Does NOT acquire an additional reference. The caller must not freeFrame
// the returned pointer; it is owned by prev_output or recent_cache.
const VSFrame* cnr3_lookup_predecessor(const CNR3CacheManager* cache,
                                        int frame_number) {
    // Frame -1 has no predecessor (used when f == 0).
    if (frame_number < 0) return nullptr;

    // Fast path: check prev_output first.
    // prev_output always holds the most recently computed frame's output.
    // For a sequential cascade, next_needed - 1 == frame_number,
    // so this will hit on almost every call.
    if (cache->prev_output != nullptr) {
        // We don't store the frame number alongside prev_output explicitly,
        // but next_needed - 1 is always the frame number of prev_output
        // (invariant maintained by cascade_drain).
        // The caller always passes (next_needed - 1) as frame_number,
        // so this is always the right frame.
        return cache->prev_output;
    }

    // Fallback: check recent_cache ring buffer.
    // Handles backward scrubs and post-recovery lookups where prev_output
    // may have been reset.
    const VSFrame* cached = cnr3_recent_cache_get(&cache->recent, frame_number);
    if (cached != nullptr) return cached;

    // Fallback: check checkpoint store.
    // Handles the case where frame_number is exactly on a checkpoint boundary
    // and has been evicted from recent_cache.
    for (int i = 0; i < cache->checkpoints.count; i++) {
        if (cache->checkpoints.slots[i].frame_number == frame_number) {
            return cache->checkpoints.slots[i].output_frame;
        }
    }

    // Predecessor not found.
    return nullptr;
}
```

### Invariant: prev_output always equals output[next_needed - 1]

`cascade_drain` maintains this invariant: after each frame is computed,
`prev_output` is updated to hold a `cloneFrameRef` of that frame's output.
At the start of the next iteration, `frame_number = next_needed - 1` is
always the frame just computed, so `prev_output` is always correct for the
fast path.

The fallback paths exist for:
- Post-recovery startup where `prev_output` has been reassigned from a
  checkpoint rather than computed locally.
- Rare cases where `prev_output` is null (filter just initialised, or
  state was reset at a scene change).

---

## 14. Jump Size Detection and Speculate Window Scaling

### Gap measurement

```cpp
int gap = n - cache->next_needed;
```

Measured at the start of `arAllFramesReady` for frame `n`. A gap of 0
means `n == next_needed` (exactly on time). A gap of 1–24 is normal jitter
on the 3900X. A gap > 160 is definitively a user seek.

A negative gap (`n < next_needed`) means frame `n` was already computed
and should be in `recent_cache`. The cache-hit fast path (Section 12)
handles this before the gap is even measured.

### Proportional speculate window

```cpp
int compute_speculate_window(int gap) {

    // Guard against zero denominator (should never occur if constants
    // satisfy CNR3_LARGE_JUMP_THRESHOLD > CNR3_REORDER_WINDOW).
    static_assert(CNR3_LARGE_JUMP_THRESHOLD > CNR3_REORDER_WINDOW,
        "CNR3_LARGE_JUMP_THRESHOLD must be strictly greater than CNR3_REORDER_WINDOW");

    if (gap <= CNR3_REORDER_WINDOW) {
        // Normal jitter range — full base window.
        // The gap is within expected thread-jitter bounds; wait the full
        // base window before declaring a problem.
        return CNR3_SPECULATE_BASE;
    }

    if (gap > CNR3_LARGE_JUMP_THRESHOLD) {
        // Unambiguous large seek — no point speculating.
        // Recovery is cheaper than waiting.
        return 0;
    }

    // Medium gap: linear taper from CNR3_SPECULATE_BASE (at gap == REORDER_WINDOW)
    // down to 0 (at gap == LARGE_JUMP_THRESHOLD).
    float t = (float)(gap - CNR3_REORDER_WINDOW)
            / (float)(CNR3_LARGE_JUMP_THRESHOLD - CNR3_REORDER_WINDOW);
    // t == 0.0 at gap == REORDER_WINDOW   → window = SPECULATE_BASE
    // t == 1.0 at gap == LARGE_JUMP_THRESHOLD → window = 0
    int window = (int)((float)CNR3_SPECULATE_BASE * (1.0f - t));
    return std::clamp(window, 0, CNR3_SPECULATE_MAX);
}
```

Visualised:

```
Speculate window (frames to wait before recovery)
    32 ─┐
        │╲
        │  ╲
        │    ╲
        │      ╲
     0  └────────╲─────────────────────────────
        0   32   160     gap size (frames)
             ↑    ↑
          REORDER  LARGE_JUMP
          _WINDOW  _THRESHOLD

Gap ≤ 32:   full window (32) — thread jitter; wait it out
Gap 32–160: linear taper   — medium jump; proportional wait
Gap > 160:  window = 0     — unambiguous seek; instant recovery
```

### Large jump threshold rationale

`CNR3_LARGE_JUMP_THRESHOLD = 160` frames = 6.4 seconds PAL:
- No thread-jitter scenario on any plausible hardware produces a 160-frame gap.
- A user scrubbing 6+ seconds forward is definitively seeking, not jitter.
- The value is expressed as `5 × CNR3_REORDER_WINDOW` so it scales with
  the reorder window if that constant is adjusted.

---

## 15. Recovery Strategy

### When recovery is triggered

Recovery is entered when:
1. `compute_speculate_window(gap) == 0` (gap >= `CNR3_LARGE_JUMP_THRESHOLD`), OR
2. `speculate_countdown` reaches 0 without gap filling, OR
3. `seek_mode == CNR3_SEEK_RECOVER` and any gap > `CNR3_REORDER_WINDOW`

### Finding the recompute start

```cpp
int cnr3_find_recompute_start(const CNR3CacheManager* cache, int target) {

    int ideal_start = target - CNR3_RECOMPUTE_LOOKBACK;
    if (ideal_start < 0) ideal_start = 0;

    // Find the largest checkpoint frame number <= ideal_start.
    // Only iterate over valid (non-empty) slots.
    int best_checkpoint = -1;
    for (int i = 0; i < cache->checkpoints.count; i++) {
        int fn = cache->checkpoints.slots[i].frame_number;
        if (fn >= 0 && fn <= ideal_start && fn > best_checkpoint) {
            best_checkpoint = fn;
        }
    }

    if (best_checkpoint >= 0) {
        // Warm start from checkpoint.
        return best_checkpoint;
    }

    // No suitable checkpoint found.
    // Also check recent_cache: if output[ideal_start] is in cache,
    // use that as the warm start (saves recomputing from frame 0).
    for (int f = ideal_start; f >= 0; f--) {
        if (cnr3_recent_cache_get(&cache->recent, f) != nullptr) {
            return f;
        }
    }

    // Cold start: no usable prior state found.
    // Only expected for seeks within the first CNR3_RECOMPUTE_LOOKBACK frames,
    // or before any checkpoints have been stored.
    return 0;
}
```

### Recovery cost analysis

```
Checkpoint interval = 50, recompute lookback = 50:

Case 1 — warm seek (checkpoints accumulated during prior linear playback):
  Nearest checkpoint to ideal_start is at most (CHECKPOINT_INTERVAL - 1) = 49
  frames before ideal_start.
  Recompute cost = 49 + RECOMPUTE_LOOKBACK = 49 + 50 = 99 frames maximum.
  Bounded: always ≤ 2 × CNR3_RECOMPUTE_LOOKBACK = 100 frames.
  Independent of clip length.  ← Key invariant.

Case 2 — cold seek (no checkpoints yet, e.g. preview before first linear play):
  No checkpoint available. Falls back to cold start from frame 0.
  Recompute cost = target frames (can be large).
  Bounded by: target, which may be the full clip length.
  Mitigation: during recovery, opportunistically store checkpoints
  at every CNR3_CHECKPOINT_INTERVAL frame, so that subsequent seeks
  are warm seeks. After the first cold recovery to any target, all
  future seeks within the recovered range are warm.

Case 3 — seek within first CNR3_RECOMPUTE_LOOKBACK frames:
  Ideal start would be negative; clamped to 0.
  Recompute cost = target (at most CNR3_RECOMPUTE_LOOKBACK frames).
  Cheap by definition.
```

### Executing recovery

```cpp
void cnr3_execute_recovery(CNR3Data* d, int recovery_start, int recovery_target,
                            VSFrameContext* frameCtx, VSCore* core,
                            const VSAPI* vsapi) {
    // Load initial prev_output from checkpoint at recovery_start,
    // or from recent_cache, or null if recovery_start == 0.
    if (d->cache.prev_output) {
        vsapi->freeFrame(d->cache.prev_output);
        d->cache.prev_output = nullptr;
    }

    const VSFrame* warm_prev = cnr3_lookup_predecessor(&d->cache, recovery_start);
    if (warm_prev != nullptr) {
        d->cache.prev_output = vsapi->cloneFrameRef(warm_prev);
    }
    // warm_prev == nullptr is correct for recovery_start == 0 (scene start).

    d->cache.next_needed = recovery_start + 1;

    // Recompute frames recovery_start+1 through recovery_target in order.
    for (int f = recovery_start + 1; f <= recovery_target; f++) {

        // Get source frame. It may already be in the reorder buffer
        // (pre-fetched during arInitial) or needs to be retrieved now
        // (it was requested via requestFrameFilter in arInitial for this range).
        const VSFrame* src_f = vsapi->getFrameFilter(f, d->node, frameCtx);

        // Also remove from reorder buffer if present (clean up).
        if (d->cache.reorder.pending.count(f)) {
            vsapi->freeFrame(d->cache.reorder.pending[f]);
            d->cache.reorder.pending.erase(f);
        }

        // Scene change check.
        bool is_scene_start = (f == 0) || (d->cache.prev_output == nullptr);
        if (!is_scene_start && d->scene_chroma) {
            is_scene_start = cnr3_detect_scene_change(src_f, d->cache.prev_output,
                                                       d, vsapi);
            if (is_scene_start) {
                cnr3_invalidate_recursive_state(&d->cache, vsapi);
            }
        }

        // Allocate and compute output frame.
        VSFrame* dst = vsapi->newVideoFrame(
            vsapi->getVideoFrameFormat(src_f),
            vsapi->getFrameWidth(src_f, 0),
            vsapi->getFrameHeight(src_f, 0),
            src_f, core);

        if (is_scene_start) {
            cnr3_copy_all_planes(src_f, dst, vsapi);
        } else {
            cnr3_copy_plane(src_f, dst, 0, vsapi);
            cnr3_filter_chroma(src_f, d->cache.prev_output, dst, 1, d, vsapi);
            cnr3_filter_chroma(src_f, d->cache.prev_output, dst, 2, d, vsapi);
        }

        vsapi->freeFrame(src_f);

        // Store in recent_cache and checkpoints.
        cnr3_recent_cache_put(&d->cache.recent, f, dst, vsapi);
        cnr3_maybe_store_checkpoint(&d->cache, f, dst, vsapi);

        // Update prev_output.
        if (d->cache.prev_output) vsapi->freeFrame(d->cache.prev_output);
        d->cache.prev_output = vsapi->cloneFrameRef(dst);
        vsapi->freeFrame(dst);

        d->cache.next_needed = f + 1;
    }

    d->cache.mode = MODE_STREAMING;
}
```

### Synchronous source fetch precondition

`cnr3_execute_recovery` calls `vsapi->getFrameFilter(f, d->node, frameCtx)`
for each frame `f` in the recovery range. This is only valid if those frames
were previously requested via `vsapi->requestFrameFilter(f, d->node, frameCtx)`
in the same `getFrame` invocation's `arInitial` pass.

**This is the reason arInitial must request all frames from
`recovery_start+1` to `n` (see Section 10).** If arInitial requests only
`src[n]`, then `getFrameFilter` calls for `src[recovery_start+1]` through
`src[n-1]` in the recovery loop would be invalid and would crash or return
garbage.

The recovery detection in `arInitial` uses the same `cnr3_find_recompute_start`
function called with `d->cache.next_needed` as the state at the time of the
`arInitial` call. This gives the same `recovery_start` that `arAllFramesReady`
will use.

---

## 16. The frameData Parameter

VapourSynth's `getFrame` callback signature is:

```cpp
const VSFrame* VS_CC getFrame(int n, int activationReason,
                               void* instanceData, void** frameData,
                               VSFrameContext* frameCtx, VSCore* core,
                               const VSAPI* vsapi);
```

`frameData` is a `void**` that VS provides as a per-invocation scratch space.
It is unique per `(frame number, getFrame invocation)` — VS guarantees that
the same `frameData` pointer is passed to both the `arInitial` and
`arAllFramesReady` calls for the same frame in the same invocation.

**CNR3 uses `frameData` to pass the recovery plan from `arInitial` to
`arAllFramesReady`**, because:
- `arInitial` is where we detect whether a large jump has occurred and
  compute `recovery_start` (we know `n` and `cache.next_needed` at that point).
- `arAllFramesReady` is where we execute the recovery (source frames are now ready).
- There is no other VS-sanctioned mechanism to pass per-frame state between
  the two passes.

### Usage pattern

```cpp
// arInitial:
CNR3FrameState* fs = new CNR3FrameState();
fs->recovery_needed = true;
fs->recovery_start  = computed_start;
fs->recovery_target = n;
*frameData = (void*)fs;       // store pointer for arAllFramesReady

// arAllFramesReady:
CNR3FrameState* fs = (CNR3FrameState*)(*frameData);
if (fs != nullptr) {
    // Use fs->recovery_needed, fs->recovery_start, fs->recovery_target
    delete fs;      // must free; VS does not manage this allocation
    *frameData = nullptr;
}
```

### Ownership

- `frameData` storage is allocated with `new` in `arInitial`.
- It must be `delete`d in `arAllFramesReady` (or in an error path that
  bypasses `arAllFramesReady`).
- VS does not free or track this allocation. Forgetting to `delete` leaks.
- `*frameData` should be set to `nullptr` in `arInitial` for normal frames
  (no recovery needed), so that `arAllFramesReady` can safely check for null
  without dereferencing garbage.

---

## 17. Checkpoint Management

### Storage

```cpp
void cnr3_maybe_store_checkpoint(CNR3CacheManager* cache, int frame_number,
                                  const VSFrame* output, const VSAPI* vsapi) {
    if (frame_number % CNR3_CHECKPOINT_INTERVAL != 0) return;

    CNR3CheckpointStore& store = cache->checkpoints;
    int slot = store.next_slot;

    // Evict oldest checkpoint if store is full.
    // next_slot always points to the oldest entry when count == CNR3_CHECKPOINT_COUNT.
    if (store.slots[slot].frame_number >= 0 && store.slots[slot].output_frame != nullptr) {
        vsapi->freeFrame(store.slots[slot].output_frame);
    }

    store.slots[slot].frame_number  = frame_number;
    store.slots[slot].output_frame  = vsapi->cloneFrameRef(output);
    store.next_slot = (slot + 1) % CNR3_CHECKPOINT_COUNT;
    if (store.count < CNR3_CHECKPOINT_COUNT) store.count++;
}
```

### Lookup by exact frame number

```cpp
const VSFrame* cnr3_find_checkpoint(const CNR3CacheManager* cache, int frame_number) {
    for (int i = 0; i < cache->checkpoints.count; i++) {
        if (cache->checkpoints.slots[i].frame_number == frame_number) {
            return cache->checkpoints.slots[i].output_frame;
        }
    }
    return nullptr;
}
```

### Lookup: nearest checkpoint at or before target

```cpp
// Returns the frame number of the nearest checkpoint <= target,
// or -1 if no such checkpoint exists.
int cnr3_find_nearest_checkpoint_before(const CNR3CacheManager* cache, int target) {
    int best = -1;
    for (int i = 0; i < cache->checkpoints.count; i++) {  // iterate count, not CNR3_CHECKPOINT_COUNT
        int fn = cache->checkpoints.slots[i].frame_number;
        if (fn >= 0 && fn <= target && fn > best) {
            best = fn;
        }
    }
    return best;
}
```

Note: iterate `cache->checkpoints.count`, not the full `CNR3_CHECKPOINT_COUNT`
array size. Only `count` slots contain valid entries; the rest have
`frame_number == -1` and would be incorrectly matched by the `fn <= target`
check if iterated.

### Eviction policy

Checkpoints are evicted in **FIFO** order (oldest first) when the store is full.
This is correct because:
- During linear encode, old checkpoints are never needed again once the
  encode has moved past them.
- During seeking, the most recently computed checkpoints are closest to
  the user's current position and most likely to be useful.
- FIFO requires no sorting or LRU tracking and is O(1).

The `next_slot` pointer always points to the oldest entry in the circular
buffer (or the first empty slot when `count < CNR3_CHECKPOINT_COUNT`).
Overwriting `next_slot` evicts the oldest checkpoint atomically.

---

## 18. Scene Change Interaction

CNR3 has a `scdthr` (scene change detection threshold) parameter.
When a scene change is detected between frame N-1 and frame N, the
recursive chroma filter state should reset: `output[N]` is treated as
a new scene start and computed from `source[N]` only (no history from
the previous scene).

### Scene change and recursive state

```cpp
// Called inside cascade_drain and cnr3_execute_recovery before computing
// output[f], when f > 0 and prev_output is available.
// Returns true if a scene change is detected between prev_output and src_f.
bool cnr3_detect_scene_change(const VSFrame* src_f, const VSFrame* prev_output,
                               const CNR3Data* d, const VSAPI* vsapi);
// Implementation: compare luma SAD or similar metric between src_f
// and prev_output's luma plane. If metric > d->scdthr (scaled to bit depth),
// return true. Details are part of the core filter algorithm, not the
// cache manager.

// Called when a scene change is confirmed. Resets recursive state
// without invalidating the frame cache (checkpoints and recent_cache
// remain intact for recovery purposes).
void cnr3_invalidate_recursive_state(CNR3CacheManager* cache, const VSAPI* vsapi) {
    if (cache->prev_output) {
        vsapi->freeFrame(cache->prev_output);
        cache->prev_output = nullptr;
    }
    // Do NOT clear recent_cache or checkpoints here.
    // They remain valid for predecessor lookup in non-chroma planes
    // and for recovery navigation. Only the chroma recursive state
    // (embodied by prev_output) is reset.
}
```

### Scene change and checkpoints

A frame immediately following a scene change is a natural checkpoint anchor,
even if it does not fall on the `CNR3_CHECKPOINT_INTERVAL` grid. The
programmer may optionally store an extra checkpoint at scene boundaries:

```cpp
if (is_scene_start && d->scene_chroma) {
    // Store a checkpoint at the scene boundary regardless of interval.
    // This ensures recovery across a scene cut always has a nearby anchor.
    cnr3_force_store_checkpoint(&d->cache, f, dst, vsapi);
}
```

`cnr3_force_store_checkpoint` is identical to `cnr3_maybe_store_checkpoint`
but without the modulo check.

### Scene change and recovery

When recovery walks through a scene change (i.e. `recovery_start < scene_cut_frame
<= recovery_target`), the scene change detection logic inside `cnr3_execute_recovery`
will detect it and reset `prev_output` at the cut. This is correct: the
recursive state is naturally re-initialised at the cut point, and frames
after the cut accumulate fresh history.

If `recovery_start` falls within a scene (not at a cut), the first
`CNR3_RECOMPUTE_LOOKBACK` frames of the recovery will have "wrong" history
(since they start from a checkpoint that may be before the cut). This is
acceptable — the filter will converge to correct output within a few frames
after the cut. This is the same convergence behaviour as any scene-aware
temporal filter.

---

## 19. Reorder Buffer Overflow Handling

The reorder buffer is nominally bounded to `CNR3_REORDER_WINDOW` entries.
In normal operation, the cascade drain empties the buffer as fast as frames
arrive, so it rarely holds more than a handful of entries.

However, if VS issues requests faster than the buffer drains — or if a
genuine seek occurs and many frames pile up — the buffer may receive more
than `CNR3_REORDER_WINDOW` entries.

### Overflow detection

```cpp
// Called inside arAllFramesReady before inserting a new entry.
bool cnr3_reorder_is_full(const CNR3ReorderBuffer* buf) {
    return (int)buf->pending.size() >= CNR3_REORDER_WINDOW;
}
```

### Overflow policy

When the reorder buffer is full and a new frame `n` arrives that cannot
immediately be drained (because `n > next_needed` and the buffer is at
capacity):

**Option 1 — Drop the new frame (reject):**
Do not insert `src[n]` into the reorder buffer. Free it immediately.
Return an error frame for `n`. VS may re-request it.
Simple, conservative, no memory explosion.

**Option 2 — Evict the largest-numbered pending frame:**
Remove the pending frame with the highest frame number (furthest from
`next_needed`) and replace it with the new arrival if the new arrival
is closer to `next_needed`.
Maximises the chance of cascade drain making progress.

**Option 3 — Trigger speculative mode early:**
If the buffer is full, treat the current state as a large gap and enter
`MODE_SPECULATING` or `MODE_RECOVERING` immediately, regardless of gap size.

**Recommended for initial implementation: Option 1.**
The initial implementation should treat a full reorder buffer as an
overflow error and return an error frame. This is safe, easy to debug,
and will only be triggered by pathological scheduling. If it triggers
frequently in practice, migrate to Option 2 or increase
`CNR3_REORDER_WINDOW`.

```cpp
if (cnr3_reorder_is_full(&d->cache.reorder)) {
    vsapi->setFilterError("CNR3: reorder buffer overflow. "
        "Increase reorder_window or reduce vspipe -r depth.", frameCtx);
    return nullptr;
}
```

---

## 20. BestSource Integration Notes

BestSource is the recommended source plugin. Its behaviour relevant to
this design:

**During linear playback:**
- Decodes in forward order with a small internal lookahead.
- Contributes a few frames of additional jitter on top of VS thread scheduling.
- Effective jitter at CNR3: likely 4–16 frames on the 3900X.
  `CNR3_REORDER_WINDOW = 32` comfortably covers this.

**During random access (seeking):**
- BestSource supports indexed random access via its own index files.
- Seeking to an arbitrary frame pays a decode penalty (distance to nearest
  keyframe in the source file). This is internal to BestSource and invisible
  to CNR3.
- From CNR3's perspective, BestSource delivers `arAllFramesReady` for the
  requested source frame when that frame is decoded and ready. CNR3 does not
  need to know or care about BestSource's internal seek cost.
- BestSource's seek penalty is separate from and additive to CNR3's own
  recovery recompute cost.

**No special handling required:**
BestSource behaves as a standard VS source plugin from CNR3's perspective.
All interaction is via `requestFrameFilter` / `getFrameFilter` as normal.
CNR3 makes no BestSource-specific API calls.

**BestSource lookahead and CNR3_REORDER_WINDOW:**
BestSource may internally decode a few frames beyond the requested frame.
These pre-decoded frames may arrive at CNR3's arAllFramesReady slightly
ahead of the requested frame, contributing to the effective jitter depth.
`CNR3_REORDER_WINDOW = 32` provides ample margin over BestSource's typical
lookahead depth.

---

## 21. Reference Counting Obligations

Every `VSFrame*` stored in the cache manager must have a `cloneFrameRef`
call to acquire the reference, and a corresponding `freeFrame` call to
release it. Failure to do so causes either memory leaks (missing `freeFrame`)
or use-after-free crashes (missing `cloneFrameRef`).

### Per-store obligations

```
Reorder buffer (pending source frames):
  On insert:       cloneFrameRef(src_frame)     ← acquire
  On drain:        freeFrame(src_frame)          ← release after pixel read
  On overflow/evict: freeFrame(src_frame)        ← release without processing

Recent output cache (ring buffer):
  On insert:       cloneFrameRef(output_frame)   ← acquire
  On slot evict:   freeFrame(old_slot.frame)     ← release before overwrite

Checkpoint store:
  On insert:       cloneFrameRef(output_frame)   ← acquire
  On slot evict:   freeFrame(slot.output_frame)  ← release (FIFO)

prev_output (single rolling reference):
  On update:       cloneFrameRef(new_frame)      ← acquire new ref
                   freeFrame(old_prev)           ← release old ref
  On scene change: freeFrame(prev_output)        ← release; set to nullptr
  On destroy:      freeFrame(prev_output)        ← release final ref

Returned dst frame (from arAllFramesReady):
  Created by:      newVideoFrame(…)              ← refcount = 1, you own it
  Before return:   cloneFrameRef(dst)            ← for recent_cache
  Before return:   cloneFrameRef(dst)            ← for checkpoint (if applicable)
  On return dst:   ownership transfers to VS     ← do NOT freeFrame after return
                                                    do NOT read or write after return

Cache-hit return (from recent_cache):
  cloneFrameRef(cached)                          ← acquire new ref for VS
  return cloneFrameRef(cached)                   ← VS takes this ref
  recent_cache retains its own ref               ← do NOT freeFrame the cache slot
```

### Summary table

| Store | Acquire when | Release when |
|---|---|---|
| Reorder buffer entry | source frame retrieved via arAllFramesReady | drained (processed) or dropped on overflow |
| Recent cache slot | output frame computed | slot overwritten by newer frame (ring eviction) |
| Checkpoint slot | checkpoint frame stored | slot evicted (FIFO, on next_slot overwrite) |
| `prev_output` | each frame computed or checkpoint loaded | next frame computed, scene change, or filter destroyed |
| `frameData` allocation | `new` in arInitial | `delete` in arAllFramesReady |
| Returned `dst` | `newVideoFrame` | `return dst` transfers to VS; do not free |
| Cache-hit return | `cloneFrameRef(cached)` | VS frees after use; `recent_cache` retains its own ref |

---

## 22. Filter Destroy / Cleanup

The filter's free callback must release all held references.
This is called by VS when the script ends or the filter is garbage-collected.

```cpp
void VS_CC CNR3Free(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    CNR3Data* d = (CNR3Data*)instanceData;
    if (!d) return;

    // Release prev_output
    if (d->cache.prev_output) {
        vsapi->freeFrame(d->cache.prev_output);
        d->cache.prev_output = nullptr;
    }

    // Release all pending source frames in reorder buffer
    for (auto& [fn, frame] : d->cache.reorder.pending) {
        vsapi->freeFrame(frame);
    }
    d->cache.reorder.pending.clear();

    // Release all valid recent cache entries
    for (int i = 0; i < CNR3_RECENT_CACHE_SIZE; i++) {
        if (d->cache.recent.slots[i].frame_number >= 0 &&
            d->cache.recent.slots[i].frame != nullptr) {
            vsapi->freeFrame(d->cache.recent.slots[i].frame);
            d->cache.recent.slots[i].frame        = nullptr;
            d->cache.recent.slots[i].frame_number = -1;
        }
    }

    // Release all valid checkpoint entries
    // Use count, not CNR3_CHECKPOINT_COUNT, to skip uninitialised slots
    for (int i = 0; i < CNR3_CHECKPOINT_COUNT; i++) {
        if (d->cache.checkpoints.slots[i].frame_number >= 0 &&
            d->cache.checkpoints.slots[i].output_frame != nullptr) {
            vsapi->freeFrame(d->cache.checkpoints.slots[i].output_frame);
            d->cache.checkpoints.slots[i].output_frame  = nullptr;
            d->cache.checkpoints.slots[i].frame_number  = -1;
        }
    }

    // Release source node reference
    vsapi->freeNode(d->node);

    delete d;
}
```

---

## 23. Exposed User Parameters

In addition to the existing CNR3 filter parameters, the cache manager
adds the following optional parameters. All have sensible defaults and
most users will not need to change them.

```python
core.cnr3.CNR3(
    clip,

    # Existing chroma filter parameters
    mode         = "oxx",    # filter mode string
    ln = 35,  lm = 192,      # luma thresholds (8-bit public units)
    un = 47,  um = 255,      # U chroma thresholds
    vn = 47,  vm = 255,      # V chroma thresholds
    scdthr       = 10.0,     # scene change detection threshold
    scene_chroma = False,    # reset chroma state on scene change
    debug        = False,    # overlay debug info on output

    # Cache manager parameters
    seek_mode           = "auto",
    # "strict"  — error on any out-of-order access; use with vspipe -r 1 only.
    #             Zero cache overhead. Correct for sequential encode.
    # "auto"    — full cache manager active. Handles jitter and seeks.
    #             Default; works for both vspipe and preview.
    # "recover" — skip speculation entirely; recover immediately on any gap.
    #             Fastest preview response at cost of slightly more recompute.

    reorder_window       = 32,
    # Number of out-of-order source frames held in the reorder buffer.
    # Must be >= core.num_threads for reliable jitter absorption.
    # Increase to 48 or 64 if running with more than 32 VS threads.

    recent_cache_size    = 50,
    # Number of most-recently-computed output frames retained.
    # Covers backward scrubs up to this many frames without recompute.
    # ~2 seconds PAL at default. Reduce on memory-constrained systems.

    checkpoint_interval  = 50,
    # Store a permanent output frame snapshot every N frames.
    # Must be >= recompute_lookback for recovery cost to be bounded.
    # Reduce for more seek-friendly behaviour; increase to save memory.

    checkpoint_count     = 20,
    # Maximum number of checkpoints retained (FIFO eviction).
    # checkpoint_count × checkpoint_interval = covered frame range.
    # Default: 20 × 50 = 1000 frames = 40 seconds PAL.

    large_jump_threshold = 160,
    # Gap size above which speculation is skipped and recovery is immediate.
    # Must be strictly greater than reorder_window.
    # Default: 5 × reorder_window = 160 frames = 6.4 seconds PAL.

    recompute_lookback   = 50,
    # Frames to recompute before the seek target to warm recursive state.
    # Must be <= checkpoint_interval for cost to be bounded.
    # ~2 seconds PAL at default.
)
```

### Parameter validation at filter creation

The following checks should be performed in `CNR3Create` and reported
as VS filter errors if violated:

```cpp
if (large_jump_threshold <= reorder_window)
    error("large_jump_threshold must be > reorder_window");

if (recompute_lookback > checkpoint_interval)
    error("recompute_lookback must be <= checkpoint_interval "
          "for recovery cost to be bounded");

if (reorder_window < 1 || recent_cache_size < 1 ||
    checkpoint_interval < 1 || checkpoint_count < 1)
    error("all cache sizes must be >= 1");

// Advisory (not hard error):
int vs_threads = vsapi->getCoreInfo(core)->numThreads;
if (reorder_window < vs_threads)
    log_warning("reorder_window (%d) < VS thread count (%d); "
                "normal jitter may trigger speculative mode",
                reorder_window, vs_threads);
```

---

## 24. Implementation Phases

Implement strictly in phase order. Do not begin a phase until the
previous phase passes correctness validation.

### Phase 1 — Strict streaming (initial / current)

```
seek_mode = "strict" hardcoded (no parameter yet).
fmUnordered.
Track next_needed. Error if n != next_needed (and n != 0).
Store prev_output as single cloneFrameRef.
No reorder buffer, no recent cache, no checkpoints.
No frameData usage yet.

Validation:
  vspipe -r 1 script.vpy /dev/null   → correct output, no errors
  Visual inspection of filtered VHS clip for chroma stability
```

### Phase 2 — Reorder buffer and cascade drain

```
Add CNR3ReorderBuffer.
Add cascade_drain().
Add cnr3_recent_cache_get/put (needed by cascade to avoid recompute
  of pre-drained frames).
Add cache-hit fast path check at top of arAllFramesReady.
Still no checkpoints, no recovery. Error on large gaps.

Validation:
  vspipe -r 24 script.vpy /dev/null  → correct output, no errors
  vspipe -r 1  script.vpy /dev/null  → still correct
  Confirm cascade drain resolves jitter without recompute by
    adding a debug counter for recompute invocations (should be 0
    for linear encode with adequate reorder_window).
```

### Phase 3 — Checkpoints

```
Add CNR3CheckpointStore.
Add cnr3_maybe_store_checkpoint() called from cascade_drain.
Add cnr3_find_nearest_checkpoint_before().
Still no recovery. Error on large gaps.
Checkpoint storage is silent — no behavioural change yet.

Validation:
  Run a long clip (1000+ frames) with vspipe -r 24.
  Verify checkpoints are stored at correct intervals.
  Verify eviction is FIFO and releases frame references.
  Valgrind or AddressSanitizer: no leaks or use-after-free.
```

### Phase 4 — frameData, arInitial multi-request, recovery

```
Add CNR3FrameState struct.
Add frameData allocation/deletion in arInitial / arAllFramesReady.
Add large-gap detection in arInitial.
Add multi-frame requestFrameFilter in arInitial for recovery range.
Add cnr3_execute_recovery().
Add compute_speculate_window() and MODE_SPECULATING.
Add MODE_RECOVERING transition.

Validation:
  Open clip in VapourSynth Editor.
  Seek to frame 500 (cold). Verify recovery occurs and output is correct.
  Seek back to frame 100. Verify correct output.
  Seek to frame 501. Verify no recompute (recent_cache hit).
  Verify speculate window fires correctly for medium gaps.
  Valgrind: no leaks across seek/recovery cycles.
```

### Phase 5 — Expose user parameters and seek_mode

```
Wire all constants to Python-facing filter parameters.
Add seek_mode parameter ("strict" / "auto" / "recover").
Add parameter validation in CNR3Create.
Add advisory warning for reorder_window < num_threads.
Write user-facing documentation.

Validation:
  seek_mode="strict" + vspipe -r 1 → Phase 1 behaviour unchanged.
  seek_mode="recover" + preview     → immediate recovery, no speculation.
  seek_mode="auto" + vspipe -r 24   → Phase 2 behaviour unchanged.
```

---

## Appendix A — Linear Encode Timeline (PAL, 3900X)

Scenario: `vspipe -r 24 script.vpy output.y4m`, PAL 720×576,
Ryzen 3900X (24 threads), BestSource upstream, frames 0–4 in flight.
seek_mode = "auto".

```
VS thread pool: 24 threads (A–X shown as A–E for brevity)

── arInitial passes (serialised by fmUnordered) ──────────────────────────
  gap = n - next_needed = n - 0 = n for first 5 frames.
  All gaps <= CNR3_REORDER_WINDOW (32). No recovery planned.
  *frameData = nullptr for all.

Thread A: CNR3.getFrame(0, arInitial)
  gap = 0. requestFrameFilter(0, src). *frameData = nullptr. return nullptr.
Thread B: CNR3.getFrame(1, arInitial)
  gap = 1. requestFrameFilter(1, src). *frameData = nullptr. return nullptr.
Thread C: CNR3.getFrame(2, arInitial)
  gap = 2. requestFrameFilter(2, src). *frameData = nullptr. return nullptr.
Thread D: CNR3.getFrame(3, arInitial)
  gap = 3. requestFrameFilter(3, src). *frameData = nullptr. return nullptr.
Thread E: CNR3.getFrame(4, arInitial)
  gap = 4. requestFrameFilter(4, src). *frameData = nullptr. return nullptr.
  (Each arInitial is tiny — register dependency and return immediately.)
  (All five gaps are <= 32; normal jitter range; no recovery planned.)

── upstream fetches (BestSource, can overlap across threads) ─────────────

Thread A: decoding src[0] ─────────────────────────────────┐
Thread B: decoding src[1] ───────────────────────┐         │
Thread C: decoding src[2] ──────────────┐        │         │
Thread D: decoding src[3] ─────────────────────────────┐   │
Thread E: decoding src[4] ──────┐       │        │     │   │
                                 ↓       ↓        ↓     ↓   ↓
  BestSource completion order:  [4]     [2]      [1]   [3] [0]
  (non-deterministic; determined by BestSource internals + thread jitter)

── arAllFramesReady — reorder buffer absorbs out-of-order arrivals ────────

  (Each call serialised by fmUnordered — only one active at a time.)

src[4] ready first:
  Thread E: CNR3.getFrame(4, arAllFramesReady)
  cache-hit check: recent_cache[4] = empty. Miss.
  frameData = nullptr. No recovery.
  src[4] = getFrameFilter(4, src, ctx).
  cnr3_process_frame: gap = 4 - 0 = 4. Within REORDER_WINDOW.
  Insert src[4] into reorder_buffer. cascade_drain():
    next_needed=0 not in buffer. Stop.
  return nullptr... wait — must return a VSFrame*.
  *** See note below on returning from cascade with no progress. ***

  NOTE: In the initial implementation (Phase 2), when cascade_drain
  makes no progress (next_needed not yet available), cnr3_process_frame
  returns an error frame or passthrough of src[4]. The correct full
  solution (arInitial pre-fetching for recovery) is Phase 4.
  For Phase 2, rely on vspipe -r 1 to avoid this case during testing.

src[2] ready:
  reorder_buffer = {4, 2}. cascade: next_needed=0 not in buffer. No progress.

src[1] ready:
  reorder_buffer = {4, 2, 1}. No progress.

src[3] ready:
  reorder_buffer = {4, 2, 1, 3}. No progress.

src[0] ready:
  Thread A: CNR3.getFrame(0, arAllFramesReady)
  cache-hit check: miss. frameData = nullptr.
  src[0] = getFrameFilter(0, src, ctx).
  Insert src[0] → reorder_buffer = {0, 1, 2, 3, 4}.
  cascade_drain():
    f=0 in buffer. prev = lookup_predecessor(-1) = nullptr (correct: scene start).
    compute output[0] = copy(src[0]).
    recent_cache[0] = output[0]. checkpoint[0] = output[0].
    prev_output = output[0]. next_needed = 1. remove 0 from buffer.

    f=1 in buffer. prev = lookup_predecessor(0) = prev_output = output[0]. ✓
    compute output[1] = filter(src[1], output[0]).
    recent_cache[1] = output[1].
    prev_output = output[1]. next_needed = 2. remove 1.

    f=2 in buffer. compute output[2] from output[1]. Cache. next_needed = 3.
    f=3 in buffer. compute output[3] from output[2]. Cache. next_needed = 4.
    f=4 in buffer. compute output[4] from output[3].
    recent_cache[4] = output[4]. next_needed = 5. Buffer empty.

  return output[0].   ← output for the originally requested frame (n=0)

VS returns output[0] to vspipe. ✓

Later — VS calls arAllFramesReady for frames 1, 2, 3, 4:
  Each hits cache-hit fast path: recent_cache[n] is populated.
  Each returns cloneFrameRef(recent_cache[n]) immediately. No recompute. ✓

Result: all 5 frames computed correctly. Zero recompute.
Cascade drain triggered by frame 0's arrival resolved all 5 pending frames.
```

---

## Appendix B — Warm Seek Recovery Timeline

Scenario: linear playback has previously run from frame 0 to at least frame 450,
accumulating checkpoints at frames 0, 50, 100, ..., 450. The user then seeks
to frame 500. seek_mode = "auto".

```
State at time of seek:
  mode = MODE_STREAMING (or idle after encode)
  next_needed = (some value > 450, or 0 if filter was idle)
  checkpoints: {0, 50, 100, 150, 200, 250, 300, 350, 400, 450}
  prev_output = output[450] (or whatever the last computed frame was)
  recent_cache covers frames ~401–450

VS calls CNR3.getFrame(500, arInitial):
  gap = 500 - next_needed.
  Assume next_needed = 0 (filter idle). gap = 500 > CNR3_LARGE_JUMP_THRESHOLD(160).
  compute_speculate_window(500) = 0 → instant recovery.

  cnr3_find_recompute_start(cache, 500):
    ideal_start = 500 - 50 = 450.
    Nearest checkpoint <= 450: checkpoint[450]. ✓
    recovery_start = 450.

  Request src[451] through src[500] via requestFrameFilter (50 calls).
  *frameData = new CNR3FrameState{recovery_needed=true,
                                   recovery_start=450, recovery_target=500}.
  return nullptr.

VS fetches src[451..500] (BestSource seeks to frame 451, decodes forward).

VS calls CNR3.getFrame(500, arAllFramesReady):
  cache-hit check: recent_cache[500] = miss.
  frameData → recovery_needed=true, recovery_start=450, recovery_target=500.
  Execute cnr3_execute_recovery(d, 450, 500, …):
    Load checkpoint[450] as prev_output. ← warm start from known-good state.
    next_needed = 451.
    for f = 451 to 500:
      src_f = getFrameFilter(f, src, ctx)  ← all available (requested in arInitial)
      compute output[f] = filter(src_f, prev_output)
      recent_cache[f] = output[f]
      checkpoint at 500 stored (500 % 50 == 0).
      prev_output = output[f]
      next_needed = f + 1
    mode = MODE_STREAMING.
  return cloneFrameRef(recent_cache[500]).  ← output[500] correct. ✓

Recompute cost: 50 frames (451–500). Bounded by CNR3_RECOMPUTE_LOOKBACK. ✓

User scrubs forward: frames 501, 502, 503 …
  Each has prev_output available (prev_output = output[500] after recovery).
  Zero recompute for sequential frames after the seek. ✓

User scrubs backward to frame 490:
  recent_cache covers 451–500. cnr3_recent_cache_get(490) = hit. ✓
  Return immediately from cache. ✓
```

---

## Appendix C — Cold Seek Recovery Timeline

Scenario: the user opens a script in a preview tool and immediately seeks
to frame 500 without any prior linear playback. No checkpoints have been
accumulated. seek_mode = "auto".

```
State at time of seek:
  mode = MODE_STREAMING
  next_needed = 0
  checkpoints: {} (empty)
  prev_output = nullptr
  recent_cache: all empty

VS calls CNR3.getFrame(500, arInitial):
  gap = 500 - 0 = 500 > CNR3_LARGE_JUMP_THRESHOLD(160). Instant recovery.
  cnr3_find_recompute_start(cache, 500):
    ideal_start = 500 - 50 = 450.
    Nearest checkpoint <= 450: none found.
    Nearest recent_cache entry <= 450: none found.
    Cold start: recovery_start = 0.

  Request src[1] through src[500] via requestFrameFilter (500 calls).
  *frameData = new CNR3FrameState{recovery_needed=true,
                                   recovery_start=0, recovery_target=500}.
  return nullptr.

VS fetches src[1..500]. BestSource decodes from the beginning of the file.
Cost: full decode from frame 1 (or nearest keyframe) to frame 500.
This is expensive. It is the unavoidable cost of seeking on a recursive
filter with no prior state.

VS calls CNR3.getFrame(500, arAllFramesReady):
  cnr3_execute_recovery(d, 0, 500, …):
    recovery_start = 0: prev_output = nullptr (scene start). ✓
    next_needed = 1.
    for f = 1 to 500:
      compute output[f] from src[f] + prev_output
      recent_cache[f % 50] = output[f]    ← ring wraps; only last 50 retained
      checkpoint at f=50, 100, 150, ..., 500 stored
      prev_output = output[f]
      next_needed = f + 1
  Return output[500]. ✓

Recompute cost: 500 frames. Expensive, but unavoidable cold start.

Subsequent seeks within this session are now WARM:
  Checkpoints accumulated at {50, 100, 150, ..., 500}.
  Any future seek near any of these will cost at most 50 frames. ✓

MITIGATION: For workflows where the user frequently seeks, consider
running a background "warm-up" pass with vspipe before opening in the
preview tool. This accumulates checkpoints at zero extra cost and makes
all subsequent preview seeks cheap.
```

---

## Appendix D — Proportional Speculate Window Examples

```
Constants: CNR3_REORDER_WINDOW=32, CNR3_LARGE_JUMP_THRESHOLD=160,
           CNR3_SPECULATE_BASE=32, CNR3_SPECULATE_MAX=64

Gap    t value   Window   Interpretation
────   ───────   ──────   ─────────────────────────────────────────────────────
  1    n/a         32     Well within jitter range. Full window. Normal.
 16    n/a         32     Still jitter. Full window. Normal.
 32    0.000       32     Edge of jitter range. Full window.
 50    0.141       27     Slightly unusual. Reduced window.
 80    0.375       20     Probably a small scrub in preview. Moderate wait.
 96    0.500       16     Mid-range. Shorter wait.
128    0.750        8     Likely a deliberate seek. Short wait.
144    0.875        4     Almost certainly a seek.
160    1.000        0     At threshold. Instant recovery.
200    > 1.0        0     Above threshold. Instant recovery.
500    > 1.0        0     Large seek. Instant recovery.
5000   > 1.0        0     Cold seek. Instant recovery.

Formula: t = (gap - REORDER_WINDOW) / (LARGE_JUMP_THRESHOLD - REORDER_WINDOW)
         window = clamp((int)(SPECULATE_BASE * (1.0f - t)), 0, SPECULATE_MAX)
```

---

## Appendix E — Cascade Drain Worked Example

Scenario: reorder_buffer = {1, 3, 4, 5}, next_needed = 1.
Frame 2 arrives and triggers a cascade.
Originally-requested frame (outer arAllFramesReady call) = frame 2.

```
Before cascade:
  reorder_buffer = {1, 3, 4, 5}
  next_needed    = 1
  prev_output    = output[0]   (computed earlier; cloneFrameRef held)
  recent_cache: slot[0]=output[0], others empty

Frame 2's src arrives. Insert → reorder_buffer = {1, 2, 3, 4, 5}.

cascade_drain(requested_n = 2):

  Iteration 1: f = next_needed = 1.
    f=1 in buffer? YES.
    prev = lookup_predecessor(0):
      prev_output frame number = 0 (invariant) → return prev_output = output[0]. ✓
    compute output[1] = filter(src[1], output[0]).
    recent_cache[1 % 50 = 1] = output[1].  (evict old occupant: empty, no-op)
    checkpoint: 1 % 50 != 0, skip.
    freeFrame(old prev_output = output[0]).
    prev_output = cloneFrameRef(output[1]).
    next_needed = 2.
    freeFrame(src[1]). Remove 1 from buffer → {2, 3, 4, 5}.

  Iteration 2: f = 2.
    f=2 in buffer? YES.
    prev = lookup_predecessor(1) → prev_output = output[1]. ✓
    compute output[2] = filter(src[2], output[1]).
    recent_cache[2] = output[2].
    freeFrame(prev_output). prev_output = output[2]. next_needed = 3.
    Remove 2 → buffer = {3, 4, 5}.
    f == requested_n (2). result_for_caller = cloneFrameRef(output[2]).

  Iteration 3: f = 3.
    compute output[3]. recent_cache[3] = output[3].
    prev_output = output[3]. next_needed = 4. Remove 3 → {4, 5}.
    f != requested_n. result_for_caller unchanged.

  Iteration 4: f = 4.
    compute output[4]. recent_cache[4] = output[4].
    prev_output = output[4]. next_needed = 5. Remove 4 → {5}.

  Iteration 5: f = 5.
    compute output[5]. recent_cache[5] = output[5].
    prev_output = output[5]. next_needed = 6. Remove 5 → {}.

  f=6 not in buffer. Stop.

return result_for_caller = cloneFrameRef(output[2]).   ← returned to VS for getFrame(2)

After cascade:
  reorder_buffer = {}
  next_needed    = 6
  prev_output    = output[5]
  recent_cache: slots 0–5 populated with output[0]–output[5]

All 5 pending frames resolved in one cascade. Zero recompute.
```

---

## Appendix F — Cache-Hit Fast Path Worked Example

Continuing from Appendix E. VS now calls getFrame for frames 1, 3, 4, 5
(which were pre-computed in the cascade above).

```
VS calls CNR3.getFrame(1, arInitial):
  gap = 1 - 6 = -5. Negative gap: frame already computed.
  requestFrameFilter(1, src). return nullptr.
  (VS may avoid this call if it has output[1] cached internally,
   but we must handle it if VS does call us.)

VS calls CNR3.getFrame(1, arAllFramesReady):
  cache-hit check: cnr3_recent_cache_get(recent, 1).
    slot = 1 % 50 = 1. slots[1].frame_number = 1 == 1. HIT. ✓
  return cloneFrameRef(recent_cache[1].frame).
  (No source frame retrieved. No pixel work. Immediate return.)

VS calls CNR3.getFrame(3, arAllFramesReady):
  cache-hit: slots[3].frame_number = 3. HIT.
  return cloneFrameRef(recent_cache[3].frame). ✓

VS calls CNR3.getFrame(4, arAllFramesReady):
  cache-hit: slots[4].frame_number = 4. HIT.
  return cloneFrameRef(recent_cache[4].frame). ✓

VS calls CNR3.getFrame(5, arAllFramesReady):
  cache-hit: slots[5].frame_number = 5. HIT.
  return cloneFrameRef(recent_cache[5].frame). ✓

All four pre-computed frames served from cache.
No pixel work performed. No additional source frames fetched.
```

---

*Document generated for the `vapoursynth-cnr3` project.*
*Covers cache manager design, VapourSynth API4, Windows x64, R76+.*
*Companion document: `vapoursynth-cnr3-interaction-model.md`*
