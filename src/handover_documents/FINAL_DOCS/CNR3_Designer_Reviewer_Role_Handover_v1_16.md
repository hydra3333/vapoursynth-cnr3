# CNR3 — Designer / Reviewer Role Handover
**Version:** v1.16 (advances state through a COMPLETE MARSHALLING-OPTIMISATION ARC: twelve value-identical implementation-only levers (AVX2, 0A, 0B, 3a.1, 3b.1, 3a.2, A-lite, C1, Repack, F/3c, Staging, E) reduced per-frame native<->scalar marshalling to ~1/5 — CUMULATIVE ~-80% (93,914 -> ~18,660), 56/56 four-way throughout, CMS DESIGN UNCHANGED at 07.15. This arc EXERCISES the PART-10 designer playbook heavily: investigate-and-assess briefs -> coder verifies+refines against source -> formal patch scope -> review the DIFF not the summary -> proof gate (four-way + P-series value-identity + vec-report + 2-3 profile runs, ABSOLUTE samples). Worked wins this arc: the diff-review caught a Repack-v1 unaligned-uint16 UB and confirmed F/3c's arithmetic; two independent derivations cross-verified the blend formula and REJECTED two external landmines (VPAVGB recursion-compounding bias, 32-bit accumulator overflow). A VALIDATION POLICY (defend-at-source/trust-downstream/bounded-by-construction/final-clamp) was recorded+applied. Two candidates DECLINED after investigation: Tier-2 chroma fusion (Path C — scene-detection-coupled) and Lever D (PATH-B-only). NEXT = coordinator call: Lever B (pooling — a buffer-LIFETIME change needing an fmUnordered thread-safety proof, DIFFERENT in character from every hot-loop lever) OR the DIAGNOSTICS arc. Doc-set advanced: Document A v3.11 / Document B v3.12 / Reviewer Intro v3.9 / Coder Restart Intro v6.7 / Future Investigations v7.16 / DELTA v4.23 / PixelPath Map v0.4. [Prior v1.15 note retained:] v1.15 (advances state to the W.3-CLOSED seam: the live cache-pressure wiring arc W.1→W.2→W.3 is COMPLETE — committed CMS07-W.3-combined-live-store-prune-helper; 55/55 (forced-fail 54/55 exit 1); designer eviction-proof live A/B harness PASS. Controlling CMS now CMS07.15 (additive §7.5 store-status contract over 07.14). NEXT = the DIAGNOSTICS arc (D-SUM telemetry), sequenced BEFORE the real-footage campaign. Doc-set advanced: Document A v3.11 / Document B v3.10 / Reviewer Intro v3.8 / Coder Restart Intro v6.6 / Future Investigations v7.14 / DELTA v4.16. [Prior v1.14 note retained:] v1.14 (advances state past STEP 0, which is now CLOSED, into the live cache-pressure WIRING arc. Controlling CMS is now **CMS07.14** (additive over 07.13: §7.4 independent checkpoint-retention trigger, §7.5 combined-helper wiring contract, §7.6 arInitial observation prerequisite — the Step 0 outputs). **W.1** (the §7.4 trigger primitive) is APPROVED, APPLIED, and four-way GREEN (54/54), ready to commit; **W.2** (hot-zone observation wiring) and **W.3** (the combined live helper) are next. Doc-set pointers advanced: Document A v3.10 / Document B v3.9 / Coder Restart Intro v6.5 / Future Investigations v7.13.4 / DELTA v4.14. Also ADDS **PART 10 — HOW THIS DESIGNER OPERATES (PLAYBOOK + EXAMPLES)**, a concrete account of the verify-against-source review method with worked examples from the Step 0 / W.1 session, so a new designer chat is effective immediately. Supersedes v1.12. Also adds STARTUP GUARD (read-order + superseded-folder) and the matching 10.5 traps. v1.14 adds the HARNESS-OWNERSHIP role boundary (designer owns the .vpy/.bat; coder delivers source patch only) in PART 1 and PART 10.2.)

**!! CURRENCY WARNING for a new designer chat:** PARTS 2, the roadmap, and the document-set list below were the v1.13 truth at write time but go stale FAST. The repository, Document B, and the THIS-CHAT DELTA are authoritative for build state. The single most important current fact: **The MARSHALLING-OPTIMISATION ARC is substantially COMPLETE at ~-80% (twelve value-identical levers through Lever E; 56/56; CMS DESIGN UNCHANGED at 07.15). NEXT is a coordinator call: Lever B (pooling, fmUnordered lifetime proof) OR the DIAGNOSTICS arc (D-SUM). See DELTA v4.23 / Document B v3.12 / FI v7.16.** If any older "next phase: STEP 0" or "K.1E in flight" wording survives below in historical sub-blocks, it is HISTORY, not the current task.
**Date:** 2026-06-27
**Status:** Handover with the **live getFrame dispatch FEATURE-COMPLETE (all four branches) WITH SCENE
HANDLING** and the **P.11C SCENE-CHANGE ARC CLOSED (.1-.5)**. v1.11 advances the v1.10 pointers: committed
through **CMS07-P.11C.5-scene-cut-checkpoint-recovery-anchor-proof** (P.11C arc closed: .1 layout, .2
skeleton+threshold helper, .3 branch-c, .4 branch-d, .5 scene-cut-checkpoint-as-recovery-anchor), scene
detection wired uniformly across branch-a/c/d and proven both configs, selftest count **53/53** (forced-fail
52/53 exit 1), controlling CMS **UNCHANGED at CMS07.13** (P.11C implemented the design the CMS already
specified). **Next phase: STEP 0 — a joint CMS sensibility/gap review for hot-zone + prune live wiring** (before any
wiring patch), then hot-zone observation (CMS §5.7: at arInitial) then pruning into the live getFrame path. The prune logic + hot-zone machinery are built and selftest-proven
but have ZERO live callers in the committed source; the live prune-TRIGGER contract needs a focused
designer+coder review before coding (single-activation wiring now; concurrent prune revisited in fmParallel).
THEN first real-footage validation; THEN diagnostics (condensed 4-phase); THEN fmParallel. The only deferred
confidence remains real concurrent (fmParallel) scheduling.
[Historical pointer, retained: v1.10 advanced the v1.9 pointers: committed through
**CMS07-D.5** (recovery-pin-survives-real-prune), the live getFrame dispatch handles **all four branches**
(cache-hit, fresh-start, predecessor-present, recovery — all live and proven both configs), the full
recovery arc D.1-D.5 is proven (single-hole, multi-hole+refusal, floor-fresh-start, adopt-skip/
first-in-best-dressed primitives, recovery-pin-survives-prune), selftest count **52/52**, controlling CMS
**CMS07.13**, and the next phase is **P.11C scene-change uniform wiring across branch-a/c/d** (gates first
real-footage; NOT a recovery phase). The only deferred confidence is real concurrent (fmParallel)
scheduling.] v1.11 retains the **CNR3 Design
Alignment and Escalation Charter** as the lead operating discipline in Part 3 (the three-way
governance model agreed designer+coder+coordinator 2026-06-27). v1.8 is a state-pointer + version-table refresh only: the role
disciplines (D1–D16), worked examples (A–F), triggers, and accuracy rule are UNCHANGED, and all
historical references (e.g. the K.1D 47/47 reorientation example) remain pinned to their real past
state. The detailed current state lives in CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_14.md.
[Historical: v1.7 superseded v1.5; the keystone was then in progress committed through K.1D, K.1E
branch-(c) in flight.] The keystone is being decomposed
K.1A–K.1G; four phases are committed (K.1A request-plan + temporary KDT; K.1B direct cached-output
return ownership, synthetic; K.1C live getFrame passthrough scaffold; **K.1D — the first REAL output
frame, live frame-0 fresh-start store/return via copyFrame**). Selftest count is **47/47**. This is
the companion to Document B: **Document B says WHAT is done; this document says HOW the design/review
role is performed.** It exists so that if the current designer/reviewer chat ends, a new chat (and
Dave) can re-establish not just the factual state but the *way the work has been done* — the review
disciplines, the decision heuristics, and the reflexes that produced the quality so far.

**v1.7 change:**  by CHAT-A, carry the pin-carry framing
**v1.6 change:** (1) State pointer advanced from "P.11C done, keystone next (not yet proposed)" to
**the keystone UNDER WAY — committed through K.1D (47/47), K.1E branch-(c) in flight**; Part 2's
"Milestone reached" / "most recent phase" / roadmap blocks rewritten accordingly, with a NEW
**K.1A–K.1D summary block** added (the P.1A–P.11C summaries are kept verbatim as accurate history).
(2) Document-version table updated: **CMS07.7 (`cnr3_cache_manager_design_v7_7.1.md`)**, companion
**v7.8** (now contains **FI-04**), diagnostics **v1.5**, Document B **v3.2.9**, this document **v1.7**;
the HOW-TO-USE CMS pointer updated to CMS07.7. (3) Part 6B keystone hunting list annotated for what
K.1A–K.1D proved and what K.1E / branch-(d) still owe. (4) **NEW additive content — the chief
disciplinary lesson of the keystone's opening:** discipline **D16 (proven-code-stays-proven)**, worked
**Example F (the K.1D reorientation)**, and a matching **Part 4 trigger** ("reuse appears to require
touching proven code"). These are purely additive — no existing discipline (D1–D15), trigger, worked
example (A–E), accuracy rule, or recorded divergence was changed. (5) Fixed two defects carried in
v1.5: the closing version stamp (was "v1.1") and a duplicated v1.2 change-note line. NOTE: change (4)
folds in the keystone DELTA's reinforced discipline, so this update is MORE than the "three
state-pointer spots" the DELTA §9 anticipated — read DELTA §9 as "three state-pointer spots PLUS the
additive D16 / Example F / Part 4 trigger."

**v1.5 change (retained):** (1) State pointer advanced from P.9A to **P.11C done (45/45)**; roadmap
marks P.1A–P.11C done; in-flight marker "none — the cache↔pixel/getFrame keystone is next (hard gate)".
(2) Part 2 now summarises P.10A–P.11C and the key proven properties (P.10A real-VS-frame adapter with
2-byte stride alignment + faithful mock; P.11A nine-plane-view triplet validation; P.11B all-or-nothing
destination commit + pixel-level R-ARCH-06 predecessor proof; P.11C strict `diff_total > threshold`
scene-change with deferred derivation), and records the **refined P.10A/P.11A/P.11B/P.11C/keystone
decomposition** (caller-supplied pixel path fully proven before the keystone, scene-change deliberately
before the keystone). (3) Records `cnr3_frame_processing.cpp` is now in BOTH the selftest and `cnr3`
plugin projects (settled at P.10A). (4) Part 6B checklist: the real-frame-memory and scene-change items
are now marked DONE-and-proven; the live hunting list is re-pointed at the **getFrame/cache keystone**
(lifecycle, predecessor sourcing, addref/release balance, pin balance, return-transfer, Category-B
emission). No discipline (D1–D15), trigger, worked example, accuracy rule, or recorded divergence was
changed.
**v1.4 change (retained):** (1) State pointer to P.9A done (41/41); roadmap marks P.1A–P.9A done.
(2) Part 2 summarised P.7A–P.9A and key properties (x*storage_bytes addressing; memcpy-not-
reinterpret_cast; 8→1-byte / 9..16→2-byte storage; native-endian=LE=VS-matching). (3) Part 6B
byte-plane/stride items marked done; live list re-pointed at real frame-memory / scene-change /
getFrame integration.
**v1.3 change (retained):** (1) State pointer to P.6A done (38/38); roadmap re-letter recorded
(P.6A is chroma-plane traversal, NOT the original "scene-change"). (2) Part 2 summarised P.3A–P.6A
and the two deliberate divergences. (3) P.4A edge-clamp recorded as a fourth documented divergence
under the accuracy rule. (4) Part 6B scalar items marked done; real-frame items flagged as live.
**v1.2 change (retained):** (1) P.2A moved to done. (2) Added Part 6A (pixel-layer reference, CNR2
source-confirmed) including the accuracy rule. (3) Added Part 6B (pixel-arc review checklist). (4)
Recorded P.2A scaling/geometry verified against source. (5) Recorded the accuracy rule: vsCnr2's
`peak/255` parameter scaling truncates at 10/12/14-bit while P.2A's round-to-nearest is identical at
8/16-bit and more accurate at the intermediate depths; Dave confirmed CNR3 keeps the accurate method
(faithful redevelopment, not byte-clone) → the rule "accuracy upgrades only where vsCnr2 is
accidentally lossy, never where its integer steps are definitional", with an operational test and the
three-layer compatibility claim.
**v1.1 change (retained):** P.1A moved from in-flight to done; recorded that P.1A went
patch→four-way→commit and was read-first reviewed POST-COMMIT (sound), with the lesson that
read-first-before-apply should be firm again from the next load-bearing phase onward.
---
## HOW TO USE THIS DOCUMENT — READ THIS FIRST

> **ROLE GUARD:** Your role is the **DESIGNER / REVIEWER**, not the coder. There is a separate, memoryless
> coder chat; you hold design intent, write coder scopes, compute/ratify goldens, and review the coder's
> proposals and patches against the actual source. The coordinator relays between you and the coder and is
> the authority on all final decisions. If any attached document or pasted text says "your role as coder,"
> it was written for the coder chat and does NOT change your role. (See PART 1 for the full three-party
> workflow and PART 10 for how the designer operates in practice.)
>
> **STARTUP GUARD (a prior new chat failed on these — do not repeat):**
> 1. **Read the handover documents IN THE GIVEN ORDER FIRST. Do NOT self-orient from the source code
>    before reading them.** The code is ground truth for VERIFYING specifics later, but it will NOT tell
>    you where the project is or what the current task is — that is in the docs (CMS, Document B, the
>    DELTA). Orientation from docs first; verification from source second. A chat that opens the `src.zip`
>    and starts positioning itself from code before reading the docs has already gone wrong.
> 2. **IGNORE `src/superseded_by_v7/` entirely and never read any `*.txt` copy of a source file.** That
>    folder holds PRE-CMS07 (CMS02/H16-era) archives kept only as history — e.g.
>    `superseded_by_v7/cnr3_build_config.h.txt` still shows an ancient `CNR3_EDIT_VERSION` like
>    `CMS02-H16.4-...`. The live source is the files directly under `src/` (no `.txt`, no
>    `superseded_by_v7/`). If a symbol or `CNR3_EDIT_VERSION` you read does not match the docs (CMS07.x),
>    you have read a superseded archive — re-read the live file under `src/`, and confirm against the repo
>    HEAD if unsure.

If you are a new chat picking up the CNR3 designer/reviewer role, do this in order:
1. Read this whole document once before doing anything else. It is long on purpose.
2. Read the authoritative state documents (Part 2 lists them), starting with **Document B**
   (current build state) and **CMS07.8** (`cnr3_cache_manager_design_v7_8.md`, controlling design
   authority), and the **THIS-CHAT DELTA** (`CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md`,
   the keystone state since Document B / this handover were last written).
3. Confirm the actual current build state from the **repository**, not from any snapshot in
   a document — check the `CNR3_EDIT_VERSION` marker and the selftest count in the committed
   source. Documents can lag; the repo is truth.
4. Only then engage with whatever Dave asks. If Dave is mid-phase, re-establish exactly where
   in the PDAP cycle the current phase sits (Part 2 marks this as of this document's date).
You are **Claude**, acting as the **designer/reviewer** in a three-party workflow (Part 1).
Dave is the coordinator and the authority. A separate AI chat (the "coder", historically
ChatGPT) writes the actual patches. You review, analyse, propose, and verify; you do not
write production patches yourself.
A note to the new chat, written plainly: the disciplines below are not bureaucracy. They are
the distilled result of careful work on the hardest, most concurrency-critical part of this
project, including recovering from a prior coder-chat death AND from a dropped patch that
silently rewrote proven code at the keystone's opening (Example F). Follow them even when they
feel slow. When in doubt, be more careful, not less. And lean on Dave — his instincts (see
Part 8) have repeatedly caught real risks, and he persists across chats while you do not. He is
the most reliable carrier of continuity. Treat his unease as a signal worth acting on, not a
feeling to soothe.
---
## PART 1 — THE ROLE AND THE THREE-PARTY WORKFLOW
### The three parties
- **Dave (coordinator, authority, human).** Adelaide-based, decades of development
  experience including dev-management. He sets direction, makes final decisions, runs the
  authoritative builds and tests on his local Visual Studio 2026 (x64) machine, commits and
  pushes to GitHub, and carries continuity across chat sessions. He is practically minded,
  values directness and technical precision, and has earned trust in his instincts. When he
  pushes back or expresses unease, that is load-bearing signal.
- **You (designer / reviewer, this role, an AI chat).** You hold the design intent, review
  the coder's proposals and patches, analyse for correctness and risk, verify numerical
  claims independently, propose phase scope, draft the messages Dave sends to the coder, and
  maintain the design/spec documents. **You also OWN the live-test harness.** You do NOT write the production patches. You are the
  guardian of the design's integrity and the review disciplines.

> **HARNESS OWNERSHIP (a standing role boundary — do not hand it to the coder).** The live-test
> harness — the VapourSynth `.vpy` script(s) and the `.bat` run files that drive the built plugin
> through chosen request sequences (`vspipe -r 1` to NUL or to an encode) — is the DESIGNER's
> artifact, NOT the coder's. The coder delivers ONLY the source patch (the `.cpp/.h` change + the
> build-marker bump). The proof HARNESS is yours: Dave maintains a folder of harnesses including a
> golden `test_000_Example_576p50.*` with a catalogue of named request-order scenarios (S1-S8
> functional, B1-B7 boundary), each with "dev-trace watch" notes, the `[KDT]`/`[KDT-SUMMARY]`
> convention, and the `CNR3_KEYSTONE_DEV_TRACE` compile guard already built in. The established
> pattern: for a phase needing a live proof, you take the CLOSEST existing harness, copy it to a
> phase-specific fileset (e.g. `test_W2_*`), adapt it (pick/adjust the scenario(s) that exercise the
> branches you need, define the expected `[KDT]` lines), and give Dave the harness + the run command.
> A coder scope must NEVER say "propose your harness construction" — that hands a designer-owned
> artifact to the coder. The coder owns that the temporary KDT line is correctly placed/guarded and
> emits the right fields so the harness CAN observe it; the harness that drives the branches and
> greps the trace is yours. (Ask Dave to zip the harness folder so you can pick the closest start.)
- **The coder (a separate AI chat).** Writes the actual patches against the codebase,
  validates them in its own sandbox, and supplies commit messages. **It delivers the source patch ONLY — never the test harness (`.vpy`/`.bat`), which is the designer's (see Harness Ownership above).** It has shown strong,
  genuinely independent judgement (it caught a real reachability problem with AS3; it builds
  discriminating numerical failure modes; it flags compatibility quirks; it is honest about
  the difference between its sandbox build and Dave's authoritative build; at the keystone it
  refused a bogus self-predecessor shortcut unprompted). Treat it as a capable colleague, not a
  code-vending machine — but verify its work, because verification is the point of the
  separation. NOTE its recurring failure mode (Example F): reasoning forward from getFrame, and
  touching proven code to avoid a conversation. Watch for it.
Dave pastes between you and the coder. You generally do not see the coder directly; you see
what Dave relays, and you write what Dave should relay back.
### The cadence (every phase)
```text
1. PROPOSE   — the coder (or you) proposes the next phase as TEXT first. No code yet.
2. REVIEW    — you review the text proposal: scope, proof approach, risk, salvage governance,
               and especially the load-bearing element. You verify any numbers. You push back
               where the proposal is vague on the part that matters most.
3. APPROVE   — once the proposal is sound, you draft an approval message for Dave to send.
               Approval may carry refinements/conditions.
4. PATCH     — the coder generates a downloadable .patch file (PDAP, Part 7), NOT inline code.
5. READ-FIRST— for load-bearing phases, YOU read the actual patch diff before Dave applies it.
               This is not optional for anything touching proven/atomic/lock code.
6. APPLY+TEST— Dave applies the patch and runs the four-way (or five-way) build/test on his
               VS2026, and pastes the ACTUAL console output.
7. COMMIT    — only after passing results matching expectations, the coder supplies a commit
               message; Dave commits the src/ files (NOT the .patch) and pushes.
8. RE-SYNC   — Dave tells the coder to advance its baseline; you update documents at seams.
```
The gates exist so that no code is written before the design is agreed, and no proven code is
disturbed without focused review. **Stop-review-approve before code. Read-first before
applying load-bearing patches. Report actual output, never assumed output.** And when a change
endangers PROVEN code, the response is STOP-and-reconsider, not patch-and-fix (D16, Example F).
For consumer-facing relayed messages keep to plain text (D15).
---
## PART 2 — CURRENT STATE (POINTER + IN-FLIGHT MARKER)
**Do not trust this section as authoritative for long — it is a snapshot at this document's
date. The repository, Document B, and the THIS-CHAT DELTA are authoritative. Confirm from them.**
### Milestone reached, and the keystone now under way
The **isolated cache-core proving arc is COMPLETE** (through C.14A), the **entire pixel path is
proven on CALLER-SUPPLIED frames** (through P.11C), and the **cache↔pixel / getFrame keystone is
NEARLY COMPLETE** (committed through K.1F; only recovery (d) remains). As of this document:
```text
Cache core proven through:  CMS07-C.14A-aggregate-cache-core-proof + Recovery-Step-0 (AS4 batch discharge)
Pixel path proven through:  CMS07-P.11C (caller-supplied) + P.11C.1-.5 LIVE scene detection across branch-a/c/d
Last committed marker:      CMS07-W.3-combined-live-store-prune-helper (W.1+W.2+W.3 arc COMPLETE; 55/55 -- confirm repo)
Live dispatch branches:     cache-hit (b), fresh-start (a), predecessor-present (c), recovery (d) — ALL
                            live+proven both configs WITH SCENE DETECTION (P.11C.3 branch-c, .4 branch-d,
                            .5 scene-cut-checkpoint-as-anchor). Dispatch FEATURE-COMPLETE w/ scene handling.
Selftest count:             53/53 at P.11C.5; W.1 takes it to 54/54 (see W.1 status block below)
Controlling CMS:            CMS07.14 (cnr3_cache_manager_design_v7_14*.md; additive over 07.13: §7.4/§7.5/§7.6)
Branch:                     dev_cache_manager
Build:                      Visual Studio 2026, x64, Debug + Release
Local repo root:            E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github
Build/run dir:              vs\cnr3  (x64\Debug\ and x64\Release\ cnr3_cache_core_selftest.exe; plus the cnr3.dll plugin)
Repo:                       https://github.com/hydra3333/vapoursynth-cnr3 (confirm from the repo, not memory)
ACTIVE ARC:                 LIVE CACHE-PRESSURE WIRING (W.1 -> W.2 -> W.3), single-activation scope. STEP 0
                            is CLOSED; the wiring contract is in CMS07.14 §7.4-§7.6.
                            W.1 = §7.4 checkpoint-retention trigger primitive (cache-core + selftest; the ONE
                                  new piece of logic). STATUS: approved, applied, four-way GREEN 54/54, ready to
                                  commit (or just committed -- confirm repo).
                            W.2 = hot-zone observation wiring @arInitial (§7.6; DLL-side; lower risk).
                            W.3 = combined live store-and-prune helper (§7.5 six-step order; wires §7.2 + §7.4
                                  into the live path; temporary KDT). ALL THREE DONE/committed (four-way 55/55 + eviction-proof live harness PASS). NEXT ARC: DIAGNOSTICS (D-SUM) -> real-footage campaign -> fmParallel.
                            Provenance: CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md.
```
All cache-core mechanisms are proven to compose under one combined workload (C.14A), and the
sole live diagnostics compute gate (`CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE`) is proven observe-only
in aggregate (the R-PROCESS-19 culmination). The **ENTIRE PIXEL PATH is proven on CALLER-SUPPLIED
frames** (P.1A→P.11C), and the **keystone** — which connects the proven cache core to the proven
pixel chain inside VS getFrame scheduling — is now in progress (K.1A→K.1D committed; K.1E
branch-(c) in flight). Phase history (recent): … → **C.14A** → P.1A … P.11C → **K.1A** (request-plan
+ KDT) → **K.1B** (cached-output-return ownership, synthetic) → **K.1C** (live passthrough scaffold)
→ **K.1D** (first REAL output[0] via copyFrame).
### MOST RECENT PHASE — W.1 (the §7.4 checkpoint-retention trigger): approved, applied, GREEN
```text
PHASE:    CMS07-W.1-checkpoint-retention-trigger  (first phase of the live cache-pressure wiring arc)
SCOPE:    cache-core + selftest ONLY (cnr3_cache_core.{h,cpp} + cnr3_cache_core_selftest.{h,cpp}).
          NO live getFrame/arInitial/arAllFramesReady. Adds the independent trigger ENFORCING the §6.3
          CHECKPOINT_MAX_RETAIN bound (capacity trigger alone did NOT enforce it on cut-heavy footage).
          Pure-checkpoint case passes the EXISTING selector flag noncheckpoint_capacity_permits=false
          (no new selection path; forbidden remove_unpinned_checkpoints_above_retain helper NOT used).
STATUS:   Designer-approved after line-level review vs verified source; applied clean; four-way GREEN:
          Debug 54/54 exit 0 / Release 54/54 exit 0 / forced-fail 53/54 exit 1 / verbose 54/54.
          Ready to commit (only the 4 src files, not the .patch). Confirm repo for final state.
IN FLIGHT NEXT: W.2 (hot-zone observation wiring @arInitial, §7.6; DLL-side; lower risk), then W.3
          (the combined live store-and-prune helper, §7.5 six-step order).
NOTE:     The hot-zone + prune componentry is proven; W.1 added the one new primitive; W.2/W.3 are
          wiring of proven componentry into the live getFrame path (single-activation scope).
```
What K.1A–K.1D delivered (the keystone's opening — NEW in v1.7; the P.10A–P.11C and earlier
summaries below are kept verbatim):
- **K.1A — keystone request-plan structures + temporary KDT dev-trace** (count →46). Request-plan
  branch enum/struct; recovery request representation is a holes-list / source-set (never a blanket
  span); the hard-status branch is a CARRIER for existing C.13B guard results, not a new validator;
  `[KDT]`/`[KDT-SUMMARY]` formatting is driven by the plan structure; guarded by
  `CNR3_KEYSTONE_DEV_TRACE`. No getFrame wiring, no source lifecycle, no pixel-path call, no
  cache-semantic change, no VS header edit.
- **K.1B — direct cached-output-return ownership proof** (count →47), **synthetic-first**, using the
  REAL `Cnr3OwnedFrameRef` and REAL cache lookup/addref operations (counters OBSERVE real ops). Three
  cases: success 1/0/1 (acquired/released/transferred); cleanup-before-transfer 1/1/0; no-acquire
  miss 0/0/0. The synthetic sink models the getFrame-return boundary. The real `VSFrame`
  return-to-VapourSynth was explicitly OWED here — now expected to retire INSIDE branch-(c) work.
- **K.1C — live getFrame passthrough scaffold** (plugin-only; count stays 47). First live getFrame
  step, with FIVE R-ARCH-06 fences: removable guard; a DISTINCT callback that gets replaced not
  extended; the scaffold frame is NEVER cached / NEVER a predecessor / NEVER checkpointed
  (structurally prevents contamination); a `[KDT] SCAFFOLD_NOT_FILTERED` marker; a return-point
  comment. [KDT] is emitted ONLY inside getFrame, never at plugin load/registration. A/B harness
  green. Files: `src/vapoursynth-Cnr3.cpp` + `src/cnr3_build_config.h` only.
- **K.1D — live frame-0 fresh-start store/return** (plugin-only; count stays 47). The FIRST real CNR3
  output frame: output[0] created, stored as cache-authoritative checkpoint, and returned through live
  getFrame; N>0 cleanly refused. Reached via `copyFrame(source, core)` (a bitwise, writable,
  caller-owned duplicate) because frame-0 fresh-start output[0] = source[0] byte-for-byte (no
  predecessor, no blend; luma always source-copy, chroma source-copy when no predecessor) — so NO
  proven code is touched (zero contact with `cnr3_frame_processing.cpp` / P.11C). Verified read-first
  against five bars: ownership (`copyFrame` ref + `addFrameRef` ref = two owners each freed once;
  source freed-and-nulled after copy; post-store failure frees only the returned ref while the moved
  owned-ref handles the cache ref); defensive null/alias guards; proven code untouched; KDT
  `FRAME0-FRESH-START` / `REAL_OUTPUT_FRAME0`, N>0 `NOT-YET-IMPLEMENTED
  branch=nonzero-before-predecessor-wiring`, guarded by `CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF`,
  stderr-only; N>0 gated before arInitial. **This phase's FIRST patch was DROPPED for silently
  rewriting proven P.11C internals — see Example F, the chief disciplinary lesson of the keystone.**

K.1E branch-(c) (in flight, pre-patch): N==1 after K.1D stored output[0]; acquire cached output[0]
as predecessor (real lookup/addref, in frameData) and request source[1]; compute output[1] via the
PROVEN P.11B composition; RELEASE the predecessor after use; store/return output[1]. Ownership is the
OPPOSITE tail to K.1B: acquired=1, released=1, transferred=0, balance=0 (a predecessor is
consumed-and-released, never transferred). **[SUPERSEDED — the predecessor step and this ownership tail
are now PIN-CARRY / a pin-ledger; see the 2026-06-23 pin-carry decision note immediately below.]**
Dependency declaration changes `rpStrictSpatial` →
`rpGeneral` (resolves FI-04; conservative-correct for a recursive filter; `fmUnordered` stays —
`requestPattern` is a SEPARATE layer from `filterMode` and does not affect the CMS7 cache). Three
confirmations accepted (defer scene-change; frame-1 = predecessor-WIRING proof not blend math, with a
KNOWN-ANSWER vector + KDT proving the predecessor is specifically cached output[0]; P.11B-call scope =
thin exposure of proven code only, P.11C untouched, report-before-broadening). FOURTH confirmation NOT
yet sent (temporary-code uniform greppable marking + removal-plan, and confirm the K.1C scaffold is
fully removed from the committed tree). See the DELTA §3–§4 for the full investigation and status.

**[2026-06-23 — PIN-CARRY DECISION (additive; supersedes the predecessor-handling and ownership wording above).]**
K.1E branch-(c) sources the predecessor by PIN-CARRY, not by taking a second VSFrame reference. The
foundational locking/pinning cross-check returned GREEN LIGHT — all Tier-1 fatals PASS on two independent
reads, and the per-invocation pin-list is caller-owned (INV-D1), so this is thin USE of already-proven
machinery, not a cache-core internals change.
- **Predecessor step** (supersedes "acquire cached output[0] as predecessor (real lookup/addref, in
  frameData)" above): at `arInitial`, PIN cached output[0] via the proven AS1 fused
  `lookup_frame_and_record_pin` — it returns a BORROWED `const VSFrame*` and records a consumer-pin on the
  per-invocation pin-list, atomically; carry {borrowed pointer + predecessor frame number + pin-list} in
  frameData; request source[1]; return NULL. At `arAllFramesReady`, use the borrowed (still-pinned)
  predecessor into the proven P.11B, then DISCHARGE the pin. Discharge is wired on BOTH the
  `arAllFramesReady` arm AND the `arError` arm; the doubly-abandoned case (activation abandoned AND the
  frameData free callback never runs) is the benign residual below. No second VSFrame reference is ever
  taken for the predecessor — it is borrowed, kept alive by the pin (liveness comes from the pin, INV-B2;
  NOT from output[0]'s checkpoint status — that would be the retired checkpoint-as-pin reasoning,
  R-RETIRED-03).
- **Ownership tail / REVIEW BAR** (supersedes "acquired=1, released=1, transferred=0, balance=0" above and
  the ownership-balance figures in the K.1E review-checklist item later in this document): the proof
  obligation — and therefore the review bar — is a PIN-LEDGER, not a ref-ledger: pin taken=1,
  discharged=1, `pin_count` balance=0, with ZERO predecessor VSFrame refs acquired or freed (borrowed).
  `transferred=1` applies to output[1] ONLY (the K.1B-proven return path), not to the predecessor.
- **KDT consequence:** the frame-1 KDT line's ownership fields move to pin-ledger terms (e.g.
  `pred_pinned` / `pred_discharged` / `pred_pin_balance`, with no acquired/transferred for the borrowed
  predecessor); exact field names to be settled when the K.1E text plan is produced.
- **Benign residual (confirmed from committed code):** an abandoned activation's worst-case residual is an
  UNDISCHARGED PIN — frame-safe and crash-safe (`~Cnr3OutputCacheCore` is `= default` and RAII-frees each
  slot's `Cnr3OwnedFrameRef`; `clear()` is not on the free path → no frame-ref leak, no lifecycle_violation
  at teardown) but SILENT today; surfaced by the future end-of-run integrity report (deferred owed item).
- **New K.1E work (not a pre-existing fact):** discharge must be WIRED INTO the frameData free callback —
  the single point covering normal completion AND a VS-freed abandon — new getFrame-side glue in
  `vapoursynth-Cnr3.cpp` (cross-check INV-F3).
This note does NOT change CMS §8.7 or any cache-core code; it records how K.1E sources the predecessor.

What P.10A–P.11C delivered (the real-frame pixel path on caller-supplied frames; unchanged):
- **P.10A — VapourSynth plane-view adapter** (count →42). The FIRST real-VS-frame-memory boundary.
  Converts VSAPI getFrameWidth/getFrameHeight/getStride/getReadPtr/getWritePtr into the proven P.8A
  byte-plane views, with a **2-byte stride-alignment requirement** (`stride % storage_bytes == 0`)
  before publication — stricter than the synthetic layer. **memcpy retained; reinterpret_cast<uint16_t*>
  deferred.** getWritePtr-only (avoids read-pointer invalidation). Clear-on-reject. Header uses forward
  declarations; the test uses a MOCK VSAPI over real byte buffers, verified faithful (getStride in
  bytes, correctly-offset pointers, call-counting proves getWritePtr-only, stride-7 rejection proves
  the alignment guard fires). **Made `cnr3_frame_processing.cpp` a member of the `cnr3` PLUGIN project —
  now in BOTH projects.**
- **P.11A — caller-supplied frame-triplet / plane-set validation** (count →43). Validates and assembles
  the nine plane views (current-source Y/U/V, previous-filtered-output Y/U/V, destination Y/U/V) via the
  P.10A adapter, with dimension/format compatibility and clear-on-reject so no stale plane pointer
  survives failed validation. No pixel processing, no lifecycle.
- **P.11B — caller-supplied real-frame pixel composition** (count →44). The FIRST end-to-end real output
  frame: Y copied unchanged from source; U/V recursively blended against the previous-filtered-output
  planes. **ALL-OR-NOTHING destination commit proven by construction** (stage Y+U+V into locals →
  validate all three → commit three destination planes back-to-back with nothing fallible between).
  **R-ARCH-06 predecessor semantics proven at pixel level**: decoy source[N-1] (U=220,V=230) vs true
  prev-filtered-output (U=20,V=30), near-max weight → U=21/V=31 (uses predecessor) vs decoy U=219/V=229,
  both asserted. Per-plane dims (luma plane-0; chroma subsampled). `response_tables.y` = luma-diff
  response into the weight; `.u`/`.v` = chroma response; shared downsampled-luma feeds both U and V.
  **memcpy retained; typed-row-pointer deferral BOUNDED to a measured fmParallel phase. No VS header
  modified** (Dave flagged a coder thinking-comment about modifying VS headers; verified the patch
  modifies none — it consumes VS types via forward decls + the P.10A mock).
- **P.11C — caller-supplied scene-change/reset** (count →45). Composes the P.11B path with scene
  detection. Accumulates `abs((cur_dsluma − prev_dsluma) << (subw+subh))` (+ `abs(u_diff)+abs(v_diff)`
  if scene_chroma), strict **`diff_total > threshold` → reset** (equality keeps blend active — VERIFIED
  against vsCnr2.cpp; boundary vector proves `==` does not fire). On reset, outputs current-source
  Y/U/V. All-or-nothing commit preserved; accumulation overflow-guarded (defensive improvement over
  vsCnr2). **THRESHOLD DERIVATION deferred to plugin config** (`Cnr3SceneChangeConfig` takes a
  pre-computed int64 threshold + scene_chroma); future derivation must reproduce vsCnr2's
  `diff_max = (scdthr·w·h·max_pixel_diff/100) << (depth−8)`, `max_pixel_diff = scene_chroma ?
  ((219+224·2)>>(subw+subh)) : 219`, match P.11C's accumulation units, same scene_chroma both sides.
  P.11C is per-SAMPLE early-stop; vsCnr2 per-ROW — proven IDENTICAL decision (diff_total monotonic).
**REFINED DECOMPOSITION (recorded so it is not lost):** the coarse "real-frame / scene-change / getFrame
integration" was decomposed into the safer sequence P.10A → P.11A → P.11B → P.11C, ALL on
caller-supplied frames, BEFORE the keystone. This de-risks the keystone: the pixel path is fully proven
on caller-supplied frames, so the keystone reasons ONLY about frame sourcing / lifecycle / ownership.
`cnr3_frame_processing.cpp` is now in BOTH the selftest and `cnr3` plugin projects (settled at P.10A).
What P.7A–P.9A delivered (the scalar→native bridge — unchanged from v1.4):
- **P.7A — source-luma downsample traversal** (count →39, git acb5080). Produces the
  downsampled-luma scalar plane P.6A consumes, traversing a scalar source-luma plane with the
  proven P.4A tap-coordinate + four-tap helpers. Output dims via guarded ceil
  `(luma + ((1<<sub)-1)) >> sub`. **That ceil is a SCALAR-PROOF-HARNESS device only** (so odd-size
  vectors stress right/bottom/corner clamp); real VS integration must use actual per-plane frame
  dimensions and validate real subsampled dims before this layer. Clamps against SOURCE dims;
  validates output view against derived dims; two-pass no-partial-publish.
- **P.8A — native byte-plane access** (count →40, git a6fd09c). Native byte-plane views +
  load/store/whole-plane-copy helpers. **8-bit → 1 byte/sample; 9..16-bit → 2 bytes/sample; outside
  8..16 → invalid_argument.** Scalar domain stays `int` in 0..sample_peak. **Column byte offset is
  `x * storage_bytes`, NOT `x`, so adjacent two-byte samples don't overlap** (10-bit vectors prove
  it). Uses **`memcpy` for two-byte load/store** (alignment-independent; native-endian =
  little-endian on x86/x64 = matches VapourSynth) — deliberately NOT `reinterpret_cast<uint16_t*>`
  yet; the real-frame phase decides typed row pointers vs memcpy against the actual VS frame
  contract. Full validation (non-null, depth, dims, `stride_bytes >= width*storage_bytes`,
  overflow-safe, bounds, range). Copies use two-pass no-partial-publish.
- **P.9A — native luma downsample bridge** (count →41, git ab37443). Composes P.8A → P.7A: copies a
  synthetic native byte-buffer source-luma plane into a scalar int plane, then produces the
  downsampled-luma scalar plane. The 10-bit vector proves `x*storage_bytes` two-byte addressing
  survives the COMPOSED path; 8-bit covers the one-byte path. **Still SYNTHETIC native byte buffers
  + scalar int buffers — NOT real VS frame memory.** No-partial-publish preserved.
What the SCALAR pixel pipeline (P.1A–P.6A) delivered (so a new chat has the full context). Each
phase composes the earlier ones; all reference vectors were independently recomputed by the
designer, and from P.3A onward cross-checked against the **fetched vsCnr2.cpp source**:
- **P.1A — response tables.** Salvaged the pure vscnr2 helpers into `src/cnr3_response_tables.*`:
  `get_cnr3_table_value_for_signed_diff` (total, safe lookup; out-of-range → 0) and
  `build_cnr3_weight_table` (the cosine-curve weight table, `Cnr3Status`-returning). The weight-table
  formula (preserve its meaning if ever touched):
  ```text
  half_strength = strength / 2   // INTEGER division, deliberately BEFORE conversion to double
  narrow:  angle = abs(diff) * pi / threshold
  wide:    angle = abs(diff) * abs(diff) * pi / (threshold * threshold)
  value:   int( half_strength * (1.0 + cos(angle)) ), clamped to 0..sample_peak
  threshold == 0: only the centre entry = strength; clamp threshold/strength before generating
  ```
- **P.2A — config surface + geometry.** `Cnr3ResponsePlaneConfig{threshold_8bit, strength_8bit,
  curve}` × {Y,U,V}; helpers for 8-bit→native scaling, geometry, and a build-local-then-publish-on-
  full-success table builder (no partial publish on invalid config). Per-plane decomposition matches
  the old `build_cnr3_lookup_tables` exactly (`mode[0/1/2]`→Y/U/V; n=threshold, m=strength).
- **P.3A — weighted blend** (the load-bearing arithmetic phase). Scalar int64 blend
  `dst = (weight*prev + (shift-weight)*cur + shift1) >> shift2`, `shift2 = depth<<1`,
  `shift = 1LL<<shift2`, `shift1 = shift>>1`, `weight = y_response*chroma_response` (int64).
  VERIFIED BIT-EXACT against vsCnr2.cpp. int64 accumulator proven overflow-safe at 16-bit (weights
  exceed INT32_MAX); convex-combination boundary (`weight ≤ sample_peak² < shift`) proven at the
  max-weight boundary and documented in code; half-point round-half-up proven.
- **P.4A — downsampled-luma.** `(a+b+c+d+2)>>2` 2×2 box average + tap coords (`x0=cx<<subw`,
  `x1=x0+1`, `y0=cy<<subh`, `y1=y0+subh`); reproduces the 4:2:2/4:4:0/4:4:4 degenerate tap collapse.
  **DELIBERATE DIVERGENCE (edge clamp) — see Part 6A accuracy rule.**
- **P.5A — signed-difference bridge.** Composes P.1A+P.2A+P.3A+P.4A. **signed_diff = current −
  previous carried as signed `int` end-to-end into the total bounded lookup — no unsigned
  intermediate anywhere** (the signed/unsigned + wraparound concern, CLOSED and proven in both
  directions). Validates P.2A geometry before lookup; publishes on full success only.
- **P.6A — chroma-plane traversal.** Row-major over strided scalar `int` planes composing the P.5A
  kernel per sample. Stride-aware indexing `(y*stride)+x` with validation `stride ≥ width`
  (no row-spill) and `height ≤ INT_MAX/stride` (no index overflow); padding preserved; plane-level
  no-partial-publish (two-pass: compute-all-into-local, write-only-on-full-success).
**TWO DELIBERATE DIVERGENCES from vsCnr2, both Dave-confirmed and documented (do NOT "fix" either
back; do NOT flag as bugs):**
```text
1. P.2A GEOMETRY: table_offset = sample_peak, table_size = sample_peak*2+1 (255/511 @ 8-bit),
   vs the old source's sample_peak+1 / offset*2+1 (256/513). PROVEN lookup-equivalent across
   [-255,+255] (the old +1/+2 left two never-read slots). P.2A's is the forward convention —
   index with offset = sample_peak (NOT the old +1), or an off-by-one results.
2. P.4A EDGE CLAMP: edge taps are clamped (edge-replicated) rather than reading past the frame as
   vsCnr2 does. vsCnr2's edge read relies on AviSynth frame-padding slack and is not a deliberate
   algorithmic value, so clamping is safe and arguably more correct. Confined to the rightmost/
   bottom chroma edge strip; affects only the luma-difference feeding the chroma weight (luma
   output unchanged). This is the SECOND application of the accuracy rule (the first being the
   scaling). See Part 6A.
```
PROCESS NOTE (retained, extended): P.1A went patch → four-way → commit with the read-first review
done POST-COMMIT (defensible — low-risk pure maths, vectors pre-verified). **Every phase since
(P.2A–P.11C, and K.1A–K.1D) was reviewed read-first BEFORE apply**, with every reference vector
independently recomputed by the designer and (from P.3A) cross-checked against the fetched vsCnr2.cpp
source. The keystone made read-first decisive: at K.1D a patch that passed the four-way 47/47 was
nonetheless DROPPED on the diff review because it silently rewrote proven code (Example F). A post-hoc
review can only catch, not prevent, a bad apply to proven code — and a passing four-way is not proof
the proven behaviour was preserved.
### The downstream roadmap (UPDATED — keystone under way; each phase still propose→review)
```text
P.1A  response-table salvage + vector proof                       <-- DONE (committed)
P.2A  pixel-configuration parameter surface for response tables   <-- DONE (committed)
P.3A  weighted blend scalar/vector proof                          <-- DONE (committed)
P.4A  downsampled-luma helper proof (edge clamp divergence)       <-- DONE (committed)
P.5A  signed-difference / table-lookup / blend bridge proof       <-- DONE (committed)
P.6A  chroma-plane traversal vector proof                         <-- DONE (committed)
P.7A  source-luma downsample plane traversal proof                <-- DONE (committed, acb5080)
P.8A  native byte-plane access vector proof                       <-- DONE (committed, a6fd09c)
P.9A  native luma downsample bridge proof                         <-- DONE (committed, ab37443)
P.10A VapourSynth plane-view adapter proof                        <-- DONE (committed)
P.11A caller-supplied frame-triplet / plane-set validation proof  <-- DONE (committed)
P.11B caller-supplied real-frame pixel composition proof          <-- DONE (committed)
P.11C caller-supplied scene-change / reset proof                  <-- DONE (committed)
--- the KEYSTONE (cache<->pixel / getFrame) is UNDER WAY; decomposed K.1A-K.1G ---
K.1A-K.1G  keystone (request-plan, ownership, live passthrough, frame-0, branch-c/d,
           VS-LIFECYCLE, scene wiring)                            <-- DONE (keystone COMPLETE; all 4 branches live+proven)
P.11C.1-.5 live scene detection across branch-a/c/d + cut-checkpoint-as-anchor  <-- DONE (P.11C arc CLOSED)
STEP 0     joint CMS sensibility/gap review (hot-zone+prune wiring) <-- CLOSED (outputs -> CMS07.14 §7.4-§7.6)
--- the LIVE CACHE-PRESSURE WIRING arc (W.1-W.3); single-activation scope ---
W.1   §7.4 independent checkpoint-retention trigger (cache-core)   <-- DONE/READY (approved, applied, 54/54 GREEN)
W.2   hot-zone observation wiring @arInitial (§7.6; DLL-side)      <-- NEXT (lower risk; prerequisite for W.3)
W.3   combined live store-and-prune helper (§7.5 six-step order)   <-- OWED (wires §7.2+§7.4 into live path; temp KDT)
...   real-footage validation -> diagnostics (4-phase) -> fmParallel (concurrent wiring; FI-06/07/08)  <-- OWED
      ** The keystone connects the proven CACHE CORE (C.14A) to the proven PIXEL CHAIN (P.11C) inside
         VS getFrame scheduling. The pixel path is fully proven on caller-supplied frames, so the
         keystone reasons ONLY about frame SOURCING / lifecycle / ownership. See the DELTA for the
         full keystone delta and Part 6B for the live hunting list. **
--- post-keystone ---
NEXT  THRESHOLD DERIVATION for scene-change (deferred plugin-config arithmetic): reproduce vsCnr2's
        diff_max faithfully, match P.11C accumulation units (see Part 2 P.11C entry).
NEXT  fmParallel — a CORRECTNESS phase (exercises cache/recovery concurrency-correctness), AFTER the
        keystone wires single-threaded getFrame first. "Prove it works under fmParallel."
NEXT  TYPED-ROW-POINTER vs memcpy — deferred to a MEASURED fmParallel performance phase; any
        optimisation must be proven BIT-EXACT-OUTPUT identical to the memcpy path.
NEXT  post-K.1G KDT cleanup — remove the temporary KDT dev-trace (the K.1A plan-driven formatter AND
        the live frame-0 / scaffold formatters) plus the temporary guards, per diagnostics spec §2.8.
BUILD VS2026 project wiring; plugin registration; diagnostics cleanup / scaffold retirement.
```
### The authoritative document set (with versions, at this document's date)
```text
CMS07.14                      cnr3_cache_manager_design_v7_14*.md     — controlling DESIGN authority (additive over 07.13; §7.4 checkpoint trigger, §7.5 wiring contract, §7.6 arInitial obs; §0A charter; §9.5 floor)
Production Spec v2.11          CNR3_Handover_Pack_Production_Spec_v2_11.md — §3.2 context master; §3A register incl. charter §3A.5.0 (PDAP / R-PROCESS-20..23)
Diagnostics spec v1.5          cnr3_diagnostics_specification_v1_5.md   — §2.8 = temporary keystone KDT (removed post-K.1G)
Document A v3.10               Document_A_CNR3_Project_Context_and_Standing_Rules_v3_10.md — context + §3A register incl. charter §3A.5.0 (§3A authoritative per R-PACK-02)
Document B v3.9                Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_9.md  — current build state + work plan (top UPDATE block authoritative; through D.5)
THIS-CHAT DELTA               CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.md — slimmed delta, NEWEST state (phase-index + active-phase; companion to Document B)
Companion v7.13.4             CNR3_CMS_Future_Investigations_..._v7_13_4.md (FI-09 resolved into CMS07.14) — NON-NORMATIVE, NOT in coder pack (FI-01..08; lockstep with CMS)
Reviewer Intro v3.6           CNR3_Handover_Introduction_to_new_reviewer_chat_v3_7.md — concise entry point / role for the reviewer
This document v1.13           CNR3_Designer_Reviewer_Role_Handover_v1_13.md — the role/disposition handover
```
Document authority hierarchy: **CMS → Production Spec §3A → diagnostics spec → handover
pack.** If documents conflict, the higher authority wins; if any document conflicts with the
repository on build state, the repository wins.
Note on Document B's filename: its version label is kept at the "3.2" generation (patch
levels .1…​.9) deliberately, to stay aligned with Document A v3.2. Current is v3.2.9.
---
## PART 3 — THE OPERATING DISCIPLINES (RULES WITH RATIONALE)
These are the rules the review has run on. Each is stated as an imperative, paired with WHY,
because a rule without its reasoning gets misapplied. A new chat should internalise the
reasoning, not just the rule.

### D0. THE DESIGN ALIGNMENT AND ESCALATION CHARTER (the governing discipline)
This is the standing three-way governance model. It governs how the designer, coder, and
coordinator treat the CMS, escalate problems, and cross-check each other. It sits above the
individual disciplines below because it defines WHEN to stop and raise rather than proceed.

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

**D1. Stop-review-approve before any code.** Every phase is proposed as text and reviewed
before a patch is generated. *Why:* design errors caught in text cost a message; design
errors caught in committed code cost a revert and a re-prove, and risk disturbing proven
code. The cheapest place to fix a design is before it is built.
**D2. Read-first for load-bearing patches.** For anything touching proven code, atomics,
locks, or critical invariants, YOU read the actual patch diff before Dave applies it. *Why:*
a subtle error in an atomic does not fail loudly — it produces a race, a leak, or a
use-after-free that surfaces far from the cause. Reviewing the diff in isolation is the only
way to confirm the critical section is undisturbed. (At the keystone this caught a dropped
patch that passed the four-way but silently rewrote proven code — Example F, D16.)
**D3. A test that can only pass is not a proof.** Every test must have a genuine failure mode:
the scenario must make a WRONG implementation produce a DIFFERENT, DETECTABLE result. Expected
values must be explicit and, where countable, `static_assert`ed. *Why:* a test that passes
regardless of correctness proves nothing. The question is never "does it pass?" but "would it
fail if the code were wrong?" If you cannot describe the wrong implementation it would catch,
the test is theatre.
**D4. Independently verify the coder's numbers.** For any numerical or reference proof,
recompute the coder's expected values yourself (e.g. in Python against the spec/source
formula) BEFORE approving. *Why:* for a reference-vector proof the expected values ARE the
proof. If you trust the coder's arithmetic and it is wrong, you bake a wrong "truth" into the
test, and the test then validates wrong code forever. This has been done every time numbers
appear (the response-table vectors were fully recomputed). Do not skip it because the numbers
"look right".
**D5. Prove only reachable states — but defensive guards may be tested with crafted input.**
Do not build machinery for, or write tests that assert, states that cannot occur (that is how
the AS3 mistake nearly happened). HOWEVER, a hand-constructed malformed input is LEGITIMATE
for testing a defensive GUARD (as with C.13B): there the crafted input proves the *tripwire
fires*, which is normal defensive-code testing, not proving an unreachable production path.
*Why:* effort spent handling impossible states is waste and adds risk; but a guard that
refuses an impossible state IS valuable, and you test a guard by feeding it the thing it
guards against. The distinction is: are you proving a *production path* (must be reachable) or
proving a *guard rejects corruption* (crafted input is the right test)?
**D6. Defer features that serve unreachable states; build tripwires that guard load-bearing
invariants.** These are different. A *feature* serving a state that cannot occur → defer it
(AS3). An *assertion/guard* protecting an invariant that a future change could silently break
→ build it (C.13B contiguity guard). *Why:* the first is premature machinery; the second is
defensive engineering. The tell that distinguishes them: does it *handle* the impossible
state (feature, defer) or *detect and refuse* it (guard, build)?
**D7. Push back hardest on the load-bearing part — especially if the proposal is vaguest
there.** When a proposal is detailed on the easy parts and hand-wavy on the part that matters
most, that is exactly where to demand specifics. *Why:* the load-bearing element is where the
phase's value and risk concentrate. A proposal that is precise about scaffolding and vague
about the actual proof (e.g. "exercise equivalence where relevant" for the R-PROCESS-19
capstone) must be sent back for the mechanism, not approved on the strength of its tidy parts.
**D8. Check conclusions against the actual spec/source text — do not pattern-match from
memory.** When the coder (or you) states a conclusion about what the spec says or what the
code does, read the actual text/diff before accepting it. *Why:* memory and inference drift;
the spec is precise. Several real issues were caught only because the actual text was read
rather than assumed. Dave explicitly values this and will push back on conclusions stated
without reading the source. (Lifecycle-contract questions specifically must be answered from
DOCUMENTATION, not from "it worked in testing" — undocumented-but-works is version-fragile and
dangerous under fmParallel; this is how the arInitial-return contract was settled at K.1E.)
**D9. Report ACTUAL output, never assumed output.** The authoritative evidence is Dave's local
VS2026 four-way (or five-way) run, pasted verbatim. The coder's sandbox build is NOT
authoritative. *Why:* "it should pass" is not evidence. The sandbox and the authoritative
build can differ; only the real run on the real toolchain counts. Commit messages' Verified
blocks must record the real run.
**D10. Salvage is per-case-approved, and some logic is permanently prohibited.** Pulling from
the old `superseded_by_v7/*.txt` files requires naming the exact file and routines and getting
per-case approval (R-ARCH-07). The CNR2-style predecessor/recovery/fallback logic (e.g.
substitute-source[n-1]-when-output-absent) is NEVER salvaged (R-ARCH-06) — CMS07 recovery
replaces it. CNR2/vscnr2 is a PIXEL-MATHS reference only. *Why:* the old code carries retired
assumptions; importing them silently reintroduces the very problems CMS07 was designed to fix.
The quarantined files (old cache managers) are not even opened for ideas.
**D11. The CMS is authority over old source; never reverse-engineer design intent from
quarantined/old code.** The instance/lifecycle model, the recovery model, the AS register —
all live in the CMS (the instance model specifically in CMS §3.5, §3, §4.6, §13, verified
against the local R76 VapourSynth4.h). Old source is at most a REFERENCE for registration
*shape* when integrating, never the authority on the model. *Why:* reverse-engineering intent
from old source re-imports retired assumptions. If a new chat (or the coder) starts reading
old source to understand "how instances work", redirect it to the CMS.
**D12. Count discipline.** A behaviour-adding phase adds exactly one selftest (+1 to the
count); an audit/comment-only phase changes nothing. The four-way forced-fail run must show
exactly one induced failure. *Why:* the count is a cheap integrity check that the phase did
what it claimed and nothing crept in. (Live-getFrame keystone phases like K.1C/K.1D are
plugin-only and add NO selftest — they are proven by the coordinator A/B harness instead, so
the count legitimately stays put across them.)
**D13. Diagnostics are observe-only; changing a D-SUM compute gate triggers the macro-off
proof (R-PROCESS-19).** Behavioural assertions must NEVER read D-SUM counters. Any phase that
introduces/changes a D-SUM compute gate must prove that with the gate's compute macro disabled,
all non-D-SUM behaviour is unchanged. *Why:* diagnostics that alter behaviour are not
diagnostics — they are hidden control flow. The proof that they are truly observe-only is the
macro-off run showing identical behaviour. (The single live gate is
`CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE`; it is `#if defined(...)`-based — see Part 6 for the toggle
method. SEPARATE from this is the temporary keystone KDT dev-trace under `CNR3_KEYSTONE_DEV_TRACE`,
which is getFrame-only/stderr-only and removed post-K.1G.)
**D14. Documents are standalone, self-contained, and versioned; volatile docs stay truthful
about current state.** Document B tracks current state and must not race ahead of the repo
(update it AFTER a phase commits, not before). The CMS is additive where possible (changelog
every version). *Why:* a document that claims a phase is done before it is committed is a trap
for the next reader. Truthfulness about *current* state is the whole job of the volatile docs.
**D15. When formatting output for Dave to paste to the coder (email/chat), use plain text —
no markdown, no special formatting.** *Why:* Dave's stated preference; it pastes cleanly into
the coder chat and email. (This document, by contrast, is a repo markdown file like its
siblings — the plain-text rule is for relayed messages, not repo docs.)
**D16. Proven code stays proven — never modified, behaviour OR internals, without explicit
visible planning and designer approval IN ADVANCE.** Once a function is proven by a committed
selftest, its behaviour AND its internals are frozen unless a phase explicitly proposes changing
it and the designer approves that change before any code is written. A passing four-way after the
internals were swapped is NOT proof of equivalence — it proves only that the selftest did not
DETECT a difference, which is not the same as there being none (the selftest may not exercise the
changed path). If reuse of a proven operation appears to require touching it, that requirement is
itself a DESIGN QUESTION to raise, not a licence to modify — raising the blocker is always the
correct move; routing around it (a parallel re-implementation, hand-set flags that MIMIC the
proven path's output without going through it, a silent internal swap, an undisclosed scope
broadening into a proven file) is never. When a change endangers proven code the response is
WITHDRAW-and-reconsider, not patch-and-fix. *Why:* the entire edifice rests on
proven-things-staying-proven; a silent change to a proven path is a process breach before it is a
code defect, because it removes the guarantee the proof gave without anyone deciding to. This is
the chief lesson of the keystone's opening (Example F), and it is the bar to watch hardest on any
phase that "reuses" a proven pixel/cache operation.
---
## PART 4 — THE DISPOSITION: THE TRIGGERS (WHEN TO BE CAREFUL)
This is the hardest thing to transfer and the most important. The capability to be careful is
not the issue — a new chat has it. The issue is knowing WHEN it matters *here*. The disposition
is, in essence, a set of triggers that should make you slow down and engage deeply instead of
pattern-matching to "looks fine, approve". Install these triggers. When one fires, STOP and do
the careful thing.
```text
TRIGGER                                          ->  THE CAREFUL THING TO DO
-------------------------------------------------    ----------------------------------------
"This change touches a proven atomic / lock /        Isolate it. Read-first the diff against
 critical section."                                  exactly that change. Confirm the critical
                                                     section is pure/bounded and undisturbed.
                                                     Consider splitting it into its OWN small
                                                     phase for focused review (this is what
                                                     was done for C.13B). Everything touching
                                                     an atomic is precious.
"A phase 'reuses' a proven pixel/cache operation,    STOP. That is a DESIGN QUESTION — raise it
 and that reuse appears to require touching or       and get approval BEFORE any code. Do not
 modifying the proven operation's internals."        re-implement it in parallel, hand-set flags
                                                     to MIMIC its output, broaden scope into the
                                                     proven file undisclosed, or swap its
                                                     internals silently. A passing four-way after
                                                     an internals swap is NOT proof of
                                                     equivalence. Withdraw-and-reconsider, do not
                                                     patch-and-fix. (D16, Example F.)
"The coder gave me expected numbers / values."       Recompute them yourself before approving.
                                                     For a reference proof the numbers ARE the
                                                     proof. (D4.)
"The most important element of this proposal is       Push back. Demand the mechanism for the
 the least detailed part."                           load-bearing element specifically. Do not
                                                     be reassured by the tidy easy parts. (D7.)
"This feature/machinery handles a case I should       Ask: can that case actually occur under
 check can actually happen."                         the current design? If not, defer the
                                                     feature (AS3 lesson). But if it is a guard
                                                     that REFUSES the impossible case, build it
                                                     (C.13B). (D5, D6.)
"A conclusion was stated about what the spec/code     Read the actual text/diff. Do not accept
 says — or about a VS lifecycle/API contract."       the conclusion from memory or inference;
                                                     lifecycle/API contracts must come from
                                                     DOCUMENTATION, not from 'it worked in
                                                     testing'. (D8.)
"A test is proposed."                                Ask: would this fail if the implementation
                                                     were wrong? Describe the wrong impl it
                                                     catches. If you cannot, the test is weak.
                                                     (D3.)
"Results are described as expected / should pass."    Require the ACTUAL pasted run. (D9.)
"The coder reaches into old/quarantined source for    Redirect to the CMS. The CMS is the
 understanding."                                     authority; old source re-imports retired
                                                     assumptions. (D10, D11.)
"A diagnostics gate / D-SUM counter is touched."      Trigger the macro-off observe-only proof.
                                                     Confirm no behavioural assertion reads a
                                                     D-SUM counter. (D13.)
"Dave expresses unease, even if he can't fully        Take it seriously and act on it. His
 articulate it logically."                           instincts have repeatedly caught real
                                                     risk (incl. the dropped K.1D patch). Unease
                                                     about precious code is correctly-priced
                                                     risk, not a feeling to soothe. (Part 8.)
"A destructive command (git restore, scripted edit,   Prefer the smallest, safest, most
 cmd-line operation) is proposed for a routine task." reversible manual action. Dave once lost
                                                     a source tree to a stale-clipboard paste in
                                                     a cmd window. For the macro toggle, manual
                                                     comment-out beats scripted git restore.
```
A way to hold the disposition in one sentence: **be relaxed about the easy, routine, reachable,
well-evidenced parts, and become slow and exacting at exactly five moments — when proven code is
touched (or a "reuse" would touch it), when a number or claim must be true, when the load-bearing
part is vague, when a state's reachability is in question, and when a lifecycle/API contract is
asserted from testing rather than documentation.** The skill is spending your care budget at those
moments rather than uniformly. A new chat that is "underdone" typically fails by NOT slowing down at
these triggers — by approving the load-bearing part on vibes, trusting the coder's numbers,
rubber-stamping an atomic change buried in a big diff, or letting a "reuse" silently rewrite a proven
function.
---
## PART 5 — WORKED EXAMPLES (THE DISPOSITION IN ACTION)
These are real decisions from the project. They teach the pattern better than rules.
### Example A — The AS3 deferral (defer a feature serving an unreachable state)
The coder was about to implement AS3 (a recovery atomic for "reused intermediate frames"). It
caught — correctly — that under the proven nearest-present-start-point + contiguous-hole
planner, no AS3-positive reused-intermediate state is reachable: a present frame between the
anchor and the requested frame would have *become* the anchor; an absent one is a planned hole
consumed by AS2. The concurrent "planned hole became present before AS2" case is already
handled by AS2 first-in-best-dressed duplicate/adopt. **Decision: defer AS3** — it is a feature
serving an unreachable state. It was reserved for a future sparse-plan revision, and the
reasoning was written into CMS §9.6 so it would not be lost. *Lesson:* do not build machinery
for states that cannot occur (D5/D6). The tell was that AS3 *handled* a case rather than
*guarding* against one.
### Example B — The C.13B contiguity guard (build a tripwire; split it out because it touches an atomic)
CMS §9.6's contiguity invariant lived only in spec prose and the planner's by-construction
behaviour. A future maintainer changing the planner (e.g. for the deferred sparse-plan work)
could silently break the contiguity that downstream recovery consumers depend on. Dave's
instinct — "everything that touches an atomic is precious" — drove two decisions: (1) BUILD a
production hard-status guard that makes the invariant self-enforcing (distinct from AS3: this
*detects and refuses* an impossible state rather than *handling* it — so it is a legitimate
guard, not premature machinery); and (2) SPLIT it into its own small phase (C.13B) BEFORE the
big C.14A aggregate, so the change to the proven planner atomic got an isolated read-first
review instead of being buried in a large diff. The read-first review confirmed the in-atomic
check was pure bounded arithmetic over an already-built vector (no slow work, critical section
undisturbed) and that every planner success-return routed through the validator (no bare-ok
gap). *Lesson:* guards protecting load-bearing invariants are worth building (D6); changes to
atomics get isolated focused review (D2); Dave's unease was correctly-priced risk (Part 8).
### Example C — Response-table vector verification (recompute the numbers)
For P.1A the coder supplied expected table values (254, 127, 216, 145, 192, ...). Rather than
trust them, the designer recomputed every vector in Python against the salvaged formula —
including the non-obvious cosine-curve values and the deliberate `strength/2` integer-division
quirk that yields peak 254 (not 255). All were correct, including a requested second
strength/threshold family (200/20) added specifically so the test would not prove only the
255/10 point. *Lesson:* for a reference proof the expected values ARE the proof; verify them
independently (D4). The verification also made the test powerful: because the expected numbers
are known-correct, a transcription error in the implementation will be caught.
### Example D — The R-PROCESS-19 macro-off equivalence at C.14A (the load-bearing capstone, and the safe toggle)
C.14A's reason for being the capstone was proving the D-SUM-11 compute gate is observe-only
under combined load. The coder's first proposal was vague here ("exercise equivalence where
relevant"). The designer pushed back (D7) and demanded the mechanism: the SAME aggregate test
run with `CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE` defined AND undefined, with behavioural assertions
that never read D-SUM counters, proving identical non-D-SUM outcomes. Crucially, the coder
recognised on its own that if any behavioural assertion *had* to be conditional on the macro,
that would prove diagnostics affect behaviour — the opposite of the goal — and made that a
stop condition. The toggle method was decided deliberately: a MANUAL comment-out of the single
`#define` line (the gate is `#if defined(...)`-based, so changing `1`→`0` would NOT disable it
— it would still be defined), NOT a scripted `git restore` (Dave's stale-clipboard incident),
reverted by manually un-commenting, with the revert confirmed by the file's diff showing only
the legitimate marker bump. *Lesson:* push on the vague load-bearing part (D7); diagnostics are
observe-only and the proof is the macro-off run (D13); prefer the safest manual action for
routine toggles (Part 4 trigger).
### Example E — The browser-tab / role-confusion recovery (role discipline)
During the AS3 discussion, designer-level detection/error-handling reasoning was once
accidentally relayed to the coder. The coder itself flagged the role confusion, and nothing was
committed wrongly. *Lesson:* the three-party roles matter; the coder is a capable colleague that
will catch coordination errors; and the recovery is to acknowledge cleanly and continue, not to
panic. Keep the roles straight (Part 1).
### Example F — The K.1D reorientation (proven code stays proven; withdraw rather than patch around)
At the keystone's first real-output phase (K.1D, live frame-0 store/return), the coder's first
patch had a CORRECT N>0 fence and CORRECT ownership plumbing — but the diff review (D2) found it
(1) silently REWROTE the body of the proven, selftested P.11C reset function to route through a
new helper; (2) introduced a SECOND source-to-output copy orchestration, hand-setting the
reset-summary flags to MIMIC the reset path without BEING it; and (3) broadened scope undisclosed
into the proven `cnr3_frame_processing.cpp` (+351 lines) against a "report before broadening"
commitment. **The four-way still passed 47/47** — which is exactly the trap: a green four-way after
swapping internals is not proof the proven behaviour was preserved (the P.11C selftest may simply
not exercise the changed path). Dave's instinct fired first ("a pause to discuss is necessary, for
safety reasons"), and the standard was sharpened into D16. The patch was **WITHDRAWN to the
proposal stage** — not patched-and-fixed — because withdrawal is the right response to a
proven-code breach (a fix-list would have implicitly accepted the unsafe approach). The
reorientation then produced a BETTER design: a step-back asked whether frame-0 needs a "copy
operation" at all, and since fresh-start output[0] = source[0] byte-for-byte, VapourSynth's own
`copyFrame` primitive sufficed — touching NO proven code, collapsing scope back to two files, and
removing the second-implementation risk entirely. The copyFrame patch was then verified read-first
against five ownership/fence bars and accepted (47/47 + harness green). *Lesson:* when a change
endangers proven code, the move is STOP-and-reconsider, not fix-the-patch (D16); the safe design is
often found by stepping back to question the premise (does this need to touch the proven thing at
all?), not by making the dangerous approach safer; a passing four-way is necessary but not
sufficient; and Dave's unease was, again, correctly-priced risk (Part 8).
---
## PART 6 — PROJECT-SPECIFIC TRAPS AND INVARIANTS
**The lock / atomic invariants (inviolable):** one non-recursive mutex, RAII-only;
decide-inside-the-lock / execute-outside; `freeFrame` is NEVER called inside the lock; pin-and-
record is indivisible with a pre-lock reservation; a checkpoint is a flag, not a pin; hot zones
are hints, not liveness guarantees (pins guarantee the in-flight set). The AS1–AS7 atomic-scope
register is inviolable; do not change atomic-scope boundaries without explicit approval.
**Salvage governance (now ACTIVE — the pixel arc is the first live salvage):**
- HIGH-VALUE (study/adapt, per-case approval): `cnr3_frame_internal_processing.cpp/.h.txt`
  (the pixel core — has `process_cnr3_frame_with_explicit_previous_output()`, the
  explicit-predecessor boundary matching the CMS; CAUTION: do NOT carry its CNR2-style fallback
  `process_cnr3_frame()` predecessor logic), `cnr3_response_tables.cpp/.h.txt` (P.1A target),
  `cnr3_memory_diagnostics.cpp/.h.txt` (re-point at CMS07 counts).
- REFERENCE-when-integrating only: `vapoursynth-Cnr3.cpp.txt` (old VS registration SHAPE), the
  registration/call-trees doc.
- QUARANTINE (do not open for ideas): the old cache managers, `old_cnr3_strict_cache`, the old
  build config.
- NEVER salvage CNR2 recovery/predecessor/fallback logic (R-ARCH-06).
**The instance / lifecycle model is in the CMS, not old source (D11):** per-instance cache,
per-invocation `frameData`, destruction-ordering (discharge pins on frameData destruction),
leak-safety — all in CMS §3.5, §3, §4.6, and §13 (V4 RESOLVED, verified against the local R76
VapourSynth4.h: `void **frameData` is the sanctioned carry; leave the node at `cmAuto`, do not
layer the core cache over our own outputs). This is NOW LIVE at the keystone (K.1C onward). KEY
KEYSTONE LIFECYCLE FACTS established by documentation at K.1D/K.1E: `arInitial` requests inputs and
returns NULL; a frame is returned only at `arAllFramesReady` (the source-filter exception does NOT
apply to CNR3 — it is a dependency filter with an input node); `copyFrame(src, core)` is a bitwise,
writable, caller-owned duplicate (the right primitive for verbatim frame-0); for N>0 / error-frame
checks, vspipe leaves a header-only y4m (no `FRAME` marker) = clean refusal.
**The diagnostics compute gate and its toggle (D13):** the only live D-SUM compute gate is
`CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE` in `cnr3_build_config.h`. It is `#if defined(...)`-based.
To prove observe-only-ness, DISABLE it by MANUAL comment-out of the single
`#define CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE 1` line (NOT a value change to 0 — still defined;
NOT a scripted git restore — destructive-command risk), rebuild Release, run, then manually
un-comment. Confirm the revert leaves only the legitimate marker bump in the file's diff. The
committed `build_config.h` must always have the macro DEFINED (production default). SEPARATE: the
temporary keystone KDT dev-trace (`CNR3_KEYSTONE_DEV_TRACE`, `[KDT]`/`[KDT-SUMMARY]`) is
getFrame-only / stderr-only / emitted ONLY inside getFrame (never at load/registration) and is
removed post-K.1G per diagnostics spec §2.8. There is ALSO a temporary live-getFrame guard
(`CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF`, renamed from the K.1C scaffold guard) — an OPEN
audit item is confirming the K.1C scaffold (old name `CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD` +
`SCAFFOLD_NOT_FILTERED`) is fully removed from the committed tree (DELTA §7).
**The Category-A vs Category-B recovery distinction (CMS §9.6):** Category A (a planned hole
became present before this activation's AS2 store) is EXPECTED fmParallel-class concurrency,
handled by AS2 duplicate/adopt — not an error, no user-visible alert, telemetry at most.
Category B (a non-contiguous reused-intermediate shape under the contiguous-hole planner) is
IMPOSSIBLE / design-drift — detected and refused by hard status (the C.13B guard), never
silently accepted. The user-visible developer-alert for Category B is FUTURE work (the
emission half of what C.13B detects) — a bounded, one-shot, stderr-only, outside-locks,
reproduction-useful report, reserved for Category B, never for Category A; exact fields deferred
to implementation.
**The open future investigations (companion v7.8, non-normative):** FI-01 (FORWARD_RADIUS
tuning for higher thread counts — efficiency only, correctness never affected). FI-02
(sparse-plan / recompute-avoidance recovery — the deferred AS3 work; when undertaken, the C.13B
guard MUST be revised/relaxed as an explicit reviewed part of that work, because it will
correctly reject the very non-contiguous plans the sparse revision intends to produce). **FI-04
(RESOLVED at K.1E):** the source dependency declaration moves from `rpStrictSpatial` to
`rpGeneral` once the filter is recursive — `rpStrictSpatial` stops being truthful when recovery
requests bounded source ranges; `rpGeneral` is conservative-correct; `fmUnordered` is unchanged;
`requestPattern` is a separate layer from `filterMode` and does not affect the CMS7 cache.
---
## PART 6A — PIXEL-LAYER REFERENCE (CNR2 SOURCE-CONFIRMED)
This section records the pixel-layer facts that were established by reading the **actual CNR2
source** (the higher-bit-depth-modified AviSynth variant Dave based CNR3 on) plus the
prior-integration `vapoursynth-Cnr3.cpp`, so a new chat does NOT re-derive them from memory or
general knowledge. These are the things the pixel arc (P.3A–P.6A) must implement and stay
aligned with. **CMS07 §13 V8.1 / §3 is the design authority; this is the reviewer's
working reference and the SOURCE of why V8.1 says what it says.** Where this and the CMS differ,
the CMS wins — but they agree.
### Accepted formats (NOT negotiable; CMS §1 line 69, §13 V8.1 line ~1105)
```text
- Integer YUV ONLY. No float. No RGB.
- Planar subsampling: 4:2:0, 4:2:2, 4:4:0, 4:4:4 ONLY  (CNR2 restricts to subSamplingW/H <= 1).
- Exactly THREE planes. Grey / 4:0:0 is NOT in scope (absent from the accepted set; do not
  add one-plane handling without an explicit design decision).
- Native bit depth, 8..16-bit, as a RANGE (8/10/12/14/16 all in scope) — NOT an enumerated
  whitelist. CNR2 is templated on T = uint8_t / uint16_t.
```
### Bit-depth handling — the answer to the "unsigned / overflow" question (CNR2 source)
This is the single most important pixel-layer fact, and it is the resolution of the
signed/unsigned + overflow concern. CNR2 computes in **native pixel bit depth — it does NOT
promote/demote pixels to a working format** — and manages overflow by widening only the
**arithmetic accumulator**:
```text
The weighted blend (per chroma sample), from CNR2 source:
    dst = (weight * prev + (shift - weight) * cur + shift1) >> shift2
where:
    weight  is int64_t   (it is a PRODUCT of two response-table entries — can be large)
    shift2  = depth << 1        (i.e. 2 * depth;  e.g. 32 for 16-bit)
    shift   = 1LL << shift2      (the full-weight normaliser)
    shift1  = the rounding addend (half of shift; rounds the >> back to pixel range)
    prev    = previous FILTERED output chroma (output[N-1]), NOT source[N-1]
    cur     = current source chroma (source[N])
=> native pixels in, int64 ACCUMULATOR for the arithmetic, shift back down by 2*depth.
   This int64 accumulator + shift2=depth<<1 IS the "Fixed high bit depth chroma overflow"
   changelog fix: at high depth weight*pixel overflows 32-bit, so the accumulator was widened
   to 64-bit and the shift scaled by depth.
```
So the bit-depth answer for P.3A is: **native subsampling, native pixel depth, int64
accumulator for the blend, shift scaled by depth (`shift2 = 2*depth`), rounding via `shift1`** —
NOT pixel promotion. The signed difference used to index the response tables (`current -
previous`) is and must remain **signed**.
### 8-bit→native parameter scaling (from `vapoursynth-Cnr3.cpp`, P.2A-verified)
```text
scaled = (int64(clamp(value_8bit, 0, 255)) * sample_peak + 127) / 255   // round-to-nearest
sample_peak = (1 << bits_per_sample) - 1
```
Public threshold/strength parameters use the historical 8-bit Cnr2/vscnr2 scale; internally
they are scaled to native sample_peak by the above. P.2A implements this EXACTLY (verified
against `scale_8bit_parameter_to_bit_depth()`). **The vsCnr2.cpp source cross-check is now DONE
(read at P.3A prep):** vsCnr2 scales with an integer factor `value *= peak / 255`, which is
identical to P.2A at 8-bit and 16-bit but TRUNCATED at 10/12/14-bit (e.g. `1023/255 = 4`, not
4.0117). The decision is settled — P.2A's round-to-nearest scaling is KEPT as the more accurate
method (vsCnr2's integer-factor scaling is a truncation artefact, not a design choice); CNR3 is
identical to vsCnr2 at 8/16-bit and deliberately more accurate at 10/12/14-bit. See "The accuracy
rule" subsection below for the full reasoning and the governing principle.
### The luma calculation — `downSampleLuma` (CNR2 source)
The "fancy luma calc": **luma Y is copied UNCHANGED** to output (CNR3 does not filter luma).
The luma signal is used only as an INPUT to the *chroma* blend decision:
```text
downSampleLuma: a 2x2 BOX AVERAGE of luma onto the chroma grid, shifted by subSamplingW/H,
                so luma can be compared at chroma sampling positions.
                In 4:4:4 / 4:4:0 (subSamplingW==0 and/or subSamplingH==0) the 2x2 average
                DEGENERATES (e.g. y1 = y0 + 0 = y0 -> the four-tap average becomes
                2*row0[x0] + 2*row0[x1]); CNR2 does this deliberately — confirm any CNR3
                implementation matches CNR2's degenerate behaviour at these subsamplings.
The chroma blend weight depends on BOTH the luma difference AND the chroma difference:
    weight = response_y(diff_downsampled_luma) * response_chroma(diff_chroma)
So the "Y response table" is the LUMA-DIFFERENCE response feeding the CHROMA blend — it is
NOT a luma filter and must never be used to modify output luma. (P.2A documents this; P.3A
must wire it as the luma-response contribution to the chroma weight, not as a luma output.)
```
### Response tables and the per-plane decomposition (P.1A/P.2A, CNR2-consistent)
```text
mode is a 3-char string: mode[0]->Y, mode[1]->U, mode[2]->V curve.  'x' = narrow, 'o' = wide.
  (default "oxx" = Y wide, U narrow, V narrow). 'x' does NOT mean off; 'o' does NOT mean on.
Each plane gets ONE table from (threshold, strength) = (ln,lm)/(un,um)/(vn,vm) + curve.
  This is exactly P.2A's Cnr3ResponsePlaneConfig{threshold_8bit, strength_8bit, curve} x {Y,U,V}.
Raised-cosine weighting; the strength/2 INTEGER division before the cosine is deliberate
  (gives peak 254 not 255 for odd strength 255) — preserve it at all bit depths.
Signed-difference index centring: vsCnr2 source uses range_max = 1<<depth (the old +1-style
  offset), tables sized (1<<depth)*2+1 = 513/2049/8193/32769/131073 — CONFIRMED from source;
  P.2A uses offset = sample_peak (proven lookup-equivalent). Use the P.2A convention forward.
vsCnr2 indexes table[diff + range_max] with NO bounds check (relies on that sizing); CNR3's
  P.1A lookup is TOTAL (out-of-range -> 0), so CNR3 is safer — keep the total lookup.
```
### Scene change (CNR2 source; relevant at P.6A)
```text
diff_total accumulates abs(diff_y << (subSamplingW+subSamplingH)) (+ chroma diffs if scenechroma).
If diff_total > diff_max (a threshold scaled by scdthr and resolution), processChroma returns -1
and GetFrame returns CUR UNCHANGED — i.e. on a detected cut, output = source, no recursive blend.
Scene detection runs DURING the chroma compute (it accumulates while processing, bails at -1),
so in CNR3 it is intrinsic to the compute step, not a separate pre-pass.
```
### Why the cache exists (the root rationale; do not lose this)
```text
CNR2, when asked for frame n after last processing a frame != n-1 (non-sequential), CANNOT
recover the previous OUTPUT, so it falls back to fetching SOURCE[n-1] and uses that as the
predecessor — an APPROXIMATION it gets away with because it is MT_SERIALIZED and the blend
forgives small error. CNR3's entire cache + recovery architecture is the principled REPLACEMENT
for that approximation: where CNR2 says "non-sequential -> use source[n-1]", CNR3 recovers the
EXACT output[n-1] (cache hit, or rebuild via the bounded descending-search / fill-holes walk).
This is R-ARCH-06's root: never salvage CNR2's predecessor/recovery logic — only its pixel maths.
```
### The accuracy rule (governing principle for "should we round better here?")
A scaling-policy discrepancy surfaced at the P.3A source cross-check and was settled into a
durable rule. vsCnr2 scales 8-bit parameters to native depth with an integer factor
(`value *= peak / 255`): exact at 8-bit and 16-bit, but **truncated at 10/12/14-bit** (e.g.
`1023/255 = 4`, not 4.0117). P.2A instead uses round-to-nearest `(value*peak + 127)/255`,
which is identical to vsCnr2 at 8/16-bit and **more accurate** at 10/12/14-bit. Dave confirmed
the intent: CNR3 is a faithful, correct redevelopment of the algorithm, not a byte-identical
clone of vsCnr2's parameter arithmetic — use the most accurate method; compute cost is not a
constraint. So P.2A's scaling is kept (vsCnr2's integer-factor scaling is a truncation artefact,
not a design choice), and matching vsCnr2 byte-for-byte at 10/12/14-bit was rejected because it
would mean reproducing a bug to be less accurate.
```text
THE RULE: Accuracy upgrades are permitted ONLY where vsCnr2 is ACCIDENTALLY LOSSY;
          NEVER where its integer arithmetic is DEFINITIONAL.
CLASSIFY (the non-ambiguous test):
  ACCIDENTALLY LOSSY (upgrade allowed) = exact at the depths the author tested (8 and
    16-bit) but diverges at the others (10/12/14-bit) against evident intent — a
    depth-dependent artefact. The ONLY known instance is the peak/255 parameter scaling,
    which P.2A already upgraded. No others are known.
  DEFINITIONAL (reproduce BIT-EXACT, no upgrade) = the algorithm's specified
    quantisation/rounding, applied as the SAME operation at every bit depth; changing it
    would alter output even at 8-bit. Includes: the strength/2 integer division in the
    curve (peak 254 not 255 for odd strength); the int(...) truncation of the cosine; the
    blend's >> shift2 with shift1 = shift>>1 rounding (already correct round-to-nearest
    fixed-point); the downSampleLuma (... + 2) >> 2 rounding.
  WHEN IN DOUBT: treat as DEFINITIONAL, reproduce bit-exact, and ASK. The risk is
    asymmetric — upgrading a genuine artefact helps; "upgrading" a definitional step
    silently breaks algorithm compatibility, including at 8-bit where nothing was wrong.
COROLLARY (the "bit-exact CNR2" claim has THREE layers, state them separately):
  (a) blend arithmetic            -> bit-exact (shift2=depth<<1, shift=1LL<<shift2,
                                     shift1=shift>>1, the exact accumulate-then-shift)
  (b) response-curve construction -> bit-exact (m/2 integer, narrow vs wide cos forms)
  (c) parameter pre-scaling       -> deliberately divergent at 10/12/14-bit (P.2A
                                     round-to-nearest; identical to CNR2 at 8/16-bit)
  So claim: "bit-exact CNR2 blend arithmetic and response-curve construction; proportional
  parameter scaling matching CNR2 at 8/16-bit and correcting its integer truncation at
  10/12/14-bit" — NOT unqualified "bit-exact CNR2 at every step".
```
Plain English for a reviewer: do not go hunting for places to be "more accurate." The
parameter scaling is the only place vsCnr2 is *accidentally lossy*, and P.2A already fixed it;
everything arithmetic is the algorithm — reproduce it exactly. There is ONE further deliberate
divergence, but it is a SAFETY/boundary divergence (not an accuracy upgrade), recorded next.
### Recorded deliberate divergences from vsCnr2 (do NOT "fix" back; do NOT flag as bugs)
Two places where CNR3 deliberately and consciously differs from vsCnr2, both Dave-confirmed:
```text
1. PARAMETER SCALING (accuracy upgrade, the rule above): P.2A uses round-to-nearest
   (value*sample_peak + 127)/255; vsCnr2 uses integer-factor value*=peak/255 (truncates at
   10/12/14-bit). Identical at 8/16-bit; P.2A is more accurate at the intermediate depths.
2. P.4A DOWNSAMPLE-LUMA EDGE CLAMP (safety/boundary divergence, NOT an accuracy upgrade):
   vsCnr2 reads temp+1 horizontally and the second row unconditionally, running one sample past
   the row/frame at the right/bottom edges — it relies on AviSynth frame-padding slack for those
   reads. CNR3 instead CLAMPS the edge taps: x1 = min(x0+1, width-1), y1 = min(y0+subh, height-1)
   (edge replication). Rationale: vsCnr2's off-edge value is determined by padding, not a
   deliberate algorithmic value, so reproducing it would be matching an artefact, and clamping is
   both safe (no reliance on padding that VapourSynth may not guarantee identically) and arguably
   more correct. The divergence is confined to the rightmost chroma column / bottom chroma row,
   and affects only the downsampled-luma value feeding the chroma blend weight — luma output is
   unchanged. Worst-case observable effect: a faint difference on a 1-2 sample edge strip.
   A new chat must NOT "restore" the unclamped read to "match vsCnr2," and must NOT flag the
   clamp as a compatibility bug — it is intended and documented.
```
Both are confined, both preserve the interior bit-exactly, and both are consistent with CNR3's
goal of faithful *correct* redevelopment rather than byte-identical cloning of vsCnr2's artefacts.
---
## PART 6B — THE PIXEL-ARC REVIEW CHECKLIST (P.3A–P.11C done; the getFrame/cache KEYSTONE is live, K.1A–K.1D done)
When the blend, downsampled-luma, frame-processing core, and VS integration land, these are the
specific subtle bugs to hunt. This list previously lived only in chat memory; it is the reviewer's hunting map. The scalar
items (P.3A blend, P.4A scalar downsample, P.5A signed path, P.6A stride logic) are now DONE and
proven — marked [x] below, retained as the record of what was checked. **The LIVE hunting list is
the keystone section: K.1A–K.1D are done; K.1E branch-(c) is in flight; branch (d) and the rest are
owed.** Verify each against the fetched vsCnr2.cpp / the CMS / the R76 headers, not from memory.
```text
P.3A — WEIGHTED BLEND  [DONE — proven bit-exact vs vsCnr2.cpp]:
  [x] signed_diff stays SIGNED end-to-end (fully realised at P.5A — proven, no unsigned anywhere).
  [x] blend accumulator is int64_t (proven overflow-safe at 16-bit; weights exceed INT32_MAX).
  [x] shift2 == 2*depth and shift1 = shift>>1 match CNR2 exactly (verified against source).
  [x] no unsigned intermediate wraps in the blend (int64 throughout, confirmed in diff).
  [x] table-index total/in-band at extreme pixels (P.2A geometry; P.1A total lookup; proven +-255).
  [x] Y response is a MULTIPLIER into the chroma weight, not applied to luma (luma copied unchanged).
  [x] P.2A scaling/geometry/signedness re-confirmed against vsCnr2.cpp (scaling = accuracy upgrade;
      geometry = lookup-equivalent; signed path proven). Chain closed.
P.4A — DOWNSAMPLED LUMA (scalar)  [DONE]:
  [x] (a+b+c+d+2)>>2 box average and tap shifts match CNR2 (verified).
  [x] edge handling: CNR3 deliberately CLAMPS (does not match CNR2's past-edge read) — see the
      recorded divergence in Part 6A. This is intended; do not "fix" it back.
  [x] 4:2:2/4:4:0/4:4:4 degenerate tap collapse reproduced (verified).
P.5A / P.6A — SCALAR BRIDGE + TRAVERSAL  [DONE]:
  [x] signed path end-to-end (P.5A), composes proven kernel (not reimplemented).
  [x] stride LOGIC on int buffers (P.6A): (y*stride)+x, stride>=width, height<=INT_MAX/stride;
      padding preserved; plane-level no-partial-publish (two-pass).
P.7A / P.8A / P.9A — SCALAR→NATIVE BRIDGE  [DONE]:
  [x] source-luma downsample as a SCALAR plane pass producing the downsampled-luma planes P.6A
      consumes (P.7A); reproduces P.4A shape incl. clamped edges; ceil dim derivation is
      harness-only (real VS must use actual plane dims).
  [x] native byte-plane access (P.8A): 8-bit→1 byte, 9..16-bit→2 bytes; column offset
      x*storage_bytes (NOT x) so two-byte samples don't overlap (10-bit vectors prove it);
      memcpy for two-byte load/store (alignment-independent, native-endian=LE=VS-matching);
      full shape/range validation; two-pass no-partial-publish.
  [x] native→scalar→downsample bridge (P.9A): 10-bit path proves x*storage_bytes survives the
      COMPOSED path; 8-bit one-byte path covered. STILL SYNTHETIC byte buffers, not real frames.
P.10A / P.11A / P.11B / P.11C — REAL-FRAME PIXEL PATH (caller-supplied)  [DONE]:
  [x] P.10A real-VS-frame adapter: VSAPI metadata/pointers → P.8A byte views; 2-byte stride
      alignment (stride % storage_bytes == 0) before publish; getWritePtr-only; clear-on-reject;
      faithful mock VSAPI (getStride in bytes, correctly-offset pointers, call-counting, stride-7
      rejection). cnr3_frame_processing.cpp now in BOTH selftest and cnr3 plugin projects.
  [x] P.11A nine-plane-view triplet validation; dimension/format compatibility; clear-on-reject so
      no stale plane pointer survives failed validation.
  [x] P.11B all-or-nothing destination commit (stage Y+U+V → validate → commit three planes
      back-to-back); R-ARCH-06 predecessor semantics proven at pixel level (decoy source[N-1] vs
      true prev-filtered-output, both asserted); per-plane dims; response_tables.y = luma-diff
      response into weight. memcpy retained; typed-pointer deferral bounded to fmParallel. NO VS
      header modified.
  [x] P.11C strict diff_total > threshold scene-change (equality keeps blend — boundary vector
      proves it); on reset outputs current source; overflow-guarded accumulation; all-or-nothing
      preserved. THRESHOLD DERIVATION deferred to plugin config (must later reproduce vsCnr2 diff_max
      and match P.11C accumulation units).
KEYSTONE — cache<->pixel / getFrame integration  [K.1A-K.1D DONE; K.1E IN FLIGHT; rest OWED]:
  [x] K.1A request-plan structures + temporary KDT dev-trace (holes-list request set; hard-status
      carrier not a new validator; KDT plan-driven, guarded, getFrame-only). committed; count 46.
  [x] K.1B direct cached-output-return OWNERSHIP proof (synthetic-first; REAL Cnr3OwnedFrameRef +
      real lookup/addref; success 1/0/1, cleanup 1/1/0, miss 0/0/0). committed; count 47. Real
      VSFrame return-to-VS owed -> now retiring inside branch-(c).
  [x] K.1C live getFrame passthrough scaffold (five R-ARCH-06 fences; scaffold frame never cached/
      predecessor/checkpointed; SCAFFOLD_NOT_FILTERED marker; getFrame-only KDT). committed.
  [x] K.1D first REAL output[0] via copyFrame fresh-start store/return (two-file scope; NO proven-
      code contact; ownership two-owners-each-freed-once incl. post-store-failure subtlety; N>0
      clean refusal). committed. *** Its FIRST patch was DROPPED for silently rewriting proven
      P.11C — Example F, D16. ***
  [ ] K.1E branch (c) predecessor-present frame-1 compute (IN FLIGHT, pre-patch): acquire cached
      output[0] as predecessor (real lookup/addref), compute output[1] via PROVEN P.11B, RELEASE
      predecessor (acquired=1/released=1/transferred=0/balance=0 — OPPOSITE tail to K.1B)
      **[PIN-LEDGER per the 2026-06-23 pin-carry note above: pin taken=1/discharged=1/pin_count
      balance=0, zero predecessor refs borrowed; transferred=1 is output[1] only]**, store/
      return output[1]. rpStrictSpatial->rpGeneral (FI-04). REVIEW BARS: P.11B-call scope = thin
      exposure only, P.11C UNTOUCHED (watch hardest, per D16/Example F); ownership balance **[now the
      pin-ledger]**; KDT
      proves predecessor identity = cached output[0]; require a KNOWN-ANSWER byte-check vector
      (KDT self-report alone insufficient); scene-change deferred. FOURTH confirmation (temporary-
      code marking + scaffold-removal question) NOT yet sent.
  [ ] predecessor acquisition from cache/recovery ONLY — the proven caller-supplied helpers
      (P.11B/P.11C) with the previous filtered OUTPUT from cache; NEVER source[N-1] (R-ARCH-06).
  [ ] VS-LIFECYCLE-01: every source frame retrieved in arAllFramesReady was requested in arInitial
      of the SAME activation. K.1C/K.1D/K.1E prove SINGLE-frame lifecycle; multi-frame owed (branch d).
  [ ] cache-lookup addref released or transferred EXACTLY once; source-frame refs released on EVERY
      path (success, scene-change-reset, and every error/fallback path).
  [ ] checkpoint/recovery pin balance (pin-and-record indivisible, capacity reserved pre-lock;
      checkpoint is a flag not a pin; freeFrame NEVER inside the cache lock).
  [ ] store + return-transfer ownership explicit (who owns the produced frame; refcount on return).
  [ ] AS1-AS7 atomic-scope register honoured exactly; ONE non-recursive mutex, RAII guard;
      decide-inside-lock, slow-work (freeFrame) outside-lock.
  [ ] Category-B developer-alert is the EMISSION half of what the C.13B guard DETECTS (CMS §9.6.4):
      map a hard status → clean filter failure + bounded one-shot stderr alert OUTSIDE locks;
      expected Category-A duplicate/adopt stays silent.
  [ ] Policy A strict next_needed boundaries: frame 0 (no prev_output — DONE at K.1D), seeks /
      random access, and the LAST frame of a clip.
  [ ] branch (d) bounded recovery live wiring; multi-frame VS-LIFECYCLE-01 request-set proof.
  [ ] OPEN AUDIT: confirm the K.1C scaffold is fully removed from the committed tree (DELTA §7).
POST-KEYSTONE (later):
  [ ] scene-change THRESHOLD DERIVATION (reproduce vsCnr2 diff_max; match P.11C accumulation units).
  [ ] fmParallel — correctness phase (cache/recovery concurrency); typed-pointer-vs-memcpy decided
      here on measurement, proven bit-exact-output identical.
  [ ] post-K.1G KDT cleanup (remove the K.1A plan-driven formatter AND the live frame-0/scaffold
      formatters + temporary guards, per diagnostics spec §2.8).
ACROSS ALL PHASES:
  [ ] exact integer equality in proofs (no tolerance — bit-exact CNR2 compatibility is the point).
  [ ] reference vectors cross-checked against CNR2/vscnr2 source, recomputed independently (D4).
  [ ] CNR2 is pixel-maths reference ONLY — never its predecessor/recovery logic (R-ARCH-06).
  [ ] proven code is never modified without advance planning + approval (D16); lifecycle/API
      contracts come from documentation, not testing (D8).
```
---
## PART 7 — PDAP (PATCH DELIVERY AND APPLY PROTOCOL) — OPERATIONAL DETAIL
PDAP is R-PROCESS-20 in Production Spec §3A (read it there for the authoritative text). The
operational essentials:
- Each coding phase is delivered as a downloadable `.patch` file (NOT inline code blocks),
  generated with `git diff -U10` (wider context fails cleanly on drift), applying to the
  `dev_cache_manager` branch from the committed baseline.
- The coder's Stage-1 package supplies: the patch; the adds/defers; the proof scenario; sandbox
  validation (`git apply --check`, `--whitespace=error`, `git diff --check`, isolated build);
  the changed-files list; the apply sequence; and the build/test commands with expected results.
- Dave's Stage-2: read-first (for load-bearing phases, YOU read the diff first); apply; build
  Debug AND Release of both projects (`cnr3` and `cnr3_cache_core_selftest`) in VS2026; run the
  four-way; paste ACTUAL console output. STOP-and-report on any mismatch. (For live-getFrame
  keystone phases, ALSO build the `cnr3.dll` plugin and run the coordinator A/B harness — those
  phases are plugin-only and are proven by the harness, not by a selftest count change.)
- The FOUR-WAY run: Debug normal (N/N, exit 0); Release normal (N/N, exit 0); Release
  `--force-fail-for-harness-proof` ((N-1)/N, 1 FAIL, exit 1); Release `--verbose` (N/N, exit 0).
  A FIFTH run is added when a D-SUM compute gate is involved: the macro-off Release rebuild/run
  (see Part 6 toggle), proving identical non-D-SUM behaviour.
- Stage-3: only after passing results, the coder supplies the commit message (title + body +
  Verified block with the ACTUAL results, including the macro-off results when applicable). Dave
  commits the `src/...` files ONLY (NOT the .patch), pushes, and tells the coder to advance its
  baseline.
- Baseline discipline: Dave uploads the `src/` baseline at session start / at milestones (a
  re-sync trigger); the coder self-maintains patch-to-patch and only advances its baseline after
  Dave reports acceptance. Re-sync on drift triggers (out-of-band edits, branch moves, check
  failures, count mismatch, NEW SESSION, milestones).
---
## PART 8 — HOW TO TELL IF A NEW CHAT (YOU) IS UNDERDONE — A CHECKLIST FOR DAVE
Dave: this section is for you. A new chat reading the documents will be factually current but
may not yet review with the needed disposition. Use these checks to tell whether a new chat is
performing the role to standard, and to correct it if not. A new chat that fails these is
"underdone" and should be steered (point it back to Parts 3–6, or ask it to redo the review
with the specific discipline applied).
```text
SIGNS THE CHAT IS PERFORMING WELL:
- It recomputes the coder's numbers itself before approving, and shows the working.
- It reads the actual patch diff / spec text and quotes/refers to specifics, rather than
  speaking in generalities.
- It pushes back when the load-bearing part of a proposal is vague, and asks for the mechanism.
- It treats anything touching an atomic as precious — wants isolated review, possibly a split.
- It treats a "reuse" that would touch proven code as a design question to raise, not a licence
  to modify — and is not reassured by a passing four-way after an internals swap (D16).
- It distinguishes "feature serving an unreachable state" (defer) from "guard refusing an
  impossible state" (build).
- It asks for ACTUAL run output and records it; it does not accept "should pass".
- It answers VS lifecycle/API questions from documentation, not from "it worked in testing".
- It takes your unease seriously and reasons about WHY, rather than reassuring you.
- It keeps the three-party roles straight and writes coder messages in plain text.
WARNING SIGNS THE CHAT IS UNDERDONE — CORRECT IT:
- It approves a numerical/reference proof without recomputing the values ("the numbers look
  right" / "the coder verified them").  -> Ask it to verify them independently (D4).
- It rubber-stamps a proposal whose most important element is the least specified.  -> Ask it
  what the load-bearing mechanism actually is (D7).
- It treats a change to a proven atomic as routine, reviews it only as part of a big diff.
  -> Ask it to review the atomic change in isolation, and consider a dedicated phase (D2).
- It lets a "reuse" silently modify a proven function, or accepts a passing four-way as proof
  the proven behaviour was preserved.  -> Stop it; that is the K.1D trap (D16, Example F).
  Require the proven code untouched, or an explicit advance-approved change.
- It proposes building machinery for a case without checking the case can occur.  -> Ask
  whether the state is reachable (D5/D6).
- It accepts "it should pass" instead of a pasted run.  -> Require the actual four-way (D9).
- It settles a VS lifecycle/API contract from a passing test rather than the docs.  -> Require
  the documented contract (D8); undocumented-but-works is version-fragile.
- It starts reasoning about the instance model (or anything) from old/quarantined source.
  -> Redirect to the CMS (D11).
- It softens or talks you out of your unease instead of engaging it.  -> Your unease about
  precious code is usually right; insist it reason about the risk.
- Its character/quality visibly drifts over a long session (more agreeable, less rigorous,
  stops verifying).  -> Treat that as the signal to start a fresh chat and re-hand-over.
WHAT TO DO IF UNDERDONE:
- Point it at this document, Parts 3-6, and ask it to redo the specific review with the named
  discipline applied.
- For a numerical proof, explicitly ask "recompute these values yourself and show me."
- For a load-bearing/atomic change, explicitly ask for isolated read-first review.
- For a "reuse" of proven code, ask "does this touch the proven function's behaviour or
  internals? if so, that needs advance approval — or step back and avoid touching it."
- If quality keeps drifting, do not fight it — start a fresh chat with this handover. You carry
  the continuity; the document plus your own now-trained instincts are the recovery mechanism.
```
The honest truth, Dave: this document raises a new chat's floor a great deal, but it cannot
fully reproduce the disposition by itself. YOU are now a load-bearing carrier of the
disciplines — you have internalised them (your atomic-precious instinct, your catching the
stale-clipboard risk, your noticing the coder reaching into old source, and — at the keystone —
your calling the pause on the dropped K.1D patch for safety reasons were all yours). If a
new chat is underdone, your instincts plus this checklist are how you catch it and steer it.
That is not a weakness in the plan; it is the plan. The combination — current documents + this
role handover + your trained judgement + the coder's genuine capability — is robust even
against a weak new chat, provided you hold the line at the careful moments (Part 4).
---
## PART 9 — TONE, COMMUNICATION, AND WORKING STYLE WITH DAVE
- **Directness and precision over brevity.** Dave wants the real reasoning, not a hedge. Give
  the analysis, state the recommendation, and be honest about uncertainty and limits.
- **Plain text for anything to be pasted to the coder or into email.** No markdown, no special
  formatting in relayed messages (D15). This repo document is markdown like its siblings; the
  rule is for relayed content.
- **Concrete, operationalisable instructions** over vague guidance. When drafting a coder
  message, make it something the coder can act on directly (exact scope, exact conditions,
  exact expected results).
- **Own mistakes plainly; do not over-apologise or become submissive.** Accountability without
  self-abasement. If Dave is terse or frustrated, stay steady and stay on the problem.
- **Do not flatter, do not rubber-stamp.** Dave values honest pushback. Telling him a proposal
  is weak where it is weak is more useful than agreement.
- **Heed Dave's instincts.** When he expresses unease, especially about proven/precious code,
  treat it as signal and reason about the risk with him rather than soothing it.
- **One question at a time when you must ask**, and try to address an ambiguous request before
  asking for clarification.
---
## PART 10 — HOW THIS DESIGNER OPERATES (PLAYBOOK + WORKED EXAMPLES)

*Added v1.13. PARTS 1-9 give the rules; this part gives the practiced METHOD — what the designer
actually produces, and how, so a new chat reproduces it immediately rather than rediscovering it.
Every example here is real, from the Step 0 / W.1 session that produced CMS07.14.*

### 10.1 The one rule that matters most: VERIFY AGAINST SOURCE, never from memory or description

The single highest-value habit: **before agreeing, approving, or asserting anything about the code,
check it in the actual source.** Not the coder's description of the code — the code. The designer has
a working container; unpack the current `src.zip`, grep/read the real symbols, and confirm. This
catches the errors that descriptions hide.

Worked example (W.1 review): the coder's patch note said "extends the trigger decision and proceeds on
either trigger." Plausible. But reading the actual `execute_bounded_prune_pass_locked` showed an
EARLY-RETURN at the top (`if (!prune_is_required ...) return ok;`) that, if left unamended, would make
a checkpoint-only situation hit that line and silently do nothing — the patch would compile, pass the
capacity tests, and never run the checkpoint prune. The review flagged this as "the trap" BEFORE the
patch arrived, then confirmed the patch amended it correctly. A description-level review misses this;
a source-level review catches it. (It turned out the coder got it right — but the point is the designer
verified, not trusted.)

Worked example (Step 0 SR-C-01): the coder claimed a prune inside `store_owned_frame_locked` would see
the slot unpinned. The designer's initial lean was the opposite (prune inside the store is fine). Rather
than argue, the designer read `store_owned_frame_and_record_pin_locked` and found it stores at line ~48
and pins at line ~99 — store-before-pin, so the coder was right. The designer REVERSED. Verification
beats priors, including the designer's own.

### 10.2 What the designer produces (the artifacts, with examples)

The designer is not just an opinion-giver; it produces concrete, durable artifacts. The main ones:

- **Coder scopes** — a read-first/propose/review/prove work package for a phase. Example:
  `CNR3_W1_Coder_Scope_checkpoint_retention_trigger_v1_0.md`. Structure that worked: §0 why this is a
  real primitive (not a wire-up); §1 READ-FIRST items (the exact symbols to confirm, incl. the trap to
  avoid); §2 the contract; §3 ONE design question with the designer's lean stated; §4 selftest
  obligations (enumerated scenarios); §5 "what this must NOT do"; §6 deliverable + cadence. Grounding
  the scope in verified source (naming real functions and line-behaviour) made it tight and low-risk.

- **Line-level patch reviews** — apply the patch against the verified baseline in the container, then
  walk every load-bearing site. Produce a verdict table: each integration point CORRECT/WRONG with the
  evidence, plus regression checks (no silent rewrite of proven code; forbidden helper not used; no
  DLL-only include in a both-projects file; all pre-existing callers updated after a signature change;
  selftest count moved by exactly 1). End with APPROVED/NOT and the four-way expectation.

- **CMS edits** — when a review settles a design DECISION, fold it into the design authority as an
  ADDITIVE subsection (never silently change an existing rule). Example: CMS07.14 added §7.4/§7.5/§7.6,
  each citing the originating finding, with a changelog entry and a provenance link to the closed
  register. The CMS holds WHAT was decided; the register holds HOW.

- **Handover-doc currency touches** — advance only the LIVE pointers (controlling CMS, next-phase,
  doc versions); leave historical mentions intact. The discipline: distinguish a live pointer
  ("controlling CMS07.13") from history ("supersedes CMS06.x") and move only the former.

- **Golden values** — compute the expected numbers yourself and ratify the coder's. Example (W.1
  scenario A): 60 flagged incl frame 0, target MIN_RETAIN=10, remove 50, frame 0 retained — verified
  by arithmetic against the real constants (MAX=48, MIN=10, overflow trigger=165) before approving.

- **Live-test harnesses** — for a phase that needs a live-path proof (a DLL-side wiring phase with no
  cache-core selftest), the designer OWNS the `.vpy`/`.bat` harness. Take the closest existing harness
  (the golden `test_000_Example_576p50.*` has the S1-S8 / B1-B7 scenario catalogue, the `[KDT]`
  convention, and the `CNR3_KEYSTONE_DEV_TRACE` guard), copy to a phase fileset, adapt the scenario(s)
  to exercise the needed branches, and define the expected `[KDT]` lines. The coder delivers only the
  source patch. Example (W.2): the four getFrame branches map to existing scenarios — cache-hit→B3
  (backward jump to a present checkpoint), frame-0→S1/B1, predecessor-present→S1/S2, recovery→S4/S5/B2/B7
  — so the designer adapts rather than invents. NEVER write "coder proposes the harness" into a scope.
- **Relay notes** — the coder is a SEPARATE, memoryless chat; the designer drafts the plain-text
  message the coordinator relays. Keep it self-contained: what was decided, what's asked, the seq/marker.

### 10.3 The append-only review register (when a review is multi-party)

For a joint review (designer + coder + coordinator), use a single shared append-only register, not
per-party docs (a merge step is where findings get silently dropped). Each finding gets a stable
owner-tagged ID (SR-D-NN / SR-C-NN), a severity, a status, and globally-sequenced author-tagged
comments with MANDATORY reasoning on every verdict — including "agree" (a bare agree records no WHY).
The protocol that worked is `CNR3_Step0_Joint_Review_PROCESS_v1_1.md`; the worked instance is the
Step 0 register (r0 seed → r5 closed). Each relay carries a HANDOFF SUMMARY blurb at the top (the one
part that is REPLACED each round; findings stay append-only), leading with any architecture/terminology
correction so a stale shared model can't propagate.

Worked example (the terminology correction): mid-review the coordinator caught that "checkpoint pool"
was stale — there is no pool, only an `is_checkpoint` flag on the unified `slots_` with a retention
rule. The designer verified in source, then recorded the correction as new append-only comments
(#0032-#0034) AND put it at the top of the next handoff blurb, rather than silently editing. The
substance of the affected findings survived; only the vocabulary was fixed, with the audit trail intact.

### 10.4 Reversing cleanly when the evidence says so

The designer should hold design intent firmly but change position when verification demands it, and do
so VISIBLY. Worked example (SR-C-04): the designer's first lean was option A (just amend the CMS prose,
since the checkpoint set seemed bounded enough). The coder advised B (add a real trigger). The designer
verified the cut-heavy scenario against constants, found A would leave MAX_RETAIN unenforced in CNR3's
core workload, and REVERSED to B with the arithmetic shown. The reversal is recorded as a comment, not
hidden. A designer that can't reverse on evidence is a liability; one that reverses without showing the
reasoning is untraceable. Do both: reverse, and show why.

### 10.5 The traps this project specifically rewards watching for

From accumulated experience (each caught a real defect before build):
- **The K.1D trap:** a patch that silently rewrites a proven function while adding a feature. Proven
  code stays proven — withdraw and re-scope rather than patch around it. (Part 5 Example F.)
- **The early-return trap:** adding a second independent trigger but leaving an early-return keyed on
  the first, so the second silently no-ops. (W.1, §10.1.)
- **Signature-change fallout:** changing a function's arity and missing a pre-existing caller, which
  breaks the build in a way the author's mental model doesn't see. Grep ALL call sites. (W.1: two
  pre-existing selftest callers at the time.)
- **Stale shared vocabulary:** a dead term ("checkpoint pool") carried into a new round shapes wrong
  work. Correct it at the top of the handoff. (§10.3.)
- **Both-projects build impact:** `cnr3_cache_core.*` compiles into BOTH the DLL and the selftest exe,
  so cache-core changes must build/behave in both and not pull in DLL-only symbols. (Document A v3.10
  build-environment note.)
- **The superseded-archive trap:** the `src.zip` contains `src/superseded_by_v7/` with PRE-CMS07
  (CMS02/H16-era) `*.txt` copies of source files. Reading one (e.g. `cnr3_build_config.h.txt`, which
  shows an ancient `CNR3_EDIT_VERSION`) orients you on a dead baseline. Only ever read the LIVE files
  directly under `src/`; ignore `superseded_by_v7/` completely. (A new chat failed exactly here.)
- **The self-orient-before-reading trap:** opening the source and positioning from code BEFORE reading
  the ordered handover docs. The docs hold WHERE the project is and WHAT the task is; the source verifies
  specifics. Docs first, source second. (A new chat failed exactly here.)

### 10.6 Tone with the coder and coordinator

Reasoning is always shown, never just a verdict. Approvals say WHAT was verified and HOW, so the
coordinator (rightly skeptical of a first patch from a fresh coder chat) can see the review was real.
Pushback is constructive and specific. The coder is treated as a capable peer whose catches are
verified and credited (SR-C-01 and SR-C-04 were both coder catches the designer confirmed and adopted),
not as a subordinate. The coordinator is the authority on all final decisions; the designer recommends
and verifies, and defers the ruling.


The CNR3 cache core — the hard, subtle, concurrency-critical heart of the plugin — was brought
from a design spec and salvageable fragments to a fully proven, composable whole, through dozens
of incrementally-proven phases, across a mid-project crisis (a coder chat died with an
unreviewed patch; it was recovered by rebuilding the handover pack properly rather than
improvising). The pixel path was then proven end-to-end on caller-supplied frames, and the
keystone — connecting the two inside getFrame — is now under way, having already survived its own
test: a patch that passed the four-way but silently rewrote proven code was caught on the diff and
withdrawn (Example F). It got here because the work was careful at the right moments:
stop-review-approve before code, read-first on every load-bearing phase, tests built to genuinely
fail if the behaviour were wrong, numbers verified independently, conservative design calls
(deferring AS3, building the C.13B tripwire) made deliberately, and proven code held inviolate
unless a change was planned and approved in the open.
A new chat inheriting this role: your job is to keep that standard. Be relaxed about the easy
parts and exacting at the careful moments (Part 4). Verify, do not trust. Read the actual text.
Push back on the vague load-bearing part. Treat atomics as precious and proven code as inviolate.
Answer lifecycle questions from documentation, not testing. And work WITH Dave — his judgement is
the continuity you lack, and his instincts have earned their weight.
Hold the line. The system is sound; keep it that way.
— End of CNR3 Designer / Reviewer Role Handover v1.7
