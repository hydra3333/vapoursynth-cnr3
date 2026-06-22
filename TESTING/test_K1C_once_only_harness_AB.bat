@echo off
REM test_K1C_once_only_harness_AB.bat 

REM set "rrr="
set "rrr=-r 1"
REM echo.

REM ---------------------------------------------------------------
REM SCENARIO SELECTION — set one. (definitions live in the .vpy SCENARIOS table)
REM   functional:  S1 S2 S3 S4 S5 S6 S7 S8
REM   boundary:    B1 B2a B2b B3 B4 B5
REM   end-of-clip: B6 B7a B7b
REM   S1  [0] len10                    in-order baseline (DEFAULT, the K.1C scaffold scenario)
REM   S2  [0] len120                   in-order, lays checkpoints
REM   S3  [0] len120 shuffle zone5     in-zone shuffle
REM   S4  [0,45] len30                 small in-bound jump (exact recovery)
REM   S5  [0,2000] len60               hot-zone move then far jump (floor reset)
REM   S6  [5000] len60                 deep cold seek (length independence)
REM   S7  [0,3000,1000,2000] len200    repeated realistic jumping
REM   S8  [0,3000,1000,2000] len200 shuffle zone8   as S7 + shuffle
REM   B1  [40] len15                   frame-0 clamp (N<B)
REM   B2a [0,58] len10                 bound straddle inside (gap 49)
REM   B2b [0,61] len10                 bound straddle outside (gap 52)
REM   B3  [0,50] len101                backward re-request (cache-hit)
REM   B4  [0] len100 shuffle zone20    heavy out-of-order
REM   B5  [0,500,100,800,200,1200,300] len30   hot-zone cap / churn
REM   B6  [EOF-30] len60               in-sequence into EOF
REM   B7a [EOF-1] len5                 cold jump to last frame exactly
REM   B7b [EOF-5] len10                cold jump, few frames at edge
REM ---------------------------------------------------------------
REM case sensitive
set "scenario=S1"

set "mode_passthrough=passthrough"
set "mode_processing=processing"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_K1C_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

REM ---------------------------------------------------------------
REM UN-COMMENT ONLY ONE SOURCE
REM interlaced
REM set "sourcefile=%source_path%\000_Example_576i.mp4"
REM progressive 25fps
REM set "sourcefile=%source_path%\000_Example_576p25.mp4"
REM progressive 50fps
set "sourcefile=%source_path%\000_Example_576p50.mp4"
REM ---------------------------------------------------------------

set "targetfile_passthrough_base=%sourcefile%_%mode_passthrough%"
set "targetfile_passthrough=%targetfile_passthrough_base%_temp.y4m"
set "targetstderr_passthrough=%targetfile_passthrough_base%_temp_stderr.txt"

set "targetfile_processing_base=%sourcefile%_%mode_processing%"
set "targetfile_processing=%targetfile_processing_base%_temp.y4m"
set "targetstderr_processing=%targetfile_processing_base%_temp_stderr.txt"

CD /D "%vs_root%"
COPY /Y "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"

REM ===================================================================================================================================================================
echo. >"%targetstderr_passthrough%"
echo ==================================================== >>"%targetstderr_passthrough%"
echo vspipe_only (Testing Pipe Output) PART A %mode_passthrough% scenario="%scenario%" >>"%targetstderr_passthrough%"
echo ==================================================== >>"%targetstderr_passthrough%"
echo. >>"%targetstderr_passthrough%"
echo "%vspipe%" %rrr% --arg mode="%mode_passthrough%" --arg sourcefile="%sourcefile%" --arg scenario="%scenario%" --container y4m "%vpy%" "%targetfile_passthrough%" >>"%targetstderr_passthrough%"
@echo on
"%vspipe%" %rrr% --arg mode="%mode_passthrough%" --arg sourcefile="%sourcefile%" --arg scenario="%scenario%" --container y4m "%vpy%" "%targetfile_passthrough%" 2>>"%targetstderr_passthrough%"
@echo off
echo. >>"%targetstderr_passthrough%"
REM ===================================================================================================================================================================
echo. >"%targetstderr_processing%"
echo ==================================================== >>"%targetstderr_processing%"
echo vspipe_only (Testing Pipe Output) PART B %mode_processing% scenario="%scenario%" >>"%targetstderr_processing%"
echo ==================================================== >>"%targetstderr_processing%"
echo. >>"%targetstderr_processing%"
echo "%vspipe%" %rrr% --arg mode="%mode_processing%" --arg sourcefile="%sourcefile%" --arg scenario="%scenario%" --container y4m "%vpy%" "%targetfile_processing%" >>"%targetstderr_processing%"
@echo on
"%vspipe%" %rrr% --arg mode="%mode_processing%" --arg sourcefile="%sourcefile%" --arg scenario="%scenario%" --container y4m "%vpy%" "%targetfile_processing%" 2>>"%targetstderr_processing%"
@echo off
echo. >>"%targetstderr_processing%"
REM ===================================================================================================================================================================

echo. 1>&2
echo ==================================================== 1>&2
echo BYTE COMPARE: passthrough vs processing scenario="%scenario%" 1>&2
echo ==================================================== 1>&2
fc /b "%targetfile_passthrough%" "%targetfile_processing%" 1>&2
if errorlevel 1 (
    echo RESULT: DIFFER  -- scaffold is NOT a clean passthrough, or processing changed pixels 1>&2
) else (
    echo RESULT: BYTE-IDENTICAL  -- passthrough scaffold confirmed 1>&2
)

echo. 1>&2
echo ==================================================== 1>&2
echo KDT MARKER CHECK (processing run stderr) scenario="%scenario%" 1>&2
echo ==================================================== 1>&2
findstr /C:"[KDT]" "%targetstderr_processing%" 1>&2
if errorlevel 1 (
    echo RESULT: scenario="%scenario%" NO [KDT] LINES FOUND  -- scaffold trace missing 1>&2
) else (
    echo RESULT: scenario="%scenario%" [KDT] lines present 1>&2
)
findstr /C:"[KDT-SUMMARY]" "%targetstderr_processing%" 1>&2

echo. 1>&2
echo ==================================================== 1>&2
echo NEGATIVE CHECK: passthrough run stderr should have NO CNR3/KDT output scenario="%scenario%" 1>&2
echo ==================================================== 1>&2
findstr /C:"[KDT]" "%targetstderr_passthrough%" 1>&2
if errorlevel 1 (
    echo RESULT: scenario="%scenario%" clean  -- no [KDT] in passthrough, as expected 1>&2
) else (
    echo RESULT: scenario="%scenario%" UNEXPECTED [KDT] in passthrough run 1>&2
)

pause 
goto :eof
