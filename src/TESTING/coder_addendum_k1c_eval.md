Acknowledgement received and accepted — both scope-boundary confirmations and the A/B harness
commitment are correct, and nothing drifted. Before you build, two final build notes and a
description of how the patch will be evaluated, so you can reason about and self-check your own
output. None of this reopens anything; it tightens two routing details and closes the gap of
you not seeing the instrument that measures the scaffold.

---------------------------------------------------------------------------------------------
TWO BUILD NOTES (fold into the patch; confirm in a one-line reply)
---------------------------------------------------------------------------------------------

1. [KDT] MUST BE EMITTED ONLY FROM INSIDE THE getFrame CALLBACK — never at plugin load or
   filter registration/create.
   The per-frame scaffold trace is tied to frame PROCESSING, not to the plugin existing. A
   [KDT] line emitted at VapourSynthPluginInit2 or at filter-create time would (a) be a
   fencing leak — the scaffold trace firing merely because the DLL loaded — and (b) break the
   harness's negative check (see below): the passthrough run loads the DLL but never enters
   your getFrame, so it must emit ZERO [KDT] lines. Keep every [KDT] / [KDT-SUMMARY] emission
   strictly inside the live getFrame callback path.

2. ALL DIAGNOSTIC OUTPUT TO stderr; stdout IS EXCLUSIVELY THE FRAME PIPE.
   You already confirmed stdout carries only frame data — this nails the converse: every byte
   of diagnostic/scaffold output (not just [KDT] — any print, any "scaffold active" note,
   anything) goes to stderr. The harness writes frames to a file via vspipe's positional
   output arg and greps stderr; any stray write to stdout corrupts the frame stream and breaks
   the byte-compare in a way that looks like a pixel difference but isn't. stdout = frames
   only, full stop.

---------------------------------------------------------------------------------------------
EVALUATION MEASUREMENT AND CRITERIA (how the scaffold is judged — for your understanding)
---------------------------------------------------------------------------------------------

The acceptance harness is coordinator-supplied and is NOT part of your patch; you do not build
or modify it. It is described here only so you can predict and self-check your own results.
(The literal .vpy/.bat are deliberately not shared — the harness is an INDEPENDENT check, so a
scaffold that passes it is stronger evidence if you did not build to the exact artifact. If a
run later fails in a way you cannot diagnose from the failure map below, we will share the
files for joint debugging at that point.)

THE INSTRUMENT:
  - One parameterised .vpy with two args: mode = passthrough | processing, and a scenario
    selector. For K.1C the scenario is S1 (jump_to_frames=[0], segment_length=10 — 10 in-order
    frames, no seeks, no recovery), run on the progressive 576p50 source.
  - A .bat runs that one .vpy TWICE under  vspipe -r 1 --container y4m , to two .y4m files,
    capturing each run's stderr to its own text file:
        PART A: mode=passthrough  -> CNR3 is BYPASSED in the graph (clip = source straight
                through). Your getFrame never runs. -> passthrough.y4m + passthrough_stderr.txt
        PART B: mode=processing   -> the chain routes through core.cnr3.CNR3, i.e. YOUR live
                scaffold getFrame. -> processing.y4m + processing_stderr.txt

THE MECHANISM (why this exercises your code):
  - VapourSynth pulls frames BACKWARD up the graph, so when the output is pulled, your
    getFrame receives the specific frame requests for the selected scenario. Under -r 1 they
    arrive strictly one at a time, in deterministic order — for S1 that is frames 0..9 in
    order, one per getFrame entry.
  - 576p50 is progressive, so split_into_fields returns the clip unchanged and reweave is NOT
    called. The passthrough vs processing comparison therefore isolates EXACTLY your scaffold's
    effect, with no field round-trip to confound it.

SUCCESS / FAIL COMPUTATION (three checks):
  - BYTE COMPARE:  fc /b passthrough.y4m processing.y4m  -> must be BYTE-IDENTICAL.
        (A true passthrough scaffold returns the source frame bit-unchanged, so processing
        output must equal bypass output exactly.)
  - KDT PRESENT:   findstr /C:"[KDT]" processing_stderr.txt  -> must FIND [KDT] lines, plus a
        [KDT-SUMMARY] line. (Your scaffold getFrame ran and traced.)
  - KDT ABSENT:    findstr /C:"[KDT]" passthrough_stderr.txt -> must find NOTHING. (CNR3 is not
        in the passthrough chain, so your getFrame never ran — this is what build note 1
        protects: no [KDT] at load/registration.)
  Plus, unchanged by this phase: the existing cache-core selftest four-way stays 47/47 /
  forced-fail 46/47 / verbose 47/47.

FAILURE-INTERPRETATION MAP (what a red result tells you):
  - fc /b reports DIFFER  -> your scaffold ALTERED pixels; it is not a true passthrough
        (returning a copy is fine only if bit-identical; returning anything computed is not).
  - no [KDT] in processing -> your getFrame was not entered, OR the trace was not emitted, OR
        it went to stdout instead of stderr (build note 2).
  - [KDT] appears in passthrough -> you emitted [KDT] at plugin load / filter registration
        rather than strictly inside getFrame (build note 1).
  - selftest four-way changed -> the patch touched cache-core / selftest state it should not
        have (scope: vapoursynth-Cnr3.cpp + cnr3_build_config.h only).

---------------------------------------------------------------------------------------------

Send a one-line confirmation of build notes 1 and 2, and proceed to the read-first K.1C patch.
I will review the patch against the fences, the file-touch scope, and — via the coordinator
running the harness above — the A/B byte-compare plus the unchanged selftest four-way.
