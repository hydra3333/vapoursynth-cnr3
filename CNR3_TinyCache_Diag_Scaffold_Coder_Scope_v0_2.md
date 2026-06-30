# CNR3 — Coder Build Scope: TINY-100 diagnostic cache scaffold (v0.2, DESIGNER → CODER)

**Author:** W3D (designer/reviewer)  **For:** W3C (coder)  **Coordinator:** W3X
**Branch:** dev_cache_manager  **Controlling CMS:** CMS07.15
**Baseline:** committed CMS07-W.3-combined-live-store-prune-helper, four-way 55/55.
**Status:** PROPOSAL for review — **not** an approved patch. R-PROCESS-21 (proven code stays proven).

**v0.2 supersedes v0.1.** Folds in the W3C ratify-with-amendments review. Changes from v0.1:
(1) toggle-OFF claim softened from "byte-for-byte" to **effective/behavioural** identity;
(2) prune-cadence text **corrected** — one bounded prune pass per store, hysteresis sawtooth, NOT a
loop-to-ceiling (this also corrects the acceptance check in §7);
(3) adds the **`CNR3_CACHE_PROFILE_NAME`** profile marker (its own `#if/#else` block above the constants;
selftest heading now; required D-SUM summary-header field later);
(4) constant-pinning selftest stays toggle-aware and now pins the profile name too;
(5) **visible** skip-pass required for any tiny-incompatible selftest;
(6) prominent warning: TINY-100 is a cache-policy/telemetry build, NOT a pixel-fidelity reference build.
W3C's five answers are accepted; their corrections are now folded in below (the prune-cadence one was a
genuine v0.1 error, owned and fixed).

---

## 1. Purpose (what this is and is NOT)

A compile-time toggle that, when defined, selects a **pre-computed small-but-safe** cache profile so the
live eviction machinery (capacity trigger, checkpoint-retention trigger, hot-zone retirement, bounded
prune, recovery/floor/AS2) fires on a **short** getFrame run (a few hundred frames) instead of needing
~1300. This compresses eviction behaviour into a readable window for the upcoming **diagnostics arc**
(D-SUM telemetry), and is the instrument for the eviction-**policy-health** questions the W.3 live harness
cannot answer (over-prune / thrash / hot-zone efficacy / recovery churn).

This is a **behaviour-changing scaffold**, NOT observe-only telemetry. Per the explicit rule in
`cnr3_build_config.h` ("A behaviour-changing scaffold ... must not use a CNR3_DIAG_* name"), it uses a
`CNR3_SCAFFOLD_*` name.

**TINY-100 is a cache-POLICY / TELEMETRY build, not a pixel-fidelity reference build.** Its reduced
recovery search window (BACK_RADIUS 50→15) means recovery fresh-starts are not guaranteed pixel-faithful
on long static spans (see §7 / CR2 note). Use it for trigger timing, prune cadence, hot-zone coverage,
recovery churn, and D-SUM counter behaviour. Do **NOT** byte-compare a tiny-build's recovery output against
a full-history production reference. (W3C point 7, accepted.)

---

## 2. The toggle (in `cnr3_build_config.h`, shipped OFF)

Add, grouped with the existing scaffold-convention / DIAG-gate region (after the "Temporary proof scaffold
convention" block, ~line 88), shipped commented-out:

```cpp
/*
    CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY — behaviour-changing diagnostic scaffold.

    NOT a CNR3_DIAG_* gate: it changes eviction TIMING (observe-affecting), so it must not wear a
    DIAG name. When defined, cnr3_cache_core.h selects a pre-computed small-but-safe "TINY-100" cache
    profile so eviction fires on short getFrame runs. The cache-core static_asserts re-prove the safety
    invariants against the tiny profile, so a tiny build that COMPILES is, by construction, a safe cache.

    OFF for production / for the committed four-way selftest gate. Uncomment ONLY for a diagnostic build.
    Owning context: diagnostics arc (D-SUM) enablement. Not required for production correctness.
*/
// #define CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY 1
```

**Include-order check (W3C point 1).** `cnr3_cache_core.h` already `#include "cnr3_build_config.h"` (line 9),
so the macro is visible where the constants are defined. Coder to confirm the same holds in every
translation unit that defines or pins these constants (notably the selftest TU), so no TU sees the
constants without first seeing the toggle.

---

## 3. The TINY-100 profile (in `cnr3_cache_core.h`)

### 3.1 Profile marker (new in v0.2) — placed ABOVE the constants

Immediately after the includes / before the `BYTE_BUDGET` block (~line 68), as the banner for the whole
cache-sizing region — its **own** `#if/#else` block with its own comment, in the same per-constant style:

```cpp
/*
    Profile identity marker. Keyed off CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY — the SAME single macro that
    gates every cache constant below — so it can never report a profile that was not compiled (no drift:
    there is exactly one truth, the macro). Emitted in the cache-core selftest heading, and REQUIRED as a
    field of the D-SUM summary header (diagnostics arc). Human/diagnostic identification only; MUST NOT be
    used for control flow.
*/
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr const char* CNR3_CACHE_PROFILE_NAME = "tiny-100";
#else
inline constexpr const char* CNR3_CACHE_PROFILE_NAME = "normal";
#endif
```

### 3.2 The nine independent knobs

Wrap **only the 9 independent knobs** in `#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY) / #else /
#endif`, **each in its own block, preserving its existing explanatory comment** (the CR2–CR5 rationale,
the "raised 32→48" note, the decay-margin bound, etc. — keep each constant WITH its comment; do not group).

**Do NOT wrap** the derived constants `CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS` (= BACK_RADIUS),
`CNR3_CACHE_JUMP_THRESHOLD` (= FWD+BACK+1), `CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE`,
`CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE` — they reference the primaries and recompute automatically.
`CNR3_CACHE_BYTE_BUDGET_BYTES`, the overflow factor (11/10), and `CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS` (8)
are **unchanged** (no wrap).

| Constant | line | NORMAL | TINY-100 |
|---|---|---|---|
| `CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES` | 79 | 150U | **40U** |
| `CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES` | 80 | 1000U | **100U** |
| `CNR3_CACHE_CHECKPOINT_INTERVAL` | 101 | 10 | **3** |
| `CNR3_CACHE_CHECKPOINT_MIN_RETAIN` | 102 | 10U | **4U** |
| `CNR3_CACHE_CHECKPOINT_MAX_RETAIN` | 103 | 48U | **12U** |
| `CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS` | 118 | 10 | **3** |
| `CNR3_CACHE_HOT_ZONE_BACK_RADIUS` | 119 | 50 | **15** |
| `CNR3_CACHE_MAX_HOT_ZONES` | 127 | 5U | **2U** |
| `CNR3_CACHE_HOT_ZONE_DECAY_MARGIN` | 142 | 20 | **6** |

Per-constant pattern (example — ceiling MAX, line 80):

```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 100U;   // TINY-100 diag profile
#else
inline constexpr std::size_t CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES = 1000U;  // NORMAL (production)
#endif
```

**Derived values under TINY-100 (auto):** BACK==15 (recovery window B=15; a hot zone spans `[N-15, N+3]`);
JUMP_THRESHOLD = 3+15+1 = **19**; MAX_PROTECTED_SET_ESTIMATE = 2*(15+3)+12 = **48**; GRID_FLOOR = 2*(15/3) =
**10**. **`BACK_RADIUS == 5 * CHECKPOINT_INTERVAL`** is preserved (15 = 5×3) — the lock-step assert; INTERVAL
and BACK move together.

**Designer-verified assert pass (whole chain, computed against TINY-100):** CEILMIN≤CEILMAX 40≤100 ✓;
BACK==5*INTERVAL 15==15 ✓; CMINR≤CMAXR 4≤12 ✓; CMAXR≥GRID_FLOOR 12≥10 ✓; CEILMAX≥2*PROTECTED 100≥96 ✓
(4-frame headroom); FWD≤DECAY≤BACK 3≤6≤15 ✓; victims>0 8 ✓. A tiny build that compiles re-proves all of
this; the `static_assert` block (~165–211) is **unchanged**.

**Eviction behaviour under TINY-100:** capacity prune fires at `slot_count > ceiling*11/10` = **>110**;
checkpoint-retention trigger fires when flagged count exceeds CMAXR=**12** (≈ every 36 frames at INTERVAL=3).
A **~200–300 frame** run crosses both triggers repeatedly. Two hot zones (Z=2) preserve multi-zone
diagnostics (second-zone formation on a jump, zone retirement).

---

## 4. Toggle-OFF identity claim (CORRECTED per W3C point 2)

Do **not** claim binary byte-for-byte identity for an OFF build — adding `#if/#else` branches shifts line
numbers, debug info, and PDBs, so the binary need not be literally identical even when production logic is
unchanged. The correct, provable claim is **effective/behavioural** identity:

> With `CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY` undefined, the production constants and the executable
> cache behaviour are unchanged. The normal four-way cache-core selftest remains **55/55** (forced-fail
> **54/55** exit 1; verbose 55/55), and the normal live W.3 behaviour remains governed by the committed
> production constants.

---

## 5. The constant-pinning selftest — TOGGLE-AWARE (W3C point 5, accepted)

`cnr3_cache_core_selftest.cpp` ~lines 4043–4135 is a constant-pinning guard returning `invariant_violation`
unless each constant equals its exact production value. Make it profile-aware so it pins the **correct**
profile in each build (guard keeps working in both modes):

```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
    if (CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES  != 100U) return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_HOT_ZONE_BACK_RADIUS       != 15)   return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_MAX_HOT_ZONES              != 2U)   return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_JUMP_THRESHOLD             != 19)   return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE != 48U)  return Cnr3Status::invariant_violation;
    if (std::string_view(CNR3_CACHE_PROFILE_NAME) != "tiny-100")
                                                       return Cnr3Status::invariant_violation;
    /* ...the remaining pinned constants at their TINY-100 values (see §3.2 table + derived)... */
#else
    /* the existing production pins, UNCHANGED: ceiling 1000U, BACK 50, MAX_HOT_ZONES 5U,
       JUMP_THRESHOLD 61, MAX_PROTECTED_SET_ESTIMATE 348U, etc. */
    if (std::string_view(CNR3_CACHE_PROFILE_NAME) != "normal")
                                                       return Cnr3Status::invariant_violation;
#endif
```

The profile-AGNOSTIC checks in that test (`JUMP == FWD+BACK+1`, `BACK == 5*INTERVAL`,
`ceiling >= 2*protected`, overflow num>den) stay as-is — they hold for both profiles.

---

## 6. Other ceiling-sensitive selftests — audit + classify (CODER-OWNED; W3C points 4 & 6)

~30 references to cache-sizing constants exist across `cnr3_cache_core_selftest.cpp`. Read the full surface
and, per ceiling-sensitive test, choose, using W3C's classification:

```text
Profile-aware (re-derive expectation at TINY-100 cadence, preferred where cheap):
  - constant-pinning tests
  - prune trigger / hysteresis tests
  - checkpoint-retention trigger tests
  - hot-zone capacity / retirement tests
  - W.3 store-prune tests where expectations can be derived

Visible skip in tiny build (when re-derivation is disproportionate, or it compares computed-frame content
that the tiny recovery cadence would confuse):
  - tests whose purpose is specifically production-cadence golden behaviour
  - tests comparing computed frame content over recovery spans
```

**MANDATORY for any gated-off test (report-actual discipline):** the skip must be **VISIBLE** — emit a
`SKIPPED under CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY: <test name>` line (or an explicit skip-pass that
keeps the count honest). A tiny build's summary must never look like a test silently vanished. A changed
selftest count in the tiny build is expected and fine **as long as it is visibly reported**. Gate with
`#if !defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)` around the call (not silent removal).

---

## 7. Prune cadence + CR2 — the two correctness footnotes

### 7.1 Prune cadence (CORRECTED per W3C point 3 — this was a v0.1 error)

The combined helper performs **one bounded prune pass per store** (coder to confirm against source; if it
loops-to-target instead, flag it). Therefore at TINY-100:

```text
active ceiling:        100
overflow trigger:      slot_count > 110
a store reaches:       111
bounded prune removes: up to V=8 victims  -> ~103
result:                103 is BELOW the 110 trigger, so NO further prune this store
```

The tiny cache therefore exhibits a **hysteresis sawtooth**: it may legitimately sit **between 101 and 110**
(above the active ceiling, below the overflow trigger) until a later store crosses 110 again. **This is
expected design behaviour, not a failure** — it is the same bounded-prune + hysteresis the production
profile uses, just at a small scale. (Note: returning under 100 in one pass is impossible by V=8 alone when
a single store crosses; over multiple stores the sawtooth holds the working set near the ceiling.)

### 7.2 CR2 / settling length (W3C point 7, accepted with prominent warning)

"Effective settling length" is **prose only** — never a variable/constant/computation in the source (it
appears solely in comments in `cnr3_cache_core.h` and as open item FI-02). BACK_RADIUS=15 shrinks the
recovery **search window** (a policy dial), not the blend's physical settling length. CR2's margin is
therefore unverified at the reduced B — affecting **only recovery output pixel fidelity** on long static
spans, never safety, ownership, pin balance, or the diagnostic counters. **Acceptable for an observe-only
diagnostic build; do NOT byte-compare a tiny-build recovery output against a full-history reference.**

---

## 8. Proof obligations

- **Toggle OFF (production):** four-way selftest **55/55** (forced-fail 54/55 exit 1; verbose 55/55); the
  toggle-aware pinning test pins the production values **and** `CNR3_CACHE_PROFILE_NAME == "normal"`;
  effective cache behaviour unchanged (§4 — no binary-identity claim).
- **Toggle ON (tiny diag build):** (a) compiles (re-proving the static_assert chain against TINY-100);
  (b) the pinning test passes against the tiny values and `CNR3_CACHE_PROFILE_NAME == "tiny-100"`;
  (c) the selftest run completes with any ceiling-sensitive tests either re-derived or **visibly** skipped,
  and the heading reports the profile name; (d) a short live `.vpy` run (~200–300 frames) shows the
  capacity AND checkpoint triggers firing **with detached victims**.
- **Acceptance check shape (per §7.1):** the diagnostic acceptance asserts **trigger-fired + victims-detached
  (non-vacuity)**, and the profile marker == "tiny-100". It must **NOT** require the slot count to return
  below the active ceiling of 100 — sitting at 101–110 between stores is expected hysteresis, not a failure.

---

## 9. Housekeeping

- Scaffold-enablement scope; changes proven cache-core constants behind a compile guard — R-PROCESS-21
  (propose→review→selftest). It changes NO CMS design (production values and all invariants unchanged when
  OFF); no CMS version bump implied. Escalate per §0A charter if review surfaces a design question.
- When committed, add a line to the DELTA owed-items ledger: this scaffold is the first concrete
  diagnostics-arc enablement step (next: the D-SUM family menu → core-subset choice → DIAG.1).
- Suggested commit subject on approval:
  `CNR3-DIAG: add CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY (TINY-100 profile) + profile marker`.

*End of TinyCache diag-scaffold coder scope v0.2.*
