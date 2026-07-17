<h1 align="center">
CNR3

![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-lightgrey)
![License](https://img.shields.io/badge/license-GPL--2.0-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B-00599C?logo=c%2B%2B&logoColor=white)
![Status: Experimental](https://img.shields.io/badge/status-Under%20Development-ffcc00)
</h1>

<!--
![Status](https://img.shields.io/badge/status-stable-green)
![Status](https://img.shields.io/badge/status-Initial%20Release-green)
![Status](https://img.shields.io/badge/status-Under%20Development-orange)

Common Statuses
![Status: Active](https://img.shields.io/badge/status-active-brightgreen)
![Status: Beta](https://img.shields.io/badge/status-beta-blue)
![Status: Experimental](https://img.shields.io/badge/status-experimental-orange)
![Status: Deprecated](https://img.shields.io/badge/status-deprecated-red)
![Status: Inactive](https://img.shields.io/badge/status-inactive-lightgrey)
![Status](https://img.shields.io/badge/status-Under%20Development-orange) 
![Status](https://img.shields.io/badge/status-Initial%20Release-green)


![License](https://img.shields.io/badge/license-GPL--2.0-blue)
![License](https://img.shields.io/badge/license-AGPL--3.0-green)


Common status labels 
active, maintained, stable
alpha, beta, experimental
deprecated, legacy, archived, inactive

Typical named colors
Greens: brightgreen, green, yellowgreen
Yellows/Oranges: yellow, orange
Reds: red, crimson, firebrick
Blues/Purples: blue, navy, blueviolet
Neutrals: lightgrey, grey/gray, black

Semantic: 
success (brightgreen), informational (blue), critical (red), inactive (lightgrey), important (orange) 

How to craft your own
https://img.shields.io/badge/<LABEL>-<MESSAGE>-<COLOR>
Replace <LABEL>, <MESSAGE>, and <COLOR> with whatever text and named color you like. (Spaces become %20)
-->


## Description

Targetted for use on noisy VHS/VHS-C analogue capture files, CNR3 is a temporal denoiser
designed to denoise only the chroma, and is derived from the Cnr2 family of filters.

According to the original author, this filter is suited for stationary rainbows or noisy analog captures.

The venerable old CNR2 relied on the older VapourSynth APIv3 which has been phased out,
and it additionally depended on a mode yielding SERIAL (in-number-order) arrival of frame requests which is
strongly recommended against using under VapourSynth R76+.  CNR2 implemented recursive temporal model,
where each output frame depends on the previous (filtered) output frame to be used for chroma blending instead
of the previous source frame.

Hence CNR3 uses VapourSynth supported APIv4 and supported mode fmParallelRequests, and implements
a small output-frame cache to deal with out of order frame requests.

This project is distributed under the License GNU GENERAL PUBLIC LICENSE Version 2 or later (GPL-2.0-or-later).

This is [a port/upgrade of the avisynth plugin vsCnr2](https://github.com/Asd-g/AviSynth-vsCnr2) which is
itself be [ported from the VapourSynth plugin Cnr2](https://github.com/dubhater/vapoursynth-cnr2),
and appears to be more recently updated version of CNR2.

## Requirements

- Vapoursynth R76+ with python 3.14+ (possibly portable versions).
- Microsoft VisualC++ Redistributable Package 2026+.

## Usage

### Usage with  with Progressive input material

```
Cnr3 (clip input, string "mode", float "scdthr", int "ln", int "lm", int "un", int "um", int "vn", int "vm", bool "sceneChroma")
```

### Usage with  with Interlaced input material

... describe use with Interlaced material (and provide a condensed .vpy showing only the interlaced
detection and use code and the call(s) to cnr3) and being very careful to flag that some video files (eg .avi)
do not contain enough metadata to discern interlaced material or TFF (Top Field First) or BFF (Bottom Field First)
and that such material must be treated manually (separating fields, weaving, etc. (unless you can upate the vpy's
subroutines to accept parameters (defaulting to autodetect) rather than just autodetect which they now do.

### CNR3 Parameters

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

### Examples of use

... insert 2 examples of use with progressive material here

... insert 2 examples of use with intelraced material and the vpy code above mentioned above (one auto-detect, one manually-specifying)

## Technical Info for Nerds

... briefly describe the algorithm and why SERIAL is needed

... briefly describe vapoursynth modes and why we chose fmParallelRequests (tested cache reliability and performance compared to other modes)

... briefly describe

... relatively briefly describe the cache and its mechanisms and how they operate together, eg at least each of 
cache and size , rolling wavefront for normal (non-jumping) transcodes in 99% of use cases,
checkpoints, hot zones (aimed at jumping scenarios), prunes, pins, bias delay method and reason for it ...

... describe tested FPS performance with PAL SD (with debug ON) for the chosen Release mechanisms (fmParallelRequest, bias OFF etc).

