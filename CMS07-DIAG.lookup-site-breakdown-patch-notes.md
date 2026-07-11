# CMS07-DIAG.lookup-site-breakdown — patch delivery note

## Status

Patch produced against the uploaded `src(25).zip` source snapshot.

Baseline verified from uploaded source:

```text
src/cnr3_build_config.h: edit_version = CMS07-DIAG.intent-counted-lookups
```

Patch success marker:

```text
CMS07-DIAG.lookup-site-breakdown
```

Track: analysis-arc diagnostic change, observe-only.

No cache lookup/pin/store/recovery semantics, no ownership rules, no pixel-path behaviour, and no selftest pass/fail wiring are intentionally changed.

## Files changed

```text
src/cnr3_arAllFramesReady.cpp
src/cnr3_arInitial.cpp
src/cnr3_build_config.h
src/cnr3_cache_core.cpp
src/cnr3_cache_core.h
src/cnr3_cache_diagnostics.cpp
src/cnr3_cache_diagnostics.h
```

Patch stat:

```text
 src/cnr3_arAllFramesReady.cpp  |  26 +++-
 src/cnr3_arInitial.cpp         |   9 +-
 src/cnr3_build_config.h        |   2 +-
 src/cnr3_cache_core.cpp        | 201 +++++++++++++++++++++++---
 src/cnr3_cache_core.h          |  55 +++++--
 src/cnr3_cache_diagnostics.cpp | 318 ++++++++++++++++++++++++++++++++++++++++-
 src/cnr3_cache_diagnostics.h   |  43 ++++++
 7 files changed, 615 insertions(+), 39 deletions(-)
```

SHA-256:

```text
f61a1cc3b2ca9db90da6430b850660c365e53d4eb03734c17e6cdb0c5afeda55  CMS07-DIAG.lookup-site-breakdown.patch
```

## What this patch adds

D-SUM-04 now prints a human-readable per-site lookup breakdown beneath the existing merged lookup rows.

The merged rows remain:

```text
cache_lookup_queries_total
cache_lookup_hits
cache_lookup_misses
```

The new breakdown rows are:

```text
site1_requested_frame_check
site2_predecessor_fastpath
site3_recovery_walk
site4_hole_catalogue_scan
site5_anchor_repin
site6_reacquire_already_pinned
site7a_floor_adopt_bail_early
site7b_hole_adopt_bail_early
site8a_plain_store_duplicate_check
site8b_as2_store_duplicate_check
site9_duplicate_winner_reacquire
```

Each row prints:

```text
invocations=<n> looks=<n> hits=<n> misses=<n>
```

Each row is followed by a plain-English purpose line in the D-SUM-04 runtime output. The block begins with a plain-English legend and ends with a print-only self-check line.

## Gating boundary

Per designer v3 Ruling 5, the new parameters are plain/defaulted and ungated, but all diagnostic work is gated.

Ungated/defaulted parameter surface:

```text
src/cnr3_cache_core.h: Cnr3LookupSite enum
src/cnr3_cache_core.h: Cnr3LookupSite lookup_site defaulted parameters
src/cnr3_cache_core.h: bool observe_lookup_site_breakdown defaulted parameters
```

Gated by `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`:

```text
src/cnr3_cache_diagnostics.h: Cnr3CacheLookupSiteDiagnosticStats
src/cnr3_cache_diagnostics.h: per-site fields in Cnr3CacheOwnershipDiagnosticStats
src/cnr3_cache_diagnostics.h: per-site observer helpers
src/cnr3_cache_core.h/cpp: lookup_site_stats_locked()
src/cnr3_cache_core.h/cpp: observe_lookup_site_*_locked()
src/cnr3_cache_core.cpp: per-site bumps in primitives, walk, scan, and store functions
```

Gated by existing D-SUM-04 print path:

```text
src/cnr3_cache_diagnostics.cpp: lookup-site legend
src/cnr3_cache_diagnostics.cpp: 11 per-site rows
src/cnr3_cache_diagnostics.cpp: plain-English purpose lines
src/cnr3_cache_diagnostics.cpp: breakdown self-check OK / MISMATCH line
```

Gate-off syntax validation was performed by commenting out `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE` in a temporary copy and compiling the touched translation units with fake VapourSynth headers. The diagnostic work compiled out cleanly.

## Exact insertion / opt-in locations

### src/cnr3_build_config.h

- line 35: `edit_version` updated to `CMS07-DIAG.lookup-site-breakdown`.

### src/cnr3_cache_core.h

- line 186: added `enum class Cnr3LookupSite`.
- lines 1179-1184: added defaulted `observe_lookup_site_breakdown` to `plan_bounded_recovery_search()`.
- lines 1200-1206: added defaulted `observe_lookup_site_breakdown` to `plan_bounded_recovery_search_and_record_anchor_pin()`.
- lines 1247-1252: added defaulted `Cnr3LookupSite lookup_site` to public `lookup_frame_and_add_ref()`.
- lines 1271-1275: added defaulted `Cnr3LookupSite lookup_site` to public `lookup_frame_and_record_pin()`.
- lines 1418-1421: added gated per-site observer declarations.
- lines 1655-1660: added defaulted `observe_lookup_site_breakdown` to locked recovery search.
- lines 1669-1675: added defaulted `observe_lookup_site_breakdown` to locked recovery search + anchor pin.
- lines 1711-1716: added defaulted `Cnr3LookupSite lookup_site` to locked add-ref lookup.
- lines 1729-1734: added defaulted `Cnr3LookupSite lookup_site` to locked lookup-pin-record.
- lines 1746-1750: added defaulted `Cnr3LookupSite lookup_site` to `pin_frame_locked()`.
- lines 1817-1820: added gated `lookup_site_stats_locked()` declaration.

### src/cnr3_cache_diagnostics.h

- line 74: added `Cnr3CacheLookupSiteDiagnosticStats`.
- lines 89-99: added named per-site fields to `Cnr3CacheOwnershipDiagnosticStats`.
- lines 391-414: added gated per-site inline observer helpers.

### src/cnr3_cache_core.cpp

- line 56: added gated `cnr3_lookup_site_is_specified()` helper.
- lines 1500-1536: threaded `observe_lookup_site_breakdown` through public recovery search.
- lines 1539-1583: threaded `observe_lookup_site_breakdown` through public recovery search + anchor pin.
- lines 1631-1649: threaded `lookup_site` through public add-ref lookup.
- lines 1698-1714: threaded `lookup_site` through public lookup-pin-record.
- lines 1975-2005: added gated enum-to-field mapper.
- lines 2007-2052: added gated per-site locked observer helpers.
- lines 2840-2853: added site8a plain-store invocation/look/hit observation. Invocations happen only when `duplicate_count_policy == hit_only`, before the duplicate-detect result is interpreted; looks/hits happen only on found duplicate.
- lines 2978-2994: added site8b AS2-store invocation/look/hit observation. Invocations happen only when `duplicate_count_policy == hit_only`; nested 10a -> 10b remains `none`, so a live AS2 duplicate is attributed to site8b only.
- lines 3732-3850: added live-route-gated site3 recovery-walk and site4 hole-catalogue observations. No loop bounds, control flow, early returns, or ordering changed.
- lines 3879-3903: added live-route-gated site5 anchor-repin tag.
- lines 3978-4041: added primitive site invocation/look/miss/hit observations to `lookup_frame_and_add_ref_locked()`.
- lines 4076-4081: threaded `lookup_site` through locked lookup-pin-record to `pin_frame_locked()`.
- lines 4112-4163: added primitive site invocation/look/miss/hit observations to `pin_frame_locked()`.

### src/cnr3_arInitial.cpp

- line 623: live recovery route passes `observe_lookup_site_breakdown = true`.
- line 918: site1 requested-frame lookup tagged `requested_frame`.
- line 973: site2 predecessor-fastpath lookup tagged `predecessor_fastpath`.

### src/cnr3_arAllFramesReady.cpp

- line 1093: site9 duplicate-winner re-acquire tagged `duplicate_winner_reacquire`.
- line 1198: site6 cache-hit return re-acquire tagged `reacquire_already_pinned`.
- line 1359: site6 predecessor re-acquire tagged `reacquire_already_pinned`.
- line 1772: site7a floor adopt tagged `floor_adopt`.
- line 2047: site7b hole adopt tagged `hole_adopt`.
- line 2081: site6 hole predecessor re-acquire tagged `reacquire_already_pinned`.
- line 2393: site6 target predecessor re-acquire tagged `reacquire_already_pinned`.

### src/cnr3_cache_diagnostics.cpp

- lines 270-380: added gated D-SUM-04 formatting helpers for derived misses, per-site rows, and self-check deltas.
- lines 439-463: added lookup-site legend.
- lines 473-547: added 11 per-site rows and plain-English purpose lines.
- lines 552-565: added per-site sum accumulation.
- lines 567-617: added print-only OK / MISMATCH self-check.

## Caller-map / anomaly sweep

### Recovery planner route map

`plan_bounded_recovery_search_and_record_anchor_pin()`:

```text
Live caller:
  cnr3_arInitial.cpp:623 -> observe_lookup_site_breakdown=true

Selftest callers:
  cnr3_cache_core_selftest.cpp:2047  -> default false
  cnr3_cache_core_selftest.cpp:9264  -> default false
  cnr3_cache_core_selftest.cpp:9351  -> default false
  cnr3_cache_core_selftest.cpp:9389  -> default false
  cnr3_cache_core_selftest.cpp:9521  -> default false
  cnr3_cache_core_selftest.cpp:10497 -> default false
```

`plan_bounded_recovery_search()` standalone callers:

```text
Selftest-only:
  cnr3_cache_core_selftest.cpp:2723 -> default false
  cnr3_cache_core_selftest.cpp:8887 -> default false
  cnr3_cache_core_selftest.cpp:9878 -> default false
```

Result: site3 and site4 are observed only on the live arInitial recovery route. Selftest planner routes do not bump per-site rows.

### lookup_frame_and_record_pin() route map

```text
Live:
  cnr3_arInitial.cpp:914         -> hit_only, requested_frame
  cnr3_arInitial.cpp:969         -> full, predecessor_fastpath
  cnr3_arAllFramesReady.cpp:1768 -> hit_only, floor_adopt
  cnr3_arAllFramesReady.cpp:2043 -> hit_only, hole_adopt

Selftest-only:
  31 cnr3_cache_core_selftest.cpp call/trait sites -> default unspecified unless already supplying old count_policy; no per-site tag supplied.
```

Result: only sites 1, 2, 7a, and 7b opt in through the public primitive. All selftest callers remain per-site `unspecified` and do not contribute to the new breakdown.

### lookup_frame_and_add_ref() route map

```text
Live:
  cnr3_arAllFramesReady.cpp:1088 -> none, duplicate_winner_reacquire
  cnr3_arAllFramesReady.cpp:1193 -> none, reacquire_already_pinned
  cnr3_arAllFramesReady.cpp:1354 -> none, reacquire_already_pinned
  cnr3_arAllFramesReady.cpp:2076 -> none, reacquire_already_pinned
  cnr3_arAllFramesReady.cpp:2388 -> none, reacquire_already_pinned

Selftest-only:
  23 cnr3_cache_core_selftest.cpp call sites -> default unspecified unless already supplying old count_policy; no per-site tag supplied.
```

Result: site9 reaches only arAllFramesReady:1088. The four site6 call sites carry `reacquire_already_pinned`, not `duplicate_winner_reacquire`.

### store duplicate route map

`store_owned_frame_locked()` / 10b:

```text
Live:
  cnr3_cache_core.cpp:1147 -> hit_only -> site8a plain_store_duplicate

Nested AS2:
  cnr3_cache_core.cpp:3018 -> none -> no site8a invocation, prevents 10a->10b double-count

Selftest/public locked wrappers:
  cnr3_cache_core.cpp:2811 -> none
  cnr3_cache_core.cpp:2818 -> none
```

`store_owned_frame_and_record_pin_locked()` / 10a:

```text
Live:
  cnr3_cache_core.cpp:1102 -> hit_only -> site8b as2_store_duplicate

Selftest/public wrapper route:
  cnr3_cache_core.cpp:825 -> inherited caller policy, default none from public wrapper
  cnr3_cache_core_selftest.cpp:1094,1119,1175,1285,1336,1682,10141,10178,10216,10239 -> default none

Repair delegate route:
  cnr3_cache_core.cpp:877 -> public wrapper default none
```

Result: live plain-store duplicates are attributed to site8a, live AS2 duplicates to site8b, nested 10a->10b is silent, and selftest/public routes stay out of production counters.

### Primitive default sweep

The two lookup/pin public primitives, their locked forms, and `pin_frame_locked()` now have defaulted `Cnr3LookupSite::unspecified`. The only live explicit tags are listed above. Unspecified routes may continue to exercise cache logic or the old count policy, but they do not bump the new per-site breakdown fields.

Sweep result: clean. No extra live caller was found with an unintended `unspecified` tag, and no selftest-only route was found with a production site tag.

## Zero-proof rows

The three zero-proof families have no `_counted` bump path:

```text
site4_hole_catalogue_scan:
  invocations only, inside live-route-gated hole-catalogue loop.

site5_anchor_repin:
  invocation only through lookup_frame_and_record_pin_locked(..., none, anchor_repin).

site6_reacquire_already_pinned:
  invocation only through lookup_frame_and_add_ref(..., none, reacquire_already_pinned).

site9_duplicate_winner_reacquire:
  invocation only through lookup_frame_and_add_ref(..., none, duplicate_winner_reacquire).
```

`looks_counted`, `hits_counted`, and `misses_counted` are only bumped beside the existing shared `observe_cache_lookup_query_locked()` / `observe_cache_lookup_hit_locked()` calls, or on the full-miss path beside the derived-miss source. Since these zero-proof rows always use `Cnr3LookupCountPolicy::none`, there is no path to bump their counted fields.

## Whole-diff deletion enumeration

Only replacement/removal classes in the diff:

```text
src/cnr3_build_config.h:
  - old edit_version marker replaced.

src/cnr3_cache_core.h/cpp:
  - function signatures replaced to add defaulted parameters.
  - internal forwarding calls replaced to pass through the new parameters.
  - no control-flow statement, loop bound, return condition, ownership operation, store operation, or recovery decision was removed.

src/cnr3_cache_diagnostics.cpp:
  - previous inline derived-misses expression replaced by the local cache_lookup_misses variable.
  - no D-SUM-04 existing row name, counter name, or gate removed.

src/cnr3_arInitial.cpp / src/cnr3_arAllFramesReady.cpp:
  - call argument lists expanded with site tags / live-route flag.
  - no branch condition, return path, source-frame request, frame retrieval, store, or ownership operation removed.
```

R-PROCESS-25 statement: proven walk/store functions received counting statements and defaulted observation parameters only. No loop bounds, early returns, control-flow ordering, ownership transfer, pin/store semantics, or cache mutation semantics were intentionally changed.

## Sandbox validation

Patch apply validation:

```text
git apply --check CMS07-DIAG.lookup-site-breakdown.patch                         PASS
git apply --check --whitespace=error CMS07-DIAG.lookup-site-breakdown.patch      PASS
git apply CMS07-DIAG.lookup-site-breakdown.patch                                 PASS
git diff --check                                                                 PASS
```

Expected `git status --short` after apply:

```text
 M src/cnr3_arAllFramesReady.cpp
 M src/cnr3_arInitial.cpp
 M src/cnr3_build_config.h
 M src/cnr3_cache_core.cpp
 M src/cnr3_cache_core.h
 M src/cnr3_cache_diagnostics.cpp
 M src/cnr3_cache_diagnostics.h
```

Limited syntax validation with fake VapourSynth headers:

```text
D-SUM-04 gate ON:
  cnr3_cache_core.cpp            PASS
  cnr3_cache_diagnostics.cpp     PASS
  cnr3_diagnostics.cpp           PASS
  cnr3_owned_frame_ref.cpp       PASS
  cnr3_arInitial.cpp             PASS
  cnr3_arAllFramesReady.cpp      PASS

D-SUM-04 gate OFF in a temporary copy:
  cnr3_cache_core.cpp            PASS
  cnr3_cache_diagnostics.cpp     PASS
  cnr3_diagnostics.cpp           PASS
  cnr3_owned_frame_ref.cpp       PASS
  cnr3_arInitial.cpp             PASS
  cnr3_arAllFramesReady.cpp      PASS
```

This is not a substitute for VS2026. It is only a sandbox syntax and gating sanity check.

## Coordinator proof gate after designer review

Expected after applying and building in VS2026:

```text
Canonical 4-way: 56/56 unchanged
Forced-fail harness: 55/56, first_failed_status invariant_violation, exit 1
R-PROCESS-19 macro-off byte-identical: S8 A/B, fc /b identical
D-SUM-04 breakdown self-check: OK on L1, L2, and ON build byte-identical run
L1 oracle: site sums 7279 / 7279 / 0, self-check OK
L2 oracle: site sums 12109 / 7279 / 4830, self-check OK
D-SUM-12 per-frame branch counters unchanged vs committed L1/L2
```
