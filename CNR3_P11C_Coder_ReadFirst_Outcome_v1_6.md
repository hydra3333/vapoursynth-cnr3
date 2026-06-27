# CNR3 — P.11C Coder Read-First: Outcome & Resolutions

**Version:** v1.6 (versioned in lockstep with the P.11C scope at coordinator request. History: v1.0 first coder round; v1.1 second-round review + threshold resolution §7A; v1.2 §7B arithmetic/colourspace verification + RESOLVED depth-scaling accuracy decision; v1.6 = lockstep bump + post-D.5 source confirmed byte-identical for the P.11C files, and the depth-scaling decision rationale carried to the coder as 'consistency with the P.2A parameter-scaling decision')
**Date:** 2026-06-27
**Status:** Record of the coder's read-first response to the P.11C approach-brief, the designer's
verification of its claims against CMS07.13 + source, and the resolutions carried into the P.11C scope.
This is the coder-read-first step output (the pattern that made D.4/D.5 land cleanly). NOT yet a scope.
**Controlling:** CMS07.13 / Production Spec v2.11 / Document A v3.5 / Document B v3.5.1 / DELTA v4.12.
**Source:** coder file coder_CNR3_P11C_read_first_response.md (read in full).
**Source baseline (confirmed 2026-06-27):** latest ZIP = CMS07-D.5-recovery-pin-survives-bounded-prune-proof; P.11C-relevant files byte-identical to the analysis baseline (frame_processing.cpp/.h, arAllFramesReady.cpp, cache_core.h, cache_core_selftest.cpp).

---

## 1. Headline outcome

The coder CONFIRMED the brief's core read (P.11C is live wiring-and-compose, not new pixel maths) and
made ONE important correction plus surfaced ONE real CMS-GAP (now resolved against CMS §6.3). The phase
is confirmed bigger than "switch one call site" but still bounded: complete live scene-change integration
(detection + reset + checkpoint promotion + recovery-hole handling + KDT proof), with diagnostics/
telemetry/real-footage all correctly deferred.

## 2. Confirmations (coder agreed with the brief)

- 2.1 TWO entry points exist; `_with_scene_change` is the intended live entry point WHERE a real previous
  filtered output exists. Do NOT collapse the two public entry points (fresh-start/floor lack a predecessor).
- 2.2 Cnr3SceneChangeConfig = {scene_change_threshold:int64, scene_chroma:bool}. scene_chroma=false means
  luma-only detection; true adds chroma U/V diffs. Recommended live default scene_chroma=false (luma-only
  safer: CNR3 is a chroma denoiser, noisy analogue chroma risks false positives).
- 2.3 Detector compares current source vs previous FILTERED (downsampled luma always; chroma only if
  scene_chroma); diff_total accumulates abs diffs; scene_change = diff_total > threshold. The `>` (strict)
  matters: equality stays non-cut.
- 2.4 Reset already implemented; no new pixel reset algorithm; P.11C proves it runs live.
- 2.5 Summary bits exist; scene_change_detected is the authoritative "this computed frame is a cut" bit;
  store routing consumes it.
- 2.6 Checkpoint store exists with promote-only semantics (see correction in §3.1).
- 2.7 Live store wrapper currently lacks a checkpoint flag (the real seam for final-target stores) -- CONFIRMED.
- 2.8 Live path currently defers (p11c_called=0, scene_change_deferred=1); P.11C flips these.

## 3. CORRECTIONS the coder made (carry into scope)

### 3.1 The store seam is BROADER than "the final-target wrapper"
The brief framed task B as "thread the cut bit into cnr3_store_live_output_frame_for_authoritative_return."
The coder's correction: scene-change status must be wired into EVERY computed-output store that can produce
a cut, not only the final-target wrapper:
  - branch-c predecessor-present TARGET store;
  - branch-d recovery per-HOLE stores;
  - branch-d recovery TARGET store.
And the right primitive differs by site:
  - isolated final-target store -> store_checkpoint_owned_frame (or the wrapper carrying a force_checkpoint);
  - recovery stores that ALSO pin -> the combined store-and-pin path with is_checkpoint, e.g.
    store_owned_frame_and_record_pin(..., is_checkpoint=true, ...) or the planned-hole variant
    store_recovery_plan_hole_owned_frame_and_record_pin(..., is_checkpoint=true, ...).
The REQUIREMENT (not the function name) is: the checkpoint flag/promotion decision enters the AS2 store
atomic; it must NOT be a later separate promotion under a second lock. (CMS 6.6 monotonic promote-only.)

### 3.2 Decision logic should be CENTRALISED, not scattered
store_as_checkpoint = grid_checkpoint || scene_change_detected, computed via one helper
(cnr3_live_output_frame_should_store_as_checkpoint(...)) rather than duplicating `grid || scene` across
branch-c and branch-d. Coder prefers a small store-request struct / named boolean over another anonymous
bool param:
    struct Cnr3LiveOutputStoreRequest { int frame_number; bool force_checkpoint; };

### 3.3 Threshold is PER-INSTANCE CONFIG, not a compile-time flag
Compile-time flags are for diagnostics gates, not live scene-change behaviour. Put Cnr3SceneChangeConfig in
instance/filter data so later plugin parameters feed it without rewiring the live path. Use a named CMS/
proof default now; expose plugin params in a later option-surface phase. Initial 8-bit 4:2:0 luma-only
default proposed: threshold = chroma_width * chroma_height * 4 * 20 (≈ avg luma diff of 20 over the chroma
grid; 4 == 1 << (sub_sampling_w + sub_sampling_h)). For proofs, construct deterministic cut/no-cut thresholds.

## 4. CMS-GAP surfaced and RESOLVED (designer verified against CMS)

CODER'S GAP: the detector needs a previous filtered output; branch-a fresh-start and floor-fresh-start have
NO previous filtered predecessor, so cut detection cannot run "honestly" there without a defined predecessor
policy. Coder's recommended resolution: treat fresh-start/floor as detection-NOT-APPLICABLE for P.11C while
preserving their existing checkpoint policy; raise CMS-GAP only if CMS requires literal detection there.

DESIGNER VERIFICATION (against CMS07.13): the resolution is CMS-CONSISTENT and it is NOT actually a gap once
§6.3 is read:
  - CMS §1 states a cut produces "a fresh start ... a THIRD reset point alongside frame 0 and the lower-bound
    floor." So frame-0 / floor / cut are EQUIVALENT reset points -- a fresh-start/floor frame is already what
    a cut produces (chroma-copy, no predecessor dependency). Running cut detection there is meaningless (no
    predecessor) AND unnecessary (already a reset).
  - CMS §6.3 establishes checkpoints by a GRID (every CHECKPOINT_INTERVAL-th frame AND frame 0) PLUS detected
    cuts. So a fresh-start/floor frame's checkpoint status comes from the GRID/establishment rule (§6.3), NOT
    from cut detection. P.11C does not need to (and must not) drive their checkpoint status via detection.
RESOLUTION (carried into scope): fresh-start (branch-a, N==0) and floor-fresh-start are DETECTION-NOT-
APPLICABLE in P.11C. P.11C wires detection ONLY at live recursive computes that have a valid previous
filtered output (branch-c target; branch-d hole computes; branch-d target). No CMS change needed; record
as a settled scope boundary.

CRITICAL CLARIFICATION (three DISTINCT frame categories -- do NOT conflate):
This resolution is ONLY about WHERE DETECTION CAN RUN. It does NOT say scene-change frames skip the
checkpoint flag. The three fresh-start categories are different frames governed by different rules:

  (1) DETECTED-CUT frame K (the first frame of a NEW scene):
      - reached via a NORMAL RECURSIVE compute -- it HAS a predecessor (output[K-1]);
      - the detector runs, compares K's source vs that predecessor, returns "cut";
      - output[K] is then computed as a fresh-start (chroma-copy, chain severed)
        AND STORED WITH THE CHECKPOINT FLAG SET.  <-- CMS 6.4 / 9.2. P.11C WIRES THIS.
      - i.e. a scene change DOES make the new frame a checkpoint, via DETECTION. (Coordinator's
        memory is correct.) The "fresh-start" is the RESULT of detecting the cut, not a reason
        detection was skipped.

  (2) FRAME 0:
      - no predecessor exists at all -> detection CANNOT run (nothing to diff);
      - it is a fresh-start for STRUCTURAL reasons;
      - it IS a checkpoint -- but by the §6.3 GRID rule (frame 0 is always a grid checkpoint),
        NOT by cut detection.

  (3) BOUNDED-SEARCH FLOOR (max(0,N-B)):
      - no in-window predecessor -> detection CANNOT run there;
      - fresh-start for STRUCTURAL reasons;
      - checkpoint ONLY if it happens to land on the §6.3 grid; not made a checkpoint by detection.

So "detection-not-applicable at fresh-start/floor" refers to categories (2) and (3) ONLY -- the
STRUCTURAL fresh-starts that have no predecessor to detect against. It does NOT touch category (1):
a detected cut is reached through a recursive compute, detection runs, and the new-scene frame becomes
a checkpoint per §6.4. P.11C's whole job INCLUDES making detected-cut frames checkpoints.

## 5. Proof strategy (coder-agreed shape)

Layer 1 -- live synthetic harness (primary), constructed cuts, asserting: detection used; cut detected;
reset output used; recursive blend NOT used; output chroma == current source chroma; output stored/promoted
as checkpoint; later recovery finds that frame as an anchor. Branch coverage:
  - branch-c no-cut control;
  - branch-c cut at target N;
  - branch-d cut at intermediate hole K;
  - branch-d cut at final target N;
  - later recovery using a detected-cut frame as anchor.
May be split into proof subphases for tighter increments.
Layer 2 -- selftest ONLY if the live harness cannot deterministically force duplicate checkpoint promotion.
Candidate: store K non-checkpoint -> store duplicate K as checkpoint -> assert promoted, identity preserved,
non-checkpoint duplicate does not demote. NOTE: some may already be covered by G.12A -- do NOT duplicate
unless P.11C needs a narrower proof through a specific combined store-and-pin helper. (Designer: verify G.12A
coverage before adding a selftest.)

Expected KDT (per-frame): p11c_called, scene_change_deferred, scene_change_detection_used, scene_chroma_used,
scene_change_threshold, scene_change_diff_total, scene_change_samples_examined, scene_change_detected,
scene_change_reset_output_used, recursive_chroma_blend_used, store_as_checkpoint, scene_checkpoint_store,
checkpoint_promoted, resulting_slot_is_checkpoint. Per-hole (recovery): hole=K, hole_scene_change_detected,
hole_scene_change_reset_output_used, hole_store_as_checkpoint, hole_resulting_slot_is_checkpoint.

## 6. fmParallel interleaving (coder-supplied, carry into the design note)

- checkpoint flag raise-only, never demote;
- duplicate checkpoint store promotes an existing non-checkpoint slot (frame data unchanged);
- duplicate non-checkpoint store cannot demote an existing checkpoint;
- pin count and checkpoint flag coexist under cache lock;
- recovery store-and-pin and checkpoint promotion under ONE atomic;
- no after-the-fact second-lock promotion.
Three benign interleavings the coder enumerated: (A stores K non-cp; B detects K cut -> promotes), (A stores
K cp; B computes K non-cp -> no demote), (A pins/adopts K while B promotes K -> pin+flag coexist).

## 7. Open items for the designer before drafting the scope

1. RESOLVED (designer checked source 2026-06-27): G.12A coverage CONFIRMED. The existing selftest
   `cnr3_cache_core_selftest_checkpoint_store_flag_lifecycle` already proves the §6.6 monotonic-checkpoint
   primitive at cache-core level: store frame non-checkpoint -> store checkpoint duplicate returns
   `duplicate` AND raises checkpoint_count 0->1 (PROMOTION proven) AND preserves frame identity (first-in-
   best-dressed: original data kept, only flag raised) -> and the never-demote direction. CONCLUSION: P.11C
   does NOT need a new cache-core selftest for the promotion primitive. P.11C proves the LIVE WIRING (a
   detected cut actually routes to that proven promote path) via the live synthetic harness. (Matches the
   coder's own "do not duplicate G.12A" guidance.)
2. CONFIRM the exact recovery per-hole store call shape from the LATEST post-D.5 source (the coder flagged
   that exact signatures/call shapes need the latest ZIP before patch prep -- approach is fine without it,
   patch is not).
3. DECIDE the named CMS/proof threshold default to bake in (the dimension-scaled luma-only formula is the
   coder's proposal; confirm or set the proof value).
4. CONFIRM the store-request-struct vs named-bool shape for threading force_checkpoint (coder prefers a
   small struct / named bool; designer to ratify the public shape).
5. DECIDE proof subphasing (single P.11C proof vs split increments per branch coverage line in §5).

## 7A. SECOND-ROUND coder review + threshold resolution (2026-06-27)

The coder reviewed the v1.0 scope: APPROVED directionally, with three pre-patch tightenings, all folded
into scope v1.1:
1. Threshold default generality -- do not bake a literal proxy; express via subsampling + bit depth.
2. KDT: emit scene_change_not_applicable (not scene_change_deferred=1) for frame-0/floor paths, so the
   CMS-settled boundary does not look like unfinished work.
3. Delivery: source-confirmed file/function layout proposal FIRST, patch SECOND; and subphase the proof
   (P.11C.1..5) to preserve the D.1-D.5 increment discipline.

THRESHOLD RESOLUTION (designer, answering coordinator's "use the CNR2 default"):
Use the VERIFIED vsCnr2 derivation, NOT the coder's invented width*height*4*20 proxy. Recorded + verified
against vsCnr2.cpp (Role Handover), and consistent with the detector's accumulation units:
    diff_max = (scdthr * w * h * max_pixel_diff / 100) << (depth - 8)
    max_pixel_diff = scene_chroma ? ((219 + 224*2) >> (subw+subh)) : 219
    (full_w, full_h = FULL-FRAME/LUMA dims -- vsCnr2 uses vi.width*vi.height, NOT chroma grid;
     verified against vsCnr2.cpp source 2026-06-27. Default scdthr=10.0 (Create_vsCnr2 AsFloat(10.0)).)
Decision-equivalent to vsCnr2 (detector per-SAMPLE early-stop vs vsCnr2 per-ROW -- proven identical,
diff_total monotonic). Chosen over the proxy because it (1) exposes the familiar scdthr knob, (2)
reproduces real CNR2 scene-detection decisions, (3) correctly rescales max_pixel_diff for luma-only vs
scene_chroma and for depth/subsampling. This SUBSUMES coder tightening #1 using the real formula.
OPEN: ratify the exact CNR2 default scdthr scalar (the formula is settled; the default value rides on it).

## 7B. Arithmetic / colourspace-depth handling -- VERIFIED (2026-06-27)

Coordinator asked whether the scene-change threshold arithmetic handles colourspaces (420/422/444) and
bit depth with the same large-integer care as the blend maths. Verified against source -- YES, two
separate but consistent mechanisms, both int64:

BLEND MATHS (cnr3_frame_processing.cpp): cnr3_blend_scale_for_bit_depth uses shift2 = bits_per_sample<<1,
blend_scale = int64{1} << shift2 (8-bit->16, 10-bit->20, 16-bit->32; the <<32 case REQUIRES int64), weight
is int64 (cnr3_calculate_combined_blend_weight). Bit-exact match to vsCnr2 (shift2=depth<<1, shift=1LL<<shift2,
shift1=shift>>1).

SCENE-CHANGE THRESHOLD / ACCUMULATION: diff_total is int64; each luma_diff computed in int64 with
<< luma_scale_shift (= subsw+subsh, the colourspace term) applied in int64; cnr3_add_scene_diff has an
OVERFLOW GUARD (defensive improvement over vsCnr2, which had none). diff_max carries depth via <<(depth-8)
and colourspace via max_pixel_diff's >>(subsw+subsh) (scene_chroma branch), computed in int64.

COLOURSPACE flows through TWO terms that must stay paired: (1) subsampling subsw+subsh -- 420 -> <<2,
422 -> <<1, 444 -> <<0 -- appears in BOTH the per-sample luma accumulation (shifts luma UP) AND in
max_pixel_diff for the chroma ceiling (shifts DOWN); (2) bit depth -- blend shift2=depth<<1 and threshold
<<(depth-8), both int64.

IMPORTANT -- THE THREE-LAYER ACCURACY STANCE (coordinator corrected an over-flat earlier answer; this is a
RECORDED project decision, see governing rule below). CNR3 is a FAITHFUL CORRECT redevelopment of vsCnr2,
NOT a byte-identical clone (coordinator's steer: "most accurate method, compute cost is OK"). vsCnr2 fidelity
is stated in THREE layers, not loosely:
  (a) BLEND arithmetic        -> BIT-EXACT (shift2=depth<<1, shift1=shift>>1, int64 weight). Reproduce exactly.
  (b) RESPONSE-CURVE build     -> BIT-EXACT (int((m/2)*(1+cos)), integer m/2 is the curve's defined shape).
  (c) PARAMETER pre-scaling    -> DELIBERATELY MORE ACCURATE than vsCnr2: CNR3 uses round-to-nearest
        (value*peak+127)/255 vs vsCnr2's integer peak/255 which TRUNCATES at 10/12/14-bit (identical at
        8/16-bit). e.g. value=128 @10-bit: vsCnr2=512, CNR3=514.
GOVERNING RULE: accuracy upgrades ONLY where vsCnr2 is ACCIDENTALLY LOSSY (exact at 8/16-bit, diverges at
10/12/14-bit against evident intent -- only known instance: peak/255 parameter scaling). Where vsCnr2's
integer arithmetic is DEFINITIONAL (strength/2 curve quantisation; >>shift2 blend with shift1 rounding;
+2>>2 luma box-average), reproduce BIT-EXACT. When in doubt: treat as definitional, reproduce exactly, ASK
THE DESIGNER -- do not let the coder default.

*** RESOLVED (coordinator, 2026-06-27): apply the MOST-ACCURATE proportional round-to-nearest depth
scaling (consistent with the P.2A parameter-scaling decision). See decision note at end of 7B. Original
question retained for context: the scdthr->diff_max derivation contains vsCnr2's <<(depth-8) integer-factor DEPTH scaling. The live
detector takes a PRE-COMPUTED scene_change_threshold in native accumulation units (verified: it does NOT
itself apply <<(depth-8); the shift lives entirely in the derivation helper P.11C will build). The recorded
"deliberate divergences" list classifies the parameter-scaling case but does NOT classify the scene-change
threshold derivation -- it is UNCLASSIFIED under the governing rule, because the derivation was always
"deferred to plugin config" and is net-new in P.11C. <<(depth-8) is STRUCTURALLY the same integer-factor
depth-scaling family as the peak/255 case the rule says to UPGRADE. So the question is: for the threshold
derivation, does P.11C (i) reproduce vsCnr2's <<(depth-8) BIT-EXACT [treat as definitional -- the threshold
is a sensitivity knob, not pixel output, so coarser depth scaling may be harmless], or (ii) apply the MOST-
ACCURATE method per the governing rule [round-to-nearest proportional depth scaling, consistent with the
P.2A parameter-scaling decision]? Designer to classify definitional-vs-accidentally-lossy and decide BEFORE
the helper is written. NOTE: this is a SENSITIVITY THRESHOLD (a cut/no-cut decision boundary), not pixel
output -- the accuracy stakes differ from pixel maths, which may argue either way; that is the designer call.

CONCLUSION (corrected): P.11C adds NO new blend/accumulation arithmetic (those are settled: (a)/(b) bit-exact,
the accumulation is int64 + subsampling-aware as verified). The ONLY new arithmetic is the scdthr->diff_max
derivation helper -- and its DEPTH-scaling accuracy classification is the OPEN question above. The layout
proposal must demonstrate the helper's output for 8-bit 4:2:0 luma-only, 8-bit 4:2:0 scene_chroma, and a
high-bit-depth case -- and the high-bit-depth case is exactly where the definitional-vs-accurate choice shows.

## 8. Boundary (agreed both sides)

P.11C does NOT include: D-SUM-14, new concise telemetry, hot-zone/prune/hole-fill telemetry, real-footage
campaign, fmParallel stress validation, production diagnostics gates. All deferred to the diagnostics arc /
campaign / fmParallel arc per the sequencing in DELTA v4.12 §5.

---

*End of CNR3 P.11C Coder Read-First Outcome v1.6.*
