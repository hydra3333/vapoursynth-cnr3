# CNR3 — Development restart on the new CMS07.0 cache architecture

*(Coder introductory brief — paste ahead of the CMS07.0 design document.)*

We are at a natural pause point in CNR3 development. A reassessment found the
current caching mechanism, while workable, is not genuinely fit for purpose. A new
mechanism has been designed — reusing some concepts but **completely superseding the
previous cache design.** The attached **CMS07.0** (`cnr3_cache_manager_design_v7_0.md`)
is the new controlling design authority. Please study it carefully before responding.

**All existing cache-related code is explicitly superseded.** Some code remains
useful — the pixel-computation layer, response-table creation, and memory diagnostics
are largely cache-independent and are expected to be reused (D27/D28 *require* reusing
the existing frame-processing boundary rather than writing new pixel logic).

## Build / transition (decided)

Rename all existing `.h`/`.cpp` files **except `VapourSynth4.h` and `VSHelper4.h`**
to `.txt`, so old code stays available as reference for verifiable salvage but leaves
the active build. The old binary need not build; GitHub CI may break for now; builds
will be done in **Visual Studio 2026**. Phase-numbering for the new work is your
choice.

## Do not conflate old and new concepts

Three highest-risk traps:

1. Treating pinning as optional/deferred — it is now the **mandatory** correctness
   baseline (this supersedes old decision D13).
2. Reintroducing held-ref-only predecessor reservation — superseded by
   **consumer-held pins** on a per-invocation pin-list.
3. Thinking of a checkpoint as a pin — a checkpoint is now a **separate
   eviction-protection flag** with its own retention rule.

There is exactly one pin concept (consumer-claim). Where old code and CMS07.0
disagree, the spec wins; old `.txt` code is salvage reference only.

## First milestone — prove ownership before behaviour (CMS07.0 §11, per D30)

In isolation, **no VapourSynth wiring yet**, build the cache-manager core:

- data structures: slot = `VSFrame*` ref + frame number + pin_count + is_checkpoint;
  ordered frame-number index; non-checkpoint + checkpoint pools; hot-zone state;
  per-invocation pin-list;
- the single cache-wide-lock skeleton with the inside/outside-lock discipline;
- pin / unpin + pin-list record/discharge + single-ownership/null-on-consume +
  discharge-before-free ordering;
- the composite eviction predicate + bounded prune (decide+detach in lock, batch
  `freeFrame` outside, K-bound).

Prove pin/unpin balance = 0, lookup-ref balance = 0, no leaks, no double-free, and
that eviction never selects a pinned / checkpoint / in-zone slot — **before** any
`getFrame` integration.

**Please estimate and create the header/structure layout yourself** — the likely
`.h` files, the structures, and the function names/signatures (with purpose/parameter
comments) needed to do all of the above — aligned with separation of responsibilities
(pixel processing must not own cache/scheduling policy; the cache manager must not
contain pixel logic). Propose them back to me for review.

## Second step — salvage

Once the core is proven, identify old code verifiably safe to reuse (response tables,
memory diagnostics, the pixel/frame-processing layer including the explicit-predecessor
boundary) and copy/modify it from the `.txt` files into the right new locations,
preserving separation of responsibilities.

## Prevailing rules — please enumerate ALL of them back to me

CMS07.0 and the prior handover Document A carry a set of standing coding / process /
design rules, for example:

- code-comment Rule 1;
- before/after-patch Rule 2;
- expanded phase/SubPhase naming;
- Visual Studio commit-message format;
- reuse-existing-processing-boundaries / no-parallel-pixel-algorithms (override
  discipline);
- compute / store / return-decision / return-transfer / output-authority each
  separately provable;
- diagnostics-as-hard-gate (unexpected non-zero error counter stops work);
- the CR1–CR5 parameter-coherence rules, to be codified as comments above the relevant
  constants;
- the fmParallel final-goal invariant.

**Please list every prevailing rule you can identify from CMS07.0 and the handover
documents and as we may have agreed separately, each stated briefly, so I can confirm,
modify, supersede, or retire each one explicitly before coding begins.
** Do not assume any rule carries over silently — surface them all for my sign-off.

## Please respond with

- confirmation of your understanding of the restart and the old/new separation;
- any questions on CMS07.0;
- your proposed header/structure layout;
- the enumerated prevailing-rules list.

**Do you have any comments or questions?**
