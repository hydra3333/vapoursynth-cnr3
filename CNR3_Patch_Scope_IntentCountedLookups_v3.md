# CNR3 — PATCH SCOPE: intent-counted cache lookup metrics (replaces uniform lookup counting) — v3

**Marker on success:** `CMS07-DIAG.intent-counted-lookups`
**Baseline:** `CMS07-DIAG.honest-cache-hit-metrics` (current committed tree).
**Track:** analysis-arc diagnostic change, observe-only. No cache algorithm, lookup semantics, ownership,
recovery, store, or pixel-path behaviour changes. CMS design unchanged.
**Authority for the rules:** `CNR3_Cache_Lookup_Taxonomy_Findings_v06.md` (the census + W3X Decisions 1 and 2).
This scope is a proposal; the coder's confirm-report reconciles it with real source before any patch.

---

## 1. Goal, in plain English

Replace the current uniform lookup counting (every call through the two pinned primitives counts) with
**intent counting**: a cache probe is counted only when its outcome was genuinely uncertain and changes what
happens next. Probes whose outcome is structurally certain (guaranteed-hit re-acquires of already-pinned
frames; structurally-certain cold misses) count nothing. The headline number keeps its name
(`cache_lookup_hit_rate_percent`) and becomes a "the cache delivered a frame that would otherwise be
computed" gauge: ~100% on a clean single-threaded linear run, rising signal under fmParallel when peer
threads pre-produce frames.

The OLD uniform counting is **removed, not kept alongside** — W3X ruling: the old numbers will not be
reviewed and would confuse a future maintainer. The prior L1/L2 lookup oracle is explicitly retired with it.

## 2. The counting rules (normative table)

Counter set is unchanged in shape: `cache_lookup_queries_total`, `cache_lookup_hits`, derived
`cache_lookup_misses` (= queries − hits, underflow-guarded, unchanged code), and the health rows. Only WHERE
and WHEN the increments fire changes. Site numbers are from the v05 census.

| site | where | probe of | rule |
|---|---|---|---|
| 1 | arInitial:913 | frame N itself | **hit-only**: HIT → query+1, hit+1; MISS → nothing (structurally certain cold miss) |
| 2 | arInitial:963 | predecessor N−1 (fast path) | **full**: query+1 always; HIT → hit+1; MISS → counted miss (genuine — routes to recovery) |
| 3 | recovery walk loop at cache_core:3606, probe `frame_index_.find(candidate_frame)` at 3607 (N−1 key: `candidate_frame == upper_bound`, upper_bound set 3593) | candidates N−1, N−2, … | **full for every candidate EXCEPT N−1**, which is **hit-only** (a walk-N−1 MISS merely re-confirms site 2's counted miss; a walk-N−1 HIT is new concurrency information — a peer produced it between the two lock holds). Per walk: anchor found at candidate c → probes N−2..c+1 are counted misses (plus N−1 nothing-or-hit), c is a counted hit. Floor fresh-start → all probed non-N−1 candidates are counted misses, no hit. **Insert counting at 3607 only; do NOT touch the hole-catalogue scan (site 4, distinct loop at 3640–3641).** |
| 4 | hole-catalogue scan (cache_core:3640–3641, distinct loop) | plan derivation | **never counted** (unchanged from today — do not add) |
| 5 | anchor pin (cache_core:3683) | re-pin of found anchor | **not counted** (TODAY IT IS — this is a removal) |
| 6 | arAllFramesReady:1191 | re-acquire N to return | **not counted** (removal) |
| 7 | arAllFramesReady:1350 | re-acquire pinned N−1 | **not counted** (removal) |
| 8 | arAllFramesReady:2035 and :1762 | hole/floor adopt (bail-early) | **hit-only**: adopted → query+1, hit+1; absent → nothing (speculative; today its miss IS counted — removal of that miss) |
| 9 | arAllFramesReady:2066, :2376 | re-acquire hole predecessor | **not counted** (removal) |
| 10a | store_owned_frame_and_record_pin_locked (find at cache_core:2832) | duplicate-detect at pin-recording store | **hit-only**: existing found (adopt path) → query+1, hit+1; not found → nothing. NEW instrumentation. |
| 10b | store_owned_frame_locked (find at cache_core:2710) | duplicate-detect at plain store | **hit-only**: returns `Cnr3Status::duplicate` → query+1, hit+1; not found → nothing. NEW instrumentation. |
| 11 | arAllFramesReady:1088 | race-loser re-acquires winner | **not counted** (removal) |
| 12–16 | store re-finds, prune/unpin bookkeeping, invariant audit | — | **never counted** (unchanged — the site-16 invariant audit especially must never be instrumented) |

By construction, every counted query resolves to exactly one counted hit or counted miss in the same probe,
so the printed identity `hits + misses == queries` continues to hold on every run.

## 3. Proposed implementation shape (coder to confirm or counter-propose)

The current increments live inside the two primitives (`lookup_frame_and_add_ref_locked` at ~3792 region and
`pin_frame_locked` at ~3890 region) and count uniformly; the primitives cannot know call-site intent.
Proposal:

- **Add a counting-policy parameter** to the two public lookup entry points and their locked forms:
  `enum class Cnr3LookupCountPolicy { none, full, hit_only }` with **default `none`**. The primitive
  increments per policy at the same two internal locations the counters live today (query at entry per
  policy, hit on the found path per policy). Default `none` means: every existing call site — including all
  58 selftest call sites — compiles unchanged and counts nothing; the six counted sites opt in explicitly
  (site 1 `hit_only`, site 2 `full`, site 8 `hit_only`). This makes the removals (5, 6, 7, 9, 11) automatic
  and impossible to miss: they simply never opt in.
- **Walk counting (site 3)** goes inside `plan_bounded_recovery_search_locked` at the existing raw
  `frame_index_.find` loop: candidate == requested_frame−1 → hit-only; otherwise full. Already under
  `cache_mutex_`, so the `_locked` observers are used directly. **This touches proven recovery code:
  R-PROCESS-25 applies — the coder must quote the exact insertion lines in the confirm-report and change
  nothing else in the function; counting statements only, no control-flow, no early-exit, no reordering.**
- **Store counting (10a/10b)** goes inside the two store functions at their existing duplicate-detect
  branches (hit-only), also already under the lock. Same R-PROCESS-25 handling: counting statements only,
  placed inside the already-taken branch.
- The two counters, the derived misses row, the D-SUM-04 emission rows, and the
  `cache_lookup_hit_rate_percent` health row are **kept as-is** (names, gating
  `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`, placement). **Add two health rows**:
  `cache_lookup_hits_percent` and `cache_lookup_misses_percent` (each over queries; n/a on zero queries;
  they must sum to 100.000 when live). Same gate, same disabled-marker style as the existing row.
- `edit_version` → `CMS07-DIAG.intent-counted-lookups`.

If the coder judges the policy parameter too invasive (e.g. header ripple), the acceptable alternative is
explicit observer calls at the six counted decision points with the primitives' internal counting removed —
but that must be proposed in the confirm-report with the exact touched lines, not silently chosen.

## 4. Fence — what must NOT change

- No lookup/pin/store/walk semantics, no control flow, no early returns, no reordering in any touched
  function. Counting statements and a defaulted parameter only.
- Sites 4 and 12–16 stay uninstrumented. The site-16 invariant audit must not be touched at all.
- The three per-frame health rows (`pred_returned_from_cache_percent` etc.) are D-SUM-12-based and are OUT
  of scope — untouched.
- No existing counter other than the lookup pair changes meaning; `lookup_refs_acquired` and all ownership/
  balance counters untouched.
- Macro-off (`CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE` commented out) must compile to byte-identical
  frame output, as today.

## 5. Proof gate (before commit)

1. **Canonical 4-way** (R-PROCESS-26): expect **56/56 UNCHANGED**. Caveat for the coder confirm-report:
   confirm whether any selftest asserts on `cache_lookup_*` values; if one does, report it BEFORE patching —
   expectations there may legitimately change and the designer will rule.
2. **R-PROCESS-19 macro-off byte-identical**: same S8 A/B harness as the previous commit; `fc /b` identical.
3. **Designer harness, NEW oracle** (the old lookup oracle is retired):
   - **L1 (`-r 1`, linear):** queries **7279**, hits **7279**, misses **0**, hit-rate **100.000**,
     hits% 100.000, misses% 0.000. (Per frame N≥1: site 1 miss uncounted, site 2 hit counted; frame 0
     contributes nothing.) This is exact, not approximate.
   - **L2 (`-r 1`, shuffle8):** derived by hand-check at proof time against the run's own plan counts
     (recovery_plans_created, holes, adopt counts) — the designer performs the reconciliation. Invariants
     that must hold: `hits + misses == queries` printed identity; `hits% + misses% == 100.000`;
     site-3 walk arithmetic consistent with recovery counts.
   - **Exact machine-checkable identity (every run, any threading level):**
     **site-1 counted hits == D-SUM-12 `frames_cache_hit`.** The cache-hit-return branch (arInitial:915) is
     taken if and only if the site-1 lookup at arInitial:913 succeeds, so the two increment together on every
     run — verified cold against source. L1: 0 == 0; L2: == 4830; L1noR: == whatever bubbling produced. This
     replaces the earlier (incorrect) "site-1 must be zero at `-r 1`" tripwire.
   - **Interleaving-only zero tripwire (correct set):** at `-r 1`, the sites that require a frame to appear
     *between two separate lock acquisitions* — **walk-N−1-hit, site 8 adopt, site 10a/10b duplicate** (and
     the site-11 race path) — must contribute **zero**, because serial execution cannot interleave two lock
     holds regardless of request order. Corroborated by `duplicates_seen == 0` on L2. Any nonzero there on
     `-r 1` is a defect finding. **Site 1 is NOT in this set:** shuffle (L2) pre-produces frame N with no
     threads at all (a later frame's early request builds its chain through N and stores it), so site-1 hits
     are ~4830 on L2 by design — using the exact identity above, not a zero check.
   - **L1noR (threaded):** the interleaving-only sites may now be nonzero — record, don't gate; this is the
     new signal working. `hits + misses == queries` and the site-1==`frames_cache_hit` identity must still
     hold.
4. Per-frame health rows (D-SUM-12 family) must be **unchanged** vs the committed L1/L2 values — proves the
   fence held.

## 6. Review discipline for this patch (heightened, per W3X)

This patch inserts lines into three proven functions (the walk, two stores) and modifies the two proven
primitives. Both reviews are mandatory and independent:
- **Coder self-check (required in the confirm-report):** exact file:line of every insertion; explicit
  statement that no line other than counting statements and the defaulted parameter was added, removed, or
  moved in proven functions; the selftest-assert check from §5.1; whole-diff deletion enumeration.
- **Designer review:** whole-patch deletion scan; verify each of the six counted sites carries the right
  policy (1 hit-only, 2 full, 3 mixed with the N−1 special case, 8 hit-only, 10a/10b hit-only); verify all
  former counted sites (5, 6, 7, 9, 11) now count nothing; verify invocation (not just definition) of any
  new observer; trace the walk insertion against the loop structure to confirm no iteration-behaviour
  change; confirm the derived-misses row and gate untouched.

## 7. Documentation touch on commit

DELTA/Doc B/Provenance currency entry: old uniform-lookup semantics retired (including the L1 66.664 oracle,
which was correct for those semantics and is void for these); new rules table (this scope §2) recorded;
A1 spec §7.7 note — the reconcile query now reconstructs against the NEW rules only (the old counters no
longer exist to reconcile against).
