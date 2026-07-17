7. CNR3 Plugin Development — CODER Role (chat 7)

Hello. This chat continues from a prior coder chat that hit a hard limit. 
We are developing a VapourSynth DLL plugin (`vapoursynth-cnr3`) to specification, 
where each step's choices are validated against the specs' intent and against sensibility.

I have Visual Studio 2026 (vs2026) with the latest updates, on my PC with a local git repository
(dev branch `dev_cache_manager`) pushed to GitHub
https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager 
at the end of every agreed successful step.  Target Windows 10/11 x64 /arch:AVX2.

The prior coder chat worked with a rolling set of handover documents and specifications to
bring you 'up to speed' with your role as **coder**, which also encompasses responding to a
designer/reviewer — assessing, drafting proposals, and responding to designer/reviewer
feedback — i.e. conversing about the current status of this project, with the coordinator
(me, W3X) relaying between chats and running the builds/commits.

The full scope and context may not be totally clear until you have read all of the documents.
Note:
Your role is **CODER (W3C)**: implement to the designer's scope, INVESTIGATE/CONFIRM BEFORE PATCHING,
and respond to designer/reviewer feedback. Coordinator (me, **W3X**) relays between chats and runs builds/commits.
Designer/reviewer (**W3D**) scopes and reviews the diff. The .vpy/.bat harnesses are the DESIGNER's
deliverable; you deliver source patches and run the canonical 4-way selftest (Document A R-PROCESS-26 — read
it; never invent run mechanics unless explicitly requested to do so).



This is a MID-TASK resume. Read all attachments in order; do not comment until I prompt you to confirm
understanding. (A previous chat did not read them and the process suffered — please read them.)

Read CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v7_0.md first.

Attachments (latest versions), in read order:
  1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v7_0.md    (read first)
  2. Document_A_CNR3_Project_Context_and_Standing_Rules_v4_0.md (R-PROCESS register; incl. R-PROCESS-25/26/27)
  3. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v4_0.md(top UPDATE block authoritative)
  4. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v5_0.md
  5. cnr3_cache_manager_design_v7_15.md                        (CMS07.15, UNCHANGED by all diagnostic work)
  6. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v2_0.md (decisions ledger)
  7. CNR3_CMS_Future_Investigations_and_Open_Questions_v8_0.md (FI ledger)
  8. cnr3_diagnostics_specification_v1_5.md
  9. src.zip                                                    (committed source — CONFIRM its marker on arrival)
 (the designer will relay the specific scope + any older src.zip for the current task separately)


**Reference docs — no need to read now; consult when a task makes them relevant:** the CMS design
(cnr3_cache_manager_design_v7_15.md), the diagnostics spec (cnr3_diagnostics_specification_v1_5.md), the
memory-diagnostics spec (cnr3_memory_diagnostics_spec_v3_4.md), the condensed diagnostics plan, the lookup
taxonomy findings (v06), and the A1 plan-trace tool spec (v0.4). These are authoritative when their subject
comes up; skim them then, not now.

## CURRENT DEVELOPMENT STATE

The in-plugin diagnostics arc is CLOSED. An analysis/instrumentation sub-arc then committed FOUR patches,
all through the full gate (canonical 4-way 56/56, forced-fail 55/56 exit 1; R-PROCESS-19 macro-off
byte-identical; L1/L2 oracles; CMS UNCHANGED at 07.15):
  1. CMS07-DIAG.intent-counted-lookups        (intent counting replaces uniform lookup counting)
  2. CMS07-DIAG.lookup-site-breakdown         (11 per-site D-SUM-04 counters + self-check)
  3. CMS07-DIAG.frame-lifecycle-bail-counters (five-origin lifecycle; a/b/e/x/f; spine b==f+e+x)
Committed marker: CMS07-DIAG.frame-lifecycle-bail-counters.

APPROVED but HELD (not committed): CMS07-SCAFFOLD.filter-mode-selector — compile-time VS filter-mode selector
at the top of cnr3_build_config.h (uncomment one of fmUnordered/fmParallelRequests/fmParallel; exactly-one
#error guard; mode suffixed onto CNR3_EDIT_VERSION; filter_mode= provenance line). Default fmUnordered =
identical behaviour. Held pending the task below (both touch cnr3_create_filter).

## YOUR FIRST TASK (once oriented and I prompt you)

A RUN-LOG MARKER REGRESSION needs pinning and fixing. The plugin used to print its edit_version/CMS07 marker
to the RUN LOG; it no longer does. Cold-verified by the designer: CNR3_EDIT_VERSION appears in EXACTLY ONE
source place (cnr3_cache_core_selftest_main.cpp — the selftest summary), so nothing prints it on the plugin
run path; it was already absent at the earliest snapshot, so the regression PREDATES the recent arc and is not
caused by any of the four patches.

The task has two parts, and the designer will relay a precise scope:
  (a) BISECT: I will upload older GitHub `src` trees one at a time; for each, grep for a plugin-run-path
      marker emission and report whether it exists and — if it does — the EXACT original line/format/stream,
      so it can be restored faithfully. Verify each uploaded src by its MARKER, not its filename (a stale
      zip caused a scare last session).
  (b) RESTORE + GATE: once the original emission is characterised, a small scoped patch restores the run-path
      marker emission (natural home: cnr3_create_filter in vapoursynth-Cnr3.cpp, beside the new filter_mode=
      line), AND adds an emission-presence check so the gap that hid this can't recur (now R-PROCESS-27:
      proof gates must assert run-log emission content, not only selftest + frame bytes). This likely folds
      with, or lands immediately before, the held filter-mode-selector commit.

Do NOT start until I confirm. When the designer issues the scope, first CONFIRM to me you understand it and
wait for my approval before patching.

## HOW WE WORK (what keeps this project healthy)

- CONFIRM-BEFORE-PATCH, always: reconcile the scope against real source (file:line) and report back BEFORE
  writing the patch. Your confirm-reports have repeatedly caught what a scope missed (the 10a->10b store
  nesting; 10a selftest-reachability; the five-origin-not-three frame model) — that is the process working.
- Verify claims COLD against live src/, never from memory or a patch's own claims.
- Whole-patch deletion scan in every delivery; exact insertion file:line; state the gate macro you matched.
- R-PROCESS-25: any touch of proven code is proposed first; counting/observe statements only where that's
  the scope.
- Gating boundary (R-PROCESS-25 precedent, Ruling 5 this arc): gate what EXECUTES; leave new defaulted
  parameters plain/ungated to match committed plumbing; do not macro-wrap signatures.
- Run the canonical 4-way (R-PROCESS-26) and report the four results exactly; never invent run mechanics.
  Do NOT run the L1/L2 harness proofs — those are the designer's.

After reading all documents and once I prompt you, confirm your understanding of: the committed state (four
patches + the held selector), the marker-regression task and its two parts, and R-PROCESS-25/26/27. Then I
will relay the designer's scope.
