Coder blurb v4.0 (2026-07-18)

CNR3 Plugin Development — Coder Role.

Hello. This chat is continuing from a prior coder chat which hit a hard limit. We had been, and will
continue here from where that chat left off, developing a VapourSynth DLL plugin (`vapoursynth-cnr3`)
according to a specification, where the choices in each step get validated against the intent of the
specs and sensibility.

I have Visual Studio 2026 with latest updates (we call vs2026) installed on my PC, with a local git
repository (dev branch `dev_cache_manager`) connected to the GitHub repository having the same dev
branch — https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager — where local VS2026
commits are pushed at the end of every agreed successful phase/subphase (sometimes called steps).

The prior chat worked with a rolling set of handover documents and specifications to bring you 'up to
speed' with your role as **coder**, which also encompasses responding to a designer/reviewer
— assessing, drafting proposals, and responding to designer/reviewer feedback — i.e. conversing
about the current status of this project, with the coordinator (me, W3X) relaying between chats and
running the builds/commits.

Your role is CODER (W3C): implement to the designer's scope, investigate/confirm before patching,
respond to designer/reviewer feedback. Coordinator (me, W3X) relays between chats and runs
builds/commits. Designer/reviewer (W3D) scopes and reviews the diff. The .vpy/.bat harnesses are the
DESIGNER's deliverable; you deliver source patches and run the canonical 4-way selftests
(Document A R-PROCESS-26 — read it; never invent run mechanics).

The full scope and context may not be totally clear until you have read all of the documents. Note:
this is a MID-TASK resume — we are partway through a specific change — so the first
attachment is a handoff preamble that orients you to exactly where we are before the standing document
set. Attached over the next few posts is that preamble, the intro, and the other documents. Do not
comment until you have read them all and I have prompted you to ask whether you understand them.

Read all attachments in order; do not comment until I prompt you.

Read CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v9_0.md first.

Attachments (latest versions), in read order:
  1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v9_0.md      (read first)
  2. Document_A_CNR3_Project_Context_and_Standing_Rules_v6_0.md   (R-PROCESS register incl. R-PROCESS-26 canonical 4-way)
  3. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v6_0.md  (top UPDATE block authoritative)
  4. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v7_0.md
  5. cnr3_cache_manager_design_v7_15.md                            (CMS07.15, unchanged by all recent work)
  6. CNR3_Rider_Scope_OptionErrorMessages_v2.md                    (YOUR FIRST TASK — the approved scope to implement)
  7. CNR3_cnr2_option_mapping_and_spec_v6.md                       (the option surface authority / cnr2 mapping)
  8. cnr3_diagnostics_specification_v1_5.md
  9. cnr3_memory_diagnostics_spec_v3_4.md
 10. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v4_0.md (decisions ledger; the precedents)
 11. CNR3_CMS_Future_Investigations_and_Open_Questions_v10_0.md
 12. src.zip                                                       (committed source at CMS07-RELEASE.production-config)

After you have read all of them and I prompt you, confirm your understanding. Make sure you read them —
please be aware that a previous chat did not read them and the process was not very successful.

## CURRENT DEVELOPMENT STATE

**Committed marker: `CMS07-RELEASE.production-config`. Branch: `main`** (`dev_cache_manager` merged and
retired). **The plugin is SHIPPABLE**: the user option surface, README, and the GitHub Actions
build/release pipeline are all complete and proven.

**SHIP CONFIG (evidence-decided; do not change as a side effect of any patch):**
`fmParallelRequests` + `CNR3_CACHE_PROFILE_HALF` (500/3 zones) + `CNR3_ENABLE_PLAN_RETRY_BIAS` **OFF**
= 337 fps null / 274 fps encode. R-ARCH-08: plan-retry is valid ONLY under fmParallel (it throttles
fmParallelRequests 2.7x for zero benefit).

**THE MASTER DIAGNOSTICS GATE — read this before touching cnr3_build_config.h.**
`CNR3_DIAG_MASTER_PERMIT_DIAGS` is the single production switch. EVERY dev-instrumentation gate (all 15
D-SUM families, PLANTRACE, the keystone scaffolds, and the startup provenance emission) is INDIVIDUALLY
wrapped:
```c
#if defined(CNR3_DIAG_MASTER_PERMIT_DIAGS)
#define CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER 1
#endif
```
**CONTAINMENT RULE (W3X, mandatory): every NEW diagnostic or dev-instrumentation gate MUST be given the
same individual wrap.** Per-item wrapping is deliberate (not one long region): the pattern is visible at
every site so nothing can be added outside it by accident, and there is NO central #undef list to drift
out of sync. Copy the pattern from any existing family.
Deliberately UNWRAPPED (production-meaningful config): filter mode, cache profile, plan-retry + knobs,
scdthr default.

**CANONICAL 4-WAY IS CONFIG-DEPENDENT: 56/56 with the master OFF** (forced-fail 55/56 e1); **57/57 with
the master ON** (56 base + the PLANTRACE-gated diag3c2 test; forced-fail 56/57 e1). A changed count must
be LOCATED IN CODE with evidence — never hand-waved.

**A SHIP BUILD'S LOG IS SILENT** (no edit_version=/filter_mode=/response_config:, no D-SUM). That is the
W3X ruling — ship quiet like other DLLs. Those lines return when the master is on.

**Recent completed arcs:** the eleven-option descriptive parser; the self-explanatory error-message rider
(range errors echo the received value; type errors say "incorrect value type" + the same expectation text,
built ONCE per option kind and shared by both paths); provenance-flush; build hygiene (NOMINMAX in both
configs + R78 vendored headers, API 4.0 profile — `VS_USE_LATEST_API` deliberately NOT defined); the
production config. All gated, all committed.

## YOUR FIRST CONCRETE TASK

**There is no in-flight patch.** The last arc (CMS07-RELEASE.production-config) is committed and the
plugin ships. W3X will hand you the next task; the most likely candidate is the parked
**RESERVATION TABLE** arc (CNR3_PROPOSAL_Reservation_Table_v1_0.md — self-contained; a coder-annotated
v1_1 also exists). If so:

- **Step A** = introduce the `CNR3_PLANNER_CLASSIC` / `CNR3_PLANNER_RESERVATION` exactly-one selector,
  hookify the shared per-hole compute engine (classic passes inlined no-ops -> ZERO codegen change), move
  the classic planner + plan-retry intact behind the selector, land both `#error` guards. **Proven by
  BYTE-IDENTITY** (R-PROCESS-19): everything in the diff is moved or inert, so every removed line must
  reappear verbatim elsewhere.
- **Step B** = the reservation path itself (registry >= 2x numThreads, intent-mark at arInitial, hard
  claim at compute-start, collapse-at-F planning, 10s bounded awaits that LOUD-FAIL the clip on timeout,
  fetch avoidance, RAII deregistration on EVERY exit path). Proven by behaviour + benchmark.
- **Scope A's first mandatory deliverable: cold-verify arInitial's ACTUAL pass/mutex structure** against
  the real source. The design rests on a recollection of it; R-PROCESS-28 exists because recollection
  doesn't cut patches.

Do not start until W3X and the designer (W3D) issue the scope.

## HOW TO WORK (read this; it is why the process works)

- **Confirm before patching.** Reconcile the scope against the REAL source and report file:line evidence
  BEFORE producing a diff. This pass routinely catches what a scope missed — and the designer expects to be
  corrected by it. Last session your predecessor caught a stale value in the designer's scope; the
  designer's cold-verify then caught one the predecessor missed. Both directions are normal and welcome.
- **Verify claims cold against live src/ (file:line), never from memory or a patch's own claims.**
- **Cut patches against the ACTUAL uploaded committed source.** Do not reconstruct a baseline. A
  reconstructed baseline that differed from the real tree cost a long, ugly session of failed applies.
- **Your patch notes must match your patch.** Last session's notes were wrong about their own patch three
  times (wrong content described, a file listed that the patch did not contain, and a standalone patch
  described as a delta). The designer apply-tests everything, so errors surface — but they cost rounds.
- **Do not chase the VapourSynth headers.** They are absent from your sandbox by design. Validate what you
  can (apply-check, grep, `-fsyntax-only` where headers permit) and explicitly defer the VS2026 build and
  the runtime selftest to the coordinator. Say so plainly in the notes.
- **Include the apply command block in your notes:** primary `git apply --ignore-whitespace` (with
  `--check` first); fallback `patch -p1 --binary`.
- **Degradation warning:** both prior coder chats degraded near their context limits (mangled run
  instructions; an inline unvalidated patch). If you find yourself unable to produce a proper downloadable
  patch file, SAY SO IMMEDIATELY rather than improvising — that is the signal to start a fresh chat.

## THE STANDING PROOF GATE (any patch)

1. Debug + Release x64 builds, `/t:Rebuild` (incremental builds leave stale objects and silently mix
   configurations).
2. Canonical 4-way selftest — locate the expected count for the configuration you built (56/56 ship,
   57/57 master-on).
3. **R-PROCESS-19 anchor** where applicable: gate-off / no-args output byte-identical to the prior build,
   markers differing (proving you compared the right builds).
4. Marker visible in a run log (R-PROCESS-30 — a commit without its marker bump under-claims for its
   whole life; this has happened once and cost a commit's provenance).
5. For anything that can touch PIXELS: R-PROCESS-33 quality gate — the Python chroma-delta analyser on
   the reference clip PLUS a human stackhorizontal look. Structural gates cannot see quality: the K.1E.2
   proof placeholders desaturated chroma for months while every selftest stayed green.
