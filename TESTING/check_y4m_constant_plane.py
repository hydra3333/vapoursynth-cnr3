#!/usr/bin/env python3
# check_y4m_constant_plane.py
#
# Verify that a single-frame 8-bit YUV420 .y4m file has CONSTANT planes with exactly the
# expected Y, U, V values across every active sample. Used by the K.1E.2 harness to assert
# the frame-1 known-answer golden bytes (Y=128 U=161 V=95) without an A/B reference file.
#
# Usage:
#   check_y4m_constant_plane.py <file.y4m> <expected_Y> <expected_U> <expected_V>
#
# Exit code:
#   0  -> every active Y sample == expected_Y, every active U == expected_U, every active V == expected_V
#   1  -> mismatch, malformed file, or unexpected format
#
# This deliberately reads the raw y4m itself (no VapourSynth dependency) so it can run from the
# portable python without importing vs. It supports YUV420 (the harness's synthetic format).

import sys


def fail(msg):
    sys.stderr.write("check_y4m_constant_plane: FAIL - " + msg + "\n")
    sys.exit(1)


def main():
    if len(sys.argv) != 5:
        fail("usage: check_y4m_constant_plane.py <file.y4m> <Y> <U> <V>")

    path = sys.argv[1]
    try:
        exp_y = int(sys.argv[2])
        exp_u = int(sys.argv[3])
        exp_v = int(sys.argv[4])
    except ValueError:
        fail("expected Y/U/V must be integers")

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

    # Frame header: "FRAME[...params...]\n"
    fpos = data.find(b"FRAME", nl + 1)
    if fpos < 0:
        fail("no FRAME marker found (no frame produced)")
    fnl = data.find(b"\n", fpos)
    if fnl < 0:
        fail("no frame-header newline found")

    y_size = width * height
    c_w = width // 2
    c_h = height // 2
    c_size = c_w * c_h
    need = y_size + 2 * c_size

    body = data[fnl + 1:]
    if len(body) < need:
        fail("frame payload too short: have %d, need %d (Y=%d C=%d each)"
             % (len(body), need, y_size, c_size))

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
            "check_y4m_constant_plane: PASS - frame is constant Y=%d U=%d V=%d (%dx%d 420)\n"
            % (exp_y, exp_u, exp_v, width, height)
        )
        sys.exit(0)
    sys.exit(1)


if __name__ == "__main__":
    main()
