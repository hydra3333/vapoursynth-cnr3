# Document A — CNR3 Project Context and Standing Rules (CMS07.0 restart)

*** v3.13 MINIMAL ADVANCE — 2026-07-04 — ratifies TWO rules into the §3A register (audit verdict: document
HEALTHY, advanced in place, NOT regenerated). No existing rule is changed. ***

### R-PROCESS-24 — Diagnostic emissions flush per line (RATIFIED; formerly the proposed R-PROCESS-2x)
Diagnostic emissions flush per line by default (the cnr3_diag_write_line default). no_flush is permitted
ONLY for a high-volume family that flushes once at a bounded end-point, and NEVER on a path that can
precede a failure bail or that carries ordering-sensitive output. Rationale (both load-bearing):
(1) CRASH-SURVIVAL — unflushed stdio buffers are lost when the host tears down on a bail; per-line flush
guarantees bytes are on the wire before `return nullptr`. (2) MULTITHREAD ORDERING — per-line flush keeps
interleaved diagnostic output in true temporal order, which datetime-ordered analysis depends on.

### R-PROCESS-25 — Propose before transforming proven code (RATIFIED; codifies the A2/D-2 precedent)
ANY modification of an existing proven line — however small, however behaviourally pure — must be
PROPOSED (in the confirm report or a follow-up question) and designer-sanctioned BEFORE it appears in a
patch. "Behaviourally identical" is the DESIGNER'S determination to make, not the coder's to assume. The
strictly-additive alternative is the default reflex. Precedents on record: DIAG.2b amendment A2 (the
CNR3_DSUM05_FAIL macro transform — proposed, sanctioned, then implemented: the CORRECT sequence) and
DIAG.3b finding D-2 (a pure-function hoist implemented without prior proposal — behaviourally safe,
retro-sanctioned, boundary restated: the sequence violation this rule now prohibits).

### R-PROCESS-26 — Canonical 4-way build/run: invocation form and folder layout are FIXED (RATIFIED)
*** v3.14 MINIMAL ADVANCE — 2026-07-09 — ratifies R-PROCESS-26 into the §3A register. No existing rule changed. ***
The selftest build/run has a SINGLE canonical form; deviations (invented run-folders, non-canonical paths, copied
binaries, renamed flags) are errors to be reconciled against this rule, not followed. This rule exists because a
coder chat twice produced mangled run instructions after a long reliable run.

BUILD (VS2026): both configs of BOTH projects (cnr3_build_config.h is shared -> all four rebuild):
  cnr3  Debug|x64 ; cnr3  Release|x64 ; cnr3_cache_core_selftest  Debug|x64 ; cnr3_cache_core_selftest  Release|x64

BINARIES (FIXED location; do NOT invent run-folders, do NOT copy exes):
  E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug
  E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release

RUN (cd once, then RELATIVE paths; output to console for copy/paste):
  cd /d "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"
  x64\Debug\cnr3_cache_core_selftest.exe 1>NUL      & echo Debug normal exit_code=%ERRORLEVEL%
  x64\Release\cnr3_cache_core_selftest.exe 1>NUL    & echo Release normal exit_code=%ERRORLEVEL%
  x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL & echo Release forced-fail exit_code=%ERRORLEVEL%
  x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL & echo Release verbose exit_code=%ERRORLEVEL%

EXPECTED (N = the phase's committed selftest count; carry the last known-green N, do NOT trust a coder-supplied
number -- currently N=56):
  Debug normal   -> N/N PASS exit 0 ; Release normal -> N/N PASS exit 0 ;
  Release --force-fail -> N-1 pass, 1 expected FAIL (invariant_violation), exit 1 ; Release --verbose -> N/N PASS exit 0.
Rationale: Debug AND Release both run (Release exposes optimiser-sensitive defects Debug hides; Debug catches
assertion/iterator misuse Release runs past). Force-fail proves the harness can fail. Verbose adds the trace.
CONSTRAINTS: no arbitrary test-run folders; no files created in VS-managed or repo folders; no copying .exe/.dll to
scratch; outputs to console (not files unless asked); flags are exactly --force-fail-for-harness-proof / --verbose.
HARNESS OWNERSHIP (restated for adjacency): the .vpy/.bat harnesses (S-/B-/S9-series) are the DESIGNER's deliverable;
the coder delivers the source patch and runs the 4-way SELFTESTS. Harness proofs (macro-off byte-identical, macro-on
hand-checks, div-zero, mixed-gating on live footage) are run by the designer/coordinator, not the selftest orchestrator.

(Prior v3.12 content follows unchanged:)


**Version:** Document A v3.11 — RESUME-state regeneration generated from Production Spec **v2.14**
populated §3.2 (canonical context, reproduced verbatim) and §3A (Prevailing Rules Register, reproduced
in full). **v3.12 change over v3.11 (currency touch only; NO register-owned rule text changed):** advances the build-state note past a COMPLETE MARSHALLING-OPTIMISATION ARC — twelve value-identical implementation-only levers (AVX2, 0A, 0B, 3a.1, 3b.1, 3a.2, A-lite, C1, Repack, F/3c, Staging, E) cut per-frame native<->scalar marshalling to ~1/5, CUMULATIVE ~-80% (93,914 -> ~18,660 samples), committed through CNR3-OPT-LeverE-scenechange-local-accumulator, **56/56** (forced-fail 55/56 exit 1). Controlling design UNCHANGED at **CMS07.15** (the arc is implementation-only — no CMS rule/constant/AS-scope change; every lever was value-identical, P-series preserved). **NEXT = coordinator call: Lever B (allocation pooling, needs an fmUnordered thread-safety proof) OR the diagnostics arc (D-SUM).** Doc-set advanced: DELTA v4.23 / Document B v3.12 / Role Handover v1.16 / Reviewer Intro v3.9 / Coder Restart Intro v6.7 / Future Investigations v7.16 / PixelPath Map v0.4 / Production Spec v2.16. **STANDING ENVIRONMENT NOTE ADDED (build protocol):** the build is now **x64-ONLY** with **`/arch:AVX2` a HARD REQUIREMENT** (EnableEnhancedInstructionSet=AdvancedVectorExtensions2 on Release+Debug of BOTH cnr3.vcxproj and cnr3_cache_core_selftest.vcxproj; Win32/x86 removed from the .slnx). AVX2 hard-faults on pre-AVX2 CPUs (Haswell 2013 / Excavator 2015+) — documented in build_config header/README/release notes. This is an environment fact, NOT a registered process rule. (Also: diagnostic `/Qvec-report:2` flags may sit DIRTY in vs/cnr3/*.vcxproj working trees during optimisation work — revert before committing project files.) No register-owned rule changed; the arc added no rule. [Prior v3.11 note retained:] **v3.11 change over v3.10 (currency touch only; NO register-owned rule text changed):** advances the build-state note to the **W.3-closed** seam — the live cache-pressure wiring arc **W.1→W.2→W.3 is COMPLETE** (committed CMS07-W.3-combined-live-store-prune-helper; **55/55**, forced-fail 54/55 exit 1; designer eviction-proof live A/B harness PASS), controlling design **CMS07.15** (`cnr3_cache_manager_design_v7_15.md`; additive §7.5 store-status contract over 07.14), and **NEXT = the diagnostics arc** (D-SUM telemetry) ahead of the real-footage campaign (coordinator decision 2026-06-30). No register-owned rule changed; W.3 added no rule. [Prior v3.10 note retained:] **v3.10 change over v3.9 (currency touch only; NO register-owned rule text changed):** folds in the VS2026
build-environment facts confirmed from the uploaded `vs.zip` solution audit (2026-06-29): the corrected
toolset/SDK (PlatformToolset **v145**, WindowsTargetPlatformVersion **10.0.28000.0** — supersedes older
v143 memory), and the per-file PROJECT-MEMBERSHIP MAP (which file is built into the DLL project vs the
selftest project vs both), recorded in the build-protocol section below as a standing environment note so
every future patch knows the project boundary without re-auditing. No rule changed. [Prior note retained as
history:]

**v3.9 change over v3.8 (currency touch only; NO register-owned rule text changed):** (a) advances the
controlling-design pointer to **CMS07.14** (`cnr3_cache_manager_design_v7_14.md`) — CMS07.14 is an ADDITIVE
clarification over 07.13 adding §7.4 (independent checkpoint-retention trigger), §7.5 (live wiring
contract), §7.6 (arInitial observation prerequisite); no existing rule/constant/AS-scope changed; (b) records
that the Step 0 joint review is CLOSED (the live cache-pressure wiring contract is settled in CMS §7.4-§7.6;
provenance CNR3_Step0_Findings_Register_r5_FINAL_CLOSED.md); (c) advances the next-action to the WIRING
implementation: W.1 (the §7.4 checkpoint-retention trigger primitive, cache-core + selftest) -> W.2 (hot-zone
observation wiring @arInitial) -> W.3 (the combined live store-and-prune helper) -> real-footage -> diagnostics
-> fmParallel. The process-rule set R-PROCESS-01..23 and canonical context (§3.2) remain UNCHANGED. [Prior note
retained as history:]

**v3.8 change over v3.6 (currency touch only; NO register-owned rule text changed):** (a) corrects the
**Generation source** pointer field to Spec **v2.14** (a v3.6 miss caught in the coder handover review — the
header had advanced but this concrete field had not); (b) advances all version pointers to the merged pack
(Spec v2.14 / Document B v3.8 / Role Handover v1.12 / Reviewer Intro v3.5 / Coder Restart Intro v6.4 /
DELTA v4.14 / CMS07.13 unchanged); (c) records the banked **Step 0** next-action: a JOINT CMS SENSIBILITY/GAP
REVIEW for hot-zone + prune live wiring BEFORE any wiring patch (do not assume the CMS is reliable as-is
merely because the componentry is proven). The complete process-rule set R-PROCESS-01..23 and canonical
context (§3.2) remain UNCHANGED. [Prior note retained as history:]

**v3.6 change over v3.5 (currency touch only; NO register-owned rule text changed):** (a) advances the
generation-source pointer to Spec **v2.12** and confirms the controlling-design pointer **UNCHANGED at
CMS07.13** (P.11C implemented the scene-change design the CMS already specified — a state advance, not a
rule change; the CMS carries only an additive implementation-state note); (b) advances the build-state
note to **P.11C SCENE-CHANGE ARC CLOSED (.1-.5)** at **53/53 selftests**, scene detection wired+proven
across branch-a/c/d, the live getFrame dispatch feature-complete with scene handling; (c) advances the
next-phase note to **live cache-pressure WIRING** (hot-zone observation @arInitial then pruning into the
live getFrame path — logic built+proven but not yet live-wired; the live prune-trigger contract needs a
designer+coder review first; single-activation now, concurrent prune in fmParallel) → real-footage →
diagnostics (condensed 4-phase) → fmParallel. The complete process-rule set R-PROCESS-01 through
R-PROCESS-23 and the canonical context (§3.2) are reproduced UNCHANGED (P.11C introduced no new rule).

**v3.5 change over v3.4:** (a) adds the **CNR3 Design Alignment and Escalation Charter** as
§3A.5.0 (the three-way governance discipline — designer/coder/coordinator; RULE-DEVIATION / CMS-GAP
escalation; bidirectional cross-checking; durable recording — which entered the Spec at v2.10 and CMS at
§0A, and which v3.4 predated); (b) advances the generation-source pointer to Spec v2.11 and the
controlling-design pointer to **CMS07.14** (`cnr3_cache_manager_design_v7_14.md`); (c) advances the
build-state note to **branch-(d) recovery arc COMPLETE (D.1-D.5)**. No register-owned rule text is
otherwise changed; the canonical context (§3.2) is reproduced unchanged (it is enduring and did not move).
This Document A reproduces the complete current process-rule set R-PROCESS-01 through R-PROCESS-23 plus
the charter §3A.5.0.  
**Date:** 2026-06-27  
**Role:** Human-facing front door to the CNR3 project. It preserves the canonical
project context and reproduces the register-owned standing rules for a new chat or human
maintainer.
**Generation source:** `CNR3_Handover_Pack_Production_Spec_v2_14.md`  
**Controlling design authority:** the latest prevailing CMS, currently **CMS07.13**
(`cnr3_cache_manager_design_v7_14.md`). CMS07.14 is the controlling design authority; it supersedes
CMS07.12/.11/.10/.9/.8 and all earlier. CMS07.11 added §0A (the Design Alignment and Escalation Charter,
mirrored here as §3A.5.0); CMS07.12 clarified the bounded-search report semantics in §9.5; CMS07.13 made
the materialized-floor-is-the-foundation invariant explicit in §9.5 — none added a design rule, AS scope,
or section-number change. References to "CMS07.0" in reproduced rule text below mean the latest prevailing
CMS (currently CMS07.13), per the CMS's own version-neutrality rule; specific section pointers are
re-checked against the prevailing version.  
**Precedence:** if this document conflicts with the latest prevailing CMS on a design
point, the CMS wins. If this document conflicts with Production Spec §3.2 or §3A on
canonical context or register-owned rules, the Production Spec wins and this document is
corrected.
**Version-line note (what changed from the documents this supersedes):** This is a
RESUME-state regeneration of the handover set. The CNR3 build is well advanced: the scalar
pixel pipeline, the scalar→native bridge, and the caller-supplied pixel path are proven,
the isolated cache core is proven, the cache↔pixel / getFrame keystone is complete, and the
**branch-(d) recovery arc is COMPLETE (D.1-D.5)** AND the **P.11C SCENE-CHANGE ARC is now CLOSED
(.1-.5)** — committed through phase **CMS07-P.11C.5-scene-cut-checkpoint-recovery-anchor-proof** at
**53/53 selftests** (forced-fail 52/53 exit 1). The live getFrame dispatch is feature-complete
across all four branches (cache-hit, fresh-start, predecessor-present, recovery), and the full
recovery arc is proven: D.1 single-hole, D.2 multi-hole + bounded-window refusal, D.3
floor-fresh-start, D.4 adopt-skip / first-in-best-dressed primitives, D.5 recovery-pin-survives-
prune. The only deferred confidence is real concurrent (fmParallel) scheduling, bounded to the
fmParallel validation phase. **Next phase: STEP 0 CLOSED (wiring contract settled in CMS §7.4-§7.6). W.1→W.2→W.3 DONE (live cache-pressure wiring arc COMPLETE; 55/55 + eviction-proof live harness PASS). NEXT: the diagnostics arc (D-SUM telemetry) → real-footage campaign → fmParallel**
(gates the first real-footage test) — NOT a recovery phase. See Document B (current state) for the
live build state and the next phase. The controlling design is now **CMS07.14**: earlier revisions
(CMS07.1 §6.6 monotonic checkpoint flag; CMS07.3 §9.6 minimal recovery path; CMS07.7 §9.7 keystone
predecessor-sourcing; CMS07.8 §9.7.7 rpGeneral + §9A.1.1 frame-return contract; CMS07.9 pre-compute
adopt-and-skip normative; CMS07.10 R-LIFECYCLE correction to §9A.1.1) are carried forward, and
**CMS07.11 added §0A (the Design Alignment and Escalation Charter, mirrored here as §3A.5.0);
CMS07.12 clarified the bounded-search report semantics in §9.5; CMS07.13 made the
materialized-floor-is-the-foundation invariant explicit in §9.5** — none of CMS07.11/.12/.13 added a
design rule, AS scope, or section-number change. The Production Spec is now **v2.11**, which advanced
the currency pointers to CMS07.13 and the recovery-complete build state; the register-owned rule set
(R-PROCESS-01 through R-PROCESS-23) and the canonical context (§3.2) are unchanged from v2.10.
**v3.5 regeneration:** this Document A adds the charter §3A.5.0 (which v3.4 predated — it entered the
Spec at v2.10), advances all version pointers to Spec v2.11 / CMS07.13, and advances the build-state
note to recovery-arc-complete. It reproduces the complete current process-rule set R-PROCESS-01 through
R-PROCESS-23 plus the charter §3A.5.0. The canonical context (§3.2) is reproduced unchanged.
**Reading order for a coder restart chat:** read the coder restart introduction first,
then CMS07.13, then this Document A and the current Document B. "Front door"
means this is the human-facing orientation document within the pack; it does not override
the restart introduction's start-here sequencing. NOTE: this is a RESUME, not a fresh
start — the current Document B carries the live build state and the next phase (the
keystone); do not treat the original "first milestone / rename files to .txt" framing as
the current task. The first milestone is long complete; the live task is the cache↔pixel /
getFrame keystone.
---
## 1. Canonical project context
The following canonical context block is reproduced from Production Spec §3.2. It is
copied as the authoritative project-context master for this handover checkpoint.
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
---
## 2. Standing rules
The following rules section is generated from Production Spec §3A. The Production Spec
§3A remains the authoritative master for register-owned rules. CMS-defined rules are
handed off to the latest prevailing CMS and are not restated as independent rules here.
## 3A. Prevailing Rules Register (master list of OWNED rules — lives HERE)
**Register status:** `POPULATED — FIRST AUTHORITATIVE REGISTER-OWNED RULESET`  
**Population source:** `CNR3_Register_Owned_Rules_Review_RECONCILED_v3_3_for_the_coder.md`  
**Population date:** 2026-06-13 (additive process rules R-PROCESS-19/20/21/22 added at
later spec versions; see each rule's Last-revised date and the spec front-matter change
notes)  
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

### 3A.5.0 CNR3 Design Alignment and Escalation Charter (governance preface)
**Status:** confirmed (designer+coder+coordinator, 2026-06-27)
**Statement:**
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

**Build environment (confirmed 2026-06-29 from the `vs.zip` solution audit; standing reference for all patches).**
- **Solution:** `vs/cnr3/cnr3.slnx`, containing exactly TWO projects.
- **Toolset / SDK:** PlatformToolset **v145**, WindowsTargetPlatformVersion **10.0.28000.0** (VS2026). This
  SUPERSEDES any older v143 reference in prior notes/memory; follow the uploaded project files as the target.
- **Project files are LF-only** in the repo, consistent with the LF canonical-base rule.
- **Project 1 — `cnr3.vcxproj`** (DynamicLibrary -> `cnr3.dll`; defines `CNR3_EXPORTS;_WINDOWS;_USRDLL`): the
  live plugin DLL. Includes `cnr3_arInitial.cpp`, `cnr3_arAllFramesReady.cpp`, `vapoursynth-Cnr3.cpp`,
  `cnr3_instance_config.cpp`, `cnr3_cache_diagnostics.cpp`, `cnr3_memory_diagnostics.cpp`, plus the shared
  cache/frame/pixel support files.
- **Project 2 — `cnr3_cache_core_selftest.vcxproj`** (Application -> `cnr3_cache_core_selftest.exe`; defines
  `CNR3_SELFTEST_CONSOLE;_CONSOLE`): the isolated console selftest. Includes `cnr3_cache_core_selftest_main.cpp`,
  `cnr3_cache_core_selftest.cpp`, and the cache-core / owned-frame / response-table / frame-processing support.
  It does NOT include the live-dispatch files (`cnr3_arInitial.cpp`, `cnr3_arAllFramesReady.cpp`,
  `vapoursynth-Cnr3.cpp`) and does NOT include the diagnostics / instance-config TUs.
- **PER-FILE MEMBERSHIP (governs every patch's build impact):**
  - `cnr3_cache_core.cpp` / `.h` ............ BOTH projects.
  - `cnr3_frame_processing.cpp` ............. BOTH projects.
  - `cnr3_cache_core_selftest.cpp` / `.h` ... BOTH projects.
  - `cnr3_cache_core_selftest_main.cpp` ..... selftest project ONLY.
  - `cnr3_arInitial.cpp`, `cnr3_arAllFramesReady.cpp`, `vapoursynth-Cnr3.cpp` ... DLL project ONLY.
  - `cnr3_cache_diagnostics.cpp`, `cnr3_memory_diagnostics.cpp`, `cnr3_instance_config.cpp` ... DLL project ONLY.
- **Consequence for cache-core patches (e.g. the W.1 §7.4 trigger):** because `cnr3_cache_core.*` and
  `cnr3_cache_core_selftest.*` compile into BOTH projects, cache-core changes MUST compile and behave
  correctly in BOTH the DLL and the selftest-exe builds — they must not depend on symbols that exist only in
  the DLL project (VapourSynth API, the diagnostics TUs). The four-way run already exercises both; this note
  makes the constraint explicit so a DLL-only `#include` or symbol does not slip into a cache-core file and
  break the selftest-exe link. A purely live-dispatch (DLL-only) phase touches only DLL-only files and does
  not change the selftest count.
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
### R-PROCESS-23 — Patch validation must match the target environment, and proof level must be labelled honestly
**Status:** confirmed  
**Source:** CMS07-K.1E.2/K.1E.3 keystone delivery (a five-revision patch saga: a hunk-count corruption, a stale base, a context-narrowing non-fix, an LF-vs-CRLF apply failure, and an API3-vs-API4 compile error — each rooted in validating against an ASSUMED environment rather than the real one); user-approved process rule  
**Last revised:** 2026-06-24
**Statement:**
```text
A patch is validated only against the EXACT target environment, never an assumed one. This
complements R-PROCESS-20 (PDAP delivery mechanics) and is owned as a distinct rule because it
governs environment conformance and proof-level honesty, not the review cadence.

(1) CANONICAL BASE. The patch base is the current committed source obtained via the GitHub
branch-ZIP path (the repository's stored form). It is NOT a local-working-tree copy, NOT
GitHub-raw single-file reads, and NOT an inferred/reconstructed source. The coordinator
standardises on the branch-ZIP upload so the base is deterministic; the coder confirms its
baseline marker before producing any patch (R-PROCESS-20 baseline discipline).

(2) LINE ENDINGS. This repository stores source as LF (core.autocrlf=true: LF in the index,
CRLF in the Windows working tree; no .gitattributes). Do NOT change this setting mid-project
(it would renormalise every file). Patches are emitted to MATCH THE STORED (LF) FORM and
validated LF-against-LF in a line-ending-matched temporary repo. An LF-seeded validation that
"passes git apply" is NOT valid evidence for the CRLF working tree, and a CRLF patch is NOT the
fix — it matches the converted form, the wrong layer. The coordinator applies on the CRLF
working tree with autocrlf mediating; `git apply --ignore-whitespace` is the standing,
understood fallback (by construction the only difference at that point is the EOL conversion).

(3) COMPILE BEFORE GREEN. "Applies" is not "compiles" and "compiles" is not "runs." For any
phase touching the plugin (the live getFrame path), the patch is not validated until the
affected build has been COMPILED against the real target headers (R76 / API4) in BOTH Debug and
Release — not merely applied. API-shape assumptions (e.g. VSVideoInfo::format is a VALUE member
in API4, not a pointer; const VSFrame* vs VSFrame*; copyFrame/addFrameRef/freeFrame signatures)
are checked against the R76 headers before delivery (this is the R-PROCESS-22 obligation applied
at the call site), and any new VSVideoInfo/VSVideoFormat access is audited for the same shape in
one pass.

(4) PROOF-LEVEL HONESTY. Each claim is labelled at its true proof level: "patch applies
(LF-against-LF)", "DLL compiles (Debug+Release)", "selftest/harness passes". A green check in a
sandbox that does not match the target's line endings and headers is not a proof for the target;
say what was and was not validated.

(5) DIAGNOSE THE FAILURE CLASS, DO NOT NARROW CONTEXT. When a patch fails to apply, diagnose the
CLASS of failure first — malformed/hunk-count, wrong base, line-ending mismatch, whitespace,
path, or genuine source conflict — and fix the cause. Narrowing hunk context to make `git apply`
stop complaining is prohibited: it masks the cause (it gives the tool fewer lines to disagree
with) and can land a hunk in the wrong place. Reducing context is allowed only with a deliberate,
stated reason, never as a response to a failed apply.

This rule generalises R-PROCESS-20 (delivery/baseline) and R-PROCESS-22 (contracts from
documentation) to the mechanical delivery layer, and exists to prevent the "passes for me, fails
for you" class of failure.
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
---
## 3. Salvage policy
- Salvage is the second step, only after the new cache-core ownership/pinning/eviction
  discipline is proven in isolation.
- Likely salvageable, subject to explicit approval and verification: response-table
  creation, memory diagnostics, and the pixel/frame-processing layer including the
  explicit-predecessor boundary.
- CNR2 may be used as guidance for pixel maths only: response-table construction,
  int64-accumulator weighted blend, downsampled-luma, and in-compute scene-change
  detection.
- CNR2 recovery/predecessor logic must not be adopted. Its serialized
  `last_frame != n-1 -> source[n-1]` approximation is exactly what the CMS cache and
  recovery architecture replaces.
- Old `.txt` code is not copied into new `.h/.cpp` files without explicit per-case
  approval.
---
## 4. Continuity note
This Document A is for the CMS07.0 restart. The old CMS06-era Document B/C, old
CMS06.x design documents, old reconciliation notes, and old proof-phase state are
historical archive only unless explicitly pulled in for a narrow verification or salvage
question. They are not active inputs to the restart.
