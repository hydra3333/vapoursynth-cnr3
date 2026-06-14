# CNR3 Register-Owned Rules Review — Reconciled

**Purpose:** Reconciled review of the proposed register-owned rules before populating
Production Spec §3A. This version makes every decision, states each comment in plain
human terms, and supplies replacement wording where a rule needs it.
**Status:** Draft review aid only. Not yet §3A. For the user's review — NOT to be handed
to the coder as-is.
**Date:** 2026-06-13

---

## Reviewer general comment (read first)

Three systemic problems run through the proposed list. They should be fixed
across ALL rules, not just the ones mentioned individually below.

1. **Lossy re-paraphrasing.** Many rules appear to have been restated in broader, vaguer language than
   we had agreed over the prior development. The effect is rules that "sound about
   right" but may have lost the precision that made them safe. The fix throughout is to pull
   each rule back to its specific, operationalisable intent — a human reading it should
   not be able to construe it unsafely.

2. **Version-pinning trap.** Several rules name "CMS07.0" as if that specific version is
   permanently authoritative. The standing intent is **the latest prevailing CMS**
   (CMS07.0 *or its later approved successor*). Every reference to the controlling design
   should read as "the latest prevailing CMS" — EXCEPT specific section pointers (e.g.
   "§9A.2"), which are version-specific and must be re-checked against a successor, and
   historical statements about what a version superseded, which stay pinned to that
   version.

3. **Handover-pack vs its specification confused.** Some rules appear to muddle "the handover pack"
   (a point-in-time snapshot of project state, generated *from* the Handover Pack
   Creation Specification, possibly recycling parts of the prior Document A) with "the
   specification that governs how packs are made." These are different things; rules that
   conflate them are wrong as written and must be re-grounded on that distinction.

A note on safety: where a rule touches the fmParallel end-goal, correctness of the
recursive chain, mutex/lock scoping, ownership/reference-count discipline, or anything
that could be construed to permit an unsafe shortcut, the wording must be unambiguous and
must not leave room for a permissive reading. fmParallel is the explicit end-game;
nothing may be implemented that obstructs it unless it is an unavoidable, explicitly
recorded, temporary stepping-stone that does not compromise the design path toward it.

It is probably worth including this definition or similar above the inserted ruleset:
**Definition used throughout (for clarity):**
- **Register-owned rule** — a standing process / governance / authority / architecture /
  salvage rule that has NO home in the CMS design document, so the §3A register is its
  authoritative home.
- **CMS-defined (handed-off) rule** — a design / cache-core / reference-count / recovery /
  constant / instrumentation rule that lives in the latest prevailing CMS; §3A does NOT
  restate, index, or rename it.
- **§3A** — the Prevailing Rules Register section inside the Handover Pack Creation
  Specification; the durable, authoritative home for register-owned rules.

The per-rule line starting with `**Decision:**` may be imprecise for this exercise, a
change could also be inferred from the suggested wording and the rule category;
where doubt exists ask for confirmation.

The per-rule line starting with `**Reviewer comment:**` may be imprecise or irrelevant
for this exercise with you, however if relevant they may inform the reason(s) for a
suggested change.

The per-rule suggested wording text has been reviewed and it would take a persuasive
argument to vary it; not impossible, although a change must be supported by compelling
reason(s).

---

# Rules for review

## R-AUTH-01 — Latest prevailing CMS is the controlling design authority

**Decision:** CONFIRM with modification (version-neutral wording).

**Reviewer comment:** Right rule. Only change: don't pin it to the literal "CMS07.0" —
say the latest prevailing CMS, so the rule stays correct after a future successor.

**Suggested wording:**
```text
The latest prevailing CMS (currently CMS07.0, or a later approved successor) is the
controlling design authority for the cache restart. Prior handover material, memories,
code assumptions, and earlier discussion are subordinate to it.
```

---

## R-AUTH-02 — Conflict or unclear-alignment with the CMS → CMS wins

**Decision:** CONFIRM with modification (version-neutral wording).

**Reviewer comment:** Correct as intended; version-neutralise.

**Suggested wording:**
```text
If the latest prevailing CMS conflicts with — or is merely unclear in its alignment
with — prior material, the CMS wins unless the user explicitly says otherwise.
```

---

## R-AUTH-03 — CMS ambiguity stops work (do not guess)

**Decision:** CONFIRM with modification (version-neutral wording).

**Reviewer comment:** Keep exactly this intent — it is one of the load-bearing safety
rules. Version-neutralise only.

**Suggested wording:**
```text
If the latest prevailing CMS is itself silent, ambiguous, or incomplete on an
implementation point, stop and ask. Do not guess and do not improvise — especially on
anything touching correctness, locking, ownership, or the fmParallel path.
```

---

## R-AUTH-04 — CMS06.x-or-earlier design/decisions/state are historical only

**Decision:** CONFIRM with modification (the user's wording, adopted; then narrowed per
coder review so it does not accidentally exclude reviewed process rules).

**Reviewer comment:** The user's clarification correctly broadens "CMS06.x" to
"CMS06.x-or-earlier" and makes the never-carry-forward intent explicit. The coder then
flagged — correctly — that "never carry forward … decisions, assumptions, designs, or
code" read literally would also prohibit carrying forward the reviewed *process* rules
that legitimately came from prior work (comment Rule 1, before/after Rule 2,
commit-on-PASS, etc.) — which is self-contradictory, since those rules are in this very
register being carried forward. Narrowed so the prohibition targets
cache/design/implementation material (what was actually meant), while reviewed
process/governance rules may carry forward via §3A.

**Suggested wording:**
```text
CMS06.x-or-earlier designs, decisions, assumptions, phase state, proof state, and
decision trail are historical archive only and are not active implementation direction
for the CMS07 restart. To be clear: never carry forward CMS06.x-or-earlier
cache/design/implementation decisions, assumptions, designs, or code as active direction.
Prior process/governance rules may carry forward ONLY if explicitly reviewed and recorded
as register-owned rules in §3A. The only code exception is verified salvage under
R-AUTH-05.
```

---

## R-AUTH-05 — Old code is salvage reference only, and only when verified

**Decision:** CONFIRM with modification (the user's own wording, lightly tightened).

**Reviewer comment:** The user's clarification is good and adopted, with one tightening:
make explicit that salvage is non-cache code, examined and verified safe, AND aligned to
the latest prevailing CMS, AND explicitly approved per case.

**Suggested wording:**
```text
All CMS06.x-or-earlier code is fully superseded. Old code may be re-used only as
verifiable salvage reference, and only when ALL of the following hold: it is explicitly
identified as non-cache-related; it has been examined and verified as safe to re-use; it
is compliant with the latest prevailing CMS; and its specific re-use has been explicitly
approved. (See also R-ARCH-05/06/07 for salvage timing and the CNR2 boundary.)
```

---

## R-PACK-01 — §3A is the durable home for register-owned rules

**Decision:** CONFIRM with modification (add the clarifying definitions the user asked
for).

**Reviewer comment:** The user is right that the rule must say what "register-owned"
means versus what lives in the CMS, so a reader knows what belongs where and why. Wording
below incorporates that.

**Suggested wording:**
```text
The Prevailing Rules Register (§3A), inside the Handover Pack Creation Specification, is
the authoritative durable home for REGISTER-OWNED rules — the standing process,
governance, authority, architecture, and salvage rules that have no home in the CMS.
Design and cache-core rules are NOT register-owned: they live in the latest prevailing
CMS, which is their authoritative home (see R-PACK-03). §3A is where a register-owned
rule becomes active; a rule not recorded in §3A (or, for design rules, in the CMS) is not
controlling.
```

---

## R-PACK-02 — Document A reproduces §3A; on mismatch §3A wins

**Decision:** CONFIRM with modification (adopt the user's "latest version" intent, but
streamline — the user's draft was slightly tangled in places).

**Reviewer comment:** The user correctly wants (a) the latest spec to be the single
source of truth, (b) Document A to be a faithful reproduction, and (c) a reconciliation
step on any mismatch. Streamlined wording below keeps all three without the doubled
sentences.

**Suggested wording:**
```text
The latest version of the Handover Pack Creation Specification (which contains §3A) is
the single source of truth for register-owned rules. Every Document A generated from it
reproduces those register-owned rules for reader convenience. If a Document A disagrees
with §3A, §3A wins; the mismatch is treated as a transcription error and reconciled —
with explicit agreement and approval — before proceeding.
```

---

## R-PACK-03 — CMS-defined rules are handed off, not restated or indexed in §3A

**Decision:** CONFIRM with modification (version-neutral wording — the user's intent).

**Reviewer comment:** Correct and important — this is the anti-duplication rule. Only
version-neutralise.

**Suggested wording:**
```text
Design and cache-core rules already defined in the latest prevailing CMS are not
duplicated, indexed, summarised, or renamed in §3A. The CMS remains their authoritative
home; §3A points to it rather than restating it. Where §3A and the CMS overlap in scope,
the CMS wins.
```

---

## R-PACK-04 — Candidate rules are non-controlling until confirmed

**Decision:** CONFIRM with modification (version-neutral wording — the user's intent).

**Reviewer comment:** Correct. Version-neutralise the design-level destination.

**Suggested wording:**
```text
A remembered or prior-context-derived rule is a candidate only, and is not controlling,
until explicitly confirmed — into §3A if it is register-owned, or into the latest
prevailing CMS if it is design-level.
```

---

## R-PACK-05 — Retired/superseded rule facts are retained, not deleted

**Decision:** CONFIRM with modification (the user's tightening, adopted).

**Reviewer comment:** The user's "cannot and must not silently re-enter" tightening is
good and adopted.

**Suggested wording:**
```text
When a rule is retired or superseded, the fact of that decision is retained in the
register with its status and reason, so that the retired rule cannot and must not
silently re-enter later.
```

---

## R-PACK-06 — Draft-vs-released pack gate

**Decision:** CONFIRM with modification (adopt the user's intent; then corrected per coder
review to match the v2.2 context-from-spec model).

**Reviewer comment:** §3A is defined at the top of this document, so the rule names it
directly. The coder correctly flagged that "may recycle portions of the immediately-prior
Document A as context" now conflicts with the v2.2 model, where the canonical context
lives in the spec's §3.2 and Document A reproduces it verbatim. Corrected so context comes
from the spec, not from recycling the prior Document A.

**Suggested wording:**
```text
A handover pack whose §3A is still pending is a DRAFT only. A handover pack is generated
from the latest version of the Handover Pack Creation Specification; Document A's context
is reproduced from the specification's canonical context section (§3.2), not independently
recycled from a prior Document A. A RELEASED pack requires §3A populated and Document A
regenerated to reproduce both the canonical context and the register-owned rules.
```

---

## R-PACK-07 — A handover pack is a point-in-time snapshot generated from the spec

**Decision:** CONFIRM with significant modification (the rule as written was wrong; the
user correctly identified the conflation; then corrected per coder review to match the
v2.2 context-from-spec model).

**Reviewer comment:** The user is right — the original rule confused "what files are in a
pack" with "what a pack IS." Restated as: a pack is a snapshot of project state, produced
from the latest spec, distinct from companion working material. The coder then flagged the
residual "recycle portions of the immediately-prior Document A" phrase as conflicting with
the v2.2 model; corrected so context is reproduced from the spec's §3.2, and prior
Document A material may only *inform deliberate edits to §3.2*, never be independently
recycled at generation time.

**Suggested wording:**
```text
A handover pack is a point-in-time snapshot of project state, generated from the latest
version of the Handover Pack Creation Specification. Document A's context is reproduced
from the specification's canonical context section (§3.2); prior Document A material may
inform deliberate edits to that canonical context section, but is not independently
recycled during pack generation. The durable pack documents (Document A, Document B, the
CMS, the specification, the coder introduction, the manifest) are distinct from companion
working material supplied alongside for a coding session (e.g. a current .txt code
snapshot, CNR2 reference excerpts, logs). Companion material is not part of the durable
pack and is not governed as a pack document.
```

---

## R-PACK-08 — Released packs are not silently mutated

**Decision:** CONFIRM with modification (the user's addition, adopted).

**Reviewer comment:** Good as the user has it — add that regeneration is from the latest
spec.

**Suggested wording:**
```text
A released handover pack is never silently mutated. To change it, regenerate from the
latest version of the Handover Pack Creation Specification, producing a new manifest and
checksums with an incremented pack version.
```

---

## R-PROCESS-01 — Code comments: concise but never safety-incomplete

**Decision:** CONFIRM with modification (the user's wording, adopted and lightly
polished).

**Reviewer comment:** The user's addition — do not re-word or re-summarise in a way that
loses detail when modifying — is an important catch and is adopted. This directly
counters the lossy-paraphrasing problem.

**Suggested wording:**
```text
Code comments must be clear, reasonably concise, relevant, and genuinely useful to a
human maintainer, but must never over-compress contextual nor safety-critical information such
as locking and threading invariants, ownership and lifetime, reference-count discipline, non-obvious
pre/postconditions, and silent-bug invariants. When modifying code, do not needlessly
re-word or re-summarise existing safety comments in a way that obscures or loses detail. This
obligation includes placing clear explanatory comments above the CR1–CR5 constants; the
substance of CR1–CR5 remains defined in the latest prevailing CMS.
```

---

## R-PROCESS-02 — Code-update instructions: uniquely matchable before/after blocks

**Decision:** CONFIRM with modification (the user's wording, adopted and tightened for
clarity).

**Reviewer comment:** The user is right the original was too loose. The key requirement
is that a human can unambiguously locate the change. Tightened wording below keeps the
user's intent (extra context shown before and after, in both blocks) and removes the
typos.

**Suggested wording:**
```text
Code-update instructions for the human developer/maintainer (e.g. using the latest Visual
Studio) must let a human uniquely and visually locate the change. Each instruction must:
state the file and the function/location; show a BEFORE block and an AFTER block; include
enough surrounding context in BOTH blocks (a few lines immediately before and after the
change) that the location matches uniquely and unambiguously; and show the exact existing
code to be replaced and its exact replacement.
```

---

## R-PROCESS-03 — Phase/SubPhase numbering restarts; naming proposed before coding

**Decision:** CONFIRM with modification (the user's wording, adopted).

**Reviewer comment:** Good — the user's addition that phases may be split/joined/renumbered
mid-development on approval is a useful, realistic clarification and is adopted.

**Suggested wording:**
```text
CMS07 development restarts Phase/SubPhase numbering. The coder proposes the concrete
expanded Phase/SubPhase naming convention before coding, unless the user approves a
different convention. During development, the coder may propose to split, join, remove, or
renumber in-progress and/or subsequent phases based on prevailing circumstances, and does
so only on approval from the managing user.
```

---

## R-PROCESS-04 — PASS responses include a Visual Studio-style commit title/body

**Decision:** CONFIRM with modification (the user's wording, adopted). R-CAND-05 MERGES
here.

**Reviewer comment:** The user's format clarification (single-line title, blank line,
multi-line body; github/Visual Studio compatible) is good and adopted. R-CAND-05 is a
duplicate of this rule and is merged in here, not kept separate. The user's addition —
that a commit message is also produced when it is agreed to move forward on a part-PASS,
a no-PASS, or another condition — is adopted.

**Suggested wording:**
```text
When a development or test step is agreed PASS, provide a suitable commit message unless
the user asks otherwise. The format is github / Visual Studio compatible: a single-line
title, then a blank line, then a multi-line body. Title and body should carry the
relevant summary information about the change. Following an agreement to move forward even
with a part-PASS or a no-PASS or another condition, a commit message should be generated
after that agreement.
```

---

## R-PROCESS-05 — Diagnostic-assessment-as-hard-gate

**Decision:** CONFIRM with modification (the user's wording, adopted and tidied).

**Reviewer comment:** The user's expansion is good — it makes explicit that you actively
check diagnostics after each phase and form a PASS/FAIL view, and that a partial fail is
a FAIL. Adopted. This is a core safety gate; keep it strong.

**Suggested wording:**
```text
After each development/maintenance phase or subphase, the relevant diagnostic information
must be captured and checked, and a PASS or FAIL view formed (a partial fail is a
FAIL and noted as such). Any unexpected or anomalous counter or error value, or
reference-count, prune, validation, recovery-search, or other diagnosed value,
stops progression until it is understood and discussed, and the diagnosis,
rectification, and direction are assessed and agreed.
```

---

## R-PROCESS-06 — Output-authority proof discipline

**Decision:** CONFIRM with modification (version-neutral wording — the user's intent).

**Reviewer comment:** Correct as a process/proof rule (this is why it is register-owned
rather than handed off). Only version-neutralise the pointer.

**Suggested wording:**
```text
Compute, store, return-decision, return-transfer, and output-authority must each be
separately provable. This is a process/proof obligation owned here; the underlying
mechanisms being proven live in the latest prevailing CMS.
```

---

## R-PROCESS-07 — Design Compliance Review run after each phase/coherent block

**Decision:** CONFIRM with modification (version-neutral wording — the user's intent).

**Reviewer comment:** Correct. The obligation-to-run is owned here; the checklist content
stays in the CMS. Version-neutralise.

**Suggested wording:**
```text
Run the Design Compliance Review after each phase or coherent block of work. The
obligation to run it is owned here; the checklist itself is defined in the latest
prevailing CMS and is not duplicated in §3A.
```

---

## R-PROCESS-08 — No file/scope action without explicit instruction

**Decision:** CONFIRM with modification (the user's addition, adopted — it is an
important safety catch).

**Reviewer comment:** The user's addition of "or change any mutex scoping" is a genuinely
important safety catch and is adopted — lock-scope changes are exactly the kind of thing
that must not happen without discussion, consistent with the CMS atomic-scope/firewall
rules. Broadened "instruction" to "discussion, agreement and instruction" as the user
intends.

**Suggested wording:**
```text
Do not rename files, create files, copy salvage code, integrate getFrame, or change any
mutex/lock scoping without explicit user discussion, agreement, and instruction. This
brief is read-understand-propose, not act.
```

---

## R-PROCESS-09 — Layout proposal is text-only until signed off

**Decision:** CONFIRM with modification (the user's wording, adopted and tidied).

**Reviewer comment:** Good. Tidied the typos; kept the user's "at restart commencement"
intent.

**Suggested wording:**
```text
At restart commencement, the coder proposes the file/header/structure layout as text, for
review. No files are created until that layout is explicitly signed off.
```

---

## R-PROCESS-10 — Diagnostic output goes to stderr, not stdout

**Decision:** PROPOSED new — CONFIRM (clean, standalone; no overlap).

**Reviewer comment:** Correct and important — stdout is the data path (vspipe → ffmpeg
etc.), so diagnostics on stdout would corrupt the pipe. Standalone rule; tidied wording.

**Suggested wording:**
```text
All program-generated diagnostic output (e.g. printed diagnostics, progress, summaries)
must go to stderr, never to stdout, because stdout is the frame-data pipe (e.g. vspipe
into ffmpeg). Where practical, flush diagnostic output so the order of events can be
inferred. For clarity, this does not govern the normal frame/data interchange between
this filter and VapourSynth itself.
```

---

## R-PROCESS-11 — Diagnostic formatting: machine-detail allowed, human summaries required

**Decision:** PROPOSED new — CONFIRM. **R-CAND-03 MERGES into this rule.**

**Reviewer comment:** This is the natural home for the diagnostics-formatting topic, and
R-CAND-03 (compact/one-line prints) is the same topic from the other side, so the two are
merged here. The coherent rule: detailed per-event diagnostics may be terse/compact (and
even machine-oriented, since AI can collate them from logs), BUT human-facing summaries —
especially end-of-run tables — must be cleanly formatted and readable. The compact-where-
practical allowance and the human-readable-summary requirement are two halves of one rule,
not a conflict.

**Suggested wording:**
```text
Diagnostics serve two audiences and may be formatted accordingly:
- Detailed/per-event diagnostics may be terse and compact (including single-line records
  where practical to reduce line count), and may be machine-oriented, since automated
  tooling can collate and analyse them from logs. Clarity must still be preserved.
- Human-facing summaries — especially end-of-run summaries — must be well-formatted and
  easily read and interpreted by a human. Any tables must have aligned headings,
  columns, and data, clear labels, and, where applicable or by agreement,
  a short legend and/or explanatory notes.
```

---

## R-PROCESS-12 — Diagnostics are compile-time gated: two named gates, print subordinate to compute

**Decision:** PROPOSED new (renumbered from the duplicate R-PROCESS-10 to resolve the
collision) — CONFIRM. **R-CAND-04 MERGES into this rule (as Part B).**

**Reviewer comment:** This is the compile-time-gating rule. Decisions taken (user
confirmed): 
(1) **two independently-named gates per diagnostic** — a COMPUTE gate and a
PRINT gate, not one shared name; 
(2) both are preprocessor gates (`#if defined`), because
compute and print are foreseen to live in *separate* places (e.g. accumulate during
processing, print a summary later), and `if constexpr` is deemed unsuitable for a separated
print gate (the print code would still have to compile and its data still exist when
printing is off); 
(3) the **print gate is structurally subordinate to the compute gate** —
the print `#define` is nested inside `#if defined(<compute gate>)`, so "print on, compute
off" is impossible to express, not merely detected; 
(4) a defensive `#error` cross-check is
kept even though the structure already makes it unreachable, because a future edit to the
header could break the subordination — the guard then catches that mistake at compile time.
R-CAND-04 (no correctness behind a disabled guard) folds in as Part B. **Part C is net-new
(not from R-CAND-04):** it fences off the legitimate case of a temporary behaviour-changing
scaffold so it cannot be confused with — or hidden behind — a diagnostic gate, keeping
Part B's "diagnostics observe only" invariant absolute.

**Suggested wording:**
```text
Part A — Diagnostics gating (two named compile-time gates; print subordinate to compute):
Each diagnostic has two independently-named preprocessor gates: a COMPUTE gate (wrapping
the code that calculates/accumulates the diagnostic) and a PRINT gate (wrapping the code
that prints/emits it). Both are #if defined(...) gates, so each block is truly included or
excluded from the build regardless of where it sits; the compute and print blocks may live
in different locations. The PRINT gate is defined only inside the COMPUTE gate's block, so
printing can never be enabled unless computing is enabled (compute-on/print-off — "compute
but stay silent" — is allowed and is a normal case). A defensive #error cross-check is
retained to catch any future edit that breaks this subordination. Disabling a gate must
leave the build warning-clean.
Canonical header pattern:
    // config.h — COMMENT OUT a #define to disable that gate.
    #define DIAG_COMPUTE_X 1          // define to CALCULATE diagnostic X
    #if defined(DIAG_COMPUTE_X)
    #   define DIAG_PRINT_X 1         // define to PRINT diagnostic X (subordinate to compute)
    #endif
    // Defensive cross-check: unreachable given the structure above, but retained so a
    // future edit that breaks the subordination fails loudly rather than silently.
    #if defined(DIAG_PRINT_X) && !defined(DIAG_COMPUTE_X)
    #   error "DIAG_PRINT_X requires DIAG_COMPUTE_X - cannot print a diagnostic that isn't computed"
    #endif
Then: the compute block guards on DIAG_COMPUTE_X; the print block guards on DIAG_PRINT_X,
wherever each lives. (Confirm concrete macro naming when proposing the layout.)

Part B — Proof/correctness gating (observation gates observe only):
Code required for CORRECTNESS must never live inside a disabled diagnostic/proof-observation
guard. Enabling or disabling any diagnostic or proof-observation gate must never change
correct program behaviour — a DIAG_* gate may add observation only, never alter behaviour
or results. (No exceptions: if toggling a DIAG_* gate would change behaviour, it is
misused — the behaviour-changing part belongs under Part C, not behind an observation gate.
A behaviour-changing scaffold must use the Part C SCAFFOLD_* markers, never a DIAG_* name.)

Part C — Temporary behavioural scaffolds (NOT diagnostic gates):
A change that deliberately alters behaviour for a phase/subphase (e.g. forcing a path,
injecting a stub, disabling an optimisation to expose a defect) is NOT a diagnostic gate
and must not use a DIAG_* name. It is permitted only when ALL of the following hold:
  - it is clearly described in the subphase proposal, with the behaviour change and its
    rationale stated explicitly;
  - it carries a standard searchable marker so every active scaffold is found with a
    single search — both a comment tag `// BEHAVIOURAL-SCAFFOLD:` AND a macro named with a
    `SCAFFOLD_*` prefix (the exact token set is confirmed when the layout is proposed, but
    one agreed convention is used everywhere — coders/sessions must not invent variants);
  - it is recorded as an outstanding behaviour-fixing action to be unwound at the earliest
    safe point;
  - the outstanding actions are reviewed at every subphase to see whether it is yet safe
    to unwind them;
  - if any remain outstanding at the end of a phase, a discussion occurs to agree when
    each will be fixed (rescheduling is permitted by discussion and agreement).
A behavioural scaffold must never silently persist.

First-milestone restriction: during the first cache-core milestone (proving the cache
manager in isolation), behavioural scaffolds are DISALLOWED unless explicitly agreed per
case — the proving phase is kept free of behaviour-altering scaffolds so the core is
proven on its real behaviour.
```

---

## R-PROCESS-13 — No printing or long-running actions inside atomic/locked scopes

**Decision:** PROPOSED new — CONFIRM (clean; complements the CMS atomic-scope discipline).

**Reviewer comment:** Correct and safety-relevant — it reinforces the CMS atomic-scope
rules from the process side. Tidied wording.

**Suggested wording:**
```text
Do not perform printing (e.g. diagnostics) or any long-running action inside an atomic /
locked scope. Atomic scopes are specifically defined by the designer (see the latest
prevailing CMS atomic-scope register); only the minimum — but all necessary — processing
occurs within them. Variables and data structures (including diagnostic counters) may be
manipulated inside an atomic scope, but any long-running action not already specified by
the designer requires prior discussion and approval before coding.
```

---

## R-PROCESS-14 — Minimal unrelated-change discipline (promoted from R-CAND-02)

**Decision:** CONFIRM (promoted from candidate R-CAND-02 to a full process rule, per coder
review; the user had already upgraded R-CAND-02 to CONFIRMED).

**Reviewer comment:** Worth confirming as a full rule — it directly protects against churn
and against the lossy-rewrite hazard this very review exposed. (R-CAND-02 is retired as a
candidate, superseded by this entry.)

**Suggested wording:**
```text
Do not rename, reformat, or change unrelated code, comments, layout, or names unless it
is required for correctness or has been explicitly agreed. Minimise diffs to what the
change actually needs.
```

---

## R-PROCESS-15 — ASCII-safe code-update text (promoted from R-CAND-01)

**Decision:** CONFIRM as a "should" rule (promoted from candidate R-CAND-01, per coder
review; the user had already upgraded R-CAND-01 to CONFIRMED). The coder's fuller wording
is adopted over the terser candidate text.

**Reviewer comment:** Practical for Windows / Visual Studio / diff workflows. Kept as a
"should" (not "must") so an explicitly-approved need can use non-ASCII. (R-CAND-01 is
retired as a candidate, superseded by this entry.)

**Suggested wording:**
```text
Code-update instructions, replacement code, comments, macro names, file names, and commit
messages should use ASCII-only text unless the existing source or an explicitly approved
requirement needs otherwise. This reduces copy/paste, compiler, editor, shell, and diff
ambiguity. Non-ASCII prose may be used in discussion if it is not intended for direct
source-code insertion.
```

---

## R-PROCESS-16 — No silent rule/title paraphrase when propagating rules

**Decision:** PROPOSED new — CONFIRM (coder-proposed; strongly endorsed).

**Reviewer comment:** This is the most important of the new process rules — it
operationalises the exact failure mode that triggered this whole review (lossy
re-paraphrasing). Currently the hazard is only stated as a warning in the general comment;
making it an enforceable rule is right.

**Suggested wording:**
```text
When moving an agreed rule into §3A, Document A, an introduction, a handover, or a later
regeneration, do not paraphrase it in a way that weakens, broadens, narrows, or changes
its operational meaning. If wording is changed for clarity, preserve the exact safety
intent and flag any substantive change for review.
```

---

## R-PROCESS-17 — Explicit override discipline (no silent exceptions)

**Decision:** PROPOSED new — CONFIRM (coder-proposed; accepted as a general backstop).

**Reviewer comment:** A general "no silent exceptions" rule. The substance is implied in
pieces (R-PROCESS-08, R-PROCESS-12 Part C), but a single clear backstop is useful and the
pieces do not cover every case. Accepted as a general rule that the specific ones then
reinforce.

**Suggested wording:**
```text
Any intentional departure from an active rule, CMS requirement, approved phase plan, or
agreed coding convention requires explicit discussion, agreement, and documentation before
implementation. Silent exceptions are not permitted.
```

---

## R-PROCESS-18 — Partial-PASS / conditional-progression recording

**Decision:** PROPOSED new — CONFIRM (coder-proposed; complements R-PROCESS-04/05).

**Reviewer comment:** Fills a real gap: R-PROCESS-04 requires a commit message after
conditional progression but does not require the *condition itself* to be recorded so
later work does not mistake it for a clean PASS. Pairs naturally with the R-PROCESS-12
Part C tracking discipline.

**Suggested wording:**
```text
If work proceeds after a part-PASS, no-PASS, waiver, temporary exception, or conditional
agreement, the condition and reason must be recorded in the phase notes, handover notes,
or commit body as appropriate, so later work does not mistake it for an unconditional PASS.
```

---

## R-ARCH-01 — Reuse the existing per-frame processing boundary (no parallel pixel algorithm)

**Decision:** CONFIRM with significant modification. The original wording was unsafe and
is replaced.

**Reviewer comment:** This is the rule the user correctly flagged as extremely unsafe.
Two faults in the original: (1) it invoked "warm-up", a concept the CMS two-phase
recovery model replaced — recovery is now a descending search for the nearest present
output followed by an ascending fill-of-holes, with no separate warm-up phase; (2) it was
silent about the fmParallel end-goal, leaving room to read it as licence to build
sequential helper machinery. The rule's *useful* core is narrow and worth keeping: when
filling a genuine hole during recovery, reuse the ONE existing per-frame processing
function rather than writing a second, parallel pixel/frame algorithm. Restated below to
say exactly that and nothing more, with the fmParallel guard explicit.

**Suggested wording:**
```text
When recovery needs to compute a genuinely missing output ("a hole"), it must reuse the
single existing per-frame processing path — it must NOT introduce a second or parallel
pixel/frame algorithm, and must NOT reintroduce any sequential/serialized predecessor
mechanism. (There is no separate "warm-up" phase: recovery is the CMS two-phase model —
descending search for the nearest present output, then ascending fill-of-holes-only.)
Nothing in recovery or hole-filling may obstruct the fmParallel end-goal; any unavoidable
temporary stepping-stone toward it must be explicitly recorded as such, must be slated for
removal, and must not compromise the design path to fmParallel. The recovery model this
rule refers to is the one defined in the latest prevailing CMS.
```

---

## R-ARCH-02 — Pixel/frame processing performs no cache or scheduling actions

**Decision:** CONFIRM with modification (the user's intent — strengthen to "no cache
actions").

**Reviewer comment:** The user asked that it explicitly say the pixel layer must not
perform cache-related actions. Adopted and made concrete. (The file-layout material the
user sketched here has been moved to GUIDANCE-ARCH-01, since it is layout guidance, not a
behavioural boundary, and the concrete layout is the coder's proposal under the latest
prevailing CMS §11.)

**Suggested wording:**
```text
The pixel/frame-processing layer performs pixel-layer work only. It must not own or
perform any cache, recovery, request, pinning, eviction, checkpoint, hot-zone, or
scheduling action, and must not read or mutate cache state. Such actions belong
solely to the cache manager.
```

---

## R-ARCH-03 — Cache manager owns cache state and policy, not the pixel algorithm

**Decision:** CONFIRM with modification (restore the items the user noticed were missing).

**Reviewer comment:** The user correctly noticed the original dropped response tables and
memory diagnostics from the picture. The cleanest fix is to state the cache manager's
scope and exclusions clearly, and let R-ARCH-04 carry the cache-independent utilities
(response tables, memory diagnostics). Wording below restores the completeness.

**Suggested wording:**
```text
The cache manager owns cache state and policy: pools, the ordered index, pins and the
pin-list, hot zones, checkpoint flags, store/prune, recovery planning, validation, and
cache diagnostics. It must not contain pixel-algorithm logic. (Cache-independent
utilities — response-table construction and memory diagnostics — are not part of the
cache manager; see R-ARCH-04.)
```

---

## R-ARCH-04 — Response tables and memory diagnostics are cache-independent utilities

**Decision:** CONFIRM with modification (minor clarity; the user left this blank, which I
read as broadly content).

**Reviewer comment:** Sound rule; only made the boundary explicit so it pairs cleanly with
R-ARCH-03.

**Suggested wording:**
```text
Response-table construction and memory diagnostics are cache-independent utilities. They
sit outside both the pixel-algorithm layer and the cache manager, and must not be made to
depend on cache state unless explicitly redesigned and agreed.
```

---

## R-ARCH-05 — Salvage is the second step only

**Decision:** CONFIRM (no change needed).

**Reviewer comment:** Correct and clear as written. Salvage happens only after the new
cache core is proven in isolation. No change.

**Suggested wording:**
```text
Salvage from old .txt code happens only after the new cache-core
ownership/pinning/eviction discipline is proven in isolation.
```

---

## R-ARCH-06 — CNR2 pixel maths may be salvaged; its recovery logic must not

**Decision:** CONFIRM with minor modification (make the boundary unmissable).

**Reviewer comment:** Correct and important. Strengthened slightly so the
"pixel-maths-yes / recovery-logic-no" boundary cannot be misread.

**Suggested wording:**
```text
CNR2 (the AviSynth vsCnr2 source) may be used as guidance for PIXEL maths only —
response-table construction, the int64-accumulator weighted blend, downsampled-luma, and
in-compute scene-change detection. Its serialized recovery/predecessor shortcut
(substituting source[n-1] when the previous output is absent) must NEVER be adopted; that
approximation is exactly what the CMS cache-and-recovery architecture exists to replace.
```

---

## R-ARCH-07 — Old .txt code is not copied into new files without per-case approval (emphasised during the first milestone)

**Decision:** CONFIRM with modification (strip the leaked review-note from the wording;
broaden beyond the first milestone while keeping first-milestone emphasis).

**Reviewer comment:** Clear and correct. Consistent with R-AUTH-05 and R-ARCH-05/06. The
principle (no old .txt code copied into new files without explicit per-case approval) is a
standing salvage-discipline rule, not limited to the first milestone — broadened so it does
not accidentally read as "after milestone one, copy freely," with the first milestone
called out as the period of strictest application.

**Suggested wording:**
```text
Old .txt code is not copied into new .h/.cpp files without explicit per-case approval.
This applies throughout development; it is enforced most strictly during the first
cache-core milestone, where the new core is being proven in isolation.
```

---

# Layout guidance (NOT authoritative rules)

The following is GUIDANCE, deliberately kept out of the R- rule register so it is never
read as a binding rule. It is recorded as `GUIDANCE-ARCH-01` rather than `R-ARCH-NN`
precisely so future coders do not treat illustrative filenames as mandatory.

## GUIDANCE-ARCH-01 — Indicative file separation (the coder confirms the actual layout)

**Status:** GUIDANCE, not a rule. The concrete file/header layout is the coder's proposal
under the latest prevailing CMS §11, proposed as text and signed off before any files are
created (R-PROCESS-09). The names below are illustrative; the coder confirms the actual
names. This guidance carries the file-layout material the user originally sketched in
R-ARCH-02; it was moved here (per coder review) so it cannot be mistaken for an
authoritative rule.

**Guidance text:**
```text
This is INDICATIVE guidance on file separation, not a fixed layout.

To encourage separation of concerns, each of the following will likely reside in its own
`.cpp`:
  - the pixel/frame-processing layer;
  - the cache manager;
  - the memory diagnostics.

Likely there is also:
  - a common global header (illustrative name `cnr3_common.h`) for common imports and
    shared definitions. (OPEN QUESTION for the coder: what exactly belongs in this common
    header versus the per-concern files — to be settled in the layout proposal.)
  - a separate global build-configuration header (illustrative name `cnr3_build_config.h`)
    holding the controlling `#if defined` gate blocks per R-PROCESS-12.

The VapourSynth headers `VapourSynth4.h` and `VSHelper4.h` remain separate, as copied
from the VapourSynth sources (not merged into project headers).
```

---

# Retired-fact entries (anti-regression record)

These record the *fact* that an old mechanism was considered and dropped, so it cannot
silently return. They are register-owned (the fact lives nowhere in the CMS; the CMS
states the replacement). All are low-risk and broadly correct as proposed; the only
systemic fix is version-neutral wording for the "replacement lives in the CMS" pointer.

## R-RETIRED-01 — Deferred/optional non-checkpoint pinning is superseded

**Decision:** CONFIRM (record as superseded).
**Reviewer comment:** Correct. Replacement (mandatory consumer-pinning) lives in the
latest prevailing CMS.
**Suggested wording:**
```text
The old model in which non-checkpoint pinning was deferred or emergency-only is completely
superseded by mandatory consumer-pinning, as defined in the latest prevailing CMS.
```

---

## R-RETIRED-02 — Held-ref-only predecessor reservation is superseded

**Decision:** CONFIRM (record as superseded).
**Reviewer comment:** Correct. Replacement (consumer-held pins on per-invocation
pin-lists) lives in the CMS.
**Suggested wording:**
```text
Held-ref-only predecessor reservation as the default architecture is completely superseded by
consumer-held pins on per-invocation pin-lists, as defined in the latest prevailing CMS.
```

---

## R-RETIRED-03 — Checkpoint-as-pin reasoning is retired

**Decision:** CONFIRM (record as retired/superseded).
**Reviewer comment:** Correct and worth keeping prominent — checkpoint-as-pin was a
recurring confusion. A checkpoint is a separate eviction-protection flag.
**Suggested wording:**
```text
Any reasoning or wording that treats a checkpoint as a pin is completely retired. A checkpoint
is now a separate eviction-protection flag with its own retention rule; there is exactly one pin
concept (consumer-claim), as defined in the latest prevailing CMS.
```

---

## R-RETIRED-04 — Hot-zone-as-active-findability guarantee is superseded

**Decision:** CONFIRM with minor modification (record as superseded).
**Reviewer comment:** Correct. Pins provide active liveness; hot zones are prune-policy
hints. Replacement lives in the CMS.
**Suggested wording:**
```text
The model in which hot zones guaranteed active-frame findability is superseded. Instead, pins
provide active liveness; hot zones are now prune-policy hints only, as defined in the latest
prevailing CMS.
```

---

## R-RETIRED-05 — Blanket bounded-warmup source window is superseded

**Decision:** CONFIRM (record as superseded).
**Reviewer comment:** Correct, and ties to the R-ARCH-01 fix — the blanket backward
window AND the "warm-up" framing are both gone, replaced by the dissolved source-window
model.
**Suggested wording:**
```text
The old blanket backward source-request window (and its "bounded-warmup" framing) is
completely superseded and is replaced by the dissolved source-window model — request
source N plus genuine holes only — as defined in the latest prevailing CMS.
```

---

## R-RETIRED-06 — CMS06.x / H15.6B is not an active continuation path

**Decision:** CONFIRM (record as retired active work item).
**Reviewer comment:** Correct. This is a restart; the old work item is not continued.
**Suggested wording:**
```text
CMS06.x / H15.6B coding is completely discontinued as an active path, in and beyond the
CMS07 restart. It is recorded here as a completely retired work item to prevent accidental
resumption.
```

---

## R-RETIRED-07 — Old strict-streaming bridge is not final fmParallel output authority

**Decision:** CONFIRM (record as superseded/retired as final authority).
**Reviewer comment:** Correct. The old next_needed / prev_output-style streaming bridge
is not the final output authority under the fmParallel-targeted design.
**Suggested wording:**
```text
The old strict-streaming bridge (including next_needed / prev_output-style authority) is
not the final fmParallel output authority and is recorded here as completely superseded/retired
in that role.
The authoritative model now lives in the latest prevailing CMS.
```

---

# Candidate rules (non-controlling until confirmed)

These are remembered preferences. They stay marked CANDIDATE until you explicitly confirm
each. None should be treated as controlling yet.

## R-CAND-01 — Prefer ASCII-only code-update instructions — PROMOTED to R-PROCESS-15

**Decision:** PROMOTED to R-PROCESS-15 (no longer a candidate).
**Reviewer comment:** Confirmed and promoted to a full process rule (R-PROCESS-15), with
the coder's fuller wording. Retained here as a retired-candidate record so it cannot
silently re-enter as a separate item.
**Suggested wording:** (see R-PROCESS-15.)

---

## R-CAND-02 — Avoid unnecessary unrelated code/comment/layout/name changes — PROMOTED to R-PROCESS-14

**Decision:** PROMOTED to R-PROCESS-14 (no longer a candidate).
**Reviewer comment:** Confirmed and promoted to a full process rule (R-PROCESS-14).
Retained here as a retired-candidate record so it cannot silently re-enter as a separate
item.
**Suggested wording:** (see R-PROCESS-14.)

---

## R-CAND-03 — Large diagnostic prints may be one line where practical — MERGED into R-PROCESS-11

**Decision:** MERGE WITH R-PROCESS-11 (resolved — do not keep as a separate candidate).

**Reviewer comment:** Resolved your query. This is the same topic as R-PROCESS-11
(diagnostic formatting), so it is merged there: R-PROCESS-11 now states both halves —
detailed/per-event diagnostics may be terse/compact (your one-line allowance), while
human-facing summaries and tables must be cleanly formatted. Nothing is lost; it is just
no longer a separate rule.

**Suggested wording:** (folded into R-PROCESS-11 above.)

---

## R-CAND-04 — Compile-time constexpr proof gates; no correctness behind disabled guards — MERGED into R-PROCESS-12

**Decision:** MERGE WITH R-PROCESS-12 (resolved — do not keep as a separate candidate).

**Reviewer comment:** Resolved your query. This is the correctness side of the same
compile-time-gating mechanism as R-PROCESS-12, so it is folded in there as Part B: code
required for correctness must never live behind a disabled diagnostic/proof guard, and
enabling/disabling a gate must never change correct behaviour. (Mechanism per R-PROCESS-12:
two named preprocessor gates with the print gate subordinate to the compute gate — so this
candidate's original bare-`constexpr` framing is superseded by the agreed `#if defined`
approach.) Nothing is lost; it is now one coherent gating rule.

**Suggested wording:** (folded into R-PROCESS-12, Part B, above.)

---

## R-CAND-05 — PASS response includes commit message — MERGED into R-PROCESS-04

**Decision:** MERGE WITH R-PROCESS-04 (do not keep as a separate rule).
**Reviewer comment:** This duplicates R-PROCESS-04 (commit message on PASS) and is merged
there. (Your cross-reference query mentioning the gating rule looked like a copy-paste
slip — R-CAND-05 is purely the commit-message duplicate and has nothing to do with
diagnostics gating, so there is no further overlap to resolve here.)

**Suggested wording:** (folded into R-PROCESS-04 above.)

---
