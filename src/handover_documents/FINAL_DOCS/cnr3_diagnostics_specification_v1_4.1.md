# CNR3 Diagnostics Specification v1.4.1

**Status:** Draft v1.4.1 for review and iteration, separate numbering convention to CMS
**Date:** 2026-06-19  
**Design epoch:** CMS07.0 restart  

**v1.4.1 change (additive clarification only):** adds §2.4 distinguishing ordinary
observe-only D-SUM telemetry from a future hard developer-alert internal-error channel,
and the developer-alert principle (bounded, one-shot, stderr-only, outside locks,
reproduction-useful, reserved for impossible/design-drift states, never for expected
concurrency; exact fields deferred to implementation). Adds a cross-reference note on
D-SUM-12 that an expected planned-hole duplicate/adopt is telemetry, not a recovery-plan
invariant failure. Aligns with CMS §9.6. No D-SUM compute/print gate is added or changed,
so this does not itself trigger R-PROCESS-19; no existing summary or field is removed.

---

## 1. Purpose

This specification defines CMS07.0-era end-of-test-run diagnostic summaries for CNR3.

It does **not** preserve old proof scaffolds, old phase names, or old implementation
names. It specifies what a new CMS07-aligned development should print at the end of a
test run so that a human maintainer can understand:

```text
- what VapourSynth request-order pattern was observed;
- whether memory use moved in a plausible way;
- whether recovery search and hole filling behaved plausibly;
- whether ownership, pins, lookup references, source frames, and temporary outputs
  balanced correctly;
- whether store, duplicate-store, prune, hot-zone, return-transfer, recalculation, and
  scene-change/checkpoint-promotion behaviour is sane;
- which counters are FAIL, WARN/investigate, or INFO/context only.
```

The latest prevailing CMS remains authoritative for cache design, reference-counting,
recovery, atomic scopes, and the VapourSynth lifecycle. If this diagnostic spec appears
to conflict with the CMS, the CMS wins and this spec must be corrected.

Where this spec restates 
(a) a prevailing `CMS` owned rule, or 
(b) a prevailing `CNR3 Handover Pack Production Specification` owned rule
eg (stderr, lock-scope, gates, scaffolds, summary readability), then 
the rule owner (the CMS for design rules; the Production Spec §3A for
register-owned process rules) is authoritative and this spec must be
corrected on any divergence.

---

## 2. Global rules

### 2.1 Stderr only

All diagnostic summaries must print to `stderr`, never to `stdout`.

Reason: in common VapourSynth usage, `stdout` may be a frame-data pipe, for example:

```text
vspipe script.vpy - | ffmpeg -i - ...
```

Printing diagnostics to stdout can corrupt that pipe.

### 2.2 No formatting or printing inside locked / atomic scopes

Counters may be incremented inside a CMS-defined atomic/locked scope when the CMS allows
the relevant state update there and the update is minimal.

Summary formatting, table construction, and emission must happen outside atomic/locked
scopes. Absolutely no printing, heavy string formatting, heap-heavy diagnostics, or long-running
work occurs inside a cache lock.

### 2.3 Observation gates observe only

Each diagnostic summary should have unique compute and print gates. Exact macro names
are confirmed in the implementation layout, but the pattern is per this example, taking
into account Item 1 in "IMPORTANT NOTE 4A" under Section 4 Summary catalogue.

Template for Item 1 of "IMPORTANT NOTE 4A" under Section 4. Item 2 has different
fields and applies later to printed summary `Notes:` sections.

```cpp
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-xx
//  Name: [PLACEHOLDER: diagnostic summary name]
//  Purpose: [PLACEHOLDER: compact purpose text from the relevant D-SUM catalogue entry]
//  Activation condition: [PLACEHOLDER: compact activation condition from the relevant D-SUM catalogue entry]
//  Likely collection: [PLACEHOLDER: compact likely collection text from the relevant D-SUM catalogue entry]
//  Field definitions: [PLACEHOLDER: compact field definitions from the relevant D-SUM catalogue entry]
//  Human interpretation: [PLACEHOLDER: compact human interpretation from the relevant D-SUM catalogue entry]
#define CNR3_DIAG_COMPUTE_DSUMxx_NAME 1
#if defined(CNR3_DIAG_COMPUTE_DSUMxx_NAME)
#   define CNR3_DIAG_PRINT_DSUMxx_NAME 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUMxx_NAME) && !defined(CNR3_DIAG_COMPUTE_DSUMxx_NAME)
#   error "Cannot print DSUMxx_NAME without computing DSUMxx_NAME"
#endif
// ---------------------------------------------------------------------------------------------
```

Example for Item 1 of "IMPORTANT NOTE 4A" under Section 4. Item 2 has different
fields and applies later to printed summary `Notes:` sections.

```cpp
// ---------------------------------------------------------------------------------------------
// NOTE:    Comment out the relevant #define line(s) to
//          disable compute and/or print for this diagnostic.
// Gate Description:
//  ID: D-SUM-01
//  Name: Frame request arrival / ordering summary
//  Purpose: Shows what request ordering CNR3 observed; helps a human see whether the run
//           was sequential, mostly sequential, or strongly out of order, and whether the
//           test exercised the scheduling stress it was meant to exercise.
//  Activation condition: Add when request-arrival observation exists; in full VapourSynth
//                        integration this is arInitial, while isolated cache-core tests may
//                        use a synthetic request driver.
//  Likely collection: Collect the requested frame number at request-arrival time and compare
//                     it with the previous request for the same instance; optionally keep a
//                     bounded sample of first/last arrivals.
//  Field definitions: arInitial_count, arAllFramesReady_count if available,
//                     first_requested_frame, last_requested_frame, monotonic_forward_count,
//                     same_frame_or_duplicate_count, backward_jump_count, forward_jump_count,
//                     max_forward_jump, max_backward_jump, arrival_gap_histogram,
//                     out_of_order_count.
//  Human interpretation: Out-of-order arrivals are INFO if intentionally stressed, WARN if
//                        sequential order was expected, and not correctness failures by
//                        themselves; impossible accounting is failure evidence.
#define CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER 1
#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
#   define CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER 1
#endif
// paired safety cross-check:
#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER) && !defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
#   error "Cannot print DSUM01_REQUEST_ORDER without computing DSUM01_REQUEST_ORDER"
#endif
// ---------------------------------------------------------------------------------------------
```

Diagnostic observation gates must not affect correct program behaviour or output frames.
They may observe only.

#### 2.3.1 Compute-disabled observe-only proof

For D-SUM diagnostic compute gates, the compute-disabled observe-only proof is governed
by the prevailing Handover Pack Production Specification §3A rule
`R-PROCESS-19 — D-SUM compute-disabled observe-only proof`. This diagnostics
specification records the diagnostic checklist reminder only; it does not define or
replace the authoritative process rule.
A behaviour-changing scaffold is not a diagnostic. It must use the project's
behavioural-scaffold rules and must not use a `DIAG_*` name.

IMPORTANT:
Refer to Item 1 in "IMPORTANT NOTE 4A" under Section 4 Summary catalogue, which
outlines the text to go into each `Gate Description:` text area.

    
### 2.4 Human-readable end summaries

Detailed per-event diagnostics may be compact and machine-oriented. End-of-run summaries
must be human-readable.

Rules:

```text
- headings are clear;
- columns and values are aligned;
- units are included in column headings where applicable;
- every percentage states or implies a named denominator;
- every non-obvious field has a legend or notes entry;
- hard-fail counters are never hidden.
```

### 2.5 Zero-row suppression

For condition/count/percent tables:

```text
- informational zero-count rows may be omitted;
- rows whose zero value proves safety should be retained, or covered by an explicit
  "all fail counters zero" line;
- hard-gate fields must not be hidden merely because they are zero unless the summary
  makes that proof clear.
```

Note regarding zero-row suppression vs the many "_store_errors: 0 → FAIL if >0" fields: 
those zero rows are proof-of-safety rows, so they should be retained per the aforementioned 
carve-out.

### 2.6 Parameterised histogram buckets

Histogram bucket boundaries are per-summary. The default for depth-like, hole-like, or
jump-like diagnostics is:

```text
0, 1, 2, 3, 4, 5, 6_plus
```

The final bucket may be parameterised per diagnostic, such as `10_plus` or `64_plus`, with the
intervening buckets being created, when that is more meaningful. 
The bucket definition must be stated in the summary notes or the spec entry.

### 2.7 PASS / WARN / FAIL

Each diagnostic defines its own interpretation rules.

```text
FAIL:
    Correctness, ownership, lifecycle, lock-scope, cache-safety, or impossible-accounting
    breach. Stop and investigate before progressing.

WARN:
    Not automatically wrong, but unusual enough to investigate, explain, or track.

INFO:
    Contextual observation. Often expected under out-of-order scheduling, stress tests,
    or parallelism.
```

A single summary may contain a mix of FAIL, WARN, and INFO fields.

### 2.4 Two distinct channels: ordinary D-SUM telemetry vs hard developer-alert internal errors

D-SUM summaries (this whole specification) are end-of-run **observe-only telemetry**: they
count and report, they have compute/print gates, and changing a D-SUM compute gate triggers
the macro-off observe-only proof rule (Production Spec R-PROCESS-19). They are NOT an error
channel and must never alter behaviour.

Separately, a future production-integration layer may need to surface a **hard developer-
alert** for an impossible internal state that makes output correctness unsafe. This is a
DIFFERENT channel from D-SUM telemetry and the two must not be conflated. The distinction is
made explicit here because the recovery model (CMS §9.6) has two superficially similar states:

```text
EXPECTED concurrency (telemetry, never a user-visible error):
    A planned output hole became present before this activation's AS2 store. AS2
    first-in-best-dressed duplicate/adopt handles it: existing winner authoritative,
    incoming duplicate freed, winner pinned, one pin recorded (CMS §9.2/§9.3, proven
    H.3A). Provided ownership, pin-list, and loser-release accounting stay clean, this is
    correct. It may be counted later (e.g. duplicate-store-computed-but-discarded /
    duplicate-adopt / recompute-discarded telemetry under D-SUM-08 / D-SUM-12 / D-SUM-13),
    but it is NOT a failure and produces NO user-visible stderr alert.

IMPOSSIBLE / design-drift (hard error, may raise a developer-alert):
    A non-contiguous reused-intermediate state under the current contiguous-hole planner
    (CMS §9.6.1/§9.6.3 Category B) — a present frame between start point and requested that
    is neither the start point nor a planned hole. The cache-core layer reports it by
    returning a hard status (invariant_violation / lifecycle_violation); it does NOT print
    inside a cache lock. When the VapourSynth integration / getFrame error-mapping layer
    exists, it maps the hard status to clean filter failure and MAY emit ONE bounded
    developer-alert to stderr (never stdout), OUTSIDE all cache locks.
```

**Developer-alert principle (the emission itself is future integration work, not a current
cache-core requirement).** When built, a developer-alert must be a bounded, structured,
one-shot (or rate-limited per instance/condition) incident report on stderr only, emitted
outside all locks, carrying enough real context to write a targeted reproducer, and reserved
strictly for the impossible/Category-B states — never emitted for the expected Category-A
concurrency case. "Bounded / one-shot" means it must not flood stderr per frame and must not
dump full internal state; it does NOT mean information-poor — it should be rich enough to
identify and replicate the exact condition (consistent with how CNR3's custom selftests
pinpoint conditions today). The exact field set is deferred to implementation time, because it
depends on what the integration layer can observe; it is decided then (designer to confirm the
field set at that point). A developer-alert is not a D-SUM observation gate and does not carry
D-SUM compute/print macros.

---

## 3. Common table styles with examples

All columns and values must line up vertically.

### 3.1 Metric min/avg/max table

Use for sampled metrics, especially memory.

```text
CNR3 <name>: instance=<id>, summary (<sample_count> samples)
  Metric                         Min (<unit>)  Avg (<unit>)  Max (<unit>)  Min->Max (%)
  metric_name                         84.82        94.62       103.88        +22.46
  Legend:
  metric_name              Meaning and interpretation.
  Min->Max (%)             Percentage spread from min to max; not proof by itself.
```

### 3.2 Condition/count/percent table

Use for reason splits, result splits, and histograms.

```text
CNR3 <name>: instance=<id>, summary (<denominator_name>=<N>)
  Condition / bucket                         Count    Percent
  condition_a                                  120      60.00
  condition_b                                   42      21.00
  condition_6_plus                              38      19.00
  Notes:
  Percent denominator: <denominator_name>.
  Zero-count informational rows omitted.
```

### 3.3 Balance/status table

Use for correctness balances.

```text
CNR3 <name>: instance=<id>, balance summary
  Check                                      Value      Expected     Status
  pin_balance                                  0             0       PASS
  lookup_ref_balance                           0             0       PASS
  leak_count                                   0             0       PASS
```

---

## 4. Summary catalogue

The accepted CMS07.0-era diagnostic summaries are:

```text
D-SUM-01  Frame request arrival / ordering summary
D-SUM-02  Memory diagnostics summary
D-SUM-03  Recovery-search summary
D-SUM-04  Ownership / pin / lookup-ref balance summary
D-SUM-05  Cache integrity / teardown summary
D-SUM-06  Source-frame request / retrieve / release summary
D-SUM-07  Temporary-output / owned-output-ref lifecycle summary
D-SUM-08  Cache store / duplicate-store / first-in-best-dressed summary
D-SUM-09  Return-decision / return-transfer summary
D-SUM-10  Prune / eviction safety summary
D-SUM-11  Hot-zone operation summary
D-SUM-12  Recovery planning / hole-filling summary
D-SUM-13  Recalculation histogram
D-SUM-14  Scene-change / recursive-reset / checkpoint-promotion summary
```

Discarded from v1.0:

```text
Long-run diagnostic throttling summary
```

### IMPORTANT NOTE 4A:

1.  In 2.3 above, there is provision for a comment block gate description.
    Text from the following sections (refer below) for each diag gate are to be 
    transcribed (compactly, no blank lines) into that comment block:
        ID, Name, Purpose, Activation condition, Likely collection,
        Field definitions, Human interpretation

2.  Printed Summaries may in principle consist of a header, the data block,
    a trailer, PASS/FAIL, a Legend, and Notes.
    Text from the following sections (refer below) for each printed summary are
    to also be printed (compactly, no blank lines) as 'labelled' notes under
    each printed Notes:
        ID, Name, Purpose, Activation condition, Human interpretation

---

## D-SUM-01 - Frame request arrival / ordering summary

### Purpose

Shows what request ordering CNR3 observed. This is first because out-of-order request
arrival is the practical reason the cache architecture exists.

A human should be able to see whether the run was sequential, mostly sequential, or
strongly out of order; whether forward/backward jumps occurred; whether duplicates
occurred; and whether the test actually exercised the scheduling stress it was meant to
exercise.

Out-of-order arrivals are not failures by themselves. In a stress test, they may be the
expected and desirable observation.

### Activation condition

Add when request-arrival observation exists. In full VapourSynth integration this is
`arInitial`. In an isolated cache-core test, a synthetic request driver may feed the
same counters.

### Likely collection

Collect the requested frame number at request-arrival time and compare it with the
previous request for the same instance. Optionally keep a bounded sample of the first N
and/or last N arrivals.

### Field definitions

```text
arInitial_count:
    Number of request-arrival events observed.

arAllFramesReady_count:
    Number of ready/complete callback events observed, if available.

first_requested_frame:
    First requested frame number observed.

last_requested_frame:
    Last requested frame number observed.

monotonic_forward_count:
    Arrivals where current frame number was greater than previous arrival.

same_frame_or_duplicate_count:
    Arrivals where current frame number equalled the previous arrival or was classified
    as a duplicate request.

backward_jump_count:
    Arrivals where current frame number was less than previous arrival.

forward_jump_count:
    Arrivals where current frame number was greater than the expected next sequential
    frame.

max_forward_jump:
    Largest positive request-to-request jump.

max_backward_jump:
    Largest backward jump magnitude.

arrival_gap_histogram:
    Bucketed request-to-request deltas.

out_of_order_count:
    Arrivals not matching the expected next sequential frame.
```

### Human interpretation

```text
- High out_of_order_count is INFO if the test intentionally stresses scheduling.
- High out_of_order_count is WARN if the run was expected to be sequential.
- max_forward_jump / max_backward_jump describe stress severity, not correctness failure.
- A bounded arrival sample is explanatory, not a proof by itself.
```

### Non-prescriptive example

```text
CNR3 request-order: instance=1, summary (arInitial_count=20)
  Field                                      Value
  first_requested_frame                          0
  last_requested_frame                          19
  monotonic_forward_count                       15
  same_frame_or_duplicate_count                  1
  backward_jump_count                            2
  forward_jump_count                             2
  max_forward_jump                               4
  max_backward_jump                              3
  out_of_order_count                             4

  Arrival delta histogram (denominator=19 transitions)
  Delta bucket                              Count    Percent
  delta_1                                     14      73.68
  delta_2_to_5                                 2      10.53
  delta_negative                               2      10.53
  delta_0_or_duplicate                         1       5.26

  Bounded arrival sample:
  0, 1, 2, 6, 3, 4, 5, 7, 8, 8, 9, 10, 14, 11, 12, 13, 15, 16, 17, 19

  Notes:
  Percent denominator is request-to-request transitions, not frames.
  Out-of-order arrivals are scheduling context, not a cache failure by themselves.
```

Note that this table uses delta_1 / delta_2_to_5 / delta_negative / delta_0_or_duplicate,
and that it deliberately departs from the default buckets appearing in section 2.6.


### PASS / WARN / FAIL

```text
FAIL:
    impossible accounting, such as histogram counts not summing to declared denominator.

WARN:
    strong out-of-order behaviour in a test expected to be sequential.

INFO:
    out-of-order arrivals in a stress or parallelism-oriented test.
```

---

## D-SUM-02 - Memory diagnostics summary

### Purpose

Shows process/system memory movement during the run. It helps detect leaks, runaway cache
growth, failure to release after cleanup, and excessive memory pressure.

Memory movement is interpretive. Min-to-max growth is not proof of a leak by itself.
Persistent post-cleanup elevation is more important than normal in-run growth.

### Activation condition

Can be implemented independently, likely in a separate memory diagnostics utility source
file. It still obeys the global stderr/alignment/no-lock-printing rules.

### Likely collection

Suggested sample points:

```text
- baseline / instance creation;
- periodic in-run samples;
- pre-cleanup;
- post-cleanup;
- final summary before instance data deletion.
```

### Field definitions

```text
process_working_set:
    RAM actively mapped to this process.

process_private_usage:
    Process-private committed memory. Often the best process-level growth indicator.

system_avail_phys:
    Free physical RAM system-wide.

system_used_phys:
    Physical RAM in use system-wide.

commit_total:
    Total committed virtual memory system-wide.

peak_working_set:
    Highest process working-set value seen.

peak_private_usage:
    Highest process private-usage value seen.

Min / Avg / Max:
    Minimum, average, and maximum sampled value.

Min->Max (%):
    Percentage spread from minimum to maximum sample.
```

### Human interpretation

```text
- process_private_usage is usually the best leak-suspicion indicator.
- working_set can drop due to OS trimming, so do not use it alone as proof.
- system-wide metrics can move due to other processes.
- high Min->Max suggests movement, not a leak by itself.
- compare final/post-cleanup values against baseline.
```

### Non-prescriptive example

```text
CNR3 memory: instance=1, summary (3 samples)
  Metric                         Min (MB)   Avg (MB)   Max (MB)  Min->Max (%)
  process_working_set               84.82      94.62     103.88        +22.46
  process_private_usage             90.12     100.20     110.05        +22.12
  system_avail_phys              19984.67   19993.26   20004.77         +0.10
  system_used_phys               12672.84   12684.35   12692.94         +0.16
  commit_total                   15265.82   15276.29   15285.76         +0.13
  Legend:
  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.
  process_private_usage  Best process-level memory-growth indicator; should broadly correlate with cache growth but is not cache-only.
  system_avail_phys      Free physical RAM system-wide; falls as the process/system uses more; small percent change is normal.
  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; helps confirm system-level impact.
  commit_total           Total committed virtual memory system-wide; can grow with cache and should mostly recover after cleanup.
  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from processing.
  peak_private_usage     Highest private committed memory seen this run; compare with after-cleanup value.
  Min->Max (%)           Percentage spread from minimum to maximum sample; shows movement during the run, not proof of a leak.
```

### PASS / WARN / FAIL

```text
FAIL:
    memory diagnostic API failure if memory diagnostics are a required hard gate for the
    run.

WARN:
    large persistent post-cleanup increase relative to baseline.
    missing final sample when memory diagnostics were enabled.

INFO:
    normal in-run cache growth that mostly recovers after cleanup.
```

---

## D-SUM-03 - Recovery-search summary

### Purpose

Shows how recovery searched for a usable existing predecessor/start output. This is the
search phase: how deep the search went and why it stopped.

This summary must be very explanatory because a deep search is not automatically bad. It
may be expected under out-of-order stress. The human should look for impossible
accounting, search failure where recovery should be possible, and repeated deep searches
that indicate retention/prune pressure.

### Activation condition

Add when CMS07 recovery-search logic exists.

### Likely collection

During recovery search. Counter increments may occur near the search decision; summary
formatting prints outside locks.

### Field definitions

```text
search_attempts:
    Number of recovery searches attempted.

search_successes:
    Searches that found a usable present output.

search_failures:
    Searches that failed to find a usable output.

depth_histogram:
    Search-depth distribution. Default buckets may be 0,1,2,3,4,5,6_plus.

terminated_on_present_output:
    Searches stopped because a present output was found.

terminated_on_frame0:
    Searches reached frame 0.

terminated_on_bound:
    Searches stopped at the permitted search bound.

terminated_on_failure:
    Searches failed without a valid start.

holes_filled:
    Headline count linking to D-SUM-12; detailed hole filling belongs there.
```

### Human interpretation

```text
- depth_0 usually means immediate present output.
- depth_6_plus is not automatically wrong, but should be explained.
- repeated deep search may indicate retention, prune, or workload pressure.
- denominator mismatches or bounded-start honesty failures are serious.
```

### Non-prescriptive example

```text
CNR3 recovery-search: instance=1, summary (search_attempts=40)
  Result                                     Count    Percent
  search_success                               39      97.50
  search_failure                                1       2.50

  Search depth histogram (denominator=40 searches)
  Depth bucket                              Count    Percent
  depth_0                                     22      55.00
  depth_1                                      9      22.50
  depth_2                                      5      12.50
  depth_3                                      2       5.00
  depth_6_plus                                 2       5.00

  Terminated-on split (denominator=40 searches)
  Termination condition                     Count    Percent
  present_output                              37      92.50
  frame0                                       2       5.00
  failure                                      1       2.50

  Notes:
  Zero-count informational rows omitted.
  Depth buckets use 0..6_plus in this example. Deep search is WARN/investigate, not automatic FAIL.
```

### PASS / WARN / FAIL

```text
FAIL:
    search failure where the CMS says recovery must be possible.
    histogram denominator mismatch.
    bounded-start honesty violation.

WARN:
    repeated deep searches, especially 6_plus, if not expected.

INFO:
    shallow searches and expected deep searches under stress.
```

---

## D-SUM-04 - Ownership / pin / lookup-ref balance summary

### Purpose

Shows whether ownership-sensitive mechanisms balanced. This catches leaks, missing
releases, missing transfers, double release symptoms, and pin-list discharge problems.

### Activation condition

Add as soon as ownership/pin/lookup-reference machinery exists.

### Likely collection

At pin/unpin, lookup addref/release/transfer, pin-list record/discharge, and ownership
handoff points.

### Field definitions

```text
pins_acquired:
    Consumer pins successfully acquired.

pins_released:
    Consumer pins successfully released.

pin_balance:
    pins_acquired - pins_released.

lookup_refs_acquired:
    Lookup-owned frame references acquired.

lookup_refs_released:
    Lookup-owned frame references released without transfer.

lookup_refs_transferred:
    Lookup-owned frame references transferred to caller/output authority.

lookup_ref_balance:
    lookup_refs_acquired - lookup_refs_released - lookup_refs_transferred.

pin_list_records:
    Per-invocation pin-list entries recorded.

pin_list_discharges:
    Per-invocation pin-list entries discharged.

pin_list_balance:
    pin_list_records - pin_list_discharges.

ownership_errors:
    Detected ownership protocol errors.
```

### Human interpretation

```text
- pin_balance must be zero after active requests drain.
- lookup_ref_balance must be zero at end-of-run.
- acquired == released + transferred is the lookup-ref invariant.
- non-zero final balance is a correctness failure.
```

### Non-prescriptive example

```text
CNR3 ownership: instance=1, balance summary
  Check                                      Value      Expected     Status
  pin_balance                                   0             0       PASS
  lookup_ref_balance                           0             0       PASS
  pin_list_balance                              0             0       PASS
  ownership_errors                              0             0       PASS

  Lookup refs:
  lookup_refs_acquired                         18
  lookup_refs_released                          7
  lookup_refs_transferred                      11

  Notes:
  lookup_ref_balance = acquired - released - transferred.
  Any non-zero final balance is FAIL after active requests have drained.
```

### PASS / WARN / FAIL

```text
FAIL:
    pin_balance != 0 after active requests drain.
    lookup_ref_balance != 0.
    pin-list balance != 0.
    ownership_errors > 0.

WARN:
    non-zero pins at shutdown only if shutdown is knowingly occurring during abort;
    otherwise treat as FAIL.

INFO:
    non-zero transferred count when frames are returned to caller.
```

---

## D-SUM-05 - Cache integrity / teardown summary

### Purpose

Shows final cache state and cleanup result. It answers whether the cache released what it
owned and whether validation found stale index entries, invalid slot state, pinned
shutdown state, or reference-balance errors.

### Activation condition

Add when cache data structures and clear/shutdown logic exist.

### Likely collection

During validation and teardown. Measure final slot state before detach/clear, then print
after cleanup has completed.

### Field definitions

```text
non_checkpoint_count:
    Non-checkpoint cached slots present.

checkpoint_count:
    Checkpoint cached slots present.

total_cached_frame_count:
    Total cached slots present.

total_pin_count:
    Sum of slot pin counts.

has_pinned_checkpoints:
    Whether any checkpoint had non-zero pin count.

invariants_ok:
    Validation passed indicator.

integrity_errors:
    Structural cache integrity errors.

validation_failures:
    Failed validation checks.

ref_balance_errors:
    Detected reference-balance errors.

clear_successes:
    Successful clear/cleanup operations.

clear_failures:
    Failed clear/cleanup operations.
```

### Human interpretation

```text
- non-zero cached count before cleanup may be normal.
- non-zero cached count after cleanup is suspicious unless explicitly retained.
- total_pin_count at final shutdown should be zero after requests drain.
- integrity_errors and validation_failures are correctness failures.
```

### Non-prescriptive example

```text
CNR3 cache-teardown: instance=1, summary
  Field                                      Value
  non_checkpoint_count                          0
  checkpoint_count                              0
  total_cached_frame_count                      0
  total_pin_count                               0
  has_pinned_checkpoints                        0
  integrity_errors                              0
  validation_failures                           0
  ref_balance_errors                            0
  clear_successes                               1
  clear_failures                                0

  Final status:
  cache_integrity                               PASS
```

### PASS / WARN / FAIL

```text
FAIL:
    integrity_errors > 0.
    validation_failures > 0.
    ref_balance_errors > 0.
    total_pin_count != 0 after active requests drained.
    clear_failures > 0.

WARN:
    non-zero cached frames after cleanup if intentional retention is not documented.

INFO:
    non-zero cached frames before cleanup.
```

---

## D-SUM-06 - Source-frame request / retrieve / release summary

### Purpose

Shows source-frame lifecycle compliance. It proves that source frames retrieved from
upstream were requested for the same activation and released exactly once.

### Activation condition

Add when source-frame request/retrieve/release handling exists.

### Likely collection

Request counts are collected at source request planning time. Retrieve counts are
collected when source frames are acquired. Release counts are collected when owned source
frames are released.

### Field definitions

```text
source_frames_requested_total:
    Source-frame requests issued upstream.

source_frames_retrieved_total:
    Source frames successfully retrieved.

source_frames_released_total:
    Retrieved/owned source frames released.

source_frame_release_balance:
    source_frames_retrieved_total - source_frames_released_total.

same_activation_request_violations:
    Attempts to retrieve a source frame not requested in arInitial of the same activation.

source_frame_count_max:
    Maximum simultaneously owned source frames, as defined by implementation.

partial_acquire_failures:
    Failures after some but not all expected source frames were acquired.

source_frame_release_balance_errors:
    Detected source-frame release-balance errors.
```

### Human interpretation

```text
- retrieved and released counts should balance by end-of-run.
- retrieve without same-activation request is a lifecycle violation.
- partial acquire failure is not necessarily a bug if cleanup is correct, but must be
  inspected.
```

### Non-prescriptive example

```text
CNR3 source-frames: instance=1, lifecycle summary
  Check                                      Value      Expected     Status
  source_frame_release_balance                  0             0       PASS
  same_activation_request_violations            0             0       PASS
  source_frame_release_balance_errors           0             0       PASS

  Totals:
  source_frames_requested_total                57
  source_frames_retrieved_total                57
  source_frames_released_total                 57
  source_frame_count_max                        4
  partial_acquire_failures                      0
```

### PASS / WARN / FAIL

```text
FAIL:
    same_activation_request_violations > 0.
    source_frame_release_balance != 0.
    source_frame_release_balance_errors > 0.

WARN:
    partial_acquire_failures > 0 if cleanup is otherwise clean.

INFO:
    high source_frame_count_max interpreted against expected request plan.
```

---

## D-SUM-07 - Temporary-output / owned-output-ref lifecycle summary

### Purpose

Shows handling of newly computed or temporary output frame references before they are
stored, discarded, released, or transferred.

This summary is intentionally verbose because temporary-output ownership is
complex-to-humans: an output can be created, successfully stored, rejected because a
duplicate already exists, returned to the caller, or discarded through an error path.
Every path must have a clear ownership outcome.

### Activation condition

Add when temporary output creation exists.

### Likely collection

At output creation, store attempt, store success/failure, duplicate discard, release,
and transfer.

### Field definitions

```text
temporary_outputs_created:
    Temporary output frames created.

temporary_outputs_stored:
    Temporary outputs accepted into the cache/store path.

temporary_outputs_released:
    Temporary outputs released without being transferred.

temporary_outputs_transferred:
    Temporary outputs transferred to caller/output authority.

temporary_output_balance:
    Balance according to the implementation's documented ownership equation.

caller_still_owns_temporary_output:
    Count/flag showing temporary outputs still owned where they should have been
    consumed, released, or transferred.

duplicate_computed_but_discarded:
    Computed duplicate output discarded because another valid output already won.
```

### Human interpretation

```text
- duplicate computed/discarded outputs can be normal under out-of-order/parallel paths.
- the key question is clean ownership: no leak, no double-free, no ambiguous owner.
- the exact balance equation must be documented in implementation comments.
```

### Non-prescriptive example

```text
CNR3 temporary-output: instance=1, lifecycle summary
  Field                                      Value
  temporary_outputs_created                    20
  temporary_outputs_stored                     18
  temporary_outputs_released                    2
  temporary_outputs_transferred                 0
  duplicate_computed_but_discarded              2
  caller_still_owns_temporary_output            0
  temporary_output_balance                      0

  Notes:
  Store is treated here as consuming the temporary output reference.
  Duplicates are acceptable only when ownership balance remains clean.
```

### PASS / WARN / FAIL

```text
FAIL:
    temporary_output_balance != 0 under the documented ownership equation.
    caller_still_owns_temporary_output > 0 at end-of-run.
    duplicate discard leaks or double-frees an output.

WARN:
    high duplicate discard count if not expected by test.

INFO:
    duplicate discard count with clean ownership under stress.
```

---

## D-SUM-08 - Cache store / duplicate-store / first-in-best-dressed summary

### Purpose

Shows cache store behaviour and duplicate store cases. Under out-of-order and future
parallel paths, duplicate computation may occur. The store policy must remain
first-in-best-dressed: an already stored valid frame is not overwritten by a later
duplicate.

This is both a correctness summary and a health summary. It tells a human whether the
cache concept is behaving normally, whether duplicates are expected, and whether any
unexpected anomalies are arising.

### Activation condition

Add when cache store logic exists.

### Likely collection

At the single store helper or CMS store boundary.

### Field definitions

```text
store_attempts:
    Attempts to store an output.

store_successes:
    Outputs successfully stored.

store_errors:
    Genuine store-operation errors, such as allocation failure, integrity breach,
    ownership imbalance, or invariant violation. A first-in-best-dressed duplicate skip
    is not a store error and must be counted separately.

duplicate_skipped_already_cached:
    Store skipped because the output was already cached.

duplicate_computed_but_discarded:
    Computed duplicate output discarded because it must not replace the existing frame.

first_in_best_dressed_duplicate_count:
    Cases where the first stored output retained authority over a later duplicate.
```

### Human interpretation

```text
- duplicate counts are not automatically bad.
- duplicate count plus clean ownership is INFO or WARN depending on test intent.
- duplicate count plus ownership imbalance is FAIL.
- genuine store errors must be explained.
```

### Non-prescriptive example

```text
CNR3 cache-store: instance=1, summary (store_attempts=25)
  Store result                              Count    Percent
  store_success                               20      80.00
  duplicate_skipped_already_cached             3      12.00
  duplicate_computed_but_discarded             2       8.00

  Error counters:
  store_errors                                 0
  first_in_best_dressed_duplicate_count        5

  Notes:
  Percent denominator is store_attempts.
  Duplicate outcomes are acceptable only when reference ownership remains balanced.
```

### PASS / WARN / FAIL

```text
FAIL:
    store_errors > 0 unless explicitly handled, documented, and proven to leave ownership clean.
    duplicate handling causes overwrite, leak, or reference imbalance.

WARN:
    duplicate counts unexpectedly high for a sequential test.

INFO:
    duplicate counts under stress/parallel request patterns with clean ownership.
```

---

## D-SUM-09 - Return-decision / return-transfer summary

### Purpose

Shows the distinction between deciding that an output should be returned and actually
transferring ownership to the caller. It prevents a false sense that "would return" is
the same as "did transfer".

### Activation condition

Add when output-return decision and transfer paths exist.

### Likely collection

At return-decision and return-transfer boundaries.

### Field definitions

```text
return_decisions_checked:
    Return-decision evaluations.

return_decision_yes:
    Decisions to return output.

return_decision_no:
    Decisions not to return output yet.

return_no_reason_split:
    Reasons why output was not returned.

return_transfer_attempted:
    Transfer attempts.

return_transfer_succeeded:
    Successful transfers to caller/output authority.

lookup_ref_transferred:
    Lookup-acquired references transferred.

lookup_ref_released:
    Lookup-acquired references released without transfer.

lookup_ref_balance:
    Lookup ownership balance.

output_authoritative:
    Count/flag indicating returned output was authoritative under CMS conditions.
```

### Human interpretation

```text
- decision and transfer are separate and both must be accounted.
- yes decision without transfer must have cleanup or error-path accounting.
- no-return reasons explain normal waiting/fallback behaviour.
```

### Non-prescriptive example

```text
CNR3 return-path: instance=1, summary (return_decisions_checked=20)
  Decision result                           Count    Percent
  return_decision_yes                         18      90.00
  return_decision_no                           2      10.00

  No-return reason split (denominator=2 no-decisions)
  Reason                                    Count    Percent
  predecessor_missing                          1      50.00
  source_not_ready                             1      50.00

  Transfer:
  return_transfer_attempted                   18
  return_transfer_succeeded                   18
  lookup_ref_transferred                      11
  lookup_ref_released                          7
  lookup_ref_balance                           0
```

### PASS / WARN / FAIL

```text
FAIL:
    return_transfer_attempted != return_transfer_succeeded without documented cleanup.
    lookup_ref_balance != 0.
    output marked authoritative when CMS conditions were not met.

WARN:
    high return_decision_no count if not expected.

INFO:
    no-return reasons in expected recovery/waiting tests.
```

---

## D-SUM-10 - Prune / eviction safety summary

### Purpose

Shows whether pruning/eviction obeyed protection rules. This is a hard safety summary:
prune must not evict frames that CMS07 says are protected.

### Activation condition

Add when prune/eviction exists.

### Likely collection

At prune candidate examination and detach decisions.

### Field definitions

```text
prune_attempts:
    Prune passes attempted.

prune_candidates_examined:
    Candidate slots examined.

prune_candidates_detached:
    Slots detached for later free.

prune_candidates_rejected_pinned:
    Rejected because pinned.

prune_candidates_rejected_checkpoint:
    Rejected because checkpoint-protected.

prune_candidates_rejected_in_hot_zone:
    Rejected because hot-zone policy protected them.

prune_batches:
    Batch prune/free operations.

prune_k_limit_hits:
    Prune passes that reached the K-bound limit.

hard_ceiling_abort_count:
    Hard-ceiling aborts.

post_prune_cache_count:
    Cache count after prune.
```

### Human interpretation

```text
- rejected pinned/checkpoint/in-zone counts are usually good: protections are active.
- detached count should make sense relative to pressure.
- K-limit hits mean prune pressure reached the per-pass cap.
- any protected-frame eviction is FAIL.
```

### Non-prescriptive example

```text
CNR3 prune: instance=1, summary (prune_attempts=5)
  Prune outcome                             Count
  prune_candidates_examined                    64
  prune_candidates_detached                    12
  prune_candidates_rejected_pinned              4
  prune_candidates_rejected_checkpoint          8
  prune_candidates_rejected_in_hot_zone        16
  prune_batches                                 3
  prune_k_limit_hits                            1
  hard_ceiling_abort_count                      0
  post_prune_cache_count                       52

  Notes:
  Rejection counts are protection activity, not failure.
  Any selected pinned/checkpoint/in-zone slot would be FAIL.
```

### PASS / WARN / FAIL

```text
FAIL:
    eviction/detach of pinned slot.
    eviction/detach of checkpoint or in-zone slot when CMS protection forbids it.
    hard_ceiling_abort_count > 0 unless test intentionally exercises abort handling.

WARN:
    prune_k_limit_hits > 0 if unexpected.
    post_prune_cache_count remains above target pressure range.

INFO:
    candidate rejection counts showing protections are active.
```

---

## D-SUM-11 - Hot-zone operation summary

### Purpose

Shows whether hot-zone prune-policy hints are being created, updated, merged, decayed,
expired, and used plausibly.

Hot zones are not active liveness guarantees. Pins provide active liveness. This summary
must not imply hot zones make active-request frames safe. It shows whether hot zones are
working at all and tracking along plausibly as prune-policy hints.

### Activation condition

Add when hot-zone state exists.

### Likely collection

At hot-zone create, slide, merge, decay, expiry, and prune rejection caused by hot-zone
protection.

### Field definitions

```text
hot_zone_updates:
    Hot-zone update events.

zones_created:
    New zones created.

zones_slid:
    Zone slide/update operations.

zones_merged:
    Zone merge operations.

zones_decayed:
    Decay operations.

zones_expired:
    Zones removed by expiry.

zone_count_min / avg / max:
    Minimum, average, and maximum number of zones present across samples.

protected_range_min / max:
    Observed min/max protected frame-range span.

frames_rejected_from_prune_due_to_hot_zone:
    Prune candidates rejected by hot-zone policy.
```

### Human interpretation

```text
- this summary shows policy activity and plausibility, not active-frame safety.
- zero activity may be expected in small/synthetic tests.
- unbounded zone growth, no expiry, or constant merging may indicate policy problems.
```

### Non-prescriptive example

```text
CNR3 hot-zone: instance=1, summary (hot_zone_updates=40)
  Field                                      Value
  zones_created                                6
  zones_slid                                  28
  zones_merged                                 3
  zones_decayed                               10
  zones_expired                                4
  zone_count_min                               0
  zone_count_avg                               2.4
  zone_count_max                               5
  protected_range_min                          3
  protected_range_max                         18
  frames_rejected_from_prune_due_to_hot_zone  16

  Notes:
  Hot zones are prune-policy hints only. Active liveness is proven by pins, not zones.
```

### PASS / WARN / FAIL

```text
FAIL:
    hot-zone logic directly substitutes for pin-based active liveness.
    corrupted zone state or impossible ranges.

WARN:
    unbounded zone growth or no expiry where expiry should happen.
    zero activity in a test designed to exercise zones.

INFO:
    plausible create/slide/merge/decay/expire activity.
```

---

## D-SUM-12 - Recovery planning / hole-filling summary

### Purpose

Shows what happened after recovery search found a start/anchor. This is distinct from
D-SUM-03:

```text
D-SUM-03:
    How the nearest usable present output/start was found.

D-SUM-12:
    What missing output holes were identified and filled after the start was found.
```

This distinction matters for humans because a successful search can still be followed by
bad planning, excessive hole filling, incorrect source requests, or cleanup failure.

**Note (CMS §9.6 / telemetry vs hard error, §2.4):** a planned hole that became present
before this activation's AS2 store (handled by AS2 first-in-best-dressed duplicate/adopt)
is EXPECTED fmParallel-class concurrency, counted here as ordinary INFO/performance
telemetry (e.g. a duplicate-adopt / recompute-discarded count), NOT a recovery-plan
invariant failure, provided ownership, pin-list, and loser-release accounting remain clean.
A non-contiguous reused-intermediate shape under the current contiguous-hole planner
(CMS §9.6.3 Category B) is the opposite: a hard invariant/design-drift failure reported by
status, not a D-SUM observation, and never silently accepted.

### Activation condition

Add when recovery planning and hole filling exist.

### Likely collection

At recovery plan creation, hole enumeration, source request planning for holes, hole
compute/store, and plan teardown.

### Field definitions

```text
recovery_plans_created:
    Recovery plans created.

recovery_plans_destroyed:
    Recovery plans destroyed/cleaned up.

recovery_plan_balance:
    created - destroyed.

bounded_start_frame:
    Chosen recovery start frame, summarized as needed.

requested_frame:
    Requested output frame, summarized as needed.

nearest_present_output_found:
    Plans where a present output was found as start.

holes_identified:
    Genuine missing outputs between start and requested frame.

holes_filled:
    Holes successfully filled.

source_frames_for_holes_requested:
    Source frames requested to fill genuine holes.

source_frames_for_holes_retrieved:
    Source frames retrieved to fill genuine holes.

fallback_failures:
    Recovery/fallback plans that failed.

bounded_start_honesty_failures:
    Reported start did not match actual bounded search result.
```

### Human interpretation

```text
- holes_identified and holes_filled should match for successful plans.
- plan create/destroy must balance.
- source frames for holes should correspond to genuine holes, not a blanket backward
  request window.
- deep/repeated hole filling may indicate retention/prune pressure or stress workload.
```

### Non-prescriptive example

```text
CNR3 recovery-plan: instance=1, summary (recovery_plans_created=8)
  Check                                      Value      Expected     Status
  recovery_plan_balance                         0             0       PASS
  bounded_start_honesty_failures                0             0       PASS

  Hole filling:
  nearest_present_output_found                  8
  holes_identified                             13
  holes_filled                                 13
  source_frames_for_holes_requested            13
  source_frames_for_holes_retrieved            13
  fallback_failures                             0

  Holes-per-plan histogram (denominator=8 plans)
  Hole bucket                               Count    Percent
  holes_0                                      2      25.00
  holes_1                                      3      37.50
  holes_2                                      2      25.00
  holes_6_plus                                 1      12.50

  Notes:
  Hole buckets use 0..6_plus in this example.
  Source requests must correspond to genuine holes, not a blanket old-style window.
```

### PASS / WARN / FAIL

```text
FAIL:
    recovery_plan_balance != 0.
    bounded_start_honesty_failures > 0.
    holes_filled != holes_identified for successful plans.
    source frames requested for non-hole blanket window contrary to CMS model.

WARN:
    high holes_6_plus count if not expected.
    fallback_failures > 0 where recovery was expected.

INFO:
    hole filling under stress with clean balances.
```

---

## D-SUM-13 - Recalculation histogram

### Purpose

Shows how often outputs were recomputed and how deep recomputation went. This helps
detect cache/recovery policy degenerating into excessive recomputation.

Recalculation is not automatically wrong. Interpret it with test intent, recovery
planning, store duplicates, and ownership summaries.

### Activation condition

Add when recomputation or hole filling can occur.

### Likely collection

At each computed output, classify first computation vs recalculation. Track depth or
chain length according to the implementation's documented definition.

### Field definitions

```text
recalculated_frame_count:
    Output computations classified as recalculations.

recalculation_depth_histogram:
    Bucketed recomputation depth/chain length.

max_recalculation_depth:
    Maximum recalculation depth observed.

frames_recalculated_once:
    Frame numbers recalculated once.

frames_recalculated_multiple_times:
    Frame numbers recalculated more than once.
```

### Human interpretation

```text
- some recalculation may be expected under out-of-order stress.
- recalculation with clean ownership is not a failure by itself.
- deep/repeated recalculation may indicate retention, checkpoint density, pruning, or
  recovery-planning problems.
```

### Non-prescriptive example

```text
CNR3 recalculation: instance=1, summary (recalculated_frame_count=18)
  Recalculation depth bucket                Count    Percent
  depth_1                                      9      50.00
  depth_2                                      4      22.22
  depth_3                                      3      16.67
  depth_6_plus                                 2      11.11

  Field                                      Value
  max_recalculation_depth                       8
  frames_recalculated_once                     14
  frames_recalculated_multiple_times            2

  Notes:
  Percent denominator is recalculated_frame_count.
  Recalculation is evaluated with ownership, store, and recovery-plan summaries.
```

### PASS / WARN / FAIL

```text
FAIL:
    recalculation causes ownership imbalance, duplicate overwrite, or invalid output.

WARN:
    high depth_6_plus or many multiple-recalculation frames outside expected stress.

INFO:
    shallow recalculation with clean balances under expected stress.
```

---

## D-SUM-14 - Scene-change / recursive-reset / checkpoint-promotion summary

### Purpose

Shows the interaction between pixel-layer scene-change detection, recursive reset, and
cache checkpoint promotion.

This diagnostic is both pixel-layer and cache-relevant. The pixel layer does not set
cache flags. It reports metadata and/or booleans used by cache/store orchestration to
determine and set cache flags.

### Activation condition

Add when pixel compute can report scene-change/reset metadata and the store path can
apply checkpoint flags.

### Likely collection

Pixel/frame processing reports scene-change and reset metadata as part of its compute
result. Cache/store orchestration consumes that metadata and sets checkpoint flags when
CMS conditions require it. Count both the pixel-layer observation and the cache/store
consequence.

### Field definitions

```text
frames_processed:
    Output frames processed by pixel/frame path.

scene_changes_detected:
    Frames where pixel/frame processing detected a scene change/cut.

recursive_blend_frames:
    Frames that used recursive blend against previous filtered output.

source_copy_reset_frames:
    Frames where recursion was severed and current source chroma was copied as fresh start.

scene_change_checkpoint_promotions:
    Scene-change/reset outputs promoted to checkpoint status by cache/store orchestration.

scene_change_checkpoint_store_successes:
    Scene-change checkpoint candidate stores that succeeded.

scene_change_checkpoint_store_duplicate_skips:
    Scene-change checkpoint candidate stores that lost first-in-best-dressed because an
    authoritative output was already cached. This is not a store error if the existing
    cached output ends with the required checkpoint status and ownership remains clean.

scene_change_checkpoint_store_errors:
    Genuine store-operation errors while handling scene-change checkpoint candidates,
    such as allocation failure, integrity breach, ownership imbalance, or invariant
    violation. A first-in-best-dressed duplicate skip is not a store error.

scene_change_checkpoint_promotion_mismatches:
    Eligible scene-change/reset outputs whose authoritative cached output did not end
    with the checkpoint flag set when the integration required it.

cut_near_grid_checkpoint_count:
    Scene-change checkpoints that coincided with, or were near, regular grid checkpoints
    as defined by implementation.

scene_chroma_enabled:
    Whether scene-change/chroma scene logic was enabled.

scene_threshold_used:
    Scene-change threshold used for the run, if applicable.
```

### Human interpretation

```text
- scene_changes_detected is a pixel-layer observation.
- source_copy_reset_frames is an algorithmic consequence: recursion was severed.
- scene_change_checkpoint_promotions is a cache/store consequence: the fresh-start frame
  was checkpoint-protected.
- a scene-change detection does not mean the pixel layer sets cache flags.
- a first-in-best-dressed duplicate skip is not a store error if the authoritative cached
  output has the required checkpoint status and ownership remains clean.
- mismatch between eligible reset frames and checkpoint promotion is a serious integration
  issue once active.
```

### Non-prescriptive example

```text
CNR3 scene-reset: instance=1, summary (frames_processed=200)
  Field                                      Value
  scene_chroma_enabled                          1
  scene_threshold_used                      10.00
  scene_changes_detected                        5
  recursive_blend_frames                      195
  source_copy_reset_frames                      5
  scene_change_checkpoint_promotions            5
  scene_change_checkpoint_store_successes       5
  scene_change_checkpoint_store_duplicate_skips 0
  scene_change_checkpoint_store_errors          0
  scene_change_checkpoint_promotion_mismatches  0
  cut_near_grid_checkpoint_count                1

  Scene/reset outcome split (denominator=200 frames)
  Outcome                                   Count    Percent
  recursive_blend                            195      97.50
  source_copy_reset                            5       2.50

  Notes:
  Pixel processing reports scene-change/reset metadata.
  Cache/store orchestration sets checkpoint flags.
  Eligible reset frames should become scene-change checkpoints when integration is active.
```

### PASS / WARN / FAIL

```text
FAIL:
    scene_change_checkpoint_store_errors > 0.
    scene_change_checkpoint_promotion_mismatches > 0 once integration is active.
    eligible reset frames are not checkpoint-promoted once integration is active.
    pixel/frame processing directly mutates cache checkpoint flags.

WARN:
    scene-change count unexpectedly high or low for known test material.
    cut_near_grid_checkpoint_count affects checkpoint-density interpretation.

INFO:
    normal scene-change/reset counts with matching checkpoint promotions.
    first-in-best-dressed duplicate skips where the authoritative cached output has the
    required checkpoint status and ownership remains clean.
```

---

## 5. Discarded / deferred item

### Long-run diagnostic throttling summary

Not included in v1.0.
It may be reconsidered later only if diagnostic output volume
becomes a practical problem. For now, it risks distracting from the safety summaries.

---

## 6. Implementation notes for future layout proposal

A future layout proposal should decide:

```text
- where each diagnostic data structure lives;
- whether each diagnostic is per-instance, per-request, or global;
- exact compute/print gate macro names;
- exact field names, preserving this spec's meanings;
- which counters are updated inside CMS atomic scopes;
- which counters are sampled outside locks;
- where formatting/emission occurs;
- how summaries print during normal destruction, abort, and test-failure paths.
```

Memory diagnostics may naturally live in a separate utility `.cpp`. Cache diagnostics
should remain separate from pixel algorithm logic. Pixel/frame processing may report
metadata such as scene-change/reset booleans, but cache/store code owns cache flags and
checkpoint promotion.

---

## 7. Checklist for adding a diagnostic summary

```text
[ ] Unique D-SUM ID and descriptive name.
[ ] Purpose explains what a human should learn.
[ ] Activation condition says when it becomes relevant.
[ ] Collection points are CMS07-aligned and do not cite old scaffolds.
[ ] Field definitions explain what each number represents.
[ ] Human interpretation notes explain how to use the numbers.
[ ] Formal example layout included and marked non-prescriptive.
[ ] Formatting rules follow aligned human-summary requirements.
[ ] PASS / WARN / FAIL rules included.
[ ] stderr-only.
[ ] No formatting/emission inside locked scopes.
[ ] Observation gates observe only.
[ ] Field names are suggestive until implementation.
[ ] CMS-defined design rules are not restated as new design authority.
```
