## Description

vapoursynth-cnr3 is an experimental VapourSynth API4 chroma denoiser derived from the
Cnr2 family of filters.

It is specifically targetted at vapoursynth scripts for "fire and forget"
chroma cleanup and conversion of noisy VHS/VHS-C analogue capture files.
According to the original author, this filter is suited for stationary rainbows or noisy analog captures.

Due to the way it works, vapoursynth-cnr3 is forced to run in a single thread.
Cnr3 will also bottleneck the entire script, preventing it from using all the available CPU cores
and crushing frame random access speed. For serial VHS denoising conversions "fire and forget"
this may not be an issue.

A possible way to work around the serial issue is splitting the video into
chunks at scene changes, and filtering them in parallel with two or three
instances of vspipe. Or not.

The initial implementation intentionally preserves the recursive 
temporal model, where each output frame depends on the previous filtered output
frame rather than the previous frame itself. This is expected to be force serial
and that issue is accepted as a quality-first design choice for a VHS/VHS-C 
analogue chroma cleanup and conversion.

This project is distributed under the GNU Affero General Public License v3.0 or later,
being compatible with GPL-2.0-or-later.

This is [a port/upgrade of the avisynth plugin vsCnr2](https://github.com/Asd-g/AviSynth-vsCnr2) which is
itself be [ported from the VapourSynth plugin Cnr2](https://github.com/dubhater/vapoursynth-cnr2),
and appears to be more recently updated version.

### Requirements:

- Vapoursynth R76+ with python 3.14+ (possibly portable versions).

- Microsoft VisualC++ Redistributable Package 2026.

### Usage:

```
Cnr3 (clip input, string "mode", float "scdthr", int "ln", int "lm", int "un", int "um", int "vn", int "vm", bool "sceneChroma")
```

### Parameters:

- input<br>
    A clip to process.<br>
    It must be in YUV 8..16-bit planar format with chroma subsampling 420, 422, 440 or 444.

- mode<br>
    Mode for each plane.<br>
    The letter `o` means wide mode, which is less sensitive to changes in the pixels, and more effective.<br>
    The letter `x` means narrow mode, which is less effective.<br>
    Default: "oxx".

- scdthr<br>
    Scene change detection threshold as percentage of maximum possible change.<br>
    Lower values make it more sensitive.<br>
    Must be between 0.0 and 100.0.<br>
    Default: 10.0.

- ln, un, vn<br>
    Sensitivity to movement in the Y, U, and V planes, respectively.<br>
    Higher values will denoise more, at the risk of introducing ghosting in the chroma.<br>
    Must be between 0 and 255.<br>
    Default: ln = 35; un = vn = 47.

- lm, um, vm<br>
    Strength of the denoising.<br>
    Higher values will denoise harder.<br>
    Must be between 0 and 255.<br>
    Default: lm = 192; um = vm = 255.

- sceneChroma<br>
    If True, the chroma is considered in the scene change detection.<br>
    Default: False.
