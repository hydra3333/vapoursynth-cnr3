#!/usr/bin/env python3
"""
cnr3_parse_planretry_ladder_v3.py

Parse CNR3 VapourSynth run logs or findstr extracts and emit CSV + Markdown
for the PlanRetry sleep/core-thread ladder.

Typical use:

  python cnr3_parse_planretry_ladder_v3.py -ms 35 ^
    -file1="D:\\TEST\\run_log_T0_fmParallel_CNR3_EXPERIMENT_PLAN_RETRY_BIAS_FINDSTR.txt" ^
    -file2="D:\\TEST\\run_log_T8_fmParallel_CNR3_EXPERIMENT_PLAN_RETRY_BIAS_FINDSTR.txt" ^
    -file3="D:\\TEST\\run_log_T4_fmParallel_CNR3_EXPERIMENT_PLAN_RETRY_BIAS_FINDSTR.txt" ^
    -file4="D:\\TEST\\run_log_T2_fmParallel_CNR3_EXPERIMENT_PLAN_RETRY_BIAS_FINDSTR.txt" ^
    -file5="D:\\TEST\\run_log_T1_fmParallel_CNR3_EXPERIMENT_PLAN_RETRY_BIAS_FINDSTR.txt"

Alternative use:

  python cnr3_parse_planretry_ladder_v3.py --ms 35 --file log_T0.txt --file log_T8.txt

Outputs by default:

  table_Tx_ms35.csv
  table_Tx_ms35.md

v3 fixes:
  - removes the -f ambiguity permanently;
  - prevents D-SUM self-check / CMS07 text from corrupting metrics;
  - accepts likely aliases for PlanRetry dumped-plan counter names.
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from dataclasses import dataclass, fields
from typing import Iterable, Optional


FLOAT_RE = re.compile(r"(-?\d+(?:\.\d+)?)")
T_RE = re.compile(r"(?:^|[_\-.])T(\d+)(?:[_\-.]|$)", re.IGNORECASE)
OUTPUT_RE = re.compile(
    r"Output\s+(?P<frames>\d+)\s+frames\s+in\s+(?P<seconds>\d+(?:\.\d+)?)\s+seconds\s+\((?P<fps>\d+(?:\.\d+)?)\s+fps\)",
    re.IGNORECASE,
)
CORE_THREADS_RE = re.compile(r"core\.num_threads\s*=\s*(\d+)", re.IGNORECASE)
NUM_THREADS_HALF_RE = re.compile(r"numThreads/2\s*=\s*(-?\d+)", re.IGNORECASE)


@dataclass
class Row:
    row: str = ""
    ms: int = 0
    t_label: str = ""
    threads_reported: str = ""
    num_threads_half: str = ""
    runtime_s: str = ""
    fps: str = ""
    output_frames: str = ""

    plan_retry_enabled: str = ""
    plan_retry_sleep_ms: str = ""
    plan_retry_hole_threshold: str = ""
    plan_retry_max_cap: str = ""
    plan_retry_max: str = ""
    plan_retry_plan_attempts_total: str = ""
    plans_dumped_total: str = ""
    retry_sleeps_total: str = ""
    plans_kept_on_attempt_1: str = ""
    plans_kept_on_attempt_2: str = ""
    plans_kept_on_attempt_3plus: str = ""
    dumped_plan_holes_total: str = ""
    kept_plan_holes_total: str = ""

    arInitial_count: str = ""
    arAllFramesReady_count: str = ""
    out_of_order_count: str = ""
    source_frames_requested_total: str = ""
    source_frames_retrieved_total: str = ""
    source_frame_count_max: str = ""
    temporary_outputs_created: str = ""
    temporary_outputs_stored: str = ""
    temporary_outputs_released: str = ""
    temporary_outputs_transferred: str = ""
    duplicate_computed_but_discarded: str = ""

    frames_computed: str = ""
    bailed_before_compute_since_already_in_cache: str = ""
    bailed_after_compute_because_another_activation_stored_it_first: str = ""
    frames_computed_and_stored: str = ""
    lookup_ref_balance: str = ""
    ownership_errors: str = ""
    invariant_violations_detected: str = ""
    store_failures: str = ""

    stores_total: str = ""
    duplicates_seen: str = ""
    incoming_rejected: str = ""
    frames_evicted: str = ""
    recovery_plans_created: str = ""
    holes_identified: str = ""
    holes_filled: str = ""
    source_frames_for_holes_requested: str = ""
    source_frames_for_holes_retrieved: str = ""
    recovery_span_mean: str = ""
    recovery_span_max: str = ""
    recalculated_frame_count: str = ""
    max_recalculation_depth: str = ""
    frames_recalculated_multiple_times: str = ""
    return_to_vs_success_percent: str = ""

    input_file: str = ""


def clean_number(value: str) -> str:
    return value.replace(",", "").strip()


def metric_int(line: str, label: str) -> str:
    """Return the metric number only when label is followed by '=', spaces, or total=N.

    This deliberately does not parse self-check/expectation prose such as:
      frames_computed <= D-SUM-07 ...
    which was the source of the v2 '-07' corruption.
    """
    pattern = re.compile(
        r"(?<![A-Za-z0-9_])" + re.escape(label) +
        r"(?![A-Za-z0-9_])\s*(?:=\s*)?(?:total\s*=\s*)?(-?\d[\d,]*)\b",
        re.IGNORECASE,
    )
    m = pattern.search(line)
    return clean_number(m.group(1)) if m else ""


def metric_float(line: str, label: str) -> str:
    pattern = re.compile(
        r"(?<![A-Za-z0-9_])" + re.escape(label) +
        r"(?![A-Za-z0-9_])\s*(?:=\s*)?(-?\d+(?:\.\d+)?)\b",
        re.IGNORECASE,
    )
    m = pattern.search(line)
    return m.group(1) if m else ""


def metric_int_any(line: str, labels: Iterable[str]) -> str:
    for label in labels:
        value = metric_int(line, label)
        if value != "":
            return value
    return ""


def detect_t_label(path: str, text: str) -> str:
    basename = os.path.basename(path)
    m = T_RE.search(basename)
    if m:
        return f"T{m.group(1)}"

    for line in text.splitlines():
        if "Threads set:" in line:
            m2 = re.search(r"cnr3_core_threads\s*=\s*(\d+)", line, re.IGNORECASE)
            if m2:
                return f"T{m2.group(1)}"

    return "T?"


INT_KEYS = (
    "arInitial_count",
    "arAllFramesReady_count",
    "out_of_order_count",
    "source_frames_requested_total",
    "source_frames_retrieved_total",
    "source_frame_count_max",
    "temporary_outputs_created",
    "temporary_outputs_stored",
    "temporary_outputs_released",
    "temporary_outputs_transferred",
    "duplicate_computed_but_discarded",
    "lookup_ref_balance",
    "ownership_errors",
    "invariant_violations_detected",
    "store_failures",
    "stores_total",
    "duplicates_seen",
    "incoming_rejected",
    "frames_evicted",
    "recovery_plans_created",
    "holes_identified",
    "holes_filled",
    "source_frames_for_holes_requested",
    "source_frames_for_holes_retrieved",
    "recovery_span_max",
    "recalculated_frame_count",
    "max_recalculation_depth",
    "frames_recalculated_multiple_times",
)

TOTAL_KEYS = (
    "frames_computed_and_stored",
    "frames_computed",
    "bailed_before_compute_since_already_in_cache",
    "bailed_after_compute_because_another_activation_stored_it_first",
)

PLAN_RETRY_ALIASES = {
    "plan_retry_enabled": ("plan_retry_enabled",),
    "plan_retry_sleep_ms": ("plan_retry_sleep_ms",),
    "plan_retry_hole_threshold": ("plan_retry_hole_threshold",),
    "plan_retry_max_cap": ("plan_retry_max_cap",),
    "plan_retry_max": ("plan_retry_max",),
    "plan_retry_plan_attempts_total": (
        "plan_retry_plan_attempts_total",
        "plan_attempts_total",
        "attempts_total",
    ),
    "plans_dumped_total": (
        "plans_dumped_total",
        "plan_retry_plans_dumped_total",
        "plans_dumped_due_to_retry_total",
        "plans_dumped_due_to_holes_total",
        "plans_dumped",
    ),
    "retry_sleeps_total": ("retry_sleeps_total",),
    "plans_kept_on_attempt_1": ("plans_kept_on_attempt_1",),
    "plans_kept_on_attempt_2": ("plans_kept_on_attempt_2",),
    "plans_kept_on_attempt_3plus": ("plans_kept_on_attempt_3plus",),
    "dumped_plan_holes_total": ("dumped_plan_holes_total",),
    "kept_plan_holes_total": ("kept_plan_holes_total",),
}


def parse_log(path: str, ms: int) -> Row:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as exc:
        raise SystemExit(f"ERROR: could not read {path!r}: {exc}") from exc

    t_label = detect_t_label(path, text)
    row = Row(row=f"{ms}ms{t_label}", ms=ms, t_label=t_label, input_file=path)

    for line in text.splitlines():
        output = OUTPUT_RE.search(line)
        if output:
            row.output_frames = output.group("frames")
            row.runtime_s = output.group("seconds")
            row.fps = output.group("fps")
            continue

        m = CORE_THREADS_RE.search(line)
        if m:
            row.threads_reported = m.group(1)
            continue

        m = NUM_THREADS_HALF_RE.search(line)
        if m:
            row.num_threads_half = m.group(1)
            continue

        # PLANRETRY aliases first. The alias matching is still strict: the label
        # must be followed by a numeric metric value, not merely appear in prose.
        matched = False
        for attr, labels in PLAN_RETRY_ALIASES.items():
            value = metric_int_any(line, labels)
            if value != "":
                setattr(row, attr, value)
                matched = True
                break
        if matched:
            continue

        for key in INT_KEYS:
            value = metric_int(line, key)
            if value != "":
                setattr(row, key, value)
                matched = True
                break
        if matched:
            continue

        for key in TOTAL_KEYS:
            value = metric_int(line, key)
            if value != "":
                setattr(row, key, value)
                matched = True
                break
        if matched:
            continue

        value = metric_float(line, "recovery_span_mean")
        if value != "":
            row.recovery_span_mean = value
            continue

        value = metric_float(line, "return_to_vs_success_percent")
        if value != "":
            row.return_to_vs_success_percent = value
            continue

    return row


def natural_t_order(row: Row) -> tuple[int, int, str]:
    m = re.search(r"T(\d+)", row.t_label, re.IGNORECASE)
    if not m:
        return (9, 9999, row.row)
    t = int(m.group(1))
    if t == 0:
        return (0, 0, row.row)
    return (1, -t, row.row)


def write_csv(rows: list[Row], path: str) -> None:
    field_names = [f.name for f in fields(Row)]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=field_names, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row.__dict__)


def markdown_escape(value: object) -> str:
    s = str(value) if value is not None else ""
    return s.replace("|", "\\|")


def write_md(rows: list[Row], path: str) -> None:
    cols = [
        "row",
        "ms",
        "threads_reported",
        "num_threads_half",
        "runtime_s",
        "fps",
        "source_frames_requested_total",
        "frames_computed",
        "duplicates_seen",
        "holes_identified",
        "recalculated_frame_count",
        "recovery_span_mean",
        "recovery_span_max",
        "retry_sleeps_total",
        "plans_dumped_total",
        "kept_plan_holes_total",
        "invariant_violations_detected",
        "ownership_errors",
    ]
    headers = [
        "Row",
        "Sleep",
        "Threads",
        "N/2",
        "Runtime",
        "FPS",
        "Source requested",
        "Frames computed",
        "Duplicates",
        "Holes",
        "Recalc count",
        "Span mean",
        "Span max",
        "Retry sleeps",
        "Plans dumped",
        "Kept holes",
        "Invariants",
        "Ownership",
    ]

    with open(path, "w", encoding="utf-8") as f:
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("|" + "|".join(["---"] * len(headers)) + "|\n")
        for row in rows:
            values = [markdown_escape(getattr(row, col, "")) for col in cols]
            f.write("| " + " | ".join(values) + " |\n")


def collect_files(args: argparse.Namespace, unknown: list[str]) -> list[str]:
    paths: list[str] = []

    if args.file:
        paths.extend(args.file)

    i = 0
    while i < len(unknown):
        token = unknown[i]
        m_eq = re.match(r"^-{1,2}file\d+=(.*)$", token, re.IGNORECASE)
        if m_eq:
            paths.append(m_eq.group(1).strip('"'))
            i += 1
            continue

        m_sep = re.match(r"^-{1,2}file\d+$", token, re.IGNORECASE)
        if m_sep and i + 1 < len(unknown):
            paths.append(unknown[i + 1].strip('"'))
            i += 2
            continue

        i += 1

    seen: set[str] = set()
    result: list[str] = []
    for p in paths:
        if p not in seen:
            result.append(p)
            seen.add(p)
    return result


def parse_args(argv: Optional[Iterable[str]] = None) -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description="Parse CNR3 PlanRetry ladder logs into CSV and Markdown tables."
    )
    parser.add_argument("-ms", "--ms", type=int, required=True, help="PlanRetry sleep setting in milliseconds, e.g. 25, 35, 50.")
    parser.add_argument("--file", action="append", help="Input log file. Can be repeated.")
    parser.add_argument("--out-prefix", default=None, help="Output basename without extension. Default: table_Tx_ms<ms>")
    parser.add_argument("--csv", default=None, help="CSV output path. Overrides --out-prefix for CSV.")
    parser.add_argument("--md", default=None, help="Markdown output path. Overrides --out-prefix for Markdown.")
    parser.add_argument("--no-md", action="store_true", help="Do not write Markdown output.")
    parser.add_argument("--no-csv", action="store_true", help="Do not write CSV output.")
    parser.add_argument("--no-sort", action="store_true", help="Keep files in command-line order instead of T0,T8,T4,T2,T1 ordering.")
    return parser.parse_known_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args, unknown = parse_args(argv)
    paths = collect_files(args, unknown)
    if not paths:
        print("ERROR: no input files. Use --file log.txt or -file1=log.txt", file=sys.stderr)
        return 2

    rows = [parse_log(path, args.ms) for path in paths]
    if not args.no_sort:
        rows.sort(key=natural_t_order)

    out_prefix = args.out_prefix or f"table_Tx_ms{args.ms}"
    csv_path = args.csv or f"{out_prefix}.csv"
    md_path = args.md or f"{out_prefix}.md"

    if not args.no_csv:
        write_csv(rows, csv_path)
        print(f"wrote {csv_path}")

    if not args.no_md:
        write_md(rows, md_path)
        print(f"wrote {md_path}")

    print("\nSummary:")
    print("row,threads_reported,runtime_s,fps,frames_computed,duplicates_seen,holes_identified,retry_sleeps_total,plans_dumped_total")
    for row in rows:
        print(
            f"{row.row},{row.threads_reported},{row.runtime_s},{row.fps},"
            f"{row.frames_computed},{row.duplicates_seen},{row.holes_identified},"
            f"{row.retry_sleeps_total},{row.plans_dumped_total}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
