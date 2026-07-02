# CNR3 — Development RESUME on the CMS07.15 cache + pixel + scene-change architecture
*(Coder restart introduction (v6.6) — paste this at the start of a new memoryless chat,
ahead of the attached handover pack files. This is a RESUME of an in-progress, proven build
that is past its cache-core milestone, through its entire real-frame pixel path on
caller-supplied frames, and through the cache↔pixel / getFrame KEYSTONE — whose live dispatch
is now COMPLETE for all four branches (cache-hit, fresh-start, predecessor-present, recovery),
and the **branch-(d) recovery arc is COMPLETE (D.1-D.5)**, AND the **P.11C SCENE-CHANGE ARC is CLOSED
(.1-.5)** — scene detection is wired+proven across branch-a/c/d, committed through CMS07-P.11C.5. It is
NOT a fresh start, it is NOT "early cache-core work," it is NOT pixel-path work, and neither the keystone
dispatch, the recovery arc, nor the scene-change arc is un-started or partial — read the state below
carefully. The live cache-pressure wiring arc (W.1→W.2→W.3) is COMPLETE and committed (CMS07-W.3, 55/55 + eviction-proof live harness PASS); the immediate next work is the **DIAGNOSTICS ARC** (D-SUM telemetry), sequenced before the real-footage campaign.
The prune logic + hot-zone machinery are already built and selftest-proven but have ZERO live callers in the
committed source — so the task is wiring, NOT a new algorithm (with ONE genuinely new primitive at W.1; see
below). The joint CMS sensibility/gap review (Step 0) that this wiring required is now CLOSED: it confirmed
the CMS against the post-P.11C.5 state and SETTLED the live wiring contract, which is recorded in the design
authority as CMS §7.4 (independent checkpoint-retention trigger), §7.5 (the combined locked store-and-prune
helper and its six-step order — when the store path prunes, and how it is safe against the active pin_list
and the arInitial->arAllFramesReady gap), and §7.6 (the arInitial hot-zone observation prerequisite for
unpinned produced output). Provenance: CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md. So you do NOT redo
Step 0; you IMPLEMENT its contract, single-activation scope (concurrent prune is the fmParallel arc). The
ordered work is: **W.1** the §7.4 checkpoint-retention trigger as a proven cache-core primitive + selftest
(the one new piece of logic — read-first/propose/review/prove like a K/D phase; your read-first + 3a proposal
is already produced and ACCEPTED, so resume at the patch step), then **W.2** hot-zone observation wiring at
arInitial, then **W.3** the combined live store-and-prune helper.
v6.7 supersedes v6.6. Build state ADVANCED by a full MARSHALLING-OPTIMISATION ARC (implementation-only; CMS DESIGN UNCHANGED at 07.15): twelve value-identical levers (AVX2, 0A, 0B, 3a.1, 3b.1, 3a.2, A-lite, C1, Repack, F/3c, Staging, E) reduced per-frame native<->scalar marshalling to ~1/5 of its cost — CUMULATIVE ~-80% (93,914 -> ~18,660 samples), all 56/56 four-way with P-series value-identity preserved, each separately profiled on YUV420P8. The arc is SUBSTANTIALLY COMPLETE. A VALIDATION POLICY was recorded+applied (defend-at-source Tier-1 / trust-downstream Tier-2 / bounded-by-construction Tier-3 / final-clamp-always). CODER-RELEVANT METHOD NOTES for any resumed optimisation work: (1) profiles are VS2026 CPU-sampling — read ABSOLUTE samples not %; noise band ~±1,300 on 3500f. (2) The winning mechanism throughout was CALL-CHAIN ELIMINATION (inline per-sample calls + hoist invariants + row-pointer form), not SIMD — 'didn't vectorise' does NOT mean 'flat' (A-lite proved this). (3) COMPUTE WIDE, NARROW AT STORE (int/int64 accumulators; the store rejects out-of-range, does not clamp). (4) the blend arithmetic is LOCKED and cross-verified (int64 weight=y_res*c_res -> previous_filtered; (shift-weight) -> current_source; +shift1 >>shift2; P.3A/P.5A are the arbiter) — do NOT alter it or narrow to 32-bit. (5) two external suggestions (VPAVGB, 32-bit accumulator) were REJECTED as value-corrupting — reproduce arithmetic from SOURCE verified against P-series, never from a reconstruction. NEXT-ACTION is a coordinator call: Lever B (allocation pooling — a buffer-LIFETIME change needing an fmUnordered thread-safety proof) OR pivot to the DIAGNOSTICS arc (D-SUM). [Prior v6.6 note:] v6.6 supersedes v6.5. Build state ADVANCED: the live cache-pressure wiring arc W.1→W.2→W.3 is COMPLETE (committed CMS07-W.3, 55/55 + eviction-proof live harness PASS); controlling CMS now CMS07.15 (additive §7.5 store-status contract); the next-action is the DIAGNOSTICS arc (D-SUM), before real-footage. [Prior v6.5 note:] v6.5 supersedes v6.4. Build state unchanged (P.11C arc CLOSED .1-.5, 53/53). Two advances: (1) the STEP 0
joint review is now CLOSED — the live cache-pressure wiring contract is SETTLED and recorded in the design
authority as **CMS07.14** (additive over 07.13: new §7.4 independent checkpoint-retention trigger, §7.5
combined-helper wiring contract, §7.6 arInitial observation prerequisite; provenance
CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md). So the controlling CMS is now **CMS07.14**. (2) The
next-action is therefore no longer "do Step 0" but the WIRING IMPLEMENTATION, starting at **W.1** (the §7.4
checkpoint-retention trigger primitive; cache-core + selftest; scope
CNR3_W1_Coder_Scope_checkpoint_retention_trigger_v1_0.md). v6.5 advances doc-set pointers to Spec v2.14 /
Document A v3.9 / Document B v3.9 / DELTA v4.14 / Future Investigations v7.13.4 / Diagnostics Plan v1.3. The
Design Alignment and Escalation Charter (section 0.0) is retained.)*

This chat **resumes** CNR3 development on the CMS07.14 architecture. The build is far advanced
and proven phase-by-phase: the cache core is complete and proven, the entire scalar pixel
decision pipeline is complete and proven, the native byte-buffer access/bridge layer is complete
and proven, the real-VapourSynth-frame pixel path is complete and proven on caller-supplied
frames, AND the getFrame/cache keystone live dispatch is COMPLETE for all four branches —
cache-hit (K.1F), fresh-start (K.1D), predecessor-present (K.1E.2/E.3), and recovery (the full
branch-(d) arc D.1-D.5). Your job is **P.11C scene-change uniform wiring across branch-a/c/d** — the
next phase — after first confirming the build state from the repository.

Do not treat older CNR3 memories, prior chats, or old source layout as active implementation
authority unless the attached pack says so. In particular, do **not** treat any "first
milestone / rename to .txt / propose the file layout / next phase is H.2A" framing from older
introductions as current — that is many phases out of date. Equally, do **not** treat
"the keystone is next / not yet proposed" framing (true in the v4.x introductions) as current
— the keystone is under way and committed through K.1D.

CNR3 is a VapourSynth **API4-only**, **integer-YUV-only** recursive temporal chroma stabiliser
(VHS/analogue chroma restoration). Its load-bearing difficulty is:
```text
output[N] depends on source[N] and already-filtered output[N - 1]
```
The predecessor is the already-filtered **output**, not merely `source[N - 1]`. Modern
VapourSynth scheduling may request frames out of display order, so CNR3 needs a correct
cache/recovery architecture before any parallel-performance work can be trusted. That
architecture is the CMS07.14 design (additive over CMS07.13), and it **completely supersedes** the previous CMS06.x
cache design and proof path. The eventual end-goal is `fmParallel` (a correctness phase).

---
## 0. WHERE THE BUILD ACTUALLY IS (read this before anything else)
**This is the single most important orientation point, because this introduction has
historically been pasted while badly out of date.** Confirm the live state from the
repository (section 2), but the expected state is:
```text
Cache core:          COMPLETE and proven through the C.14A aggregate milestone.
Scalar pixel chain:  COMPLETE and proven — P.1A (response tables) -> P.2A (config/geometry)
                     -> P.3A (int64 weighted blend) -> P.4A (downsampled-luma) -> P.5A
                     (signed-difference/lookup/blend bridge) -> P.6A (chroma-plane traversal).
Scalar->native:      COMPLETE and proven — P.7A (source-luma downsample traversal) -> P.8A
                     (native byte-plane access) -> P.9A (native luma downsample bridge).
Real-frame path:     COMPLETE and proven ON CALLER-SUPPLIED FRAMES — P.10A (VapourSynth
                     plane-view adapter) -> P.11A (caller-supplied frame-triplet validation)
                     -> P.11B (caller-supplied real-frame pixel composition) -> P.11C
                     (caller-supplied scene-change/reset).
KEYSTONE+RECOVERY+SCENE: LIVE DISPATCH FEATURE-COMPLETE (all four branches) WITH SCENE HANDLING; recovery arc COMPLETE + P.11C arc CLOSED, committed through P.11C.5:
                       K.1A request-plan structures + temporary KDT dev-trace   (count -> 46)
                       K.1B direct cached-output-return ownership (synthetic)    (count -> 47)
                       K.1C live getFrame passthrough scaffold                   (plugin-only)
                       K.1D live frame-0 fresh-start store/return (copyFrame)    (FIRST real output)
                       K.1E.2/E.3 live predecessor-present compute (N==1,2)      (branch-c; R-ARCH-06)
                       Recovery-Step-0  AS4 single-lock batch discharge          (count 48 -> 49)
                       K.1F  live direct cached-output return (cache hit)         (branch-b; count 49)
                       K.1G  plugin source split (no behaviour change)           (cnr3_arInitial.cpp /
                             cnr3_arAllFramesReady.cpp / cnr3_plugin_internal.h)
                       D.1   exact-anchor SINGLE-hole recovery                    (plugin-only; first live recovery)
                       D.2   exact-anchor MULTI-hole + bounded-window refusal      (plugin-only)
                       D.3   floor-fresh-start recovery                            (plugin-only; materialized-floor invariant)
                       D.4   adopt-skip + first-in-best-dressed primitives         (selftest; count 49 -> 51)
                       D.5   recovery-pin-survives-real-prune-pass                 (selftest; count 51 -> 52)
                       P.11C.1 scene-change uniform-wiring layout                  (plugin-only)
                       P.11C.2 scene-config + scdthr->threshold helper + store-req  (plugin-only)
                       P.11C.3 branch-c (predecessor-present) scene detection       (plugin-only; .vpy-proven)
                       P.11C.4 branch-d (recovery) per-hole+target scene detection  (plugin-only; .vpy-proven)
                       P.11C.5 scene-cut-checkpoint found as recovery anchor        (selftest; count 52 -> 53)
Latest committed:    CNR3-OPT-LeverE-scenechange-local-accumulator  (MARSHALLING-OPTIMISATION ARC COMPLETE, ~-80%; on top of CMS07-DIAG-tinycache-scaffold / CMS07-W.3; 56/56)
Controlling CMS:     CMS07.14 (additive over 07.13: §7.4 checkpoint-retention trigger, §7.5 live-wiring contract,
                     §7.6 arInitial observation prerequisite — Step 0 outputs; §0A charter + §9.5 materialized-floor +
                     R-LIFECYCLE carried forward unchanged)
Selftest count:      53/53 PASS (forced-fail 52/53 exit 1; verbose 53/53).
Next phase:          live cache-pressure WIRING IMPLEMENTATION (Step 0 CLOSED; contract in CMS §7.4-§7.6). Order:
                     W.1 §7.4 checkpoint-retention trigger primitive (cache-core + selftest; count 53->54; scope
                     CNR3_W1_Coder_Scope_checkpoint_retention_trigger_v1_0.md; coder read-first + 3a proposal already
                     ACCEPTED) -> W.2 hot-zone observation wiring (@arInitial) -> W.3 combined live store-and-prune helper
                     (§7.5 six-step order; temporary KDT). THEN real-footage -> diagnostics (condensed 4-phase) ->
                     fmParallel. Single-activation scope; concurrent prune is the fmParallel arc.
Branch:              dev_cache_manager
```
So: the cache core is done, the pixel maths is done, the scalar->native bridge is done, the
real-VS-frame pixel path is done on caller-supplied frames, AND the keystone live dispatch is
COMPLETE — all four getFrame branches (cache-hit, fresh-start, predecessor-present, recovery)
are live and proven both Debug+Release. The plugin produces correct recursive output through live
getFrame, and the entire branch-(d) recovery arc is proven: single-hole (D.1), multi-hole + bounded-
window refusal (D.2), floor-fresh-start (D.3), adopt-skip / first-in-best-dressed primitives (D.4), and
recovery-pin-survives-real-prune (D.5). The P.11C SCENE-CHANGE ARC is now also CLOSED (.1-.5): scene
detection runs uniformly across branch-a/c/d (predecessor-present .3, recovery holes+target .4), a detected
cut resets chroma + promotes the frame to a checkpoint, and such a scene-cut checkpoint survives prune and
serves as a later recovery anchor (.5) — all proven both configs. What is NOT yet done, and is your
immediate forward work, is the **live cache-pressure WIRING IMPLEMENTATION** — the prune logic and hot-zone
machinery are built and selftest-proven but are NOT yet called from the live getFrame path. The Step 0 review
is CLOSED and the wiring contract is settled in CMS §7.4-§7.6; your next concrete task is W.1 (the §7.4
checkpoint-retention trigger primitive). Wire hot-zone observation (CMS
§5.7: at arInitial) then pruning, after a designer+coder review of the live prune-TRIGGER contract. (The
only deferred confidence remains real concurrent fmParallel scheduling, bounded to the fmParallel phase;
the concurrent prune case is explicitly part of that arc.)

## 0.0 THE DESIGN ALIGNMENT AND ESCALATION CHARTER (read before any work — it governs HOW you work)
This is the standing three-way governance model (designer/reviewer, coder, coordinator). It governs
how all three treat the CMS, escalate problems, and cross-check each other. The full text is also in
the Production Spec section 3A.5.0 and the Role Handover Part 3 (D0); it is reproduced here so a fresh
coder internalises it before touching anything.

```text
(Three-way working charter: designer/reviewer, coder, coordinator. The coordinator holds final
authority on scope, sequencing, and commits.)

1. CMS is the controlling guide; strict alignment is the default. Two distinct issue types license
   departing from "follow the CMS as written," and both are surfaced rather than handled silently:
   - RULE-DEVIATION issue (case a): a NAMED, SPECIFIC CMS rule, if followed, would produce a
     demonstrably wrong, inconsistent, or unsafe result. This bar is HIGH: comparable to the
     evidence that produced the CMS07.10 correction (analysis/source-level proof, not a hunch), and
     never invoked for convenience, brevity, or preference.
   - CMS-GAP issue (case b): a bigger-picture concern (emergent risk, missing abstraction,
     fmParallel/reliability/safety implication) with little or no correspondence to any specific
     existing rule, which may call for a NEW or REVISED rule or approach. This is NOT gated behind
     the high deviation bar; identifying that the CMS is silent or under-specified on something that
     matters is encouraged, and lands as a surfaced critical issue or proposed rule even when no
     single existing rule is in conflict.
   - Issues are classified RULE-DEVIATION or CMS-GAP when raised; the classification may be
     corrected as evidence develops (a gap that turns out to conflict with a specific rule, or
     vice versa).

2. On either issue type: stop and raise -- never route around silently. Work ON THE AFFECTED CHANGE
   pauses (unrelated, clearly out-of-scope work may continue); the issue surfaces as an explicit
   decision, resolved by designer+coder agreement with coordinator approval before proceeding. For
   RULE-DEVIATION the resolution amends or excepts the named rule; for CMS-GAP it produces a
   new/updated rule, a recorded approach, or an owed-items entry. No party implements a deviation,
   or quietly works around a gap, unilaterally or with deferred mention. Local experiments to
   UNDERSTAND an issue are allowed, but must be labelled exploratory and must not be committed or
   treated as accepted design until the issue is resolved.

3. Cross-checking is bidirectional and substantive, into each other's domain. The designer
   read-firsts the coder's diffs against design intent and independently computes/verifies golden
   values; the coder checks the designer's scope against code and primitive reality. Each verifies
   the other's home turf rather than deferring to it. The coordinator arbitrates and holds final
   authority on scope, sequencing, and commits.

4. Weight scales to risk. Full review ceremony for changes to proven code, lifecycle/concurrency,
   anything bearing on the long-term fmParallel goal, or anything where a gap would be silent and
   costly; lighter touch for mechanical steps. For the fmParallel goal specifically, concurrency
   reasoning is recorded at design time, not deferred to "it passed single-threaded."

5. Agreed deviations, new/updated rules, and discovered gaps are recorded durably -- CMS correction
   block, new/revised CMS rule, owed-items ledger, or DELTA/handover note as appropriate -- so the
   reasoning persists across chats and is neither lost nor re-litigated. For behaviour, lifecycle,
   ownership, concurrency, or proven-code changes, the agreed resolution is recorded BEFORE OR AS
   PART OF the commit that implements it -- not deferred.
```

The D.1 design road (see Document B / the DELTA) is the worked example of this charter in action: the
designer's first routing sketch contained a time-of-check/time-of-use hazard, a false "minimal change"
premise, a wrong recovery-search assumption, and a source-set/hole-count mismatch — all caught by the
coder's independent review across several rounds BEFORE any code. That is the charter working as
intended. You are expected to push back the same way.

---
### The keystone commits, in one line each (what they did)
```text
K.1A  Added the keystone request-plan structures (branch enum/struct; recovery request is a
      holes-list / source-set, NEVER a blanket span; the hard-status branch is a CARRIER for
      existing C.13B guard results, not a new validator) and a temporary KDT dev-trace
      (CNR3_KEYSTONE_DEV_TRACE; [KDT]/[KDT-SUMMARY] driven by the plan structure). No getFrame
      wiring, no source lifecycle, no pixel call, no cache-semantic change, no VS header edit.
      Behaviour-adding -> +1 selftest (45 -> 46).
K.1B  Proved direct cached-output-return ownership, synthetic-first, using the REAL
      Cnr3OwnedFrameRef and REAL cache lookup/addref (counters OBSERVE real ops): success
      1/0/1 (acquired/released/transferred), cleanup-before-transfer 1/1/0, no-acquire miss
      0/0/0. The synthetic sink models the getFrame-return boundary. The real VSFrame
      return-to-VapourSynth was explicitly OWED here and is expected to retire INSIDE the
      branch-(c) work. Behaviour-adding -> +1 selftest (46 -> 47).
K.1C  First live getFrame step: a passthrough scaffold with FIVE R-ARCH-06 fences (removable
      guard; a DISTINCT callback that gets replaced not extended; the scaffold frame is NEVER
      cached / NEVER a predecessor / NEVER checkpointed; a [KDT] SCAFFOLD_NOT_FILTERED marker;
      a return-point comment). [KDT] is emitted ONLY inside getFrame, never at load/registration.
      PLUGIN-ONLY (changes only src/vapoursynth-Cnr3.cpp + src/cnr3_build_config.h); proven by
      the coordinator A/B byte-compare harness, NOT by a selftest -> count stays 47.
K.1D  The FIRST REAL output frame: output[0] created, stored as a cache-authoritative
      checkpoint, and returned through live getFrame; N>0 cleanly refused. Reached via
      copyFrame(source, core) (a bitwise, writable, caller-owned duplicate) because frame-0
      fresh-start output[0] = source[0] byte-for-byte (no predecessor, no blend; luma always
      source-copy, chroma source-copy when no predecessor) -> so NO proven code is touched
      (zero contact with cnr3_frame_processing.cpp / P.11C). PLUGIN-ONLY; A/B harness green
      (frame-0 byte-identical to source; N>0 clean refusal leaves a header-only y4m, no FRAME
      marker). Count stays 47. Guard: CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF.
```

```text
K.1E.2/E.3  Live predecessor-present compute for N==1 then N==2 (branch-c): acquire cached
      output[N-1] as predecessor (real lookup/addref, carried in frameData), request source[N],
      compute output[N] via the PROVEN P.11B path, release predecessor, store, return. Ownership
      tail acquired=1/released=1/transferred=0 (consumed-and-released, the opposite of a cache-hit
      return). Known-answer goldens 161/95 (N=1) and 163/93 (N=2). Closed R-ARCH-06 live (the
      predecessor is the cached filtered OUTPUT, never source[N-1]).
Recovery-Step-0  AS4 single-lock batch discharge: discharge_all delegates to the cache core taking
      cache_mutex_ ONCE for the whole pin-list. Cache-core only; count 48 -> 49.
K.1F  Live direct cached-output return (branch-b): present-N pins output[N] at arInitial (gap
      protection), requests source[N] as an Option-C lifecycle TRIGGER (retrieved+immediately freed,
      NOT consumed), returns the cached frame at arAllFramesReady via add_ref + Step-0 discharge. This
      proved the R-LIFECYCLE correction (CMS07.10 9A.1.1): EVERY getFrame branch requests >=1 real
      source at arInitial and returns only at arAllFramesReady. Plugin-only; count unchanged 49.
K.1G  Plugin source split, no behaviour change: vapoursynth-Cnr3.cpp split into cnr3_arInitial.cpp
      (branch START), cnr3_arAllFramesReady.cpp (branch-tag EXECUTION), cnr3_plugin_internal.h
      (private shared decls). New .cpp in the cnr3 DLL project only; selftest compiles neither. The
      live getFrame code now lives in these files — that is where D.2 lands.
D.1   Live branch-(d) exact-anchor SINGLE-hole recovery (the first live recovery): A-safe-1 routing
      (cache-hit pin -> N==0 fresh-start -> predecessor pin -> recovery), each decision a find-and-pin
      (NO naked peek). Reconstructs absent output[N-1] from a pinned anchor at N-2, then computes
      output[N]. Accept gate restricts to hole_count==1 & anchor==N-2 (or zero-hole degenerate
      anchor==N-1); source set DERIVED as {N} U holes (dissolved window); pre-compute adopt-and-skip;
      authoritative target return on duplicate; AS4 batch discharge. Branch-c refactored to ACCEPT an
      already-recorded predecessor pin from routing. P.11C scene-change DEFERRED uniformly
      (scene_change_deferred=1 — a shared owed ledger item, to be wired across all branches before any
      real-footage test). Goldens 145/111 (hole) and 147/109 (target). Plugin-only; count 49.
```

### THE K.1D REORIENTATION — the chief disciplinary lesson of the keystone (read this; it is about YOU)
**The first K.1D patch was DROPPED, even though it built and passed the four-way 47/47.** On
the read-first diff review it was found to (1) silently REWRITE the body of the proven,
selftested P.11C reset function to route through a new helper; (2) introduce a SECOND
source-to-output copy orchestration, hand-setting the reset-summary flags to MIMIC the reset
path without BEING it; and (3) broaden scope undisclosed into the proven
`cnr3_frame_processing.cpp` (+351 lines) against a "report before broadening" commitment.
**A passing four-way after swapping proven internals is NOT proof of equivalence** — it proves
only that the existing selftests did not DETECT a difference, not that there is none (the
P.11C selftest may simply not exercise the changed path). The patch was **WITHDRAWN to the
proposal stage** — not patched-and-fixed — and the reorientation produced a smaller, safer
design (`copyFrame`, touching no proven code). This is now a standing rule (R-PROCESS-21,
below). It exists because the coder's observed failure mode at the keystone is reasoning
forward from getFrame and touching proven code to avoid a conversation. **If standing up a
phase appears to require touching proven pixel or cache-core internals, STOP and raise it as a
design question before writing it — do not fold it into the patch.**

**Document B (current version, see section 1) sections 8 / 11 and its keystone status note are
authoritative for state and the forward roadmap, including the full K.1A–K.1D detail and the
K.1E branch-(c) plan (now historical — committed as K.1E.2/E.3). This introduction carries the live
P.11C state (section 0.1); where it overlaps Document B, Document B (top UPDATE block) and the repository
win on build state.**

---
## 0.1 THE LIVE TASK — P.11C scene-change uniform wiring (across branch-a/c/d)
The live getFrame dispatch is FEATURE-COMPLETE (all four branches) and the branch-(d) recovery arc is
COMPLETE (D.1-D.5, all committed both configs). The recovery roadmap is DONE:
```text
D.1  exact-anchor SINGLE-hole recovery                                   DONE (committed)
D.2  exact-anchor MULTI-hole (k>=2) + bounded-window refusal             DONE (committed)
     (RUN C bounded-window refusal is TRANSITIONAL — superseded by D.3 for reachable in-range N)
D.3  floor-fresh-start recovery (no in-window anchor: copyFrame base + walk forward)  DONE (committed)
D.4  pre-compute adopt-skip + first-in-best-dressed PRIMITIVES (selftest)             DONE (committed)
D.5  recovery-pin-survives-real-prune-pass (selftest, paired control)                 DONE (committed)
P.11C  scene-change uniform wiring across branch-a/c/d                    <- NEXT (NOT a recovery phase)
```

### What P.11C must do (the shape; the designer will deliver a full scope first)
```text
Marker name (expected):  CMS07-P.11C-... (designer to finalise)
Purpose: scene-change handling is currently DEFERRED UNIFORMLY across all live branches
  (scene_change_deferred=1) because the entire D-series was proven on synthetic constant-plane test
  footage that has NO scene cuts. Real footage has cuts. P.11C wires scene-change detection/reset in
  UNIFORMLY across branch-a (fresh-start), branch-c (predecessor-present), and branch-d (recovery),
  BEFORE the first real-footage test.
Why it must happen before real footage:
  - a deferred P.11C would BLEND chroma across a hard cut (visibly wrong output);
  - a detected cut must PROMOTE to a checkpoint (CMS 6.4 / 9.5) = an exact, longer-retained recovery
    anchor found naturally by the descending search. So P.11C INTERLOCKS with the recovery machinery
    just completed: cuts become recovery anchors.
Scope reach: P.11C touches the PIXEL PIPELINE (cut detection + reset of the recursive blend at the cut)
  AND the CHECKPOINT/RECOVERY interaction (cut -> checkpoint promotion). It is its own phase, wired
  uniformly across the branches at once — NOT bolted onto any single branch.
After P.11C: first REAL-footage validation, then the fmParallel arc (the deferred concurrency work).
```

### Carry these settled findings (do NOT re-derive)
```text
1. R-LIFECYCLE (CMS07.10 9A.1.1): EVERY getFrame branch requests >=1 real source at arInitial and
   returns only at arAllFramesReady. The act-time branch is keyed on the frameData branch tag set
   at arInitial, never on re-inspecting frame state.
2. A-safe-1 routing hard floor: no branch exits arInitial relying on an UNPINNED observation; each
   selected branch owns its pinned foundation via its own atomic find-and-pin; a miss is only a
   routing opportunity. (No naked presence peeks.)
3. Dissolved source window: request {N} U genuine-hole-sources, DERIVED from the hole catalogue,
   never a blanket backward span and never hardcoded.
4. P.11C scene-change is DEFERRED uniformly across all live branches (scene_change_deferred=1) — and
   wiring it in uniformly across branch-a/c/d IS the current P.11C task. It must be wired across all
   three branches TOGETHER (never one branch alone) before any real-footage test (see the ledger).
5. fmParallel goal: every phase's design note must carry an explicit interleaving analysis (the D.1 scope
   has the six-case template), shown benign by citing existing mechanisms (AS1 anchor-pin,
   first-in-best-dressed/AS2 store-or-adopt, pre-compute adopt-and-skip).
6. Golden-chain note (durable): at threshold=255 the recursive output moves ~1 LSB frame-to-frame, so
   ROBUST pixel proofs combine the KDT mechanism + direct byte checks via cache-hit follow-up, never the
   target byte margin alone. The designer computes and supplies any verified golden chain. (P.11C's proof
   centres on cut-detection/reset behaviour + cut->checkpoint promotion, with real-footage validation
   following; the synthetic D-series goldens are in the dev-branch git history.)
```

### Where the live getFrame code lives (post-K.1G source layout; for P.11C orientation)
```text
src/cnr3_arInitial.cpp          all branch STARTs (cache-hit / fresh-start / predecessor-present /
                                recovery). P.11C cut detection feeds the branch decision + the
                                cut->checkpoint promotion; wire it uniformly, not per-branch.
src/cnr3_arAllFramesReady.cpp   all branch COMPLETIONs incl. the recovery ascending fill loop. The
                                recursive blend (P.11B) that P.11C must RESET at a cut runs here.
src/cnr3_plugin_internal.h      frameData carries the per-branch tags + recovery vectors; the
                                scene_change_deferred=1 flag that P.11C flips lives in this area.
src/cnr3_cache_core_selftest.cpp  D.4/D.5 primitive + composition proofs (selftest-only) live here.
```

## 1. Attachments expected for this resume (and how they lead into Doc A and Doc B)
Do not proceed from this introduction alone. This intro is the LEAD-IN; the authority is the pack.
Read in this order, and take the HIGHEST version present if names differ:

**This intro orients you. Then Document A gives you the standing project context and rules; then
Document B gives you the live build state and work plan. The CMS is the design authority above both.**
```text
1. This introduction (orientation only):
   CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_1.md
2. Controlling design authority:
   cnr3_cache_manager_design_v7_13.md
   (CMS07.14 — additive over 07.13; supersedes all earlier CMS07.x and CMS06.x. Carries: §7.4-§7.6 live-wiring contract, 9.7 keystone
    predecessor-sourcing; 9.7.7 rpGeneral source-input dependency (FI-04 resolved); 9A.1.1 the
    arInitial/arAllFramesReady frame-return contract WITH the R-LIFECYCLE correction proven by
    K.1F; 9.2/9.6.5 pre-compute adopt-and-skip normative since CMS07.9; 9.1/9.5/9.6 the bounded
    recovery model that the full branch-(d) arc D.1-D.5 wired and completed; 0A the Design Alignment
    and Escalation Charter; 9.5 the materialized-floor-is-the-foundation invariant. CMS07.11/.12/.13
    are charter+clarifications — no design rule, AS scope, or section-number change. 6.4/9.5 cut->
    checkpoint promotion is the recovery interlock P.11C wires to.)
3. Project context / standing rules  ->  DOCUMENT A:
   Document_A_CNR3_Project_Context_and_Standing_Rules_v3_5.md  (or highest present)
   (the standing project context + the full section 3A register reproduced from the Production
    Spec, incl. R-PROCESS-20..23 and the Design Alignment and Escalation Charter at 3A.5.0.
    NOTE: if only an older Document A (v3.4 or earlier) is present, it is on the K.1D/CMS07.8
    baseline and its STATE pointers are stale — trust THIS intro's section 0 and Document B for
    current state, and the Production Spec section 3A for the authoritative rule text.)
4. Current work plan + BUILD STATE  ->  DOCUMENT B:
   Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_5_1.md  (or highest present)
   (the live build state through D.5 (recovery arc COMPLETE), the working method, salvage inventory,
    next phases, and the do-not-implement list. READ THIS for where the build actually is. Its v3.5.1
    UPDATE block at the TOP is authoritative; older blocks below are history. If a higher version exists, use it.)
5. Production Spec (canonical context master + section 3A rules):
   CNR3_Handover_Pack_Production_Spec_v2_11.md  (or highest present)
   (populated section 3A register: R-PROCESS-20 (PDAP), R-PROCESS-21 (proven-code-stays-proven),
    R-PROCESS-22 (lifecycle/API from documentation, not test behaviour), R-PROCESS-23, and the
    Design Alignment and Escalation Charter at 3A.5.0. Section 3A is the AUTHORITATIVE rule text.)
6. Diagnostics spec:
   cnr3_diagnostics_specification_v1_5.md
   (subordinate to the CMS and section 3A; section 2.8 = the temporary keystone KDT dev-trace,
    removed at the post-keystone cleanup.)
7. Manifest:
   CNR3_Handover_Pack_*_MANIFEST.md
   (reading order and pack contents; may lag the version numbers above — the individual documents'
    own version headers win.)
```
Companion documents (read if present; not the controlling design):
```text
- CNR3_Designer_Reviewer_Role_Handover_v1_9.md  (or highest present)
  (the DESIGNER/REVIEWER role doc — review disciplines D0-D16 (D0 is the Design Alignment and
   Escalation Charter), decision heuristics, the pixel-layer reference confirmed against vsCnr2.cpp,
   the accuracy rule, recorded deliberate divergences, and the review checklist. It is the clearest
   map of what the coordinator will scrutinise — and what you should self-check before proposing.)
- CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.md  (or highest present)
  (the SLIMMED current-state delta companion to Document B — a committed-phase INDEX through D.5
   (recovery arc COMPLETE), the R-LIFECYCLE finding kept in full, the P.11C active-phase detail, and
   the owed-items ledger. Per-phase golden chains / proof narratives live in the dev-branch git
   history. If included, read it for current state + depth pointers.)
```
NOT part of the durable implementation authority (do not treat as controlling):
```text
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13.md
  (NON-NORMATIVE companion to CMS07.14, lockstep filename; deferred tuning / fmParallel-phase
   questions only (FI-01..08); ignore for implementation. FI-04 is RESOLVED into CMS 9.7.7. The open
   FI items are the subject of the upcoming fmParallel phase.)
- the old .txt reference source under src/superseded_by_v7/ (salvage is per-case, approval-only).
- older Document A / Document B / Role Handover / introduction versions, and the old CMS06-era
  decision log — out of scope as active inputs (intentionally, to keep stale assumptions out).
```
If **the controlling CMS (CMS07.14, file cnr3_cache_manager_design_v7_14*.md) is not attached**, stop and say so. You may comment on this introduction, but
you cannot enumerate rules or proceed without the controlling design.

---
## 2. FIRST action — confirm the build state from the repository, then audit the K.1C scaffold
Before anything else (before enumerating rules, before proposing the next subphase), confirm the
build state from the authoritative source — the repository — not from these documents' say-so.
This re-establishes the project's "prove it, do not assert it" discipline from your very first
action:
```text
1. Read the recent git log (~25 commits). Confirm the latest commit is
       CMS07-K.1D: prove live frame-0 fresh-start store/return   (plugin-only)
   and that K.1A, K.1B, K.1C, K.1D are present on top of the pixel-arc commits P.1A through
   P.11C and the C.14A aggregate cache-core milestone.
2. Read src/cnr3_build_config.h; confirm CNR3_EDIT_VERSION reads:
       CMS07-D.5-recovery-pin-survives-bounded-prune-proof
3. Build + run the isolated cache-core selftest (it also carries the pixel-proof tests and the
   K.1A/K.1B keystone selftests) and confirm the four-way:
       Debug   normal                              -> 52/52 PASS, exit 0
       Release normal                              -> 52/52 PASS, exit 0
       Release --force-fail-for-harness-proof      -> 51/52 PASS, 1 FAIL, exit 1
       Release --verbose                           -> 52/52 PASS, exit 0
   In the --verbose trace, confirm the pixel scenarios P.1A through P.11C AND the keystone
   scenarios K.1A (request-plan) and K.1B (cached-output-return ownership) are all present and
   passing — not just the total count. (K.1C and K.1D are plugin-only and add no selftest; they
   are proven by the coordinator A/B harness, so they do NOT appear in the selftest trace.)
4. Audit the committed tree for the K.1C passthrough scaffold and report (see section 0.1):
   grep src/vapoursynth-Cnr3.cpp and src/cnr3_build_config.h for
       CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD, SCAFFOLD_NOT_FILTERED, LIVE-SCAFFOLD-PASSTHROUGH,
       and any source[N] passthrough-return branch.
   They should be ABSENT after K.1D; confirm, and report anything still present.
5. Confirm src/cnr3_frame_processing.cpp is a member of BOTH the cnr3_cache_core_selftest project
   AND the cnr3 plugin project (settled at P.10A). The keystone builds on the plugin side, so if
   for any reason this membership is missing it must be re-added (Visual Studio 2026 GUI: Add
   Existing Item) or the plugin build will fail to link.
If any of these do not match, STOP and report the discrepancy before doing anything else.
```
Repository: `https://github.com/hydra3333/vapoursynth-cnr3` (local working tree
`E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github`, branch `dev_cache_manager`). Builds in
Visual Studio 2026, x64. The selftest builds/runs in `vs\cnr3` (`x64\Debug\` and
`x64\Release\cnr3_cache_core_selftest.exe`); the plugin is `cnr3.dll`.

---
## 3. WHEN IN DOUBT — raise it for review; do not decide alone
This project runs through a **designer/coordinator review loop**, and that is exactly what has
kept the build sound across every phase. There are two different "ask" situations, and BOTH mean
stop and raise it — not proceed on your own judgement:
```text
A. CMS gaps (R-AUTH-03): if the CMS is silent, ambiguous, or incomplete on an implementation
   point — STOP and ask. Do not guess, do not improvise.
B. Design-coordination questions (broader than the CMS): if you have ANY doubt about
       - the direction of the work,
       - which subphase comes next or a subphase's scope,
       - whether a test case is adequate / genuinely discriminating,
       - a subphase's exit bar or per-phase goal,
       - whether something is in scope for the current subphase,
       - whether reuse of a proven operation would TOUCH it (always a design question — see
         section 0.1 / R-PROCESS-21),
       - whether something diverges from vsCnr2 and, if so, whether that divergence is intended,
   then RAISE IT to the user/coordinator for review before acting. "The CMS does not forbid it"
   is NOT license to decide a direction, scope, or test-design question unilaterally.
```
Concretely: the build cadence is **propose → review → approve → code**, never idea straight to
committed code. The coordinator runs a separate designer review of each proposal against the
spec and the vsCnr2.cpp source. If you are unsure whether to raise something, raise it. A
question is cheap; a wrong unilateral call on scope, test design, a silent divergence from the
reference, or a touch of proven code is not.

This has been borne out repeatedly — most sharply at the keystone, where a patch that passed the
four-way but silently rewrote proven code was caught on review and withdrawn (the K.1D
reorientation, section 0). The strongest phases were the ones where the coder surfaced a doubt
(a reachability question, an integer-division quirk, an edge-handling divergence, a
self-predecessor shortcut it refused) for review rather than deciding it alone. That instinct is
the project's main safety mechanism — keep it.

---
## 4. How this phase of work differs from the cache-core and pixel eras (orientation)
The cache-core phases (F/G/H/C series, through C.14A) proved concurrency-correctness: atomic
lock scopes, pin/checkpoint/prune discipline, recovery contiguity. The pixel phases (P.1A–P.11C)
proved arithmetic- and real-frame-correctness: exact-integer reference vectors cross-checked
against vsCnr2.cpp (no tolerance), then the real-VS-frame pixel path on CALLER-SUPPLIED frames.
**You are now in the third kind of correctness — the keystone — connecting the two proven halves:
frame SOURCING / lifecycle / ownership through getFrame.** It is under way (K.1A–K.1D) and has
already surfaced its chief lesson (the K.1D reorientation; proven code stays proven).

The risks of this era:
```text
- Memory layout: real byte strides, plane pointers, 8-bit uint8_t* vs 16-bit uint16_t*, and
  alignment — already crossed at P.10A and proven through P.11C/K.1D. The typed-row-pointer vs
  memcpy decision is deferred to a measured fmParallel performance phase; it is NOT part of the
  keystone.
- Lifecycle/ownership: the VapourSynth two-phase request model (arInitial / arAllFramesReady),
  frame reference counting, and the cache's pin/recovery discipline meeting real getFrame. These
  are now LIVE, not hypothetical.
- Integration: the predecessor MUST be the explicit previous filtered OUTPUT supplied by the
  cache/recovery layer — NEVER source[N-1]. Substituting source[N-1] is the exact approximation
  CMS07 was built to replace; it is prohibited (R-ARCH-06).
- Proven-code protection: the keystone "reuses" the proven pixel path and cache core. Reuse is a
  THIN public call only; touching proven internals is a design question to raise, never a patch
  inclusion (R-PROCESS-21; the K.1D reorientation).
```
The proof surface for keystone phases is lifecycle/ownership sequences and a coordinator-side A/B
acceptance harness, not scalar reference vectors. A live-getFrame keystone subphase may be
PLUGIN-ONLY: it adds no cache-core selftest, the coordinator additionally builds the cnr3.dll
plugin, and the behavioural proof is the A/B harness — so the selftest count legitimately stays
put across such a subphase (this is a recognised third category beside "behaviour-adding +1
selftest" and "audit/comment unchanged" — see the R-PROCESS-20 clarification in Production Spec
v2.7). Part of each proposal should still be **how the phase will be proven** — what genuinely
discriminates a correct implementation from a plausible-but-wrong one (e.g. for K.1E, a KDT that
proves the predecessor was specifically cached output[0], plus a known-answer byte-check vector).

---
## 5. Hard precedence and old/new separation
```text
If CMS07.14 conflicts with, or is merely unclear in alignment with, prior material:
    CMS07.14 wins unless the user explicitly says otherwise.
If CMS07.14 itself is silent, ambiguous, or incomplete on an implementation point:
    stop and ask (see section 3). Do not guess and do not improvise.
```
References to "CMS07.0" (or any earlier CMS) as controlling, inside reproduced rule text, mean
the latest prevailing CMS — currently **CMS07.14**. Specific CMS section pointers are
version-specific and must be re-checked against CMS07.14. All pre-CMS07 cache code/design is
superseded: old code is salvage reference only, per Document B section 8.5 and the section 3A
salvage rules.

A LIFECYCLE/API epistemics rule now applies (R-PROCESS-22): a VapourSynth lifecycle or API
contract — the arInitial/arAllFramesReady activation-reason and frame-return contract, what may
be requested/retrieved in which activation, a dependency/request-pattern declaration
(rpStrictSpatial vs rpGeneral), a threading/ownership guarantee — must be established from the
AUTHORITATIVE DOCUMENTATION (the R76 VapourSynth4.h header and the CMS), NOT inferred from "it
compiled / a test passed / a path worked." Undocumented-but-works is version-fragile and
especially dangerous under fmParallel. If a contract is unclear, resolve it against the docs; if
the docs are silent, stop and ask.

---
## 6. The highest-risk traps (do not conflate old and new concepts)
These remain inviolable even though the cache core and pixel path are proven — the keystone
re-touches all of them, and adds the proven-code trap.

### 6.1 Cache-core traps (binding at the keystone)
```text
1. Pinning is the mandatory correctness baseline — never optional/deferred. There is exactly ONE
   pin concept: consumer-claim, recorded on a per-invocation pin-list.
2. A checkpoint is a separate eviction-protection FLAG, not a pin.
3. Hot zones are prune-policy HINTS only — pins provide active liveness, hot zones do not.
4. Recovery uses the CMS two-phase model: request source N plus genuine holes only — never a
   blanket bounded-warmup source window.
5. The predecessor is the previous filtered OUTPUT from the cache/recovery layer, NEVER
   source[N-1] (R-ARCH-06). This is the keystone correctness property of the whole design.
```

### 6.2 Pixel/arithmetic traps (load-bearing as the keystone feeds real pixels into P.11B)
```text
6. Signed differences (current - previous) MUST stay signed int end-to-end into the table lookup
   — NO unsigned intermediate anywhere on the sample->diff->index path. (Proven correct through
   P.5A; preserve it as real pixels feed the path through the keystone.)
7. The blend accumulator is int64; shift2 = depth<<1, shift = 1LL<<shift2, shift1 = shift>>1,
   reproduced bit-exactly. Do NOT "improve" definitional integer arithmetic.
8. Recorded deliberate divergences from vsCnr2 (do NOT "fix" back, do NOT flag as bugs — see the
   Role Handover accuracy rule for the full reasoning):
     - parameter scaling uses round-to-nearest (value*peak+127)/255, more accurate than vsCnr2's
       integer-factor truncation at 10/12/14-bit (identical at 8/16-bit);
     - P.4A downsample-luma CLAMPS edge taps rather than reading past the frame as vsCnr2 does.
   The GOVERNING RULE: accuracy upgrades only where vsCnr2 is accidentally lossy; definitional
   integer arithmetic is reproduced bit-exactly. When unsure which a step is, treat it as
   definitional and ask.
```

### 6.3 The proven-code trap (the keystone's defining risk — R-PROCESS-21)
```text
9. Proven code stays proven. Once a function is proven by a committed selftest (the P.11B/P.11C
   pixel path, the cache-core internals), its behaviour AND internals are frozen unless a phase
   explicitly proposes the change and the designer approves it IN ADVANCE. A passing four-way
   after an internals swap is NOT proof of equivalence. If reuse appears to require touching
   proven code, that is a DESIGN QUESTION to raise — not a licence to modify, re-implement in
   parallel, hand-set flags to MIMIC the proven path, or broaden scope into a proven file
   undisclosed. When a change endangers proven code, WITHDRAW-and-reconsider, do not patch-and-fix.
   (This is the K.1D reorientation lesson; it is the bar to watch hardest on K.1E's P.11B reuse.)
```
Nothing may be implemented that obstructs the fmParallel end-goal unless it is an unavoidable,
explicitly recorded, temporary stepping-stone preserving the path to fmParallel.

---
## 7. Engineered guards you must respect
### 7.1 Atomic-scope register (AS1-AS7)
CMS07.10 defines an atomic-scope register, AS1-AS7. It is designer-owned and inviolable. Every
cache critical section is enumerated there, including what happens inside one lock acquisition and
in what order. Implement these scopes exactly — do not shrink, split, merge, reorder, or
reinterpret them. If implementation reveals a needed operation the register does not cover, raise
it; do not invent an ad-hoc lock scope. (Directly relevant as the keystone wires the cache to real
requests; the cache is used through its public store/lookup API at K.1E.)

### 7.2 V5 firewall
VapourSynth frame reference counts are internally atomic — and that **gives you NOTHING over lock
scopes.** It protects a single `addFrameRef`/`freeFrame` only. It is not a licence to take a pin
outside the cache lock or to shrink any critical section. The protected thing is the multi-step
cache decision (find-then-pin, decide-then-detach), not the refcount bump.

### 7.3 VapourSynth lifecycle + frame-return contract (VS-LIFECYCLE-01; CMS 9A.1 / 9A.1.1)
Any source frame retrieved in `arAllFramesReady` must have been requested in `arInitial` of the
same activation (VS-LIFECYCLE-01). And the frame-return contract (CMS section 9A.1.1, established
from the R76 header): `arInitial` requests inputs and returns NULL; a frame is returned only at
`arAllFramesReady`. The source-filter exception (return a frame from arInitial) does NOT apply to
CNR3 — it is a dependency filter with an input node. **Both are binding now, as the keystone wires
getFrame — they are no longer hypothetical.** Settle these from documentation, not from a passing
run (R-PROCESS-22).

### 7.4 Dependency declaration: rpGeneral (FI-04 resolved, CMS 9.7.7)
The source-input dependency declaration is **rpGeneral**, not rpStrictSpatial, for a recursive
filter. rpStrictSpatial stops being truthful once recovery requests bounded source ranges;
rpGeneral is conservative-correct. **fmUnordered is unchanged** — `requestPattern` is a SEPARATE
layer from `filterMode` and does not affect the CMS7 cache. K.1E makes this declaration change.

### 7.5 Lock / ownership disciplines held at every phase
```text
- ONE cache-wide non-recursive mutex; RAII guard only.
- Decide INSIDE the lock; do the slow part (especially freeFrame) OUTSIDE it.
- freeFrame is NEVER called inside the cache lock (detach under lock, free after).
- pin-and-record is indivisible; pin-list capacity reserved BEFORE the lock.
- checkpoint is a flag, not a pin; hot zones are hints, not liveness.
```
Document B section 7 carries the full list. These are inviolable.

### 7.6 Category-B developer-alert (emission half of the C.13B guard)
At getFrame integration / error-mapping time, a hard cache status must map to a clean filter
failure (setFilterError) plus a bounded one-shot stderr developer-alert OUTSIDE locks. This is the
EMISSION half of what the C.13B recovery-contiguity guard already DETECTS (CMS section 9.6.4).
Expected Category-A duplicate/adopt outcomes stay silent. The cache core itself emits no stderr.
(For K.1E specifically, the error surface is the N>1 clean refusal; the full developer-alert wiring
arrives with the recovery branch.)

---
## 8. Section 3A is populated — rule enumeration is verification, not first population
The Production Spec section 3A Prevailing Rules Register is populated. Enumerate the prevailing
rules back to the user, but the purpose is **verification/reconciliation**, not first population.
Distinguish:
```text
REGISTER-OWNED rules:
    authority, pack governance, process, architecture/salvage, retired-fact entries — recorded in
    Production Spec section 3A (and reproduced in full in Document A v3.3). The process rules now
    run R-PROCESS-01 through R-PROCESS-22, including R-PROCESS-20 (PDAP), R-PROCESS-21 (proven-code
    stays proven), and R-PROCESS-22 (lifecycle/API contracts from documentation).
CMS-DEFINED / HANDED-OFF rules:
    design / cache-core / reference-count / VapourSynth-lifecycle / recovery / constant /
    instrumentation / atomic-scope rules defined in CMS07.10. NOT duplicated, indexed, or re-IDed
    in section 3A.
```
If you find an apparent missing rule, conflict, ambiguity, or candidate, raise it for user
decision (section 3). Do not silently treat it as controlling.

---
## 9. Salvage policy (per-case, approval-only)
The cache core is proven complete and the pixel-maths reference has largely been salvaged/
reimplemented through P.1A–P.11C, so check what is already active before re-salvaging anything.
Every salvage remains per-case, inspected, and explicitly approved (R-ARCH-07). The inventory is
in **Document B section 8.5**. Key points:
```text
- The pixel-maths reference (vscnr2-style response tables; the frame_internal_processing core with
  its explicit-previous-output boundary) is largely DONE via P.1A-P.11C. That proven code is now
  protected by R-PROCESS-21 — reuse it through thin public calls, do not re-touch its internals.
- TREAT WITH CAUTION: cnr3_common.h (stale CMS06 assumptions may ride along).
- QUARANTINE (do not open for ideas): the old cache managers — they embody the retired concepts and
  are the main route by which they creep back.
- CNR2 / vscnr2 is PIXEL-MATHS reference only. NEVER salvage CNR2 recovery/predecessor logic — that
  approximation (substitute source[n-1] when previous output is absent) is exactly what CMS07
  replaces (R-ARCH-06).
- vsCnr2.cpp is the bit-exact reference for the pixel arithmetic. The canonical source the designer
  verifies against is:
  https://raw.githubusercontent.com/Asd-g/AviSynth-vsCnr2/refs/heads/main/src/vsCnr2.cpp
```
Old `.txt` code is not copied into new files without approval.

---
## 10. Process rules that matter immediately (orientation only — section 3A holds the wording)
```text
- Comments: concise, useful, never safety-incomplete.
- Code delivery uses the PDAP (R-PROCESS-20): a downloadable .patch per phase (git diff -U10 from
  the committed baseline), with a Stage-1 validation block (git apply --check, --whitespace=error,
  git diff --check, isolated build, changed-files list, apply sequence, build/test commands). The
  coordinator does the read-first review, applies, builds Debug+Release of BOTH cnr3 and
  cnr3_cache_core_selftest (and, for a plugin-only keystone phase, the cnr3.dll plugin), runs the
  four-way, and commits the src/ files (NOT the .patch). Provide a Visual Studio-style commit
  title/body with each PASS. Inline before/after edit blocks are NOT used for code delivery.
- The four-way: Debug normal (N/N exit 0), Release normal (N/N exit 0), Release
  --force-fail-for-harness-proof ((N-1)/N exit 1), Release --verbose (N/N exit 0). A D-SUM
  compute-gate change adds the R-PROCESS-19 macro-off observe-only proof (a fifth run); keystone
  phases so far have added no D-SUM gate.
- A live-getFrame keystone phase may be PLUGIN-ONLY: it adds NO selftest, the coordinator also
  builds cnr3.dll, and the behavioural proof is the coordinator-side A/B harness — so the count
  legitimately stays unchanged (the R-PROCESS-20 v2.7 clarification). Record it as such.
- PROVEN CODE STAYS PROVEN (R-PROCESS-21): never modify proven behaviour or internals without an
  explicit, approved-in-advance proposal; a passing run after an internals swap is not proof of
  equivalence; reuse that would touch proven code is a design question to raise; withdraw rather
  than patch around. (The K.1D reorientation, section 0.)
- LIFECYCLE/API FROM DOCUMENTATION (R-PROCESS-22): settle VS lifecycle/API contracts from the R76
  header and the CMS, not from "it worked in testing".
- Diagnostics are hard gates; a partial fail is a FAIL. Output to stderr, never stdout. Diagnostics
  are compile-time gated (compute gate + print gate, print subordinate to compute). Observation
  gates observe only; behaviour-changing scaffolds use the BEHAVIOURAL-SCAFFOLD comment tag +
  SCAFFOLD_* macro with an unwind note, NOT DIAG_* names.
- No printing or long-running work inside locked/atomic scopes.
- Minimise unrelated diffs; do not silently paraphrase agreed rules; ASCII-safe code-update text.
- Reference vectors are proven by EXACT integer equality (no tolerance) and, where countable,
  static_assert. Any numeric claim the coordinator can recompute, they will.
- Any override requires explicit discussion, agreement, and documentation.
```
Consult section 3A directly for authoritative text. Document B section 6 and the Role Handover
describe the full working method (read-first patches, the four-way, genuine-failure-mode tests,
count discipline, the --verbose trace, the diagnostics module boundary, and the review checklist).

---
## 11. Your first response in this resume chat
Please respond with:
```text
a) Confirmation that you understand this is a RESUME well past the cache-core milestone, through
   the entire real-frame pixel path on caller-supplied frames (through P.11C), AND into the
   getFrame/cache KEYSTONE, which is UNDER WAY and committed through K.1D (the first real output
   frames) — NOT a fresh start, NOT early cache-core work, NOT pixel-path work, and NOT a
   not-yet-started keystone. Confirm you understand the old/new separation, the
   propose -> review -> approve rule, the proven-code-stays-proven rule (R-PROCESS-21, and the
   K.1D reorientation it came from), and the lifecycle-from-documentation rule (R-PROCESS-22).
b) The result of confirming the build state from the repository (section 2): the latest commit,
   the edit-version marker (CMS07-K.1D-live-frame0-fresh-start-store-return-proof), the four-way
   selftest results (49/49), and confirmation that the --verbose trace shows the P.1A-P.11C pixel
   scenarios AND the K.1A/K.1B keystone scenarios present and passing. Plus the K.1C scaffold
   audit result (section 0.1 / section 2 step 4), and confirmation that cnr3_frame_processing.cpp
   is a member of the cnr3 PLUGIN project. If you cannot run the build, say so and confirm from the
   git log and source instead.
c) Any questions or ambiguities in CMS07.10 or the pack — and any direction / scope / proof-approach
   / test-design questions you want reviewed (section 3). For K.1E, the load-bearing points are the
   predecessor-sourcing path (cached output[0] only, never source[N-1]), the VS-LIFECYCLE-01 /
   frame-return timing, and the proven-code boundary on the P.11B reuse.
d) An enumerated prevailing-rules list for verification/reconciliation, marking each item
   REGISTER-OWNED (section 3A) or CMS-DEFINED / HANDED-OFF (CMS07.10).
e) For the LIVE task, D.2 (section 0.1), respond as TEXT for review — NOT code, NOT
   applied:
     - your INDEPENDENT reasoned view on the OPEN structural question (option (a) edit the N-gate
       in place vs option (b) additive separate frame-1 block), judged against the ACTUAL committed
       shape of the getFrame callback, with your reasoning (the designer wants your independent
       read, not a confirmation of the coordinator's leaning toward (b));
     - your plan for the marking (the two greppable families: SCAFFOLD_* for new K.1E behavioural
       code with an unwind note; CNR3_KEYSTONE_* for the existing trace/gate), the proven-code
       boundary (thin public P.11B call; P.11C body byte-unchanged; cache via public API; stop-and-
       raise if it needs touching proven internals), and the proof (the KDT predecessor-identity
       fields + at least one known-answer byte-check vector; the ownership tail acquired=1/released=1/
       transferred=0/balance=0; the rpStrictSpatial -> rpGeneral declaration);
     - the K.1C scaffold audit result in the cover-note form.
   Send this for designer review and agreement on the structure BEFORE producing the patch; the
   patch is read-first.
```
Do not assume any rule carries over silently. Do not code, create files, rename files, copy
salvage, or wire getFrame without explicit user discussion, agreement, and instruction. When in
doubt about anything — direction, scope, proof approach, test design, whether reuse would touch
proven code, or whether a divergence from vsCnr2 is intended — raise it for review (section 3).
