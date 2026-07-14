# CNR3 — PATCH SCOPE: operational response-table defaults (fix chroma desaturation) — v1

**Marker on success:** `CMS07-FIX.operational-response-defaults`
**Baseline:** current committed dev_cache_manager tree (the src_...zip / half-cache commit).
**Nature:** replaces the K.1E.2 proof-placeholder response-table defaults with vscnr2-style operational
defaults, and emits the effective response config at filter creation. DELIBERATELY CHANGES OUTPUT.

## Root cause being fixed (agreed W3D + W3C, confirmed by the Y4M delta analyser)

vapoursynth-Cnr3.cpp ~358-383 hardcodes CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT=255 and
..._STRENGTH_8BIT=255 on all three planes, all curves narrow, with a comment explicitly deferring the
real default policy. No user parameters are parsed. threshold=255 means the response never shuts off
across real chroma changes; strength=255 means peak ~98% previous-retention. Result: recursive chroma
blending across real chroma differences -> broad neutral pull. Analyser proof: frames 8/9 show 83% of
U/V samples pulled toward neutral, red/brown V delta ~ -5.8, luma untouched.
IMPORTANT CORRECTION (W3X visual check): there is NO visible scene change at frame 8. The analyser's
"chroma-inclusive scene candidate" at 8 is the chroma diff vs the previous FILTERED frame crossing the
diagnostic threshold — i.e. a moderately larger real chroma difference (motion/content, sub-scene-change)
which correct tables would blend lightly (response tapers over the 0..47 diff range) but the 255/255
placeholders blended at near-full strength. Same mechanism as the mild damage on frames 1/3/5/7 — a
continuum, not a cut event. Chroma-inclusive scene detection is therefore NOT implicated for this clip;
the fix rests entirely on the defaults. The math is correct; only the parameter surface regressed.
Expected post-patch shape: frames 8/9 drop to MINOR-class like frame 5 (small, sign-balanced chroma
smoothing, no red/brown magnitude collapse).

## ITEM A — the default swap (vapoursynth-Cnr3.cpp)

Replace the two proof constants and the per-plane assignments with named operational defaults
(vscnr2-style, per the historical mode "oxx"):

```cpp
/* vscnr2-style operational defaults (historical mode "oxx"):
   Y wide so luma structure does not block chroma stabilisation too eagerly;
   U/V narrow so real chroma changes are handled conservatively. */
inline constexpr int CNR3_DEFAULT_Y_THRESHOLD_8BIT  = 35;
inline constexpr int CNR3_DEFAULT_Y_STRENGTH_8BIT   = 192;
inline constexpr int CNR3_DEFAULT_UV_THRESHOLD_8BIT = 47;
inline constexpr int CNR3_DEFAULT_UV_STRENGTH_8BIT  = 255;
```
Assignments become:
```cpp
config.y.threshold_8bit = CNR3_DEFAULT_Y_THRESHOLD_8BIT;   config.y.strength_8bit = CNR3_DEFAULT_Y_STRENGTH_8BIT;
config.y.curve = Cnr3ResponseCurveKind::wide;              // 'o'
config.u.threshold_8bit = CNR3_DEFAULT_UV_THRESHOLD_8BIT;  config.u.strength_8bit = CNR3_DEFAULT_UV_STRENGTH_8BIT;
config.u.curve = Cnr3ResponseCurveKind::narrow;            // 'x'
config.v.threshold_8bit = CNR3_DEFAULT_UV_THRESHOLD_8BIT;  config.v.strength_8bit = CNR3_DEFAULT_UV_STRENGTH_8BIT;
config.v.curve = Cnr3ResponseCurveKind::narrow;            // 'x'
```
- REMOVE the K1E2_PROOF constants entirely (grep-clean: zero occurrences of K1E2_PROOF_DEFAULT remain).
- Update the surrounding comment: no longer "proof config"; note that full user option parsing
  (ln/lm/un/um/vn/vm/mode/scdthr) remains a deferred follow-up patch, but the DEFAULTS are now operational.
- The 8-bit values continue through the existing round-to-nearest native-depth scaling path unchanged.
- Confirm cold that Cnr3ResponseCurveKind::wide exists and maps to wide_response=true in the table build.

## ITEM B — effective-config emission at creation (coder suggestion, accepted)

On the same gate as the existing startup provenance (CNR3_EMIT_PLUGIN_STARTUP_PROVENANCE, default on),
emit one line alongside edit_version/filter_mode:
```
CNR3[i] INFO CONFIG: response_config: y=35/192/wide u=47/255/narrow v=47/255/narrow
```
Values printed from the LIVE config struct (not the #define literals) so the line can never lie about
what is actually in effect. This prevents this regression class from hiding again.

## Explicitly OUT of scope (follow-ups, do not fold in)

1. User option parsing (ln/lm/un/um/vn/vm/mode/scdthr) — its own patch with error-surface design.
2. scene_chroma exposure / chroma-inclusive scene detection policy — separate decision; do not conflate.
3. Any change to blend math, tables, downsample, scene detection, cache, or plan-retry.

## Proof gate

1. Canonical 4-way selftest: count unchanged (this patch touches no selftest and no gated diag code).
2. **R-PROCESS-19 byte-identical DOES NOT APPLY** — this patch DELIBERATELY changes frame output.
   State this explicitly in the notes so nobody runs the fc harness and misreads a difference as a defect.
3. THE proof: re-run the exact 10-frame Y4M in/out + the Python delta analyser. Expected:
   - frames 8/9 no longer classified LIKELY_DESATURATION_BUG (or U/V MAD and red/brown magnitude loss
     drop sharply);
   - frames 0/2/4/6 remain byte-identical passthrough (resets unaffected);
   - frames 1/3/5/7 remain MINOR (in-scene smoothing still active — the filter must still DO something).
4. Eyeball: the stackhorizontal comparison — brown/red holds saturation.
5. Startup line present: response_config printed with the live values.
6. grep-clean: K1E2_PROOF_DEFAULT gone; new CNR3_DEFAULT_* present at all assignment sites.
