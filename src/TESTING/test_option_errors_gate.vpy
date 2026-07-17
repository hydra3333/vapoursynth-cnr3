# test_option_errors_gate.vpy
# CNR3 RIDER.option-error-messages -- designer-owned runtime gate harness.
# Reads the option set from environment variable CNR3_TEST_ARGS and applies it
# to core.cnr3.CNR3(). The companion .bat sets the variable per case and
# checks exit code + stderr text. Python-expression syntax is deliberate: it
# lets cases express newlines ('bad\nvalue') and long strings ('a'*200).
#
# CNR3_TEST_ARGS examples:
#   y_threshold=256
#   y_curve='wobbly'
#   y_curve='bad\nvalue'
#   y_curve='a'*200
#   (empty) -> no options (valid baseline case)

import os
import vapoursynth as vs

core = vs.core

args_text = os.environ.get("CNR3_TEST_ARGS", "").strip()
kwargs = eval("dict({})".format(args_text)) if args_text else {}

# Small, fast, format-compatible clip: 16 frames of YUV420P8.
clip = core.std.BlankClip(width=640, height=576, format=vs.YUV420P8,
                          length=16, fpsnum=25, fpsden=1, color=[81, 90, 240])

out = core.cnr3.CNR3(clip, **kwargs)
out.set_output()
