# CNR3 VapourSynth Registration and Call Structure v0.6

Date: 2026-05-27
Project: vapoursynth-cnr3
Purpose: explain how VapourSynth loads, registers, creates, calls, processes, caches, and frees the CNR3 plugin, reflecting the current v0.6 implementation state.

This document supersedes:

```text
CNR3_VapourSynth_Registration_and_Call_Structure.md
```

The earlier document described the scaffold/pass-through era. This v0.6 version updates the call structure to include:

```text
- real recursive chroma blend;
- shared downsampled-luma guard buffers;
- vscnr2-style scene-change detection;
- strict streaming cache policy;
- fmUnordered API4 scheduling consequences;
- Visual Studio 2026 / GitHub Actions verified build status.
```

---

## 1. Practical mental model

The most important distinction remains:

```text
CNR3 instance:
    owns one input source node
    owns that source stream's options
    owns that source stream's lookup tables
    owns that source stream's scene-change threshold
    owns that source stream's recursive cache

Worker thread:
    may execute work for any CNR3 instance
    receives instanceData for the specific request
    must use only that instance's cache/state for that frame
```

The cache belongs with:

```text
Cnr3Data
```

not with:

```text
a global plugin object
an OS thread
a VapourSynth worker thread
```

Future cache-manager problem:

```text
How do we safely tolerate out-of-order frame requests for the same instance while preserving recursive output[N - 1] correctness?
```

Current development answer:

```text
Use strict streaming and run tests with vspipe -r 1.
```

---

## 2. DLL load and plugin registration

When VapourSynth loads `cnr3.dll`, it calls the API4 plugin entry point:

```cpp
VS_EXTERNAL_API(void) VapourSynthPluginInit2(
    VSPlugin *plugin,
    const VSPLUGINAPI *vspapi
)
```

Registration-time structure:

```text
VapourSynth loads cnr3.dll
|
|- VapourSynthPluginInit2(plugin, vspapi)
   |
   |- vspapi->configPlugin(...)
   |     |
   |     |- tells VapourSynth:
   |          plugin id
   |          namespace = "cnr3"
   |          description
   |          version
   |          API version
   |
   |- vspapi->registerFunction(...)
         |
         |- registers:
              core.cnr3.CNR3(...)
         |
         |- tells VapourSynth:
              creation callback = cnr3_create
```

Intended neutral plugin identifier:

```cpp
vspapi->configPlugin(
    "org.vapoursynth.cnr3",
    "cnr3",
    "CNR3 experimental recursive chroma stabiliser",
    VS_MAKE_VERSION(0, 1),
    VAPOURSYNTH_API_VERSION,
    0,
    plugin
);
```

Important:

```text
The plugin identifier should not contain a personal name.
The Python namespace comes from "cnr3".
The Python function name comes from "CNR3".
Therefore the public call is core.cnr3.CNR3(...).
```

---

## 3. Function registration

The function registration should include the public options currently supported by the source.

The historically stable core options are:

```text
clip
mode
ln/lm
un/um
vn/vm
scdthr
scene_chroma
debug
```

During development, `blend` has also existed as a maintenance/testing switch and defaults to true. Check the current source before relying on whether it is registered publicly.

Representative registration shape:

```cpp
vspapi->registerFunction(
    "CNR3",
    "clip:vnode;"
    "mode:data:opt;"
    "ln:int:opt;"
    "lm:int:opt;"
    "un:int:opt;"
    "um:int:opt;"
    "vn:int:opt;"
    "vm:int:opt;"
    "scdthr:float:opt;"
    "scene_chroma:int:opt;"
    "blend:int:opt;"
    "debug:int:opt;",
    "clip:vnode;",
    cnr3_create,
    nullptr,
    plugin
);
```

If `blend` is not registered in the current source, remove it from this signature. The implementation has used `blend=true` as the default release behaviour.

---

## 4. Script/create time

When a script calls:

```python
clip2 = core.cnr3.CNR3(clip, debug=True)
```

VapourSynth does not process all frames immediately.

Instead it calls:

```cpp
static void VS_CC cnr3_create(...)
```

Creation-time responsibilities:

```text
- read input clip node;
- read video info;
- validate format/dimensions/sample type;
- read options;
- assign instance_id;
- calculate bits_per_sample and sample_peak;
- scale public 8-bit-domain thresholds to actual bit depth;
- build Y/U/V response lookup tables;
- calculate vscnr2-style scene_change_threshold;
- create one Cnr3Data object;
- create the output filter node.
```

The important per-instance object:

```cpp
Cnr3Data *data = new Cnr3Data(local);
```

That object owns:

```text
input node
video info pointer
options
lookup tables
scene-change threshold
cache
instance_id
debug flag
```

Creation-time call structure:

```text
Python script calls core.cnr3.CNR3(clip, ...)
|
|- VapourSynth calls cnr3_create(...)
   |
   |- mapGetNode(in, "clip", ...)
   |- getVideoInfo(local.node)
   |- validate_cnr3_format(local.vi, out, vsapi)
   |- read optional parameters
   |- scale 8-bit-domain parameters to bit depth
   |- build_cnr3_lookup_tables(local, out, vsapi)
   |- calculate scene_change_threshold
   |- new Cnr3Data(local)
   |- createVideoFilter(..., cnr3_get_frame, cnr3_free, fmUnordered, ..., data, ...)
```

---

## 5. Output filter node creation

CNR3 creates the output filter node with:

```cpp
vsapi->createVideoFilter(
    out,
    "CNR3",
    data->vi,
    cnr3_get_frame,
    cnr3_free,
    fmUnordered,
    deps,
    1,
    data,
    core
);
```

This tells VapourSynth:

```text
output filter name:
    CNR3

output video info:
    same as input clip

frame callback:
    cnr3_get_frame

free callback:
    cnr3_free

scheduling mode:
    fmUnordered

dependency:
    source clip node

instanceData:
    data
```

The `data` pointer later returns to CNR3 as `instanceData` on every frame callback.

---

## 6. fmUnordered and strict streaming

CNR3 uses:

```text
fmUnordered
```

Important:

```text
fmUnordered means only one thread can call this filter's getframe at a time, but it does not guarantee display-order frame requests.
```

Because CNR3 is recursive:

```text
output[N] depends on output[N - 1]
```

current development uses a strict streaming cache policy:

```text
Only frame n == cache.next_needed is accepted.
```

Tests must currently use:

```bat
vspipe -r 1
```

without `-r 1`, VapourSynth may request ahead, for example:

```text
requested=3 while next_needed=2
```

That is expected until a real cache/reorder manager exists.

---

## 7. Frame callback phase 1: arInitial

When called with `arInitial`, CNR3 asks VapourSynth for the matching source frame:

```cpp
vsapi->requestFrameFilter(n, d->node, frameCtx);
return nullptr;
```

Call structure:

```text
Downstream asks for output frame n
|
|- VapourSynth calls cnr3_get_frame(n, arInitial, instanceData, ...)
   |
   |- Cnr3Data *d = static_cast<Cnr3Data *>(instanceData)
   |- requestFrameFilter(n, d->node, frameCtx)
   |- return nullptr
```

---

## 8. Frame callback phase 2: arAllFramesReady

When the requested source frame is ready, VapourSynth calls CNR3 again with `arAllFramesReady`.

Current high-level structure:

```text
VapourSynth calls cnr3_get_frame(n, arAllFramesReady, instanceData, ...)
|
|- Cnr3Data *d = static_cast<Cnr3Data *>(instanceData)
|- src = getFrameFilter(n, d->node, frameCtx)
|- check strict recursive order:
|     if n != d->cache.next_needed:
|         setFilterError(...)
|         free src if necessary
|         return nullptr
|- dst = newVideoFrame(...)
|- process_cnr3_frame(d, n, src, dst, frameCtx, vsapi)
|- cnr3_cache_store_output_frame(d->cache, dst, n, vsapi)
|- freeFrame(src)
|- return dst
```

The output `dst` is returned to VapourSynth, and an additional read-only reference is stored as `prev_output` for the next recursive frame.

---

## 9. Current process_cnr3_frame structure

Current frame processing:

```text
process_cnr3_frame(d, frame_number, src, dst, frameCtx, vsapi)
|
|- validate d/src/dst/frame_number
|- prev_output = d->cache.prev_output
|- frame 0:
|     no previous output required
|- frame N > 0:
|     require prev_output
|- bytes_per_sample = (bits_per_sample + 7) / 8
|- copy Y plane unchanged
|- chroma_width/chroma_height = dimensions of plane 1
|- verify plane 2 dimensions match plane 1
|- build current_luma once at chroma resolution
|- if frame N > 0:
|     build previous_luma once at chroma resolution from prev_output
|- if frame N > 0:
|     detect vscnr2-style scene change using current_luma, previous_luma,
|     and optional U/V differences
|     if scene_change:
|         copy current source U and V
|         return true
|- process U through process_cnr3_chroma_plane(...)
|- process V through process_cnr3_chroma_plane(...)
```

Y is always copied unchanged.

Scene-change detection happens before U/V recursive blending.

---

## 10. Shared downsampled-luma buffers

Current luma-buffer design:

```text
- current_luma is built once per frame at chroma resolution;
- previous_luma is built once per frame at chroma resolution for frame N > 0;
- U and V plane processing share the same luma buffers.
```

This replaced the earlier inefficient structure where U built luma buffers and V rebuilt them again.

Defensive geometry policy:

```text
- use U plane dimensions as reference chroma geometry;
- verify V plane dimensions match;
- fail clearly if they do not.
```

This matters for accepted YUV 4:2:0, 4:2:2, 4:4:0, and 4:4:4 clips.

---

## 11. Chroma-plane processing

Current chroma-plane dispatcher:

```text
process_cnr3_chroma_plane(d, frame_number, plane, src, prev_output, dst,
                          shared_chroma_width, shared_chroma_height,
                          bytes_per_sample,
                          current_luma, previous_luma, vsapi)
|
|- validate d/src/dst/vsapi
|- validate plane is 1 or 2
|- frame N > 0 requires prev_output
|- verify current plane dimensions match shared chroma dimensions
|- verify luma buffer sizes match expected sample count
|- dispatch:
|     bytes_per_sample == 1 -> process_cnr3_chroma_plane_u8(...)
|     bytes_per_sample == 2 -> process_cnr3_chroma_plane_u16(...)
```

Per-sample chroma path:

```text
for each chroma sample:
    current_chroma = source current U/V
    previous_chroma = prev_output U/V when frame > 0
    chroma_signed_diff = current_chroma - previous_chroma
    y_signed_diff = current_downsampled_luma - previous_downsampled_luma
    chroma_response = U/V table lookup
    y_response = Y table lookup
    if blend and previous output exists:
        blend previous filtered chroma with current source chroma
    else:
        write current source chroma
```

Frame 0 writes source chroma.

`blend=false` remains a maintenance/testing switch that forces source chroma while still keeping read/difference/table paths available.

---

## 12. Recursive blend formula

Current recursive blend formula:

```text
weight = y_response * chroma_response
shift2 = bits_per_sample * 2
shift = 1 << shift2
shift1 = shift / 2

dst = (
        weight * previous_filtered_chroma
      + (shift - weight) * current_source_chroma
      + shift1
      ) >> shift2
```

Interpretation:

```text
High weight:
    reuse more previous filtered chroma.

Low weight:
    keep more current source chroma.
```

This is the first real vscnr2-style recursive chroma blend.

---

## 13. Scene-change detection call structure

Frame-level scene-change detection is deliberately not inside the separate U/V plane processors because vscnr2 combines luma and optional U/V differences in one scene metric.

Current detector flow:

```text
process_cnr3_frame(...)
|
|- after current_luma and previous_luma are ready:
|     detect_cnr3_scene_change(...)
|       |
|       |- dispatch by bytes_per_sample:
|       |     u8 or u16
|       |
|       |- for each chroma-resolution sample:
|       |     diff_y = current_luma[i] - previous_luma[i]
|       |     diff_total += abs(diff_y << (subSamplingW + subSamplingH))
|       |
|       |     if scene_chroma:
|       |         diff_total += abs(current_u - previous_output_u)
|       |         diff_total += abs(current_v - previous_output_v)
|       |
|       |     if diff_total > scene_change_threshold:
|       |         scene_change = true
|       |         stop early
|       |
|       |- return Cnr3SceneChangeStats
|
|- if scene_change:
|     copy current source U and V
|     skip recursive chroma blend
```

This mirrors vscnr2's behaviour of returning the current frame when scene-change detection fires.

---

## 14. Scene-change threshold calculation

CNR3 calculates the vscnr2-style threshold once in `cnr3_create()`.

Concept:

```text
subsampling_shift = subSamplingW + subSamplingH

if scene_chroma is false:
    max_pixel_diff = 219
else:
    max_pixel_diff = (219 + 224 * 2) >> subsampling_shift

scene_change_threshold =
    (scdthr * width * height * max_pixel_diff / 100.0) << (bits_per_sample - 8)
```

Examples observed:

```text
640x480 YUV420 8-bit scene_chroma=0:
    6727680

640x480 YUV420 8-bit scene_chroma=1:
    5099520

640x480 YUV420 16-bit scene_chroma=0:
    1722286080

640x480 YUV420 16-bit scene_chroma=1:
    1305477120
```

The 16-bit values are 256x the 8-bit values, matching `<< (depth - 8)`.

---

## 15. Scene-change debug

Scene-change debug is intentionally compact.

Print only when:

```text
scene_change == true
```

or:

```text
diff_total >= 80% of diff_max
```

Debug line includes:

```text
rows
samples
diff_total
diff_max
threshold_percent
scene_change
scene_chroma
```

Example:

```text
CNR3 debug: instance=2, frame=40, scene-change stats: rows=132, samples=42240, diff_total=5111040, diff_max=5099520, threshold_percent=100.23%, scene_change=1, scene_chroma=1
CNR3 debug: instance=2, frame=40, scene change detected; copying current source chroma and skipping recursive blend.
```

---

## 16. Cleanup time

When VapourSynth no longer needs the output filter node, it calls:

```cpp
static void VS_CC cnr3_free(...)
```

Cleanup structure:

```text
VapourSynth no longer needs output filter node
|
|- cnr3_free(instanceData, ...)
   |
   |- Cnr3Data *d = static_cast<Cnr3Data *>(instanceData)
   |- if d->node != nullptr:
   |     freeNode(d->node)
   |- clear cache:
   |     if cache.prev_output != nullptr:
   |         freeFrame(cache.prev_output)
   |     cache.prev_output = nullptr
   |     cache.next_needed = 0
   |- delete d
```

Frame ownership remains critical. Stored `prev_output` is an added frame reference and must be freed.

---

## 17. Multiple CNR3 instances

This script creates two independent instances:

```python
first_denoised  = core.cnr3.CNR3(first, debug=True)
second_denoised = core.cnr3.CNR3(second, debug=True)
```

They have separate:

```text
input node
options
lookup tables
scene-change threshold
recursive cache
instance_id
```

This is required for separated-field workflows.

`instance_id` debug distinguishes logs:

```text
instance=1
instance=2
```

Do not share recursive cache between instances.

---

## 18. Current complete call diagram

```text
VapourSynth loads cnr3.dll
|
|- VapourSynthPluginInit2
|  |
|  |- configPlugin(namespace="cnr3", ...)
|  |- registerFunction("CNR3", ..., cnr3_create, ...)
|
|- Python script calls core.cnr3.CNR3(clip, ...)
|  |
|  |- cnr3_create
|     |
|     |- read node/video info/options
|     |- validate format
|     |- scale thresholds
|     |- build lookup tables
|     |- calculate scene_change_threshold
|     |- allocate Cnr3Data
|     |- createVideoFilter(..., fmUnordered, instanceData=Cnr3Data*)
|
|- Downstream requests frame n
|  |
|  |- cnr3_get_frame(n, arInitial, instanceData, ...)
|  |  |
|  |  |- requestFrameFilter(n, source node)
|  |  |- return nullptr
|  |
|  |- source frame n becomes ready
|  |
|  |- cnr3_get_frame(n, arAllFramesReady, instanceData, ...)
|     |
|     |- get source frame n
|     |- enforce strict order n == cache.next_needed
|     |- allocate dst
|     |- process_cnr3_frame
|     |  |
|     |  |- copy Y unchanged
|     |  |- build shared luma buffers
|     |  |- detect scene change
|     |  |- if scene change:
|     |  |     copy source U/V
|     |  |- else:
|     |        recursively blend U/V using output[N - 1]
|     |
|     |- store dst as cache.prev_output using addFrameRef
|     |- free source frame
|     |- return dst
|
|- Later cleanup
   |
   |- cnr3_free
      |
      |- free input node
      |- free cached previous output frame
      |- delete Cnr3Data
```

---

## 19. Build verification status

Local Visual Studio 2026 Release x64 build:

```text
works
local cnr3.dll size observed: 32,256 bytes
```

GitHub Actions build:

```text
runner: windows-2025-vs2026
MSVC: 14.51 / VS 2026
Windows SDK: 10.0.26100.0
raw cnr3.dll size: 29,184 bytes
artifact ZIP size: 14,433 bytes
machine: x64
export: VapourSynthPluginInit2
SHA256: aa58a971aeb1295d1beb12a81ea1695db92fb3ad1e697abc08604208c9d7073f
```

The artifact ZIP size is not a problem because upload-artifact compresses the raw DLL.

---

## 20. Current test implications

For current testing:

```bat
vspipe -r 1 --container y4m script.vpy - | ffmpeg ...
```

For real clips with camera wobble/zoom:

```python
core.cnr3.CNR3(clip, scdthr=20.0)
```

may be more practical than the historical default:

```python
scdthr=10.0
```

Keep default 10.0 for vscnr2 compatibility unless a deliberate decision is made to change default behaviour.

---

## 21. Next development implications

Do not start by rewriting frame processing.

Recommended next work:

```text
1. Commit/tag current known-good state if desired.
2. Do low-risk file-splitting planning.
3. Split stable support code first.
4. Rebuild/test.
5. Then start cache-manager design.
```

Good first split candidates:

```text
cnr3_debug.h/.cpp
cnr3_tables.h/.cpp
cnr3_cache.h/.cpp
```

Keep pixel-processing and scene-change loops in the main file until the algorithm is more settled.
---

## Appendix A - Current CNR3 function-call cascade trees

This appendix records what calls what in the current v0.6 source structure. It is intended as a practical navigation aid for future maintenance, file-splitting, and cache-manager work.

Scope rules for these trees:

```text
- CNR3-to-CNR3 function calls are shown as forward cascade trees.
- VapourSynth-to-CNR3 entry points are shown separately.
- CNR3-to-VapourSynth API calls are listed separately in reverse/API-use form.
- Standard library calls, arithmetic, loops, and simple field assignments are omitted.
- Some debug/stat helper functions may remain compiled but have their call sites commented out during quieter test runs. Those are noted where relevant.
```

### A.1 VapourSynth loading and registration cascade

```text
VapourSynth loads cnr3.dll
|
|- VapourSynth looks for exported API4 plugin entry point
|  |
|  |- VapourSynthPluginInit2(plugin, vspapi)
|     |
|     |- vspapi->configPlugin(...)
|     |  |
|     |  |- tells VapourSynth the plugin id, namespace, description,
|     |     version, and API version
|     |
|     |- vspapi->registerFunction(...)
|        |
|        |- registers public script function:
|        |     core.cnr3.CNR3(...)
|        |
|        |- records creation callback:
|              cnr3_create
```

Important consequence:

```text
VapourSynthPluginInit2 is called by VapourSynth at plugin-load/registration time.
It is not called by CNR3 code.
```

### A.2 Script create-time cascade

When a script calls:

```python
clip2 = core.cnr3.CNR3(clip, ...)
```

VapourSynth calls the creation callback:

```text
VapourSynth
|
|- cnr3_create(in, out, userData, core, vsapi)
   |
   |- vsapi->mapGetNode(in, "clip", ...)
   |- vsapi->getVideoInfo(local.node)
   |
   |- validate_cnr3_format(local.vi, out, vsapi)
   |  |
   |  |- on validation failure:
   |       vsapi->mapSetError(out, ...)
   |
   |- get_optional_data_string(in, vsapi, "mode", "oxx")
   |  |
   |  |- vsapi->mapGetData(...)
   |
   |- get_optional_int(in, vsapi, "ln", ...)
   |  |
   |  |- vsapi->mapGetInt(...)
   |
   |- get_optional_int(in, vsapi, "lm", ...)
   |- get_optional_int(in, vsapi, "un", ...)
   |- get_optional_int(in, vsapi, "um", ...)
   |- get_optional_int(in, vsapi, "vn", ...)
   |- get_optional_int(in, vsapi, "vm", ...)
   |- get_optional_int(in, vsapi, "scene_chroma", ...)
   |- get_optional_int(in, vsapi, "blend", ...)
   |- get_optional_int(in, vsapi, "debug", ...)
   |
   |- get_optional_float(in, vsapi, "scdthr", ...)
   |  |
   |  |- vsapi->mapGetFloat(...)
   |
   |- scale_8bit_parameter_to_bit_depth(local.ln, bits_per_sample)
   |- scale_8bit_parameter_to_bit_depth(local.lm, bits_per_sample)
   |- scale_8bit_parameter_to_bit_depth(local.un, bits_per_sample)
   |- scale_8bit_parameter_to_bit_depth(local.um, bits_per_sample)
   |- scale_8bit_parameter_to_bit_depth(local.vn, bits_per_sample)
   |- scale_8bit_parameter_to_bit_depth(local.vm, bits_per_sample)
   |
   |- calculate vscnr2-style scene_change_threshold directly
   |  |
   |  |- uses scdthr
   |  |- uses frame width and height
   |  |- uses bit depth
   |  |- uses subSamplingW + subSamplingH
   |  |- uses scene_chroma
   |
   |- build_cnr3_lookup_tables(local, out, vsapi)
   |  |
   |  |- build_cnr3_weight_table(table_y, ...)
   |  |  |
   |  |  |- clamp_int(...)
   |  |
   |  |- build_cnr3_weight_table(table_u, ...)
   |  |  |
   |  |  |- clamp_int(...)
   |  |
   |  |- build_cnr3_weight_table(table_v, ...)
   |     |
   |     |- clamp_int(...)
   |
   |- optional startup debug diagnostics
   |  |
   |  |- cnr3_debug_printf(...)
   |  |  |
   |  |  |- cnr3_vfprintf_stderr(...)
   |  |
   |  |- get_cnr3_table_value_for_signed_diff(...) for table sample reporting
   |
   |- new Cnr3Data(local)
   |
   |- vsapi->createVideoFilter(...)
      |
      |- records frame callback:
      |     cnr3_get_frame
      |
      |- records free callback:
      |     cnr3_free
      |
      |- records filter mode:
            fmUnordered
```

### A.3 Frame-request cascade from VapourSynth into CNR3

VapourSynth calls the CNR3 frame callback in activation phases. The current implementation uses strict streaming and expects tests to run with:

```text
vspipe -r 1
```

Representative cascade:

```text
VapourSynth asks CNR3 for output frame n
|
|- cnr3_get_frame(n, activationReason, instanceData, frameData, frameCtx, core, vsapi)
   |
   |- arInitial
   |  |
   |  |- strict-streaming check against d->cache.next_needed
   |  |
   |  |- on out-of-order request:
   |  |  |
   |  |  |- cnr3_debug_print_cache_state(...)
   |  |  |  |
   |  |  |  |- cnr3_debug_printf(...)
   |  |  |     |
   |  |  |     |- cnr3_vfprintf_stderr(...)
   |  |  |
   |  |  |- vsapi->setFilterError(...)
   |  |
   |  |- on accepted request:
   |     |
   |     |- vsapi->requestFrameFilter(n, d->node, frameCtx)
   |
   |- arAllFramesReady
   |  |
   |  |- vsapi->getFrameFilter(n, d->node, frameCtx)
   |  |
   |  |- vsapi->newVideoFrame(...)
   |  |
   |  |- process_cnr3_frame(d, n, src, dst, frameCtx, vsapi)
   |  |  |
   |  |  |- see Appendix A.4
   |  |
   |  |- on success:
   |  |  |
   |  |  |- cnr3_cache_store_output_frame(d, dst, vsapi)
   |  |  |  |
   |  |  |  |- if old d->cache.prev_output exists:
   |  |  |  |     vsapi->freeFrame(old_prev_output)
   |  |  |  |
   |  |  |  |- vsapi->addFrameRef(dst)
   |  |  |  |- d->cache.prev_output = retained reference to dst
   |  |  |  |- d->cache.next_needed = n + 1
   |  |  |
   |  |  |- vsapi->freeFrame(src)
   |  |  |- return dst
   |  |
   |  |- on failure:
   |     |
   |     |- vsapi->freeFrame(src)
   |     |- vsapi->freeFrame(dst)
   |     |- return nullptr
   |
   |- arError / close paths as applicable
      |
      |- VapourSynth later calls cnr3_free when the filter instance is destroyed
```

### A.4 Frame processing cascade

```text
process_cnr3_frame(d, frame_number, src, dst, frameCtx, vsapi)
|
|- validate d/src/dst/frame_number
|  |
|  |- on error:
|       vsapi->setFilterError(...)
|
|- prev_output = d->cache.prev_output
|
|- for frame_number > 0:
|  |
|  |- require prev_output != nullptr
|  |
|  |- on missing previous output:
|       vsapi->setFilterError(...)
|
|- bytes_per_sample = (bits_per_sample + 7) / 8
|
|- copy_plane_bytes(src, dst, plane=0, bytes_per_sample, vsapi)
|  |
|  |- copies Y/luma unchanged
|
|- get U/V plane dimensions using vsapi->getFrameWidth/getFrameHeight
|
|- verify U and V plane dimensions match
|  |
|  |- on mismatch:
|       vsapi->setFilterError(...)
|
|- build_cnr3_downsampled_luma_buffer(d, src, chroma_width, chroma_height, bytes_per_sample, current_luma, vsapi)
|  |
|  |- if bytes_per_sample == 1:
|  |  |
|  |  |- build_cnr3_downsampled_luma_buffer_u8(...)
|  |     |
|  |     |- vsapi->getReadPtr(frame, plane=0)
|  |     |- vsapi->getStride(frame, plane=0)
|  |     |- vsapi->getFrameWidth(frame, plane=0)
|  |     |- vsapi->getFrameHeight(frame, plane=0)
|  |     |- clamp_int(...) for edge reads
|  |
|  |- if bytes_per_sample == 2:
|     |
|     |- build_cnr3_downsampled_luma_buffer_u16(...)
|        |
|        |- vsapi->getReadPtr(frame, plane=0)
|        |- vsapi->getStride(frame, plane=0)
|        |- vsapi->getFrameWidth(frame, plane=0)
|        |- vsapi->getFrameHeight(frame, plane=0)
|        |- clamp_int(...) for edge reads
|
|- if frame_number > 0:
|  |
|  |- build_cnr3_downsampled_luma_buffer(d, prev_output, ..., previous_luma, vsapi)
|
|- if frame_number > 0:
|  |
|  |- detect_cnr3_scene_change(d, src, prev_output, chroma_width, chroma_height, bytes_per_sample, current_luma, previous_luma, vsapi)
|  |  |
|  |  |- if bytes_per_sample == 1:
|  |  |  |
|  |  |  |- detect_cnr3_scene_change_u8(...)
|  |  |     |
|  |  |     |- vsapi->getReadPtr(src, U/V)
|  |  |     |- vsapi->getReadPtr(prev_output, U/V)
|  |  |     |- vsapi->getStride(src, U/V)
|  |  |     |- vsapi->getStride(prev_output, U/V)
|  |  |     |- accumulate vscnr2-style diff_total
|  |  |     |- early exit when diff_total > scene_change_threshold
|  |  |
|  |  |- if bytes_per_sample == 2:
|  |     |
|  |     |- detect_cnr3_scene_change_u16(...)
|  |        |
|  |        |- same metric using uint16_t sample reads
|  |
|  |- cnr3_print_scene_change_debug_stats(d, frame_number, scene_stats)
|  |  |
|  |  |- prints only scene-change or near-threshold frames after debug reduction
|  |  |- cnr3_debug_printf(...)
|  |     |
|  |     |- cnr3_vfprintf_stderr(...)
|  |
|  |- if scene_stats.scene_change:
|     |
|     |- cnr3_debug_printf(... scene change detected ...)
|     |- copy_plane_bytes(src, dst, plane=1, bytes_per_sample, vsapi)
|     |- copy_plane_bytes(src, dst, plane=2, bytes_per_sample, vsapi)
|     |- return true
|
|- process_cnr3_chroma_plane(d, frame_number, plane=1, src, prev_output, dst, shared_chroma_width, shared_chroma_height, bytes_per_sample, current_luma, previous_luma, vsapi)
|  |
|  |- see Appendix A.5
|
|- process_cnr3_chroma_plane(d, frame_number, plane=2, src, prev_output, dst, shared_chroma_width, shared_chroma_height, bytes_per_sample, current_luma, previous_luma, vsapi)
   |
   |- see Appendix A.5
```

### A.5 Chroma-plane processing cascade

```text
process_cnr3_chroma_plane(... plane=1 or plane=2 ...)
|
|- validate d/src/dst/vsapi
|- validate plane is U or V
|- if frame_number > 0, require prev_output
|- get plane_width/plane_height from VapourSynth
|- verify plane dimensions match shared chroma dimensions
|- verify current_luma size matches expected chroma sample count
|- if frame_number > 0, verify previous_luma size matches expected count
|
|- if bytes_per_sample == 1:
|  |
|  |- process_cnr3_chroma_plane_u8(...)
|     |
|     |- vsapi->getReadPtr(src, plane)
|     |- vsapi->getWritePtr(dst, plane)
|     |- if prev_output is available:
|     |  |
|     |  |- vsapi->getReadPtr(prev_output, plane)
|     |
|     |- vsapi->getStride(src, plane)
|     |- vsapi->getStride(dst, plane)
|     |- vsapi->getStride(prev_output, plane)
|     |- vsapi->getFrameWidth(src, plane)
|     |- vsapi->getFrameHeight(src, plane)
|     |
|     |- cnr3_get_table_for_chroma_plane(d, plane)
|     |
|     |- calculate_cnr3_max_possible_blend_weight(d, chroma_table)
|     |  |
|     |  |- get_cnr3_table_value_for_signed_diff(d->table_y, ..., 0)
|     |  |- get_cnr3_table_value_for_signed_diff(chroma_table, ..., 0)
|     |  |- calculate_cnr3_combined_blend_weight(...)
|     |
|     |- for each chroma sample:
|        |
|        |- current_chroma = source U/V sample
|        |- previous_chroma = previous filtered output U/V sample when available
|        |- chroma_signed_diff = current_chroma - previous_chroma
|        |- chroma_response = get_cnr3_table_value_for_signed_diff(chroma_table, ...)
|        |- y_signed_diff = current_luma[index] - previous_luma[index]
|        |- y_response = get_cnr3_table_value_for_signed_diff(d->table_y, ...)
|        |- cnr3_update_response_debug_stats(...)
|        |
|        |- if d->blend and previous output exists:
|        |  |
|        |  |- blend_weight = calculate_cnr3_combined_blend_weight(y_response, chroma_response)
|        |  |- cnr3_update_blend_debug_stats(...)
|        |  |- blend_cnr3_chroma_sample(current_chroma, previous_chroma, y_response, chroma_response, bits_per_sample)
|        |  |  |
|        |  |  |- get_cnr3_blend_scale(bits_per_sample)
|        |  |  |- calculate_cnr3_combined_blend_weight(y_response, chroma_response)
|        |  |  |- apply vscnr2-style weighted recursive blend formula
|        |  |
|        |  |- clamp_int(blended_chroma, 0, sample_peak)
|        |  |- write blended U/V sample
|        |
|        |- else:
|           |
|           |- write current source U/V sample
|
|- if bytes_per_sample == 2:
   |
   |- process_cnr3_chroma_plane_u16(...)
      |
      |- same structure as u8 path, but source, destination, and previous rows
         are interpreted as uint16_t samples
```

After debug reduction, the per-plane response/blend printing calls are normally commented out, but the diagnostic helper functions remain useful for future tuning:

```text
cnr3_print_response_debug_stats(...)
cnr3_print_blend_debug_stats(...)
```

### A.6 Cache and cleanup cascade

```text
cnr3_cache_store_output_frame(d, output_frame, vsapi)
|
|- if d->cache.prev_output != nullptr:
|  |
|  |- vsapi->freeFrame(d->cache.prev_output)
|
|- d->cache.prev_output = vsapi->addFrameRef(output_frame)
|- d->cache.next_needed += 1 or is set to the next expected frame by caller logic
```

```text
VapourSynth destroys CNR3 filter instance
|
|- cnr3_free(instanceData, core, vsapi)
   |
   |- cnr3_cache_clear(d, vsapi)
   |  |
   |  |- if d->cache.prev_output != nullptr:
   |     |
   |     |- vsapi->freeFrame(d->cache.prev_output)
   |
   |- vsapi->freeNode(d->node)
   |- delete d
```

---

## Appendix B - Reverse list: CNR3 functions that directly call VapourSynth API methods

This section lists only direct calls from CNR3 code into VapourSynth's API objects. It deliberately does not expand what VapourSynth itself does internally.

### B.1 Plugin API calls through `vspapi`

```text
VapourSynthPluginInit2(...)
|
|- vspapi->configPlugin(...)
|- vspapi->registerFunction(...)
```

### B.2 General API calls through `vsapi`

```text
cnr3_create(...)
|
|- vsapi->mapGetNode(in, "clip", ...)
|- vsapi->getVideoInfo(local.node)
|- vsapi->mapSetError(out, ...) on create/validation failures
|- vsapi->freeNode(local.node) on create failure after node acquisition
|- vsapi->createVideoFilter(..., cnr3_get_frame, cnr3_free, fmUnordered, ...)
```

```text
get_optional_int(...)
|
|- vsapi->mapGetInt(...)
```

```text
get_optional_float(...)
|
|- vsapi->mapGetFloat(...)
```

```text
get_optional_data_string(...)
|
|- vsapi->mapGetData(...)
```

```text
validate_cnr3_format(...)
|
|- vsapi->mapSetError(out, ...)
```

```text
build_cnr3_lookup_tables(...)
|
|- vsapi->mapSetError(out, ...) on internal table/option errors
```

```text
cnr3_get_frame(...)
|
|- vsapi->requestFrameFilter(n, d->node, frameCtx)
|- vsapi->setFilterError(...)
|- vsapi->getFrameFilter(n, d->node, frameCtx)
|- vsapi->newVideoFrame(...)
|- vsapi->freeFrame(src)
|- vsapi->freeFrame(dst) on failure
```

```text
process_cnr3_frame(...)
|
|- vsapi->setFilterError(...)
|- vsapi->getFrameWidth(src, 1/2)
|- vsapi->getFrameHeight(src, 1/2)
```

```text
copy_plane_bytes(...)
|
|- vsapi->getReadPtr(src, plane)
|- vsapi->getWritePtr(dst, plane)
|- vsapi->getStride(src, plane)
|- vsapi->getStride(dst, plane)
|- vsapi->getFrameWidth(src, plane)
|- vsapi->getFrameHeight(src, plane)
```

```text
build_cnr3_downsampled_luma_buffer_u8(...)
build_cnr3_downsampled_luma_buffer_u16(...)
|
|- vsapi->getReadPtr(frame, 0)
|- vsapi->getStride(frame, 0)
|- vsapi->getFrameWidth(frame, 0)
|- vsapi->getFrameHeight(frame, 0)
```

```text
detect_cnr3_scene_change_u8(...)
detect_cnr3_scene_change_u16(...)
|
|- vsapi->getReadPtr(src, 1)
|- vsapi->getReadPtr(src, 2)
|- vsapi->getReadPtr(prev_output, 1)
|- vsapi->getReadPtr(prev_output, 2)
|- vsapi->getStride(src, 1)
|- vsapi->getStride(src, 2)
|- vsapi->getStride(prev_output, 1)
|- vsapi->getStride(prev_output, 2)
```

```text
process_cnr3_chroma_plane(...)
|
|- vsapi->getFrameWidth(src, plane)
|- vsapi->getFrameHeight(src, plane)
```

```text
process_cnr3_chroma_plane_u8(...)
process_cnr3_chroma_plane_u16(...)
|
|- vsapi->getReadPtr(src, plane)
|- vsapi->getWritePtr(dst, plane)
|- vsapi->getReadPtr(prev_output, plane) when previous output exists
|- vsapi->getStride(src, plane)
|- vsapi->getStride(dst, plane)
|- vsapi->getStride(prev_output, plane) when previous output exists
|- vsapi->getFrameWidth(src, plane)
|- vsapi->getFrameHeight(src, plane)
```

```text
cnr3_cache_store_output_frame(...)
|
|- vsapi->freeFrame(old_prev_output)
|- vsapi->addFrameRef(output_frame)
```

```text
cnr3_cache_clear(...)
|
|- vsapi->freeFrame(d->cache.prev_output)
```

```text
cnr3_free(...)
|
|- vsapi->freeNode(d->node)
```

### B.3 Why this reverse list matters

This reverse list is useful before file splitting because functions that call VapourSynth directly usually need one or more of these types in their declaration or implementation:

```text
VSAPI
VSFrame
VSFrameContext
VSNode
VSVideoInfo
VSMap
VSCore
VSPlugin
VSPLUGINAPI
```

Functions that do not call VapourSynth directly are better candidates for early low-risk extraction into support modules, for example:

```text
scale_8bit_parameter_to_bit_depth
clamp_int
get_cnr3_table_value_for_signed_diff
get_cnr3_blend_scale
calculate_cnr3_combined_blend_weight
calculate_cnr3_max_possible_blend_weight
blend_cnr3_chroma_sample
build_cnr3_weight_table
```

However, even pure helpers should not be split until their surrounding ownership and header boundaries are clear.

