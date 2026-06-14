# CNR3 Register-Owned Rules Review — Reconciled

**Purpose:** Reconciled review of the proposed register-owned rules before populating
Production Spec §3A. This version makes every decision, states each comment in plain
human terms, and supplies replacement wording where a rule needs it.
**Status:** Draft review aid only. Not yet §3A. For the user's review — NOT to be handed
to the coder as-is.
**Date:** 2026-06-13

---

## Reviewer general comment (read first)

Three systemic problems run through the coder's proposed list. They should be fixed
across ALL rules, not just the ones called out individually below.

1. **Lossy re-paraphrasing.** Many rules were restated in broader, vaguer language than
   we had agreed over the prior development. The effect is rules that "sound about
   right" but have lost the precision that made them safe. The fix throughout is to pull
   each rule back to its specific, operationalisable intent — a human reading it should
   not be able to construe it unsafely.

2. **Version-pinning trap.** Several rules name "CMS07.0" as if that specific version is
   permanently authoritative. The standing intent is **the latest prevailing CMS**
   (CMS07.0 *or its later approved successor*). Every reference to the controlling design
   should read as "the latest prevailing CMS" — EXCEPT specific section pointers (e.g.
   "§9A.2"), which are version-specific and must be re-checked against a successor, and
   historical statements about what a version superseded, which stay pinned to that
   version.

3. **Handover-pack vs its specification confused.** Some rules muddle "the handover pack"
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

**Definition used throughout (for clarity):**
- **Register-owned rule** — a standing process / governance / authority / architecture /
  salvage rule that has NO home in the CMS design document, so the §3A register is its
  authoritative home.
- **CMS-defined (handed-off) rule** — a design / cache-core / reference-count / recovery /
  constant / instrumentation rule that lives in the latest prevailing CMS; §3A does NOT
  restate, index, or rename it.
- **§3A** — the Prevailing Rules Register section inside the Handover Pack Creation
  Specification; the durable, authoritative home for register-owned rules.

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

**Decision:** CONFIRM with modification (the user's own wording, adopted).

**Reviewer comment:** The user's clarification is good and is adopted. It correctly
broadens "CMS06.x" to "CMS06.x-or-earlier" and makes the never-carry-forward intent
explicit, with the single salvage exception.

**Suggested wording:**
```text
CMS06.x-or-earlier designs, decisions, assumptions, phase state, proof state, and
decision trail are historical archive only and are not active implementation direction
for the CMS07 restart. To be clear: never carry forward CMS06.x-or-earlier decisions,
assumptions, designs, or code (the only exception is verified salvage under R-AUTH-05).
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

**Decision:** CONFIRM with modification (adopt the user's intent; §3A now defined above
so the user's uncertainty is resolved).

**Reviewer comment:** The user worked around not being sure what §3A was — it is now
defined at the top of this document, so the rule can name it directly. The user's point
that a pack is generated from the latest spec (optionally recycling the prior Document A)
is correct and folded in.

**Suggested wording:**
```text
A handover pack whose §3A is still pending is a DRAFT only. A handover pack must be
generated using the latest version of the Handover Pack Creation Specification (it may
recycle portions of the immediately-prior Document A as context). A RELEASED pack
requires §3A populated and Document A regenerated to reproduce the register-owned rules.
```

---

## R-PACK-07 — A handover pack is a point-in-time snapshot generated from the spec

**Decision:** CONFIRM with significant modification (the rule as written was wrong; the
user correctly identified the conflation).

**Reviewer comment:** The user is right — the original rule confused "what files are in a
pack" with "what a pack IS." Restated to capture the correct meaning: a pack is a
snapshot of project state, produced from the latest spec, distinct from companion working
material. This preserves the genuinely useful core/companion distinction without the
conflation.

**Suggested wording:**
```text
A handover pack is a point-in-time snapshot of project state, generated from the latest
version of the Handover Pack Creation Specification (and may recycle portions of the
immediately-prior Document A). The durable pack documents (Document A, Document B, the
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
duplicate of this rule and should be merged in, not kept separate.

PLEASE REFER TO R-CAND-05 to see if these can be cleaned up somehow.

**Suggested wording:**
```text
When a development or test step is agreed PASS, provide a suitable commit message unless
the user asks otherwise. The format is github / Visual Studio compatible: a single-line
title, then a blank line, then a multi-line body. Title and body should carry the
relevant summary information about the change. Following an agreement to move forward even
with a part-PASS or a no PASS or another condition, a commit message should generated
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
stops progression until it is understood and discussed and the 
diagnostic/rectification/direction is assessed and agreed.
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
## R-PROCESS-10 — All diagnostic output goes to stderr

**Decision:** PROPOSED new.

**Reviewer comment:** outputs going to stdout interferes with vspipe piping into ffmpeg etc.

**Suggested wording:**
```text
All outputs such as printed diagnostic information must go to stderr and not to stdout.
Wherever possible, print should be flushed so that order of occurrence may be inferred.
For clarity, this specifically does not apply to calls/information exchanged
between this program and vapoursynth itself.
```

## R-PROCESS-11 — Human readable diagnostics

**Decision:** PROPOSED new.

**Reviewer comment:** not all diagnostics need to be easily readable as AI may more easily find/collate some
                        however summaries (eg at the end) must be well-formatted and easily
                        read and interpreted by humans. All tables nust have lined-up headings
                        and column data and be well llabeleld and in some cases have relevant legends/comments.

NOW REFER TO R-CAND-03 for potential cleanup somehow.

**Suggested wording:**
```text
Although preferable, not all diagnostics need to be easily readable by humans as AI may more
easily find/collate some categories of diagnostics in logs and analyse and report, however
summaries (eg at the end) must be well-formatted and easily read and interpreted by humans.
All tables must have well-aligned headings and column data and be well labeleld and in some
cases have relevant legends/comments.
```

## R-PROCESS-10 — All diagnostic must be compile-time gated

**Decision:** PROPOSED new.

**Reviewer comment:** 

Code for diagnostics must be hard gated so that at compile-time it does not end up as 
executable code unless intentionally enabled; further, associated diagnostic printing must be
secondarily gated so that diagnostic information may be collected/computed but the
associated printing of it can be enabled or disabled.
It is foreseeable that one or several diaganostics may be associated with a specific feature or action
or proving test, and as such a hard gate pair (one for computing diagnostics and one for the
associated printing of those diagnostics) may surround several compute-blocks and print-blocks;
never enable a print-block without its associated-compute block unless the data used in the
print block is computed differently/elsewhere (eg as a paert of non-diagnostics code).
Perhaps `static constexpr` and/or `inline constexpr` in a global `.h` can be used to facilitate this
although Google tells me we may need to adopt a different approach:
    To prevent blocks of code from being compiled into your executable based on a global
    configuration header, neither static `constexpr` nor `inline constexpr` should be used alone
    to surround code blocks. Instead, you must use preprocessor macros (#if / #endif) combined
    with these constants.
    How to Prevent Code Compilation:
        C++ statements like if constexpr do not stop code from being compiled into the executable;
        they only stop it from executing. The compiler still parses and compiles the code block,
        and it must be syntactically valid. To completely exclude code from compilation across
        different modules, you must use preprocessor macros.
        Approach 1: Preprocessor Macros (True Exclusion)
            This is the only way to completely prevent code from being compiled or checked
            by the compiler in that build configuration.
            config.h
                #pragma once
                // Comment this line out to completely exclude the feature
                #define ENABLE_FEATURE_A 1 
                // Use code with caution.
            module.cpp
                #include "config.h"
                void process() {
                #if defined(ENABLE_FEATURE_A)
                    // This block is compiled ONLY if ENABLE_FEATURE_A is defined
                    run_heavy_feature();
                #endif
                }
        Approach 2: if constexpr (Dead Code Elimination)
            If you want to use C++ constants instead of macros, you can use if constexpr.
            The compiler will still compile the code, but if the condition is false,
            the compiler optimizer will strip the dead code out of the final executable binary.
            Note: For if constexpr to work, run_heavy_feature() must still exist and be syntactically valid, even if enable_feature_a is false.
            config.h
                #pragma once
                // inline constexpr is preferred in C++17 to avoid duplicate instances
                inline constexpr bool enable_feature_a = true; 
            module.cpp
                #include "config.h"
                void process() {
                    if constexpr (enable_feature_a) {
                        // Compiled, but stripped from binary if enable_feature_a is false
                        run_heavy_feature(); 
                    }
                }
If do not know if/how nested #if defined works/
Please help: how to choose which approach and then encapsulate in a rule ? Probably Approach 1 as a guess.

NOW REFER TO R-CAND-04 for potential cleanup somehow.

**Suggested initial wording subject to reviewing and choosing an approach:**
```text
Code for diagnostics must be hard gated so that at compile-time it does not end up as 
executable code unless intentionally enabled; further, associated diagnostic printing must be
secondarily gated so that diagnostic information may be collected/computed but the
associated printing of it can be enabled or disabled.
It is foreseeable that one or several diaganostics may be associated with a specific feature or action
or proving test, and as such a hard gate pair (one for computing diagnostics and one for the
associated printing of those diagnostics) may surround several compute-blocks and print-blocks;
never enable a print-block without its associated-compute block unless the data used in the
print block is computed differently/elsewhere (eg as a paert of non-diagnostics code).
```

## R-PROCESS-13 — Do not perform print and long-running actions etc inside atomics

**Decision:** PROPOSED new.

**Reviewer comment:** atomics are specififcally scoped and the minimum (but all necessary) processing should occur within them

**Suggested wording:**
```text
Do not perform print (eg for diagnostics) within atomics; all atomics are specifically
scoped by the designer and minimum (but all necessary) processing should occur within them.
Variables and data structures etc (eg including for diagnostics) may be maniplulated within
atomics however long-running actions not specified by the designer individually require prior
discussion and approval to proceed before coding.
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
removal, and must not compromise the design path to fmParallel.
```

---

## R-ARCH-02 — Pixel/frame processing performs no cache or scheduling actions

**Decision:** CONFIRM with modification (the user's intent — strengthen to "no cache
actions").

**Reviewer comment:** The user asked that it explicitly say the pixel layer must not
perform cache-related actions. Adopted and made concrete.

**Suggested wording:**
```text
The pixel/frame-processing layer performs pixel-layer work only. It must not own or
perform any cache, recovery, request, pinning, eviction, checkpoint, hot-zone, or
scheduling action, and must not read or mutate cache state. Such actions belong solely to
the cache manager.
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
(Confirm as written.) Salvage from old .txt code happens only after the new cache-core
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

## R-ARCH-07 — No old .txt code copied into new files during the first milestone without approval

**Decision:** CONFIRM (no change needed).

**Reviewer comment:** Clear and correct. Consistent with R-AUTH-05 and R-ARCH-05. No
change.

**Suggested wording:**
```text
(Confirm as written.) During the first cache-core milestone, old .txt code is not copied
into new .h/.cpp files without explicit per-case approval.
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
is a separate eviction-protection flag with its own retention rule; there is exactly one pin
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
provide active liveness; hot zones are prune-policy hints only, as defined in the latest
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
completely superseded by the dissolved source-window model — request source N plus
genuine holes only — as defined in the latest prevailing CMS.
```

---

## R-RETIRED-06 — CMS06.x / H15.6B is not an active continuation path

**Decision:** CONFIRM (record as retired active work item).
**Reviewer comment:** Correct. This is a restart; the old work item is not continued.
**Suggested wording:**
```text
CMS06.x / H15.6B coding is completely discontinued as an active path in and beyond the
CMS07 restart. It is recorded here as a retired work item to prevent accidental resumption.
```

---

## R-RETIRED-07 — Old strict-streaming bridge is not final fmParallel output authority

**Decision:** CONFIRM (record as superseded/retired as final authority).
**Reviewer comment:** Correct. The old next_needed / prev_output-style streaming bridge
is not the final output authority under the fmParallel-targeted design.
**Suggested wording:**
```text
The old strict-streaming bridge (including next_needed / prev_output-style authority) is
not the final fmParallel output authority. It is recorded here as superseded/retired in
that role; the authoritative model lives in the latest prevailing CMS.
```

---

# Candidate rules (non-controlling until confirmed)

These are remembered preferences. They stay marked CANDIDATE until you explicitly confirm
each. None should be treated as controlling yet.

## R-CAND-01 — Prefer ASCII-only code-update instructions

**Decision:** CONFIRMED (lean: confirm later as a low-cost hygiene rule).
**Reviewer comment:** Reasonable, low-risk preference (avoids editor/compiler/copy-paste
issues with non-ASCII). Recommend confirming, but it stays candidate until you say so.
**Suggested wording (if confirmed):**
```text
Code-update instructions should prefer ASCII-only text, to avoid editor, compiler, and
copy-paste issues with non-ASCII characters.
```

---

## R-CAND-02 — Avoid unnecessary unrelated code/comment/layout/name changes

**Decision:** CONFIRMED (lean: confirm — this is a valuable discipline).
**Reviewer comment:** This is worth confirming; it directly protects against churn and
against the lossy-rewrite problem seen in this very review. Stays candidate until you
confirm.
**Suggested wording (if confirmed):**
```text
Do not rename, reformat, or change unrelated code, comments, layout, or names unless it
is required for correctness or has been explicitly agreed. Minimise diffs to what the
change actually needs.
```

---

## R-CAND-03 — Large diagnostic prints may be one line where practical

**Decision:** ???? KEEP AS CANDIDATE (lean: confirm as a soft preference, not a hard rule).
**Reviewer comment:** Fine as a preference; should not be a hard rule. Stays candidate.

QUERY:
CAN WE COMBINE THIS OR SOMEHOW OTHERWISE MAKE USEFUL SEPARATE RULES WHEN LOOKING
AT THIS IN CONJUCTION WITH  R-PROCESS-11 — Human readable diagnostics
I LOOK TO TO DECIDE WHAT TO DO AND HOW TO DO IT.

**Suggested wording (if confirmed):**
```text
Large diagnostic prints may be formatted on a single line where practical to reduce line
count, provided clarity is preserved.

```

---

## R-CAND-04 — Compile-time constexpr proof gates; no correctness behind disabled guards

**Decision:** KEEP AS CANDIDATE (lean: confirm — this is a genuine safety discipline).
**Reviewer comment:** This one matters more than the other candidates: it is a real safety
rule (correctness must never live inside a disabled debug/proof guard). Recommend
confirming. Note it pairs with the diagnostics gate (R-PROCESS-05). Stays candidate until
you confirm.

QUERY:
CAN WE COMBINE THIS OR SOMEHOW OTHERWISE MAKE USEFUL SEPARATE RULES WHEN LOOKING
AT THIS IN CONJUCTION WITH  R-PROCESS-10 — All diagnostic must be compile-time gated
I LOOK TO TO DECIDE WHAT TO DO AND HOW TO DO IT.


**Suggested wording (if confirmed):**
```text
Proof scaffolding uses compile-time constexpr gates. Code required for correctness must
never sit inside a disabled proof/debug guard — disabling diagnostics must never change
correct behaviour.
```

---

## R-CAND-05 — PASS response includes commit message — MERGE into R-PROCESS-04

**Decision:** MERGE WITH R-PROCESS-04 (do not keep as a separate rule).
**Reviewer comment:** This duplicates R-PROCESS-04. Merge it in there and drop the
separate entry, as already flagged.

QUERY:
CAN WE COMBINE THIS OR SOMEHOW OTHERWISE MAKE USEFUL SEPARATE RULES WHEN LOOKING
AT THIS IN CONJUCTION WITH  R-PROCESS-10 — All diagnostic must be compile-time gated
I LOOK TO TO DECIDE WHAT TO DO AND HOW TO DO IT.


**Suggested wording:** (folded into R-PROCESS-04 above.)

---

# Summary of decisions

```text
CONFIRM as written (no change):            R-ARCH-05, R-ARCH-07
CONFIRM with modification:                 R-AUTH-01..05, R-PACK-01..06, R-PACK-08,
                                           R-PROCESS-01..09, R-ARCH-02..04, R-ARCH-06,
                                           all R-RETIRED-01..07
CONFIRM with SIGNIFICANT modification:     R-PACK-07 (pack vs spec), R-ARCH-01 (safety)
KEEP AS CANDIDATE:                         R-CAND-01..04
MERGE:                                     R-CAND-05 -> R-PROCESS-04
```

**Most important items:** R-ARCH-01 (was unsafe; rewritten with fmParallel guard and the
warm-up concept removed) and R-PACK-07 (was conflated; restated as pack-is-a-snapshot).
The systemic fixes — version-neutral "latest prevailing CMS", no lossy re-summarising,
and pack-vs-spec clarity — are applied throughout and called out in the general comment.
