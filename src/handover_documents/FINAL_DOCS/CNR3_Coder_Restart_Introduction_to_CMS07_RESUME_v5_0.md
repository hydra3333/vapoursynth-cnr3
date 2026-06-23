# CNR3 — Development RESUME on the CMS07.8 cache + pixel architecture
*(Coder restart introduction (v5.0) — paste this at the start of a new memoryless chat,
ahead of the attached handover pack files. This is a RESUME of an in-progress, proven build
that is past its cache-core milestone, through its entire real-frame pixel path on
caller-supplied frames, AND into the cache↔pixel / getFrame KEYSTONE — which is now UNDER WAY
and committed through K.1D (the first real output frames). It is NOT a fresh start, it is NOT
"early cache-core work," it is NOT pixel-path work, and the keystone is NOT un-started — read
the state below carefully. The immediate live work is the next keystone subphase, K.1E
branch-(c), which is at the pre-patch / open-question stage.)*

This chat **resumes** CNR3 development on the CMS07.8 architecture. The build is far advanced
and proven phase-by-phase: the cache core is complete and proven, the entire scalar pixel
decision pipeline is complete and proven, the native byte-buffer access/bridge layer is
complete and proven, the real-VapourSynth-frame pixel path is complete and proven on
caller-supplied frames, AND the getFrame/cache keystone is now under way — four keystone
subphases (K.1A–K.1D) are committed, including the first REAL output frames produced through
live getFrame. Your job is the NEXT keystone subphase — **K.1E branch-(c), the
live-predecessor-present frame-1 compute** — after first confirming the build state from the
repository and running a short scaffold audit.

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
architecture is the CMS07.8 design, and it **completely supersedes** the previous CMS06.x
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
KEYSTONE:            UNDER WAY, committed through K.1D (decomposed K.1A-K.1G):
                       K.1A request-plan structures + temporary KDT dev-trace   (count -> 46)
                       K.1B direct cached-output-return ownership (synthetic)    (count -> 47)
                       K.1C live getFrame passthrough scaffold                   (plugin-only)
                       K.1D live frame-0 fresh-start store/return via copyFrame  (plugin-only)
                            *** the FIRST REAL CNR3 output frame ***
Latest committed:    CMS07-K.1D-live-frame0-fresh-start-store-return-proof
Selftest count:      47/47 PASS (forced-fail 46/47 exit 1; verbose 47/47).
Branch:              dev_cache_manager
```
So: the cache core is done, the pixel maths is done, the scalar->native bridge is done, the
real-VS-frame pixel path is done on caller-supplied frames, AND the keystone has begun and is
committed through K.1D — the point at which the plugin produces its first real output frame
(frame 0) through live getFrame, stored as cache-authoritative and returned, with N>0 cleanly
refused. What is NOT yet done, and is your immediate forward work, is the **next keystone
subphase, K.1E branch-(c)** — see section 0.1 for the live state of that.

### The K.1A–K.1D commits, in one line each (what they did)
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
K.1E branch-(c) plan. This introduction carries the live K.1E state (section 0.1) because that
state is newer than Document B. Read both; where they overlap, Document B and the repository
win on build state.**

---
## 0.1 THE LIVE TASK — K.1E branch-(c) (live predecessor-present frame-1 compute), AT PRE-PATCH
This is your immediate forward work, and it is at a specific point: the designer has stated the
marking rules and the proven-code boundary, and has put ONE open structural question to the
coder for an independent reasoned view, BEFORE any patch. A prior coder chat began answering and
was cut off by a hard limit; **nothing was agreed.** You are asked to form your OWN independent
view (the designer wants an independent read, not a confirmation of theirs), and to do a short
scaffold audit, before producing any patch.

### What K.1E branch-(c) must do
```text
Marker name:  CMS07-K.1E-live-predecessor-present-frame1-compute-proof
N == 1, after K.1D has stored output[0]:
  arInitial:          acquire cached output[0] as the predecessor (REAL lookup/addref, carried
                      in frameData); request source[1]; return NULL.
  arAllFramesReady:   retrieve source[1]; use cached output[0] as the predecessor; compute
                      output[1] via the PROVEN P.11B composition path; RELEASE the predecessor
                      after use; store output[1] per existing checkpoint policy; return output[1].
Ownership (the OPPOSITE tail to K.1B): acquired=1, released=1, transferred=0, balance=0
  (a predecessor is consumed-and-released, NOT transferred).
Dependency declaration: rpStrictSpatial -> rpGeneral (this resolves companion FI-04; see below).
N > 1: clean refusal (branch = after-frame1-before-recovery-wiring) until the next phase wires it.
This phase proves N == 1 ONLY.
```

### Three confirmations already settled with the prior designer/coder loop (carry them)
```text
1. DEFER scene-change. K.1E is predecessor-present composition only; P.11C already proves
   reset for a given threshold. (Scene-change is a pixel-path decision, deliberately placed
   before the keystone.)
2. Frame-1 acceptance = predecessor WIRING proof, NOT blend math (P.11B owns the math). The KDT
   must prove the predecessor was specifically the cached output[0] (e.g. pred=0,
   pred_source=output_cache, pred_lookup=hit) and was released (pred_released=1, pred_balance=0);
   AND there must be at least one KNOWN-ANSWER vector so frame 1 has a real byte-check, not pure
   KDT self-report.
3. P.11B-call scope = a THIN exposure of proven code only — the P.11C body is untouched, no
   re-routing of proven internals, no new pixel/copy algorithm, report-before-broadening.
   (This is the bar to watch hardest, per the dropped K.1D patch.)
```

### Marking rules for K.1E (point 1 — settled ground rules; agreed, restated)
```text
- The KDT dev-trace (CNR3_KEYSTONE_DEV_TRACE) is observation-only and stays OUTSIDE the
  SCAFFOLD framework, per diagnostics spec section 2.8. Do NOT wrap it in SCAFFOLD_*. The new
  frame-1 KDT line (e.g. PREDECESSOR-PRESENT-COMPUTE) EXTENDS the existing KDT family under
  CNR3_KEYSTONE_DEV_TRACE.
- Behaviour-ALTERING temporary code newly introduced for K.1E uses BOTH a `// BEHAVIOURAL-SCAFFOLD:`
  comment tag AND a `SCAFFOLD_*` macro/guard, with an inline unwind note (what replaces it / when),
  per R-PROCESS-12 Part C.
- Do NOT rename the existing CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF guard, and do NOT restructure
  the existing KDT gating "for tidiness." The CNR3_KEYSTONE_* family is already uniformly named and
  is scheduled for removal at the post-K.1G cleanup, which is the place to consolidate.
- K.1E therefore temporarily spans TWO documented, greppable families: SCAFFOLD_* (new K.1E
  behavioural scaffold) and CNR3_KEYSTONE_* (the existing trace/gate). State this in the cover note.
```

### Proven-code boundary for this changeover (point 3 — settled; this carries real risk)
```text
- The proven pixel path (cnr3_frame_processing.cpp, P.10A-P.11C) and the proven cache-core
  internals are NOT modified. P.11B is reached only by a THIN call to a public entry point; the
  P.11C body is byte-unchanged; the cache core is used only through its public store/lookup API.
- cnr3_frame_processing.h is currently included only by cnr3_frame_processing.cpp, so K.1E will
  be the FIRST new includer (from vapoursynth-Cnr3.cpp) — keep that include and call thin and public.
- If standing up the predecessor-present compute appears to require touching ANY proven pixel or
  cache-core internal, STOP and raise it as a design question before writing it; do not fold it
  into the patch (R-PROCESS-21). The standing preference is not to touch proven code absent a very
  good reason, large safety margins, and explicit before/after marking.
```

### THE OPEN STRUCTURAL QUESTION (point 2 — genuinely open; your independent reasoned view is wanted)
K.1E needs N==1 to become a real predecessor-present compute, and the NOT-YET-IMPLEMENTED refusal
boundary to move from N>0 to N>1. Two structures are possible for how the frame-1 path relates to
the existing, harness-proven K.1D frame-0 block:
```text
(a) Edit the existing N-gate block in place, with explicit BEFORE/AFTER temporary-code comments
    marking the change; or
(b) Leave the K.1D frame-0 path byte-untouched and add the frame-1 predecessor-present path as a
    separate, additive SCAFFOLD_*-guarded block alongside it.
```
The coordinator's preference LEANS toward (b) — keeping the harness-proven K.1D block byte-stable
and the diff purely additive — **but that preference is NOT absolute**: it yields when (b) has no
clean form and would produce two tangled half-gates that are harder to read and to unwind than one
cleanly edited gate. **Give your reasoned view on which is better given the ACTUAL local shape of
the getFrame callback in the committed source** — including whether (b) duplicates control flow in
a way that hurts readability/unwind, and whether (a) can be confined and clearly marked. This is a
genuine judgement call; the designer wants your independent read of the real code, not a
confirmation of the leaning. Send it as TEXT for review BEFORE producing the patch.

### Scaffold audit owed before the patch cover note (K.1C passthrough must be confirmed gone)
Before producing the K.1E patch, audit the committed source and state explicitly, in the cover
note, whether the K.1C passthrough scaffold is fully removed from the committed tree:
```text
- CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD   (the old K.1C guard)
- SCAFFOLD_NOT_FILTERED                  (the passthrough marker)
- LIVE-SCAFFOLD-PASSTHROUGH              (the KDT line)
- any source[N] passthrough-return branch that existed only to prove the getFrame wiring
```
Given K.1D, these should be ABSENT — but verify against the actual committed
`src/vapoursynth-Cnr3.cpp` and `src/cnr3_build_config.h` (NOT any circulating .txt snapshot)
and report. If anything is deliberately retained as a permanent guard rather than scaffold, say
so and why. (The K.1C five R-ARCH-06 fences are a separate matter; this audit is about the
temporary passthrough only.)

---
## 1. Attachments expected for this resume
Do not proceed from this introduction alone. The handover pack is (current versions — take the
highest version number present in the handover-documents folder if any differ from the names
below):
```text
1. This introduction:
   CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v5_0.md
2. Controlling design:
   cnr3_cache_manager_design_v7_8.md
   (CMS07.8 — controlling design authority. Supersedes all earlier CMS07.x and CMS06.x.
    Since CMS07.3 it has added: section 9.7 (the keystone predecessor-sourcing consolidation),
    section 9.7.7 (the source-input dependency declaration rpGeneral, resolving FI-04), and
    section 9A.1.1 (the arInitial/arAllFramesReady frame-return contract).)
3. Project context / standing rules:
   Document_A_CNR3_Project_Context_and_Standing_Rules_v3_3.md
   (reproduces Production Spec canonical context + the full section 3A register, incl.
    R-PROCESS-20/21/22.)
4. Current work plan + BUILD STATE:
   Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_2_9.md
   (RESUME-state: current build state through K.1D, the working method, the salvage inventory,
    the NEXT phases, the do-not-implement list, and a keystone status note carrying the K.1A-K.1D
    detail and the K.1E branch-(c) plan. READ THIS for where the build actually is and what comes
    next. If a higher v3.2.x exists, use it.)
5. Production Spec:
   CNR3_Handover_Pack_Production_Spec_v2_7.md
   (canonical context master + populated section 3A register-owned rules, incl. R-PROCESS-19,
    R-PROCESS-20 (PDAP), R-PROCESS-21 (proven-code-stays-proven), R-PROCESS-22
    (lifecycle/API contracts from documentation, not test behaviour).)
6. Diagnostics spec:
   cnr3_diagnostics_specification_v1_5.md
   (subordinate to the CMS and section 3A; section 2.8 = the temporary keystone KDT dev-trace,
    removed at the post-K.1G cleanup.)
7. Manifest:
   CNR3_Handover_Pack_*_MANIFEST.md
   (reading order and pack contents; may lag the version numbers above — the individual
    documents' own version headers win.)
```
Companion documents that may also be included (read if present; they are not the controlling
design):
```text
- CNR3_Designer_Reviewer_Role_Handover_v1_6.md
  (the DESIGNER/REVIEWER role doc — HOW the design/review role is performed: review disciplines
   D1-D16, decision heuristics, the pixel-layer reference confirmed against vsCnr2.cpp, the
   accuracy rule, the recorded deliberate divergences, and the review checklist + keystone "live
   hunting list". It is reviewer-oriented, but it is the clearest map of what the coordinator will
   scrutinise — and what you should self-check before proposing. D16 is the proven-code rule above.)
- CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md
  (the keystone delta companion to Document B — the K.1A-K.1D commits, the K.1D reorientation, the
   K.1E investigation, the owed items, and the scaffold audit. If included, it is the fullest
   single narrative of the keystone so far.)
```
NOT part of the durable implementation authority (do not treat as controlling):
```text
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7_8.md
  (NON-NORMATIVE companion to CMS07.8; deferred tuning questions only; ignore for implementation.
   Note: its FI-04 is now RESOLVED into CMS section 9.7.7 — the rpGeneral declaration.)
- the old .txt reference source under src/superseded_by_v7/ (see Document B section 8.5 salvage
  inventory; salvage is per-case, approval-only).
- older Document B / Role Handover / introduction versions, and the old CMS06-era decision log
  and volatile-state snapshot — out of scope as active inputs (intentionally, to keep stale
  assumptions from re-entering).
```
If **CMS07.8 itself is not attached**, stop and say so. You may comment on this introduction, but
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
       CMS07-K.1D-live-frame0-fresh-start-store-return-proof
3. Build + run the isolated cache-core selftest (it also carries the pixel-proof tests and the
   K.1A/K.1B keystone selftests) and confirm the four-way:
       Debug   normal                              -> 47/47 PASS, exit 0
       Release normal                              -> 47/47 PASS, exit 0
       Release --force-fail-for-harness-proof      -> 46/47 PASS, 1 FAIL, exit 1
       Release --verbose                           -> 47/47 PASS, exit 0
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
If CMS07.8 conflicts with, or is merely unclear in alignment with, prior material:
    CMS07.8 wins unless the user explicitly says otherwise.
If CMS07.8 itself is silent, ambiguous, or incomplete on an implementation point:
    stop and ask (see section 3). Do not guess and do not improvise.
```
References to "CMS07.0" (or any earlier CMS) as controlling, inside reproduced rule text, mean
the latest prevailing CMS — currently **CMS07.8**. Specific CMS section pointers are
version-specific and must be re-checked against CMS07.8. All pre-CMS07 cache code/design is
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
CMS07.8 defines an atomic-scope register, AS1-AS7. It is designer-owned and inviolable. Every
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
    instrumentation / atomic-scope rules defined in CMS07.8. NOT duplicated, indexed, or re-IDed
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
   selftest results (47/47), and confirmation that the --verbose trace shows the P.1A-P.11C pixel
   scenarios AND the K.1A/K.1B keystone scenarios present and passing. Plus the K.1C scaffold
   audit result (section 0.1 / section 2 step 4), and confirmation that cnr3_frame_processing.cpp
   is a member of the cnr3 PLUGIN project. If you cannot run the build, say so and confirm from the
   git log and source instead.
c) Any questions or ambiguities in CMS07.8 or the pack — and any direction / scope / proof-approach
   / test-design questions you want reviewed (section 3). For K.1E, the load-bearing points are the
   predecessor-sourcing path (cached output[0] only, never source[N-1]), the VS-LIFECYCLE-01 /
   frame-return timing, and the proven-code boundary on the P.11B reuse.
d) An enumerated prevailing-rules list for verification/reconciliation, marking each item
   REGISTER-OWNED (section 3A) or CMS-DEFINED / HANDED-OFF (CMS07.8).
e) For the LIVE task, K.1E branch-(c) (section 0.1), respond as TEXT for review — NOT code, NOT
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
