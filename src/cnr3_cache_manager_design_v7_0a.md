# CNR3 Cache Manager Design Specification

**Date:** 2026-06-12
**Version:** CMS07.0a (working draft increment of CMS07.0)
**Status:** Design specification — architectural supersession. DRAFT assembled
from the settled-decision capture; sections depending on unresolved verify-items
are marked `[VERIFY]`. V1 resolved (causal, confirmed against CNR2). Recovery
model, dissolved source window, and scene-cut handling locked in this increment.
Ready for coder study and the isolated cache-core milestone.
**Supersedes:** CMS07.0 supersedes CMS06.11 and ALL earlier CMS06.x / CMS0x cache
designs. This is an architectural supersession, not a refinement: the pinning model
and the hot-zone model both change fundamentally. Earlier documents are reference
material only.

---

## 0. How to read this document (and what changed)

CMS07.0 replaces the previous cache architecture. The two headline changes:

1. **Pinning is now the mandatory correctness mechanism.** In the old design
   (CMS06.11 and earlier), actively-needed frames were kept findable by a
   combination of held references plus hot-zone heuristics, and non-checkpoint
   pinning was a *deferred emergency escalation*. That is superseded. In CMS07.0,
   any frame a request actively needs is **pinned** by that request (a
   consumer-held pin), and the cache is the **complete liveness index** for the
   active set.

2. **Hot zones are demoted to prune-policy hints.** They no longer guarantee
   findability of active frames (pins do that). They protect the *anticipatory /
   decaying* set — frames likely-to-be-needed-soon or recently-needed-but-not-yet-
   cold — which pins structurally cannot protect (a pin needs a present consumer).

**You must not conflate the old and new concepts.** The three highest-risk traps:
- treating pinning as optional/deferred — it is now baseline;
- reintroducing held-ref-only predecessor reservation — superseded by consumer-pins;
- thinking of a checkpoint as a kind of pin — a checkpoint is a separate
  eviction-protection FLAG with its own retention rule; there is exactly ONE pin
  concept (consumer-claim).

**Design vs code:** CMS07.0 is a design supersession. The existing CODE is at the
prior (CMS06.11 / sequential-fast-path-through-H15.5) state. Development restarts
against this spec; old source is reference material for verifiable salvage only.

---

## 1. Project context (orientation — adapted from handover Document A §A2/§A3/§A4)

CNR3 is a VapourSynth **API4-only**, **integer-YUV-only** recursive temporal chroma
stabiliser, inspired by the CNR2/vscnr2 algorithm, intended for analogue / video-
capture material (chroma shimmer, temporal chroma noise). The final operational
target is **fmParallel** (Section 2).

**The load-bearing fact:** `output[N]` depends on `source[N]` and on the
*already-filtered* `output[N-1]` — not on `source[N-1]`. This recursion is why a
naive stateless filter cannot work.

**Algorithmic core (§A4 — V1 RESOLVED: confirmed causal by Dave against CNR2):**
- For luma Y: copy source luma unchanged.
- For chroma U/V: compare current source chroma vs previous *filtered* chroma, and
  current downsampled luma vs previous downsampled luma; use signed-difference
  response tables; blend
  `output_chroma[N] = blend(output_chroma[N-1], source_chroma[N])` by the combined
  response.
- **Scene-change (cut) detection** runs *during compute* as part of comparing frame
  N to its predecessor. On a true cut at N, output[N] is computed as a **fresh start**
  (copy source chroma, skip the recursive blend), so output[N] does NOT depend on
  output[N-1]. A cut therefore SEVERS the recursive chain at N. (This is a third
  reset point alongside frame 0 and the lower-bound floor; see Section 9.5/9.6.)
- The algorithm is **purely causal**: computing output N uses only the current
  source (N) and the previous filtered output (N-1). There is NO forward-source peek
  / look-ahead / bilateral reach. The source reach to compute N is therefore
  **backward-only**. (V1 confirmed against CNR2 itself; corroborated by §A4, by the
  proven request window in CMS06.11, and by the absence of any forward reference.)

**Why the cache exists (correctness, not performance):** a modern VapourSynth graph
may request output frames out of display order (e.g. 0, 3, 1, 2, 4). To compute
output 3 the filter needs filtered output 2, which may not yet exist. The cache
retains computed outputs so the recursion can find predecessors (or recover them)
rather than rebuilding the chain from frame 0 on every request. **The cache is a
correctness subsystem.**

---

## 2. Operational goal and the fmParallel invariant (stated outright — from §A8)

```text
safe under fmUnordered now
structurally compatible with fmParallelRequests
final design target: fmParallel
```

**Invariant (top-level, binding):** fmParallel is the final operational target.
Interim development may pass through fmUnordered and fmParallelRequests, but design
and coding MUST NOT make choices that block eventual safe fmParallel operation
unless explicitly justified and recorded. This invariant is the justification for
adopting Position B (Section 3) over the simpler held-ref-only model.

`old_strict_cache.next_needed` and `old_strict_cache.prev_output` are NOT final
fmParallel output authority and must be retired/bypassed/redesigned before final
authority (carried forward from decisions D26/D33). The restart is the occasion to
retire the old strict-streaming bridge.

---

## 3. Foundational model — the cache holds references, Position B mandatory

**3.1 The cache holds references, not pixels.** A cache slot holds an
`addFrameRef` reference to a VapourSynth-core-owned `VSFrame`; it does not own
pixel memory. The core reclaims a frame when its atomic reference count reaches
zero. `addFrameRef`/`freeFrame` are atomic/thread-safe on R76 API4 `[VERIFY V5]`.

**3.2 Two leak surfaces.**
- (a) The per-request `frameData` struct — plain heap, allocated and freed by US.
  The core carries the *pointer* between activations but never owns the struct.
- (b) `VSFrame` references — core-owned memory; our obligation is ref discipline
  (`freeFrame` every ref we take), never deallocation.
- **Destruction ordering (mandatory):** on `frameData` destruction, first discharge
  every ref the struct tracks (walk the pin-list; unpin / `freeFrame`), THEN free
  the struct. Freeing the struct first loses the owed-ref list → leak.

**3.3 Position B — mandatory architecture.** Slot and frame operate as a managed
unit; the cache is the complete liveness index for the active set. A frame that is
alive-but-unfindable is, for the cache's purpose, equivalent to one that does not
exist (recompute either way) — so completeness of the index IS searchability for
our goals.
- **Position A (held-ref-only)** is the proven correctness floor and may be used
  ONLY in a specific, named circumstance where B is demonstrated unnecessary or
  unworkable, and ONLY by explicit per-case agreement, documented. The default is
  B; A is the documented exception, never a silent fallback. A B/A mix is rejected.
- **Justification:** (i) goal-driven — under fmParallel, predecessors are the most-
  shared frames, so alive-but-unfindable becomes a frequent recompute tax; (ii)
  conjectural on the performance margin (unmeasurable until testing — instrument and
  confirm); (iii) KISS / maintainability — B is ONE mental model (cache = liveness
  truth; slot+frame are one unit), whereas A forces every contributor to carry the
  decoupling forever, a comprehension tax that erodes and reintroduces bugs. B holds
  even if the performance margin proves small.
- **This supersedes decision D13** ("non-checkpoint pinning deferred"). See §12.

**3.4 Direction of causation.** The output side drives: downstream requests output
N; the filter then requests the source frame(s) it needs. Demand flows
output → filter → source. The input clip is passive. VS-LIFECYCLE-01 (only retrieve
in arAllFramesReady what was requested in arInitial) is a contract with the engine's
source handling. The filter NEVER requests its own output frames from VapourSynth —
outputs exist only when computed; the cache is their sole retainer.

**3.5 Per-instance, per-invocation (D03/D16).** The cache is per filter instance,
never global. Per-request planning/ownership state (source plan, pin-list) is
carried in per-invocation `frameData`, never in shared `Cnr3Data` state.

---

## 4. Pinning scheme (the correctness mechanism)

**4.1 Consumer-pins principle.** A pin represents a consumer's active, in-flight
need. The consumer takes the pin; the consumer releases it. **Production never
pins** (producing output N is not consuming it; nobody consumes N yet — its future
consumer N+1 will pin it when it arrives). No pin without a present consumer. Pin
ownership is unambiguous and self-balancing per request: the thread that pinned
unpins, within its own arInitial → arAllFramesReady lifetime.

**4.2 No pre-anticipation pinning.** A speculative pin by a producer on behalf of a
future, possibly-never-arriving consumer creates ambiguous ownership. Rejected. The
anticipatory case is the hot zone's job (Section 5), not a pin's.

**4.3 Pin-and-record inside the atomic.** Every pin taken is appended to the
request's private per-invocation `frameData` pin-list, INSIDE the same atomic as the
pin (pin and record are indivisible). The pin-list is private per request → no
cross-thread contention on it.

**4.4 Single-ownership / null-on-consume.** A reserved/owned ref or pin entry is
owned by `frameData` until consumed:
- successful consumption releases (or transfers) the ref exactly once, then sets the
  field/entry to nullptr/consumed;
- cleanup releases an entry only if it is still non-null;
- the null field is the idempotency guard; exactly one of {consume, cleanup}
  releases any given entry;
- consume and cleanup attribute counters correctly (cleanup increments the cleanup
  counter; consume increments the consume counter — never both for one entry).

**4.5 Final unpin.** At end of arAllFramesReady, unpin the entire recorded pin-list
in ONE short atomic (count decrements, marshalled inside the lock). Hold-to-end,
uniform, once. Mid-walk unpinning is a known-available future optimisation,
deliberately NOT used (avoids variable intra-request pin lifetimes).

**4.6 Leak-safety.** `frameData` cleanup/destructor must unpin any pins still on the
list on EVERY exit path including errors. Makes leaked pins structurally hard.

**4.7 One pin concept.** "Pin" = consumer-claim only. There is no "policy-pin." (See
Section 6 for checkpoints, which are a separate flag, not a pin.)

**4.8 Ownership accounting (carried forward from D05/D06/D55).** Pin/owned refs
participate in the existing lookup-ref accounting:
`lookup_owned_ref_acquired_total == released_total + transferred_total`. A consumed
predecessor/intermediate is an INPUT → **released, not transferred**; only the
returned output frame N is transferred to VapourSynth.

---

## 5. Hot-zone scheme — decay-hints over the pinned liveness floor

**5.1 Role (demoted from the old design).** Hot zones do NOT guarantee findability
of active frames — pins do. Zones protect the **anticipatory / decaying** set:
frames no current request has claimed but which are likely-needed-soon (the
store→next-claim handoff) or recently-needed-but-not-yet-cold (the dwindling-zone
graceful-decay window). This is orthogonal to pinning, not a substitute for it.

**5.2 B de-risks the zone scheme.** Because pins guarantee the active set
independently, the zone's failures (premature prune, merge churn, zone-limit churn)
cost EFFICIENCY (a recompute), never CORRECTNESS. The old scheme's soft spots become
harmless inefficiencies.

**5.3 Zone definition.** A zone is a `[low, high]` window tracking where recent
activity has been, carrying `last_observed_frame`. Frames inside a live zone are
prune-DEFERRED (a preference); frames outside all zones are prune-ELIGIBLE.

**5.4 Slide / spawn / merge (machinery carried forward from D08, revalidated).**
- Slide rule: for an arriving frame F, find the nearest active zone within
  `JUMP_THRESHOLD`; if found, slide it so `low = max(0, F - BACK_RADIUS)`,
  `high = F + FORWARD_RADIUS`, update `last_observed_frame`. Else it is a jump →
  allocate a new zone (or, if no free slot and none retire-eligible, merge the two
  closest zones conservatively: `min(lows), max(highs)`).
- Sliding is safe because correctness rests on pins, not on the zone covering the
  predecessor.

**5.5 Decay sequence (makes dwindling-zone pruning safe; prune after criteria):**
active (pins in range OR recently observed) → dwindling (no new observation, pins
clearing) → retire-eligible (NO pins in range AND `decay_margin` frames elapsed
since `last_observed_frame`) → retired (anticipatory protection withdrawn) → frames
prune-eligible → pruned by capacity pressure, furthest-from-zone first. Safe at every
step because pins underwrite correctness.

**5.6 Retirement test is EXACT and cheap.** "No pins in range" is answerable in the
same locked prune pass from the pin-count state already maintained — NO parallel
`active_request_count` counter needed. Checkpoints do NOT keep a zone alive (they
have their own separate retention rule). This eliminates the old conservative
"no pinned checkpoint" proxy and its merge-ratchet tendency.

**5.7 Hot-zone update at arInitial (carried forward from D15).** Zone activity is
registered at arInitial, not deferred to arAllFramesReady — required for safety once
multiple requests are in flight.

---

## 6. Checkpoints — a flag with its own retention rule (NOT a pin)

**6.1 A checkpoint is a FLAG, eviction-protected by a separate retention rule.** A
checkpoint with no active consumer is NOT pinned. The three independent eviction
protections, each meaning one thing: `pin_count > 0` (active consumer need),
checkpoint flag (recovery-anchor retention), hot-zone membership (anticipated need).
Only the first is a pin.

**6.2 Hard floors apply to both ordinary and checkpoint frames.** A pinned frame is
NEVER evicted; a hot-zone frame is spared. Checkpoint-ness never overrides a pin; a
pin never overrides checkpoint protection. ANDed.

**6.3 Establishment and retention (DISTINCT rules; `[VERIFY V2/V3]`):**
- Establishment: promote every `CHECKPOINT_INTERVAL`-th frame (and frame 0) to the
  checkpoint pool — the regular grid. **In addition, promote every detected
  scene-change (cut) frame to the checkpoint pool** (Section 6.4). The grid promotion
  is RETAINED alongside cut promotion (regular anchors for long static scenes + exact
  anchors at cuts).
- **Supplementary rule — checkpoints may be IRREGULARLY spaced** because of scene-cut
  promotion. This is safe: the checkpoint search is position-agnostic (it finds the
  greatest checkpoint frame ≤ the search bound via the ordered frame-number map, D04),
  making no assumption of regular spacing. The only place INTERVAL appears beyond
  establishment is the capacity sizing estimate (CR5), which becomes a density FLOOR,
  not a ceiling (cuts add density on top of the grid).
- Retention: count-based soft trigger — checkpoint prune RUNS when the pool exceeds
  `CHECKPOINT_MAX_RETAIN`, prunes toward `CHECKPOINT_MIN_RETAIN`. A checkpoint is a
  prune candidate iff frame ≠ 0 AND `pin_count == 0` AND outside every hot zone.
  Evict greatest-hot-zone-distance first. Retain limits are SOFT triggers (a
  hot-zone or pinned checkpoint is retained past MAX_RETAIN). Frame 0 never pruned.
- "Kept longer but bounded": pool lives between MIN_RETAIN and MAX_RETAIN; does not
  grow with clip length.
- Under fmParallel scatter, count-trigger + hot-zone-distance ordering (not a
  distance-from-a-single-front metric, which is ill-defined under scatter).

**6.4 Scene-change frames as checkpoints.** A cut detected during compute (Section
9.2) makes output[K] a fresh-start, dependency-free reset — the IDEAL recovery anchor
(exact, not approximate, and nothing before it is needed). Therefore output[K] is
stored with the checkpoint flag set. No special search or recovery handling is needed:
a cut-checkpoint is just a present output the Phase-1 descending search (9.5) finds
like any other, and its longer retention makes it more likely to survive in a cold
region. Cuts-as-checkpoints absorbs the previously-considered "remember cut positions"
optimisation. **Sizing caveat:** checkpoint density is now "grid + cuts," so in
cut-heavy content the pool runs nearer MAX_RETAIN; the soft-trigger retention sheds the
unprotected (cold, out-of-zone) cut-checkpoints first, which are exactly the ones safe
to drop. A cut-checkpoint inside a hot zone or pinned is retained regardless (it should
never be evicted while protected — instrument for that as an invariant).

---

## 7. Eviction predicate and bounded prune

**7.1 Composite eviction predicate (read atomically under the single lock):**
```text
evict(slot) iff
    slot.pin_count == 0
    AND slot.frame_number is outside every live hot zone [low, high]
    AND ( slot.is_checkpoint
            ? checkpoint-retention-permits (Section 6.3)
            : capacity-permits (Section 7.2) )
ordering: evict greatest-distance-from-any-hot-zone-boundary first.
```
Zone retirement (5.5/5.6) flips a zone inactive so its range stops contributing to
"outside every live hot zone."

**7.2 Capacity trigger (carried forward from D10/D11, self-debouncing).**
Non-checkpoint prune fires when the pool strictly exceeds
`active_ceiling × OVERFLOW_FACTOR`; prunes back toward `active_ceiling`. The
capacity→overflow gap IS the hysteresis (won't re-fire until the pool climbs back
over threshold). No percentage marks, no periodic timers. Store prunes before
hard-ceiling rejection (D11).

**7.3 Bounded prune — three acts (composes the decide/detach/free separation).**
- (a) DECIDE: under the lock, evaluate the eviction predicate, select up to **K**
  victims (greatest-distance-first).
- (b) DETACH: under the SAME lock, detach each selected slot from the cache index
  via the single central remove helper (D09), collecting its `VSFrame*` ref into a
  local list. (a)+(b) are one critical section — a pin cannot creep in between select
  and detach (the slot leaves the index the moment selected; a pinned frame is never
  selected).
- (c) FREE: release the lock; then `freeFrame` the collected refs in one post-lock
  BATCH (freeFrame may trigger core deallocation — kept outside the lock).
- **K** caps the lock-hold for BURST prunes (post-seek, cold-start); rarely binds in
  steady trickle. Instrumentation: count "prune stopped at K with pool still over
  threshold"; raise K if it fires regularly.

---

## 8. Locking — one cache-wide lock, held minimally

**8.1 Decision.** A single mutex guards all cache state (slot index, both pools,
zones, checkpoint flags, pin-counts). Correct-by-construction; the single consistent
snapshot that region-scoped search-and-pin and the composite eviction predicate both
require is FREE under one lock; no lock-ordering, no deadlock; one mental model.

**8.2 Discipline.** Do ALL the right in-memory cache operations inside the lock and
no more — without being stingy. Slow work goes OUTSIDE the lock.
- INSIDE (indivisible commits): search; pin; hole-catalogue; store; pin-record;
  final unpin; eviction-predicate read; prune decide+detach.
- OUTSIDE (never serialises threads through the cache mutex): pixel computation;
  VapourSynth source requests; `freeFrame` of evicted/duplicate refs.

**8.3 Combined principle:** pin-and-record the plan INSIDE the lock; execute the slow
parts OUTSIDE it. The inside-lock pinning is what makes outside-lock execution safe.

**8.4 Fine-grained / cross-region locks: REJECTED as baseline.** They reintroduce
the "separated things humans fail at" problem at the locking layer, threaten the
snapshot property the pin scheme depends on, and add deadlock risk. Deferred to a
MEASURED optimisation applied only if instrumentation proves the single lock is the
actual bottleneck. Fine-graining attacks a load-bearing assumption → requires strong
evidence.

**8.5 Mutex-scoping audit (coder task).** Enumerate every cache operation that
checks-then-acts and confirm none drops the lock between check and act:
search-identify-and-pin; store-on-compute-complete (+ first-in-best-dressed); bounded
prune (decide+detach / free); hot-zone slide; zone retirement; checkpoint establish;
final unpin. PLUS a broad open ask: identify any scenario not enumerated here where
cache state is checked-then-acted across a lock boundary, given this caching goal.

---

## 9. Request lifecycle with atomic scope

**9.1 arInitial (one atomic + slow work outside).**
- INSIDE one atomic: determine the recovery plan for N (Section 9.5) — i.e.
  Phase-1 descending search for the start point, pinning the start point and any
  present cached outputs that will be reused, and cataloguing the OUTPUT HOLES that
  must be computed; record all pins to the `frameData` pin-list; update hot zone(s)
  for N (D15). One indivisible operation. The backward search is BOUNDED — it must
  bound the search interval `[max(0, N-B), N]` itself, not find a global nearest
  prior anchor and reject it afterward (carried forward from CMS06.11 §4.5.3 /
  `prepare_bounded_recovery_plan()`).
- OUTSIDE the atomic: request from VapourSynth the SOURCE frames needed — **source N
  plus the sources for the genuine output holes only** (Section 9.5.1 — the dissolved
  source window). NOT a blanket `[max(0,N-B), N]` source window. Slow (may trigger
  upstream decode) — never inside the lock.
- Safe because present/reused frames (and the start point) are pinned (protected) and
  holes are absent (cannot be pruned). The plan snapshot stays valid after lock
  release.

**9.2 arAllFramesReady (compute outside, per-frame store-and-pin atomic, end-unpin).**
- The WHOLE activation is the single overarching consumer for the hole-filling pass.
- For each output hole, in ASCENDING frame-number order from the start point forward:
  compute the output OUTSIDE the lock; then a brief per-frame atomic
  { first-in-best-dressed check; store; pin; record }; unlock; proceed. Store-and-pin
  MUST be the same atomic (else a gap lets another thread prune the just-stored frame).
- **Scene-change detection runs during each compute** (Section 1, §A4): if a cut is
  detected at hole K, output[K] is computed as a fresh start (copy source chroma,
  skip the recursive blend) and stored WITH the checkpoint flag set (Section 6.4);
  otherwise output[K] = blend(output[K-1], source[K]). Either way it is store-and-
  pinned and the ascending walk continues. A cut severs the chain at K (frames below
  K do not influence frames ≥ K).
- Compute output N from the now-present, pinned predecessor chain via the existing
  explicit-predecessor processing boundary (D27/D37 — no new pixel maths); return N
  to VapourSynth (the returned frame's ref is TRANSFERRED, not released).
- Final unpin of the whole pin-list in one short atomic at end (4.5).

**9.3 First-in-best-dressed — both branches release (D07).**
- Winner: store-and-pin-and-record (inside atomic), no extra addref beyond the store.
- Loser: `freeFrame` its computed-but-unstored duplicate before discarding — never
  drop the pointer (else leak). Wasted compute acceptable
  (`duplicate_store_computed_but_discarded` counter); the frame ref must still be
  freed.

**9.4 Terminology (kept distinct to stop one word hiding two things):**
*unlock* (release the cache lock) ≠ *freeFrame* (release a frame ref); *source hole*
(missing source → request from VS) ≠ *output hole* (missing cached output →
recompute); *pin* (consumer need, in lock) ≠ *hot zone* (anticipated, heuristic) ≠
*checkpoint flag* (recovery-anchor retention).

**9.5 Recovery model — two-phase: descending search, then ascending fill-holes-only.**
The old conservative `[max(0,N-B), N]` source-request window is GONE (it was an
artifact of held-ref-only defensiveness). Because pins guarantee a present
predecessor cannot be pruned, recovery only computes genuine holes and only requests
their sources. This makes the old "H17 sparse-hole" efficiency the BASELINE, because
the pin scheme makes it safe.

**Phase 1 — find the start point (DESCENDING search from N-1).**
```text
scan K = N-1, N-2, ... down to the lower-bound floor max(0, N-B):
    first PRESENT cached output found  -> start_point = K, STOP (pin it)
        (checkpoint flag IRRELEVANT here: the first present frame wins, close or
         far. A checkpoint matters only because its longer retention makes it MORE
         LIKELY to still be present in a cold region — not because it is preferred
         when a closer ordinary output exists.)
    none present before the floor       -> floor fallback (below)
```
The search is bounded to `[max(0,N-B), N]` (do not scan the whole clip).

**Phase 2 — fill (ASCENDING from start_point to N), fill-holes-only (CMS06.11 §2.1).**
```text
for K ascending from start_point+1 .. N:
    if output[K] already present -> reuse it (find-and-pin); do NOT recompute
    else (hole)                  -> compute (scene-change-aware, Section 9.2);
                                    store-and-pin (first-in-best-dressed)
then return output[N].
```
(There may be OTHER present frames above the start point; fill-holes-only reuses each
of them too — only genuine holes are computed.)

**Floor fallback — D29 approximate fresh-start (confirmed).** If Phase 1 reaches the
floor `max(0, N-B)` without finding any present output, compute the floor frame as a
FRESH START (no predecessor, reset/start semantics — as if frame 0), then fill
ascending to N. Justification: the recursive chroma blend smears toward its
predecessor, so the influence of the approximate start frame DECAYS over the B-frame
walk and the visible result at N-1/N converges to near-continuous-from-0 blending.
**Coherence constraint:** B (BACK_RADIUS) MUST exceed the effective settling length of
the recursive blend, so the approximation is invisible at N (relates CR2/CR3).

**Scene cuts bound the approximation further.** A cut between the start point (or
floor) and N severs the chain and resets EXACTLY at the cut, wiping any approximate-
start error below it. So the floor approximation only matters for a long static span
with NO cut between floor and N — precisely where recursive blending is most stable
anyway. Detected cuts are promoted to checkpoints (Section 6.4), so they become exact,
longer-retained start points found naturally by the Phase-1 search — this absorbs the
previously-considered "remember cut positions" optimisation for free.

**9.5.1 Dissolved source window (the request set).** arInitial requests **source N +
the sources for the genuine output holes only** (the holes found in Phase 1 between
start_point and N). NOT a blanket backward window. Example: start point a checkpoint
at N-10, holes only at N-3 and N-1 → request sources {N-3, N-1, N}, three frames, not
eleven. Requesting a source for a position whose output is already present is
unnecessary and avoided (it would be harmless per CMS06.11 first-in-best-dressed
layering, but the point of the dissolved window is to not do it).

---

## 10. Constants and parameter coherence

**10.1 Sizing constants (carried forward from CMS06.11; `[VERIFY V2/V3/V8]`).**
```text
active_ceiling            : frame-count, derived at creation from a 1 GiB nominal
                            byte budget / estimated frame bytes, clamped
                            [MIN_HARD_CEILING=150, MAX_HARD_CEILING=1000].
                            8-bit 4:2:0 PAL 720x576 (~0.59 MiB/frame) → byte budget
                            non-binding → ceiling = 1000 (~590 MiB). [VERIFY V8 format]
OVERFLOW_FACTOR           = 1.1
CHECKPOINT_INTERVAL       = 10        (+ frame 0; grid promotion)
CHECKPOINT_MAX_RETAIN     = 48  (soft; raised from 32 for cut-checkpoint density)
CHECKPOINT_MIN_RETAIN     = 10
HOT_ZONE_FORWARD_RADIUS   = 10
HOT_ZONE_BACK_RADIUS      = 50
MAX_HOT_ZONES             = 5
JUMP_THRESHOLD            = FORWARD_RADIUS + BACK_RADIUS + 1 = 61   (DERIVED)
decay_margin              = 20        (NEW — Section 5.5)
K (bounded-prune victims) = 8         (NEW — Section 7.3; instrumentation-tunable)
```

**10.2 Coherence rules (CR1–CR5) — coder MUST codify each as a comment directly
above the constant's definition (per Rule 1 / Document A §A12):**
- **CR1** JUMP_THRESHOLD is DERIVED = FORWARD_RADIUS + BACK_RADIUS + 1; never set
  independently.
- **CR2** BACK_RADIUS ≥ bounded recovery search window B; ideally = B (currently
  both 50). **AND** B MUST exceed the effective settling length of the recursive
  chroma blend, so the floor-approximation fresh-start (Section 9.5) is invisible at
  N. (If B were shrunk for memory, confirm it still exceeds the blend settling
  length, or the approximate start would show at the output.)
- **CR3** BACK_RADIUS ≈ 5 × CHECKPOINT_INTERVAL (a zone covers ~5 grid anchors);
  50=5×10.
- **CR4** active_ceiling ≥ ~2 × max-protected set, where max-protected ≈
  MAX_HOT_ZONES × (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool ≈ 5×60 + 48 =
  ~348; 1000 ≫ 348. (If pruning can never reach target, this rule is violated.)
- **CR5** CHECKPOINT_MAX_RETAIN ≥ MAX_HOT_ZONES × (BACK_RADIUS / INTERVAL) = 25 grid
  checkpoints, treated as a density FLOOR (cuts add irregular checkpoints on top —
  Section 6.4). MAX_RETAIN raised 32 → 48: 25 grid floor + ~23 headroom for scene-cut
  checkpoints within the ~300-frame protected span. Content-dependent starting value;
  if cut-heavy material keeps the pool pinned at 48 with prune unable to reduce, raise
  it. MIN_RETAIN unchanged at 10.
- **decay_margin bound:** FORWARD_RADIUS ≤ decay_margin ≤ BACK_RADIUS (10 ≤ 20 ≤ 50);
  far below active_ceiling.

**10.3 MAX_HOT_ZONES = 5** scales with concurrent DISTINCT access regions, NOT thread
count. Linear pipe-to-ffmpeg uses ~1 zone regardless of threads → 5 ample. Raise only
if a scatter workload appears (raising it pushes CR4 → active_ceiling may follow).

**10.4 Instrumentation discipline.** A counter BUMP (single increment) may occur
inside a lock; FORMATTING and EMISSION (sprintf, string building, file/stderr writes)
MUST be outside the lock unless specifically justified. Emitting inside a critical
section extends it and pollutes contention measurement. Non-zero error counters are a
hard gate (D14).

---

## 11. First implementation milestone (prove ownership before behaviour — D30)

Build, against this spec and **in isolation (no VapourSynth wiring yet)**, the
cache-manager core, mirroring the staged "prove the mechanism before wiring
behaviour" discipline:
- cache-manager data structures (slot = `VSFrame*` ref + frame number + pin_count +
  is_checkpoint; ordered frame-number index per D04; non-checkpoint + checkpoint
  pools per D05; hot-zone state; per-invocation pin-list);
- single cache-wide lock skeleton + the inside/outside-lock discipline;
- pin / unpin + pin-list record/discharge + single-ownership/null-on-consume +
  discharge-before-free ordering;
- composite eviction predicate + bounded prune (decide+detach in lock, batch
  freeFrame outside, K-bound).

Prove in isolation: pin/unpin balance = 0, lookup-ref balance = 0, no leaks, no
double-free, eviction never selects a pinned/checkpoint/in-zone slot — BEFORE any
getFrame integration. Header/structure design and exact function naming are the
coder's, aligned with separation-of-responsibilities (pixel processing must not own
cache/scheduling policy; the cache manager must not contain pixel logic).

**Second step — salvage:** identify old code verifiably safe to reuse (response-table
creation, memory diagnostics, the pixel/frame-processing layer incl. the explicit-
predecessor boundary) and copy/modify from the `.txt` reference files into the new
locations, preserving separation of responsibilities.

---

## 12. Decision cross-check vs prior settled decisions (Document B)

**12.1 Carried forward UNCHANGED:** D01 (API4-only), D03 (per-instance), D04 (ordered
maps), D05 (pools own refs, index non-owning), D06 (addref under mutex — this is the
find-and-pin primitive), D07 (first-in-best-dressed), D08 (sliding zones), D09
(central remove helper), D10 (frame-count ceiling), D11 (prune before reject), D14
(diagnostics gate), D15 (zone update at arInitial), D16 (per-invocation frameData),
D26/D33 (old_strict not final authority; retire before parallel), D27/D28 (reuse
processing boundaries; no parallel pixel algorithms — override discipline), D29
(bounded-start reset semantics), D30 (compute/store/return/transfer/authority
separately provable), D31/D32 (CMS02-J0 mandatory before parallel wiring; diagnostics
consolidation), D36/D37 (sequential predecessor reuse; fast path = source N + cached
output N-1 via explicit-predecessor boundary — generalised: predecessor is PINNED).

**12.2 SUPERSEDED:** **D13** ("non-checkpoint pinning deferred") — superseded by
Position B (Section 3.3): consumer-pinning is the mandatory baseline, not a deferred
escalation. Reason: fmParallel goal + KISS/maintainability.

**12.3 Superseded IN MECHANISM, sub-rules RETAINED** (the old held-ref-only
predecessor-reservation cluster, D38–D58): the held-ref reservation MECHANISM is
replaced by consumer-pins, BUT these sub-rules survive (generalised from "the
reserved predecessor ref" to "any consumer pin"): single-ownership/null-on-consume
(D45/D53/D54 → Section 4.4); lookup-ref accounting `acquired==released+transferred`
(D55 → 4.8); ordinary refs are lookup-addref not a distinct pin-kind, don't conflate
with checkpoints (D56 → Section 6); H17 deferred (D48/D57 → 9.5); Option A (rely on
serial callback ordering) rejected (D49 → Section 3.3). The fail-closed-only H15.6B
draft stays retired (D46/D58).

---

## 13. Verify items (confirm before treating dependent sections as final)

- **V1 RESOLVED** algorithmic temporal reach backward-only / purely causal —
  confirmed by Dave against CNR2 itself (authoritative); corroborated by §A4, the
  proven CMS06.11 request window, and absence of any forward reference. Closed.
- **V2** checkpoint retention metric/params (INTERVAL=10, MAX=32, MIN=10) still
  intended for the new architecture.
- **V3** checkpoint establishment (every-10th + frame 0) still intended.
- **V4** R76 API4 frameData carry mechanism + cleanup discharge ordering (Section 3.2).
- **V5** addFrameRef/freeFrame atomicity on R76 API4 — externally indicated yes;
  confirm vs headers (load-bearing).
- **V6** VapourSynth upstream source-cache policy on R76 (affects over-fetch cost).
- **V7** the original failure mode that motivated hot zones — confirm the new scheme
  addresses it.
- **V8** CNR3 operating pixel format (bit depth + subsampling; integer YUV only per
  §A3) — sets frame bytes / active_ceiling.

---

## 14. Changelog

### CMS07.0a — 2026-06-12 (working draft increment)
- **V1 RESOLVED:** algorithm confirmed purely causal against CNR2 (current source +
  previous output; no forward peek). `[VERIFY]` dropped from Section 1.
- **Recovery model rewritten (Section 9.5):** two-phase — Phase 1 DESCENDING search
  from N-1 for the nearest present cached output (start point; checkpoint flag
  irrelevant at search time), bounded to `[max(0,N-B), N]`; Phase 2 ASCENDING
  fill-holes-only (reuse every present frame, compute only genuine holes).
- **Dissolved source window (Section 9.5.1):** the old blanket `[max(0,N-B), N]`
  source-request window is GONE. arInitial requests source N + sources for genuine
  holes only. The old "H17 sparse-hole" efficiency becomes the baseline (pins make it
  safe). H17-as-deferred-item retired.
- **Floor fallback (Section 9.5):** D29 approximate fresh-start confirmed; justified
  by recursive-blend smear/decay; CR2 extended — B must exceed blend settling length.
- **Scene cuts (Sections 1, 9.2, 6.4):** detection runs during compute; a cut is an
  in-walk fresh-start that severs the chain; cut frames are promoted to checkpoints
  (irregular spacing — search is position-agnostic, D04); grid promotion retained
  alongside; absorbs the deferred "remember cut positions" idea.
- **CHECKPOINT_MAX_RETAIN 32 → 48** for cut-checkpoint density; CR5 reframed as a
  density floor; CR4 max-protected updated to ~348.

### CMS07.0 — 2026-06-12
Architectural supersession of CMS06.11. Pinning becomes the mandatory correctness
mechanism (consumer-pins); hot zones demoted to decay-hints over a pinned liveness
index; checkpoints become a flag + retention rule, not a pin; single cache-wide lock
held minimally; bounded prune (decide+detach in lock, freeFrame outside, K=8); new
constants decay_margin=20 and K=8; five parameter-coherence rules (CR1–CR5) to be
codified as code comments; fmParallel final-goal invariant stated outright. D13
superseded; held-ref-only predecessor-reservation cluster superseded in mechanism
with sub-rules retained; foundational decisions D01–D11/D14–D16/D26–D33/D36–D37
carried forward. DRAFT: sections dependent on V1–V8 marked `[VERIFY]`.

---

## 15. Open items / completeness note (DRAFT status)

This is a working CMS07.0 draft assembled from the settled-decision capture. Before
it is treated as final controlling authority:
- resolve V1–V8;
- complete the bring-across-and-revalidate audit against the full prior CMS06.11 text
  (this draft carries the decision-level audit via Document B §12; a section-level
  pass against CMS06.11 body should confirm no still-valid mechanism — e.g. exact
  recovery/fallback proof structure, VS-LIFECYCLE-01 wording, RC discipline detail —
  was dropped);
- the matched handover pack (Documents A/B/C → "CMS07.0-or-later") is produced per
  the Handover Pack Production Spec (continuity-preserving, §11 checklist);
- a self-review grep pass (version strings, cross-references, no orphaned old-concept
  references) as per prior CMS practice.
