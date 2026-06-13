# CNR3 CMS07.0 Handed-Off Rules Reconciliation List

**Purpose:** Designer reconciliation aid.  
**Status:** Review aid only — not a Production Spec §3A population, and not a second rule register.  
**Date:** 2026-06-13

This file lists the rules from the earlier full enumeration that are now intended to be
**CMS07.0-DEFINED / HANDED-OFF**, not register-owned.

The row identifiers below (`HOFF-...`) are temporary reconciliation row IDs only. They
are **not** proposed permanent rule IDs. The intent is to let the designer confirm that
each originally identified rule is either already owned by CMS07.0 or should be moved
back into the register-owned list.

The register-owned rules remain elsewhere:
- authority / precedence / old-new separation;
- pack governance and §3A mechanics;
- coding process rules;
- architecture separation and salvage;
- retired-fact entries;
- candidate prior-context-derived rules.

---

## Reconciliation principles

1. **One source per rule.** If CMS07.0 already defines the rule, the Production Spec
   §3A register should not restate, index, or rename it.

2. **CMS07.0 keeps its own identifiers.** Rules such as `AS1-AS7`, `RC1-RC8`,
   `CR1-CR5`, `VS-LIFECYCLE-01`, and CMS07.0 section-numbered rules remain addressed
   by CMS07.0 identifiers/sections.

3. **Temporary row IDs are not rule IDs.** The `HOFF-*` labels in this file are only
   for reconciliation discussion.

4. **Process/proof obligations can still be register-owned.** For example, the
   obligation that compute/store/return-decision/return-transfer/output-authority be
   separately provable is register-owned as a process/proof discipline, while the
   mechanisms being proven live in CMS07.0.

---

## A. VapourSynth lifecycle and operational target

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-VS-01 | R-VS-LIFECYCLE-01 | Source retrieve must match same-activation request | CMS07.0 | CMS07.0 §9A.1 / `VS-LIFECYCLE-01`. Do not duplicate in §3A. |
| HOFF-VS-02 | R-FRAME-DATA-01 | Per-request state lives in frameData | CMS07.0 | CMS07.0 defines per-request frameData ownership/lifetime and pin-list placement. |
| HOFF-VS-03 | R-FMPARALLEL-01 | fmParallel remains the final target | CMS07.0 | CMS07.0 §2 owns this as the final operational target / invariant. Document A hand-off note now points to it. |

---

## B. Cache-core / pinning / checkpoint / hot-zone / locking / pruning

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-CACHE-01 | R-PIN-01 | Pinning is mandatory correctness baseline | CMS07.0 | CMS07.0 §0 / §4. Core design rule. |
| HOFF-CACHE-02 | R-PIN-02 | Exactly one pin concept: consumer-claim | CMS07.0 | CMS07.0 §4.7. Checkpoint is separate flag. |
| HOFF-CACHE-03 | R-PIN-03 | Production never pins | CMS07.0 | CMS07.0 §4.1 / §4.2. |
| HOFF-CACHE-04 | R-PIN-04 | Pin-and-record is one atomic operation | CMS07.0 | CMS07.0 §4.3 and AS1/AS2/AS3. |
| HOFF-CACHE-05 | R-PIN-05 | Hold-to-end final unpin | CMS07.0 | CMS07.0 §4.5 and AS4. |
| HOFF-CACHE-06 | R-PIN-06 | Single ownership / null-on-consume | CMS07.0 | CMS07.0 §4.4. |
| HOFF-CACHE-07 | R-CHECKPOINT-01 | Checkpoint is a flag, not a pin | CMS07.0 | CMS07.0 §6. |
| HOFF-CACHE-08 | R-HOTZONE-01 | Hot zones are prune-policy hints, not active-liveness guarantee | CMS07.0 | CMS07.0 §5. |
| HOFF-CACHE-09 | R-LOCK-01 | One cache-wide lock baseline | CMS07.0 | CMS07.0 §8. |
| HOFF-CACHE-10 | R-LOCK-02 | Slow work outside the cache lock | CMS07.0 | CMS07.0 §8.2 / §8.7. |
| HOFF-CACHE-11 | R-AS-01 | AS1-AS7 are inviolable | CMS07.0 | CMS07.0 §8.7. Designer-owned atomic-scope register. |
| HOFF-CACHE-12 | R-V5-FIREWALL-01 | Core refcount atomicity does not shrink lock scopes | CMS07.0 | CMS07.0 §8.6. |
| HOFF-CACHE-13 | R-PRUNE-01 | Composite eviction predicate | CMS07.0 | CMS07.0 §7.1. |
| HOFF-CACHE-14 | R-PRUNE-02 | Bounded prune decide+detach inside, free outside | CMS07.0 | CMS07.0 §7.3 / AS5. |

---

## C. Reference-count and RAII discipline

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-RC-01 | R-RC-01 | RC1 - Single store helper | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-02 | R-RC-02 | RC2 - Single remove helper | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-03 | R-RC-03 | RC3 - Store error paths rebalance | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-04 | R-RC-04 | RC4 - Lookup/addref error paths rebalance | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-05 | R-RC-05 | RC5 - Caller-owned refs discharged on every exit path | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-06 | R-RC-06 | RC6 - Shutdown clear | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-07 | R-RC-07 | RC7 - Validation enforces balance | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-08 | R-RC-08 | RC8 - First-in-best-dressed store idempotency | CMS07.0 | CMS07.0 §9A.2. |
| HOFF-RC-09 | R-RAII-01 | RAII owned-ref wrapper is baseline | CMS07.0 | CMS07.0 §9A.3. |

---

## D. Recovery, request planning, bounded-start, and scene cuts

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-RECOVERY-01 | R-RECOVERY-01 | Recovery search is bounded by interval, not global-then-reject | CMS07.0 | CMS07.0 recovery/search model. |
| HOFF-RECOVERY-02 | R-RECOVERY-02 | Two-phase recovery model | CMS07.0 | Descending search, then ascending fill-holes-only walk. |
| HOFF-RECOVERY-03 | R-REQUEST-01 | Dissolved source window | CMS07.0 | Request source N plus genuine holes only; no blanket backward window. |
| HOFF-RECOVERY-04 | R-BOUNDED-START-01 | Bounded-start honesty | CMS07.0 | CMS07.0 §9A.7. Note: process docs may mention the obligation, but authoritative rule text lives in CMS07.0. |
| HOFF-RECOVERY-05 | R-SCENE-CUT-01 | Scene cuts sever the chain and become checkpoints | CMS07.0 | CMS07.0 algorithm/checkpoint model. |

---

## E. Constants and parameter-coherence rules

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-CONST-01 | R-CR-01 | CR1 - JUMP_THRESHOLD is derived | CMS07.0 | CMS07.0 §10.2. |
| HOFF-CONST-02 | R-CR-02 | CR2 - BACK_RADIUS bounds recovery and blend settling | CMS07.0 | CMS07.0 §10.2. |
| HOFF-CONST-03 | R-CR-03 | CR3 - BACK_RADIUS tracks checkpoint interval density | CMS07.0 | CMS07.0 §10.2. |
| HOFF-CONST-04 | R-CR-04 | CR4 - Active ceiling must exceed protected set | CMS07.0 | CMS07.0 §10.2. |
| HOFF-CONST-05 | R-CR-05 | CR5 - Checkpoint retain density floor | CMS07.0 | CMS07.0 §10.2. |
| HOFF-CONST-06 | R-CR-06 | decay_margin bound | CMS07.0 | CMS07.0 §10.2. The register-owned comment rule can require CR comments, but CR substance lives in CMS07.0. |

---

## F. Instrumentation and recovery-search summary

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-INSTR-01 | R-INSTRUMENT-01 | Recovery-search summary required | CMS07.0 | CMS07.0 §10.4 / §10.5. |
| HOFF-INSTR-02 | R-INSTRUMENT-02 | Prune K pressure must be instrumented | CMS07.0 | CMS07.0 bounded-prune instrumentation. |

---

## G. First-milestone proof gates and layout boundary

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-MILESTONE-01 | R-MILESTONE-01 | Prove ownership before behaviour | CMS07.0 | CMS07.0 §11 / D30 first milestone. Document B tracks the current work-plan, but CMS07.0 owns the milestone proof obligations. |
| HOFF-MILESTONE-02 | R-LAYOUT-01 | Layout proposal before file creation | REGISTER-OWNED / PROCESS | This one should NOT be handed off if treated as a no-action/process gate. It belongs with register-owned process/no-action rules. Included here because it was near the first-milestone group in the original list; designer should confirm it remains register-owned. |

---

## H. Borderline / deliberately retained as register-owned process

These were in or near the original handed-off groups but should remain register-owned
as process/proof obligations, not CMS07.0 design mechanisms.

| Temp ID | Original label | Rule title from earlier enumeration | Intended owner | Notes for designer reconciliation |
|---|---|---|---|---|
| HOFF-BORDER-01 | R-DCR-01 | Design Compliance Review after each coherent block | REGISTER-OWNED / PROCESS | Obligation to run DCR is process-owned; checklist contents live in CMS07.0 §9A.8. |
| HOFF-BORDER-02 | R-DIAGNOSTICS-01 | Diagnostics are a hard gate | REGISTER-OWNED / PROCESS | Gate is process-owned; detailed counters/values are CMS07.0-defined. |
| HOFF-BORDER-03 | R-PROCESS-06 | Output-authority proof discipline | REGISTER-OWNED / PROCESS | Separate-provability is process-owned; mechanisms being proven live in CMS07.0. |
| HOFF-BORDER-04 | R-COMMENT-01 / CR comments part | CR1-CR5 must be codified as comments above constants | REGISTER-OWNED / PROCESS plus CMS07.0 | Commenting obligation is process-owned; CR substance is CMS07.0-defined. |

---

## I. Possible reconciliation questions for designer

1. Confirm that HOFF-VS-01 through HOFF-INSTR-02 are all CMS07.0-defined and should not
   appear in §3A except via the general hand-off clause.

2. Confirm that HOFF-MILESTONE-01 is CMS07.0-defined, while HOFF-MILESTONE-02 is
   register-owned process/no-action.

3. Confirm that the borderline entries in Section H remain register-owned as process
   obligations, with their underlying technical mechanisms handed off to CMS07.0.

4. Confirm whether `bounded-start honesty` should be fully CMS07.0-defined or whether
   there should also be a register-owned process wording requiring docs/diagnostics not
   to misrepresent bounded starts. Current recommendation: leave rule substance in
   CMS07.0, and avoid duplicate §3A wording unless designer sees a process gap.

5. Confirm that no permanent `R-...` IDs should be assigned to the CMS07.0-defined
   rules; CMS07.0 identifiers and section names remain their only durable addresses.
