# CNR3 — COORDINATOR DIRECTIVE to DESIGNER (ND): plan-trace view (a) sort key + tick/seq invariant → fold into spec v2.2

**Context:** resolves the view-(a) ordering question you raised. This is clarification/tightening of the
already-settled design (tick-not-datetime is closed, §6) — NOT a design change. Fold all three items into
spec v2.2 with the reasoning bound inline per the anti-lossy convention. No need to route v2.2 back for
re-review; carry it forward into the DIAG.3c scope.

---

## 1. Wording fix (§8 view (a) label)

Change **"sort by enter_datetime, phase"** → **"sort by enter-tick (display column: UTC)"**.

Read against §6 this always meant the enter-TICK, with UTC as the printed column — but the old label could
be misread as a string sort on the formatted datetime. Pure clarification; prevents a wrong implementation.

## 2. Decision — view (a) sort key is the ordered tuple (enter_tick ASC, action_seq ASC)

```text
view (a) primary  = enter_tick   ASC
view (a) tie-break = action_seq   ASC
phase is NO LONGER a sort term (it remains a DISPLAYED field).
```

**Rationale (bind inline next to the decision):**
- **enter_tick primary** gives the printed UTC timestamp column a MONOTONIC ASCENDING read down the block —
  the least-surprising thing for a human scanning timestamps in a human-primary copy-pasteable format. That
  readable-column property is the whole reason view (a) exists.
- **action_seq tie-break** gives a DETERMINISTIC, GAP-FREE TOTAL ORDER. It is globally unique per instance
  (it is the lock-protected counter — see §3), so it resolves EVERY remaining tie by itself. This is why it
  supersedes "phase" as the tie-break: phase only distinguishes O from R and leaves same-tick ties among
  DIFFERENT frames (TT / RR) unresolved and non-deterministic; action_seq resolves all of them uniquely.
- **O-before-R still falls out naturally** — so dropping phase as a sort term loses nothing. Within one
  frame the O record is entered and committed BEFORE the R record, so O carries the lower action_seq and
  sorts above its own R automatically. Pairing/readability is preserved by construction; do not re-add phase
  as a tie-break "to be safe" — it is strictly coarser than action_seq and redundant.

## 3. Invariant — state explicitly, with the failure mode bound to each half

The tick/seq capture discipline is load-bearing and must be stated so a future coder cannot "optimize" it
into a race. A record's (enter_tick, action_seq) pair is formed as:

```text
- enter_tick  : sampled OUTSIDE the diagnostics mutex (a cheap monotonic clock read, carried INTO the lock
                with the record). WHY NOT MOVE IT INSIDE: moving the clock read inside the lock needlessly
                serializes it behind the buffer mutex — contention for zero benefit.
- action_seq  : bumped INSIDE the diagnostics mutex, in the SAME critical section as the buffer append.
                WHY IT MUST STAY INSIDE: moving the increment outside the lock breaks uniqueness and
                monotonicity under fmUnordered — two threads could interleave the read-increment-write and
                collide, which destroys action_seq's ONE JOB as the deterministic tie-break (§2). This is
                the load-bearing reason it is lock-protected; it is not incidental.
```

Note this matches the existing §8 sequence ("capture timestamp OUTSIDE the lock; briefly lock to write the
record + bump action_seq; format/emit OUTSIDE the lock") — item 3 makes the WHY explicit so the ordering
survives a well-meaning refactor. State the failure mode, not just the rule: a coder who sees only "don't
move this" may judge a move safe; a coder who sees "moving it reintroduces the fmUnordered race" will not.

---

## Why these are safe to fold without re-review
All three are clarification of settled design: (1) is a label fix, (2) makes an already-implicit choice
explicit with rationale (the spec structure already keyed view (a) on the tick and cast action_seq as the
sortable commit-order column), (3) states a discipline the spec already follows. No decision is reversed;
nothing new is introduced. Next checkpoint is the DIAG.3c scope itself — bring these into it, and only flag
for a second set of eyes if something about the 3c packaging (3c.1/3c.2) turns out to interact with them.
