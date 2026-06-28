# CNR3 STEP 0 — Joint Review Process and Rules (v1.1)

**Purpose.** Step 0 is the joint CMS sensibility / gap review for **hot-zone + prune live
wiring**, performed BEFORE any wiring patch. It exists because the prune and hot-zone
componentry is built and selftest-proven but has ZERO live callers, and we will NOT assume the
CMS is reliable-as-is for the live wiring merely because the componentry exists. Step 0 decides
whether the CMS is still sensible and complete against the post-P.11C.5 implementation state, and
produces an agreed wiring contract (and any CMS clarification/bump that review warrants).

**This document defines HOW the three of us run that review.** The review itself lives in a
separate single shared file, the **Findings Register** (`CNR3_Step0_Findings_Register_*.md`).

*(v1.1 change: adds §9 — a REQUIRED hand-off summary blurb at the top of every relay, leading with
any architecture/terminology correction. No other rule changed.)*

---

## 0. Roles

- **D — Designer** (holds design intent; CMS/architecture lens).
- **C — Coder** (holds implementation feasibility; live-path/lifecycle lens).
- **X — Coordinator** (you; final authority, relays the file between D and C, holds the
  authoritative copy and the comment-sequence counter).

D and C are the two reviewing parties. X is authority and does not have to raise findings, but
MAY, and rules disputes.

---

## 1. The single-register principle (no merge step)

There is **ONE** findings file, not one-per-party. It is append-only and passed back and forth.
Each party adds their own findings and comments INTO the same file. There is deliberately **no
"consolidation" step**, because a merge of two separate documents is exactly where findings get
silently dropped. The register is always the single source of truth; X holds the authoritative
copy and relays it.

Round flow:
1. **Round 0 (seed):** D populates the register with the opening designer findings + the scope.
2. **Round 1:** X relays to C. C appends `SR-C-*` findings and comments on D's findings. Returns to X.
3. **Round 2:** X relays to D. D comments on C's findings, raises any new `SR-D-*`, updates statuses.
4. **Iterate** (rounds 3,4,…) until the exit condition (§6) is met.

The round number increments each time the file changes hands between D and C (via X).

---

## 2. Finding IDs (stable, owner-tagged, never reused)

Format: **`SR-<OWNER>-<NN>`**
- `SR` = Step 0 Review.
- `<OWNER>` = `D`, `C`, or `X` — the party who FIRST raised the finding. Owner never changes.
- `<NN>` = zero-padded sequence within that owner, increasing, never reused (`SR-D-01`,
  `SR-D-02`, `SR-C-01`, …).

Rules:
- **Append-only.** A finding is NEVER deleted or renumbered. If it turns out to be wrong or moot,
  it is not removed — its status becomes `WITHDRAWN` (by its owner) or `REJECTED` (by ruling),
  with reasoning, and it stays in the register as history.
- A finding raised by one party is never re-owned by the other. If C's comment on `SR-D-03`
  spawns a genuinely new concern, C raises it as a new `SR-C-*` and cross-links it.

---

## 3. Finding anatomy

```
### SR-D-03   [severity: MAJOR]   [status: OPEN]
Anchor: <CMS section / source symbol / specific question>  e.g. "CMS §6.3 / execute_bounded_prune_pass"
Finding (D): <one clear statement of the concern, gap, or question>

Comments:
<comment lines — see §4>
```

- **severity** (set by the finding's owner; may be revised by ruling): `BLOCKER` / `MAJOR` /
  `MINOR` / `NIT`.
  - `BLOCKER` = must be resolved or explicitly X-waived before ANY wiring code.
  - `MAJOR` = must reach `AGREED`/`RESOLVED`/`DEFERRED` before wiring code.
  - `MINOR` / `NIT` = should be addressed but do not gate the wiring.
- **status** (moves only per §5): `OPEN` / `ACKNOWLEDGED` / `AGREED` / `DISAGREED` / `PARTIAL` /
  `DEFERRED-to-<arc>` / `RESOLVED` / `WITHDRAWN` / `REJECTED`.
- **Anchor** is mandatory and concrete — a CMS section, a source symbol, or a specific question.
  No anchor = not reviewable.

---

## 4. Comment anatomy (the heart of the protocol)

Every comment is ONE line header + reasoning:

```
[#<SEQ> | <AUTHOR> | r<ROUND> | <verdict>]  <reasoning — REQUIRED, always>
```

- **`#<SEQ>`** = a single GLOBAL, monotonically increasing integer across the WHOLE register
  (not per-finding). Whoever appends takes the next integer above the highest `#` currently in the
  file. This gives an unambiguous global order of who-said-what-when, independent of timezones/clocks
  (we deliberately do NOT use datetime for ordering). X is the keeper of the next-free number; in
  practice each appender just uses `max(existing)+1`.
- **`<AUTHOR>`** = `D` / `C` / `X`. Every comment is authored; no anonymous comments.
- **`r<ROUND>`** = the round in which the comment was added.
- **`<verdict>`** = one of: `agree` / `disagree` / `partial` / `ack` / `question` / `withdraw`
  (owner withdrawing own finding) / `RULING` (X only).

**Reasoning is REQUIRED for EVERY verdict — including `agree`.** A bare "agree" is almost as lossy
as a silent drop: it records no *why*, so a later reader (or a memoryless coder restart) cannot tell
a considered agreement from a rubber-stamp. So:
- `agree` must state WHAT is being agreed and confirm/narrow the scope ("agree — fully covered by
  §5.7, no new work needed").
- `partial` must state which part is accepted, which is not, and why.
- `disagree` must give the counter-reason.
- `question` poses a specific question back; it does not resolve anything.

**The honest-agree rule:** an `agree` that only holds for a NARROWER claim than the finding states
is NOT an `agree` — it is a `partial`, with the narrowing written out. This stops scope from
quietly shifting under cover of agreement.

Comments are append-only too: never edit or delete a prior comment. To change your mind, add a NEW
comment with a new sequence number.

---

## 5. Status transitions (the only legal moves)

A finding's status reflects the STATE OF AGREEMENT between its owner and the other reviewing party:

- `OPEN` → `ACKNOWLEDGED`: the other party has read it (an `ack` comment) but not yet verdicted.
- `ACKNOWLEDGED` → `AGREED`: BOTH parties' LATEST verdicts are `agree` (with reasoning).
- `→ PARTIAL`: latest verdicts conflict in scope (one `agree`, one `partial`, or both `partial`
  on different parts). Stays `PARTIAL` until reconciled or ruled.
- `→ DISAGREED`: latest verdicts are opposed (`agree` vs `disagree`, or `disagree` standing).
- `→ DEFERRED-to-<arc>`: both parties (or X) agree the concern is real but belongs to a later arc
  (e.g. `DEFERRED-to-fmParallel` for concurrent-prune questions). Must name the arc.
- `→ RESOLVED`: the finding's question is answered AND both latest verdicts align (or X ruled).
  A `RESOLVED` finding records, in its final comment, the agreed answer/decision.
- `→ WITHDRAWN`: the OWNER withdraws it (a `withdraw` comment with reasoning). History retained.
- `→ REJECTED`: X rules it out (a `RULING` comment). History retained.

**Hard rules:**
- A finding CANNOT reach `RESOLVED` on one party's say-so. Either both latest verdicts align, or X
  issues a `RULING`.
- `disagree` does NOT close a finding. A disagreement keeps it `DISAGREED` (open) until reconciled
  or X rules. You cannot self-close by disagreeing.
- Severity may only be raised/lowered by the owner (with reasoning) or by X `RULING`.

---

## 6. Exit condition (when Step 0 is done)

Step 0 is complete when, in a full round, NO new findings are raised AND:
- every `BLOCKER` is `RESOLVED`, `DEFERRED-to-<arc>`, or explicitly `X`-waived (a `RULING`); and
- every `MAJOR` is `RESOLVED`, `DEFERRED`, `WITHDRAWN`, or `REJECTED`; and
- `MINOR`/`NIT` items are each at least `ACKNOWLEDGED` with a disposition.

**Step 0 output (the deliverable):** the converged register IS the output. From it, D produces:
1. the agreed **live wiring contract** (hot-zone observation point + prune-trigger contract:
   when the store path fires prune, and the lifecycle-safety argument), and
2. any **CMS clarification or version bump** the review warranted (a legitimate, expected output —
   not a precondition that was skipped).

Only after Step 0 exits do we scope the actual wiring (hot-zone observation first, then prune).

---

## 7. Mechanical conventions (to keep the file clean across hands)

- **One file, plain Markdown, LF line endings.** Do not reflow or reformat others' text.
- **Never renumber** findings or comments. Append only.
- When you hand the file back, do nothing else to it — no silent edits to another party's rows.
- If you must correct a typo in your OWN earlier comment, do NOT edit it; add a new comment noting
  the correction (append-only is absolute — it is what makes the register loss-proof).
- X keeps the authoritative copy. If two edits ever collide, X's copy wins and the loser re-applies
  their additions as new (higher-numbered) entries.
- Keep findings atomic: one concern per finding. If a finding has two separable parts, split it.

---

## 8. Quick start for each party

**Designer (D), Round 0:** populate the register with the opening designer findings (the scope
questions for the prune-trigger contract, hot-zone wiring point, lifecycle safety vs the active
pin_list and the arInitial→arAllFramesReady gap, single-activation-now vs concurrent-later),
each with an anchor + severity. Hand to X.

**Coordinator (X):** relay the file to C. Keep the authoritative copy. Track the next-free comment
sequence number. Relay back and forth each round. Rule on `DISAGREED`/`PARTIAL` stalemates when asked.

**Coder (C), Round 1:** read D's findings; add an `ack`/`agree`/`partial`/`disagree`/`question`
comment (with reasoning) under each one you have a view on; raise your own `SR-C-*` findings for
anything D missed (especially implementation-feasibility and lifecycle-safety concerns). Hand back to X.

Then iterate per §1 until §6 is met.

---

## 9. Hand-off summary blurb (REQUIRED at the top of every relay)

Every time the register changes hands (D→X→C or C→X→D), the party who just finished a round MUST
place a short **HANDOFF SUMMARY** blurb at the TOP of the file, above the Register Control Block,
addressed to the next receiver. It is an orientation aid so the receiver gets the round's shape in
one read before diving into the threads.

The blurb MUST contain, briefly:
1. **Who → who** and which round (e.g. "r2 → C").
2. **Any CORRECTION that changes shared understanding FIRST** — especially a terminology or
   architecture correction (e.g. "there is no checkpoint pool; it is a flag on the unified cache").
   Lead with it; a stale shared model is the most dangerous thing to carry into the next round.
3. **What was decided this round** (the findings that reached AGREED / RESOLVED / DEFERRED, one line each).
4. **What is OPEN and needs the receiver** (questions, rulings, confirmations).
5. The **next free comment seq** and whether the **exit condition** is met.

Rules for the blurb:
- It is an INDEX, not a substitute. It never replaces, edits, or contradicts the findings/comments
  below it; if the blurb and a finding ever disagree, the finding wins and the blurb is wrong.
- It is REPLACED (not appended to) each round — it always reflects the CURRENT hand-off only. (This is
  the ONE part of the file that is overwritten rather than appended; the findings and comments remain
  strictly append-only. The blurb is disposable orientation; the register below it is the record.)
- Keep it short — a screenful. Detail lives in the findings, not the blurb.
- It is markdown blockquote (`>`) styled so it is visually distinct from the register body.

Rationale: this convention was adopted after r2, when a stale "checkpoint pool" term (the design is a
unified cache with a checkpoint FLAG) risked propagating into the next round. A leading correction
blurb makes such shared-model fixes impossible to miss.
