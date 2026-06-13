# CNR3 Handover Pack v3.0 Manifest (CMS07.0 restart)

**Date:** 2026-06-13
**Generated from:** CMS07.0 (`cnr3_cache_manager_design_v7_0.md`), Production Spec v2.0,
the prior Document A (enduring context only), and the CMS07.0 coder restart
introduction.
**Purpose:** Clean CMS07.0 restart pack. CMS07.0 is a complete architectural
supersession of the CMS06.x cache design. This pack deliberately quarantines the
CMS06.x decision trail and volatile state while preserving the enduring project context
and standing rules.

---

## Files in this pack

```text
Document_A_CNR3_Project_Context_and_Standing_Rules_v3_0.md
Document_B_CNR3_Restart_Work_Plan_and_First_Milestone_v3_0.md
cnr3_cache_manager_design_v7_0.md
CNR3_Handover_Pack_Production_Spec_v2_0.md
CNR3_Coder_Restart_Introduction_to_CMS07_0_FINAL.md
CNR3_Handover_Pack_v3_0_MANIFEST.md   (this file)
```

## Reading order

```text
1. CNR3_Coder_Restart_Introduction_to_CMS07_0_FINAL.md   (paste-ahead brief; start here)
2. cnr3_cache_manager_design_v7_0.md                     (CMS07.0 — the controlling design)
3. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_0.md  (context + standing rules)
4. Document_B_CNR3_Restart_Work_Plan_and_First_Milestone_v3_0.md  (current plan + milestone)
5. CNR3_Handover_Pack_Production_Spec_v2_0.md            (how this pack is regenerated)
```

## Hard precedence

```text
1. CMS07.0 (cnr3_cache_manager_design_v7_0.md) is the controlling design authority.
2. If CMS07.0 conflicts with — or is unclear in alignment with — any other document
   here (Document A, the restart intro), prior decision log, memory, or earlier
   discussion: CMS07.0 wins, unless the user explicitly says otherwise.
3. If CMS07.0 ITSELF is silent, ambiguous, or incomplete on an implementation point:
   stop and ask. Do not guess. "Final and complete" means controlling, not
   all-detail-specified.
4. Document B is volatile (re-issued per session); Document A is context/standing
   rules; both are subordinate to CMS07.0.
```

## Current authority and immediate task

```text
Controlling authority: CMS07.0 (final/complete for this restart unless revised).
Immediate task:        Cache-manager core isolation milestone (CMS07.0 §11) —
                       AFTER prevailing-rules enumeration + layout sign-off.
                       No VapourSynth wiring until the core is proven.
```

## Explicit supersession and exclusions

```text
- CMS07.0 completely supersedes CMS06.11 and all earlier CMS06.x cache designs.
- Pinning is now mandatory (supersedes old decision D13); held-ref predecessor
  reservation is superseded by consumer-pins; a checkpoint is a flag, not a pin.
- The old CMS06-era Document B (decision log) and Document C (volatile state) are
  NOT in this pack — historical archive only. CMS07.0 §9A/§12/§12A carry forward the
  still-valid rules and decisions.
- Old design documents (cnr3_cache_manager_design_v6_11.md and earlier) are historical
  archive only; CMS07.0 is the sole design authority.
- CNR2 (github.com/Asd-g/AviSynth-vsCnr2) is a PIXEL-layer salvage reference only; its
  recovery/predecessor logic must not be adopted.
```

## Not in scope for this restart pack (archive only)

```text
Document_B_CNR3_Decision_Log_v2_0.md            (CMS06.x decision trail — historical)
Document_C_CNR3_Current_Session_Handover_v2_0.md (CMS06.x volatile state — historical)
cnr3_cache_manager_design_v6_11.md and earlier   (superseded design — historical)
CNR3_Handover_Pack_Production_Spec_v1_5.md       (CMS06.x-continuity spec — superseded)
old CMS06.x reconciliation notes                 (historical)
```

## Pack integrity

SHA256 checksums:

```text
Document_A_CNR3_Project_Context_and_Standing_Rules_v3_0.md
  f5bef12e35a91dd0819e306015a234940cb3589f5ad4909114b89669e1f24108
Document_B_CNR3_Restart_Work_Plan_and_First_Milestone_v3_0.md
  9fd613c7d0d478cf6022f966e427ecc184097376184b754d92a019a70c682b58
cnr3_cache_manager_design_v7_0.md
  500b136b9d7ca181ff4867ce41daa5ae8c950840c0c753ac1c7b508f7df1d3bc
CNR3_Handover_Pack_Production_Spec_v2_0.md
  a0f68c71c85d015663599d1ef69618f1772550e84493438a21da2f80b0cf699f
CNR3_Coder_Restart_Introduction_to_CMS07_0_FINAL.md
  5809ff2e83af4093ed7d95d162eff74627d402a58cbde34457bdc149e536543c
```

*(Checksums cover the files as generated in this session. If any file is edited,
regenerate its checksum and increment the pack version.)*

## New-chat starter prompt

```text
You are resuming CNR3 development at the CMS07.0 restart. Attached: the coder restart
introduction, CMS07.0 (cnr3_cache_manager_design_v7_0.md — the controlling design),
Document A (project context + standing rules), Document B (current plan + first
milestone), and the Production Spec v2.0. CMS07.0 is the controlling authority: if it
conflicts with anything else, CMS07.0 wins; if CMS07.0 itself is unclear, stop and ask.
The CMS06-era decision log and volatile-state documents are NOT in scope. Do not rename
files or create anything from reading this. First: confirm your understanding of the
restart and the old/new separation, raise any questions on CMS07.0, enumerate all
prevailing rules for my sign-off, then propose the file/header/structure layout (text
only — no files yet).
```
