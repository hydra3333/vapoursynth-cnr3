# CNR3 coder response to W3D: Intent-counted cache lookup metrics v3

From: coder (W3C), via coordinator (W3X), to designer/reviewer (W3D)  
Re: `CNR3_Patch_Scope_IntentCountedLookups_v3.md` and `CNR3_Cache_Lookup_Taxonomy_Findings_v06.md`  
Source inspected: uploaded `src(24).zip`

## 1. Source currency check

The uploaded source appears to be the expected current source for this scope.

Cold checks against `src(24).zip`:

- `src/cnr3_build_config.h:35` has
  `CNR3_EDIT_VERSION = "CMS07-DIAG.honest-cache-hit-metrics"`.
- The `cache_lookup_misses` row from the previous approved patch is present in
  `src/cnr3_cache_diagnostics.cpp:312-317`, between `cache_lookup_hits` and
  `lookup_refs_acquired`.
- The current lookup counters are still the uniform primitive counters:
  - `src/cnr3_cache_core.cpp:3788-3803` in `lookup_frame_and_add_ref_locked()`.
  - `src/cnr3_cache_core.cpp:3886-3901` in `pin_frame_locked()`.
- `grep` over `src/cnr3_cache_core_selftest.cpp` and
  `src/cnr3_cache_core_selftest_main.cpp` found no direct references/assertions
  for `cache_lookup_*`, `cache_lookup_hit_rate_percent`, or
  `queries_total`. There are many selftest calls to the lookup primitives, but
  no apparent selftest expectation on the lookup counter values.

So I would treat `src(24).zip` as the intended post-commit baseline for this
review: `CMS07-DIAG.honest-cache-hit-metrics`.

## 2. High-level verdict

The intent-counted rule set is implementable and coherent. I agree with the
human-facing direction: the new `cache_lookup_hit_rate_percent` should measure
informative cache service, not structurally-guaranteed cold misses or guaranteed
re-acquires.

I would not patch yet without W3D acknowledging three mechanical points below.
Two are implementation hazards that could otherwise silently violate the new
semantics; the third is a documentation/provenance cleanup needed to avoid
shipping a new misleading line immediately after fixing the old misleading line.

## 3. Required clarification / correction 1: `hit_only` cannot count query before find

The scope proposes a policy enum:

```cpp
enum class Cnr3LookupCountPolicy { none, full, hit_only };
```

I agree with this shape, but the exact increment rule must be stated carefully.
For `hit_only`, the query increment cannot occur before the `find`. If it does,
a miss would increment `cache_lookup_queries_total` without a hit, and the
derived `cache_lookup_misses` row would still count the structurally-certain or
speculative miss that the new rules explicitly suppress.

Recommended primitive policy semantics:

```text
none:
  no D-SUM-04 lookup query/hit increments

full:
  query++ before the find, after argument/invariant early returns
  hit++ as first statement of the found path

hit_only:
  no query++ before the find
  on found path only: query++ and hit++ together, before later ownership/
  validation returns
  on miss: no query, no hit
```

That preserves the printed invariant because every counted query still resolves
inside the same probe:

```text
full miss:      query+1, hit+0 -> derived miss+1
full hit:       query+1, hit+1 -> derived miss+0
hit_only miss:  query+0, hit+0 -> no event
hit_only hit:   query+1, hit+1 -> derived miss+0
```

This matters at all hit-only sites:

- site 1: arInitial frame N hit-only
- site 3: walk-N-1 special-case hit-only
- site 8: adopt probes hit-only
- site 10a/10b: duplicate-detect hit-only

If W3D agrees, the patch should implement `hit_only` as "count query+hit only
on the found branch", not as "count query at entry".

## 4. Required clarification / correction 2: site 10a and 10b can double-count unless routed carefully

The scope treats 10a and 10b as two duplicate-detect sites:

- 10a: `store_owned_frame_and_record_pin_locked()`, find at
  `src/cnr3_cache_core.cpp:2832`.
- 10b: `store_owned_frame_locked()`, find at
  `src/cnr3_cache_core.cpp:2710`.

Cold source check: 10a currently calls through 10b:

```text
src/cnr3_cache_core.cpp:2832  10a pre-find in store_owned_frame_and_record_pin_locked()
src/cnr3_cache_core.cpp:2857  const Cnr3Status store_status =
src/cnr3_cache_core.cpp:2858      store_owned_frame_locked(frame_number, frame, is_checkpoint);
src/cnr3_cache_core.cpp:2710  10b find in store_owned_frame_locked()
```

Therefore, if both locations are instrumented naïvely, an AS2 duplicate store
will count twice: once at 10a and again in the nested 10b call.

Recommended implementation approach:

1. Keep 10a as the AS2/pin-recording-store duplicate signal. If
   `existing_slot_found` is true in `store_owned_frame_and_record_pin_locked()`,
   count query+hit once at the start of that found path.
2. Add a private/defaulted store duplicate-count policy to
   `store_owned_frame_locked()` so it can count direct/plain-store duplicates
   without double-counting nested AS2 duplicates.
3. When `store_owned_frame_and_record_pin_locked()` calls
   `store_owned_frame_locked()`, pass `none` for the nested 10b duplicate count.
4. When the direct non-AS2 store route calls `store_owned_frame_locked()` from
   `store_owned_frame_and_prune_impl()` at `src/cnr3_cache_core.cpp:1110`, pass
   `hit_only` so plain-store duplicate returns count once.
5. Leave public/selftest store wrappers defaulted to `none` unless W3D wants
   selftest synthetic store duplicates included, which I do not recommend under
   the current scope.

This preserves the taxonomy distinction while avoiding an AS2 double count.

## 5. Required clarification / correction 3: output prose must change or it will become false

The scope says the D-SUM-04 emission rows and health row are kept as-is, with two
new health rows added. The row names can remain as-is, but at least two existing
human-facing prose lines must be updated because the old uniform meaning will no
longer be true.

Current source lines that will become misleading:

```text
src/cnr3_cache_diagnostics.cpp:301-304
  "cache_lookup_* counts both add-ref lookup and pin lookup entry points; ..."
```

Under intent counting, this is false: some add-ref and pin lookup entry-point
calls intentionally count nothing, while site 3 and site 10 are not simply the
old two primitive entry points.

```text
src/cnr3_diagnostics.cpp:2726-2728
  "cache_lookup_hit_rate_percent = cache_lookup_hits / cache_lookup_queries_total"
  "This is the true lookup hit rate across both lookup entry points..."
```

The formula remains correct, but the interpretation should say it is the
intent-counted lookup hit rate over counted probes. It is no longer a uniform
rate across both primitive entry points.

```text
src/cnr3_diagnostics.cpp:2777
  "cache_lookup_hit_rate_percent <- D-SUM-04 cache_lookup_hits / cache_lookup_queries_total; ..."
```

This note should probably be extended or paired with a second note for the two
new rows:

```text
cache_lookup_hits_percent   <- D-SUM-04 cache_lookup_hits / cache_lookup_queries_total
cache_lookup_misses_percent <- D-SUM-04 derived (queries - hits) / cache_lookup_queries_total
```

Coder recommendation: the fence should explicitly permit updating diagnostic
comments/provenance text that describes the lookup counter semantics. Without
that, the patch would replace one misleading human-facing metric with another
misleading human-facing explanation.

## 6. Suggested implementation shape after the clarifications

I recommend the policy-parameter approach, with a small extension for stores.

### 6.1 Lookup/pin primitive policy

Add a public/internal enum, likely in `src/cnr3_cache_core.h` near the cache-core
API declarations:

```cpp
enum class Cnr3LookupCountPolicy {
    none,
    full,
    hit_only
};
```

Add defaulted parameters:

```cpp
lookup_frame_and_add_ref(...,
    Cnr3LookupCountPolicy count_policy = Cnr3LookupCountPolicy::none) const;

lookup_frame_and_record_pin(...,
    Cnr3LookupCountPolicy count_policy = Cnr3LookupCountPolicy::none);

lookup_frame_and_add_ref_locked(...,
    Cnr3LookupCountPolicy count_policy = Cnr3LookupCountPolicy::none) const;

lookup_frame_and_record_pin_locked(...,
    Cnr3LookupCountPolicy count_policy = Cnr3LookupCountPolicy::none);

pin_frame_locked(...,
    Cnr3LookupCountPolicy count_policy = Cnr3LookupCountPolicy::none);
```

Reason for including `pin_frame_locked()`: the actual `frame_index_.find()` and
current uniform increments for the pin-recording primitive live in
`pin_frame_locked()` at `src/cnr3_cache_core.cpp:3886-3901`, not directly in
`lookup_frame_and_record_pin_locked()`.

Counted call-site opt-ins:

```text
site 1  arInitial:913   lookup_frame_and_record_pin(n, ..., hit_only)
site 2  arInitial:963   lookup_frame_and_record_pin(n - 1, ..., full)
site 8  arAll:1762      lookup_frame_and_record_pin(floor_frame, ..., hit_only)
site 8  arAll:2035      lookup_frame_and_record_pin(hole_frame, ..., hit_only)
```

Formerly counted sites 5/6/7/9/11 should not opt in and should therefore count
nothing under the default `none`.

### 6.2 Recovery walk site 3

Instrument only the anchor-search loop:

```text
src/cnr3_cache_core.cpp:3606-3630
```

Do not touch the hole-catalogue loop:

```text
src/cnr3_cache_core.cpp:3640-3658
```

Recommended counting placement:

- compute whether `candidate_frame == upper_bound` before the probe;
- for non-N-1 candidates, count query before the `find`;
- after a found result, count hit; for N-1 hit-only, count query+hit together;
- for N-1 miss, count nothing;
- make no control-flow, loop-bound, early-return, or ordering change.

This respects the W3X Decision 1 special case and R-PROCESS-25.

### 6.3 Store duplicate sites 10a/10b

Add store duplicate-count support with explicit route control to prevent nested
double-counting.

Possible shape:

```cpp
store_owned_frame_locked(...,
    Cnr3LookupCountPolicy duplicate_count_policy = Cnr3LookupCountPolicy::none);
```

Then:

```text
10a AS2/pin-recording store:
  count query+hit if existing_slot_found at 2837;
  call nested store_owned_frame_locked(..., none)

10b direct/plain store:
  count query+hit in the duplicate branch at 2712 only when the caller passed hit_only;
  direct route at 1110 passes hit_only;
  public/selftest store wrappers remain default none unless deliberately changed
```

This is the part I most want W3D to review before patching, because it is the
one place where the v3 scope's table is correct in intent but incomplete about
current call nesting.

### 6.4 Health rows

Add under the same D-SUM-04 compute+print gate as the existing hit-rate row:

```text
cache_lookup_hits_percent   = cache_lookup_hits / cache_lookup_queries_total
cache_lookup_misses_percent = derived_misses / cache_lookup_queries_total
```

Use the same disabled-marker style as `cache_lookup_hit_rate_percent` when
D-SUM-04 is disabled. The derived numerator for misses should use the same
underflow guard as the D-SUM-04 summary row.

The existing `cache_lookup_hit_rate_percent` row can remain as an alias for
`cache_lookup_hits_percent` if W3D wants continuity, but the two rows will then
carry the same numeric value. That duplication is acceptable only because W3D's
scope explicitly asks to add the two rows while keeping the existing row.

## 7. Macro-off and D-SUM-10 interaction

I recommend leaving the D-SUM-10 `observe_lookup_miss_rechurn_locked()` sites
unchanged in this patch.

Reason: the scope says no existing counter other than the lookup pair changes
meaning. D-SUM-10 is a prune/re-churn observer, not the human-facing
intent-counted lookup hit-rate. After this patch, some primitive not-found events
that are suppressed from `cache_lookup_misses` may still feed D-SUM-10. That is
acceptable if recorded as intentional.

However, any previous documentation statement that `cache_lookup_misses >=`
D-SUM-10 ring-matched re-churn traffic should be retired or narrowed, because
intent-counted misses are no longer the same population as primitive miss
observations.

## 8. Selftest impact

Source grep found no direct selftest assertions on `cache_lookup_*` values.
Therefore I do not see a selftest-fixture blocker before patching.

Expected impact:

- canonical 4-way count remains 56/56 unchanged;
- selftest synthetic lookup primitive calls compile unchanged and count nothing
  under default `none`;
- D-SUM-04 ownership counters for pins/lookup refs remain live and unchanged;
- D-SUM-04 lookup query/hit/miss rows may show lower values in selftest output,
  but there appears to be no asserted fixture depending on those values.

## 9. Documentation nits in the provided documents

Two small doc-currency nits found while reading:

1. `CNR3_Patch_Scope_IntentCountedLookups_v3.md` says site numbers are from the
   "v05 census" while the authority document is v06. If v3 is revised, I suggest
   changing this to v06.
2. `CNR3_Cache_Lookup_Taxonomy_Findings_v06.md` disposition still names
   `CNR3_Patch_Scope_IntentCountedLookups_v2.md`; current uploaded scope is v3.

These are not implementation blockers, but they are worth cleaning if the docs
are being versioned again.

## 10. Coder recommendation

Proceed with the patch after W3D accepts these clarifications:

1. `hit_only` means query+hit only on found, no query before find.
2. Site 10a/10b must be routed to avoid double-counting AS2 duplicate stores,
   because 10a currently calls through 10b.
3. The patch may update D-SUM-04 and D-SUM-HEALTH prose/comments that describe
   lookup-counter semantics, because the old "both lookup entry points" wording
   will become false under intent counting.

No algorithm, lookup/pin/store semantics, recovery behavior, ownership behavior,
or pixel-path behavior should change. The patch should remain diagnostic-only
and observe-only.
