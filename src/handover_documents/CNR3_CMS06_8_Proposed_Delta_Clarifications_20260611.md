# CNR3 CMS06.8 Proposed Delta Clarifications

**Document type:** Proposed design-spec delta for designer review  
**Target:** `cnr3_cache_manager_design_v6_7.md` -> proposed CMS06.8  
**Date:** 2026-06-11  
**Status:** Proposal for designer review/incorporation  
**Base used:** CMS06.7 uploaded by the user on 2026-06-11

---

## Purpose

This proposal records the design-spec updates suggested by the implementation
and validation work completed after CMS06.7, through
`CMS02-H15.5 / sequential fast-path return-transfer proof`.

CMS06.7 remains the current accepted base unless and until these proposed
changes are incorporated. This proposal is intentionally written as a delta,
not as a full rewritten design spec.

The main design advance since CMS06.7 is that the output-cache authority path
now has a proven sequential fast-path return-transfer path for eligible
sequential frames after frame 0:

```text
output_cache[N-1] exists
source[N] is acquired
output[N] is computed using explicit previous output N-1
output[N] is stored in output_cache
output_cache[N] is looked up and transferred as the returned frame
```

The remaining immediate gap is that `arInitial` still creates/requests the
bounded-warmup source plan before the fast-path return decision. CMS02-H15.6 and
H15.7 should address request-plan reduction separately.

---

## CMS06.8-1 - Changelog entry for CMS02-H6 through H15.5 progress

### Target location

`cnr3_cache_manager_design_v6_7.md`, Changelog, above `CMS06.7`.

### Recommended insertion

```text
### CMS06.8 - 2026-06-11

CMS06.8-1 - CMS02-H6 through H15.5 implementation progress recorded.
H6 through H8 proved bounded-warmup store, return-decision, and return-transfer
mechanics. H9 integrated output-cache authority. H10-H13 quarantined old strict
state and old strict streaming gates from selected output authority. H14.1-H14.5
normalised selected output-cache authority naming and labels. H15.1-H15.5 proved
and then activated the sequential fast-path return-transfer path for eligible
sequential frames after frame 0.

CMS06.8-2 - Sequential fast-path authority rule added.
For N > 0, when output_cache[N-1] is available through
cnr3_output_cache_find_frame_and_add_ref(), the selected path may compute output
N using cached output N-1 and source frame N, store output N in output_cache,
look up output_cache[N], transfer that lookup ref, and return it. Frame 0 remains
ineligible and uses reset/start fallback.

CMS06.8-3 - H15.5 current limitation recorded.
H15.5 proves return transfer, but does not yet reduce arInitial source-frame
requests. arInitial request-plan reduction is a separate VapourSynth lifecycle
phase because any source frame retrieved in arAllFramesReady must have been
requested in the same callback activation's arInitial.

CMS06.8-4 - H15.6 and H15.7 next-phase direction added.
H15.6 should first be an arInitial source-plan reduction probe. H15.7 should be
an active arInitial request reduction proof if H15.6 evidence is clean.

CMS06.8-5 - Old strict quarantine preserved.
The sequential fast path must not restore old_strict_cache.next_needed or
old_strict_cache.prev_output as selected output authority. Logs/evidence must
continue proving old strict non-mutation until old strict state is retired.
```

---

## CMS06.8-2 - Add sequential fast-path authority subsection

### Target location

Section 4.5 or a new Section 4.10 after Section 4.9.

### Recommended insertion

```text
### 4.10 Sequential Fast Path - Cached Predecessor Authority

After CMS02-H15.5, the selected output-cache authority path has a proven
sequential fast path for eligible frames.

Eligibility:
- requested frame N must be greater than 0;
- output_cache[N-1] must be found with cnr3_output_cache_find_frame_and_add_ref();
- source frame N must have been requested in arInitial and must be retrievable in
  arAllFramesReady;
- output N must be computed through the existing explicit-predecessor processing
  boundary;
- all caller-owned references must have a release or transfer path;
- old_strict_cache state must not be mutated or used as selected output authority.

Fast-path operation:
1. Look up output_cache[N-1] and take a caller-owned predecessor reference.
2. Retrieve source frame N.
3. Allocate local output frame N.
4. Compute output N using process_cnr3_frame_with_explicit_previous_output()
   or the currently named equivalent explicit-predecessor boundary.
5. Store output N in output_cache. Store idempotency still applies.
6. Release the local output reference after the store path has taken any cache-owned
   reference it needs, unless that local reference is explicitly transferred by a
   later proven phase.
7. Release source frame N.
8. Release predecessor lookup reference N-1.
9. Look up output_cache[N] and transfer that caller-owned lookup reference as the
   returned frame.

Frame 0 remains ineligible because it has no predecessor. It must use reset/start
or bounded-warmup normal-path handling unless a later phase explicitly supersedes
that handling.
```

### Rationale

This codifies the H15.2-H15.5 proof sequence. It also prevents future work from
falling back to bounded-window recomputation in the hot sequential case when a
valid cached predecessor is already available.

---

## CMS06.8-3 - Clarify arInitial request-plan limitation after H15.5

### Target location

Section 2.3, after VS-LIFECYCLE-01 consequence wording, and/or Section 8 H15
subphase table.

### Recommended insertion

```text
H15.5 note - return-transfer proof is not the same as arInitial request-plan
reduction.

H15.5 proves that eligible sequential frames can return through the sequential
fast path. It does not by itself prove that arInitial can stop requesting the
bounded-warmup source window. Request-plan reduction must be handled separately
because of VS-LIFECYCLE-01.

The safe sequence is:
- H15.6: probe/log the intended arInitial request-plan reduction without changing
  the requested frame set;
- H15.7: actively request only source frame N for eligible sequential fast-path
  candidates, while retaining fallback request behaviour for frame 0, holes,
  non-sequential requests, and failed eligibility.
```

### Rationale

The implementation has proven that the fast path can return output, but source
request decisions occur earlier, in arInitial. The spec should prevent a future
chat from treating H15.5 as permission to make an unproven request-plan change.

---

## CMS06.8-4 - Update CMS02-H subphase sequence

### Target location

Section 8, CMS02-H sequence.

### Recommended replacement/addition

```text
CMS02-H / SubPhase H6 / bounded-warmup-store-proof
    Status: complete / PASS.
    Proved bounded-warmup local outputs can be stored with first-in-best-dressed
    duplicate-store semantics and caller-side local-frame release discipline.

CMS02-H / SubPhase H7 / bounded-warmup-return-decision-dry-run
    Status: complete / PASS.
    Proved return-decision shape without transferring/returning output.

CMS02-H / SubPhase H8 / bounded-warmup-return-transfer-proof
    Status: complete / PASS.
    Proved lookup-ref transfer/return path for bounded warm-up output.

CMS02-H / SubPhase H9 / bounded-warmup-authority-integration-proof
    Status: complete / PASS.
    Integrated output-cache authority proof path with selected plan selection.

CMS02-H / SubPhase H10 / old-strict-state-bypass-proof
    Status: complete / PASS after plan-selection fix.
    Proved selected path can bypass old strict state.

CMS02-H / SubPhase H11 / old-strict-quarantine-proof
    Status: complete / PASS.
    Proved old strict state is not output/predecessor/return authority and is not
    mutated by the selected path.

CMS02-H / SubPhase H13 / old-strict-streaming-gate-quarantine-proof
    Status: complete / PASS.
    Proved old strict next_needed streaming gate does not reject selected
    out-of-order proof path.

CMS02-H / SubPhase H14.1-H14.5 / selected output-cache authority normalisation
    Status: complete / PASS.
    Normalised selected output-cache authority path naming from temporary cutover
    language toward normal-path and output-cache-authority helper naming.

CMS02-H / SubPhase H15.1 / output-cache authority normal-path scaffold
    Status: complete / PASS.
    Proved selected normal-path labels and gates without behavioural regression.

CMS02-H / SubPhase H15.2 / sequential predecessor-cache reuse probe
    Status: complete / PASS.
    Proved cached output N-1 is available and lookup refs can be released cleanly
    in sequential operation.

CMS02-H / SubPhase H15.3 / sequential fast-path dry run
    Status: complete / PASS.
    Proved that eligible sequential frames would reuse predecessor N-1, request
    current source N only, and compute current output N only.

CMS02-H / SubPhase H15.4 / sequential fast-path compute/store proof
    Status: complete / PASS.
    Proved compute/store using cached output N-1 plus source N, while still
    falling through to existing normal return path.

CMS02-H / SubPhase H15.5 / sequential fast-path return-transfer proof
    Status: complete / PASS.
    Proved eligible sequential frames after frame 0 can compute, store, look up,
    transfer, and return through OUTPUT-CACHE-AUTHORITY-SEQUENTIAL-FAST-PATH-RETURN.

CMS02-H / SubPhase H15.6 / arInitial source-plan reduction probe
    Status: next recommended phase.
    Must not yet change request behaviour. It should log whether a request would
    use current-source-only arInitial planning for the sequential fast path.

CMS02-H / SubPhase H15.7 / arInitial source-plan reduction active proof
    Status: pending H15.6 evidence.
    May actively reduce source requests for eligible sequential fast-path frames
    only after H15.6 evidence is clean.
```

---

## CMS06.8-5 - Add H15.5 proof evidence summary

### Target location

Section 14 implementation snapshot/evidence section.

### Recommended insertion

```text
CMS02-H15.5 proof evidence summary:

20-frame sequential validation completed.
Frame 0 fell back to OUTPUT-CACHE-AUTHORITY-NORMAL-PATH-RETURN.
Frames 1..19 emitted OUTPUT-CACHE-AUTHORITY-SEQUENTIAL-FAST-PATH-RETURN.

Representative eligible-frame fields:
- predecessor_lookup_attempted=1
- predecessor_cache_hit=1
- current_source_acquired=1
- local_output_allocated=1
- process_ok=1
- store_attempted=1
- store_ok=1
- returned_lookup_attempted=1
- returned_lookup_success=1
- returned_lookup_ref_transferred=1
- predecessor_lookup_ref_released=1
- current_source_released=1
- local_output_released=1
- returned_fast_path_output=1
- output_authoritative=1
- mutates_old_strict=0
- proof_ok=1

Pre-cleanup summary shape:
- total_cached_frame_count=20
- addframeref_total=20
- freeframe_total=0
- balance=20
- lookup_ref_acquired=39
- lookup_ref_released=19
- lookup_ref_transferred=20
- lookup_ref_balance=0
- store_attempts=20
- store_successes=20
- duplicate_skipped_already_cached=0
- duplicate_computed_but_discarded=0

After output_cache clear:
- total_cached_frame_count=0
- invariants_ok=1
- integrity_errors=0
- validation_failures=0
- ref_balance_errors=0
- addframeref_total=20
- freeframe_total=20
- balance=0
- lookup_ref_balance=0
- clear_successes=1
- clear_failures=0
```

### Rationale

This is the first proof where eligible sequential frames actually return through
the fast path rather than merely proving or dry-running eligibility.

---

## CMS06.8-6 - Preserve old strict-state quarantine language

### Target location

Sections 1, 13.17, and current implementation snapshot.

### Recommended clarification

```text
H15.5 does not make old_strict_cache final authority. The selected sequential
fast path must continue to prove old strict non-mutation. The final fmParallel
warning remains active: old_strict_cache.next_needed and old_strict_cache.prev_output
are not final output-cache authority and must not silently influence final
selected output authority.
```

### Rationale

The fast path is now authoritative for eligible sequential frames. That increases,
rather than decreases, the need to keep old strict state quarantined until it can
be removed or reduced to diagnostics-only code.

---

## CMS06.8-7 - No handover production-spec change required

### Target location

Changelog and/or companion document note.

### Recommended wording

```text
Handover Pack Production Spec v1.5 remains adequate. It already requires
current-state hard gates, output-authority discipline, ownership/release balance,
old strict-state review, and durable-rule preservation. A new production-spec
version is not required solely for CMS06.8 proposed deltas.
```

---

## Acceptance checklist for incorporating CMS06.8

Before incorporating these deltas into a full CMS06.8 base spec, verify:

```text
- H15.5 log evidence remains available or is summarised in the handover pack.
- Frame 0 fallback remains documented.
- Sequential fast-path eligibility requires N > 0 and cached output N-1.
- arInitial source-plan reduction is not claimed complete before H15.7.
- old strict-state quarantine remains a hard requirement.
- No fmParallelRequests or fmParallel readiness claim is introduced.
```
