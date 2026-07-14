@echo off

set "rrr="
REM set "rrr=-r 1"

set "top_root=D:\TEST"
set "vs_root=%top_root%\Vapoursynth_x64_R76"
set "plugin_dir=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"

REM Pre-built DLL source folders
set "dll_before=%top_root%\DLL_fmParallel_CNR3_EXPERIMENT_PLAN_RETRY_BIAS"
set "dll_after=%top_root%\DLL_fc3"

REM The vpy must have the diagS8 scenario uncommented
set "test=diagS8"
set "vpy=%vs_root%\test_000_Example_576p50_%test%.vpy"

set "y4m_BEFORE=%top_root%\frames_%test%_BEFORE.y4m"
set "y4m_AFTER=%top_root%\frames_%test%_AFTER.y4m"
set "log_BEFORE=%top_root%\run_log_%test%_BEFORE.txt"
set "log_AFTER=%top_root%\run_log_%test%_AFTER.txt"

cd /D "%vs_root%"

echo ============================================================
echo  Pass 1 of 2 : BEFORE ... 
echo ============================================================
echo copy /y "%dll_before%\cnr3.dll" "%plugin_dir%\" >NUL
copy /y "%dll_before%\cnr3.dll" "%plugin_dir%\" >NUL
if exist "%dll_before%\cnr3.pdb" copy /y "%dll_before%\cnr3.pdb" "%plugin_dir%\" >NUL
del "%y4m_BEFORE%"  >NUL 2>&1
del "%log_BEFORE%"  >NUL 2>&1
echo Running BEFORE build -> "%y4m_BEFORE%"
"%vspipe%" %rrr% --container y4m "%vpy%" "%y4m_BEFORE%" 2>>"%log_BEFORE%"
echo BEFORE exit_code=%ERRORLEVEL%
findstr /C:"edit_version=" /C:"filter_mode=" "%log_BEFORE%"
echo.

echo.
echo ============================================================
echo  Pass 2 of 2 : AFTER ...
echo ============================================================
copy /y "%dll_after%\cnr3.dll" "%plugin_dir%\" >NUL
echo copy /y "%dll_after%\cnr3.dll" "%plugin_dir%\" >NUL
if exist "%dll_after%\cnr3.pdb" copy /y "%dll_after%\cnr3.pdb" "%plugin_dir%\" >NUL
del "%y4m_AFTER%" >NUL 2>&1
del "%log_AFTER%" >NUL 2>&1
echo Running AFTER build -> "%y4m_AFTER%"
"%vspipe%" %rrr% --container y4m "%vpy%" "%y4m_AFTER%" 2>>"%log_AFTER%"
echo AFTER exit_code=%ERRORLEVEL%
findstr /C:"edit_version=" /C:"filter_mode=" "%log_AFTER%"
echo.

echo.
echo ============================================================
echo  Byte-identical compare of FRAME OUTPUT (y4m)
echo ============================================================
dir "%y4m_BEFORE%" "%y4m_AFTER%" | findstr /I "frames_"
echo.
echo fc /b "%y4m_BEFORE%" "%y4m_AFTER%"
fc /b "%y4m_BEFORE%" "%y4m_AFTER%"
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo  RESULT: PASS  -- frame output BYTE-IDENTICAL BEFORE vs AFTER
    echo ============================================================
) else (
    echo.
    echo ============================================================
    echo  RESULT: FAIL  -- frame output DIFFERS ^(investigate^)
    echo ============================================================
)

echo.
echo Finished %test% fc compare
echo.
pause
goto :eof
