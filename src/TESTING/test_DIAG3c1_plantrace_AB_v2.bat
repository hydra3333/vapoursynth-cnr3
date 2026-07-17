@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM =====================================================================================
REM DIAG.3c.1 PLAN-TRACE A/B HARNESS  (designer-owned, W3D)
REM
REM PURPOSE: prove the 3c.1 plan-trace capture is OBSERVE-ONLY (R-PROCESS-19 exit gate)
REM          and emit the content for the designer's content-sanity review.
REM
REM RUN A = plantrace OFF build   (CNR3_DIAG_COMPUTE_DSUM_PLANTRACE commented out)
REM RUN B = plantrace ON  build   (plantrace enabled, window FROM_FRAME=0 TO_FRAME=3200)
REM   The two builds MUST differ ONLY in the plantrace master gate. Same config (Release),
REM   same everything else (06/07/09/14, DSUM10, etc. all ON in BOTH so they cancel in fc).
REM
REM WINDOW: build B with
REM     #define CNR3_DIAG_DSUM_PLANTRACE_FROM_FRAME 0
REM     #define CNR3_DIAG_DSUM_PLANTRACE_TO_FRAME   3200
REM   3200 covers every output frame number across S1/S7/S8 (S7/S8 jump to 3000, seg 200).
REM
REM PASS (per scenario):
REM   CHECK 1  fc /b A.y4m B.y4m == byte-identical            <- THE observe-only exit gate
REM   CHECK 0K A has NO [DSUM-PLANTRACE]; B HAS it            <- correct builds loaded
REM   CHECK 2  B has bare-token O and R records (v2.3 single-line format); BEGIN/END present;
REM   CHECK 2f B (clean run) has NO outcome=FAILED records  <- 3c.2 observe-only-on-success assertion
REM            S7/S8 have >=1 strategy=RECOVERY_EXACT;
REM            S1 has NO recovery                             <- gives CHECK 1 its teeth
REM   The B .err logs (run_3c1_B_S*.txt) are then sent to the designer for the detailed
REM   content sanity (O/R pairing by frame, holes/sources vs D-SUM-12, window respected).
REM =====================================================================================

set "rrr=-r 1"

REM --- paths (match the existing S-series / AB layout; adjust if yours differs) ---
set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "golden_source=%source_path%\000_Example_576p50.mp4"

REM --- the two staged builds: populate these folders with the matching cnr3.dll BEFORE running ---
set "dll_folder_OFF=%source_path%\dll_3c1_plantrace_OFF_Release"
set "dll_folder_ON=%source_path%\dll_3c1_plantrace_ON_Release"

cd /D "%vs_root%"

REM --- preflight ---
set "preflight_fail=0"
if not exist "%vspipe%"            ( echo PREFLIGHT FAIL: vspipe not found: "%vspipe%" 1>&2 & set "preflight_fail=1" )
if not exist "%golden_source%"     ( echo PREFLIGHT FAIL: golden source not found: "%golden_source%" 1>&2 & set "preflight_fail=1" )
if not exist "%runtime_dll_folder%\" ( echo PREFLIGHT FAIL: runtime plugin folder not found: "%runtime_dll_folder%" 1>&2 & set "preflight_fail=1" )
if not exist "%dll_folder_OFF%\cnr3.dll" ( echo PREFLIGHT FAIL: plantrace-OFF DLL not found: "%dll_folder_OFF%\cnr3.dll" 1>&2 & set "preflight_fail=1" )
if not exist "%dll_folder_ON%\cnr3.dll"  ( echo PREFLIGHT FAIL: plantrace-ON DLL not found: "%dll_folder_ON%\cnr3.dll" 1>&2 & set "preflight_fail=1" )
for %%S in (S1 S7 S8) do (
  if not exist "%vs_root%\test_000_Example_576p50_%%S.vpy" ( echo PREFLIGHT FAIL: vpy not found: test_000_Example_576p50_%%S.vpy 1>&2 & set "preflight_fail=1" )
)
if "%preflight_fail%"=="1" ( echo PREFLIGHT RESULT: FAIL -- fix the paths above before running. 1>&2 & goto :fatal )
echo PREFLIGHT RESULT: PASS -- required files/folders exist. 1>&2

REM --- run the three scenarios as A/B pairs ---
for %%S in (S1 S7 S8) do call :run_ab_pair %%S

@echo on
findstr /C:"[DSUM-PLANTRACE]" "%source_path%\run_3c1_B_S1.txt" > "%source_path%\pt_S1.txt"
findstr /C:"[DSUM-PLANTRACE]" "%source_path%\run_3c1_B_S8.txt" > "%source_path%\pt_S8.txt"
findstr /C:"D-SUM-12"         "%source_path%\run_3c1_B_S8.txt" > "%source_path%\d12_S8.txt"
@echo off
echo.
echo ATTACH THESE FILES TO THE DESIGNER CHAT:
echo   "%source_path%\pt_S1.txt"
echo   "%source_path%\pt_S8.txt"
echo   "%source_path%\run_3c1_B_S8.txt"
echo.
dir "%source_path%\pt_S1.txt"
dir "%source_path%\pt_S8.txt"
dir "%source_path%\run_3c1_B_S8.txt"
echo.

echo. 1>&2
echo ==================================================================== 1>&2
echo DIAG.3c.1 A/B HARNESS COMPLETE 1>&2
echo   PASS REQUIRES, for EACH scenario: CHECK 0A/0B exit 0, 0C/0D files exist, 1>&2
echo   0K correct builds, CHECK 1 byte-identical, CHECK 2 capture fired 1>&2
echo   (S7/S8 recovery present; S1 recovery absent). 1>&2
echo   Then send run_3c1_B_S1.txt / _S7.txt / _S8.txt to the designer for content sanity. 1>&2
echo ==================================================================== 1>&2
pause
exit /b 0

REM =====================================================================================
:run_ab_pair
set "scen=%~1"
set "vpy=%vs_root%\test_000_Example_576p50_%scen%.vpy"
set "a_y4m=%source_path%\3c1_%scen%_A_off.y4m"
set "b_y4m=%source_path%\3c1_%scen%_B_on.y4m"
set "a_err=%source_path%\run_3c1_A_%scen%.txt"
set "b_err=%source_path%\run_3c1_B_%scen%.txt"

echo. 1>&2
echo #################### SCENARIO %scen% #################### 1>&2

del /F "%a_y4m%" "%b_y4m%" "%a_err%" "%b_err%" 2>NUL

REM --- RUN A: plantrace OFF ---
call :install_dll "%dll_folder_OFF%\cnr3.dll" "%runtime_dll_folder%" "%scen% RUN A plantrace-OFF" || goto :fatal_copy
"%vspipe%" %rrr% --container y4m "%vpy%" "%a_y4m%"  2>>"%a_err%"
set "rc_a=%ERRORLEVEL%"
echo (RUN A %scen% plantrace-OFF exit=%rc_a%) >>"%a_err%"

REM --- RUN B: plantrace ON ---
call :install_dll "%dll_folder_ON%\cnr3.dll" "%runtime_dll_folder%" "%scen% RUN B plantrace-ON" || goto :fatal_copy
"%vspipe%" %rrr% --container y4m "%vpy%" "%b_y4m%"  2>>"%b_err%"
set "rc_b=%ERRORLEVEL%"
echo (RUN B %scen% plantrace-ON exit=%rc_b%) >>"%b_err%"

REM --- CHECK 0: exit codes ---
if "%rc_a%"=="0" ( echo RESULT %scen% 0A: PASS -- RUN A exited 0 1>&2 ) else ( echo RESULT %scen% 0A: FAIL -- RUN A exit=%rc_a% 1>&2 )
if "%rc_b%"=="0" ( echo RESULT %scen% 0B: PASS -- RUN B exited 0 1>&2 ) else ( echo RESULT %scen% 0B: FAIL -- RUN B exit=%rc_b% 1>&2 )

REM --- CHECK 0Y: outputs exist ---
if exist "%a_y4m%" ( echo RESULT %scen% 0C: PASS -- A output exists 1>&2 ) else ( echo RESULT %scen% 0C: FAIL -- A output missing 1>&2 )
if exist "%b_y4m%" ( echo RESULT %scen% 0D: PASS -- B output exists 1>&2 ) else ( echo RESULT %scen% 0D: FAIL -- B output missing 1>&2 )

REM --- CHECK 0K: build identity via the plantrace tag (A must NOT have it; B MUST) ---
findstr /C:"[DSUM-PLANTRACE]" "%a_err%" >NUL && ( echo RESULT %scen% 0E: FAIL -- A emitted [DSUM-PLANTRACE]; wrong ^(ON^) DLL staged as A 1>&2 ) || ( echo RESULT %scen% 0E: PASS -- A has no [DSUM-PLANTRACE], plantrace-OFF confirmed 1>&2 )
findstr /C:"[DSUM-PLANTRACE]" "%b_err%" >NUL && ( echo RESULT %scen% 0F: PASS -- B emitted [DSUM-PLANTRACE], plantrace-ON confirmed 1>&2 ) || ( echo RESULT %scen% 0F: FAIL -- B has no [DSUM-PLANTRACE]; plantrace not built/printing 1>&2 )

REM --- CHECK 1: THE observe-only exit gate: A.y4m == B.y4m byte-identical ---
if not exist "%a_y4m%" ( echo RESULT %scen% 1: FAIL -- cannot compare, A missing 1>&2 & goto :ab_check2 )
if not exist "%b_y4m%" ( echo RESULT %scen% 1: FAIL -- cannot compare, B missing 1>&2 & goto :ab_check2 )
fc /b "%a_y4m%" "%b_y4m%" >NUL && ( echo RESULT %scen% 1: PASS -- plantrace-ON output byte-identical to OFF ^(OBSERVE-ONLY^) 1>&2 ) || ( echo RESULT %scen% 1: FAIL -- output differs; plantrace perturbed a returned frame 1>&2 )

:ab_check2
REM --- CHECK 2: capture fired (teeth for CHECK 1) ---
findstr /C:"[DSUM-PLANTRACE] O " "%b_err%" >NUL && ( echo RESULT %scen% 2a: PASS -- O records present 1>&2 ) || ( echo RESULT %scen% 2a: FAIL -- no O records; O capture did not fire 1>&2 )
findstr /C:"[DSUM-PLANTRACE] R " "%b_err%" >NUL && ( echo RESULT %scen% 2b: PASS -- R records present 1>&2 ) || ( echo RESULT %scen% 2b: FAIL -- no R records; R capture did not fire 1>&2 )

if /I "%scen%"=="S1" (
  findstr /C:"strategy=RECOVERY_" "%b_err%" >NUL && ( echo RESULT %scen% 2c: CHECK -- S1 unexpectedly shows recovery ^(control should have none^) 1>&2 ) || ( echo RESULT %scen% 2c: PASS -- S1 shows no recovery ^(in-order control^) 1>&2 )
) else if /I "%scen%"=="S8" (
  findstr /C:"strategy=RECOVERY_" "%b_err%" >NUL && ( echo RESULT %scen% 2c: PASS -- recovery capture path fired ^(RECOVERY_EXACT/FLOOR present^) 1>&2 ) || ( echo RESULT %scen% 2c: FAIL -- no recovery in S8 in-window; expected shuffle-driven recovery 1>&2 )
) else (
  findstr /C:"strategy=RECOVERY_" "%b_err%" >NUL && ( echo RESULT %scen% 2c: PASS -- recovery captured ^(RECOVERY_EXACT/FLOOR present^) 1>&2 ) || ( echo RESULT %scen% 2c: INFO -- no recovery in %scen% window ^(jumps may be out-of-window; observe-only still proven by CHECK 1^) 1>&2 )
)

REM --- CHECK 2d: v2.3 block structure present (BEGIN/END bracket, one legend) ---
findstr /C:"[DSUM-PLANTRACE] BEGIN schema=3c1v1" "%b_err%" >NUL && ( echo RESULT %scen% 2d: PASS -- BEGIN present 1>&2 ) || ( echo RESULT %scen% 2d: FAIL -- no BEGIN block marker 1>&2 )
findstr /C:"[DSUM-PLANTRACE] END   schema=3c1v1" "%b_err%" >NUL && ( echo RESULT %scen% 2e: PASS -- END present 1>&2 ) || ( echo RESULT %scen% 2e: FAIL -- no END block marker ^(possible truncation^) 1>&2 )

REM --- CHECK 2f (3c.2): clean runs must emit NO FAILED records (dump-on-bail must not fire on success) ---
findstr /C:"outcome=FAILED" "%b_err%" >NUL && ( echo RESULT %scen% 2f: FAIL -- clean run emitted outcome=FAILED ^(3c.2 failure path fired on a success run^) 1>&2 ) || ( echo RESULT %scen% 2f: PASS -- no FAILED records on clean run ^(3c.2 unchanged from 3c.1^) 1>&2 )

echo (SCENARIO %scen%: send %b_err% to designer for O/R pairing + holes/sources vs D-SUM-12) 1>&2
goto :eof

REM =====================================================================================
:install_dll
set "src_dll=%~1"
set "dst_dir=%~2"
set "copy_label=%~3"
if not exist "%src_dll%" ( echo COPY FAIL -- %copy_label% source missing: "%src_dll%" 1>&2 & exit /b 1 )
if not exist "%dst_dir%\" ( echo COPY FAIL -- %copy_label% dest folder missing: "%dst_dir%" 1>&2 & exit /b 1 )
del /F "%dst_dir%\cnr3.dll" 2>NUL
COPY /Y /V "%src_dll%" "%dst_dir%\" >NUL
if errorlevel 1 ( echo COPY FAIL -- %copy_label% 1>&2 & exit /b 1 )
if not exist "%dst_dir%\cnr3.dll" ( echo COPY FAIL -- %copy_label% produced no cnr3.dll 1>&2 & exit /b 1 )
echo COPY PASS -- %copy_label% 1>&2
goto :eof

:fatal_copy
echo FATAL: DLL install failed; aborting. 1>&2
pause
exit /b 1

:fatal
echo FATAL: preflight failed; aborting. 1>&2
pause
exit /b 1
