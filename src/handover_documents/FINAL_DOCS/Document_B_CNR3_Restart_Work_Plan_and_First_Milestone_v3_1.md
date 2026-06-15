# Document B — CNR3 Restart Work Plan and First Milestone (CMS07.0)

**Version:** v3.1 (CMS07.0 restart; post-§3A-population checkpoint)  
**Date:** 2026-06-13  
**Role:** Current-state / work-plan document. Re-issued for the checkpoint handover set.
It states the controlling authority, the immediate work, proof obligations, no-action
rules, and what must not be implemented yet.

**Generation source:** `CNR3_Handover_Pack_Production_Spec_v2_2_POPULATED_3A.md`  
**Precedence:** volatile. If this document ever conflicts with the latest prevailing CMS,
the CMS wins. If this document conflicts with the Production Spec §3A on register-owned
rules, §3A wins.

---

## 1. Controlling authority

- The latest prevailing CMS, currently **CMS07.0** (`cnr3_cache_manager_design_v7_0.md`),
  is the controlling design authority.
- References to CMS07.0 as controlling mean CMS07.0 or its later approved successor.
  Specific CMS07.0 section pointers are version-specific and must be re-checked against
  any successor.
- If the CMS conflicts with, or is merely unclear in alignment with, prior material, the
  CMS wins unless the user explicitly says otherwise.
- If the CMS itself is silent, ambiguous, or incomplete on an implementation point, stop
  and ask. Do not guess or improvise.

---

## 2. Handover-pack state

```text
Production Spec:       v2.2, §3A populated.
Document A:            generated from Production Spec §3.2 and §3A.
Document B:            this current-state / work-plan checkpoint document.
Coder introduction:    to be reviewed after Document A/B generation.
CMS design:            CMS07.0, included unchanged as controlling design authority.
Manifest/checksums:    to be produced after intro review and final checkpoint assembly.
```

The Production Spec now contains the populated register-owned rule set. A released
checkpoint pack still requires Document A/B/intro/manifest consistency and checksum
generation.

---

## 3. Build / transition state

- Rename all existing `.h`/`.cpp` files **except `VapourSynth4.h` and `VSHelper4.h`**
  to `.txt`, so old code is reference-only and out of the active build.
- The old binary need not build; GitHub CI may break for now.
- Builds are done in **Visual Studio 2026**.
- Do not action the rename or any file creation without explicit user instruction.
  The restart brief is read-understand-propose, not act.

---

## 4. First milestone — prove ownership before behaviour

Build, **in isolation with no VapourSynth wiring yet**, the cache-manager core:

```text
- slot = VSFrame* ref + frame number + pin_count + is_checkpoint;
- ordered frame-number index;
- non-checkpoint and checkpoint pools;
- hot-zone state;
- per-invocation pin-list;
- single cache-wide-lock skeleton with inside/outside-lock discipline;
- atomic scopes implemented exactly as the CMS AS register defines them;
- pin / unpin + pin-list record/discharge + single-ownership/null-on-consume;
- discharge-before-free ordering;
- RAII owned-ref wrapper as baseline;
- composite eviction predicate;
- bounded prune: decide+detach in lock, batch freeFrame outside, K-bound;
- single remove helper.
```

**Proof obligations before any `getFrame` integration:**

```text
- pin/unpin balance = 0
- lookup-ref balance = 0  (acquired == released + transferred)
- no leaks
- no double-free
- eviction never selects a pinned / checkpoint / in-zone slot
- shutdown clear() releases everything, with a warning on any non-zero pin
```

---

## 5. Rule status and coder enumeration task

The register-owned rules have been populated into Production Spec §3A. A coder restart
should still enumerate all prevailing rules back to the user, but the task is now a
**verification/reconciliation step**, not first population.

The coder should distinguish:

```text
REGISTER-OWNED:
    authority, pack governance, process, architecture/salvage, retired-fact entries,
    and resolved candidate dispositions already recorded in Production Spec §3A.

CMS-DEFINED / HANDED-OFF:
    design/cache-core/reference-count/VapourSynth-lifecycle/recovery/constant/
    instrumentation/atomic-scope/first-milestone rules defined in CMS07.0.
```

If the coder identifies an apparent omission, conflict, or candidate rule, it is raised
for user decision before coding. No rule carries silently.

---

## 6. Do-not-implement list

```text
- No continuation of CMS06.x / H15.6B coding.
- No patching of the old cache manager.
- No old cache concepts assumed to carry forward:
    deferred pinning,
    held-ref predecessor reservation,
    checkpoint-as-pin,
    zone-as-findability-guarantee,
    bounded-warmup conservative source window.
- No getFrame / VapourSynth wiring until the cache core is proven in isolation.
- No old-code salvage copy into new files until explicitly approved.
- No CNR2 recovery/predecessor logic.
- No file renaming, file creation, salvage copy, getFrame integration, or mutex/lock
  scoping change without explicit user discussion, agreement, and instruction.
```

---

## 7. Hard gates

- Diagnostics-as-hard-gate: after each phase or subphase, diagnostics are captured and
  assessed. A partial fail is a FAIL. Unexpected/anomalous values stop progression until
  understood, discussed, and agreed.
- Design Compliance Review: run after each phase or coherent block. The obligation is
  register-owned; the checklist itself is defined in the CMS.
- Atomic-scope boundaries and the V5 firewall are inviolable CMS-defined design rules.
  Any uncovered critical-section need is raised to the user, not improvised.
- Observation gates observe only. Behaviour-changing scaffolds must be explicitly
  marked, tracked, reviewed, and unwound under the register-owned process rule.

---

## 8. Expected first responses from the coder chat

After reading the intro, CMS07.0, Document A, Document B, and Production Spec, the coder
should respond with:

```text
a) confirmation of understanding of the restart and old/new separation;
b) any questions on CMS07.0;
c) an enumerated prevailing-rules list for verification/reconciliation, distinguishing
   REGISTER-OWNED rules from CMS-DEFINED / HANDED-OFF rules;
d) the proposed file/header/structure layout as a text proposal only — no files yet.
```

---

## 9. Current status summary

```text
Design authority:      Latest prevailing CMS, currently CMS07.0.
Production Spec:       v2.2 populated §3A.
Code state:            CMS06.11-era code present, to be moved to .txt (reference only).
                       No CMS07.0 code written yet.
Immediate task:        Review coder restart introduction, then assemble final checkpoint
                       pack/manifest; coder restart follows after that.
First coding task:     Cache-core isolation milestone after rule verification and layout
                       sign-off.
Pixel layer:           Deferred to salvage step; CNR2 is pixel-maths reference only.
Old development trail: Historical archive only unless explicitly pulled for narrow
                       verification/salvage.
```
