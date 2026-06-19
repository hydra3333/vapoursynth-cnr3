# CNR3 — Development RESUME on the CMS07.2 cache architecture

*(Coder restart introduction — paste this at the start of a new memoryless chat, ahead of
the attached handover pack files. This is a RESUME of an in-progress, proven build, NOT a
fresh start.)*

This chat **resumes** CNR3 cache-manager development on the CMS07 cache architecture. The
build is already well advanced and proven phase-by-phase. Do not treat older CNR3
memories, prior chats, or old source layout as active implementation authority unless the
attached pack says so.

**You are resuming, not starting.** The cache core is built and proven through phase
**CMS07-H.1A** (28 of 28 isolated cache-core selftests passing). Your job is the NEXT
phase, after first confirming the build state from the repository. Do NOT re-propose the
file layout, do NOT rename files, do NOT rebuild proven phases, and do NOT treat any
"first milestone / rename to .txt" framing from older documents as current.

CNR3 is a VapourSynth API4-only, integer-YUV recursive temporal chroma stabiliser. Its
load-bearing difficulty is:

```text
output[N] depends on source[N] and already-filtered output[N - 1]
```

The predecessor is the already-filtered **output**, not merely `source[N - 1]`. Modern
VapourSynth scheduling may request frames out of display order, so CNR3 needs a correct
cache/recovery architecture before any parallel-performance work can be trusted. That
architecture is the CMS07 design, and it **completely supersedes** the previous CMS06.x
cache design and proof path.

---

## 1. Attachments expected for this resume

Do not proceed from this introduction alone. The CORE handover files are:

```text
1. This introduction:
   CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v3_2.md

2. Controlling design:
   cnr3_cache_manager_design_v7_2.md
   (CMS07.2 — controlling design authority. Supersedes CMS07.1 and CMS07.0.)

3. Project context / standing rules:
   Document_A_CNR3_Project_Context_and_Standing_Rules_v3_2.md
   (reproduces Production Spec §3.2 canonical context + the §3A register,
    including R-PROCESS-19.)

4. Current work plan + BUILD STATE:
   Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_2.md
   (RESUME-state: current build state through H.1A, the working method, the
    salvage inventory, the next phase, the do-not-implement list. READ THIS for
    where the build actually is.)

5. Production Spec:
   CNR3_Handover_Pack_Production_Spec_v2_4.md
   (canonical context master §3.2 + populated §3A register-owned rules, incl.
    R-PROCESS-19.)

6. Diagnostics spec:
   cnr3_diagnostics_specification_v1_3.md
   (subordinate to the CMS and §3A.)

7. Manifest:
   CNR3_Handover_Pack_RESUME_v3_2_MANIFEST.md
   (reading order and pack contents.)
```

NOT part of the durable pack (do not treat as authority):

```text
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7.2.md
  (NON-NORMATIVE companion to CMS07.2; deferred tuning questions only; ignore for
   implementation.)
- the old .txt reference source under src/superseded_by_v7/ (see Document B §8.5
  salvage inventory; salvage is a later step, per-case approval only).
```

If **CMS07.2 itself is not attached**, stop and say so. You may comment on this
introduction, but you cannot enumerate rules or proceed without the controlling design.

---

## 2. FIRST action — confirm the build state from the repository

Before anything else (before enumerating rules, before proposing the next phase), confirm
the build state from the authoritative source — the repository — not from these documents'
say-so. This re-establishes the project's "prove it, do not assert it" discipline from
your first action:

```text
1. Read the recent git log (~25 commits). Confirm the latest commit is the
   CMS07-H.1A bounded recovery search scaffold, and the F-series and G-series
   phases listed in Document B section 4 are present.

2. Read src/cnr3_build_config.h; confirm CNR3_EDIT_VERSION reads:
       CMS07-H.1A-as1-bounded-recovery-search-scaffold-proof

3. Build + run the isolated cache-core selftest and confirm:
       Debug   normal       -> 28/28 PASS, exit 0
       Release normal       -> 28/28 PASS, exit 0
       Release --force-fail-for-harness-proof -> 27/28, 1 FAIL, exit 1
       Release --verbose    -> 28/28 PASS, exit 0

If any do not match, STOP and report the discrepancy before doing anything else.
```

Repository: `https://github.com/hydra3333/vapoursynth-cnr3` (local working tree
`E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github`). Builds in Visual Studio 2026, x64.

---

## 3. WHEN IN DOUBT — raise it for review; do not decide alone

This project runs through a **designer/coordinator review loop**, and that is exactly what
has kept the build sound. There are two different "ask" situations, and BOTH mean stop and
raise it — not proceed on your own judgement:

```text
A. CMS gaps (R-AUTH-03): if the CMS is silent, ambiguous, or incomplete on an
   implementation point — STOP and ask. Do not guess, do not improvise.

B. Design-coordination questions (broader than the CMS): if you have ANY doubt about
       - the direction of the work,
       - which phase comes next or a phase's scope,
       - whether a test case is adequate / genuinely discriminating,
       - a phase's exit bar or per-phase goal,
       - whether something is in scope for the current phase,
   then RAISE IT to the user/coordinator for review before acting. "The CMS does not
   forbid it" is NOT license to decide a direction, scope, or test-design question
   unilaterally.
```

Concretely: the build cadence is **propose → review → approve → code**, never idea
straight to committed code. The coordinator runs a separate designer review of each
proposal against the spec. If you are unsure whether to raise something, raise it. A
question is cheap; a wrong unilateral call on scope or test design is not.

---

## 4. Hard precedence and old/new separation

```text
If CMS07.2 conflicts with, or is merely unclear in alignment with, prior material:
    CMS07.2 wins unless the user explicitly says otherwise.

If CMS07.2 itself is silent, ambiguous, or incomplete on an implementation point:
    stop and ask (see section 3). Do not guess and do not improvise.
```

References to "CMS07.0" as controlling (in reproduced rule text) mean the latest prevailing
CMS — currently **CMS07.2**. Specific CMS section pointers are version-specific and must be
re-checked against CMS07.2. All existing pre-CMS07 cache code/design is superseded: old
code is salvage reference only, per Document B §8.5 and the §3A salvage rules. The old
CMS06-era Document B decision log and Document C volatile-state snapshot are out of scope
as active inputs — excluding them is intentional, to keep stale CMS06 assumptions from
re-entering.

---

## 5. The five highest-risk traps (do not conflate old and new concepts)

```text
1. Treating pinning as optional/deferred:
   Wrong. Consumer-held pinning is the mandatory correctness baseline.

2. Reintroducing held-ref-only predecessor reservation:
   Wrong. Superseded by consumer-held pins on a per-invocation pin-list.

3. Thinking of a checkpoint as a pin:
   Wrong. A checkpoint is a separate eviction-protection flag. There is exactly one pin
   concept: consumer-claim.

4. Treating hot zones as active-frame findability guarantees:
   Wrong. Pins provide active liveness. Hot zones are prune-policy hints only.

5. Reintroducing a blanket bounded-warmup source window:
   Wrong. Recovery uses the CMS two-phase model: request source N plus genuine holes only.
```

Nothing may be implemented that obstructs the fmParallel end-goal unless it is an
unavoidable, explicitly recorded, temporary stepping-stone preserving the path to fmParallel.

---

## 6. Engineered guards you must respect

### 6.1 Atomic-scope register (AS1-AS7)
CMS07.2 defines an atomic-scope register, AS1-AS7. It is designer-owned and inviolable.
Every cache critical section is enumerated there, including what happens inside one lock
acquisition and in what order. Implement these scopes exactly — do not shrink, split,
merge, reorder, or reinterpret them. If implementation reveals a needed operation the
register does not cover, raise it to the user; do not invent an ad-hoc lock scope.

### 6.2 V5 firewall
VapourSynth frame reference counts are internally atomic — and that **gives you NOTHING
over lock scopes.** It protects a single `addFrameRef`/`freeFrame` only. It is not a licence
to take a pin outside the cache lock or to shrink any critical section. The protected thing
is the multi-step cache decision (find-then-pin, decide-then-detach), not the refcount bump.

### 6.3 VapourSynth lifecycle rule
Any source frame retrieved in `arAllFramesReady` must have been requested in `arInitial` of
the same activation. Request planning happens at `arInitial`; do not retrieve source frames
that were not requested for that activation. (Binding when getFrame integration arrives.)

### 6.4 Lock / ownership disciplines already held at every phase
```text
- ONE cache-wide non-recursive mutex; RAII guard only.
- Decide INSIDE the lock; do the slow part (especially freeFrame) OUTSIDE it.
- freeFrame is NEVER called inside the cache lock (detach under lock, free after).
- pin-and-record is indivisible; pin-list capacity reserved BEFORE the lock.
- checkpoint is a flag, not a pin; hot zones are hints, not liveness.
```
Document B section 7 carries the full list. These are inviolable.

---

## 7. §3A is populated — rule enumeration is verification, not first population

The Production Spec §3A Prevailing Rules Register is populated. Enumerate the prevailing
rules back to the user, but the purpose is **verification/reconciliation**, not first
population. Distinguish:

```text
REGISTER-OWNED rules:
    authority, pack governance, process, architecture/salvage, retired-fact entries,
    already recorded in Production Spec §3A (and reproduced in Document A v3.2).

CMS-DEFINED / HANDED-OFF rules:
    design / cache-core / reference-count / VapourSynth-lifecycle / recovery / constant /
    instrumentation / atomic-scope rules defined in CMS07.2. NOT duplicated, indexed, or
    re-IDed in §3A.
```

If you find an apparent missing rule, conflict, ambiguity, or candidate, raise it for user
decision (section 3). Do not silently treat it as controlling.

---

## 8. Salvage policy (the second step, not now)

Salvage happens AFTER the cache core is proven complete (through the C.14A aggregate proof
— see Document B section 8). The salvage inventory in **Document B section 8.5** lists the
old `.txt` reference files and what is/ is not salvageable. Key points:

```text
- HIGH-VALUE salvage (study/adapt, do not rewrite): the pixel-processing core
  (frame_internal_processing — note its explicit-previous-output boundary already
  matches the CMS), the vscnr2-style response tables, and the memory diagnostics.
- TREAT WITH CAUTION: cnr3_common.h (stale CMS06 assumptions may ride along).
- QUARANTINE (do not open for ideas): the old cache managers — they embody the retired
  concepts and are the main route by which they creep back.
- CNR2 / vscnr2 is PIXEL-MATHS reference only. NEVER salvage CNR2 recovery/predecessor
  logic — that approximation (substitute source[n-1] when previous output is absent) is
  exactly what CMS07 replaces.
```

Every salvage is per-case, inspected, and explicitly approved (R-ARCH-07). Old `.txt` code
is not copied into new files without approval.

---

## 9. Process rules that matter immediately (orientation only — §3A holds the wording)

```text
- Comments: concise, useful, never safety-incomplete.
- Code updates: exact before/after blocks with uniquely matchable context; ASCII-safe.
- Phase/SubPhase numbering continues the CMS07 line (next is H.2A).
- PASS includes a Visual Studio-style commit title/body unless the user says otherwise.
- Diagnostics are hard gates; a partial fail is a FAIL.
- Diagnostic output to stderr, never stdout; human summaries must be readable.
- Diagnostics are compile-time gated (compute gate + print gate; print subordinate to
  compute). A D-SUM compute-gate change requires the R-PROCESS-19 macro-off observe-only
  proof.
- Observation gates observe only; behaviour-changing scaffolds use SCAFFOLD_* markers,
  not DIAG_* names.
- No printing or long-running work inside locked/atomic scopes.
- Minimise unrelated diffs; do not silently paraphrase agreed rules.
- Any override requires explicit discussion, agreement, and documentation.
```

Consult §3A directly for authoritative text. Document B section 6 describes the full
working method (read-first patches, the four-way test run, genuine-failure-mode tests,
count discipline, --verbose trace, the diagnostics module boundary).

---

## 10. Your first response in this resume chat

Please respond with:

```text
a) Confirmation that you understand this is a RESUME at phase H.2A (not a fresh start),
   the old/new separation, and the no-action / propose-review-approve rule.

b) The result of confirming the build state from the repository (section 2): the latest
   commit, the edit-version marker, and the four-way selftest results. If you cannot run
   the build, say so and confirm from the git log and source instead.

c) Any questions or ambiguities in CMS07.2 or the pack — and any direction/scope/
   test-design questions you want reviewed (section 3).

d) An enumerated prevailing-rules list for verification/reconciliation, marking each item
   REGISTER-OWNED (§3A) or CMS-DEFINED / HANDED-OFF (CMS07.2).

e) Your proposed approach for CMS07-H.2A (recovery anchor pin-record) as a text proposal
   for review — NOT code yet, and NOT applied. (The previous chat's H.2A patch was never
   reviewed and never applied; it is discarded. Regenerate fresh.)
```

Do not assume any rule carries over silently. Do not code, create files, rename files,
copy salvage, or wire getFrame without explicit user discussion, agreement, and
instruction. When in doubt about anything — direction, scope, or test design — raise it
for review (section 3).
