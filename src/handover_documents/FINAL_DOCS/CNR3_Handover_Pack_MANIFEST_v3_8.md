# CNR3 Handover Pack — MANIFEST v3.8 (handover-safe, complete bundle)

Date: 2026-06-28
Build state: P.11C SCENE-CHANGE ARC CLOSED (.1-.5), committed CMS07-P.11C.5-scene-cut-checkpoint-recovery-anchor-proof, selftests 53/53 (forced-fail 52/53 exit 1; verbose 53/53). CMS07.13 (unchanged design; additive implementation note + front-matter currency fix).

NEXT ACTION (banked): STEP 0 — a joint CMS sensibility/gap review for hot-zone + prune live wiring, BEFORE any wiring patch. The prune/hot-zone componentry is built and selftest-proven but has ZERO live callers in the committed source; do not assume the CMS is reliable as-is merely because the componentry exists. The live prune-TRIGGER contract is the load-bearing part of that review. A CMS clarification or version bump may be a legitimate output of Step 0.

## Provenance of this pack
This v3.8 pack is the DESIGNER base set (the surgical-edit refresh) MERGED with the good catches from the
coder's handover-review zip (CODER_DRAFT ... v2026_06_28). Adopted from the coder review: (1) DELTA title
fix (through D.5 -> through P.11C.5); (2) stale concrete pointer-line fixes (Document A generation-source,
Document B doc-set line); (3) the STEP 0 CMS-sensibility-review framing for the next action; (4) the two CMS
front-matter currency fixes (Status paragraph; companion-doc example); (5) the completeness instinct — this
manifest + bundle physically includes the CMS, Role Handover, Reviewer Intro, and diagnostics specs.
REJECTED from the coder review: the DELTA slim that DROPPED the K.1F / R-LIFECYCLE lifecycle-resolution
section (DELTA §3, "important, don't re-derive") and left dangling §3/§4 cross-references. That section is
PRESERVED here — it is load-bearing context for the imminent live getFrame wiring work. Version numbers are
set ABOVE the coder-zip numbers so the numbering never forks.

## Controlling design authority
- cnr3_cache_manager_design_v7_13.md   (CMS07.13 — THE controlling design authority; must be supplied to any coder restart)

## Current-state pack (read in this order for a restart)
1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_4.md   (coder restart — paste first)
2. CNR3_Handover_Introduction_to_new_reviewer_chat_v3_5.md   (reviewer restart)
3. CNR3_Designer_Reviewer_Role_Handover_v1_12.md             (designer/reviewer role + process)
4. cnr3_cache_manager_design_v7_13.md                        (CMS — design authority)
5. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_8.md (context + standing rules R-PROCESS-01..23)
6. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_8.md (current build state; top UPDATE banner authoritative)
7. CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_14.md        (per-phase ledger + owed-items; KEEPS the §3 R-LIFECYCLE resolution)
8. CNR3_Handover_Pack_Production_Spec_v2_14.md                (production spec / pack rules)

## Companions (forward-looking; not required for Step 0, supplied for completeness)
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13_3.md   (open questions; FI-09 = single-activation prune-trigger contract / Step 0)
- CNR3_Diagnostics_Arc_Condensed_Plan_v1_3.txt                   (condensed 4-phase diagnostics forward plan)
- cnr3_diagnostics_specification_v1_5.md                         (D-SUM-01..14 telemetry spec — diagnostics arc)
- cnr3_memory_diagnostics_spec_v2.md                            (memory-diagnostics formatted-output spec; front-matter says "Companion document: CMS06" — included as the memory-diag spec ONLY, NOT a design authority; the CMS remains controlling)

## Authority chain
CMS (cnr3_cache_manager_design_v7_13.md) -> Production Spec §3A -> Document A (reproduces §3A) -> Document B (current state) -> DELTA (live ledger). Repository wins over any document on build state; on Document A vs Spec §3A mismatch, §3A wins (R-PACK-02).

## Known non-blocking currency notes (recorded, not silently edited)
- CMS Status paragraph historically referenced the "isolated cache-core milestone" — now annotated with a 2026-06-28 currency note (design text unchanged).
- cnr3_memory_diagnostics_spec_v2.md retains older "Companion document: CMS06" front-matter — included as the memory-diag formatted-output spec only.
