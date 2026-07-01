# CNR3 CODER BRIEF — pixel-path marshalling: INVESTIGATE, ASSESS, REPORT (not a patch order)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigate-and-assess brief. This is NOT a scope to implement. Read the findings, check them
against the real committed source, assess the proposed direction, and REPORT BACK with your independent
read (agree / disagree / refine / risks / questions). A patch scope is written only AFTER your report.
**Companion artifact:** `CNR3_PixelPath_DataFlow_Map_and_TypedPointer_Approach_v0_2.md` (the designer's
source-derived data-flow map — read it alongside this brief; it has the buffer/dependency detail).
**Controlling docs:** CMS07.15 (unchanged — nothing here is a design change), FI-10 (Future
Investigations v7.15) is the banked finding this brief acts on.

---

## 1. What prompted this (the finding)

A VS2026 CPU-sampling profile of the NORMAL production build, real 576p25 footage, single instance,
established where per-frame time actually goes. Stable across 200 and 3500 frames, cache-on and
cache-off, and now also across the pre-AVX2 and post-AVX2 builds:

- **Native<->scalar plane MARSHALLING is ~50% of per-frame time.** Specifically
  `cnr3_load_native_plane_sample` ~17-18% SELF (the innermost per-sample native-byte -> int load),
  inside `cnr3_copy_native_plane_to_scalar_buffer` ~24% and the downsample chain ~32%, plus
  `cnr3_stage_scalar_plane_to_native_bytes` ~17% (the reverse int -> native repack).
- **The actual denoise MATH is < ~10%** (downsample average ~4% self, chroma blend ~2.5% self,
  response-table lookup inlined/negligible).
- **The whole cache manager is < ~3%** (store/prune/pin/hot-zone). NOT a bottleneck.
- Per-frame `std::vector<int>` allocate/resize/free churn is visible and plausibly feeds part of the
  ~10% ntdll self-time (first-touch page faults).

In plain terms: the plugin spends ~5-6x more time UNPACKING pixels into int buffers and REPACKING them
than doing the denoise arithmetic those buffers exist for. The int-buffer design was correct for
CORRECTNESS (uniform int maths, 8/16-bit independence — it is what the P.1A-P.11B selftests prove); it
is just expensive per frame.

### 1a. Sub-finding — multi-thread makes it WORSE (context, not a task)

Running the SAME sequential clip WITHOUT `-r 1` (VS thread pool active, the real encode regime) put
~half the frames through `cnr3_complete_live_recovery` (~52% of total) EVEN on sequential footage. The
filter is `fmUnordered` (one thread per instance at a time, but requests arrive in ARBITRARY order), so
frame N can be requested before predecessor N-1 is produced -> recovery fires. Recovery LOGIC is cheap
(<1%); it is expensive because it RE-RUNS the same pixel path (the same ~50% marshalling) to rebuild
the missing predecessor. So real parallel cost is ~(1 + recovery_rate) x the per-frame marshalling.
Consequence for this arc: any speedup to the pixel path pays off DOUBLY under real multi-threaded
encoding. (Whether that recovery rate is inherent to fmUnordered request-ordering or a tunable cache
mismatch is a SEPARATE future diagnostics question — NOT part of this brief.)

---

## 2. Build change already committed (context you build on)

- The project is now **x64-only** (Win32/x86 configs removed from both `cnr3.vcxproj` and
  `cnr3_cache_core_selftest.vcxproj`, and from the `.slnx`).
- **AVX2 is a hard requirement**, set via `/arch:AVX2`
  (`<EnableEnhancedInstructionSet>AdvancedVectorExtensions2</...>`) on Release AND Debug, BOTH projects.
  Documented as required (Haswell 2013 / Excavator-Zen 2015 or newer) in the build_config header, README,
  and release notes. On a pre-AVX2 CPU the DLL hard-faults (no graceful degradation) — this is a
  deliberate, documented decision.
- **Proven neutral:** four-way selftest stayed **56/56** with AVX2 on (all pixel-math P-series tests
  passed), so AVX2 changes no result. (Note: `keystone_request_plan_dev_trace_proof` /
  `K.1A`/`K.1B`-style dev-trace tests only run when `CNR3_KEYSTONE_DEV_TRACE` is compiled in; with it
  off the suite shows 54/56 for that reason alone — a known test-harness conditionality, not a
  regression. Run proofs with dev-trace ON.)

---

## 3. THE KEY VECTORISATION FINDING (please verify this yourself)

We added `/arch:AVX2` and re-profiled. **It changed NOTHING** — `cnr3_load_native_plane_sample` stayed
~17.7% self, the marshalling stayed ~50%, identical to the pre-AVX2 build within noise.

Our READING (assess it): AVX2 bought nothing because the hot loop **cannot auto-vectorise as currently
structured** — `cnr3_load_native_plane_sample` is CALLED PER SAMPLE inside the copy loop, and a
function call per element is an auto-vectorisation WALL. The compiler cannot use the wider instructions
where the hot work is. So the flag is a latent prerequisite whose payoff is unlocked only when the loops
are reshaped to let the compiler (or explicit SIMD) actually vectorise.

We would like you to CONFIRM OR CHALLENGE this from the actual build:
- Build a hot TU with `/Qvec-report:2` and report what MSVC says about the copy/downsample/stage loops
  ("vectorized" vs "not vectorized" + the reason code). Our hypothesis predicts "not vectorized" with a
  function-call / unvectorizable-construct reason on the per-sample-call loops.
- If your read differs (e.g. it IS vectorising and the cost is memory-bound, or there is a different
  wall), say so — that changes the whole approach and we want to know before scoping a patch.

---

## 4. The data-flow map (summary — full detail in the companion doc)

Source: `cnr3_frame_processing.cpp` ~L1387-1745, the caller-supplied real-frame pixel-composition body.

- NINE per-activation `std::vector<int>` scalar buffers, lifetimes OVERLAP (scene-change detection reads
  six at once: current+previous downsampled-luma + current/previous U + current/previous V). So
  within-activation buffer COUNT reduction is essentially unavailable — the algorithm needs them
  concurrent. The lever is ELIMINATION of the copy, not reorganising buffers. (Please verify the
  live-ranges from source; correct us if any buffer is freed earlier than we think.)
- TWO luma roles via TWO reads of the source luma plane, and this distinction is load-bearing:
  1. FULL-RES luma (buffer 1) is unpacked then staged back to output UNCHANGED (read by nothing else) —
     a pure native->int->native round-trip with no math. CNR3 filters chroma; luma is passthrough.
  2. DOWNSAMPLED luma (buffers 2/3) is read NATIVELY by the downsample and IS the blend GUIDE (the
     signed current-vs-previous luma diff drives the response-table that modulates the chroma blend).
  Any luma optimisation must touch ONLY role 1 and NOT role 2. Confirm this split from source.

---

## 5. Proposed direction (CANDIDATE levers, easiest->hardest — for your assessment, not yet ordered work)

All value-preserving, all gated by the existing P.1A-P.11B selftests. Governing rule: read through a
typed native pointer, WIDEN IMMEDIATELY to the existing int/int64 accumulator, compute wide, narrow/clamp
only at the native store. (Computing in the narrow sample type is the corrupting mistake the int64
selftests catch.)

- **Lever 0 — full-res luma passthrough.** Replace buffer 1's native->int->native round-trip with a
  direct native luma row-copy (honoring stride) source_y -> destination_y. Touches only role-1 luma,
  NOT the guide. Smallest, most isolated; proves the whole loop (propose->selftest->re-profile). Risk is
  low but NOT zero — bit-identity holds only if the current round-trip is lossless; P.11B ("luma staged
  and copied unchanged") is the proof.
- **Lever 1 — buffer reuse / pool.** Remove the nine per-frame allocations. Under fmUnordered,
  per-INSTANCE scratch is thread-safe within an instance (intra-instance getFrame is serialized) — BUT
  two field-stream instances run concurrently, so scratch must be per-instance-or-narrower, never
  static/global. Removes allocation churn; keeps the copy.
- **Lever 2 — fuse unpack/process/repack** into fewer passes.
- **Lever 3 — typed-row-pointer in-place (biggest win).** Template plane traversal on sample type
  (const uint8_t* / const uint16_t* + stride), inline the per-sample load/widen/compute/store, write
  straight to native output — deletes the int buffers and both copies, AND (per section 3) is what lets
  the loop finally auto-vectorise under /arch:AVX2. Est. ~1.5-2x on this path. Touches the most proven
  code (P.7A-P.11B); must reproduce exact 8/16-bit results.

SIMD approach: we intend **Path A (auto-vectorisation)** — clean typed loops the MSVC optimiser
vectorises itself, proven with `/Qvec-report:2`. Explicit-SIMD (Path B: Agner Fog VCL / intrinsics,
per-ISA files + runtime dispatch, ref impl HolyWu VapourSynth-EEDI3/Deblock, INTEGER VCL types only, NO
-ffast-math since we are bit-exact) is the higher-ceiling fallback, DEFERRED behind measuring what Path A
delivers. Assess whether Path A is realistic for these loops or whether you foresee Path B being needed.

**DESIGNER LEAN (a recommendation, not a mandate — push back if the source says otherwise).** We think
the real prize is **Lever 3, the typed-row-pointer in-place rewrite**: it is the only lever that removes
the ~40% COPY cost (Levers 0/1 only remove specific round-trips or the ~10% allocation churn), AND per
section 3 it is the change that finally lets the hot loop auto-vectorise under the /arch:AVX2 we already
committed — so it captures the marshalling win and the latent SIMD win in one move, and it compounds
under multi-thread (every recovery re-run gets faster too). That is the DESTINATION we are aiming at.
BUT we still recommend **Lever 0 as the first STEP** — not because it is the main win (it is a modest
one), but because it is isolated, near-unbreakable (luma passthrough, no math), and it proves the whole
propose->selftest->re-profile->measure loop on something safe before we touch the proven P.7A-P.11B
pixel maths. So the lean is: Lever 0 first to prove the mechanism and the measurement, Lever 3 as the
real target, Levers 1/2 as optional intermediate wins only if the measurements say they earn their
keep. If your source read suggests a better ordering — e.g. that Lever 0's win is too small to bother
and we should go straight to a typed-pointer prototype on one plane — say so; that is exactly the kind
of assessment we want back.

---

## 6. WHAT WE'RE ASKING YOU TO DO (report, don't patch)

1. **Verify the findings against the committed source** — the nine-buffer live-ranges, the two-luma-role
   split (§4), and the marshalling structure. Correct anything we have wrong.
2. **Confirm or challenge the vectorisation-wall reading (§3)** with `/Qvec-report:2` output on the hot
   loops. This is the single most decision-relevant item.
3. **Assess the lever ladder (§5)** — is Lever 0 as isolated/safe as we think? Is the per-instance buffer
   scope for Lever 1 correct under fmUnordered + two instances? Any ordering you'd change? Any lever you'd
   drop or add? Any correctness risk we've missed relative to the P-series selftests?
4. **Recommend a first step and its proof/measurement plan** — we expect Lever 0 first (isolated, proves
   the loop, measurable against the fresh AVX2 baseline), but say so in your own terms.
5. **Flag the open question** we couldn't resolve from the map: does `cnr3_stage_scalar_plane_to_native_bytes`
   do a redundant heap `std::vector<std::uint8_t>` staging allocation (~L1712+) that could be removed
   independently? Assess from source.

Report back in the normal form (findings / assessment / recommended first step / risks / questions). No
source changes yet — the patch scope follows your report.

### Constraints (unchanged, apply to everything that follows)
- Every step is R-PROCESS-21 (proven code): propose -> review-against-source -> four-way selftest 56/56
  (dev-trace ON) -> re-profile against the AVX2 baseline. No step ships without 56/56 + a measured result.
- No CMS design change; no invariant change; the maths/spec are fixed. This is an implementation
  optimisation of the P.7A-P.11B realisation, nothing more.
- Value-preserving or it is wrong: the P.1A-P.11B selftests decide bit-identity, per profile and bit depth.
- The `.vpy`/`.bat` profiling + selftest harnesses are the DESIGNER's deliverable, not yours.
