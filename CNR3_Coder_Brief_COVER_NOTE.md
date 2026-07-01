# Cover note — marshalling / typed-pointer investigation (for the coder)

Two documents attached:

1. **CNR3_Coder_Brief_Marshalling_Investigate_and_Assess_v1.md** — the brief. Read this first.
2. **CNR3_PixelPath_DataFlow_Map_and_TypedPointer_Approach_v0_2.md** — the designer's source-derived
   data-flow map (buffer inventory, dependency graph, per-function detail). The brief's companion; read
   alongside §4-5 of the brief.

**What this is:** an INVESTIGATE-AND-ASSESS request, not a patch order. We profiled the production build
and found ~50% of per-frame time is native<->scalar plane marshalling (unpacking pixels into
std::vector<int> and repacking), while the denoise maths is <10% and the cache manager <3%. We want your
independent read of the finding and the proposed direction against the real committed source, reported
back in the normal form — BEFORE any code is written.

**The three things we most want from you:**
1. **Verify** the data-flow findings from source — the nine overlapping-lifetime buffers, and especially
   the two-luma-role split (full-res luma is a pass-through; downsampled luma is the blend guide — these
   must not be conflated). Correct anything we have wrong.
2. **Confirm or challenge the vectorisation finding (brief §3).** We added /arch:AVX2 and it changed
   NOTHING in the profile. Our reading: the per-sample `cnr3_load_native_plane_sample` CALL inside the
   copy loop is an auto-vectorisation wall. Please check with `/Qvec-report:2` on the hot loops and tell
   us what MSVC actually reports. This is the single most decision-relevant item.
3. **Assess the lever ladder and our lean.** We lean toward the typed-row-pointer in-place rewrite
   (Lever 3) as the real target — it removes the copy AND unlocks the AVX2 we committed — but recommend
   Lever 0 (luma passthrough) as the safe first STEP to prove the loop. Tell us if the source suggests a
   better ordering.

**Ground rules (unchanged):** nothing here changes CMS design or any invariant; it is an implementation
optimisation, value-preserving, gated by the P.1A-P.11B selftests (56/56, dev-trace ON) and measured
against a fresh AVX2 baseline. The .vpy/.bat harnesses are the designer's deliverable, not yours. No
source changes yet — the patch scope follows your report.

Report back: findings / assessment / recommended first step + its proof & measurement plan / risks /
questions.
