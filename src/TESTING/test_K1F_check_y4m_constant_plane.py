#!/usr/bin/env python3
# check_y4m_constant_plane.py
#
# Verify that a single-frame 8-bit YUV420 .y4m file has CONSTANT planes with exactly the
# expected Y, U, V values across every active sample. Used by the K.1E.2 harness to assert
# the frame-1 known-answer golden bytes (Y=128 U=161 V=95) without an A/B reference file.
#
# Usage:
#   check_y4m_constant_plane.py <file.y4m> <expected_Y> <expected_U> <expected_V> [frame_index]
#
# frame_index (optional, default 0): which FRAME in a multi-frame y4m to check, 0-based.
#   For a recursive filter, frame 1's predecessor must be computed first in the same process,
#   so the harness pipes frames 0..1 together and checks frame_index=1 (the second frame).
#
# Exit code:
#   0  -> at the selected frame, every active Y sample == expected_Y, U == expected_U, V == expected_V
#   1  -> mismatch, malformed file, or unexpected format
#
# This deliberately reads the raw y4m itself (no VapourSynth dependency) so it can run from the
# portable python without importing vs. It supports YUV420 (the harness's synthetic format).

import sys


def fail(msg):
    sys.stderr.write("check_y4m_constant_plane: FAIL - " + msg + "\n")
    sys.exit(1)


def main():
    if len(sys.argv) not in (5, 6):
        fail("usage: check_y4m_constant_plane.py <file.y4m> <Y> <U> <V> [frame_index]")

    path = sys.argv[1]
    try:
        exp_y = int(sys.argv[2])
        exp_u = int(sys.argv[3])
        exp_v = int(sys.argv[4])
        frame_index = int(sys.argv[5]) if len(sys.argv) == 6 else 0
    except ValueError:
        fail("expected Y/U/V (and optional frame_index) must be integers")
    if frame_index < 0:
        fail("frame_index must be >= 0")

    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError as exc:
        fail("cannot read file: %s" % exc)

    if not data:
        fail("file is empty (no frame produced)")

    # y4m stream header: "YUV4MPEG2 ...params...\n"
    nl = data.find(b"\n")
    if nl < 0:
        fail("no stream header newline found")
    header = data[:nl].decode("ascii", "replace")
    if not header.startswith("YUV4MPEG2"):
        fail("not a YUV4MPEG2 stream: %r" % header[:32])

    width = height = None
    colorspace = "420"  # y4m default when C param absent is 420
    for tok in header.split(" ")[1:]:
        if not tok:
            continue
        key, val = tok[0], tok[1:]
        if key == "W":
            width = int(val)
        elif key == "H":
            height = int(val)
        elif key == "C":
            colorspace = val
    if width is None or height is None:
        fail("stream header missing W/H: %r" % header)
    if not colorspace.startswith("420"):
        fail("expected 420 colorspace, got C%s" % colorspace)
    if (width % 2) or (height % 2):
        fail("420 requires even W/H, got %dx%d" % (width, height))

    y_size = width * height
    c_w = width // 2
    c_h = height // 2
    c_size = c_w * c_h
    need = y_size + 2 * c_size

    # Walk frame-by-frame to the requested index. Each frame is "FRAME...\n" + payload(need bytes).
    search_from = nl + 1
    body = None
    for idx in range(frame_index + 1):
        fpos = data.find(b"FRAME", search_from)
        if fpos < 0:
            fail("frame index %d not found (only %d frame(s) present; recursive predecessor "
                 "may not have been computed)" % (frame_index, idx))
        fnl = data.find(b"\n", fpos)
        if fnl < 0:
            fail("no frame-header newline found for frame %d" % idx)
        frame_body = data[fnl + 1:fnl + 1 + need]
        if len(frame_body) < need:
            fail("frame %d payload too short: have %d, need %d (Y=%d C=%d each)"
                 % (idx, len(frame_body), need, y_size, c_size))
        if idx == frame_index:
            body = frame_body
            break
        # advance past this frame's payload to look for the next FRAME marker
        search_from = fnl + 1 + need

    y_plane = body[0:y_size]
    u_plane = body[y_size:y_size + c_size]
    v_plane = body[y_size + c_size:y_size + 2 * c_size]

    def check_plane(name, plane, expected):
        # report the distinct values present; pass only if the single value == expected
        distinct = set(plane)
        if distinct != {expected & 0xFF}:
            sample = sorted(distinct)[:8]
            sys.stderr.write(
                "check_y4m_constant_plane: %s plane mismatch - expected constant %d, "
                "found distinct values %s%s\n"
                % (name, expected, sample, " ..." if len(distinct) > 8 else "")
            )
            return False
        return True

    ok_y = check_plane("Y", y_plane, exp_y)
    ok_u = check_plane("U", u_plane, exp_u)
    ok_v = check_plane("V", v_plane, exp_v)

    if ok_y and ok_u and ok_v:
        sys.stderr.write(
            "check_y4m_constant_plane: PASS - frame %d is constant Y=%d U=%d V=%d (%dx%d 420)\n"
            % (frame_index, exp_y, exp_u, exp_v, width, height)
        )
        sys.exit(0)
    sys.exit(1)


if __name__ == "__main__":
    main()
