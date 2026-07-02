# CNR3 — 8-BIT VHS HOT-PATH REVIEW — INVESTIGATE & REPORT (no code)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigation with a decision at the end. NO code changes. Read the real post-3a.2 YUV420P8
profile + source, sort the candidate list below into memory-bound vs arithmetic-bound, and REPORT a
recommended sequence. Output chooses the next lever(s).
**Target module:** `cnr3_frame_processing.cpp`. **Build:** x64, /arch:AVX2, YUV420P8 (the 99% production case).
**Governing docs:** CMS07.15 (no design change), FI-10, CNR3_Validation_Policy_recorded_v1.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2. Cumulative ~-55% (93,914 -> ~41,850).

---

## 1. Why this review (and the load-bearing finding it must respect)

The overhead era is over: 3a.1's -37% came from removing the per-sample CALL (a compute cost). 3a.2
vectorised the 16-BIT conversion loop (first C5001 in the codebase) but the profile was CONFIRMED 8-bit
(YUV420P8) so that loop never ran; the total was FLAT. The **8-bit conversion loop itself stayed SCALAR
(reason 501)** — so we have NOT actually measured a vectorised 8-bit copy. Our working HYPOTHESIS is that
the residual 8-bit marshalling is MEMORY/ALLOCATION-bound (the copy + the per-frame std::vector<int>
alloc), not arithmetic-bound — but that is an INFERENCE, not yet a measurement. This review's job is to
TEST that hypothesis against the real profile and sort the candidates accordingly, so we squeeze the
8-bit path (fps compounds for users chaining many filters) WITHOUT wasting effort on arithmetic wins that
memory-boundedness would render flat.

## 2. Deliverable: the current post-3a.2 YUV420P8 profile shape

We already have four post-3a.2 runs on the YUV420P8 clip (last session). Reconstruct the leaf table in
ABSOLUTE samples (NOT %), noise band ~+/-1,300 on a 3500f run. For each significant cnr3 leaf report:
self samples, what the loop BODY does, and — the key classification — is its cost dominated by
MEMORY TRAFFIC (byte moves, int-buffer round-trips, allocation) or ARITHMETIC (compute the vectoriser
could accelerate)? Known leaves to classify (verify from source, don't assume):
```text
cnr3_copy_native_plane_to_scalar_buffer  (~7,000+2,800)  the unpack; 8-bit loop is scalar (501)
cnr3_process_chroma_plane_from_downsampled_luma (~6,700) the BLEND; largest real-math leaf, untyped/buffered
cnr3_stage_scalar_plane_to_native_bytes  (~4,700)        the repack (scalar->native)
cnr3_downsample_luma_plane_to_chroma_grid (~2,600)       the tap-average (small leaf)
std::vector<int>::resize + ntdll                          per-frame allocation churn (~1,500-2,200 resize)
cnr3_detect_scene_change_from_scalar_planes               per-FRAME (hot in frequency), not a top leaf
```

## 3. The candidate menu to sort (each -> memory-attacker or arithmetic-attacker, + risk)

```text
A. __restrict on the 8-bit copy loop (GAIS + our idea).
   Add __restrict to source-byte / dest-int pointers so MSVC can vectorise the 8-bit conversion (currently
   501). DIAGNOSTIC VALUE: this directly TESTS the memory-bound hypothesis. If a vectorised 8-bit copy is
   still flat -> memory-bound CONFIRMED. If it helps -> the copy was partly aliasing/compute-bound.
   Low risk (source plane vs owned int-buffer genuinely don't alias — but VERIFY no in-place/staging caller
   overlaps). Value-safe (pure copy, no math change).

B. Allocation pooling / resize-hoist.
   The per-frame std::vector<int> alloc+free is pure recurring waste (identical size every frame). Pool the
   scalar buffers on the instance / frameData, size-on-first-use, reuse across frames. Removes the resize
   samples AND (per GAIS) hoisting the throwing alloc out of the kernel MAY unblock vectorisation of the
   loops left in the function (try/alloc can inhibit MSVC vectorisation). Memory/allocation attacker.
   Moderate risk (buffer lifetime, thread-safety under fmUnordered per-instance). Value-safe (lifetime only).

C. Buffer ELIMINATION / native->native / pass fusion.
   The structural play: stop materialising the int buffer where a pass could read native bytes directly or
   consume the prior pass in place -> fewer memory touches. Highest ceiling, highest risk (crosses pixel-path
   logic; interacts with scene-change/downsample consumers that expect materialised int planes). Needs full
   value-identity proof. Memory attacker.

D. EXACT SIMD downsample (replaces GAIS's REJECTED VPAVGB).
   GAIS's two-level VPAVGB is REJECTED: measured +0.375-code upward BIAS (max 1 code, 0 on flats) that
   COMPOUNDS through the temporal recursion to ~+1.9 codes at k=0.8, ~+7.5 at k=0.95 (visible chroma drift).
   The SAFE form: widen bytes to 16-bit (vpmovzxbw), add four taps + 2, >>2, pack (vpackuswb) = BIT-EXACT
   (a+b+c+d+2)>>2. Arithmetic attacker on the SMALL (~2,600) leaf — likely low total impact, but cheap+safe
   so worth it under the every-cycle goal IF the review says the leaf is arithmetic-bound. Value-safe ONLY
   in the exact form; P.4A is the arbiter.

E. Scene-change local-accumulator (GAIS + our revision).
   Accumulate into a LOCAL int64 (not through stats.diff_total) + inline the abs -> vectoriser hygiene on a
   PER-FRAME loop. CORRECTION from last session: this is hot in FREQUENCY (runs every frame), not "cold" —
   worth doing under the every-cycle goal. Arithmetic attacker. CAUTION: keep exit semantics identical —
   do NOT move the early-exit to per-row if any P-series test reads diff_total EXACTLY (per-row overshoots
   the reported total even though the boolean decision is unchanged). VERIFY against the selftests first.

F. blend (3c) typed/native path.
   The largest real-math leaf, still untyped/buffered. The response-table GATHER is the hard part (data-
   dependent). GAIS floats a vpshufb SIMD-LUT for the 256-entry 8-bit table — INTERESTING but needs multi-
   stage construction (vpshufb selects from 16 bytes/lane) and prototyping; not a slam dunk. Mixed
   memory+arithmetic; the review should say which dominates before we pick the 3c approach.
```

## 4. What to report

```text
1. The post-3a.2 YUV420P8 leaf table (absolute samples), each leaf tagged MEMORY-bound or ARITHMETIC-bound
   with the reason from the loop body.
2. For each candidate A-F: does the real profile predict it will MOVE the total, and by roughly how much?
   (Distinguish "cheap+safe, small win, worth it for cumulative fps" from "no measurable effect".)
3. A recommended SEQUENCE, evidence-first: which to do next, which to defer, which to drop.
4. Specifically resolve the open question: is the 8-bit copy memory-bound? (Candidate A is the direct test —
   recommend whether to run it as the first experiment.)
5. Anything the profile reveals that ISN'T on this menu.
```

## 5. Constraints

```text
- NO code in this step — investigate and report only.
- ABSOLUTE samples, not percentages (percentages misrank when the total shrinks).
- Value-identity is non-negotiable; P-series is the arbiter; VPAVGB-as-written is REJECTED (see D).
- Respect the validation policy (defend-at-source / trust-downstream / clamp-at-output) — no candidate
  weakens the Tier-1 source gate or a shared proof helper.
- "Unsure whether a leaf is memory- or arithmetic-bound" -> say so; recommend the cheap test (e.g. A) rather
  than committing to the expensive structural change (C) on an assumption.
```
