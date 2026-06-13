# Document A — CNR3 Project Context and Standing Rules (CMS07.0 restart)

**Version:** v3.0 (CMS07.0 restart)
**Role:** Human-facing front door to the CNR3 project — the rich context source. It
sets the scene — what CNR3 is, why it exists, why it is hard — then reproduces the
standing rules that govern all work. It is **reference and context**, subordinate to
CMS07.0: where this document and CMS07.0 disagree, CMS07.0 wins.

*Reading order for a coder restart chat:* read the **coder restart introduction
first**, then **CMS07.0**, then this Document A and Document B. "Front door" means
this is the human-facing orientation document within the pack — not that it overrides
the restart introduction's start-here sequencing.

*CMS07.0 reading note:* references to **CMS07.0** as the controlling design mean
**CMS07.0 or its later approved successor**. Specific section pointers (e.g. §9A.2) are
version-specific to CMS07.0 and should be re-checked against any successor; historical
statements about what CMS07.0 superseded stay pinned to CMS07.0; the filename
`cnr3_cache_manager_design_v7_0.md` denotes that specific file.

---

## 1. What CNR3 is, and why this project exists

CNR3 is a VapourSynth **API4-only**, **integer-YUV-only** recursive temporal chroma
stabiliser, inspired by the CNR2 / vscnr2 temporal chroma-stabilisation algorithm.

**The human reason for the project.** Old analogue video — VHS, VHS-C, and similar
tape and capture sources — carries temporal chroma instability: colour that shimmers,
crawls, and flickers frame to frame even where the picture is essentially still. CNR3
exists to *stabilise that chroma* during restoration: to reduce chroma shimmer,
dot-crawl-like instability, and temporal chroma noise by reusing controlled amounts of
the **previous filtered chroma** where the content is stable enough to allow it —
without smearing genuine motion and without dragging colour across scene cuts. The aim
is clean, stable colour on restored analogue captures that still looks natural.

The project's concrete aims:
- redevelop CNR2/vscnr2-style chroma stabilisation in modern C++;
- target VapourSynth API4 only (no API3-era types, assumptions, or scheduling
  shortcuts);
- support integer YUV formats only;
- get recursive chroma-stabilisation correctness right *before* pursuing parallel
  performance;
- operate safely under modern, possibly out-of-order, VapourSynth frame requests;
- provide strong diagnostics for cache, threading, memory, and reference-count
  behaviour.

Primary target material is PAL VHS/VHS-C captures (720×576), though the design is
format-adaptive.

---

## 2. The load-bearing fact — why CNR3 is not a normal filter

This section is intentionally detailed. A new maintainer must understand it before
touching anything.

CNR3 is **not** a normal stateless image filter. It is a **recursive temporal**
filter: the output for one frame depends on the *already-filtered output* of the
previous frame.

```text
output[N] depends on source[N] and output[N - 1]
```

The predecessor is **not** merely `source[N - 1]`. It is the already-filtered output
frame `N - 1`.

If VapourSynth requested frames strictly in display order (0, 1, 2, 3, 4), a naive
previous-frame-only implementation could appear to work. But a modern VapourSynth graph
is **not** required to request frames in display order. A filter may be asked for:

```text
0, 3, 1, 2, 4
```

A naive implementation fails at frame 3, because the correct filtered `output[2]` does
not yet exist. It might reject the request, use the wrong predecessor, or corrupt the
recursive chain. **This single fact is the source of every hard problem in CNR3, and
the reason the cache subsystem exists.**

---

## 3. The algorithmic core

CNR3 is a recursive temporal chroma filter. At a high level:

```text
For luma Y:
    copy source luma unchanged.

For chroma U/V:
    compare current source chroma against previous filtered chroma;
    compare current downsampled luma against previous downsampled luma;
    use signed-difference response tables for luma and chroma;
    blend current source chroma toward previous filtered chroma by the combined
    response.
```

Conceptually:

```text
weight = response_y(diff_y) * response_chroma(diff_chroma)
output_chroma[N] = weighted blend of  output_chroma[N-1]  and  source_chroma[N]
```

Small differences (stable content) → strong blend toward the previous filtered chroma
(denoise). Large differences (motion, change) → weak blend, trust the current source
(don't smear).

**Scene-change handling.** Carrying previous filtered chroma across a true scene cut
would smear or contaminate the new scene. When scene-change detection fires (during the
compute, by comparing the frame to its predecessor), CNR3 copies the current source
chroma and skips the recursive blend for that frame — a fresh start.

*(Pixel-layer implementation note: CNR3 computes in native subsampling at native bit
depth, using a wide int64 arithmetic accumulator for the weighted blend with the shift
scaled by depth — the proven overflow-safe approach. This is pixel-layer detail; see
CMS07.0 §13 V8.1.)*

---

## 4. Why VapourSynth scheduling is central

VapourSynth evaluates a filter graph by asking filters for frames through a `getFrame`
callback.

```text
arInitial:
    VapourSynth is asking the filter to start work for frame N.
    The filter requests any upstream/source frames it needs.

arAllFramesReady:
    The requested upstream/source frames are available.
    The filter reads source data, produces output, and returns a frame.
```

Implications a recursive filter must respect:
- `arInitial` is request-arrival; `arAllFramesReady` is **not** the same as frame N
  being next in display order.
- A filter must not assume `N-1` was already requested or produced.
- A filter must not assume only nearby frame numbers will be requested.
- A filter must not let pruning discard a frame an active request needs.
- A recursive filter must deliberately manage predecessor availability.

(There is also a hard API rule — any source retrieved in `arAllFramesReady` must have
been requested in `arInitial` of the same activation. CMS07.0 §9A.1.)

---

## 5. Filter modes and the final goal

- **`fmUnordered`** — the filter may be called for frames in any order, one activation
  at a time per frame; safe baseline for current work.
- **`fmParallelRequests`** — multiple requests in flight; the filter must be safe under
  concurrent arInitial activity.
- **`fmParallel`** — fully parallel execution; the final operational target.

Goal posture:

```text
safe under fmUnordered now
structurally compatible with fmParallelRequests
final design target: fmParallel
```

Design and coding must not make choices that block eventual safe `fmParallel` unless
explicitly justified and recorded. This posture is the reason CMS07.0 adopts mandatory
consumer-pinning (see §7).

---

## 6. Why the cache manager exists (correctness, not performance)

The cache manager is a **correctness subsystem, not a performance cache.** It retains
computed outputs so the recursion can find each predecessor — or recover it — rather
than rebuilding the chain from frame 0 on every request. It exists to provide:
- safe predecessor availability and cache-hit reuse;
- checkpoints and bounded recovery of missing outputs ("holes");
- pruning with bounded memory use;
- safety under out-of-order and (eventually) parallel requests;
- no dangling frame pointers, no leaked `VSFrame` references, no double frees, no stale
  index entries.

**Historical note that motivates the whole design.** The ancestor CNR2 runs serialized
(`MT_SERIALIZED`) and keeps a single previous-output member. When asked for a
non-sequential frame, CNR2 cannot supply the true previous output and substitutes the
previous *source* frame as an approximate predecessor (`last_frame != n-1 →
GetFrame(n-1)`). It gets away with this only because it is serialized and the recursive
blend forgives a one-frame stand-in. CNR3 abandons serialization to reach `fmParallel`,
so it **cannot** rely on that approximation — and CNR3's cache + recovery architecture
is precisely the principled replacement: supply the *exact* previous output, present or
recovered.

---

## 7. The CMS07.0 restart — what it supersedes, and the old/new separation

A reassessment found the previous (CMS06.x) cache mechanism workable but not genuinely
fit for purpose. **CMS07.0 is a new architecture that completely supersedes the
previous cache design.** It is the controlling design authority.

**What changed, in plain terms:**
- **Pinning is now the mandatory correctness mechanism.** Any frame a request actively
  needs is *pinned* by that request (a consumer-held pin), and the cache is the
  complete liveness index for the active set. (Previously, pinning was a deferred
  escalation — that decision is superseded.)
- **Hot zones are demoted to prune-policy hints.** They no longer guarantee findability
  of active frames (pins do that); they protect the anticipatory/decaying set.
- **A checkpoint is a retention flag, not a pin.** There is exactly one pin concept
  (consumer-claim).
- **One cache-wide lock, held minimally**; slow work (pixel compute, source requests,
  freeFrame) happens outside it.
- **Bounded prune** (decide-and-detach under the lock, batch freeFrame outside).
- **Recovery is two-phase:** a descending search from N-1 for the nearest present
  output (checkpoint flag irrelevant at search time), then an ascending
  fill-holes-only walk; with a dissolved source-request window (request source N plus
  the genuine holes only, not a blanket backward window).

**The old/new separation — three highest-risk traps (do not conflate):**
1. Treating pinning as optional/deferred — it is now the mandatory baseline.
2. Reintroducing held-ref-only predecessor reservation — superseded by consumer-pins.
3. Thinking of a checkpoint as a pin — it is a separate eviction-protection flag.

**Controlling authority and precedence:**
- CMS07.0 is final and complete for this restart unless explicitly revised.
- If CMS07.0 conflicts with — or is unclear in alignment with — any prior material
  (this document included), **CMS07.0 wins** unless the user says otherwise.
- If CMS07.0 **itself** is silent, ambiguous, or incomplete on an implementation
  point, **stop and ask — do not guess.**

---

## 8. High-level architecture (intended separation of responsibilities)

For the restart, responsibilities separate as follows (the concrete new file layout is
the coder's proposal under CMS07.0 §11, not fixed here):
- **Pixel/frame processing** — luma copy, downsampled-luma buffers, scene-change
  detection, recursive chroma blend. Must NOT own cache or scheduling policy.
- **Cache manager** — pools, ordered index, pins/pin-list, hot zones, checkpoint flags,
  store/prune, recovery planning, validation, diagnostics. Must NOT contain pixel
  logic.
- **Response tables** and **memory diagnostics** — cache-independent utilities.

---

## 9. Standing rules

The authoritative source for rules is split to avoid duplication that drifts:
- **Register-owned rules** (authority, pack governance, coding process, architecture /
  salvage) live in the Production Spec's Prevailing Rules Register (§3A); this section
  reproduces them for the reader. If this section and §3A disagree, §3A wins.
- **Design / cache-core rules** (pinning, checkpoints, hot zones, locking, pruning,
  RC1–RC8, RAII, VS-LIFECYCLE-01, recovery, CR1–CR5, instrumentation, AS1–AS7, the V5
  firewall, the first-milestone gates) are **defined in CMS07.0** and are NOT restated
  here — consult CMS07.0 directly (see the hand-off note at the end of this section).

Once the coder's restart enumeration is signed off into §3A, this section is
regenerated to reproduce the owned rules. The owned standing rules are:

**9.1 Reuse existing processing boundaries.** Recovery/warm-up compute reuses the
existing per-frame processing boundary; no parallel/duplicate pixel or frame algorithms
without explicit agreement (override discipline).

**9.2 Output-authority discipline.** Compute, store, return-decision, return-transfer,
and output-authority are each separately provable (D30).

**9.3 Diagnostics are proof of safety.** Unexpected reference-count, prune, validation,
or recovery-search values stop the next phase until understood. An unexpected non-zero
error counter is a hard gate.

**9.4 Design Compliance Review.** After each phase or coherent block, run the review
(CMS07.0 §9A.8 defines the 17-item checklist, including item 17: every critical section
matches its AS-register entry).

**9.5 fmParallel final-goal invariant.** fmParallel is the final target; no choice may
block eventual safe fmParallel without explicit justification.

**9.6 Architecture separation.** Pixel/frame processing must not own cache or
scheduling policy; the cache manager must not contain pixel logic.

**9.7 Salvage discipline.** Salvage is the second step only, after the cache core is
proven; salvage CNR2 pixel maths but never its recovery/predecessor logic (§11).

**Hand-off note (design rules live in CMS07.0).** The reference-count discipline
(RC1–RC8) and RAII baseline, the atomic-scope register (AS1–AS7) and V5 firewall,
VS-LIFECYCLE-01, the pinning / checkpoint / hot-zone / locking / pruning rules, the
recovery and source-request rules, bounded-start honesty, the parameter-coherence
constants (CR1–CR5, decay_margin), and the instrumentation/recovery-search rules are
**defined and numbered in CMS07.0**. CMS07.0 is the authoritative register for them;
they are deliberately NOT restated here, to keep one source per rule. Consult CMS07.0
directly.

---

## 10. Coding rules (reproduced from the Production Spec §3A register)

*(As with §9, the Production Spec's Prevailing Rules Register is authoritative; this
section reproduces the coding rules for the reader.)*

**Rule 1 — Code comments.** Concise, but never over-compress safety-critical comments:
locking/threading invariants, ownership/lifetime, reference-count discipline, non-obvious
pre/postconditions. This includes codifying CR1–CR5 as comments above their constants.

**Rule 2 — Code update instructions.** Before/after patch format: state the file and
function; the before-block must uniquely match with enough surrounding context; the
after-block is the exact replacement.

**Phase/SubPhase naming.** Numbering restarts for the new development; use the expanded
Phase/SubPhase naming convention, with the concrete new convention proposed before
coding (unless the user approves a different one).

**Commit titles/bodies.** Follow the established Visual Studio commit title/body
convention on each PASS.

---

## 11. Salvage policy

- Salvage is the **second** step — only after the new cache-core
  ownership/pinning/eviction discipline is proven in isolation.
- **Likely-salvageable (cache-independent):** response-table creation, memory
  diagnostics, the pixel/frame-processing layer including the explicit-predecessor
  boundary.
- **CNR2 reference** (github.com/Asd-g/AviSynth-vsCnr2): salvage the **pixel maths** —
  response-table construction, the int64-accumulator weighted blend with
  `shift2 = 2*depth` (its high-bit-depth overflow fix), `downSampleLuma`, in-compute
  scene-change detection. **Never** adopt CNR2's recovery/predecessor logic; its
  serialized `last_frame != n-1 → source[n-1]` shortcut is exactly what CNR3 exists to
  eliminate.
- No old `.txt` code is copied into new files during the first milestone without
  explicit per-case approval.

---

## 12. Continuity note

This Document A is for the CMS07.0 restart. The enduring project context above is
preserved/adapted from the prior project documentation; descriptions of the *old* cache
architecture have been corrected to the CMS07.0 model. The old CMS06-era Document B
(decision log) and Document C (volatile state) are **historical archive only** and are
not part of this pack — CMS07.0 (§9A, §12, §12A) already carries forward the still-valid
rules and decisions. Old design documents (CMS06.x) are historical archive only;
CMS07.0 is the sole design authority.
