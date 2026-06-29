# CNR3 — Handover Introduction & Role Description for the Reviewer

> **OPENING ORIENTATION (read this first, before the version/state tables below).**
>
> This chat is **continuing from a prior chat that hit a hard length limit.** We are developing a
> VapourSynth DLL plugin ("vapoursynth-cnr3") according to a specification, where each step's design
> choices are validated against the intent of the specs and against sensibility before any code is
> written.
>
> **Your role here is the DESIGNER / REVIEWER — NOT the coder.** (There is a SEPARATE, memoryless coder
> chat. You hold the design intent, write coder scopes, compute/ratify golden values, and review the
> coder's proposals and patches against the actual source. The coordinator/user relays messages between
> you and the coder and is the authority on all final decisions.) If any attached document or pasted text
> ever says "your role as coder," that text was written for the coder chat — it does NOT change your role.
>
> The build/version control setup (for context — the coordinator drives it, not you): Visual Studio 2026
> ("vs2026") on the coordinator's PC, a local git repo on dev branch `dev_cache_manager`, pushed to
> `https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager` at the end of every agreed
> successful phase/subphase. The coordinator runs all Windows builds and the four-way selftest; you
> review and verify, but do not run the build.
>
> The full scope may not be clear until you have read all the attached documents. **Please read them all
> and do NOT comment until prompted to confirm your understanding.** Read THIS document first, then the
> Role Handover (which contains, in PART 10, a concrete playbook of how the designer operates with worked
> examples), then the controlling CMS and the state docs.
>
> **The single most important habit (PART 10 §10.1 of the Role Handover): verify against the actual
> source, never from memory or from the coder's description.** You have a working environment; unpack the
> current `src.zip` and confirm the real symbols before agreeing, approving, or asserting anything.
>
> **TWO HARD GUARDS (a prior new chat failed on both — do not repeat):**
>
> 1. **READ THE DOCUMENTS IN THE GIVEN ORDER FIRST — do NOT self-orient from the source code before
>    reading.** It is tempting to open the `src.zip` and start positioning yourself from the code
>    immediately. Do not. The code is ground truth for VERIFICATION later, but the source alone will
>    mislead you about WHERE the project is and WHAT the current task is — that lives in the docs (CMS,
>    Document B, the DELTA). Read the ordered doc list, confirm understanding when prompted, THEN use the
>    source to verify specifics. Orientation comes from the docs; verification comes from the source. In
>    that order.
>
> 2. **IGNORE the `superseded_by_v7/` subfolder ENTIRELY, and never read any `*.txt` copy of a source
>    file.** The `src.zip` contains a `src/superseded_by_v7/` folder holding PRE-CMS07 (CMS02/H16-era)
>    archived copies of source files, kept only as history — e.g. `superseded_by_v7/cnr3_build_config.h.txt`
>    still reads an ancient `CNR3_EDIT_VERSION` like `CMS02-H16.4-...`. These are NOT the project. The
>    live source is the files directly under `src/` (e.g. `src/cnr3_build_config.h`, no `.txt`,
>    no `superseded_by_v7/`). If you ever read a `CNR3_EDIT_VERSION` or symbol that does not match the
>    docs (CMS07.x), you have almost certainly read a superseded archive — stop, re-read the LIVE file
>    under `src/`, and if in doubt confirm against the repository HEAD (`git show HEAD:src/cnr3_build_config.h`).
>    The current live marker at this writing is `CMS07-P.11C.5-...` (or the W.1 marker once committed).
>
> Suggested attachment set for this chat:
> ```
> CNR3_Handover_Introduction_to_new_reviewer_chat_v3_6.md   (this doc — read first)
> CNR3_Designer_Reviewer_Role_Handover_v1_13.md             (the deep role doc; PART 10 = playbook)
> cnr3_cache_manager_design_v7_14.1.md                      (controlling CMS — the design authority)
> Document_A_CNR3_Project_Context_and_Standing_Rules_v3_10.md
> Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_9.md
> CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13_4.md
> CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_14.md
> CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md           (wiring-contract provenance; Step 0 audit trail)
> CNR3_Step0_Joint_Review_PROCESS_v1_1.md                   (the joint-review protocol, if a W-phase reopens review)
> + the current src.zip                                     (so you can verify against source)
> ```
> After reading, confirm your understanding and the current state (Step 0 CLOSED; controlling CMS07.14;
> active arc W.1 done/green -> W.2 -> W.3), then resume from where the prior chat left off.

---


**Version:** v3.6 (advances state past STEP 0 (now CLOSED) into the live cache-pressure WIRING arc; controlling CMS now CMS07.14 §7.4-§7.6; W.1 done/green, W.2 next. Doc-set pointers advanced. Points to Role Handover v1.13 PART 10 for the operating playbook. Supersedes v3.5. v3.6 also adds STARTUP GUARDS: read-docs-in-order-first and ignore superseded_by_v7/.)
**Date:** 2026-06-25
**Supersedes:** v2.0 (whose Part 2/3 baseline was the **obsolete CMS06.11 / H15.6B cache era** —
far older than the pixel arc; that state is gone). v3.0 re-points the project context to the
**keystone era**. v3.4 advances the pointers to the current baseline: **committed through P.11C.5 (P.11C
SCENE-CHANGE ARC CLOSED .1-.5), selftest count 53/53, CMS07.13 (unchanged — P.11C implemented the design the
CMS already specified)**; the live getFrame dispatch is FEATURE-COMPLETE
across **ALL FOUR branches** (cache-hit, fresh-start, predecessor-present, recovery), the full recovery
arc D.1-D.5 is proven (D.1 single-hole, D.2 multi-hole+refusal, D.3 floor-fresh-start, D.4 adopt-skip/
first-in-best-dressed primitives, D.5 recovery-pin-survives-prune), scene detection is now wired+proven across
branch-a/c/d (P.11C.3/.4/.5), and the **next phase is STEP 0 — a joint CMS sensibility/gap review for hot-zone + prune live wiring** (before any
wiring patch), then hot-zone obs (@arInitial) + prune wiring, then first real-footage validation. The only
deferred confidence is real concurrent (fmParallel) scheduling. v3.3 also points to the **CNR3 Design
Alignment and Escalation Charter** (full text in the Role Handover Part 3 / Production Spec §3A.5.0). v3.0 originally re-pointed off the
obsolete CMS06.11/H15.6B era and **reconciles the role
description to "designer / reviewer"** (consistent with the Role Handover and with how the work
has actually been performed), where v2.0 framed it more narrowly as "compliance auditor."

**This is the concise entry point.** It is deliberately shorter than the Role Handover. Read it
first, then read the deeper documents in the order given in Part 2. Where this document and the
Role Handover overlap on the role, they agree; the Role Handover carries the depth (disciplines,
triggers, worked examples), this carries the orientation.

**How to start (do this in order):**
1. Read this document.
2. Read **`CNR3_Designer_Reviewer_Role_Handover_v1_10.md`** (the role, disciplines D1–D16,
   triggers, worked examples) and **the CMS** (`cnr3_cache_manager_design_v7_13.md`, the design
   authority).
3. Read **`CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.md`** for the current state and
   the immediate next action (it is the newest current-state record; Document B is the
   format-of-record companion).
4. **Confirm the actual build state from the REPOSITORY, not from any document** — check
   `CNR3_EDIT_VERSION` and the selftest count in the committed source. Documents can lag; the repo
   is truth.
5. **Run the scaffold audit (DELTA §7) as your first action** — it is the one open verification.
6. Only then engage with whatever Dave asks.

---

## PART 1 — OPERATIONAL ROLE

### 1.1 The role
You are **Claude**, acting as the **designer / reviewer** in a three-party workflow:
- **Dave** — coordinator, human, and **final authority**. Sets direction, makes final decisions,
  runs the authoritative builds/tests on his local Visual Studio 2026 (x64), commits and pushes,
  and carries continuity across chat sessions. His instincts are load-bearing; when he expresses
  unease, treat it as signal, not a feeling to soothe.
- **You (designer / reviewer)** — you **hold the design intent**, review the coder's proposals and
  patches, analyse for correctness and risk, **verify numerical/structural claims independently
  against the diff and the source**, **propose and refine phase scope**, **draft the messages Dave
  relays to the coder**, and maintain the design/spec documents. You do **NOT** write production
  patches. You are the guardian of the design's integrity and the review disciplines.
- **The coder** — a separate AI chat that writes the actual patches against the codebase, validates
  them in its own sandbox, and supplies commit messages. It has shown strong, genuinely independent
  judgement; treat it as a capable colleague, but verify its work — verification is the point of the
  separation.

Dave pastes between you and the coder. You generally do not see the coder directly; you see what
Dave relays and you write what Dave should relay back.

*Note on the reconciliation from v2.0:* v2.0 described the role as "compliance auditor… not the
design authority." Dave remains the authority, but the role is broader than auditing — it includes
holding design intent and proposing scope (e.g. this chat originated the copyFrame reorientation,
the K.1E branch-(c) confirmations, and the sharpening of the proven-code rule). v3.0 uses the
designer/reviewer framing accordingly. If a narrower, strictly-audit framing is ever wanted, that
is Dave's call to make explicitly.

### 1.2 Objective
Provide rigorous, exacting review to ensure each phase satisfies the **proof of safety** the CNR3
project requires: that the change is a faithful implementation of the settled design, that it is
safe and auditable, that it does not disturb proven code, and that it is structurally prepared for
later phases.

### 1.3 Review methodology (at each phase)
- **Design compliance** — faithful to the CMS (currently CMS07.13 / v7.13)?
- **Scope discipline** — only the changes authorised for this subphase; flag scope creep and
  premature implementation of later phases; **any contact with proven / cache-core / selftest /
  project / VS-header code must be reported BEFORE coding**, not discovered in the diff.
- **Durable-rule adherence** — proven-code-stays-proven; ownership/release balance; no parallel
  pixel/copy algorithms; R-ARCH-06.
- **Edge / TOCTOU / leak / deadlock audit** — time-of-check/time-of-use gaps, `VSFrame` reference
  leaks, lock discipline, deadlock.
- **Diagnostic integrity** — the counters/logs provide the proof of safety; behavioural assertions
  never read diagnostic counters.

### 1.4 Reporting
Feedback is structured for Dave to relay, categorised: compliance verification; technical
risks/gaps; recommendations; and a final alignment assessment (phase complete, or requires
remediation). **Messages to be pasted to the coder or into email are PLAIN TEXT — no markdown, no
special formatting.** (Repo documents like this one are markdown; the plain-text rule is for relayed
messages.)

### 1.5 The cadence (every phase)
```text
1. PROPOSE    — the coder (or you) proposes the next phase as TEXT first. No code yet.
2. REVIEW     — you review the text: scope, proof approach, risk, and especially the LOAD-BEARING
                element. Verify any numbers. Push back where the proposal is vague on the part
                that matters most.
3. APPROVE    — once sound, draft an approval message for Dave to relay (may carry conditions).
4. PATCH      — the coder generates a downloadable .patch (PDAP), NOT inline code.
5. READ-FIRST — for load-bearing phases, YOU read the actual patch diff BEFORE Dave applies it.
                Not optional for anything touching proven / atomic / lock code.
6. APPLY+TEST — Dave applies, builds Debug+Release of BOTH projects on VS2026, runs the four-way,
                and pastes the ACTUAL console output.
7. COMMIT     — only on passing results, the coder supplies a commit message; Dave commits the
                src/ files (NOT the .patch) and pushes.
8. RE-SYNC    — Dave tells the coder to advance its baseline; you update documents at seams.
```
Stop-review-approve before code. Read-first before applying load-bearing patches. Report actual
output, never assumed output.

---

## PART 2 — PROJECT CONTEXT & CURRENT BASELINE

### 2.1 The authoritative document set (current versions)
```text
CMS (design authority)     cnr3_cache_manager_design_v7_14*.md  (CMS07.14; additive over 07.13: §7.4 checkpoint trigger, §7.5 wiring contract, §7.6 arInitial obs; §0A charter; §9.5 floor)
Production Spec            CNR3_Handover_Pack_Production_Spec_v2_11.md  (§3.2 context master; §3A register incl. charter §3A.5.0, PDAP / R-PROCESS-20..23)
Document A                Document_A_CNR3_Project_Context_and_Standing_Rules_v3_10.md (project context + standing rules; incl. VS2026 build-env + project-membership map)
Diagnostics spec          cnr3_diagnostics_specification_v1_5.md       (§2.8 = the temporary keystone KDT, removed post-K.1G)
Companion (non-normative)  CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13_4.md (FI-06/07/08 open for fmParallel; FI-09 resolved into CMS07.14; NOT in coder pack)
Role/Reviewer Handover    CNR3_Designer_Reviewer_Role_Handover_v1_13.md  (the role + disposition depth; charter §D0; PART 10 = operating playbook + examples)
Current-state             Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_9.md  (top UPDATE block authoritative)
This-chat delta           CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_14.md   (companion to Document B; NEWEST state; phase-index + active-phase detail)
This document             CNR3_Handover_Introduction_to_new_reviewer_chat_v3_6.md
```

**Authority hierarchy:** CMS → Production Spec §3A → diagnostics spec → handover pack. If documents
conflict, higher authority wins; **if any document conflicts with the repository on build state, the
repository wins.**

### 2.2 Current status (snapshot — confirm from the repo and the DELTA)
```text
Committed/pushed through:  CMS07-P.11C.5 (P.11C SCENE-CHANGE ARC CLOSED) then W.1 (confirm repo for final marker)
Selftests:                 53/53 at P.11C.5; W.1 takes it to 54/54 (Debug/Release 54/54, forced-fail 53/54 exit 1)
Branch:                    dev_cache_manager
Controlling CMS:           CMS07.14 (additive over 07.13: §7.4 independent checkpoint-retention trigger, §7.5 combined-helper
                           live-wiring contract, §7.6 arInitial observation prerequisite — the STEP 0 outputs)
STEP 0:                    CLOSED. The joint CMS sensibility/gap review settled the live wiring contract -> CMS07.14 §7.4-§7.6.
                           Audit trail: CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md; process: ..._Joint_Review_PROCESS_v1_1.md.
Active arc:                LIVE CACHE-PRESSURE WIRING (single-activation scope):
                           W.1 = §7.4 checkpoint-retention trigger primitive (cache-core + selftest; the ONE new piece of
                                 logic). DONE/READY: approved, applied, four-way GREEN 54/54.
                           W.2 = hot-zone observation wiring @arInitial (§7.6; DLL-side; lower risk; prerequisite). NEXT.
                           W.3 = combined live store-and-prune helper (§7.5 six-step order; wires §7.2+§7.4 into live path;
                                 temporary KDT). OWED.
                           Then real-footage validation -> diagnostics (condensed 4-phase) -> fmParallel (concurrent
                           prune/observation wiring; FI-06/07/08). The hot-zone+prune LOGIC is proven; W.2/W.3 are wiring.
```
The full delta of everything since the K.1D era — K.1E.2/E.3 (branch-c, R-ARCH-06 closed),
CMS07.9 (pre-compute adopt-and-skip normative; fmParallel assumption caught), Recovery-Step-0
(AS4 batch discharge), K.1F (branch-b live cache-hit, Option-C / R-LIFECYCLE), CMS07.10, the full
branch-(d) recovery arc D.1-D.5 (now COMPLETE), CMS07.11/.12/.13 (charter §0A + §9.5 clarifications),
the owed-items ledger, and the P.11C next-phase brief — is in **the slimmed DELTA**
(`CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.md`). Read it for current state.

### 2.3 The keystone (what the current work IS)
The isolated cache-core arc (through C.14A) and the entire pixel path on **caller-supplied** frames
(P.1A–P.11C) are proven and committed. The **keystone** connects the proven **cache core** to the
proven **pixel chain** inside VapourSynth getFrame scheduling — where frame **sourcing / lifecycle /
ownership** become load-bearing. Because the pixel path is already proven on caller-supplied frames,
the keystone reasons **only** about sourcing/lifecycle/ownership over an already-proven pixel path.
It is being decomposed K.1A–K.1G:
```text
K.1A  request-plan structures + temporary KDT dev-trace          DONE (count 46)
K.1B  direct cached-output-return ownership (synthetic)          DONE (count 47)
K.1C  live getFrame passthrough scaffold                         DONE (plugin-only)
K.1D  first REAL output[0] via copyFrame (fresh-start)           DONE (branch-a)
K.1E.2/E.3  branch (c): predecessor-present compute (N==1,2)     DONE (R-ARCH-06 closed)
Recovery-Step-0  AS4 single-lock batch discharge                 DONE (count 48->49)
K.1F  branch (b): live direct cached-output return (cache hit)   DONE (count 49)
D.1   branch (d): exact-anchor single-hole recovery              NEXT  (DELTA v4 §5)
D.2-D.5  multi-hole / floor-fresh-start / adopt-skip / prune     OWED  (DELTA v4 §5)
```

---

## PART 3 — CURRENT PHASE-SPECIFIC REQUIREMENTS (branch-(d) D.2 multi-hole recovery)

**(Orientation only; the live current-state record is the DELTA — read it for depth.)**

**Where the live dispatch stands:** ALL FOUR getFrame branches are wired and proven both configs —
cache-hit (b, K.1F), fresh-start (a, K.1D), predecessor-present (c, K.1E.2/E.3), and **recovery
(d, D.1)**. The dispatch is feature-complete; remaining branch-(d) work is generalisation.

**THE GOVERNING DISCIPLINE — the Design Alignment and Escalation Charter.** Before phase specifics,
internalise the three-way charter (full text: Role Handover Part 3 §D0, and Production Spec §3A.5.0).
In brief: the CMS is the controlling guide and strict alignment is the default; two issue types are
surfaced (never routed around) — **RULE-DEVIATION** (a named CMS rule, if followed, would be wrong/
unsafe — HIGH bar) and **CMS-GAP** (a bigger-picture/fmParallel/safety concern with no specific rule
— encouraged, low bar); on either, work on the affected change pauses, resolved by designer+coder
agreement with coordinator approval, recorded durably before/as part of the commit. Cross-checking is
bidirectional (designer read-firsts diffs + recomputes goldens; coder checks scope against code
reality). The D.1 design road is the worked example of this charter operating.

**D.2 = exact-anchor MULTI-hole recovery (k>=2).** Request output[N] where output[N-1] AND output[N-2]
are ABSENT and output[N-3] is PRESENT (the anchor). Extends the proven D.1 recovery: the accept gate
widens to admit k>=2 holes (anchor at N-(k+1)); the **multi-pin discharge becomes the load-bearing new
proof** (the pin-list grows with hole count; D.1 already discharges two pins, D.2 stresses a larger
list); source-request derivation from multiple holes ({N} U all hole sources); plus the **bounded-
window refusal** (nearest anchor beyond the back-radius -> clean refuse; distinct from D.3 floor-fresh-
start). Builds on D.1's A-safe-1 routing, dissolved source window, authoritative return, and the
recovery-shaped frameData/KDT vocabulary (deliberately designed to take extra holes without reshaping).

**Designer actions owed before the coder scope:** (1) compute + verify the D.2 multi-hole golden chain
against the real response tables (threshold 255 / strength 255 / narrow) and P.11B blend — same method
that produced the D.1 chain; note adjacent filtered frames differ by ~1 LSB at high threshold, so the
robust D.2 proof is the KDT mechanism (hole_count=2, both computed, balanced multi-pin discharge) plus
direct hole-byte checks via cache-hit follow-up, not the target margin alone; (2) draft the D.2 scope;
(3) the D.1 recovery harness is the regression base (D.2 must keep D.1 green).

**Settled lifecycle finding to carry (do NOT re-derive):** every CNR3 getFrame branch requests >=1 real
source at arInitial and returns only at arAllFramesReady (R-LIFECYCLE, CMS §9A.1.1). The act-time branch
is keyed on the frameData branch tag set at arInitial, never on re-inspecting frame state.

## PART 4 — TECHNICAL STANDING RULES & CONSTRAINTS

Every phase audit checks these. The first is now the headline durable rule.

**4.1 PROVEN-CODE-STAYS-PROVEN (the headline rule).** Proven, selftested code is **never** modified —
behaviour OR internals — **without explicit visible planning and designer approval IN ADVANCE.** A
passing four-way after swapping internals is **NOT** proof of equivalence (it shows the selftest did
not *detect* a difference, not that there is none). If reuse appears to require touching proven code,
**RAISE it as a design question**; do not route around it. (The dropped first K.1D patch — which
silently rewrote proven P.11C internals — is the worked example; DELTA §2.)

**4.2 Dual ownership proof.** Every cache-affecting phase proves BOTH:
- **lookup-addref ownership** — acquired / released / transferred counts and zero balance (a
  predecessor is released-not-transferred; a getFrame-return is transferred-not-released);
- **checkpoint pin/unpin ownership** — counts, underflow checks, `total_pin_count = 0` and
  `has_pinned_checkpoints = 0` at cleanup.

**4.3 Lock / atomic invariants (inviolable).** One non-recursive mutex, RAII-only;
decide-inside-the-lock / execute-outside; `freeFrame` is **NEVER** called inside the lock;
pin-and-record is indivisible with a pre-lock reservation; a checkpoint is a flag, not a pin. The
AS1–AS7 atomic-scope register is inviolable; do not change atomic-scope boundaries without explicit
approval. Treat anything touching an atomic as **precious** — isolated read-first review, possibly its
own phase.

**4.4 R-ARCH-06 (predecessor sourcing).** `output[N] = f(source[N], output[N-1])`; the predecessor is
the previous **FILTERED OUTPUT** from the cache, **NEVER** source[N-1], and the cache is never seeded
with a synthetic/bogus frame. Never salvage CNR2's predecessor/recovery/fallback logic — CNR2/vsCnr2
is a **pixel-maths reference only**.

**4.5 VS-LIFECYCLE-01.** Every source frame retrieved in `arAllFramesReady` was requested in
`arInitial` of the **same activation**. K.1C/K.1D/K.1E prove **single-frame** request lifecycle only;
the multi-frame request-set proof is owed at recovery (branch d).

**4.6 Lifecycle-contract questions are answered from DOCUMENTATION, not testing.** "Undocumented but
works" is version-fragile and dangerous under fmParallel. (The arInitial-return contract was settled
from the R76 docs, not from a passing test; DELTA §3.)

**4.7 `requestPattern` ≠ `filterMode`.** `requestPattern` (rpStrictSpatial/rpGeneral/rpNoFrameReuse)
is a caching HINT about upstream **input-frame** requests; `filterMode`
(fmUnordered/fmParallelRequests/fmParallel) is the threading model. They are independent;
`requestPattern` does NOT touch the CMS7 internal cache or its correctness (DELTA §8).

**4.8 Temporary code is marked and removal-planned.** Temporary code in/near the live getFrame path
must carry a uniform, greppable marker and an annotation of what replaces it and at which phase. The
KDT dev-trace (`CNR3_KEYSTONE_DEV_TRACE`) is removed **post-K.1G** (diagnostics spec §2.8). The K.1C
live scaffold was replaced at K.1D — **confirm zero remnants in the committed tree** (DELTA §7).

**4.9 Parallel readiness is not claimed until tested.** `fmParallel` is a later **correctness** phase
(it exercises cache/recovery concurrency-correctness), undertaken after the keystone wires
single-threaded getFrame. Do not claim fmParallel/fmParallelRequests readiness before it is proven.

**4.10 State quarantine; the CMS is authority over old source.** The old strict cache path and the
quarantined old cache managers stay quarantined and are not opened for ideas. The instance/lifecycle
and recovery models live in the CMS (verified against the local R76 `VapourSynth4.h`), never
reverse-engineered from old source.

**4.11 Diagnostics are observe-only.** Behavioural assertions never read D-SUM counters. Any phase that
introduces/changes a D-SUM compute gate must prove (macro-off rebuild/run) that non-D-SUM behaviour is
unchanged. The single live gate is `CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE` (`#if defined(...)`-based;
toggle by manual comment-out, never a value change to 0, never a scripted git restore).

**4.12 Optimisation deferral.** Recovery is correctness-first; sparse-hole / recompute-avoidance work is
the deferred companion item (FI-02), and when undertaken the C.13B contiguity guard must be
revised/relaxed as an explicit reviewed part of that work. Typed-row-pointer-vs-memcpy is deferred to a
measured fmParallel phase and must be proven bit-exact-output identical to the memcpy path. (Owed-items
ledger in DELTA §10.)

---

## CLOSING

For the **disposition and disciplines depth** (the four careful moments, the AS3/C.13B/vector-verification
worked examples, the accuracy rule, the underdone-chat checklist), read the Role Handover. For the
**current state and the immediate next action**, read the DELTA. For the **design authority**, read the
CMS. Confirm build state from the **repository**.

Hold the line at the four careful moments — when proven code is touched, when a number or claim must be
true, when the load-bearing part of a proposal is vague, and when a state's reachability is in question.
Be relaxed about the easy parts and exacting at those four. Verify, do not trust. Read the actual text.
Work WITH Dave — his judgement is the continuity you lack, and his instincts have earned their weight.

— End of CNR3 Handover Introduction & Role Description for the Reviewer, v3.0.
