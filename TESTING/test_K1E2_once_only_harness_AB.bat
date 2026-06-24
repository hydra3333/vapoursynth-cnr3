@echo off
REM test_K1E2_once_only_harness_AB.bat
REM
REM K.1E.2 acceptance harness. FOUR vspipe runs over a SYNTHETIC deterministic clip,
REM frame-exact via --start/--end:
REM   RUN 1  frame 0, processing  -> real fresh-start output[0]              (must SUCCEED)
REM   RUN 2  frame 0, passthrough -> source frame 0 (CNR3 bypassed)          (must SUCCEED)
REM   RUN 3  frame 1, processing  -> predecessor-present compute output[1]   (must SUCCEED, KNOWN-ANSWER)
REM   RUN 4  frame 2, processing  -> N>1 not-yet-implemented                 (must FAIL CLEANLY)
REM
REM SYNTHETIC SOURCE (from the .vpy, constant planes):
REM   source[0]: Y=128 U=96  V=160   -> output[0] == source[0] (fresh start)
REM   source[1]: Y=128 U=224 V=32
REM GOLDEN expected output[1]: Y=128 U=161 V=95
REM   (independently verified against the committed response-table + P.11B source;
REM    distinct from predecessor 96/160 AND source 224/32 -> strong three-way discriminator)
REM
REM CHECKS:
REM   A. fc /b RUN1 vs RUN2  -> BYTE-IDENTICAL  (frame-0 fresh-start copies source Y/U/V).
REM   B. RUN1 stderr contains [KDT] FRAME0-FRESH-START (or REAL_OUTPUT_FRAME0), and does NOT
REM      contain SCAFFOLD_NOT_FILTERED.
REM   C. RUN2 (bypass) stderr contains NO [KDT].
REM   D. RUN3 frame 1: SUCCEEDED (exit 0), KDT contains PREDECESSOR-PRESENT-COMPUTE, and the
REM      decoded frame-1 active-pixel values are EXACTLY Y=128 U=161 V=95 (KNOWN-ANSWER).
REM      Also confirm the result is NOT the predecessor (U=96/V=160) and NOT source[1]
REM      (U=224/V=32) -- the golden triple already guarantees this, the check just asserts it.
REM   E. RUN4 frame 2: FAILED CLEANLY -- vspipe exited NONZERO (INVERTED: nonzero = PASS),
REM      stderr contains [KDT] NOT-YET-IMPLEMENTED branch=after-frame1-before-recovery-wiring,
REM      and no valid frame was produced.

REM set "rrr="
set "rrr=-r 1"

set "mode_passthrough=passthrough"
set "mode_processing=processing"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_K1E2_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

REM NOTE: this harness uses a SYNTHETIC BlankClip source built inside the .vpy, so there is no
REM external 'sourcefile' argument. The golden values live in the .vpy and in CHECK D below.

REM GOLDEN expected frame-1 values (must match the .vpy synthetic source + verified arithmetic):
set "golden_y=128"
set "golden_u=161"
set "golden_v=95"

REM RUN 1: frame 0, processing (real fresh-start output[0])
set "f0proc_y4m=%source_path%\K1E2_frame0_processing_temp.y4m"
set "f0proc_err=%source_path%\K1E2_frame0_processing_temp_stderr.txt"
REM RUN 2: frame 0, passthrough (source bypass)
set "f0bypass_y4m=%source_path%\K1E2_frame0_passthrough_temp.y4m"
set "f0bypass_err=%source_path%\K1E2_frame0_passthrough_temp_stderr.txt"
REM RUN 3: frame 1, processing (predecessor-present compute -> known-answer)
set "f1proc_y4m=%source_path%\K1E2_frame1_processing_temp.y4m"
set "f1proc_err=%source_path%\K1E2_frame1_processing_temp_stderr.txt"
REM RUN 4: frame 2, processing (must fail cleanly)
set "f2proc_y4m=%source_path%\K1E2_frame2_processing_temp.y4m"
set "f2proc_err=%source_path%\K1E2_frame2_processing_temp_stderr.txt"

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

REM Pre-delete RUN4 output so a stale file can't masquerade as a produced frame.
if exist "%f2proc_y4m%" del /F "%f2proc_y4m%"

REM ===================================================================================================================================================================
echo. >"%f0proc_err%"
echo ==================================================== >>"%f0proc_err%"
echo RUN 1: frame 0 PROCESSING (real fresh-start output[0]) >>"%f0proc_err%"
echo ==================================================== >>"%f0proc_err%"
echo "%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f0proc_y4m%" >>"%f0proc_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f0proc_y4m%" 2>>"%f0proc_err%"
@echo off
set "rc_f0proc=%ERRORLEVEL%"
echo (RUN1 frame0 processing exit=%rc_f0proc%) >>"%f0proc_err%"

REM ===================================================================================================================================================================
echo. >"%f0bypass_err%"
echo ==================================================== >>"%f0bypass_err%"
echo RUN 2: frame 0 PASSTHROUGH (source bypass) >>"%f0bypass_err%"
echo ==================================================== >>"%f0bypass_err%"
echo "%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_passthrough%" --container y4m "%vpy%" "%f0bypass_y4m%" >>"%f0bypass_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_passthrough%" --container y4m "%vpy%" "%f0bypass_y4m%" 2>>"%f0bypass_err%"
@echo off
set "rc_f0bypass=%ERRORLEVEL%"
echo (RUN2 frame0 passthrough exit=%rc_f0bypass%) >>"%f0bypass_err%"

REM ===================================================================================================================================================================
echo. >"%f1proc_err%"
echo ==================================================== >>"%f1proc_err%"
echo RUN 3: frame 1 PROCESSING (predecessor-present compute - KNOWN ANSWER) >>"%f1proc_err%"
echo ==================================================== >>"%f1proc_err%"
echo "%vspipe%" %rrr% --start 1 --end 1 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f1proc_y4m%" >>"%f1proc_err%"
@echo on
"%vspipe%" %rrr% --start 1 --end 1 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f1proc_y4m%" 2>>"%f1proc_err%"
@echo off
set "rc_f1proc=%ERRORLEVEL%"
echo (RUN3 frame1 processing exit=%rc_f1proc%) >>"%f1proc_err%"

REM ===================================================================================================================================================================
echo. >"%f2proc_err%"
echo ==================================================== >>"%f2proc_err%"
echo RUN 4: frame 2 PROCESSING (must FAIL CLEANLY - N^>1 not yet implemented) >>"%f2proc_err%"
echo ==================================================== >>"%f2proc_err%"
echo "%vspipe%" %rrr% --start 2 --end 2 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f2proc_y4m%" >>"%f2proc_err%"
@echo on
"%vspipe%" %rrr% --start 2 --end 2 --arg mode="%mode_processing%" --container y4m "%vpy%" "%f2proc_y4m%" 2>>"%f2proc_err%"
@echo off
set "rc_f2proc=%ERRORLEVEL%"
echo (RUN4 frame2 processing exit=%rc_f2proc%) >>"%f2proc_err%"
echo.

REM ===================================================================================================================================================================
REM CHECK A: frame-0 byte-compare (processing vs passthrough) must be byte-identical
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

REM ===================================================================================================================================================================
REM CHECK B: RUN1 stderr has frame-0 fresh-start KDT and NO SCAFFOLD_NOT_FILTERED
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK B - frame0 processing KDT markers 1>&2
echo ==================================================== 1>&2
findstr /C:"FRAME0-FRESH-START" "%f0proc_err%" 1>&2
if errorlevel 1 (
    findstr /C:"REAL_OUTPUT_FRAME0" "%f0proc_err%" 1>&2
    if errorlevel 1 (
        echo RESULT B1: MISSING frame-0 fresh-start KDT  -- real frame-0 trace not found 1>&2
    ) else (
        echo RESULT B1: REAL_OUTPUT_FRAME0 present 1>&2
    )
) else (
    echo RESULT B1: FRAME0-FRESH-START present 1>&2
)
findstr /C:"SCAFFOLD_NOT_FILTERED" "%f0proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT B2: clean  -- no SCAFFOLD_NOT_FILTERED on the real frame-0 path, as expected 1>&2
) else (
    echo RESULT B2: UNEXPECTED SCAFFOLD_NOT_FILTERED  -- scaffold marker leaked onto real frame-0 1>&2
)

REM ===================================================================================================================================================================
REM CHECK C: RUN2 (bypass) stderr has NO [KDT]
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK C - frame0 passthrough negative check (no [KDT]) 1>&2
echo ==================================================== 1>&2
findstr /C:"[KDT]" "%f0bypass_err%" 1>&2
if errorlevel 1 (
    echo RESULT C: clean  -- no [KDT] in bypass, as expected 1>&2
) else (
    echo RESULT C: UNEXPECTED [KDT] in bypass run 1>&2
)

REM ===================================================================================================================================================================
REM CHECK D: RUN3 (frame 1) must SUCCEED and hit the golden bytes.
REM   D1 - vspipe exit was ZERO (compute succeeded)
REM   D2 - stderr contains [KDT] PREDECESSOR-PRESENT-COMPUTE
REM   D3 - decoded frame-1 active pixels are EXACTLY Y=golden_y U=golden_u V=golden_v
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK D - frame1 predecessor-present compute (KNOWN ANSWER Y=%golden_y% U=%golden_u% V=%golden_v%) 1>&2
echo ==================================================== 1>&2
echo RUN3 frame1 processing vspipe exit code = %rc_f1proc% 1>&2
if "%rc_f1proc%"=="0" (
    echo RESULT D1: PASS  -- frame 1 compute exited 0 1>&2
) else (
    echo RESULT D1: FAIL  -- frame 1 compute exited nonzero; predecessor-present compute did not succeed 1>&2
)
findstr /C:"PREDECESSOR-PRESENT-COMPUTE" "%f1proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT D2: FAIL  -- PREDECESSOR-PRESENT-COMPUTE KDT line not found for frame 1 1>&2
) else (
    echo RESULT D2: PASS  -- PREDECESSOR-PRESENT-COMPUTE KDT line present for frame 1 1>&2
)
REM D3: decode the y4m frame-1 active pixels and assert the exact golden triple.
"%python_exe%" "%~dp0check_y4m_constant_plane.py" "%f1proc_y4m%" %golden_y% %golden_u% %golden_v% 1>&2
if errorlevel 1 (
    echo RESULT D3: FAIL  -- frame 1 bytes do NOT match golden Y=%golden_y% U=%golden_u% V=%golden_v% 1>&2
) else (
    echo RESULT D3: PASS  -- frame 1 bytes match golden Y=%golden_y% U=%golden_u% V=%golden_v% 1>&2
)

REM ===================================================================================================================================================================
REM CHECK E: RUN4 (frame 2) must FAIL CLEANLY.
REM   E1 - vspipe exit was NONZERO (INVERTED: nonzero = PASS here)
REM   E2 - stderr contains [KDT] NOT-YET-IMPLEMENTED branch=after-frame1-before-recovery-wiring
REM   E3 - no valid frame produced (output .y4m absent or zero bytes)
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK E - frame2 N^>1 clean-failure (INVERTED: nonzero exit is PASS) 1>&2
echo ==================================================== 1>&2
echo RUN4 frame2 processing vspipe exit code = %rc_f2proc% 1>&2
if "%rc_f2proc%"=="0" (
    echo RESULT E1: FAIL  -- frame 2 returned exit 0; N^>1 was NOT cleanly refused 1>&2
) else (
    echo RESULT E1: PASS  -- frame 2 exited nonzero, consistent with clean refusal 1>&2
)
findstr /C:"NOT-YET-IMPLEMENTED" "%f2proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT E2: FAIL  -- NOT-YET-IMPLEMENTED KDT line not found for frame 2 1>&2
) else (
    findstr /C:"after-frame1-before-recovery-wiring" "%f2proc_err%" 1>&2
    if errorlevel 1 (
        echo RESULT E2: PARTIAL  -- NOT-YET-IMPLEMENTED present but branch token not the expected after-frame1-before-recovery-wiring 1>&2
    ) else (
        echo RESULT E2: PASS  -- NOT-YET-IMPLEMENTED branch=after-frame1-before-recovery-wiring present for frame 2 1>&2
    )
)
if not exist "%f2proc_y4m%" (
    echo RESULT E3: PASS  -- no output file produced for frame 2 1>&2
) else (
    for %%F in ("%f2proc_y4m%") do set "f2size=%%~zF"
    call :report_f2size
)

echo. 1>&2
echo ==================================================== 1>&2
echo SUMMARY: A byte-identical / B1 frame0-fresh-start / B2 no-scaffold / C bypass-clean / 1>&2
echo          D1 frame1-exit0 / D2 PREDECESSOR-PRESENT-COMPUTE / D3 golden-bytes / 1>&2
echo          E1 frame2-nonzero-exit / E2 NOT-YET-IMPLEMENTED / E3 no-frame  =  all PASS for K.1E.2 green 1>&2
echo ==================================================== 1>&2

pause
goto :eof

:report_f2size
if "%f2size%"=="0" (
    echo RESULT E3: PASS  -- frame 2 output file exists but is zero bytes 1>&2
) else (
    echo RESULT E3: CHECK  -- frame 2 output file is %f2size% bytes; inspect whether a valid y4m frame was written 1>&2
)
goto :eof
