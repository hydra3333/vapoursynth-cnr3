# CNR3 — DESIGNER REVIEW: `CMS07-DIAG.honest-cache-hit-metrics` (replacement patch) — APPROVED

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Re:** your replacement patch adding the derived `cache_lookup_misses` emission row.
**Verdict:** APPROVED. Clean. Proceed to the proof gate (build + canonical 4-way); the coordinator will
relay the result and the designer runs the harness proofs before commit.

*(This note follows the house pattern: it says WHAT was checked, HOW, and WHY the change is sound — not
just "looks good." The reasoning is the point: it lets you see the review was done against source, and it
gives the next coder chat a worked example of the bar to clear.)*

## What you were asked to do, and what you delivered
Asked: take the previously-approved honest-cache-hit patch and add the ONE omitted item — the derived
`cache_lookup_misses` row in the D-SUM-04 summary — changing nothing else.
Delivered: exactly that. Confirmed by mechanical diff of your replacement against the approved patch:

```
diff <(grep '^[+-]' approved.patch) <(grep '^[+-]' replacement.patch)
```
-> a single added hunk of six lines, zero deletions, zero other changes:
```
+    cnr3_cache_diag_write_uint64_row(
+        instance_id, "D-SUM-04", "D-SUM-04", "cache_lookup_misses",
+        stats.cache_lookup_queries_total >= stats.cache_lookup_hits
+            ? (stats.cache_lookup_queries_total - stats.cache_lookup_hits)
+            : 0U);   // derived; else-branch unreachable when increments are placed correctly ...
```
That is exactly the discipline this project runs on: a scoped one-line task comes back as a one-line diff,
so the review is a five-minute mechanical confirmation rather than a re-read of the whole patch. Well done —
this is the pattern.

## Why the change is correct (the reasoning, verified cold)
1. **Placement.** The row sits immediately AFTER the `cache_lookup_hits` write_uint64_row, inside the same
   D-SUM-04 summary emission block. It therefore inherits the block's D-SUM-04 gating — no separate `#if`
   is needed or wanted (adding one would have been an error). Correct call.
2. **Derived, not stored.** No new struct field, no third counter, no new increment site. Misses are
   computed at emission from the two existing counters. This is right: a stored miss counter would be a
   second source of truth that could drift from `queries - hits`; deriving it makes drift impossible.
3. **Underflow guard.** `queries >= hits ? queries - hits : 0U` — correct for unsigned arithmetic (a bare
   subtract would wrap catastrophically if hits ever exceeded queries). And the guard's else-branch is a
   deliberate tripwire: it is UNREACHABLE if the two increments are placed correctly (query++ before the
   find, hit++ on the found branch), so if it ever fires it means a placement regression — the comment says
   exactly this. Good defensive instinct.
4. **The invariant is now visible.** With this row, `cache_lookup_hits + cache_lookup_misses ==
   cache_lookup_queries_total` is a PRINTED line a human can read at a glance, instead of mental
   arithmetic. That was the whole point of requiring it — a health summary should let a human sense what is
   happening without a calculator.

## What was carried unchanged from the approved patch (correctly untouched)
You did not disturb any of the reviewed-correct parts — confirmed by the deletion scan (zero deletions):
the three per-frame health rows (`pred_returned_from_cache_percent` / `current_frame_returned_from_cache_
percent` / `cache_hit_percent`), the two counters and their increments at BOTH lookup entry points
(query++ before the `find`, hit++ first statement of the found path), the decision NOT to reuse
`lookup_refs_acquired` (it covers one entry point, the miss observer covers two — reusing it would break
the hits+misses==queries identity), the `cache_lookup_hit_rate_percent` health row, the block-guard
extension, and the `edit_version` bump. Leaving proven code alone is as important as adding the new row
(R-PROCESS-21/25) — you did.

## What happens next (so you know the state)
This is diagnostic-only and observe-only, so the objective gates are:
1. **Coordinator builds** Debug|x64 + Release|x64, runs the canonical 4-way (R-PROCESS-26). Expected
   **56/56** UNCHANGED — the new row is additive emission; the health block is plugin-teardown-only and is
   absent from the selftest orchestrator, so the count cannot change. If it changes, stop and report.
2. **Designer harness proofs** (the .vpy/.bat harness is the designer's; you do not run these): macro-off
   byte-identical, and L1/L2 re-runs against a fixed oracle, checking both invariants (branch sum-to-100;
   hits+misses==queries) and confirming `cache_lookup_misses` reads 0 on L1 (no recovery) and > 0 on L2.
3. If green -> coordinator commits `CMS07-DIAG.honest-cache-hit-metrics` and pushes.

Nothing is owed from you right now beyond the 4-way result. If the harness proofs surface anything (they
are not expected to — this is observe-only), the designer will relay a specific finding.

## One thing worth carrying forward (the reason this patch existed)
This whole change came from a metric that was arithmetically correct but MISLEADING: the old
`cache_hit_and_supplied_percent` read 0.000 on a perfectly healthy production run, because it counted only
one of the two ways the cache serves a frame. The fix was fundamentally an honesty relabel plus the missing
denominator. The standing lesson for all of us — coder, designer, coordinator — is: **a counter named for a
human must mean what the human thinks it means.** When a number surprises you, suspect the NAME and the
DENOMINATOR before you suspect the machinery. Your misses row is a small piece of that same principle:
making the hit/miss/query relationship legible rather than implied.

Good work. Await the coordinator's 4-way result.
