# CNR3 A1 — Plan-Trace Log Analysis Tool — SPECIFICATION v0.2

**Track:** A1 (external analysis; NOT plugin code). No plugin gate / macro-off / 4-way / proof
discipline applies. This is a standalone Python 3.14+ program that reads a CNR3 run log with
`[DSUM-PLANTRACE]` emission enabled and answers structural questions about plan/recovery behaviour and
execution ordering.

**Author role:** designer (W3D) spec → coder (W3C) implements → coordinator (W3X) runs against real logs.

**Interpreter:** Python 3.14+. The tool imports **nothing VapourSynth-specific** — it only reads a text log
file — so it runs on either the embedded portable-VapourSynth Python (3.14+) or a higher 3.14.x global
install, with no change. Run it wherever is convenient; the VS interpreter is not required.

**Status:** v0.2. §10 decision 1 (stdlib vs pandas) is now **resolved**: stdlib is the canonical model and
carries all trust-critical logic; pandas is an **opt-in aggregation/presentation layer over a scalar
projection only** — never the parse target, never the canonical store, never in the reconciliation path
(§3.1, §7 tags). Decision 2 (build scope) remains open. Log-format section verified cold against `src/` at
marker `CMS07-DIAG.honest-cache-hit-metrics`.

**Changes from v0.1:** resolved the stdlib/pandas decision as a hybrid; added the `plans_dataframe` scalar
projection and its dtype/parity rules (§3.1); tagged every §7 query pure-Python vs pandas-eligible; added
the parity-assert backstop; pinned interpreter-agnosticism.

---

## 1. Purpose — the two questions this tool must definitively answer

Everything else is scaffolding for these. The tool exists because two conclusions were reached by reasoning
and must instead be **proven from the trace**:

- **Q-B (execution order under threading).** Are arInitial requests actually *received* in frame order, and
  are frames actually *finished* in frame order, when running without `-r 1` (real fmParallel threading)?
  D-SUM-01 reported `out_of_order = 0` on L1noR, but D-SUM-01 samples a single serialising mutex at
  arInitial and may be structurally blind to true execution disorder. This tool must reconstruct the real
  dispatch order and completion order **from per-frame ticks** and state, with evidence, whether ordering is
  sequential or not — and by extension whether D-SUM-01's 0 is honest or a false negative.

- **Q-A (lookup accounting honesty).** "A cache hit is a cache hit" — every attempt to find a frame in the
  cache (as predecessor **or** as frame N, in plan formation **or** in recovery walks) should be counted.
  The tool must reconstruct, from the plan structure, how many lookups each plan *implies* and of what role,
  and reconcile that reconstruction against the D-SUM-04 `cache_lookup_queries_total / hits / misses`
  totals in the same log. Any lookup the plan structure implies but the counters did not capture is a
  finding to surface — "if it is not counted, we ask why and answer definitively."

The tool is a **question-runner**: a library of independently toggleable query functions over a parsed,
order-preserving model of the trace. Adding a question later must not require touching the parser.

---

## 2. Input — the `[DSUM-PLANTRACE]` log format (verified against source)

### 2.1 Line envelope
Every diagnostic line in the run log has the form:
```
CNR3[<inst>] INFO D-SUM-PLANTRACE: <message>
```
`<inst>` is the filter instance id (integer). A single log MAY contain multiple instances; records MUST be
partitioned by `<inst>` for any ordering analysis (ticks are only comparable within one instance/run).
Only lines whose message begins with `[DSUM-PLANTRACE]` are in scope for this tool. All other families
(`[DSUM-SUMMARY]`, `[DSUM-HEALTH]`, `[DSUM10-*]`, etc.) are ignored for parsing into the plan model, EXCEPT
the D-SUM-04 summary rows, which are captured separately for the Q-A reconciliation (§7.7).

### 2.2 Message kinds
Four message kinds under `[DSUM-PLANTRACE]`:

| kind | leading token | when | parse into |
|---|---|---|---|
| legend | `legend` | once per run, before records | legend dictionary (§8) |
| plan-open | `O` | per plan, emitted at arInitial | `OpenRecord` |
| plan-result | `R` | per plan, emitted at arAllFramesReady | `ResultRecord` |
| formatting_error | `formatting_error` | rare failure path | anomaly counter only |

### 2.3 O record (plan-open) grammar
Emitted from the arInitial RAII guard. Field order is fixed:
```
[DSUM-PLANTRACE] O enter_tick=<D20> seq=<D> frame=<KEY> exit_tick=<D20> \
  strategy=<STRAT> enter_utc=<UTC> exit_utc=<UTC> run_ms=<F> \
  target=<L> predecessor=<L> anchor=<L> floor=<L> holes=<L> sources=<L> pinned=<L> codes=<CL>
```

### 2.4 R record (plan-result) grammar
Emitted at arAllFramesReady completion. `fail_reason` is present ONLY when `outcome=FAILED`:
```
[DSUM-PLANTRACE] R enter_tick=<D20> seq=<D> frame=<KEY> exit_tick=<D20> \
  outcome=<OUTCOME> [fail_reason=<FR>] enter_utc=<UTC> exit_utc=<UTC> run_ms=<F> \
  computed=<L> adopted_skipped=<L> post_compute_discarded=<L> codes=<CL>
```

### 2.5 Field value types
| token | type | notes |
|---|---|---|
| `<D20>` enter_tick, exit_tick | uint64, zero-padded width 20 | **steady_clock nanoseconds. THE ordering key.** Monotonic within a run. Strip leading zeros on parse; keep as int. |
| `<D>` seq | uint64, zero-padded | `action_seq` — monotonic capture counter (order records were emitted). Secondary ordering key. |
| `<KEY>` frame | uint64 zero-padded, OR literal `<none>` | the plan's key frame (target N). `<none>` = invalid/absent → treat as null, flag if unexpected. |
| `<UTC>` enter_utc, exit_utc | `YYYY-MM-DDThh:mm:ss.sssZ` | **Derived** from ticks against a single anchor; millisecond resolution; approximate. Use for human display only — **do NOT use for fine ordering; use ticks.** |
| `<F>` run_ms | float, 3 dp | `(exit_tick - enter_tick)/1e6`. Convenience; recompute from ticks if cross-checking. |
| `<L>` frame list | `[a,b,c]` | comma-separated unpadded ints, no spaces. A trailing `*` on an int = checkpoint-grid frame (e.g. `120*`). Empty list = `[]`. |
| `<CL>` code list | `[a=CC,b=CC]` | per-frame composed codes, sorted by frame ascending. `CC` = 1+ role/outcome letters. Checkpoint `*` may ride the frame number (`120*=K`). Empty = `[]`. |
| `<STRAT>` | enum | see §2.6 |
| `<OUTCOME>` / `<FR>` | enum / string | see §2.6; treat unknown `FR` as opaque string + flag |

### 2.6 Enumerations and codes (from the emitted legend — embed as canonical dict, §8)
**O strategy:** `CACHE_HIT`, `FRAME0`, `PRED_PRESENT`, `RECOVERY_EXACT`, `RECOVERY_FLOOR`
**R outcome:** `RETURNED_CACHE_HIT`, `RETURNED_COMPUTED`, `RETURNED_RECOVERED`, `FAILED`

**O-item fields:** `target` = requested output frame N; `predecessor` = N-1 compute base (PRED_PRESENT);
`anchor` = recovery start frame with present output (RECOVERY_EXACT); `floor` = fresh-start base when no
anchor (RECOVERY_FLOOR); `holes` = absent output frames the plan will compute/fill; `sources` = source
frames queued for fetch (`[N]` for non-recovery branches); `pinned` = frames pinned as compute base.

**R-item fields:** `computed` = frames computed and stored this plan; `adopted_skipped` = frames found
already present, pinned, compute skipped; `post_compute_discarded` = frames computed then discarded (lost
the race).

**O-codes:** `T`=target `P`=predecessor `A`=anchor `F`=floor `H`=hole `S`=source `N`=pinned
**R-codes:** `C`=computed `K`=adopted_skipped `L`=post_compute_discarded `N`=none `X`=not_reached
`E`=error_here
**glyph:** `*` = checkpoint-grid frame.

### 2.7 Emission window
Plan-trace only emits for frames in a compile-time window `[FROM_FRAME, TO_FRAME]`
(`CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME/TO_FRAME`). Frames outside the window are **intentionally absent** and
MUST NOT be flagged as gaps. The tool accepts the window as an optional CLI parameter
(`--window FROM TO`); if omitted, it infers the window as `[min(frame), max(frame)]` observed and notes the
inference. All completeness/gap checks are evaluated **within the window only**.

### 2.8 Pairing model
Each plan produces exactly one O and one R sharing the same key `frame` (target N). The canonical
"plan" is the (O, R) pair. Pair O→R by frame within instance; when duplicates exist (multiple O and/or R
for one frame — a race signature), pair by nearest following R in tick order and record the multiplicity as
an anomaly (§5). Unpaired O (no R) or unpaired R (no O) are anomalies, not errors — record and continue.

---

## 3. Data model

- `Phase` enum {OPEN, RESULT}; `Strategy`, `Outcome` enums per §2.6; `RoleFrame = (int frame, bool
  checkpoint)`.
- `OpenRecord`: instance, action_seq, frame(nullable), enter_tick, exit_tick, enter_utc, exit_utc, run_ms,
  strategy, target[], predecessor[], anchor[], floor[], holes[], sources[], pinned[], codes{frame:str},
  raw_line, line_no.
- `ResultRecord`: instance, action_seq, frame, enter_tick, exit_tick, enter_utc, exit_utc, run_ms, outcome,
  fail_reason(nullable), computed[], adopted_skipped[], post_compute_discarded[], codes{frame:str},
  raw_line, line_no.
- `Plan`: frame, open(nullable OpenRecord), result(nullable ResultRecord), + derived convenience
  (hole_count, computed_count, adopted_count, discarded_count, lifecycle ticks).
- `Dsum04Totals`: cache_lookup_queries_total, cache_lookup_hits, cache_lookup_misses, lookup_refs_acquired,
  (+ any other D-SUM-04 rows present) — captured from the summary for §7.7.
- `TraceModel` (the global structure):
  - `records`: **the ordered list of all O/R records exactly as read** (natural capture order). This list is
    **immutable after parse** — no query mutates it; every sort returns a new list (§6).
  - `plans`: dict frame→Plan (and a list for duplicate handling).
  - `legend`: parsed legend dict + the embedded canonical dict (§8).
  - `dsum04`: Dsum04Totals or None.
  - `window`: (from, to).
  - `anomalies`: list of structured anomaly records (§5).

Use `dataclasses` (frozen where practical) and `enum`. **The canonical model — parse, records, plans,
pairing, sensibility checks, and every reconciliation/ordering query — is stdlib-only.** pandas never
touches this layer.

### 3.1 Optional pandas projection (`plans_dataframe(model) -> DataFrame`)

A single opt-in helper flattens the plans into a **scalar-only** DataFrame, one row per plan, built on
demand from the canonical model. It is a *derived view*, never the parse target and never the store.

**Rules — non-negotiable:**
- **Scalar-only rows.** Columns are scalars: `frame, instance, action_seq, o_enter_tick, o_exit_tick,
  r_enter_tick, r_exit_tick, strategy, outcome, fail_reason, hole_count, source_count, pinned_count,
  computed_count, adopted_count, discarded_count, arinitial_dwell_ns, initial_to_ready_ns,
  arall_dwell_ns, total_ns`. The variable-length list/dict fields (`holes`, `sources`, `pinned`, `codes`,
  `computed`, …) are **reduced to counts/booleans here and NOT carried** — any query that needs the actual
  frame sets works on the canonical model, not this frame.
- **Explicit dtypes at construction** — no inference: nullable `Int64` for `frame` and all tick/count
  columns (so `<none>`/missing → `pd.NA`, never a silent float `NaN`); `category` for `strategy`,
  `outcome`, `fail_reason`; `boolean` (nullable) for any flag columns. Assert the resulting dtypes match a
  declared schema before returning; raise on drift.
- **Flattening is a few visible lines** and stays auditable; no hidden coercion path.
- **Parity backstop.** For any statistic cheap to compute both ways (e.g. mean holes/plan, plan counts by
  strategy, total computed), compute it in pure Python over the canonical model **and** via pandas over the
  projection, and `assert` equality (exact for counts; tolerance only for floats). A mismatch means the
  flattening is wrong — fail loudly. This is the objective backstop, not an optional nicety.
- **Determinism.** Stable sorts only; declared column order; no reliance on dict/DataFrame ordering
  incidentals.

pandas is thus confined to counting/describing scalars (its safe sweet spot). It is absent from parsing,
the canonical store, hole-set pairing, and both reconciliation queries (§7.6, §7.7).

---

## 4. Parsing (`parse_log(path, window=None) -> TraceModel`)

1. Read the file line by line, preserving order; track 1-based `line_no`.
2. Select in-scope lines (envelope §2.1 → message begins `[DSUM-PLANTRACE]`); also skim for the D-SUM-04
   summary rows and stash them into `Dsum04Totals`.
3. Dispatch by kind (§2.2). Parse O/R with a **tolerant field tokenizer** keyed on `name=value` pairs, not
   fixed column positions — accept the fields in the documented order but do not break if whitespace runs
   differ. Unknown/extra `name=` tokens are retained in a `.extra` dict and flagged (forward-compat).
4. Append every O/R to `records` in read order (this defines natural/action order). Do not sort here.
5. Build `plans` by pairing (§2.8).
6. Parse legend lines into `legend`; merge with the embedded canonical dict.
7. Run all sensibility checks (§5), populating `anomalies`.
8. Return the `TraceModel`. Parsing never raises on malformed content — it records an anomaly and continues,
   so one bad line cannot abort analysis of a multi-thousand-frame log.

---

## 5. Sensibility checks (populate `anomalies`; each: severity, frame/line, description)

Minimum set:
- **Ordering integrity:** `action_seq` strictly increasing in read order (per instance); tick monotonicity
  where expected (`exit_tick >= enter_tick` per record; `O.enter_tick <= O.exit_tick <= R.enter_tick <=
  R.exit_tick` per plan — flag inversions, they are the interesting threading signal, not necessarily an
  error).
- **Pairing:** every in-window frame has exactly one O and one R; flag missing-O, missing-R, duplicate-O,
  duplicate-R (duplicates = race/re-plan signature).
- **Window completeness:** within `[from,to]`, every frame appears; flag interior gaps (edge-absence is fine
  per §2.7).
- **Strategy/outcome coherence:** e.g. `PRED_PRESENT` O should have a non-empty `predecessor` and empty
  `holes`; `RECOVERY_EXACT` should have an `anchor`; `RECOVERY_FLOOR` a `floor`; `CACHE_HIT` O should pair
  with `RETURNED_CACHE_HIT` R; `FAILED` R must carry a `fail_reason`. Flag mismatches.
- **Hole conservation (feeds §7.5/Q-A):** every frame in O.`holes` must appear in exactly one of R.`computed`
  / R.`adopted_skipped` / R.`post_compute_discarded` (or be explained by `FAILED`). Flag holes that vanish
  or appear un-planned.
- **Code/list agreement:** the `codes` map must be consistent with the role lists (e.g. a frame coded `H`
  appears in `holes`; coded `C` appears in `computed`). Flag divergence.
- **Formatting errors:** count `formatting_error` lines and any `.extra`/unknown-enum occurrences.

Anomalies are reported, never fatal. A clean run should report zero.

---

## 6. Sorting (non-destructive)

`sorted_view(records, keys) -> list` returns a **new** list; `records` is never reordered. Support composite
keys, at minimum: `action_seq`; `enter_tick`; `exit_tick`; `frame`; `(phase, enter_tick)`;
`(instance, enter_tick)`; `(strategy)`; `(outcome)`. Keys are passed as an ordered list of field accessors
so new combinations need no new function. Zero-padding in the raw text is irrelevant here — sort on the
parsed integer/enum values.

---

## 7. Query functions (each independently toggleable; comment in/out in `main`)

Convention: each query is a pure function `query_xxx(model) -> QueryResult` that prints a titled block and
returns structured data. `main` holds a list of enabled queries; disabling one = commenting one line. No
query mutates `model`. Each result states **method, evidence, verdict** — never a bare number.

**Layer tag (per §3.1):** each query is marked **[pure]** — implemented against the canonical stdlib model
only — or **[pandas-eligible]** — an aggregation/description over the scalar `plans_dataframe`, permitted to
use pandas, still carrying the parity backstop where a cheap pure-Python cross-check exists. The two
reconciliation/ordering-integrity queries (7.6, 7.7) and every query that inspects actual frame *sets*
(list fields) are **[pure]** by rule and never touch pandas.

### 7.1 `query_arinitial_received_order`  (Dave (a); Q-B)  **[pure]**
Sort O records by `enter_tick` (arInitial dispatch). Compare that order to frame-ascending. Report: count of
adjacent inversions, longest run of monotonic frames, max/mean displacement between tick-rank and
frame-rank, and a verdict ("dispatch is/ isn't frame-sequential"). Cross-report the D-SUM-01
`out_of_order_count` from the same log alongside this tick-derived count — if they disagree, that is the
direct evidence for/against the "D-SUM-01 is fooled by dispatch serialisation" hypothesis. **This is the
primary Q-B answer.**

### 7.2 `query_arinitial_finish_order`  (Dave (b))  **[pure]**
Same as 7.1 but keyed on `O.exit_tick` (arInitial return). Answers whether arInitial *completions* were
frame-ordered.

### 7.3 `query_arallframesready_order`  (Dave (c))  **[pure]**
Two passes over R records: by `R.enter_tick` (arAllFramesReady entry) and by `R.exit_tick` (frame produced /
completion). Report inversions and displacement for each. `R.exit_tick` order vs frame order is the true
"were frames finished in order" answer under threading.

### 7.4 `query_open_holes_stats`  (Dave (d))  **[pandas-eligible]**
Over all O records: count plans by strategy; for each and overall, report hole-count distribution — total
holes, mean holes/plan, median, max, histogram, and % of plans with ≥1 hole. Break out by strategy
(recovery plans vs pred/cache-hit). This quantifies bubbling depth. Scalar aggregation over
`plans_dataframe` (`groupby('strategy')`, `describe`, `value_counts` on `hole_count`) — carry the parity
backstop on `total holes` and `mean holes/plan` (pure vs pandas, assert equal).

### 7.5 `query_recovery_hole_fill`  (Dave (e); feeds Q-A)  **[pure]**
Per-frame set membership across the `holes`/`computed`/`adopted_skipped`/`post_compute_discarded` list
fields — inherently non-tabular, so canonical-model only, no pandas.
Over all recovery plans (O.holes non-empty), pair each hole to its R disposition: computed (C),
adopted_skipped (K), post_compute_discarded (L), or unfilled/failed. Report per-plan and aggregate:
holes identified, holes filled, holes adopted-vs-computed split, discards (race losers), and any unfilled
holes. "Were holes looked up and successfully calculated" = share landing in computed∪adopted. Unfilled or
discarded holes are the wasted/lost-work signal.

### 7.6 `query_execution_disorder_vs_dsum01`  (Q-B, explicit)  **[pure — rule]**
Dedicated head-to-head: compute the tick-derived out-of-order metric (from 7.1) and print it beside
D-SUM-01's reported `out_of_order_count`, `backward_jump_count`, `forward_jump_count`, and gap histogram.
Emit an explicit verdict on whether D-SUM-01 is a faithful or blind detector of execution disorder for this
run. Settles the open dispute with numbers.

### 7.7 `query_lookup_accounting_reconcile`  (Q-A, explicit)  **[pure — rule]**
Reconstruct, from plan structure, the lookups each plan *implies* by role: a target probe per plan; a
predecessor probe on PRED_PRESENT; anchor + per-hole probes on recovery. Sum by role. Compare the
reconstructed query total (and, where derivable, hit/miss split) to the captured D-SUM-04
`cache_lookup_queries_total / hits / misses`. Report: reconstructed vs counted, the delta, and a per-role
decomposition that *explains* the observed hit-rate (e.g. "the mandatory cold target probe misses once per
frame"). Any implied lookup not reflected in the counters, or vice-versa, is flagged as a
"lookup-accounting gap — investigate." **This is the direct Q-A answer.**

### 7.8 `query_lifecycle_spans`  (supporting)  **[pandas-eligible]**
Span columns are scalar ns integers precomputed in the projection; `describe`/quantiles over
`plans_dataframe` are safe here. The tick arithmetic that produces the spans lives in the projection builder
(visible, dtype-checked), not in pandas.
Per plan, derive the four-point lifecycle (§2.8) span durations from ticks: arInitial dwell
(`O.exit-O.enter`), initial→ready gap (`R.enter-O.exit`), arAllFrames dwell (`R.exit-R.enter`), total
(`R.exit-O.enter`). Report distributions and the max initial→ready gap — the "in-flight window" that drove
recovery on L1noR. Correlate large gaps with recovery strategy.

Additional seed queries banked (from Provenance A1 set, implement as needed): O→R divergence (planned holes
vs actual C/K/L); recovery-span distribution vs `BACK_RADIUS` (≈50); FI-11 thrash correlation (recovered
targets that were in the D-SUM-10 recently-evicted ring); arrival-disorder vs recovery-rate.

---

## 8. Field/code self-documentation (Dave's requirement 6)

Ship a single canonical dictionary in the source (a module-level structure) mapping every field name, every
strategy/outcome/fail_reason, and every O/R code letter and the `*` glyph to a one-line meaning (text from
§2.6, which is the emitted legend). Requirements: (a) it is embedded as comments AND as a printable table
(`--legend` prints it); (b) at parse time the tool compares the embedded dictionary to the legend lines
found in the log and flags drift (if the plugin's legend changes, the tool notices it is stale rather than
silently misreading). This keeps the analyser honest against future emission changes.

---

## 9. CLI and output

- `tool.py --input <logfile> [--window FROM TO] [--instance N] [--legend] [--anomalies-only] [--json OUT]`
- Default run: parse, print parse summary (records, plans, window, instance count), print anomalies, then
  each enabled query block in a fixed order with method/evidence/verdict.
- `--json` dumps the structured `QueryResult`s for downstream tooling.
- Exit code 0 on successful analysis regardless of anomalies found (anomalies are data, not tool failure);
  non-zero only on unreadable input.

---

## 10. Constraints, non-goals, open decisions

**Constraints:** Python 3.14+; deterministic output (stable sorts, fixed field order); natural-order record
list immutable post-parse; parser resilient to one-off malformed lines; must handle multi-thousand-record
logs comfortably.

**Non-goals (v0.2):** no plugin interaction; no live capture; no plotting (numbers/tables only — a plotting
layer can come later); does not re-derive frame pixels; not bound by any plugin proof discipline.

**Decision 1 — Dependencies: RESOLVED (v0.2).** Hybrid. stdlib is the canonical model and carries parsing,
the store, sensibility checks, hole-set pairing, and both reconciliation/ordering-integrity queries. pandas
is an opt-in aggregation/presentation layer over the scalar `plans_dataframe` projection only (§3.1), used
by the **[pandas-eligible]** queries (7.4, 7.8), with explicit dtypes and a parity backstop; it is barred
from the parse target, the canonical store, and the reconciliation path. pandas/numpy are already present in
both the portable-VS and global interpreters, so no install/portability cost; auditability and the
non-tabular list fields are handled by keeping the trust-critical work pure. Run on the global 3.14.x for
newest-subversion syntax; the tool needs no VS interpreter.

**Decision 2 — build scope: OPEN.** Implement only Q-A + Q-B + Dave's five (7.1–7.7) now and stub the banked
Provenance queries (7.x tail) as named no-ops to fill at A3/real-footage time — or build the full banked set
up front. Recommend the former (answer the live disputes first, expand against real footage).

---

## 11. Verification note (house rule)

The §2 format is transcribed from `cnr3_diagnostics.cpp` (record assembly ~L790–904, legend ~L701–786,
list/tick/utc formatters ~L437–656) and the arInitial emission guard (`cnr3_arInitial.cpp` ~L98–133) at
marker `CMS07-DIAG.honest-cache-hit-metrics`. Anyone implementing should re-confirm the field order and
enum spelling against the same source before coding the tokenizer — the legend the tool embeds must match
the legend the plugin emits, byte for byte, or §8's drift check will (correctly) complain.
