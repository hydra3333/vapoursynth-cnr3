# CNR3 — PATCH SCOPE: per-site lookup breakdown counters (D-SUM-04) — v1

**Marker on success:** `CMS07-DIAG.lookup-site-breakdown`
**Baseline:** `CMS07-DIAG.intent-counted-lookups` (current committed tree — patch-on-patch against the fresh commit).
**Track:** analysis-arc diagnostic change, observe-only. Permanent counters (this diagnostic era; they compile
out with the D-SUM-04 gate when gates are disabled for a production release). No cache algorithm,
lookup/pin/store/recovery semantics, ownership, or pixel-path behaviour changes. CMS design unchanged.

## 1. Goal, in plain English

Today D-SUM-04 prints three merged lookup totals (`queries / hits / misses`). We cannot read from the log
**where** each counted look happened — requested-frame check vs predecessor check vs recovery walk. This
patch adds a counter at **every** location that participates in (or is deliberately excluded from) the
lookup totals, so the end-of-run report shows the full breakdown by location, and a self-check line proves
the per-location numbers add up to the merged totals. This turns the L2 reconciliation from an arithmetic
derivation into a literal printed fact.

**W3X rulings baked in:** positive-zero proof on the three never-count locations (they print with their real
invocation volume and a `contributes 0/0/0` marker, so their non-participation is a fact, not a claim); a
mismatch between per-site sums and the merged totals prints a loud `MISMATCH` line in the D-SUM-04 block and
is **never** tied to the selftest pass/fail machinery.

## 2. The locations (normative list — dummy-proof plain English + exact code site)

Each location records four things: `invocations` (how many times the code reached this probe),
`looks_counted` / `hits_counted` / `misses_counted` (what this location contributed to the merged D-SUM-04
totals). For counted locations, the three `_counted` values are bumped on the **identical lines** as the
existing shared `observe_cache_lookup_query_locked` / `observe_cache_lookup_hit_locked` calls, so they cannot
drift from what ships. For never-count locations, the three `_counted` values are structurally always 0 and
only `invocations` moves.

**Phase: arInitial** (VapourSynth first asks the plugin for output frame N; the plan is decided here)

- **Site 1 — requested frame N present-check.** The plugin asks "do I already have finished output frame N?"
  Rule: hit-only (a hit counts; a not-found counts nothing). COUNTED.
  Code: `cnr3_arInitial.cpp:913-916` (the `lookup_frame_and_record_pin(n, ..., hit_only)` call).
- **Site 2 — predecessor N-1 fast-path check.** Only reached when N was not found. "Is N-1 present so I can
  build N directly from it?" Rule: full (look always; hit if present; miss if absent → routes to recovery).
  COUNTED. Code: `cnr3_arInitial.cpp:967-970`.
- **Site 3 — recovery backward walk.** Only reached when N-1 was absent. Steps N-1, N-2, N-3… asking "is this
  earlier frame present?" until it finds the anchor. Rule: first step (N-1) hit-only; deeper steps full.
  COUNTED. Code: the walk loop in `cnr3_cache_core.cpp` (the `observe_cache_lookup_*` calls already inside the
  `for (candidate_frame = upper_bound; …)` loop, ~3665-3682).
- **Site 4 — hole-catalogue scan.** After the anchor is found, lists which frames between anchor and N are
  missing. Never a presence *decision*, just plan bookkeeping. Rule: NEVER COUNTS (zero-proof). Code: the
  separate `for (frame_number = anchor_frame + 1; …)` loop, ~3715-3730.
- **Site 5 — anchor re-pin.** Pins the frame the walk just found. Guaranteed-present re-pin of a frame located
  microseconds earlier. Rule: NEVER COUNTS (zero-proof). Code: the
  `lookup_frame_and_record_pin_locked(anchor, …, none)` call, ~3757-3760.

**Phase: arAllFramesReady** (source frames are ready; the plugin actually builds/returns N)

- **Site 6 — re-acquire an already-pinned frame to compute or return.** Re-fetches a frame that was already
  found and pinned back in arInitial (the requested frame for a cache-hit return; the predecessor or a hole's
  predecessor to compute from; the winner after a lost race). Guaranteed hit. Rule: NEVER COUNTS (zero-proof).
  This one site aggregates the behaviourally-identical re-acquire call locations
  (`cnr3_arAllFramesReady.cpp:1088, 1191, 1350, 2068, 2378`, all `lookup_frame_and_add_ref(…, none)`).
- **Site 7 — adopt / bail-early check.** Just before computing a recovery hole: "did another activation
  already produce this, so I can skip the work?" Rule: hit-only (found → look+hit, adopt; not-found → nothing,
  compute anyway). COUNTED. Code: `cnr3_arAllFramesReady.cpp:1762-1765` (floor) and `2036-2039` (hole).
- **Site 8 — bail-before-store check.** Just after computing, at store time: "did someone else store this
  while I was computing?" Rule: hit-only (found → look+hit, discard my duplicate; not-found → nothing).
  COUNTED. Two code locations, both under this one site: `cnr3_cache_core.cpp:2746-2752` (plain store, 10b)
  and `2879-2885` (AS2 pin-recording store, 10a).

Sites 1, 2, 3, 7, 8 are the five that can contribute to the totals. Sites 4, 5, 6 are the zero-proof set.

## 3. Implementation shape (coder to confirm or counter-propose)

Mirror the existing policy plumbing. Add a diagnostic-only site tag threaded to the primitives, and inline
counting where counting is already inline (walk, scan, stores).

- **New enum**, `src/cnr3_cache_core.h` near `Cnr3LookupCountPolicy`:
  ```
  enum class Cnr3LookupSite {
      unspecified,        // default; selftest and any un-tagged call — counts into no per-site bucket
      requested_frame,    // site 1
      predecessor_fastpath, // site 2
      recovery_walk,      // site 3 (set inline, not via a primitive parameter)
      hole_catalogue_scan,// site 4 (inline)
      anchor_repin,       // site 5
      reacquire_pinned,   // site 6
      hole_adopt,         // site 7
      store_duplicate     // site 8 (inline)
  };
  ```
- **Defaulted parameter** `Cnr3LookupSite site = Cnr3LookupSite::unspecified` on the two lookup/pin primitives
  and their public/locked forms (the same functions that already carry `count_policy`). The primitive:
  - bumps `per_site[site].invocations` once, unconditionally, at the same point the shared query would be
    considered (after argument/invariant early-returns, before the find) — so site 6's invocations are visible
    even though it counts nothing;
  - beside each existing `observe_cache_lookup_query_locked()` / `observe_cache_lookup_hit_locked()` call
    (already policy-gated), bumps the matching `per_site[site].looks_counted` / `hits_counted`; on the miss
    return path, bumps `per_site[site].misses_counted` where the shared derived-miss arises (i.e. `full`
    miss). These sit on the identical lines as the shared bumps.
- **Inline sites (3, 4, 8):** bump the per-site fields directly at the existing inline `observe_cache_lookup_*`
  lines (walk, stores) and at the hole-catalogue scan loop (site 4: `invocations` only, on each `find`).
- **Opt-in tags at the counted call sites:** site 1 → `requested_frame`; site 2 → `predecessor_fastpath`;
  site 7 → `hole_adopt`; anchor re-pin → `anchor_repin`; the re-acquire calls → `reacquire_pinned`. The walk,
  scan, and stores set their site inline.
- **Storage:** add the per-site fields to the existing D-SUM-04 ownership-balance diagnostic struct (same
  struct as `cache_lookup_queries_total`), one `{invocations, looks_counted, hits_counted, misses_counted}`
  group per site. Named fields (not an array) for readable emission. Same
  `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE` gate throughout.
- `edit_version` → `CMS07-DIAG.lookup-site-breakdown`.

## 4. Emission (D-SUM-04 block, teardown)

Add a per-site breakdown sub-block after the existing `cache_lookup_*` summary rows, each row named in full
plain English, e.g.:
```
D-SUM-04 site1_requested_frame_check     invocations=…  looks=…  hits=…  misses=…
D-SUM-04 site2_predecessor_fastpath      invocations=…  looks=…  hits=…  misses=…
D-SUM-04 site3_recovery_walk             invocations=…  looks=…  hits=…  misses=…
D-SUM-04 site4_hole_catalogue_scan       invocations=…  looks=0  hits=0  misses=0   (excluded from totals)
D-SUM-04 site5_anchor_repin              invocations=…  looks=0  hits=0  misses=0   (excluded from totals)
D-SUM-04 site6_reacquire_pinned          invocations=…  looks=0  hits=0  misses=0   (excluded from totals)
D-SUM-04 site7_adopt_bail_early          invocations=…  looks=…  hits=…  misses=…
D-SUM-04 site8_bail_before_store         invocations=…  looks=…  hits=…  misses=…
```
Then a **self-check line**:
```
D-SUM-04 site_breakdown_selfcheck  sum_looks=…  sum_hits=…  sum_misses=…  vs totals …/…/…  -> OK
```
and if any of the three sums does not equal the corresponding merged total
(`cache_lookup_queries_total` / `cache_lookup_hits` / derived misses), replace `OK` with a loud
`*** MISMATCH ***` line naming which of the three disagrees and by how much. The self-check is print-only; it
does not touch `Cnr3Status`, the selftest, or any exit code.

## 5. Fence — what must NOT change

- No lookup/pin/store/walk/recovery semantics, no control flow, no early returns, no reordering. Counting
  statements, a defaulted enum parameter, struct fields, and emission lines only.
- The three never-count sites (4, 5, 6) get `invocations` only; their `_counted` fields must be provably
  never bumped.
- The existing merged `cache_lookup_*` counters, the `hit_rate`/`misses_percent` health rows, and every
  ownership/balance counter are untouched — the per-site counters are additive observers beside them.
- R-PROCESS-25 applies to the walk and the two store functions (proven code): counting statements only; the
  coder quotes exact insertion lines in the confirm-report.
- Macro-off (`CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE` commented) must compile to byte-identical frame
  output.

## 6. Proof gate

1. **Canonical 4-way** (R-PROCESS-26): expect **56/56** unchanged (additive observers; teardown-only
   emission absent from the selftest orchestrator). Confirm no selftest asserts on the new fields.
2. **R-PROCESS-19 macro-off byte-identical**: S8 A/B, `fc /b` identical.
3. **Self-check must print `OK`** on every run (L1, L2, and the byte-identical runs' ON build).
4. **L1 per-site oracle (linear, -r 1), exact:**
   - site 1 requested_frame: invocations 7280, looks 0, hits 0, misses 0 (every N is a first-time cold miss,
     suppressed by hit-only).
   - site 2 predecessor_fastpath: invocations 7279, looks 7279, hits 7279, misses 0.
   - sites 3, 4, 5, 7: invocations 0 (recovery never triggers on a clean linear run).
   - site 6 reacquire_pinned: invocations > 0, contributes 0/0/0.
   - site 8 bail_before_store: invocations > 0 (every store checks), contributes 0/0/0 (no collisions at -r 1).
   - self-check: sum looks 7279 == queries 7279; sum hits 7279 == hits; sum misses 0 == misses. OK.
5. **L2 (shuffle8, -r 1):** the per-site sums must equal 12109 / 7279 / 4830 with self-check OK. Then we READ
   the split — expected shape: site 1 hits == `frames_cache_hit` == 4830; sites 3 (walk) and 2 carry the rest;
   sites 4/5/6 contribute 0; sites 7/8 contribute 0 at -r 1 (interleaving-only). This measured split replaces
   the earlier hand-derivation — whatever it shows is the answer, no calculation.
6. **D-SUM-12 per-frame branch counters unchanged** vs committed L1/L2 (fence held).

## 7. Delivery requirements (coder confirm-report)

- Exact file:line of every insertion; the site-tag opt-in at each counted call site listed and ticked.
- Independent re-derivation of which call sites route into each primitive (the caller-map / anomaly sweep, as
  last patch) — confirm each carries the intended site tag and that `unspecified` (selftest/untagged) counts
  into no per-site bucket.
- Whole-diff deletion enumeration; confirmation no proven line beyond counting statements moved.
- Explicit confirmation the three zero-proof sites' `_counted` fields have no bump path.
