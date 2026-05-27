# CNR3 Handover Snapshot v0.6 - Scene Change, Visual Studio 2026, GitHub Actions

Date: 2026-05-27
Project: vapoursynth-cnr3
Repository: https://github.com/hydra3333/vapoursynth-cnr3
Purpose: detailed handover checkpoint for continuing development in a new ChatGPT chat with no hidden assumptions.

This document supersedes the earlier handover snapshot:

```text
CNR3_Handover_Snapshot_v0.5_expanded.md
```

This v0.6 snapshot is intentionally detailed. Do not compress it aggressively in a future handover unless the relevant detail has been superseded by code or by a later explicit decision.

The major additions since v0.5 are:

```text
- Visual Studio 2026 solution/project setup now works locally.
- .sln/.vcxproj files are intended to be committed.
- Release x64 local build works.
- GitHub Actions build has been aligned with VS 2026 / MSVC 14.51 / C++20.
- GitHub Actions DLL build has been verified as x64 and exporting VapourSynthPluginInit2.
- Real recursive vscnr2-style chroma blend is connected.
- Luma guard buffers are now built once per frame and shared by U and V.
- vscnr2-style scene-change detection is implemented.
- scene_chroma is implemented in the scene-change metric.
- debug output has been reduced and made more useful.
- synthetic 8-bit and 16-bit scene-change tests pass.
- realclip scdthr sensitivity was investigated and understood.
```

---

## 1. Project overview

CNR3 is a Windows x64 VapourSynth API4 C++ plugin. It is a modern redevelopment of the Cnr2 / vscnr2 family of recursive temporal chroma stabilisers.

Target use case:

```text
- analogue video restoration;
- VHS / VHS-C capture cleanup;
- chroma shimmer;
- unstable analogue chroma noise;
- stationary rainbows;
- mild chroma crawl/flicker;
- field-separated interlaced workflows.
```

The immediate goal is not to invent a new denoiser. The goal is to follow AviSynth-vsCnr2/Cnr2 behaviour as closely as is good and practicable, while adapting safely to VapourSynth API4 and modern build tooling.

Current licensing position:

```text
SPDX-License-Identifier: AGPL-3.0-or-later
```

Working licensing assumption:

```text
The visible more recent AviSynth-vsCnr2 code is GPL-family compatible for this redevelopment path. If that assumption is challenged later, re-evaluate and possibly use GPL-2.0-or-later instead.
```

---

## 2. Current repository and source layout

Current important paths:

```text
.github/workflows/build-windows-x64.yml
src/vapoursynth-Cnr3.cpp
third_party/vapoursynth/include/VapourSynth4.h
third_party/vapoursynth/include/VSHelper4.h
vs/cnr3/...
```

The current implementation is still mostly a single C++ source file:

```text
src/vapoursynth-Cnr3.cpp
```

Visual Studio solution/project files now exist and should be committed:

```text
vs/cnr3/cnr3.sln
vs/cnr3/cnr3.vcxproj
```

Rationale for committing the VS files:

```text
- local VS build is now useful;
- the user wants repeatable local project setup;
- GitHub Actions still performs a direct command-line cl build;
- both build paths should remain aligned.
```

Do not commit Visual Studio build rubbish:

```text
.vs/
Debug/
Release/
x64/
*.obj
*.pdb
*.ilk
*.exp
*.lib when generated as build output
```

The vendored API4 headers remain in:

```text
third_party/vapoursynth/include/
```

This keeps CI independent of a Python/VapourSynth installation just to get headers.

---

## 3. API4-only policy

CNR3 must remain API4-only.

Correct includes:

```cpp
#include "VapourSynth4.h"
#include "VSHelper4.h"
```

Do not use API3-era names:

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

Important API4 frame-reference rule:

```cpp
vsapi->addFrameRef(frame)
```

is used to retain a frame reference.

Do not use:

```cpp
vsapi->cloneFrame(...)
```

Cached previous frames should be stored as:

```cpp
const VSFrame *
```

not mutable `VSFrame *`, because previous output frames are read-only temporal history.

Cached frames must be released with:

```cpp
vsapi->freeFrame(...)
```

---

## 4. Current public CNR3 call

Current intended Python call:

```python
clip2 = core.cnr3.CNR3(
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
    blend=True,
    debug=False,
)
```

Check actual registration in source before relying on the public `blend` option. During development, a `blend` maintenance option has existed and has defaulted to true. It was intentionally retained for testing/reuse, rather than removed, but release behaviour is expected to have Cnr2-style blending enabled.

Historical public parameters use 8-bit Cnr2/vscnr2-compatible units. For clips above 8-bit, CNR3 scales them internally.

Examples:

```text
8-bit:
    ln=35 -> 35
    lm=192 -> 192
    un=47 -> 47
    um=255 -> 255

16-bit:
    ln=35 -> 8995
    lm=192 -> 49344
    un=47 -> 12079
    um=255 -> 65535
```

The scaling currently uses rounded proportional scaling:

```text
(value_8bit * peak + 127) / 255
```

This was deliberately kept because it is mathematically sensible. It is exactly equivalent at 16-bit because 65535 / 255 = 257. It may differ slightly from an integer-multiplier interpretation for 10/12/14-bit, but that was accepted as preferable unless later compatibility testing proves otherwise.

---

## 5. Accepted input formats

Current validation accepts:

```text
- constant-format video only;
- constant-dimension video only;
- YUV only;
- integer sample type only;
- 8-bit through 16-bit;
- exactly 3 planes;
- chroma subsampling W/H in range 0..1.
```

This covers:

```text
YUV 4:2:0
YUV 4:2:2
YUV 4:4:0
YUV 4:4:4
```

Float is deliberately rejected for now.

The user specifically asked whether the luma/chroma handling covers the expected incoming range. The current implementation derives chroma dimensions from the actual VapourSynth plane dimensions and explicitly checks U and V match before sharing luma buffers, so it is robust for the accepted planar YUV subsampling cases.

---

## 6. Diagnostic output rule

Hard rule:

```text
CNR3 must never write plugin diagnostics to stdout.
```

Reason:

```text
vspipe may use stdout for video output.
```

Current convention:

```text
- debug/status messages go to stderr;
- VapourSynth user-facing errors use mapSetError() or setFilterError().
```

Helper:

```cpp
static void cnr3_debug_printf(
    bool debug_enabled,
    const char *format,
    ...
)
```

It is a printf-style varargs wrapper around `std::vfprintf(stderr, ...)` and flushes stderr.

Debug output has now been reduced. Normal per-frame/per-plane response and blend statistics are commented out, not deleted. They can be re-enabled if tuning response tables or blend strength.

Current active debug should generally include:

```text
- startup configuration line;
- table sample line;
- scene-change stats only when scene_change=1 or diff_total >= 80% of diff_max;
- explicit scene-change action line;
- strict-streaming/cache error diagnostics.
```

---

## 7. Recursive processing and VapourSynth scheduling

Cnr2/vscnr2 is inherently temporal and recursive:

```text
output[N] depends on source[N] and output[N - 1]
```

It does not depend merely on:

```text
source[N - 1]
```

This makes the algorithm naturally serial.

Older VapourSynth-era recursive filters could sometimes rely on compatibility-style scheduling assumptions. In particular, `fmFrameState` meant only one thread would call a filter's getframe function at a time and only one frame would be processed at a time.

Current VapourSynth API4 documentation says `fmFrameState` is compatibility-only and must not be used in new filters.

CNR3 therefore uses:

```text
fmUnordered
```

Important nuance:

```text
fmUnordered prevents concurrent entry into this filter's getframe function, but it does not guarantee display-order frame requests.
```

VapourSynth may still request frames in a non-serial order, which is inconvenient for a recursive `output[N - 1]` algorithm.

Current development policy:

```text
strict streaming only
```

The current cache manager accepts only:

```text
frame n == cache.next_needed
```

For testing, use:

```bat
vspipe -r 1 --container y4m script.vpy - | ffmpeg ...
```

This is a deliberate correctness-first API4 bridge. A future cache manager will relax strict ordering by adding reorder, seek, checkpoint, or recomputation support.

---

## 8. Cnr3CacheManager current state

Current minimal cache manager:

```cpp
struct Cnr3CacheManager {
    const VSFrame *prev_output = nullptr;
    int next_needed = 0;
};
```

It lives inside `Cnr3Data`, so each CNR3 filter instance has its own cache.

This is essential for field-separated interlaced workflows:

```python
first_denoised  = core.cnr3.CNR3(first, debug=True)
second_denoised = core.cnr3.CNR3(second, debug=True)
```

Those are two independent CNR3 instances with separate recursive histories.

Current invariant:

```text
cache.prev_output holds a read-only reference to output[next_needed - 1], or nullptr before frame 0 has been processed.
```

After processing frame N:

```text
cache.prev_output = addFrameRef(output_frame)
cache.next_needed = N + 1
```

When replacing or clearing cached previous output:

```text
freeFrame(cache.prev_output)
```

Observed scheduling issue without `-r 1`:

```text
VapourSynth can request frame 3 while next_needed is still 2.
```

So the current strict mode intentionally errors on out-of-order requests until a future cache manager exists.

---

## 9. Mode string semantics

Current agreed mode interpretation follows the later vscnr2 model:

```text
mode has three characters:
    mode[0] controls Y/luma response
    mode[1] controls U response
    mode[2] controls V response
```

Important:

```text
'x' does not mean disabled.
'o' does not mean enabled.
```

Instead:

```text
'x' = narrow response curve
'o' = wide response curve
```

Narrow response:

```text
The response falls away sooner as current-vs-previous differences increase.
It is more conservative and reduces risk of chroma lag or smearing.
```

Wide response:

```text
The response remains non-zero across a wider difference range.
It allows stronger stabilisation but increases risk of lag/smearing/ghosting around real motion.
```

Historical default:

```text
mode="oxx"
```

Current interpretation:

```text
Y uses wide response, so luma structure does not block chroma stabilisation too eagerly.
U and V use narrow responses, so true chroma changes are treated more conservatively.
All three planes still participate in the blend decision.
```

---

## 10. Lookup tables

CNR3 builds signed-difference response tables for Y, U, and V.

Table indexing:

```text
table[signed_diff + table_offset]
```

where:

```text
signed_diff = current_sample - previous_sample
```

Table value range:

```text
0..sample_peak
```

The table is signed because the vscnr2-style formula uses signed current-vs-previous differences when indexing Y/U/V response tables. For cosine response curves the result is symmetric, but signed indexing keeps the lookup path aligned with the blend formula.

Default 8-bit samples observed:

```text
Y[0]=192, Y[17]=166, Y[35]=0, Y[255]=0
U[0]=254, U[23]=131, U[47]=0, U[255]=0
V[0]=254, V[23]=131, V[47]=0, V[255]=0
```

Default 16-bit samples observed:

```text
Y[0]=49344, Y[4497]=42120, Y[8995]=0, Y[65535]=0
U[0]=65534, U[6039]=32771, U[12079]=0, U[65535]=0
V[0]=65534, V[6039]=32771, V[12079]=0, V[65535]=0
```

Important historical shape detail:

```text
An odd maximum strength such as 255 gives a peak table value of 254 because the vscnr2-style calculation uses integer division by 2 before applying the cosine response.
```

This is intentionally preserved.

---

## 11. Current frame-processing structure

Current frame-level path:

```text
process_cnr3_frame(d, frame_number, src, dst, frameCtx, vsapi)
|
|- validate d/src/dst/frame_number
|- get prev_output from d->cache.prev_output
|- frame 0: initial source-copy path
|- frame N > 0: require prev_output
|- calculate bytes_per_sample
|- copy Y plane unchanged from current source
|- derive chroma geometry from U plane
|- verify V plane has matching dimensions
|- build current downsampled-luma buffer once
|- build previous-output downsampled-luma buffer once for frame N > 0
|- run vscnr2-style scene-change detection for frame N > 0
|- if scene_change:
|     copy current source U/V and skip recursive chroma blend
|- otherwise:
|     process U through recursive chroma blend
|     process V through recursive chroma blend
```

Y is always copied unchanged because CNR3 is a chroma stabiliser.

U/V normally use recursive blend unless scene-change detection fires.

---

## 12. Luma-buffer sharing optimisation

Before the optimisation:

```text
U plane built current_luma and previous_luma.
V plane built current_luma and previous_luma again.
```

After the optimisation:

```text
process_cnr3_frame() builds current_luma and previous_luma once per frame.
U and V share the same luma buffers.
```

Defensive rule agreed and implemented:

```text
- derive shared chroma dimensions from plane 1 (U);
- verify plane 2 (V) has the same width and height;
- if U/V dimensions differ unexpectedly, fail clearly rather than guessing.
```

The optimisation was tested and preserved behaviour:

```text
8-bit static test: same expected blend stats before debug reduction.
16-bit static test: same expected high-bit-depth behaviour.
Synthetic luma-constant/chroma-change test: U/V distinction remained correct.
Real clip: shared-buffer debug path was exercised before later debug reduction.
```

---

## 13. Recursive vscnr2-style chroma blend

The real blend is now connected.

For each chroma sample:

```text
current_chroma = source[N].U_or_V[x, y]
previous_chroma = output[N - 1].U_or_V[x, y]
current_luma = downsampled source[N].Y at chroma resolution
previous_luma = downsampled output[N - 1].Y at chroma resolution
```

Because Y is copied unchanged, previous-output luma is equivalent to previous-source luma, but using `prev_output` keeps the structure aligned with the recursive model.

Response values:

```text
y_response = table_y[current_luma - previous_luma + offset]
chroma_response = table_u_or_v[current_chroma - previous_chroma + offset]
```

Combined blend weight:

```text
weight = y_response * chroma_response
```

vscnr2-style blend formula:

```text
shift2 = bits_per_sample * 2
shift  = 1 << shift2
shift1 = shift / 2

dst = (
        weight * previous_filtered_chroma
      + (shift - weight) * current_source_chroma
      + shift1
      ) >> shift2
```

Frame 0 writes current-source chroma because there is no previous filtered output.

`blend=false` still exists as a maintenance/testing mode and forces current-source chroma output while keeping diagnostic read/table paths active.

The default/release behaviour should be `blend=true`.

---

## 14. Scene-change detection

Scene-change detection is now implemented using the vscnr2-style `diff_total` / `diff_max` model.

The earlier proposed average-luma-only detector was deliberately discarded because it was not close enough to vscnr2.

Reference behaviour from AviSynth-vsCnr2:

```text
for each chroma-resolution sample:
    diff_y = current_downsampled_luma - previous_downsampled_luma
    diff_total += abs(diff_y << (subSamplingW + subSamplingH))

    if scene_chroma:
        diff_total += abs(diff_u) + abs(diff_v)

if diff_total > diff_max:
    scene change
```

Threshold calculation:

```text
max_pixel_diff =
    if scene_chroma is false:
        219
    else:
        (219 + 224 * 2) >> (subSamplingW + subSamplingH)

diff_max = (
    scdthr * frame_width * frame_height * max_pixel_diff / 100.0
) << (bits_per_sample - 8)
```

CNR3 stores this as:

```cpp
int64_t scene_change_threshold;
```

For `scene_chroma=false`, only luma contributes to scene-change detection.

For `scene_chroma=true`, U and V chroma differences are added.

When scene-change detection fires, CNR3 does not merely reduce blend strength. To match vscnr2 more closely, it outputs the current source frame unchanged for chroma:

```text
Y was already copied unchanged.
U and V are copied from current source.
Recursive chroma blending is skipped for that frame.
```

Debug message when it fires:

```text
scene change detected; copying current source chroma and skipping recursive blend.
```

---

## 15. Scene-change debug policy

The first scene-change debug version printed every frame. That was too noisy.

Current reduced policy:

```text
Print scene-change stats only when:
    scene_change == true
or:
    diff_total >= 80% of diff_max
```

The debug line includes:

```text
rows
samples
diff_total
diff_max
threshold_percent
scene_change
scene_chroma
```

The 80% near-threshold case is useful because camera wobble, zoom, field jitter, pans, or large object motion can approach the vscnr2-style scene-change threshold without being true edit cuts.

---

## 16. Scene-change realclip findings

The real separated-field clip has camera wobble/zoom/motion but no actual edit cut in the tested range.

At default:

```text
scdthr=10.0
```

there were false positives. This is now understood as threshold sensitivity, not a code bug.

For the realclip at scdthr=10:

```text
scene_change_threshold=4541184
```

Effective average luma difference trigger for 720x288 separated fields with YUV420 is roughly:

```text
4541184 / (51840 chroma samples * 4 luma scale) ~= 21.9 8-bit luma levels
```

Camera wobble/zoom/field movement can exceed that.

Observed scdthr=10 log after debug reduction:

```text
frames near 97-98% before firing;
instance 2 frame 2 fired at about 100.59%;
frames 16-23 repeatedly fired just above 100% during wobble/zoom/motion.
```

Testing with higher thresholds:

```text
scdthr=20.0:
    scene_change_threshold=9082368
    no scene-change hits in the realclip range

scdthr=30.0:
    scene_change_threshold=13623552
    no scene-change hits in the realclip range
```

Conclusion:

```text
- implementation appears correct;
- default scdthr=10 is vscnr2-compatible but sensitive;
- for this shaky/zooming realclip, scdthr=20 is a better practical value;
- scdthr=30 is also safe for this clip but may miss moderate real cuts.
```

Do not change the algorithm based only on this realclip.

---

## 17. Synthetic scene-change tests

Two deterministic synthetic scene-change tests were created:

```text
test_cnr3_scenechange_8bit.vpy
test_cnr3_scenechange_16bit.vpy
```

They use controlled synthetic scenes rather than real footage or web samples. This is intentional because deterministic Y/U/V level steps make `diff_total` and `diff_max` easier to reason about.

Each script outputs something like:

```python
core.std.StackHorizontal([source, cnr3_scene_chroma_0, cnr3_scene_chroma_1])
```

The synthetic sequence uses 7 scenes of 8 frames each:

```text
00-07 base
08-15 small luma step: expected no scene change
16-23 moderate luma step: expected near-threshold but no scene change
24-31 larger luma step: expected scene change
32-39 modest chroma-only step: expected no scene change
40-47 large chroma-only step: expected scene change only when scene_chroma=1
48-55 major luma/chroma cut: expected scene change
```

8-bit results:

```text
frame 16:
    scene_chroma=1 near threshold only, about 96.39%, scene_change=0

frame 24:
    scene_chroma=0 fires
    scene_chroma=1 fires

frame 40:
    scene_chroma=1 fires only

frame 48:
    scene_chroma=0 fires
    scene_chroma=1 fires
```

16-bit results mirror 8-bit:

```text
frame 16:
    scene_chroma=1 near threshold only, about 96.76%, scene_change=0

frame 24:
    scene_chroma=0 fires
    scene_chroma=1 fires

frame 40:
    scene_chroma=1 fires only

frame 48:
    scene_chroma=0 fires
    scene_chroma=1 fires
```

Threshold scaling confirmed:

```text
8-bit scene_chroma=0:   6727680
16-bit scene_chroma=0:  1722286080

8-bit scene_chroma=1:   5099520
16-bit scene_chroma=1:  1305477120
```

These are exactly 256x at 16-bit, matching vscnr2-style `<< (depth - 8)` scaling.

---

## 18. Current test set

Current important test scripts:

```text
test_cnr3-8bit.vpy
test_cnr3-8bit_luma_constant_chroma_changes.vpy
test_cnr3-16bit.vpy
test_cnr3_realclip.vpy
test_cnr3_scenechange_8bit.vpy
test_cnr3_scenechange_16bit.vpy
```

Typical command pattern:

```bat
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" -r 1 --container y4m "D:\TEST\Vapoursynth_x64_R76\test_cnr3-8bit.vpy" - | "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: ...
```

Important:

```text
Use -r 1 until the future cache manager exists.
```

---

## 19. Visual Studio 2026 local build status

Visual Studio 2026 was set up locally and is now useful.

Key local choices:

```text
Project type:
    Dynamic-Link Library (DLL), C++, Windows, Library

Do not use:
    DLL (Universal Windows) / UWP

Platform:
    x64 only

C++ language standard:
    C++20

Precompiled headers:
    Not using precompiled headers
```

Local Release x64 build works.

Observed local Release DLL size:

```text
31.5 KB (32,256 bytes)
```

A previous build produced warnings from `ptrdiff_t` to `int`; these were cleaned up. Debug x64 and Release x64 builds succeeded after cleanup.

The local VS project contains the source and headers but the `.github/workflows/*.yml` file is not part of the C++ project. That is normal. Edit it through Folder View, File -> Open -> File, or an external editor.

---

## 20. GitHub Actions build status

Workflow file:

```text
.github/workflows/build-windows-x64.yml
```

Current runner:

```yaml
runs-on: windows-2025-vs2026
```

Current toolchain observed in Actions:

```text
Visual StudioVersion: 18.0
VSCMD_VER: 18.6.0
MSVC tools: 14.51.36231
COFF/PE Dumper: 14.51.36243.0
Windows SDK: 10.0.26100.0
Target arch: x64
```

Current build command uses direct `cl`, not MSBuild:

```text
cl /nologo /EHsc /std:c++20 /permissive- /O2 /GL /LD /MD /DNOMINMAX /I third_party\vapoursynth\include /Fo:build\ src\vapoursynth-Cnr3.cpp /Fe:build\cnr3.dll /link /LTCG
```

The workflow also checks for obvious legacy VapourSynth API usage.

The diagnostic step should be kept for now:

```cmd
dir build
dir /s build
dumpbin /headers build\cnr3.dll | findstr /i "machine DLL"
dumpbin /exports build\cnr3.dll
certutil -hashfile build\cnr3.dll SHA256
```

Verified Actions build output:

```text
raw cnr3.dll size: 29,184 bytes
artifact ZIP size: 14,433 bytes
machine: 8664 machine (x64)
File Type: DLL
export: VapourSynthPluginInit2
SHA256: aa58a971aeb1295d1beb12a81ea1695db92fb3ad1e697abc08604208c9d7073f
```

The artifact ZIP size is smaller than the local raw DLL because GitHub uploads a compressed artifact ZIP. The raw CI DLL is 29,184 bytes, which is plausible compared with the local VS Release DLL size of 32,256 bytes.

The export `VapourSynthPluginInit2` is present, so the CI DLL should be loadable as an API4 VapourSynth plugin.

---

## 21. Known-good current status

At this checkpoint:

```text
- API4-only plugin.
- AGPL-3.0-or-later header in source.
- Local VS 2026 Release x64 build works.
- GitHub Actions VS 2026 build works.
- GitHub Actions DLL verified as x64.
- GitHub Actions DLL exports VapourSynthPluginInit2.
- CNR3 registers as core.cnr3.CNR3().
- Strict streaming works under vspipe -r 1.
- Two field-stream CNR3 instances have independent caches.
- Lookup tables now follow narrow/wide response semantics.
- Real recursive chroma blend is connected.
- 8-bit and 16-bit paths work.
- Luma-buffer sharing optimisation works.
- vscnr2-style scene-change detection works.
- scene_chroma=1 detects chroma-only scene changes.
- debug output has been reduced to useful threshold/scene diagnostics.
- synthetic 8-bit and 16-bit scene-change tests pass.
- realclip scdthr sensitivity is understood.
```

---

## 22. Current caveats and unresolved items

### Strict streaming remains temporary

CNR3 still requires:

```bat
vspipe -r 1
```

for reliable current testing. This is not intended to be the final user experience.

### Cache manager is the major future work

The future cache manager must handle out-of-order frame requests while preserving recursive correctness.

Potential future policies:

```text
Policy B:
    strict streaming with clearer diagnostics and limited reset/recovery handling

Policy C:
    small reorder/checkpoint cache that can tolerate limited out-of-order requests
```

Earlier observations suggest the first practical scheduling problem is small forward readahead, often gap=1, not random seeking. A small pending/reorder capability may be the first useful cache-manager step before full checkpoint/recompute support.

### File splitting should probably precede cache-manager work

The source file is now large. The next non-algorithm step should probably be a no-logic-change file-organisation pass.

Good first split candidates:

```text
cnr3_debug.h/.cpp
    cnr3_debug_printf
    cnr3_vfprintf_stderr

cnr3_tables.h/.cpp
    scale_8bit_parameter_to_bit_depth
    build_cnr3_weight_table
    build_cnr3_lookup_tables
    table lookup helpers

cnr3_cache.h/.cpp
    Cnr3CacheManager
    cache state helpers
```

Do not split pixel-processing loops yet:

```text
build_cnr3_downsampled_luma_buffer_u8/u16
detect_cnr3_scene_change_u8/u16
process_cnr3_chroma_plane_u8/u16
process_cnr3_frame
```

Reason:

```text
Those functions still share frame geometry, bit depth, luma/chroma buffer assumptions, scene-change rules, and recursive state. Moving them too early increases risk without much benefit.
```

### Range handling remains a later review item

Scene-change threshold uses vscnr2 TV-range constants:

```text
Y max pixel diff = 219
chroma max pixel diff = 224
```

This matches the vscnr2-style logic. Full-range handling can be reviewed later if needed, but do not change it casually.

### Algorithm fidelity checklist remains open

Items to revisit later:

```text
- 10/12/14-bit scaling differences from exact original behaviour;
- scene-change behaviour on more real clips;
- visual comparison against vscnr2 where possible;
- whether blend switch remains public, becomes hidden, or is hard-coded true;
- whether debug output should be controlled by levels instead of bool.
```

---

## 23. Recommended next sequence

Do not do more algorithm work in the old long chat.

Recommended next sequence in a new chat:

```text
1. Load this v0.6 handover and the current source.
2. Confirm current Git status and latest commit.
3. Ensure scene-change/VS2026 checkpoint is committed and pushed.
4. Confirm GitHub Actions build passes.
5. Decide whether to tag the current stable redevelopment point.
6. Do a no-logic-change file-splitting boundary review.
7. Split only low-risk stable support code first.
8. Rebuild locally Debug x64 and Release x64.
9. Run existing tests.
10. Push and verify GitHub Actions.
11. Then begin cache-manager design.
```

Suggested commit message for current checkpoint:

```text
Implement vscnr2-style scene-change detection and VS2026 build verification
```

Shorter alternative:

```text
Implement vscnr2-style scene-change detection
```

---

## 24. User code-editing rules for future assistants

The user strongly prefers code edits in this format:

```text
1. Explain what changes and why.
2. Show prior code block with enough context lines to locate it.
3. Show replacement code block.
4. Avoid unnecessary changes to layout, names, comments, or unrelated code.
5. Use ASCII only in code/comments unless unavoidable.
6. Keep comments detailed and helpful for future maintainers.
7. Be explicit about whether output should remain pass-through or become visually different.
```

Additional standing preferences:

```text
- prioritize future maintainers;
- make code easy to understand;
- include excellent relevant comments;
- avoid unnecessary changes to code, comments, layout, names, variables, or functions;
- when proposing changes, clearly explain what changes and why;
- show prior and updated code blocks with a few context lines above and below;
- use ASCII only in code/comments;
- preserve incoming video properties where possible;
- treat video properties as potentially missing and apply defensible defaults carefully.
```

Avoid overlapping patch blocks. If a function signature changes, replacing the whole helper function may be safer than trying to patch several overlapping fragments.

---

## 25. Recommended prompt for the next chat

Use something like:

```text
We are developing CNR3, a VapourSynth API4 C++ plugin intended to closely follow AviSynth-vsCnr2/Cnr2 recursive chroma stabilisation.

Please read CNR3_Handover_Snapshot_v0.6_scenechange_vs2026.md and continue from there. The current implementation has real recursive chroma blending, shared luma buffers, vscnr2-style scene-change detection, VS2026 local build, and verified GitHub Actions build. Strict streaming still requires vspipe -r 1.

Next intended step: review whether to checkpoint/tag, then plan low-risk file splitting before starting the serious cache-manager work. Please preserve the user's patch style: explain what changes and why, show prior/replacement blocks with enough context, avoid unnecessary changes, and keep comments maintainable.
```

