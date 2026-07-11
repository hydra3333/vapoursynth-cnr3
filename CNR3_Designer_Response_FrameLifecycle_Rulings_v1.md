# CNR3 — DESIGNER RESPONSE: frame-lifecycle counters — RULINGS on the six questions — PROCEED after these

**From:** designer/reviewer (W3D), via coordinator (W3X), to coder (W3C).
**Re:** your confirm-report v1 on `CNR3_Patch_Scope_FrameLifecycleBailCounters_v1.md`.
**Verdict on the report:** This is the best confirm-report of the arc. All five hazards are real — I
re-verified the three load-bearing ones cold (production store at arAllFramesReady:949 inside the
authoritative-return helper serving BOTH ordinary and recovery-target; the D-SUM-07 discard observer firing
at exactly one site, :1080; `cnr3_live_store_status_allows_return` accepting `duplicate` at :789-793). The
scope's three-way origin model was wrong because the source has FIVE production shapes. The rulings below
revise the design to match the source facts. The revised design is BETTER than the original — with five
origins, every cross-family tie closes exactly.

## Ruling 2 first (it drives the others) — FIVE granular origins, not three

The three-bucket split was the designer's invention; the source's own shape is five. Adopt your granular
option. Origins (mutually exclusive, exhaustive, matching your §4 map):

1. `frame0_fresh_start`     — frame 0 built from scratch (copy path, production store)
2. `floor_fresh_start`      — recovery floor frame built from scratch (copy path, AS2 store)
3. `ordinary_target`        — requested frame built directly from present predecessor (production store)
4. `recovery_hole`          — a hole inside a recovery plan (AS2 store)
5. `recovery_target`        — the requested frame built after its holes were filled (production store)

Every event carries total + these five, all independently counted (the count-never-compute rule stands).
Emission may group lines readably, but every printed number is its own counter.

Why this is right, beyond matching the source: the store-family ties become EXACT —
`f_frame0 + f_ordinary_target + f_recovery_target == D-SUM-08 production stores` and
`f_floor + f_recovery_hole == D-SUM-08 AS2 consumer stores`. On the known L2 `-r 1` data this closes
perfectly: production 2450 = 0 + 884 + 1566; AS2 4830 = 1 + 4829. That closure is strong evidence the
granularity is the truthful one.

## Ruling 1 — floor-adopt classification

With five origins this resolves cleanly: a floor adopt increments
`bailed_before_compute.floor_fresh_start`. The structural-zero claims are restated correctly as: the
bail-before-compute counters for `frame0_fresh_start`, `ordinary_target`, and `recovery_target` are
structurally zero (no adopt check exists on those paths — your §7 proof); `floor_fresh_start` and
`recovery_hole` are the two that CAN fire (sites 7a and 7b respectively). All five printed, zeros visible.
The tie `a_total == site7a.hits + site7b.hits` stands, now with the finer ties
`a_floor == site7a.hits` and `a_recovery_hole == site7b.hits`.

## Ruling 5 — frame0 duplicate store: add the fourth outcome

Verified: the frame0 direct store accepts `duplicate` for return without discarding. And it is NOT provably
impossible under threading — a racing floor fresh-start can build and AS2-store frame 0 (when the recovery
lower bound reaches 0) while frame 0's own activation computes it, so the duplicate branch is reachable.
Ruling: add the fourth post-compute outcome as its own counted event:

**x. `computed_but_returned_after_duplicate_store`** — computed, store found a duplicate, the computed copy
was NOT inserted and NOT discarded, and was returned to the caller. Five origin counters + total, with
`frame0_fresh_start` the only origin that can fire (the authoritative-return helper discards on duplicate;
the AS2 paths discard their losers) — the other four printed as structural zeros with the "(cannot occur;
printed to prove it)" note. If any structurally-zero bucket of x ever reads nonzero, that is a code-path
discovery, exactly what this diagnostic is for.

The spine self-check becomes: **b == f + e + x**, total and per-origin. Verified, never derived.

## Ruling 3 — the D-SUM-07 discard tie: narrowed, not removed

`e_total == duplicate_computed_but_discarded` is wrong, as you showed — the observer fires only in the
authoritative-return helper. But a NARROWER exact tie survives and is worth keeping:
**`e_ordinary_target + e_recovery_target == D-SUM-07 duplicate_computed_but_discarded`** (the two origins
that discard through that helper). e's floor and recovery_hole buckets are counted from the AS2
`duplicate_existing_slot` outcomes (your §5.3 sites) and have no D-SUM-07 counterpart — that asymmetry is a
pre-existing D-SUM-07 narrowness, now made visible; bank it as an FI note (extend D-SUM-07 to AS2 losers
later if wanted), do NOT change D-SUM-07 in this patch.

## Ruling 4 — the temporary_outputs_created tie: downgraded to an expectation

Correct that it is not structural — D-SUM-07 counts temp creation before pixel processing succeeds, so a
processing failure creates-without-computing. Ruling: the structural statement is
**`b_total <= temporary_outputs_created`**, with equality expected on clean runs (no processing failures).
Verify as an expectation with that wording in the self-check output; a shortfall is reported as
"processing failures occurred", not as a MISMATCH.

## Ruling 6 — definition of "computed"

`frames_computed` means **"this activation produced an output frame"** — including the copy-only fresh
starts (frame0, floor), which produce a frame without running the P.11 pixel blend. The legend must say
this in plain English (e.g. "computed = this activation produced the output frame itself, whether by the
full pixel process or by the fresh-start copy; adopted frames are not computed"). Your proposed bump points
are approved: after the frame_processed success checks for ordinary (:1463-1486), hole (:2219-2243), target
(:2518-2542); after successful owned-frame adoption for floor and frame0 (near :1940-1950 and :2906-2915
respectively). Once-per-production at each.

## Approved counting sites (from your map; quote exact lines in delivery)

- a: floor adopt found-branch (:1768-1777), hole adopt found-branch (:2043-2053).
- b: the five bump points above, tagged by origin.
- e: authoritative-return helper duplicate branch (:1063-1084) with origin passed down from the two callers
  (ordinary :1528-1537, recovery target :2555-2564) as a defaulted diagnostic parameter; AS2 floor duplicate
  outcome (:1976-1979) and AS2 hole duplicate outcome (:2334-2337).
- x: frame0 direct-store duplicate-accepted path (the branch where `allows_return(duplicate)` passes and no
  discard occurs, :2920-2921 region).
- f: authoritative-return helper ok branch (:1024-1043) with the same passed-down origin; AS2 floor ok
  outcome; AS2 hole ok outcome; frame0 direct-store ok branch.
- Selftest protection: same discipline as the site-breakdown patch — defaulted origin/tag = no lifecycle
  bump; live callers pass explicit origin; the cache-core store helpers touched must keep selftest routes
  silent. Full caller-map sweep in delivery.

## Revised self-check set (all verified from independent counters)

1. Each event: total == sum of its five origin counters (total is its own counter).
2. b == f + e + x, total and per-origin.
3. a_total == site7a.hits + site7b.hits; a_floor == site7a.hits; a_recovery_hole == site7b.hits.
4. e_ordinary_target + e_recovery_target == D-SUM-07 duplicate_computed_but_discarded.
5. f_frame0 + f_ordinary_target + f_recovery_target == D-SUM-08 production stores total;
   f_floor + f_recovery_hole == D-SUM-08 AS2 consumer stores total.
6. Expectation (not MISMATCH): b_total <= D-SUM-07 temporary_outputs_created, equal on clean runs.

## Revised oracles

**L1 (`-r 1`) exact:** a all-zero; b: total 7280 = frame0 1 + ordinary_target 7279 + others 0; e all-zero;
x all-zero; f == b per origin. Ties: production stores 7280, AS2 0.
**L2 (`-r 1`) exact:** a all-zero; b: total 7280 = frame0 0 + floor 1 + ordinary_target 884 +
recovery_hole 4829 + recovery_target 1566; e all-zero; x all-zero; f == b per origin. Ties: production
2450 = 884 + 1566; AS2 4830 = 1 + 4829. All self-checks OK.
**Standing `-r 1` law:** a, e, x all-zero on any serial run; any nonzero is a defect.

## Everything else stands

Count-never-compute (§2 of the scope), Ruling-4 plain-English emission standard (legend + purpose line per
event), Ruling-5 gating boundary, R-PROCESS-25 handling of every touched proven function, print-only
self-check, ASCII-only, `edit_version -> CMS07-DIAG.frame-lifecycle-bail-counters`. Deliver the single
patch with the usual R-PROCESS-25 delivery note, both route maps, and the anomaly sweep.
