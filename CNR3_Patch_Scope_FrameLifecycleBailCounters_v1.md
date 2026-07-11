# CNR3 — PATCH SCOPE: frame-lifecycle bail/compute/store counters (D-SUM-04) — v1

**Marker on success:** `CMS07-DIAG.frame-lifecycle-bail-counters`
**Baseline:** `CMS07-DIAG.lookup-site-breakdown` (current committed tree).
**Track:** analysis-arc diagnostic, observe-only, permanent (diagnostic-era; compiles out with the D-SUM-04
gate). No cache/lookup/pin/store/recovery semantics, ownership, or pixel-path changes. CMS unchanged.

## 1. Goal, in plain English

Print a frame-lifecycle summary in the D-SUM-04 block that a human can read top-to-bottom as the life of a
frame — was the work skipped because the frame was already there (bail before compute), how many frames were
actually computed, how many computed frames were thrown away because another activation stored first (bail
after compute), and how many were stored — with EVERY line split three ways by the frame's production
origin. This is aimed squarely at the upcoming threaded runs: under fmParallel the splits show WHICH kind of
frame wins/loses which kind of race.

## 2. The cardinal rule of this patch: COUNT, never compute

Every printed number is its own counter, bumped in the code at the moment the event happens. NO printed
value may be derived at emission time (no `x - y`, no `x + y`, no reuse of another counter as a proxy).
The self-checks in §6 are VERIFICATION ONLY: independently-counted numbers that must agree. If a self-check
fails, that is a real miscount finding, never a display issue. This is deliberate redundancy: independent
counts that must agree are the proof; a single count displayed several ways proves nothing.

## 3. The three-way origin split (applies to every event)

Every counted event is tagged with the frame's production origin, exactly one of:

- **frame0_or_floor_fresh_start** — built from scratch with no predecessor (frame 0, or a floor fresh-start
  when recovery found no anchor in radius).
- **ordinary_from_predecessor** — predecessor was present; built directly from it (the fast path).
- **recovery_hole_fill** — built by the recovery mechanism (a hole in a recovery plan, or the recovery
  target computed from filled holes).

The three buckets must be MUTUALLY EXCLUSIVE and EXHAUSTIVE at each counting site — every event lands in
exactly one. §7 makes proving this the coder's first job, before any patch.

## 4. The counters (16 independent counters; four events x [total + three origins])

Event labels are fixed (W3X wording):

**a. `bailed_before_compute_since_already_in_cache`** — about to produce a frame, checked first, found it
already present, skipped the compute (the adopt). Bumped at the adopt check, found branch, before any
compute. Four counters: total, frame0, ordinary, recovery.
NOTE (structural): the adopt check exists only on the recovery path (sites 7a/7b), so the frame0 and
ordinary counters here are expected to be STRUCTURALLY ZERO — they are still real counters and still
printed (positive-zero symmetry, per W3X): a visible 0 proves "adopt is recovery-only" on every run instead
of asserting it.

**b. `frames_computed`** — this activation actually ran the filter and produced pixels for the frame,
regardless of whether the result was later stored or discarded. Bumped at the compute site, once per real
compute, tagged by origin. Must include later race-losers; must NOT include adopted frames (a) — they were
never computed. Four counters.

**e. `bailed_after_compute_because_another_activation_stored_it_first`** — the frame was computed (in b),
then the store-time duplicate check found another activation had stored it first, so this copy was
discarded. Bumped at the store duplicate-detect, found branch, tagged by the origin of THIS activation's
computed copy. Four counters.

**f. `frames_computed_and_stored`** — the frame was computed (in b) and its store succeeded. Bumped at the
successful-store point, tagged by origin. Four counters.

(Letters c/d/g/h from the discussion are the ordinary/recovery sub-lines of b and f; they are ordinary
counters like the rest, just displayed indented.)

## 5. Emission (D-SUM-04 block; Ruling 4 plain-English standard applies in full)

A new sub-block after the per-site breakdown, lifecycle order, every value a direct counter read:

```
[DSUM-SUMMARY] D-SUM-04 frame lifecycle summary (each number is counted where the event happens; nothing is derived):
[DSUM-SUMMARY] D-SUM-04 bailed_before_compute_since_already_in_cache      total=0
[DSUM-SUMMARY] D-SUM-04     of which frame0/floor fresh-start             = 0   (cannot occur; printed to prove it)
[DSUM-SUMMARY] D-SUM-04     of which ordinary (from predecessor)          = 0   (cannot occur; printed to prove it)
[DSUM-SUMMARY] D-SUM-04     of which recovery (hole fill)                 = 0
[DSUM-SUMMARY] D-SUM-04    -> before computing, checked whether another activation had already produced the frame; when found, adopted it and skipped the work.
[DSUM-SUMMARY] D-SUM-04 frames_computed                                   total=7280
[DSUM-SUMMARY] D-SUM-04     of which frame0/floor fresh-start             = 1
[DSUM-SUMMARY] D-SUM-04     of which ordinary (from predecessor)          = 7279
[DSUM-SUMMARY] D-SUM-04     of which recovery (hole fill)                 = 0
[DSUM-SUMMARY] D-SUM-04    -> frames this run actually computed (pixels produced), including any later discarded as duplicates.
[DSUM-SUMMARY] D-SUM-04 bailed_after_compute_because_another_activation_stored_it_first  total=0
        (same three sub-lines + purpose line)
[DSUM-SUMMARY] D-SUM-04 frames_computed_and_stored                        total=7280
        (same three sub-lines + purpose line)
[DSUM-SUMMARY] D-SUM-04 lifecycle self-check: computed == stored + discarded (7280 == 7280 + 0), splits partition their totals -> OK
```

(Numbers illustrative: L1 values shown.) Self-check line prints OK, or a loud `*** MISMATCH ***` naming
which identity failed and the two disagreeing counted values. Print-only; never wired to selftest.
ASCII-only. Legend addition explaining "counted where the event happens; nothing is derived".

## 6. Self-checks (verified, never derived — all must hold every run)

1. Per event: total == frame0 + ordinary + recovery (four partition checks — note the total is ITS OWN
   counter, bumped alongside the bucket counter, so this check is two independent counts agreeing).
2. b_total == f_total + e_total (everything computed is either stored or discarded); ALSO per-bucket:
   b_frame0 == f_frame0 + e_frame0, b_ordinary == f_ordinary + e_ordinary, b_recovery == f_recovery + e_recovery.
3. Cross-family ties (existing validated counters): a_total == site7a.hits + site7b.hits;
   e_total == D-SUM-07 `duplicate_computed_but_discarded`; b_total == D-SUM-07 `temporary_outputs_created`
   (coder to confirm this last equivalence cold — if `temporary_outputs_created` includes any non-compute
   creation, report it; do NOT force the tie by moving b's bump).
4. Stored-split ties: f_ordinary + f_frame0 vs D-SUM-08 production stores, f_recovery vs D-SUM-08 AS2
   consumer stores — EXPECTED ties; the coder must confirm cold how frame0/floor stores are classified in
   D-SUM-08 store families and report the exact mapping rather than assume it.

## 7. Coder investigation FIRST (confirm-before-patch; this is the heart of this scope)

W3X requires 100% certainty of no ambiguity. Before any patch, the confirm-report must establish, cold
against source, with file:line:

1. **The compute site(s).** Where exactly are pixels produced (the place b must be bumped)? Is it one site
   or several? At each, can the code determine the origin {frame0/floor, ordinary, recovery} at that moment,
   mutually exclusively and exhaustively? Show the branch structure. If ANY compute can occur where the
   origin is ambiguous or a fourth path exists, STOP and report — do not pick a bucket silently.
2. **The store site(s).** Same questions at the successful-store point for f, and at the duplicate-detect
   found branch for e (including: at the store duplicate-detect, does the storing code KNOW the origin of
   the copy being discarded? If the origin isn't in scope there, propose how to carry it — do not infer it).
3. **The adopt check.** Confirm the ordinary fast-predecessor path and the frame0/floor path have NO
   before-compute presence/adopt check anywhere — proving a's frame0/ordinary counters are structurally
   zero. If any such check exists, report it (that would change the design).
4. **Once-per-compute.** Prove b's bump site fires exactly once per real compute: not at request (would
   count adopted frames), not at store (would miss race-losers), no path that computes twice for one
   activation without two real computes.
5. **Frame0-in-store-families.** Report how frame0/floor stores are classified in D-SUM-08 (production vs
   AS2) so §6.4's ties are stated correctly.
6. **Caller-map / anomaly sweep** as per the previous two patches, for every function touched — including
   confirming selftest routes cannot bump the new counters (same opt-in/default discipline as the
   site-breakdown patch; reuse its live-route mechanisms where the counting sites coincide).

The confirm-report answers these BEFORE patching; W3D re-verifies cold; only then the patch.

## 8. Gating, fence, discipline

- Gating per Ruling 5 pattern: all new counters/structs/bumps/emission under
  `CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE`; any new defaulted parameter plain and ungated; committed
  plumbing untouched.
- R-PROCESS-25: the compute and store sites are proven code — counting statements and defaulted parameters
  only; no control flow, no reordering; exact insertion lines quoted in delivery.
- Fence: existing counters (site breakdown, merged lookups, D-SUM-07/08/12) untouched; no row renamed.
- Macro-off must remain byte-identical.
- `edit_version` -> `CMS07-DIAG.frame-lifecycle-bail-counters`.

## 9. Proof gate

1. Canonical 4-way: 56/56 unchanged (forced-fail 55/56 e1). Confirm selftest prints the new block at
   all-zero (or absent per the live-route gating — coder to state which and why).
2. R-PROCESS-19 macro-off byte-identical (S8 A/B, fc /b).
3. **L1 (`-r 1`) exact oracle:** a: 0/0/0/0. b: total 7280, frame0 1, ordinary 7279, recovery 0.
   e: 0/0/0/0. f: identical to b (no races serial). All self-checks OK.
4. **L2 (`-r 1`):** a and e all-zero; b_total == f_total == 7280; per-bucket b == f; b/f buckets must tie to
   the known plan structure (frame0/floor 1, recovery == the AS2-store count, ordinary the remainder — read,
   then verified against §6.4 ties). Self-checks OK. Existing site-breakdown self-check still OK; D-SUM-12
   unchanged.
5. At `-r 1`, a == 0 and e == 0 EVERYWHERE is itself an oracle line: any nonzero at `-r 1` is a defect.
