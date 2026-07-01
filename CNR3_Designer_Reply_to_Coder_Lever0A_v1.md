# Designer reply to coder — marshalling arc: assessment accepted, order proposed, one confirmation needed

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Re:** your investigate-and-assess report on the pixel-path marshalling arc

Strong report — it did exactly what an investigate-and-assess pass should. Three things you gave us that
change or sharpen the plan, and then a proposed order and one question back to you before I write the
first patch scope.

## What we accept from your report

1. **Lever 0A over the original Lever 0 — accepted unreservedly. You caught a real correctness bug.**
   The original "direct `current_source_y -> destination_y` copy" would write luma to the destination
   BEFORE U/V validation, breaking the all-or-nothing output discipline the late-failure selftests pin
   (a late U/V failure must leave destination planes unchanged). Your **Lever 0A** — stage the native
   luma passthrough into `staged_y` and commit at the existing final commit point with U/V — preserves
   that atomicity while still deleting the full-res scalar round-trip. The first patch will be specified
   as staged passthrough, not an early destination write. This is the most important outcome of your
   report.

2. **The redundant inner staging allocation — confirmed as a real, separate lever.** Your trace of
   `copy_scalar_buffer_to_native_plane` allocating an inner `resolved_bytes` before writing into the
   outer staging vector answers the brief's open question: yes, there is redundant staging when the
   destination is already a throwaway buffer. We're calling that **Lever 0B** and treating it as its own
   step (see order below).

3. **Vectorisation calibration — accepted.** Agreed the `/Qvec-report:2` run must be Release `/O2
   /arch:AVX2` (Debug's AVX2 + `/Od` is not a meaningful test), target TU `cnr3_frame_processing.cpp`.
   And your honest split is well taken: simple copy/passthrough/stage loops are likely to vectorise once
   typed and inlined, but the chroma blend (data-dependent table lookups, validation branches, int64
   arithmetic, multiple source arrays) is much less certain and may need loop-shape work or fall through
   to Path B. We are NOT assuming MSVC vectorises the full blend for free. That report is owed from OUR
   side (coordinator at the machine) before Lever 3 is scoped.

One note on your Lever 1 caution (per-instance scratch as a possible future footgun if the project moves
to fmParallel): reasonable discipline, and we agree with your *recommendation* to prefer the staging
cleanup over instance scratch near-term. For risk-weighting context (not to override your caution): our
current reading, backed by the VS docs, is that fmParallel is likely UNREACHABLE for a recursive cache
filter (it forbids shared mutable state during getFrame, which the cache inherently has), so per-instance
scratch is less of a live hazard than it might appear — but "make the threading precondition explicit or
defer" is still the right posture. So Lever 1 stays deferred/optional, decided by measured residual cost.

## Proposed order (each step measured against the fresh AVX2 baseline before the next)

```text
Step 1  Lever 0A  — staged native full-res luma passthrough. ALONE.
        Smallest safe change; proves the propose->selftest->profile->measure loop end-to-end
        before anything touches the pixel maths. Modest but real win expected.

Step 2  Lever 0B  — remove the redundant inner resolved_bytes staging allocation
        (copy_scalar_buffer_to_native_plane, when the destination is already a throwaway
        staging buffer), keeping all-or-nothing semantics. Its own patch, its own measurement.

Step 3  /Qvec-report:2 evidence run (coordinator, at the machine; NOT a code change).
        Release /O2 /arch:AVX2, target cnr3_frame_processing.cpp. Confirms or challenges the
        vectorisation-wall hypothesis with real compiler output. Can run in parallel with 1/2.

Step 4  Lever 3  — typed-row-pointer in-place rewrite. The real target (removes the copy AND
        unlocks AVX2). Scoped only AFTER Step 3's evidence, because what the report shows about
        the chroma-blend loop shapes how Lever 3 is written. Levers 1/2 remain optional, decided
        by measured residual cost after 0A/0B.
```

Rationale for the order: front-load the safe, provable wins (0A, 0B) that establish the loop and cannot
break the maths; gather the vectorisation evidence in parallel; defer the big proven-code rewrite
(Lever 3, touching P.7A-P.11B) until it is backed by measurement, not inference.

## The one confirmation we need before the first patch scope

You flagged (your question 2) whether Step 1 should be Lever 0A alone or 0A + the staging cleanup (0B)
together, and you leaned separate. **We also lean separate** — 0A alone keeps the measurement clean and
attributable (bundling would make it impossible to tell which change bought what).

**Please confirm: 0A alone as the first patch, 0B as a separate follow-up — OR, if your source read now
shows the two are entangled in the same code path** (e.g. 0A's staging change naturally exposes/forces
the 0B redundancy in the same spot, making separation artificial), **say so and we'll reconsider bundling.**

Once you confirm the separation, we (designer) write the formal **Lever 0A patch scope** — the
R-PROCESS-21 propose/review/selftest/profile contract — and you implement from that. Build-context is now
fully verified on your side (both projects x64-only + AVX2 Release/Debug, selftest compiles
`cnr3_frame_processing.cpp` directly so it gates the real production code), so there's nothing blocking
the scope once the 0A/0B ordering is confirmed.

## Proof gate that will apply to Step 1 (for your awareness; the scope will restate it)

```text
- Build Debug + Release (both projects).
- Four-way selftest, dev-trace ON:
    Debug normal 56/56
    Release normal 56/56
    Release forced-fail 55/56 (exit 1)
    Release verbose 56/56
- P.11B still proves: luma copied unchanged; U/V blended vs previous filtered output;
  invalid late-sample paths leave destination planes UNCHANGED (the atomicity your 0A protects).
- Re-profile against the AVX2 baseline; report: total per-frame change, change in
  load_native_plane_sample self-time, change in stage/commit time, whether destination-Y
  staging left the hot profile.
- Value-preserving or it is wrong. No CMS/invariant change.
```
