# CNR3 PATCH SCOPE — Selftest skip-pass fix: KDT-conditional tests report honest not-applicable

**From:** designer/reviewer (W3D), via coordinator (W3X)
**Type:** formal patch scope (harness-only; NO production code, NO diagnostics). Implement exactly this;
propose back for review before commit. **Skip-pass fix ALONE — separate from DIAG.1.**
**Target:** `cnr3_cache_core_selftest.cpp` — the two `CNR3_KEYSTONE_DEV_TRACE`-conditional keystone tests.
**Governing docs:** Document B (parked item: "selftest skip-pass fix — dev-trace-conditional test showing
54/56 when dev-trace is OFF"); R-PROCESS-21 (do not change proven test assertions).
**Context:** the marshalling arc is complete (~-80%); the diagnostics arc is open (DIAG.1 under gate). This
fix UNBLOCKS a trustworthy default-config baseline for the whole diagnostics arc.

---

## 1. The bug (pre-existing, independent of DIAG.1)

Two registered selftests are conditionally compiled to `return Cnr3Status::lifecycle_violation` when
`CNR3_KEYSTONE_DEV_TRACE` is UNDEFINED:
```text
  cnr3_cache_core_selftest_keystone_request_plan_dev_trace_proof        (line ~15431)
  cnr3_cache_core_selftest_keystone_direct_cached_output_return_proof   (line ~15607)
      #if !defined(CNR3_KEYSTONE_DEV_TRACE)
          return Cnr3Status::lifecycle_violation;   // <-- the bug: NOT-APPLICABLE reported as a VIOLATION
      #else
          ... real KDT assertions ...
      #endif
```
The run loop is strictly binary — `cnr3_status_is_ok(status)` -> passed, else -> failed; there is NO skip
category in the run-result struct (`total/passed/failed` only) and NO skip value in the `Cnr3Status` enum.
So in the DEFAULT build config (build_config.h ships `//#define CNR3_KEYSTONE_DEV_TRACE 1` COMMENTED OUT),
these two return `lifecycle_violation` and are counted as FAILURES -> the default four-way is 54/56, not
56/56. The "56/56" baseline we have been quoting was only true with KDT turned ON.

Returning a lifecycle_violation for "this test does not apply in this build" is itself the defect: a
not-applicable test is not a failure. This is the parked "selftest skip-pass" item; DIAG.1 (the first phase
to re-run the four-way since it was parked) surfaced it. DIAG.1 is NOT the cause (its D-SUM-01 counters are
correct; the KDT-ON four-way is a clean 56/56).

## 2. The fix — return ok + emit a skip line (not-applicable = honest pass)

For BOTH tests, in the `#if !defined(CNR3_KEYSTONE_DEV_TRACE)` branch ONLY:
```text
  #if !defined(CNR3_KEYSTONE_DEV_TRACE)
      cnr3_cache_core_selftest_skip_line("<test_name>");   // honest verbose accounting (see 2.1)
      return Cnr3Status::ok;                                 // not-applicable in a KDT-off build = pass
  #else
      ... real KDT assertions UNCHANGED ...
  #endif
```
Rationale: no skip category exists in the enum or run-result, and adding one would restructure the enum +
run-result + run loop + verbose reporter (disproportionate and risky right before DIAG.1's gate). Returning
`ok` for a not-applicable-by-construction test is the correct, minimal semantics: there is nothing to fail
in a build where KDT dev-trace does not exist.

### 2.1 Preserve the information via the existing skip-line mechanism
The harness already has `cnr3_cache_core_selftest_skip_line(name)` (used for other conditional tests, e.g.
d5_recovery_pin_survives_bounded_prune_pass, hot_zone_dsum11_counter_model, etc.). Emit a skip line for each
of the two tests in the KDT-off branch so `--verbose` HONESTLY shows the test was skipped-not-run rather than
silently counting a bare pass. Match the existing skip-line call convention exactly (same function, same
placement pattern the other conditional tests use). This keeps the accounting truthful: default config reads
56/56, and verbose shows which 2 were not-run-because-KDT-off.

## 3. Hard constraints (do / do not)

```text
DO:
  - Change ONLY the two #if !defined(CNR3_KEYSTONE_DEV_TRACE) branches (lines ~15431 and ~15607):
    lifecycle_violation -> skip_line + ok.
  - Use the EXISTING cnr3_cache_core_selftest_skip_line with the two registered names verbatim:
    "keystone_request_plan_dev_trace_proof" and "keystone_direct_cached_output_return_proof".
  - Leave the #else (KDT-ON real-assertion) bodies of BOTH tests COMPLETELY UNCHANGED (R-PROCESS-21 — those
    are the proven KDT-formatting assertions; only the not-applicable path changes).
DO NOT:
  - Touch build_config.h (do NOT enable CNR3_KEYSTONE_DEV_TRACE to "fix" this — that would make the default
    config depend on a flag flip; the point is an honest default-config 56/56).
  - Add a skip category to the Cnr3Status enum or the run-result struct (out of scope; disproportionate).
  - Change the run loop, the registration table, or any other test.
  - Touch DIAG.1 / D-SUM-01 anything (separate patch, separate commit).
  - Weaken or alter any KDT-ON assertion.
```

## 4. Correctness argument

The KDT-ON path (both tests' real assertions) is untouched, so the KDT-ON four-way stays 56/56 exactly as
just observed. The KDT-OFF path changes from "false failure (lifecycle_violation)" to "honest not-applicable
pass (ok) + skip line". No production code, no diagnostics, no other test touched. The only behavioural
change is that the DEFAULT (KDT-off) build config now reports an honest 56/56 instead of a misleading 54/56,
with verbose showing the 2 skipped tests.

## 5. Proof gate

```text
1. Build Debug + Release (both projects), /arch:AVX2, DEFAULT build_config.h (CNR3_KEYSTONE_DEV_TRACE OFF,
   as committed/shipped).
2. Four-way selftest, dev-trace ON in the harness sense (i.e. the normal verbose flag, NOT the KDT macro):
   Debug 56/56; Release 56/56; forced-fail 55/56 exit 1; verbose 56/56 — and verbose shows the two
   keystone dev-trace tests as SKIPPED (skip line present), not passed-silently and not failed.
3. Regression check the OTHER direction: build once with CNR3_KEYSTONE_DEV_TRACE DEFINED and confirm the
   two tests still run their real assertions and pass (KDT-ON path unaffected) -> still 56/56.
4. No production/diagnostic behaviour touched, so no P-series / no R-PROCESS-19 obligation here — this is a
   harness-accounting fix only.
```

## 6. Why this goes BEFORE DIAG.1's commit

DIAG.1 (and every later diagnostics phase) is gated on the four-way and on the R-PROCESS-19 macro-off proof.
If the DEFAULT config is silently 54/56, every macro-off proof is muddied ("is that failure the 2 KDT tests
or my new diagnostic?"). Fixing the baseline first gives the whole diagnostics arc a clean, honest
default-config 56/56 to prove against. Commit THIS first (separately), then re-run DIAG.1's macro-on four-way
+ R-PROCESS-19 macro-off proof against the clean baseline.

## 7. Out of scope

DIAG.1 / D-SUM-01, any production code, any diagnostic gate, the Cnr3Status enum, the run-result struct, the
run loop, any KDT-ON assertion, build_config.h, any other selftest.
