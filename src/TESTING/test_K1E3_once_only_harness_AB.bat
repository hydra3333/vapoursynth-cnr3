@echo off
REM test_K1E3_once_only_harness_AB.bat
REM
REM K.1E.3 acceptance harness. Proves the recursive FILTERED-PREDECESSOR DISTINCTION at N=2.
REM Runs over a SYNTHETIC deterministic clip, frame-exact via --start/--end:
REM   RUN 1  frame 0, processing  -> real fresh-start output[0]                 (must SUCCEED)
REM   RUN 2  frame 0, passthrough -> source frame 0 (CNR3 bypassed)             (must SUCCEED)
REM   RUN 3  frames 0..1, proc    -> frame 1 predecessor-present output[1]      (must SUCCEED, KNOWN-ANSWER idx 1)
REM   RUN 4  frames 0..2, proc    -> frame 2 predecessor-present output[2]      (must SUCCEED, KNOWN-ANSWER idx 2)
REM   RUN 5  frame 3, processing  -> N>2 not-yet-implemented                    (must FAIL CLEANLY)
REM
REM SYNTHETIC SOURCE (constant planes):
REM   source[0]: Y128 U96  V160  -> output[0] == source[0] (fresh start)
REM   source[1]: Y128 U224 V32   -> output[1] = Y128 U161 V95   (K.1E.2 golden)
REM   source[2]: Y128 U192 V64   -> output[2] = Y128 U163 V93   (K.1E.3 golden)
REM
REM LOAD-BEARING R-ARCH-06 CHECK (frame 2):
REM   correct pred=filtered output[1]=161/95 -> 163/93 (golden, must match)
REM   wrong   pred=source[1]        =224/32  -> 222/34 (must NOT match)
REM   passthrough source[2]         =192/64  -> 192/64 (must NOT match)
REM   All three byte-distinct on both planes -> only genuine filtered-predecessor use hits 163/93.
REM
REM RUN 4 MUST request frames 0..2 in ONE process: output[2]'s predecessor output[1] is itself a
REM recursive compute, so the whole chain must be built in the same filter instance.

set "rrr=-r 1"

set "mode_passthrough=passthrough"
set "mode_processing=processing"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_K1E3_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

REM Golden frame-1 (index 1) and frame-2 (index 2) values:
set "golden1_y=128"
set "golden1_u=161"
set "golden1_v=95"
set "golden2_y=128"
set "golden2_u=163"
set "golden2_v=93"

REM Output files
set "f0proc_y4m=%source_path%\K1E3_frame0_processing_temp.y4m"
set "f0proc_err=%source_path%\K1E3_frame0_processing_temp_stderr.txt"
set "f0bypass_y4m=%source_path%\K1E3_frame0_passthrough_temp.y4m"
set "f0bypass_err=%source_path%\K1E3_frame0_passthrough_temp_stderr.txt"
set "f1proc_y4m=%source_path%\K1E3_frames01_processing_temp.y4m"
set "f1proc_err=%source_path%\K1E3_frames01_processing_temp_stderr.txt"
set "f2proc_y4m=%source_path%\K1E3_frames012_processing_temp.y4m"
set "f2proc_err=%source_path%\K1E3_frames012_processing_temp_stderr.txt"
set "f3proc_y4m=%source_path%\K1E3_frame3_processing_temp.y4m"
set "f3proc_err=%source_path%\K1E3_frame3_processing_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"
echo del /F "%runtime_dll_folder%\cnr3.dll"
del /F "%runtime_dll_folder%\cnr3.dll"
echo COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
echo dir /tw "%runtime_dll_folder%\cnr3.dll"
dir /tw "%runtime_dll_folder%\cnr3.dll"
echo.

REM Pre-delete RUN5 output so a stale file can't masquerade as a produced frame.
if exist "%f3proc_y4m%" del /F "%f3proc_y4m%"

REM === RUN 1: frame 0 processing (fresh-start output[0]) ===
echo. >"%f0proc_err%"
echo RUN 1: frame 0 PROCESSING (fresh-start output[0]) >>"%f0proc_err%"
echo "%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f0proc_y4m%" >>"%f0proc_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f0proc_y4m%" 2>>"%f0proc_err%"
@echo off
set "rc_f0proc=%ERRORLEVEL%"
echo (RUN1 frame0 processing exit=%rc_f0proc%) >>"%f0proc_err%"

REM === RUN 2: frame 0 passthrough (source bypass) ===
echo. >"%f0bypass_err%"
echo RUN 2: frame 0 PASSTHROUGH (source bypass) >>"%f0bypass_err%"
echo "%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_passthrough%" --container y4m "%vpy%" "%f0bypass_y4m%" >>"%f0bypass_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_passthrough%" --container y4m "%vpy%" "%f0bypass_y4m%" 2>>"%f0bypass_err%"
@echo off
set "rc_f0bypass=%ERRORLEVEL%"
echo (RUN2 frame0 passthrough exit=%rc_f0bypass%) >>"%f0bypass_err%"

REM === RUN 3: frames 0..1 processing (predecessor-present compute -> known-answer idx 1) ===
echo. >"%f1proc_err%"
echo RUN 3: frames 0..1 PROCESSING (frame 1 predecessor-present compute - KNOWN ANSWER) >>"%f1proc_err%"
echo "%vspipe%" %rrr% --start 0 --end 1 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f1proc_y4m%" >>"%f1proc_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f1proc_y4m%" 2>>"%f1proc_err%"
@echo off
set "rc_f1proc=%ERRORLEVEL%"
echo (RUN3 frames01 processing exit=%rc_f1proc%) >>"%f1proc_err%"

REM === RUN 4: frames 0..2 processing (predecessor-present compute -> known-answer idx 2) ===
echo. >"%f2proc_err%"
echo RUN 4: frames 0..2 PROCESSING (frame 2 predecessor-present compute - KNOWN ANSWER, R-ARCH-06) >>"%f2proc_err%"
echo "%vspipe%" %rrr% --start 0 --end 2 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f2proc_y4m%" >>"%f2proc_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 2 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f2proc_y4m%" 2>>"%f2proc_err%"
@echo off
set "rc_f2proc=%ERRORLEVEL%"
echo (RUN4 frames012 processing exit=%rc_f2proc%) >>"%f2proc_err%"

REM === RUN 5: frame 3 processing (must FAIL CLEANLY - N>2 not yet implemented) ===
echo. >"%f3proc_err%"
echo RUN 5: frame 3 PROCESSING (must FAIL CLEANLY - N^>2 not yet implemented) >>"%f3proc_err%"
echo "%vspipe%" %rrr% --start 3 --end 3 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f3proc_y4m%" >>"%f3proc_err%"
@echo on
"%vspipe%" %rrr% --start 3 --end 3 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f3proc_y4m%" 2>>"%f3proc_err%"
@echo off
set "rc_f3proc=%ERRORLEVEL%"
echo (RUN5 frame3 processing exit=%rc_f3proc%) >>"%f3proc_err%"
echo.

REM ================= CHECK A: frame-0 byte-compare =================
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK A - BYTE COMPARE frame0 processing vs passthrough 1>&2
echo ==================================================== 1>&2
fc /b "%f0proc_y4m%" "%f0bypass_y4m%" 1>&2
if errorlevel 1 (
    echo RESULT A: DIFFER  -- frame-0 fresh-start output is NOT byte-identical to source 1>&2
) else (
    echo RESULT A: BYTE-IDENTICAL  -- frame-0 fresh-start output matches source 1>&2
)

REM ================= CHECK B: frame-0 KDT markers =================
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK B - frame0 processing KDT markers 1>&2
echo ==================================================== 1>&2
findstr /C:"FRAME0-FRESH-START" "%f0proc_err%" 1>&2
if errorlevel 1 (
    findstr /C:"REAL_OUTPUT_FRAME0" "%f0proc_err%" 1>&2
    if errorlevel 1 ( echo RESULT B1: MISSING frame-0 fresh-start KDT 1>&2 ) else ( echo RESULT B1: REAL_OUTPUT_FRAME0 present 1>&2 )
) else ( echo RESULT B1: FRAME0-FRESH-START present 1>&2 )
findstr /C:"SCAFFOLD_NOT_FILTERED" "%f0proc_err%" 1>&2
if errorlevel 1 ( echo RESULT B2: clean  -- no SCAFFOLD_NOT_FILTERED on real frame-0 path 1>&2 ) else ( echo RESULT B2: UNEXPECTED SCAFFOLD_NOT_FILTERED 1>&2 )

REM ================= CHECK C: passthrough has no [KDT] =================
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK C - frame0 passthrough negative check (no [KDT]) 1>&2
echo ==================================================== 1>&2
findstr /C:"[KDT]" "%f0bypass_err%" 1>&2
if errorlevel 1 ( echo RESULT C: clean  -- no [KDT] in bypass 1>&2 ) else ( echo RESULT C: UNEXPECTED [KDT] in bypass run 1>&2 )

REM ================= CHECK D: frame 1 golden (index 1) =================
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK D - frame1 predecessor-present compute (KNOWN ANSWER Y=%golden1_y% U=%golden1_u% V=%golden1_v%) 1>&2
echo ==================================================== 1>&2
echo RUN3 frames01 processing vspipe exit code = %rc_f1proc% 1>&2
if "%rc_f1proc%"=="0" ( echo RESULT D1: PASS  -- frame 1 compute exited 0 1>&2 ) else ( echo RESULT D1: FAIL  -- frame 1 compute exited nonzero 1>&2 )
findstr /C:"PREDECESSOR-PRESENT-COMPUTE" "%f1proc_err%" 1>&2
if errorlevel 1 ( echo RESULT D2: FAIL  -- PREDECESSOR-PRESENT-COMPUTE KDT not found 1>&2 ) else ( echo RESULT D2: PASS  -- PREDECESSOR-PRESENT-COMPUTE KDT present 1>&2 )
"%python_exe%" "%~dp0test_K1E3_check_y4m_constant_plane.py" "%f1proc_y4m%" %golden1_y% %golden1_u% %golden1_v% 1 1>&2

if errorlevel 1 ( echo RESULT D3: FAIL  -- frame 1 bytes do NOT match golden Y=%golden1_y% U=%golden1_u% V=%golden1_v% 1>&2 ) else ( echo RESULT D3: PASS  -- frame 1 bytes match golden Y=%golden1_y% U=%golden1_u% V=%golden1_v% 1>&2 )

REM ================= CHECK E: frame 2 golden (index 2) - LOAD-BEARING R-ARCH-06 =================
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK E - frame2 FILTERED-PREDECESSOR DISTINCTION (KNOWN ANSWER Y=%golden2_y% U=%golden2_u% V=%golden2_v%) 1>&2
echo            golden 163/93 (pred=filtered output[1]); a source[1]-substitution bug yields 222/34; passthrough 192/64 1>&2
echo ==================================================== 1>&2
echo RUN4 frames012 processing vspipe exit code = %rc_f2proc% 1>&2
if "%rc_f2proc%"=="0" ( echo RESULT E1: PASS  -- frame 2 compute exited 0 1>&2 ) else ( echo RESULT E1: FAIL  -- frame 2 compute exited nonzero 1>&2 )
findstr /C:"source=2 pred=1" "%f2proc_err%" 1>&2
if errorlevel 1 ( echo RESULT E2: FAIL  -- frame-2 KDT source=2 pred=1 not found 1>&2 ) else ( echo RESULT E2: PASS  -- frame-2 KDT source=2 pred=1 present 1>&2 )
"%python_exe%" "%~dp0test_K1E3_check_y4m_constant_plane.py" "%f2proc_y4m%" %golden2_y% %golden2_u% %golden2_v% 2 1>&2
if errorlevel 1 ( echo RESULT E3: FAIL  -- frame 2 bytes do NOT match golden Y=%golden2_y% U=%golden2_u% V=%golden2_v% ^(R-ARCH-06 NOT proven^) 1>&2 ) else ( echo RESULT E3: PASS  -- frame 2 bytes match golden Y=%golden2_y% U=%golden2_u% V=%golden2_v% ^(filtered-predecessor proven^) 1>&2 )

REM ================= CHECK F: frame 3 N>2 clean refusal (INVERTED) =================
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK F - frame3 N^>2 clean-failure (INVERTED: nonzero exit is PASS) 1>&2
echo ==================================================== 1>&2
echo RUN5 frame3 processing vspipe exit code = %rc_f3proc% 1>&2
if "%rc_f3proc%"=="0" ( echo RESULT F1: FAIL  -- frame 3 returned exit 0; N^>2 was NOT cleanly refused 1>&2 ) else ( echo RESULT F1: PASS  -- frame 3 exited nonzero, consistent with clean refusal 1>&2 )
findstr /C:"NOT-YET-IMPLEMENTED" "%f3proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT F2: FAIL  -- NOT-YET-IMPLEMENTED KDT not found for frame 3 1>&2
) else (
    findstr /C:"after-frame2-before-recovery-wiring" "%f3proc_err%" 1>&2
    if errorlevel 1 ( echo RESULT F2: PARTIAL  -- NOT-YET-IMPLEMENTED present but branch token not after-frame2-before-recovery-wiring 1>&2 ) else ( echo RESULT F2: PASS  -- NOT-YET-IMPLEMENTED branch=after-frame2-before-recovery-wiring present 1>&2 )
)
if not exist "%f3proc_y4m%" ( echo RESULT F3: PASS  -- no output file produced for frame 3 1>&2 ) else ( for %%F in ("%f3proc_y4m%") do echo RESULT F3: CHECK  -- frame 3 output file is %%~zF bytes ^(47 = header-only = correct refusal^) 1>&2 )

echo. 1>&2
echo ==================================================== 1>&2
echo SUMMARY (read the per-check RESULT lines above; this line is a label, not a verdict): 1>&2
echo   A byte-identical / B frame0-KDT / C bypass-clean / D frame1 161/95 / 1>&2
echo   E frame2 163/93 (R-ARCH-06 load-bearing) / F frame3 N^>2 refusal 1>&2
echo ==================================================== 1>&2

pause
