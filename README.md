<h1 align="center">
CNR3

![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-lightgrey)
![License](https://img.shields.io/badge/license-GPL--2.0-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B-00599C?logo=c%2B%2B&logoColor=white)
![Status: Released](https://img.shields.io/badge/status-Released-brightgreen)
</h1>    

<!--
Common Statuses
![Status: Active](https://img.shields.io/badge/status-active-brightgreen)
![Status: Active](https://img.shields.io/badge/status-active-brightgreen)
![Status: Beta](https://img.shields.io/badge/status-beta-blue)
![Status: Experimental](https://img.shields.io/badge/status-experimental-orange)
![Status: Deprecated](https://img.shields.io/badge/status-deprecated-red)
![Status: Inactive](https://img.shields.io/badge/status-inactive-lightgrey)
![Status](https://img.shields.io/badge/status-Under%20Development-orange) 

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
designed to denoise only the chroma (colour), and is derived from the Cnr2 family of filters.
According to the original author, this style of filter is suited for stationary rainbows and
noisy analog captures.

For each pixel, CNR3 asks how much that location really changed since the previous *filtered*
frame. Where the change looks like noise rather than real motion or a scene cut, it gently pulls
the current colour toward the previous already-cleaned colour. Brightness (luma) is never
filtered — it passes through untouched and is used only as a guard for the colour decision.

The venerable old CNR2 relied on VapourSynth APIv3, which has been phased out, and additionally
depended on a serial (in-number-order) output frame request mode which is strongly recommended against
under VapourSynth R76+. Like CNR2, CNR3 implements a recursive temporal model — each output
frame depends on the previous *filtered* output frame, not the previous source frame — which is
inherently serial. CNR3 therefore uses the supported APIv4 and the supported `fmParallelRequests`
mode, and implements an internal output-frame cache (with checkpointing and bounded recovery) to
deal correctly with out-of-order frame requests.

This is [a port/upgrade of the AviSynth plugin vsCnr2](https://github.com/Asd-g/AviSynth-vsCnr2),
itself [ported from the VapourSynth plugin Cnr2](https://github.com/dubhater/vapoursynth-cnr2).
Defaults and 8-bit behaviour match cnr2 (although outputs are not bit exact since CNR3 uses
slightly more accurate arithmetic). The CNR3 parameter names are new but equivalent to those
in CNR2 (see the [name equivalence table](#cnr2--cnr3-name-equivalence) for migration).

CNR3 is 100% AI re-developed.

This project is distributed under the GNU GENERAL PUBLIC LICENSE Version 2 or later
(GPL-2.0-or-later).

## Requirements

- VapourSynth R76+ with Python 3.14+ (portable versions work).
- Microsoft Visual C++ Redistributable 2026+ (x64).
- a PC with AVX2 instructions. Google: "AVX2 has been a mainstream CPU feature for about 13 years. Estimated 85% to 90% of modern PCs currently in use have AVX2 support."

## Usage

```
cnr3.CNR3(vnode clip[, int y_threshold=35, int y_strength=192, string y_curve="wide",
                      int u_threshold=47, int u_strength=255, string u_curve="narrow",
                      int v_threshold=47, int v_strength=255, string v_curve="narrow",
                      float scene_threshold=10.0, bint scene_chroma=False])
```

All options are optional; the defaults are the operational cnr2-equivalent values. Invalid values
are rejected at filter creation with a one-line error stating what was received and what is
acceptable, e.g.

```
CNR3: invalid y_threshold option: got 256, expected an integer in the range 0..255 inclusive.
CNR3: invalid y_curve option: got "wobbly", expected exactly "wide" or "narrow".
```

### Parameters

- `clip`<br>
  A clip to process. Must be YUV 8..16-bit integer planar with chroma subsampling 420, 422,
  440 or 444. Frame dimensions and format must be constant.

- `y_threshold`, `u_threshold`, `v_threshold` (0..255; defaults 35 / 47 / 47)<br>
  How big a frame-to-frame difference each plane is willing to treat as "just noise".
  Differences LARGER than the threshold are treated as real content (motion, cuts) and passed
  through untouched; smaller differences are candidates for smoothing. Raise a threshold to
  denoise more aggressively (risking colour ghosting/smearing on moving objects); lower it to be
  more conservative. `y_threshold` is special: **luma is never filtered** — the Y response acts
  as a guard for the chroma decision (if brightness moved at that spot, something real happened,
  so the colour is left alone). `threshold=0` is valid and means only exactly-identical samples
  retain previous chroma.

- `y_strength`, `u_strength`, `v_strength` (0..255; defaults 192 / 255 / 255)<br>
  Once a difference is judged noise-like, how HARD to pull toward the previous frame's colour.
  255 allows near-total reuse of the previous cleaned value for tiny differences; lower values
  blend more gently. Threshold sets the reach of the response; strength sets its peak.

- `y_curve`, `u_curve`, `v_curve` ("wide" or "narrow"; defaults "wide" / "narrow" / "narrow")<br>
  The SHAPE of the response between zero difference and the threshold. `"wide"` stays near full
  strength for small differences and falls off sharply near the threshold — more effective
  smoothing, slightly bolder. `"narrow"` tapers steadily from the start — gentler, more
  cautious. The default gives luma a wide guard (small brightness flicker should not block
  chroma cleaning) and the chroma planes narrow, conservative responses.

- `scene_threshold` (0.0..100.0; default 10.0)<br>
  Scene-change detector sensitivity, as a percentage of the maximum possible whole-frame change.
  When a frame differs from the previous one by more than this, CNR3 declares a scene change and
  RESETS — the new frame passes through unfiltered and smoothing restarts from it. Lower = more
  sensitive (more resets, safer on rapid cutting); higher = fewer resets (better continuity,
  riskier across missed cuts).

- `scene_chroma` (True/False; default False)<br>
  Whether colour changes count toward scene-change detection, or brightness only (the default,
  matching cnr2). Leave False for typical footage. Set True for material with colour-only
  transitions (stage lighting shifts, flashing lights, chroma-heavy cuts): it prevents smoothing
  across a cut that brightness alone cannot see — the cause of rare colour-washing on such
  content.

### cnr2 -> CNR3 name equivalence

CNR3 uses descriptive option names. The cnr2/vsCnr2 names are not accepted; migrate once using
this table. Defaults are identical to cnr2.

| cnr2 | CNR3 | default | range |
|---|---|---|---|
| `ln` | `y_threshold` | 35 | 0..255 |
| `lm` | `y_strength` | 192 | 0..255 |
| `un` | `u_threshold` | 47 | 0..255 |
| `um` | `u_strength` | 255 | 0..255 |
| `vn` | `v_threshold` | 47 | 0..255 |
| `vm` | `v_strength` | 255 | 0..255 |
| `mode` ("oxx") | `y_curve`/`u_curve`/`v_curve` | "wide"/"narrow"/"narrow" | "wide" or "narrow" |
| `scdthr` | `scene_threshold` | 10.0 | 0.0..100.0 |
| `sceneChroma` | `scene_chroma` | False | bool |

(cnr2's `mode` used `'x'` for narrow and treated any other character as wide; CNR3 validates the
curve strings strictly.)

### Usage with progressive input material

```python
import vapoursynth as vs
core = vs.core

clip = core.bs.VideoSource(r"capture_progressive.mpg")     # any source filter
clip = core.cnr3.CNR3(clip)                                # defaults: cnr2-equivalent
clip.set_output()
```

Stronger chroma cleaning for a very noisy capture (raise chroma thresholds carefully — too high
causes colour ghosting on movement):

```python
den = core.cnr3.CNR3(clip, u_threshold=60, v_threshold=60)
```

### Usage with interlaced input material

CNR3 filters frames; interlaced material must be processed PER FIELD, then re-woven, or the two
fields' different time instants will be blended together and cause combing/ghosting in the
chroma. Use the two helper functions in **Appendix A** (proven against real BFF VHS captures);
the calling pattern is below.

**WARNING — metadata-poor input videos:**     
**Many** container/codec combinations (notably `.avi`) carry **NO reliable interlacing metadata**
- `_FieldBased` may be absent (or read as progressive) or plain wrong    
- TFF vs BFF may be unknowable from the input file    

If you have a source like that then you **must manually separatefields and re-weave them yourself**
rather than use the functions in Appendix A.  Function `split_into_fields` attempts to autodetect `_FieldBased`,
so for such material it can silently take the flag Progressive for Interlaced footage. 
You can determine the truth by inspection (bob the clip and step fields) and handle
it manually — call `SeparateFields`/`reweave_fields` yourself with the field order you verified,
rather than trusting autodetection.

```python
import vapoursynth as vs
core = vs.core
# ... paste the Appendix A helpers here (split_into_fields, reweave_fields) ...

# this input video must have well-defined Progressive/Interlaced metadata,
# or this will fail without manual intervention (see below).
clip = core.bs.VideoSource(r"capture_interlaced_PAL.mpg")   

# try to detect Progressive/Interlaced etc and separate fields
# this will FAIL if the input has insufficient metadata  - see WARNING below
# in which case you MUST manually
#   - identify Progressive or Interlaced and if interlaced whether TFF or BFF
#   - if Interlaced, separatefields into first and second
#   - if progressive, set first to the clip and second to None
#   - set the tag as one of "P", "TFF" or "BFF"
tag, first, second = split_into_fields(clip)     # tag "P", "TFF" or "BFF" + field streams

# process strictly depending on what was detected (or perhaps manually set tag/first/second yourself) 
# tag = strictly one of "P", "TFF" or "BFF"
# first = first field as a video clip
# second = second field  as a video clip
if tag == "P":                                   # progressive: one CNR3 instance
    den = core.cnr3.CNR3(first)
else:                                            # interlaced: one instance PER field stream
    first_d  = core.cnr3.CNR3(first)
    second_d = core.cnr3.CNR3(second)
    den = reweave_fields(tag, first_d, second_d)

den.set_output()
```

Each field stream gets its OWN CNR3 instance (the recursion must follow each field's own
timeline); startup provenance lines therefore appear once per instance (`CNR3[1]`, `CNR3[2]`) —
this is normal.

## Errors

Every rejected option produces a single-line creation error naming the option, what was
received, and what is acceptable. Type mismatches for int/float options may instead be rejected
by VapourSynth's own Python layer before CNR3 runs — also a clean, hard error.

## Technical info for nerds

### **Why serial.**    

CNR3's cardinal rule is `output[N] = f(source[N], output[N-1])` — a recursive
IIR along the timeline, with scene changes resetting the chain. That recursion is inherently
serial: you cannot compute frame N without the *filtered* N-1. VapourSynth R76+, however, may
request frames out of order and concurrently.

### **Modes, and why fmParallelRequests.**    

All three API4 filter modes were implemented and
measured on a 3000-frame PAL SD workload (24 threads, Release, internal diagnostics ON):

| mode | fps | wasted recomputes | notes |
|---|---|---|---|
| fmUnordered | 95 | 0 | clean but serialises everything |
| **fmParallelRequests** | **337** | **~0 (1 duplicate)** | overlaps requests/planning, serialises compute — **shipped** |
| fmParallel | 126 | 78% overcompute | overlapped compute races the recursion; a reservation-table fix is designed but parked |

End-to-end with x264 (CRF18, PAL SD): ~274 fps, 5.5x realtime — the filter is not the bottleneck.

### **The CNR3 internal cache

Because frame requests may arrive from vapoursynth out of order (and concurrently), CNR3 keeps
an internal cache of recent *filtered* output frames. The cache has several simple mechanisms
to manage itself and deliver "past *filtered* output frame(s)" for the compute sequence.

Two main use-cases are apparent:
(1) an end-to-end "linear progression" arising from a transcode (eg vspipe to ffmpeg)
(2) viewing or editing, jumping from point to point

It is estimated that for VHS/VHS-C analogue capture files, use-case (1) is 99.9% of CNR3 use.

**The wavefront (the normal case).** A linear transcode requests frames 0, 1, 2, ... in order
(or close enough to it under mode fmParallelRequests).
The cache simply holds the most recent filtered frames, and each new frame finds its predecessor
(N-1) immediately: zero recomputation, zero overhead. This serves the overwhelming majority of
real use (encodes) — everything below exists for the exceptions.

**Bounded size + eviction.** The cache cannot grow forever, so it has a ceiling (~500 frames at
SD) and a pruner that evicts the least useful frames as new ones arrive. Eviction is safe
because anything evicted can be recomputed (see recovery) — the bound trades a little potential
recomputation for a hard memory guarantee.

**Checkpoints (bounding the cost of a cold request).** The recursion means frame N depends on
filtered N-1, which depends on N-2, and so on back to the last scene change. A request far from
anything cached would naively force recomputation from frame 0. Instead, CNR3 designates every
~10th frame a checkpoint: a legal restart point for the recursion. Checkpoints are pruned from
the cache less frequently than non-checkpoint frames and so benefit a backward search. A cold
request walks back at most to the nearest checkpoint, never to the start of the clip — recovery
cost is bounded by the checkpoint spacing, not the clip length.

**Recovery (filling a hole).** When a requested frame's predecessor is not cached, CNR3 walks
back to the nearest cached predecessor or checkpoint (bounded, per above), then recomputes
forward from there to the requested frame, caching as it goes. Recovery is the general fallback
that makes every other bound safe.

**Hot zones (protecting where you are working).** Seeking and scrubbing (jumping around in an
editor) create activity clusters far from the wavefront. Regions around recent activity are
marked hot and protected from eviction, so scrubbing back and forth over the same section does
not repeatedly evict and recompute it.

**Pins (correctness under concurrency).** While a computation is using a cached frame as its
predecessor, that frame is pinned: the pruner cannot evict it mid-use, no matter what. Pins are
the hard correctness guarantee that the memory bound can never compromise an in-flight
computation.

**Measured results (shipped configuration, PAL SD workloads).** The tell-tale counter
`recently-evicted-then-re-requested` — "did we throw away something we immediately needed
again?" — reads **0** on both linear and shuffled request patterns: the cache does not thrash,
and halving its ceiling (1000 -> 500) changed neither that counter nor throughput. And the
bottom line for correctness: output is
**tested as byte-identical across all filter modes and thread counts** — scheduling changes,
pixels did not.

## Appendix A — Interlaced input .vpy helper functions

These helper functions were used in the CNR3 re-development test harnesses (validated against real
BFF VHS captures). 

**WARNING — metadata-poor input videos:**     
**Many** container/codec combinations (notably `.avi`) carry **NO reliable interlacing metadata**
- `_FieldBased` may be absent (or read as progressive) or plain wrong    
- TFF vs BFF may be unknowable from the input file    

If you have a source like that then you **must manually separatefields and re-weave them yourself**
rather than use the functions in Appendix A.  Function `split_into_fields` attempts to autodetect `_FieldBased`,
so for such material it can silently take the flag Progressive for Interlaced footage. 
You can determine the truth by inspection (bob the clip and step fields) and handle
it manually — call `SeparateFields`/`reweave_fields` yourself with the field order you verified,
rather than trusting autodetection.

NOTE: these are the exact helpers used by the CNR3 test harnesses (validated against real BFF
VHS captures), with ONE line added for the README: the `SetFieldBased` call at the end of
`reweave_fields`, restoring the interlaced flag that `SeparateFields` clears, so downstream
filters/encoders see correct metadata.

```python
from typing import Optional, Tuple
import vapoursynth as vs
core = vs.core

SplitResult = Tuple[str, vs.VideoNode, Optional[vs.VideoNode]]

def split_into_fields(clip: vs.VideoNode) -> SplitResult:
    """
    Inspect a clip's field order and separate it into same-parity field streams.
    Returns a 3-tuple (scan_tag, first, second):
        scan_tag: "P" = progressive, "TFF" / "BFF" = interlaced field order
        first:    progressive: the original clip; interlaced: the temporally-first fields
        second:   progressive: None;              interlaced: the temporally-second fields
    Process first and second independently, then pass all three values to
    reweave_fields().  "first"/"second" are positions in the separated stream,
    not top/bottom; reweave_fields() reconstructs the correct order from scan_tag,
    so the caller never tracks tff explicitly.
    SeparateFields output ordering:
        TFF: [T0, B0, T1, B1, ...]   BFF: [B0, T0, B1, T1, ...]
    """
    frame0 = clip.get_frame(0)
    field_based = int(frame0.props.get("_FieldBased", 0))   # 0=P, 1=BFF, 2=TFF
    if field_based == 0:
        return ("P", clip, None)
    tff = (field_based == 2)
    separated = core.std.SeparateFields(clip, tff=tff)
    first  = core.std.SelectEvery(separated, cycle=2, offsets=[0])
    second = core.std.SelectEvery(separated, cycle=2, offsets=[1])
    return ("TFF" if tff else "BFF", first, second)

def reweave_fields(
    scan_tag: str,
    first: vs.VideoNode,
    second: Optional[vs.VideoNode],
) -> vs.VideoNode:
    """
    Reweave two processed field streams from split_into_fields back into a
    full-height interlaced clip, or return the processed progressive clip
    directly ("P").
    """
    if scan_tag == "P":
        return first
    if second is None:
        raise ValueError(
            "reweave_fields: second field stream is None but scan_tag is not 'P'. "
            "Pass the second field stream returned by split_into_fields()."
        )
    if scan_tag not in ("TFF", "BFF"):
        raise ValueError(
            f"reweave_fields: unrecognised scan_tag {scan_tag!r}. "
            "Expected 'P', 'TFF', or 'BFF'."
        )
    tff = (scan_tag == "TFF")
    # Interleave restores the original SeparateFields ordering:
    #   TFF: [T0, B0, T1, B1, ...]   BFF: [B0, T0, B1, T1, ...]
    reinterleaved = core.std.Interleave([first, second])
    # DoubleWeave produces pairs: even index = clean same-parity weave (keep),
    # odd index = dirty adjacent-parity weave (discard).
    rewoven = core.std.DoubleWeave(reinterleaved, tff=tff)
    woven = core.std.SelectEvery(rewoven, cycle=2, offsets=[0])
    # ADDED FOR README: SeparateFields cleared _FieldBased; restore it so the
    # output carries correct interlacing metadata (1=BFF, 2=TFF).
    return core.std.SetFieldBased(woven, 2 if tff else 1)
```

(`core.std.Weave` does not exist in R76 — re-weaving is the DoubleWeave + SelectEvery pattern above.)




## Credits

- dubhater — the original VapourSynth Cnr2.
- Asd-g — the AviSynth vsCnr2 port this project consulted for reference semantics.
- The original Cnr2 concept by Marc FD.
- Iterative re-development by ClaudeAI (designer/scoper/reviewer/advisor) and ChatGPT (coder/scope-reviewer/advisor)