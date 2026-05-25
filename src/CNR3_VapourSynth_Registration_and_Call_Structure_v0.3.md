# CNR3 VapourSynth Registration and Call Structure

Date: 2026-05-25  
Project: vapoursynth-cnr3  
Purpose: explain how VapourSynth loads, registers, creates, calls, and frees the CNR3 plugin, and how per-instance state relates to worker threads.

---

## 1. Practical mental model

The most important distinction is this:

```text
CNR3 instance:
    owns one input source node
    owns that source stream's options
    owns that source stream's lookup tables
    owns that source stream's recursive cache

Worker thread:
    may execute work for any CNR3 instance
    receives instanceData for the specific request
    should use only that instance's cache/state for that frame
```

So the cache belongs with `Cnr3Data`, not with a global plugin object and not with an OS or VapourSynth worker thread.

The future cache-manager problem is not:

```text
How do we share cache across instances?
```

It is:

```text
How do we safely allow multiple worker threads to request frames from the same
instance without corrupting that instance's recursive state?
```

For now, `vspipe -r 1` sidesteps that scheduling/concurrency problem while the core Cnr2-style algorithm is being developed.

---

## 2. DLL load and plugin registration

When VapourSynth starts, or when it autoloads plugins from its plugin directory, it loads `cnr3.dll` into the process.

The DLL exposes this API4 plugin entry point:

```cpp
VS_EXTERNAL_API(void) VapourSynthPluginInit2(
    VSPlugin *plugin,
    const VSPLUGINAPI *vspapi
)
```

VapourSynth calls this function once when it loads the DLL.

### Registration-time call structure

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

Example intended neutral plugin identifier:

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

The Python namespace comes from:
    "cnr3"

The Python function name comes from:
    "CNR3"

Therefore this remains unchanged:
    core.cnr3.CNR3(...)
```

---

## 3. Function registration

Inside `VapourSynthPluginInit2(...)`, CNR3 registers the callable filter function:

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
    "debug:int:opt;",
    "clip:vnode;",
    cnr3_create,
    nullptr,
    plugin
);
```

This tells VapourSynth:

```text
public function name:
    CNR3

full Python name:
    core.cnr3.CNR3

input arguments:
    clip
    mode
    ln/lm
    un/um
    vn/vm
    scdthr
    scene_chroma
    debug

return value:
    clip

creation callback:
    cnr3_create
```

After this registration, the following Python call is valid:

```python
clip_denoised = core.cnr3.CNR3(clip, debug=True)
```

---

## 4. Script/create time

When a VapourSynth script calls:

```python
clip_denoised = core.cnr3.CNR3(clip, debug=True)
```

VapourSynth does not immediately process all frames.

Instead, it calls the CNR3 creation callback:

```cpp
static void VS_CC cnr3_create(...)
```

That function:

```text
- reads the input clip node
- reads video info
- validates format/dimensions
- reads user parameters
- scales thresholds to bit depth
- builds lookup tables
- assigns instance_id
- allocates a Cnr3Data object
- registers a new output filter node
```

The important object is:

```cpp
Cnr3Data *data = new Cnr3Data(local);
```

That `Cnr3Data` is the per-filter-instance state:

```text
one input source node
one set of options
one set of lookup tables
one recursive cache
one instance_id
```

### Script/create-time call structure

```text
Python script calls:
    clip2 = core.cnr3.CNR3(clip, ...)
|
|- VapourSynth calls cnr3_create(...)
   |
   |- mapGetNode(in, "clip", ...)
   |     |
   |     |- stores input source node in local.node
   |
   |- getVideoInfo(local.node)
   |     |
   |     |- stores video info in local.vi
   |
   |- validate_cnr3_format(local.vi, out, vsapi)
   |
   |- get_optional_data_string(...)
   |- get_optional_int(...)
   |- get_optional_float(...)
   |
   |- scale_8bit_parameter_to_bit_depth(...)
   |
   |- build_cnr3_lookup_tables(local, out, vsapi)
   |     |
   |     |- build_cnr3_weight_table(local.table_y, ...)
   |     |- build_cnr3_weight_table(local.table_u, ...)
   |     |- build_cnr3_weight_table(local.table_v, ...)
   |
   |- new Cnr3Data(local)
   |     |
   |     |- creates one per-call / per-source CNR3 instance
   |     |- owns:
   |          input node
   |          video info pointer
   |          options
   |          lookup tables
   |          cache
   |          instance_id
   |
   |- vsapi->createVideoFilter(...)
         |
         |- creates output filter node
         |- frame callback = cnr3_get_frame
         |- free callback  = cnr3_free
         |- instanceData   = data
```

---

## 5. Output filter node creation

At the end of `cnr3_create(...)`, CNR3 calls:

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

This gives VapourSynth:

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

The `data` pointer is what later comes back to CNR3 as `instanceData`.

---

## 6. Frame request time

When something downstream asks for output frame `n`, VapourSynth calls:

```cpp
cnr3_get_frame(
    n,
    activationReason,
    instanceData,
    frameData,
    frameCtx,
    core,
    vsapi
)
```

Inside that function, CNR3 does:

```cpp
Cnr3Data *d = static_cast<Cnr3Data *>(instanceData);
```

Now CNR3 knows exactly which filter instance/source stream this frame request belongs to.

That is the key link:

```text
VapourSynth passes instanceData back to CNR3.
CNR3 casts it to Cnr3Data *.
CNR3 uses d->node and d->cache for that source stream.
```

---

## 7. Frame callback phase 1: arInitial

When called with `arInitial`, CNR3 asks VapourSynth for the matching source frame:

```cpp
vsapi->requestFrameFilter(n, d->node, frameCtx);
return nullptr;
```

Meaning:

```text
I need source frame n from my input node before I can produce output frame n.
```

### arInitial call structure

```text
Downstream asks VapourSynth for output frame n
|
|- VapourSynth calls cnr3_get_frame(n, arInitial, instanceData, ...)
   |
   |- Cnr3Data *d = static_cast<Cnr3Data *>(instanceData)
   |
   |- vsapi->requestFrameFilter(n, d->node, frameCtx)
   |     |
   |     |- asks the source node for frame n
   |
   |- return nullptr
```

---

## 8. Frame callback phase 2: arAllFramesReady

When the requested source frame is ready, VapourSynth calls CNR3 again with `arAllFramesReady`.

CNR3 then:

```text
- gets source frame n
- checks strict recursive order
- creates destination frame
- copies/processes Y/U/V
- stores output frame as prev_output
- frees source frame
- returns destination frame
```

### arAllFramesReady call structure

```text
When source frame n is ready
|
|- VapourSynth calls cnr3_get_frame(n, arAllFramesReady, instanceData, ...)
   |
   |- Cnr3Data *d = static_cast<Cnr3Data *>(instanceData)
   |
   |- src = vsapi->getFrameFilter(n, d->node, frameCtx)
   |
   |- check strict recursive order:
   |     |
   |     |- if n != d->cache.next_needed:
   |          setFilterError(...)
   |          return nullptr
   |
   |- dst = vsapi->newVideoFrame(...)
   |
   |- process_cnr3_frame_passthrough_for_now(d, n, src, dst, ...)
   |     |
   |     |- checks d/src/dst/frame_number
   |     |
   |     |- prev_output = d->cache.prev_output
   |     |
   |     |- if frame 0:
   |     |     initial-copy path
   |     |
   |     |- if frame > 0:
   |     |     require prev_output != nullptr
   |     |
   |     |- copy_plane_bytes(src, dst, plane 0)
   |     |     |
   |     |     |- copies Y unchanged
   |     |
   |     |- process_cnr3_chroma_plane(... plane 1 ...)
   |     |     |
   |     |     |- processes U plane
   |     |
   |     |- process_cnr3_chroma_plane(... plane 2 ...)
   |           |
   |           |- processes V plane
   |
   |- cnr3_cache_store_output_frame(d->cache, dst, n, vsapi)
   |     |
   |     |- free old cache.prev_output if present
   |     |- cache.prev_output = vsapi->addFrameRef(dst)
   |     |- cache.next_needed = n + 1
   |
   |- vsapi->freeFrame(src)
   |
   |- return dst
```

---

## 9. Chroma processing structure

Current/near-current chroma path:

```text
process_cnr3_chroma_plane(d, frame_number, plane, src, prev_output, dst, ...)
|
|- validate:
|     d/src/dst/vsapi not null
|     plane is 1 or 2
|     frame > 0 requires prev_output
|
|- bytes_per_sample = (d->bits_per_sample + 7) / 8
|
|- if bytes_per_sample == 1:
|     process_cnr3_chroma_plane_passthrough_u8(...)
|
|- if bytes_per_sample == 2:
|     process_cnr3_chroma_plane_passthrough_u16(...)
|
|- return true/false
```

The current diagnostic scaffold U8/U16 loops conceptually do this:

```text
process_cnr3_chroma_plane_passthrough_u8/u16(...)
|
|- read current source chroma plane
|- read destination chroma plane
|- if frame_number > 0:
|     read previous output chroma plane
|     read current source luma plane
|     read previous output luma plane
|
|- for each chroma row
|     |
|     |- map chroma y to representative luma y
|     |
|     |- for each chroma sample x
|          |
|          |- current_chroma = src chroma sample
|          |
|          |- if previous output exists:
|          |     previous_chroma = prev_output chroma sample
|          |     chroma_signed_diff = current_chroma - previous_chroma
|          |     chroma_response = table_u/table_v lookup
|          |
|          |     map chroma x to representative luma x
|          |     current_luma = src luma sample
|          |     previous_luma = prev_output luma sample
|          |     y_signed_diff = current_luma - previous_luma
|          |     y_response = table_y lookup
|          |
|          |     update response debug stats
|          |
|          |- dst chroma sample = current_chroma
|              |
|              |- still pass-through at this stage
|
|- optionally print compact response stats
```

Important:

```text
The current diagnostic scaffold still writes current source chroma unchanged.
Real blending is not connected yet.
```

---

## 10. Cleanup time

When VapourSynth no longer needs the output filter node, it calls:

```cpp
static void VS_CC cnr3_free(...)
```

### Cleanup call structure

```text
VapourSynth no longer needs the output filter node
|
|- VapourSynth calls cnr3_free(instanceData, ...)
   |
   |- Cnr3Data *d = static_cast<Cnr3Data *>(instanceData)
   |
   |- if d->node != nullptr:
   |     vsapi->freeNode(d->node)
   |
   |- cnr3_cache_clear(d->cache, vsapi)
   |     |
   |     |- if cache.prev_output != nullptr:
   |          vsapi->freeFrame(cache.prev_output)
   |     |- cache.next_needed = 0
   |
   |- delete d
```

---

## 11. Multiple CNR3 calls and multiple sources

This script:

```python
first_denoised  = core.cnr3.CNR3(first, debug=True)
second_denoised = core.cnr3.CNR3(second, debug=True)
```

creates two independent `Cnr3Data` objects:

```text
instance 1:
    input node = first
    cache = first stream history

instance 2:
    input node = second
    cache = second stream history
```

The same DLL code handles both, but `instanceData` tells CNR3 which state object to use on each call.

A worker thread can process frames from either source at different times, but the thread receives the `instanceData` for the frame request it is currently executing.

Therefore:

```text
A CNR3 instance is associated with one source stream.
A worker thread is just an executor.
A worker thread can work on any instance.
The instanceData pointer identifies the correct CNR3 instance.
The recursive cache must stay with that instance/source stream.
```

---

## 12. Compact full call diagram

```text
VapourSynth
|
|- loads cnr3.dll
|  |
|  |- VapourSynthPluginInit2
|     |
|     |- configPlugin
|     |- registerFunction
|        |
|        |- binds core.cnr3.CNR3 -> cnr3_create
|
|- script calls core.cnr3.CNR3(...)
|  |
|  |- cnr3_create
|     |
|     |- validate_cnr3_format
|     |- get_optional_data_string
|     |- get_optional_int
|     |- get_optional_float
|     |- scale_8bit_parameter_to_bit_depth
|     |- build_cnr3_lookup_tables
|     |  |
|     |  |- build_cnr3_weight_table
|     |  |- build_cnr3_weight_table
|     |  |- build_cnr3_weight_table
|     |
|     |- createVideoFilter
|        |
|        |- registers cnr3_get_frame as frame callback
|        |- registers cnr3_free as free callback
|        |- stores Cnr3Data as instanceData
|
|- downstream requests frame n
|  |
|  |- cnr3_get_frame arInitial
|  |  |
|  |  |- requestFrameFilter(n, source node)
|  |
|  |- cnr3_get_frame arAllFramesReady
|     |
|     |- getFrameFilter(n, source node)
|     |- newVideoFrame
|     |- process_cnr3_frame_passthrough_for_now
|     |  |
|     |  |- copy_plane_bytes for Y
|     |  |- process_cnr3_chroma_plane for U
|     |  |  |
|     |  |  |- process_cnr3_chroma_plane_passthrough_u8
|     |  |  |  |
|     |  |  |  |- cnr3_get_chroma_to_luma_x
|     |  |  |  |- cnr3_get_chroma_to_luma_y
|     |  |  |  |- cnr3_get_table_for_chroma_plane
|     |  |  |  |- get_cnr3_table_value_for_signed_diff
|     |  |  |  |- cnr3_update_response_debug_stats
|     |  |  |  |- cnr3_print_response_debug_stats
|     |  |  |
|     |  |  |- process_cnr3_chroma_plane_passthrough_u16
|     |  |     |
|     |  |     |- cnr3_get_chroma_to_luma_x
|     |  |     |- cnr3_get_chroma_to_luma_y
|     |  |     |- cnr3_get_table_for_chroma_plane
|     |  |     |- get_cnr3_table_value_for_signed_diff
|     |  |     |- cnr3_update_response_debug_stats
|     |  |     |- cnr3_print_response_debug_stats
|     |  |
|     |  |- process_cnr3_chroma_plane for V
|     |     |
|     |     |- same U/V helper path as above
|     |
|     |- cnr3_cache_store_output_frame
|     |- freeFrame(src)
|     |- return dst
|
|- output node destroyed
   |
   |- cnr3_free
      |
      |- freeNode(d->node)
      |- cnr3_cache_clear
      |  |
      |  |- freeFrame(cache.prev_output)
      |
      |- delete d
```

---

## 13. Key takeaway

```text
VapourSynth owns scheduling.
CNR3 owns per-instance state.

The same compiled CNR3 functions serve every source stream, but the
instanceData pointer selects the correct Cnr3Data, cache, options, tables,
and source node for the frame currently being processed.
```
