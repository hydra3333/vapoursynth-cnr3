@echo off
REM ============================================================================
REM  test_000_Example_576p50_diagS8_fc.bat
REM  R-PROCESS-19 byte-identical proof for the derived-health-ratios patch.
REM  Runs S8 through the ON-build DLL and the OFF-build DLL, capturing the
REM  y4m FRAME OUTPUT (stdout) of each to a file, then fc /b compares them.
REM  The frame bytes MUST be identical: the health block is teardown-only and
REM  must not affect the pixel pipeline.  (The stderr logs DIFFER by design and
REM  are NOT what we compare.)
REM ============================================================================

set "rrr=-r 1"

set "top_root=D:\TEST"
set "vs_root=%top_root%\Vapoursynth_x64_R76"
set "plugin_dir=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"

REM Pre-built DLL source folders (ON = all diag families on; OFF = health-source families off)
set "dll_on=%top_root%\diagS8ON_Release"
set "dll_off=%top_root%\diagS8OFF_Release"

REM The vpy must have the diagS8 scenario uncommented
set "test=diagS8"
set "vpy=%vs_root%\test_000_Example_576p50_%test%.vpy"

set "y4m_on=%top_root%\frames_%test%_ON.y4m"
set "y4m_off=%top_root%\frames_%test%_OFF.y4m"
set "log_on=%top_root%\run_log_%test%_ON.txt"
set "log_off=%top_root%\run_log_%test%_OFF.txt"

cd /D "%vs_root%"

echo ============================================================
echo  Pass 1 of 2 : ON build  (all diag families enabled)
echo ============================================================
copy /y "%dll_on%\cnr3.dll" "%plugin_dir%\" >NUL
if exist "%dll_on%\cnr3.pdb" copy /y "%dll_on%\cnr3.pdb" "%plugin_dir%\" >NUL
del "%y4m_on%"  >NUL 2>&1
del "%log_on%"  >NUL 2>&1
echo Running ON build -> "%y4m_on%"
"%vspipe%" %rrr% --container y4m "%vpy%" "%y4m_on%" 2>>"%log_on%"
echo ON exit_code=%ERRORLEVEL%

echo.
echo ============================================================
echo  Pass 2 of 2 : OFF build (health-source families disabled)
echo ============================================================
copy /y "%dll_off%\cnr3.dll" "%plugin_dir%\" >NUL
if exist "%dll_off%\cnr3.pdb" copy /y "%dll_off%\cnr3.pdb" "%plugin_dir%\" >NUL
del "%y4m_off%" >NUL 2>&1
del "%log_off%" >NUL 2>&1
echo Running OFF build -> "%y4m_off%"
"%vspipe%" %rrr% --container y4m "%vpy%" "%y4m_off%" 2>>"%log_off%"
echo OFF exit_code=%ERRORLEVEL%

echo.
echo ============================================================
echo  Byte-identical compare of FRAME OUTPUT (y4m)
echo ============================================================
dir "%y4m_on%" "%y4m_off%" | findstr /I "frames_"
echo.
echo fc /b "%y4m_on%" "%y4m_off%"
fc /b "%y4m_on%" "%y4m_off%"
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo  RESULT: PASS  -- frame output BYTE-IDENTICAL ON vs OFF
    echo ============================================================
) else (
    echo.
    echo ============================================================
    echo  RESULT: FAIL  -- frame output DIFFERS ^(investigate^)
    echo ============================================================
)

echo.
echo --- sanity: [DSUM-HEALTH] present in ON log, ABSENT in OFF log ---
echo ON  log:
findstr /C:"[DSUM-HEALTH]" "%log_on%"  | findstr /C:"derived health ratios"
echo OFF log ^(expect no output below this line^):
findstr /C:"[DSUM-HEALTH]" "%log_off%"

echo.
echo Finished %test% fc compare
echo.
pause
goto :eof
