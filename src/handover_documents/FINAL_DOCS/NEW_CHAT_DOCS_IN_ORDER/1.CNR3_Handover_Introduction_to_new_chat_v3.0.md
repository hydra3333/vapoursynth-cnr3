# CNR3 — Handover Introduction & Role Description for the Reviewer

**Version:** v3.0
**Date:** 2026-06-23
**Supersedes:** v2.0 (whose Part 2/3 baseline was the **obsolete CMS06.11 / H15.6B cache era** —
far older than the pixel arc; that state is gone). v3.0 re-points the project context to the
**keystone era** (K.1A–K.1D committed, K.1E branch-(c) in flight) and **reconciles the role
description to "designer / reviewer"** (consistent with the Role Handover and with how the work
has actually been performed), where v2.0 framed it more narrowly as "compliance auditor."

**This is the concise entry point.** It is deliberately shorter than the Role Handover. Read it
first, then read the deeper documents in the order given in Part 2. Where this document and the
Role Handover overlap on the role, they agree; the Role Handover carries the depth (disciplines,
triggers, worked examples), this carries the orientation.

**How to start (do this in order):**
1. Read this document.
2. Read **`CNR3_Designer_Reviewer_Role_Handover_v1.6.md`** (the role, disciplines D1–D15,
   triggers, worked examples) and **the CMS** (`cnr3_cache_manager_design_v7_8.md`, the design
   authority).
3. Read **`CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md`** for the current state and
   the immediate next action.
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
- **Design compliance** — faithful to the CMS (currently CMS07.8 / v7.8)?
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
CMS (design authority)     cnr3_cache_manager_design_v7_8.md
Production Spec            CNR3_Handover_Pack_Production_Spec_v2_7.md   (§3A Prevailing Rules Register, incl. PDAP / R-PROCESS-20..22)
Diagnostics spec          cnr3_diagnostics_specification_v1_5.md       (§2.8 = the temporary keystone KDT, removed post-K.1G)
Companion (non-normative)  CNR3_CMS_Future_Investigations_and_Open_Questions_v7_8.md   (FI-04 resolved into CMS §9.7.7; NOT in the coder pack)
Role/Reviewer Handover    CNR3_Designer_Reviewer_Role_Handover_v1.6.md  (the role + disposition depth)
Current-state             Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_2_9.md
This-chat delta           CNR3_THIS_CHAT_DELTA_keystone_K1A_through_K1E_branch_c.md   (companion to Document B; newest state)
This document             CNR3_Handover_Introduction_to_new_chat_v3.0.md
```

**Authority hierarchy:** CMS → Production Spec §3A → diagnostics spec → handover pack. If documents
conflict, higher authority wins; **if any document conflicts with the repository on build state, the
repository wins.**

### 2.2 Current status (snapshot — confirm from the repo and the DELTA)
```text
Committed/pushed through:  CMS07-K.1D  (live frame-0 fresh-start store/return — first REAL output frame)
Selftests:                 47/47 PASS  (forced-fail 46/47, exit 1; verbose 47/47 with all priors)
Branch:                    dev_cache_manager
In flight:                 CMS07-K.1E branch-(c) (predecessor-present frame-1 compute) — acknowledgement
                           accepted, PRE-PATCH; the fourth confirmation is drafted but NOT yet sent (DELTA §4).
```
The full delta of everything since Document B v3.2.8 — the four keystone commits, the K.1D
reorientation, the K.1E investigation and branch-(c) status, the owed-items ledger, the open
scaffold audit, and the reinforced disciplines — is in **the DELTA document**. Read it for current
state.

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
K.1D  first REAL output[0] via copyFrame (fresh-start)           DONE (plugin-only)
K.1E  branch (c): predecessor-present frame-1 compute            IN FLIGHT (pre-patch)
...   branch (d) recovery; multi-frame VS-LIFECYCLE-01; etc.     OWED (DELTA §10)
```

---

## PART 3 — CURRENT PHASE-SPECIFIC AUDIT REQUIREMENTS (K.1E branch-(c))

**(This replaces v2.0's H15.6B audit section, which is obsolete.)**

**K.1E = `CMS07-K.1E-live-predecessor-present-frame1-compute-proof`.** N==1 after K.1D stored
output[0]: at `arInitial`, acquire cached output[0] as predecessor (real lookup/addref, carried in
frameData) and request source[1]; at `arAllFramesReady`, retrieve source[1], compute output[1] via
the **proven P.11B** composition, **release** the predecessor after use, store output[1] per existing
checkpoint policy, return output[1]. N>1 is a clean refusal
(`NOT-YET-IMPLEMENTED branch=after-frame1-before-recovery-wiring`). **Proves N==1 only.**

**The four confirmations (three accepted; the fourth NOT yet sent — send it first, DELTA §4):**
1. **Scene-change deferred** — K.1E proves predecessor-present composition only; P.11C already proves
   reset for a given threshold. Live scene-change threshold derivation + reset wiring is deferred.
2. **Frame-1 acceptance = predecessor WIRING proof, not blend math.** P.11B owns the blend math.
   K.1E must prove the predecessor was **specifically cached output[0]** (KDT: `pred=0`,
   `pred_source=output_cache`, `pred_lookup=hit`) and was released (`pred_released=1`,
   `pred_balance=0`), AND there must be **at least one known-answer vector** giving frame 1 a real
   byte-check (not pure KDT self-report). KDT self-report alone is insufficient for a load-bearing
   claim.
3. **P.11B-call scope = thin exposure of proven code only.** Any `cnr3_frame_processing.cpp/.h`
   contact must be ONLY to expose/call the proven P.11B path; **P.11C body untouched**; no re-routing
   of proven internals; no new pixel/copy algorithm; **report-before-broadening**. This is the bar to
   watch hardest (see the K.1D reorientation in the DELTA §2 for why).
4. **[NOT YET SENT] Temporary-code marking + scaffold-removal question.** Temporary live-path code (the
   N==1 gate / N>1 refusal control-flow, any scene-change-deferral stub, the KDT line) must be
   **uniformly, greppably marked** AND annotated with **what replaces it and when**, so cleanup is
   grep-and-remove, not archaeology. Also ask the coder to confirm the K.1C scaffold is fully removed
   from the committed tree.

**Ownership bar for branch (c) (the core proof) — OPPOSITE tail to K.1B:**
```text
predecessor lookup/addref:  acquired = 1
predecessor used as input, NOT transferred to VapourSynth
released after compute / on every error path before compute:  released = 1
                                                               transferred = 0
                                                               balance = 0
```
**Dependency declaration:** `rpStrictSpatial` → `rpGeneral` (resolves FI-04; conservative-correct for
a recursive filter). **`fmUnordered` stays.** `requestPattern` is a SEPARATE layer from `filterMode`
and does NOT affect the CMS7 cache design (DELTA §8).

**Patch-review bars (verify against the diff):** P.11B-call scope (P.11C byte-unchanged); ownership
balance as above; the five-fence pattern; KDT getFrame-only / stderr-only / no `SCAFFOLD_NOT_FILTERED`;
N>1 gated before arInitial.

**Acceptance:** four-way unchanged (47/47) + the K.1E-shaped harness (frame-0 A/B byte-identical;
frame-1 KDT check + known-answer byte-check; N>1 clean refusal). The harness is coordinator-side and
is **not yet built** (DELTA §5).

---

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
