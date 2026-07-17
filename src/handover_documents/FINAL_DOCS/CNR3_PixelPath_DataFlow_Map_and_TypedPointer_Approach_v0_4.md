# CNR3 pixel-path data-flow map + typed-pointer optimisation — DESIGNER working note (W3D) — v0.2 (adds AVX2 requirement + Path-A auto-vectorisation contract)

> **ARC-COMPLETE NOTE (2026-07-02):** the marshalling arc this map guided is now substantially COMPLETE at ~-80% (see DELTA v4.23). The unpack/downsample/blend/staging materialisations described below have been typed, inlined, or eliminated (C1 removed the full-res luma downsample buffer; F/3c inlined the blend; Staging inlined the scalar->native conversion). The nine-buffer inventory and per-frame allocation cost described here is the PRE-arc state; the allocation leaf that remains (~587) is Lever B (pooling), the only un-taken headroom. The dependency analysis, two-luma-role split, and edge-clamp geometry still hold.


> **STALENESS NOTE (added post-3b.1).** This map predates the marshalling levers. Two things are now
> historical: (1) the buffer inventory still lists `current_luma_storage` / buffer 1 — **Lever 0A REMOVED
> that** (full-res luma is now a native passthrough staged directly into `staged_y`, no int round-trip).
> (2) The lever ladder here is the ORIGINAL plan; actual progress: AVX2 committed (neutral), 0A committed
> (-28%), 0B committed (staging cleanup, flat), 3a.1 committed (typed unpack, -37%), 3b.1 committed
> (inlined downsample, flat-but-cleaner), 3a.2 in progress (Tier-1 range-check hoist). The vectorisation
> WALL is empirically confirmed (vec-report 506 = per-sample call, then the validation branch after
> call-elimination). Validation posture is now governed by CNR3_Validation_Policy_recorded_v1
> (defend-at-source / trust-downstream / clamp-at-output). Everything ELSE in this map — the dependency
> analysis, two-luma-role split, nine-buffer overlap (minus buffer 1), edge-clamp geometry — still holds.

**Purpose.** The load-bearing artifact for the marshalling optimisation arc (FI-10). Maps, from source,
exactly which native reads feed which scalar buffer and which buffer feeds which computation, so a
typed-pointer / in-place rewrite can be scoped without guessing what is live when. This note + the
profiler findings + the problem statement + the proposed approach are the basis of the coder scope.

Source: `cnr3_frame_processing.cpp`, the caller-supplied real-frame pixel-composition body
(~L1387-1745), reached live via `cnr3_process_caller_supplied_vapoursynth_frame_triplet_impl`.

---

## 1. The profiler finding (why we are here)

VS2026 CPU sampling, NORMAL build, real 576p25 footage, single instance. Stable across 200/3500 frames,
`-r 1` and no-`-r 1`, cache-on/off:

- `cnr3_load_native_plane_sample` ~17% SELF; the native<->scalar copy chain
  (`cnr3_copy_native_plane_to_scalar_buffer` + `cnr3_stage_scalar_plane_to_native_bytes` +
  `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid`) ~50% of per-frame time.
- Denoise MATH < ~10% (downsample average, chroma blend, response-table lookup).
- Whole cache manager < ~3% (store/prune/pin/hot-zone). NOT a bottleneck.
- Sub-finding (no `-r 1`, fmUnordered): out-of-ORDER requests put ~half the frames through recovery,
  which RE-RUNS this same pixel path — so the marshalling cost is paid (1 + recovery_rate) times per
  output frame under real (unordered) execution. (Cache overhead stayed <3% throughout; the fmUnordered
  serialisation means the cache lock is uncontended — the recovery is a request-ORDER effect, separate.)

**One-line problem statement.** Every plane of every frame is unpacked from native VS bytes into a
`std::vector<int>` scalar buffer, processed, and (for outputs) repacked to native bytes. The unpack +
repack + the nine per-frame allocations cost ~5-6x the arithmetic they wrap.

---

## 2. Buffer inventory (nine scalar int buffers, all per-activation, all on the C++ stack)

All are `std::vector<int>`, declared at L1387-1395, allocated L1397-1490 via
`cnr3_allocate_scalar_plane_storage`, lifetime = this one function call (one activation). Sizes:
LUMA-grid = luma_width x luma_height; all others = chroma_width x chroma_height.

| # | buffer | size grid | filled by | native source |
|---|--------|-----------|-----------|---------------|
| 1 | current_luma_storage           | luma   | `copy_native_plane_to_scalar_buffer`        | views.current_source_y |
| 2 | current_downsampled_luma_storage | chroma | `downsample_native_luma_plane_to_scalar_chroma_grid` | views.current_source_y (downsampled) |
| 3 | previous_downsampled_luma_storage | chroma | `downsample_native_luma_plane_to_scalar_chroma_grid` | views.previous_filtered_y (downsampled) |
| 4 | current_u_storage              | chroma | `copy_native_plane_to_scalar_buffer`        | views.current_source_u |
| 5 | current_v_storage              | chroma | `copy_native_plane_to_scalar_buffer`        | views.current_source_v |
| 6 | previous_u_storage             | chroma | `copy_native_plane_to_scalar_buffer`        | views.previous_filtered_u |
| 7 | previous_v_storage             | chroma | `copy_native_plane_to_scalar_buffer`        | views.previous_filtered_v |
| 8 | output_u_storage               | chroma | written by the U consumer (below)           | — (output) |
| 9 | output_v_storage               | chroma | written by the V consumer (below)           | — (output) |

Plus three OUTPUT staging vectors (`staged_y/u/v`, `std::vector<std::uint8_t>`) inside
`stage_scalar_plane_to_native_bytes` — the repack scratch.

Note the asymmetry, stated CAREFULLY (an earlier draft got this wrong — corrected against source):
there are TWO distinct luma roles, via TWO separate reads of the source luma plane.
- **Full-res luma (buffer 1, current_luma)**: unpacked from `views.current_source_y` (L1496) and, its
  ONLY other reference, staged back to native output UNCHANGED (L1723 -> destination_y). Verified: buffer
  1 is NOT read by the downsample, scene-change, or chroma blend. CNR3 passes the luma plane through
  unmodified (it filters CHROMA; luma is copied source->dest). So buffer 1 is a native->int->native
  round-trip with no math — a real Lever-0 candidate (see below).
- **Downsampled luma (buffers 2,3)**: produced by `downsample_native_luma_plane_to_scalar_chroma_grid`
  reading the NATIVE `views.current_source_y` / `views.previous_filtered_y` DIRECTLY (L1511/L1531 — NOT
  buffer 1). These ARE the GUIDE: `process_chroma_plane_from_downsampled_luma` takes current+previous
  downsampled luma as its first two args and, inside the blend, computes the signed luma difference
  (L694/L713, int64) that drives the response-table lookup modulating the chroma blend. **Luma guides
  the blend — via the downsampled buffers 2/3, read natively, NOT via buffer 1.**

So luma is BOTH passed through (buffer 1) AND used as a guide (buffers 2/3), through two independent
reads of the source luma plane. Lever 0 touches ONLY the passthrough (buffer 1 -> output); it must NOT
touch the native downsample read that feeds the guide, which is real math.

---

## 3. Data-dependency graph (who reads what)

```
NATIVE INPUTS (VS frame planes, via views.*)                    NATIVE OUTPUTS (views.destination_*)
  current_source_y  ─┬─> [copy]      -> (1) current_luma ───────────────────────> destination_y  (UNCHANGED passthrough)
                     └─> [downsample]-> (2) current_downsampled_luma ─┐
  previous_filtered_y ─> [downsample]-> (3) previous_downsampled_luma ┤
  current_source_u  ─> [copy] -> (4) current_u ─┬───────────────────┤
  current_source_v  ─> [copy] -> (5) current_v ─┤                   │
  previous_filtered_u ─> [copy] -> (6) previous_u ┤                 │
  previous_filtered_v ─> [copy] -> (7) previous_v ┤                 │
                                                  │                 │
        scene-change detect (optional) reads:  (2)(3)(4)(5)(6)(7)  <- SIX live at once
                                                  │                 │
        if scene_change:  output_u/v := copy of current_u/v (4)(5)  │
        else: process_chroma_plane_from_downsampled_luma:           │
              U-pass reads (2)(3)(4)(6) -> writes (8) output_u ──────┼─> destination_u
              V-pass reads (2)(3)(5)(7) -> writes (9) output_v ──────┴─> destination_v
```

### Live-range table (which buffers must coexist) — this decides what CAN and CANNOT collapse

| stage | live buffers |
|-------|--------------|
| scene-change detect (worst case) | 2,3,4,5,6,7 (six) |
| U-pass                           | 2,3,4,6 -> writes 8 |
| V-pass                           | 2,3,5,7 -> writes 9 |
| stage-out                        | 1 -> dest_y; 8 -> dest_u; 9 -> dest_v |

**Consequence for buffer COLLAPSE (confirms the earlier read):**
- The two downsampled-luma buffers (2,3) are live through the whole compute — cannot be freed early,
  cannot be merged.
- 4,5,6,7 are all live together during scene-change detect — so **U and V cannot share a buffer**
  (the earlier "U/V sequential" guess was WRONG; scene-change reads all four at once).
- Therefore within-activation buffer-count reduction is essentially UNAVAILABLE. The nine buffers are
  needed concurrently. **This is why the lever is ELIMINATION (typed pointers), not reorganisation.**

---

## 4. What each leaf function does (the marshalling to eliminate, and the math to preserve)

- `cnr3_copy_native_plane_to_scalar_buffer` (5x here) — the UNPACK: reads native bytes (8-bit 1 byte /
  16-bit 2 byte LE) via `cnr3_load_native_plane_sample`, widens to int, writes the int buffer. Pure
  marshalling, no math. THIS is the ~17%-self hot leaf.
- `cnr3_downsample_native_luma_plane_to_scalar_chroma_grid` (2x) — reads native luma, box-averages the
  subsampling tap group `(a+b+c+d+2)>>2` (P.4A rule), writes int chroma-grid buffer. Marshalling +
  a small average; the average is real math to preserve, the native read is marshalling to fold in.
- `cnr3_process_chroma_plane_from_downsampled_luma` (2x) — the ACTUAL DENOISE: signed current-minus-
  previous diffs -> P.1A response-table lookup -> P.3A int64-accumulator weighted blend -> narrow/clamp
  -> write output int buffer. ~2.5% self. This is the math that MUST stay bit-identical.
- `cnr3_detect_scene_change_from_scalar_planes` — reads the six int buffers, accumulates scaled diffs,
  compares to threshold. Math to preserve.
- `cnr3_stage_scalar_plane_to_native_bytes` (3x) — the REPACK: reads int buffer, narrows to native
  bytes, writes native output. Pure marshalling, no math. ~10-17% of the chain.

---

## 5. Proposed approach (levers, easiest->hardest; all value-preserving, selftest-gated)

**Governing rule for ALL levers (the one correctness invariant): the arithmetic is untouched.** Read
through a typed native pointer, WIDEN IMMEDIATELY to the existing accumulator width (int / int64 as the
current code uses), compute in that width, narrow-and-clamp only at the final native store. Computing in
the narrow (sample) type is the one mistake that corrupts and is exactly what the P.3A/P.5A int64
selftests catch. Every lever is "value-preserving or it is wrong", and the P.1A-P.11B selftests decide it.

**Lever 0 (isolated win, do first — but NARROW and selftest-decided, not "obviously safe"): full-res
luma passthrough only.** Buffer 1 (current_luma) is unpacked then staged back UNCHANGED (§2), and is
read by NOTHING else — NOT the guide. So the native->int->native round-trip of buffer 1 can be replaced
by a DIRECT native luma copy (row-wise memcpy honoring stride) `current_source_y -> destination_y`.
Deletes one full unpack + one full repack + one buffer, on the largest plane (luma is full-res, ~4x a
chroma plane's area) that does NONE of the math. STRICT SCOPE: touch ONLY buffer 1's passthrough. Do
NOT touch the SECOND, native read of the luma plane by `downsample_native_luma_plane_to_scalar_chroma_grid`
(buffers 2/3) — that read feeds the blend GUIDE and is real math; conflating the two would corrupt the
blend. RISK: low but NOT zero — the change is bit-identical only if the current unpack->repack round-trip
is itself lossless (native->int->native with no scaling/clamp that alters luma). The P.11B selftest
("luma staged and copied unchanged from current source luma") is what PROVES identity; ship only if the
four-way stays 56/56 and luma output is byte-identical.

**Lever 1 (easy, allocation cost): per-activation buffer reuse / pool.** Remove the nine per-frame
allocations. Under the CURRENT mode (fmUnordered: one thread per instance at a time) instance-scoped
reusable scratch is thread-SAFE within an instance; note two interlaced INSTANCES still run concurrently,
so scratch must be per-instance (not static/global). If CNR3 ever targets fmParallel (unlikely for a
recursive cache filter — see FI note), it must become per-ACTIVATION. Removes the ~allocation/fault cost
(part of the ~10% ntdll self), keeps the copy. Keep any pool lock to checkout/checkin only, never over
the plane loop.

**Lever 2 (medium): fuse unpack+process+repack.** Where a plane is unpacked, processed, repacked in
sequence, fuse to fewer passes (process during read; write native output directly from the computed
value). Cuts memory traffic and buffers. Moderate rewrite of the traversal; value-preserving.

**Lever 3 (hardest, biggest win): typed-row-pointer in-place.** Template the plane traversal on sample
type (`const uint8_t*` / `const uint16_t*` with stride), do the diff/lookup/blend inline in the wide
accumulator, write straight to native output. Deletes BOTH copies and the int buffers entirely. Est.
~1.5-2x on this path, plus likely SIMD auto-vectorisation upside (tight contiguous typed loops vectorise
where vector<int> indexing does not) — MEASURE it. Touches P.7A-P.11B; must reproduce the exact 8/16-bit
results the P.3A/P.5A/P.8A selftests pin.

Levers are a STAIRCASE, independently shippable: 0 and 1 are safe quick wins that don't touch the math;
2 and 3 progressively eliminate the copy. Re-profile after each so the gain is MEASURED, not projected.

---

## 5a. Build-target decision: AVX2 required (2026-07-01)

The project targets **AVX2 as a hard requirement** (Intel Haswell 2013 / AMD Excavator-Zen 2015 or
newer). Rationale: effectively all x86-64 desktop/laptop silicon since ~2015 (and most since 2013) has
AVX2; both target machines (Ryzen 3900X, i4670) clear it; the excluded population is negligible. Set via
`/arch:AVX2` (MSBuild `<EnableEnhancedInstructionSet>AdvancedVectorExtensions2</...>`), Release AND Debug
for parity.

BUILD-SCOPE (do not miss): the solution has TWO projects — `cnr3.vcxproj` (the plugin DLL) AND
`cnr3_cache_core_selftest.vcxproj` (the correctness gate). BOTH must get `/arch:AVX2` (Release+Debug),
or the selftest validates the maths under DIFFERENT codegen than the shipping DLL — which undercuts the
"AVX2 is neutral" proof. Also: the project + solution were reduced to x64-ONLY (Win32/x86 platforms
removed from cnr3.vcxproj, cnr3_cache_core_selftest.vcxproj, and the .slnx) — a VS plugin is x64-only
(VS itself is 64-bit), and this removes the footgun of ever building a non-AVX2 32-bit DLL by accident.

CONSEQUENCES (decided consciously, not drifted into):
- `/arch:AVX2` is WHOLE-DLL and enables AVX+AVX2+FMA+BMI/BMI2 (broader than the name). It gives the
  auto-vectoriser 256-bit integer vectors and the full AVX2 integer instruction set — the point of the
  flag for the Path-A auto-vectorisation below.
- On a non-AVX2 CPU the DLL HARD-CRASHES (illegal instruction), possibly at load before any message —
  NOT graceful degradation. Acceptable given the requirement, but it means the requirement MUST be
  documented (header + README + release notes) as the only thing between an old-CPU user and a cryptic
  crash. Graceful CPU-detect-and-exit is the HolyWu per-ISA-dispatch pattern (Path B), explicitly NOT done.
- Proof obligation: after adding the flag, the four-way selftest MUST stay 56/56 (same integer maths,
  wider instructions; FMA can perturb FLOAT rounding but we are integer — the selftest proves neutral).
- Sequencing: land `/arch:AVX2` as its OWN isolated commit BEFORE the typed-pointer work (flag + the
  three doc notes + selftest 56/56), then take a fresh AVX2 baseline profile. Every later lever is then
  measured against the AVX2 baseline, so the typed-pointer gain is attributed to the CODE change, not
  accidentally credited with the flag's win. Two variables, separated.
- Document in THREE places: (a) code header note (build_config.h) so the flag is not later removed as
  accidental; (b) README minimum-requirements line ("Requires AVX2 — Haswell 2013 / Excavator 2015 or
  newer; will not run on older CPUs"); (c) release notes on the FIRST release that introduces it.

## 5b. Path A (auto-vectorisation) — giving the VS2026 (MSVC) compiler its best chance, and PROVING it took it

The chosen SIMD route is AUTO-vectorisation (Path A): write the hot loops so MSVC `/O2` + `/arch:AVX2`
vectorises them itself — NO explicit intrinsics/VCL, no per-ISA files, no runtime dispatch (that is
Path B, deferred; reference impl = HolyWu VapourSynth-EEDI3/Deblock with Agner Fog's Vector Class
Library, INTEGER types only, and emphatically NOT -ffast-math since we are bit-exact).

CODE-SHAPE REQUIREMENTS for the coder (auto-vectorisation only fires if the inner loop is "obviously
parallel" to the optimiser):
- Per-sample work must be INLINED into the loop body. The current `cnr3_load_native_plane_sample`
  CALLED PER SAMPLE is an auto-vectorisation WALL — a function call per element cannot vectorise. Typed
  native pointers with the load/widen/compute/store inlined is the enabling change.
- Inner loop over a row: no loop-carried data dependence, no branches inside the hot span (handle
  edge/clamp outside the span, or via masking), a trip count the compiler can reason about, contiguous
  access via `const uint8_t*`/`const uint16_t*` + stride.
- Keep the wide accumulator (int/int64) — vectorises fine; the widen-on-load rule (§5/§6) still holds.

PROVE-IT-VECTORISED (a DELIVERABLE, not a hope — "written to vectorise" != "vectorised"):
- Build with `/Qvec-report:2` during development. MSVC then prints, per loop, "vectorized" OR "not
  vectorized" WITH a reason code (e.g. 1105 = loop contains a function call -> inline it). The hot loops
  must report vectorized; any "not vectorized" reason is reported back so it can be addressed. (Noisy;
  development-time diagnostic, removed after.)
- Cross-check: the hot loop's disassembly shows PACKED integer ops (vpaddw/vpmullw/pmaddwd over
  xmm/ymm) not scalar (add/imul).
- Verdict: the before/after profile shows the load/copy self-time drop. (Vectorised-but-memory-bound is
  possible; the profile is the "did it actually get faster" answer the vec-report cannot give.)

Settings delta for Path A: `/O2` (have it) + `/arch:AVX2` (§5a). `/fp:` is irrelevant (integer). That
is the whole flag change; the rest is code shape.

## 6. Correctness net (what proves each lever value-preserving)

The existing pixel-path selftests are the acceptance gate — a lever that changes any of these results is
wrong by definition:
- P.1A response-table vectors; P.2A table geometry/scaling (8/16-bit).
- P.3A weighted blend: shift maths, shift1 round-half-up, **int64 accumulator** at 8/16-bit bounds.
- P.4A downsample: `(a+b+c+d+2)>>2` rounding, 4:2:0/4:2:2/4:4:0/4:4:4 shapes, edge-tap CLAMP.
- P.5A signed-diff -> table lookup -> blend composition (no unsigned wrap; signed lookup before blend).
- P.6A per-plane traversal (strides, padding not treated as active width).
- P.8A native byte access: 8-bit 1 byte, 9-16-bit 2 byte LE, `x*storage_bytes` column offset.
- P.11B real-frame composition: luma staged/copied UNCHANGED; chroma blended vs previous filtered output.
Any typed-pointer rewrite runs the four-way selftest and must stay 56/56 (production/normal build).

---

## 7. Scope-shaping notes for the coder scope (draft next)

- Order: Lever 0 first (isolated, near-zero risk, proves the harness + measurement loop), then decide 1
  vs 3 based on its measured gain. Do NOT bundle all levers into one patch — each is separately provable
  and separately measurable; bundling defeats the "re-profile after each" discipline.
- Every lever is R-PROCESS-21 (proven pixel-path code): propose -> review-against-source -> four-way
  selftest 56/56 -> re-profile. No CMS design change (the maths/spec are unchanged; this is an
  implementation optimisation of the P.7A-P.11B realisation).
- Measurement is part of the deliverable: before/after profile per lever (same footage, `-r 1`, NORMAL
  build) so the gain is observed. The profiling build settings (DebugInformationFormat + COMDATFolding
  off) are already in the .vcxproj; the end-of-arc cleanup (keep PDB, restore COMDAT folding, tune
  release flags for the new code shape, incl. verifying SIMD) is a separate final step.
- Open question the coder review should answer from source: does `cnr3_stage_scalar_plane_to_native_bytes`
  currently do a redundant heap `std::vector<std::uint8_t>` staging allocation that could also be removed
  independently of the typed-pointer work? (Seen at L1712+; a possible extra lever-1-class win.)
- AVX2 + auto-vectorisation is EXPLICIT scope for the coder (§5a/§5b): (1) `/arch:AVX2` lands first as an
  isolated commit with selftest 56/56 + the three doc notes + a fresh baseline profile; (2) the pixel-path
  levers are written for Path-A auto-vectorisation (inline per-sample work, branch-free contiguous inner
  loops); (3) the coder MUST prove vectorisation with `/Qvec-report:2` and report any "not vectorized"
  reason codes — "written to vectorise" is not acceptance; "vectorised" (report) + "faster" (profile) is.
