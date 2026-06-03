# CNR3 Memory Diagnostics — Formatted Output Specification

**Date:** 2026-06-02
**Status:** Specification — ready for implementation
**Applies to:** `cnr3_memory_diagnostics.h/.cpp`, `vapoursynth-Cnr3.cpp`
**Companion document:** CMS06 (cnr3_cache_manager_design_v6.md)

---

## 1. Purpose

The memory diagnostics output helps verify that CNR3's cache manager
and frame processing are not leaking memory, and to provide metrics
to assess the ipmact on memory arising from various run-time scenarios
when using various vapoursynth modes (eg fmunordered, fmparallelrequests
and fmparallel) with various cache related settings. 

Such metrics will inform potential future decisions around adjusting
and testing cache related parameters before finally releasing the
final dll and code.

The formatted output replaces the current single-line key=value dump
with a compact aligned table that includes delta columns relative to
the baseline snapshot taken at filter creation.

## 1.1 Code and Examples - indicative, not cast in stone

All code snippets in this spec are for indicative informational
purposes only and must not be construed as a compatible with
current-code status change set.

All examples in this spec are for indicative informational purposes only
showing the proposed data to be displayed and that all values in columns
must be well aligned and informative and that there are no dead-space
blank lines.

---

## 2. Snapshot Points

Memory snapshots are taken at the following points, in order:

| Point | When | Label in output | Legend printed |
|---|---|---|---|
| Baseline | End of `cnr3_create()` | `at cnr3_create (baseline)` | Yes |
| Periodic | Every `CNR3_MEMORY_DIAG_FRAME_INTERVAL` frames in `cnr3_get_frame` arAllFramesReady | `frame=N` | No |
| Pre-cleanup | Start of `cnr3_free()`, before cache clear | `before cnr3_free cleanup` | No |
| Post-cleanup | After `cnr3_output_cache_clear()` in `cnr3_free()` | `after cnr3_free cache cleanup` | No |
| Summary | End of `cnr3_free()`, before `Cnr3Data` delete | `summary (N samples)` | Yes |

The legend is printed only at the baseline and summary snapshots to
keep mid-run output compact.

All snapshot and summary output is gated on `d->debug`. No memory
diagnostic output appears unless `debug=1` is set by the caller.
No memory diagnostic output is ever written to stdout.

---

## 3. Periodic Frame Interval Parameter

A new compile-time constant controls how often the periodic in-run
snapshot fires:

```cpp
// In cnr3_build_config.h:

/*
    Memory diagnostics periodic frame interval.

    When debug=1 is active, a memory snapshot is printed every time
    frame_number % CNR3_MEMORY_DIAG_FRAME_INTERVAL == 0 and
    frame_number > 0.

    Set to 0 to disable periodic in-run snapshots entirely.
    Useful values: 100, 500, 1000.
    Default: 500.
*/
static constexpr int CNR3_MEMORY_DIAG_FRAME_INTERVAL = 500;
```

The interval is compile-time only in the first implementation.
A VapourSynth parameter can be added later if runtime tuning is needed.

---

## 4. Metrics Printed

Five metrics are printed in all snapshot tables. System-level totals,
virtual address space, and kernel counters are retained internally
in `Cnr3MemoryStats` but are not printed — they are static or outside
CNR3's influence.

| Field name in output | Source field | Notes |
|---|---|---|
| `process_working_set` | `working_set_mb` | Per-process |
| `process_private_usage` | `private_usage_mb` | Per-process |
| `system_avail_phys` | `system_avail_phys_mb` | System-wide |
| `system_used_phys` | `system_used_phys_mb` | System-wide |
| `commit_total` | `commit_total_mb` | System-wide |

Two peak values are printed separately (no delta column):

| Field name in output | Source field |
|---|---|
| `peak_working_set` | `peak_working_set_mb` |
| `peak_private_usage` | `peak_pagefile_usage_mb` |

All values are printed in MB to two decimal places.

---

## 5. Table Format

### 5.1 Snapshot table (baseline, periodic, pre/post-cleanup)

```
CNR3 memory: instance=I, <label>
  Metric                    Now (MB) Start (MB)  Delta (MB)   Delta (%)
  process_working_set          XX.XX       XX.XX      +XXX.XX    +XXXX.XX
  process_private_usage        XX.XX       XX.XX      +XXX.XX    +XXXX.XX
  system_avail_phys         XXXXX.XX    XXXXX.XX      +XXX.XX    +XXXX.XX
  system_used_phys          XXXXX.XX    XXXXX.XX      +XXX.XX    +XXXX.XX
  commit_total              XXXXX.XX    XXXXX.XX      +XXX.XX    +XXXX.XX
  peak_working_set             XX.XX   (cumulative peak, no delta)
  peak_private_usage           XX.XX   (cumulative peak, no delta)
```

Rules:
- Two-space indent for all rows.
- Metric name field: 24 characters wide, left-aligned.
- Numeric columns: 10 characters wide, right-aligned, two decimal places.
- Delta (MB) column: 10 characters wide, explicit sign (`+` or `-`).
- Delta (%) column: 12 characters wide, explicit sign. Process-level metrics
  can exceed 100% or even 1000% when the process grows significantly relative
  to its startup size. System-level metrics are typically fractions of a
  percent. The wider column accommodates both without truncation.
- The baseline snapshot uses `+0.00` for all deltas and `+0.00%`.
- `Start (MB)` column shows the baseline value for every snapshot.
  On the baseline snapshot itself this is identical to `Now (MB)`.

### 5.2 Legend (baseline and summary only)

Printed immediately after the peak rows, no blank line between:

```
  Legend:
  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.
  process_private_usage  Committed private memory for this process; most reliable CNR3 allocation indicator; should track cache size closely.
  system_avail_phys      Free physical RAM system-wide; falls as CNR3 uses more; small percent change is normal and expected.
  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; confirms system-level CNR3 impact.
  commit_total           Total committed virtual memory system-wide; grows with CNR3 cache; should largely recover after cache cleanup.
  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from CNR3 processing.
  peak_private_usage     Highest private committed memory seen this run; reveals worst-case allocation; compare with after-cleanup value.
```

### 5.3 Summary table

```
CNR3 memory: instance=I, summary (N samples)
  Metric                        Min (MB)    Avg (MB)    Max (MB)
  process_working_set              XX.XX       XX.XX       XX.XX
  process_private_usage            XX.XX       XX.XX       XX.XX
  system_avail_phys             XXXXX.XX    XXXXX.XX    XXXXX.XX
  system_used_phys              XXXXX.XX    XXXXX.XX    XXXXX.XX
  commit_total                  XXXXX.XX    XXXXX.XX    XXXXX.XX
```

Legend follows the summary table using the same format as Section 5.2.

---

## 6. Implementation Notes

### 6.1 Baseline storage

`Cnr3MemoryStats` gains a new field:

```cpp
struct Cnr3MemoryStats {
    // ... existing fields ...

    // Baseline snapshot taken at cnr3_create completion.
    // Used as the reference for all delta calculations.
    // valid=false until the first snapshot is recorded.
    bool baseline_valid = false;
    double baseline_working_set_mb = 0.0;
    double baseline_private_usage_mb = 0.0;
    double baseline_avail_phys_mb = 0.0;
    double baseline_used_phys_mb = 0.0;
    double baseline_commit_total_mb = 0.0;
};
```

The baseline is populated during the first call to
`cnr3_memory_record_and_print_snapshot()` from `cnr3_create()`, before
the baseline fields are used. Subsequent calls read the baseline fields
for delta calculation.

### 6.2 Print helper signature

A new helper replaces the current single-line print:

```cpp
void cnr3_memory_print_formatted_snapshot(
    const Cnr3MemoryStats& stats,
    bool debug_enabled,
    int instance_id,
    const char* label,
    bool show_legend
);
```

`show_legend` is `true` only for the baseline (`cnr3_create`) and
summary calls. All other calls pass `false`.

### 6.3 Periodic trigger in cnr3_get_frame

In the `arAllFramesReady` block, after the existing store/prune proving
code:

```cpp
if (
    CNR3_MEMORY_DIAG_FRAME_INTERVAL > 0 &&
    n > 0 &&
    (n % CNR3_MEMORY_DIAG_FRAME_INTERVAL) == 0
) {
    cnr3_memory_record_and_print_snapshot(
        d->memory_stats,
        d->debug,
        d->instance_id,
        "frame=N"   // format n into this label
    );
}
```

The label is formatted at the call site using `snprintf` into a small
stack buffer, e.g. `"frame=500"`, `"frame=1000"`.

### 6.4 Delta calculation

```cpp
const double delta_mb  = now_value - baseline_value;
const double delta_pct = (baseline_value > 0.0)
    ? (delta_mb / baseline_value) * 100.0
    : 0.0;
```

Sign is explicit in the format string: `%+.2f` for MB delta,
`%+.2f` for percent delta.

### 6.5 No structural change to data collection

The existing `Cnr3MemoryStats` collection logic, sample counts, and
min/avg/max tracking are unchanged. Only the print formatting changes.
All previously collected fields remain available for future use.

---

## 7. Example Output — Short Run (1 instance, ~600 frames)

```
CNR3 memory: instance=1, at cnr3_create (baseline)
  Metric                    Now (MB) Start (MB)  Delta (MB)   Delta (%)
  process_working_set          38.20      38.20       +0.00       +0.00
  process_private_usage        41.10      41.10       +0.00       +0.00
  system_avail_phys         21731.50   21731.50       +0.00       +0.00
  system_used_phys          10946.11   10946.11       +0.00       +0.00
  commit_total              14202.28   14202.28       +0.00       +0.00
  peak_working_set             38.20   (cumulative peak, no delta)
  peak_private_usage           41.10   (cumulative peak, no delta)
  Legend:
  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.
  process_private_usage  Committed private memory for this process; most reliable CNR3 allocation indicator; should track cache size closely.
  system_avail_phys      Free physical RAM system-wide; falls as CNR3 uses more; small percent change is normal and expected.
  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; confirms system-level CNR3 impact.
  commit_total           Total committed virtual memory system-wide; grows with CNR3 cache; should largely recover after cache cleanup.
  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from CNR3 processing.
  peak_private_usage     Highest private committed memory seen this run; reveals worst-case allocation; compare with after-cleanup value.

CNR3 memory: instance=1, frame=500
  Metric                    Now (MB) Start (MB)  Delta (MB)   Delta (%)
  process_working_set          43.10      38.20       +4.90      +12.83
  process_private_usage        49.20      41.10       +8.10      +19.71
  system_avail_phys         21723.40   21731.50       -8.10       -0.04
  system_used_phys          10954.21   10946.11       +8.10       +0.07
  commit_total              14210.38   14202.28       +8.10       +0.07
  peak_working_set             48.50   (cumulative peak, no delta)
  peak_private_usage           55.30   (cumulative peak, no delta)

CNR3 memory: instance=1, before cnr3_free cleanup
  Metric                    Now (MB) Start (MB)  Delta (MB)   Delta (%)
  process_working_set          45.45      38.20       +7.25      +18.98
  process_private_usage        52.93      41.10      +11.83      +28.78
  system_avail_phys         21720.34   21731.50      -11.16       -0.05
  system_used_phys          10957.27   10946.11      +11.16       +0.10
  commit_total              14214.11   14202.28      +11.83       +0.08
  peak_working_set             51.13   (cumulative peak, no delta)
  peak_private_usage           58.44   (cumulative peak, no delta)

CNR3 memory: instance=1, after cnr3_free cache cleanup
  Metric                    Now (MB) Start (MB)  Delta (MB)   Delta (%)
  process_working_set          40.88      38.20       +2.68       +7.01
  process_private_usage        48.05      41.10       +6.95      +16.91
  system_avail_phys         21724.49   21731.50       -7.01       -0.03
  system_used_phys          10953.12   10946.11       +7.01       +0.06
  commit_total              14209.53   14202.28       +7.25       +0.05
  peak_working_set             51.13   (cumulative peak, no delta)
  peak_private_usage           58.44   (cumulative peak, no delta)

CNR3 memory: instance=1, summary (4 samples)
  Metric                        Min (MB)    Avg (MB)    Max (MB)
  process_working_set              38.20       41.91       45.45
  process_private_usage            41.10       46.09       52.93
  system_avail_phys             21720.34    21724.83    21731.50
  system_used_phys              10946.11    10950.87    10957.27
  commit_total                  14202.28    14207.27    14214.11
  Legend:
  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.
  process_private_usage  Committed private memory for this process; most reliable CNR3 allocation indicator; should track cache size closely.
  system_avail_phys      Free physical RAM system-wide; falls as CNR3 uses more; small percent change is normal and expected.
  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; confirms system-level CNR3 impact.
  commit_total           Total committed virtual memory system-wide; grows with CNR3 cache; should largely recover after cache cleanup.
  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from CNR3 processing.
  peak_private_usage     Highest private committed memory seen this run; reveals worst-case allocation; compare with after-cleanup value.
```

Note: The example above shows modest deltas because the run is short and
the cache is small relative to the process baseline. On a longer run or
with a larger clip, process-level deltas can be much larger. For example,
if a process starts at 40 MB and grows to 450 MB during a long encode,
the Delta (%) for process_private_usage would be +1025.00 — hence the
wider column. System-level percentages remain small fractions of a percent
because the system has 32 GB of physical RAM and CNR3 uses tens of MB;
on other PCs (eg a Windows-10 i4670 with 8Gb GB of physical RAM) the
numbers may look very different.

---

## 8. Interpreting the Example Output

**At frame=500:** **process_private_usage delta** is the best available
process-level indicator of memory growth during CNR3 processing. It
should broadly correlate with output-cache growth, but it is not an
isolated CNR3-cache-only measurement. Other VapourSynth, plugin,
runtime, allocator, and operating-system activity in the same process
can also affect it.

For cache-impact interpretation, compare **process_private_usage** across
the baseline, periodic frame snapshots, before-cleanup, and after-cleanup
snapshots. A rise during processing followed by a substantial drop after
cache cleanup is consistent with cache memory being released. A persistent
or repeatedly increasing after-cleanup delta across comparable runs would
be a stronger leak warning.   Perhaps somehow mention this in the table legend.

**Before cleanup:** delta of +11.83 MB private usage represents the
peak cache footprint plus process overhead above baseline.

**After cleanup:** delta of +6.95 MB private usage remaining above
baseline is expected — it represents process overhead, response tables,
`Cnr3Data` struct, and other persistent allocations. If this number
grew across repeated runs of the same clip, that would indicate a leak.

**Summary min:** the minimum `process_working_set` of 38.20 MB is the
baseline itself (captured at create before frames flow). If the minimum
were higher than baseline, it would mean the baseline was not taken
early enough.

**Key check:** compare `after cnr3_free cache cleanup` delta with
`at cnr3_create` delta (always zero). The remaining delta after cleanup
should be small and consistent across runs. Unexplained growth across
repeated runs is the primary leak indicator.

