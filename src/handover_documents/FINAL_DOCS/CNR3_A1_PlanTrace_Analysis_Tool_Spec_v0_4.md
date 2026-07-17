# CNR3 A1 — Plan-Trace Log Analysis Tool — SPECIFICATION v0.4

**Track:** A1 (external analysis; NOT plugin code). No plugin gate / macro-off / 4-way / proof discipline
applies. This is a standalone Python 3.14+ program that reads a CNR3 run log and answers structural
questions about how the cache behaved.

**Audience:** written to be handed to a coder who has NOT worked on CNR3 before. Part A is a plain-English
primer on the plugin, its cache, and the log format — enough to interpret the numbers correctly. Part B is
the tool specification. Read Part A first; the queries in Part B assume its vocabulary.

**Interpreter:** Python 3.14+. Imports nothing VapourSynth-specific (it only reads a text file), so it runs
on the embedded portable-VapourSynth Python or any standalone 3.14.x. Prefer a standalone install.

**Status:** v0.4. Incorporates the coder/coordinator feedback round (all ten items; item 6 sharpened).
Prior: v0.3 Adds the Part A onboarding primer; folds in everything settled since v0.2 (intent-counted
lookups, the lookup-site taxonomy, the five-origin frame lifecycle); reframes Q-B (execution order vs the
D-SUM-01 "out_of_order = 0" reading) as the tool's driving question. Build scope (former §10 decision 2) is
RULED: full question set up front, one function each, implemented easiest-first.

---

# PART A — CNR3, its cache, and the log (plain-English primer)

## A1. What CNR3 is

CNR3 is a VapourSynth video filter plugin (VapourSynth API4, 64-bit, Windows) used in restoring old
VHS/analogue footage. Its job is **temporal chroma noise reduction**: it steadies the colour channels of a
frame by blending them against the *previous output frame*, so colour noise that jitters frame-to-frame is
averaged away.

The one fact that drives everything else: **CNR3 is recursive.** To produce output frame N it needs two
things — the *source* frame N (the raw input) and the *previous output* frame N−1 (the frame CNR3 itself
already produced). Formally: `output[N] = f(source[N], output[N-1])`. It depends on the previous *output*,
never the previous *source*. That is the cardinal rule.

Consequence: output frames must effectively be built in a chain — you can't make frame 100 until frame 99
exists, which needed 98, and so on back to frame 0 (which is special: it has no predecessor, so it's built
fresh from source alone). The filter runs in VapourSynth's **fmUnordered** mode — a single thread computes
CNR3 frames, but the engine is allowed to *request* frames in a non-sequential order and to prefetch ahead.
That request reordering is where most of the interesting behaviour comes from, and it's the reason this
tool exists.

## A2. Why there is a cache at all

Because output[N] needs output[N−1], CNR3 must keep its own recently-produced output frames so the next
frame can build on them. VapourSynth won't hand a filter its own past outputs — outputs only exist while the
filter holds them. So CNR3 runs its **own output cache**: the sole retainer of produced output frames. If a
needed previous output isn't in the cache, CNR3 has to rebuild it — that rebuild is the expensive event the
whole cache design exists to avoid, and the whole log exists to measure.

## A3. How the cache works — slots, pins, hot zones, checkpoints

Four ideas, each doing one job. A newcomer must keep them distinct because the log names all four.

**Slot.** The cache is a set of *slots*. A slot holds a reference to one produced output frame (by frame
number). "Is frame N in the cache?" means "is there a slot holding frame N?" The cache is the complete
index of which output frames currently exist — if it's not in a slot, for CNR3's purposes it doesn't exist
and must be recomputed.

**Pin (the correctness mechanism).** A *pin* is a claim placed on a slot by a request that is actively using
that frame right now. While frame N−1 is pinned, it cannot be evicted — so the frame you're about to build
from is guaranteed to still be there. The rule is strict: a consumer pins the frame it needs, and the same
consumer releases the pin when done (within one request's lifetime). Producing a frame does NOT pin it
(nobody is consuming it yet). Pins are what make the cache correct under reordering: any frame a request
needs is pinned by that request, so it can't vanish mid-use.

**Hot zone (a prune *hint*, not a guarantee).** A *hot zone* is a sliding window `[low, high]` around where
recent activity has been. Frames inside a live zone are *preferred to keep* when the cache has to evict
something; frames outside all zones are evict-eligible first. Zones protect frames that no request is
actively pinning but that are *likely needed soon* (e.g. a just-produced frame waiting for its consumer to
arrive). Crucially, zones only affect *efficiency* (a wrong guess costs a recompute), never correctness —
pins handle correctness. The relevant tunable is the **back-radius** (`BACK_RADIUS`), roughly how far behind
the current frame the zone reaches; it bounds how far recovery is expected to look back.

**Checkpoint (a retention flag for anchors).** A *checkpoint* is a flag on certain output frames marking
them as protected-for-longer, so they survive eviction and remain available as *recovery anchors* (see
below). Checkpoints are established on a regular grid (every Nth frame, plus frame 0) and additionally at
scene-cuts (a cut frame is a perfect fresh-start anchor). A checkpoint is NOT a pin — it's a separate
eviction-protection with its own retention budget (kept between a MIN and MAX count). Keep these three
protections mentally separate: **pin** = active need now; **hot zone** = anticipated need soon; **checkpoint**
= long-lived recovery anchor.

## A4. Recovery — what happens when the predecessor is missing

Normally frame N is built the cheap way: its predecessor N−1 is present in the cache, so CNR3 builds N
directly from it (the **fast/predecessor-present path**).

But under request reordering, N can be asked for when N−1 has *not been produced yet*. CNR3 then runs
**recovery**:

1. **Backward walk (anchor search).** Starting at N−1 and stepping backward (N−2, N−3, …), probe the cache
   for the nearest present output frame, bounded by the back-radius. The first present frame found is the
   **anchor** — a solid foundation to rebuild forward from.
2. **Floor fresh-start.** If the walk finds no anchor within the bound, CNR3 falls back to a **floor**: it
   rebuilds from a fresh start (like frame 0) at the bottom of the search window. Floor frames, like frame 0,
   have no predecessor.
3. **Holes.** The frames *between* the anchor and N that are absent are **holes** — they must be built, in
   order, to bridge from the anchor up to N. Frame N itself, built last once its holes exist, is the
   **recovery target**.

So a single recovery event produces: one anchor (already present — just re-used), some number of **holes**
(intermediate frames built to bridge the gap), and one **recovery target** (the frame that was actually
requested). A key optimisation: before computing a hole, CNR3 re-checks the cache — if reordering meant
another activation already produced that hole in the meantime, CNR3 **adopts** it instead of recomputing
(the "bail before compute"). Recovery is the expensive path; adoption is how the cache keeps it cheap.

## A5. The five origins of a produced frame

Every output frame CNR3 produces comes from exactly one of five paths. This taxonomy is used throughout the
log and this tool — memorise it:

1. **frame0 fresh-start** — frame 0, built from source alone (no predecessor).
2. **floor fresh-start** — a recovery floor frame, built fresh because no anchor was found in range.
3. **ordinary target** — the normal case: the requested frame, built directly from its present predecessor.
4. **recovery hole** — an in-between frame built to bridge an anchor up to a recovery target.
5. **recovery target** — the requested frame, built last after its holes were filled.

Origins 1/3/5 store through the "production" store family; origins 2/4 store through the "AS2" (recovery
consumer) store family. The tool's origin buckets and the store-family cross-checks both rest on this split.

## A6. What the log contains

A CNR3 run emits a large diagnostic log at teardown, organised into families named `D-SUM-01` … `D-SUM-14`
plus `[DSUM-HEALTH]` (derived ratios) and `[DSUM-PLANTRACE]` (the per-frame trace this tool parses). Each
family summarises one aspect: e.g. D-SUM-01 = request-order counts, D-SUM-04 = cache-lookup and
frame-lifecycle counters, D-SUM-07/08 = temporary-output and store counters, D-SUM-12 = which of the five
origins each frame took. Most families print end-of-run *totals*; the plan-trace prints *per-frame records*.

**This tool's primary input is the `[DSUM-PLANTRACE]` records.** It may also read a few summary lines
(notably D-SUM-04 and D-SUM-12) to cross-check its per-record reconstruction against the plugin's own
totals.

## A7. The plan-trace: what a "plan" is, and the record line

When CNR3 handles a requested frame it forms a **plan**: the decision of how to produce that frame — which
branch (cache-hit / frame0 / predecessor-present / recovery-exact / recovery-floor), which predecessor or
anchor to build from, which holes to fill, which source frames to fetch. The plan-trace emits **two records
per plan**:

- an **O record** — the **Original/Open-Plan record**: the plan formed when the requested frame is first
  handled. In the current source it is emitted from **arInitial**, when the plan is decided.
- an **R record** — the **Result-of-Plan record**: what happened when the plan reached its terminal result.
  In the current normal successful live paths it is emitted from **arAllFramesReady**.

**R is a result record, not a synonym for the callback.** The letters are defined by meaning; the callback
placement is a current-source fact that could differ for failure/minimal-result records or future changes.
Current-source placement, all strategies (CACHE_HIT / FRAME0 / PRED_PRESENT / RECOVERY_EXACT /
RECOVERY_FLOOR): O at arInitial; R at arAllFramesReady on the normal success path.

(arInitial and arAllFramesReady are VapourSynth's two callbacks per frame: "you were asked for frame N" and
"the source frames you asked for are ready, now produce N".)

**The four-point tick lifecycle — ticks measure BOTH duration and order.** Each record carries
high-precision monotonic timestamps ("ticks", nanoseconds). They are useful two ways. First, duration:
`exit_tick - enter_tick` is how long that stage took. Second, order: sorting records by one tick shows which
frame reached that stage first, second, third. The O and R records give four moments per frame — FOUR
DISTINCT ORDER SURFACES; a report must always say which one it measured:

| order surface | sort key | plain-English question |
|---|---|---|
| arInitial received/request order | `O.enter_tick` | Which requested frame entered planning first? |
| arInitial finish/plan-decision order | `O.exit_tick` | Which frame finished plan formation first? |
| result-entry/readiness order | `R.enter_tick` | Which frame entered the result phase first? |
| production/return completion order | `R.exit_tick` | Which frame finished and returned first? |

Never say "execution disorder" bare — name the surface. (The D-SUM-01 lesson applied to ourselves: a number
honest at one measurement point can mislead about another.) The human UTC timestamp is *derived and coarse*
— integer ticks for ordering, UTC for display only.

**Flush order.** Records are written to the log in the order the events were captured (a monotonic
`action_seq` counter), each flushed immediately. So the log's natural line order is capture order, NOT frame
order and NOT necessarily completion order. The tool must preserve this read order as the canonical
sequence, and produce sorted *copies* for analysis (by frame number, by any tick, by strategy, etc.) — never
reorder the canonical list.

**Record fields (both record kinds share a `name=value` layout).** Every field is documented in Part B §B2;
in plain English the important ones are: `seq` (capture order), `frame` (the target frame N), the four ticks,
`strategy` (O: how the plan will build — cache-hit / frame0 / predecessor-present / recovery-exact /
recovery-floor), `outcome` (R: how it ended — returned-from-cache / computed / recovered / failed), and role
frame-lists — `target`, `predecessor`, `anchor`, `floor`, `holes`, `sources`, `pinned` (O side) and
`computed`, `adopted_skipped`, `post_compute_discarded` (R side). A short **legend** is printed once in the
log defining the codes; the tool embeds its own copy and checks it against the log's (so it notices if the
plugin's format drifts).

## A8. What we are trying to measure (and the question that started this)

The driving question: **the plugin's D-SUM-01 summary reports `out_of_order = 0`, but the work demonstrably
reorders** — on a linear clip, running without the `-r 1` throttle, roughly half the frames end up on the
recovery path purely because of request/prefetch reordering. So the frames are plainly NOT arriving in
simple order, yet the counter says zero disorder. The suspicion (to be proven, not assumed) is that D-SUM-01
samples arrival order at one narrow point (arInitial dispatch, through a serialising lock) and is
structurally blind to the reordering that actually matters, which lives in the prefetch/completion stream.

The plan-trace carries the per-frame ticks needed to reconstruct the **real** dispatch order and completion
order, independently of D-SUM-01. So the tool's headline job is: rebuild the true order from the ticks, print
it beside D-SUM-01's reported zero, and let the numbers say whether the counter is honest. Everything else
the tool measures (recovery depth, hole counts, adoption, lookup accounting) surrounds that question.

---

# PART B — Tool specification

## B2. Input — the `[DSUM-PLANTRACE]` log format (verified against source)

### B2.1 Line envelope
Every diagnostic line in the run log has the form:
```
CNR3[<inst>] INFO D-SUM-PLANTRACE: <message>
```
`<inst>` is the filter instance id (integer). A single log MAY contain multiple instances; records MUST be
partitioned by `<inst>` for any ordering analysis (ticks are only comparable within one instance/run).
Only lines whose message begins with `[DSUM-PLANTRACE]` are in scope for this tool. All other families
(`[DSUM-SUMMARY]`, `[DSUM-HEALTH]`, `[DSUM10-*]`, etc.) are ignored for parsing into the plan model, EXCEPT
selected summary rows captured for comparison: D-SUM-01 (request/order counters), D-SUM-04 (lookup-site and
frame-lifecycle counters), D-SUM-12 (branch/origin totals), and selected D-SUM-07/08 rows where lifecycle
ties are checked.

**Authority principle (motivating this whole tool):** D-SUM summary rows are PLUGIN-REPORTED COUNTERS UNDER
TEST, never ground truth. The tool exists because one such counter (D-SUM-01 out_of_order) may be
structurally misleading. PLANTRACE records are the tool's primary independent evidence; captured summaries
are comparison data. If the PLANTRACE reconstruction and a D-SUM summary disagree, the tool REPORTS the
disagreement — it does not assume the summary is right. (Source code remains the final authority for what
should be counted where.)

### B2.2 Message kinds
Four message kinds under `[DSUM-PLANTRACE]`:

| kind | leading token | when | parse into |
|---|---|---|---|
| legend | `legend` | once per run, before records | legend dictionary (§8) |
| plan-open | `O` | per plan, emitted at arInitial | `OpenRecord` |
| plan-result | `R` | per plan, emitted at arAllFramesReady | `ResultRecord` |
| formatting_error | `formatting_error` | rare failure path | anomaly counter only |

### B2.3 O record (plan-open) grammar
Emitted from the arInitial RAII guard. Field order is fixed:
```
[DSUM-PLANTRACE] O enter_tick=<D20> seq=<D> frame=<KEY> exit_tick=<D20> \
  strategy=<STRAT> enter_utc=<UTC> exit_utc=<UTC> run_ms=<F> \
  target=<L> predecessor=<L> anchor=<L> floor=<L> holes=<L> sources=<L> pinned=<L> codes=<CL>
```

### B2.4 R record (plan-result) grammar
Emitted at arAllFramesReady completion. `fail_reason` is present ONLY when `outcome=FAILED`:
```
[DSUM-PLANTRACE] R enter_tick=<D20> seq=<D> frame=<KEY> exit_tick=<D20> \
  outcome=<OUTCOME> [fail_reason=<FR>] enter_utc=<UTC> exit_utc=<UTC> run_ms=<F> \
  computed=<L> adopted_skipped=<L> post_compute_discarded=<L> codes=<CL>
```

### B2.5 Field value types
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

### B2.6 Enumerations and codes (from the emitted legend — embed as canonical dict, §8)
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

### B2.7 Emission window
Plan-trace only emits for frames in a compile-time window `[FROM_FRAME, TO_FRAME]`
(`CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME/TO_FRAME`). Frames outside the window are **intentionally absent** and
MUST NOT be flagged as gaps. The tool accepts the window as an optional CLI parameter
(`--window FROM TO`); if omitted, it infers the window as `[min(frame), max(frame)]` observed and notes the
inference. All completeness/gap checks are evaluated **within the window only**.

### B2.8 Pairing model
Each plan produces exactly one O and one R sharing the same key `frame` (target N). The canonical
"plan" is the (O, R) pair. Pair O→R by frame within instance. Normally a frame number appears in only ONE plan within
the trace window. If the log contains more than one plan for the same frame number, do NOT merge or
overwrite silently — keep ALL such plans (pair each O to its nearest following R in tick order) and report a
duplicate-plan-for-frame anomaly; a re-planned frame is itself reordering evidence, exactly what this tool
must not eat. Implementation consequence: never `dict[frame] = Plan` (overwrite-prone); use
`dict[frame] -> list[Plan]` or a strict pairing structure that records the anomaly. Unpaired O (no R) or
unpaired R (no O) are anomalies, not errors — record and continue.

---

## B3. Data model

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
  - `plans`: `dict[frame] -> list[Plan]` — a list ALWAYS, so a duplicate plan can never overwrite an
    earlier one (see B2.8); length > 1 raises the duplicate-plan anomaly.
  - `legend`: parsed legend dict + the embedded canonical dict (§8).
  - `dsum04`: Dsum04Totals or None.
  - `window`: (from, to).
  - `anomalies`: list of structured anomaly records (§5).

Use `dataclasses` (frozen where practical) and `enum`. **The canonical model — parse, records, plans,
pairing, sensibility checks, and every reconciliation/ordering query — is stdlib-only.** pandas never
touches this layer.

### B3.1 Optional pandas projection (`plans_dataframe(model) -> DataFrame`)

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

## B4. Parsing (`parse_log(path, window=None) -> TraceModel`)

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

## B5. Sensibility checks (populate `anomalies`; each: severity, frame/line, description)

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

## B6. Sorting (non-destructive)

`sorted_view(records, keys) -> list` returns a **new** list; `records` is never reordered. Support composite
keys, at minimum: `action_seq`; `enter_tick`; `exit_tick`; `frame`; `(phase, enter_tick)`;
`(instance, enter_tick)`; `(strategy)`; `(outcome)`. Keys are passed as an ordered list of field accessors
so new combinations need no new function. Zero-padding in the raw text is irrelevant here — sort on the
parsed integer/enum values.

---

## B7. Query functions (each independently toggleable; comment in/out in `main`)

**Build order (RULED): full set up front, one function each, EASIEST FIRST** — B7.1, B7.2, B7.3 (order
queries) first; then B7.4, B7.5, B7.8; then the two headline reconciliations B7.6 and B7.7 last. Land and
verify each against a real log before starting the next. The Provenance-banked queries (B7.8 tail bullet)
are part of the full set, not optional. The public query list and the output envelope (B9) are STABLE from
the start even though functions land incrementally.

**Out-of-order metrics, in plain English first (used by B7.1-B7.3, B7.6).** After sorting events by a chosen
tick, look at the frame numbers. 0, 1, 2, 3, 4 means that surface is frame-sequential. 0, 4, 1, 2, 3 means
the stream went forward to 4 then backward to 1 — that backward step is out-of-order evidence on that
surface. The metrics:
- `adjacent_backward_steps` — after sorting by the selected tick, count neighbouring pairs where the later
  record has a SMALLER frame number than the previous one.
- `max_rank_displacement` — compare where each frame appears in time order vs where it would sit in simple
  frame-number order; report the largest movement.
- `frames_with_nonzero_displacement` — how many frames did not appear at their frame-number-order position.
More complex metrics (e.g. global inversion pairs) are deferred beyond v1.


Convention: each query is a pure function `query_xxx(model) -> QueryResult` that prints a titled block and
returns structured data. `main` holds a list of enabled queries; disabling one = commenting one line. No
query mutates `model`. Each result states **method, evidence, verdict** — never a bare number.

**Layer tag (per §3.1):** each query is marked **[pure]** — implemented against the canonical stdlib model
only — or **[pandas-eligible]** — an aggregation/description over the scalar `plans_dataframe`, permitted to
use pandas, still carrying the parity backstop where a cheap pure-Python cross-check exists. The two
reconciliation/ordering-integrity queries (7.6, 7.7) and every query that inspects actual frame *sets*
(list fields) are **[pure]** by rule and never touch pandas.

### B7.1 `query_arinitial_received_order`  (Dave (a); Q-B)  **[pure]**
Sort O records by `enter_tick` (arInitial dispatch). Compare that order to frame-ascending. Report: count of
adjacent inversions, longest run of monotonic frames, max/mean displacement between tick-rank and
frame-rank, and a verdict ("dispatch is/ isn't frame-sequential"). Cross-report the D-SUM-01
`out_of_order_count` from the same log alongside this tick-derived count — if they disagree, that is the
direct evidence for/against the "D-SUM-01 is fooled by dispatch serialisation" hypothesis. **This is the
primary Q-B answer.**

### B7.2 `query_arinitial_finish_order`  (Dave (b))  **[pure]**
Same as 7.1 but keyed on `O.exit_tick` (arInitial return). Answers whether arInitial *completions* were
frame-ordered.

### B7.3 `query_arallframesready_order`  (Dave (c))  **[pure]**
Two passes over R records: by `R.enter_tick` (arAllFramesReady entry) and by `R.exit_tick` (frame produced /
completion). Report inversions and displacement for each. `R.exit_tick` order vs frame order is the true
"were frames finished in order" answer under threading.

### B7.4 `query_open_holes_stats`  (Dave (d))  **[pandas-eligible]**
Over all O records: count plans by strategy; for each and overall, report hole-count distribution — total
holes, mean holes/plan, median, max, histogram, and % of plans with ≥1 hole. Break out by strategy
(recovery plans vs pred/cache-hit). This quantifies bubbling depth. Scalar aggregation over
`plans_dataframe` (`groupby('strategy')`, `describe`, `value_counts` on `hole_count`) — carry the parity
backstop on `total holes` and `mean holes/plan` (pure vs pandas, assert equal).

### B7.5 `query_recovery_hole_fill`  (Dave (e); feeds Q-A)  **[pure]**
Per-frame set membership across the `holes`/`computed`/`adopted_skipped`/`post_compute_discarded` list
fields — inherently non-tabular, so canonical-model only, no pandas.
Over all recovery plans (O.holes non-empty), pair each hole to its R disposition: computed (C),
adopted_skipped (K), post_compute_discarded (L), or unfilled/failed. Report per-plan and aggregate:
holes identified, holes filled, holes adopted-vs-computed split, discards (race losers), and any unfilled
holes. "Were holes looked up and successfully calculated" = share landing in computed∪adopted. Unfilled or
discarded holes are the wasted/lost-work signal.

### B7.6 `query_execution_disorder_vs_dsum01`  (Q-B, explicit)  **[pure — rule]**
Dedicated head-to-head: compute the tick-derived out-of-order metric (from 7.1) and print it beside
D-SUM-01's reported `out_of_order_count`, `backward_jump_count`, `forward_jump_count`, and gap histogram.
Emit an explicit verdict on whether D-SUM-01 is a faithful or blind detector of execution disorder for this
run. Settles the open dispute with numbers.

### B7.7 `query_lookup_accounting_reconcile`  (Q-A, explicit)  **[pure — rule]**
Reconstruct, from plan structure, the lookups each plan *implies* by role — **plan-implied
expectations**, not raw probe accounting. Some identities are EXACT BY CONSTRUCTION (e.g. one
requested-frame probe per plan; one predecessor probe per non-cache-hit plan; predecessor misses ==
recovery plans — several already proven against real logs); others are ESTIMATES (e.g. per-walk probe
counts inferred from spans). The tool LABELS each line exact-by-construction vs estimate. Compare the
reconstruction to the captured D-SUM-04 rows (merged totals AND the per-site breakdown the plugin now
emits). Three-way authority: PLANTRACE-derived = plan-implied expectation; D-SUM-04 rows = plugin-reported
counters; source code = final authority for what should be counted where. Any disagreement is reported as
an INVESTIGATION LEAD — never as automatic proof that either side is wrong. **This is the direct Q-A
answer.**

### B7.8 `query_lifecycle_spans`  (supporting)  **[pandas-eligible]**
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

## B8. Field/code self-documentation (Dave's requirement 6)

Ship a single canonical dictionary in the source (a module-level structure) mapping every field name, every
strategy/outcome/fail_reason, and every O/R code letter and the `*` glyph to a one-line meaning (text from
§2.6, which is the emitted legend). Requirements: (a) it is embedded as comments AND as a printable table
(`--legend` prints it); (b) at parse time the tool compares the embedded dictionary to the legend lines
found in the log and flags drift (if the plugin's legend changes, the tool notices it is stale rather than
silently misreading). This keeps the analyser honest against future emission changes.

---

## B9. CLI and output

- `tool.py --input <logfile> [--window FROM TO] [--instance N] [--legend] [--anomalies-only] [--json OUT]`
- Default run: parse, print parse summary (records, plans, window, instance count), print anomalies, then
  each enabled query block in a fixed order using the CONTRACT below.
- **Every query verdict carries a COVERAGE statement** — plan-trace windows are deliberately bounded, so a
  verdict must say whether it describes the full traced run or only a window (a limited window can still be
  representative; the point is labelling the conclusion honestly).
- **Output contract (normative shape for every query):**
```
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
  PLANTRACE window 0..500. This verdict applies only to that window, not to the
  full run.
VERDICT:
  arInitial received order was not frame-sequential in this window.
ANOMALIES AFFECTING VERDICT:
  none
```
- `--json` dumps the structured `QueryResult`s for downstream tooling.
- Exit code 0 on successful analysis regardless of anomalies found (anomalies are data, not tool failure);
  non-zero only on unreadable input.

---

## B10. Constraints, non-goals, open decisions

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

**Decision 2 — build scope: RESOLVED.** Build the FULL question set up front, ONE named function per
question, each independently toggleable. Functions may be built and verified in easiest-first order
(B7 intro gives the recommended sequence), but the public query list and the output envelope (B9 contract)
are stable from the start.

---

## B10a. Block envelope (verified against current source)

The plan-trace block is bracketed by envelope lines the parser should use to bound and validate it:
- `[DSUM-PLANTRACE] BEGIN schema=3c1v1 instance=<n>] records=<count>` — opens the block; carries the schema
  tag and the expected record count.
- the legend lines (B8), then the O/R records,
- `[DSUM-PLANTRACE] END   schema=3c1v1 instance=<n> records=<count>` — closes the block.

Parser requirements: (a) read the `schema=` tag and compare it to the embedded expected schema (`3c1v1`);
if it differs, warn loudly — the format may have drifted and the field mapping could be wrong. (b) Compare
the actual parsed record count to the `records=<count>` in BEGIN/END; a mismatch is an anomaly (truncated or
over-long block). (c) Handle the rare `[DSUM-PLANTRACE] warning reserve_or_append_failed
records_may_be_incomplete`, `formatting_error`, and `snapshot_error` lines — record them as anomalies; they
mean the trace itself may be partial, so downstream verdicts must say so rather than assume completeness.

## B11. Verification note (house rule)

**Re-verified against the current committed tree at marker `CMS07-DIAG.frame-lifecycle-bail-counters`
(this v0.3):** the plan-trace record grammar in B2 — O-record field order
(`enter_tick seq frame exit_tick strategy enter_utc exit_utc run_ms target predecessor anchor floor holes
sources pinned codes`), R-record field order
(`enter_tick seq frame exit_tick outcome [fail_reason] enter_utc exit_utc run_ms computed adopted_skipped
post_compute_discarded codes`), the strategy/outcome/code legends, the `*` checkpoint glyph, and the
BEGIN/END `schema=3c1v1` envelope — all match the live emission in `cnr3_diagnostics.cpp`. The intervening
commits (intent-counted lookups, lookup-site breakdown, frame-lifecycle counters) changed D-SUM-04 summary
rows only; they did NOT change the plan-trace record format. The implementer should still re-confirm the
field order against source before coding the tokenizer, but as of this version the spec is current.

Original house-rule note follows.

The §2 format is transcribed from `cnr3_diagnostics.cpp` (record assembly ~L790–904, legend ~L701–786,
list/tick/utc formatters ~L437–656) and the arInitial emission guard (`cnr3_arInitial.cpp` ~L98–133) at
marker `CMS07-DIAG.honest-cache-hit-metrics`. Anyone implementing should re-confirm the field order and
enum spelling against the same source before coding the tokenizer — the legend the tool embeds must match
the legend the plugin emits, byte for byte, or §8's drift check will (correctly) complain.
