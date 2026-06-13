# CNR3 — Development restart on the new CMS07.0 cache architecture

*(Coder introductory brief — paste ahead of the CMS07.0 design document.)*

We are at a natural pause point in CNR3 development. A reassessment found the
current caching mechanism, while workable, is not genuinely fit for purpose.

A new mechanism has been designed — reusing some concepts but
**completely superseding the previous cache design.**

The attached **CMS07.0** (`cnr3_cache_manager_design_v7_0.md`) is the new
controlling design authority. It is **final and complete**: all verify items
(V1–V8) are resolved — several against authoritative sources (the CNR2 reference
source code; the local R76 `VapourSynth4.h` header) — and a section-level
bring-across audit of the old CMS06.11 body has been completed (§9A), so
still-valid hard rules are stated inside CMS07.0 itself. You do not need the old
CMS06 spec to implement; it is reference history only. Please study CMS07.0
carefully before responding.

**Precedence (read this first).** Two distinct situations, opposite responses:
1. If CMS07.0 conflicts with — OR is merely unclear in its alignment with — any
   prior handover document (including Document A), decision log, memory, or earlier
   coding discussion, **CMS07.0 wins** unless I explicitly say otherwise. This
   prevents CMS06-era assumptions leaking back in.
2. If CMS07.0 **itself** is ambiguous, silent, or incomplete on an implementation
   point, that is NOT a "CMS wins" case — **stop and ask. Do not guess.** "Final and
   complete" means controlling unless explicitly revised; it does not mean every
   implementation detail is pre-specified, and it never licenses improvising.

**Document set for this work:** this introduction, CMS07.0, Document A (project
context / standing rules — reference only, subordinate to CMS07.0 per the precedence
rule above), and any current code snapshot after the `.h`/`.cpp` → `.txt` transition.
Documents B and C (decision log, volatile state snapshot) are deliberately NOT in
scope — CMS07.0 §9A already carries forward the still-valid rules, and excluding B/C
removes the main route for stale CMS06-era rules to re-enter.

**All existing cache-related code and design is explicitly superseded.**

Some code remains useful — the pixel-computation layer, response-table creation,
and memory diagnostics may be largely cache-independent and it is anticipated that
these may possibly be reused in part or in whole (D27/D28 *require* reusing
the existing frame-processing boundary rather than writing new pixel logic).

## Build / transition (decided)

Rename all existing `.h`/`.cpp` files **except `VapourSynth4.h` and `VSHelper4.h`**
to `.txt`, so old code stays available as reference for verifiable salvage but leaves
the active build.

The old binary need not build; GitHub CI may break for now; builds
will be done in **Visual Studio 2026**.

Being a new design/development, Phase/SubPhase numbering will restart
and be your choice, based on the latest agreed rules for
Phase/SubPhase numbering.

Existing rules requiring high levels of instrumentation and excellent
code commenting also apply to this development.

**Do not act on this section yet.** Do NOT perform the `.h`/`.cpp` → `.txt`
rename, nor create any files, merely from reading this introduction. First confirm
your understanding and the planned transition steps, and wait for my explicit
instruction to apply them. This brief is read-understand-and-propose, not act.

## Do NOT conflate old and new concepts or designs

Three highest-risk traps:

1. Treating pinning as optional/deferred — it is now the **mandatory** correctness
   baseline (this supersedes old decision D13).
2. Reintroducing held-ref-only predecessor reservation — superseded by
   **consumer-held pins** on a per-invocation pin-list.
3. Thinking of a checkpoint as a pin — a checkpoint is now
   a **separate eviction-protection flag** with its own retention rule.

There is exactly one pin concept (consumer-claim).

**Where old code and CMS07.0 disagree or are ambiguous in alignment,
the spec wins; old `.txt` code is salvage reference only.**

## Two further traps — engineered guards you must respect

**4. The atomic-scope register (CMS07.0 §8.7) is designer-owned and inviolable.**
Every cache critical section is enumerated as AS1–AS7, each stating exactly what
happens inside one lock acquisition and in what order. You implement these
EXACTLY. You may not shrink, split, merge, or reorder the contents of any scope.
If implementation reveals an operation the register does not cover, you raise it
back to me — you do not improvise an ad-hoc smaller lock.

**5. The V5 firewall (CMS07.0 §8.6): core refcount atomicity gives you NOTHING
over lock scopes.** VapourSynth's internal frame reference count is atomic. That
protects a single `addFrameRef`/`freeFrame` operation only. It is NOT a licence
to take a pin outside the cache lock, or to reason "the refcount is already
atomic, so this critical section can be smaller." The protected thing is the
multi-step decision (find-then-pin, decide-then-detach), not the refcount bump.
Shrinking a scope on refcount-atomicity grounds reintroduces exactly the TOCTOU
races this architecture eliminates.

## Hard rules stated in the spec — read these sections first

- **§8.6 / §8.7** — the V5 firewall and the atomic-scope register AS1–AS7 (above).
- **§9A.1 — VS-LIFECYCLE-01** (hard VapourSynth API rule): any source frame
  retrieved in arAllFramesReady must have been requested in arInitial of the same
  activation. All request planning therefore happens at arInitial (AS1).
- **§9A.2 — RC1–RC8 reference-count discipline.** Obligations are mandatory;
  the old helper NAMES are indicative — you will propose new names, but every
  obligation (single store helper, single remove helper, error-path rebalance,
  exit-path free, shutdown clear with pin-count warning, balance validation,
  first-in-best-dressed idempotency) carries in full.
- **§9A.3 — RAII owned-ref wrapper is BASELINE**, not optional. The old code's
  explicit manual ref handling was an accepted legacy interim; new code uses the
  move-only RAII wrapper from the start.
- **§9A.5 / §9A.6 — hard-ceiling abort policy and failure-path cleanup.** Every
  failure path discharges the frameData pin-list first, then frees remaining
  owned refs; a ceiling abort is also a CR4-violation signal.
- **§10.2 — coherence rules CR1–CR5** must be codified as comments directly above
  each constant's definition, including the decay_margin bounds.
- **§10.4 / §10.5 — instrumentation discipline and the required end-of-run
  recovery-search summary** (depth histogram, terminated-on split, holes-filled,
  zero-count lines omitted; counter bumps may be in-lock, formatting/emission
  outside).
- **§9A.8 — Design Compliance Review**: after each phase or coherent block, the
  17-item checklist runs, including item 17: every critical section matches its
  AS register entry exactly.

## First milestone — prove ownership before behaviour (CMS07.0 §11, per D30)

In isolation, **no VapourSynth wiring yet**, build the cache-manager core:

- data structures: slot = `VSFrame*` ref + frame number + pin_count + is_checkpoint;
  ordered frame-number index; non-checkpoint + checkpoint pools; hot-zone state;
  per-invocation pin-list;
- the single cache-wide-lock skeleton with the inside/outside-lock discipline,
  implementing the atomic scopes per the AS register;
- pin / unpin + pin-list record/discharge + single-ownership/null-on-consume +
  discharge-before-free ordering, with the RAII wrapper as baseline;
- the composite eviction predicate + bounded prune (decide+detach in lock, batch
  `freeFrame` outside, K-bound), through the single remove helper (RC2).

Prove pin/unpin balance = 0, lookup-ref balance = 0 (acquired == released +
transferred), no leaks, no double-free, that eviction never selects a pinned /
checkpoint / in-zone slot, and that shutdown clear() releases everything with a
warning on any non-zero pin — **before** any `getFrame` integration.

**Please PROPOSE the file/header/structure layout (as text, for review) — do not
create any files yet.** Base it on CMS07.0 and your experience: a draft program
structure design aligned with separation of responsibilities (e.g. pixel processing
must not own cache/scheduling policy; the cache manager must not contain pixel logic,
etc). Include the likely `.h` files, the `.cpp` files, internal structures within
each, the includes, and the function names/signatures (with function purpose/parameter
comments) needed to comply with CMS07.0 (some of which is noted above). Propose it
back to me; actual file creation follows only after my review and explicit sign-off
(per "do not act on this section yet" above).

## Second step — salvage

Once the core is proven, identify old code verifiably safe to reuse (eg perhaps
from response tables, memory diagnostics, the pixel/frame-processing layer
including the explicit-predecessor boundary) for copy/modify from the `.txt`
files into the right new locations, preserving separation of responsibilities.

**Salvage warning — the CNR2 reference source (CMS07.0 §12B, §13 V8.1).** The
upstream CNR2 source (github.com/Asd-g/AviSynth-vsCnr2) is prime salvage for the
PIXEL layer: response-table construction, the int64-accumulator weighted blend
with `shift2 = 2×depth` (its high-bit-depth overflow fix), `downSampleLuma`, and
the in-compute scene-change detection. **You MUST NOT adopt its recovery or
predecessor logic.** CNR2 is serialized and, on a non-sequential request,
substitutes SOURCE[n-1] as an approximate predecessor
(`last_frame != n-1 → child->GetFrame(n-1)`). That approximation is precisely
what CNR3's cache and recovery architecture exists to eliminate. Salvage the
pixel maths; never the `GetFrame` recovery shortcut.

## Prevailing rules — please enumerate ALL of them back to me

CMS07.0, Document A, and rules agreed with me during development of the
now-superseded cnr3 carry a set of standing coding / process / design / safety
rules, for example:

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
- the RC1–RC8 reference-count discipline (CMS07.0 §9A.2);
- the atomic-scope register AS1–AS7 as inviolable boundaries (CMS07.0 §8.7);
- the V5 firewall (CMS07.0 §8.6);
- VS-LIFECYCLE-01 (CMS07.0 §9A.1);
- RAII owned-ref wrapper as baseline (CMS07.0 §9A.3);
- bounded-start honesty (CMS07.0 §9A.7);
- the Design Compliance Review and its 17-item checklist (CMS07.0 §9A.8);
- instrumentation discipline and the required recovery-search summary
  (CMS07.0 §10.4/§10.5);
- the fmParallel final-goal invariant.

**Please list every prevailing rule you can identify from the aforementioned sources
(CMS07.0, Document A, and our prior agreements), each numbered and stated briefly but
with enough detail so that it is clear to a human, so that I can confirm, modify,
supersede, or retire each one explicitly before this process coding begins. A new
list of rules will thus be created applying to this new design and development.**

**Do not assume any rule carries over silently — surface them all for my sign-off.**

## Please respond with

a) confirmation of your understanding of the restart and the old/new separation;
b) any questions on CMS07.0;
c) the enumerated prevailing-rules list;
d) your proposed file/header/structure layout (as a text proposal — no files yet).

**Do you have any comments or questions?**
