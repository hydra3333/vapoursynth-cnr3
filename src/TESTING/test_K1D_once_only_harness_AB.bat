@echo off
REM test_K1D_once_only_harness_AB.bat
REM
REM K.1D acceptance harness. THREE vspipe runs, frame-exact via --start/--end:
REM   RUN 1  frame 0, processing  -> real fresh-start output[0]   (must SUCCEED)
REM   RUN 2  frame 0, passthrough -> source frame 0 (CNR3 bypassed) (must SUCCEED)
REM   RUN 3  frame 1, processing  -> N>0 not-yet-implemented        (must FAIL CLEANLY)
REM
REM CHECKS:
REM   A. fc /b RUN1 vs RUN2  -> BYTE-IDENTICAL (fresh-start copies source Y/U/V).
REM   B. RUN1 stderr contains [KDT] FRAME0-FRESH-START, and does NOT contain SCAFFOLD_NOT_FILTERED.
REM   C. RUN2 (bypass) stderr contains NO [KDT].
REM   D. RUN3 FAILED CLEANLY: vspipe exited NONZERO (expected here), stderr contains
REM      [KDT] NOT-YET-IMPLEMENTED, and no valid frame was produced. NOTE the success
REM      condition for RUN3 is INVERTED: a nonzero exit is a PASS, a zero exit is a FAIL.

REM set "rrr="
set "rrr=-r 1"

set "mode_passthrough=passthrough"
set "mode_processing=processing"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_K1D_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

REM ---------------------------------------------------------------
REM UN-COMMENT ONLY ONE SOURCE  (p50 is the standard; frame 0 / frame 1 exist on any source)
REM interlaced
REM set "sourcefile=%source_path%\000_Example_576i.mp4"
REM progressive 25fps
REM set "sourcefile=%source_path%\000_Example_576p25.mp4"
REM progressive 50fps
set "sourcefile=%source_path%\000_Example_576p50.mp4"
REM ---------------------------------------------------------------

REM RUN 1: frame 0, processing (real fresh-start output[0])
set "f0proc_y4m=%sourcefile%_K1D_frame0_processing_temp.y4m"
set "f0proc_err=%sourcefile%_K1D_frame0_processing_temp_stderr.txt"
REM RUN 2: frame 0, passthrough (source bypass)
set "f0bypass_y4m=%sourcefile%_K1D_frame0_passthrough_temp.y4m"
set "f0bypass_err=%sourcefile%_K1D_frame0_passthrough_temp_stderr.txt"
REM RUN 3: frame 1, processing (must fail cleanly)
set "f1proc_y4m=%sourcefile%_K1D_frame1_processing_temp.y4m"
set "f1proc_err=%sourcefile%_K1D_frame1_processing_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"
echo del /F "%runtime_dll_folder%\cnr3.dll"
del /F "%runtime_dll_folder%\cnr3.dll"
echo dir /tw "%runtime_dll_folder%\cnr3.dll"
dir /tw "%runtime_dll_folder%\cnr3.dll"
echo COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
echo dir /tw "%runtime_dll_folder%\cnr3.dll"
dir /tw "%runtime_dll_folder%\cnr3.dll"
echo.

REM Pre-delete RUN3 output so a stale file can't masquerade as a produced frame.
if exist "%f1proc_y4m%" del /F "%f1proc_y4m%"

REM ===================================================================================================================================================================
echo. >"%f0proc_err%"
echo ==================================================== >>"%f0proc_err%"
echo RUN 1: frame 0 PROCESSING (real fresh-start output[0]) >>"%f0proc_err%"
echo ==================================================== >>"%f0proc_err%"
echo "%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_processing%" --arg sourcefile="%sourcefile%" --container y4m "%vpy%" "%f0proc_y4m%" >>"%f0proc_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_processing%" --arg sourcefile="%sourcefile%" --container y4m "%vpy%" "%f0proc_y4m%" 2>>"%f0proc_err%"
@echo off
set "rc_f0proc=%ERRORLEVEL%"
echo (RUN1 frame0 processing exit=%rc_f0proc%) >>"%f0proc_err%"

REM ===================================================================================================================================================================
echo. >"%f0bypass_err%"
echo ==================================================== >>"%f0bypass_err%"
echo RUN 2: frame 0 PASSTHROUGH (source bypass) >>"%f0bypass_err%"
echo ==================================================== >>"%f0bypass_err%"
echo "%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_passthrough%" --arg sourcefile="%sourcefile%" --container y4m "%vpy%" "%f0bypass_y4m%" >>"%f0bypass_err%"
@echo on
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="%mode_passthrough%" --arg sourcefile="%sourcefile%" --container y4m "%vpy%" "%f0bypass_y4m%" 2>>"%f0bypass_err%"
@echo off
set "rc_f0bypass=%ERRORLEVEL%"
echo (RUN2 frame0 passthrough exit=%rc_f0bypass%) >>"%f0bypass_err%"

REM ===================================================================================================================================================================
echo. >"%f1proc_err%"
echo ==================================================== >>"%f1proc_err%"
echo RUN 3: frame 1 PROCESSING (must FAIL CLEANLY - N^>0 not yet implemented) >>"%f1proc_err%"
echo ==================================================== >>"%f1proc_err%"
echo "%vspipe%" %rrr% --start 1 --end 1 --arg mode="%mode_processing%" --arg sourcefile="%sourcefile%" --container y4m "%vpy%" "%f1proc_y4m%" >>"%f1proc_err%"
@echo on
"%vspipe%" %rrr% --start 1 --end 1 --arg mode="%mode_processing%" --arg sourcefile="%sourcefile%" --container y4m "%vpy%" "%f1proc_y4m%" 2>>"%f1proc_err%"
@echo off
set "rc_f1proc=%ERRORLEVEL%"
echo (RUN3 frame1 processing exit=%rc_f1proc%) >>"%f1proc_err%"
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
REM CHECK B: RUN1 stderr has [KDT] FRAME0-FRESH-START and NO SCAFFOLD_NOT_FILTERED
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK B - frame0 processing KDT markers 1>&2
echo ==================================================== 1>&2
findstr /C:"FRAME0-FRESH-START" "%f0proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT B1: MISSING FRAME0-FRESH-START  -- real frame-0 trace not found 1>&2
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
REM CHECK D: RUN3 (frame 1) must FAIL CLEANLY.
REM   D1 - vspipe exit was NONZERO (INVERTED: nonzero = PASS here)
REM   D2 - stderr contains [KDT] NOT-YET-IMPLEMENTED
REM   D3 - no valid frame produced (output .y4m absent or zero bytes)
echo. 1>&2
echo ==================================================== 1>&2
echo CHECK D - frame1 N^>0 clean-failure (INVERTED: nonzero exit is PASS) 1>&2
echo ==================================================== 1>&2
echo RUN3 frame1 processing vspipe exit code = %rc_f1proc% 1>&2
if "%rc_f1proc%"=="0" (
    echo RESULT D1: FAIL  -- frame 1 returned exit 0; N^>0 was NOT cleanly refused 1>&2
) else (
    echo RESULT D1: PASS  -- frame 1 exited nonzero, consistent with clean refusal 1>&2
)
findstr /C:"NOT-YET-IMPLEMENTED" "%f1proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT D2: FAIL  -- NOT-YET-IMPLEMENTED KDT line not found for frame 1 1>&2
) else (
    echo RESULT D2: PASS  -- NOT-YET-IMPLEMENTED KDT line present for frame 1 1>&2
)
REM also confirm frame 1 did not silently fall through to a SCAFFOLD passthrough
findstr /C:"SCAFFOLD_NOT_FILTERED" "%f1proc_err%" 1>&2
if errorlevel 1 (
    echo RESULT D2b: clean  -- no SCAFFOLD_NOT_FILTERED for frame 1 1>&2
) else (
    echo RESULT D2b: UNEXPECTED SCAFFOLD_NOT_FILTERED  -- frame 1 fell through to passthrough 1>&2
)
if not exist "%f1proc_y4m%" (
    echo RESULT D3: PASS  -- no output file produced for frame 1 1>&2
) else (
    for %%F in ("%f1proc_y4m%") do set "f1size=%%~zF"
    call :report_f1size
)

echo. 1>&2
echo ==================================================== 1>&2
echo SUMMARY: A byte-identical / B1 FRAME0-FRESH-START / B2 no-scaffold / C bypass-clean / 1>&2
echo          D1 nonzero-exit / D2 NOT-YET-IMPLEMENTED / D3 no-frame  =  all PASS for K.1D green 1>&2
echo ==================================================== 1>&2

pause
goto :eof

:report_f1size
if "%f1size%"=="0" (
    echo RESULT D3: PASS  -- frame 1 output file exists but is zero bytes 1>&2
) else (
    echo RESULT D3: CHECK  -- frame 1 output file is %f1size% bytes; inspect whether a valid y4m frame was written 1>&2
)
goto :eof
