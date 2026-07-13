# CNR3 — PATCH SCOPE: HALF-500 cache profile + plan-retry rename (single patch) — v1

**Marker on success:** `CMS07-FEATURE.half-cache-profile-and-retry-rename`
**Baseline:** committed `CMS07-EXPERIMENT.plan-retry-bias` tree.
**Nature:** two coordinated changes in one patch — (A) a new gated HALF cache profile beside NORMAL/TINY,
and (B) a mechanical rename of the now-accepted plan-retry macro. Both are gated/name changes; default
build behaviour (no new macro defined, old default profile) stays byte-identical.

---

## ITEM A — HALF-500 cache profile (gated, separate tunable values)

### A.1 Structure
The cache profile currently uses a two-way `#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY) / #else`
(TINY vs NORMAL; NORMAL is the default `#else`, not itself gated). Convert to three-way on EVERY
profile knob:

```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
    ... tiny-100 value ...
#elif defined(CNR3_CACHE_PROFILE_HALF)
    ... HALF value (own tunable number) ...
#else
    ... NORMAL value (UNCHANGED; still the default) ...
#endif
```

W3X ruling: HALF gets its OWN explicit value for every primitive knob (separate tuning surface), NOT a
fallthrough to NORMAL. HALF-500 STARTS as NORMAL except TWO coupled changes:
    ACTIVE_CEILING_MAX_FRAMES = 500   (the purpose of the profile)
    MAX_HOT_ZONES             = 3     (the REQUIRED CR4-preserving companion — not an independent tuning
                                       decision; it exists so the 2x protected-set static_assert holds)
So the first sweep is a two-coupled-variable test, stated honestly (coder review section 3 accepted).
Every other HALF knob is independently tunable later without touching NORMAL.

### A.2 The gate define (cnr3_build_config.h, near the TINY scaffold gate)
```cpp
//  HALF cache profile: half-size active ceiling (500) for lower memory footprint on end-user PCs.
//  Uncomment to select. Mutually exclusive with the TINY diagnostic scaffold.
//#define CNR3_CACHE_PROFILE_HALF 1
```
Add an exactly-one guard:
```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY) && defined(CNR3_CACHE_PROFILE_HALF)
#   error "Select at most ONE cache profile: TINY scaffold OR HALF, not both."
#endif
```

### A.3 Primitive knobs — three-way, HALF starts = NORMAL except the ceiling
(All in cnr3_cache_core.h. TINY column shown for reference; do not change TINY or NORMAL values.)

| Knob | TINY | HALF (start) | NORMAL | HALF intent |
|---|---|---|---|---|
| CNR3_CACHE_PROFILE_NAME | "tiny-100" | "half-500" | "normal" | self-identifying logs |
| ACTIVE_CEILING_MIN_FRAMES | 40 | 150 | 150 | copied; tunable |
| ACTIVE_CEILING_MAX_FRAMES | 100 | **500** | 1000 | **THE deliberate change** |
| CHECKPOINT_INTERVAL | 3 | 10 | 10 | copied; tunable |
| CHECKPOINT_MIN_RETAIN | 4 | 10 | 10 | copied; tunable |
| CHECKPOINT_MAX_RETAIN | 12 | 48 | 48 | copied; tunable |
| HOT_ZONE_FORWARD_RADIUS | 3 | 10 | 10 | copied; tunable |
| HOT_ZONE_BACK_RADIUS | 15 | 50 | 50 | copied; tunable |
| MAX_HOT_ZONES | 2 | 3 | 5 | **reduced so 500 fits CR4 (see A.5)** |
| HOT_ZONE_DECAY_MARGIN | 6 | 20 | 20 | copied; tunable |

Mark each copied HALF knob with a short comment: `// HALF: copied from NORMAL; tunable.` and the ceiling
with `// HALF: deliberate half-size change.` so future drift is visible and intent is explicit.

### A.4 DERIVED knobs — DO NOT give HALF a hardcoded value; note TWO of them CHANGE under HALF
These auto-compute from the primitives; adding HALF literals would risk inconsistency. Leave the derivations
untouched. CORRECTED (v3, coder review + designer cold-verify of the actual formulas at cnr3_cache_core.h
~225-260): because HALF reduces MAX_HOT_ZONES to 3, two derived values CHANGE and the selftest HALF branch
must expect the NEW derived values, not NORMAL's:
- CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE = zones x (BACK + FORWARD) + MAX_RETAIN (no +1 term; formula
  verified cold) = 3 x 60 + 48 = **228** under HALF (NOT 348 — v2's "stays 348" was a scope error).
- CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE = zones x (BACK / INTERVAL) = 3 x 5 = **15** under HALF
  (NOT 25 — this one derives from MAX_HOT_ZONES too; both prior reviews missed it).
Derived values that DO NOT change under HALF Option 1:
- CNR3_CACHE_JUMP_THRESHOLD = FORWARD + BACK + 1 = 61 (radii unchanged).
- CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS = BACK_RADIUS = 50 (unchanged).
- BYTE_BUDGET_BYTES (1 GiB) — unchanged; ceiling clamps from byte budget then bounds to MIN/MAX; lowering
  MAX to 500 is the operative change.
All 14 header static_asserts VERIFIED COLD to pass under HALF Option 1 (500/zones=3/rest NORMAL), including
CR4 (500 >= 2x228=456) and CR5 (48 >= 15). Option 1 compiles clean; the coder confirms by building.

### A.5 static_assert relationships (cnr3_cache_core.h ~262-305) — CR4 and the max-protected set
The existing CR-rule static_asserts (CEILING_MIN <= CEILING_MAX; CR4: CEILING_MAX >= ~2 x max-protected set)
MUST still pass under HALF. max-protected = MAX_HOT_ZONES x (BACK_RADIUS + FORWARD_RADIUS) + checkpoint_pool.

At NORMAL: 5 x (50+10) + 48 = 348 -> CR4 wants ceiling >= ~696. A HALF-500 with 5 zones VIOLATES CR4.

RESOLUTION (W3X-directed) — reduce HALF's hot-zone count so the protected set fits 500, keeping the CR4
rule and its 2x factor INTACT (preferred over relaxing the assert). Coder to evaluate, in order, and report
which actually compiles clean against the static_asserts as written:

  OPTION 1 (PRIMARY): MAX_HOT_ZONES = 3 for HALF.
     max-protected = 3 x 60 + 48 = 228 -> CR4 wants >= ~456 -> 500 FITS (500 >= 456). Everything else = NORMAL.
     This is the intended HALF-500 starting profile: NORMAL except ACTIVE_CEILING_MAX=500 AND MAX_HOT_ZONES=3.

  OPTION 2 (fallback if Option 1 static_asserts still fail for a reason not visible from the header):
     MAX_HOT_ZONES = 2 for HALF. max-protected = 2 x 60 + 48 = 168 -> CR4 wants >= ~336 -> 500 fits with margin.

  OPTION 3 (only if neither zone reduction is acceptable): keep zones and relax CR4 to a documented ~1.5x
     factor made profile-aware (1.5 x 348 = 522 > 500 -> still fails; 1.4 x 348 = 487 < 500 -> fits). If this
     route is taken the relaxed factor must be justified as headroom-not-correctness and the static_assert
     made profile-aware, not deleted.

Coder deliverable for A.5: for each option, state whether the header static_asserts (CR1-CR6) compile clean
as written, and report the derived MAX_PROTECTED_SET_ESTIMATE value each option produces (it auto-computes
from MAX_HOT_ZONES and the radii, so it will change under Options 1/2 — confirm the derived value and that
the selftest expected_max_protected_set branch for HALF matches it). DO NOT hardcode past any assert; bring
numbers to W3D. This is R-PROCESS-27 lineage: a changed proof relationship must be located and justified,
never waved through.

Behavioural note for the sweep: fewer hot zones = fewer concurrently prune-protected access regions. Linear
clips used only 1 zone (zone_count_max=1 in all logs), so 3 (or 2) is ample there; the sweep's re-churn gate
must confirm the reduction does not RAISE re-churn under the workload (it is the same D-SUM-10 re-churn gate
already specified below, now also validating the zone reduction, not just the ceiling).

### A.6 selftest expected-value branch (cnr3_cache_core_selftest.cpp ~4355)
The policy-constants selftest hardcodes expected values in a two-way `#if TINY / #else NORMAL`. Add an
`#elif defined(CNR3_CACHE_PROFILE_HALF)` branch with HALF's expected values (profile_name "half-500",
active_ceiling_max 500U, all others = NORMAL's expected numbers EXCEPT the two changed derived values: max_protected 228U,
grid_floor 15U (see A.4; expecting 348/25 would fail the 4-way). Without this branch the 4-way FAILS under HALF. This selftest is the proof that config and
expectations agree — it is the mechanism that would catch a mis-set HALF value.

---

## ITEM B — rename CNR3_EXPERIMENT_PLAN_RETRY_BIAS -> CNR3_ENABLE_PLAN_RETRY_BIAS

Now that the plan-retry mitigation is accepted (default S=50 chosen), rename the gate to reflect status.
Mechanical rename, 15 occurrences across 4 files (build_config.h, plugin_internal.h, arInitial.cpp,
vapoursynth-Cnr3.cpp). The per-knob defines (SLEEP_MS/HOLE_THRESHOLD/MAX_CAP) keep their names; only the
enabling gate macro is renamed.

Confirm-report MUST include a whole-tree grep proving:
- zero occurrences of `CNR3_EXPERIMENT_PLAN_RETRY_BIAS` remain anywhere (incl. comments, patch-notes refs);
- `CNR3_ENABLE_PLAN_RETRY_BIAS` appears at every former site (count matches);
- the `[DSUM-PLANRETRY]` block, the arInitial `#if/#else`, the counter gating, and the numThreads
  derivation are all still correctly gated under the new name.
A rename that misses one site produces a half-gated feature — worse than either state. Grep-all is mandatory.

Set the default retry sleep to the chosen production value:
```cpp
#   define CNR3_PLAN_RETRY_SLEEP_MS  50   // Fixed cross-CPU PlanRetry default; chosen at the efficiency knee of the S-sweep ladder (see PlanRetry ladder tests).
```

---

## Proof gate

1. Canonical 4-way, DEFAULT build (no HALF, no TINY, plan-retry OFF): 57/57 — byte-identical marker aside,
   behaviour unchanged. Rename alone must not shift the count.
2. R-PROCESS-19 byte-identical: default build (NORMAL profile, plan-retry OFF) S8 vs prior commit -> identical.
   Proves ITEM B rename and the three-way restructuring changed nothing when no new macro is selected.
3. Canonical 4-way, HALF build (CNR3_CACHE_PROFILE_HALF defined, plan-retry OFF): must pass, with the
   policy-constants selftest confirming HALF expected values (500 ceiling etc.). This proves ITEM A.6.
4. CR4 static_assert resolution (A.5) documented and agreed with W3D BEFORE the build is claimed to pass.
5. Rename grep-all clean (ITEM B).
6. NORMAL + PlanRetry ON smoke (CNR3_ENABLE_PLAN_RETRY_BIAS defined): compiles, runs, [DSUM-PLANRETRY]
   reports under the new macro. (Default-OFF testing cannot prove the renamed enabled-only path compiles —
   a missed rename inside gated code would survive the default build. Coder review section 7.)
7. Composition smoke (preferred): HALF + CNR3_ENABLE_PLAN_RETRY_BIAS together — proves the profile gating
   and the mitigation compose BEFORE the sweep begins (this is also the sweep's build config).
8. TINY scaffold smoke (policy-constants selftest under CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY): the
   three-way restructuring touches every profile knob's preprocessor block; this catches accidental drift
   in the TINY branch. (Coder review section 8.)

## Then — the cache-size sweep (proves 500 is genuinely as good as 1000)

Separate run activity (not a commit gate; a measurement). Build HALF profile WITH the accepted mitigation:
`CNR3_ENABLE_PLAN_RETRY_BIAS` on, `CNR3_PLAN_RETRY_SLEEP_MS = 50`. fmParallel, 3000-frame clip.

Sweep the ceiling by comparing HALF (500) against NORMAL (1000) — and, if a mid-point is wanted, a
temporary 750 HALF value. Watch, per run:
- **frames_recently_evicted_then_re_requested (D-SUM-10)** — THE decisive number. ~0 at 500 = the wavefront
  still fits, halving is free. Climbing above 0 = eviction is now cutting into needed frames (found the floor).
- frames_evicted, duplicates_seen, holes_identified, recovery_span_mean, fps.
Success = at 500, re-churn stays ~0 and duplicates/fps ~= the 1000 case -> 500 is proven as good, ship as an
option (or default). Failure = re-churn rises -> back off to the smallest ceiling holding re-churn at 0; that
is the real minimum safe cache size for end-user PCs.

## Forward (parked, recorded)

Once HALF-500 is proven under the S=50 mitigation, proceed to the RESERVATION-TABLE patch discussion (the
real fix, research scheme #1). DESIGN CONSTRAINT to carry into that scope (W3X): the reservation table,
CNR3_ENABLE_PLAN_RETRY_BIAS, and the profile gating must all COMPOSE — none may structurally prevent the
others from working, whether enabled separately or together.
