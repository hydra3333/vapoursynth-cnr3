# CNR3 CMS06.1 Update Recommendations after CMS02-G.9AB

**Document type:** Targeted recommendation note for `cnr3_cache_manager_design_v6.1.md`  
**Date:** 2026-06-06  
**Status:** Recommendation file, not a wholesale replacement specification  
**Scope:** Bring CMS06.1 current-state and phase-plan notes into alignment with development completed through CMS02-G.9AB and the planned CMS02-G.10ABC dry-run compute skeleton.

---

## 1. Purpose

This file recommends targeted updates to `cnr3_cache_manager_design_v6.1.md`.

It does not attempt to rewrite CMS06.1 wholesale. CMS06.1 remains the detailed design authority unless and until a later design spec explicitly supersedes it.

The main issue is that CMS06.1 predates the completed CMS02-G.7/G.8/G.9AB proof work. Its implementation-state snapshot and phase plan should be updated so a future chat does not mistake old snapshot text for current state.

---

## 2. Recommended update: implementation-state snapshot

Add or update a current-state note near CMS06.1 Section 14 or the phase sequence:

```text
Implementation-state update after CMS02-G.9AB:

The current normal committed state is:
    CNR3_EDIT_VERSION = CMS02-G9AB-source-frame-set-proof-disabled-v1

Completed after the original CMS06.1 snapshot:
    CMS02-G.7A/G.7B/G.7C:
        per-invocation source-request-plan skeleton, lifecycle proof, and
        widened source-request/retrieve proof.

    CMS02-G.8A/G.8B/G.8C/G.8D:
        recovery decision/walk skeleton, checkpoint selection, checkpoint
        ref acquisition/release, checkpoint rollover, and pre-store
        would_compute detection.

    CMS02-G.9AB:
        local recovery source-frame-set acquisition/release proof.

All G.7/G.8/G.9 proof paths are disabled in the normal committed state.
The old strict-streaming path remains output-authoritative.
The output cache is still not output-authoritative.
Recovered outputs are not yet computed, stored, or returned.
```

---

## 3. Recommended update: phase plan after CMS02-G.9AB

Add the next phase plan before any real recovery computation phase:

```text
Phase CMS02-G.10ABC - Dry-run non-mutating recovery compute skeleton

Status:
    Next recommended phase after CMS02-G.9AB.

Purpose:
    Prove the future recovery compute orchestration shape without actual
    recovered-frame computation.

Allowed:
    - prepare bounded recovery plan;
    - identify checkpoint and walk range;
    - inspect/source-frame-set availability;
    - log would-compute steps;
    - log predecessor requirements;
    - prove cleanup paths;
    - disable proof gates again before normal commit.

Not allowed:
    - allocate recovered output frames;
    - call process_cnr3_frame() for recovery computation;
    - store recovered outputs;
    - return recovered outputs;
    - mutate d->old_strict_cache.prev_output;
    - mutate d->old_strict_cache.next_needed;
    - change output authority;
    - enable fmParallelRequests or fmParallel.

Reason:
    process_cnr3_frame() currently reads the recursive predecessor from
    d->old_strict_cache.prev_output. Actual recovery computation needs a
    non-mutating processing boundary with explicit predecessor input, or an
    equivalent safe design.
```

Then add a later phase placeholder:

```text
Phase CMS02-G.10D or later - First actual local recovered-frame computation proof

Purpose:
    Compute recovered frames locally, then immediately release them.

Precondition:
    A safe processing boundary exists that does not depend on or mutate
    old_strict_cache previous-output state.

Still not allowed until separately proven:
    - storing recovered outputs as authoritative;
    - returning recovered outputs;
    - changing output authority.
```

---

## 4. Recommended update: mark proof phases completed

Add a completion table similar to:

| Phase | Status | Evidence |
|---|---|---|
| CMS02-G.7A | Complete | Disabled source-request-plan skeleton compiled/run clean. |
| CMS02-G.7B | Complete | Source-request-plan create/consume/destroy lifecycle proven. |
| CMS02-G.7C | Complete | Widened source request/retrieve proof passed and disabled. |
| CMS02-G.8A | Complete | Disabled decision/walk skeleton compiled/run clean. |
| CMS02-G.8B | Complete | Enabled post-store decision/walk proof passed; checkpoint rollover observed. |
| CMS02-G.8C | Complete | Decision/walk probe moved pre-store; disabled state clean. |
| CMS02-G.8D | Complete | Pre-store decision/walk proof showed current frame `would_compute=1`; disabled state restored. |
| CMS02-G.9AB | Complete | Local source-frame-set acquire/release proof passed; disabled state restored. |
| CMS02-G.10ABC | Next | Dry-run compute orchestration only. |

---

## 5. Recommended update: preserve final target wording

CMS06.1 already clarifies that `fmParallel` is the final target. Keep that wording, but tighten it where helpful:

```text
Current implementation target:
    safe under fmUnordered now.

Structural design target:
    compatible with future fmParallelRequests.

Final long-term target:
    safe operation under fmParallel, subject to later explicit design review.
```

Do not let interim `fmUnordered` work introduce shared current-request state or ordering assumptions that would block future concurrent modes.

---

## 6. Recommended update: add named deferred items

### G-PAR-HZ-ARINITIAL-01 - hot-zone update at arInitial

Add this as a prerequisite note before `fmParallelRequests` or `fmParallel` work:

```text
Hot-zone updates must occur at arInitial before fmParallelRequests/fmParallel
work. Under concurrent request modes, deferring hot-zone update to
arAllFramesReady can allow pruning decisions to ignore active request intent.
```

Current status:

```text
The current code appears to update hot zones at arInitial. Preserve this.
```

### G-DIAG-RECALC-HIST-01 - recalculation histogram

Add this as deferred diagnostics:

```text
Add a compile-time-only per-instance recalculation histogram showing how many
frames were calculated once, twice, three times, etc. Count true calculation
work, not cache returns. This is useful once recovery can compute/duplicate
work under first-in-best-dressed store idempotency.
```

### G-DIAG-LOG-VOLUME-01 - long-run diagnostic throttling

Add this as deferred diagnostics:

```text
Add compile-time diagnostic verbosity controls for 50+ and 100+ frame tests so
routine long-run logs remain readable while detailed proof logs remain available
for the currently active change.
```

---

## 7. Recommended update: note comment/label drift but do not mix with G.10ABC

Several code comments and diagnostic labels still use CMS05/CMS05-3A wording. This is documentation drift, not necessarily a functional defect.

Recommendation:

```text
Do not combine broad CMS05/CMS06 wording cleanup with CMS02-G.10ABC.
```

Reason:

```text
G.10ABC is a safety-critical proof phase. Cleanup-only wording changes make
patch review harder and increase the chance of accidental logic changes.
```

Handle wording cleanup as a separate small phase after G.10ABC or during a dedicated documentation/comment cleanup pass.

---

## 8. Recommended update: warn about process_cnr3_frame predecessor coupling

Add this near the G.10 phase plan:

```text
process_cnr3_frame() currently obtains the recursive predecessor from:
    d->old_strict_cache.prev_output

That is correct for the current strict-streaming path, but unsuitable for actual
local recovery computation because recovery must compute from an explicit local
predecessor without mutating old_strict_cache state.

Before actual recovered-frame computation, introduce or design a safe processing
boundary that accepts an explicit predecessor frame, or otherwise proves that
old strict-streaming state cannot be modified or misread by recovery.
```

---

## 9. Recommended update: retain current output-authority warning

Add or preserve this statement prominently:

```text
The output cache is not yet output-authoritative.
The old strict-streaming path remains the source of returned frames.
Recovery scaffolding is proof-only and disabled in normal committed state.
```

Do not describe recovery as implemented until recovered outputs are actually computed, stored, returned, and proven.
