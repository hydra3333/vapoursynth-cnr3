# CNR3 — Diagnostics Arc: Findings, Decisions & Provenance

**Version:** v1.3 (supersedes v1.2)

*** v1.3 ADVANCE (2026-07-04): DIAG.2b COMMITTED (was in-flight in v1.2). KEY DECISIONS banked this cycle:
(a) A1 — D-SUM-08 store_failures counts (status != ok && status != duplicate); duplicate is a healthy
first-in-best-dressed outcome eligible for authoritative return, NOT a failure.
(b) A2 — D-SUM-05 observes the central cache_state_invariants_hold_locked() via a DESIGNER-SANCTIONED
CNR3_DSUM05_FAIL(tag) macro on the 18 return-false sites (macro-off byte-identical); checks counted once at entry.
This is the R-PROCESS-21 mechanical-transformation precedent: a proven-code transform is permitted when
explicitly proposed and designer-approved.
(c) A3 — D-SUM-04 pin-list fields consciously omitted (Option A, avoids AS4 double-count); narrowing printed as a
summary note so the gate-comment/field mismatch self-documents.
(d) A5 — ownership_errors tripwire on the wrapper adoption-failure rebalance path only.
(e) D-SUM-04 completeness PROVEN empirically: S-series -r 1 balances zero under 168-eviction churn (S7/S8) — the
FI-12 missed-release-path risk is retired for the two narrow balances.
(f) Coder handled two unspecified subtleties unprompted: unlocked-context re-lock for wrapper-site observers
(no race), and multi-exit exactly-once store observation. DIAG.3a now scoped (D-SUM-03/12/13). ***

**Date:** 2026-06-27
**Status:** Companion record of the diagnostics discussion, the source-state findings, the agreed
sequencing/scope decisions, and WHERE each fact was found. This is a working record for the diagnostics
arc (NOW the NEXT arc: AFTER the COMPLETE W.1→W.2→W.3 live cache-pressure wiring arc, and BEFORE the real-footage campaign — the 2026-06-30 coordinator decision, which matches this record's §4.1 ordering). It is NOT a scope and NOT a CMS change — the diagnostics DESIGN is
already settled in the two specs cited below; what is captured here is the implementation-state findings
and the coordination decisions made on 2026-06-27.
**Controlling:** CMS07.15 / Production Spec v2.15 / Document A v3.11 / Document B v3.10 / slimmed DELTA v4.16. (v1.1 header reconcile, 2026-06-30: state advanced from the v1.0 D.5/52-52 baseline to the W.3-closed/55-55 seam; the DESIGN BODY of this record is UNCHANGED and durable — only these state/sequencing headers are refreshed.)
**Branch:** dev_cache_manager. Baseline (v1.1): committed through CMS07-W.3 (live cache-pressure wiring arc COMPLETE), 55/55. (v1.0 baseline was D.5/52-52.)

*** v1.2 ADVANCE (2026-07-04): the diagnostics arc has EXECUTED through DIAG.2a. Committed + pushed: selftest skip-pass fix (honest default 56/56), DIAG.1 (framework + D-SUM-01 + R-PROCESS-19 observe-only proof), DIAG.2a (D-SUM-11 hot-zone writer + D-SUM-10 prune/eviction/re-churn). Baseline now post-DIAG.2a, 56/56 default config. KEY DECISIONS since v1.1: (a) [DSUM-SUMMARY] is the diagnostics summary tag, distinct from Keystone [KDT-SUMMARY] (a collision cost a debugging cycle in DIAG.1). (b) R-PROCESS-19 observe-only proof (macro-off byte-identical) is the arc's exit gate; DIAG.2a additionally proved INDEPENDENT gate matrices (10off/11on, 11off/10on). (c) Option A mutable-diagnostic-member pattern for const-method observation (consistent with mutable cache_mutex_). (d) DIAG.2a S7/S8 finding -> FI-11: D-SUM-10 re-churn hooks the predecessor-lookup path; the costly evict-then-rebuild churn is on the recovery/anchor path, which is D-SUM-12's job (DIAG.3). (e) DIAG.2b D-SUM-04 re-sited (coder inventory review, verified): global VSFrame ref balance is unprovable (RAII releases via Cnr3OwnedFrameRef reset/dtor/transfer_to_caller); use two narrow provable balances (pin balance + lookup-ref handoff invariant) per the build_config gate comment -> FI-12 records the deferred broad detector. (f) FI-13: production-dup checkpoint-promotion signal not exposed; D-SUM-08 counts AS2 promotions only. ***

---

## 0. Why this doc exists

The coordinator recalled "a lot of prior discussion" about compile-time-gated concise telemetry (watching
hot zones, pruning, hole-filling — needed under fmParallel) and asked whether it was recorded or only in
transcripts. It IS recorded — in the diagnostics spec. This doc consolidates: (1) where the design lives,
(2) what is actually in the source today vs what is owed, (3) the agreed sequencing and coder-prep
decisions, and (4) the provenance (where each fact was found) so nothing has to be re-derived from memory.

---

## 1. The diagnostics DESIGN is fully specified (not lost in transcripts)

The compile-time gating mechanism the coordinator remembered is settled and documented.

### 1.1 Where the design lives
- **`cnr3_diagnostics_specification_v1_5.md`** — the master diagnostics design:
  - **§2.3 "Observation gates observe only"** — the per-summary COMPUTE/PRINT compile-time gate pattern.
  - **§2.3.1** — the R-PROCESS-19 compute-disabled observe-only proof obligation.
  - **§4** — the 14-family D-SUM catalogue (D-SUM-01..14), each with purpose / activation / fields /
    human interpretation.
  - The FAIL / WARN-investigate / INFO severity model.
  - The per-frame `[KDT]` line vs end-of-run `[KDT-SUMMARY]` distinction.
- **`cnr3_memory_diagnostics_spec_v2.md`** — the memory-diagnostics design (D-SUM-02 specifically).

### 1.2 The compile-time gate pattern (the "ifdef decision" — found at spec §2.3, lines ~84-152)
Each diagnostic summary has TWO independent gates, COMPUTE and PRINT, with PRINT subordinate to COMPUTE
and a paired `#error` cross-check making "print-on / compute-off" a COMPILE failure:

```cpp
#define CNR3_DIAG_COMPUTE_DSUMxx_NAME 1            // comment out to disable COMPUTE
#if defined(CNR3_DIAG_COMPUTE_DSUMxx_NAME)
#   define CNR3_DIAG_PRINT_DSUMxx_NAME 1           // print only possible if compute is on
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUMxx_NAME) && !defined(CNR3_DIAG_COMPUTE_DSUMxx_NAME)
#   error "Cannot print DSUMxx_NAME without computing DSUMxx_NAME"
#endif
```

So: comment a `#define` to turn that telemetry off entirely (zero cost when off); the `#error` makes the
print-without-compute mistake impossible to compile. This is the recorded answer to "how that had to occur
with ifdef or something."

### 1.3 Selectively-gated per-family telemetry (the "watch it bubble along" requirement)
Each of the 14 families has its OWN independent gate pair, so any subset can be turned on alone — e.g.
just hot-zone (D-SUM-11), just prune (D-SUM-10), just recovery/hole-filling (D-SUM-12). This is exactly
the selective observation needed to watch hot zones, pruning, and hole-filling — and to isolate behaviour
under fmParallel. The per-frame `[KDT]` concise line is the "bubbling along" view; the end-of-run
`[KDT-SUMMARY]` D-SUM blocks are the verification view.

### 1.4 The observe-only guarantee
**R-PROCESS-19 (D-SUM compute-disabled observe-only proof)** — turning a compute gate OFF must not change
program behaviour or output frames. This is a register-owned rule (Production Spec §3A / Document A §3A);
the diagnostics spec §2.3.1 ties the gates to it. It keeps diagnostics from ever affecting correctness.

### 1.5 The 14 D-SUM families (found at spec §4, lines 368-381)
```
D-SUM-01  Frame request arrival / ordering summary
D-SUM-02  Memory diagnostics summary
D-SUM-03  Recovery-search summary
D-SUM-04  Ownership / pin / lookup-ref balance summary
D-SUM-05  Cache integrity / teardown summary
D-SUM-06  Source-frame request / retrieve / release summary
D-SUM-07  Temporary-output / owned-output-ref lifecycle summary
D-SUM-08  Cache store / duplicate-store / first-in-best-dressed summary
D-SUM-09  Return-decision / return-transfer summary
D-SUM-10  Prune / eviction safety summary
D-SUM-11  Hot-zone operation summary
D-SUM-12  Recovery planning / hole-filling summary
D-SUM-13  Recalculation histogram
D-SUM-14  Scene-change / recursive-reset / checkpoint-promotion summary
```

---

## 2. What is actually in the SOURCE today (findings from src.zip, 2026-06-27)

The diag files are SHELLS / scaffolds, not implementations. Verified line counts and content:

| File | Lines | State |
|------|-------|-------|
| `cnr3_diagnostics.h` | 92 | generic stderr-output boundary declaration (CMS07-B.2.4) |
| `cnr3_diagnostics.cpp` | 41 | minimal generic output core |
| `cnr3_cache_diagnostics.h` | 183 | **only the D-SUM-11 hot-zone COUNTER MODEL** — `struct Cnr3CacheHotZoneDiagnosticStats` + saturating-increment observers. Header comment: "introduces only the D-SUM-11 hot-zone counter model. It does not format or print summaries..." |
| `cnr3_cache_diagnostics.cpp` | 10 | reserved stub ("reserved for later cache-specific summary formatting") |
| `cnr3_memory_diagnostics.h` | 34 | scaffold (CMS07-B.2.5) |
| `cnr3_memory_diagnostics.cpp` | 10 | explicit PLACEHOLDER — "Memory sampling and D-SUM-02 accumulation/printing will be added only in a later explicit memory-diagnostics implementation phase." |

**Finding:** of the 14 specified families, exactly ONE (D-SUM-11) has a counter model, and NONE have
end-of-run formatting/printing. The diagnostics are a declared CONTRACT with stub bodies. What is OWED is
IMPLEMENTATION, not design.

**The D-SUM-11 counter model** (already present, `cnr3_cache_diagnostics.h`): a pure counter snapshot —
no formatting, printing, heap strings, cache-mutation authority, frame ownership, or control-flow. Counter
updates may occur inside cache-lock scopes as minimal observations; summary FORMATTING must be implemented
later, outside all cache locks. This is the template the other families' counter models should follow.

**Memory diagnostics salvage** (coordinator-supplied fact): the deprecated-but-mostly-useful memory-diag
implementation is ARCHIVED in the GitHub repo. It is a strong salvage reference for D-SUM-02 — but it is
DEPRECATED: it predates CMS07, the D-SUM gate framework, R-PROCESS-19, and the print-subordinate-to-
compute discipline. So it is adapt-to-the-current-gate-pattern-and-prove-observe-only, NOT paste-in.
"Mostly salvageable" yes; "drop in as-is" no.

---

## 3. Why the clip-test harness needs the diagnostics (finding)

The real-footage clip-test harness — `test_000_Example_576p50.vpy` / `.bat` (runs 576p50 through the live
plugin to NUL or to an ffmpeg encode) — has little verification value WITHOUT the D-SUM summaries +
selectively-gated concise telemetry in place. A bare run only shows it did not crash; it does not show
whether pin balance held (D-SUM-04), recovery fired correctly (D-SUM-12), integrity stayed clean
(D-SUM-05), prune stayed safe (D-SUM-10), or scene-change/checkpoint-promotion behaved (D-SUM-14) across
thousands of real frames. Therefore the large clip-test CAMPAIGN is sequenced AFTER the diagnostics arc.

---

## 4. DECISIONS AGREED (2026-06-27)

### 4.1 Sequencing (coordinator decision)
P.11C FIRST, then the diagnostics arc, then the campaign:
```
P.11C scene-change calc (synthetic-proven, like D.1-D.5; real-footage validation deferred)
  -> Diagnostics arc (D-SUM families + compute/print gates + per-family R-PROCESS-19 observe-only proofs;
                      includes D-SUM-14 scene-change telemetry and D-SUM-02 memory via salvage;
                      includes the end-of-run integrity report + abort_on_error + warn-vs-hard-fail
                      severity policy — these are PART OF this arc, not separate)
  -> first verifiable real-footage run + the large 576p50 campaign
  -> fmParallel arc (telemetry families now available to watch concurrency)
```
Rationale: P.11C is the last piece of pipeline CORRECTNESS; finishing it first means the diagnostics arc
instruments a complete pipeline rather than a moving target. (Trade-off acknowledged: P.11C is therefore
proven on SYNTHETIC footage; its real-footage validation folds into the campaign once diagnostics exist.)

### 4.2 D-SUM-14 belongs to the diagnostics arc, not P.11C
D-SUM-14 (scene-change / recursive-reset / checkpoint-promotion summary, spec §4 / detailed at spec
~line 1718) is the family that will OBSERVE P.11C on real footage. It is implemented in the diagnostics
arc, NOT bundled into P.11C. This keeps P.11C smaller (wiring + checkpoint promotion only) and keeps the
telemetry with the rest of the diagnostics work.

### 4.3 Core-subset choice is DEFERRED pending a 2-liner menu (Claude-owed first step of the arc)
The coordinator does not yet have enough per-family info to choose which D-SUM families form the core
implementation subset vs deferred. FIRST STEP of the diagnostics arc (Claude-owed): produce a concise
2-line summary of EACH of the 14 families (purpose + what it gates/observes) so the coordinator can choose
the core subset. Candidate core (for that discussion, not yet decided): the verification set D-SUM-04
(ownership/pin balance), D-SUM-05 (integrity/teardown), D-SUM-10 (prune safety), D-SUM-12 (recovery
planning); the watch set D-SUM-11 (hot-zone, already has its counter model); D-SUM-14 (scene-change);
D-SUM-02 (memory, via salvage); plus the severity / abort_on_error policy. The arc is implementable
incrementally — family by family, each with its own gates and its own R-PROCESS-19 observe-only proof.

### 4.4 Coder preparation (recorded; ACTION AT DIAGNOSTICS-ARC KICKOFF, NOT during P.11C)
When the diagnostics arc kicks off (after P.11C), the coder's restart/scope package includes:
- `cnr3_diagnostics_specification_v1_5.md`
- `cnr3_memory_diagnostics_spec_v2.md`
- a pointer to the ARCHIVED deprecated memory-diag code in GitHub
explicitly framed as: **orientation + salvage reference; the gate framework (spec §2.3) and R-PROCESS-19
observe-only proof govern; adapt, don't paste; await per-family scope.** Per R-PROCESS-08 / R-ARCH-05/07,
the coder reads early but does NOT implement until the diagnostics phase is scoped + approved. This
prepares the coder early (the memory-diag is mostly salvageable and readily implemented once adapted)
WITHOUT breaking the propose-review-approve discipline. It is recorded as a kickoff step rather than
actioned now, so handing the coder diagnostics material does not muddy P.11C as the live task.

---

## 5. Provenance (where each fact was found)

| Fact | Source location |
|------|-----------------|
| Compile-time COMPUTE/PRINT gate pattern + `#error` cross-check | diagnostics spec §2.3, lines ~84-152 |
| R-PROCESS-19 observe-only proof obligation | diagnostics spec §2.3.1; rule owned in Prod Spec §3A / Doc A §3A |
| The 14 D-SUM families | diagnostics spec §4, lines 368-381 |
| D-SUM-14 detailed catalogue entry | diagnostics spec ~line 1718 |
| `[KDT]` per-frame vs `[KDT-SUMMARY]` end-of-run | diagnostics spec, lines ~10, ~292 |
| D-SUM-11 hot-zone counter model (only implemented family) | `cnr3_cache_diagnostics.h` (183 lines), `struct Cnr3CacheHotZoneDiagnosticStats` ~line 42 |
| Diag files are shells (line counts) | src.zip: cnr3_diagnostics.{h,cpp}, cnr3_cache_diagnostics.{h,cpp}, cnr3_memory_diagnostics.{h,cpp} |
| Memory-diag is an explicit placeholder | `cnr3_memory_diagnostics.cpp` ~line 4 ("CMS07-B.2.5 memory diagnostics placeholder") |
| Memory-diag deprecated code archived in GitHub | coordinator-supplied |
| D-SUM-02 memory design | `cnr3_memory_diagnostics_spec_v2.md` |
| Clip-test harness | `test_000_Example_576p50.vpy` / `.bat` (uploads) |
| Sequencing + coder-prep decisions | this session (2026-06-27); recorded in slimmed DELTA v4.12 §5 |

---

## 6. Cross-reference

The binding ledger entries for all of the above live in the slimmed DELTA v4.12 §5 (OWED-ITEMS LEDGER):
the "DIAGNOSTICS ARC — sequenced AFTER P.11C" entry (with the 2-liner first step and the CODER PREP
sub-note) and the "CLIP-TEST HARNESS depends on the diagnostics" entry. This companion doc is the
expanded record; the DELTA is the binding ledger.

---

*End of CNR3 Diagnostics Arc: Findings, Decisions & Provenance v1.0.*
