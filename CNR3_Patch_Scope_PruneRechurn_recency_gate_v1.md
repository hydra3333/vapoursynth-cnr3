# CNR3 — PATCH SCOPE: PRUNE-RECHURN COUNTER — recency-gate the evict-then-re-request counter

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Status:** PROPOSAL for coder investigate/confirm. Confirm-before-patch. Verify every claim COLD against live
`src/` (file:line) before touching anything.
**Scope size:** SMALL and SURGICAL. One counter's semantics change + its rename + emission/selftest updates + one
new proof scenario. NO refactor, NO structural change to the shared scan, NO touch to the histogram or
top-thrashers. Diagnostic-build-only code (already under `#if CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION`), so there
is NO runtime-performance consideration — do not optimise, do not bound the scan for speed.

## 0. Why (the finding this fixes)
The existing D-SUM-10 counter `frames_evicted_then_re_requested` counts, on any cache miss whose frame is found
ANYWHERE in the recently-evicted ring, "this frame was evicted at some point and is now re-requested." Live
testing (S9/S9b vs S9c/S9d) proved this is DOMINATED by intended far-revisits (a jump back to an old region),
which is legitimate workload, NOT a cache problem — so the raw counter is misleading and, worse, invites a
maintainer to chase phantom thrash (it already misled us). The USEFUL question is narrower: "was a frame that was
evicted *recently* needed again shortly after?" — i.e. a frame the cache dropped and then had to rebuild within a
short eviction-window, which is the signal that smells of a prune/policy problem (especially the concurrent case:
one thread prunes frame Y, another re-fetches Y moments later).

Design reasoning already settled (do not re-litigate): (1) frame-number "locality" is NOT usable here — the ring
matches the EXACT looked-up frame, so requested==evicted always, and the output frame N is not in scope at the
lookup; the recursion already confines lookups to N's back-radius by construction, so locality is guaranteed, not
measured. (2) The only meaningful discriminator is EVICTION-RECENCY: how many evictions happened between this
frame's eviction and its re-request. That value is `eviction_gap`, ALREADY COMPUTED in the function (currently
only for the gap histogram). (3) The bound is `Z = BACK_RADIUS + 2` in eviction-count units (see §3).

## 1. THE CHANGE (exact)
File: `src/cnr3_cache_core.cpp`, function `Cnr3OutputCacheCore::observe_lookup_miss_rechurn_locked(int
frame_number)`.

CURRENT structure (verified cold):
```
2315   if (!found) { return; }
2319   cnr3_cache_diag_saturating_increment(prune_diag_stats_.frames_evicted_then_re_requested);   // UNCONDITIONAL
2323   ... top-thrashers block ...
2364   std::uint64_t eviction_gap = total_evicted_records >= newest_eviction_sequence
                                    ? total_evicted_records - newest_eviction_sequence : 0U;       // computed LATE
2369   if (eviction_gap == 0U) { eviction_gap = 1U; }
2372   ... gap_bin histogram binning uses eviction_gap ...
```

REQUIRED change — two moves, nothing else:
1. **Hoist the `eviction_gap` computation** (the lines currently at ~2364–2370, the `total_evicted_records − newest_eviction_sequence` and the `==0 → 1` normalisation) to immediately AFTER `if (!found) return;` (after 2316), so `eviction_gap` is available before the counter. The histogram below then reuses the same local (delete the now-duplicate computation at 2364; do NOT compute it twice).
2. **Gate the counter increment** on the recency bound:
```
   if (eviction_gap <= CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP) {
       cnr3_cache_diag_saturating_increment(prune_diag_stats_.frames_recently_evicted_then_re_requested);
   }
```
   (new counter name per §4; new constant per §3.)

That is the entire logic change. The `found` scan, the top-thrashers block, and the gap histogram binning are
UNTOUCHED except that the histogram now reads the hoisted `eviction_gap` local instead of computing its own.

## 2. FENCE — must NOT change
```
- The full ring scan stays full (the histogram legitimately needs deep/old matches for its far bins). Do NOT
  bound, truncate, or early-out the scan. It is diag-only; speed is irrelevant.
- The `newest_eviction_sequence` selection (scan keeps the NEWEST eviction of a multiply-evicted frame) stays.
- The gap histogram (`gap_bin`, [DSUM10-GAP-HISTO]) and top-thrashers / `frames_re_requested_repeatedly` logic
  are UNCHANGED in behaviour and output. They are separate signals; leave them exactly as-is.
- No other D-SUM family, no plan-trace, no cache algorithm (prune/evict/hot-zone) touched. This is a diagnostic
  counter definition change only — it must not alter any cache behaviour or any returned frame.
```

## 3. The constant (new, derived, profiles automatically)
Add beside the other prune/hot-zone constants in `cnr3_cache_core.h`:
```cpp
// Recency bound for the prune-rechurn counter, in EVICTION-COUNT units (not frame numbers).
// A re-fetched frame counts as "recently evicted" only if <= this many evictions occurred since it was dropped.
// Derived from BACK_RADIUS (the recursion confines predecessor lookups to ~one back-radius of the output frame,
// so nothing older is meaningful) + 2 slack for the frame-count-vs-eviction-count unit mismatch under prune
// bursting. Must stay > 1x BACK_RADIUS (else it merely restates locality) and well under ~3x (else it re-admits
// the stale far-revisits this counter exists to exclude). Auto-profiles with BACK_RADIUS (52 NORMAL / 17 TINY-100).
inline constexpr std::uint64_t CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP =
    static_cast<std::uint64_t>(CNR3_CACHE_HOT_ZONE_BACK_RADIUS) + 2U;
```
Express it as `BACK_RADIUS + 2`, NOT a literal 52 — it must track the profile.

## 4. Rename — this REPLACES the old counter (the misleading total is deleted, not kept alongside)
Old name `frames_evicted_then_re_requested` → new `frames_recently_evicted_then_re_requested`.
Rationale for the name: the bound is EVICTION-RECENCY, not frame-number back-radius — naming it
`_within_back_radius` would reintroduce exactly the frame-locality implication we proved false. "recently" is the
honest word. (Coordinator may override the name, but not back to a frame-locality implication.)

All 4 references move together (verified cold):
```
cnr3_cache_diagnostics.h:179     field decl:  std::uint64_t frames_evicted_then_re_requested = 0;
                                  -> rename field to frames_recently_evicted_then_re_requested
cnr3_cache_core.cpp:~2320         the (now recency-gated) increment  -> new field name
cnr3_cache_diagnostics.cpp:461    the D-SUM-10 emission row (label string + stats.<field>) -> new name both places
cnr3_cache_core_selftest_main.cpp:584   fixture: stats.frames_evicted_then_re_requested = 3;  -> new field name
```

## 5. Emission label / column width
In `cnr3_cache_diagnostics.cpp:461` the row label string becomes `"frames_recently_evicted_then_re_requested"`
(longer than the old name). Confirm the D-SUM-10 label column is wide enough for the longer label (it aligns via
the `cnr3_cache_diag_write_uint64_row` formatting); widen the label field if the new name overflows the current
alignment, matching how the other D-SUM-10 rows align. (Same discipline as the memory-table width fix: labels
right/left-align cleanly, no ragged column.)

## 6. Selftest
`cnr3_cache_core_selftest_main.cpp:584` sets the field to 3 in a fixture — rename the field reference. If any
selftest asserts on the OLD semantics (unconditional count), update the expectation to the recency-gated
semantics; state whether any assertion changed and the new four-way count (expected unchanged: this is a
diagnostic-field rename + a gating condition, not a new selftest case — confirm).

## 7. PROOF matrix (the oracle already exists from the S9-series)
Run under TINY-100 (`CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY`), all families + plantrace on, `-r 1`, and read
the new `frames_recently_evicted_then_re_requested`:
```
S9c (contiguous 250, shuffle 5, NO jumps)          -> 0    (nothing evicted-then-refetched at all)
S9d (disjoint jumps fwd+back, no revisit, shuffle) -> 0    (evicted frames never re-requested)
S9  / S9b (far revisits, jumps 500..2500)          -> SMALL / near 0, and MUCH less than the old 481/565
                                                       (those re-requests are hundreds/thousands of evictions
                                                        old -> gap >> Z -> excluded). This is the whole point:
                                                        the far-revisit noise disappears.
S9e (NEW positive control, §8)                      -> NON-ZERO (a frame evicted only a few evictions ago is
                                                       re-requested; gap <= Z -> counted). Proves it still fires
                                                       when it should.
```
Cross-check: on every run, `frames_recently_evicted_then_re_requested <= ` the old-style total (sanity: the
gap histogram's near bins [DSUM10-GAP-HISTO] `<=10`/`<=50` should roughly bracket the new counter — it is
essentially the sum of the histogram bins with gap <= Z).

## 8. S9e — the new positive-control scenario (harness-only, add to the .vpy alongside S1..S9d)
Intent: force a frame to be evicted and then re-requested within a FEW evictions (small `eviction_gap`), so the
recency counter must fire. Under TINY-100 (ceiling 100), process a run that overflows the cache, then revisit a
region whose frames were evicted only recently — a SMALL backward jump in eviction-terms (note: it can be a large
FRAME-number jump; what matters is that the target was evicted only a few evictions ago). Candidate:
```
# *** SCENARIO S9e — recency positive control: revisit a JUST-evicted region so eviction_gap <= Z.
#   Under TINY-100 (ceiling 100): fill past the ceiling, then jump back to a region evicted only a few evictions
#   ago. Expect frames_recently_evicted_then_re_requested > 0 (small). -r 1.
#clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments(clip_denoised_base, jump_to_frames=[0, 5], segment_length=120, shuffle_in_zones=False, zone_size=1)
```
The exact non-zero value is workload-dependent; the acceptance is simply `> 0` on S9e AND `0` on S9c/S9d. If the
candidate does not produce a small gap (e.g. the revisit target turns out still-resident), tune the second jump
target closer to the just-evicted frames — the coder/proof may adjust to land `eviction_gap <= Z`; state what was
used.

## 9. Coder confirm-report delivers
1. Cold confirmation of the 4 rename sites + the current `eviction_gap` computation site, and that hoisting it
   above the counter is behaviour-neutral for the histogram (same value, computed once, used twice).
2. Confirmation the scan / top-thrashers / histogram are otherwise untouched (whole-function diff).
3. Selftest impact (§6) + four-way count.
4. The S9e result (>0) and S9c/S9d (0) and S9/S9b (now small) — the proof matrix.
5. Anything this scope got wrong or missed (highest-value part).

## 10. Out of scope (explicitly deferred, do NOT bundle)
- The maintainability refactor (splitting the one function into scan + per-purpose observers) — a SEPARATE future
  item with its own scope + byte-identical-output proof. Not now.
- Any second/independent "local-window eviction pressure" metric — deferred; not built here.
Marker at commit: to be assigned (this is an analysis-track diagnostic fix, not a DIAG-arc step).
