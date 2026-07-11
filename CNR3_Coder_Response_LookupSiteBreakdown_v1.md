# CNR3 — Coder response to `CNR3_Patch_Scope_LookupSiteBreakdown_v1.md` — confirm-before-patch assessment v1

**From:** coder (W3C), via coordinator (W3X), to designer/reviewer (W3D).  
**Re:** `CMS07-DIAG.lookup-site-breakdown` proposed D-SUM-04 per-site lookup breakdown counters.  
**Assessment baseline reconstructed:** `src(24).zip` (`CMS07-DIAG.honest-cache-hit-metrics`) + committed patch `CMS07-DIAG.intent-counted-lookups.patch` -> `CNR3_EDIT_VERSION = CMS07-DIAG.intent-counted-lookups`.

## 1. Verdict

The scope is directionally sound and implementable, and the diagnostic objective is well matched to W3X's concern: it will make the merged lookup totals legible by showing exactly which logical sites contributed to `cache_lookup_queries_total`, `cache_lookup_hits`, and derived `cache_lookup_misses`, and which sites were deliberately excluded.

I would **not patch v1 exactly as written** without two clarifications/amendments, because both can cause the printed breakdown to be misleading even though the merged counters remain correct:

1. **Inline site instrumentation must not accidentally count selftest-only routes.** This affects the recovery walk/hole-catalogue code and the store duplicate-detect code.
2. **The proposed aggregate site rows should be explicitly accepted as semantic-site rows, not exact code-location rows.** If W3X wants literal code-location clarity, several rows should be split before patching.

Neither issue is a cache semantics defect. Both are diagnostic-shape questions that should be settled before touching the proven walk/store code.

## 2. Source reconciliation summary

I rechecked the current source shape after the intent-counted patch:

- `cnr3_arInitial.cpp`
  - site 1: `lookup_frame_and_record_pin(n, ..., Cnr3LookupCountPolicy::hit_only)` at current line ~913.
  - site 2: `lookup_frame_and_record_pin(n-1, ..., Cnr3LookupCountPolicy::full)` at current line ~967.
  - recovery route: `plan_bounded_recovery_search_and_record_anchor_pin(...)` at current line ~623.
- `cnr3_cache_core.cpp`
  - current policy helpers are at file top; `hit_only` currently counts query+hit only on found path.
  - public primitives: `lookup_frame_and_add_ref(...)`, `lookup_frame_and_record_pin(...)`.
  - locked primitives: `lookup_frame_and_add_ref_locked(...)`, `lookup_frame_and_record_pin_locked(...)`, `pin_frame_locked(...)`.
  - recovery walk inline merged counting occurs in `plan_bounded_recovery_search_locked(...)` around the `candidate_frame` loop.
  - hole-catalogue scan is the separate `for (frame_number = anchor_frame + 1; ...)` loop after anchor selection.
  - anchor re-pin is `lookup_frame_and_record_pin_locked(anchor, ..., Cnr3LookupCountPolicy::none)` inside `plan_bounded_recovery_search_and_record_anchor_pin_locked(...)`.
  - store duplicate-detect 10b is `store_owned_frame_locked(...)` around the first `frame_index_.find(frame_number)`.
  - store duplicate-detect 10a is `store_owned_frame_and_record_pin_locked(...)`; it performs its own duplicate pre-check and then calls `store_owned_frame_locked(..., Cnr3LookupCountPolicy::none)` to avoid double counting.
- `cnr3_arAllFramesReady.cpp`
  - five add-ref re-acquire call sites remain default `none` after the intent-counted patch.
  - two adopt/bail-early calls use `lookup_frame_and_record_pin(..., Cnr3LookupCountPolicy::hit_only)`.
- `cnr3_cache_diagnostics.cpp`
  - D-SUM-04 currently emits merged query/hit/derived-miss rows and the corrected intent-counted prose.
- `cnr3_diagnostics.cpp`
  - health rows currently include `cache_lookup_hit_rate_percent` and `cache_lookup_misses_percent`.

## 3. Scope items that look correct

### 3.1 Per-site shape is compatible with the current counter design

Adding `{invocations, looks_counted, hits_counted, misses_counted}` per site to `Cnr3CacheOwnershipDiagnosticStats` is a good fit. It keeps the lookup breakdown inside D-SUM-04, under `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`, and avoids changing cache return, pin, store, recovery, or pixel semantics.

### 3.2 Invocation vs counted contribution is the right distinction

The proposed four-column row is the right way to make zero-proof sites visible. For example:

```text
site6_reacquire_pinned  invocations=N  looks=0  hits=0  misses=0  (excluded from totals)
```

This is much clearer than merely omitting those sites.

### 3.3 Self-check is valuable and should remain print-only

The proposed self-check should compare:

```text
sum(site looks_counted)  == cache_lookup_queries_total
sum(site hits_counted)   == cache_lookup_hits
sum(site misses_counted) == derived cache_lookup_misses
```

It should print `OK` or `*** MISMATCH ***` only. I agree it must not feed `Cnr3Status`, the selftest failure machinery, or exit codes.

### 3.4 Named fields are preferable to an array

For this phase, named fields are safer and more reviewable than an indexed array. A small repeated struct is still useful, e.g.:

```cpp
struct Cnr3CacheLookupSiteDiagnosticStats {
    std::uint64_t invocations = 0;
    std::uint64_t looks_counted = 0;
    std::uint64_t hits_counted = 0;
    std::uint64_t misses_counted = 0;
};
```

Then `Cnr3CacheOwnershipDiagnosticStats` can have named members such as `site1_requested_frame_check`, `site2_predecessor_fastpath`, etc.

## 4. Clarification needed: inline instrumentation must not count selftest-only routes

This is the main technical hazard in v1.

The scope says `Cnr3LookupSite::unspecified` is the default for selftest and untagged calls, and that such calls count into no per-site bucket. That is good. However, the scope also says inline sites 3, 4, and 8 should bump directly inside the walk, scan, and stores. If implemented unconditionally, this would bypass the `unspecified` protection and pull selftest-only calls into per-site rows.

### 4.1 Recovery walk and hole-catalogue scan need an opt-in flag/tag

`plan_bounded_recovery_search_and_record_anchor_pin(...)` is called by live `arInitial`, but it is also called by multiple selftests. The inline walk and scan are inside the shared cache-core implementation. Therefore, site 3 and site 4 instrumentation should be enabled only for the live route.

Recommended amendment:

- Add a defaulted diagnostic site-observation parameter to the recovery planning public/locked path, for example:

```cpp
enum class Cnr3LookupSiteObservation { none, live_recovery_plan };
```

or a clearer boolean such as:

```cpp
bool observe_lookup_site_breakdown = false
```

- `cnr3_arInitial.cpp` passes `true` / `live_recovery_plan`.
- All selftest callers inherit the default false/none.
- Site 3 walk invocations and counted fields are bumped only when this opt-in is live.
- Site 4 hole-catalogue scan invocations are bumped only when this opt-in is live.
- Site 5 anchor re-pin receives the primitive site tag only when this opt-in is live; otherwise `unspecified`.

This preserves the previous rule: selftest synthetic probes do not enter production-style per-site buckets.

### 4.2 Store duplicate site 8 must be policy-gated, not unconditional

Both store duplicate-detect functions are reachable from production and from selftest-only/public wrapper routes. The intent-counted patch already solved this for merged counters by defaulting `duplicate_count_policy = none` and passing `hit_only` only from the live production store routes.

The same rule must apply to site 8 invocations:

- If `duplicate_count_policy == Cnr3LookupCountPolicy::hit_only`, bump `site8_bail_before_store.invocations` at the store duplicate-detect find.
- If `duplicate_count_policy == none`, do not bump site 8 at all.
- On duplicate found under `hit_only`, bump site 8 `looks_counted` and `hits_counted` beside the existing merged query/hit increments.
- On not-found under `hit_only`, only `invocations` moves; `looks/hits/misses` remain zero.

This keeps the L1 oracle shape (`site8 invocations > 0`, contributes 0/0/0) while avoiding selftest-only store probes being printed as production site-8 volume.

## 5. Clarification needed: does W3X want semantic-site rows or literal code-location rows?

The scope's goal says the report should show the full breakdown **by location**, but several proposed rows intentionally aggregate multiple code locations:

- site 6 aggregates five add-ref re-acquire call sites:
  - duplicate-winner lookup after lost store race,
  - requested-frame cache-hit return re-acquire,
  - predecessor re-acquire,
  - hole predecessor re-acquire,
  - target predecessor re-acquire.
- site 7 aggregates floor adopt and hole adopt.
- site 8 aggregates plain-store duplicate detect and AS2 pin-recording-store duplicate detect.

This aggregation is implementable and defensible if the rows are **semantic-site rows**. It is less satisfactory if W3X's desired output is literal code-location provenance.

### 5.1 Specific concern: site 6 label is too narrow

The proposed `site6_reacquire_pinned` name is true for several call sites, but not quite true for the duplicate-winner re-acquire after a lost store race. That lookup is guaranteed-present because the store returned `duplicate`, but it is not necessarily a re-acquire of a frame already pinned by this request.

Recommended options:

- Minimal wording fix: rename site 6 to something like:

```text
site6_guaranteed_reacquire
```

and describe it as:

```text
guaranteed-present re-acquire: already-pinned frame or duplicate winner proven present by store status
```

- Higher-clarity split: keep a separate zero-proof row for the duplicate-winner re-acquire, e.g.:

```text
site6_reacquire_already_pinned
site7_duplicate_winner_reacquire
```

and renumber later sites. This would make the former taxonomy site 11 visible again as a zero-proof race-loss lookup location.

I prefer the minimal wording fix if W3D wants to keep the row count small. I prefer the split if W3X wants maximum diagnostic clarity for out-of-order/race intuition.

### 5.2 Optional splits for sites 7 and 8

For maximum clarity, these could also be split:

```text
site7a_floor_adopt_bail_early
site7b_hole_adopt_bail_early
site8a_plain_store_duplicate_check
site8b_as2_store_duplicate_check
```

I do not require this before patching. The aggregate rows are mechanically safe. But the split would answer the literal question "which code location did this come from?" without requiring the reader to know the grouping.

## 6. Suggested implementation adjustments if v1 is approved with the clarifications above

### 6.1 Site enum

The proposed enum is fine, with one possible rename:

```cpp
enum class Cnr3LookupSite {
    unspecified,
    requested_frame,
    predecessor_fastpath,
    recovery_walk,
    hole_catalogue_scan,
    anchor_repin,
    guaranteed_reacquire,   // preferred over reacquire_pinned if site 6 remains aggregated
    hole_adopt,
    store_duplicate
};
```

If W3D wants exact v1 labels, `reacquire_pinned` is implementable; I would add a comment saying it also includes duplicate-winner guaranteed re-acquire.

### 6.2 Primitive parameters

Add defaulted `Cnr3LookupSite site = Cnr3LookupSite::unspecified` after the existing `Cnr3LookupCountPolicy count_policy` on:

- `lookup_frame_and_add_ref(...)`
- `lookup_frame_and_add_ref_locked(...)`
- `lookup_frame_and_record_pin(...)`
- `lookup_frame_and_record_pin_locked(...)`
- `pin_frame_locked(...)`

Primitive helper rules:

- `site == unspecified`: no per-site field changes.
- Any concrete site: bump `invocations` after argument/invariant early returns and before `frame_index_.find`.
- `full`: bump site looks beside merged query before find; on miss bump site misses before returning not_found; on hit bump site hits beside merged hit.
- `hit_only`: on miss only invocation moves; on hit bump site looks and hits beside the merged query+hit increments.
- `none`: invocation moves if site is concrete, but no looks/hits/misses move.

This supports zero-proof sites 5 and 6 cleanly.

### 6.3 Recovery plan opt-in

Add a defaulted recovery-site observation flag through:

- `plan_bounded_recovery_search_and_record_anchor_pin(...)`
- `plan_bounded_recovery_search_and_record_anchor_pin_locked(...)`
- `plan_bounded_recovery_search_locked(...)`

Live `arInitial` opts in. Selftests remain defaulted out.

### 6.4 Store site 8 opt-in

Use the existing `duplicate_count_policy` as the site-8 opt-in:

- live production plain store route passes `hit_only` -> site8 invocation visible.
- live AS2 route passes `hit_only` -> site8 invocation visible at 10a, while nested 10b stays `none`.
- selftest/public wrappers stay `none` -> no site8 invocation.

This continues the exact anti-double-count rule established in `CMS07-DIAG.intent-counted-lookups`.

## 7. Caller-map / anomaly sweep expectation for the actual patch

Before delivering a patch, I will repeat the caller-map sweep from the previous phase. The delivery note should include at least:

1. all live and selftest callers of `lookup_frame_and_add_ref`, with site tags;
2. all live and selftest callers of `lookup_frame_and_record_pin`, with site tags;
3. all routes into `pin_frame_locked`, proving the site tag is not dropped;
4. all live and selftest callers of `plan_bounded_recovery_search_and_record_anchor_pin`, proving recovery walk/scan instrumentation is opt-in;
5. all routes into `store_owned_frame_locked`, proving site8 is policy-gated and 10a->10b remains `none`;
6. all routes into `store_owned_frame_and_record_pin_locked`, proving only the live AS2 route opts in;
7. explicit confirmation that site 4/5/6 counted fields have no bump path.

## 8. Selftest impact expectation

I agree the canonical 4-way count should remain 56/56, but I would phrase the reason carefully:

- The new per-site fields are additive observers.
- Existing selftests do not assert on these fields.
- However, because some of the touched cache-core functions are selftest-reachable, avoiding selftest-only per-site noise depends on the opt-in/defaulting rules described above.

## 9. Proposed response to W3D

My recommended ruling request is:

1. Please confirm whether the D-SUM-04 breakdown rows are intended to be **semantic-site aggregates** or **literal code-location rows**. If semantic aggregates are intended, I can patch with the v1 site list, but I recommend renaming site 6 to `guaranteed_reacquire` or explicitly documenting that it includes the duplicate-winner re-acquire.
2. Please confirm that recovery walk/hole-catalogue site instrumentation should be live-route opt-in only, so selftest calls to recovery planning do not enter per-site rows.
3. Please confirm that store duplicate site 8 invocations should be gated by the same `duplicate_count_policy == hit_only` production opt-in used by the merged counters, so selftest store routes remain uncounted.

With those clarifications, I see no need to reject the scope. The patch should be straightforward but review-heavy because it touches proven walk/store code and multiple defaulted call chains.

## 10. Current coder position

**Proceed after clarification.** I would not patch from v1 silently because the selftest-route issue is exactly the sort of instrumentation drift that the previous 10a/10b review caught. Once W3D confirms the two opt-in rules and the desired aggregation granularity, I can produce a single patch with the usual R-PROCESS-25 delivery note.
