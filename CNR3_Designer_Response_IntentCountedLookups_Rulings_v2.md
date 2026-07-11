# CNR3 — DESIGNER RESPONSE to coder confirm-report: intent-counted lookups — PROCEED, with rulings — v2 (10a addendum + anomaly sweep)

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Re:** your response v1 to `CNR3_Patch_Scope_IntentCountedLookups_v3.md`.
**Verdict:** Excellent confirm-report — this is exactly what confirm-before-patch is for. All three of your
required clarifications are ACCEPTED (two of them caught real hazards). One design simplification is ruled
below (§4 health rows). Proceed to a single patch on these terms.

I independently re-verified your load-bearing source claims cold before ruling: the 10a→10b nesting
(the nested call at cnr3_cache_core.cpp:2857-2858), the complete caller set of `store_owned_frame_locked`
(exactly four: 1110 direct-live, 2858 nested-in-10a, 2683/2690 selftest-only wrappers), the primitive
increment locations (3788-3803, 3886-3901), the prose lines (cache_diagnostics 301-304, diagnostics
2726-2728, 2777), and your no-selftest-assertions finding. All confirmed.

## Ruling 1 — `hit_only` semantics: ACCEPTED, your reading is the correct one

`hit_only` = **no query increment before the find; on the found path only, query+1 and hit+1 together
(before later ownership/validation returns); on miss, nothing.** You are right that "query at entry" would
have silently re-counted every suppressed miss through the derived row — that would have violated the new
rules while looking correct. The scope §2 rules table was the intent ("MISS → nothing"); your §3 reading of
my §3 wording was the hazard. Your four-line outcome table is now the normative statement:

```
full miss:      query+1, hit+0  -> derived miss+1
full hit:       query+1, hit+1  -> derived miss+0
hit_only miss:  query+0, hit+0  -> no event
hit_only hit:   query+1, hit+1  -> derived miss+0
```

One placement note to carry into the patch: for `hit_only` hits, increment at the same point the current
uniform hit++ sits (first statement of the found path), with the query++ immediately beside it — so a later
ownership failure in the found path still counts the probe as a hit, consistent with the committed
convention.

## Ruling 2 — 10a/10b nesting: ACCEPTED, your routing is right — AMENDED with a 10a addendum

Confirmed cold: 10a (`store_owned_frame_and_record_pin_locked`) calls through 10b
(`store_owned_frame_locked`) at 2857-2858, so naïve instrumentation of both would double-count every AS2
duplicate. Your routing is approved exactly as proposed:

- 10a counts query+hit once at the top of its `existing_slot_found` path (~2837);
- `store_owned_frame_locked` gains a defaulted `duplicate_count_policy = none`;
- the nested call from 10a passes `none`;
- the direct live route at 1110 passes `hit_only`;
- the two remaining 10b callers (2683/2690, selftest-only wrappers) stay defaulted `none` — agreed,
  selftest synthetic duplicates stay out of the production counters.

**ADDENDUM (cross-review, verified cold): 10a needs the SAME defaulted-policy treatment as 10b — my
original ruling was asymmetric.** 10a is also selftest-reachable, and counting unconditionally at its
found path would pull selftest synthetic duplicates into the production counters — the exact hazard the
10b ruling closed. The complete verified route map into `store_owned_frame_and_record_pin_locked`:

1. **cache_core:1066** — `store_owned_frame_and_prune_impl`, AS2 branch. The ONLY live route.
   → opts in **`hit_only`**.
2. **cache_core:790** — via the public wrapper `store_owned_frame_and_record_pin` (753). Zero live
   callers; its 10 call sites are ALL in cnr3_cache_core_selftest.cpp (1094, 1119, 1175, 1285, 1336,
   1682, 10141, 10178, 10216, 10239). → defaulted **`none`**.
3. **cache_core:841** — the repair delegate `store_recovery_plan_hole_owned_frame_and_record_pin` (802),
   which forwards to the public wrapper. Zero live callers; its 7 call sites are ALL selftest (9568,
   9604, 9698, 9953, 10536, 10591, 10668). → defaulted **`none`** (inherited via the wrapper).

So: thread `duplicate_count_policy = none` (defaulted) through 10a's chain (public wrapper → `_locked`;
the repair delegate needs no signature change — it inherits the wrapper's default), with the single
opt-in at 1066. Since routes 2 and 3 are live-dead, the default covers them with no live-code churn; the
production behaviour is set by exactly one line.

**Coder cross-check required (patch delivery):** independently re-derive BOTH complete route maps —
10b's four routes AND 10a's three routes above — from your own grep of the source, and tick each with its
policy in the delivery note. Do not transcribe the lists from this document; re-derive them, and report
any route either of us missed.

## Ruling 3 — prose/comment updates: ACCEPTED, fence explicitly extended

You are right that shipping intent counting under the old "counts both add-ref and pin lookup entry points"
prose would replace one misleading line with another. **Fence amendment:** the patch MAY update D-SUM-04 and
D-SUM-HEALTH comments, notes, and legend prose that describe the lookup-counter semantics — specifically
cache_diagnostics ~301-304 and diagnostics ~2726-2728 and ~2777, plus the note for the new misses-percent
row. Requirements: prose only (no row names, no counter names, no gating); the new wording must say
"intent-counted probes" not "both entry points"; and the exact replacement text appears in the patch for
designer review like any other line. Everything else in the §4 fence stands unchanged.

## Ruling 4 — health rows: SIMPLIFIED (supersedes scope §3's "add two rows")

Your §6.4 alias observation is right: `cache_lookup_hit_rate_percent` and a new `cache_lookup_hits_percent`
would print the same number twice, which is clutter. Ruling, matching W3X's intent ("lookups, hits, misses,
and their % of lookups"):

- **Keep `cache_lookup_hit_rate_percent` as the hits-percent row** (name unchanged, per W3X).
- **Add ONE new row: `cache_lookup_misses_percent`** (derived misses / queries, same underflow guard as the
  summary row, same gate, same disabled-marker style, "n/a" on zero queries).
- **Do NOT add `cache_lookup_hits_percent`.** Invariant to note in the row comment:
  hit_rate_percent + misses_percent == 100.000 when live.

## Accepted as recorded (no change needed)

- **D-SUM-10 untouched** (your §7): agreed and now recorded as intentional — the rechurn observer keeps
  seeing primitive not-found events that intent counting suppresses; they are different populations by
  design. Any doc statement tying `cache_lookup_misses` to D-SUM-10 traffic is retired at the doc touch.
- **Selftest impact** (your §8): matches my expectation; 4-way stays 56/56; no fixture blocker. Your grep
  finding of zero `cache_lookup_*` assertions closes scope §5.1's caveat.
- **Doc nits** (your §9): both real; they will be fixed in the commit-time doc touch (scope cite v05→v06
  census wording; findings v06 disposition pointing at scope v2→v3). Not patch blockers.

## Proceed — single patch, on these terms

Everything in scope v3 stands except as amended above (Rulings 1-4). Reminders that will be checked at
review, R-PROCESS-25 applying throughout:

1. Counting statements and defaulted parameters only; no control-flow, loop-bound, early-return, or
   ordering change anywhere — especially in the walk (insert at the 3606-3607 loop only; hole-catalogue
   loop at 3640-3641 untouched) and the two store functions.
2. Old uniform increments in the two primitives are REMOVED (replaced by policy-driven increments in the
   same functions); sites 5/6/7/9/11 never opt in.
3. `edit_version` -> `CMS07-DIAG.intent-counted-lookups`.
4. Your patch delivery must include: exact file:line of every insertion and removal; BOTH store route maps
   (10b's four, 10a's three) independently re-derived and ticked with policies; the whole-diff deletion
   enumeration; and confirmation that no proven line beyond the sanctioned set moved.
5. **Missed-anomaly sweep (new, required):** the 10a asymmetry was found only by re-walking caller maps
   after the ruling was already written — so before patching, run the same sweep over every function this
   patch touches: for EACH function gaining a policy parameter or an increment (the two lookup primitives,
   `pin_frame_locked`, the walk function, both store functions, and any wrapper you thread a parameter
   through), enumerate its complete caller set from your own grep, classify each caller live vs
   selftest-only, and confirm its intended policy. Report the full table in the delivery note, and
   explicitly flag anything that looks anomalous — a caller neither of us has classified, a wrapper with an
   unexpected extra route, a delegate that bypasses the parameter, a call site where the default would
   count something it shouldn't or silence something it shouldn't. Finding nothing is a reportable result
   ("sweep clean"); finding something stops the patch and comes back as a question.

Proof gate unchanged from scope v3 §5 (canonical 4-way 56/56; R-PROCESS-19 macro-off byte-identical; new
L1 oracle 7279/7279/0 = 100.000; the site-1 == frames_cache_hit identity; the interleaving-only zero
tripwire at -r 1; per-frame D-SUM-12 health rows unchanged).
