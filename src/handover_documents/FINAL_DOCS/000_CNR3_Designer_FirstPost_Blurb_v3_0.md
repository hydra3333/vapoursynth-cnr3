Coder blurb v3.0 (2026-07-15)

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

Read CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v8_0.md first.

Attachments (latest versions), in read order:
  1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v8_0.md      (read first)
  2. Document_A_CNR3_Project_Context_and_Standing_Rules_v5_0.md   (R-PROCESS register incl. R-PROCESS-26 canonical 4-way)
  3. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v5_0.md  (top UPDATE block authoritative)
  4. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v6_0.md
  5. cnr3_cache_manager_design_v7_15.md                            (CMS07.15, unchanged by all recent work)
  6. CNR3_Patch_Scope_OptionErrorMessages_ENRICHED_v3.md           (YOUR FIRST TASK — the approved scope; self-contained, READ IT ALL)
  7. CNR3_cnr2_option_mapping_and_spec_v6.md                       (the option surface authority / cnr2 mapping)
  8. cnr3_diagnostics_specification_v1_5.md
  9. cnr3_memory_diagnostics_spec_v3_4.md
 10. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v3_0.md (decisions ledger; the precedents)
 11. CNR3_CMS_Future_Investigations_and_Open_Questions_v9_0.md
 12. src.zip                                                       (committed source at CMS07-FEATURE.cnr2-descriptive-option-parser)

After you have read all of them and I prompt you, confirm your understanding. Make sure you read them —
please be aware that a previous chat did not read them and the process was not very successful.

## CURRENT DEVELOPMENT STATE

**Committed marker: `CMS07-FEATURE.cnr2-descriptive-option-parser`** (pushed).

The plugin now has a real user-facing option surface: eleven descriptive options parsed at create time,
strictly validated, applied, and echoed in a live `response_config:` log line. Defaults are cnr2-equivalent.
The canonical 4-way selftest is **57/57** (forced-fail 56/57 invariant_violation, exit 1) — the count is
CONFIG-DEPENDENT (56 base + `diag3c2_induced_live_bail_plantrace`, which `CNR3_DIAG_COMPUTE_DSUM_PLANTRACE`
enables). A changed count must be LOCATED IN CODE, never hand-waved.

The SHIP configuration for the PyPI-distributed DLL is `fmParallelRequests` + `CNR3_CACHE_PROFILE_HALF`
(500 ceiling, 3 hot zones) + `CNR3_ENABLE_PLAN_RETRY_BIAS` **OFF**. Each is an evidence-backed decision;
do not change them as a side effect of any patch.

## YOUR FIRST CONCRETE TASK

Implement **CNR3_RIDER.option-error-messages** from the approved scope
(CNR3_Patch_Scope_OptionErrorMessages_ENRICHED_v3.md). Your predecessor chat APPROVED this scope, then hit its limit
and emitted an inline draft which it immediately retracted as "unvalidated draft content". **No patch
exists.** Implement it fresh, from the scope, against the attached committed src.zip.

It is MESSAGE TEXT ONLY. Required message family:

    type error  : CNR3: invalid y_threshold option: incorrect value type, expected an integer in the range 0..255 inclusive.
    range error : CNR3: invalid y_threshold option: got 256, expected an integer in the range 0..255 inclusive.

- Build the expectation text ONCE per option kind (int 0..255 / float 0.0..100.0 / curve string / bool 0..1)
  and reuse it for BOTH the type-error and the range-error paths, so the two can never drift apart.
- Range/value errors echo the received value: int (`%lld`), float (`%g` — NOT `%.1f`; a near-miss like
  100.0001 must not render as "got 100.0, expected 0.0..100.0"), curve string (quoted, width-limited
  `%.32s`, nullptr-guarded), bool (`%lld`).
- Type errors do NOT invent a value — there is none to echo; they state the type problem plus the same
  expectation.
- Keep all formatting snprintf-bounded within the existing `detail[128]` / `message[256]` fixed buffers.
- **Validation logic must NOT change.** Which inputs are accepted/rejected stays exactly as-is.
- Bump the marker to `CMS07-RIDER.option-error-messages`.
- Expected changed files: `src/vapoursynth-Cnr3.cpp` and `src/cnr3_build_config.h` (marker only).

**STOP CONDITION:** if you discover that any invalid option is currently ACCEPTED, stop and report. That is
a parser validation bugfix, not this rider.

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

## AFTER THE RIDER

1. Its proof gate: Debug+Release build; canonical 4-way (57/57 unchanged); the invalid-option runs (range
   AND type classes, all four option kinds); a long/odd curve string truncating safely; a valid spot-check
   still succeeding; and the R-PROCESS-19 anchor — no-args output byte-identical to the committed parser
   build (message text cannot affect pixels; this confirms nothing else moved).
2. Then `CMS07-DOC.cnr2-descriptive-options-readme` — the deferred user documentation for the option
   surface (draft text exists; the designer holds it).
3. Then the parked reservation-table work (the real fix for the fmParallel predecessor-in-flight
   redundant-recompute race). Design constraint: it, `CNR3_ENABLE_PLAN_RETRY_BIAS`, and the cache-profile
   gating must all COMPOSE — separately or together.
