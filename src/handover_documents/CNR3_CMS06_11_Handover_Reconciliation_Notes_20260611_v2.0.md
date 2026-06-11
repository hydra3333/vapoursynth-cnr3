# CNR3 CMS06.11 Handover Reconciliation Notes

**Date:** 2026-06-11  
**Status:** Companion review/delta notes for CNR3 Handover Pack v2.0  
**Inputs:** Handover Pack v1.9, Handover Production Spec v1.5, CMS06.11, designer review comments 2026-06-11  
**Output pack:** CNR3_Handover_Pack_v2.0.zip

---

## 1. Purpose

These notes reconcile the v1.9 handover pack with CMS06.11. They explain why v2.0 exists and what changed since v1.9.

This is not a replacement for CMS06.11. CMS06.11 is the design authority. These notes are a handover-facing bridge so a future chat understands why v2.0 supersedes v1.9.

---

## 2. Why v2.0 was required

v1.9 treated CMS06.10 as controlling. CMS06.11 now supersedes CMS06.10. CMS06.11 was warranted because one of the CMS06.10 review points was a real correctness fix to the spec body: stale H15.6/H15.7 wording contradicted the current H15.6A / H15.6B.1 / H15.6B.2 / H15.6B.3 phase plan.

The designer accepted all eight coder review points and produced CMS06.11 rather than CMS06.10a.

---

## 3. CMS06.11 changes mirrored into v2.0

The v2.0 handover pack mirrors these CMS06.11 items:

```text
- CMS06.11 is now controlling; CMS06.10 is superseded.
- H15.6B.3 eligibility requires an actually held reserved predecessor ref.
- frameData release helper must be named/null-guarded/counter-correct.
- H15.6B.2 PASS fields must prove null-on-consume and cleanup behaviour.
- reserved predecessor refs are part of lookup-ref accounting.
- reserved predecessor refs use lookup-addref, not checkpoint/non-checkpoint pinning.
- H17 sparse-hole/minimal fallback repair is deferred.
- the fail-closed-only H15.6B draft remains superseded and is now warned about in Section 8 of CMS06.11.
```

---

## 4. Current next implementation task after reconciliation

The next coding task remains:

```text
CMS02-H15.6B.1 / arInitial predecessor reservation lifecycle proof
```

but it must now be implemented against CMS06.11, not CMS06.10.

H15.6B.1 must not reduce source requests.

---

## 5. Items intentionally not changed

This handover update does not change:

```text
- H16.3 PASS/committed status;
- H16.4 PASS/committed status;
- pre-H15.6B checkpoint status;
- old strict state quarantine;
- deferred fmParallelRequests/fmParallel readiness;
- the future H17 item remaining deferred;
- handover production spec v1.5.
```

---

## 6. Future handover note

Future handover packs should carry CMS06.11 as the design authority until a later CMS version explicitly supersedes it. If another CMS version is produced before H15.6B.1 coding starts, regenerate this pack or add a clear current-state override before starting coding.
