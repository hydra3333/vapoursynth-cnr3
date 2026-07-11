# CNR3 — DESIGNER RESPONSE to coder confirm-report: lookup-site-breakdown — RULINGS, PROCEED — v2 (adds Ruling 4: self-explanatory log output)

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Re:** your response v1 to `CNR3_Patch_Scope_LookupSiteBreakdown_v1.md`.
**Verdict:** Excellent confirm-report again — both hazards are real (I re-verified them cold: the recovery
planner has 6 selftest call sites plus exactly one live caller at arInitial:623; and the race-winner
lookup at arAllFramesReady:1088 fetches a frame this request never pinned, so the v1 site-6 label was
wrong for it). All three of your ruling requests are answered below. Proceed to a single patch on these
terms.

## Ruling 1 — LITERAL code-location rows: SPLIT everything

The coordinator's stated goal for this patch is to be "100% clear on what/where/why contributes... and
what/where/why deliberately doesn't", with locations spelled out for a reader who does not know the code's
groupings. That is literal code-location provenance, not semantic aggregation. So take your higher-clarity
options everywhere:

- **Split site 6 into its distinct meanings** (your §5.1 split, adopted):
  - `site6_reacquire_already_pinned` — re-acquiring a frame this request already found and pinned earlier
    (the cache-hit return re-acquire, predecessor re-acquire, hole-predecessor re-acquire, target-predecessor
    re-acquire: arAllFramesReady 1191, 1350, 2068, 2378). Zero-proof.
  - `site9_duplicate_winner_reacquire` — after losing a store race, fetching the OTHER activation's winning
    frame, guaranteed present by the store's `duplicate` status but never pinned by this request
    (arAllFramesReady 1088; taxonomy site 11 made visible again). Zero-proof. At `-r 1` its invocations must
    also be zero — a free extra tripwire.
- **Split site 7** (your §5.2): `site7a_floor_adopt_bail_early` (arAllFramesReady ~1762) and
  `site7b_hole_adopt_bail_early` (~2036). Both hit-only, both counted.
- **Split site 8** (your §5.2): `site8a_plain_store_duplicate_check` (store_owned_frame_locked, 10b) and
  `site8b_as2_store_duplicate_check` (store_owned_frame_and_record_pin_locked, 10a). Both hit-only, both
  counted. This also makes the 10a/10b anti-double-count rule directly visible in the output: on any run,
  a duplicate is counted at exactly one of the two rows, never both.

Final row list (11 rows; keep this naming and order in the emission):
```
site1_requested_frame_check        counted (hit-only)     arInitial ~913
site2_predecessor_fastpath         counted (full)         arInitial ~967
site3_recovery_walk                counted (mixed)        cache_core walk loop
site4_hole_catalogue_scan          zero-proof             cache_core scan loop
site5_anchor_repin                 zero-proof             cache_core anchor pin
site6_reacquire_already_pinned     zero-proof             arAllFramesReady 1191/1350/2068/2378
site7a_floor_adopt_bail_early      counted (hit-only)     arAllFramesReady ~1762
site7b_hole_adopt_bail_early       counted (hit-only)     arAllFramesReady ~2036
site8a_plain_store_duplicate_check counted (hit-only)     store_owned_frame_locked (10b)
site8b_as2_store_duplicate_check   counted (hit-only)     store_owned_frame_and_record_pin_locked (10a)
site9_duplicate_winner_reacquire   zero-proof             arAllFramesReady 1088
```
Enum names follow the rows (`requested_frame`, `predecessor_fastpath`, `recovery_walk`,
`hole_catalogue_scan`, `anchor_repin`, `reacquire_already_pinned`, `floor_adopt`, `hole_adopt`,
`plain_store_duplicate`, `as2_store_duplicate`, `duplicate_winner_reacquire`, plus `unspecified`).
Each emitted row carries a short plain-english purpose comment in the emission source, taken from scope v1
§2's wording.

## Ruling 2 — recovery walk/scan instrumentation is LIVE-ROUTE OPT-IN: yes

Confirmed. Your §4.1 amendment is adopted: a defaulted opt-in threaded through
`plan_bounded_recovery_search_and_record_anchor_pin` (public → locked → search). Use the boolean form
(`bool observe_lookup_site_breakdown = false`) — one flag, clearer than a second enum, and there is only
one live route. arInitial:623 passes true; all six selftest callers inherit false. Sites 3 and 4 bump only
when true; site 5's anchor re-pin receives its site tag only when true (otherwise `unspecified`).

## Ruling 3 — site 8a/8b invocations gated by the existing duplicate_count_policy: yes

Confirmed. Your §4.2 is adopted exactly: `duplicate_count_policy == hit_only` is the site-8 opt-in;
`none` bumps nothing, not even invocations. The nested 10a→10b call stays `none`, so a live AS2 duplicate
appears at site8b only. Selftest/public store wrappers stay `none` and print zero volume. This keeps the L1
oracle shape: site8a invocations > 0 (every live plain store checks), contributes 0/0/0 at `-r 1`.

## Consequential updates to the scope's L1 oracle (with the splits)

Exact, per the split rows (linear, `-r 1`):
```
site1  invocations 7280,  looks 0,    hits 0,    misses 0
site2  invocations 7279,  looks 7279, hits 7279, misses 0
site3/4/5/7a/7b: invocations 0 (recovery and adoption never occur on clean linear)
site6  invocations > 0,   contributes 0/0/0
site8a invocations > 0,   contributes 0/0/0   (every live plain store checks; no collisions serial)
site8b invocations 0                            (no AS2 stores without recovery)
site9  invocations 0                            (no store races serial)
selfcheck: sums 7279/7279/0 == totals -> OK
```
L2 (`-r 1`): sums must equal 12109/7279/4830, selfcheck OK; sites 5/6 invocations > 0 with 0/0/0;
sites 7a/7b/8b/9 expectations: 7a/7b invocations > 0 with hits 0 at -r 1; 8b invocations > 0 (AS2 stores
happen under recovery) with hits 0; site9 invocations 0. Then we READ site1/site2/site3 — measured, not
derived.

## Ruling 4 — the emitted log output must be self-explanatory to a non-coder (REINFORCED)

This is a first-class requirement, not a nicety. **The log output itself must be readable by someone who
does not know the code** — every row prints a plain-English purpose, and the block opens with a short legend
defining the columns. That way the artifact read by a human and/or a future maintainer after the run
explains itself, without needing the source, this scope, or the designer to interpret it.

Plain-English labels in C++ *comments* do not satisfy this — comments are not in the log. The requirement is
about the printed D-SUM-04 block a person sees at run time.

Concretely, the emission must include:

**(a) A legend block, printed once, immediately before the per-site rows**, defining each column in plain
English. For example:
```
[DSUM-SUMMARY] D-SUM-04 lookup-site breakdown legend:
[DSUM-SUMMARY] D-SUM-04   a "lookup" is the plugin asking the cache "is this frame present?"
[DSUM-SUMMARY] D-SUM-04   invocations = how many times the code reached this location and asked
[DSUM-SUMMARY] D-SUM-04   looks       = of those, how many were COUNTED toward the cache_lookup totals
[DSUM-SUMMARY] D-SUM-04   hits        = of the counted looks, how many found the frame present
[DSUM-SUMMARY] D-SUM-04   misses      = of the counted looks, how many found it absent
[DSUM-SUMMARY] D-SUM-04   "excluded from totals" = this location deliberately never counts (looks/hits/misses always 0); invocations shown only to prove it participated but was not counted
```

**(b) Every per-site row prints a one-line plain-English purpose** after the numbers — what the plugin was
doing at that location and, for counted sites, what the count means. The purpose text must be understandable
with no knowledge of the code. Worked examples of the exact shape expected in the log:
```
[DSUM-SUMMARY] D-SUM-04 site1_requested_frame_check      invocations=7280 looks=4830 hits=4830 misses=0
[DSUM-SUMMARY] D-SUM-04    -> when a frame was first requested, checked if it was already finished in the cache; counted only when found (a frame produced early by out-of-order work). 4830 were already there.
[DSUM-SUMMARY] D-SUM-04 site6_reacquire_already_pinned   invocations=7280 looks=0 hits=0 misses=0  (excluded from totals)
[DSUM-SUMMARY] D-SUM-04    -> re-fetched a frame this request had already found and pinned earlier; guaranteed present, so deliberately not counted.
[DSUM-SUMMARY] D-SUM-04 site8b_as2_store_duplicate_check invocations=1566 looks=1566 hits=1566 misses=0
[DSUM-SUMMARY] D-SUM-04    -> after computing a recovery frame, checked whether another thread had already stored it; when found, discarded our duplicate. Found 1566 times.
```
(Numbers illustrative. The coder supplies the purpose wording per the plain-English descriptions in scope v1
§2 and this document's row list; W3D reviews the wording as part of the diff.)

**(c) The self-check line also reads in plain English**, e.g.:
```
[DSUM-SUMMARY] D-SUM-04 breakdown self-check: per-site looks/hits/misses add up to the cache_lookup totals (7279/7279/0) -> OK
```
or, on failure, a loud line naming which of the three does not add up and by how much.

ASCII-only, consistent with house output rules. Verbosity here is intended and correct: this block is the
human-facing artifact this whole patch exists to produce.

## Everything else stands as scoped

Fence, self-check (print-only `OK` / `*** MISMATCH ***`, never selftest-coupled), the identical-line
placement rule for counted bumps, D-SUM-04 gate, R-PROCESS-25 handling of walk/store insertions,
`edit_version -> CMS07-DIAG.lookup-site-breakdown`, and your §7 caller-map sweep list — with one addition
to the sweep: prove site9's tag reaches ONLY the 1088 call site and that the four site6 call sites carry
`reacquire_already_pinned`, none of them `duplicate_winner_reacquire`.

Deliver the single patch with the usual R-PROCESS-25 delivery note when ready.
