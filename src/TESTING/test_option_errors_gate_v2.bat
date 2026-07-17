@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM ============================================================
REM CNR3 RIDER.option-error-messages -- runtime gate harness
REM Designer-owned (W3D). Runs every remaining gate case:
REM   range errors, type errors, D1/D2/D3 proofs, valid runs.
REM Each case: vspipe evaluates the gate .vpy with CNR3_TEST_ARGS
REM set; frames go to NUL; stderr captured per case.
REM   expect FAIL     -> exit != 0 AND "CNR3: invalid" present (plugin-layer message)
REM   expect FAIL_ANY -> exit != 0 (binding-layer type rejection; VS typedDictToMap
REM                      throws for int/float/bool type mismatches before the plugin runs)
REM   expect PASS -> vspipe exit == 0
REM Byte-identity (gate last item) is NOT here: use the existing
REM test_000_Example_576p50_diagS8_fc3.bat with
REM   dll_before = parser-commit DLL, dll_after = rider DLL.
REM ============================================================

set "top_root=D:\TEST"
set "vs_root=%top_root%\Vapoursynth_x64_R76"
set "plugin_dir=%vs_root%\Lib\site-packages\vapoursynth\plugins"

REM ---- adjust these two to your environment (same as LOOPING harness) ----
set "VSPIPE=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "VPY=%vs_root%\test_option_errors_gate.vpy"
set "LOGDIR=%top_root%\test_option_errors_gate"
REM ------------------------------------------------------------------------

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
set /a total=0
set /a good=0
set "SUMMARY="

echo ============================================================
echo  RANGE / VALUE ERROR CASES (each must FAIL cleanly)
echo ============================================================
set "CASE=r1_y_threshold_256"     & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=y_threshold=256"          & call :run_case
set "CASE=r2_u_strength_neg1"     & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=u_strength=-1"            & call :run_case
set "CASE=r3_scene_thr_101"       & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=scene_threshold=101.0"    & call :run_case
set "CASE=r4_y_curve_wobbly"      & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=y_curve='wobbly'"         & call :run_case
set "CASE=r5_u_curve_o_cnr2"      & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=u_curve='o'"              & call :run_case
set "CASE=r6_scene_chroma_2"      & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=scene_chroma=2"           & call :run_case

echo ============================================================
echo  TYPE ERROR CASES (each must FAIL with "incorrect value type")
echo ============================================================
set "CASE=t1_y_threshold_str"     & set "EXPECT=FAIL_ANY" & set "CNR3_TEST_ARGS=y_threshold='bad'"        & call :run_case
set "CASE=t2_scene_thr_str"       & set "EXPECT=FAIL_ANY" & set "CNR3_TEST_ARGS=scene_threshold='bad'"    & call :run_case
set "CASE=t3_y_curve_int"         & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=y_curve=123"              & call :run_case
set "CASE=t4_scene_chroma_str"    & set "EXPECT=FAIL_ANY" & set "CNR3_TEST_ARGS=scene_chroma='bad'"       & call :run_case

echo ============================================================
echo  D-RULING PROOFS
echo ============================================================
set "CASE=d1a_near_miss_100_0001" & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=scene_threshold=100.0001" & call :run_case
set "CASE=d1b_neg_0_1"            & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=scene_threshold=-0.1"     & call :run_case
set "CASE=d3_control_char"        & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=y_curve='bad\nvalue'"     & call :run_case
set "CASE=d3_long_200"            & set "EXPECT=FAIL" & set "CNR3_TEST_ARGS=y_curve='a'*200"          & call :run_case

echo ============================================================
echo  VALID CASES (each must SUCCEED)
echo ============================================================
set "CASE=v1_y_threshold_0"       & set "EXPECT=PASS" & set "CNR3_TEST_ARGS=y_threshold=0"            & call :run_case
set "CASE=v2_u_threshold_0"       & set "EXPECT=PASS" & set "CNR3_TEST_ARGS=u_threshold=0"            & call :run_case
set "CASE=v3_no_options"          & set "EXPECT=PASS" & set "CNR3_TEST_ARGS="                         & call :run_case

echo.
echo ============================================================
echo  RESULT: !good! / !total! cases behaved as expected
echo ============================================================
echo   (per-case verdicts are listed above)
echo.
echo Now eyeball these specific message contents in %LOGDIR%:
echo   d1a_near_miss_100_0001.log  ^> must show: got 100.0001   (NOT 100, NOT 100.0)
echo   d1b_neg_0_1.log             ^> should show: got -0.1
echo   d3_control_char.log         ^> ONE line, newline rendered as ?
echo   d3_long_200.log             ^> ONE line, truncated with ...
echo   t*_*.log                    ^> "incorrect value type," + SAME expectation text as range twin
echo   v1_y_threshold_0.log        ^> response_config shows y=0/192/wide
echo Byte-identity: run test_000_Example_576p50_diagS8_fc3.bat with
echo   dll_before = parser-commit DLL folder, dll_after = rider DLL folder.
pause
exit /b 0

:run_case
set /a total+=1
set "log=%LOGDIR%\%CASE%.log"
"%VSPIPE%" "%VPY%" NUL 1>nul 2>"%log%"
set "rc=!errorlevel!"
set "verdict=UNEXPECTED"
if /i "%EXPECT%"=="FAIL" (
    if not "!rc!"=="0" (
        findstr /C:"CNR3: invalid" "%log%" >nul 2>&1 && set "verdict=OK"
        if "!verdict!"=="UNEXPECTED" set "verdict=FAILED-BUT-NO-CNR3-MESSAGE"
    ) else (
        set "verdict=ACCEPTED-INVALID-VALUE---STOP"
    )
) else if /i "%EXPECT%"=="FAIL_ANY" (
    REM binding-layer type errors: VapourSynth's Python typedDictToMap rejects
    REM int/float/bool type mismatches BEFORE the plugin runs (verified: t4 ->
    REM rangeToIntFilter ValueError). Clean non-zero exit from EITHER layer = OK.
    if not "!rc!"=="0" (
        set "verdict=OK"
    ) else (
        set "verdict=ACCEPTED-INVALID-VALUE---STOP"
    )
) else (
    if "!rc!"=="0" set "verdict=OK"
)
if "!verdict!"=="OK" (set /a good+=1)
echo   [!verdict!]  %CASE%  (expect %EXPECT%, exit !rc!)
REM show the CNR3 error line for immediate eyeballing
findstr /C:"CNR3: invalid" "%log%" 2>nul
REM for valid runs show the resolved config line
if /i "%EXPECT%"=="PASS" findstr /C:"response_config:" "%log%" 2>nul
echo.
exit /b 0
