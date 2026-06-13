# Document B — CNR3 Restart Work Plan and First Milestone (CMS07.0)

**Version:** v3.0 (CMS07.0 restart)
**Role:** Current-state / work-plan document (replaces the old volatile "Document C").
Re-issued each session. States the controlling authority, the immediate work, the proof
obligations, the rule-enumeration requirement, and what must NOT be implemented yet.
**Precedence:** volatile. If this document ever conflicts with CMS07.0, **CMS07.0
wins**. CMS07.0 is the stable design authority; this document is the changing plan.

---

## 1. Controlling authority

- **CMS07.0** (`cnr3_cache_manager_design_v7_0.md`) is the controlling design authority.
  It is **final and complete for this restart unless explicitly revised**.
- *CMS07.0-or-later:* references to CMS07.0 as the controlling design mean CMS07.0 or its
  later approved successor; specific section pointers are version-specific to CMS07.0 and
  re-checked against a successor; historical supersession statements stay pinned to
  CMS07.0; the filename denotes that specific file.
- **Precedence rules:**
  1. If CMS07.0 conflicts with — or is unclear in alignment with — any prior material
     (Document A, prior agreements, memory, earlier discussion), **CMS07.0 wins** unless
     the user explicitly says otherwise.
  2. If CMS07.0 **itself** is silent, ambiguous, or incomplete on an implementation
     point, that is NOT a "CMS wins" case — **stop and ask. Do not guess.** "Final and
     complete" means controlling, not all-detail-specified.

---

## 2. Build / transition state (decided)

- Rename all existing `.h`/`.cpp` files **except `VapourSynth4.h` and `VSHelper4.h`** to
  `.txt`, so old code is reference-only and out of the active build.
- The old binary need not build; GitHub CI may break for now.
- Builds are done in **Visual Studio 2026**.
- **Do not action the rename or any file creation without explicit instruction.** The
  restart brief is read-understand-propose, not act.

---

## 3. First milestone — prove ownership before behaviour (CMS07.0 §11, per D30)

Build, **in isolation with no VapourSynth wiring yet**, the cache-manager core:
- data structures: slot = `VSFrame*` ref + frame number + pin_count + is_checkpoint;
  ordered frame-number index; non-checkpoint + checkpoint pools; hot-zone state;
  per-invocation pin-list;
- the single cache-wide-lock skeleton with the inside/outside-lock discipline,
  implementing the atomic scopes per the AS register (CMS07.0 §8.7);
- pin / unpin + pin-list record/discharge + single-ownership/null-on-consume +
  discharge-before-free ordering, RAII wrapper as baseline;
- the composite eviction predicate + bounded prune (decide+detach in lock, batch
  `freeFrame` outside, K-bound), through the single remove helper (RC2).

**Proof obligations (must hold before any `getFrame` integration):**
```text
- pin/unpin balance = 0
- lookup-ref balance = 0  (acquired == released + transferred)
- no leaks, no double-free
- eviction never selects a pinned / checkpoint / in-zone slot
- shutdown clear() releases everything, with a warning on any non-zero pin
```

**Layout step:** the coder PROPOSES the file/header/structure layout (as text — likely
`.h`/`.cpp` files, internal structures, includes, function signatures with
purpose/parameter comments), aligned with separation of responsibilities, for review.
**No files are created until the layout is reviewed and explicitly signed off.**

---

## 4. Prevailing-rules enumeration (required before coding)

The coder enumerates **all** standing rules — from CMS07.0, Document A, and prior
agreements — each numbered and stated clearly, **marking each as either
REGISTER-OWNED or CMS07.0-DEFINED / HANDED-OFF**, for explicit confirmation,
modification, supersession, or retirement before process coding begins.
- **Only the signed-off REGISTER-OWNED set is recorded into the Production Spec's
  Prevailing Rules Register (§3A)**, which is the durable home for owned rules.
  CMS07.0-defined rules are NOT copied into §3A — CMS07.0 remains their authoritative
  register, and they are confirmed by confirming CMS07.0 itself. Document A reproduces
  the owned §3A rules and carries the hand-off clause. This document (B) only tracks the
  *status* of that task (see §8).
- **No rule carries over silently** — both kinds are enumerated for visibility; the
  owned/handed-off mark determines only where each is recorded.
- A rule remembered from prior context but not present in CMS07.0, Document A, or the
  restart introduction is surfaced as a **candidate** rule, marked as
  remembered/prior-context-derived, and is **not controlling until confirmed** (into
  §3A if owned, or into CMS07.0 if it is a design rule).

---

## 5. Do-not-implement list (restart form)

```text
- No continuation of CMS06.x / H15.6B coding.
- No patching of the old cache manager.
- No old cache concepts assumed to carry forward (deferred pinning, held-ref
  predecessor reservation, checkpoint-as-pin, zone-as-findability-guarantee,
  bounded-warmup conservative source window).
- No getFrame / VapourSynth wiring until the cache core is proven in isolation.
- No old-code salvage copy into new files until explicitly approved (salvage is the
  second step).
- No CNR2 recovery/predecessor logic (its serialized source[n-1] approximation).
```

---

## 6. Hard gates

- **Diagnostics-as-hard-gate:** an unexpected non-zero error counter stops work until
  understood.
- **Design Compliance Review (CMS07.0 §9A.8):** after each phase or coherent block, run
  the 17-item checklist, including item 17 — every critical section matches its
  AS-register entry (AS1–AS7).
- **Atomic-scope boundaries (CMS07.0 §8.7) and the V5 firewall (CMS07.0 §8.6)** are
  inviolable; uncovered critical-section needs are raised to the user, not improvised.

---

## 7. Expected first responses from the coder chat

```text
a) confirmation of understanding of the restart and the old/new separation;
b) any questions on CMS07.0;
c) the enumerated prevailing-rules list for sign-off;
d) the proposed file/header/structure layout (text proposal — no files yet).
```

---

## 8. Current status summary

```text
Design authority:      CMS07.0 (final/complete for restart unless revised).
Code state:            CMS06.11-era code present, to be moved to .txt (reference only).
                       No CMS07.0 code written yet.
Immediate task:        Cache-core isolation milestone (§3) — after rule enumeration (§4)
                       and layout sign-off.
Pixel layer:           Deferred to the salvage step; V8.1 approach recorded in CMS07.0
                       §13 (native subsampling, int64 blend accumulator); CNR2 is the
                       pixel-maths reference (not its recovery logic).
Handover pack bump:    This pack (v3.0) is the CMS07.0-aligned replacement for the
                       CMS06-era v2.0 pack.
Prevailing Rules
Register (Spec §3A):   PENDING FIRST POPULATION — to be filled by the coder's restart
                       enumeration + user sign-off (owned rules only). Until then,
                       CMS07.0 hard rules (its own register) + the owned process/
                       architecture/salvage rules in Document A §9/§10 are operative.
```
