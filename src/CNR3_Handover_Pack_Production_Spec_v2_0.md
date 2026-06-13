# CNR3 Handover Pack Production Specification

**Date:** 2026-06-13
**Version:** v2.0
**Supersedes:** Production Spec v1.5 (which governed continuity through the CMS06.x
proof phases). v2.0 governs the **CMS07.0 restart**: a clean architectural
supersession, not a continuation. The governing purpose has changed, so this is a new
version rather than an in-place edit of v1.5.

---

## 1. Purpose of this specification

This specification defines how to produce the CNR3 handover pack for the **CMS07.0
restart**. A handover pack lets a new chat (AI or human) resume CNR3 work with full
controlling context and without re-deriving settled design.

**What changed from v1.5.** v1.5 was built to *preserve and continue* the CMS06.x
development line — it required carrying the previous pack forward as a baseline,
preserving the full decision trail, and maintaining proof-phase state (H15.6B,
CMS02-J0, old strict-state quarantine). CMS07.0 **completely supersedes** the previous
cache design. Therefore v2.0 inverts several v1.5 rules: it deliberately *quarantines*
the CMS06.x decision trail and proof state rather than preserving it as active, while
preserving the enduring project context and standing coding/process rules.

**The single most important principle:**

```text
CMS07.0 is the controlling design authority.
The CMS06.x decision trail and proof state are historical, not active.
Old code is salvage reference only, where verifiably safe.
No superseded cache assumption carries forward silently.
```

---

## 2. Required handover pack files (LEAN — v2.0)

The v2.0 pack is deliberately smaller than the v1.5/v2.0-era pack. Concerns that were
separate documents are folded into sections where they will actually be read.

```text
Document_A_CNR3_Project_Context_and_Standing_Rules_<version>.md
    Human-facing project context + goal + the old/new supersession story +
    standing coding/process/design/safety rules + salvage policy (as a section).

Document_B_CNR3_Restart_Work_Plan_and_First_Milestone_<version>.md
    Current controlling authority pointer, first milestone, proof obligations,
    prevailing-rules-enumeration requirement, current do-not-implement list.

cnr3_cache_manager_design_v7_0.md
    CMS07.0 — included UNCHANGED as the controlling design authority.

CNR3_Handover_Pack_Production_Spec_v2.0.md
    This spec — included for future regeneration.

CNR3_Coder_Restart_Introduction_to_CMS07_0_FINAL.md
    The coder restart brief (paste-ahead introduction).

CNR3_Handover_Pack_<version>_MANIFEST.md
    Reading order, hard precedence, pack integrity (checksums).
```

**Deliberately NOT in the pack:**
- The old Document B (CMS06.x decision log) and Document C (CMS06.x volatile state) —
  excluded as active inputs. CMS07.0 §9A and §12/§12A already carry forward the
  still-valid rules and decisions; including the old B/C is the main route by which
  stale CMS06-era assumptions re-enter. They may be retained OUTSIDE the pack as
  historical archive only.
- The old `cnr3_cache_manager_design_v6_11.md` and earlier — historical archive only,
  never an active design input. CMS07.0 is the sole design authority.
- The old CMS06.x reconciliation notes — historical.

**Numbering note:** the pack and its documents take a fresh version line appropriate to
the restart (this spec is v2.0; the pack manifest version is the producer's choice,
e.g. v3.0, to signal the clean break from the CMS06-era v2.0 pack). Do not reuse the
old CMS06-era pack version number unqualified.

---

## 2A. Drafting rules for the restart (replaces v1.5 §2A continuity rules)

v1.5 §2A required the previous approved pack as the drafting baseline and forbade
abbreviation. For the restart, the baseline changes and some preservation is
deliberately dropped.

**2A.1 Drafting baseline.** Draft the new pack from: CMS07.0 (controlling), the old
Document A (for enduring human/project context and standing rules ONLY), and the
CMS07.0 coder restart introduction. Do NOT use the old Document B/C as drafting
baselines.

**2A.2 No abbreviation of ENDURING context.** The human-facing project context, the
reason the project exists, the algorithmic orientation, the goal statement, and the
standing coding/process rules must be preserved in full or enhanced — never reduced to
a stub. (See §3 — the context requirement is mandatory and detailed.)

**2A.3 DO quarantine superseded material.** Unlike v1.5, the restart REQUIRES omitting
superseded development material from active sections. The following must NOT appear as
current/active direction (only, if at all, as explicitly-labelled historical note):
- CMS06.x phase state as active state;
- CMS06.x decisions as controlling decisions;
- old Document B/C content as active decision/state input;
- H15.6B, CMS02-H, CMS02-J0, old strict-state quarantine, old cache-authority work, or
  reserved-predecessor/held-ref mechanisms as current implementation direction;
- old cache-manager implementation detail except as explicitly marked
  historical/salvage reference.

**2A.4 Correct, don't silently distort.** Where old Document A context is still true,
keep it. Where it described the OLD architecture (deferred pinning, held-ref
reservation, bounded-warmup conservative window, zone-as-findability-guarantee), it
must be CORRECTED to the CMS07.0 model, not silently carried. Mark such corrections so
a reader sees the supersession.

**2A.5 Review requirement.** A generated pack is reviewed before use: confirm CMS07.0
is the stated authority everywhere, confirm no superseded material appears as active,
confirm the §3 context requirement is met, and run the §11 checklist.

---

## 3. Document A — project context and standing rules

`Document_A_CNR3_Project_Context_and_Standing_Rules_<version>.md`

### 3.1 Purpose

Document A orients a new AI or human maintainer BEFORE they touch anything. It is the
human-facing front door to the project. It must set the scene richly: what CNR3 is,
why it exists, why it is hard, what changed at the CMS07.0 restart, and the standing
rules that govern all work.

### 3.2 Mandatory detailed context (THIS IS THE HEART OF DOCUMENT A — keep it rich)

This section is intentionally detailed and must remain so. A new maintainer should be
able to understand the project's purpose and dangers from Document A alone. It must
cover, in human-readable prose (adapted/preserved from the enduring parts of the prior
Document A §A2–A8):

**3.2.1 What CNR3 is and WHY it exists (the project's reason for being).**
- CNR3 is a VapourSynth API4-only, integer-YUV-only recursive temporal chroma
  stabiliser, inspired by the CNR2/vscnr2 algorithm.
- It exists to reduce temporal chroma instability — chroma shimmer, dot-crawl-like
  instability, temporal chroma noise — in analogue / video-capture material (VHS,
  VHS-C, and similar), by reusing controlled amounts of the previous *filtered* chroma
  where the content is stable enough to permit it.
- The human reason: restoring old analogue captures so the chroma is stable and clean
  without smearing motion or crossing scene cuts.

**3.2.2 The load-bearing fact (why CNR3 is not a normal filter).**
- CNR3 is recursive and temporal: `output[N]` depends on `source[N]` AND on the
  already-filtered `output[N-1]` — NOT on `source[N-1]`.
- A modern VapourSynth graph may request frames out of display order (e.g. 0, 3, 1, 2,
  4). A naive previous-frame-only implementation fails at frame 3 because the correct
  filtered `output[2]` does not yet exist.
- This single fact is the source of every hard problem in the project, and the reason
  the cache subsystem exists.

**3.2.3 What VapourSynth is doing (scheduling orientation).**
- VapourSynth evaluates a filter graph by asking filters for frames via a `getFrame`
  callback. `arInitial` is request-arrival (request the upstream/source frames needed);
  `arAllFramesReady` is when those sources are available (read them, produce output,
  return a frame).
- A filter must NOT assume N-1 was already requested or produced, must NOT assume only
  nearby frame numbers will be requested, and a recursive filter must deliberately
  manage predecessor availability.

**3.2.4 The relevant filter modes and the final goal.**
- `fmUnordered`, `fmParallelRequests`, `fmParallel` — describe each briefly.
- State the goal posture explicitly:
  `safe under fmUnordered now / structurally compatible with fmParallelRequests /
  final design target: fmParallel.`

**3.2.5 Why the cache manager exists (correctness, not performance).**
- The cache is a CORRECTNESS subsystem: it retains computed outputs so the recursion
  can find predecessors (or recover them) rather than rebuilding from frame 0 on every
  request. It is not a speed optimisation.

**3.2.6 What changed at the CMS07.0 restart (the supersession story).**
- A reassessment found the previous (CMS06.x) cache mechanism workable but not fit for
  purpose. CMS07.0 is a new architecture that completely supersedes it.
- The headline changes, in plain terms: pinning is now the mandatory correctness
  mechanism (consumer-pins); hot zones are demoted to prune-policy hints over a pinned
  liveness floor; a checkpoint is a retention flag, not a pin; one cache-wide lock held
  minimally; bounded prune. The recovery model is two-phase (descending search for the
  nearest present output, ascending fill-holes-only) with a dissolved source window.
- Why it matters historically: CNR2 ran serialized and approximated a missing
  predecessor with the previous SOURCE frame; CNR3 abandons serialization for
  fmParallel and replaces that approximation with exact output recovery.

**3.2.7 What safety means here.** Correctness of the recursive chain before
performance; no dangling pointers, no leaked `VSFrame` refs, no double frees, no stale
index entries; proof before progression.

**3.2.8 Why diagnostics are mandatory.** Diagnostics are proof of safety. Unexpected
reference-count, prune, validation, or recovery-search values stop the next phase until
understood. An unexpected non-zero error counter is a hard gate.

### 3.3 Required Document A sections (structure)

```text
1. Project context and reason for the project (§3.2 content) — RICH, human-facing,
   at the TOP to set the scene.
2. The load-bearing recursion fact and why scheduling is central.
3. The CMS07.0 restart: what it supersedes and the old/new separation.
4. Current controlling authority: CMS07.0 (with precedence: CMS07.0 wins over any
   prior material; if CMS07.0 itself is silent/ambiguous, stop and ask).
5. Standing coding / process / design / safety rules (§3.5/§3.6/§3.7/§3.8).
6. Salvage policy (§3.9) — as a section, not a separate document.
7. Continuity note (brief): this Document A is for the CMS07.0 restart; the old
   CMS06-era Documents B/C are historical archive only.
```

### 3.4 Required high-level architecture content

State the intended separation of responsibilities for the restart (NOT the old code
layout as current): pixel/frame processing must not own cache or scheduling policy; the
cache manager must not contain pixel logic; response-table and memory-diagnostic
utilities are cache-independent. The actual new file layout is the coder's proposal
under CMS07.0 §11 and is not fixed by this document.

### 3.5 Required safety-critical and durable rules (carried, CMS07.0-aligned)

Document A states these standing rules (each also cross-referenced to CMS07.0 where it
elaborates):
- Reuse existing processing boundaries; no parallel pixel/frame algorithms without
  explicit agreement (override discipline).
- Ownership and release-balance discipline (RC1–RC8; CMS07.0 §9A.2).
- Output-authority discipline (compute / store / return-decision / return-transfer /
  output-authority each separately provable).
- Bounded-start honesty (an approximate fresh-start is never described as exact
  full-history recursion; CMS07.0 §9A.7).
- Diagnostics-as-hard-gate.
- The atomic-scope register AS1–AS7 as inviolable boundaries (CMS07.0 §8.7) and the V5
  firewall (CMS07.0 §8.6).
- VS-LIFECYCLE-01 (CMS07.0 §9A.1).
- The parameter-coherence rules CR1–CR5 codified as comments above their constants
  (CMS07.0 §10.2).
- The fmParallel final-goal invariant.

### 3.6 Required coding rules (carried verbatim in intent)

- **Rule 1 — Code comments.** Concise but never over-compressing safety-critical detail
  (locking/threading invariants, ownership/lifetime, reference-count discipline,
  non-obvious pre/postconditions). This now also covers codifying CR1–CR5 as constant
  comments.
- **Rule 2 — Code update instructions.** Before/after patch format: file and function
  stated, before-block uniquely matchable with enough context, after-block the exact
  replacement.

### 3.7 Required phase and SubPhase naming rules

Phase/SubPhase numbering restarts for the new development. Document A states the
expanded-naming convention; the coder proposes the concrete new convention before
coding (CMS07.0 restart intro), using the expanded style unless the user approves a
different one.

### 3.8 Commit title and body rules

Carry the Visual Studio commit title/body convention used on each PASS.

### 3.9 Salvage policy (section, not a separate document)

- Salvage is the SECOND step, only after the new cache-core ownership/pinning/eviction
  discipline is proven in isolation.
- Likely-salvageable (cache-independent): response-table creation, memory diagnostics,
  the pixel/frame-processing layer including the explicit-predecessor boundary.
- CNR2 reference (github.com/Asd-g/AviSynth-vsCnr2): salvage the PIXEL maths
  (response tables, the int64-accumulator weighted blend with `shift2 = 2*depth`,
  `downSampleLuma`, in-compute scene detection). **Never** adopt CNR2's
  recovery/predecessor logic (its serialized `last_frame != n-1 → source[n-1]`
  approximation is exactly what CNR3 replaces).
- No old `.txt` code is copied into new files during the first milestone without
  explicit per-case approval.

---

## 4. Document B — restart work plan and first milestone

`Document_B_CNR3_Restart_Work_Plan_and_First_Milestone_<version>.md`

(This replaces the old volatile "Document C". It is the current-state document and is
re-issued each session.)

### 4.1 Purpose

Document B states the current controlling authority, the immediate work, the proof
obligations, the rule-enumeration requirement, and what must NOT be implemented yet.

### 4.2 Required sections

```text
1. Controlling authority: CMS07.0 (final/complete for this restart unless revised).
   Precedence: CMS07.0 wins over prior material; if CMS07.0 itself is silent or
   ambiguous, stop and ask — do not guess.
2. Build/transition state: .h/.cpp → .txt rename decided; old binary need not build;
   CI may break; Visual Studio 2026; transition not to be actioned without explicit
   instruction.
3. First milestone (CMS07.0 §11, per D30): prove the cache-manager core in isolation,
   no VapourSynth wiring — data structures, single-lock skeleton, pin/pin-list,
   composite eviction predicate + bounded prune. Proof obligations: pin/unpin
   balance = 0, lookup-ref balance = 0 (acquired == released + transferred), no leaks,
   no double-free, eviction never selects a pinned/checkpoint/in-zone slot, shutdown
   clear() releases everything with a warning on any non-zero pin.
4. Prevailing-rules enumeration requirement: the coder enumerates ALL standing rules
   (from CMS07.0, Document A, prior agreements) for explicit sign-off before coding;
   no rule carries silently; a remembered-but-undocumented rule is surfaced as a
   candidate, not treated as controlling until confirmed.
5. Do-not-implement list (restart form):
   - no continuation of CMS06.x / H15.6B coding;
   - no patching of the old cache manager;
   - no old cache concepts assumed forward;
   - no getFrame/VapourSynth wiring until the core is proven;
   - no old-code salvage copy until explicitly approved (second step);
   - no CNR2 recovery/predecessor logic.
6. Hard gates: diagnostics-as-hard-gate; the §11-style design-compliance review per
   phase (CMS07.0 §9A.8, 17-item checklist incl. AS-register match).
7. Proposed-layout expectation: the coder proposes the file/header/structure layout
   (text) for review before any file creation.
```

### 4.3 Required current-state precedence wording

Document B states plainly that if it ever conflicts with CMS07.0, CMS07.0 wins; and
that Document B is volatile (re-issued per session) whereas CMS07.0 is the stable
design authority.

---

## 5. Companion design document rule

CMS07.0 (`cnr3_cache_manager_design_v7_0.md`) is included UNCHANGED as the controlling
design authority. It is not edited as part of pack production; if a design change is
needed, CMS07.0 is revised on its own and the pack regenerated against it. No earlier
CMS06.x design document is included as an active input.

---

## 6. New chat starter prompt

The pack includes a short starter prompt for a new chat. It must:
- name CMS07.0 as controlling and the precedence rule;
- list the in-scope attachments (Document A, Document B, CMS07.0, this spec, the coder
  intro, the manifest) and note B/C-era docs are historical only;
- ask the new chat to confirm understanding, raise questions on CMS07.0, enumerate the
  prevailing rules for sign-off, then propose a layout — before any file action.

---

## 7. Maintenance rules

- **7.1 Updating Document A** — when project-wide context, standing rules, or the
  architecture story change. A CMS07.0 revision is such a trigger.
- **7.2 Updating Document B** — every session (it is the volatile current-state doc).
- **7.3 Regenerating the pack** — produce a new manifest with updated checksums and an
  incremented pack version; never silently mutate a released pack.

---

## 8. Success criteria for a handover pack

```text
- A new chat can resume with CMS07.0 as unambiguous controlling authority.
- The human-facing project context and reason-for-being are rich and at the top.
- No superseded CMS06.x material appears as active direction.
- Old code is clearly salvage-reference-only.
- The first milestone and its proof obligations are unambiguous.
- The prevailing-rules-enumeration-before-coding requirement is explicit.
- Precedence and stop-and-ask rules are stated.
- The manifest gives reading order, precedence, and integrity checksums.
```

---

## 9. Final production checklist

```text
[ ] Document A leads with rich human-facing context and the project's reason for being.
[ ] Document A states the CMS07.0 supersession story and old/new separation.
[ ] Document A's standing rules are CMS07.0-aligned (no old-architecture rule stated
    as current).
[ ] Salvage policy present as a Document A section (not a separate document).
[ ] Document B states controlling authority, first milestone, proof obligations,
    rule-enumeration requirement, do-not-implement list, hard gates.
[ ] CMS07.0 included unchanged.
[ ] This Production Spec v2.0 included.
[ ] Coder restart introduction included.
[ ] No old Document B/C, no old CMS06.x design docs, as ACTIVE inputs (archive only).
[ ] Precedence + stop-and-ask wording present.
[ ] Manifest: reading order, hard precedence, SHA256 checksums, pack version.
[ ] Pack reviewed against §8 success criteria.
```
