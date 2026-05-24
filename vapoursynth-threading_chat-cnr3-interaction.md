# VapourSynth API4 Plugin Interaction Model — CNR3 Conceptual Guide

A conceptual reference for understanding how VapourSynth interacts with a native C/C++ plugin DLL,
specifically for a **recursive temporal filter** such as `vapoursynth-cnr3`.

---

## Table of Contents

1. [Linear Encode — The Normal Case](#1-linear-encode--the-normal-case)
2. [Frame Threading — Parallel Requests](#2-frame-threading--parallel-requests)
3. [Seeking and Out-of-Order Requests](#3-seeking-and-out-of-order-requests)
4. [Filter Modes](#4-filter-modes)
5. [Frame Ownership and Lifetime](#5-frame-ownership-and-lifetime)
6. [What Is a Clip?](#6-what-is-a-clip)
7. [CNR3 Specifics](#7-cnr3-specifics)

---

## 1. Linear Encode — The Normal Case

### Who asks for frames, and how does it flow?

In a normal `vspipe script.vpy output.y4m` run, the call chain is:

```
vspipe
  └─ requests output clip frame 0, 1, 2… in order
        └─ VapourSynth core calls your plugin's getFrame(n, …)
              └─ plugin calls requestFrameFilter(n, sourceClip, frameCtx)
                    (tells VS "I need source frame n before I can continue")
              └─ VS suspends your callback
              └─ VS fetches source frame n (from upstream filter/decoder)
              └─ VS resumes your callback
              └─ plugin calls getFrameFilter(n, sourceClip, frameCtx)
                    (now retrieves the ready source frame)
              └─ plugin allocates a new output frame via newVideoFrame()
              └─ plugin writes pixels into it
              └─ plugin returns the VSFrame*
        └─ VS hands the VSFrame* back to vspipe
  └─ vspipe encodes it and requests frame 1, …
```

### `requestFrameFilter` vs `getFrameFilter`

**`requestFrameFilter(n, clip, ctx)`** — a *declaration of intent*.
You call this during the **first** invocation of your `getFrame`
(`activationReason == arInitial`). You are saying:
*"Before you call me again, please make sure frame n from clip is ready."*
VS may then schedule and compute that frame asynchronously.

**`getFrameFilter(n, clip, ctx)`** — the *retrieval*.
You call this only after VS calls you back with
`activationReason == arAllFramesReady`.
The frame is guaranteed to be in memory. You now have a
`const VSFrame*` you can read pixels from.

> **Analogy:** `requestFrameFilter` is placing the order at a restaurant.
> `getFrameFilter` is picking up the food from the counter when your number is called.

### The Two-Pass Callback Lifecycle

```
getFrame(n, activationReason=arInitial, …)
  → call requestFrameFilter(n, src, ctx)   // declare what you need
  → return nullptr                          // suspend; VS goes to fetch it

getFrame(n, activationReason=arAllFramesReady, …)
  → call getFrameFilter(n, src, ctx)        // retrieve ready src frame
  → newVideoFrame(…)                        // allocate output
  → do pixel work
  → cloneFrameRef(dst) → store as prev      // keep ref for next frame
  → return dst                              // hand ownership to VS
```

### When to Allocate, When to Free, What Returning Means

- Call `vsapi->newVideoFrame(…)` to get a writable destination frame. You own it until you return it.
- Once you `return dst` from `getFrame`, VS takes ownership. You must not touch it afterwards.
- Source frames from `getFrameFilter` are borrowed references — call `freeFrame` on them when done,
  unless you are storing them (in which case you hold the reference until you explicitly free it).
- If you want to keep the previous output frame for the next call (CNR3's core need),
  call `cloneFrameRef(dst)` on the destination before returning it,
  and store that clone as `previous_output_frame`.

### Complete per-frame flow (linear encode example)

```
vspipe asks output clip for frame 0
  CNR3 getFrame(0, arInitial):
    requestFrameFilter(0, src, ctx)  → suspend

  CNR3 getFrame(0, arAllFramesReady):
    src0 = getFrameFilter(0, src, ctx)
    dst0 = newVideoFrame(…)
    copy/filter pixels → dst0
    prev = cloneFrameRef(dst0)       ← store for frame 1
    freeFrame(src0)
    return dst0                      → VS/vspipe receives output[0]

vspipe asks output clip for frame 1
  CNR3 getFrame(1, arInitial):
    requestFrameFilter(1, src, ctx)  → suspend

  CNR3 getFrame(1, arAllFramesReady):
    src1 = getFrameFilter(1, src, ctx)
    dst1 = newVideoFrame(…)
    read prev (= output[0]) + src1 → filter → dst1
    freeFrame(prev)                  ← release old clone
    prev = cloneFrameRef(dst1)       ← store for frame 2
    freeFrame(src1)
    return dst1                      → VS/vspipe receives output[1]

… and so on for frame 2, 3, …
```

---

## 2. Frame Threading — Parallel Requests

### Can VapourSynth request frames ahead of time?

Yes. VapourSynth has a thread pool and a scheduler. For filters that declare
`fmParallel`, VS may issue many `getFrame` calls concurrently.
Even with `fmUnordered`, VS is allowed to request frame N+1 before N is finished —
it just will not call *your* `getFrame` for two frames simultaneously from different threads
(see [Filter Modes](#4-filter-modes)).

### Why Recursive State Is Dangerous Under Normal Threading

```
Thread A: getFrame(5) → reads prev_output[4], writes output[5]
Thread B: getFrame(6) → reads prev_output[5]   ← NOT READY YET!
                                                   Race condition / wrong data
```

A recursive filter creates a strict data dependency:
frame N **must** complete before frame N+1 can start.
This is fundamentally at odds with frame-parallel scheduling,
which assumes frames are independent.

### What `fmUnordered` Guarantees (and Does Not)

| Guarantee | Not guaranteed |
|---|---|
| Your `getFrame` is never called from two threads at the same time | Frames will be requested in order |
| You can have mutable plugin-level state without a mutex | That N-1 will always be done before N is requested |
| Single-threaded access to your filter instance | No lookahead or out-of-order requests |

> **Note:** `fmUnordered` is sufficient for a "strict streaming" CNR3
> as long as you also enforce in-order access yourself
> (e.g. error if `n != expected_next_n`).
> VS will not call you in parallel, but it *might* request frames
> out of order if a seek or preview happens.

### Why Recursive Filters Don't Naturally Fit Frame-Parallel Scheduling

```
Time →
  vspipe wants: 0  1  2  3  4  5
  VS scheduler: ↓  ↓  ↓  ↓  ↓  ↓   (issues requests ahead)
  CNR3 needs:   0 must finish before 1 can start
                         1 must finish before 2 …
  → No parallelism possible. The dependency chain is strictly serial.
```

This is inherent to the algorithm, not a VS limitation.
The only way to parallelise a recursive temporal filter is to break the recursion
(e.g. bounded lookback, checkpoints, or a non-recursive source-window approximation).

---

## 3. Seeking and Out-of-Order Requests

### What Happens When a User Seeks

Preview editors (VapourSynth Editor, vsedit, Vapoursynth Preview)
request frames by number on demand.
A user clicking frame 500 causes VS to call `getFrame(500)` on your filter.
There is no guarantee that frames 0–499 were ever computed.

```
User seeks to frame 500:
  VS calls CNR3.getFrame(500)
  CNR3 needs output[499] to compute output[500]
  output[499] was never computed → ???
```

### Out-of-Order Request Examples

**Order: 0, 1, 2, 3** — Normal. Each frame has its predecessor. ✓

**Order: 0, 2, 1, 3:**
```
Frame 2 requested before frame 1.
output[2] = f(src[2], output[1])  ← output[1] unknown!

Strict policy:    error on n=2.
Recompute policy: compute 0, 1, 2 then return 2.
```

**Order: 500, 501, 100:**
```
Frame 500 has no ancestor chain computed.

Recompute policy:    walk 0 → 500 (expensive!).
Checkpoint policy:   walk nearest_checkpoint → 500.
Bounded lookback:    use src[498..500] as approximation (not historically correct).
```

### Possible Policies

| Policy | Behaviour | Suitability for CNR3 |
|---|---|---|
| **a) Strict streaming** | Only accept `n == expected_next`. Error otherwise. | Best for `vspipe`. Breaks in preview. |
| **b) Error on OOO** | Return error frame or throw if out-of-order. | Safe, honest. Annoying in preview. |
| **c) Recompute from 0** | On any seek, walk from frame 0 up to n. | Correct but very slow for large n. |
| **d) Checkpoints** | Store fully-filtered frames at intervals (e.g. every 100). Recompute from nearest checkpoint. | Good balance of correctness vs speed. |
| **e) Bounded lookback** | Only look back k frames (e.g. 5). Approximate for seek targets. | Fast, slightly wrong after seeks. |
| **f) Non-recursive approx** | Use a fixed source window (e.g. `src[n-2..n]`). No stored state. | Fully seek-safe, weaker effect. |

> **Recommendation for initial CNR3 implementation:**
> Policy **a) strict streaming** is the right choice.
> It gives correct output for the primary use case (vspipe encode)
> and fails loudly rather than silently producing wrong results.

---

## 4. Filter Modes

### Overview

| Mode | What VS guarantees | Use when |
|---|---|---|
| `fmParallel` | Multiple frames may be in-flight simultaneously across threads. `getFrame` may be called concurrently for different frame numbers. | Stateless per-frame filters (colour correction, resize, sharpening). Each output frame depends only on the corresponding source frame(s). |
| `fmParallelRequests` | Multiple `requestFrameFilter` calls may be in flight, but only one `getFrame` (arAllFramesReady) runs at a time. | Filters that need to request multiple source frames per output frame but do their work serially. |
| `fmUnordered` | Only one `getFrame` call active on your instance at any moment. Order of frame requests is **not** guaranteed. | Stateful filters that need serial access to their own state but don't care about request order. **Correct starting point for CNR3.** |
| `fmFrameState` ⚠️ *(legacy)* | Like `fmUnordered` but also disables VS's internal frame caching for this filter. | Was intended for filters with heavy per-frame state. Discouraged: kills caching, hurts performance. **Do not use for new filters.** |

### Why `fmFrameState` Is Discouraged

Disabling VS's frame cache means every downstream request for your output frame
triggers a full recompute, even if that frame was already produced.
This is catastrophic for performance and makes seek behaviour even less predictable.
The VS API docs explicitly suggest avoiding it for new filters.

### `fmUnordered` as a Starting Point for CNR3

`fmUnordered` is the correct choice for the first CNR3 implementation.
It gives you single-threaded access to your plugin's state
(so `previous_output_frame` is safe to read/write without a mutex),
while still integrating properly with the VS scheduler.
Pair it with strict in-order enforcement in your `getFrame` logic.

### Extra Care with `fmUnordered`

- You must track `expected_next_frame` yourself and decide how to handle out-of-order requests.
- VS may still issue out-of-order requests (seeks, preview),
  so your policy choice (strict / error / recompute) must be implemented explicitly.
- The single-threaded guarantee means no mutex is needed for your own state,
  but you must not store pointers to frames that VS has already reclaimed.

---

## 5. Frame Ownership and Lifetime

VapourSynth uses **reference counting** for frames.
Every frame has an internal refcount.
You must ensure that every frame you receive or create is eventually freed exactly once.

### Ownership Table

| Frame type | How you get it | Who owns it | How to release |
|---|---|---|---|
| Source frame | `getFrameFilter(n, clip, ctx)` | You hold a reference. VS keeps its own. | `freeFrame(f)` when done reading. Do not free if you store it. |
| Destination frame | `newVideoFrame(format, w, h, …)` | You exclusively own it. Refcount = 1. | Either `return` it (transfers ownership to VS) or `freeFrame` on error. |
| Cloned frame | `cloneFrameRef(f)` | You hold an additional reference. Refcount +1. | `freeFrame` when you no longer need it. |
| Returned frame | You `return dst` from `getFrame` | VS now owns it. Refcount managed by VS. | **Do not touch it after returning.** |

### Why `previous_output_frame` Needs Reference Management

```cpp
// Frame N:
dst = newVideoFrame(…);          // refcount = 1, you own it
// … write pixels …
prev = cloneFrameRef(dst);       // refcount = 2, you AND VS will own a ref
return dst;                      // VS takes its ref; your clone ref remains

// Frame N+1:
// read pixels from prev         // still valid — you hold your clone ref
// … compute output[N+1] …
freeFrame(prev);                 // decrement your clone ref
prev = cloneFrameRef(new_dst);   // take new clone for frame N+2
return new_dst;
```

> ⚠️ **Use-after-free danger:**
> If you store `dst` directly and return it (without cloning first),
> VS may free the frame at any time after you return.
> Reading it in the next `getFrame` call is a use-after-free bug.

> **On filter close** (the free/destroy callback):
> call `freeFrame(prev)` if it is non-null, to release the final stored reference.

---

## 6. What Is a Clip?

### A Clip Is a Lazy Frame-Producing Object

A **clip** in VapourSynth is not a buffer of frames.
It is a description of *how to produce frames on demand*.
When you write `core.cnr3.CNR3(source_clip, …)` in a Python script,
no frames are computed. A new clip object is created that knows:
*"To produce my frame N, call CNR3's `getFrame(N)`."*

```python
# Python script — no computation happens here:
src = core.lsmas.LWLibavSource("input.mkv")   # lazy: no decode yet
out = core.cnr3.CNR3(src, ln=35, …)           # lazy: no filtering yet
out.set_output()                               # registers the output clip

# vspipe then drives evaluation:
#   requests out.frame(0)   → triggers CNR3.getFrame(0)
#                                → triggers src.getFrame(0)
#                                    → triggers decoder.getFrame(0)
#                                        → actual I/O and decode happens here
#   requests out.frame(1)   → same chain for frame 1
#   …
```

### Key Points

- The plugin is called **per requested output frame**, not once for the whole clip.
- The plugin never receives "all frames" — only the frame number currently being requested.
- Upstream clips are also lazy: `requestFrameFilter(n, src, ctx)` starts the chain
  that eventually produces source frame n.
- VS's frame cache means if frame 5 is requested twice, it is only computed once;
  the second requester gets the cached result.
- Your filter's output is also cached by VS (unless `fmFrameState` — avoid it).

> **Mental model:** a clip is a function `frame_number → pixel_data`.
> The whole VS graph is a composition of such functions.
> `vspipe` evaluates the composition at integer points 0, 1, 2, …, N−1.

---

## 7. CNR3 Specifics

### What CNR3 Must Store

Because `output[N] = f(src[N], output[N-1])`,
CNR3 must retain a reference to the previous output frame between `getFrame` calls:

```cpp
struct CNR3Data {
    VSNode*          node;            // source clip
    // … parameters (ln, lm, un, um, vn, vm, scdthr, …) …
    const VSFrame*   prev_output;     // the output[N-1] frame
    int              expected_n;      // strict streaming counter
};
```

### Why Strict Linear `vspipe` Works

```
vspipe requests: 0, 1, 2, 3, 4, …  (always sequential)

getFrame(0):
  no prev → treat as scene start
  output[0] = src[0]  (pass-through / initialise recursive state)
  store cloneFrameRef(output[0]) as prev_output
  expected_n = 1

getFrame(1):
  n == expected_n ✓
  read prev_output (= output[0])
  compute output[1] = f(src[1], output[0])
  freeFrame(old prev_output)
  store cloneFrameRef(output[1])
  expected_n = 2

getFrame(2):
  n == expected_n ✓  … and so on.
```

Under strict streaming with `vspipe`, this always works correctly.
`vspipe` requests frames in strict ascending order, and `fmUnordered`
ensures only one `getFrame` runs at a time.

### Why Random Preview Seeking Breaks It

```
User seeks to frame 500 in preview:

  getFrame(500):
    n=500, expected_n=0  → MISMATCH
    prev_output is null or stale
    output[499] was never computed → cannot produce correct output[500]

  Policy choice kicks in:
    Strict:     return error frame
    Recompute:  internally loop getFrame(0)…getFrame(499), then compute 500
    Checkpoint: find nearest stored checkpoint ≤ 500, walk from there
```

### Does `vspipe` Ever Parallelise Requests?

`vspipe` can request multiple frames in parallel to keep the pipeline busy
(controlled by the `-r` requests flag).
However, with `fmUnordered`, VS will **not** call your `getFrame`
for two frames simultaneously.
VS serialises the calls to your filter instance.
The parallelism happens in upstream filters (the decoder, other processing stages),
not in your recursive filter.

> **Example:** if `vspipe -r 4` issues four concurrent requests,
> VS will call `getFrame(0)`, `getFrame(1)`, `getFrame(2)`, `getFrame(3)`
> in sequence on your `fmUnordered` filter,
> each one waiting for the previous to complete before VS hands it to you.
> This is safe for CNR3 — but only if you enforce order.

### Why the First Implementation Should Be Quality-First and Serial

- The primary use case is `vspipe` encode, which is always linear.
- `fmUnordered` + strict order check = correct, safe, simple.
- No need for mutexes, no need for checkpoints yet.
- Correct recursive filtering is more valuable than seek-safe approximation
  for VHS cleanup workflows.

### Future Improvements

| Feature | What it enables |
|---|---|
| Checkpoint every K frames | Seeks land at worst K frames from a known-good state |
| Bounded lookback (k = 5) | Approximate seek support; small quality loss after a cut |
| Scene-change reset | `scdthr` param already planned; resetting state at cuts is naturally seek-friendly at scene boundaries |
| Non-recursive source window | Full seek safety at cost of weaker temporal smoothing |

### Frame Request Order — Summary Comparison

```
Order: 0, 1, 2, 3
  → Normal linear encode. Every predecessor is available. ✓

Order: 0, 2, 1, 3
  → Frame 2 arrives before frame 1.
    output[1] not yet computed when frame 2 is requested.
    Strict policy: error at n=2 (expected n=1).
    Recompute policy: walk 0→2 to produce predecessors, then return 2.

Order: 500, 501, 100
  → Frame 500 has no ancestor chain computed at all.
    Strict/error policy:   error immediately.
    Recompute policy:      walk 0→500 (very slow for a long clip).
    Checkpoint policy:     walk nearest_checkpoint→500 (bounded cost).
    Bounded lookback:      use src[498..500] as state seed (fast, approximate).
    Non-recursive approx:  ignore history entirely; use src[498..500] window only.
```

---

## Appendix: Quick Reference

### Filter Mode Selection

```
Is your filter stateless (each output frame depends only on input[n])?
  YES → fmParallel

Does your filter need to request multiple source frames but work serially?
  YES → fmParallelRequests

Does your filter have mutable state shared across frame calls?
  YES → fmUnordered  (and enforce ordering yourself if needed)

Never use fmFrameState for new filters.
```

### CNR3 getFrame Pseudocode (Strict Streaming)

```cpp
VSFrame* CNR3GetFrame(int n, int activationReason,
                       void* instanceData, void** frameData,
                       VSFrameContext* frameCtx, VSCore* core,
                       const VSAPI* vsapi)
{
    CNR3Data* d = (CNR3Data*)instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        return nullptr;
    }

    if (activationReason != arAllFramesReady)
        return nullptr;

    // Strict order enforcement
    if (n != 0 && n != d->expected_n) {
        vsapi->setFilterError("CNR3: out-of-order frame request. "
                              "Only sequential access is supported.", frameCtx);
        return nullptr;
    }

    const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);
    const VSVideoFormat* fmt = vsapi->getVideoFrameFormat(src);
    int w = vsapi->getFrameWidth(src, 0);
    int h = vsapi->getFrameHeight(src, 0);

    VSFrame* dst = vsapi->newVideoFrame(fmt, w, h, src, core);

    if (n == 0 || d->prev_output == nullptr) {
        // Scene start: pass luma and chroma unchanged
        // (or copy src to dst; recursive state initialises here)
        copy_all_planes(src, dst, vsapi);
    } else {
        // Recursive chroma filter
        copy_plane(src, dst, 0, vsapi);                    // Y unchanged
        filter_chroma_plane(src, d->prev_output, dst, 1, d, vsapi);  // U
        filter_chroma_plane(src, d->prev_output, dst, 2, d, vsapi);  // V
    }

    // Update recursive state
    if (d->prev_output)
        vsapi->freeFrame(d->prev_output);
    d->prev_output = vsapi->cloneFrameRef(dst);
    d->expected_n = n + 1;

    vsapi->freeFrame(src);
    return dst;
}
```

### Reference Counting Cheat Sheet

```
newVideoFrame(…)     → refcount = 1   (you own it exclusively)
cloneFrameRef(f)     → refcount + 1   (you gain an extra reference)
freeFrame(f)         → refcount - 1   (frame destroyed when refcount = 0)
return dst           → ownership transferred to VS; do not use dst afterwards
getFrameFilter(…)    → refcount + 1   (you must freeFrame when done)
```

---

*Document generated for the `vapoursynth-cnr3` project.*
*Covers VapourSynth API4, targeting Windows x64, R76+.*
