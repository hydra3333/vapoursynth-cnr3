# CNR3 — CODER FIRST-POST BLURB (DIAG.3a — clean phase boundary)

*Paste the block below as the first message to a new memoryless coder chat, ahead of attachments.
This is a CLEAN PHASE BOUNDARY handoff (DIAG.2b committed) — no mid-task preamble needed.*

---

Hello. This chat continues development of a VapourSynth DLL plugin (vapoursynth-cnr3) per a
specification, where each step's choices are validated against the intent of the specs and
sensibility. VS2026, local git dev branch dev_cache_manager, pushed to
https://github.com/hydra3333/vapoursynth-cnr3/tree/dev_cache_manager at the end of each agreed step.

Your role is CODER (W3C): implement to the designer's scope, investigate/confirm before patching,
respond to designer/reviewer feedback. Coordinator (me, W3X) relays between chats and runs
builds/commits. Designer/reviewer (W3D) scopes and reviews the diff.

The prior coder chat committed DIAG.2b (cache-core diagnostics). We are now at a clean phase boundary
starting DIAG.3a. Read all attachments in order; do not comment until I prompt you.

Read CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_8.md first.

Attachments (latest versions), in read order:
  1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_8.md   (read first)
  2. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_12.md
  3. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_14.md
  4. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_25.md
  5. cnr3_cache_manager_design_v7_15.md
  6. CNR3_Diagnostics_Arc_Condensed_Plan_v1_5.txt
  7. cnr3_diagnostics_specification_v1_5.md
  8. CNR3_CMS_Future_Investigations_and_Open_Questions_v7_17.md
  9. z_CNR3_Diagnostics_Arc_Findings_Decisions_Provenance_v1_3.md
 10. CNR3_Patch_Scope_DIAG3a_recovery_rate_recalc_v1.md            (the live task)
 11. CNR3_DIAG_PlanResult_Vocabulary_and_Architecture_Spec_v1.md   (parallel cross-check, review only)
 12. src.zip                                                       (current post-DIAG.2b source baseline)

Standing rules (also in Document A / the intro): R-PROCESS-19 observe-only (a family's COMPUTE macro
off => it compiles out, behaviour byte-identical — the arc's exit gate); R-PROCESS-21 additive-only
(no restructuring proven code to host diagnostics; a mechanical transform is allowed ONLY if proposed
and designer-approved first — see the DIAG.2b CNR3_DSUM05_FAIL precedent); snapshot-outside-lock;
[DSUM-SUMMARY] tag (never [KDT-SUMMARY]); flush per line; NEVER git stash (use git switch -c wip-name);
read real source; review the diff; propose before commit. Always use the LATEST version of any doc even
if an older version is referenced.

After reading, confirm understanding when I prompt. Then your task is the DIAG.3a scope §8:
investigate + confirm (D-SUM-01 sync pattern, the 03/12/13 hook sites, the D-SUM-13 bounded container
proposal), report back, AND separately cross-check the plan/result spec (item 11) for sensibility/gaps.
Then generate the DIAG.3a patch.

---

## Maintainer notes (NOT pasted)
VERSION POINTERS (2026-07-04): intro v6.8 | Doc A v3.12 | Doc B v3.14 | DELTA v4.25 | design v7.15 |
condensed plan v1.5 | diag spec v1.5 | FI v7.17 | provenance v1.3 | DIAG.3a scope v1 | plan-result spec v1.
Batching decision (3a-split vs one DIAG.3 batch) is a coordinator call in the scope §0 — resolve before coder starts.
Provide src.zip = the post-DIAG.2b committed baseline (contains CNR3_DSUM05_FAIL, ownership_diag_stats_).
