# CNR3 A1 Plan-Trace Analysis Tool Spec v0.3 — Feedback Suggestions for Investigation

**Status of this note:** These are suggestions for the spec author to investigate, comment on, and refine before making any changes to the specification. They are not directives and should not be treated as final design rulings.

**Overall view:** The v0.3 spec is strong. It is mostly plain-English, and Part A does a good job onboarding a coder who has not worked on CNR3 before. It explains the recursive nature of CNR3, why the private output cache exists, the distinction between slots/pins/hot zones/checkpoints, recovery, the five frame origins, and why PLANTRACE exists.

The main improvement area is not the CNR3 primer itself. The main improvement area is making the analysis vocabulary more explicit and less jargon-heavy so the implementer does not accidentally repeat the same kind of ambiguity we are investigating in D-SUM-01.

---

## 1. Clarify O and R terminology without changing the current source fact

The spec can keep saying that, in the current normal live path, O records are emitted from `arInitial` and R records are emitted from `arAllFramesReady`.

However, it should define the letters by meaning rather than by callback name:

- **O = Original Plan / Open Plan** — the plan formed when the requested frame is first handled.
- **R = Result of Plan** — the result record showing what happened when the plan completed.

Suggested wording:

```text
An O record is the Original/Open Plan record. In the current source it is emitted
from arInitial when the plan is formed.

An R record is the Result-of-Plan record. In the current normal successful live
paths it is emitted from arAllFramesReady when the plan reaches its terminal
result.
```

This wording preserves the current implementation fact while preventing a future reader from thinking that `R` means the callback itself. It also leaves room for failure/minimal-result records, if any, which may not follow the normal success placement.

It may also be useful to include a short current-source placement table:

| strategy / path | O placement | R placement, current normal success path |
|---|---|---|
| `CACHE_HIT` | `arInitial` | `arAllFramesReady` |
| `FRAME0` | `arInitial` | `arAllFramesReady` |
| `PRED_PRESENT` | `arInitial` | `arAllFramesReady` |
| `RECOVERY_EXACT` | `arInitial` | `arAllFramesReady` |
| `RECOVERY_FLOOR` | `arInitial` | `arAllFramesReady` |

The important wording is: **R is a result record, not a synonym for the callback.**

---

## 2. Explain ticks in plain English: they measure both duration and order

The spec already says that tick fields are the real ordering keys. That is correct, but a newcomer may read `enter_tick` and `exit_tick` only as performance timing fields.

Add a plain-English explanation like this:

```text
Each O or R record has an enter_tick and an exit_tick. These ticks are useful in
two different ways.

First, they measure duration: exit_tick - enter_tick tells how long that stage
took.

Second, they measure order: if we sort many records by a tick, we can see which
frame reached that stage first, second, third, and so on.
```

Then state the four order surfaces explicitly:

| order surface | sort key | plain-English question |
|---|---|---|
| arInitial received/request order | `O.enter_tick` | Which requested frame entered planning first? |
| arInitial finish/plan-decision order | `O.exit_tick` | Which frame finished plan formation first? |
| result-entry/readiness order | `R.enter_tick` | Which frame entered the result phase first? |
| production/return completion order | `R.exit_tick` | Which frame finished and returned first? |

Avoid using a single phrase like “execution disorder” unless the report says which order surface is being measured.

---

## 3. Replace or explain “inversion” jargon

The idea is good, but the word “inversion” may be too abstract for a plain-English spec.

Before giving formal metric names, explain the idea like this:

```text
After sorting events by time, look at the frame numbers.

If the frames appear as 0, 1, 2, 3, 4, that order surface is frame-sequential.

If the frames appear as 0, 4, 1, 2, 3, then the stream went forward to frame 4
and then backward to frame 1. That backward step is evidence of out-of-order
behaviour on that order surface.
```

Then define any metrics in plain language:

```text
adjacent_backward_steps:
  After sorting by the selected tick, count neighbouring pairs where the later
  record has a smaller frame number than the previous record.

max_rank_displacement:
  Compare where each frame appears in time order with where it would appear in
  simple frame-number order. Report the largest movement.

frames_with_nonzero_displacement:
  Count how many frames did not appear in the same position they would have had
  in frame-number order.
```

Consider avoiding more complex metrics such as “global inversion pairs” in the first tool version unless they are also explained plainly.

---

## 4. Clarify one plan, one O, one R — and how to handle repeated frame numbers

The current model says each plan produces exactly one O and one R sharing the same requested frame number. That should remain the central rule.

The possible ambiguity is not “one plan has multiple O/R records.” The possible ambiguity is: can the trace contain more than one plan for the same frame number?

Suggested wording:

```text
Each plan should produce exactly one O record and exactly one R record for the
same requested frame number.

Normally, a frame number should appear in only one plan within the trace window.
If the log contains more than one plan for the same frame number, do not merge
or overwrite them silently. Keep all such plans and report a duplicate-plan-for-
frame anomaly.
```

For implementation, this means the tool should not use a simple `dict[frame] = Plan` that can overwrite an earlier plan. Either store `dict[frame] -> list[Plan]`, or use a strict pairing structure that records an anomaly if a second plan for the same frame appears.

---

## 5. PLANTRACE should remain the primary evidence; D-SUM summaries are comparison data

The tool’s main source of independent evidence should remain `[DSUM-PLANTRACE]`.

Because the motivating issue is that one D-SUM summary counter may be wrong or too narrow, the tool should not treat D-SUM-01, D-SUM-04, D-SUM-12, or any other summary as ground truth.

Suggested wording:

```text
D-SUM summary rows are plugin-reported counters under test. The tool captures
selected summary rows only so it can compare them against the independent
PLANTRACE reconstruction.

If PLANTRACE reconstruction and a D-SUM summary disagree, the tool reports the
disagreement. It does not assume the D-SUM summary is correct.
```

The parser/data model should therefore probably capture selected summary families as comparison data:

- D-SUM-01 for request/order counters;
- D-SUM-04 for lookup-site and frame-lifecycle counters;
- D-SUM-12 for branch/origin totals;
- possibly selected D-SUM-07/D-SUM-08 rows where lifecycle ties are being checked.

But these should be labelled as **reported summary counters**, not as authority.

---

## 6. Reword lookup reconstruction as an estimate or expectation, not exact proof

Plans and cache-probe counters are different things.

The O/R plan structure says what the plan intended and what the result reported. It does not necessarily record every individual cache probe, nor every counted/non-counted lookup-site distinction.

Therefore Q-A should not claim that exact lookup counts can be reconstructed from plan structure alone.

Suggested wording:

```text
From O/R plan structure, the tool may estimate the lookups implied by the plan.
This is a plan-implied estimate, not exact probe accounting.

The estimate is still useful: it can be compared against the plugin-reported
D-SUM-04 lookup and lookup-site counters to identify likely accounting gaps.
But any difference must be reported as an investigation lead, not as automatic
proof that either side is wrong.
```

If the tool uses D-SUM-04 lookup-site rows, it should still keep the distinction clear:

- PLANTRACE-derived values = plan-implied expectations / estimates;
- D-SUM-04 rows = plugin-reported counter values;
- source code = final authority for what should be counted where.

This prevents the tool from conflating plan semantics with counter semantics.

---

## 7. Add coverage wording to every query verdict

Plantrace windows can be deliberately limited because the output is voluminous. A query result should therefore always say whether it describes the full run or only a trace window.

Suggested query output line:

```text
COVERAGE:
  PLANTRACE window 0..500. This verdict applies only to that window, not to the
  full run.
```

Or, when appropriate:

```text
COVERAGE:
  PLANTRACE appears to cover the full observed run. This verdict applies to the
  full traced run.
```

A limited window can still be representative and useful. The point is to label the conclusion honestly.

---

## 8. Resolve the build-scope contradiction

The status section says the build scope is ruled: full question set up front, one function each. Later, the spec still says the build-scope decision is open.

If the decision is now “one function per question,” replace the open-decision text with something like:

```text
Decision 2 - build scope: RESOLVED.
Implement one named function per question. Functions may be built and verified
in easiest-first order, but the public query list and output envelope should be
stable from the start.
```

That removes a source of confusion for the implementer.

---

## 9. Add a minimal expected-output contract

The spec should show at least one sample query block. This helps prevent ad hoc print formatting that is hard to compare between logs.

Suggested skeleton:

```text
QUERY: arInitial received order

METHOD:
  Sort O records by O.enter_tick and compare the resulting frame sequence with
  simple frame-number order.

EVIDENCE:
  First 20 frames by O.enter_tick: 0, 1, 4, 2, 3, ...

COUNTS:
  adjacent_backward_steps: 1
  max_rank_displacement: 2
  frames_with_nonzero_displacement: 3

COVERAGE:
  PLANTRACE window 0..500. This verdict applies only to this window.

VERDICT:
  arInitial received order was not frame-sequential in this window.

ANOMALIES AFFECTING VERDICT:
  none
```

A consistent block shape will make the tool easier to test and easier to compare across runs.

---

## 10. Preserve the canonical-model-first rule

The current rule is good and should stay:

- parsing, canonical records, pairing, sensibility checks, set/list reconciliation, and ordering-integrity checks should be pure Python over the canonical model;
- pandas, if used, should be an optional scalar projection only;
- pandas output should have explicit dtypes and parity checks against pure-Python counts where cheap.

This is a good balance between auditability and convenience.

---

## Final suggested summary for the spec author

```text
The v0.3 spec is strong and mostly plain-English. It gives enough CNR3/cache/
plan context for a coder who has not worked on CNR3 before.

The suggested improvements are mainly about making the analysis vocabulary safer
and clearer:
- define O/R by meaning: Original/Open Plan and Result of Plan;
- keep the current source fact that normal success R records are emitted from
  arAllFramesReady;
- explain ticks as both duration fields and ordering keys;
- explain out-of-order metrics in plain English before naming them;
- clarify how repeated frame numbers / duplicate plans are handled;
- treat D-SUM summaries as reported counters under test, not as ground truth;
- describe lookup reconstruction from plantrace as an estimate/expectation, not
  exact probe accounting;
- label every query result with full-run vs window-limited coverage;
- resolve the build-scope contradiction;
- add one sample output block.

These are suggestions for investigation and comment before changing the spec.
```
