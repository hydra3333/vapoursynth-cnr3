# CNR3 VapourSynth plugin options

CNR3 is a temporal chroma denoiser for VapourSynth API4. It steadies colour
planes (U/V) between frames while leaving brightness (Y) unchanged. For each
pixel, CNR3 asks how much that location changed since the previous filtered
frame. Where the change looks like noise rather than real motion or a cut, it
pulls the current colour gently toward the previous already-filtered colour.
The options below tune that judgement.

## Defaults and CNR2 name equivalence

CNR3 uses descriptive option names. The older cnr2/vscnr2 names are not CNR3
parser names, but the defaults and behaviour are mapped from cnr2's operational
surface.

| cnr2 name | CNR3 name | Default | Range / rule | Meaning |
|---|---|---:|---|---|
| `ln` | `y_threshold` | 35 | integer 0..255 | Luma-change guard threshold |
| `lm` | `y_strength` | 192 | integer 0..255 | Luma guard response strength |
| `un` | `u_threshold` | 47 | integer 0..255 | U difference threshold |
| `um` | `u_strength` | 255 | integer 0..255 | U response strength |
| `vn` | `v_threshold` | 47 | integer 0..255 | V difference threshold |
| `vm` | `v_strength` | 255 | integer 0..255 | V response strength |
| `mode[0]` | `y_curve` | `wide` | `wide` or `narrow` | Y response-table curve |
| `mode[1]` | `u_curve` | `narrow` | `wide` or `narrow` | U response-table curve |
| `mode[2]` | `v_curve` | `narrow` | `wide` or `narrow` | V response-table curve |
| `scdthr` | `scene_threshold` | 10.0 | float 0.0..100.0 | Scene-reset sensitivity |
| `sceneChroma` | `scene_chroma` | false | bool / 0 or 1 | Include chroma in scene reset |

CNR2's `mode="oxx"` is equivalent to:

```python
y_curve="wide", u_curve="narrow", v_curve="narrow"
```

CNR2 treated the character `x` as narrow and any non-`x` character as wide.
CNR3 intentionally uses explicit `wide` or `narrow` strings and rejects other
curve names.

## Threshold options

`y_threshold`, `u_threshold`, and `v_threshold` decide how large a
frame-to-frame difference may still be treated as noise-like. Differences larger
than the threshold are treated as real content, such as motion or a cut, and are
passed through without chroma smoothing at that location. Differences smaller
than the threshold are candidates for smoothing.

Raising a threshold denoises more aggressively but increases the risk of colour
ghosting or smearing on moving objects. Lowering a threshold is more
conservative.

`y_threshold` is special: it does not filter luma output. CNR3 leaves the Y plane
unchanged. The Y response is a luma-change guard for chroma filtering. If
brightness changed at a location, CNR3 treats that as evidence that something
real moved there and suppresses chroma blending.

A threshold value of 0 is valid for cnr2 range compatibility. Internally, CNR3
special-cases threshold 0 so table construction never divides by zero. At
threshold 0, only an exact zero difference can receive the configured strength;
all nonzero differences receive zero blend.

## Strength options

`y_strength`, `u_strength`, and `v_strength` control how strongly CNR3 can pull
toward the previous filtered chroma value once a difference is judged noise-like.
A value of 255 allows near-total reuse of the previous cleaned value for tiny
differences. Lower values blend more gently.

Threshold sets the reach of the response. Strength sets the peak pull.

## Curve options

`y_curve`, `u_curve`, and `v_curve` control the shape of the response between
zero difference and the threshold.

`wide` uses the j*j cosine response. It stays near full strength for small
differences and falls off sharply near the threshold. This is more effective for
smoothing but slightly bolder.

`narrow` uses the j cosine response. It tapers off more steadily from zero to the
threshold and is therefore gentler and more cautious.

The default gives luma a wide guard, so small brightness flicker does not block
chroma cleaning, while U and V use narrow responses so colour changes are treated
conservatively.

## Scene reset options

`scene_threshold` is the scene-change/reset sensitivity, mapped from cnr2's
`scdthr`. When a frame differs from the previous one by more than this threshold,
CNR3 declares a scene change, passes the new frame through unfiltered, and
restarts smoothing from it.

Lower values are more sensitive and produce more resets, which is safer on rapid
cutting. Higher values produce fewer resets, which may preserve continuity but
can risk smoothing across a missed cut.

`scene_chroma` controls whether colour changes contribute to scene-change
detection. The default is false, matching cnr2's luma-only scene detection.
Leave it false for typical footage. Set it true for material with colour-only
transitions, lighting colour shifts, flashing stage lights, or chroma-heavy cuts.
It prevents CNR3 from smoothing across a cut that brightness alone cannot see.

## Example

The no-option call uses cnr2-equivalent defaults:

```python
clip = core.cnr3.CNR3(clip)
```

The same defaults written explicitly:

```python
clip = core.cnr3.CNR3(
    clip,
    y_threshold=35,
    y_strength=192,
    u_threshold=47,
    u_strength=255,
    v_threshold=47,
    v_strength=255,
    y_curve="wide",
    u_curve="narrow",
    v_curve="narrow",
    scene_threshold=10.0,
    scene_chroma=False,
)
```

For chroma-heavy cuts or lighting changes, try:

```python
clip = core.cnr3.CNR3(clip, scene_chroma=True)
```

## Bit-depth compatibility note

The option values are specified in the historical 8-bit cnr2 domain. For 8-bit
clips, CNR3's defaults and response-table behaviour are intended to match cnr2's
operational defaults. For 9..16-bit clips, CNR3 follows its CMS07 native-depth
round-to-nearest scaling policy unless a future exact cnr2 high-depth emulation
phase is explicitly scoped.
