# CNR3 CMS06.10 Handover Reconciliation Notes

**Date:** 2026-06-11  
**Status:** Companion review/delta notes for CNR3 Handover Pack v1.9  
**Inputs:** Handover Pack v1.8, Handover Production Spec v1.5, CMS06.10  
**Output pack:** CNR3_Handover_Pack_v1.9.zip

---

## 1. Purpose

These notes reconcile the new handover pack with CMS06.10 and record which recent discussion items were intentionally promoted into the enhanced handover pack.

This is not a replacement for CMS06.10. CMS06.10 is the design authority. These notes are a handover-facing bridge so a future chat understands why v1.9 differs materially from v1.8.

---

## 2. CMS06.10 review conclusion

CMS06.10 already incorporates the main design changes from the H15.6B review:

```text
- Option B adopted: arInitial atomic predecessor find-and-addref reservation.
- frameData reserved-predecessor ownership rule added.
- single-ownership/null-on-consume rule formalised.
- H15.6B split into B.1 / B.2 / B.3.
- fail-closed-only H15.6B draft retired.
- Option A, relying on serial callback ordering, rejected.
- reservation/loss counters specified.
```

Therefore, the handover pack does not propose a CMS06.10 correction for those items. It mirrors them and makes their coding consequences explicit.

---

## 3. Handover v1.9 tracked items

### 3.1 H15.6B status and superseded draft

Handover v1.9 records that the earlier fail-closed-only H15.6B active source-request reduction patch is superseded draft work. It must not be committed.

The new sequence is:

```text
H15.6B.1 / arInitial predecessor reservation lifecycle proof
H15.6B.2 / reserved-predecessor fast-path consumption proof
H15.6B.3 / active sequential source-request reduction
```

### 3.2 Ownership-proof standing rule

Handover v1.9 records a standing rule:

```text
Lookup-addref ownership must be proven for ordinary cached-frame use.
Checkpoint pin/unpin ownership must be proven for checkpoint/recovery paths.
Future cache-affecting phases must report both where relevant.
```

### 3.3 Future H17 sparse-hole/minimal fallback recovery optimisation

Handover v1.9 records a future optimisation:

```text
Current conservative fallback may recompute/discard already-cached lower-bound frames.
Future H17 sparse-hole repair should start from nearest suitable cached predecessor/checkpoint.
It should retrieve/compute only missing forward frames plus the requested frame.
```

This remains deferred and must not be mixed into H15.6B.

---

## 4. Suggested future CMS07.0 / CMS-next items

CMS06.10 is adequate for the next coding phase. If a future CMS07.0 or CMS-next document is produced, consider promoting the following from handover-level tracking into a forward design roadmap section:

```text
- H17 sparse-hole/minimal fallback recovery as an explicit future phase family.
- Mandatory ownership-proof table for each cache-affecting phase.
- Explicit distinction between normal cached-frame lookup refs and checkpoint pin/unpin ownership.
- A final-readiness checklist for fmParallelRequests/fmParallel, including old_strict retirement, proof scaffold cleanup, and output-authority observability.
```

---

## 5. Next coding recommendation

Do not resume from `CMS02-H15_6B_active_sequential_source_request_reduction.patch`.

Start instead with:

```text
CMS02-H15.6B.1 / arInitial predecessor reservation lifecycle proof
```

The first proof should add and validate the frameData ownership/release mechanism without changing source request ranges.
