# CNR3 — Coder Build Scope: TINY-100 diagnostic cache scaffold (v0.1, DESIGNER → CODER)

**Author:** W3D (designer/reviewer)  **For:** W3C (coder)  **Coordinator:** W3X
**Branch:** dev_cache_manager  **Controlling CMS:** CMS07.15
**Baseline:** committed CMS07-W.3-combined-live-store-prune-helper, four-way 55/55.
**Status:** PROPOSAL for review — **not** an approved patch. Per the project's propose→review→approve
discipline (and R-PROCESS-21, proven code stays proven), this scope asks the coder to **ratify or
challenge the scheme from the feasibility/lifecycle lens BEFORE implementing.** See §6 (coder verify mandate).

---

## 1. Purpose (what this is and is NOT)

A compile-time toggle that, when defined, selects a **pre-computed small-but-safe** cache profile so the
live eviction machinery (capacity trigger, checkpoint-retention trigger, hot-zone retirement, bounded
prune, recovery/floor/AS2) fires on a **short** getFrame run (a few hundred frames) instead of needing
~1300. This compresses eviction behaviour into a readable window for the upcoming **diagnostics arc**
(D-SUM telemetry), and is the instrument for the eviction-**policy-health** questions the W.3 live harness
cannot answer (over-prune / thrash / hot-zone efficacy / recovery churn).

This is a **behaviour-changing scaffold**, NOT observe-only telemetry. Therefore, per the explicit rule in
`cnr3_build_config.h` ("A behaviour-changing scaffold ... must not use a CNR3_DIAG_* name"), it uses a
`CNR3_SCAFFOLD_*` name, not `CNR3_DIAG_*`.

It does **not** change production behaviour: shipped OFF (commented out), a normal build is byte-for-byte
the current build. The four-way selftest in a normal (toggle-off) build must remain 55/55.

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

---

## 3. The TINY-100 profile (in `cnr3_cache_core.h`)

Wrap **only the 9 independent knobs** in `#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY) / #else /
#endif`. **Do NOT wrap** the derived constants `CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS` (= BACK_RADIUS),
`CNR3_CACHE_JUMP_THRESHOLD` (= FWD+BACK+1), `CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE`, or
`CNR3_CACHE_CHECKPOINT_GRID_FLOOR_ESTIMATE` — they reference the primaries and recompute automatically.
`CNR3_CACHE_BYTE_BUDGET_BYTES`, the overflow factor (11/10), and `CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS`
(8) are **unchanged** (no wrap).

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

**Derived values under TINY-100 (auto, for reference):** BACK==15 (so recovery window B=15 and a hot zone
spans `[N-15, N+3]`); JUMP_THRESHOLD = 3+15+1 = **19**; MAX_PROTECTED_SET_ESTIMATE = 2*(15+3)+12 = **48**;
GRID_FLOOR = 2*(15/3) = **10**. **`BACK_RADIUS == 5 * CHECKPOINT_INTERVAL`** is preserved (15 = 5×3) — this
is the lock-step assert; INTERVAL=3 and BACK=15 must move together.

**Designer-verified assert pass (the whole chain, computed against TINY-100):**
- `CEILMIN <= CEILMAX`: 40 ≤ 100 ✓
- `BACK == 5*INTERVAL`: 15 == 15 ✓
- `CMINR <= CMAXR`: 4 ≤ 12 ✓
- `CMAXR >= GRID_FLOOR`: 12 ≥ 10 ✓
- `CEILMAX >= 2*PROTECTED`: 100 ≥ 96 ✓ (4-frame headroom)
- `FWD <= DECAY <= BACK`: 3 ≤ 6 ≤ 15 ✓
- victims > 0: 8 ✓
A tiny build that compiles re-proves all of the above; if any value here is wrong, the tiny build fails to
compile (it cannot silently produce an unsafe cache). The `static_assert` block (~165–211) is **unchanged**.

**Eviction behaviour under TINY-100:** capacity prune fires at `slot_count > ceiling*11/10` = **>110**; the
checkpoint-retention trigger fires when flagged count exceeds CMAXR=**12** (≈ every 36 frames at INTERVAL=3).
So a **~200–300 frame** run crosses both triggers repeatedly. Two hot zones (Z=2) preserve multi-zone
diagnostics (second-zone formation on a jump, zone retirement).

---

## 4. The constant-pinning selftest — make it TOGGLE-AWARE (preferred), not gated-off

`cnr3_cache_core_selftest.cpp` ~lines 4043–4135 is a constant-pinning guard: it returns
`invariant_violation` unless each cache constant equals its exact production value (ceiling==1000U,
BACK==50, MAX_HOT_ZONES==5U, JUMP_THRESHOLD==61, MAX_PROTECTED_SET_ESTIMATE==348U, etc.). Left as-is, a tiny
build would FAIL this test (turning 55/55 into a failing run).

Make it profile-aware so it pins the **correct** profile in each build (keeps the guard doing its job in
both modes — proving production constants when OFF, proving the tiny set when ON):

```cpp
#if defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)
    if (CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES != 100U)        return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_HOT_ZONE_BACK_RADIUS      != 15)          return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_MAX_HOT_ZONES             != 2U)          return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_JUMP_THRESHOLD            != 19)          return Cnr3Status::invariant_violation;
    if (CNR3_CACHE_MAX_PROTECTED_SET_ESTIMATE != 48U)        return Cnr3Status::invariant_violation;
    /* ...the remaining pinned constants at their TINY-100 values (see §3 table + derived)... */
#else
    /* the existing production pins, UNCHANGED: ceiling 1000U, BACK 50, MAX_HOT_ZONES 5U,
       JUMP_THRESHOLD 61, MAX_PROTECTED_SET_ESTIMATE 348U, etc. */
#endif
```

The profile-AGNOSTIC checks in that same test (e.g. `JUMP == FWD+BACK+1`, `BACK == 5*INTERVAL`,
`ceiling >= 2*protected`, overflow num>den) stay as-is — they hold for both profiles.

---

## 5. Other ceiling/1100-dependent selftests — audit + classify (CODER-OWNED)

There are ~30 references to cache-sizing constants across `cnr3_cache_core_selftest.cpp`. Most are likely
incidental (frame IDs such as `1000+i`), but any test that **drives the cache to its ceiling and asserts
the result** (e.g. fills past 1100 and expects a prune; references near lines 7864–7886) may carry golden
expectations tied to the production ceiling. The designer cannot classify these blind. **Coder task:** read
the full selftest surface and, per ceiling-sensitive test, choose:

- **(a) toggle-aware** — re-derive the expectation at the TINY-100 cadence (preferred where cheap), OR
- **(b) gate the call** with `#if !defined(CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY)` — acceptable when the
  test compares **computed-frame content** or has golden counts whose re-derivation is disproportionate
  (i.e. the test asserts production-cadence behaviour that isn't meaningful at tiny size).

**MANDATORY for any gated-off test (report-actual discipline):** the skip must be **VISIBLE** — emit a
`SKIPPED under CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY: <test name>` line (or an explicit skip-pass that keeps
the count honest). A tiny build's summary must never look like a test silently vanished. A changed selftest
count in the tiny build is expected and fine **as long as it is visibly reported**.

---

## 6. CODER VERIFY MANDATE (ratify or challenge — do this BEFORE implementing)

Please confirm or push back on each, from the feasibility/lifecycle/atomicity lens:

1. **Scheme soundness.** Is the compile-time `#if/#else` per-constant approach (9 primaries wrapped, derived
   left to recompute, static_assert block untouched) correct and complete? Did I miss an independent knob,
   or wrongly treat a derived one as independent?
2. **No latent ceiling assumption in the LIVE path.** Does any live code (`cnr3_arAllFramesReady.cpp`, the
   combined helper, prune selection) assume a value the 1000-era profile guaranteed — e.g. ceiling ≥ some
   constant, an off-by-one safe only at large sizes, or an index/window that could go degenerate at BACK=15
   / Z=2 / ceiling=100? The static_asserts cover the *declared* invariants; this asks about *undocumented*
   assumptions in the runtime.
3. **Prune cadence at tiny size.** Bounded prune removes ≤ V=8 victims per pass; at ceiling 100 the prune
   fires at >110, so returning under 100 may take **two** passes (110→102→94). Production has the same
   per-pass cap, so this should be existing behaviour, not new — but please confirm the per-store prune
   cadence keeps the tiny cache bounded (does the helper prune once per store, or loop to target?), and that
   "sits slightly over ceiling between stores" is the same acceptable behaviour at tiny size as at 1000.
4. **Selftest audit (your read).** The §5 classification of the ~30 constant-references is yours to make;
   flag any test that needs (b)-gating because it compares computed frames and would "get mixed up" at the
   tiny cadence.
5. **CR2 / settling-length footnote (confirm, don't fix).** "Effective settling length" is **prose only** —
   it is never a variable/constant/computation in the source (it appears solely in comments in
   `cnr3_cache_core.h` and as open item FI-02). BACK_RADIUS=15 shrinks the recovery *search window* (a
   policy dial), not the blend's physical settling length. CR2's margin is therefore unverified at the
   reduced B — but this affects only **recovery output pixel fidelity** on long static spans, never safety,
   ownership, pin balance, or the diagnostic counters. **Acceptable for an observe-only diagnostic build;
   do NOT byte-compare a tiny-build recovery output against a full-history reference.** Please confirm this
   reasoning holds from your side (i.e. nothing in the live path treats BACK_RADIUS as a correctness gate
   whose reduction would break more than fidelity).

---

## 7. Proof obligations

- **Toggle OFF (production):** four-way selftest **55/55** (forced-fail 54/55 exit 1; verbose 55/55),
  byte-identical to the current build. This proves the scaffold is a no-op for production.
- **Toggle ON (tiny diag build):** (a) compiles (re-proving the static_assert chain against TINY-100);
  (b) the pinning test passes against the tiny values; (c) selftest run completes with any ceiling-sensitive
  tests either re-derived or **visibly** skipped; (d) a short live `.vpy` run (~200–300 frames) shows the
  capacity AND checkpoint triggers firing with detached victims — i.e. the eviction the full-size cache
  needed ~1300 frames to reach.

---

## 8. Numbering / housekeeping

- This is a scaffold-enablement scope; it changes proven cache-core constants behind a compile guard, so it
  is an R-PROCESS-21 touch (propose→review→selftest). It does NOT change CMS design (the constants' production
  values and all invariants are unchanged when the toggle is off); no CMS version bump is implied by this
  scope. If the coder review surfaces a design question, escalate per §0A charter.
- Suggested commit subject on approval: `CNR3-DIAG: add CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY (TINY-100 profile)`.

*End of TinyCache diag-scaffold coder scope v0.1.*
