# CNR3 — CODER FIRST-POST BLURB (mid-DIAG.2b resume)

*Paste the block below as the first message to a new memoryless coder chat, ahead of the attachments.
Reusable for future handovers: bump the version pointers and the read-order list, keep the structure.
The "mid-task resume" framing + the handoff-preamble-first instruction apply whenever a coder session
dies PARTWAY through a scoped task; at a clean phase boundary, drop item 1 and start from the intro.*

---

Hello. This chat is continuing from a prior coder chat which hit a hard limit. We had been, and will
continue here from where that chat left off, developing a VapourSynth DLL plugin (`vapoursynth-cnr3`)
according to a specification, where the choices in each step get validated against the intent of the
specs and sensibility.

I have Visual Studio 2026 with latest updates (we call vs2026) installed on my PC, with a local git
repository (dev branch `dev_cache_manager`) connected to the GitHub repository having the same dev
branch — https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager — where local VS2026
commits are pushed at the end of every agreed successful phase/subphase (sometimes called steps).

The prior chat worked with a rolling set of handover documents and specifications to bring you 'up to
speed' with your role as **coder (W3C)**, which also encompasses responding to a designer/reviewer
(W3D) — assessing, drafting proposals, and responding to designer/reviewer feedback — i.e. conversing
about the current status of this project, with the coordinator (me, W3X) relaying between chats and
running the builds/commits.

The full scope and context may not be totally clear until you have read all of the documents. Note:
this is a MID-TASK resume — we are partway through a specific change (DIAG.2b) — so the first
attachment is a handoff preamble that orients you to exactly where we are before the standing document
set. Attached over the next few posts is that preamble, the intro, and the other documents. Do not
comment until you have read them all and I have prompted you to ask whether you understand them.

Read **CNR3_Coder_MidDIAG2b_Handoff_Preamble.md** first, then
**CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_8.md**, then the rest.

Attachments (latest versions), in read order:
  1. CNR3_Coder_MidDIAG2b_Handoff_Preamble.md                          (read first — where we are right now)
  2. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_8.md           (read second)
  3. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_12.md
  4. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_13.md
  5. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_24.md
  6. cnr3_cache_manager_design_v7_15.md
  7. CNR3_Diagnostics_Arc_Condensed_Plan_v1_5.txt
  8. cnr3_diagnostics_specification_v1_5.md
  9. CNR3_CMS_Future_Investigations_and_Open_Questions_v7_17.md
 10. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v1_2.md
 11. CNR3_Patch_Scope_DIAG2b_ownership_integrity_store_v2.md           (the live task)
 12. CNR3_DIAG2b_pre_patch_inventory_review.md                        (your predecessor session's own review — read after the v2 scope)
 13. src.zip                                                          (current post-DIAG.2a source baseline)

After you have read all of them and I prompt you, confirm your understanding. Then your immediate task
is the v2 scope's §8 TARGETED CONFIRM (not a fresh broad investigation — the big questions are already
resolved), reported back to the designer, after which you generate the DIAG.2b patch against src.zip.

---

## Maintainer notes (NOT part of the pasted block)

VERSION POINTERS current as of this blurb (2026-07-04) — bump on each future handover:
  intro v6.8 | Document A v3.12 | Document B v3.13 | DELTA v4.24 | design v7.15 | condensed plan v1.5 |
  diag spec v1.5 | Future Investigations v7.17 | provenance v1.2 | DIAG.2b scope v2.

DELIBERATELY OMITTED from the primary read order (available on request, not core onboarding for a
resuming coder): the Step-0-era findings registers (x_CNR3_Step0_Findings_Register_*,
x_CNR3_Step0_Joint_Review_PROCESS_*) and provenance v1.0 — Step 0 is CLOSED and the marshalling arc is
complete; the live Provenance v1.2 and FI v7.17 carry forward anything still relevant. A resuming coder
should read CURRENT STATE + LIVE TASK deeply and CLOSED HISTORY only on demand.

WHEN THIS BLURB IS REUSED AT A CLEAN PHASE BOUNDARY (not mid-task): drop item 1 (the mid-DIAG2b
preamble), change "MID-TASK resume" wording back to a normal resume, and point the read-first at the
intro. The preamble pattern is specifically for a coder session dying PARTWAY through a scoped patch.
