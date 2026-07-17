#!/usr/bin/env python3
"""
cnr3_y4m_chroma_report.py

Compare a selected-input Y4M clip against its corresponding CNR3 output Y4M
clip, frame by frame, with diagnostics focused on chroma attenuation /
desaturation and CNR3-style scene-change/reset behaviour.

Intended use:

  python cnr3_y4m_chroma_report.py \
    --input selected_input.y4m \
    --output cnr3_output.y4m \
    --out-prefix cnr3_chroma_delta

Outputs by default:

  cnr3_chroma_delta_per_frame.csv
  cnr3_chroma_delta_report.md
  cnr3_chroma_delta_summary.json

The CNR3 scene detector implemented here mirrors the scalar P.11C detector
shape from the CNR3 source snapshot used during this diagnostic discussion:

  - current selected input frame N is compared against previous filtered output
    frame N-1;
  - luma is downsampled to the chroma grid using the CNR3 4-tap rounded average;
  - luma differences are scaled by 1 << (subSamplingW + subSamplingH);
  - the default detector is luma-only;
  - the diagnostic detector also includes U and V differences;
  - threshold is derived from the vscnr2-style scdthr percentage formula used
    by CNR3, with CNR3's native-depth rounding policy.

This is a linear-stream analyser. For heavily shuffled/jump diagnostic clips,
CNR3's true internal predecessor may not be simply output[N-1]. The chroma
attenuation and input/output equality metrics are still valid, but scene
candidate labels should be interpreted more cautiously for non-linear request
orders.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys
from dataclasses import asdict, dataclass
from typing import BinaryIO, Iterator, Optional

try:
    import numpy as np
except ImportError as exc:  # pragma: no cover - user environment check
    raise SystemExit(
        "ERROR: numpy is required. Install it for the Python used to run this script."
    ) from exc


@dataclass(frozen=True)
class Y4MInfo:
    path: str
    width: int
    height: int
    fps: str
    interlace: str
    aspect: str
    chroma: str
    frame_count_hint: Optional[int]
    bits_per_sample: int
    sub_sampling_w: int
    sub_sampling_h: int
    y_size: int
    u_size: int
    v_size: int
    frame_payload_size: int


@dataclass
class FrameStats:
    frame: int

    y_changed_pct: float
    u_changed_pct: float
    v_changed_pct: float
    y_mean_delta: float
    u_mean_delta: float
    v_mean_delta: float
    y_mad: float
    u_mad: float
    v_mad: float
    y_rms: float
    u_rms: float
    v_rms: float
    y_max_abs: int
    u_max_abs: int
    v_max_abs: int

    chroma_mag_mean_input: float
    chroma_mag_mean_output: float
    chroma_mag_mean_delta: float
    chroma_strong_samples: int
    chroma_strong_toward_neutral_pct: float
    chroma_strong_mean_mag_delta: float

    red_brown_samples: int
    red_brown_mean_u_delta: float
    red_brown_mean_v_delta: float
    red_brown_mean_mag_delta: float

    exact_all: bool
    exact_chroma: bool
    near_all: bool
    likely_reset_passthrough: bool

    cnr3_default_scene_candidate: Optional[bool]
    cnr3_default_scene_diff_total: Optional[int]
    cnr3_default_scene_samples_examined: Optional[int]
    cnr3_chroma_scene_candidate: Optional[bool]
    cnr3_chroma_scene_diff_total: Optional[int]
    cnr3_chroma_scene_samples_examined: Optional[int]
    scene_mode_match: Optional[bool]

    classification: str
    notes: str


@dataclass
class Summary:
    input_file: str
    output_file: str
    width: int
    height: int
    chroma: str
    bits_per_sample: int
    frames_analyzed: int
    scdthr: float
    cnr3_default_scene_threshold: int
    cnr3_chroma_scene_threshold: int
    exact_all_frames: int
    exact_chroma_frames: int
    near_all_frames: int
    cnr3_default_scene_candidates: int
    cnr3_chroma_scene_candidates: int
    scene_candidate_matches: int
    scene_candidate_mismatches: int
    substantive_chroma_frames: int
    likely_desaturation_frames: int
    suspicious_scene_default_without_passthrough: int
    chroma_only_scene_candidates: int
    worst_frames_by_chroma_mad: list[int]
    worst_frames_by_desaturation: list[int]
    final_assessment: str


class Y4MReader:
    def __init__(self, path: str) -> None:
        self.path = path
        self.f: BinaryIO = open(path, "rb")
        header = self.f.readline()
        if not header.startswith(b"YUV4MPEG2 "):
            raise ValueError(f"{path}: not a YUV4MPEG2/Y4M stream")
        self.info = self._parse_header(header.decode("ascii", errors="replace").strip())
        self.frame_index = 0

    def close(self) -> None:
        self.f.close()

    def __enter__(self) -> "Y4MReader":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[no-untyped-def]
        self.close()

    def _parse_header(self, header: str) -> Y4MInfo:
        tokens = header.split()[1:]
        width: Optional[int] = None
        height: Optional[int] = None
        fps = ""
        interlace = ""
        aspect = ""
        chroma = "420jpeg"
        frame_count_hint: Optional[int] = None

        for token in tokens:
            if token.startswith("W"):
                width = int(token[1:])
            elif token.startswith("H"):
                height = int(token[1:])
            elif token.startswith("F"):
                fps = token[1:]
            elif token.startswith("I"):
                interlace = token[1:]
            elif token.startswith("A"):
                aspect = token[1:]
            elif token.startswith("C"):
                chroma = token[1:]
            elif token.startswith("XLENGTH="):
                try:
                    frame_count_hint = int(token.split("=", 1)[1])
                except ValueError:
                    frame_count_hint = None

        if width is None or height is None:
            raise ValueError(f"{self.path}: missing W/H in Y4M header")

        chroma_lower = chroma.lower()
        # This tool deliberately starts with the common CNR3 diagnostic format.
        # The parser accepts common 8-bit 4:2:0 tags and fails clearly otherwise.
        if chroma_lower in ("420", "420jpeg", "420mpeg2", "420paldv"):
            bits_per_sample = 8
            sub_sampling_w = 1
            sub_sampling_h = 1
        elif chroma_lower in ("422", "422jpeg"):
            bits_per_sample = 8
            sub_sampling_w = 1
            sub_sampling_h = 0
        elif chroma_lower in ("444", "444jpeg"):
            bits_per_sample = 8
            sub_sampling_w = 0
            sub_sampling_h = 0
        else:
            raise ValueError(
                f"{self.path}: unsupported Y4M chroma tag C{chroma}. "
                "This first diagnostic version supports 8-bit C420/C422/C444."
            )

        if width % (1 << sub_sampling_w) != 0 or height % (1 << sub_sampling_h) != 0:
            raise ValueError(
                f"{self.path}: dimensions {width}x{height} are not divisible by chroma subsampling."
            )

        bytes_per_sample = 1
        y_size = width * height * bytes_per_sample
        cw = width >> sub_sampling_w
        ch = height >> sub_sampling_h
        u_size = cw * ch * bytes_per_sample
        v_size = u_size
        frame_payload_size = y_size + u_size + v_size

        return Y4MInfo(
            path=self.path,
            width=width,
            height=height,
            fps=fps,
            interlace=interlace,
            aspect=aspect,
            chroma=chroma,
            frame_count_hint=frame_count_hint,
            bits_per_sample=bits_per_sample,
            sub_sampling_w=sub_sampling_w,
            sub_sampling_h=sub_sampling_h,
            y_size=y_size,
            u_size=u_size,
            v_size=v_size,
            frame_payload_size=frame_payload_size,
        )

    def read_frame(self) -> Optional[tuple[np.ndarray, np.ndarray, np.ndarray]]:
        line = self.f.readline()
        if line == b"":
            return None
        if not line.startswith(b"FRAME"):
            raise ValueError(
                f"{self.path}: expected FRAME header before frame {self.frame_index}, got {line[:80]!r}"
            )

        payload = self.f.read(self.info.frame_payload_size)
        if len(payload) != self.info.frame_payload_size:
            raise ValueError(
                f"{self.path}: short frame payload at frame {self.frame_index}: "
                f"got {len(payload)}, expected {self.info.frame_payload_size}"
            )

        w = self.info.width
        h = self.info.height
        cw = w >> self.info.sub_sampling_w
        ch = h >> self.info.sub_sampling_h

        y_end = self.info.y_size
        u_end = y_end + self.info.u_size

        y = np.frombuffer(payload[:y_end], dtype=np.uint8).reshape((h, w)).copy()
        u = np.frombuffer(payload[y_end:u_end], dtype=np.uint8).reshape((ch, cw)).copy()
        v = np.frombuffer(payload[u_end:], dtype=np.uint8).reshape((ch, cw)).copy()

        self.frame_index += 1
        return y, u, v


def llround_positive(value: float) -> int:
    return int(math.floor(value + 0.5))


def cnr3_scene_threshold(
    scdthr: float,
    width: int,
    height: int,
    bits_per_sample: int,
    sub_sampling_w: int,
    sub_sampling_h: int,
    scene_chroma: bool,
) -> int:
    sample_peak = (1 << bits_per_sample) - 1
    if scene_chroma:
        max_pixel_diff = (219 + (224 * 2)) >> (sub_sampling_w + sub_sampling_h)
    else:
        max_pixel_diff = 219
    base8 = (scdthr * width * height * max_pixel_diff) / 100.0
    native_threshold = base8 * sample_peak / 255.0
    return llround_positive(native_threshold)


def cnr3_downsample_luma(y: np.ndarray, sub_sampling_w: int, sub_sampling_h: int) -> np.ndarray:
    h, w = y.shape
    cw = w >> sub_sampling_w
    ch = h >> sub_sampling_h

    # CNR3 taps:
    #   x0 = chroma_x << sub_sampling_w
    #   y0 = chroma_y << sub_sampling_h
    #   x1 = min(x0 + 1, width - 1), even for 4:4:4/4:4:0
    #   y1 = min(y0 + sub_sampling_h, height - 1)
    x0 = (np.arange(cw, dtype=np.int64) << sub_sampling_w)
    y0 = (np.arange(ch, dtype=np.int64) << sub_sampling_h)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + sub_sampling_h, h - 1)

    top_left = y[np.ix_(y0, x0)].astype(np.int32)
    top_right = y[np.ix_(y0, x1)].astype(np.int32)
    bottom_left = y[np.ix_(y1, x0)].astype(np.int32)
    bottom_right = y[np.ix_(y1, x1)].astype(np.int32)

    return ((top_left + top_right + bottom_left + bottom_right + 2) >> 2).astype(np.int32)


def cnr3_detect_scene(
    current_input_y: np.ndarray,
    current_input_u: np.ndarray,
    current_input_v: np.ndarray,
    previous_output_y: np.ndarray,
    previous_output_u: np.ndarray,
    previous_output_v: np.ndarray,
    sub_sampling_w: int,
    sub_sampling_h: int,
    threshold: int,
    scene_chroma: bool,
) -> tuple[bool, int, int]:
    current_luma = cnr3_downsample_luma(current_input_y, sub_sampling_w, sub_sampling_h)
    previous_luma = cnr3_downsample_luma(previous_output_y, sub_sampling_w, sub_sampling_h)
    luma_scale_shift = sub_sampling_w + sub_sampling_h

    diff = np.abs(current_luma.astype(np.int64) - previous_luma.astype(np.int64)) << luma_scale_shift
    if scene_chroma:
        diff = diff + np.abs(current_input_u.astype(np.int64) - previous_output_u.astype(np.int64))
        diff = diff + np.abs(current_input_v.astype(np.int64) - previous_output_v.astype(np.int64))

    # CNR3 exits as soon as the threshold is crossed. The total at that point can
    # be less than the full-frame sum. For reporting usefulness, compute full sum;
    # the candidate decision is equivalent to full_sum > threshold for non-negative
    # per-sample diffs.
    full_sum = int(diff.sum(dtype=np.int64))
    samples_examined = int(diff.size)
    return full_sum > threshold, full_sum, samples_examined


def delta_stats(src: np.ndarray, dst: np.ndarray) -> tuple[float, float, float, float, int, float]:
    d = dst.astype(np.int16) - src.astype(np.int16)
    changed_pct = float(np.count_nonzero(d) * 100.0 / d.size)
    mean_delta = float(d.mean())
    absd = np.abs(d)
    mad = float(absd.mean())
    rms = float(np.sqrt(np.mean(d.astype(np.float64) ** 2)))
    max_abs = int(absd.max(initial=0))
    return changed_pct, mean_delta, mad, rms, max_abs, float(np.count_nonzero(d))


def mean_or_zero(values: np.ndarray) -> float:
    if values.size == 0:
        return 0.0
    return float(values.mean())


def classify_frame(
    y_mad: float,
    u_mad: float,
    v_mad: float,
    u_changed_pct: float,
    v_changed_pct: float,
    chroma_mag_mean_delta: float,
    chroma_strong_toward_neutral_pct: float,
    red_brown_samples: int,
    red_brown_mean_mag_delta: float,
    exact_all: bool,
    cnr3_default_scene_candidate: Optional[bool],
    likely_reset_passthrough: bool,
) -> tuple[str, str]:
    notes: list[str] = []
    chroma_mad = max(u_mad, v_mad)
    chroma_changed_pct = max(u_changed_pct, v_changed_pct)

    if exact_all:
        return "UNCHANGED", "input and output are byte-identical"

    if likely_reset_passthrough:
        return "NEAR_RESET", "input and output are near-identical"

    if cnr3_default_scene_candidate and not likely_reset_passthrough:
        notes.append("CNR3-default scene candidate without input==output passthrough")

    if y_mad <= 0.01 and chroma_mad >= 2.0 and chroma_changed_pct >= 50.0:
        broad_neutral_pull = chroma_mag_mean_delta <= -1.0
        red_brown_neutral_pull = red_brown_samples > 0 and red_brown_mean_mag_delta <= -2.0
        strong_mask_neutral_pull = chroma_strong_toward_neutral_pct >= 50.0
        if broad_neutral_pull or red_brown_neutral_pull or strong_mask_neutral_pull:
            notes.append("broad chroma-only neutral pull")
            if red_brown_neutral_pull:
                notes.append("red/brown mask also desaturates")
            return "LIKELY_DESATURATION_BUG", "; ".join(notes)

    if chroma_mad >= 2.0 or chroma_changed_pct >= 50.0:
        if chroma_mag_mean_delta < -0.5 or chroma_strong_toward_neutral_pct >= 50.0:
            notes.append("substantive chroma attenuation")
            return "SUBSTANTIVE_DESATURATION", "; ".join(notes)
        return "SUBSTANTIVE_CHROMA_CHANGE", "; ".join(notes)

    if chroma_mad >= 0.5 or chroma_changed_pct >= 20.0:
        return "MODERATE", "; ".join(notes)

    return "MINOR", "; ".join(notes)


def analyze_frame(
    frame_index: int,
    in_y: np.ndarray,
    in_u: np.ndarray,
    in_v: np.ndarray,
    out_y: np.ndarray,
    out_u: np.ndarray,
    out_v: np.ndarray,
    prev_out: Optional[tuple[np.ndarray, np.ndarray, np.ndarray]],
    sub_sampling_w: int,
    sub_sampling_h: int,
    threshold_default: int,
    threshold_chroma: int,
    near_mad_threshold: float,
    chroma_strong_threshold: float,
    red_brown_v_threshold: int,
) -> FrameStats:
    y_changed_pct, y_mean_delta, y_mad, y_rms, y_max_abs, _ = delta_stats(in_y, out_y)
    u_changed_pct, u_mean_delta, u_mad, u_rms, u_max_abs, _ = delta_stats(in_u, out_u)
    v_changed_pct, v_mean_delta, v_mad, v_rms, v_max_abs, _ = delta_stats(in_v, out_v)

    u_in_f = in_u.astype(np.float64) - 128.0
    v_in_f = in_v.astype(np.float64) - 128.0
    u_out_f = out_u.astype(np.float64) - 128.0
    v_out_f = out_v.astype(np.float64) - 128.0
    mag_in = np.sqrt(u_in_f * u_in_f + v_in_f * v_in_f)
    mag_out = np.sqrt(u_out_f * u_out_f + v_out_f * v_out_f)
    mag_delta = mag_out - mag_in

    chroma_mag_mean_input = float(mag_in.mean())
    chroma_mag_mean_output = float(mag_out.mean())
    chroma_mag_mean_delta = float(mag_delta.mean())

    strong_mask = mag_in > chroma_strong_threshold
    chroma_strong_samples = int(np.count_nonzero(strong_mask))
    if chroma_strong_samples > 0:
        strong_delta = mag_delta[strong_mask]
        chroma_strong_toward_neutral_pct = float(np.count_nonzero(strong_delta < 0.0) * 100.0 / strong_delta.size)
        chroma_strong_mean_mag_delta = float(strong_delta.mean())
    else:
        chroma_strong_toward_neutral_pct = 0.0
        chroma_strong_mean_mag_delta = 0.0

    red_brown_mask = in_v.astype(np.int16) > red_brown_v_threshold
    red_brown_samples = int(np.count_nonzero(red_brown_mask))
    if red_brown_samples > 0:
        u_delta = out_u.astype(np.int16) - in_u.astype(np.int16)
        v_delta = out_v.astype(np.int16) - in_v.astype(np.int16)
        red_brown_mean_u_delta = float(u_delta[red_brown_mask].mean())
        red_brown_mean_v_delta = float(v_delta[red_brown_mask].mean())
        red_brown_mean_mag_delta = float(mag_delta[red_brown_mask].mean())
    else:
        red_brown_mean_u_delta = 0.0
        red_brown_mean_v_delta = 0.0
        red_brown_mean_mag_delta = 0.0

    exact_all = bool(y_max_abs == 0 and u_max_abs == 0 and v_max_abs == 0)
    exact_chroma = bool(u_max_abs == 0 and v_max_abs == 0)
    near_all = bool(y_mad <= near_mad_threshold and u_mad <= near_mad_threshold and v_mad <= near_mad_threshold)
    likely_reset_passthrough = exact_all or near_all

    default_candidate: Optional[bool] = None
    default_diff: Optional[int] = None
    default_samples: Optional[int] = None
    chroma_candidate: Optional[bool] = None
    chroma_diff: Optional[int] = None
    chroma_samples: Optional[int] = None
    scene_mode_match: Optional[bool] = None

    if prev_out is not None:
        prev_y, prev_u, prev_v = prev_out
        default_candidate, default_diff, default_samples = cnr3_detect_scene(
            in_y, in_u, in_v,
            prev_y, prev_u, prev_v,
            sub_sampling_w, sub_sampling_h,
            threshold_default,
            scene_chroma=False,
        )
        chroma_candidate, chroma_diff, chroma_samples = cnr3_detect_scene(
            in_y, in_u, in_v,
            prev_y, prev_u, prev_v,
            sub_sampling_w, sub_sampling_h,
            threshold_chroma,
            scene_chroma=True,
        )
        scene_mode_match = bool(default_candidate == chroma_candidate)

    classification, notes = classify_frame(
        y_mad=y_mad,
        u_mad=u_mad,
        v_mad=v_mad,
        u_changed_pct=u_changed_pct,
        v_changed_pct=v_changed_pct,
        chroma_mag_mean_delta=chroma_mag_mean_delta,
        chroma_strong_toward_neutral_pct=chroma_strong_toward_neutral_pct,
        red_brown_samples=red_brown_samples,
        red_brown_mean_mag_delta=red_brown_mean_mag_delta,
        exact_all=exact_all,
        cnr3_default_scene_candidate=default_candidate,
        likely_reset_passthrough=likely_reset_passthrough,
    )

    return FrameStats(
        frame=frame_index,
        y_changed_pct=y_changed_pct,
        u_changed_pct=u_changed_pct,
        v_changed_pct=v_changed_pct,
        y_mean_delta=y_mean_delta,
        u_mean_delta=u_mean_delta,
        v_mean_delta=v_mean_delta,
        y_mad=y_mad,
        u_mad=u_mad,
        v_mad=v_mad,
        y_rms=y_rms,
        u_rms=u_rms,
        v_rms=v_rms,
        y_max_abs=y_max_abs,
        u_max_abs=u_max_abs,
        v_max_abs=v_max_abs,
        chroma_mag_mean_input=chroma_mag_mean_input,
        chroma_mag_mean_output=chroma_mag_mean_output,
        chroma_mag_mean_delta=chroma_mag_mean_delta,
        chroma_strong_samples=chroma_strong_samples,
        chroma_strong_toward_neutral_pct=chroma_strong_toward_neutral_pct,
        chroma_strong_mean_mag_delta=chroma_strong_mean_mag_delta,
        red_brown_samples=red_brown_samples,
        red_brown_mean_u_delta=red_brown_mean_u_delta,
        red_brown_mean_v_delta=red_brown_mean_v_delta,
        red_brown_mean_mag_delta=red_brown_mean_mag_delta,
        exact_all=exact_all,
        exact_chroma=exact_chroma,
        near_all=near_all,
        likely_reset_passthrough=likely_reset_passthrough,
        cnr3_default_scene_candidate=default_candidate,
        cnr3_default_scene_diff_total=default_diff,
        cnr3_default_scene_samples_examined=default_samples,
        cnr3_chroma_scene_candidate=chroma_candidate,
        cnr3_chroma_scene_diff_total=chroma_diff,
        cnr3_chroma_scene_samples_examined=chroma_samples,
        scene_mode_match=scene_mode_match,
        classification=classification,
        notes=notes,
    )


def validate_pair(a: Y4MInfo, b: Y4MInfo) -> None:
    fields = ["width", "height", "chroma", "bits_per_sample", "sub_sampling_w", "sub_sampling_h"]
    for field in fields:
        av = getattr(a, field)
        bv = getattr(b, field)
        if av != bv:
            raise ValueError(f"Y4M mismatch: {field}: input={av!r}, output={bv!r}")


def bool_cell(value: Optional[bool]) -> str:
    if value is None:
        return "NA"
    return "1" if value else "0"


def fmt_float(value: float, places: int = 3) -> str:
    return f"{value:.{places}f}"


def write_csv(path: str, rows: list[FrameStats]) -> None:
    field_names = list(asdict(rows[0]).keys()) if rows else list(FrameStats.__dataclass_fields__.keys())
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=field_names)
        writer.writeheader()
        for row in rows:
            writer.writerow(asdict(row))


def build_final_assessment(rows: list[FrameStats]) -> str:
    if not rows:
        return "No frames were analysed."

    likely_desat = [r for r in rows if r.classification == "LIKELY_DESATURATION_BUG"]
    substantive = [r for r in rows if r.classification in ("LIKELY_DESATURATION_BUG", "SUBSTANTIVE_DESATURATION", "SUBSTANTIVE_CHROMA_CHANGE")]
    default_scene_no_reset = [
        r for r in rows
        if r.cnr3_default_scene_candidate is True and not r.likely_reset_passthrough
    ]
    chroma_only_scene = [
        r for r in rows
        if r.cnr3_default_scene_candidate is False and r.cnr3_chroma_scene_candidate is True
    ]

    if likely_desat:
        return (
            "Likely substantive chroma attenuation/desaturation issue. "
            f"{len(likely_desat)} frame(s) show broad chroma-only neutral pull."
        )
    if substantive:
        return (
            "Substantive chroma changes are present, but the strongest desaturation-bug signature was not met. "
            f"{len(substantive)} frame(s) need visual/code review."
        )
    if default_scene_no_reset:
        return (
            "Suspicious scene/reset mismatch: CNR3-default scene candidates were found without matching input==output passthrough."
        )
    if chroma_only_scene:
        return (
            "No broad desaturation issue detected, but chroma-inclusive scene detection finds candidates missed by default luma-only detection."
        )
    return "Broadly in line with minor CNR-style chroma smoothing; no substantive desaturation signature detected."


def make_summary(
    input_info: Y4MInfo,
    output_info: Y4MInfo,
    rows: list[FrameStats],
    scdthr: float,
    threshold_default: int,
    threshold_chroma: int,
) -> Summary:
    exact_all_frames = sum(1 for r in rows if r.exact_all)
    exact_chroma_frames = sum(1 for r in rows if r.exact_chroma)
    near_all_frames = sum(1 for r in rows if r.near_all)
    default_candidates = sum(1 for r in rows if r.cnr3_default_scene_candidate is True)
    chroma_candidates = sum(1 for r in rows if r.cnr3_chroma_scene_candidate is True)
    matches = sum(1 for r in rows if r.scene_mode_match is True)
    mismatches = sum(1 for r in rows if r.scene_mode_match is False)
    substantive = sum(
        1 for r in rows
        if r.classification in ("LIKELY_DESATURATION_BUG", "SUBSTANTIVE_DESATURATION", "SUBSTANTIVE_CHROMA_CHANGE")
    )
    likely_desat = sum(1 for r in rows if r.classification == "LIKELY_DESATURATION_BUG")
    default_scene_no_reset = sum(
        1 for r in rows
        if r.cnr3_default_scene_candidate is True and not r.likely_reset_passthrough
    )
    chroma_only = sum(
        1 for r in rows
        if r.cnr3_default_scene_candidate is False and r.cnr3_chroma_scene_candidate is True
    )

    worst_chroma = sorted(rows, key=lambda r: max(r.u_mad, r.v_mad), reverse=True)[:10]
    worst_desat = sorted(rows, key=lambda r: r.chroma_mag_mean_delta)[:10]

    return Summary(
        input_file=input_info.path,
        output_file=output_info.path,
        width=input_info.width,
        height=input_info.height,
        chroma=input_info.chroma,
        bits_per_sample=input_info.bits_per_sample,
        frames_analyzed=len(rows),
        scdthr=scdthr,
        cnr3_default_scene_threshold=threshold_default,
        cnr3_chroma_scene_threshold=threshold_chroma,
        exact_all_frames=exact_all_frames,
        exact_chroma_frames=exact_chroma_frames,
        near_all_frames=near_all_frames,
        cnr3_default_scene_candidates=default_candidates,
        cnr3_chroma_scene_candidates=chroma_candidates,
        scene_candidate_matches=matches,
        scene_candidate_mismatches=mismatches,
        substantive_chroma_frames=substantive,
        likely_desaturation_frames=likely_desat,
        suspicious_scene_default_without_passthrough=default_scene_no_reset,
        chroma_only_scene_candidates=chroma_only,
        worst_frames_by_chroma_mad=[r.frame for r in worst_chroma],
        worst_frames_by_desaturation=[r.frame for r in worst_desat],
        final_assessment=build_final_assessment(rows),
    )


def write_json(path: str, summary: Summary) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(asdict(summary), f, indent=2)
        f.write("\n")


def write_md(path: str, summary: Summary, rows: list[FrameStats]) -> None:
    likely_desat_rows = [r for r in rows if r.classification == "LIKELY_DESATURATION_BUG"]
    substantive_rows = [
        r for r in rows
        if r.classification in ("LIKELY_DESATURATION_BUG", "SUBSTANTIVE_DESATURATION", "SUBSTANTIVE_CHROMA_CHANGE")
    ]
    scene_mismatches = [r for r in rows if r.scene_mode_match is False]
    default_scene_no_reset = [
        r for r in rows
        if r.cnr3_default_scene_candidate is True and not r.likely_reset_passthrough
    ]
    chroma_only_scene = [
        r for r in rows
        if r.cnr3_default_scene_candidate is False and r.cnr3_chroma_scene_candidate is True
    ]

    worst_chroma = sorted(rows, key=lambda r: max(r.u_mad, r.v_mad), reverse=True)[:20]
    worst_desat = sorted(rows, key=lambda r: r.chroma_mag_mean_delta)[:20]

    def frame_list(frames: list[int], limit: int = 80) -> str:
        if not frames:
            return "none"
        head = frames[:limit]
        suffix = "" if len(frames) <= limit else f" ... (+{len(frames) - limit} more)"
        return ", ".join(str(x) for x in head) + suffix

    with open(path, "w", encoding="utf-8") as f:
        f.write("# CNR3 Y4M Chroma Delta Report\n\n")
        f.write("## Inputs\n\n")
        f.write(f"- Input: `{summary.input_file}`\n")
        f.write(f"- Output: `{summary.output_file}`\n")
        f.write(f"- Geometry: {summary.width}x{summary.height}, C{summary.chroma}, {summary.bits_per_sample}-bit\n")
        f.write(f"- Frames analysed: {summary.frames_analyzed}\n")
        f.write(f"- CNR3 scdthr: {summary.scdthr}\n")
        f.write(f"- CNR3 default luma-only threshold: {summary.cnr3_default_scene_threshold}\n")
        f.write(f"- CNR3 chroma-inclusive diagnostic threshold: {summary.cnr3_chroma_scene_threshold}\n\n")

        f.write("## Final assessment\n\n")
        f.write(summary.final_assessment + "\n\n")

        f.write("## Totals\n\n")
        f.write("| Metric | Count |\n")
        f.write("|---|---:|\n")
        f.write(f"| Exact input==output frames | {summary.exact_all_frames} |\n")
        f.write(f"| Exact chroma input==output frames | {summary.exact_chroma_frames} |\n")
        f.write(f"| Near passthrough frames | {summary.near_all_frames} |\n")
        f.write(f"| CNR3-default scene candidates | {summary.cnr3_default_scene_candidates} |\n")
        f.write(f"| Chroma-inclusive scene candidates | {summary.cnr3_chroma_scene_candidates} |\n")
        f.write(f"| Scene detector candidate matches | {summary.scene_candidate_matches} |\n")
        f.write(f"| Scene detector candidate mismatches | {summary.scene_candidate_mismatches} |\n")
        f.write(f"| Chroma-only scene candidates | {summary.chroma_only_scene_candidates} |\n")
        f.write(f"| Substantive chroma frames | {summary.substantive_chroma_frames} |\n")
        f.write(f"| Likely desaturation-bug frames | {summary.likely_desaturation_frames} |\n")
        f.write(f"| CNR3-default scene candidates without passthrough | {summary.suspicious_scene_default_without_passthrough} |\n\n")

        f.write("## Frame lists\n\n")
        f.write(f"- Exact input==output frames: {frame_list([r.frame for r in rows if r.exact_all])}\n")
        f.write(f"- CNR3-default scene candidates: {frame_list([r.frame for r in rows if r.cnr3_default_scene_candidate is True])}\n")
        f.write(f"- Chroma-inclusive scene candidates: {frame_list([r.frame for r in rows if r.cnr3_chroma_scene_candidate is True])}\n")
        f.write(f"- Scene detector mismatches: {frame_list([r.frame for r in scene_mismatches])}\n")
        f.write(f"- Chroma-only scene candidates: {frame_list([r.frame for r in chroma_only_scene])}\n")
        f.write(f"- Substantive chroma frames: {frame_list([r.frame for r in substantive_rows])}\n")
        f.write(f"- Likely desaturation-bug frames: {frame_list([r.frame for r in likely_desat_rows])}\n")
        f.write(f"- CNR3-default scene candidates without passthrough: {frame_list([r.frame for r in default_scene_no_reset])}\n\n")

        f.write("## Worst frames by chroma mean absolute delta\n\n")
        f.write("| Frame | Class | Y MAD | U MAD | V MAD | U changed % | V changed % | Chroma mag delta | Default scene | Chroma scene | Exact | Notes |\n")
        f.write("|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n")
        for r in worst_chroma:
            f.write(
                f"| {r.frame} | {r.classification} | {fmt_float(r.y_mad)} | {fmt_float(r.u_mad)} | {fmt_float(r.v_mad)} | "
                f"{fmt_float(r.u_changed_pct)} | {fmt_float(r.v_changed_pct)} | {fmt_float(r.chroma_mag_mean_delta)} | "
                f"{bool_cell(r.cnr3_default_scene_candidate)} | {bool_cell(r.cnr3_chroma_scene_candidate)} | {1 if r.exact_all else 0} | {r.notes} |\n"
            )
        f.write("\n")

        f.write("## Worst frames by chroma magnitude loss\n\n")
        f.write("| Frame | Class | Chroma mag in | Chroma mag out | Chroma mag delta | Strong toward neutral % | Red/brown samples | Red/brown U delta | Red/brown V delta | Red/brown mag delta |\n")
        f.write("|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for r in worst_desat:
            f.write(
                f"| {r.frame} | {r.classification} | {fmt_float(r.chroma_mag_mean_input)} | {fmt_float(r.chroma_mag_mean_output)} | "
                f"{fmt_float(r.chroma_mag_mean_delta)} | {fmt_float(r.chroma_strong_toward_neutral_pct)} | "
                f"{r.red_brown_samples} | {fmt_float(r.red_brown_mean_u_delta)} | {fmt_float(r.red_brown_mean_v_delta)} | {fmt_float(r.red_brown_mean_mag_delta)} |\n"
            )
        f.write("\n")

        f.write("## Per-frame compact table\n\n")
        f.write("| Frame | Class | Y chg % | U chg % | V chg % | U MAD | V MAD | Chroma mag delta | Default scene | Chroma scene | Scene match | Exact | Notes |\n")
        f.write("|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n")
        for r in rows:
            f.write(
                f"| {r.frame} | {r.classification} | {fmt_float(r.y_changed_pct)} | {fmt_float(r.u_changed_pct)} | {fmt_float(r.v_changed_pct)} | "
                f"{fmt_float(r.u_mad)} | {fmt_float(r.v_mad)} | {fmt_float(r.chroma_mag_mean_delta)} | "
                f"{bool_cell(r.cnr3_default_scene_candidate)} | {bool_cell(r.cnr3_chroma_scene_candidate)} | {bool_cell(r.scene_mode_match)} | "
                f"{1 if r.exact_all else 0} | {r.notes} |\n"
            )


def analyze(args: argparse.Namespace) -> tuple[Summary, list[FrameStats], str, str, str]:
    rows: list[FrameStats] = []

    with Y4MReader(args.input) as input_reader, Y4MReader(args.output) as output_reader:
        input_info = input_reader.info
        output_info = output_reader.info
        validate_pair(input_info, output_info)

        threshold_default = cnr3_scene_threshold(
            args.scdthr,
            input_info.width,
            input_info.height,
            input_info.bits_per_sample,
            input_info.sub_sampling_w,
            input_info.sub_sampling_h,
            scene_chroma=False,
        )
        threshold_chroma = cnr3_scene_threshold(
            args.scdthr,
            input_info.width,
            input_info.height,
            input_info.bits_per_sample,
            input_info.sub_sampling_w,
            input_info.sub_sampling_h,
            scene_chroma=True,
        )

        prev_out: Optional[tuple[np.ndarray, np.ndarray, np.ndarray]] = None
        frame_index = 0
        while True:
            in_frame = input_reader.read_frame()
            out_frame = output_reader.read_frame()
            if in_frame is None and out_frame is None:
                break
            if in_frame is None or out_frame is None:
                raise ValueError("input/output frame-count mismatch")
            if args.max_frames is not None and frame_index >= args.max_frames:
                break

            in_y, in_u, in_v = in_frame
            out_y, out_u, out_v = out_frame
            row = analyze_frame(
                frame_index=frame_index,
                in_y=in_y,
                in_u=in_u,
                in_v=in_v,
                out_y=out_y,
                out_u=out_u,
                out_v=out_v,
                prev_out=prev_out,
                sub_sampling_w=input_info.sub_sampling_w,
                sub_sampling_h=input_info.sub_sampling_h,
                threshold_default=threshold_default,
                threshold_chroma=threshold_chroma,
                near_mad_threshold=args.near_mad_threshold,
                chroma_strong_threshold=args.chroma_strong_threshold,
                red_brown_v_threshold=args.red_brown_v_threshold,
            )
            rows.append(row)
            prev_out = (out_y, out_u, out_v)
            frame_index += 1

    summary = make_summary(input_info, output_info, rows, args.scdthr, threshold_default, threshold_chroma)

    out_prefix = args.out_prefix
    csv_path = args.csv or f"{out_prefix}_per_frame.csv"
    md_path = args.md or f"{out_prefix}_report.md"
    json_path = args.json or f"{out_prefix}_summary.json"

    if not args.no_csv:
        write_csv(csv_path, rows)
    if not args.no_md:
        write_md(md_path, summary, rows)
    if not args.no_json:
        write_json(json_path, summary)

    return summary, rows, csv_path, md_path, json_path


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Compare selected-input and CNR3-output Y4M clips for chroma attenuation/desaturation."
    )
    p.add_argument("--input", required=True, help="Selected input/reference Y4M clip.")
    p.add_argument("--output", required=True, help="Processed CNR3 output Y4M clip.")
    p.add_argument("--out-prefix", default="cnr3_chroma_delta", help="Output path prefix.")
    p.add_argument("--csv", default=None, help="Explicit CSV output path.")
    p.add_argument("--md", default=None, help="Explicit Markdown report path.")
    p.add_argument("--json", default=None, help="Explicit JSON summary path.")
    p.add_argument("--no-csv", action="store_true", help="Do not write CSV.")
    p.add_argument("--no-md", action="store_true", help="Do not write Markdown.")
    p.add_argument("--no-json", action="store_true", help="Do not write JSON.")
    p.add_argument("--max-frames", type=int, default=None, help="Optional frame limit for quick tests.")
    p.add_argument("--scdthr", type=float, default=10.0, help="CNR3/vscnr2-style scene threshold percentage. Default: 10.0")
    p.add_argument("--near-mad-threshold", type=float, default=0.05, help="Mean-absolute-delta threshold for near passthrough. Default: 0.05")
    p.add_argument("--chroma-strong-threshold", type=float, default=16.0, help="Input chroma magnitude threshold for strong-chroma mask. Default: 16.0")
    p.add_argument("--red-brown-v-threshold", type=int, default=144, help="Input V threshold for red/brown mask. Default: 144")
    return p.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    try:
        summary, rows, csv_path, md_path, json_path = analyze(args)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if not args.no_csv:
        print(f"wrote {csv_path}")
    if not args.no_md:
        print(f"wrote {md_path}")
    if not args.no_json:
        print(f"wrote {json_path}")

    print("\nSummary:")
    print(f"frames_analyzed={summary.frames_analyzed}")
    print(f"exact_all_frames={summary.exact_all_frames}")
    print(f"cnr3_default_scene_candidates={summary.cnr3_default_scene_candidates}")
    print(f"cnr3_chroma_scene_candidates={summary.cnr3_chroma_scene_candidates}")
    print(f"scene_candidate_mismatches={summary.scene_candidate_mismatches}")
    print(f"substantive_chroma_frames={summary.substantive_chroma_frames}")
    print(f"likely_desaturation_frames={summary.likely_desaturation_frames}")
    print(f"assessment={summary.final_assessment}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
