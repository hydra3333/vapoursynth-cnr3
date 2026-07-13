# CNR3 Scope Review Note - HALF-500 Cache Profile and PlanRetry Gate Rename

**Subject:** Review of `CNR3_Patch_Scope_HalfCacheProfile_and_Rename_v2.md`  
**Purpose:** Identify scope corrections before implementation, especially around HALF profile derived constants, CR4 static assertions, and rename proof gates.  
**Recommended status:** Proceed after scope amendment.

---

## 1. Overall assessment

The patch direction is sound.

The scope combines two coordinated changes:

1. Add a gated `CNR3_CACHE_PROFILE_HALF` cache profile beside the existing TINY scaffold and default NORMAL profile.
2. Rename the now-accepted PlanRetry gate from:

```cpp
CNR3_EXPERIMENT_PLAN_RETRY_BIAS
```

to:

```cpp
CNR3_ENABLE_PLAN_RETRY_BIAS
```

This is a reasonable single patch because both changes are gating/name-surface changes and the intended default behaviour remains:

```text
NORMAL cache profile
PlanRetry disabled unless explicitly enabled
```

Therefore, the default build should remain behaviourally unchanged, with the usual byte-identical proof expected.

---

## 2. Hard inconsistency to fix before coding

There is one important contradiction in the current scope.

### 2.1 A.5 correctly changes HALF's protected-set inputs

The scope correctly observes that NORMAL has:

```text
MAX_HOT_ZONES = 5
HOT_ZONE_BACK_RADIUS = 50
HOT_ZONE_FORWARD_RADIUS = 10
CHECKPOINT_MAX_RETAIN = 48
```

Using the stated protected-set formula:

```text
5 x (50 + 10) + 48 = 348
```

CR4 wants approximately twice the max-protected set, so NORMAL needs about:

```text
2 x 348 = 696
```

That fits NORMAL's `ACTIVE_CEILING_MAX_FRAMES = 1000`.

However, HALF proposes:

```text
ACTIVE_CEILING_MAX_FRAMES = 500
```

A HALF profile with `MAX_HOT_ZONES = 5` would violate CR4 because:

```text
500 < 696
```

The scope's proposed resolution is therefore correct:

```text
HALF MAX_HOT_ZONES = 3
```

Using the scope's stated arithmetic:

```text
3 x (50 + 10) + 48 = 228
2 x 228 = 456
500 >= 456
```

So Option 1 is the right starting point: preserve CR4's 2x factor and reduce HALF hot-zone count to 3.

### 2.2 A.4 and A.6 still incorrectly say the derived max-protected value stays 348

The inconsistency is that other parts of the scope still say the HALF derived value remains the NORMAL value:

```text
CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE stays 348
```

and the selftest branch should expect:

```text
max_protected 348U
```

That cannot be correct if HALF changes `MAX_HOT_ZONES` from 5 to 3 and the value is truly derived from the primitive macros.

For HALF Option 1, the expected max-protected value should be the actual value derived by the header under:

```text
MAX_HOT_ZONES = 3
HOT_ZONE_BACK_RADIUS = 50
HOT_ZONE_FORWARD_RADIUS = 10
CHECKPOINT_MAX_RETAIN = 48
```

Using the scope's stated formula, that is:

```text
3 x (50 + 10) + 48 = 228
```

If the actual header formula includes an inclusive jump span such as:

```text
HOT_ZONE_BACK_RADIUS + HOT_ZONE_FORWARD_RADIUS + 1
```

then the derived value may instead be:

```text
3 x 61 + 48 = 231
```

The implementation must confirm the actual macro formula in `cnr3_cache_core.h` and set the HALF selftest expectation to the value the header really derives.

### Required correction

Replace the stale "348 under HALF" language with:

```text
Under HALF Option 1, CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE must be the
actual value derived from HALF's primitive knobs. It must not remain the
NORMAL value 348 unless the header formula itself unexpectedly ignores
MAX_HOT_ZONES, which would itself require investigation.
```

---

## 3. "Single-variable test" wording should be revised

The current scope says HALF starts as:

```text
NORMAL values except ACTIVE_CEILING_MAX = 500
```

That was originally intended as a clean single-variable test.

However, the CR4 resolution requires:

```text
ACTIVE_CEILING_MAX_FRAMES = 500
MAX_HOT_ZONES = 3
```

So the final HALF-500 starting profile is not strictly single-variable.

That is acceptable, but the scope should say so explicitly.

Recommended replacement wording:

```text
HALF-500 starts as NORMAL except:
  ACTIVE_CEILING_MAX_FRAMES = 500
  MAX_HOT_ZONES = 3

The MAX_HOT_ZONES reduction is the required CR4-preserving companion change.
It is not an independent performance tuning decision. It exists so that the
existing 2x protected-set static_assert relationship remains intact.
```

This preserves the proof logic and prevents future readers from thinking the hot-zone reduction was accidental or unrelated.

---

## 4. Static-assert recommendation

Use Option 1 as the primary implementation path:

```text
CNR3_CACHE_PROFILE_HALF:
  ACTIVE_CEILING_MAX_FRAMES = 500
  MAX_HOT_ZONES = 3
```

Do not relax CR4 in this patch.

Preferred order:

1. Try Option 1: `MAX_HOT_ZONES = 3`.
2. If Option 1 fails for a genuine header-level reason, report the exact failing static_assert and derived values.
3. Only then consider Option 2: `MAX_HOT_ZONES = 2`.
4. Do not take Option 3, CR4 relaxation, unless explicitly re-approved as a separate design decision.

Reason:

```text
CR4 is a safety/proof relationship. Preserving it by reducing HALF's protected
hot-zone count is cleaner than making the assert profile-aware or weakening it.
```

---

## 5. Corrected HALF selftest expectation

The policy-constants selftest should gain a HALF branch, but the expected values must reflect the corrected HALF profile.

Expected HALF primitive values under Option 1:

```text
CNR3_CACHE_PROFILE_NAME              = "half-500"
ACTIVE_CEILING_MIN_FRAMES            = 150
ACTIVE_CEILING_MAX_FRAMES            = 500
CHECKPOINT_INTERVAL                  = 10
CHECKPOINT_MIN_RETAIN                = 10
CHECKPOINT_MAX_RETAIN                = 48
HOT_ZONE_FORWARD_RADIUS              = 10
HOT_ZONE_BACK_RADIUS                 = 50
MAX_HOT_ZONES                        = 3
HOT_ZONE_DECAY_MARGIN                = 20
```

Expected HALF derived values:

```text
CNR3_CACHE_JUMP_THRESHOLD
CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE
CNR3_CACHE_CHECKPOINT_GRID_FLOOR
CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS
```

must be taken from the actual derived macros. Do not hardcode NORMAL's `348U` for HALF unless the header actually derives that value after the HALF primitive branch is active.

Likely expectations based on the scope arithmetic:

```text
CNR3_CACHE_CHECKPOINT_GRID_FLOOR        = 25U
CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS = 50U
CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE   = 228U or 231U depending on actual formula
```

The implementer should confirm the exact value by compiling with `CNR3_CACHE_PROFILE_HALF` and checking the policy-constants selftest/static_asserts.

---

## 6. Rename scope assessment

The rename is appropriate now that the PlanRetry mitigation is accepted and the default sleep has been selected.

Rename:

```cpp
CNR3_EXPERIMENT_PLAN_RETRY_BIAS
```

to:

```cpp
CNR3_ENABLE_PLAN_RETRY_BIAS
```

The per-knob defines should remain unchanged:

```cpp
CNR3_PLAN_RETRY_SLEEP_MS
CNR3_PLAN_RETRY_HOLE_THRESHOLD
CNR3_PLAN_RETRY_MAX_CAP
```

The accepted default should be:

```cpp
#   define CNR3_PLAN_RETRY_SLEEP_MS        50
#   define CNR3_PLAN_RETRY_HOLE_THRESHOLD  2
#   define CNR3_PLAN_RETRY_MAX_CAP         4
```

Recommended maintainable comment:

```cpp
#   define CNR3_PLAN_RETRY_SLEEP_MS        50  // Fixed cross-CPU PlanRetry candidate; see PlanRetry ladder tests.
```

Avoid comments such as:

```text
50 is best
```

because the actual decision is more nuanced: 50 ms is the chosen cross-machine sweet spot after broader AVX2/core-count testing.

---

## 7. Rename proof gate should include enabled-build coverage

The current grep-all proof is necessary and should remain mandatory:

```text
zero occurrences of CNR3_EXPERIMENT_PLAN_RETRY_BIAS remain
CNR3_ENABLE_PLAN_RETRY_BIAS appears at every former site
DSUM-PLANRETRY block remains correctly gated
arInitial #if/#else remains correctly gated
counter gating remains correctly gated
numThreads derivation remains correctly gated
```

However, the proof gate should also include at least one compile/run with the new gate enabled.

Reason:

```text
Default-OFF testing cannot prove the renamed enabled-only path still compiles.
A missed rename inside enabled-only code could survive the default build.
```

Recommended additional gate:

```text
Canonical compile/run smoke, NORMAL profile with CNR3_ENABLE_PLAN_RETRY_BIAS defined.
This proves the renamed gate still enables the accepted mitigation path and
that DSUM-PLANRETRY still compiles and reports under the new macro.
```

Optional but preferred:

```text
HALF profile + CNR3_ENABLE_PLAN_RETRY_BIAS defined.
This proves the selected cache profile and accepted PlanRetry mitigation compose
before the measurement sweep begins.
```

---

## 8. TINY scaffold should receive at least one smoke check

Because the patch restructures every profile knob from two-way to three-way, the TINY branch is touched even though its values should not change.

Recommended additional proof:

```text
TINY scaffold smoke build/selftest, or at least policy-constants selftest under
CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY.
```

Reason:

```text
This catches accidental preprocessor drift in the old TINY branch after the
three-way restructuring.
```

---

## 9. Recommended amended proof gate

Recommended final proof gate:

```text
1. DEFAULT build:
   NORMAL profile, PlanRetry OFF.
   Canonical 4-way selftest remains 57/57.
   Behaviour unchanged except marker/name text.

2. R-PROCESS-19 byte-identical default proof:
   NORMAL profile, PlanRetry OFF.
   S8 output vs prior commit remains byte-identical.
   Proves the rename and three-way profile restructuring do not affect the default path.

3. HALF build:
   CNR3_CACHE_PROFILE_HALF defined, PlanRetry OFF.
   Canonical selftest passes.
   Policy-constants selftest confirms HALF primitive and derived expectations.

4. CR4/static_assert report:
   Document which HALF option compiled clean.
   Preferred: Option 1, MAX_HOT_ZONES = 3.
   Report actual derived CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE.
   Report that CR1-CR6 static_asserts compile clean as written.

5. Rename grep-all:
   zero old macro names remain;
   new macro appears at every former site;
   PlanRetry gated blocks are all under CNR3_ENABLE_PLAN_RETRY_BIAS.

6. NORMAL + PlanRetry ON smoke:
   CNR3_ENABLE_PLAN_RETRY_BIAS defined.
   Proves enabled PlanRetry path still compiles and DSUM-PLANRETRY reports.

7. Preferred additional composition smoke:
   HALF + CNR3_ENABLE_PLAN_RETRY_BIAS.
   Proves profile gating and PlanRetry gating compose.

8. TINY scaffold smoke:
   Confirms the three-way restructure did not damage the existing diagnostic TINY profile.
```

---

## 10. Cache-size sweep remains a measurement activity

The proposed cache-size sweep should remain separate from the commit gate unless the team explicitly wants to gate on measurement.

For the sweep:

```text
HALF-500
CNR3_ENABLE_PLAN_RETRY_BIAS ON
CNR3_PLAN_RETRY_SLEEP_MS = 50
fmParallel
3000-frame clip
```

Primary decisive metric:

```text
D-SUM-10 frames_recently_evicted_then_re_requested
```

Interpretation:

```text
~0 at 500:
  the wavefront still fits; HALF-500 is not causing destructive re-churn.

>0 and rising:
  the reduced ceiling is cutting into needed frames; 500 may be too low.
```

Secondary metrics:

```text
frames_evicted
duplicates_seen
holes_identified
recovery_span_mean
fps
```

Success condition:

```text
At 500, re-churn remains approximately 0 and duplicates/fps are close enough
to the NORMAL-1000 case.
```

Failure condition:

```text
Re-churn rises materially above 0. Back off to the smallest ceiling that holds
re-churn at 0.
```

---

## 11. Recommended corrected scope wording

The core wording to carry back into the scope:

```text
HALF-500 starts as NORMAL except ACTIVE_CEILING_MAX_FRAMES=500 and
MAX_HOT_ZONES=3. MAX_HOT_ZONES is reduced only to preserve the existing CR4
2x protected-set relationship. Derived constants must not be hardcoded for
HALF; the HALF selftest branch must expect the values actually derived from
the HALF primitive knobs.
```

And for PlanRetry:

```text
CNR3_ENABLE_PLAN_RETRY_BIAS replaces CNR3_EXPERIMENT_PLAN_RETRY_BIAS now that
the mitigation has been accepted. The selected default is 50/2/4:
sleep 50 ms, hole threshold 2, max cap 4. This remains a bounded retry bias,
not the final reservation-table architecture.
```

---

## 12. Final recommendation

Proceed with the patch after correcting the scope.

The main required amendment is:

```text
Do not expect CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE = 348 under HALF if HALF
uses MAX_HOT_ZONES = 3. Confirm and use the actual derived HALF value.
```

With that correction, the scope is coherent and the implementation can proceed without weakening CR4 or overcomplicating PlanRetry with thread-count-specific heuristics.
