# CNR3 Handover Pack Production Specification
**Date:** 2026-06-23
**Version:** v2.7
**Supersedes:** Production Spec v2.6. Production Spec v2.6 superseded v2.5; v2.5 superseded
v2.4; v2.4 superseded v2.3; v2.3 superseded Production Spec v1.5, which governed continuity
through the CMS06.x proof phases. v2.7 continues the **CMS07.0 restart** production-spec
line: a clean architectural supersession, not a continuation. The governing purpose changed
at v2.3, so this remains a new version line rather than an in-place edit of v1.5.
**v2.7 change (additive — three rule items + currency refresh; no rule changed or removed):**
(1) Adds **R-PROCESS-21** (proven code stays proven — no change to proven behaviour or
internals without prior approval), the keystone's chief disciplinary lesson, owned here as a
process rule. (2) Adds **R-PROCESS-22** (lifecycle / API contracts are settled from
documentation, not from observed test behaviour). (3) Adds an additive **clarification to
R-PROCESS-20** for live-getFrame plugin-only keystone phases (a third selftest-count
category, and the coordinator A/B acceptance harness as their behavioural proof). (4)
Currency refresh: the current controlling CMS is **CMS07.8** (`cnr3_cache_manager_design_v7_8.md`);
the pinned design-file references in §2/§5 are updated to the current file; the Document B
filename and §4 framing are genericised from the restart-era "first milestone" to
"current-state / work-plan for the prevailing phase" (the project is well past the first
milestone — it is in the cache↔pixel / getFrame keystone; see the current Document B). No
existing rule (R-AUTH-*, R-PACK-*, R-PROCESS-01..20, R-ARCH-*, R-RETIRED-*, R-CAND-*) and no
canonical-context text (§3.2) is changed or removed; the only rule-text additions are
R-PROCESS-21, R-PROCESS-22, and the labelled R-PROCESS-20 clarification. (v2.4 added
R-PROCESS-19; v2.5 added R-PROCESS-20; v2.6 clarified R-PROCESS-20; v2.7 adds R-PROCESS-21/22
and a second R-PROCESS-20 clarification.) **Note on Document A lag:** the most recently
generated Document A is v3.2, which predates R-PROCESS-20, R-PROCESS-21, and R-PROCESS-22; per
R-PACK-02, on any Document A / §3A mismatch §3A (this spec) is authoritative, so §3A is the
controlling source for those rules until Document A is regenerated (to v3.3) to reproduce this
register.
**Reading Rule:** Unless a historical version is being discussed explicitly
(make no assumption about that), references in this document to this
spec version, pack version, or generated handover document version should be
read as “this version or later.”
For example, a reference to `v2.4` means `v2.4-or-later` once a later approved
version exists.
**CMS07.0 Reading Rule:** References to **CMS07.0** as the controlling design mean
**CMS07.0 or its later approved successor** (currently **CMS07.8**). EXCEPT:
(a) specific CMS section pointers (e.g. “§9A.2”, “§8.7”, “§9.7.7”) are version-specific and
must be re-checked against the prevailing version — “or-later” does NOT promise a
section number is stable across versions; (b) historical statements about what CMS07.0
superseded remain pinned to CMS07.0; (c) a literal design filename (e.g.
`cnr3_cache_manager_design_v7_0.md`, or the current `cnr3_cache_manager_design_v7_8.md`)
denotes that specific file and does NOT auto-advance — the pack includes the CURRENT CMS
file, and these literal filename references are updated by hand when the CMS is bumped. This
convention is stated once here and governs the whole document; body references are written as
plain “CMS07.0”.
---
## 1. Purpose of this specification
This specification defines how to produce the CNR3 handover pack for the **CMS07.0
restart**. A handover pack lets a new chat (AI or human) resume CNR3 work with full
controlling context and without re-deriving settled design.
**What changed from v1.5.** v1.5 was built to *preserve and continue* the CMS06.x
development line — it required carrying the previous pack forward as a baseline,
preserving the full decision trail, and maintaining proof-phase state (H15.6B,
CMS02-J0, old strict-state quarantine). CMS07.0 **completely supersedes** the previous
cache design. Therefore v2.3 inverts several v1.5 rules: it deliberately *quarantines*
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
## 2. Required handover pack files (LEAN — v2.4)
The v2.4 pack is deliberately smaller than the v1.5-era pack. Concerns that were
separate documents are folded into sections where they will actually be read.
```text
Document_A_CNR3_Project_Context_and_Standing_Rules_<version>.md
    Human-facing project context + goal + the old/new supersession story +
    standing coding/process/design/safety rules + salvage policy (as a section).
Document_B_CNR3_Restart_Work_Plan_and_Current_State_<version>.md
    Current controlling authority pointer, the CURRENT phase and its proof obligations,
    prevailing-rules-enumeration requirement, current do-not-implement list. (Filename
    note: this document began life as "..._Restart_Work_Plan_and_First_Milestone_..."
    in the restart era; it has since been re-issued as "..._and_Current_State_..." as the
    project advanced past the first milestone. It is the volatile current-state document,
    re-issued each session — see §4.)
cnr3_cache_manager_design_v7_8.md
    The current CMS (CMS07.8) — included UNCHANGED as the controlling design authority.
    (This literal filename is the CURRENT controlling-design file; it is updated by hand
    at each CMS bump per the CMS07.0 Reading Rule clause (c).)
CNR3_Handover_Pack_Production_Spec_v2_7.md
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
  never an active design input. The current CMS is the sole design authority.
- The old CMS06.x reconciliation notes — historical.
**Core pack files vs companion coding-start attachments.** The files listed above are
the CORE handover pack — self-contained for understanding the project and the
controlling design. A coding-restart chat additionally needs COMPANION attachments that
are not part of the pack proper (they are environment/working material, not durable pack
documents):
```text
Core handover pack files (durable, versioned, checksummed in the manifest):
    Document A, Document B, the current CMS, this Production Spec, the coder restart
    introduction, the manifest.
Companion coding-start attachments (provided alongside, not pack documents):
    - current .txt code snapshot (after the .h/.cpp -> .txt transition), if available;
    - CNR2 reference source/excerpts for pixel-layer salvage, if not relying on live
      web lookup;
    - any relevant logs.
```
A pack can be structurally complete while a coding restart still lacks these companion
materials; the manifest/starter prompt notes which companion attachments are expected.
**Numbering note:** the pack and its documents take a fresh version line appropriate to
the restart (this spec is v2.7; the pack manifest version is the producer's choice,
e.g. v3.0, to signal the clean break from the CMS06-era v2.0-or-lower pack). Do not reuse the
old CMS06-era pack version number unqualified.
---
## 2A. Drafting rules for the restart (replaces v1.5 §2A continuity rules)
v1.5 §2A required the previous approved pack as the drafting baseline and forbade
abbreviation. For the restart, the baseline changes and some preservation is
deliberately dropped.
**2A.1 Drafting baseline (context AND rules are mastered in THIS spec).**
- **Enduring human context** is mastered in §3.2 of THIS specification (the canonical
  context). Document A's context section is a **verbatim reproduction** of §3.2 — copied,
  not paraphrased, summarised, or regenerated. This reverses the earlier
  forward-carry-from-Document-A approach: because the regeneration tooling favours brevity
  and may silently thin detail, the authoritative copy is held in the spec (hand-edited,
  infrequent) and Document A reproduces it. If Document A's context and §3.2 differ, §3.2
  wins and Document A is corrected to match (§3.2.0). Context changes are made in §3.2,
  not in Document A.
- **The prevailing rules** are mastered in §3A (the Prevailing Rules Register held in
  THIS spec). Document A's rules section is GENERATED to reproduce §3A; it is not an
  independent source.
- **The controlling design** is the latest prevailing CMS.
- Do NOT use the old Document B/C as drafting baselines for context or rules.
**2A.2 No abbreviation of ENDURING context — richness is MANDATORY and checkable.**
The canonical context (§3.2) is rich by design, and Document A reproduces it verbatim.
The human-facing project context, the reason the project exists, the algorithmic
orientation, and the goal statement must never be reduced to a stub or a bare bullet
list. This is the one part of the pack where length and detail are a FEATURE, not a
fault: it must read as genuine, engaging human orientation that lets a newcomer
understand what CNR3 is, why it exists, and why it is hard, from the document alone. A
generated Document A whose context is thinned, summarised, or otherwise diverges from
§3.2 FAILS the §9 checklist and is corrected to match §3.2. The rules content likewise
comes from §3A (the register), reproduced in full.
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
### 3.2 Canonical project context (AUTHORITATIVE MASTER — Document A reproduces this verbatim)
**This section is the authoritative master copy of the CNR3 overall-context material.**
It is not merely a checklist of what Document A must cover — the prose below IS the
context, and Document A reproduces it **verbatim** (see §3.2.0). It is intentionally
detailed and must remain so: a new maintainer should be able to understand the project's
purpose and dangers from this text alone.
**3.2.0 Why the context master lives HERE, and how it propagates.**
The authoritative copy of the overall-context material lives in THIS specification, not
in Document A, for a concrete operational reason: Document A is regenerated at every
handover, and the regeneration tooling tends to favour brevity and may silently thin or
"mangle" detail — especially if asked to do something else (e.g. a code cross-check) in
the same pass. The specification, by contrast, is hand-edited deliberately and
infrequently. Holding the master here therefore protects the context from gradual
erosion. Propagation and precedence:
- Document A's context section is a **verbatim reproduction** of §3.2 (§3.2.1–§3.2.8
  below) — copied, NOT paraphrased, summarised, or regenerated. The region is reproduced
  exactly, headings and all.
- If Document A's context and §3.2 ever differ, **§3.2 wins**; the difference is treated
  as a transcription/thinning error and Document A is corrected (regenerated) to match,
  with the mismatch noted.
- When the context genuinely needs to change, it is changed **HERE in §3.2** (deliberate,
  hand-edited), with the change date noted; the next Document A regeneration carries it
  forward verbatim.
- This is a deliberate, documented duplication (master here, verbatim copy in Document A).
  It is NOT accidental redundancy and must not be "tidied away" by removing the copy from
  Document A — the copy exists so the human-facing front door is self-contained, while the
  master exists so the text cannot be silently eroded by regeneration tooling.
*(The §3.2.x text below is the canonical context. BEGIN verbatim-reproduction region.)*
**3.2.1 What CNR3 is, and why this project exists.**
CNR3 is a VapourSynth **API4-only**, **integer-YUV-only** recursive temporal chroma
stabiliser, inspired by the CNR2 / vscnr2 temporal chroma-stabilisation algorithm.
The human reason for the project: old analogue video — VHS, VHS-C, and similar tape and
capture sources — carries temporal chroma instability: colour that shimmers, crawls, and
flickers frame to frame even where the picture is essentially still. CNR3 exists to
*stabilise that chroma* during restoration: to reduce chroma shimmer, dot-crawl-like
instability, and temporal chroma noise by reusing controlled amounts of the **previous
filtered chroma** where the content is stable enough to allow it — without smearing
genuine motion and without dragging colour across scene cuts. The aim is clean, stable
colour on restored analogue captures that still looks natural.
The project's concrete aims:
- redevelop CNR2/vscnr2-style chroma stabilisation in modern C++;
- target VapourSynth API4 only (no API3-era types, assumptions, or scheduling shortcuts);
- support integer YUV formats only;
- get recursive chroma-stabilisation correctness right *before* pursuing parallel
  performance;
- operate safely under modern, possibly out-of-order, VapourSynth frame requests;
- provide strong diagnostics for cache, threading, memory, and reference-count behaviour.
Primary target material is PAL VHS/VHS-C captures (720×576), though the design is
format-adaptive.
**3.2.2 The load-bearing fact (why CNR3 is not a normal filter).**
CNR3 is **not** a normal stateless image filter. It is a **recursive temporal** filter:
the output for one frame depends on the *already-filtered output* of the previous frame.
```text
output[N] depends on source[N] and output[N - 1]
```
The predecessor is **not** merely `source[N - 1]`. It is the already-filtered output
frame `N - 1`.
If VapourSynth requested frames strictly in display order (0, 1, 2, 3, 4), a naive
previous-frame-only implementation could appear to work. But a modern VapourSynth graph
is **not** required to request frames in display order. A filter may be asked for, e.g.:
```text
0, 3, 1, 2, 4
```
A naive implementation fails at frame 3, because the correct filtered `output[2]` does
not yet exist. It might reject the request, use the wrong predecessor, or corrupt the
recursive chain. **This single fact is the source of every hard problem in CNR3, and the
reason the cache subsystem exists.**
**3.2.3 The algorithmic core.**
At a high level:
```text
For luma Y:
    copy source luma unchanged.
For chroma U/V:
    compare current source chroma against previous filtered chroma;
    compare current downsampled luma against previous downsampled luma;
    use signed-difference response tables for luma and chroma;
    blend current source chroma toward previous filtered chroma by the combined response.
```
Conceptually:
```text
weight = response_y(diff_y) * response_chroma(diff_chroma)
output_chroma[N] = weighted blend of  output_chroma[N-1]  and  source_chroma[N]
```
Small differences (stable content) → strong blend toward the previous filtered chroma
(denoise). Large differences (motion, change) → weak blend, trust the current source
(don't smear).
Scene-change handling: carrying previous filtered chroma across a true scene cut would
smear or contaminate the new scene. When scene-change detection fires (during the
compute, by comparing the frame to its predecessor), CNR3 copies the current source
chroma and skips the recursive blend for that frame — a fresh start.
*(Pixel-layer implementation note: CNR3 computes in native subsampling at native bit
depth, using a wide int64 arithmetic accumulator for the weighted blend with the shift
scaled by depth — the proven overflow-safe approach. This is pixel-layer detail; see the
latest prevailing CMS, V8.1.)*
**3.2.4 Why VapourSynth scheduling is central.**
VapourSynth evaluates a filter graph by asking filters for frames through a `getFrame`
callback.
```text
arInitial:
    VapourSynth is asking the filter to start work for frame N.
    The filter requests any upstream/source frames it needs.
arAllFramesReady:
    The requested upstream/source frames are available.
    The filter reads source data, produces output, and returns a frame.
```
Implications a recursive filter must respect:
- `arInitial` is request-arrival; `arAllFramesReady` is **not** the same as frame N being
  next in display order.
- A filter must not assume `N-1` was already requested or produced.
- A filter must not assume only nearby frame numbers will be requested.
- A filter must not let pruning discard a frame an active request needs.
- A recursive filter must deliberately manage predecessor availability.
(There is also a hard API rule — any source retrieved in `arAllFramesReady` must have
been requested in `arInitial` of the same activation; see the latest prevailing CMS,
the VS-LIFECYCLE rule.)
**3.2.5 Filter modes and the final goal.**
- **`fmUnordered`** — the filter may be called for frames in any order, one activation at
  a time per frame; the safe baseline for current work.
- **`fmParallelRequests`** — multiple requests in flight; the filter must be safe under
  concurrent arInitial activity.
- **`fmParallel`** — fully parallel execution; the final operational target.
Goal posture:
```text
safe under fmUnordered now
structurally compatible with fmParallelRequests
final design target: fmParallel
```
Design and coding must not make choices that block eventual safe `fmParallel` unless
explicitly justified and recorded. This posture is the reason the CMS adopts mandatory
consumer-pinning (see §3.2.7).
**3.2.6 Why the cache manager exists (correctness, not performance).**
The cache manager is a **correctness subsystem, not a performance cache.** It retains
computed outputs so the recursion can find each predecessor — or recover it — rather than
rebuilding the chain from frame 0 on every request. It exists to provide:
- safe predecessor availability and cache-hit reuse;
- checkpoints and bounded recovery of missing outputs ("holes");
- pruning with bounded memory use;
- safety under out-of-order and (eventually) parallel requests;
- no dangling frame pointers, no leaked `VSFrame` references, no double frees, no stale
  index entries.
Historical note that motivates the whole design: the ancestor CNR2 runs serialized
(`MT_SERIALIZED`) and keeps a single previous-output member. When asked for a
non-sequential frame, CNR2 cannot supply the true previous output and substitutes the
previous *source* frame as an approximate predecessor (`last_frame != n-1 →
GetFrame(n-1)`). It gets away with this only because it is serialized and the recursive
blend forgives a one-frame stand-in. CNR3 abandons serialization to reach `fmParallel`,
so it **cannot** rely on that approximation — and CNR3's cache + recovery architecture is
precisely the principled replacement: supply the *exact* previous output, present or
recovered.
**3.2.7 What changed at the CMS07.0 restart (the supersession story), and what safety means.**
A reassessment found the previous (CMS06.x) cache mechanism workable but not genuinely
fit for purpose. The current CMS is a new architecture that completely supersedes the
previous cache design. In plain terms:
- **Pinning is now the mandatory correctness mechanism.** Any frame a request actively
  needs is *pinned* by that request (a consumer-held pin), and the cache is the complete
  liveness index for the active set. (Previously, pinning was a deferred escalation —
  that decision is superseded.)
- **Hot zones are demoted to prune-policy hints.** They no longer guarantee findability
  of active frames (pins do that); they protect the anticipatory/decaying set.
- **A checkpoint is a retention flag, not a pin.** There is exactly one pin concept
  (consumer-claim).
- **One cache-wide lock, held minimally**; slow work (pixel compute, source requests,
  freeFrame) happens outside it.
- **Bounded prune** (decide-and-detach under the lock, batch freeFrame outside).
- **Recovery is two-phase:** a descending search from N-1 for the nearest present output
  (checkpoint flag irrelevant at search time), then an ascending fill-holes-only walk;
  with a dissolved source-request window (request source N plus the genuine holes only,
  not a blanket backward window).
The old/new separation — three highest-risk traps (do not conflate): (1) treating pinning
as optional/deferred — it is now the mandatory baseline; (2) reintroducing held-ref-only
predecessor reservation — superseded by consumer-pins; (3) thinking of a checkpoint as a
pin — it is a separate eviction-protection flag.
What safety means here: correctness of the recursive chain before performance; no
dangling pointers, no leaked `VSFrame` refs, no double frees, no stale index entries;
proof before progression.
**3.2.8 High-level architecture, and why diagnostics are mandatory.**
For the restart, responsibilities separate as follows (the concrete new file layout is
the coder's proposal under the latest prevailing CMS, not fixed here):
- **Pixel/frame processing** — luma copy, downsampled-luma buffers, scene-change
  detection, recursive chroma blend. Must NOT own cache or scheduling policy.
- **Cache manager** — pools, ordered index, pins/pin-list, hot zones, checkpoint flags,
  store/prune, recovery planning, validation, cache diagnostics. Must NOT contain pixel
  logic.
- **Response tables** and **memory diagnostics** — cache-independent utilities.
Why diagnostics are mandatory: diagnostics are proof of safety. Unexpected
reference-count, prune, validation, or recovery-search values stop the next phase until
understood. An unexpected non-zero error counter is a hard gate.
*(END verbatim-reproduction region. The §3.2.1–§3.2.8 text above is reproduced verbatim
into Document A.)*
### 3.3 Required Document A sections (structure)
```text
1. Project context and reason for the project — a VERBATIM reproduction of §3.2 of this
   spec (the canonical context). RICH, human-facing, at the TOP to set the scene. Copied
   exactly, not paraphrased or regenerated (§3.2.0).
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
### 3.5 Required rules content (OWNED rules reproduced + hand-off clause)
Document A's standing-rules section is GENERATED from §3A: it reproduces the
REGISTER-OWNED rules in full (authority, pack, process, architecture/salvage,
retired-facts, candidates) AND carries the §3A hand-off clause for the rules that live
in CMS07.0. It does **NOT** restate, index, or summarise the handed-off CMS07.0 rules
(RC1–RC8, AS1–AS7, CR1–CR5, VS-LIFECYCLE-01, recovery, instrumentation, V5 firewall,
first-milestone) — for those it points the reader to CMS07.0, consistent with §3A.
The §3A register is authoritative for the owned rules; if Document A and §3A disagree,
§3A wins. **A Document A whose owned-rules section does not faithfully reproduce the
populated §3A owned rules, or which restates/duplicates the handed-off CMS07.0 rules,
FAILS production review.** (Until §3A is populated, this requirement is held in
abeyance; a RELEASED pack, by §3A.1, requires the populated register and a matching
Document A.)
Document A therefore presents, for the human reader: the owned rules in full, then a
short hand-off paragraph — e.g. *"Design, cache-core, reference-count, VapourSynth-
lifecycle, recovery, parameter-coherence, instrumentation, atomic-scope and
first-milestone rules are defined in CMS07.0; consult it directly — they are not
restated here."* This keeps a single source per rule and avoids the duplication that
drifts.
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
## 3A. Prevailing Rules Register (master list of OWNED rules — lives HERE)
**Register status:** `POPULATED — FIRST AUTHORITATIVE REGISTER-OWNED RULESET`  
**Population source:** `CNR3_Register_Owned_Rules_Review_RECONCILED_v3_3_for_the_coder.md`  
**Population date:** 2026-06-13 (additive process rules R-PROCESS-19/20/21/22 added at
later spec versions; see each rule's Last-revised date and the §14-equivalent front-matter
change notes)  
**Scope:** REGISTER-OWNED rules only. CMS-defined rules remain handed off to the latest
prevailing CMS and are not duplicated here.
### Definitions used throughout
```text
Register-owned rule:
    A standing process / governance / authority / architecture / salvage rule that has
    no home in the CMS design document, so this §3A register is its authoritative home.
CMS-defined / handed-off rule:
    A design / cache-core / reference-count / recovery / constant / instrumentation /
    atomic-scope / first-milestone rule that lives in the latest prevailing CMS. §3A
    does not restate, index, rename, or re-ID it.
§3A:
    The Prevailing Rules Register section inside the Handover Pack Creation
    Specification; the durable, authoritative home for register-owned rules.
```
### Scope — the register OWNS some rules and HANDS OFF others
This project's recurring failure mode is paired artifacts drifting out of sync. To
prevent it, a rule is stored in exactly one authoritative place.
```text
OWNED by this register (full text held HERE):
    - Authority / precedence / supersession / old-new separation / no-action gate
    - Pack governance and §3A register mechanics
    - Coding process: comments, patch format, phase naming, commits, diagnostics gates,
      Design Compliance Review obligation, diagnostic formatting, proof/observation
      gates, behavioural-scaffold controls, minimal-change discipline, ASCII-safe
      code-update text, no silent paraphrase, override discipline,
      conditional-progression recording, proven-code-stays-proven, and
      lifecycle/API-contracts-from-documentation
    - Architecture separation and salvage policy
    - Retired-fact entries
    - Candidate dispositions / prior-context-derived rule dispositions
HANDED OFF to the latest prevailing CMS (NOT listed, indexed, restated, or renamed here):
    - Design, cache-core, pinning, checkpoint, hot-zone, locking, pruning rules
    - Reference-count discipline (RC1-RC8) and RAII baseline
    - VapourSynth lifecycle (VS-LIFECYCLE-01), frameData, operational modes
    - Recovery, source-request planning, bounded-start honesty, scene cuts
    - Parameter-coherence constants (CR1-CR5, decay_margin)
    - Instrumentation and recovery-search summary
    - Atomic-scope register (AS1-AS7) and the V5 firewall
    - The first-milestone proof gates
    - The keystone predecessor-sourcing decision and the source-input dependency
      declaration (rpGeneral) — CMS §9.7 / §9.7.7
```
### Hand-off clause for CMS-defined rules
```text
Design, cache-core, reference-count, VapourSynth-lifecycle, recovery, parameter-
coherence, instrumentation, atomic-scope, and first-milestone rules are defined and
numbered in the latest prevailing CMS (currently CMS07.0, or a later approved
successor). The CMS is the authoritative register for those rules. This register does
not restate, index, rename, re-ID, or duplicate them — consult the CMS directly. Where
this register and the CMS overlap in scope, the CMS wins.
```
### Why not even an index
An index entry (ID + title + section pointer) is still a second copy: the title
restates the rule and the pointer can rot if the CMS renumbers. A clean hand-off has
nothing to keep in sync. The CMS is both the source and the register for its own rules;
§3A is the source and register only for the owned rules below.
### Retired-fact entries are register-owned
When the CMS supersedes a CMS06-era rule, the CMS states the new rule, not the retired
one. The fact that the old rule was considered and retired lives here as an
anti-regression record. A retired-fact entry records the retirement fact and points to
the CMS for the replacement; it does not restate the superseded mechanism as active.
### Why the OWNED rules live in this spec, not in Document A
Document A is regenerated at every handover and is therefore where content can be
silently thinned, paraphrased, or mistranscribed. This spec is a whole-of-project-life
document, updated deliberately and infrequently. Placing the authoritative owned-rule
list here gives persistence and forced propagation: Document A is generated to reproduce
this register's owned rules and hand-off clause. If Document A and this register ever
disagree, this register wins and Document A is corrected.
### Identifier scheme
Owned rules use a stable short ID separate from the editable title:
`R-<CATEGORY>-<NN>` (for example, `R-AUTH-01`, `R-PROCESS-02`, `R-PACK-01`,
`R-RETIRED-01`, `R-CAND-01`). No `CMS07` prefix is used, because the register belongs
to this restart epoch and must not age badly into a later epoch. No reserved numeric
bands are used; the category code groups the rules, and numbering is sequential within
category. Familiar nicknames stay in the title or statement, not the ID.
Handed-off CMS rules keep the CMS's own identifiers and section references, such as
`RC8`, `AS3`, `CR1`, or a CMS section number. They are not given a second `R-...` ID.
### Lifecycle
- An owned rule is active/controlling when it appears in this register as confirmed or
  modified.
- A change to an owned rule is made here, with the revision date bumped. The next
  Document A regeneration reproduces it.
- A change to a handed-off CMS-defined rule is made in the CMS, not here.
- A remembered or prior-context-derived rule that is not confirmed here is a candidate
  only and is not controlling.
- Retired and superseded entries remain recorded so they cannot silently re-enter later.
### 3A.1 Draft vs released pack status
```text
DRAFT CMS07 handover pack:
    permitted while §3A is pending or while Document A has not yet been regenerated to
    match populated §3A. For review/working use only. Must be labelled draft.
RELEASED CMS07 handover pack:
    §3A must be populated, Document A's rules section must faithfully reproduce the
    populated §3A, and the manifest/checksums must be generated. A pack with §3A
    pending, or with Document A not regenerated to match §3A, must not be treated or
    circulated as an authoritative released pack.
```
**Current state acknowledgement:** this Production Spec now has §3A populated from the
agreed reconciled rule set. A released handover pack still requires a newly generated
Document A that reproduces this §3A, an up-to-date Document B, and a manifest/checksum
set.
### 3A.2 Rule dispositions
The register preserves the outcome of every register-owned rule review, not just the
live rules. Each entry carries a disposition by status. Superseded and retired rules are
kept with their status and reason; they are not deleted. This is deliberate: a retired
CMS06-era rule recorded as retired cannot silently creep back in later.
Active/controlling entries are those marked `confirmed` or equivalent. Resolved
candidate entries are not separate active rules unless they were promoted to an active
rule elsewhere in this register.
## 3A.3 Authority / precedence / old-new separation rules
### R-AUTH-01 — Latest prevailing CMS is the controlling design authority
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
The latest prevailing CMS (currently CMS07.0, or a later approved successor) is the
controlling design authority for the cache restart. Prior handover material, memories,
code assumptions, and earlier discussion are subordinate to it.
```
### R-AUTH-02 — Conflict or unclear-alignment with the CMS → CMS wins
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
If the latest prevailing CMS conflicts with — or is merely unclear in its alignment
with — prior material, the CMS wins unless the user explicitly says otherwise.
```
### R-AUTH-03 — CMS ambiguity stops work (do not guess)
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
If the latest prevailing CMS is itself silent, ambiguous, or incomplete on an
implementation point, stop and ask. Do not guess and do not improvise — especially on
anything touching correctness, locking, ownership, or the fmParallel path.
```
### R-AUTH-04 — CMS06.x-or-earlier design/decisions/state are historical only
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
CMS06.x-or-earlier designs, decisions, assumptions, phase state, proof state, and
decision trail are historical archive only and are not active implementation direction
for the CMS07 restart. To be clear: never carry forward CMS06.x-or-earlier
cache/design/implementation decisions, assumptions, designs, or code as active direction.
Prior process/governance rules may carry forward ONLY if explicitly reviewed and recorded
as register-owned rules in §3A. The only code exception is verified salvage under
R-AUTH-05.
```
### R-AUTH-05 — Old code is salvage reference only, and only when verified
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
All CMS06.x-or-earlier code is fully superseded. Old code may be re-used only as
verifiable salvage reference, and only when ALL of the following hold: it is explicitly
identified as non-cache-related; it has been examined and verified as safe to re-use; it
is compliant with the latest prevailing CMS; and its specific re-use has been explicitly
approved. (See also R-ARCH-05/06/07 for salvage timing and the CNR2 boundary.)
```
## 3A.4 Pack governance and §3A mechanics rules
### R-PACK-01 — §3A is the durable home for register-owned rules
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
The Prevailing Rules Register (§3A), inside the Handover Pack Creation Specification, is
the authoritative durable home for REGISTER-OWNED rules — the standing process,
governance, authority, architecture, and salvage rules that have no home in the CMS.
Design and cache-core rules are NOT register-owned: they live in the latest prevailing
CMS, which is their authoritative home (see R-PACK-03). §3A is where a register-owned
rule becomes active; a rule not recorded in §3A (or, for design rules, in the CMS) is not
controlling.
```
### R-PACK-02 — Document A reproduces §3A; on mismatch §3A wins
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
The latest version of the Handover Pack Creation Specification (which contains §3A) is
the single source of truth for register-owned rules. Every Document A generated from it
reproduces those register-owned rules for reader convenience. If a Document A disagrees
with §3A, §3A wins; the mismatch is treated as a transcription error and reconciled —
with explicit agreement and approval — before proceeding.
```
### R-PACK-03 — CMS-defined rules are handed off, not restated or indexed in §3A
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Design and cache-core rules already defined in the latest prevailing CMS are not
duplicated, indexed, summarised, or renamed in §3A. The CMS remains their authoritative
home; §3A points to it rather than restating it. Where §3A and the CMS overlap in scope,
the CMS wins.
```
### R-PACK-04 — Candidate rules are non-controlling until confirmed
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
A remembered or prior-context-derived rule is a candidate only, and is not controlling,
until explicitly confirmed — into §3A if it is register-owned, or into the latest
prevailing CMS if it is design-level.
```
### R-PACK-05 — Retired/superseded rule facts are retained, not deleted
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
When a rule is retired or superseded, the fact of that decision is retained in the
register with its status and reason, so that the retired rule cannot and must not
silently re-enter later.
```
### R-PACK-06 — Draft-vs-released pack gate
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
A handover pack whose §3A is still pending is a DRAFT only. A handover pack is generated
from the latest version of the Handover Pack Creation Specification; Document A's context
is reproduced from the specification's canonical context section (§3.2), not independently
recycled from a prior Document A. A RELEASED pack requires §3A populated and Document A
regenerated to reproduce both the canonical context and the register-owned rules.
```
### R-PACK-07 — A handover pack is a point-in-time snapshot generated from the spec
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
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
### R-PACK-08 — Released packs are not silently mutated
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
A released handover pack is never silently mutated. To change it, regenerate from the
latest version of the Handover Pack Creation Specification, producing a new manifest and
checksums with an incremented pack version.
```
## 3A.5 Coding / process rules
### R-PROCESS-01 — Code comments: concise but never safety-incomplete
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Code comments must be clear, reasonably concise, relevant, and genuinely useful to a
human maintainer, but must never over-compress contextual nor safety-critical information such
as locking and threading invariants, ownership and lifetime, reference-count discipline, non-obvious
pre/postconditions, and silent-bug invariants. When modifying code, do not needlessly
re-word or re-summarise existing safety comments in a way that obscures or loses detail. This
obligation includes placing clear explanatory comments above the CR1–CR5 constants; the
substance of CR1–CR5 remains defined in the latest prevailing CMS.
```
### R-PROCESS-02 — Code-update instructions: uniquely matchable before/after blocks
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Code-update instructions for the human developer/maintainer (e.g. using the latest Visual
Studio) must let a human uniquely and visually locate the change. Each instruction must:
state the file and the function/location; show a BEFORE block and an AFTER block; include
enough surrounding context in BOTH blocks (a few lines immediately before and after the
change) that the location matches uniquely and unambiguously; and show the exact existing
code to be replaced and its exact replacement.
```
**Operational note (R-PROCESS-20 supersedes the delivery mechanism):** from CMS07-H.2A
onward, code is delivered as downloadable unified-diff `.patch` files, not as inline
before/after edit blocks (R-PROCESS-20). R-PROCESS-02's intent — uniquely, visually
locatable changes with full context — is preserved by the wider-context patch (`git diff
-U10`); the before/after-block delivery form is retained here for any non-patch manual edit.
### R-PROCESS-03 — Phase/SubPhase numbering restarts; naming proposed before coding
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
CMS07 development restarts Phase/SubPhase numbering. The coder proposes the concrete
expanded Phase/SubPhase naming convention before coding, unless the user approves a
different convention. During development, the coder may propose to split, join, remove, or
renumber in-progress and/or subsequent phases based on prevailing circumstances, and does
so only on approval from the managing user.
```
### R-PROCESS-04 — PASS responses include a Visual Studio-style commit title/body
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
When a development or test step is agreed PASS, provide a suitable commit message unless
the user asks otherwise. The format is github / Visual Studio compatible: a single-line
title, then a blank line, then a multi-line body. Title and body should carry the
relevant summary information about the change. Following an agreement to move forward even
with a part-PASS or a no-PASS or another condition, a commit message should be generated
after that agreement.
```
### R-PROCESS-05 — Diagnostic-assessment-as-hard-gate
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
After each development/maintenance phase or subphase, the relevant diagnostic information
must be captured and checked, and a PASS or FAIL view formed (a partial fail is a
FAIL and noted as such). Any unexpected or anomalous counter or error value, or
reference-count, prune, validation, recovery-search, or other diagnosed value,
stops progression until it is understood and discussed, and the diagnosis,
rectification, and direction are assessed and agreed.
```
### R-PROCESS-06 — Output-authority proof discipline
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Compute, store, return-decision, return-transfer, and output-authority must each be
separately provable. This is a process/proof obligation owned here; the underlying
mechanisms being proven live in the latest prevailing CMS.
```
### R-PROCESS-07 — Design Compliance Review run after each phase/coherent block
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Run the Design Compliance Review after each phase or coherent block of work. The
obligation to run it is owned here; the checklist itself is defined in the latest
prevailing CMS and is not duplicated in §3A.
```
### R-PROCESS-08 — No file/scope action without explicit instruction
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Do not rename files, create files, copy salvage code, integrate getFrame, or change any
mutex/lock scoping without explicit user discussion, agreement, and instruction. This
brief is read-understand-propose, not act.
```
### R-PROCESS-09 — Layout proposal is text-only until signed off
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
At restart commencement, the coder proposes the file/header/structure layout as text, for
review. No files are created until that layout is explicitly signed off.
```
### R-PROCESS-10 — Diagnostic output goes to stderr, not stdout
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
All program-generated diagnostic output (e.g. printed diagnostics, progress, summaries)
must go to stderr, never to stdout, because stdout is the frame-data pipe (e.g. vspipe
into ffmpeg). Where practical, flush diagnostic output so the order of events can be
inferred. For clarity, this does not govern the normal frame/data interchange between
this filter and VapourSynth itself.
```
### R-PROCESS-11 — Diagnostic formatting: machine-detail allowed, human summaries required
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
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
### R-PROCESS-12 — Diagnostics are compile-time gated: two named gates, print subordinate to compute
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
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
### R-PROCESS-13 — No printing or long-running actions inside atomic/locked scopes
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Do not perform printing (e.g. diagnostics) or any long-running action inside an atomic /
locked scope. Atomic scopes are specifically defined by the designer (see the latest
prevailing CMS atomic-scope register); only the minimum — but all necessary — processing
occurs within them. Variables and data structures (including diagnostic counters) may be
manipulated inside an atomic scope, but any long-running action not already specified by
the designer requires prior discussion and approval before coding.
```
### R-PROCESS-14 — Minimal unrelated-change discipline (promoted from R-CAND-02)
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Do not rename, reformat, or change unrelated code, comments, layout, or names unless it
is required for correctness or has been explicitly agreed. Minimise diffs to what the
change actually needs.
```
### R-PROCESS-15 — ASCII-safe code-update text (promoted from R-CAND-01)
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Code-update instructions, replacement code, comments, macro names, file names, and commit
messages should use ASCII-only text unless the existing source or an explicitly approved
requirement needs otherwise. This reduces copy/paste, compiler, editor, shell, and diff
ambiguity. Non-ASCII prose may be used in discussion if it is not intended for direct
source-code insertion.
```
### R-PROCESS-16 — No silent rule/title paraphrase when propagating rules
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
When moving an agreed rule into §3A, Document A, an introduction, a handover, or a later
regeneration, do not paraphrase it in a way that weakens, broadens, narrows, or changes
its operational meaning. If wording is changed for clarity, preserve the exact safety
intent and flag any substantive change for review.
```
### R-PROCESS-17 — Explicit override discipline (no silent exceptions)
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Any intentional departure from an active rule, CMS requirement, approved phase plan, or
agreed coding convention requires explicit discussion, agreement, and documentation before
implementation. Silent exceptions are not permitted.
```
### R-PROCESS-18 — Partial-PASS / conditional-progression recording
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
If work proceeds after a part-PASS, no-PASS, waiver, temporary exception, or conditional
agreement, the condition and reason must be recorded in the phase notes, handover notes,
or commit body as appropriate, so later work does not mistake it for an unconditional PASS.
```
### R-PROCESS-19 — D-SUM compute-disabled observe-only proof
**Status:** confirmed  
**Source:** CMS07-G.6A D-SUM-11 counter-model proof; user/designer-approved process-rule clarification  
**Last revised:** 2026-06-17
**Statement:**
```text
For any phase that introduces or changes a D-SUM diagnostic compute gate, exit evidence
must include a compute-disabled observe-only proof for that D-SUM gate. Build at least
one relevant configuration with the corresponding CNR3_DIAG_COMPUTE_DSUMxx_* macro
undefined, confirm that the build compiles and links cleanly with that D-SUM's
observation bodies compiled out, and confirm that all pre-existing non-D-SUM
behavioural/selftest coverage still passes unchanged. Any behavioural divergence under
the compute-disabled build is a defect in the diagnostic gate and must be fixed before
the phase is complete, not merely recorded as a result. A D-SUM-specific selftest may
no-op or be safely disabled when its compute macro is undefined, but the rest of the
relevant suite must continue to prove the same behaviour. This requirement applies to
every D-SUM diagnostic compute gate, not only the first implemented D-SUM counter.
```
### R-PROCESS-20 — Patch Delivery and Apply Protocol (PDAP)
**Status:** confirmed  
**Source:** user-directed standardisation of the established patch workflow (CMS07-G/H series); adopted at CMS07-H.2A  
**Last revised:** 2026-06-23
**Summary:** From CMS07-H.2A onward, each coding phase is delivered as a downloadable
unified-diff `.patch` file plus the exact git/build/test commands for the coordinator to
run in Visual Studio 2026 and push to GitHub. Inline before/after edit blocks for manual
application are NOT used for code delivery. Patches apply to the active development branch
(currently `dev_cache_manager`).
**Statement:**
The cycle for each phase has three stages.
**Stage 1 — the coder supplies (for review, before the coordinator applies):**
- a single unified-diff `.patch` file, `git apply`-compatible, named for the phase
  (replacement patches after review are versioned `_v2`, with a note on what changed);
- what the phase adds, and what it deliberately does not do (the deferral list);
- the key proof scenario and what the selftest proves;
- the coder's own sandbox validation, including:
  ```text
  git apply --check: PASS
  git apply --check --whitespace=error: PASS
  git diff --check: PASS
  isolated selftest build: PASS
  (expected normal / forced-fail / verbose selftest totals)
  ```
  Here "isolated selftest build: PASS" means the coder's sandbox-local isolated
  compile of the cache-core source from the uploaded baseline — it is the coder's
  own patch-validation evidence, NOT a substitute for the coordinator's required
  VS2026 Debug/Release builds, which remain the authoritative build evidence
  (Stage 2 and the commit `Verified:` block).
- the apply sequence (run from the repo root, on `dev_cache_manager`):
  ```text
  git status --short
  git apply --check <phase>.patch
  git apply <phase>.patch
  git diff --check
  git status --short
  ```
- the build + test run. In VS2026, build both **DEBUG and RELEASE** for (a) project
  `cnr3` and (b) project `cnr3_cache_core_selftest`. Then run the selftest (the VS2026
  x64 binary just built):
  ```text
  cd /d "...\vapoursynth-cnr3\github\vs\cnr3"
  x64\Debug\cnr3_cache_core_selftest.exe 1>NUL
  echo exit_code=%ERRORLEVEL%
  x64\Release\cnr3_cache_core_selftest.exe 1>NUL
  echo exit_code=%ERRORLEVEL%
  x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL
  echo exit_code=%ERRORLEVEL%
  x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL
  echo exit_code=%ERRORLEVEL%
  ```
  This is the standard run, for a phase whose selftest count is N:
  ```text
  Debug   normal        -> total N, passed N, result PASS, exit_code 0
  Release normal        -> total N, passed N, result PASS, exit_code 0
  Release --force-fail  -> total N, passed N-1, failed 1, result FAIL,
                           first_failed_status invariant_violation, exit_code 1
  Release --verbose     -> total N, passed N, result PASS, exit_code 0 (preceded by
                           the per-scenario trace lines)
  ```
  Debug-normal and Release-normal both confirm the PASS (a Release build can expose
  optimiser-sensitive defects a Debug build hides, and a Debug build can catch
  assertion/iterator misuse a Release build runs past, so both are run). The Release
  force-fail run proves the harness can actually fail. The Release verbose run adds the
  human-readable trace. When the phase changes a D-SUM compute gate, additionally perform
  the R-PROCESS-19 macro-off run (rebuild with the relevant `CNR3_DIAG_COMPUTE_DSUMxx_*`
  undefined; expect non-D-SUM behaviour unchanged).
**Stage 2 — the coordinator reviews and runs:**
- load-bearing phases (atomics, pin-record, trigger, recovery bounds, AS2) are read first,
  reviewed against the spec, before `git apply`;
- the coordinator applies the patch, builds both DEBUG and RELEASE of the `cnr3` and
  `cnr3_cache_core_selftest` projects, runs the four-way selftest, and posts the actual
  console output back to the coder.
**Stage 3 — the coder supplies the commit, after the coordinator posts passing results:**
- the Visual Studio-style commit message: title, body (what the phase adds, the explicit
  deferrals), and a `Verified:` block with the actual run results;
- a final pre-commit check (`git diff --check`; `git status --short`);
- the coordinator commits only the modified `src/...` files (NOT the `.patch` file) and
  pushes; `git status --short` is clean afterward.
**Standing rules for the protocol:**
- one `.patch` per phase; a behaviour-adding phase changes the selftest count by exactly 1,
  while an audit / comment / corrective phase leaves it unchanged;
- on any `git apply --check` failure, changed-files mismatch, or test-count mismatch, work
  STOPS and is reported — it is not forced or improvised;
- the coordinator runs the Windows build; the coder does not. The coordinator's local
  four-way run plus the commit `Verified:` block is the authoritative build evidence for
  the phase (the coder need not re-flag inability to run the Windows build each phase);
- the order is always propose -> review -> approve -> apply -> test -> commit; code is
  never applied before review and approval.
**Source baseline and patch generation (so `git apply --check` evidence is genuine):**
- The coder validates each patch with `git apply --check` against a **local source baseline
  held in its working environment**, not against ad-hoc web reads of individual files. The
  coordinator therefore **uploads the current source baseline** (the relevant `src/` files,
  or the whole `src/` directory plus the `vs/cnr3` project files) at the start of a working
  session, pinned to a known commit (e.g. the current HEAD code commit), and the coder
  confirms its baseline marker matches before producing any patch.
- The coder **self-maintains** this source baseline patch-to-patch within a session:
  generate each patch from the current baseline; validate it without advancing the
  baseline; and only **advance the maintained baseline after the coordinator reports the
  patch applied, built, tested, and accepted** — so a later phase is never based on a draft
  patch that was revised or rejected.
- The coordinator **re-syncs** the coder (re-uploads the authoritative source) whenever
  drift is possible or detected: anything applied out-of-band; a manual edit after applying
  a patch; the branch moving in a way the coder did not produce; an unexpected
  `git apply --check` failure; a changed-files or selftest-count mismatch; the start of a
  new chat/session; or at a major checkpoint (e.g. after a milestone phase) for a clean
  anchor. A per-phase re-upload is not required when all patches are produced in-session,
  applied exactly, and the posted results match.
- Patches are generated with **wider unified-diff context (`git diff -U10`, ten context
  lines each side, not the default three)** so that `git apply --check` fails cleanly on
  base drift rather than risking application near the wrong code. The apply/check commands
  are unchanged; the wider context is carried inside the patch file itself.
**Clarification (v2.7) — live-getFrame plugin-only keystone phases.** Once getFrame
integration begins (the cache↔pixel keystone), a phase may be PLUGIN-ONLY: it changes the
`cnr3` plugin (the live getFrame path) without adding a cache-core selftest. For such a
phase:
- (a) the coordinator additionally builds the **`cnr3.dll` plugin**, not only the selftest
  projects (the four-way selftest run still applies and confirms the cache core is
  undisturbed);
- (b) the phase's BEHAVIOURAL proof is a **coordinator-side A/B acceptance harness** — a
  parameterised `.vpy` plus a `.bat` byte-compare — held INDEPENDENTLY of the coder (the
  coder is told its shape, not given the files, unless debugging), NOT a selftest-count
  change;
- (c) the selftest count legitimately STAYS UNCHANGED even though real behaviour was added
  — this is a THIRD category alongside "behaviour-adding (+1 selftest)" and
  "audit/comment/corrective (unchanged)". A plugin-only phase that adds live behaviour but
  no selftest is expected and correct, and must be recorded as such (so the unchanged count
  is not mistaken for a no-op).
This clarification adds no new obligation beyond the existing protocol; it records how the
established run/count discipline applies once live-getFrame phases exist.
### R-PROCESS-21 — Proven code stays proven (no change to proven behaviour or internals without prior approval)
**Status:** confirmed  
**Source:** CMS07-K.1D keystone reorientation (a patch that passed the four-way was withdrawn for silently rewriting a proven function); user-approved process rule  
**Last revised:** 2026-06-23
**Statement:**
```text
Once code is proven by a committed selftest (or otherwise explicitly accepted as proven),
its behaviour AND its internals are frozen: neither may be modified without an explicit,
visible proposal of that specific change AND approval BEFORE any code is written.

A passing test run after internals were changed is NOT proof of equivalence — it shows only
that the existing tests did not DETECT a difference, not that there is none (the test may
not exercise the changed path). Treating a green four-way (or any green run) as evidence
that proven behaviour was preserved across an internals change is the specific error this
rule exists to prevent.

If reusing a proven operation appears to require touching it, that requirement is itself a
design / scope question to RAISE for approval — never a licence to modify. Routing around
the conversation is prohibited: do not silently swap a proven function's internals; do not
re-implement the proven operation in parallel; do not hand-set state to MIMIC a proven
path without going through it; and do not broaden a patch's scope into a proven file
undisclosed (it is a disclosure/approval breach even if the result happens to be correct).

When a change would endanger proven code, the response is WITHDRAW-and-reconsider, not
patch-and-fix: a fix-list applied to an unsafe patch implicitly accepts the unsafe
approach. The safe design is often found by stepping back to question the premise (does
this need to touch the proven thing at all?), not by making the dangerous approach safer.

This rule generalises R-PROCESS-08 (no unapproved action), R-PROCESS-14 (minimal,
agreed-only change), and R-PROCESS-17 (no silent exceptions) to the high-value case of
already-proven code, and complements the CMS Design Compliance Review (which checks that
unchanged helpers invoked by a change still follow the prevailing CMS). It applies to the
proven pixel path, the proven cache core, and any other committed-and-selftested code.
```
### R-PROCESS-22 — Lifecycle / API contracts are settled from documentation, not from observed test behaviour
**Status:** confirmed  
**Source:** CMS07-K.1C/K.1D/K.1E keystone (the arInitial/arAllFramesReady return contract and the rpStrictSpatial->rpGeneral dependency declaration were settled from the R76 header and the CMS, not from test runs); user-approved process rule  
**Last revised:** 2026-06-23
**Statement:**
```text
A VapourSynth (or other platform) lifecycle or API contract — for example the
arInitial/arAllFramesReady activation-reason contract, what may be requested or retrieved
in which activation, a frame-return obligation, a dependency / request-pattern declaration
(e.g. rpStrictSpatial vs rpGeneral), or a threading / ownership guarantee — must be
established from the AUTHORITATIVE DOCUMENTATION (the R76 VapourSynth4.h header and the
prevailing CMS), not inferred from the fact that a build happened to compile, a test
happened to pass, or a path happened to work.

"It worked in testing" is NOT evidence that an undocumented assumption is correct.
Undocumented-but-works is version-fragile and is especially dangerous under fmParallel,
where an assumption that held under one schedule can fail under another. A contract relied
upon must be traceable to documented behaviour.

When a lifecycle/API contract is unclear, resolve it against the documentation; if the
documentation is itself silent, ambiguous, or incomplete, stop and ask (R-AUTH-03) before
relying on it. This is a process/epistemics obligation owned here; the specific resolved
contracts live in the prevailing CMS (e.g. VS-LIFECYCLE-01 in CMS §9A.1, the frame-return
contract in CMS §9A.1.1, and the rpGeneral declaration in CMS §9.7.7).
```
## 3A.6 Architecture and salvage rules
### R-ARCH-01 — Reuse the existing per-frame processing boundary (no parallel pixel algorithm)
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
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
### R-ARCH-02 — Pixel/frame processing performs no cache or scheduling actions
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
The pixel/frame-processing layer performs pixel-layer work only. It must not own or
perform any cache, recovery, request, pinning, eviction, checkpoint, hot-zone, or
scheduling action, and must not read or mutate cache state. Such actions belong
solely to the cache manager.
```
### R-ARCH-03 — Cache manager owns cache state and policy, not the pixel algorithm
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
The cache manager owns cache state and policy: pools, the ordered index, pins and the
pin-list, hot zones, checkpoint flags, store/prune, recovery planning, validation, and
cache diagnostics. It must not contain pixel-algorithm logic. (Cache-independent
utilities — response-table construction and memory diagnostics — are not part of the
cache manager; see R-ARCH-04.)
```
### R-ARCH-04 — Response tables and memory diagnostics are cache-independent utilities
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Response-table construction and memory diagnostics are cache-independent utilities. They
sit outside both the pixel-algorithm layer and the cache manager, and must not be made to
depend on cache state unless explicitly redesigned and agreed.
```
### R-ARCH-05 — Salvage is the second step only
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Salvage from old .txt code happens only after the new cache-core
ownership/pinning/eviction discipline is proven in isolation.
```
### R-ARCH-06 — CNR2 pixel maths may be salvaged; its recovery logic must not
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
CNR2 (the AviSynth vsCnr2 source) may be used as guidance for PIXEL maths only —
response-table construction, the int64-accumulator weighted blend, downsampled-luma, and
in-compute scene-change detection. Its serialized recovery/predecessor shortcut
(substituting source[n-1] when the previous output is absent) must NEVER be adopted; that
approximation is exactly what the CMS cache-and-recovery architecture exists to replace.
```
### R-ARCH-07 — Old .txt code is not copied into new files without per-case approval (emphasised during the first milestone)
**Status:** confirmed  
**Source:** CNR3 Register-Owned Rules Review — Reconciled v3.3; user-approved suggested wording  
**Last revised:** 2026-06-13
**Statement:**
```text
Old .txt code is not copied into new .h/.cpp files without explicit per-case approval.
This applies throughout development; it is enforced most strictly during the first
cache-core milestone, where the new core is being proven in isolation.
```
## 3A.7 Layout guidance — not authoritative rules
### GUIDANCE-ARCH-01 — Indicative file separation (the coder confirms the actual layout)
**Status:** guidance only — not an authoritative rule  
**Source:** Layout guidance from reconciled review v3.3; non-authoritative  
**Last revised:** 2026-06-13
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
## 3A.8 Retired-fact entries retained for anti-regression
### R-RETIRED-01 — Deferred/optional non-checkpoint pinning is superseded
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
The old model in which non-checkpoint pinning was deferred or emergency-only is completely
superseded by mandatory consumer-pinning, as defined in the latest prevailing CMS.
```
### R-RETIRED-02 — Held-ref-only predecessor reservation is superseded
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
Held-ref-only predecessor reservation as the default architecture is completely superseded by
consumer-held pins on per-invocation pin-lists, as defined in the latest prevailing CMS.
```
### R-RETIRED-03 — Checkpoint-as-pin reasoning is retired
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
Any reasoning or wording that treats a checkpoint as a pin is completely retired. A checkpoint
is now a separate eviction-protection flag with its own retention rule; there is exactly one pin
concept (consumer-claim), as defined in the latest prevailing CMS.
```
### R-RETIRED-04 — Hot-zone-as-active-findability guarantee is superseded
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
The model in which hot zones guaranteed active-frame findability is superseded. Instead, pins
provide active liveness; hot zones are now prune-policy hints only, as defined in the latest
prevailing CMS.
```
### R-RETIRED-05 — Blanket bounded-warmup source window is superseded
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
The old blanket backward source-request window (and its "bounded-warmup" framing) is
completely superseded and is replaced by the dissolved source-window model — request
source N plus genuine holes only — as defined in the latest prevailing CMS.
```
### R-RETIRED-06 — CMS06.x / H15.6B is not an active continuation path
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
CMS06.x / H15.6B coding is completely discontinued as an active path, in and beyond the
CMS07 restart. It is recorded here as a completely retired work item to prevent accidental
resumption.
```
### R-RETIRED-07 — Old strict-streaming bridge is not final fmParallel output authority
**Status:** retired / superseded fact retained for anti-regression  
**Source:** CMS07.0 restart rule reconciliation; supersession/retirement record  
**Last revised:** 2026-06-13
**Statement:**
```text
The old strict-streaming bridge (including next_needed / prev_output-style authority) is
not the final fmParallel output authority and is recorded here as completely superseded/retired
in that role.
The authoritative model now lives in the latest prevailing CMS.
```
## 3A.9 Resolved candidate entries
### R-CAND-01 — Prefer ASCII-only code-update instructions — PROMOTED to R-PROCESS-15
**Status:** resolved candidate disposition retained for anti-regression  
**Source:** Prior-context candidate review; disposition resolved in v3.3  
**Last revised:** 2026-06-13
**Statement:**
```text
Candidate resolved: promoted to R-PROCESS-15. Retained here only as a resolved candidate record; do not keep as a separate active rule.
```
### R-CAND-02 — Avoid unnecessary unrelated code/comment/layout/name changes — PROMOTED to R-PROCESS-14
**Status:** resolved candidate disposition retained for anti-regression  
**Source:** Prior-context candidate review; disposition resolved in v3.3  
**Last revised:** 2026-06-13
**Statement:**
```text
Candidate resolved: promoted to R-PROCESS-14. Retained here only as a resolved candidate record; do not keep as a separate active rule.
```
### R-CAND-03 — Large diagnostic prints may be one line where practical — MERGED into R-PROCESS-11
**Status:** resolved candidate disposition retained for anti-regression  
**Source:** Prior-context candidate review; disposition resolved in v3.3  
**Last revised:** 2026-06-13
**Statement:**
```text
Candidate resolved: merged into R-PROCESS-11. Retained here only as a resolved candidate record; do not keep as a separate active rule.
```
### R-CAND-04 — Compile-time constexpr proof gates; no correctness behind disabled guards — MERGED into R-PROCESS-12
**Status:** resolved candidate disposition retained for anti-regression  
**Source:** Prior-context candidate review; disposition resolved in v3.3  
**Last revised:** 2026-06-13
**Statement:**
```text
Candidate resolved: merged into R-PROCESS-12. Retained here only as a resolved candidate record; do not keep as a separate active rule.
```
### R-CAND-05 — PASS response includes commit message — MERGED into R-PROCESS-04
**Status:** resolved candidate disposition retained for anti-regression  
**Source:** Prior-context candidate review; disposition resolved in v3.3  
**Last revised:** 2026-06-13
**Statement:**
```text
Candidate resolved: merged into R-PROCESS-04. Retained here only as a resolved candidate record; do not keep as a separate active rule.
```
## 4. Document B — current-state work plan and the prevailing phase
`Document_B_CNR3_Restart_Work_Plan_and_Current_State_<version>.md`
(This replaces the old volatile "Document C". It is the current-state document and is
re-issued each session.)
**Genericisation note (v2.7).** This section was written for the restart, when the
immediate work was the "first cache-core milestone." That milestone is long complete; the
project is well past it — the scalar pixel pipeline, the scalar→native bridge, and the
caller-supplied pixel path are proven, and the cache↔pixel / getFrame **keystone** is now
under way (see the current Document B, e.g. v3.2.9, for the actual current build state).
Read §4.1–§4.3 below as the GENERAL contract for Document B — "state the controlling
authority, the CURRENT phase and its proof obligations, the rule-enumeration requirement,
the do-not-implement list, and the hard gates" — with the first-milestone content (§4.2
items 2/3/5) being the restart-era INSTANCE of that contract, not the current task.
Document B is the volatile current-state document and is re-issued each session against
whatever phase prevails; the CMS remains the stable design authority.
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
   [v2.7 note: this is the restart-era state; the current build state is whatever the
   prevailing Document B records (currently the keystone), confirmed from the repository.]
3. First milestone (CMS07.0 §11, per D30): prove the cache-manager core in isolation,
   no VapourSynth wiring — data structures, single-lock skeleton, pin/pin-list,
   composite eviction predicate + bounded prune. Proof obligations: pin/unpin
   balance = 0, lookup-ref balance = 0 (acquired == released + transferred), no leaks,
   no double-free, eviction never selects a pinned/checkpoint/in-zone slot, shutdown
   clear() releases everything with a warning on any non-zero pin.
   [v2.7 note: the first milestone is COMPLETE. This item is retained as the restart-era
   instance of "the current phase and its proof obligations"; for the prevailing phase's
   proof obligations see the current Document B.]
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
   [v2.7 note: "no getFrame wiring until the core is proven" was satisfied at the C.14A
   cache-core milestone; getFrame integration is now the live keystone work. The other
   items remain in force. The current Document B carries the prevailing do-not-implement
   list, which also includes the proven-code-stays-proven rule (R-PROCESS-21).]
6. Hard gates: diagnostics-as-hard-gate; the §11-style design-compliance review per
   phase (CMS07.0 §9A.8, 17-item checklist incl. AS-register match).
7. Proposed-layout expectation: the coder proposes the file/header/structure layout
   (text) for review before any file creation.
   [v2.7 note: the layout was proposed, signed off, and built long ago; retained as the
   restart-era instance.]
```
### 4.3 Required current-state precedence wording
Document B states plainly that if it ever conflicts with CMS07.0, CMS07.0 wins; and
that Document B is volatile (re-issued per session) whereas CMS07.0 is the stable
design authority.
---
## 5. Companion design document rule
The current CMS (`cnr3_cache_manager_design_v7_8.md`, CMS07.8) is included UNCHANGED as the
controlling design authority. It is not edited as part of pack production; if a design
change is needed, the CMS is revised on its own and the pack regenerated against it. No
earlier CMS06.x design document is included as an active input. (The literal CMS filename
here is the CURRENT controlling-design file and is updated by hand at each CMS bump per the
CMS07.0 Reading Rule clause (c).)
---
## 6. New chat starter prompt
The pack includes a short starter prompt for a new chat. It must:
- name CMS07.0 as controlling and the precedence rule;
- list the in-scope CORE attachments (Document A, Document B, the current CMS, this spec,
  the coder intro, the manifest) and the expected COMPANION attachments (.txt snapshot,
  CNR2 reference), and note the old CMS06-era B/C docs are historical only;
- make the ORDER explicit: the first substantive action is **prevailing-rules
  enumeration for sign-off**; the layout proposal follows ONLY after the rule register
  is settled or the user explicitly defers it;
- state the no-action rule: **no file renaming, no file creation, no salvage copy, and
  no getFrame integration without explicit user instruction.**
---
## 7. Maintenance rules
- **7.1 Updating Document A** — when project-wide context, standing rules, or the
  architecture story change. A CMS revision is such a trigger. (A change to a
  register-owned rule — e.g. adding R-PROCESS-21/22 — is made in §3A here, then Document A
  is regenerated to reproduce it.)
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
[ ] Document A's context section is a VERBATIM reproduction of §3.2 (the canonical
    context master) — copied exactly, not paraphrased, summarised, regenerated, or
    thinned. Any divergence from §3.2 FAILS review and is corrected to match (§3.2.0).
[ ] Document A states the CMS07.0 supersession story and old/new separation.
[ ] Document A's standing rules are CMS07.0-aligned (no old-architecture rule stated
    as current).
[ ] Salvage policy present as a Document A section (not a separate document).
[ ] §3A Prevailing Rules Register present; if populated, Document A's rules section
    faithfully reproduces it (a mismatch FAILS review); if pending, both say so and the
    pack is labelled DRAFT (§3A.1). The reproduction includes ALL current process rules
    (through R-PROCESS-22); a Document A missing any current rule (e.g. R-PROCESS-20/21/22)
    FAILS review and is regenerated.
[ ] Pack status correct: DRAFT if §3A pending; RELEASED only if §3A populated AND
    Document A regenerated to match it (§3A.1).
[ ] Rule dispositions retained in §3A (confirmed/modified/superseded/retired/candidate);
    retired/superseded rules kept with status, not deleted (§3A.2).
[ ] Core pack files vs companion coding-start attachments distinguished; expected
    companion attachments (.txt snapshot, CNR2 reference) noted in manifest/starter
    prompt (§2).
[ ] Starter prompt and Document B state the no-action rule: no file renaming, file
    creation, salvage copy, or getFrame integration without explicit user instruction.
[ ] Starter prompt states the order: rule enumeration/sign-off first; layout proposal
    only after the register is settled or explicitly deferred.
[ ] Any rule change since the last pack was made IN §3A (the register), with the
    revision date bumped — not only in Document A.
[ ] Document B states controlling authority, current phase + proof obligations,
    rule-enumeration requirement, do-not-implement list, hard gates.
[ ] The current CMS included unchanged (currently cnr3_cache_manager_design_v7_8.md).
[ ] This Production Spec (current version, v2.7) included.
[ ] Coder restart introduction included.
[ ] No old Document B/C, no old CMS06.x design docs, as ACTIVE inputs (archive only).
[ ] Precedence + stop-and-ask wording present.
[ ] Manifest: reading order, hard precedence, SHA256 checksums, pack version.
[ ] Pack reviewed against §8 success criteria.
```
