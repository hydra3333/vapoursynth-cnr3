# CNR3 — PATCH SCOPE: HONEST CACHE-HIT METRICS — per-frame branch rows + true lookup hit-rate

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Status:** PROPOSAL. Confirm-before-patch. Verify every claim COLD against live `src/` (file:line).
**One commit.** Scope: (Part 1) fix the misleading health ratio #1 with three honest per-frame rows (health-block
arithmetic only, ZERO new counters); (Part 2) add a true cache-lookup hit-rate, which requires TWO new D-SUM-04
counters at the two lookup entry points. Diagnostic-only; no cache algorithm, pixel path, or returned-frame change.

## 0. Why (findings from the L1/L2 production-case runs)
L1 (whole-clip linear, the 99% vspipe->ffmpeg case) read `cache_hit_and_supplied_percent = 0.000` while the cache
served the predecessor from cache for 7279/7280 frames. Cold trace: the linear case takes the
`predecessor_present_compute` branch (frames_pred_present, predecessor found IN CACHE and pinned), while
`frames_cache_hit` counts ONLY the `cache_hit_return` branch (the REQUESTED frame N itself pre-cached, returned
without compute). Ratio #1 as shipped therefore reads 0% on a perfectly healthy production run — a misleading
headline (coordinator ruling: "anything named cache hit to a human should be a cache hit"). Additionally there is NO
total-cache-queries counter, so a classical hit-rate (hits / queries) is currently uncomputable; coordinator ruled
"a query is a query and a hit is a hit regardless of kind" — count ALL lookups uniformly at the primitive; the
top-level-vs-recovery-walk decomposition is A1's job (plan-trace), not the counter's.

## PART 1 — per-frame branch rows (health-block arithmetic; NO new counters)

### 1.1 REPLACE health row #1
Delete `cache_hit_and_supplied_percent` (misleading). In its place emit THREE rows (all denominators frames_total,
all operands ALREADY in the D-SUM-12 snapshot: frames_pred_present, frames_cache_hit, frames_frame0, frames_total):
```
pred_returned_from_cache_percent           = frames_pred_present / frames_total
current_frame_returned_from_cache_percent  = frames_cache_hit    / frames_total
cache_hit_percent                          = (frames_pred_present + frames_cache_hit) / frames_total
```
Semantics (bake into per-row provenance comments): pred_returned_from_cache = the predecessor output[N-1] was found
in cache (and pinned); N was computed from it. current_frame_returned_from_cache = the requested frame N itself was
in cache and returned WITHOUT compute. cache_hit = the human headline: the cache held what was needed (either kind).
frame0 is in NO numerator (the cold start is neither hit nor miss) but stays in the denominator — e.g. L1 headline
is honestly 99.986, not 100.000.

### 1.2 Sum-to-100 integrity invariant (REQUIRED, verified in proof)
```
cache_hit_percent + cache_miss_recovery_plan_percent + (frames_frame0/frames_total)*100 == 100.000
```
(the D-SUM-12 branches partition frames_total: frame0 + pred_present + cache_hit + recovered_exact + recovered_floor
== frames_total. Coder verifies this identity holds in the proof runs; a footnote line may state it.)
Expected values (already computed from the L1/L2 logs — the proof oracle):
```
            pred_returned  current_returned  cache_hit   recovery   frame0
L1 linear      99.986            0.000        99.986       0.000     0.014
L2 shuffle8    12.143           66.346        78.489      21.511     0.000
```

### 1.3 Raw D-SUM-12 counter labels — UNCHANGED
Do NOT rename frames_pred_present / frames_cache_hit raw emission rows in this patch (label churn on committed
counters is not needed to fix the health headline; the health-row provenance comments carry the mapping). If the
coordinator later wants the raw labels renamed, that is a separate follow-up.

## PART 2 — true cache-lookup hit-rate (TWO new counters + one health row)

### 2.1 The two counters (D-SUM-04, alongside lookup_refs_acquired)
Add to Cnr3CacheOwnershipDiagnosticStats (cnr3_cache_diagnostics.h ~line 80):
```cpp
std::uint64_t cache_lookup_queries_total = 0;  // every lookup query at the two cache lookup entry points
std::uint64_t cache_lookup_hits = 0;           // queries where the frame was found (hit branch)
```
(misses are DERIVED = queries - hits; do not add a third counter. The derivation is exact by construction when the
increments are placed per 2.2.)

WHY NOT reuse existing counters (verified cold; bake into comments): lookup_refs_acquired increments at ONE site
only (line 3818, inside lookup_frame_and_add_ref_locked) while the miss observer fires at TWO sites (3784 AND 3874,
both entry points) — so lookup_refs_acquired + any miss count would mix one path's hits with two paths' misses and
the hits+misses==queries identity would be false. pins_acquired counts pin OWNERSHIP events, not lookups. The new
pair is deliberately independent of the ownership counters.

### 2.2 Placement (exact, both entry points; verified cold in the committed tree)
Both entry points share an identical shape: `frame_index_.find(frame_number)` then a miss branch (calls
observe_lookup_miss_rechurn_locked under CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION) then the hit path continues.
```
cnr3_cache_core.cpp lookup_frame_and_add_ref_locked:  find ~3780; miss branch 3782-3786; hit path continues 3788+
cnr3_cache_core.cpp pin_frame_locked:                 find ~3870; miss branch 3872-3876; hit path continues 3878+
```
At EACH of the two entry points:
- increment `cache_lookup_queries_total` IMMEDIATELY BEFORE the `frame_index_.find(...)` line (after the argument /
  invariant early-returns, so a rejected/invalid call is NOT a query — only calls that actually reach the find);
- increment `cache_lookup_hits` on the FOUND branch (immediately after the `== end()` miss check falls through, i.e.
  first statement of the hit path), NOT deeper in the hit path (later invariant_violation / capacity returns in the
  hit path must still count as hits for the found/not-found identity to hold — the query FOUND the frame; what
  ownership does with it afterwards is not the lookup's concern).
Gating: the two increments live under a D-SUM-04 compute gate (CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE, matching
how the neighbouring ownership observers are gated — coder verifies the exact macro name cold and uses the same
observer-helper style as observe_pin_acquired_locked / observe_lookup_ref_acquired_locked: a small locked observer
method calling a cnr3_cache_ownership_diagnostic_observe_* helper; no raw increments inline).

### 2.3 Emission (D-SUM-04 summary) + health row
D-SUM-04 summary adds three rows (queries, hits, derived misses):
```
cache_lookup_queries_total     <value>
cache_lookup_hits              <value>
cache_lookup_misses            <queries - hits>   (derived at emission; guard queries >= hits, else emit both raw
                                                    and flag — that would indicate a placement bug)
```
Health block adds ONE row (gated on D-SUM-04 compute+print like the other rows are gated on their families):
```
cache_lookup_hit_rate_percent = cache_lookup_hits / cache_lookup_queries_total     ("n/a" when queries == 0)
```
Provenance comment + footnote: "all lookups at the two cache lookup entry points, ALL contexts (top-level and
recovery-walk probes counted uniformly); the per-context decomposition is the A1 tool's job. Distinct from
cache_hit_percent, which is per OUTPUT FRAME (denominator frames_total)."

### 2.4 Integrity invariant (REQUIRED, verified in proof)
`cache_lookup_hits + cache_lookup_misses == cache_lookup_queries_total` (exact, by derivation) AND
`cache_lookup_misses` must be CONSISTENT with the DSUM10 miss-observer traffic (same two sites): on any run,
cache_lookup_misses >= the number of ring-matched rechurn events (the rechurn observer sees a SUBSET of misses —
only those found in the eviction ring). Sanity, not equality.

## 3. FENCE — must NOT change
```
- No cache algorithm, lookup semantics, pin/ref ownership, prune, recovery, or returned-frame behaviour.
- The existing miss observer (observe_lookup_miss_rechurn_locked), lookup_refs_acquired, pins_acquired,
  and ALL existing raw counter labels/rows unchanged (Part 1.3: no raw renames).
- The other 7 health rows unchanged (cache_miss_recovery_plan_percent, holes_filled, return_transfer, rechurn,
  frames_per_prune_event, recalc x2).
- R-PROCESS-19: with the relevant compute gates off, byte-identical behaviour; the new counters compile out under
  the D-SUM-04 gate; the new health rows gate out per the existing per-row all-operands-live scheme (the three
  per-frame rows gate on D-SUM-12; the hit-rate row gates on D-SUM-04).
```

## 4. Proof matrix (harness runs are the designer's; coder delivers build + 4-way)
Coder: Debug|x64 + Release|x64 clean; canonical 4-way (R-PROCESS-26) — count expected UNCHANGED at 56 (new counters
are observe-only; state if any selftest touches D-SUM-04 fixtures and needs a field add).
Designer (NORMAL profile, -r 1):
1. L1 (linear whole clip): three per-frame rows == the oracle table (99.986 / 0.000 / 99.986); sum-to-100 holds;
   cache_lookup_hit_rate == 100.000 (zero misses expected — no recovery); queries == hits; D-SUM-04 shows the
   triple with misses == 0.
2. L2 (shuffle8 whole clip): per-frame rows == oracle (12.143 / 66.346 / 78.489); sum-to-100 holds;
   cache_lookup_misses > 0 (recovery lookups miss); hit-rate < 100 and its DIVERGENCE from cache_hit_percent is
   expected (different denominators) — record both.
3. Macro-off (D-SUM-04 compute off): the two counters compile out; hit-rate row shows "(source D-SUM-04 disabled)";
   per-frame rows still emit (D-SUM-12 on). Full all-off: no health block (existing behaviour).
## 5. Coder confirm-report
1. Cold confirm of the two entry-point placements (exact lines in live src), the D-SUM-04 gate macro name, and the
   observer-helper style used.
2. Confirmation lookup_refs_acquired / pins_acquired / miss-observer untouched.
3. The hit-increment placement decision (first statement of hit path, before later ownership returns) confirmed or
   argued against WITH the found/not-found identity implication stated.
4. Selftest impact (fixture field adds if any) + four-way count.
5. Anything wrong or missed in this scope.
Marker suggestion: CMS07-DIAG.honest-cache-hit-metrics. Analysis-track diagnostic change.
