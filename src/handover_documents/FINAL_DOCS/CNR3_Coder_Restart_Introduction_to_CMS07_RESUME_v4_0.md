# CNR3 — Development RESUME on the CMS07.3 cache + pixel architecture

*(Coder restart introduction — paste this at the start of a new memoryless chat, ahead of
the attached handover pack files. This is a RESUME of an in-progress, proven build that is
well past its cache-core milestone and most of the way through its pixel pipeline. It is
NOT a fresh start, and it is NOT "early cache-core work" — read the state below carefully.)*

This chat **resumes** CNR3 development on the CMS07.3 architecture. The build is far
advanced and proven phase-by-phase: the cache core is complete and proven, the entire
scalar pixel decision pipeline is complete and proven, and the native byte-buffer
access/bridge layer is complete and proven. Your job is the NEXT phase — which now crosses
into **real VapourSynth frame memory and getFrame/cache integration** — after first
confirming the build state from the repository.

Do not treat older CNR3 memories, prior chats, or old source layout as active
implementation authority unless the attached pack says so. In particular, do **not** treat
any "first milestone / rename to .txt / propose the file layout / next phase is H.2A"
framing from older introductions as current — that is years of phases out of date.

CNR3 is a VapourSynth **API4-only**, **integer-YUV-only** recursive temporal chroma
stabiliser (VHS/analogue chroma restoration). Its load-bearing difficulty is:

```text
output[N] depends on source[N] and already-filtered output[N - 1]
```

The predecessor is the already-filtered **output**, not merely `source[N - 1]`. Modern
VapourSynth scheduling may request frames out of display order, so CNR3 needs a correct
cache/recovery architecture before any parallel-performance work can be trusted. That
architecture is the CMS07.3 design, and it **completely supersedes** the previous CMS06.x
cache design and proof path. The eventual end-goal is `fmParallel`.

---

## 0. WHERE THE BUILD ACTUALLY IS (read this before anything else)

**This is the single most important orientation point, because this introduction has
historically been pasted while badly out of date.** Confirm the live state from the
repository (section 2), but the expected state is:

```text
Cache core:          COMPLETE and proven through the C.14A aggregate milestone.
Scalar pixel chain:  COMPLETE and proven — P.1A (response tables) -> P.2A (config/geometry)
                     -> P.3A (int64 weighted blend) -> P.4A (downsampled-luma) -> P.5A
                     (signed-difference/lookup/blend bridge) -> P.6A (chroma-plane traversal).
Scalar->native:      COMPLETE and proven — P.7A (source-luma downsample traversal) -> P.8A
                     (native byte-plane access) -> P.9A (native luma downsample bridge).
Latest committed:    CMS07-P.9A-native-luma-downsample-bridge-proof
Selftest count:      41/41 PASS (forced-fail 40/41 exit 1; verbose 41/41).
```

So: the cache core is done, the pixel maths is done, and the bridge from native byte
buffers through the scalar pixel chain is done — **all on synthetic/scalar buffers inside
an isolated selftest harness, with every arithmetic step verified against the vsCnr2.cpp
source.** What is NOT yet done, and is your forward work, is the crossing into **real
VapourSynth machinery**:

```text
NEXT (not yet proposed):
  1. REAL VAPOURSYNTH FRAME-MEMORY ADAPTATION — bind the proven P.8A native byte-plane
     layer to actual VS frame memory: getReadPtr/getWritePtr/getStride, real per-plane
     dimensions (getFrameWidth/getFrameHeight, plane-dependent due to chroma subsampling),
     and the decision on typed row pointers (reinterpret_cast<const uint16_t*>) vs the P.8A
     memcpy approach against the real frame-memory contract and its alignment guarantees.
  2. SCENE-CHANGE accumulation (the deferred original "P.6A" content).
  3. getFrame / source-lifecycle / cache integration — where the proven CACHE CORE meets
     the proven PIXEL CHAIN. This is the architectural keystone of the whole project.
```

**Document B (current version, see section 1) section 8 is authoritative for state and the
forward roadmap. The Designer/Reviewer Role Handover (current version) carries the review
disciplines, the pixel-layer reference, the accuracy rule, the recorded deliberate
divergences, and a phase-by-phase review checklist with a "live hunting list" for exactly
these next phases. Read both. This introduction deliberately does NOT duplicate their
content — duplicated facts drift out of sync, and this project has already been bitten by
that.**

---

## 1. Attachments expected for this resume

Do not proceed from this introduction alone. The handover pack is (current versions — take
the highest version number present in the handover-documents folder if any differ from the
names below):

```text
1. This introduction:
   CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v4_0.md

2. Controlling design:
   cnr3_cache_manager_design_v7_3.md
   (CMS07.3 — controlling design authority. Supersedes all earlier CMS07.x and CMS06.x.)

3. Project context / standing rules:
   Document_A_CNR3_Project_Context_and_Standing_Rules_v3_2.md
   (reproduces Production Spec canonical context + the §3A register.)

4. Current work plan + BUILD STATE:
   Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_2_7.md
   (RESUME-state: current build state through P.9A, the working method, the salvage
    inventory, the NEXT phases, the do-not-implement list. READ THIS for where the build
    actually is and what comes next. If a higher v3.2.x exists, use it.)

5. Production Spec:
   CNR3_Handover_Pack_Production_Spec_v2_6.md
   (canonical context master + populated §3A register-owned rules, incl. R-PROCESS-19/20.)

6. Diagnostics spec:
   cnr3_diagnostics_specification_v1_4.1.md
   (subordinate to the CMS and §3A.)

7. Designer/Reviewer Role Handover:
   CNR3_Designer_Reviewer_Role_Handover_v1_4.md
   (HOW the design/review role is performed: the review disciplines D1-D15, the decision
    heuristics, the pixel-layer reference confirmed against vsCnr2.cpp, the governing
    accuracy rule, the recorded deliberate divergences, and the pixel-arc review checklist
    + live hunting list. If a higher v1.x exists, use it. This is your map for what the
    designer/coordinator will scrutinise — and what you should self-check before proposing.)

8. Manifest:
   CNR3_Handover_Pack_RESUME_v3_2_MANIFEST.md
   (reading order and pack contents; may lag the version numbers above — the individual
    documents' own version headers win.)
```

NOT part of the durable implementation authority (do not treat as controlling):

```text
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7_3.md
  (NON-NORMATIVE companion to CMS07.3; deferred tuning questions only; ignore for
   implementation.)
- the old .txt reference source under src/superseded_by_v7/ (see Document B §8.5 salvage
  inventory; salvage is per-case, approval-only).
- older Document B / Role Handover / introduction versions, and the old CMS06-era decision
  log and volatile-state snapshot — out of scope as active inputs (intentionally, to keep
  stale assumptions from re-entering).
```

If **CMS07.3 itself is not attached**, stop and say so. You may comment on this
introduction, but you cannot enumerate rules or proceed without the controlling design.

---

## 2. FIRST action — confirm the build state from the repository

Before anything else (before enumerating rules, before proposing the next phase), confirm
the build state from the authoritative source — the repository — not from these documents'
say-so. This re-establishes the project's "prove it, do not assert it" discipline from your
very first action:

```text
1. Read the recent git log (~25 commits). Confirm the latest commit is
       CMS07-P.9A: prove native luma downsample bridge
   and that the pixel-arc commits P.1A through P.9A are present, on top of the C.14A
   aggregate cache-core milestone.

2. Read src/cnr3_build_config.h; confirm CNR3_EDIT_VERSION reads:
       CMS07-P.9A-native-luma-downsample-bridge-proof

3. Build + run the isolated cache-core selftest (it now also carries the pixel-proof
   tests) and confirm the four-way:
       Debug   normal                              -> 41/41 PASS, exit 0
       Release normal                              -> 41/41 PASS, exit 0
       Release --force-fail-for-harness-proof      -> 40/41 PASS, 1 FAIL, exit 1
       Release --verbose                           -> 41/41 PASS, exit 0
   In the --verbose trace, confirm the pixel scenarios P.1A through P.9A are all present
   and passing (not just the total count) — the trace is the legible proof that the
   composed pixel pipeline is intact.

If any of these do not match, STOP and report the discrepancy before doing anything else.
```

Repository: `https://github.com/hydra3333/vapoursynth-cnr3` (local working tree
`E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github`, branch `dev_cache_manager`). Builds
in Visual Studio 2026, x64. The selftest builds/runs in `vs\cnr3`
(`x64\Debug\` and `x64\Release\cnr3_cache_core_selftest.exe`).

**One operational item is load-bearing at the very next phase:** the real-frame-memory work
is the FIRST phase that will need `src/cnr3_frame_processing.cpp` to be a member of the
**`cnr3` plugin** project, not just the `cnr3_cache_core_selftest` project (it has been a
selftest-project member since P.3A). If it is not already in the plugin project, that must
be added (via the Visual Studio 2026 GUI: Add Existing Item) before the real-frame patch, or
the plugin build will fail to link — the selftest project hit exactly this at P.3A. Confirm
or raise it.

---

## 3. WHEN IN DOUBT — raise it for review; do not decide alone

This project runs through a **designer/coordinator review loop**, and that is exactly what
has kept the build sound across every phase. There are two different "ask" situations, and
BOTH mean stop and raise it — not proceed on your own judgement:

```text
A. CMS gaps (R-AUTH-03): if the CMS is silent, ambiguous, or incomplete on an
   implementation point — STOP and ask. Do not guess, do not improvise.

B. Design-coordination questions (broader than the CMS): if you have ANY doubt about
       - the direction of the work,
       - which phase comes next or a phase's scope,
       - whether a test case is adequate / genuinely discriminating,
       - a phase's exit bar or per-phase goal,
       - whether something is in scope for the current phase,
       - whether something diverges from vsCnr2 and, if so, whether that divergence is
         intended (the project has recorded deliberate divergences — see the Role Handover
         accuracy rule — and reproducing vsCnr2's integer arithmetic bit-exactly is the
         default elsewhere),
   then RAISE IT to the user/coordinator for review before acting. "The CMS does not
   forbid it" is NOT license to decide a direction, scope, or test-design question
   unilaterally.
```

Concretely: the build cadence is **propose → review → approve → code**, never idea straight
to committed code. The coordinator runs a separate designer review of each proposal against
the spec and the vsCnr2.cpp source. If you are unsure whether to raise something, raise it.
A question is cheap; a wrong unilateral call on scope, test design, or a silent divergence
from the reference is not.

This has been borne out repeatedly: the strongest phases were the ones where the coder
surfaced a doubt (a reachability question, an integer-division quirk, an edge-handling
divergence) for review rather than deciding it alone. That instinct is the project's main
safety mechanism — keep it.

---

## 4. How this phase of work differs from the cache-core era (orientation)

The cache-core phases (F/G/H/C series, through C.14A) proved concurrency-correctness:
atomic lock scopes, pin/checkpoint/prune discipline, recovery contiguity. The pixel phases
(P.1A–P.9A) proved arithmetic-correctness: exact-integer reference vectors cross-checked
against vsCnr2.cpp, with no tolerance. **Your forward work is a third kind of correctness:
real-frame-memory layout, VapourSynth lifecycle, and the integration that connects the two
proven halves.** The risks shift accordingly:

```text
- Memory layout: real byte strides, plane pointers, 8-bit uint8_t* vs 16-bit uint16_t*,
  and alignment. P.8A/P.9A proved the byte-stride LOGIC on synthetic buffers and chose
  memcpy precisely to avoid premature alignment assumptions; the real-frame phase must
  decide typed row pointers vs memcpy against VapourSynth's actual frame-memory contract.
- Lifecycle/ownership: the VapourSynth two-phase request model (arInitial / arAllFramesReady),
  frame reference counting, and the cache's pin/recovery discipline meeting real getFrame.
- Integration: the predecessor MUST be the explicit previous filtered output supplied by
  the cache/recovery layer — NEVER source[N-1]. Substituting source[N-1] is the exact
  approximation CMS07 was built to replace; it is prohibited (R-ARCH-06).
```

The proof surface changes too: real-frame and integration phases cannot be proven purely by
scalar reference vectors. Part of each proposal should be **how the phase will be proven**
(synthetic VS-style frame buffers? a lifecycle-sequence proof? what genuinely discriminates
a correct implementation from a plausible-but-wrong one?) — raise the proof approach for
review along with the scope.

---

## 5. Hard precedence and old/new separation

```text
If CMS07.3 conflicts with, or is merely unclear in alignment with, prior material:
    CMS07.3 wins unless the user explicitly says otherwise.

If CMS07.3 itself is silent, ambiguous, or incomplete on an implementation point:
    stop and ask (see section 3). Do not guess and do not improvise.
```

References to "CMS07.0" (or any earlier CMS) as controlling, inside reproduced rule text,
mean the latest prevailing CMS — currently **CMS07.3**. Specific CMS section pointers are
version-specific and must be re-checked against CMS07.3. All pre-CMS07 cache code/design is
superseded: old code is salvage reference only, per Document B §8.5 and the §3A salvage
rules.

---

## 6. The highest-risk traps (do not conflate old and new concepts)

These remain inviolable even though the cache core is proven — the integration phases
re-touch all of them.

### 6.1 Cache-core traps (still binding at integration)
```text
1. Pinning is the mandatory correctness baseline — never optional/deferred. There is
   exactly ONE pin concept: consumer-claim, recorded on a per-invocation pin-list.
2. A checkpoint is a separate eviction-protection FLAG, not a pin.
3. Hot zones are prune-policy HINTS only — pins provide active liveness, hot zones do not.
4. Recovery uses the CMS two-phase model: request source N plus genuine holes only — never
   a blanket bounded-warmup source window.
5. The predecessor is the previous filtered OUTPUT from the cache/recovery layer, NEVER
   source[N-1] (R-ARCH-06). This is the keystone correctness property of the whole design.
```

### 6.2 Pixel/arithmetic traps (now load-bearing as integration reads real pixels)
```text
6. Signed differences (current - previous) MUST stay signed int end-to-end into the table
   lookup — NO unsigned intermediate anywhere on the sample->diff->index path. An unsigned
   wrap turns a small negative difference into a huge positive index = catastrophe. (Proven
   correct through P.5A; preserve it when real pixels feed the path.)
7. The blend accumulator is int64; shift2 = depth<<1, shift = 1LL<<shift2, shift1 = shift>>1,
   reproduced bit-exactly. Do NOT "improve" definitional integer arithmetic.
8. Recorded deliberate divergences from vsCnr2 (do NOT "fix" back, do NOT flag as bugs — see
   the Role Handover accuracy rule for the full reasoning):
     - parameter scaling uses round-to-nearest (value*peak+127)/255, more accurate than
       vsCnr2's integer-factor truncation at 10/12/14-bit (identical at 8/16-bit);
     - P.4A downsample-luma CLAMPS edge taps rather than reading past the frame as vsCnr2
       does (vsCnr2 relies on AviSynth padding; CNR3 clamps for safety).
   The GOVERNING RULE: accuracy upgrades are permitted ONLY where vsCnr2 is accidentally
   lossy; definitional integer arithmetic is reproduced bit-exactly. When unsure which a
   given step is, treat it as definitional and ask.
```

Nothing may be implemented that obstructs the fmParallel end-goal unless it is an
unavoidable, explicitly recorded, temporary stepping-stone preserving the path to fmParallel.

---

## 7. Engineered guards you must respect

### 7.1 Atomic-scope register (AS1-AS7)
CMS07.3 defines an atomic-scope register, AS1-AS7. It is designer-owned and inviolable.
Every cache critical section is enumerated there, including what happens inside one lock
acquisition and in what order. Implement these scopes exactly — do not shrink, split, merge,
reorder, or reinterpret them. If implementation reveals a needed operation the register does
not cover, raise it to the user; do not invent an ad-hoc lock scope. (Becomes directly
relevant again when getFrame integration wires the cache to real requests.)

### 7.2 V5 firewall
VapourSynth frame reference counts are internally atomic — and that **gives you NOTHING over
lock scopes.** It protects a single `addFrameRef`/`freeFrame` only. It is not a licence to
take a pin outside the cache lock or to shrink any critical section. The protected thing is
the multi-step cache decision (find-then-pin, decide-then-detach), not the refcount bump.

### 7.3 VapourSynth lifecycle rule (VS-LIFECYCLE-01)
Any source frame retrieved in `arAllFramesReady` must have been requested in `arInitial` of
the same activation. Request planning happens at `arInitial`; do not retrieve source frames
that were not requested for that activation. **This becomes binding now, as getFrame
integration arrives — it is no longer hypothetical.**

### 7.4 Lock / ownership disciplines held at every phase
```text
- ONE cache-wide non-recursive mutex; RAII guard only.
- Decide INSIDE the lock; do the slow part (especially freeFrame) OUTSIDE it.
- freeFrame is NEVER called inside the cache lock (detach under lock, free after).
- pin-and-record is indivisible; pin-list capacity reserved BEFORE the lock.
- checkpoint is a flag, not a pin; hot zones are hints, not liveness.
```
Document B section 7 carries the full list. These are inviolable.

### 7.5 Category-B developer-alert (emission half of the C.13B guard)
At getFrame integration / error-mapping time, a hard cache status must map to a clean filter
failure (setFilterError) plus a bounded one-shot stderr developer-alert OUTSIDE locks. This
is the EMISSION half of what the C.13B recovery-contiguity guard already DETECTS (CMS
§9.6.4). Expected Category-A duplicate/adopt outcomes stay silent. The cache core itself
emits no stderr — that responsibility lands at integration.

---

## 8. §3A is populated — rule enumeration is verification, not first population

The Production Spec §3A Prevailing Rules Register is populated. Enumerate the prevailing
rules back to the user, but the purpose is **verification/reconciliation**, not first
population. Distinguish:

```text
REGISTER-OWNED rules:
    authority, pack governance, process, architecture/salvage, retired-fact entries,
    already recorded in Production Spec §3A (and reproduced in Document A v3.2).

CMS-DEFINED / HANDED-OFF rules:
    design / cache-core / reference-count / VapourSynth-lifecycle / recovery / constant /
    instrumentation / atomic-scope rules defined in CMS07.3. NOT duplicated, indexed, or
    re-IDed in §3A.
```

If you find an apparent missing rule, conflict, ambiguity, or candidate, raise it for user
decision (section 3). Do not silently treat it as controlling.

---

## 9. Salvage policy (per-case, approval-only)

The cache core is proven complete, so salvage of the high-value pixel-maths reference is now
permitted — but every salvage remains per-case, inspected, and explicitly approved
(R-ARCH-07). The salvage inventory is in **Document B section 8.5**. Key points:

```text
- The pixel-maths reference (vscnr2-style response tables, the frame_internal_processing
  core with its explicit-previous-output boundary) has largely BEEN salvaged/reimplemented
  through P.1A-P.9A. Check what is already active before re-salvaging anything.
- TREAT WITH CAUTION: cnr3_common.h (stale CMS06 assumptions may ride along).
- QUARANTINE (do not open for ideas): the old cache managers — they embody the retired
  concepts and are the main route by which they creep back.
- CNR2 / vscnr2 is PIXEL-MATHS reference only. NEVER salvage CNR2 recovery/predecessor
  logic — that approximation (substitute source[n-1] when previous output is absent) is
  exactly what CMS07 replaces.
- vsCnr2.cpp is the bit-exact reference for the pixel arithmetic. The canonical source the
  designer has been verifying against is:
  https://raw.githubusercontent.com/Asd-g/AviSynth-vsCnr2/refs/heads/main/src/vsCnr2.cpp
```

Old `.txt` code is not copied into new files without approval.

---

## 10. Process rules that matter immediately (orientation only — §3A holds the wording)

```text
- Comments: concise, useful, never safety-incomplete.
- Code updates: exact before/after blocks with uniquely matchable context; ASCII-safe.
- Phase numbering continues the CMS07 pixel line. The phase letters were deliberately
  re-lettered once: P.6A is CHROMA-PLANE TRAVERSAL, not the original roadmap's
  "scene-change" (scene-change is still deferred). Do not re-litigate phase names; follow
  Document B's current roadmap.
- Delivery uses the PDAP (R-PROCESS-20): a downloadable .patch per phase (git diff -U10
  from the committed baseline), with a Stage-1 validation block; the coordinator does the
  read-first review, applies, builds Debug+Release of BOTH cnr3 and cnr3_cache_core_selftest,
  runs the four-way, and commits the src/ files (NOT the .patch). Provide a Visual
  Studio-style commit title/body with each PASS.
- The four-way: Debug normal (N/N exit 0), Release normal (N/N exit 0), Release
  --force-fail-for-harness-proof ((N-1)/N exit 1), Release --verbose (N/N exit 0).
- A D-SUM compute-gate change requires the R-PROCESS-19 macro-off observe-only proof (a
  fifth run). Pixel phases have added no D-SUM gate, so no macro-off run; if a future phase
  touches a D-SUM compute gate, the macro-off proof returns.
- Diagnostics are hard gates; a partial fail is a FAIL. Output to stderr, never stdout.
  Diagnostics are compile-time gated (compute gate + print gate, print subordinate to
  compute). Observation gates observe only; behaviour-changing scaffolds use SCAFFOLD_*
  markers, not DIAG_* names.
- No printing or long-running work inside locked/atomic scopes.
- Minimise unrelated diffs; do not silently paraphrase agreed rules.
- Reference vectors are proven by EXACT integer equality (no tolerance) and, where
  countable, static_assert. Any numeric claim the coordinator can recompute, they will.
- Any override requires explicit discussion, agreement, and documentation.
```

Consult §3A directly for authoritative text. Document B section 6 and the Role Handover
describe the full working method (read-first patches, the four-way, genuine-failure-mode
tests, count discipline, the --verbose trace, the diagnostics module boundary, and the
pixel-arc review checklist).

---

## 11. Your first response in this resume chat

Please respond with:

```text
a) Confirmation that you understand this is a RESUME well past the cache-core milestone and
   most of the way through the pixel pipeline (cache core + scalar pixel chain + native
   bridge all proven through P.9A) — NOT a fresh start and NOT early cache-core work — and
   that you understand the old/new separation and the propose -> review -> approve rule.

b) The result of confirming the build state from the repository (section 2): the latest
   commit, the edit-version marker, the four-way selftest results, and confirmation that
   the --verbose trace shows the P.1A-P.9A pixel scenarios present and passing. If you
   cannot run the build, say so and confirm from the git log and source instead. Also note
   whether cnr3_frame_processing.cpp is already a member of the cnr3 PLUGIN project (or
   flag that it must be added before the real-frame phase).

c) Any questions or ambiguities in CMS07.3 or the pack — and any direction / scope /
   proof-approach / test-design questions you want reviewed (section 3). In particular, for
   the next phase (real-frame-memory adaptation), the typed-row-pointer (reinterpret_cast
   <uint16_t*>) vs memcpy decision against VapourSynth's actual frame-memory alignment
   contract is a known open design point to raise.

d) An enumerated prevailing-rules list for verification/reconciliation, marking each item
   REGISTER-OWNED (§3A) or CMS-DEFINED / HANDED-OFF (CMS07.3).

e) Your proposed approach for the NEXT phase as a text proposal for review — NOT code yet,
   and NOT applied. Confirm with the coordinator which phase is next (Document B section 8
   lists the forward order: real VapourSynth frame-memory adaptation, then scene-change,
   then getFrame/cache integration) and include HOW you propose to prove it (the proof
   surface has shifted from scalar reference vectors to real/synthetic frame-buffer and
   lifecycle proofs — say what genuinely discriminates a correct implementation).
```

Do not assume any rule carries over silently. Do not code, create files, rename files, copy
salvage, or wire getFrame without explicit user discussion, agreement, and instruction. When
in doubt about anything — direction, scope, proof approach, test design, or whether a
divergence from vsCnr2 is intended — raise it for review (section 3).
