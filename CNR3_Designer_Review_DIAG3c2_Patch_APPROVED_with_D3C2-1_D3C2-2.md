# CNR3 — DESIGNER REVIEW: DIAG.3c.2 patch (v2) — APPROVED (both findings cleared; proof gate next)

**From:** designer/reviewer (W3D), via coordinator (W3X).
**Re:** `CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail-v2.patch` (+ notes) — replaces v1 — against spec
v2.3 §4/§7/§8, the 3c.2 scope, the accepted confirm decisions (M1(a) / M2 / master-gate-only / E=actual /
AI-06 split), and the approved site-to-category table.
**Verdict:** **APPROVED.** v2 clears both v1 findings; commit gated only on the standard proof gate now.

## v2 CLEARANCE (the two v1 findings)
- **D3C2-1 (build-safety) — CLEARED.** Coder full-tree static check reports the FAILED-helper definitions
  (`observe_initial_failed`, `make_failed_result_from_request`, `observe_failed_from_request`,
  `observe_failed_with_progress`) inside `#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)`. Corroborated: the
  containing hunk sits in `cnr3_diag_plantrace_make_recovery_result` (itself a plantrace-gated builder region).
  Not independently provable from the diff (the enclosing `#if` opens above the hunk); the macro-off four-way
  build at `/W4 /WX` is the definitive backstop (proof gate step 3).
- **D3C2-2 (indentation) — CLEARED.** No 4-space-indented plantrace `#if` artifacts remain; the deletion scan
  dropped from 5 to 4 — the v1 re-indent artifact on the `cnr3_start_live_recovery` `set_filter_error` line is
  gone, so that proven line is now a pure insertion-before. v2 is also proper `git diff` format now.
- **Substance re-verified on v2:** 65 sites still wired (50 AR / 14 arInitial / 1 top-level, all covered);
  additive form, dump refactor, once-guard, and fence all intact; 4 remaining deletions all benign (marker,
  legend, dump-refactor-to-shared, local enum `+failed`).

## (v1 review retained below for the record)

**Original v1 verdict:** DIFF REVIEW **APPROVED IN SUBSTANCE** — complete, additive, fenced, all 65 sites wired.
Commit gated on **D3C2-1** (build-safety, confirm/fix before the four-way) plus the standard proof gate.
**D3C2-2** (indentation cleanup) recommended, not blocking. No coder redo.

---

## 1. Verified COLD against the patch

- **Invocation — all 65 sites wired (the load-bearing check for this patch).** Every `cnr3_set_filter_error`
  call has a preceding gated FAILED-writer: 50 arAllFramesReady / 14 arInitial / 1 top-level, ZERO uncovered.
  Total writer calls 68 = 65 sites + 3 legitimate splits (AI-06 refusal->15 / discharge->8, per the decision).
- **Deletion scan — 5 deletions, all benign/sanctioned:**
  1. `CNR3_EDIT_VERSION` marker bump (3c.1 -> 3c.2).
  2. legend line `... (R; FAILED: 3c.2)` -> the real FAILED legend (FAILED / X=not_reached / E=error_here).
  3. `Cnr3DiagPlanTraceOutcome` enum gains `failed` (LOCAL plan-trace enum; NOT production
     `Cnr3LiveRecoveryHoleOutcome`).
  4. clean-end dump refactored into a shared `cnr3_diag_plantrace_write_dump_to_stderr()` (anon namespace),
     with `write_clean_end_dump_to_stderr()` kept as a wrapper — the M2 shared-dump. `write_bail_dump_to_stderr()`
     added using the SAME `buffer.dumped` once-guard.
  5. one re-indent artifact on a `set_filter_error` line (see D3C2-2). No proven LOGIC removed.
- **Fence intact:** no cache-core, project files, pin-list accessor, or production-enum changes. Each site is
  additive: the gated FAILED-writer, then the EXISTING `set_filter_error(...)` + `return nullptr` (both
  preserved). Success-path O/R capture untouched except sharing the once-guard.
- **Decisions honoured:** M1(a) per-site writers + shared builders; M2 once-guarded bail dump (no reliance on
  `free_filter`); master gate only (no bail sub-gate); AI-06 code-derived split; `E` = supplied actual failing
  frame; `X` = unreached recovery-plan remainder, `[]` for non-recovery (per the notes' derivation).

## 2. FINDING D3C2-1 — build-safety (CONFIRM/FIX before the four-way; D3C1E-1 recurrence risk)

The new FAILED-helper DEFINITIONS added in `arInitial` / `arAllFramesReady`
(`observe_initial_failed`, `observe_failed_from_request`, `observe_failed_with_progress`,
`make_failed_result_from_request`) MUST sit inside the plan-trace `#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)`
block — as the 3c.1 builders do — so that with the master gate OFF they compile out entirely and produce NO
`C4505` "unreferenced local function" warnings (which fail the build under `/WX`).

Cannot be confirmed from the diff: the enclosing `#if`, if present, opens OUTSIDE the visible hunk (the helpers
are added at AR ~line 294, adjacent to the already-gated 3c.1 builders — a good sign, not proof). ACTION:
confirm the definitions are gated; if not, gate them. The macro-off four-way build (at `/W4 /WX`) is the
definitive backstop and is where this surfaces if unresolved. This is the exact class of issue as D3C1E-1
(a diagnostic-only addition stranded outside the gate).

## 3. FINDING D3C2-2 — code quality (recommend fixing; not blocking)

The inserted `#if`/observe blocks use inconsistent indentation (`#if` at 4 spaces, the observe call at 8) that
does not match the surrounding 12-space `if`-bodies, and some proven `set_filter_error` lines were re-indented
as a side effect. It compiles and behaves identically, but it is sloppy for this codebase and makes the
proven-line "touch" look larger in the diff than it is. RECOMMEND: match surrounding indentation and leave
`set_filter_error` lines at their original indent (a pure insertion before them, no re-indent).

## 4. Not fully verifiable from the diff — prove at the proof gate

- Discharge-site copy-before-discard (AR-04/13/40) and AI-06's discharge cause: the notes state facts are
  copied into diagnostic-only locals before `cnr3_discard_frame_data_with_cache()` and the FAILED record is
  written after the `discard_status` check. Trust-per-notes; verify at the failure-dump proof if a discharge
  failure is inducible.
- Recovery `X`-derivation (unreached holes + target): exercised by the refined proof method (a recovery-branch
  bail must show non-empty `X`).

## 5. Proof gate (unchanged; run after D3C2-1 resolved)

1. FAILURE-DUMP proof: induce at least one arInitial bail (minimal FAILED record, no O) AND one recovery-branch
   bail (`outcome=FAILED` + correct `fail_reason` + `E` on the failing item + NON-EMPTY `X` + progress-so-far),
   flushed BEFORE the failing function returns; a later clean-end dump must NOT duplicate (once-guard).
2. FLUSH proof: no lost tail on the induced bail.
3. R-PROCESS-19 macro-off: master gate OFF => the 65 sites byte-identical to the pre-3c.2 originals; whole
   family compiles out; four-way identical; `.vpy` A/B (`fc /b`) still PASS. (D3C2-1 surfaces here if unfixed.)
4. Four-way selftest all-on / macro-off / restored, fixture now emitting a FAILED record.
5. R-PROCESS-21/25: each site edit additive; proven bail paths otherwise unchanged; whole-patch deletion scan
   clean; cache-core + recovery selftests unchanged.
6. Clean-run S1/S7/S8 BYTE-IDENTICAL to 3c.1 (no FAILED records / `fail_reason` / X / E on success).

## 6. Commit (after D3C2-1 + proof gate green)

Seven source files + `cnr3_build_config.h` (marker only — no bail sub-gate). Marker (paste-ready, pre-commit):
`CMS07-DIAG.3c.2-plantrace-dump-on-bail-failure-detail`. Commit message: dump-on-bail + Set 4 X/E + Set 5
fail_reason across the 65 bail sites; M1(a)/M2/master-gate-only/E=actual/AI-06 split; observe-only success
paths + clean-run S-series unchanged; D3C2-1 resolved.

---
**Bottom line:** APPROVED in substance; resolve D3C2-1 (confirm/gate the helper defs), apply D3C2-2 indentation
cleanup, then run the proof gate. The three things scrutinised hardest held up: per-site additive form (no
bail-path restructuring), the shared-dump once-guard, and full 65-site invocation.
