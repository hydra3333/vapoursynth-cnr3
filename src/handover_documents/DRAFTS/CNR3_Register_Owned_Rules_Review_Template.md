# CNR3 Register-Owned Rules Review Template

**Purpose:** Review template for the non-CMS07.0 / register-owned rules before populating the Production Spec §3A Prevailing Rules Register.  
**Status:** Draft review aid only. Not yet §3A.  
**Date:** 2026-06-13


```text
Reviewer General Comment: 
The rules as they stood below seemed very wooly and potentially open to holes/(mis)interpetation.
Some attempt has been made during review to suggest draft clarifications and perhaps strengthening
but which need wordsmithing.

You may wish to also consider it for other rules based on the nature of the
user comments/clarifications below.

Some of the rules mis-interpret a handover pack vs it's specification document; that needs review for intent and then fixing.

Some of the rules do not seem to align with or have the same meaning as their counterparts the smallish list of rules identified earlier in the chat ?

Check everywhere to ensure not potentially relying on a specific non-latest version of CMS et al,
i.e. do not fall into trap of being version specific when it perhaps should be the latest prevailing CMS

Verify meaning, perhaps suggest change to intent or not, and re-word, something which may apply to all rule reviews.

```

## How to use this file

For each rule:

1. Tick or type your decision under **Review decision**.
2. Add your comments under **Reviewer comment**.
3. Add replacement wording if the rule should be modified.
4. Reupload the annotated file for reconciliation.

The goal is to determine which rules become authoritative **register-owned** rules in Production Spec §3A.

CMS07.0-defined rules are not listed here except where a process rule deliberately references them. CMS07.0 keeps its own design/cache-core rules.

## Decision keywords

Use these if editing in plain text:

```text
CONFIRM
MODIFY
RETIRE
SUPERSEDE
CANDIDATE ONLY
DELETE
HAND OFF TO CMS07.0
MERGE WITH <Rule ID>
```

---

# Rules for review

## R-AUTH-01 — CMS07.0-or-later is the controlling design authority

**Category:** Authority / controlling design  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
CMS07.0, or a later approved successor, is the controlling design authority for the cache restart. Prior handover material, memories, code assumptions, and discussion are subordinate.

**Review decision:**  
- [CONFIRM] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

**Suggested replacement wording, if any:**  

---

## R-AUTH-02 — CMS07.0 conflict or unclear-alignment rule

**Category:** Authority / conflict precedence  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
If CMS07.0 conflicts with, or is merely unclear in alignment with, prior material, CMS07.0 wins unless the user explicitly says otherwise.

**Review decision:**  
- [CONFIRM] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

**Suggested replacement wording, if any:**  

---

## R-AUTH-03 — CMS07.0 ambiguity stops work

**Category:** Authority / ambiguity  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
If CMS07.0 itself is silent, ambiguous, or incomplete on an implementation point, stop and ask. Do not guess or improvise.

**Review decision:**  
- [CONFIRM] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

**Suggested replacement wording, if any:**  

---

## R-AUTH-04 — CMS06.x decision trail and proof state are historical only

**Category:** Authority / supersession  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
CMS06.x phase state, proof state, and decision trail are historical archive only and are not active implementation direction for the CMS07 restart.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> Need clarification.

**Suggested replacement wording, if any:**  

```text
CMS06.x-or-earlier designs, decisions, assumptions, phase state, proof state, and decision trail are
historical archive only and are not active implementation direction for the CMS07 restart.
To be clear, never carry forward CMS06.x-or-earlier decisions or assumptions or designs or code
(exception: rule R-AUTH-05).
```

---

## R-AUTH-05 — Old cache-related code is salvage reference only

**Category:** Authority / old code  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Existing cache-related code is superseded. Old code may be used only as verifiable salvage reference where explicitly approved.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> requires clarification

**Suggested replacement wording, if any:**  

```text
All CMS06.x-or-earlier code is fully superseded as old code.
Old code may be re-used only as verifiable salvage reference where explicitly approved;
to be clear, only re-use code if explicitly identified as non-cache related and which has been
examined and verified as safe to re-use and is compliant with the latest CMS version.
```

---

## R-PACK-01 — Production Spec §3A is the durable home for register-owned rules

**Category:** Pack governance / rule register  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The Production Spec §3A Prevailing Rules Register is the authoritative durable home for register-owned rules.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> you must clarify what register-owned ruleset is, and what other rulesets are
> so that readers can know what should fit where and when and why.

**Suggested replacement wording, if any:**  

```text
you can think of some appropriate new text
```

---

## R-PACK-02 — Document A reproduces §3A owned rules; §3A wins on mismatch

**Category:** Pack governance / Document A propagation  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Document A reproduces the register-owned rules for reader convenience. If Document A and §3A disagree, §3A wins.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> latest version

**Suggested replacement wording, if any:**  

```text
The latest version of the Handover Pack Creation Specification document is the single source of truth of register-owned rules.
Every Handover Pack Document A produced using that specification reproduces the register-owned rules for reader convenience.
If Document A and latest version of the Handover Pack Creation Specification document disagree, the latest version of the
Handover Pack Creation Specification document wins and a reconciliation discussion must occur, with agreement and approval prior to proceeding.
If Document A or he latest version of the Handover Pack Creation Specification document disagree with §3A, §3A wins and a reconciliation discussion must occur,
with agreement and approval prior to proceeding.
```

---

## R-PACK-03 — CMS07.0-defined rules are handed off, not restated or indexed in §3A

**Category:** Pack governance / CMS07 hand-off  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Design/cache-core rules already defined in CMS07.0 are not duplicated, indexed, or renamed in §3A. CMS07.0 remains their authoritative home.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> latest version

**Suggested replacement wording, if any:**  

```text
Design/cache-core rules already defined in CMS07.0 or later are not duplicated, indexed, or renamed in §3A. CMS07.0 or later remains their authoritative home.
Specifically, the latest version of CMS07.0 or later wins.
```

---

## R-PACK-04 — Candidate rules are non-controlling until confirmed

**Category:** Pack governance / candidates  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
A remembered or prior-context-derived rule is a candidate only until explicitly confirmed into §3A if register-owned, or into CMS07.0 if design-level.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> latest version

**Suggested replacement wording, if any:**  

```text
A remembered or prior-context-derived rule is a candidate only until explicitly confirmed into §3A if register-owned, or into
the latest version of CMS07.0 or later if design-level.
```

---

## R-PACK-05 — Retired and superseded rule facts are retained, not deleted

**Category:** Pack governance / dispositions  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
When a rule is retired or superseded, the fact of that decision remains recorded with status and reason so it cannot silently re-enter later.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> enforcement, tighten

**Suggested replacement wording, if any:**  

```text
When a rule is retired or superseded, the fact of that decision remains recorded with status and reason, so that the rule cannot and must not silently re-enter later.
```

---

## R-PACK-06 — Draft-vs-released pack gate

**Category:** Pack governance / release gate  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
A CMS07 handover pack with §3A pending is a draft only. A released pack requires §3A populated and Document A regenerated to reproduce it.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> not sure what is §3A  so tried to work around it. same for some other user suggestions in this document.

**Suggested replacement wording, if any:**  

```text
A CMS07 or later handover pack with §3A pending is a draft only.
A CMS07 or later handover pack must be generated using the latest version of the Handover Pack Creation Specification document, although it may recycle portions of the just-prior Handover Document A.
A released handover pack requires §3A populated and Document A regenerated to reproduce it.
```

---

## R-PACK-07 — Core pack files are distinct from companion coding-start attachments

**Category:** Pack governance / attachments  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The durable handover pack is distinct from companion working material such as .txt code snapshots, CNR2 reference excerpts, and logs.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with significant modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> the handover pack is a snapshot in time of a project state, produced from the latest version of the
> Handover Pack Creation Specification document, possibly recycling portions of the just-prior Handover Document A
> So this rule is completely wrong, although it becomes a valid thing if it is constructed correctly with proper meaning

**Suggested replacement wording, if any:**  

```text
???
```

---

## R-PACK-08 — Released packs are not silently mutated

**Category:** Pack governance / immutability  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Do not silently mutate a released handover pack. Regenerate with a new manifest/checksums and appropriate versioning.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text
Do not silently mutate a released handover pack. Regenerate with a new manifest/checksums and appropriate versioning
and based on the latest version of the Handover Pack Creation Specification document.
```

---

## R-PROCESS-01 — Code comments must be concise but never safety-incomplete

**Category:** Coding process / comments  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Comments should be concise, relevant, and useful to maintainers, but must not over-compress safety-critical information about locking, threading, ownership, lifetime, refcounts, pre/postconditions, or silent-bug invariants. The obligation includes placing clear comments above CR1-CR5 constants, while CR substance remains CMS07.0-defined.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text
Code comments should be clear, reasonably concise, relevant, and definitely clarifying and useful to human maintainers,
but must not over-compress especially when addressing safety-critical information about locking, threading, ownership,
lifetime, refcounts, pre/postconditions, or silent-bug invariants. When modifying/updating, 
do not unnessarily re-word or re-summarize resulting in loss of detail.
The obligation includes placing clear comments above CR1-CR5 constants, while CR substance remains CMS07.0-or-later defined.
```

---

## R-PROCESS-02 — Code update instructions use uniquely matchable before/after blocks

**Category:** Coding process / patch instructions  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Code-update instructions must state file and function/location, show exact existing code to replace, show exact replacement, and include enough context for a unique match.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> this one was too loose.  a better version of the text below please, which clarifies in without ambiguity

**Suggested replacement wording, if any:**  

```text
Code-update instructions produced for the human developer/maintainer (eg using Visual Studio latest version) must
uniquely identify BEFORE and AFTER code blocks which contain enough to ensure visually uniqely identifying the location of the change,
eg with extra code shown just prior and and just after the proposed change in both BEFORE and AFTER blocks.
On other words, it must also state file and function/location, uniquely identify the exact existing code to replace, show exact replacement, and
include enough visual context for a human to uniquely match.
```

---

## R-PROCESS-03 — Phase/SubPhase numbering restarts and naming is proposed before coding

**Category:** Coding process / phase naming  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
CMS07 development restarts phase numbering. The coder proposes the concrete expanded Phase/SubPhase naming convention before coding unless the user approves another convention.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> can better wording be used ?

**Suggested replacement wording, if any:**  

```text
CMS07 development restarts phase/subphase numbering. 
The coder proposes the concrete expanded Phase/SubPhase naming convention before coding unless the user approves another convention.
During devleopment/maintenance the coder may propose to split/join/re-number in-progress and/or subsequent phases/subphases based on assessed
prevailing circumstances, and then do so upon approval from the human managing.
```

---

## R-PROCESS-04 — PASS responses include a suitable Visual Studio-style commit title/body

**Category:** Coding process / commits  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
When a development or test step is marked PASS, provide a suitable commit title and body unless the user asks otherwise.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> recommend better wording please ?

**Suggested replacement wording, if any:**  

```text
Upon agreement or when a development or test step is marked PASS,
provide a suitable commit title and body unless the user asks otherwise;
the format is github/VisualStudio compatible, being a single line title then a blank line then multiple-line body of the commit.
The title and body of the commit should contain relevant summary information about the commit.
```

---

## R-PROCESS-05 — Diagnostics-as-hard-gate

**Category:** Coding process / diagnostics gate  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Unexpected non-zero error counters or anomalous reference-count, prune, validation, or recovery-search values stop progression until understood.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text
Care must be taken to test and identify and check relevant diagnoistic information after a development/maintenace phase/subphase
and form a view of PASS or FAIL (even a part fail is a FAIL).
Unexpected counters values or error values or anomalous values for reference-count, prune, validation, or recovery-search or other
matters being diagnosed/checked, stop progression until understood and discussed and diagnostic/rectification actions and/or direction is agreed.
```

---

## R-PROCESS-06 — Output-authority proof discipline

**Category:** Coding process / output authority proof  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Compute, store, return-decision, return-transfer, and output-authority must each be separately provable. This is a process/proof obligation; the underlying mechanisms live in CMS07.0.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> check everywhere to ensure not relying on the non-latest version of CMS et al.

**Suggested replacement wording, if any:**  

```text
Compute, store, return-decision, return-transfer, and output-authority must each be separately provable.
This is a process/proof obligation; the underlying mechanisms live in the latest prevailing CMS.
```

---

## R-PROCESS-07 — Design Compliance Review must be run after each phase/coherent block

**Category:** Coding process / design review  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Run the Design Compliance Review after each phase or coherent block. The checklist itself lives in CMS07.0 and is not duplicated in §3A.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> do not fall into trap of being version specific when it perhaps should be the latest prevailing CMS

**Suggested replacement wording, if any:**  

```text
Run the Design Compliance Review after each phase or coherent large block. 
The checklist itself lives in thwe latest prevailing CMS and is not duplicated in §3A.
```

---

## R-PROCESS-08 — No file action without explicit instruction

**Category:** Coding process / no-action gate  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Do not rename files, create files, copy salvage code, or integrate getFrame without explicit user instruction.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text
Do not rename files, create files, copy salvage code, or integrate getFrame or change any mutex scoping without explicit user discussion, agreement and instruction.
```

---

## R-PROCESS-09 — Layout proposal is text-only until signed off

**Category:** Coding process / layout gate  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The coder proposes the file/header/structure layout as text for review. No files are created until the layout is explicitly signed off.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text
The coder intitially proposes the file/header/structure layout as text for review at retsart commcement.
No files are created until the layout is explicitly signed off.

```

---

## R-ARCH-01 — Reuse existing processing boundaries

**Category:** Architecture / processing boundaries  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Recovery/warm-up/fill compute reuses the existing per-frame processing boundary; no parallel or duplicate pixel/frame algorithms without explicit agreement.

**Review decision:**  
- [ ] Confirm as written
- [MODIFY] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> (I thought warm-up may no longer be a feature of CMS07.0 or later ?)
>
> I reckon this is an extremely unsafe rule as it stands, open to direly unsafe mis-interpretation, because our goals
> specifically explicitly state our end-game is fmparallel and that nothing should be implemented which gets in its way unless it
> is an unbypassable stepping-stone explicitly remembered to be fixed later and which specifically does not compromise
> design toward that goal.
>
> How can this rule be restated to clarify and be 100% clear about its intent, applicability and usefulness to a human  ?
> A re-stated rule may have some potential value but definitiely not as currently constructed and/or potentially construed.

**Suggested replacement wording, if any:**  

```text
???????????????????????
```

---

## R-ARCH-02 — Pixel/frame processing must not own cache or scheduling policy

**Category:** Architecture / pixel layer  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Pixel/frame processing handles pixel-layer work only and must not own cache, recovery, request, or scheduling policy.

**Review decision:**  
- [ ] Confirm as written
- [MODIFY] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> can it somehow say it must not perform any cache related actions or similar

**Suggested replacement wording, if any:**  

```text

```

---

## R-ARCH-03 — Cache manager must not contain pixel algorithm logic

**Category:** Architecture / cache manager  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The cache manager owns cache state, pins, zones, checkpoints, store/prune/recovery planning, validation, and diagnostics, but not the pixel algorithm.

**Review decision:**  
- [ ] Confirm as written
- [CONFIRM] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> did we forget the reponse tables, memory diagnostics, and perhaps other things we have talked about and worked on before ? Seems possible.

**Suggested replacement wording, if any:**  

```text

```

---

## R-ARCH-04 — Response tables and memory diagnostics are cache-independent utilities

**Category:** Architecture / utilities  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Response-table creation and memory diagnostics should remain cache-independent utilities unless explicitly redesigned.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-ARCH-05 — Salvage is second step only

**Category:** Architecture / salvage timing  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Salvage from old .txt code happens only after the new cache-core ownership/pinning/eviction discipline is proven in isolation.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-ARCH-06 — CNR2 pixel maths may be salvaged; recovery logic must not

**Category:** Architecture / CNR2 salvage  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
CNR2 may be used for pixel maths guidance, but its serialized recovery/predecessor shortcut must not be adopted.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-ARCH-07 — No old .txt code copied into new files during first milestone without explicit approval

**Category:** Architecture / old code copy gate  
**Current recommendation:** confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
During the first cache-core milestone, old .txt code is not copied into new .h/.cpp files without explicit per-case approval.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-01 — Deferred/non-checkpoint pinning as optional escalation is superseded

**Category:** Retired fact / pinning model  
**Current recommendation:** record as superseded  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The old idea that non-checkpoint pinning was deferred or emergency-only is superseded by CMS07.0 mandatory consumer-pinning.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-02 — Held-ref-only predecessor reservation as default architecture is superseded

**Category:** Retired fact / held-ref reservation  
**Current recommendation:** record as superseded  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Held-ref-only predecessor reservation is superseded by consumer-held pins on per-invocation pin-lists.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-03 — Checkpoint-as-pin reasoning is retired

**Category:** Retired fact / checkpoint model  
**Current recommendation:** record as retired/superseded  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Any rule or wording treating a checkpoint as a pin is retired. Checkpoint is a separate eviction-protection flag.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-04 — Hot-zone-as-active-findability guarantee is superseded

**Category:** Retired fact / hot-zone model  
**Current recommendation:** record as superseded  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Hot zones no longer guarantee active-frame findability. Pins provide active liveness; hot zones are prune-policy hints.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-05 — Blanket bounded-warmup source window is superseded

**Category:** Retired fact / source-request model  
**Current recommendation:** record as superseded  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The old blanket backward source-request window is superseded by the CMS07.0 dissolved source-window model.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-06 — CMS06.x / H15.6B coding is not an active continuation path

**Category:** Retired fact / CMS06 work item  
**Current recommendation:** record as retired active work item  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
CMS06.x / H15.6B work is not continued as an active coding path in the CMS07 restart.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-RETIRED-07 — Old strict-streaming bridge is not final output authority

**Category:** Retired fact / old strict bridge  
**Current recommendation:** record as superseded/retired as final authority  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
The old strict-streaming bridge, including next_needed / prev_output-style authority, is not final fmParallel output authority.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-CAND-01 — Prefer ASCII-only code-update instructions

**Category:** Candidate / code-update hygiene  
**Current recommendation:** review  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Prior preference: code-update instructions should prefer ASCII-only text to reduce editor/compiler/copy-paste issues. Candidate only unless confirmed.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-CAND-02 — Avoid unnecessary unrelated code/comment/layout/name changes

**Category:** Candidate / minimal-change discipline  
**Current recommendation:** review; recommended confirm  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Prior preference: avoid renaming, reformatting, or changing unrelated code/comments/layout unless needed for correctness or explicitly agreed.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-CAND-03 — Large diagnostic print formatting may be one-line where practical

**Category:** Candidate / diagnostic formatting  
**Current recommendation:** review; likely preference not hard rule  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Prior preference: large diagnostic prints may be formatted on one line where practical to reduce line count, while preserving clarity.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-CAND-04 — Use compile-time constexpr proof gates; no correctness behind disabled debug/proof guards

**Category:** Candidate / proof scaffolding  
**Current recommendation:** review  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Prior proof discipline: proof scaffolds should use compile-time constexpr gates, and code inside disabled proof/debug guards must not be required for correctness.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---

## R-CAND-05 — PASS response includes commit message unless user asks otherwise

**Category:** Candidate / commit-on-PASS wording  
**Current recommendation:** merge candidate  
**Register ownership:** REGISTER-OWNED, unless reviewer changes it.

**Proposed statement:**  
Candidate wording duplicates R-PROCESS-04. Recommended action: merge into R-PROCESS-04 rather than keep as a separate rule.

**Review decision:**  
- [ ] Confirm as written
- [ ] Confirm with modification
- [ ] Retire
- [ ] Supersede
- [ ] Keep as candidate only
- [ ] Delete from proposed register
- [ ] Move / hand off to CMS07.0
- [ ] Merge with another rule: `________________`

**Reviewer comment:**  

> 

**Suggested replacement wording, if any:**  

```text

```

---
