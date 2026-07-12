# CNR3 — DESIGNER FIRST-POST BLURB v2.0 (analysis track; filter-mode-selector IN FLIGHT, marker-emission regression open)

*Paste the block below as the first message to a new DESIGNER/REVIEWER (W3D) chat, ahead of attachments.
Maintainer notes at the end (not pasted).*

---

Hello. This chat continues from a prior "designer" chat that hit a hard limit. We are developing a
VapourSynth DLL plugin (`vapoursynth-cnr3`) to specification, where each step's choices are validated
against the specs' intent and against sensibility.

I have Visual Studio 2026 (vs2026) on my PC with a local git repo (dev branch `dev_cache_manager`) pushed to
https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager at the end of every agreed successful
step. Target: x64 /arch:AVX2.

This chat takes the DESIGNER/REVIEWER (**W3D**) role. You own the CMS specification and its rules.

**Three-party discipline:** **W3X** = coordinator (me — relays artifacts between chats, runs builds/tests/
commits, holds decisions), **W3D** = you (designer/reviewer — investigate source cold, write patch scopes,
review coder confirm-reports and DIFFS, issue amendments/decisions, gate commits, own the .vpy/.bat
harnesses), **W3C** = coder (separate memoryless chat — investigates, confirms before patching, generates
patches). You never write production patches; you scope and review. The coder never self-approves; you are
the diff authority. I relay everything; attached files sometimes arrive empty in-context — they are still on
disk at /mnt/user-data/uploads/ and must be read from there (standing fallback).

Read all attachments in order; do not act until I prompt you.

Read CNR3_Designer_Reviewer_Role_Handover_v2_0.md first (role + method + playbook), then
CNR3_Handover_Introduction_to_new_reviewer_chat_v4_0.md (its currency note = precise current state).

Attachments (latest versions), in read order:
  1. CNR3_Designer_Reviewer_Role_Handover_v2_0.md               (role + method, in depth — read first)
  2. CNR3_Handover_Introduction_to_new_reviewer_chat_v4_0.md    (orientation; currency note = current state)
  3. Document_A_CNR3_Project_Context_and_Standing_Rules_v4_0.md (standing rules incl. R-PROCESS-24/25/26)
  4. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v4_0.md(work plan / current state; top block authoritative)
  5. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v5_0.md         (detailed current-state delta)
  6. cnr3_cache_manager_design_v7_15.md                        (CMS design authority; UNCHANGED by all diag work)
  7. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v2_0.md (decisions ledger; A1 seed set; honesty precedents)
  8. CNR3_Cache_Lookup_Taxonomy_Findings_v06.md                (the lookup census + intent-counting rules)
  9. CNR3_A1_PlanTrace_Analysis_Tool_Spec_v0_4.md              (the external Python analysis-tool spec — handoff-ready)
 10. CNR3_CMS_Future_Investigations_and_Open_Questions_v8_0.md (FI ledger)
 11. test_000_Example_576p50_TESTING_L2.vpy / .bat            (the L-series harness — YOUR deliverable)
 12. src.zip                                                   (committed baseline — CONFIRM its marker on arrival)


**Reference docs — no need to read now; consult when a task makes them relevant:** the CMS design
(cnr3_cache_manager_design_v7_15.md), the diagnostics spec (cnr3_diagnostics_specification_v1_5.md), the
memory-diagnostics spec (cnr3_memory_diagnostics_spec_v3_4.md), the condensed diagnostics plan, the lookup
taxonomy findings (v06), and the A1 plan-trace tool spec (v0.4). These are authoritative when their subject
comes up; skim them then, not now.

## WHERE WE ARE (precise)

The in-plugin diagnostics arc is CLOSED. Since then, an **analysis/instrumentation sub-arc** ran to make the
cache's behaviour under out-of-order requests fully legible. FOUR patches committed this session, all through
the full gate (canonical 4-way 56/56; R-PROCESS-19 macro-off byte-identical; L1/L2 oracles; CMS UNCHANGED at
07.15):

1. **CMS07-DIAG.intent-counted-lookups** — replaced uniform cache-lookup counting with *intent counting*: a
   probe counts only when its outcome was uncertain and changes what happens next. Retired the old uniform
   L1 oracle (66.664). New L1 oracle exact: 7279/7279/0 = 100.000.
2. **CMS07-DIAG.lookup-site-breakdown** — per-site D-SUM-04 counters (11 named sites) with plain-English
   legend + purpose lines + a print-only self-check (per-site sums == merged totals -> OK / *** MISMATCH ***,
   never wired to selftest).
3. **CMS07-DIAG.frame-lifecycle-bail-counters** — FIVE-origin frame lifecycle (frame0 / floor /
   ordinary_target / recovery_hole / recovery_target). Events a(bail-before-compute) / b(computed) /
   e(bail-after-compute) / x(computed-returned-after-dup) / f(computed-and-stored), each total + 5 origins,
   all independently counted (COUNT, never compute). Spine self-check b == f + e + x. 19 self-checks, all OK.
4. **CMS07-SCAFFOLD.filter-mode-selector** — *IN FLIGHT, reviewed-APPROVED, NOT yet committed* (held; see
   below). Compile-time selector in cnr3_build_config.h (top, under the marker): uncomment exactly one of
   fmUnordered / fmParallelRequests / fmParallel; exactly-one #error guard; the selected mode is suffixed
   onto CNR3_EDIT_VERSION (marker now reads `...filter-mode-selector:fmUnordered`) and printed as a
   `filter_mode=<token>` provenance line at filter creation. Default = fmUnordered = behaviour identical to
   the prior commit. 4-way PASS (marker check passed against the suffixed string). Byte-identical satisfied
   BY INSPECTION (designer traced both change sites cold: the mode token resolves to the identical
   fmUnordered; the fprintf is stderr-only, no pixel/compute reach). `filter_mode=fmUnordered` confirmed in a
   real VS run.

Key finding from the arc (now measured, not inferred): under fmUnordered WITHOUT `-r 1`, prefetch reordering
routes ~half of a linear clip through recovery, and EVERY hole is ADOPTED (bail-before-compute), never
recomputed — zero wasted recompute (L1noR: b=7280, recovery_hole computed=0, bail-before recovery_hole=3739;
e and x stay 0 because single-threaded fmUnordered cannot race). The e/x race arms remain UNEXERCISED by any
run so far — they need genuine fmParallel (that is what the selector + the whirl are for).

## THE OPEN THREAD (why the selector commit is HELD)

A **run-log marker regression** was discovered: the plugin used to print its `edit_version`/CMS07 marker to
the RUN LOG, and it no longer does. Verified cold: in the current committed tree AND in the earliest snapshot
available (honest-cache-hit-metrics), `CNR3_EDIT_VERSION` appears in EXACTLY ONE place —
cnr3_cache_core_selftest_main.cpp (selftest summary only). No plugin-run-path emission exists; no orphaned
scaffolding. So the regression PREDATES this session's arc and is NOT caused by any of the four patches
(all reviewed cold; none touch marker emission). It went unnoticed because the proof gate verifies the
SELFTEST (4-way) and FRAME BYTES (fc /b) but NEVER asserts run-log emission content — so any run-log line can
vanish silently with all gates green. That process gap is the real defect ("what else broke unnoticed?").

W3X is downloading older GitHub trees (a few days before honest-cache-hit; K-series / CMS07-early era) to
upload one at a time. Designer task: grep each for a run-path marker emission; when one HAS it, capture the
EXACT original line/format/stream so it can be restored faithfully; bisect between "has it" and "doesn't" to
pin the removing commit. Then a small scoped patch restores the run-path emission (natural home:
cnr3_create_filter in vapoursynth-Cnr3.cpp, beside the new filter_mode= line), and — the actual fix — an
EMISSION-PRESENCE CHECK is added to the harness/gate so run-log content is gated, not assumed.

## NEXT STEPS IN ORDER

1. **Pin & restore the marker regression** (above). Likely folded with, or committed immediately after, the
   filter-mode-selector patch (both touch cnr3_create_filter). Add the emission-presence gate.
2. **Commit CMS07-SCAFFOLD.filter-mode-selector** (reviewed-approved; held only for #1's co-location).
3. **Test-case sample runs** (feed the A1 spec's example section):
   - 200-frame UNSHUFFLED TINY-100 baseline, plantrace on 0..199, NO `-r 1` (fmUnordered). Shows recovery via
     prefetch reorder + adoption K-codes + pruning (200 frames clears the ~110 TINY prune trigger).
   - then the shuffled variant, then the FIRST fmParallel whirl (plain in-order L1, TINY-100, plantrace on,
     no `-r 1`) via one-comment flip on the committed selector — the first run that can light up e/x / L-codes
     / C1-ownership-under-race (A2 territory). PROOF runs are NORMAL profile; EXPERIMENT runs are TINY.
4. **A1 tool build** — spec v0.4 is handoff-ready (full onboarding Part A; full question set, one function
   each, easiest-first; Q-B = reconstruct true order from ticks and explain D-SUM-01's out_of_order=0, which
   is the ORIGINAL driving question). A FRESH designer chat may take A1; it is a natural seam.
5. **A2** (fmParallel concurrency churn = C1-ownership-under-race gate owed from 3b) then **A3** (real-footage
   576p50 via A1).

STANDING (do not decay): FI-11 ring-recovery correlation deferred-but-flagged; D-SUM-07 AS2-widening banked
(narrow tie only, do not force); the marker-emission gate is now a REQUIRED addition; scrutiny NORMAL-HARD,
exceptional for any patch delivered at/after a coder-session limit.

## THE CODER RELATIONSHIP (what works — keep doing this)

- **Confirm-before-patch, always.** Scope = proposal; the coder's confirm-report = reconciliation with real
  source; patch only after they agree. That pass routinely catches what a scope missed — this session it
  caught the 10a->10b store nesting, the 10a selftest-reachability, and the five-origin (not three) frame
  model, each of which would have silently miscounted.
- **Verify the coder's claims COLD against source** (file:line); re-derive, don't accept summaries. Your own
  scopes get corrected by this — that is it working.
- **Verify INVOCATION, not just definition.** **Whole-patch deletion scan** on every diff.
- **Caller-map re-derivation is the highest-yield check** on counter patches (the selftest-leak class of bug).
- **R-PROCESS-25:** any touch of a proven line is PROPOSED first; "behaviourally identical" is the DESIGNER's
  call.
- **Objective backstops beat argument:** 4-way + macro-off byte-identical + hand-checks against raw counters
  have validated every "provably correct" claim. NEW lesson this session: they do NOT cover run-log emission
  content — hence the marker regression. Add emission checks to the gate.
- **Verify archives by MARKER, not filename** — a stale src.zip under a reused name caused a scare this
  session, caught by checking the marker cold. Confirm every uploaded src.zip's CNR3_EDIT_VERSION on arrival.

Note: this is a MID-TASK resume. Read all documents in order; do not comment until I prompt you. After
reading, confirm your understanding of: the four committed patches + the held selector; the marker-emission
regression and the gate gap it exposes; the five-origin lifecycle model and the intent-counting rules; the
forward order (marker restore -> commit selector -> sample runs -> A1 -> A2 -> A3); and R-PROCESS-24/25/26.
Then I will hand you the next artifact (an older src.zip to bisect the marker regression, or a sample log).

---

## Maintainer notes (NOT pasted)
VERSION POINTERS (2026-07-12): role handover v2.0 | reviewer intro v4.0 | Doc A v4.0 | Doc B v4.0 |
DELTA v5.0 | design v7.15 (unchanged) | Provenance v2.0 | FI v8.0 | taxonomy findings v06 | A1 spec v0.4 |
condensed plan v1.10 | diag spec v1.5 | memory spec v3.4.
Committed marker at handover: CMS07-DIAG.frame-lifecycle-bail-counters. Selector patch reviewed-approved,
NOT committed (held for marker co-location). src.zip must be refreshed to the committed tree and its marker
confirmed on upload.
At the marker-restore + selector commit: refresh src.zip, advance DELTA/Doc B/Provenance, flip this blurb's
IN-FLIGHT/OPEN-THREAD sections to the next task (sample runs / A1 build).
