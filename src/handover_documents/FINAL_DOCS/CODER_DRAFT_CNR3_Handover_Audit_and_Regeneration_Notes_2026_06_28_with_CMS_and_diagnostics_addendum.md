# CNR3 handover-pack audit and regeneration notes

Date: 2026-06-28

## Scope reviewed

Uploaded files reviewed:

- CNR3_Handover_Pack_Production_Spec_v2_12.md
- Document_A_CNR3_Project_Context_and_Standing_Rules_v3_6.md
- Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_6.md
- CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_2.md
- CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_12.4.md
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13.1.md
- CNR3_Diagnostics_Arc_Condensed_Plan_v1_1.txt

## Review verdict

The uploaded set was close, but not safe enough for a memoryless coder restart without correction.

The main issue was not loss of detail. The main issue was mixed-state orientation. A secondary issue was that the uploaded set did not include every file the Production Spec says a complete pack needs:

- several files correctly said P.11C is closed and live cache-pressure wiring is next;
- some active or near-active text still said P.11C was next, first real-footage was next, or the keystone was the current live task;
- Document A and Document B had stale concrete pointer lines (e.g. Document A v3.5 / Production Spec v2.11) despite newer headers;
- the DELTA baseline still showed 52/52 and v2.11/v3.5 in its current baseline block;
- the next-step instruction needed to reflect the latest banked decision: Step 0 is a CMS sensibility/gap review for hot zones/pruning before coding live wiring.

## Generated replacement versions

- CNR3_Handover_Pack_Production_Spec_v2_13.md
- Document_A_CNR3_Project_Context_and_Standing_Rules_v3_7.md
- Document_B_CNR3_Restart_Work_Plan_and_Current_State_v3_7.md
- CNR3_Coder_Restart_Introduction_to_CMS07_RESUME_v6_3.md
- CNR3_THIS_CHAT_DELTA_current_state_SLIMMED_v4_13.md
- CNR3_CMS_Future_Investigations_and_Open_Questions_v7_13.2.md
- CNR3_Diagnostics_Arc_Condensed_Plan_v1_2.txt
- CNR3_Handover_Pack_MANIFEST_v3_7_handoversafe.md

## Key correction applied across the set

The next action is now consistently:

```text
Step 0: joint CMS sensibility review for hot-zone/prune live wiring.
```

The documents now avoid implying that hot-zone/prune wiring should be coded directly from the current CMS without first reviewing whether the CMS is still sensible and complete against the post-P.11C.5 implementation state.

## Current state now recorded

```text
Latest committed phase:
  CMS07-P.11C.5-scene-cut-checkpoint-recovery-anchor-proof

Selftests:
  53/53 PASS
  forced-fail 52/53 with expected invariant_violation and exit 1
  verbose 53/53 PASS

P.11C:
  CLOSED (.1-.5)

Known live functional gap:
  cache-core hot-zone/prune componentry is built and proven;
  live getFrame has zero live callers for hot-zone observation / retirement / prune pass;
  live store appends without soft prune-trigger wiring.

Next:
  review CMS hot-zone/prune sensibility/gaps;
  then, if approved, scope hot-zone observation and prune wiring.
```

## Important remaining pack gap

The actual controlling CMS file was not included in this upload:

```text
cnr3_cache_manager_design_v7_13.md
```

A memoryless coder restart must receive that file. The production spec and generated documents point to it, but they do not replace it. The CMS is still the design authority.

The following files are also referenced by the pack/doc set but were not included in this upload:

```text
CNR3_Handover_Pack_<version>_MANIFEST.md
Role Handover document
Reviewer Introduction document
cnr3_diagnostics_specification_v1_5.md
cnr3_memory_diagnostics_spec_v2.md
```

Those may be outside the immediate handover-refresh task, but if the goal is a fully self-contained restart pack, they should either be supplied or explicitly marked as unavailable / not required for the next Step 0 review.

## Caution

These regenerated files preserve the uploaded documents and make targeted state/orientation corrections. They do not perform the actual CMS hot-zone/prune review, because the controlling CMS file was not included in this upload.

---

## Addendum — CMS supplied after initial regeneration

After the first regenerated pack was produced, the controlling CMS was supplied as `cnr3_cache_manager_design_v7_13.1.md`. Its content declares **Version: CMS07.13** and records the P.11C implementation-state note as additive, not a design-version change. To satisfy the Production Spec pack filename expectation, the handover-safe zip now includes that same content under `cnr3_cache_manager_design_v7_13.md`.

The CMS front matter still has two restart-era/currency wording issues that are worth correcting in a future CMS reissue, but which do not alter the design rules:

```text
1. The Status paragraph still says the CMS is ready for the isolated cache-core milestone.
2. The companion-document example still says `_v7.10.md` rather than the current v7.13-series companion.
```

These were not silently edited in the CMS content. They are recorded here and in the manifest so a new memoryless chat is not misled.


---

## Addendum — diagnostics specifications supplied after CMS-inclusive pack

After the CMS-inclusive handover zip was produced, the diagnostics specifications were supplied:

```text
cnr3_diagnostics_specification_v1_5(3).md
cnr3_memory_diagnostics_spec_v2(1).md
```

For handover-pack consistency they have been normalised inside the new zip to:

```text
cnr3_diagnostics_specification_v1_5.md
cnr3_memory_diagnostics_spec_v2.md
```

This resolves the previous diagnostics-specification pack gap. These files are included for diagnostics-arc completeness and for the future condensed diagnostics plan. They do not change the current next action, which remains Step 0 joint CMS sensibility review for live hot-zone/prune wiring before any implementation patch.

Caution: `cnr3_memory_diagnostics_spec_v2.md` retains older front-matter saying `Companion document: CMS06`. It is included as the memory-diagnostics formatted-output specification referenced by the diagnostics plan, not as a cache-design authority. The CMS remains the controlling design authority.
