# CNR3 — TIER-2 CHROMA-UNPACK FUSION — INVESTIGATE & REPORT (no code)

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** investigation with a feasibility verdict at the end. NO code changes. Determine whether the blend
can consume chroma DIRECTLY from native bytes (eliminating the 4 pre-unpacked scalar chroma buffers), and
report SAFE / UNSAFE / PARTIAL with the reasons. Only if SAFE do we then scope a patch.
**Target:** `cnr3_frame_processing.cpp` — the chroma unpack call sites (~L1677-1698), the blend
`cnr3_process_chroma_plane_from_downsampled_luma` (~L2788), and its inner arithmetic (~L1959+).
**Governing docs:** CMS07.15, FI-10, CNR3_Validation_Policy_recorded_v1, the cross-verified blend formula.
**Builds on committed:** AVX2 + 0A + 0B + 3a.1 + 3b.1 + 3a.2 + A-lite + C1 + Repack + F/3c + Staging. Cumulative ~-79% (-> ~19,470).

---

## 1. The target and why it MIGHT fuse (C1 precedent)

Post-Staging the #1 leaf is `cnr3_copy_native_plane_to_scalar_buffer` (~1,874 / ~1,592 self). It runs to
unpack FOUR chroma planes native->scalar int (current_u, current_v, previous_u, previous_v; ~L1677-1698),
each into its own scalar buffer, which then feed `cnr3_process_chroma_plane_from_downsampled_luma` as
`current_source_chroma` / `previous_filtered_chroma`. This is a materialisation the blend consumes at FULL
chroma resolution. C1 did the analogous thing for luma (read native taps directly in the downsample,
skipped the full-res int buffer) for -24.5%. The QUESTION: can the blend read chroma samples directly from
native bytes per (x,y), eliminating the four unpack buffers?

## 2. Why this is HARDER than C1 — the specific things to resolve

C1 fused a leaf (downsample) whose ONLY consumer was the tap-average. The chroma is different in ways that
must be checked before we call fusion safe:

```text
Q1 — MULTIPLE CONSUMERS / MULTIPLE READS. In the blend, current_source_chroma feeds BOTH:
     (a) the chroma_signed_diff = current_source_chroma - previous_filtered_chroma (-> response table), AND
     (b) the (shift - weight) * current_source_chroma term of the blend.
     previous_filtered_chroma similarly feeds both the diff and the weight*previous term.
     So each chroma sample is read (at least) TWICE per output. Reading native bytes directly means either
     reading+widening the same native sample twice, or widening once into a local. Confirm this is a local
     (register) reuse, NOT a re-read of memory — and that it stays value-identical. (Cheap; just confirm.)

Q2 — VALIDATION TIER INTERACTION. F/3c just added a Tier-1-preserving VALIDATION PRE-PASS in the blend
     (it scans the 4 input planes for out-of-range before the fused compute loop, to preserve no-partial-
     output). Today those 4 inputs are SCALAR planes already validated by the unpack's Tier-1 gate. If the
     blend instead reads native chroma DIRECTLY, then the blend itself becomes the Tier-1 SOURCE BOUNDARY
     for chroma (the native read is the first touch). The pre-pass would then need to validate NATIVE
     chroma samples (8-bit type-guaranteed; 16-bit OR-accumulate scan) — i.e. the Tier-1 gate MOVES from
     the unpack into the blend for chroma. Confirm this is coherent: is it clean for the blend to own the
     chroma Tier-1 gate, and does the existing pre-pass shape accommodate native reads? (This is the main
     design question.)

Q3 — DOWNSAMPLED-LUMA INPUTS ARE NOT CHROMA. The blend also takes current/previous DOWNSAMPLED_LUMA
     (produced by C1's bridge, already scalar int, at chroma-grid resolution). Those are NOT unpacked by the
     4 chroma copies and are NOT candidates for this fusion — they stay as-is. Only the 2 chroma roles
     (current_source, previous_filtered) x (U,V planes) are in scope. Confirm the luma inputs are untouched.

Q4 — PREVIOUS_FILTERED_CHROMA PROVENANCE. current_source chroma is a raw VS source plane (Tier-1 native).
     But previous_filtered_chroma is the PREVIOUS OUTPUT frame's chroma (the recursive feedback). Confirm
     its native-plane availability at blend time: is it a VS frame plane we can get a native byte view of
     (like the source), or is it only available as the already-produced scalar/native buffer? If it is not
     cleanly available as a native plane at the blend call site, fusion may be PARTIAL (fuse current_source
     only, keep previous_filtered unpacked) or UNSAFE. THIS IS THE LIKELY BLOCKER — check it first.

Q5 — STRIDE / SUBSAMPLING. chroma native planes have their own stride_bytes and are at chroma resolution.
     Confirm the native byte offset (x*storage_bytes + y*stride_bytes) is available for each chroma plane at
     the blend site (the Cnr3ConstNativePlaneByteView / views.* were used by the unpack — are they still in
     scope at the blend call, or consumed/dropped?).
```

## 3. The verdict we need

```text
Report ONE of:
  SAFE     — the blend can read all chroma roles directly from native bytes; the Tier-1 gate cleanly moves
             into the blend's pre-pass; all 5 questions resolve cleanly. -> we scope the full fusion.
  PARTIAL  — e.g. current_source chroma fuses but previous_filtered is not cleanly a native plane at blend
             time (Q4) -> we scope fusing only what is safe (still eliminates ~half the ~1,874).
  UNSAFE   — a blocker (likely Q4 or Q2) makes the fusion change behaviour or break the validation model
             -> we do NOT pursue it; -79% stands and we move on.
For each of Q1-Q5, state the source finding and whether it's clean. Value-identity is non-negotiable: the
fused blend must produce byte-identical output to today (same samples, same arithmetic, same reject-before-
publish). The cross-verified blend formula does NOT change — only WHERE the chroma samples are read from.
```

## 4. Constraints

```text
- NO code this step — investigate and report the SAFE/PARTIAL/UNSAFE verdict + the Q1-Q5 findings.
- Do NOT modify the shared cnr3_copy_native_plane_to_scalar_buffer primitive (it has other callers -
  luma/downsample). Fusion, if done, is a blend-private native read path, same discipline as C1/0B/Staging.
- The blend arithmetic (int64 weight/convex-combination/shift1/directionality) is LOCKED - unchanged.
- Tier-1 chroma guarantee MUST be preserved (VHS glitch defence) - if fusion moves the gate into the blend,
  it must still detect out-of-range native chroma and reject-before-publish (P.11B no-partial-output).
- Calibration: the leaf is ~1,874 (top leaf but small vs the arc's earlier leaves). A clean full fusion
  could reclaim most of it (C1-style); partial reclaims less. This is the LAST material-ish target - after
  it, remaining leaves are ~800-1,400 (Lever B pooling territory) and the arc is into diminishing returns.
  So: only pursue if the investigation says SAFE/PARTIAL AND clean. If UNSAFE or messy, -79% is a fine place
  to stop the fusion work.
```

## 5. Report format

Per-question (Q1-Q5) source finding + clean/not. Then the SAFE/PARTIAL/UNSAFE verdict with the load-bearing
reason (expected to hinge on Q4 - previous_filtered_chroma native availability - and Q2 - Tier-1 gate move).
If SAFE/PARTIAL, note the eliminable buffer count (all 4 chroma copies, or just the 2 current_source ones).
