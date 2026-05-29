# CNR3 Output Frame Cache — Design Document

---

## SECTION 1 — Background and Rationale

### 1.1 The CNR3 Recursive Dependency Problem

To calculate an OUTPUT frame `[N]`, the inputs to that process are:

- **(a)** SOURCE frame `[N]`, blended with
- **(b)** OUTPUT frame `[N-1]`

This means that calculation of OUTPUT frame `[N]` depends on all prior OUTPUT frames
`[0]..[N-1]` being pre-calculated. In the out-of-order VapourSynth OUTPUT frame request
model, this is impossible to guarantee without special measures.

Pragmatically, and with edge cases at frame `[0]` and near the end frame acknowledged,
we assume that calculation of OUTPUT frame `[N]` depends on the pre-calculation of only
OUTPUT frames `[N-100]..[N-1]`. This still presents an interesting challenge in a world
of out-of-order and potentially parallel frame requests.

The challenge is to achieve satisfactory performance without horrendous levels of
re-calculation for each and every OUTPUT frame, even at the expense of some extra memory
usage. A per-instance cache of prior pre-calculated OUTPUT frames with checkpoint frames
addresses this. It offers quick access to pre-calculated frames at the expense of cache
management complexity and some levels of re-calculation, without the CPU cost of a full
re-calculation every time.

We cannot safely compute `output[74]` from `source[74]` alone — we need a valid
predecessor chain of OUTPUT frames. Those are likely to be in the OUTPUT frame cache.
If they are missing, we go back to the nearest prior checkpoint and calculate forward
from there, which is feasible given that the corresponding source frames will already
have been requested and retrieved in order to fill the OUTPUT frame cache holes and
establish checkpointed OUTPUT frames as starting points.

---

### 1.2 Use Cases

The design prioritises use case U1. U2 must survive but may perform poorly.

**(U1) Normal use — linear encoding (by far the dominant case)**
vspipe processes the script referencing CNR3 and streams calculated final output frames
(in order, managed by VapourSynth) into ffmpeg for encoding into a watchable video.
Even here, VS delivers OUTPUT frame requests out of frame-number order and/or in
parallel, but the requests are generally close together numerically and trend upward.

**(U2) Visual viewing with timeline scrubbing (secondary, hopefully rare)**
A user feeds the script into a video viewer and jumps around in the timeline, causing
large jumps in requested OUTPUT frame numbers. This hurts the cache concept severely
but the cache must still survive in that environment, even with very degraded
performance.

For the sake of examining cache behaviour, assume U1 which starts at frame 0 and
staggers upward. Even with fmUnordered and `-r 1`, source filters may cause VS to
deliver frame requests in any order even if close-ish numerically. As a working
example, requests arrive in this order: `80 71 79 72 78 73 77 64 75 76 ...`

---

### 1.3 The VapourSynth Callback Model

VapourSynth calls CNR3 to return a processed OUTPUT frame via a two-stage callback:

**arInitial — CNR3 declares which SOURCE frames it needs (no retrieval yet):**

1. VS calls `cnr3_get_frame(output_frame_number, arInitial)` with the output frame
   number for CNR3 to calculate and return.
2. `cnr3_get_frame` assesses what SOURCE frame numbers it needs and makes individual
   calls to `requestFrameFilter(requested_source_frame_number)`, then returns to VS.

**VS then prepares those source frames:**

3. VS does what it needs including SOURCE frame caching to get the requested
   SOURCE frames ready, then calls `cnr3_get_frame(output_frame_number, arAllFramesReady)`.

**arAllFramesReady — CNR3 retrieves frames and computes (no new requests):**

4. VS calls `cnr3_get_frame(output_frame_number, arAllFramesReady)` to signal that the
   previously requested SOURCE frames are ready.
5. `cnr3_get_frame()` retrieves the now-available SOURCE frames from VS using calls to
   `getFrameFilter()`, uses the retrieved SOURCE frames to calculate the OUTPUT frame
   requested by VS, releases the SOURCE frames, and returns the OUTPUT frame to VS.

**Important constraints:**
- `requestFrameFilter()` requests one SOURCE frame per call and is used only in arInitial
- `getFrameFilter()` retrieves one SOURCE frame per call and is used only in arAllFramesReady
- `cnr3_get_frame(...arAllFramesReady...)` may only call `getFrameFilter()` for frames
  that were explicitly requested in the matching arInitial for that invocation
- `cnr3_get_frame(...arAllFramesReady...)` returns one output frame, not a list
- Retrieval of a SOURCE frame from VS is pointer-based, not a pixel data copy — cheap
- Releasing a SOURCE frame via `freeFrame()` is cheap
- All SOURCE frames retrieved via `getFrameFilter()` must be explicitly released via
  `freeFrame()` before returning — none may be left dangling

**Complete VS inside-filter frame API:**
VapourSynth documents the following as the relevant inside-filter frame functions available
to CNR3: `requestFrameFilter`, `getFrameFilter`, `releaseFrameEarly`, `cacheFrame`, and
`setFilterError`. The filter callback itself returns a single frame pointer. Of these,
CNR3 uses `requestFrameFilter` and `getFrameFilter` in normal operation and `setFilterError`
for error reporting. `releaseFrameEarly` and `cacheFrame` are available but not currently
used — they may become relevant in future cache optimisation work.

**On VS source frame caching — performance optimisation, not a correctness dependency:**

CNR3 correctness does not depend on VS source cache behaviour. CNR3 will produce correct
output whether or not VS has cached any particular source frame. The VS source cache
affects only performance — a cold VS source cache means higher latency retrieving source
frames, not wrong output.

That said, multiple `requestFrameFilter()` calls from multiple concurrent arInitial
invocations for the same source frame are very cheap when that frame is already in VS's
source cache, because VS uses reference-counted pointers internally rather than copying
pixel data. For example, if a request for output frame 80 causes CNR3 to request source
frames `[30..80]`, and a subsequent request for output frame 79 requests source frames
`[29..79]`, VS may already have most of those source frames pointer-ready from the prior
request. CNR3 should not assume VS source cache will hold everything it wants, especially
if many overlapping ranges are requested simultaneously — but in normal U1 linear playback
the overlap is substantial and the extra VS-side work per arInitial is minimal.

Users may be advised to set VS memory limit to 8 GB or 16 GB to maximise VS source cache
retention, particularly for 1080p or high-bit-depth clips. Some cache thrashing is expected
around the prevailing set of out-of-order requests, and under multi-threading requests will
definitely arrive unordered — this is normal and expected behaviour managed by the mutex
and the checkpoint-based hole-filling strategy.

---

### 1.4 VapourSynth Filter Modes

CNR3 currently uses `fmUnordered`. The target is `fmParallelRequests`. `fmParallel` is
a future benchmarking target.

#### fmUnordered

- Only one call to `cnr3_get_frame()` active at any time, for either activation reason
- No concurrency whatsoever within the filter
- Frames can still be requested out of order by VS
- `-r 1` is currently required on the vspipe command line — it prevents VS from queuing
  multiple requests simultaneously, which would otherwise cause the out-of-order rejection
  to fire constantly
- Simplest to reason about, but single-threaded by design

#### fmParallelRequests

- Multiple concurrent `cnr3_get_frame(N, arInitial)` calls are allowed simultaneously,
  each in their own thread
- Only **one** `cnr3_get_frame(N, arAllFramesReady)` is active at any time — serialised by VS
- This is the best fit for CNR3's cache design (Proposal A) because:
  - Parallel arInitial calls allow multiple threads to prefetch source frames from VS
    simultaneously, warming the VS source cache efficiently
  - Serialised arAllFramesReady means all actual computation and cache writes happen one
    at a time, removing the concurrent-write race problem entirely
  - The mutex is still needed to protect arInitial cache reads from concurrent
    arAllFramesReady writes, but the heavy computation path is never concurrent

#### fmParallel

- Both `arInitial` and `arAllFramesReady` calls can run concurrently in multiple threads
  simultaneously
- Maximum throughput potential but maximum complexity
- Two threads could both be in arAllFramesReady simultaneously, both computing hole-fills,
  both attempting cache writes — the mutex handles the writes but redundant parallel
  computation of the same holes is wasteful
- The serial recursive dependency (`output[N]` needs `output[N-1]`) makes truly parallel
  arAllFramesReady calls difficult to exploit usefully for CNR3 specifically
- Requires per-slot `active_readers` reference counts and condition variables not needed
  under fmParallelRequests — see Appendix B Thread Safety

#### Mode comparison summary

| Mode | arInitial | arAllFramesReady | Fits Proposal A |
|---|---|---|---|
| fmUnordered | serialised | serialised | works, but needs `-r 1` |
| fmParallelRequests | concurrent | serialised | **best fit** |
| fmParallel | concurrent | concurrent | workable but complex |

`fmParallelRequests` is the recommended development target. It gives parallel VS source
cache warming for free while keeping the recursive computation path serial, which matches
the algorithm's fundamental recursive nature.

---

### 1.5 Instance Isolation

#### How VS instances work

When a VapourSynth script calls `cnr3.CNR3(clipA)` and `cnr3.CNR3(clipB)`, VS calls
`cnr3_create()` twice. Each call produces a completely independent `Cnr3Data` object
allocated on the heap via `new Cnr3Data(...)`, and VS stores a pointer to each one as
`instanceData`. Every subsequent call to `cnr3_get_frame()` and `cnr3_free()` receives
only that instance's own pointer — VS never mixes them. The two instances are
structurally incapable of seeing each other's data through the VS interface.

#### How CNR3 currently enforces this

`Cnr3Data` already contains everything per-instance:
- the node pointer
- all parameters
- all lookup tables
- the `Cnr3CacheManager` with `prev_output` and `next_needed`
- the `instance_id` (read-only after creation, so no sharing needed)

`g_cnr3_next_instance_id` is the only truly global state, and it is only ever used once
per instance at creation time to assign the ID via an atomic operation, so it is safe.

#### What changes under fmParallelRequests

Under `fmUnordered` instance isolation was trivially guaranteed because only one thread
ever touched a given instance at a time. Under `fmParallelRequests` multiple threads can
now call `cnr3_get_frame(N, arInitial)` concurrently for the same instance. They all
receive the same `instanceData` pointer — meaning they all share the same `Cnr3Data`
object and everything in it including the output frame cache. This is correct and desired
— those threads are supposed to share the cache. But it means the cache and its
associated metadata become a shared resource requiring mutex protection.

#### Instance isolation invariants for CNR3

```
1. Each CNR3 filter instance owns exactly one Cnr3Data object, created in
   cnr3_create() and destroyed in cnr3_free(). VS guarantees that instanceData
   passed to cnr3_get_frame() and cnr3_free() always points to that instance's
   own Cnr3Data and never to another instance's.

2. No static or global mutable state is shared between instances at runtime.
   g_cnr3_next_instance_id is the sole global variable and is only written
   once per instance at creation time using an atomic operation.

3. All per-instance state — parameters, lookup tables, output frame cache,
   checkpoint table, cache metadata, and controlling variables — lives inside
   Cnr3Data and is invisible to all other instances.

4. All threads serving a given instance share that instance's Cnr3Data,
   which is the intended design. Access to any mutable field within Cnr3Data
   (particularly the output frame cache and its metadata) must be protected
   by a mutex that is itself a member of Cnr3Data, ensuring the mutex is
   also per-instance and never shared across instances.

5. Threads serving instance A and threads serving instance B never contend
   on the same mutex, never read or write each other's cache, and never
   interact in any way.
```

The mutex must live **inside** `Cnr3Data`, not as a global or static. A global mutex
would accidentally serialise across instances, defeating the purpose of having
independent instances. A mutex inside `Cnr3Data` is automatically scoped to that instance.

---

### 1.6 Concurrent-Write Race Analysis

#### Under fmParallelRequests

Races are structurally impossible in arAllFramesReady because it is serialised. The mutex
is needed to protect arInitial cache index reads from the single concurrent
arAllFramesReady write, and to protect the pin_count increment/decrement on checkpoint
pool slots. See Section B.14 for the checkpoint pruning edge case that makes pin_count
necessary even under fmParallelRequests.

#### Under fmParallel — race scenarios

**Basic race — handled correctly:**

Thread A computing `output[80]` and Thread B computing `output[79]` both identify hole
`output[74]` as missing and both compute it independently. The conditional store under
mutex ("only if not already filled by another thread") prevents a double write. The
redundant computation is wasteful but not incorrect because CNR3's blend is deterministic
— same inputs always produce the same output.

**Checkpoint promotion race — safe provided frame-number ordering is strictly followed:**

Thread A completes and promotes `output[80]` to a new checkpoint. Thread B is still
computing its range `[71..79]`. Thread B's re-check correctly finds `output[70]` as its
checkpoint because `output[80]` is numerically after `output[79]` and the "strictly less than
requested frame N" rule filters it out. This is safe only if checkpoint lookup always
uses frame-number order, never insertion order or recency of write — see the CRITICAL
NOTE in Appendix B.

**The genuinely unsafe case under fmParallel — requires condition variables:**

Thread A is in arAllFramesReady computing `output[80]` and needs `output[79]` as its
final predecessor. Thread B is simultaneously in arAllFramesReady computing `output[79]`
but has not yet written it to the cache. Thread A checks the cache, finds `output[79]`
missing, and cannot wait — there is no condition variable to block on. It must either
recompute from the nearest checkpoint or return an error.

Under `fmParallelRequests` this cannot happen because arAllFramesReady is serialised.
Under `fmParallel` the only clean resolution is a per-slot condition variable — Thread A
waits until Thread B signals that `output[79]` is written. This adds significant
complexity and is the primary reason `fmParallel` is deferred to a later phase.

**Checkpoint pruning between arInitial and arAllFramesReady — affects fmParallelRequests:**

This is a real edge case that requires the `pin_count` mechanism even under
`fmParallelRequests`. Consider:

```
arInitial(80):
    selected checkpoint = output[70]
    requested source range = source[71..80]
    (source[70] not requested — output[70] is the checkpoint, not a hole)

before arAllFramesReady(80) runs:
    the pruning pass in a concurrent arAllFramesReady for a different frame
    evicts checkpoint output[70]

arAllFramesReady(80):
    output[70] is gone from the checkpoint pool
    the next available checkpoint is output[60]
    but source[61..70] were never requested in arInitial(80)
    arAllFramesReady cannot call getFrameFilter() for frames it did not request
    RESULT: arAllFramesReady(80) cannot compute correctly
```

The fix is to pin the chosen checkpoint at arInitial time and unpin it at the end of
arAllFramesReady. The pruning pass skips any checkpoint slot with `pin_count > 0`.
This guarantees the checkpoint chosen at arInitial time is still present when
arAllFramesReady runs. Full details in Section B.15.

---

## SECTION 2 — Proposal A: Per-Frame Processing Model

### 2.1 Overview

Proposal A describes how CNR3 handles each individual OUTPUT frame request under
`fmParallelRequests`. The key insight is that arInitial only declares which source frames
are needed (pessimistically covering the full range back to the nearest checkpoint), while
arAllFramesReady does all actual pixel computation and cache management.

The cache provides pre-computed OUTPUT frames so that hole-filling rarely requires more
than a small amount of recomputation from the nearest checkpoint. Under normal U1 linear
playback the cache will be largely full and most arAllFramesReady invocations will find
very few or no holes.

**What "holes" means — ascending chain-walk, not independent frame computation:**

Hole-filling is not a set of independent per-frame calculations. It is a forward chain-walk
starting from the chosen checkpoint output frame. At each step in the walk:

- If the next frame is already in the cache: use it as-is as the input to the next step.
  No recomputation needed for that frame.
- If the next frame is not in the cache (a hole): compute it using the previous frame
  (cached or just computed in this walk) as the predecessor, then store it.

This is why the ascending order constraint is not merely a preference — it is structurally
required. Each step depends on the output of the previous step. A frame cannot be computed
in isolation; it needs its predecessor. The chain-walk model also means that once a gap in
the cache is encountered and computed, all subsequent steps in that walk have a valid
predecessor regardless of what else is in the cache.

Frame N (the output frame being returned to VS) is the final step in the chain-walk.
It is computed last, after all holes between the checkpoint and N-1 have been filled.

---

### 2.2 CRITICAL NOTE — Frame-Number Order is Mandatory

```
ABSOLUTELY ALL OF:
    - cache frame search and lookup
    - checkpoint frame search and lookup
    - anything else touching the frame or checkpoint cache or related items
MUST STRICTLY respect and use only frame-number order at all times.
NO EXCEPTIONS.

Checkpoint lookup MUST always be:
    the checkpoint with the highest frame number that is strictly
    less than the requested output frame N.

It is absolutely forbidden to use any other order such as insertion order
or recency of write, since it will definitively break multi-threaded
caching logic.
```

---

### 2.3 Notionally for (a) — calculating OUTPUT frame 80

**arInitial:** *(which only identifies and makes SOURCE frame requests, does not retrieve any)*

- `{sets mutex}`
- Identify most recent checkpointed OUTPUT frame in the cache which is just prior to
  the requested OUTPUT frame
- Increment `pin_count` on that checkpoint pool slot to protect it from pruning
  between now and the end of the matching arAllFramesReady
- Record the chosen checkpoint frame number for use by arAllFramesReady
  (passed via the `frameData` pointer — see bookkeeping note below)
- `{releases mutex}`
- Makes individual calls to VS `requestFrameFilter()` for:
  - SOURCE frames covering the full range back to the most recent checkpointed OUTPUT
    frame in the cache which is just prior to the requested OUTPUT frame
    (not just currently identified cache holes, since holes visible at arAllFramesReady
    time may differ — pessimistic full-range requesting ensures arAllFramesReady always
    has everything it could possibly need)
  - and SOURCE frame 80 itself
- Returns

*--- arInitial returns here; VS prepares source frames; callback arAllFramesReady eventually begins ---*

**arAllFramesReady:** *(which only retrieves and releases frames requested in arInitial)*

- Makes individual calls to VS `getFrameFilter()` to retrieve:
  - SOURCE frames covering the full range back to the most recent checkpointed OUTPUT
    frame in the cache which is just prior to the requested OUTPUT frame
  - and SOURCE frame 80 itself
- `{sets mutex}`
- Checks the OUTPUT frame cache to identify holes in the full range back to the most
  recent checkpointed OUTPUT frame in the cache which is just prior to the requested
  OUTPUT frame
- `{releases mutex}`
- Performs ascending chain-walk from the chosen checkpoint forward to frame 79:
  - for each frame in the range in strict ascending order:
    - if the frame is already in the cache: use it as the predecessor for the next step,
      no recomputation needed
    - if the frame is not in the cache (a hole): compute it using the previous frame
      (cached or just computed in this walk) as the predecessor
- `{sets mutex}`
- Stores any newly computed hole frames into the cache, only if not already filled by
  another thread (another concurrent arInitial may have triggered a fill), updates checkpoints
- `{releases mutex}`
- Computes OUTPUT frame 80 using frame 79 (cached or just computed) as the predecessor
- `{sets mutex}`
- Stores the newly calculated OUTPUT frame 80 into the cache, only if not already
  filled by another thread, updates checkpoints
- `{releases mutex}`
- `{sets mutex}`
- Decrement `pin_count` on the checkpoint pool slot identified by `frameData`
  *(this must also happen on every early-error exit path — see Section B.14)*
- `{releases mutex}`
- Releases all SOURCE frames retrieved via `getFrameFilter()`
- Returns OUTPUT frame 80 to VS

*Note: the mutex may be acquired and released several times during the above process,
for example when re-checking before storing a frame into the cache.*

**Bookkeeping note — pin_count and frameData are companions, not alternatives:**

`pin_count` and `frameData` serve two distinct purposes and both are required:

- `pin_count` on the `Cnr3CheckpointSlot`: the protection mechanism. Prevents the
  chosen checkpoint from being pruned between arInitial and arAllFramesReady. Lives
  in the checkpoint pool struct, protected by the per-instance mutex.

- `frameData`: the identification mechanism. Carries the chosen checkpoint frame
  number from arInitial to arAllFramesReady so that arAllFramesReady knows which
  checkpoint pool slot to decrement the `pin_count` on. VapourSynth provides the
  `frameData` void pointer parameter to both callbacks for exactly this per-invocation
  state passing purpose. CNR3 currently ignores this with `(void)frameData` and will
  need to use it once cache management is implemented.

In arInitial: store the chosen checkpoint frame number into `*frameData` (cast to an
appropriate integer type such as `intptr_t`), then increment that slot's `pin_count`
under mutex.

In arAllFramesReady: read the frame number back from `*frameData`, use it to look up
the slot in the checkpoint pool, and decrement its `pin_count` under mutex — on every
exit path, success and error alike.

---

### 2.4 Notionally for (b) — calculating OUTPUT frame 79

**arInitial:**

- Mirrors frame 80's arInitial exactly:
  - `{sets mutex}` identify most recent checkpointed OUTPUT frame in the cache which is
    just prior to the requested OUTPUT frame, increment its `pin_count`, record the
    chosen checkpoint frame number for arAllFramesReady via `frameData` `{releases mutex}`
  - Makes individual calls to VS `requestFrameFilter()` for SOURCE frames covering the
    full range back to the most recent checkpointed OUTPUT frame in the cache which is
    just prior to the requested OUTPUT frame, plus SOURCE frame 79 itself
  - Returns
  - In practice most or all of those source frames are already in VS's source cache from
    the prior frame 80 arInitial, so the extra VS-side work is minimal

*--- arInitial returns here; VS prepares source frames; callback arAllFramesReady eventually begins ---*

**arAllFramesReady:**

- The process is exactly per (a):
- Makes individual calls to VS `getFrameFilter()` to retrieve:
  - SOURCE frames covering the full range back to the most recent checkpointed OUTPUT
    frame in the cache which is just prior to the requested OUTPUT frame
  - and SOURCE frame 79 itself
- By this point the OUTPUT frame cache has largely been filled by the frame 80
  arAllFramesReady processing, so perhaps none or only one output frame may be missing
- `{sets mutex}`
- Checks the OUTPUT frame cache to identify holes in the full range back to the most
  recent checkpointed OUTPUT frame in the cache which is just prior to the requested
  OUTPUT frame
- `{releases mutex}`
- Performs ascending chain-walk from the chosen checkpoint forward to frame 78:
  - for each frame in the range in strict ascending order:
    - if the frame is already in the cache: use it as the predecessor for the next step,
      no recomputation needed (by this point most frames are already cached from the
      frame 80 arAllFramesReady processing)
    - if the frame is not in the cache (a hole): compute it using the previous frame
      (cached or just computed in this walk) as the predecessor
- `{sets mutex}`
- Stores any newly computed hole frames into the cache, only if not already filled by
  another thread, updates checkpoints
- `{releases mutex}`
- Computes OUTPUT frame 79 using frame 78 (cached or just computed) as the predecessor
- `{sets mutex}`
- Stores the newly calculated OUTPUT frame 79 into the cache, only if not already
  filled by another thread, updates checkpoints
- `{releases mutex}`
- `{sets mutex}`
- Decrement `pin_count` on the checkpoint pool slot identified by `frameData`
  *(this must also happen on every early-error exit path — see Section B.14)*
- `{releases mutex}`
- Releases all SOURCE frames retrieved via `getFrameFilter()`
- Returns OUTPUT frame 79 to VS

---

## APPENDIX A — Instrumentation and Development Strategy

### A.1 Development Strategy

#### Phase 1 — VS mode fmParallelRequests

Develop and validate correctness here first. The serialised arAllFramesReady means most
races are structurally impossible, so the mutex logic, pin_count mechanism, and cache
correctness can be validated without needing to reason about concurrent computation.
Get benchmarks here as the baseline.

**fmParallel is explicitly out of scope for Phase 1.** The following must NOT be
implemented during Phase 1:
- `active_readers` per-slot reference count extension
- condition variables for the "waiting on a frame being computed" case
- any fmParallel-specific logic

The `pin_count` mechanism IS implemented in Phase 1 because it is required for
correctness under fmParallelRequests. It also happens to be the foundation for
fmParallel's `active_readers` extension, but that extension itself is Phase 2 only.

#### Phase 2 — VS mode fmParallel

Add or change code after Phase 1 benchmarks are complete:
- mode flag changed to `fmParallel`
- `active_readers` per-slot reference count added to `Cnr3CheckpointSlot`
  (extends the existing `pin_count` mechanism)
- condition variables per checkpoint slot added for the case where one arAllFramesReady
  needs a frame that another concurrent arAllFramesReady is currently computing
- related debug print code added (`MUTEX_WAIT` tag)

Run the same test suite and benchmarks. Any correctness failures that appear are by
definition concurrency bugs that were latent in Phase 1 but masked by serialisation.
The instrumentation from Phase 1 will immediately show where contention and wasted
computation occur.

This is a clean methodology. The code changes between phases are targeted and the
instrumentation carries across unchanged.

---

### A.2 Instrumentation Design

Each debug print carries:
- instance ID (already in CNR3)
- frame number being processed
- thread identifier
- category tag
- the statistic or event

Example output:
```
CNR3 [inst=1] [frame=080] [thr=A] [CACHE_HIT]    output[74] found in cache, skipped computation
CNR3 [inst=1] [frame=080] [thr=A] [CACHE_MISS]   output[74] not in cache, computing
CNR3 [inst=1] [frame=080] [thr=A] [CACHE_RACE]   output[74] computed but already filled by another thread, discarded
CNR3 [inst=1] [frame=080] [thr=A] [CACHE_WRITE]  output[74] written to cache
CNR3 [inst=1] [frame=080] [thr=A] [CKPT_PROMOTE] output[80] promoted to checkpoint
CNR3 [inst=1] [frame=080] [thr=A] [CKPT_CHOSEN]  chosen checkpoint = output[70]
CNR3 [inst=1] [frame=080] [thr=A] [HOLE_COUNT]   3 holes identified in range [71..80]
CNR3 [inst=1] [frame=080] [thr=A] [SRC_REQUEST]  requesting source frames [71..80]
CNR3 [inst=1] [frame=080] [thr=A] [SRC_RETRIEVE] retrieving source frames [71..80]
CNR3 [inst=1] [frame=080] [thr=A] [OUT_RACE]     output[80] computed but already in cache, discarded
CNR3 [inst=1] [frame=080] [thr=A] [OUT_WRITE]    output[80] written to cache
CNR3 [inst=1] [frame=080] [thr=A] [FRAME_DONE]   output[80] returned to VS
```

Category tags make it easy to grep for specific event types across a full log:
```
grep CACHE_RACE cnr3.log | wc -l
grep OUT_RACE   cnr3.log | wc -l
```

Sequential numbers on individual debug lines are less useful than category tags because
numbers tell you nothing without looking up a legend, tags are self-documenting in the
log, and tags survive code reorganisation without renumbering. A category registry in
the source serves the same documentation purpose and is more maintainable — see the full
registry in Appendix B.

---

### A.3 Statistics Worth Capturing

#### Cache efficiency

| Statistic | Tag | What it tells you |
|---|---|---|
| Holes identified in range | `HOLE_COUNT` | How sparse the cache is at request time |
| Holes already filled by recheck | `HOLE_PREFILLED` | Benefit of concurrent arInitial prefetching |
| Holes computed then discarded (race) | `CACHE_RACE` | Wasted computation under fmParallel |
| Holes filled successfully | `CACHE_WRITE` | Actual cache contribution |
| Output frame computed then discarded | `OUT_RACE` | Wasted final computation under fmParallel |
| Checkpoint chosen | `CKPT_CHOSEN` | Which checkpoint was chosen and how far back |
| Checkpoint promoted | `CKPT_PROMOTE` | Checkpoint promotion rate |
| Cache size at request time | `CACHE_CAPACITY` | Rolling cache occupancy |

#### Source frame efficiency

| Statistic | Tag | What it tells you |
|---|---|---|
| Source frames requested | `SRC_REQUEST` | Request volume per output frame |
| Source frame range width | `SRC_RANGE` | How far back each arInitial reaches |

#### Threading visibility

| Statistic | Tag | What it tells you |
|---|---|---|
| Thread ID on every event | all tags | Lets you reconstruct per-thread timelines |
| Mutex wait (if measurable) | `MUTEX_WAIT` | Contention hotspots |

---

### A.4 Per-Run Aggregate Summary

Printed at `cnr3_free()` teardown:

```
CNR3 [inst=1] [SUMMARY] frames_processed=500
CNR3 [inst=1] [SUMMARY] total_holes_identified=312
CNR3 [inst=1] [SUMMARY] holes_prefilled_by_recheck=287
CNR3 [inst=1] [SUMMARY] holes_computed=25
CNR3 [inst=1] [SUMMARY] holes_discarded_race=3
CNR3 [inst=1] [SUMMARY] output_discarded_race=1
CNR3 [inst=1] [SUMMARY] checkpoint_chosen_count=48
CNR3 [inst=1] [SUMMARY] avg_checkpoint_distance=8.3
CNR3 [inst=1] [SUMMARY] cache_hit_rate=91.9%
CNR3 [inst=1] [SUMMARY] wasted_computation_rate=0.8%
```

These numbers directly answer the fmParallelRequests vs fmParallel comparison question.
`holes_discarded_race`, `output_discarded_race`, and `wasted_computation_rate` should be
zero or near-zero under fmParallelRequests and nonzero under fmParallel.

---

## APPENDIX B — Cache Management for CNR3

### B.1 Global Tunable Constants

All tunable parameters are global constants defined in one place in the source.

```
cache_capacity          = 100   // nominal non-checkpoint pool frame count
overflow_factor         = 1.1   // non-checkpoint pool overflow multiplier (gives 110 frames)
checkpoint_interval     = 10    // frames between checkpoint promotions
checkpoint_max_retain   = 16    // maximum checkpoints in checkpoint pool
checkpoint_min_retain   = 6     // minimum checkpoints always retained
```

---

### B.2 Structure

Primary structure: segmented cache with sliding window.

**Non-checkpoint pool:**

| Property | Value |
|---|---|
| Capacity | `cache_capacity` (default 100, tunable) |
| Overflow | up to `cache_capacity * overflow_factor` (default 110) |
| Eviction order | furthest behind current frontier first (lowest frame number first) |
| Eviction trigger | exceeding overflow capacity |

**Checkpoint pool:**

| Property | Value |
|---|---|
| Capacity | `checkpoint_max_retain` (default 16, tunable) |
| Minimum retain | `checkpoint_min_retain` (default 6, tunable) |
| Eviction order | lowest frame number first |
| Eviction trigger | exceeding `checkpoint_max_retain` |
| Hard constraint | never evict if it would leave fewer than `checkpoint_min_retain` |
| Hard constraint | never evict the two checkpoints nearest to current frontier |
| Special case | frame 0 checkpoint retained permanently until teardown |

The frontier is defined as the highest frame number currently in either pool.

---

### B.3 Frame Storage

VS output frames are reference-counted heap objects managed by VS. When CNR3 calls
`vsapi->addFrameRef(frame)` it increments the reference count and gets back a pointer it
can hold onto. VS will not free that frame while CNR3 holds the reference. When CNR3
calls `vsapi->freeFrame(frame)` it decrements the count and VS frees it when the count
hits zero.

The CNR3 cache stores VS-managed frame pointers held open by `addFrameRef` — not copies,
not separate pixel buffers. CNR3's cache is essentially two ordered lists of
`(frame_number, VSFrame*)` pairs where each pointer is kept alive by an outstanding
`addFrameRef` call, one list for non-checkpoint frames and one for checkpoint frames.
This is the least-impact approach — no pixel copying, no separate memory management, VS
handles the actual frame data lifetime.

No per-slot "currently being computed" status flag or condition variable is needed under
`fmParallelRequests` — the conditional store under mutex is sufficient. These may be
revisited if and when `fmParallel` is implemented.

---

### B.4 Implementation Data Structures

```cpp
// Non-checkpoint pool — ordered by frame number, O(1) lowest-first eviction
std::map<int, const VSFrame *> non_checkpoint_pool;

// Checkpoint pool slot — frame pointer plus pin count.
// const VSFrame * matches what addFrameRef() returns and enforces that
// CNR3 never writes to cached frames (API4 immutability contract).
// pin_count is incremented at arInitial time when this checkpoint is chosen
// as the computation checkpoint for an in-flight invocation, and decremented
// on every exit path of the matching arAllFramesReady (success and error).
// The pruning pass skips any slot where pin_count > 0. See Section B.14.
struct Cnr3CheckpointSlot {
    const VSFrame *frame     = nullptr;
    int            pin_count = 0;
};

// Checkpoint pool — ordered by frame number, O(1) lowest-first eviction
std::map<int, Cnr3CheckpointSlot> checkpoint_pool;

// Cache index — O(1) average lookup across both pools (frame pointer only,
// pin_count lives in checkpoint_pool slots, not here)
std::unordered_map<int, const VSFrame *> cache_index;

// Per-instance mutex — member of Cnr3Data, never global or static
std::mutex cache_mutex;

// Frontier tracking — member of Cnr3Data
// Updated whenever a frame is written to either pool
// Updated downward if the frontier frame is evicted
int highest_cached_frame_number;
```

---

### B.5 CRITICAL NOTE — Frame-Number Order is Mandatory

```
ABSOLUTELY ALL OF:
    - cache frame search and lookup
    - checkpoint frame search and lookup
    - anything else touching the frame or checkpoint cache or related items
MUST STRICTLY respect and use only frame-number order at all times.
NO EXCEPTIONS.

Checkpoint lookup MUST always be:
    the checkpoint with the highest frame number that is strictly
    less than the requested output frame N.

It is absolutely forbidden to use any other order such as insertion order
or recency of write, since it will definitively break multi-threaded
caching logic.
```

---

### B.6 Pruning Policy

**Never evict:**
- the two checkpoints with the highest frame numbers below the current frontier
- frame 0 checkpoint (permanent)
- any frame currently pinned as the chosen checkpoint for an in-flight invocation
  (addressed by per-slot `active_readers` reference count under fmParallel,
  not needed under fmParallelRequests)

Under `fmParallel` the per-slot reference count is an integer `active_readers` per cache
slot, incremented when a thread pins that frame as its chosen computation checkpoint and decremented
when the thread is done with it. The pruning pass skips any slot where
`pin_count > 0`. Under `fmParallelRequests` this pin mechanism is required to protect
against checkpoint pruning between arInitial and arAllFramesReady — see Section B.15.
Under `fmParallel` it additionally protects frames in active use by concurrent threads.

**Evict first (non-checkpoint pool):**
- furthest behind frontier first (lowest frame number first)
- triggered when non-checkpoint pool exceeds overflow capacity

**Evict last (checkpoint pool):**
- lowest frame number first
- triggered only when checkpoint pool exceeds `checkpoint_max_retain`
- subject to never-evict constraints above

---

### B.7 Checkpoint Policy

```
Promotion:     at write time, if frame_number % checkpoint_interval == 0
Interval:      checkpoint_interval = 10 (tunable global constant)
Max retained:  checkpoint_max_retain = 16 (tunable global constant)
Min retained:  checkpoint_min_retain = 6 (tunable global constant)
Frame 0:       always checkpoint, never evicted
Eviction:      lowest frame number first, subject to min_retain and never-evict constraints
```

---

### B.8 Startup

```
Frame 0:
    computed from source only, no blend with predecessor
    immediately promoted to checkpoint
    never evicted

Frames 1 through [checkpoint_interval - 1]:
    computed normally using frame 0 as the chain checkpoint
    non-checkpoint frames, subject to normal pruning

Frame checkpoint_interval (e.g. frame 10):
    first non-zero checkpoint promoted at write time
    normal checkpoint retention rules apply from here
```

---

### B.9 Seek Behaviour

In all seek cases the same general principle applies: find the nearest checkpoint in the
cache with a frame number strictly less than the requested frame N, and compute forward
from there in ascending frame order. The cases below describe what to do when that search
succeeds or fails.

Before any checkpoint search, always check whether the requested frame N is already present
in the cache. If it is, return it immediately without any recomputation.

**Jump forward (requested frame far ahead of frontier):**
- Find nearest checkpoint in cache with frame number `< requested frame`
- If found: use as the computation checkpoint, compute forward to requested frame
- If not found: treat as a normal cache-miss — request prior `cache_capacity` source
  frames, compute forward chain from furthest available checkpoint or from frame 0 if
  none available
- Recompute cost is proportional to the distance from the chosen checkpoint to the
  requested frame
- This is the worst case for use case U2 (user visual viewing with large jumps)

**Jump backward (requested frame behind all cached frames):**
- If a checkpoint exists in cache with frame number `< requested frame` and within prior
  `cache_capacity`: use as the computation checkpoint
- If not found: same treatment as the jump-forward not-found case above

Frame 0 checkpoint is retained permanently and always provides a last-resort checkpoint.

---

### B.10 Pruning Location and Triggers

#### Where pruning runs

Pruning runs **only inside arAllFramesReady**, specifically after all hole-filling and
output frame computation are complete and all cache writes for that invocation are done,
but **before** releasing source frames and returning the output frame to VS. This means:

- Under `fmParallelRequests` pruning is automatically serialised — only one
  arAllFramesReady runs at a time so pruning never runs concurrently with itself or with
  any cache write
- Under `fmParallel` the entire pruning pass must be a single mutex-protected critical
  section

Pruning never runs in arInitial — arInitial only reads the cache index to find the
nearest checkpoint, it never modifies the cache.

#### Triggers

**Trigger 1 — non-checkpoint pool overflow (normal steady-state trigger):**

```
if non_checkpoint_pool.size() > cache_capacity * overflow_factor:
    prune non-checkpoint pool
    evict lowest frame numbers first
    stop when non_checkpoint_pool.size() <= cache_capacity
```

As the frontier advances during linear playback the pool fills, overflows, and is trimmed
back to nominal capacity.

**Trigger 2 — checkpoint pool overflow (fires roughly once per checkpoint_interval frames):**

```
if checkpoint_pool.size() > checkpoint_max_retain:
    prune checkpoint pool
    evict lowest frame numbers first
    subject to never-evict constraints:
        never reduce below checkpoint_min_retain
        never evict the two checkpoints nearest to frontier
        never evict frame 0 checkpoint
    stop when checkpoint_pool.size() <= checkpoint_max_retain
    or when never-evict constraints prevent further eviction
```

#### Order of operations within arAllFramesReady including pruning

```
1.  retrieve source frames via getFrameFilter() for the full range back to
    the most recent checkpoint and source frame N itself
2.  {mutex} check cache for holes and identify nearest checkpoint strictly
    prior to frame N as the computation checkpoint {release mutex}
3.  perform ascending chain-walk from chosen checkpoint: for each frame in range,
    use cached output frame if present, otherwise compute from predecessor and
    retrieved source frame (chain-walk, not independent per-frame computation)
4.  {mutex} conditional store hole fills only if not already filled by another
    thread, update checkpoints {release mutex}
5.  compute output frame N
6.  {mutex} conditional store output frame N only if not already filled by
    another thread, update checkpoints {release mutex}
7.  CHECK TRIGGER 1: {mutex} if non-checkpoint pool over overflow capacity,
    prune {release mutex}
8.  CHECK TRIGGER 2: {mutex} if checkpoint pool over max_retain,
    prune {release mutex}
9.  release all source frames retrieved via getFrameFilter()
10. return output frame N to VS
```

Steps 7 and 8 run after all writes for this invocation are committed so pruning decisions
see a fully consistent cache state. They run before source frame release and return so
any error in pruning does not affect the output frame already computed.

#### Teardown trigger

At `cnr3_free()` all remaining frames in both pools must be released regardless of size.
This is not pruning in the eviction sense but uses the same release path. Every slot in
both pools calls `freeFrame()` and the pools are cleared.

#### What does NOT trigger pruning

- arInitial never triggers pruning
- Reading a frame from the cache as a checkpoint does not trigger pruning
- A cache miss does not trigger pruning
- Exceeding nominal capacity alone does not trigger pruning — only exceeding overflow
  capacity triggers it, giving headroom for burst computation during seeks and
  hole-filling

---

### B.11 Thread Safety Summary

**Under fmParallelRequests:**
- Multiple arInitial calls may run concurrently — arInitial only reads the cache index
  and calls `requestFrameFilter()`, both safe under mutex
- Only one arAllFramesReady runs at a time — all cache writes, hole fills, checkpoint
  promotions, and pruning passes are automatically serialised
- The per-instance mutex in `Cnr3Data` protects concurrent arInitial reads from the
  single concurrent arAllFramesReady write
- Per-slot `pin_count` on each checkpoint pool slot is required — see Section B.14
- No condition variable needed

**Under fmParallel (future):**
- Both arInitial and arAllFramesReady may run concurrently in multiple threads
- The per-instance mutex in `Cnr3Data` must protect all cache reads and writes
- The entire pruning pass must be a single mutex-protected critical section
- Per-slot `pin_count` (already required for fmParallelRequests) also prevents
  eviction of frames in active use by concurrent threads under fmParallel
- Condition variables per cache slot must be implemented to handle the case where one
  thread needs `output[N-1]` as its computation checkpoint while another thread is
  currently computing `output[N-1]`

**Instance isolation (both modes):**
- Each CNR3 filter instance owns exactly one `Cnr3Data` object
- The per-instance mutex lives inside `Cnr3Data` and is never shared across instances
- Threads serving instance A and threads serving instance B never contend on the same
  mutex and never access each other's cache
- `g_cnr3_next_instance_id` is the only global mutable state and is accessed only once
  per instance at creation time via an atomic operation

---

### B.12 Possible Use Cases

Pruning methods must take into account all use cases:

**(a) Normal — dominant case**
Incremental walking up the frame numbers in ascending order, some out of order,
`[0]..[last_frame_number]`

**(b) Upstream temporal filter**
Usually in sequence from `[0]..[last_frame_number]`, but for a frame `[N]` the upstream
filter may request VS perhaps 2 to 6 frames into the future. After any initial burst this
probably looks like case (a) to CNR3 since VS will cache CNR3 output frames.

**(c) User visual viewing — very small probability**
Large jumps up the frame count then incremental up; large jumps down the frame count
then incremental up.

**(d) Descending order — tiny probability**
Incremental walking down the frame numbers in descending order, some out of order,
`[last_frame_number]..[0]`

---

### B.13 Memory Usage

Cache holds at steady state:
- Non-checkpoint pool: up to 110 frames (100 nominal + 10% overflow)
- Checkpoint pool: up to 16 frames
- **Total: up to 126 frames maximum at any one time**

All figures below are for 8-bit YUV 4:2:0. For 10-bit or 16-bit inputs, all figures
double since VapourSynth stores samples above 8-bit as 16-bit values.

#### Frame sizes (8-bit YUV 4:2:0)

| Clip | Width | Height | Y bytes | U bytes | V bytes | Total bytes | Total KB |
|---|---|---|---|---|---|---|---|
| 720x576 progressive | 720 | 576 | 414,720 | 103,680 | 103,680 | 622,080 | 607.5 KB |
| 720x288 field (576i separated) | 720 | 288 | 207,360 | 51,840 | 51,840 | 311,040 | 303.75 KB |
| 1920x1080 progressive | 1920 | 1080 | 2,073,600 | 518,400 | 518,400 | 3,110,400 | 3,037.5 KB |
| 1920x540 field (1080i separated) | 1920 | 540 | 1,036,800 | 259,200 | 259,200 | 1,555,200 | 1,518.75 KB |

#### Cache memory at maximum occupancy (126 frames, 8-bit)

**(a) VHS PAL 720x576 25p — one CNR3 instance:**

| Pool | Frames | Bytes/frame | Total |
|---|---|---|---|
| Non-checkpoint (max) | 110 | 622,080 | 68,428,800 |
| Checkpoint (max) | 16 | 622,080 | 9,953,280 |
| **Total** | **126** | | **≈ 74.7 MB** |

**(b) VHS PAL 720x576 25i field-separated — two CNR3 instances:**

| Pool | Frames | Bytes/frame | Total per instance |
|---|---|---|---|
| Non-checkpoint (max) | 110 | 311,040 | 34,214,400 |
| Checkpoint (max) | 16 | 311,040 | 4,976,640 |
| **Total per instance** | **126** | | **≈ 37.4 MB** |

**Two instances total: ≈ 74.8 MB**

**(c) 1080p progressive — one CNR3 instance:**

| Pool | Frames | Bytes/frame | Total |
|---|---|---|---|
| Non-checkpoint (max) | 110 | 3,110,400 | 342,144,000 |
| Checkpoint (max) | 16 | 3,110,400 | 49,766,400 |
| **Total** | **126** | | **≈ 373.9 MB** |

**(d) 1080i25 field-separated — two CNR3 instances:**

| Pool | Frames | Bytes/frame | Total per instance |
|---|---|---|---|
| Non-checkpoint (max) | 110 | 1,555,200 | 171,072,000 |
| Checkpoint (max) | 16 | 1,555,200 | 24,883,200 |
| **Total per instance** | **126** | | **≈ 186.9 MB** |

**Two instances total: ≈ 373.8 MB**

#### Summary

| Scenario | Instances | Max cache frames | Memory (8-bit) | Memory (16-bit) |
|---|---|---|---|---|
| (a) 720x576 25p | 1 | 126 | ≈ 74.7 MB | ≈ 149.4 MB |
| (b) 720x576 25i fields | 2 | 126 per instance | ≈ 74.8 MB | ≈ 149.6 MB |
| (c) 1080p | 1 | 126 | ≈ 373.9 MB | ≈ 747.8 MB |
| (d) 1080i25 fields | 2 | 126 per instance | ≈ 373.8 MB | ≈ 747.6 MB |

These figures are for the CNR3 output frame cache only. VS itself also maintains a source
frame cache whose size is controlled by the VS memory limit setting. The CNR3 cache memory
sits on top of whatever VS allocates for its own source cache. All 8-bit scenarios are
comfortably manageable on any modern system with 8 GB RAM. The 16-bit 1080p scenario
approaches 750 MB for the CNR3 cache alone — users may need to set the VS memory limit
to 8 GB or 16 GB. The tunable constants `cache_capacity`, `overflow_factor`, and
`checkpoint_max_retain` directly control these figures for memory-constrained environments.

---

### B.14 Checkpoint Pin Mechanism

#### The problem

At arInitial time CNR3 identifies the most recent checkpointed OUTPUT frame strictly
prior to requested frame N and pessimistically requests source frames covering the full
range from that checkpoint forward to N. Between arInitial returning and arAllFramesReady
being called, the pruning pass in a concurrent arAllFramesReady for a different frame may
evict that chosen checkpoint. When arAllFramesReady then runs, the checkpoint it was
computing forward from is gone, and it cannot fall back to an earlier checkpoint because the
corresponding source frames were never requested for this invocation.

This edge case arises under `fmParallelRequests` because concurrent arInitial calls run
alongside the single serialised arAllFramesReady, and that arAllFramesReady includes the
pruning pass.

#### The fix — pin_count on checkpoint pool slots

Each slot in the checkpoint pool carries an integer `pin_count` alongside its frame
pointer (in the `Cnr3CheckpointSlot` struct — see Section B.4). The pin mechanism works
as follows:

**At arInitial time, under mutex:**
- Identify the chosen checkpoint (highest frame number strictly less than N)
- Increment the `pin_count` on that checkpoint pool slot
- Record the chosen checkpoint frame number for the matching arAllFramesReady
  (stored via the `frameData` pointer parameter — see bookkeeping note in Section 2.3)
- Release mutex
- The pinned checkpoint is now protected from eviction for the duration of this invocation

**Pruning pass, under mutex:**
- Before evicting any checkpoint pool slot, check its `pin_count`
- If `pin_count > 0`: skip that slot — it is pinned by an in-flight invocation
- If `pin_count == 0`: apply normal never-evict constraints, then evict if appropriate

**At arAllFramesReady completion — ALL exit paths, under mutex:**

The `pin_count` decrement MUST happen on every exit path from arAllFramesReady without
exception — both the success path and every early-error path. A missed decrement causes
the pinned checkpoint slot to remain permanently un-evictable, silently leaking cached
frame memory.

Exit paths that must all decrement `pin_count`:
- Normal success path: after pruning, before source frame release, before returning
  output frame N to VS
- Early error: failed to allocate destination frame (`newVideoFrame` returns null)
- Early error: failed to build current luma buffer
- Early error: failed to build previous luma buffer
- Early error: any `setFilterError` call within arAllFramesReady
- Early error: `src` frame null or any other precondition failure

The recommended implementation pattern is a cleanup block (analogous to a C++ RAII guard
or a C goto-cleanup label) that is reached on every exit path and performs:
- `{sets mutex}` decrement `pin_count` on the checkpoint slot identified by `frameData`
  `{releases mutex}`
- release all source frames retrieved via `getFrameFilter()` that have not already been
  released
- call `setFilterError` if not already called
- return null to VS

This guarantees `pin_count` reaches zero for every invocation regardless of how
arAllFramesReady exits.

#### Properties of this mechanism

- Under `fmParallelRequests`: at most one arAllFramesReady runs at any time, so at most
  one unpin can be in progress. Multiple concurrent arInitial calls may each pin a
  different checkpoint slot simultaneously — this is safe because each increments its own
  slot's `pin_count` under mutex independently.

- Under `fmParallel` (future): multiple arAllFramesReady calls may each hold a pin on
  different checkpoint slots simultaneously. The `pin_count` integer handles this
  correctly because each invocation increments and decrements its own slot independently
  under mutex. A slot with multiple concurrent pins has `pin_count > 1` and will not be
  evicted until all pinning invocations have completed.

- Frame 0 checkpoint is permanently retained regardless of `pin_count`, but the pin
  mechanism still applies to it for correctness under `fmParallel`.

- A `pin_count` that fails to reach zero (due to a bug in error handling paths) would
  cause a checkpoint to be permanently un-evictable. Debug instrumentation should log
  `pin_count` values at teardown and warn if any are non-zero — a non-zero `pin_count`
  at teardown is always a bug.

- At `cnr3_free()` teardown, after all frame references are released, assert or log
  a warning if any `Cnr3CheckpointSlot` has `pin_count != 0`. This catches missed
  unpin paths that would otherwise be silent memory retention bugs.

---

### B.15 Debug Category Registry

All debug output uses structured tags for grep-ability.

**Format:** `CNR3 [inst=N] [frame=NNN] [thr=X] [TAG] message`

```cpp
// CNR3 debug category registry
// CACHE_HIT        output frame found in cache, computation skipped
// CACHE_MISS       output frame not in cache, computation required
// CACHE_RACE       output frame computed but already filled by another thread, discarded
// CACHE_WRITE      output frame successfully written to cache
// CACHE_PRUNE      frame evicted from cache, with reason
// CACHE_OVERFLOW   cache exceeded nominal capacity, pruning triggered
// CACHE_CAPACITY   cache occupancy at a given moment (frame count and checkpoint count)
// CKPT_PROMOTE     output frame promoted to checkpoint
// CKPT_CHOSEN      checkpoint selected as computation checkpoint for this invocation
// CKPT_EVICT       checkpoint frame evicted, next checkpoint is now frame X
// CKPT_COUNT       current number of checkpoints in cache
// HOLE_COUNT       number of holes identified in range at arAllFramesReady entry
// HOLE_PREFILLED   hole found filled on recheck, skipped
// OUT_RACE         requested output frame computed but already in cache, discarded
// OUT_WRITE        requested output frame written to cache
// SRC_REQUEST      source frame range requested in arInitial
// SRC_RETRIEVE     source frame range retrieved in arAllFramesReady
// SEEK_FORWARD     large forward jump detected, computing forward from checkpoint X
// SEEK_BACKWARD    large backward jump detected, computing forward from checkpoint X
// STARTUP_CHAIN    computing startup forward chain from frame 0
// MUTEX_WAIT       thread waited on mutex (fmParallel only)
// FRAME_DONE       output frame returned to VS
// TEARDOWN_RELEASE releasing N frames from cache at filter teardown
// SUMMARY          per-instance aggregate statistics at filter teardown
```

**Per-instance summary fields printed at teardown:**

```
frames_processed
total_holes_identified
holes_prefilled_by_recheck
holes_computed
holes_discarded_race
output_discarded_race
checkpoint_chosen_count
avg_checkpoint_distance
cache_hit_rate
wasted_computation_rate
```
