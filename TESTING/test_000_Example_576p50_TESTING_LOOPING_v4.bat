@echo off
setlocal enabledelayedexpansion

set "top_root=D:\TEST"
set "vs_root=%top_root%\Vapoursynth_x64_R76"

cd /D "%vs_root%"

REM set "build_type=Debug"
set "build_type=Release"

REM copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.dll "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
REM copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.pdb "%vs_root%\Lib\site-packages\vapoursynth\plugins\"

REM for %%f in (fmUnordered fmParallelRequests fmParallel) do (
for %%f in (fmParallelRequests) do (
    REM echo *** Current fmMODE is %%f
    set "dll_source=%top_root%\DLL_%%f\cnr3.*"
    echo.
    echo copy /y "!dll_source!" "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
    copy /y "!dll_source!" "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
    echo.
    for %%t in (0) do (
        echo.
        echo.****************************************************************
        echo *** Current fmMODE is %%f Current vs Threads number is %%t
        echo.****************************************************************
        echo.
        if /i "%%t"=="0%" (
            set "ttt="
        ) else (
            set "ttt=--arg cnr3_core_threads=%%t"
        )

        REM set "vpy=%vs_root%\test_000_Example_576p50_TESTING_T%%t_%%f.vpy"
        REM echo copy /y "%vs_root%\test_000_Example_576p50_TESTING_example_00_baseline.vpy" "!vpy!"
        REM copy /y "%vs_root%\test_000_Example_576p50_TESTING_example_00_baseline.vpy" "!vpy!"

        set "vpy=%vs_root%\test_000_Example_576p50_TESTING_stackhorizontal_T%%t_%%f.vpy"
        echo copy /y "%vs_root%\test_000_Example_576p50_TESTING_example_00_baseline_stackhorizontal.vpy" "!vpy!"
        copy /y "%vs_root%\test_000_Example_576p50_TESTING_example_00_baseline_stackhorizontal.vpy" "!vpy!"

        REM
        set "log=%top_root%\run_log_T%%t_%%f.txt"
        set "findstr_log=%top_root%\run_log_T%%t_%%f_FINDSTR.txt"
        set "findstr_cmd=findstr /C:"edit_version=" /C:"vspipe" /C:"[vpy]" /C:"fps" /C:"PLANRETRY" /C:"INFO CONFIG" /C:"filter_mode=" /C:"active_ceiling" /C:"frames_recently_evicted_then_re_requested" /C:"frames_re_requested_repeatedly" /C:"DSUM-SUMMARY" /C:"[DSUM-HEALTH]" /C:"Output" /C:"frames_computed" /C:"bailed_after_compute_because" /C:"duplicates_seen" /C:"stores_total" /C:"frames_evicted" /C:"recovery_plans_created" /C:"holes_identified" /C:"recovery_span_mean" /C:"out_of_order_count" /C:"MISMATCH" "!log!""
        set "mp4=%top_root%\test_000_Example_576p50_RESULT_T%%t_%%f.mp4"
        set "y4m=%top_root%\test_000_Example_576p50_RESULT_T%%t_%%f.y4m"
        REM 
        echo.
        REM echo ---------------------------------------------------------------------------------------------------------------------
        REM echo call :vspipe_only "!ttt!" "!vpy!" "!log!" "!findstr_log!" "!mp4!" "!y4m!" "!findstr_cmd!"
        REM call :vspipe_only "!ttt!" "!vpy!" "!log!" "!findstr_log!" "!mp4!" "!y4m!" "!findstr_cmd!"
        REM echo ---------------------------------------------------------------------------------------------------------------------
        REM
        echo.
        echo ---------------------------------------------------------------------------------------------------------------------
        echo call :vspipe_encode "!ttt!" "!vpy!" "!log!" "!findstr_log!" "!mp4!" "!y4m!" "!findstr_cmd!"
        call :vspipe_encode "!ttt!" "!vpy!" "!log!" "!findstr_log!" "!mp4!" "!y4m!" "!findstr_cmd!"
        echo ---------------------------------------------------------------------------------------------------------------------
        echo.
    )
)
echo.
echo Finished collecting data
echo.

dir /s /b "%findstr_log%"
pause
goto :eof

set "ms=50"
echo Analysing %ms%ms data ...
echo.
@echo on
del table_Tx_ms%ms%.csv >NUL 2>&1
del table_Tx_ms%ms%.md >NUL 2>&1
dir /b "%top_root%\run_log_T0_*.txt"


REM copy /y "%top_root%\run_log_T0_fmUnordered.txt" "%top_root%\run_log_T0_fmUnordered.txt"
copy /y "%top_root%\run_log_T0_fmParallelRequests.txt" "%top_root%\run_log_T1_fmParallelRequests.txt"
copy /y "%top_root%\run_log_T0_fmParallel.txt" "%top_root%\run_log_T2_fmParallel.txt"

python cnr3_parse_planretry_ladder.py -ms %ms% ^
  -file1="%top_root%\run_log_T0_fmUnordered.txt" ^
  -file2="%top_root%\run_log_T1_fmParallelRequests.txt" ^
  -file3="%top_root%\run_log_T2_fmParallel.txt"
dir table_Tx_ms%ms%.*
type table_Tx_ms%ms%.csv
type table_Tx_ms%ms%.md
@echo off
echo.

pause
goto :eof

:vspipe_only
echo entered :vspipe_only
set "vo_ttt=%~1"
set "vo_vpy=%~2"
set "vo_log=%~3"
set "vo_findstr_log=%~4"
set "vo_mp4=%~5"
set "vo_y4m=%~6"
REM a HACK to pass strings with embedded quotes
REM set "vo_findstr_cmd=%~7"
set "vo_findstr_cmd=!findstr_cmd!"
REM
del "%vo_log%" >NUL 2>&1
del "%vo_findstr_log%" >NUL 2>&1
del  "%vo_y4m%" >NUL 2>&1
REM del  "%vo_mp4%" >NUL 2>&1
echo. 2>>"%vo_log%" 1>&2
echo findstr /C:"####" "%vo_vpy%" 2>>"%vo_findstr_log%" 1>&2
findstr /C:"####" "%vo_vpy%" 2>>"%vo_findstr_log%" 1>&2
echo findstr /C:"####" "%vo_vpy%" 2>>"%vo_log%" 1>&2
findstr /C:"####" "%vo_vpy%" 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
REM echo. 2>>"%vo_log%" 1>&2
REM echo ========================================== 2>>"%vo_log%" 1>&2
REM echo vspipe info 2>>"%vo_log%" 1>&2
REM echo ========================================== 2>>"%vo_log%" 1>&2
REM echo  "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vo_vpy%" 2>>"%vo_log%" 1>&2
REM "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vo_vpy%" 2>>"%vo_log%" 1>&2
REM echo. 2>>"%vo_log%" 1>&2
echo ========================================== 2>>"%vo_log%" 1>&2
echo [vpy] **************************************************************** 2>>"%vo_log%" 1>&2
echo [vpy] *** vspipe_only output to Y4M 2>>"%vo_log%" 1>&2
echo [vpy] *** vpy="%vo_vpy%" 2>>"%vo_log%" 1>&2
echo [vpy] *** Threads set: %vo_ttt%  2>>"%vo_log%" 1>&2
echo [vpy] **************************************************************** 2>>"%vo_log%" 1>&2
echo ========================================== 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %vo_ttt% --container y4m "%vo_vpy%" "%vo_y4m%" 2>>"%vo_log%" 1>&2
@echo on
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %vo_ttt% --container y4m "%vo_vpy%" "%vo_y4m%" 2>>"%vo_log%" 1>&2
@echo off
REM echo. 2>>"%vo_log%" 1>&2
REM dir "%vo_log%"
echo. 2>>"%vo_log%" 1>&2
echo Running %vo_findstr_cmd% 2>>"%vo_findstr_log%" 1>&2
echo. 2>>"%vo_findstr_log%" 1>&2
%vo_findstr_cmd% 2>>"%vo_findstr_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
echo.
echo type "%vo_findstr_log%"
type "%vo_findstr_log%"
echo.
echo exiting :vspipe_only
goto :eof

:vspipe_encode
echo entered :vspipe_encode
set "vo_ttt=%~1"
set "vo_vpy=%~2"
set "vo_log=%~3"
set "vo_log_enc=%~dpn3_encode.log"
set "vo_findstr_log=%~4"
set "vo_mp4=%~5"
set "vo_y4m=%~6"
REM a HACK to pass strings with embedded quotes
REM set "vo_findstr_cmd=%~7"
set "vo_findstr_cmd=!findstr_cmd!"

REM
del "%vo_log%" >NUL 2>&1
del "%findstr_log%" >NUL 2>&1
REM del  "%vo_y4m%" >NUL 2>&1
del  "%vo_mp4%" >NUL 2>&1
echo. 2>>"%vo_log%" 1>&2
echo findstr /C:"####" "%vo_vpy%" 2>>"%vo_findstr_log%" 1>&2
findstr /C:"####" "%vo_vpy%" 2>>"%vo_findstr_log%" 1>&2
echo findstr /C:"####" "%vo_vpy%" 2>>"%vo_log%" 1>&2
findstr /C:"####" "%vo_vpy%" 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
REM echo. 2>>"%vo_log%" 1>&2
REM echo ========================================== 2>>"%vo_log%" 1>&2
REM echo vspipe info 2>>"%vo_log%" 1>&2
REM echo ========================================== 2>>"%vo_log%" 1>&2
REM echo  "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vo_vpy%" 2>>"%vo_log%" 1>&2
REM "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vo_vpy%" 2>>"%vo_log%" 1>&2
REM echo. 2>>"%vo_log%" 1>&2
echo ========================================== 2>>"%vo_log%" 1>&2
echo [vpy] **************************************************************** 2>>"%vo_log%" 1>&2
echo [vpy] "vspipe_encode Encoding 576p50" 2>>"%vo_log%" 1>&2
echo [vpy] *** vpy="%vo_vpy%" 2>>"%vo_log%" 1>&2
echo [vpy] *** Threads set: %vo_ttt%  2>>"%vo_log%" 1>&2
echo [vpy] **************************************************************** 2>>"%vo_log%" 1>&2
echo ========================================== 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
ECHO ====================================================================================================================================================================================================================================================================================================
REM echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %vo_ttt% --container y4m "%vo_vpy%" - *pipe* "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 16 -preset slow -pix_fmt yuv420p -movflags +faststart+write_colr -an -y "%vo_mp4%" 2>>"%vo_log%" 1>&2
echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %vo_ttt% --container y4m "%vo_vpy%" - *pipe* "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -stats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 16 -preset slow -movflags +faststart+write_colr -an -y "%vo_mp4%" 2>>"%vo_log%" 1>&2
@echo on
pause
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %vo_ttt% --container y4m "%vo_vpy%" - 2>>"%vo_log%" | "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -stats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 16 -preset slow -movflags +faststart+write_colr -an -y "%vo_mp4%" 2>>"%vo_log_enc%" 1>&2
pause
ECHO ====================================================================================================================================================================================================================================================================================================
@echo off
echo. 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
echo %vo_findstr_cmd% 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
%vo_findstr_cmd% 2>>"%vo_findstr_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
echo. 2>>"%vo_log%" 1>&2
echo type "%vo_findstr_log%"
type "%vo_findstr_log%"
echo "%vo_findstr_log%"
echo exiting :vspipe_encode
goto :eof
