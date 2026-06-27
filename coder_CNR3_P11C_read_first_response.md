# CNR3 P.11C Read-First Response to Designer Brief

## Immediate answers to the coordinator's questions

### 1. Do I need the latest source uploaded?

For a **read-first design response**, no: the current brief plus the known D.5 baseline is enough to answer the approach question and identify the expected seams.

For an **exact coder confirmation or patch**, yes: please upload the latest post-D.5 source ZIP before implementation. The D.5 commit means the authoritative source may now include small local edits or commit-normalized text that should not be guessed from prior patches. For P.11C, exact source matters because we need to verify:

- the exact live call sites in `cnr3_arAllFramesReady.cpp`;
- the exact signatures of the two caller-supplied pixel-processing entry points;
- the exact live store wrapper and recovery-hole store call shapes;
- the current KDT fields and frameData plumbing;
- whether there are any D.5-adjacent changes affecting the selftest or cache helper APIs.

So: latest source is **not required to discuss the approach**, but **is required before patch preparation**.

### 2. Does the coordinator's goal change anything?

Your stated goal is:

> complete whatever it takes to get scene detection and its associated logic developed and in place.

Yes, that slightly broadens the framing compared with a narrow "switch one call to `_with_scene_change`" phase.

It means P.11C should be scoped as a complete live scene-change integration phase, not merely as a pixel-call replacement. In practical terms, "complete whatever it takes" should include:

1. live scene-change detection config;
2. live use of the scene-change pixel-processing entry point where a real previous filtered output exists;
3. reset behaviour being exercised live through the already-proven pixel path;
4. scene-change result propagation from pixel summary to cache-store decision;
5. checkpoint storage/promotion for detected cuts;
6. per-hole scene detection/checkpointing in branch-d recovery;
7. final-target scene detection/checkpointing in branch-c and branch-d;
8. KDT/proof fields sufficient to prove the behaviour;
9. synthetic live harness proof;
10. no D-SUM / telemetry / real-footage campaign yet.

The one boundary I would preserve is that P.11C should **not** absorb the later diagnostics/telemetry arc or real-footage campaign. Those are correctly deferred.

### 3. About the UI error

The red `Minified React error #185` is a client/UI rendering error, not a CNR3 or content issue. To avoid the previous response being lost or truncated, this file regenerates the full answer as a standalone Markdown document.

---

# P.11C Designer Brief: Coder Read-First Response

## Executive summary

The designer's brief is broadly correct: P.11C is a live wiring-and-composition phase, not a new pixel-algorithm phase. Pixel-level scene-change detection and reset behaviour already exist in the caller-supplied pixel-processing arc. P.11C should wire those existing behaviours into the live getFrame path and make detected cuts become cache checkpoints.

The main correction is this:

```text
P.11C is not just "switch the final live call sites to the
_with_scene_change variant."

It must wire scene-change status into every computed-output store that may
produce a cut:

- branch-c predecessor-present target store;
- branch-d recovery hole stores;
- branch-d recovery target store;
- fresh-start/floor policy must be clarified separately because the detector
  requires a previous filtered output.
```

The coordinator's goal means the phase should be scoped as **complete live scene-change integration**: detection, reset, checkpoint promotion, recovery-hole behaviour, KDT proof fields, and synthetic proof harnesses.

---

## Confirmations and corrections to Section 2

### 2.1 Two pixel-processing entry points

Confirmed as the expected source shape:

```text
cnr3_process_caller_supplied_vapoursynth_frame_triplet(...)
cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(...)
```

The `_with_scene_change` variant is the intended live entry point wherever the live branch has a real previous filtered output. I would not collapse the two public entry points in P.11C. Keeping both is useful because fresh-start/floor cases may not have a valid predecessor and therefore may need the non-scene path or direct source copy semantics.

Expected rule:

```text
Use _with_scene_change where previous filtered output exists.
Do not force it into branches that do not have a valid previous filtered output
unless CMS explicitly defines a synthetic predecessor policy.
```

### 2.2 `Cnr3SceneChangeConfig`

Expected struct:

```cpp
struct Cnr3SceneChangeConfig {
    std::int64_t scene_change_threshold = 0;
    bool scene_chroma = false;
};
```

Expected meaning:

```text
scene_chroma == false:
    scene detection uses downsampled luma only.

scene_chroma == true:
    scene detection includes chroma U/V absolute differences in addition to
    downsampled luma.
```

Recommended live default for P.11C:

```text
scene_chroma = false
```

Reason: luma-only is safer as an initial live default. CNR3 is a chroma denoiser, and noisy analogue chroma can cause false positives if chroma is included too early.

Threshold should be per-instance configuration, not a raw compile-time flag. For P.11C, an internal/CMS proof default is acceptable, but it should be represented as a runtime config field in the instance/filter data so later plugin parameters can feed it without rewiring the live path.

A sane initial 8-bit 4:2:0 luma-only threshold can be dimension-scaled:

```text
threshold = chroma_width * chroma_height * 4 * 20
```

This corresponds roughly to an average luma difference of 20 levels over the chroma grid for 4:2:0, where `4 == 1 << (sub_sampling_w + sub_sampling_h)`.

For synthetic proof harnesses, do not rely on a subjective default. Use constructed clips and explicit thresholds that force cut/no-cut outcomes deterministically.

### 2.3 Detector semantics

Confirmed in expected design:

```text
current source downsampled luma
    compared against previous filtered/output downsampled luma;

current source U/V
    compared against previous filtered U/V only if scene_chroma == true;

diff_total accumulates absolute differences;

scene_change == true when:
    diff_total > scene_change_threshold
```

This `>` relation matters: equality should remain non-cut unless the existing proven P.11C pixel arc says otherwise.

### 2.4 Reset behaviour

Confirmed. The reset behaviour already exists in the caller-supplied pixel implementation. When scene-change is detected, output chroma should copy current source chroma and bypass recursive chroma blending.

Therefore, once live detection is enabled through the scene-change entry point, no new pixel reset algorithm should be required. P.11C should prove the reset is exercised live.

### 2.5 Summary bits

Confirmed as expected. `scene_change_detected` should be treated as the authoritative "this computed frame is a cut" bit.

Expected relevant fields:

```text
scene_change_detection_used
scene_change_detected
scene_change_reset_output_used
recursive_chroma_blend_used
scene_change_threshold
scene_change_diff_total
scene_change_samples_examined
```

P.11C store routing should consume `scene_change_detected`.

### 2.6 Checkpoint store primitive

Mostly confirmed, with an important distinction.

For an isolated final target store, a detected cut should route to checkpoint storage/promotion. The primitive may be:

```text
store_checkpoint_owned_frame(...)
```

However, for recovery stores that also need pin-recording, the cleaner primitive is likely the combined AS2 store-and-pin path with checkpoint flag:

```text
store_owned_frame_and_record_pin(..., is_checkpoint = true, ...)
```

or for planned recovery holes:

```text
store_recovery_plan_hole_owned_frame_and_record_pin(..., is_checkpoint = true, ...)
```

The important requirement is not the function name. The requirement is:

```text
the checkpoint flag/promotion decision must enter the AS2 store atomic;
it must not be a later separate promotion call under a second lock.
```

That preserves CMS 6.6 monotonic promote-only semantics.

### 2.7 Live store wrapper gap

Confirmed. The authoritative live output store wrapper is the main seam for final target stores. It currently needs a way to receive a cut/checkpoint override.

Recommended shape:

```text
store_as_checkpoint = grid_checkpoint || scene_change_detected
```

I prefer a small store-request struct or named boolean over another anonymous boolean parameter.

Example:

```cpp
struct Cnr3LiveOutputStoreRequest {
    int frame_number;
    bool force_checkpoint;
};
```

or:

```cpp
struct Cnr3LiveOutputStoreRequest {
    int frame_number;
    bool scene_change_detected;
};
```

A helper can centralise the final decision:

```cpp
cnr3_live_output_frame_should_store_as_checkpoint(frame_number, force_checkpoint)
```

This avoids scattering `grid || scene` logic across branch-c and branch-d.

### 2.8 Live path currently defers P.11C

Confirmed at the design level. Existing KDT currently reports:

```text
p11c_called=0
scene_change_deferred=1
```

and live path currently calls the non-scene variant.

P.11C should flip this for branches where detection is enabled:

```text
p11c_called=1
scene_change_deferred=0
scene_change_detection_used=1
```

---

## Section 3 decomposition: corrections and recommended phase decomposition

The designer's A/B/C/D decomposition is directionally correct, but I would refine it as follows.

### A. Add live scene-change config

Add or populate a per-instance `Cnr3SceneChangeConfig`.

For P.11C, this may use an internal/CMS default, but the config should live in instance/filter data so later plugin parameters can feed it.

Recommended default for first wiring:

```text
scene_chroma = false
threshold = dimension-scaled luma-only default
```

### B. Enable detection at all live recursive compute sites

Use `_with_scene_change` where a valid previous filtered output exists:

```text
branch-c:
    predecessor-present target compute

branch-d:
    recovery hole compute
    recovery final target compute
```

Do not force detector execution into fresh-start cases that lack a predecessor unless CMS defines how that predecessor is supplied.

### C. Thread `scene_change_detected` into every computed-output store

For every computed output that may detect a cut:

```text
store_as_checkpoint = grid_checkpoint || scene_change_detected
```

This must apply to:

```text
branch-c final target;
branch-d per-hole stores;
branch-d final target.
```

### D. Keep fresh-start/floor semantics explicit

Branch-a/fresh-start and floor-fresh-start have no previous filtered predecessor. The existing detector compares current source to previous filtered output, so there is no honest detector input there unless CMS defines a synthetic predecessor.

Therefore P.11C should either:

```text
1. explicitly mark fresh-start/floor as detection-not-applicable and already
   fresh-start/checkpoint-policy governed; or

2. raise CMS-GAP if CMS requires literal scene detection in branch-a/floor
   without defining a predecessor.
```

My recommendation: treat fresh-start/floor as detection-not-applicable for P.11C, while preserving their existing checkpoint policy.

### E. Add KDT/proof fields

P.11C needs enough KDT/proof surface to show:

```text
detection called / not deferred;
threshold/config used;
diff_total and sample count;
cut detected or not;
reset output used or recursive blend used;
store_as_checkpoint decision;
checkpoint promotion or checkpoint store path;
resulting slot checkpoint status;
per-hole scene state in recovery.
```

---

## Section 4 answers

### Q1. Threshold source

Use per-instance config.

Do not make threshold only a compile-time flag. Compile-time flags are appropriate for diagnostics gates, not for scene-change behaviour once it is part of live plugin semantics.

Recommended P.11C posture:

```text
- add/populate Cnr3SceneChangeConfig in instance/filter data;
- use a named CMS/proof default for now;
- later expose plugin parameters in the option-surface phase;
- scene_chroma=false initially;
- threshold dimension-scaled rather than a fixed raw number.
```

For 8-bit 4:2:0, initial default:

```text
threshold = chroma_width * chroma_height * 4 * 20
```

For proof harnesses, set or construct threshold deterministically to force cut/no-cut.

### Q2. Cut detection inside recovery branches

The recovery fill loop should already compute holes through the caller-supplied frame-triplet processing path. P.11C should switch that live recovery compute to the `_with_scene_change` variant.

This enables per-hole detection because each hole has:

```text
current source frame K;
previous filtered output = reconstructed predecessor:
    floor output or prior hole output;
destination output K.
```

Detection against a reconstructed predecessor is correct. The reconstructed predecessor is the previous filtered output for the ascending recovery walk. It does not need to be cache-resident before compute; it needs to be the filtered predecessor frame.

Per-hole checkpointing must be wired at the per-hole store. Do not only checkpoint the final target. CMS 9.2 requires that if a cut is detected at hole K:

```text
output[K] is reset/fresh-started;
output[K] is stored with checkpoint flag;
ascending walk continues from output[K].
```

Therefore P.11C must ensure the per-hole store wrapper takes the per-hole `scene_change_detected` bit and stores/promotes the hole as checkpoint atomically.

### Q3. Checkpoint promotion under store atomic

Pinned and checkpoint states should coexist correctly because they are separate slot state:

```text
pin count / pin tokens protect lifecycle;
checkpoint flag affects retention/recovery anchor quality;
```

For a recovery hole that is both pinned and a detected cut, the store path should compose:

```text
store_recovery_plan_hole_owned_frame_and_record_pin(
    ...,
    is_checkpoint = grid_checkpoint || scene_change_detected,
    pin_list,
    summary
)
```

This should set/promote the checkpoint flag and record the pin under the same AS2 store atomic.

The implementation must avoid:

```text
store non-checkpoint;
then later promote checkpoint in a separate lock.
```

That would undercut CMS 6.6's concurrency rationale.

fmParallel-safe interleavings:

```text
A stores frame K non-checkpoint first.
B later computes/detects K as cut and attempts checkpoint duplicate.
B promotes existing K to checkpoint without replacing frame data.

A stores frame K checkpoint first.
B later computes K non-checkpoint.
B does not demote existing checkpoint.

A pins/adopts K while B promotes K.
Pin count and checkpoint flag coexist under cache lock.
```

### Q4. Proof strategy

The proof shape should include both live synthetic proof and, only if needed, a small selftest.

Recommended proof layers:

#### Layer 1: live synthetic harness proof

Use constructed clip patterns to prove:

```text
scene detection used;
cut detected;
reset output used;
recursive blend not used;
output chroma equals current source chroma;
output is stored/promoted as checkpoint;
later recovery finds that frame as an anchor.
```

Required branch coverage:

```text
branch-c no-cut control;
branch-c cut at target N;
branch-d cut at intermediate hole K;
branch-d cut at final target N;
later recovery using detected-cut frame as anchor.
```

This may be split into subphases if the designer wants tighter proof increments.

#### Layer 2: selftest only if necessary

Add a cache-core/selftest proof only if the live harness cannot deterministically force duplicate checkpoint promotion.

Candidate selftest:

```text
store frame K as non-checkpoint;
store duplicate frame K as checkpoint;
assert existing slot promoted to checkpoint;
assert frame identity preserved;
assert non-checkpoint duplicate does not demote checkpoint.
```

Some of this may already be covered by G.12A. If so, do not duplicate it unless P.11C needs a narrower proof through a specific combined store-and-pin helper.

### Expected KDT fields

At minimum:

```text
p11c_called=1
scene_change_deferred=0
scene_change_detection_used=1
scene_chroma_used=0/1
scene_change_threshold=<value>
scene_change_diff_total=<value>
scene_change_samples_examined=<value>
scene_change_detected=0/1
scene_change_reset_output_used=0/1
recursive_chroma_blend_used=0/1
store_as_checkpoint=0/1
scene_checkpoint_store=0/1
checkpoint_promoted=0/1
resulting_slot_is_checkpoint=0/1
```

For recovery, include per-hole fields:

```text
hole=K
hole_scene_change_detected=0/1
hole_scene_change_reset_output_used=0/1
hole_store_as_checkpoint=0/1
hole_resulting_slot_is_checkpoint=0/1
```

or equivalent compact KDT lines.

### Q5. fmParallel interleaving

The main fmParallel rule is:

```text
scene-change checkpoint promotion must be monotonic and atomic with AS2 store.
```

All live stores that can carry `scene_change_detected` must pass the checkpoint decision into the store helper before the store lock is acquired.

Safe composition requirements:

```text
- checkpoint flag can only be raised, never demoted;
- duplicate checkpoint store promotes an existing non-checkpoint slot;
- duplicate non-checkpoint store cannot demote an existing checkpoint slot;
- pin count and checkpoint flag coexist;
- recovery store-and-pin and checkpoint promotion happen under one atomic;
- no after-the-fact second-lock promotion.
```

This is the interleaving analysis P.11C should carry forward.

---

## Scope recommendation

Proceed to draft P.11C scope as a complete live scene-change integration phase.

Include:

```text
1. Add/populate per-instance Cnr3SceneChangeConfig.
2. Use _with_scene_change for branch-c and branch-d recursive computes.
3. Do not force detection on fresh-start/floor without CMS clarification.
4. Thread scene_change_detected into every computed-output store.
5. Use checkpoint store/promotion atomically under AS2.
6. For recovery holes, checkpoint per-hole when cut is detected.
7. Add KDT fields sufficient to prove detection/reset/store/checkpoint.
8. Prove with synthetic live harnesses.
9. Add a small selftest only if duplicate checkpoint promotion through the
   exact live helper path is not already covered or cannot be proven live.
10. Defer D-SUM-14, concise telemetry, hot-zone/prune/hole-fill telemetry,
    and real-footage campaign until after P.11C.
```

---

## Potential CMS-GAP to surface explicitly

There is one possible CMS-GAP:

```text
If CMS requires literal scene-change detection on branch-a/fresh-start or
floor-fresh-start, the current detector needs a previous filtered output and
therefore cannot run honestly without a defined predecessor policy.
```

Recommended resolution for P.11C:

```text
Scene detection applies to live recursive computes with a valid previous
filtered output. Fresh-start/floor outputs retain their existing fresh-start
semantics and checkpoint policy unless CMS defines otherwise.
```

---

## Bottom line

P.11C should be scoped as:

```text
complete live scene-change integration:
    detection + reset + checkpoint promotion + recovery-hole handling + proof KDT
```

not as a narrow call-site switch.

It should not include:

```text
D-SUM-14;
new concise telemetry;
real-footage campaign;
fmParallel stress validation;
production diagnostics gates.
```

Those are correctly deferred until after P.11C.
