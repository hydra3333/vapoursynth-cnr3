# CNR3 — CODER FIRST-POST BLURB (post-DIAG-arc; PENDING PATCH honest-cache-hit-metrics under review)

*Paste the block below as the first message to a new memoryless coder chat, ahead of attachments.
This is a SUCCESSION handoff: the prior coder chat hit its hard limit having just DELIVERED a patch
that is still under designer review — not a clean phase boundary.*

---
6. CNR3 Plugin Development chatGPT chat 6 Coder Role.

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

Read CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_10.md first.

Attachments (latest versions), in read order:
  1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_10.md   (read first)
  2. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_14.md   (R-PROCESS register incl. R-PROCESS-26 canonical 4-way)
  3. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_20.md  (top UPDATE block authoritative)
  4. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_30.md
  5. cnr3_cache_manager_design_v7_15.md                            (CMS07.15, unchanged by the diagnostics work)
  6. CNR3_Patch_Scope_HonestCacheHitMetrics_v1.md                  (the PENDING patch's scope — your likely first task)
  7. cnr3_diagnostics_specification_v1_5.md
  8. cnr3_memory_diagnostics_spec_v3_4.md
  9. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v1_8.md  (decisions ledger; the honesty-filter precedents)
 10. CNR3_CMS_Future_Investigations_and_Open_Questions_v7_18.md
 11. src.zip                                                        (committed source at CMS07-DIAG.derived-health-ratios)

After you have read all of them and I prompt you, confirm your understanding. 

CURRENT DEVELOPMENT STATE:
the in-plugin DIAGNOSTICS ARC is CLOSED (all 14 D-SUM families + plan-trace + [DSUM-HEALTH]
committed; latest commit marker CMS07-DIAG.derived-health-ratios; selftest 56/56). Your predecessor
chat reached its hard limit immediately after delivering a patch CMS07-DIAG.honest-cache-hit-metrics
(scope: CNR3_Patch_Scope_HonestCacheHitMetrics_v1.md) which is UNDER DESIGNER REVIEW NOW. Depending
on that review you may be asked to: adopt it, fix findings against the scope, or re-deliver parts.
Treat the delivered patch as UNVERIFIED — when asked, verify claims cold against live src/ (file:line),
never from memory or the patch's own claims.

---

## Maintainer notes (NOT part of the pasted block)

VERSION POINTERS current as of this blurb (2026-07-04) — bump on each future handover.

DELIBERATELY OMITTED from the primary read order, the live Provenance vx.y and FI va.b carry
forward anything still relevant. A resuming coder should read CURRENT STATE + LIVE TASK 
deeply and CLOSED HISTORY only on demand.

WHEN THIS BLURB IS REUSED AT A CLEAN PHASE BOUNDARY (not mid-task): drop item 1 (, change "MID-TASK resume" wording back to a normal resume, and point the read-first at the
intro. The preamble pattern is specifically for a coder session dying PARTWAY through a scoped patch.