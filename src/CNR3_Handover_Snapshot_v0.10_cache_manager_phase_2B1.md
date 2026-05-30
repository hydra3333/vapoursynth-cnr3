# CNR3 Handover Snapshot v0.10 - Cache Manager Phase 2B.1 Checkpoint

Date: 2026-05-30
Project: vapoursynth-cnr3
Repository: https://github.com/hydra3333/vapoursynth-cnr3
Purpose: detailed handover checkpoint for continuing development in a new ChatGPT chat with no hidden assumptions.

This document supersedes the earlier handover snapshots:

```text
CNR3_Handover_Snapshot_v0.5_expanded.md
CNR3_Handover_Snapshot_v0.6_scenechange_vs2026.md
CNR3_Handover_Snapshot_v0.7_scenechange_vs2026_cache_direction.md
CNR3_Handover_Snapshot_v0.8_structural_split_cache_direction.md
CNR3_Handover_Snapshot_v0.9_cache_v005_direction.md
```

This v0.10 snapshot is intentionally detailed. Do not compress it aggressively in a future handover unless the relevant detail has been superseded by code or by a later explicit decision.

The major additions since v0.5, v0.6, and v0.7 are:

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
- the previously discussed cache-manager direction is now explicitly recorded as an interim cache specification for future clarification/confirmation.
- DONE: low-risk common/cache split completed and committed.
- DONE: low-risk response-table helper split completed and committed.
- Visual Studio 2026 Debug x64 builds after the splits.
- Runtime smoke/regression tests passed after the splits.
- GitHub Actions build was updated to compile the new .cpp files.
- Current source layout is now multi-file but still one cnr3.dll.
- The cache-manager direction remains design-only; do not start implementation without explicit review/confirmation.
- A newer cache design document, `cnr3_cache_concept_v005.md`, has been accepted as the current draft direction in principle.
- The v005 cache direction supersedes the earlier speculative/not-ready/cascade-drain framing for implementation planning.
- The v005 cache direction targets `fmParallelRequests` first, not full `fmParallel`.
- The v005 cache direction uses a per-instance output-frame cache, checkpoint pool, `pin_count` checkpoint protection, and ascending chain-walk hole filling.
- A new development branch `dev_cache_manager` has started implementing the v005 cache-manager infrastructure in small safe phases.
- DONE on `dev_cache_manager`: Phase 1 cache-manager scaffold, Phase 2A basic helpers, Phase 2B lookup/checkpoint-selection helpers, and Phase 2B.1 helper thread-safety contract correction.
- Current cache-manager work remains infrastructure-only; Cnr3Data is not wired to the v005 cache manager, cnr3_get_frame() is unchanged, filter mode is unchanged, and runtime behaviour is unchanged.
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
src/cnr3_common.h
src/cnr3_cache.h
src/cnr3_cache.cpp
src/cnr3_response_tables.h
src/cnr3_response_tables.cpp
third_party/vapoursynth/include/VapourSynth4.h
third_party/vapoursynth/include/VSHelper4.h
vs/cnr3/...
```

The implementation is no longer a single large C++ source file. Two no-program-logic-change structural splits have been completed and committed.

Current source responsibilities:

```text
src/vapoursynth-Cnr3.cpp
    Main plugin implementation.
    Still owns:
        VapourSynth lifecycle / registration;
        argument parsing;
        input validation;
        public parameter scaling;
        blend-scale / blend-weight helper calculations;
        luma downsampling;
        scene-change detection;
        chroma blend processing;
        frame processing;
        diagnostics.

src/cnr3_common.h
    Shared CNR3 instance model and very small mechanical helpers.
    Contains:
        Cnr3Data;
        cnr3_clamp_int().

    This header shares type definitions across translation units. It does not
    create shared runtime state between filter instances.

src/cnr3_cache.h
src/cnr3_cache.cpp
    Strict-streaming cache state and helper functions.
    Contains:
        Cnr3CacheManager;
        cnr3_cache_clear();
        cnr3_cache_store_output_frame().

    The actual cache object is still owned by Cnr3Data, so each CNR3 filter
    instance has its own independent recursive state.

src/cnr3_response_tables.h
src/cnr3_response_tables.cpp
    Signed-difference response-table helpers.
    Contains:
        get_cnr3_table_value_for_signed_diff();
        build_cnr3_weight_table();
        build_cnr3_lookup_tables().
```

Explicitly not split yet:

```text
- parameter bit-depth scaling;
- blend scale / blend-weight helper calculations;
- blend sample action;
- luma-buffer builders;
- scene-change detectors;
- chroma plane processors;
- process_cnr3_frame();
- VapourSynth lifecycle / registration;
- debug helpers.
```

The structural splits were intentionally conservative. They moved stable support code but left algorithmically sensitive frame/chroma/scene-change processing in the main implementation file.

Visual Studio solution/project files exist and should be committed:

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

Local Release x64 build worked before the structural split.

Observed earlier local Release DLL size:

```text
31.5 KB (32,256 bytes)
```

A previous build produced warnings from `ptrdiff_t` to `int`; these were cleaned up. Debug x64 and Release x64 builds succeeded after cleanup.

DONE after v0.8 structural splits:

```text
- Visual Studio project includes the new source/header files.
- Debug x64 build succeeded after the common/cache split.
- Debug x64 build succeeded after the response-table split.
- Visual Studio compiled:
      cnr3_cache.cpp
      cnr3_response_tables.cpp
      vapoursynth-Cnr3.cpp
  and linked one cnr3.dll.
```

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

Current toolchain observed in Actions before the split:

```text
Visual StudioVersion: 18.0
VSCMD_VER: 18.6.0
MSVC tools: 14.51.36231
COFF/PE Dumper: 14.51.36243.0
Windows SDK: 10.0.26100.0
Target arch: x64
```

Current build command uses direct `cl`, not MSBuild.

DONE after the common/cache split and response-table split: GitHub Actions was updated to compile all current `.cpp` translation units:

```cmd
cl ^
  /nologo ^
  /EHsc ^
  /std:c++20 ^
  /permissive- ^
  /O2 ^
  /GL ^
  /LD ^
  /MD ^
  /DNOMINMAX ^
  /I third_party\vapoursynth\include ^
  /Fo:build\ ^
  src\vapoursynth-Cnr3.cpp ^
  src\cnr3_cache.cpp ^
  src\cnr3_response_tables.cpp ^
  /Fe:build\cnr3.dll ^
  /link ^
  /LTCG
```

The workflow also checks for obvious legacy VapourSynth API usage across `src`:

```text
#include "VapourSynth.h"
#include "VSHelper.h"
VSNodeRef
VSFrameRef
VSFuncRef
cloneFrameRef
cloneFrame(
freeFrameRef
createFilter(
```

The diagnostic step should be kept for now:

```cmd
dir build
dir /s build
dumpbin /headers build\cnr3.dll | findstr /i "machine DLL"
dumpbin /exports build\cnr3.dll
certutil -hashfile build\cnr3.dll SHA256
```

Earlier verified Actions build output before the split:

```text
raw cnr3.dll size: 29,184 bytes
artifact ZIP size: 14,433 bytes
machine: 8664 machine (x64)
File Type: DLL
export: VapourSynthPluginInit2
SHA256: aa58a971aeb1295d1beb12a81ea1695db92fb3ad1e697abc08604208c9d7073f
```

The artifact ZIP size is smaller than the local raw DLL because GitHub uploads a compressed artifact ZIP. The export `VapourSynthPluginInit2` must remain present, so the CI DLL should be loadable as an API4 VapourSynth plugin.

---

## 21. Known-good current status

At this v0.9 checkpoint:

```text
- API4-only plugin.
- AGPL-3.0-or-later header in source.
- Local VS 2026 Debug x64 build works after the splits.
- GitHub Actions VS 2026 build is updated for the new .cpp files.
- GitHub Actions DLL should continue to verify as x64 and export VapourSynthPluginInit2.
- CNR3 registers as core.cnr3.CNR3().
- Strict streaming works under vspipe -r 1.
- Two field-stream CNR3 instances have independent caches.
- Lookup tables follow narrow/wide response semantics.
- Real recursive chroma blend is connected.
- 8-bit and 16-bit paths work.
- Luma-buffer sharing optimisation works.
- vscnr2-style scene-change detection works.
- scene_chroma=1 detects chroma-only scene changes.
- debug output has been reduced to useful threshold/scene diagnostics.
- synthetic 8-bit and 16-bit scene-change tests pass.
- realclip scdthr sensitivity is understood.
- DONE: common/cache split completed and committed.
- DONE: response-table helper split completed and committed.
- DRAFT DIRECTION: cnr3_cache_concept_v005.md accepted in principle as the current cache-design basis.
- NOT IMPLEMENTED: v005 output-frame cache / fmParallelRequests cache manager.
```

Runtime smoke/regression tests run after the splits included:

```text
test_cnr3-8bit.vpy
test_cnr3-8bit_luma_constant_chroma_changes.vpy
test_cnr3-16bit.vpy
test_cnr3_realclip.vpy
test_cnr3_scenechange_8bit.vpy
test_cnr3_scenechange_16bit.vpy
```

Observed expected post-split diagnostics included:

```text
8-bit table samples:
    Y[0]=192, Y[17]=166, Y[35]=0, Y[255]=0
    U[0]=254, U[23]=131, U[47]=0, U[255]=0
    V[0]=254, V[23]=131, V[47]=0, V[255]=0

16-bit table samples:
    Y[0]=49344, Y[4497]=42120, Y[8995]=0, Y[65535]=0
    U[0]=65534, U[6039]=32771, U[12079]=0, U[65535]=0
    V[0]=65534, V[6039]=32771, V[12079]=0, V[65535]=0
```

Scene-change synthetic tests retained the expected frame pattern:

```text
8-bit and 16-bit:
    frame 16: scene_chroma=1 near threshold, scene_change=0
    frame 24: scene_change=1 for scene_chroma=0 and scene_chroma=1
    frame 40: scene_change=1 only for scene_chroma=1
    frame 48: scene_change=1 for both
```

---

## 22. Current caveats and unresolved items

### Strict streaming remains the current implemented behaviour

CNR3 still requires:

```bat
vspipe -r 1
```

for reliable current testing. This is not intended to be the final user experience.

Current implemented cache bridge:

```text
- one previous-output frame is retained per CNR3 instance;
- frame 0 initialises previous-output state;
- frame N requires output[N - 1] to have already been produced;
- n == cache.next_needed is accepted;
- out-of-order requests are rejected clearly;
- current testing/development uses vspipe -r 1;
- this is correctness-first and intentionally limited.
```

### Cache manager future work - v005 output-frame cache draft direction

Important correction for future continuation:

```text
The earlier cache-manager discussion is now superseded for implementation planning
by the newer CNR3 Output Frame Cache design document:

    cnr3_cache_concept_v005.md

The v005 design is accepted in principle as the current draft direction, but it
has not been implemented. It must still be translated carefully into CNR3 code
in small reviewed phases.
```

Historical note:

```text
Earlier handover versions preserved a broader interim direction involving recent
output back-cache, checkpointing, jump-distance assessment, speculative refusal /
not-available-yet ideas, cascade-drain behaviour, recovery/recompute, and cache
clearing/eviction policy.

That history should not be forgotten, but the speculative/not-ready/cascade-drain
framing should no longer be treated as the primary implementation plan. The v005
concept replaces it with a more VapourSynth-compatible fmParallelRequests design.
```

Current v005 design basis, in summary:

```text
Primary target mode:
    fmParallelRequests.

Reason:
    - multiple arInitial calls may run concurrently and can request source frames;
    - only one arAllFramesReady runs at a time;
    - the recursive output computation path remains serialised;
    - source-frame request/caching work can be warmed in parallel;
    - full fmParallel is deliberately deferred.

Per-instance ownership:
    Each CNR3 filter instance owns its own Cnr3Data and its own output-frame cache.
    No global runtime cache.
    No cross-instance cache sharing.
    The per-instance mutex lives inside Cnr3Data.

VapourSynth callback model:
    arInitial declares needed source frames using requestFrameFilter(), one frame per call.
    arAllFramesReady retrieves explicitly requested source frames using getFrameFilter(),
    one frame per call, computes one requested output frame, and returns one VSFrame pointer.
    arAllFramesReady must not call getFrameFilter() for source frames not requested in
    the matching arInitial for that invocation.

VS source cache:
    Useful for performance, not correctness.
    Correctness depends on source frames explicitly requested for the invocation and
    CNR3-held output/checkpoint frames.

CNR3 output-frame cache:
    Store filtered output frames, not merely source frames.
    Use two ordered pools/lists:
        - non-checkpoint output frame pool;
        - checkpoint output frame pool.
    Use a cache index for fast lookup across both pools.
    Use frame-number order for all cache/checkpoint lookup and pruning decisions.

Checkpoint policy:
    A checkpoint is a cached filtered output frame usable as a computation restart point.
    The chosen checkpoint for requested output N is the highest checkpoint frame number
    strictly less than N.
    Frame 0 is a permanent checkpoint until teardown.
    Initial draft constants in v005:
        cache_capacity = 100
        overflow_factor = 1.1
        checkpoint_interval = 10
        checkpoint_max_retain = 16
        checkpoint_min_retain = 6

Checkpoint pinning:
    The checkpoint selected in arInitial must still exist when the matching
    arAllFramesReady runs.
    v005 solves this with per-checkpoint-slot pin_count:
        - arInitial selects checkpoint K under mutex;
        - arInitial increments checkpoint[K].pin_count;
        - arInitial records K in frameData;
        - pruning skips checkpoints with pin_count > 0;
        - arAllFramesReady decrements pin_count on every exit path.
    pin_count cleanup paths are expected to need special care during coding.

Hole filling:
    "Holes" are not independent output-frame calculations.
    arAllFramesReady performs an ascending chain-walk from the chosen checkpoint toward N:
        - if output[f] is already cached, use it as the predecessor for the next step;
        - if output[f] is missing, compute it from the previous output and source[f];
        - store newly computed outputs conditionally;
        - compute output[N] last and return it.
    The chain-walk under staggered out-of-order requests is a key early instrumentation target.

Pruning:
    Pruning runs only inside arAllFramesReady after cache writes are complete.
    Under fmParallelRequests pruning is serialised with the rest of arAllFramesReady.
    arInitial never prunes.

Thread safety:
    Under fmParallelRequests, concurrent arInitial reads and pin_count changes must be
    mutex-protected.
    arAllFramesReady cache writes, checkpoint promotion, pruning, and pin_count decrement
    must also be mutex-protected.
    Heavy pixel computation should not be done while holding the mutex.

Future fmParallel:
    Full fmParallel remains a later benchmarking target only.
    It is not part of the first cache implementation phase.
    It would require extra active-reader / condition-variable logic for concurrent
    arAllFramesReady paths.

Diagnostics:
    v005 defines structured debug tags and teardown summary counters.
    The most important early diagnostics are expected to be:
        - pin_count cleanup correctness;
        - chain-walk behaviour under staggered out-of-order requests;
        - source request range width;
        - cache hit/miss rate;
        - holes computed vs holes already filled;
        - checkpoint chosen and checkpoint distance;
        - pruning and cache occupancy.
```

Important constraints for future implementation:

```text
- Recursive correctness must dominate convenience.
- Stored history must be filtered output history, not merely source frame history.
- Per-instance isolation is mandatory, especially for field-separated workflows where two CNR3 instances may run side by side.
- Do not implement full fmParallel in Phase 1.
- Do not rely on a speculative "not ready yet" return path.
- Keep current strict-streaming behaviour available and testable during development.
- Translate the v005 design into code in small tested phases.
```

Recommended cache implementation track after non-cache housekeeping:

```text
Phase 0:
    Preserve current strict-streaming implementation as the known-good baseline.

Phase 1:
    Add cache-manager data structures, per-instance mutex, checkpoint pin_count,
    frameData planning, init/free helpers, and debug counters while keeping behaviour
    as controlled as possible.

Phase 2:
    Add output cache insert/find, checkpoint promotion, pruning, and teardown release.

Phase 3:
    Change the filter mode to fmParallelRequests and implement the arInitial/arAllFramesReady
    v005 plan:
        - choose/pin checkpoint in arInitial;
        - request source range;
        - retrieve source range in arAllFramesReady;
        - ascending chain-walk from checkpoint to N;
        - conditional cache writes and checkpoint updates;
        - unpin on every exit path.

Phase 4:
    Enable structured instrumentation and compare against the known-good strict baseline.

Phase 5:
    Tune cache_capacity, checkpoint_interval, checkpoint retention, pruning, and diagnostics.
```

### Cache manager implementation branch status - dev_cache_manager through Phase 2B.1

Cache-manager implementation has started on the branch:

```text
dev_cache_manager
```

Current completed cache-manager infrastructure phases:

```text
Phase 1:
    Added cnr3_cache_manager.h and cnr3_cache_manager.cpp scaffold.
    Added v005 data structures and constants.
    Built locally.

Phase 2A:
    Added basic cache-manager inspection and clear/release helpers.
    Built and committed.

Phase 2B:
    Added lookup/checkpoint-selection helpers.
    Added nearest-prior-checkpoint terminology.
    Built and committed.

Phase 2B.1:
    Corrected the cache-manager helper contract before adding pin/unpin.
    Mutable-state public helpers now lock cache.cache_mutex internally.
    Constant-only helpers remain unlocked with comments explaining why.
    Removed the unsafe public raw-pointer output-frame lookup helper.
    Added/retained top-level safety comments and function-level thread-safety comment policy.
    Built and committed.
```

Current implementation state after Phase 2B.1:

```text
- Cnr3Data is not yet wired to Cnr3CacheManagerV005.
- cnr3_get_frame() is unchanged.
- Current strict-streaming cache behaviour is unchanged.
- VapourSynth filter mode is unchanged.
- No v005 cache manager runtime behaviour is active yet.
- No public cache-manager helper currently returns a raw borrowed VSFrame pointer.
```

Next intended implementation phase:

```text
Phase 2C:
    Add cache-manager debug/statistics counters.
    Add checkpoint pin/unpin helpers.
    Keep runtime behaviour unchanged until explicitly wired in a later phase.
```

### Cache manager implementation rules established during Phase 2B.1

The following implementation rules were agreed and should be treated as standing cache-manager coding rules.

#### Critical design rule - frame-number ordering

```text
ALL CACHES MUST ALWAYS BE STRICTLY ORDERED BY FRAME NUMBER.

All cache-related operations must strictly use only frame-number ordering.
Everything touching frame/checkpoint cache state must comply with this rule at all times, including:
    - output frame cache state;
    - checkpoint cache state;
    - cache indexes;
    - checkpoint lookup;
    - pruning;
    - promotion;
    - pin_count handling.
```

The helper name:

```cpp
cnr3_cache_manager_find_nearest_prior_checkpoint()
```

means:

```text
the checkpoint with the highest frame number that is strictly less than the requested frame number.
```

It does not mean most recently inserted, most recently used, most recently written, or nearest by cache-recency order.

#### Thread-safety rule

```text
Any code path that reads or writes mutable cache-manager state may run while another thread is also interacting with the same CNR3 filter instance.

Therefore, any code path that reads or writes mutable cache-manager state MUST hold the cache manager's per-instance cache_mutex while doing so.
```

This applies at least to:

```text
- cnr3_cache_manager_* helper functions;
- cnr3_get_frame() code that directly touches cache-manager state;
- pruning code;
- checkpoint promotion code;
- pin_count handling;
- cache index updates;
- future debug/statistics counters stored inside the cache manager.
```

Function naming and locking convention:

```text
All non-static cnr3_cache_manager_* functions MUST be thread-safe and lock internally if/as appropriate, unless their name ends in _externally_locked.

A helper whose name ends in _externally_locked MUST be called only while the caller already holds the cache manager's per-instance cache_mutex.

A public helper that locks internally must document that the caller must not already hold cache.cache_mutex, to avoid deadlock with the non-recursive mutex.
```

Mode-safety intent:

```text
fmUnordered:
    Safe, even though only one callback enters at a time.

fmParallelRequests:
    Safe with concurrent arInitial readers/pinners and serial arAllFramesReady writer.

fmParallel:
    Direct cache-manager metadata access and cache-manager helpers remain thread-safe under concurrent arInitial/arAllFramesReady calls, provided all mutable cache-manager state is accessed only while holding the per-instance cache_mutex.
    Full fmParallel algorithm correctness still needs later condition variables and active-computation state.
```

#### External frame reference safety

There are two distinct reference-ownership cases.

```text
1. Cache-owned frame references:
    Every VSFrame pointer stored in non_checkpoint_pool or checkpoint_pool MUST be a cache-owned reference obtained with vsapi->addFrameRef().
    This ensures that VapourSynth does not dispose of the frame while it is still referenced by the pointer in CNR3's cache.

    Every cache-owned frame reference MUST be released exactly once with vsapi->freeFrame() when that cache slot is pruned, when the cache is cleared, or when the owning CNR3 filter instance is destroyed.

    cache_index does not own frame references. It only aliases frame pointers owned by either non_checkpoint_pool or checkpoint_pool.

2. Caller-owned temporary frame references:
    If a public cache-manager helper returns a cached VSFrame pointer after releasing cache.cache_mutex, it MUST return a caller-owned temporary reference obtained with vsapi->addFrameRef().

    The helper name MUST make this explicit, for example:
        cnr3_cache_manager_find_output_frame_and_add_ref()

    The caller MUST release that caller-owned temporary reference exactly once with vsapi->freeFrame() on every success, error, and early-exit path.
```

Raw borrowed pointer rule:

```text
A helper that returns a raw borrowed cached VSFrame pointer without taking addFrameRef() MUST have a name ending in _externally_locked.

A raw borrowed pointer returned by an _externally_locked helper is valid only while the caller already holds cache.cache_mutex and only while the relevant cache slot remains protected from change or pruning. It must not be stored or used after the caller releases cache.cache_mutex unless the caller first takes its own addFrameRef().
```

pin_count rule:

```text
pin_count is not a VapourSynth frame reference.
pin_count does not call addFrameRef() and does not call freeFrame().
pin_count only prevents a checkpoint_pool slot from being pruned while an in-flight invocation depends on that checkpoint.
```

#### Function-level comment policy

Every cache-manager function should have a compact top-of-function comment explaining:

```text
- whether it locks internally, does not need locking, or requires external locking;
- why that locking policy is correct;
- the caller requirement;
- for _externally_locked helpers, the likely caller(s) responsible for holding cache.cache_mutex.
```

Preferred comment wording examples:

```text
Thread safety:
    Locks cache.cache_mutex internally. Reads mutable cache-manager state.
Caller requirement:
    Caller must not already hold cache.cache_mutex.
```

```text
Thread safety:
    Does not lock. Reads only compile-time constants and does not read or write mutable cache-manager state.
Caller requirement:
    None.
```

```text
Thread safety:
    Does not lock internally.
Caller requirement:
    Requires a lock to be applied by the caller before calling this function; i.e. the caller MUST already hold cache.cache_mutex.
```

### DONE: Low-risk structural splitting has been completed

Two no-program-logic-change structural splits were completed and committed before cache-manager implementation:

```text
1. Common/cache split:
       Cnr3Data -> cnr3_common.h
       cnr3_clamp_int() -> cnr3_common.h
       Cnr3CacheManager -> cnr3_cache.h
       cnr3_cache_clear() -> cnr3_cache.cpp
       cnr3_cache_store_output_frame() -> cnr3_cache.cpp

2. Response-table helper split:
       get_cnr3_table_value_for_signed_diff() -> cnr3_response_tables.cpp
       build_cnr3_weight_table() -> cnr3_response_tables.cpp
       build_cnr3_lookup_tables() -> cnr3_response_tables.cpp
```

These splits were independently checked for no unintended function/struct logic changes, built locally, runtime-tested, and committed as separate checkpoints.

Do not assume further splitting is required before the cache-manager design discussion. In particular, the following remain intentionally in `vapoursynth-Cnr3.cpp`:

```text
scale_8bit_parameter_to_bit_depth
get_cnr3_blend_scale
calculate_cnr3_combined_blend_weight
calculate_cnr3_max_possible_blend_weight
blend_cnr3_chroma_sample
build_cnr3_downsampled_luma_buffer_u8/u16
detect_cnr3_scene_change_u8/u16
process_cnr3_chroma_plane_u8/u16
process_cnr3_frame
VapourSynth lifecycle / registration
debug helpers
```

Do not split pixel-processing loops casually:

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

The previous v0.8 recommended sequence included reopening the interim cache-manager specification. That discussion has now produced `cnr3_cache_concept_v005.md`, accepted in principle as the current draft cache direction.

Recommended next sequence from this v0.10 checkpoint:

```text
1. Commit/save this v0.10 handover update.
2. Confirm the dev_cache_manager branch is clean after the Phase 2B.1 commit.
3. Move to Phase 2C:
       - add cache-manager debug/statistics counters;
       - add checkpoint pin/unpin helpers;
       - keep runtime behaviour unchanged.
4. Build locally after Phase 2C.
5. Review naming, comments, thread-safety, addFrameRef/freeFrame responsibilities, and no-runtime-change status before committing Phase 2C.
6. Only after Phase 2C is clean, proceed to later helper phases such as store/prune helpers.
```

Current non-cache housekeeping has mostly been completed or deferred. The important rule now is to continue the v005 cache-manager implementation in small buildable checkpoints. Do not wire the v005 cache manager into Cnr3Data or cnr3_get_frame() until the helper layer is sufficiently reviewed and tested.

Suggested prior structural checkpoint commit messages already completed:

```text
Split common instance data and cache helpers
```

and:

```text
Split response table helpers
```

Suggested future documentation/checkpoint commit message:

```text
Update handover with cache v005 design direction
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

Please read CNR3_Handover_Snapshot_v0.10_cache_manager_phase_2B1.md and continue from there. The current implementation has real recursive chroma blending, shared luma buffers, vscnr2-style scene-change detection, VS2026 local build, verified GitHub Actions build, and completed no-program-logic-change structural splits:

- common/cache split;
- response-table helper split.

Strict streaming still requires vspipe -r 1 in the current runtime path.

The accepted draft cache direction is `cnr3_cache_concept_v005.md`. It targets fmParallelRequests first, uses a per-instance output-frame cache, checkpoint pool, pin_count protection between arInitial and arAllFramesReady, and ascending chain-walk hole filling. It supersedes the earlier speculative/not-ready/cascade-drain framing for implementation planning.

Cache-manager implementation has started on branch `dev_cache_manager`. Completed infrastructure checkpoints are:

- Phase 1: cnr3_cache_manager.h/.cpp scaffold;
- Phase 2A: basic clear/count/contains helpers;
- Phase 2B: lookup/checkpoint-selection helpers;
- Phase 2B.1: public helper locking contract, safety comments, unsafe raw-pointer helper removal.

The v005 cache manager is still not wired into Cnr3Data or cnr3_get_frame(); runtime behaviour is unchanged. Continue with Phase 2C: stats counters and checkpoint pin/unpin helpers, still without runtime wiring. Preserve the user's patch style: explain what changes and why, show prior/replacement blocks with enough context, avoid unnecessary changes, and keep comments maintainable.
```



---

## 26. v0.7 addendum - cache-manager direction must not be forgotten

This v0.7 update was made after reviewing the next-step discussion at the end of the successful scene-change/VS2026 chat.

The important correction is that the future cache-manager direction had already been discussed in more detail than a simple "choose strict vs cache later" framing. The user reminded that the intended direction included checkpointing, a speculative "not available yet" / refusal idea to let request cascades drain, a recent output back-cache of roughly 50 frames, jump-distance evaluation, and explicit cache-clearing/recovery decisions, all per CNR3 instance.

That direction is now preserved in Section 22 as an interim cache-manager specification. It is not a final implementation contract, but it is important design context and should be brought forward into the next chat before any cache-manager coding starts.

A future assistant should not discard this direction or replace it with a vague "strict now, cache later" summary. The correct framing is:

```text
Current implementation:
    strict streaming one-previous-output API4 bridge, requiring vspipe -r 1.

Planned design direction to confirm/refine:
    per-instance cache manager and lists with recent output back-cache, checkpointing,
    jump-distance policy, cascade-drain/smart-speculative-refusal (eg perhaps
    using lists like in-progress, recently-requested-and-speculatively-refused, etc
    to attempt predict if/when the immediately prior output frame may become available
    and lead to cascade-drain), recompute/recovery (eg perhaps when speculative refusal exhausion),
    and explicit cache-clearing/eviction rules etc.  There is a draft proposed cache specifcation
    whih is to be treated as a starting point and certainly not an end-point since explicit and well defined
    rules talking care of all normal and edge cases must be well defined and logically analyzed/tested.

```

---

## 27. v0.8 addendum - structural split checkpoint completed

This v0.8 update records work completed after v0.7:

```text
DONE: common/cache split.
DONE: response-table helper split.
DONE: Visual Studio Debug x64 build after both splits.
DONE: runtime smoke/regression tests after both splits.
DONE: GitHub Actions updated to compile the new .cpp files.
DONE: both splits committed as separate checkpoints.
```

The common/cache split moved stable instance/cache support code without intending to change behaviour:

```text
Cnr3Data -> cnr3_common.h
cnr3_clamp_int() -> cnr3_common.h
Cnr3CacheManager -> cnr3_cache.h
cnr3_cache_clear() -> cnr3_cache.cpp
cnr3_cache_store_output_frame() -> cnr3_cache.cpp
```

The response-table helper split moved only the agreed signed-difference response-table helpers:

```text
get_cnr3_table_value_for_signed_diff() -> cnr3_response_tables.cpp
build_cnr3_weight_table() -> cnr3_response_tables.cpp
build_cnr3_lookup_tables() -> cnr3_response_tables.cpp
```

The following were deliberately not moved during the response-table split:

```text
scale_8bit_parameter_to_bit_depth
get_cnr3_blend_scale
calculate_cnr3_combined_blend_weight
calculate_cnr3_max_possible_blend_weight
blend_cnr3_chroma_sample
cnr3_get_table_for_chroma_plane
```

Reason:

```text
scale_8bit_parameter_to_bit_depth is parameter normalisation, not response-table creation.
The blend-scale/weight helpers and blend sample action are part of the blend calculation path.
cnr3_get_table_for_chroma_plane is a tiny chroma-plane routing helper used by processing code.
```

Important caution retained:

```text
The structural splits do not implement the future cache manager.
Current cache behaviour remains strict streaming only.
Use vspipe -r 1 for current reliable testing.
```

Next design topic:

```text
Re-open the interim cache-manager specification in Section 22.
Discuss and refine the draft design before any code is written.
```

---

## 28. v0.9 addendum - cache v005 draft direction accepted in principle

This v0.9 update records the outcome of the cache-design discussion after v0.8.

The older interim cache direction is retained as historical context, but the current accepted draft direction is now:

```text
cnr3_cache_concept_v005.md
```

This v005 concept is accepted **in principle** as the new cache implementation direction. It is still a draft design, not implemented code.

Key v005 decisions:

```text
- Target fmParallelRequests first.
- Keep fmParallel as a later benchmarking/future target only.
- Use a per-instance output-frame cache owned by Cnr3Data.
- Use ordered non-checkpoint and checkpoint pools plus a fast cache index.
- Use const VSFrame * stored via addFrameRef()/freeFrame().
- Use frame-number order for all cache and checkpoint lookup.
- Use checkpoint frames as computation restart points.
- Use pin_count to protect the checkpoint selected in arInitial until the matching arAllFramesReady finishes.
- Pass the chosen checkpoint frame number through frameData.
- Fill cache holes by ascending chain-walk, not independent per-frame calculations.
- Treat VS source cache as a performance optimisation, not a correctness dependency.
- Add structured cache instrumentation early.
```

High-risk implementation details to watch when coding begins:

```text
- pin_count decrement must happen on every arAllFramesReady exit path;
- source frames retrieved by getFrameFilter() must be released on every exit path;
- chain-walk must be validated under staggered out-of-order frame requests;
- checkpoint lookup must be strictly by frame number, never by insertion/recency order;
- mutex must remain per-instance inside Cnr3Data;
- heavy pixel computation should happen outside the mutex;
- current strict streaming behaviour should remain available and testable while the new cache is developed.
```

The next work after this handover update should be non-cache housekeeping unless already complete:

```text
- confirm Release x64 local build;
- confirm GitHub Actions baseline;
- preserve final strict-mode regression log;
- optionally tag a pre-cache-manager structural baseline.
```

---

## 29. v0.10 addendum - cache manager Phase 2B.1 checkpoint

This v0.10 update records the start of v005 cache-manager implementation on branch `dev_cache_manager`.

Completed implementation checkpoints so far:

```text
Phase 1:
    Added cnr3_cache_manager.h/.cpp scaffold.
    Added v005 cache-manager constants and data structures.

Phase 2A:
    Added basic inspection and clear/release helpers.

Phase 2B:
    Added lookup/checkpoint-selection helper direction.
    Adopted human-readable name:
        cnr3_cache_manager_find_nearest_prior_checkpoint()

Phase 2B.1:
    Made existing mutable-state public helpers lock internally.
    Removed unsafe public raw borrowed VSFrame pointer helper.
    Added explicit safety comments/rules for frame-number ordering, thread safety, external frame references, addFrameRef/freeFrame ownership, and pin_count semantics.
    Built and committed.
```

No runtime behaviour has changed as of this checkpoint:

```text
- Cnr3Data does not yet contain Cnr3CacheManagerV005.
- cnr3_get_frame() does not yet use the v005 cache manager.
- current strict-streaming cache remains active.
- VapourSynth filter mode remains unchanged.
- no v005 cache-manager helper is used by the runtime path yet.
```

Important implementation rules established in Phase 2B.1:

```text
- All cache/checkpoint ordering decisions must be by frame number only.
- All mutable cache-manager state must be accessed under cache.cache_mutex.
- Non-static cnr3_cache_manager_* functions lock internally unless their name ends in _externally_locked.
- _externally_locked helpers require the caller to already hold cache.cache_mutex.
- Public helpers must not return raw borrowed cached VSFrame pointers after unlocking.
- If a public helper later returns a cached VSFrame pointer after unlocking, it must return a caller-owned temporary addFrameRef() reference and its name must include _and_add_ref.
- Cache-owned frame references are acquired with addFrameRef() on insertion and released by pruning, clear, or teardown.
- pin_count is not a VSFrame reference; it prevents checkpoint pruning only.
- Every cache-manager function should include a compact thread-safety comment explaining its locking behaviour and caller requirements.
```

Next implementation phase:

```text
Phase 2C:
    Add cache-manager debug/statistics counters.
    Add checkpoint pin/unpin helpers.
    Keep runtime behaviour unchanged.
```

