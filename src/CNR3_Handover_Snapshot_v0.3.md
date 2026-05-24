# CNR3 Handover Snapshot v0.3

Date: 2026-05-24  
Project: vapoursynth-cnr3  
Purpose: handover checkpoint for continuing development in a new ChatGPT chat with no hidden assumptions.

This document supersedes:

```text
CNR3_Handover_Snapshot_v0.2.md
```

The v0.3 additions are mainly:

```text
- record that the pass-through chroma output was visually confirmed good again
- record that the temporary U/V-from-prev_output proof caused cumulative chroma freeze and was reverted
- record that y4m piping to FFmpeg works using --container y4m
- record the next intended algorithm step and the unresolved questions about vscnr2 fidelity, 16-bit copy/blend, mode defaults, and option 3
```

---

## 1. Project overview

We are developing `vapoursynth-cnr3`, a Windows x64 VapourSynth API4 C++ plugin.

The intended filter is a modern CNR3 chroma stabiliser, conceptually based on the old Cnr2 / vscnr2 family. The target use case is analogue video restoration, especially VHS/VHS-C capture cleanup:

```text
- chroma shimmer
- unstable analogue chroma noise
- stationary rainbows
- mild chroma crawl/flicker
- field-separated interlaced VHS workflows
```

The project is intended to be VapourSynth API4-only, because API3 is being deprecated. The plugin is currently being built and tested against VapourSynth R76.

Licensing position currently being used:

```text
AGPL-3.0-or-later
```

Working licensing assumption:

```text
The relevant Cnr2/vscnr2 licensing chain is compatible because the visible vscnr2 repository is GPL-2.0-or-later, allowing later GPL-family licensing.

If that assumption is later challenged, the fallback is to re-evaluate and possibly use GPL-2.0-or-later.
```

---

## 2. Current repo structure

Current source layout is intentionally simple:

```text
src/
    cnr3_api4_skeleton.cpp

third_party/
    vapoursynth/
        include/
            VapourSynth4.h
            VSHelper4.h
```

The VapourSynth API4 headers are vendored into the repo for the initial build phase, with attribution. This avoids requiring CI to install VapourSynth or Python just to obtain headers.

---

## 3. Current build status

Current build system:

```text
GitHub Actions
Windows x64
MSVC / Visual Studio 2026 runner
single .cpp build
output: cnr3.dll
```

The DLL builds successfully and can be manually copied into the VapourSynth autoload plugin folder.

The plugin currently registers:

```python
core.cnr3.CNR3(...)
```

GitHub Actions has, or is intended to have, a legacy API grep check that rejects obvious API3 names, such as:

```text
VSFrameRef
VSNodeRef
VSFuncRef
cloneFrameRef
cloneFrame(
freeFrameRef
createFilter(
#include "VapourSynth.h"
#include "VSHelper.h"
```

Important lesson learned:

```text
The include-name check must allow:
    #include "VapourSynth4.h"
    #include "VSHelper4.h"

So the workflow regex must check exact include lines, not just the substring "VapourSynth.h".
```

---

## 4. API4-only rules

The code should use only API4 headers:

```cpp
#include "VapourSynth4.h"
#include "VSHelper4.h"
```

Do not use API3-era names such as:

```cpp
VSFrameRef
VSNodeRef
VSFuncRef
cloneFrame()
cloneFrameRef()
freeFrameRef()
createFilter()
```

Important API4 correction already made:

```cpp
vsapi->addFrameRef(frame)
```

is used to retain a frame reference.

Do not use:

```cpp
vsapi->cloneFrame(...)
```

because `cloneFrame` is not a member of API4 `VSAPI`.

Cached frame references should be stored as:

```cpp
const VSFrame *
```

not:

```cpp
VSFrame *
```

because stored previous frames are read-only.

Stored frame references must be released with:

```cpp
vsapi->freeFrame(...)
```

---

## 5. Current public CNR3 parameters

Current public function signature:

```python
core.cnr3.CNR3(
    clip,
    mode="oxx",
    ln=35,
    lm=192,
    un=47,
    um=255,
    vn=47,
    vm=255,
    scdthr=10.0,
    scene_chroma=False,
    debug=False
)
```

Registered signature:

```text
clip:vnode;
mode:data:opt;
ln:int:opt;
lm:int:opt;
un:int:opt;
um:int:opt;
vn:int:opt;
vm:int:opt;
scdthr:float:opt;
scene_chroma:int:opt;
debug:int:opt;
```

Public threshold parameters intentionally remain in historical 8-bit Cnr2/vscnr2 units, even for 10/12/16-bit clips.

Internally, thresholds are scaled to the actual bit depth.

Example:

```text
8-bit:
    ln=35 -> 35

16-bit:
    ln=35 -> 8995
```

---

## 6. Current accepted input formats

Current validation accepts:

```text
- YUV only
- integer sample type only
- 8-bit to 16-bit
- 3-plane YUV
- constant format
- constant dimensions
- chroma subsampling W/H in range 0..1
```

Current validation rejects:

```text
- RGB
- GRAY
- float
- variable format
- variable dimensions
- unsupported chroma subsampling
```

Float is deliberately rejected for now. The first algorithm target is integer planar YUV because that matches old Cnr2/vscnr2-style processing and VHS restoration workflows. Float can be reconsidered later if there is a clear need.

---

## 7. Diagnostic-output rule

Hard rule:

```text
CNR3 must never write diagnostic/status/debug output to stdout.
```

Reason:

```text
vspipe may use stdout for raw video or y4m video output.
```

Current convention:

```text
- debug/status messages go to stderr
- VapourSynth user-facing errors use mapSetError() or setFilterError()
```

There is a helper:

```cpp
static void cnr3_debug_printf(
    bool debug_enabled,
    const char *format,
    ...
)
```

It is a small printf-style varargs wrapper around `std::vfprintf(stderr, ...)`, and it flushes stderr.

The `...` syntax is intentional C/C++ varargs syntax, documented in the code.

Important practical note:

```text
When piping vspipe to FFmpeg, debug=False is best for normal encode tests, even though stderr should not corrupt stdout.

For clean y4m piping with the current setup, use:
    vspipe --container y4m script.vpy -
```

---

## 8. Current lookup-table behaviour

The plugin currently builds lookup tables for Y/U/V guard logic.

Tables are:

```text
index:
    absolute sample difference, 0..sample_peak

value:
    integer weight 0..256
```

Current default mode:

```text
mode="oxx"
```

Meaning currently implemented:

```text
mode[0] = 'o' -> Y/luma guard enabled
mode[1] = 'x' -> U guard disabled
mode[2] = 'x' -> V guard disabled
```

Current table logic:

```text
if guard disabled:
    every table entry = 256

if guard enabled:
    abs(diff) <= threshold_low:
        weight = 256

    abs(diff) >= threshold_high:
        weight = 0

    between threshold_low and threshold_high:
        raised-cosine fade from 256 down to 0
```

Example confirmed for 8-bit default mode:

```text
Y[0]=256, Y[35]=256, Y[113]=129, Y[192]=0
U[0]=256, U[47]=256, U[151]=256, U[255]=256
V[0]=256, V[47]=256, V[151]=256, V[255]=256
```

Example confirmed for 16-bit default mode:

```text
Y[0]=256, Y[8995]=256, Y[29169]=128, Y[49344]=0
U[0]=256, U[12079]=256, U[38807]=256, U[65535]=256
V[0]=256, V[12079]=256, V[38807]=256, V[65535]=256
```

### Open question about default mode

A concern was raised at the end of v0.3:

```text
With default mode="oxx", U and V tables are disabled. Is that an unfortunate default since the purpose of CNR is to denoise/stabilise chroma?
```

This must be checked against vscnr2/Cnr2 semantics before finalising the algorithm.

Important distinction:

```text
In our current scaffold, disabled guard table currently means table entries are 256.

Depending on the blend formula, weight=256 could mean "use previous output strongly" or could mean "allowed/full strength". Therefore the meaning of mode="oxx" cannot be judged in isolation without checking the original algorithm's formula.
```

Another open question:

```text
What exactly does "ooo" mean in vscnr2/Cnr2 semantics?

Current scaffold interpretation:
    ooo = Y guard enabled, U guard enabled, V guard enabled

But before algorithm implementation, verify this against vscnr2 source behaviour and docs.
```

---

## 9. Current frame-processing status

The plugin currently does not yet perform real chroma filtering.

Current known-good frame path after v0.3:

```text
source frame
    -> allocate new destination frame
    -> copy Y from current source frame
    -> route U plane through chroma-plane processing helper
    -> route V plane through chroma-plane processing helper
    -> store output frame as previous output reference
    -> return destination frame
```

The chroma-plane helper is currently pass-through:

```text
U/V are copied from current source frame, not from prev_output.
```

This is important because an earlier temporary recursive-read proof was tried:

```text
frame 0:
    copy U/V from source

frame N > 0:
    copy U/V from prev_output
```

That test proved `prev_output` could be read safely, but it did not create a simple one-frame chroma lag. Because `prev_output` is the previous filtered output, it recursively froze chroma at frame 0:

```text
output[1].UV = output[0].UV = source[0].UV
output[2].UV = output[1].UV = source[0].UV
output[3].UV = output[2].UV = source[0].UV
...
```

Visual result:

```text
The right-hand filtered side in a StackHorizontal comparison had severely wrong chroma:
- green tree foliage faded to brown
- background leaves drifted toward grey/blue
```

That was explained as cumulative chroma freeze, not as an interlacing issue or output metadata issue.

The helper was therefore reverted to source-copy pass-through, and the chroma looked good again.

---

## 10. Named processing functions

The frame-level function currently acts as the stable insertion point for the real algorithm:

```cpp
static bool process_cnr3_frame_passthrough_for_now(
    const Cnr3Data *d,
    int frame_number,
    const VSFrame *src,
    VSFrame *dst,
    VSFrameContext *frameCtx,
    const VSAPI *vsapi
)
```

Current behaviour:

```text
- verifies d/src/dst/frame_number
- verifies prev_output exists for recursive frames
- frame 0 uses initial-copy path
- frames N > 0 use recursive previous-output path
- copies Y from source
- sends U and V through the chroma-plane helper
```

The chroma helper currently acts as the stable insertion point for chroma processing:

```cpp
static bool process_cnr3_chroma_plane(
    const Cnr3Data *d,
    int frame_number,
    int plane,
    const VSFrame *src,
    const VSFrame *prev_output,
    VSFrame *dst,
    const VSAPI *vsapi
)
```

Note:

```text
In v0.2 the helper may still have been named:
    process_cnr3_chroma_plane_passthrough_for_now

At the very end before handover, we planned a low-risk rename to:
    process_cnr3_chroma_plane

The user indicated skipping a build on the rename-only bit.
```

Current intended behaviour of chroma helper:

```text
- validates d/src/dst/vsapi
- validates plane is 1 or 2
- keeps the recursive precondition check for prev_output on frame N > 0
- copies the requested chroma plane from the current source frame
```

If the rename has not yet been committed in the code, the next chat should check the actual current source and either keep the old name temporarily or complete the rename with before/after blocks.

---

## 11. Cache manager status

A minimal cache manager exists:

```cpp
struct Cnr3CacheManager {
    const VSFrame *prev_output = nullptr;
    int next_needed = 0;
};
```

It lives inside:

```cpp
struct Cnr3Data {
    ...
    Cnr3CacheManager cache;
    ...
};
```

That is important: each `core.cnr3.CNR3(...)` call gets its own `Cnr3Data`, and therefore its own cache.

This is critical for field-separated interlaced workflows:

```python
first_denoised  = core.cnr3.CNR3(first, debug=True)
second_denoised = core.cnr3.CNR3(second, debug=True)
```

Those are two separate clips / two separate filter instances / two separate caches. Mixing same-numbered frames from those two streams into one shared cache would be catastrophic.

Current invariant:

```text
cache.prev_output holds a read-only reference to output[next_needed - 1],
or nullptr before frame 0 has been processed.
```

Current strict policy:

```text
Only frame n == cache.next_needed is accepted.
```

After processing output frame `N`:

```cpp
cache.prev_output = vsapi->addFrameRef(output_frame);
cache.next_needed = N + 1;
```

When replacing or clearing the cached frame:

```cpp
vsapi->freeFrame(cache.prev_output);
```

---

## 12. Instance ID status

An instance ID has been added and tested.

Purpose:

```text
Make logs distinguish two or more simultaneous CNR3 instances.
```

Implementation concept:

```cpp
#include <atomic>

static std::atomic<int> g_cnr3_next_instance_id{1};

struct Cnr3Data {
    ...
    int instance_id = 0;
    ...
};
```

In `cnr3_create()`:

```cpp
Cnr3Data local;
local.instance_id = g_cnr3_next_instance_id.fetch_add(1);
```

Debug output now looks like:

```text
CNR3 debug: instance=1, ...
CNR3 debug: instance=2, ...
```

This was tested and works.

---

## 13. Filter mode and scheduling findings

Current filter mode:

```cpp
fmUnordered
```

Important finding:

```text
fmUnordered is not enough to guarantee ascending frame numbers.
```

Strict streaming works with simple cases and with forced request depth:

```bat
vspipe -r 1 ...
```

But without `-r 1`, VapourSynth may request frames ahead of the cache.

Observed failure without `-r 1`:

```text
processed frame 0
processed frame 1
requested frame 3 while next_needed=2
processed frame 2
later the earlier frame 3 request had already failed
```

Concrete observed form:

```text
CNR3 debug: instance=2, out-of-order frame request: requested=3, next_needed=2, gap=1
```

This proves the earlier failure was not due to cache corruption. It was due to normal VapourSynth readahead / scheduling.

---

## 14. Policies discussed

We discussed several possible policies for a recursive filter where:

```text
output[0] = source[0]
output[N] = f(source[N], output[N-1])
```

### Policy A: strict streaming mode

Accept only:

```text
0, 1, 2, 3, ...
```

Implementation:

```text
if requested_frame != next_needed:
    error
```

Pros:

```text
- simplest
- deterministic
- easiest to develop against
- faithful to full recursive behaviour
- minimal memory
```

Cons:

```text
- fails under normal VapourSynth readahead
- requires vspipe -r 1 for real scripts
- unsuitable for random seeking / preview
```

This is the policy currently implemented.

### Policy B: strict streaming plus user workaround

Same as Policy A internally, but document that the user must run:

```bat
vspipe -r 1 ...
```

Pros:

```text
- no extra code now
- enough for initial algorithm development
```

Cons:

```text
- not acceptable as final user experience
- not robust for normal VapourSynth usage
```

This is the current practical development policy.

### Policy C: future cache/reorder/checkpoint manager

Future fuller policy that may support:

```text
- small reorder window
- recent output cache
- checkpoint cache
- recovery state
- seek modes
- maybe bounded lookback
```

Potential behaviours:

```text
1. If frame N arrives while N-1 is not yet complete:
       delay/queue/retry/reorder rather than immediately error.

2. If frame N is requested after a seek:
       recompute from a checkpoint or from frame 0.

3. If bounded lookback is configured:
       compute using only the last K frames.
```

Pros:

```text
- normal vspipe scheduling can work
- preview/seeking may be possible
- avoids requiring -r 1 forever
```

Cons:

```text
- more complex
- requires careful frame reference ownership
- may require memory management, eviction, and deterministic recovery rules
- should not be implemented before the actual chroma algorithm is working
```

### Policy D: bounded recursive approximation

Instead of true infinite recursion:

```text
output[N] depends on output[N-1] back to frame 0
```

Use a finite lookback:

```text
output[N] is computed by replaying only source[N-K..N]
```

Pros:

```text
- parallel-friendly
- seek-friendly
```

Cons:

```text
- not exactly faithful
- naive implementation is very slow
- needs visual testing to determine acceptable K
- 10 seconds at PAL = 250 frames, which is expensive if recomputed for every output frame
```

### Policy E: checkpointed bounded/recovery mode

Store periodic filtered checkpoints:

```text
checkpoint at frame 0
checkpoint at frame 100
checkpoint at frame 200
...
```

Then for frame 237:

```text
start from checkpoint 200 and replay 201..237
```

Pros:

```text
- more seek-safe
- avoids recomputing from frame 0 every time
```

Cons:

```text
- checkpoint memory/eviction policy needed
- output determinism must be maintained
- more complex than needed at this stage
```

---

## 15. Which policy we settled on

Current agreed decision:

```text
Use Policy A / strict streaming as the current implementation.
Use -r 1 for real-script testing during development.
Do not implement the full cache manager yet.
Do not bake in anything that prevents a future cache manager.
```

The code was deliberately shaped to make Policy C possible later:

```text
- cache state lives in Cnr3CacheManager
- Cnr3Data owns one cache per filter instance
- prev_output is reference-counted correctly
- instance_id exists for debugging multiple streams
- processing logic is being moved into named functions
```

Current plan:

```text
1. Keep strict streaming Policy A for now.
2. Develop and verify the actual recursive chroma algorithm under -r 1.
3. Once the algorithm is visually useful, revisit Policy C.
4. The first future Policy C feature is likely a small reorder/pending window,
   because normal vspipe scheduling has already shown small forward gaps,
   often gap=1.
```

The normal-depth test showed the first practical issue is not random seeking but scheduler readahead:

```text
requested=3, next_needed=2, gap=1
```

So the future cache manager probably needs at least a small pending/reorder capability before it needs full checkpoint/recompute support.

---

## 16. Current interlaced workflow used for testing

The real test script does this:

```text
1. Open source with BestSource.
2. Trim to a short test range when debugging.
3. Apply SetFrameProps based on earlier precheck.
4. Split interlaced clip into same-parity field streams.
5. Run CNR3 independently on first and second field streams.
6. Reweave fields.
7. Stack original and processed clips horizontally for visual comparison.
```

Example:

```python
clip = core.std.Trim(clip, first=0, last=19)

clip = core.std.SetFrameProps(
    clip,
    _FieldBased=1,
    _Matrix=5,
    _Range=0,
    _Primaries=5,
    _Transfer=5,
    _ChromaLocation=0,
    _SARNum=16,
    _SARDen=15,
    Rotation=0,
    FlipHorizontal=0,
    FlipVertical=0,
)

tag, first, second = split_fields(clip)

if tag == "P":
    first_denoised  = core.cnr3.CNR3(first, debug=False)
    second_denoised = None
else:
    first_denoised  = core.cnr3.CNR3(first, debug=False)
    second_denoised = core.cnr3.CNR3(second, debug=False)

clip_denoised = reweave_fields(tag, first_denoised, second_denoised)

stacked_comparison = core.std.StackHorizontal([clip, clip_denoised])
stacked_comparison.set_output()
```

`Trim` note:

```text
last is inclusive, so first=0, last=19 gives 20 frames.
```

---

## 17. vspipe to FFmpeg notes

For y4m piping to FFmpeg with the currently installed vspipe, the working form is:

```bat
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" -r 1 --container y4m "D:\TEST\Vapoursynth_x64_R76\test_cnr3.vpy" - | "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: ...
```

Important lessons:

```text
- Using NUL as the vspipe output target sends video to NUL, so FFmpeg receives no stream.
- Using "-" without y4m/container option gives raw output, not y4m.
- FFmpeg with "-f yuv4mpegpipe" expects a y4m header.
- The working command used "--container y4m".
```

Confirmed successful FFmpeg input:

```text
Input #0, yuv4mpegpipe, from 'pipe:'
Stream #0:0: Video: rawvideo (I420), yuv420p(progressive), 1440x576, 25 fps
```

Confirmed successful encode:

```text
Output 20 frames
20 frames encoded
exit code 0
```

---

## 18. Test results so far

### Simple BlankClip test

Confirmed working:

```python
clip = core.std.BlankClip(format=vs.YUV420P8, width=640, height=480, length=10)
clip = core.cnr3.CNR3(clip, debug=True)
clip.set_output()
```

and:

```python
clip = core.std.BlankClip(format=vs.YUV422P16, width=640, height=480, length=10)
clip = core.cnr3.CNR3(clip, debug=True)
clip.set_output()
```

Confirmed:

```text
- plugin loads
- CNR3 registers
- format validation works
- lookup tables build
- destination frame allocation works
- pass-through copy works
- strict state processes frames 0..9
```

### Real field-split script with `-r 1`

Confirmed working:

```bat
vspipe -r 1 ...
```

Observed clean instance-separated output:

```text
instance=1, requested=0, next_needed=0
instance=2, requested=0, next_needed=0
...
instance=1, requested=19, next_needed=19
instance=2, requested=19, next_needed=19
Output 20 frames ...
```

### Real field-split script without `-r 1`

Fails due to scheduler readahead:

```text
instance=2, requested=3, next_needed=2, gap=1
```

This is expected under strict Policy A.

### Temporary prev_output chroma-copy proof

Tried:

```text
frame 0:
    copy U/V from source

frame N > 0:
    copy U/V from prev_output
```

Result:

```text
- code path did not crash
- prev_output was readable
- but visual chroma was badly wrong because chroma recursively froze at frame 0
```

Conclusion:

```text
Do not use that as a simple one-frame lag proof.
It was useful only as a crude proof that prev_output can be read.
It was reverted.
```

### Current visual state after revert

After reverting chroma helper back to copying U/V from current source, the right-hand filtered side in the StackHorizontal comparison visually matches the left-hand source side again. Chroma looks good.

---

## 19. Current debug noise level

High-frequency lifecycle debug is mostly commented out:

```cpp
cnr3_debug_print_cache_state(... "arInitial/request source frame" ...)
cnr3_debug_print_cache_state(... "arAllFramesReady/entry" ...)
in-order frame accepted
```

Current retained debug while `debug=True`:

```text
- creation/format debug
- lookup-table debug
- frame 0 initial-copy path
- frame N recursive previous-output path
- processed-frame debug
- out-of-order debug
```

Out-of-order debug should remain because it is the key failure diagnostic.

The per-frame path debug can be reduced later once the real chroma algorithm begins producing visual output.

---

## 20. Outstanding questions at v0.3 handover

These questions were raised immediately before this handover and should be answered before implementing the first real blend:

### Q1. Will the final blending follow the vscnr2 algorithm?

Intended answer:

```text
Yes, the final goal is faithful vscnr2/Cnr2 behaviour, not an invented blend, unless we deliberately choose to deviate later.
```

But before coding the real blend, inspect the vscnr2 algorithm again and map its exact operations into the current API4 scaffold.

Implementation guidance:

```text
Do not treat the simple formula below as final:
    dst = (src * (256 - weight) + prev * weight + 128) >> 8

That may be useful as a proof skeleton, but the real target should be vscnr2/Cnr2-compatible behaviour.
```

### Q2. Can we do 16-bit copy at this stage too, and blend as the next stage?

Current answer:

```text
Yes. The current copy helpers already handle bytes_per_sample and should copy 8/10/12/16-bit integer frames correctly as bytes.

A good next-stage plan is:
    - keep 8/10/12/16-bit copy working
    - build the per-pixel loop safely
    - initially keep the loop pass-through for all bit depths
    - then implement blend for 8-bit first
    - then generalise the blend to 10/12/16-bit using uint16_t sample access and scaled thresholds/tables
```

Do not break 16-bit pass-through while experimenting with 8-bit blend.

### Q3. Is default mode="oxx" unfortunate for a chroma denoiser?

Concern:

```text
If U and V tables are disabled by default, does that undermine chroma denoising?
```

Current answer:

```text
Do not decide from the scaffold alone. Need to check vscnr2/Cnr2 semantics.

In the current table builder, disabled U/V means their table entries are all 256. Depending on how the final blend interprets 256, that may mean "full permission to blend", not "no chroma processing".
```

Therefore:

```text
Before changing defaults, verify original vscnr2 default mode and how the mode characters influence the actual chroma decision.
```

### Q4. What does mode="ooo" mean?

Current scaffold interpretation:

```text
mode[0] = 'o' -> Y/luma guard enabled
mode[1] = 'o' -> U guard enabled
mode[2] = 'o' -> V guard enabled
```

But this should be checked against vscnr2 source/docs.

Possible meaning in practice:

```text
ooo could mean all three difference guards participate in deciding blend strength.
oxx could mean only luma difference guards the chroma stabilisation.
```

Do not assume final semantic details until source is rechecked.

### Q5. Is Option 3 the same as mode="oxx"?

No.

Option 3 was:

```text
Add the per-pixel loop structure now but force weight=0 so output remains pass-through.
```

That is an implementation scaffolding choice, independent of the user parameter `mode`.

`mode="oxx"` is a user-facing algorithm parameter inherited from Cnr2/vscnr2-style behaviour.

They are not the same thing.

For a safe next implementation step:

```text
Option 3 can be used to build and test the loop structure while preserving pass-through output for all modes.
```

Only after that should the real mode-dependent weighting be connected.

---

## 21. Next intended coding step

The chat was becoming slow at this point. The next chat should begin by answering the outstanding questions in section 20, especially by checking the actual vscnr2 blend/mode semantics.

Recommended next coding sequence:

```text
1. Confirm current source has the chroma helper renamed to process_cnr3_chroma_plane.
   If not, either complete the rename or continue with the old name temporarily.

2. Do not implement full cache manager yet.

3. Add a per-pixel chroma loop scaffold that still outputs pass-through:
       for each chroma sample:
           read src sample
           optionally read prev_output sample if frame_number > 0
           write src sample unchanged

4. Make the loop handle both bytes_per_sample cases:
       1 byte  -> uint8_t samples
       2 bytes -> uint16_t samples

5. Build/test:
       - YUV420P8
       - YUV422P16 or another 16-bit clip
       - field-split real script with -r 1
       - stacked comparison should still visually match

6. Only after that, implement first blend.
```

Reason for this sequence:

```text
The user specifically asked whether 16-bit copy can be preserved at this stage.
Therefore the next loop scaffold should not become 8-bit-only pass-through.
It should preserve 16-bit pass-through before any blend is enabled.
```

---

## 22. Important future algorithm considerations

Real CNR3 will need to handle chroma subsampling carefully.

For YUV420/YUV422/YUV440:

```text
Y plane resolution differs from U/V plane resolution.
```

The lookup table currently indexes sample differences in the same bit depth, but actual chroma processing will need to map luma guard checks to chroma sample coordinates.

Likely approach:

```text
For each chroma sample at coordinates (cx, cy):
    map to corresponding luma coordinates based on subsampling
    use source luma/current-vs-previous luma differences as guard input
    combine Y/U/V table weights according to Cnr2/vscnr2 behaviour
```

Do not assume 4:4:4 only.

Also remember current intended default:

```text
Y/luma is not denoised/modified.
CNR3 is chroma stabilisation.
```

The likely first real algorithm should:

```text
- copy Y unchanged
- compute U/V output from current source chroma and previous output chroma
- use lookup-table weights to control blending
```

---

## 23. Developer style requirements from the user

When proposing code edits, follow this rule:

```text
Show prior code block and replacement code block.
Include enough lines before and after so the user can find the block.
Avoid unnecessary changes.
Keep comments maintainable and future-oriented.
Use ASCII only in code/comments.
Explain what changes and why.
```

The user values comments that help future maintainers understand:

```text
- why a choice was made
- what invariant is being maintained
- how frame references are owned/freed
- what is temporary scaffolding versus final design
```

---

## 24. Snapshot summary for v0.3

Current known-good state:

```text
- API4-only plugin
- AGPL-3.0-or-later
- GitHub Actions build works
- DLL loads in VapourSynth R76
- CNR3 registers as core.cnr3.CNR3()
- parameter parsing works
- format validation works
- 8-bit to bit-depth scaling works
- lookup tables work
- destination frame allocation works
- minimal Cnr3CacheManager works
- instance_id debug works
- two field-stream CNR3 instances have independent caches
- strict streaming works under vspipe -r 1
- default scheduling fails due to forward readahead, as expected
- vspipe --container y4m -> FFmpeg pipe works
- frame-level processing is split into Y copy plus U/V helper calls
- U/V helper currently copies from current source
- temporary U/V-from-prev_output proof was tried and reverted
- chroma visually matches again after reverting to source-copy pass-through
- outstanding design questions about vscnr2 blend/mode semantics are recorded in section 20
```

The next chat should not jump into full cache-manager implementation unless deliberately asked. The better next step is to answer the vscnr2/mode/blend questions, then add a bit-depth-safe per-pixel chroma loop scaffold that still outputs pass-through.
