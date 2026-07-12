# CNR3 — Mode x -r Timing + Correctness Test Plan (v1, 2026-07-12)

Purpose: on a LONG clip, measure throughput and prove output correctness across all three filter modes and
several in-flight depths, and settle the real sweet-spot question (does concurrency help heavy work
end-to-end, or does fmParallel overcompute steal encoder cores). Prepared off the A2 investigation; see
A2_first_findings.md for the defect context.

## 0. Standing lesson (do not skip)
VERIFY filter_mode= IN EVERY LOG. Filter mode is compile-time; the filename and the -r value do NOT prove
which mode ran. A mislabelled run nearly produced a false conclusion twice this session. Name each output
file from the log's own filter_mode/marker, not from what you intended to build.

## 1. Clip
- >= 2000 frames, preferably the full sequential source segment (200 was too short; fps was startup-noise
  dominated).
- Same clip for all runs (correctness hashing in section 4 requires identical input).
- NORMAL cache profile (TINY off) unless a run is specifically probing eviction.

## 2. The matrix (9 runs: mode x depth)
Build once per mode (compile-time selector in cnr3_build_config.h; uncomment exactly one), then run the
three depths without rebuilding:

  fmUnordered         : -r 1 , -r 4 , no-r
  fmParallelRequests  : -r 1 , -r 4 , no-r
  fmParallel          : -r 1 , -r 4 , no-r

Expected from A2 (to confirm at scale):
  fmUnordered        : clean at all depths (0 discards).
  fmParallelRequests : clean at all depths incl. heavy recovery (0 discards; serial compute adopts).
  fmParallel         : 0 discards at -r 1; wasted recompute from -r 2 up, scaling with depth and now with
                       CLIP LENGTH -> expect the -r 4 / no-r fmParallel runs to be slow and heavy.

## 3. Output Y4M (not NUL)
Write real Y4M for every run so output can be hashed:
  vspipe -c y4m ... script.vpy "D:\TEST\out_<mode>_<rtag>.y4m"
(Name <mode> from the verified filter_mode= line.)

## 4. Correctness proof across modes (the real prize)
Because output[N] = f(source[N], output[N-1]) is deterministic, mode and -r change only HOW frames are
scheduled, never WHAT they are. Therefore ALL 9 Y4M outputs MUST be byte-identical if the plugin is correct.
This directly proves whether the fmParallel defect only wastes work (expected) or ever corrupts output.

Windows-native hashing (no install; certutil is built in):
  certutil -hashfile "D:\TEST\out_fmparallel_r4.y4m" SHA256

Hash all and eyeball:
  for %F in (D:\TEST\out_*.y4m) do @echo %F & @certutil -hashfile "%F" SHA256 | findstr /v ":"

Pass/fail: all 9 hashes identical = correctness green across every mode/depth. ANY differing hash = that
run's mode/-r corrupted output -> a serious finding, far above the overcompute issue. (Y4M headers are
identical across runs of the same dimensions/framerate, so whole-file hash is valid; if a header ever
differs, hash frame data only.)

Alternative to hashing: pick one run as reference and  fc /b  each other output against it.

## 5. Stats pull (findstr) -- hand this to the coder as the harness grep
set "log=D:\TEST\<the run log>.txt"
findstr /C:"filter_mode=" /C:"Output " /C:"frames_computed  " /C:"bailed_after_compute_because" /C:"duplicates_seen" /C:"stores_total" /C:"frames_evicted" /C:"recovery_plans_created" /C:"holes_identified" /C:"recovery_span_mean" /C:"out_of_order_count" /C:"MISMATCH" "%log%"

Notes:
- "Output " matches the  Output N frames in X.XX seconds (YY fps)  line (time AND fps).
- filter_mode= is first so every pull self-documents the mode (section 0).
- stores_total + frames_evicted were the pair that distinguished "not visible" from "eviction" in A2;
  keep them in the standard pull.
- MISMATCH surfaces the store self-check (kept-vs-attempted definition issue; expected on fmParallel >= -r 2).

## 6. End-to-end timing WITH encoder (the sweet-spot question)
vspipe fps = filter throughput only. The real question is whether concurrency helps HEAVY work end-to-end,
or the fmParallel overcompute steals encoder cores. Measure at least the three no-r runs (one per mode)
with ffmpeg in the pipe, capturing three numbers: vspipe fps, ffmpeg fps, and end-to-end wall-clock.

Option A (redirections INSIDE the scriptblock so stderr logs are preserved; avoids a second cmd shell):
  $t = Measure-Command {
      cmd /c 'vspipe -c y4m script.vpy - 2> vspipe_stderr.txt | ffmpeg -i - <encode args> out.mkv 2> ffmpeg_stderr.txt'
  }
  "END_TO_END_SECONDS=$($t.TotalSeconds)" | Tee-Object -Append end2end.txt

- vspipe fps  <- vspipe_stderr.txt  (the Output ... fps line)
- ffmpeg fps  <- ffmpeg_stderr.txt  (ffmpeg's frame=... fps=... line)
- end-to-end  <- end2end.txt        (Measure-Command wall-clock of the whole pipe)

Interpretation:
- If vspipe fps DROPS when ffmpeg is attached vs the NUL run -> filter and encoder are competing for cores.
- Compare end-to-end wall-clock across modes: does fmParallelRequests beat fmUnordered on heavy work? Does
  fmParallel's overcompute erase its apparent speed once a real encoder shares the cores?
- On >=2000 heavy frames, startup overhead is a small fraction, so the wall-clock DELTA between modes is the
  real signal.

## 7. What each result would mean
- All 9 hashes identical -> output correctness proven across all modes (expected; x=0 supports it).
- fmParallelRequests end-to-end <= fmUnordered on heavy footage -> fmParallelRequests is the production
  sweet spot (clean AND faster). This is the A3 question the long clip finally answers.
- fmParallel end-to-end worse than fmParallelRequests -> confirms the compute-reservation fix is needed
  before fmParallel is worth using.
- Any hash mismatch -> STOP; output corruption under that mode is a correctness defect above everything else.
