# CNR3 Handover Pack — RESUME v3.2 — Manifest

**Date:** 2026-06-18  
**Purpose:** Reading order and contents for the CNR3 coder RESUME handover pack. This pack
resumes an in-progress, proven build at phase **CMS07-H.2A** (cache core proven through
H.1A, 28/28 selftests). It is NOT a fresh-start pack.

---

## Pack status

```text
Pack type:        RESUME (in-progress build), not initial start.
Controlling CMS:  CMS07.2.
Production Spec:  v2.4 (includes R-PROCESS-19).
Build state:      cache core proven through CMS07-H.1A; 28/28 isolated selftests.
Next phase:       CMS07-H.2A (recovery anchor pin-record) — to be regenerated and reviewed.
```

---

## Files in this pack (read in this order)

```text
1. CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v3_2.md
      Paste FIRST. Frames the resume, the build cadence, old/new separation, the five
      traps, the engineered guards, and the "when in doubt, raise it for review" rule.
      Directs the first action: confirm build state from the repository.

2. cnr3_cache_manager_design_v7_2.md
      CMS07.2 — controlling design authority. Supersedes CMS07.1 and CMS07.0.
      (CMS07.1 added §6.6 monotonic checkpoint; CMS07.2 added the companion-doc note.)

3. Document_A_CNR3_Project_Context_and_Standing_Rules_v3_2.md
      Project context front door. Reproduces Production Spec §3.2 canonical context and
      the §3A register-owned rules verbatim, including R-PROCESS-19.

4. Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_2.md
      RESUME-state work plan and CURRENT BUILD STATE. Phase history through H.1A, the
      working method, invariant lock disciplines, the salvage inventory (§8.5), the next
      phase (H.2A), the do-not-implement list, and proof obligations. Read this for where
      the build actually is.

5. CNR3_Handover_Pack_Production_Spec_v2_4.md
      Production specification. §3.2 canonical context master + populated §3A Prevailing
      Rules Register (the authoritative rule wording, including R-PROCESS-19).

6. cnr3_diagnostics_specification_v1_3.md
      Diagnostics specification. Subordinate to the CMS and §3A.
```

## Reference material — NOT part of the authoritative pack

```text
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7.2.md
      NON-NORMATIVE companion to CMS07.2 (version-paired by filename). Deferred tuning
      questions only (e.g. FORWARD_RADIUS at higher thread counts). NOT controlling, NOT
      to be implemented. Provided for context only; ignore for implementation.

- src/superseded_by_v7/*.txt  (in the repository, not bundled here)
      Old pre-CMS07 source retained for salvage reference. See Document B §8.5 for the
      tiered inventory (high-value salvage / caution / quarantine). Salvage is a later
      step, per-case approval only.
```

---

## Authority and precedence (summary)

```text
- CMS07.2 is the controlling design authority. If it conflicts with or is unclear against
  prior material, CMS07.2 wins unless the user says otherwise. If CMS07.2 is silent or
  ambiguous on an implementation point, STOP and ask.
- Production Spec §3A holds the authoritative register-owned rule wording. Document A
  reproduces §3A; on any mismatch, §3A (the spec) wins.
- Document B is volatile/current-state; if it ever conflicts with the CMS, the CMS wins.
- "CMS07.0" in reproduced rule text means the latest prevailing CMS (CMS07.2).
```

---

## Current build state (authoritative source: the repository)

```text
Repository:    https://github.com/hydra3333/vapoursynth-cnr3
Local tree:    E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github
Build:         Visual Studio 2026, x64.
Latest commit: CMS07-H.1A: prove bounded recovery search scaffold
Edit marker:   CMS07-H.1A-as1-bounded-recovery-search-scaffold-proof
Selftests:     28/28 PASS (Debug + Release); forced-fail 27/28 exit 1; verbose 28/28.

The coder confirms this from the repo as its first action (Introduction §2 / Document B
§3) before proposing the next phase.
```

---

## Note on document versions

```text
This RESUME pack supersedes the earlier start-oriented pack:
    - Introduction: the start-oriented intro is replaced by the RESUME introduction here.
    - Document A:   v3.1 -> v3.2 (Spec v2.4 / CMS07.2 pointers; R-PROCESS-19 added).
    - Document B:   the v3.1 "first milestone" plan is replaced by the v3.2 RESUME-state
                    work plan (current build state, working method, salvage inventory).
    - CMS:          CMS07.0 -> CMS07.2 is the controlling design.
    - Spec:         v2.2/v2.3 -> v2.4.
Do not use the older start-oriented documents to drive the resume.
```
