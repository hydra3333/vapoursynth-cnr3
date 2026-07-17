note, I have added commentary in places, bounded by '===' lines either side of by commentary and within can be preceded by 'me:'.

# CNR3 — Cache Lookup / Hit / Miss Taxonomy — Findings

*Designer (W3D) investigation for W3X. A taxonomy/understanding exercise: enumerate every place the plugin
probes the output cache, classify each as a genuine demand lookup / opportunistic probe / uncounted search
probe, and state which the committed D-SUM-04 `cache_lookup_*` counters actually count. This is NOT a claim
the committed honest-cache-hit counters are wrong — they correctly count every call routed through the two
pinned lookup primitives, which is one valid definition. The question is whether a finer definition would
aid comprehension. Verify anything below cold against `src/`.*

**Source baseline:** traced against the committed tree; call sites cited by file:line. The instrumentation
sites (query++/hit++) are the two locked primitives established by `CMS07-DIAG.honest-cache-hit-metrics`
(`lookup_frame_and_add_ref_locked` and `pin_frame_locked`); `lookup_frame_and_record_pin_locked` and the
public wrappers delegate to those, so anything routed through them is counted, and any raw `frame_index_.find`
is not.

---

## 1. Direct answers to the arInitial questions

**(1) frame N probe** — `lookup_frame_and_record_pin(n)` at `cnr3_arInitial.cpp:913`. Genuine demand lookup.
**Counted.** Hit if N already present (-> cache-hit-return branch); miss otherwise.

**(2) predecessor N−1 fast-path probe** — `lookup_frame_and_record_pin(n-1)` at `cnr3_arInitial.cpp:962`.
Genuine availability check. **Counted.** Hit if N−1 present (-> predecessor-present-compute branch); miss
otherwise (-> routes to recovery).

**(3) anchor search** — splits into two parts with *different* counting status:
- the **backward walk** that probes N−1, N−2, … to find the nearest present anchor, plus the
  **hole-catalogue scan** from anchor+1..N−1, both use **raw `frame_index_.find()`**
  (`plan_bounded_recovery_search_locked`, `cnr3_cache_core.cpp:3598–3649`) — **not counted**;
- only the final **anchor pin** (`lookup_frame_and_record_pin(anchor)`,
  `plan_bounded_recovery_search_and_record_anchor_pin_locked`, `cnr3_cache_core.cpp:3674`) is **counted**
  (query + hit; the anchor is present by construction).

===
me:
OK the above appears to contains an anopmaly compared to what is now desired.
It seems (1) counts lookup/hit/miss ? if yes, ok good.
It seems (2) counts lookup/hit/miss ? if yes, ok good.
It seems (3) may be an anomaly;
(i) the backward search should count lookups/hits/misses
(ii) I am unsure why a 'hole-catalogue scan' is needed, even so it should not count lookups/hits/misses
===

### N−1 double-probe verdict
N−1 is physically probed **twice**: once at (2) — counted — and again as the **first iteration of the
backward walk** (the walk's `upper_bound = requested_frame − 1 = N−1`, `cnr3_cache_core.cpp:3577–3580`,
first probed at `:3598`) — **uncounted**. Therefore it does **not** double-count the D-SUM-04 stats; only the
fast-path probe is counted. It is a benign **redundant physical probe**, a structural consequence of the
fast-path and recovery being separate steps — not a deliberate double-count and not a stats distortion.

===
me:
if N-1 is known to be missing from the cache and the backward walk commences;
(i) is this all done within the one mutex ?
(ii) if so, why start the backward search at N-1 rather than N-2 (also taking care of N-1 and N-2 not existing near the start of clip) ?
(iii) how can we not double-count the stat for frame N-1 and also N-1 currently in the backward walk ?
===

---

## 2. The real-vs-speculative distinction: it holds, but there is a third tier

The proposed two-tier model (real demand lookups vs speculative bail probes) is sound as a design
preference, but the code actually has **three** tiers, and the committed counter spans only the first two:

- **Tier 1 — genuine demand (counted).** The plugin needs a *specific* frame and asks for it. Hit/miss is
  meaningful (several are hits by construction because the frame was just pinned or produced).
- **Tier 2 — opportunistic, counted.** The pre-compute adopt / bail-early probe. Found -> adopt-skip (hit);
  not-found -> proceed to compute. Its **miss is counted today** — precisely the "speculative miss" the
  proposal would exclude. This is a **real divergence** between the proposal and current behaviour.
- **Tier 3 — uncounted raw-find.** The recovery backward walk, the hole-catalogue scan, and the
  store-collision check. **Counted nowhere.** This is the bulk of the "searching for a frame" activity under
  recovery, and it is invisible to the hit-rate.

===
me:
The 'genuine demand' is being redefined by me here, by examining every instance you find
and deciding what will and will not be counted on the basis of what appears 'intuitive'
to a human.
Yes Teir 2 diverges to what I now evaluate to be 'real' stats per:
...
For this commentary, let us define (a) a cache lookup (b) a cache hit and (c) a cache miss.
The bail-early/bail-before-store checks in (i)/(ii) below are opportunistic optimisation
probes — the frame's absence there is not a real "miss" because they are transitory checks
only designed for limiting cpu spend on calculating/storing.
For example if these things occur in in arallframesready (for you to confirm), then they
should be treated as follows:
(i) lookup just prior to calculating a frame to see if bail-early is viable ...
    I suggest if a frame is found to enable a bail-early, it counts as (a) and (b) but not (c)
    but if the cache frame is not found (and a recalc then occurs) then it does not count as any of (a) (b) (c)
    because I define it as a speculative miss.
(ii) just after calculating a frame and then a lookup to see if bail-before-store is viable ...
    I suggest if a frame is found to force a bail-before-store, it counts as (a) and (b) but not (c)
    but the lookup cache frame is not found (after the recalc occured) it does not count as any of (a) (b) (c)
    because it is a speculative miss.
...

===


Two consequences for the proposal:
- The **bail-before-store** intuition (ii) already matches the code: the collision detect is Tier 3
  (uncounted); only the winner re-lookup that follows a lost race is counted, as a hit — no spurious miss.
- The **anchor-search** intuition (3) does **not** match: those probes were treated as real (a)(b)(c) but
  are Tier 3 — counted nowhere. That is the surprise of this investigation.

---

## 3. Full enumeration — every cache-probe site

Q = counted as a lookup query; H = counted as a hit; M = counted as a miss.

| # | phase | site (file:line) | plain-english purpose | primitive | counted? |
|---|---|---|---|---|---|
| 1 | arInitial | cnr3_arInitial.cpp:913 | is frame N itself present? -> cache-hit-return | record_pin(N) | **Q**; H if present, else **M** |
| 2 | arInitial | cnr3_arInitial.cpp:962 | is predecessor N−1 present? (fast path) | record_pin(N−1) | **Q**; H if present, else **M** |
| 3 | arInitial (recovery search) | cnr3_cache_core.cpp:3598–3622 | anchor backward walk N−1, N−2, … | **raw find** | **not counted** |
| 4 | arInitial (recovery search) | cnr3_cache_core.cpp:3632–3649 | hole-catalogue scan anchor+1..N−1 | **raw find** | **not counted** |
| 5 | arInitial (recovery) | cnr3_cache_core.cpp:3674 | pin the found anchor | record_pin(anchor) | **Q + H** |
| 6 | arAllFramesReady | cnr3_arAllFramesReady.cpp:1191 | re-acquire N to return (cache-hit path) | add_ref(N) | **Q + H** |
| 7 | arAllFramesReady | cnr3_arAllFramesReady.cpp:1350 | re-acquire pinned predecessor to compute | add_ref(N−1) | **Q + H** |
| 8 | arAllFramesReady | cnr3_arAllFramesReady.cpp:2035, 1762 | hole **adopt / bail-early** (pre-compute) | record_pin(hole) | **Q**; H if adopted, else **M -> compute** |
| 9 | arAllFramesReady | cnr3_arAllFramesReady.cpp:2066, 2376 | re-acquire hole predecessor to compute | add_ref(pred) | **Q + H** |
| 10 | arAllFramesReady (store) | inside store_*_locked | **bail-before-store** collision detect | raw find / emplace | **not counted** |
| 11 | arAllFramesReady | cnr3_arAllFramesReady.cpp:1088 | race-loser: re-lookup winner to return | add_ref(N) | **Q + H** |

**Tier 1 (genuine demand, counted):** 1, 2, 5, 6, 7, 9, 11.
**Tier 2 (opportunistic, counted):** 8.
**Tier 3 (uncounted raw-find):** 3, 4, 10.

===
me:
For this commentary, let us define (a) a cache lookup (b) a cache hit and (c) a cache miss and
refer to my (i) and (ii) above where I defined specific counting rules for those two circumstances
which must be used when counting (or not) for them.

We have sites 1–11, BUT (important) these are only the lookups you happened to route past — you have NOT
confirmed this is the COMPLETE census, and it already has a gap: the per-store duplicate-detect find in
store_owned_frame_locked (cnr3_cache_core.cpp:2702) is a REAL cache lookup that runs on EVERY store and is
not given its own line (it was folded into "raw find / not counted"). So before we finalise a counter design
I need the full census — see (A) at the end.

Proposed disposition per site (for you to confirm cold against source, not merely accept). My axis is: COUNT
only a genuine "is a frame I need present, where the answer could be no and changes what happens next" probe;
do NOT count a re-acquire/re-pin of a frame already found-and-pinned in an earlier phase (counting that is a
redundant additive count of a re-pin as a lookup); apply my custom speculative rule to the two bail probes.

 1 = COUNT (lookup/hit/miss) — genuine first-time "is N present?" demand.
 2 = COUNT (lookup/hit/miss) — genuine first-time "is N-1 present?" demand (fast path).
 3 = COUNT (lookup/hit/miss) — the anchor backward-search probes are genuine "is this frame present?"
     demands. NOTE: currently raw-find / UNCOUNTED, so counting these is NEW instrumentation, not a
     reclassification. This is the big invisible bulk under recovery.
 4 = DO NOT COUNT — internal hole-catalogue derivation (deciding WHICH frames are holes); not a presence
     decision probe.
 5 = DO NOT COUNT — re-pin of the anchor already located in (3); a redundant additive count of a re-pin.
 6 = DO NOT COUNT — re-acquire of frame N already found & PINNED at (1); guaranteed hit; redundant additive
     count of a re-pin. (Confirm: no unpin between (1) and here.)
 7 = DO NOT COUNT — re-acquire of predecessor N-1 already found & PINNED at (2); guaranteed hit; redundant
     additive count of a re-pin. (Confirm: no unpin between (2) and here.)
 8 = COUNT PER CUSTOM RULE (check-BEFORE-compute bail-early): lookup counts (a); found -> adopt-skip, count
     lookup and hit (b); not-found -> compute anyway, do NOT count a miss (speculative — absence is not a real miss).
 9 = DO NOT COUNT — re-acquire of a hole predecessor already found/pinned during recovery; guaranteed hit;
     redundant additive count of a re-pin. (Confirm: guaranteed present here.)
10 = *** CORRECTED / SPLIT ***  There is NO separate "pre-store" cache lookup. The check-AFTER-compute occurs
     as the frame_index_.find INSIDE store_owned_frame_locked (cnr3_cache_core.cpp:2702), which runs on every
     store. This IS my bail-before-store lookup (ii). COUNT PER CUSTOM RULE: lookup counts (a); found (the
     store returns 'duplicate' = someone already stored N) -> bail-before-store, count lookup and hit (b); not-found ->
     store my freshly-computed frame, do NOT count a miss (speculative — absence just means "store it after
     all", not a cache failure). It is currently a raw find / UNCOUNTED, so counting it is NEW instrumentation.
11 = DO NOT COUNT for the hit-rate — this fires ONLY when (10) returned 'duplicate' (I lost first-in-best-
     dressed): I discard my computed loser and re-acquire the WINNER's copy to return. Guaranteed hit (the
     winner just won); redundant additive count of a re-pin. NB it is distinct from (10): (10) is the per-store
     presence check; (11) is the cleanup re-acquire that happens only on a lost race. SEPARATELY: (11) together
     with D-SUM-07 duplicate_computed_but_discarded is a precise marker of a race loss / wasted compute — bank
     that as a candidate thread-contention signal, NOT part of the cache hit-rate.

WHY the after-compute store-check (10) must be counted — the reasoning, so the design is intentional:
   A frame arrives for compute because (1)/(2)/(3) already said it was NOT in the cache. But CNR3 runs
   frames concurrently (no -r 1), so between "not present at arInitial" and "finished computing", ANOTHER
   thread may have computed and stored the same frame N. The store-time find at 2702 is the plugin's LAST and
   ONLY check of whether that happened. If it did (duplicate/hit), we throw away our now-redundant compute and
   adopt the winner (that is real wasted work, worth seeing). If it did not (not-present), we store ours.
   So (10) is the counter that measures how often concurrency caused a frame to be computed that turned out to
   already exist — i.e. the first-in-best-dressed collision rate from the STORING side. Without counting (10),
   the hit-rate is blind to the entire "computed-then-found-duplicate" phenomenon, which is precisely the
   thread-contention cost we want visible when -r 1 comes off. That is why it must be counted (lookup + hit on
   duplicate), while its not-present outcome stays a non-miss per my speculative rule.

(A) COMPLETENESS (required before we settle the counter design): sites 1–11 plus the store find at 2702 are
    what we have SO FAR. Please now enumerate EVERY remaining cache lookup / frame_index_.find in the code that
    is not yet on this list — in store/prune/eviction/checkpoint/hot-zone/selftest or anywhere a frame is
    probed — each with its file:line, plain-english purpose, primitive (record_pin / add_ref / raw find), and a
    proposed disposition on the same axis. The 2702 store-find omission shows the current list is incomplete; I
    want the full census before deciding what the honest counters should be.
===

---

## 4. What the committed hit-rate actually measures

`cache_lookup_hit_rate_percent = hits / queries` over **Tiers 1 + 2 only**. It blends demand-success with a
half-counted slice of speculative activity (the Tier-2 adopt miss) and is **blind to the Tier-3 recovery
search walk**. That is why the number is hard to interpret by context:

- **L1 (linear, no recovery):** only Tier 1 fires -> per linear frame, 2 hits + 1 mandatory-cold-N miss ->
  ~66.6%. Verified against the raw counts: queries 21838 = 3 × 7279 + 1 (frame 0), decomposing exactly as
  site 1 (miss) + site 2 (hit) + site 7 (hit) per frame N ≥ 1, plus frame 0's lone site-1 miss. The taxonomy
  closes to the frame.
- **L2 / L1noR (recovery-heavy):** the Tier-3 walk fires heavily but never shows; Tier-2 adopt misses do
  show. So the number reflects demand + an arbitrary subset of speculation, **not** search effort.

The committed counter is not wrong — it is one honest definition ("every call through the pinned
primitives"). But it is **uniform over an incomplete set**: it silently omits Tier 3. That is the central
finding.

===
me:
Acknowledged, however clearer-for-humans measures measures are being considered and proposed here.
===

---

## 5. Recommendation

A finer definition would aid comprehension. Split the single counter into per-role counters:
- **(a) demand lookups** — frame N, predecessor, anchor pin, and the compute re-acquires (Tiers 1);
- **(b) opportunistic probes** — adopt / bail-early and store-collision, where absence is not a real miss
  because the plugin proceeds anyway (Tier 2 + the currently-uncounted store collision);
- **(c) search-walk probes** — the recovery backward walk and hole scan (Tier 3), currently invisible.

With those separated, the hit-rate becomes readable per context instead of a blend, and the recovery search
cost becomes visible.

===
me:
1. thank you for the recommendation.
2. the proposed count-or-not as outlined by me is intended to make the numbers and percentages intuitively
   clearer to a human reading them cold — a hit means "a frame I needed was there", a miss means "a frame I
   needed was absent", and neither is polluted by re-pins of already-found frames or by speculative probes.
3. we STILL need the complete census: sites 1–11 AND the store-find at 2702 AND every other currently-
   uncounted lookup in the code, each classified count / do-not-count on the axis above. The 2702 omission is
   proof the current enumeration is not yet complete — please close that gap before we design the counters.
===

**Disposition:**
- This table is the reference input for the **A1 tool Q-A "lookup-accounting reconcile"** query (spec §7.7),
  which reconstructs exactly these per-role lookups from plan structure and reconciles them against the
  D-SUM-04 counters.
- Two items to bank in the **FI ledger**: (1) Tier-3 omission — the recovery search walk and hole scan are
  counted nowhere; (2) Tier-2 speculative-miss — the pre-compute adopt miss is counted as a real miss today.
  Both are candidates for the finer-definition refinement above.
- **Not** a defect in the committed honest-cache-hit patch. No change is proposed to it here; this is a
  taxonomy and a future-refinement question.
