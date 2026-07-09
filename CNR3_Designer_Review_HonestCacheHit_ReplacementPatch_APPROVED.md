# CNR3 — DESIGNER REVIEW NOTE: replacement patch `CMS07-DIAG.honest-cache-hit-metrics`

*From the outgoing (experienced) designer to the incoming designer chat. This is advice + a completed
diff-review of the replacement patch, so you can carry it forward and gate the commit with confidence.
Verify anything below COLD against live `src/` if you want independence — that is the house rule and it is
how this project stays correct. Nothing here should be taken on trust just because I wrote it.*

## Verdict
**The replacement patch is APPROVED.** It is the previously-approved patch PLUS exactly one added block —
the derived `cache_lookup_misses` emission row — and nothing else. No other line added, removed, moved, or
reformatted. The one outstanding item from the prior review is now closed. It is ready for the proof gate.

## How I verified (so you can reproduce it)
Mechanical diff of the two patches, additive lines only:
```
diff  <(grep '^[+-]' approved.patch)  <(grep '^[+-]' replacement.patch)
```
Result: a single hunk — six added lines, zero deletions, zero other changes:
```
+    cnr3_cache_diag_write_uint64_row(
+        instance_id, "D-SUM-04", "D-SUM-04", "cache_lookup_misses",
+        stats.cache_lookup_queries_total >= stats.cache_lookup_hits
+            ? (stats.cache_lookup_queries_total - stats.cache_lookup_hits)
+            : 0U);   // derived; else-branch unreachable when increments are placed correctly
+                     // (query++ before the find, hit++ on found branch) -- reaching it signals a bug.
```
Placement confirmed: immediately AFTER the `cache_lookup_hits` write_uint64_row in the D-SUM-04 summary
emission (cnr3_cache_diagnostics.cpp) — so it inherits the same D-SUM-04 gating as its siblings, no
separate `#if` needed. Derived (no third stored counter). Underflow-guarded (`>=` before subtract), and the
guard's else-branch is a placement-bug tripwire, not a runtime path. House style matches the neighbouring
rows exactly.

## What was already reviewed-correct in the base patch (do NOT re-litigate)
Carried unchanged from the approved patch; I verified these cold in the prior cycle:
- **Three per-frame health rows** replacing the misleading `cache_hit_and_supplied_percent`:
  `pred_returned_from_cache_percent` = frames_pred_present / frames_total;
  `current_frame_returned_from_cache_percent` = frames_cache_hit / frames_total;
  `cache_hit_percent` = (frames_pred_present + frames_cache_hit) / frames_total. frame0 excluded from all
  numerators (it is the cold-start remainder), in the denominator. All operands already in the D-SUM-12
  snapshot; gated D-SUM-12; disabled-markers present.
- **Two new D-SUM-04 counters** `cache_lookup_queries_total` + `cache_lookup_hits`, incremented at BOTH
  lookup entry points (`lookup_frame_and_add_ref_locked` ~3785 and `pin_frame_locked` ~3883) via
  house-style observer helpers, gated `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`. Placement is
  load-bearing and correct: **query++ AFTER the argument/invariant early-returns, immediately BEFORE the
  `frame_index_.find`** (so a rejected call is not counted as a query); **hit++ as the FIRST statement of
  the found path** (so later ownership failures in the hit path still count as a hit — the lookup DID find
  the frame; the found/not-found identity depends on this).
- Deliberately NOT reusing `lookup_refs_acquired` as the hit count: that counter increments at ONE site
  only while the miss observer fires at TWO, so reusing it would break `hits + misses == queries`. This
  asymmetry is real (I traced it cold) and is why the patch adds its own symmetric pair. Do not "simplify"
  this later without re-checking the asymmetry.
- `cache_lookup_hit_rate_percent` health row = hits / queries, "n/a" on zero, gated D-SUM-04.
- Block-level health guard extended to include D-SUM-04. Option-A wiring (fresh quiescent snapshot at
  teardown). `edit_version -> CMS07-DIAG.honest-cache-hit-metrics` bumped in-patch. Fence intact: no lookup
  semantics, no existing counter/label, no pixel path touched.

## The proof gate you should run before the coordinator commits (this is the real approval)
The sandbox cannot exercise the plugin; the harness runs ARE the proof. Two objective checks and two
invariants:
1. **Build + canonical 4-way** (R-PROCESS-26): expect **56/56** UNCHANGED (all additions are observe-only;
   the health block is plugin-teardown-only, absent from the selftest orchestrator).
2. **Macro-off byte-identical** (R-PROCESS-19): frame output identical with the diagnostics gated off (the
   change is teardown diagnostics; it cannot touch pixels — but prove it, don't assume it).
3. **L1 / L2 harness re-run against the ORACLE** (these numbers are from the real 7280-frame 576p50 runs
   that motivated the whole fix — the patch must reproduce them):

   | row | L1 (linear, -r 1) | L2 (shuffle8, -r 1) |
   |---|---|---|
   | pred_returned_from_cache_percent | 99.986 | 12.143 |
   | current_frame_returned_from_cache_percent | 0.000 | 66.346 |
   | cache_hit_percent | 99.986 | 78.489 |
   | cache_lookup_hit_rate_percent | 100.000 | < 100 (misses > 0) |
   | cache_lookup_misses | 0 | > 0 |

   INVARIANTS (both must hold on every run):
   - **branch sum-to-100:** cache_hit_percent + cache_miss_recovery_plan_percent + (frame0/total)*100 == 100.
     L1: 99.986 + 0.000 + 0.014 = 100.000. L2: 78.489 + 21.511 + 0.000 = 100.000.
   - **lookup identity:** cache_lookup_hits + cache_lookup_misses == cache_lookup_queries_total (now a
     PRINTED line thanks to this row — read it, don't compute it).
   EXPECTED DIVERGENCE (record, don't flag): on L2, cache_lookup_hit_rate_percent (query-denominated) and
   cache_hit_percent (frame-denominated) differ — different denominators, by design. The divergence is a
   signal (how much recovery-walk probing happened), not an error.

If 1-3 are green and both invariants hold -> commit `CMS07-DIAG.honest-cache-hit-metrics`, then the doc
currency touch (DELTA v4.31 / Doc B / Provenance).

## After commit — the forward thread (why this fix mattered)
This patch exists because L1 (the 99% vspipe->ffmpeg linear case) is the workload A1 must ultimately
explain, and its health headline was lying. With honest metrics in place, the immediate next runs are the
ones that answer the real production question:
- **L1noR / L2noR** — L1 and L2 with `-r 1` REMOVED (real fmParallel-style threading). Does the clean
  linear case STAY clean under threads, or do recovery / rechurn / recalc appear? That gap is **A1 question
  #1**. Watch cache_lookup_hit_rate and recent_rechurn especially.
- Then B-series (B2 exact/floor, B3 backward revisit, B4 wide shuffle, B5 hot-zone cap) to round out the
  counter-behaviour picture, then **A1 scope** (fresh designer chat; seed set in Provenance v1.8).

## Two habits that carried this whole arc (keep them)
1. **A metric named for a human must mean what the human thinks it means.** The original row #1 was
   arithmetically correct but named for a definition it did not use (it read 0% on a healthy run). The whole
   fix was an honesty relabel + the missing denominator. When a counter surprises you, suspect the NAME
   before the code.
2. **Objective backstops over argument.** Every "provably correct" claim this arc was settled by a macro-off
   byte-identical run or a hand-check against raw counters — not by how convincing the reasoning sounded.
   The sum-to-100 and hits+misses==queries invariants are that discipline made into printed lines.
