# CNR3 — Cache Lookup / Hit / Miss Taxonomy — Findings v06 (census + rulings DECIDED; §1.iii resolved; old uniform counting retired)

*Designer (W3D), responding to W3X's annotated v01. Traced cold against the LATEST tree (marker
`CMS07-DIAG.honest-cache-hit-metrics` — the committed honest-cache-hit counters are in this source; line
numbers below are from this tree and differ slightly from v01's pre-commit tree). Contents: (1) answers to
the three double-probe questions; (2) confirmation/correction of each proposed per-site disposition;
(3) the COMPLETE census requested in (A) — every `frame_index_.find` and every instrumented lookup call
site in the live code, classified; (4) assessment of whether the new counting rules are clear, with the two
ambiguities that need a ruling.*

---

## 1. The three double-probe questions, answered from source

**(i) Is the fast-path N−1 probe and the backward walk all done within one mutex?**
**No — two separate lock acquisitions.** The fast-path probe (`lookup_frame_and_record_pin(n-1)`,
arInitial:963) takes and releases `cache_mutex_` on its own. The recovery call
(`plan_bounded_recovery_search_and_record_anchor_pin`, arInitial:623) then takes the lock again
(cnr3_cache_core.cpp:2533 — wrapper does capacity/reserve work outside the lock per the settled locking
discipline, then one `lock_guard` covers search + anchor pin together). So the walk and the anchor pin are
atomic *with each other*, but **not** with the earlier fast-path miss.

**(ii) Why start the backward search at N−1 rather than N−2?**
Because of exactly that lock gap: under threading, N−1 can be stored by another activation **between** the
fast-path miss and the walk acquiring the lock. If the walk started at N−2 it could select a worse anchor
(more holes) than the truth at walk time, or, at clip start, miss a real anchor entirely. Re-probing N−1
first is correct-by-concurrency: the walk answers "what is the nearest present frame NOW", and NOW is a
later moment than the fast-path probe. (At clip start the bounds already handle it: `upper_bound = N−1`,
`lower_bound = max(0, N−back_radius)`; if the interval is empty the plan degrades to floor-fresh-start.)
Starting at N−2 would be an optimisation valid only under `-r 1` semantics — the design correctly does not
assume that.

**(iii) How can we not double-count N−1 (fast path + walk)?**
Today there is no double-count only because the walk is uncounted. **Under the new rules (walk counted),
N−1 WILL be probed-and-counted twice per recovery** — and my recommendation is to accept that, because they
are two genuinely distinct questions at two distinct times under two distinct lock holds: "was N−1 present
at fast-path time?" (miss) and "is N−1 present at walk time?" (usually miss again; occasionally HIT under
threading, in which case N−1 becomes the anchor and that hit is real and informative). Deduplicating them
would erase precisely the concurrency signal the taxonomy is trying to expose. The cost is quantifiable and
explainable: at `-r 1`, walk-counted misses for N−1 equal recovery_plans_created exactly — the A1 tool can
verify that identity. If you prefer dedup instead, the alternative ruling is "walk starts counting at its
second probe" — mechanically easy, but I advise against it for the reason above.

**RESOLVED (v06) — see §4 Decision 1.** Neither "count both" nor "dedup": the walk-N−1 probe is counted
**only on a miss→hit flip** (a hit means a peer produced N−1 between the two lock holds = new concurrency
information; a repeat miss re-confirms site 2's already-counted miss and counts nothing). The earlier
"count both" recommendation in this paragraph is superseded.

---

## 2. Per-site dispositions — confirmed / corrected against source

Your axis, restated: COUNT only a genuine "is a frame I need present, where the answer could be no and
changes what happens next" probe; do NOT count re-acquires/re-pins of frames already found-and-pinned in an
earlier phase; the two bail probes follow the custom speculative rule (found -> count lookup+hit; not-found
-> count nothing).

| # | proposed | verified verdict |
|---|---|---|
| 1 | COUNT ON HIT ONLY (see §4 Decision 2) | **Confirmed genuine probe, BUT count rule REFINED by W3X.** Frame N absent at site 1 is the EXPECTED baseline (N is being requested precisely because it has not been produced yet) — not a signal. Frame N *present* is the informative event: under fmParallel a peer thread pre-computed N and the cache delivers it (bubbling win). RULE: **hit -> count lookup + hit; miss -> count NOTHING (not lookup, not miss).** See §4 Decision 2 for the reasoning. |
| 2 | COUNT (l/h/m) | **Confirmed.** Genuine first-time "is N−1 present". |
| 3 | COUNT (l/h/m) — NEW instrumentation; walk-N−1 special-cased (see §4 Decision 1) | **Confirmed**, walk probes are genuine presence demands (each absence changes what happens next: keep walking). Per-walk arithmetic: k misses + 1 hit when an anchor is found; k misses + 0 hits on floor-fresh-start (no anchor in radius). **N−1 SPECIAL CASE (W3X ruling):** the walk's first iteration re-probes N−1, which site 2 already answered as a miss under a *separate* lock. RULE: **at the walk probe where candidate == N−1: count it ONLY if it now HITS (miss->hit flip = a peer thread produced N−1 between the two lock acquisitions = new concurrency info); if it MISSES again, count NOTHING (redundant confirmation of the already-counted site-2 miss).** All other walk probes (N−2, N−3, …) count normally. See §4 Decision 1. |
| 4 | DO NOT COUNT (hole catalogue) | **Confirmed** — and answering your "why is it needed": the walk (3) only finds the anchor; the scan (4) then enumerates which frames between anchor+1..N−1 are absent to build `hole_frame_numbers` (the plan's work list) and validates the present ones' slots. It is plan *derivation*, not a presence *decision* — the branch was already chosen. Agree: not counted. |
| 5 | DO NOT COUNT (anchor re-pin) | **Confirmed with a sharpening:** the anchor pin happens under the SAME lock hold as the walk (one `lock_guard` covers `plan_..._locked` = search + `lookup_frame_and_record_pin_locked(anchor)`), so it is a guaranteed-hit re-pin of a frame located microseconds earlier — the clearest possible "redundant additive count". NOTE: it IS counted today (Q+H) by the committed counters; excluding it is a change to the derivation, handled in the A1/counter redesign, not a code defect. |
| 6 | DO NOT COUNT (re-acquire N) | **Confirmed incl. the no-unpin check:** pins taken at arInitial live in `request_data->pin_list` and are discharged only at activation teardown (`discharge_all`, arAllFramesReady:692); the cache-hit return path asserts `pin_count == 1` still held (arAllFramesReady:1135). Guaranteed hit; re-acquire of an already-pinned frame. |
| 7 | DO NOT COUNT (re-acquire N−1) | **Confirmed**, same pin-lifetime argument (assert at arAllFramesReady:1306). |
| 8 | COUNT PER CUSTOM RULE (bail-early) | **Confirmed feasible and well-defined:** the adopt probe (arAllFramesReady:2035 holes; 1762 floor) is a single `record_pin` whose found/not-found split is exactly your (a)+(b)-on-found / nothing-on-not-found rule. NOTE: today its miss IS counted; the rule change removes that miss from the honest set. |
| 9 | DO NOT COUNT (hole-pred re-acquire) | **Confirmed guaranteed-present:** the hole's predecessor is, by plan construction, either the anchor (pinned at 5) or the previously filled/adopted hole (pinned at 8 or stored-and-pinned at 10a) — always pinned before this re-acquire (sites 2066, 2376). |
| 10 | COUNT PER CUSTOM RULE (bail-before-store, per (ii)) — CORRECTED/SPLIT | There are TWO store-time duplicate-detects... <br> **Inline count mapping (per rule (ii), explicit so it cannot drift):** <br> • found (already present → duplicate → bail, adopt winner) → count lookup (a) + hit (b); NOT miss (c). <br> • not-found (normal → store fresh) → count NOTHING (not a/b/c): speculative non-event. <br> **Intended consequence:** normal store counts nothing → site 10 is silent on clean runs, positive only on concurrency collisions = a pure first-in-best-dressed contention signal (~0 under -r 1). |
| 11 | DO NOT COUNT for hit-rate; bank as race-loss marker | **Confirmed:** fires only on the lost-race path (immediately after the D-SUM-07 `duplicate_computed_but_discarded` observer, arAllFramesReady:1080->1088); guaranteed-hit re-acquire of the winner. Agree: exclude from hit-rate; (10-duplicate, 11, D-SUM-07-discard) together are one precise wasted-compute race event — banked as the thread-contention signal. |

---

## 3. The COMPLETE census (request A)

Method: every `frame_index_.find` in the live tree (12 sites — there are no `.count`/`.contains` uses), plus
every call site of the instrumented primitives (`lookup_frame_and_add_ref*`, `lookup_frame_and_record_pin*`,
`pin_frame*`) outside cache_core, plus the store wrappers' routing. The live store routing is:
`store_production_output_and_prune` / `store_as2_floor_and_prune` / `store_recovery_hole_and_prune` -> all ->
`store_owned_frame_and_prune_impl` -> `store_owned_frame_and_record_pin_locked` (1066, pin-recording stores)
or `store_owned_frame_locked` (1110, plain stores).

**Sites 1–11: as v01 / §2 above** (latest-tree lines: 1=arInitial:913, 2=arInitial:963,
3=cache_core:3607–3631, 4=cache_core:3641–3658, 5=cache_core:3683 via 2533-locked wrapper,
6=arAllFramesReady:1191, 7=:1350, 8=:2035/:1762, 9=:2066/:2376, 10a=cache_core:2832, 10b=cache_core:2710,
11=arAllFramesReady:1088.)

**NEW sites found by the census (12–16) — completing the picture:**

| # | site | enclosing function | purpose (plain english) | disposition on your axis |
|---|---|---|---|---|
| 12 | cache_core:2874, 2921 | `store_owned_frame_and_record_pin_locked` | post-insert re-finds: locate the just-stored slot to pin it / final verification; guaranteed-found, invariant-guarded | **DO NOT COUNT** — internal bookkeeping of a store already decided; not a presence question |
| 13 | cache_core:2971 | `remove_unpinned_frame_locked` | eviction: find the victim frame's slot to remove it | **DO NOT COUNT** — prune bookkeeping; the frame's presence was already established by the prune-candidate selection |
| 14 | cache_core:3032 | `remove_unpinned_frame_locked` | eviction: after swap-remove, re-find the MOVED frame to fix its index entry | **DO NOT COUNT** — pure index maintenance |
| 15 | cache_core:3949 | `unpin_frame_locked` | unpin: find the pinned frame's slot from the pin token | **DO NOT COUNT** — pin bookkeeping; guaranteed-found |
| 16 | cache_core:4084 | `cache_state_invariants_hold_locked` | invariant audit: for EVERY slot, verify the index maps back to it | **DO NOT COUNT** — self-check. **Volume warning:** this runs on essentially every locked cache operation and does O(slots) finds each time; it dwarfs all other raw-find volume. Any future "count raw finds" instrumentation must exclude it or the numbers are meaningless. |

**Out-of-scope but enumerated for completeness:** `cnr3_cache_core_selftest.cpp` contains 58 call sites of
the instrumented primitives. These are synthetic-harness probes; they never run in the plugin process's
production path (selftest binary / selftest entry only) and must stay excluded from any production counter
design. No other file in the live tree (`cnr3_frame_processing.cpp`, `vapoursynth-Cnr3.cpp`, diagnostics
files) touches the cache index or the lookup primitives.

**Census closure statement:** sites 1–16 + the selftest set are the complete population. Every one of the
12 raw `frame_index_.find` sites is accounted for (3607, 3641 = sites 3/4; 3792, 3890 = the two instrumented
primitives' own finds; 2710, 2832 = 10b/10a; 2874, 2921 = 12; 2971, 3032 = 13/14; 3949 = 15; 4084 = 16), and
every instrumented-primitive call site outside cache_core is sites 1/2/5/6/7/8/9/11. Nothing else in the
live tree probes the cache.

---

## 4. Are the new counting rules clear? — assessment

**Yes — the axis is crisp and I could apply it mechanically to all 16 sites.** "Count only a genuine
is-a-frame-I-need-present probe whose answer could be no and changes what happens next; never count
re-acquires of already-pinned frames; bail probes count lookup+hit on found, nothing on not-found" produced
an unambiguous verdict everywhere except two places. Those two are now DECIDED by W3X below. Both decisions
share ONE principle, stated here so it cannot drift when coded:

> **SHARED PRINCIPLE — count the informative outcome, suppress the structurally-certain one.**
> A probe whose result is known in advance (by construction) carries no information and is not counted.
> The same probe is counted only when it produces the outcome that was NOT certain — which, in both cases
> below, is a concurrency event (a peer thread produced a frame we expected to be absent). This keeps the
> numbers meaning what a human reads them to mean, and turns the hit-rate into a bubbling-benefit gauge
> under fmParallel rather than a figure distorted by structurally-guaranteed misses.

**DECISION 1 — the N−1 walk re-probe (site 3, §1.iii). DECIDED: count only on a miss->hit flip.**
- **Rule:** the walk's first iteration re-probes N−1 (the walk's `upper_bound = requested_frame − 1`,
  cache_core:3579/3598). Site 2 already probed N−1 and counted a miss (recovery is entered only *because*
  that fast-path probe missed). At the walk's N−1 probe: **if it HITS, count lookup + hit; if it MISSES,
  count NOTHING.** The walk's other probes (N−2, N−3, …) count normally.
- **Why:** site 2 and the walk are SEPARATE lock acquisitions (verified: the public
  `lookup_frame_and_record_pin` takes its own `lock_guard`; the recovery search re-acquires the mutex). So a
  walk-N−1 MISS just re-confirms the site-2 miss we already counted — redundant, no information, so it is not
  counted (this removes the systematic double-count W3X objected to; on `-r 1` the walk-N−1 always misses, so
  it is never counted — zero distortion on single-thread). A walk-N−1 HIT, however, means a peer thread
  produced N−1 in the gap between the two locks — genuinely new concurrency information — so it IS counted.
  This preserves the concurrency signal precisely, only when it actually occurs, without polluting the common
  case.

**DECISION 2 — site 1's cold miss (frame N itself). DECIDED: count on hit only; a miss counts nothing.**
- **Rule:** at site 1 (lookup of frame N, the frame being requested): **if it HITS, count lookup + hit; if it
  MISSES, count NOTHING (not lookup, not miss).**
- **Why:** frame N being ABSENT at site 1 is the EXPECTED baseline — N is being requested precisely because
  nobody has produced it yet, so a miss here is structurally certain and carries no signal. Frame N being
  PRESENT is the informative event: under fmParallel a peer thread has pre-computed N and the cache now
  delivers it — the cache "making things better." Counting the certain miss would drag a perfectly healthy
  linear run's hit-rate down to ~50% (one mandatory cold miss + one predecessor hit per frame) and mislead a
  human into reading a healthy run as mediocre — the same misleading-headline failure the honest-cache-hit
  work already fixed once. Under this rule the hit-rate reads ~100% on clean single-thread linear (nothing to
  gain, nothing lost) and RISES to show real benefit exactly when fmParallel bubbling delivers pre-computed
  frames — i.e. the number becomes a bubbling-benefit gauge. Note there is no "abnormal cold miss" being
  hidden: in CNR3 a first-produced frame is either genuinely new (expected miss, uncounted) or pre-produced by
  a peer (hit, counted); a backward re-request of an already-produced frame would HIT and be counted. So no
  informative outcome is suppressed.

**Consequence for the headline hit-rate (recorded for A1 and for whoever codes the counters):** with both
decisions applied, `cache_lookup_hit_rate_percent` over the counted set reads ~100% on clean `-r 1` linear
(site 1 misses suppressed; site 2 predecessor hits dominate), and moves ABOVE the predecessor-only baseline
under fmParallel as site-1 and walk-N−1 concurrency hits accrue. The rate is therefore a
"cache-delivered-a-frame-that-would-otherwise-be-recomputed" signal, not a figure diluted by
structurally-guaranteed cold misses.


**One correction to your annotation, for the record:** in v01's §2 you kept the line "the bail-before-store
intuition (ii) already matches the code: … only the winner re-lookup is counted." With the store-find now
identified as 10a/10b, the sharper statement is: the code *performs* your (ii) probe on every store but
counts none of it today; the only counted event near it is the post-race winner re-acquire (11), which your
rules now also exclude. So (ii) is not "already matched" — it is entirely NEW instrumentation, both paths.

---

## 5. Disposition

- This v02 census + rulings table replaces v01 §3 as the reference input for **A1 Q-A §7.7**
  (lookup-accounting reconcile): the tool should reconstruct per-role lookups using the NEW rules and
  reconcile against BOTH the current counters and the proposed scheme, so the migration is checkable.
- **FI candidates (bank):** (1) Tier-3/walk omission (site 3 uncounted); (2) speculative-miss divergence
  (site 8 miss counted today, excluded by new rules); (3) the two uncounted store duplicate-detects
  (10a/10b) as the computed-then-found-duplicate concurrency-cost signal, with `Cnr3Status::duplicate`
  already observable; (4) the (10-duplicate, 11, D-SUM-07-discard) triple as the race-loss marker; (5) the
  site-16 invariant-audit volume warning for any future raw-find instrumentation.
- **Still not a defect** in the committed honest-cache-hit patch; this remains taxonomy + future counter
  design. Any actual counter change is a normal scoped patch through the coder with the usual gates.
- **W3X ruling (v06):** the old uniform lookup counting is **removed, not kept alongside** — one set of
  lookup semantics only. The old L1 lookup oracle (66.664 hit-rate / 7280 misses) is **retired** with it:
  it was correct for the uniform semantics and is void under the intent-counted rules. The counter redesign
  is specified in `CNR3_Patch_Scope_IntentCountedLookups_v2.md`.
- **Rulings status:** both §4 decisions DECIDED; §1.iii reconciled to Decision 1. No open items.
