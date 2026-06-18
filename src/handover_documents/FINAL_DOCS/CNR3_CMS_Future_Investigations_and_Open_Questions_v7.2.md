# CNR3 CMS — Future Investigations and Open Questions

**Date:** 2026-06-17
**Pairs with:** the CNR3 Cache Manager Design Specification (CMS) at the **identical
version number**, which is carried in this document's filename
(`CNR3_CMS_Future_Investigations_and_Open_Questions_v7.2.md` pairs with CMS07.2).
This document carries **no internal version number** — its version is its filename,
and that filename always matches the prevailing CMS version. The date above reflects
this document's last **content** change (which may be older than the filename version,
because this document re-versions in lockstep with the CMS even when its content is
unchanged — see the maintenance rule below).

---

## 0. Status — NON-NORMATIVE. NOT the design. NOT for implementation.

**This document is not part of the CMS and is not controlling.** It records open
design questions, deferred tuning, and investigations to be resolved later. It exists
so that good questions are not lost between sessions, and so that the CMS itself stays
purely normative (it states only what is decided and binding).

Read and obey the following:

- **Nothing in this document is a design rule.** Nothing here describes current,
  required, or approved behaviour.
- **Nothing here is implemented without formal prior approval.** An item leaves this
  document and becomes real ONLY by being adopted into the CMS through the normal
  design-change process (proposal → review/sign-off → a new CMS version). Until that
  happens, an item here has no authority of any kind.
- **This document is NOT part of the coder handover pack.** A coder restart chat must
  never receive it as an input, so it can never be mistaken for a directive. It is a
  designer/coordinator working document only.
- If you are implementing CNR3, the CMS (and the Production Spec §3A process rules) are
  your authorities. This document is background context for *future* design discussion,
  nothing more.

**Maintenance rule (lockstep versioning).** When the CMS version is bumped, this
companion is re-issued at the **identical** version number — even if its content has
not changed and only the filename changes. The rule is simply: *the companion and the
CMS always carry the same version number.* This guarantees zero ambiguity about which
companion goes with which CMS. The internal **date** is updated only when the content
actually changes, so the date tells you when an item was last genuinely revised, while
the filename tells you which CMS the document is paired with.

**How an item is resolved (lifecycle).** An open item proceeds:
1. recorded here as an open question, with its description and discussion-to-date;
2. assessed when its trigger arrives (or when the designer/coordinator chooses);
3. if adopted → written into the CMS as a real design change (new CMS version, with the
   usual diff-verify and sign-off), and the relevant build-config constants/coherence
   comments updated to match;
4. then removed from the open list here (optionally recorded in a short resolved log at
   the bottom, so the history of *why* a change was made is preserved).

---

## 1. Index of open items (maintained)

Each entry is a one-line descriptor. Full detail is in the correspondingly-numbered
section below. This index is kept current as items are added or resolved.

```text
FI-01  FORWARD_RADIUS tuning for higher thread counts — hot-zone forward window
       (=10) is sized for ~6 threads; likely needs raising (≈12) for higher
       parallelism. Efficiency-only; correctness is never affected. OPEN.
```

---

## 2. Open items (full description and discussion-to-date)

---

### FI-01 — FORWARD_RADIUS tuning for higher thread counts

**Status:** OPEN — investigation deferred to the fmParallel tuning phase.

**Trigger (when to assess this):** when fmParallel tuning is undertaken, or when CNR3
is run at higher thread counts (roughly 12 threads and above), or whenever the
recovery-search / recompute diagnostics show elevated recompute under high-thread load.

**The constant in question.** `HOT_ZONE_FORWARD_RADIUS = 10` (CMS §10.x). The hot zone
for an arriving frame `F` spans `[F - BACK_RADIUS, F + FORWARD_RADIUS]`
= `[F - 50, F + 10]` (CMS §5.4). The zone is deliberately asymmetric: it protects 50
frames *behind* the active frame and 10 frames *ahead*.

**Why the forward window matters, and how it is used.** In normal forward playback the
hot zone advances toward higher frame numbers. The forward radius is the *anticipatory*
cushion: it keeps frames just ahead of the active frontier prune-protected, because the
zone is about to advance into them (they are likely-needed-soon). Frames more than 10
ahead are outside the zone and therefore prune-eligible; frames more than 50 behind
likewise fall out of the zone and become the genuinely-cold eviction candidates. This
asymmetric shape (50 back / 10 forward) is what gives prune its effective directional
bias in forward motion — the behind-frames become the evictable ones, the just-ahead
frames stay protected — without the prune *ordering* itself needing to be directional
(prune ordering is "greatest-distance-from-zone-first", CMS §7.1 / §5.5).

**Empirical basis of the current value (CMS §9A.9).** FORWARD_RADIUS=10 rests on
measured linear-encode request jitter from the prior design's simulation: p99 forward
jitter ≈ 8 under concurrent linear encoding. It is measured-and-margined, not arbitrary
— 10 covers a ~8-frame forward jitter with a small margin.

**Discussion to date — effect of thread count.** The key fact (CMS §10.3) is that
`MAX_HOT_ZONES = 5` scales with concurrent *distinct access regions*, NOT with thread
count: a linear pipe-to-ffmpeg workload uses ~1 zone regardless of how many threads run.
So more threads on linear work do not create more zones; they widen the *spread of
in-flight frames near the single advancing frontier*. The question is therefore whether
FORWARD_RADIUS=10 covers the forward spread of the in-flight frames under N threads:

- **~6 threads:** ~6 frames in flight near the frontier; forward spread comfortably
  within the measured jitter (~8). The leading frames sit inside the +10 window →
  protected. FORWARD_RADIUS=10 is **ample**.
- **~12 threads:** ~12 frames potentially in flight; forward spread is at or near the
  edge of the +10 window. This is the **thinning-margin** case: 10 covers ~8 jitter
  with margin, but tightly-packed 12-in-flight could occasionally reach +11/+12.
  **Probably still adequate**, and any miss is a cheap recompute (see correctness note).
- **~16 threads:** ~16 frames in flight; forward spread can plausibly exceed +10 (the
  leading frame might sit at +14/+15), placing some leading frames outside the zone and
  hence prune-eligible by the distance rule. At this level the +10 cushion is likely
  **too small** for the anticipatory role.

**Correctness is never affected — this is an efficiency matter only.** Pins, not zones,
guarantee the in-flight set (CMS §5.1, §5.2). Any frame a thread is actively computing
or holding as a predecessor is pinned, and §7.1's eviction predicate excludes pinned
slots unconditionally (`pin_count == 0` is the first conjunct). So even if the forward
cushion is overrun at high thread counts, the frames that matter are protected by pins;
the forward radius overrunning costs *recompute pressure* (a frame anticipated-but-not-
yet-pinned may be pruned and later recomputed), never wrong output. This is exactly the
§5.2 guarantee: the zone scheme's soft spots are harmless inefficiencies because pins
underwrite correctness.

**Likely change when this is assessed.** Raise `FORWARD_RADIUS` (a candidate value of
≈12 has been suggested as a first step for higher thread counts). This is a tuning
adjustment, not a redesign.

**Ripple effects to assess BEFORE changing the value** (these are why it is a CMS change,
not a free edit — the constants and their coherence rules live in CMS §10.x):

- **CR1 — JUMP_THRESHOLD is DERIVED** = `FORWARD_RADIUS + BACK_RADIUS + 1`. It is never
  set independently, so raising FORWARD_RADIUS automatically raises JUMP_THRESHOLD;
  confirm the derivation is recomputed (e.g. FORWARD_RADIUS 10→12 makes JUMP_THRESHOLD
  61→63).
- **CR4 — memory budget / headroom.** `active_ceiling ≥ ~2 × max-protected set`, where
  `max-protected ≈ MAX_HOT_ZONES × (BACK_RADIUS + FORWARD_RADIUS) + checkpoint pool`.
  Raising FORWARD_RADIUS raises the protected-set size (e.g. with the current others,
  5×(50+12)+48 ≈ 358 vs the prior ~348); confirm active_ceiling (1000) still
  comfortably exceeds ~2× that, which it does with large margin.
- **decay_margin bound.** Must remain `FORWARD_RADIUS ≤ decay_margin ≤ BACK_RADIUS`
  (currently 10 ≤ 20 ≤ 50). Raising FORWARD_RADIUS to 12 keeps the bound satisfied
  (12 ≤ 20 ≤ 50); but if FORWARD_RADIUS were ever raised above decay_margin (20),
  decay_margin would need raising too.

**How the need will be detected (diagnostics-informed).** The recovery-search summary
diagnostics (the D-SUM family covering search depth, holes filled, and recompute
pressure — e.g. D-SUM-03 and the hot-zone D-SUM-11 counters) are the instruments that
reveal whether the forward cushion is being overrun under a given thread count. The CMS
already anticipates validating the tuning guesses (K, MAX_RETAIN, decay_margin, B, and
by extension the radii) against a real run. So the decision to raise FORWARD_RADIUS
should be driven by observed recompute pressure at the target thread count, not by
guesswork.

**Authority note.** FORWARD_RADIUS and the CR coherence rules are CMS-owned (§10.x).
Changing the value is therefore a CMS revision (new CMS version, diff-verified), and the
coherence comments (CR1–CR5, decay_margin bound) in the build-config header
(`cnr3_build_config.h`, established in the G.1A constants phase) must be updated to match
the new value. This is not a Production Spec §3A change (it is design, not process) and
not a diagnostics-spec change.

---

## 3. Resolved items (history)

*(none yet — when an item is adopted into the CMS, move its descriptor here with the
CMS version that absorbed it, so the rationale is preserved.)*
