@echo off
REM set "rrr="
set "rrr=-r 1"
REM echo.

set "top_root=D:\TEST"
set "vs_root=%top_root%\Vapoursynth_x64_R76"

cd /D "%vs_root%"

REM set "build_type=Debug"
set "build_type=Release"

copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.dll "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.pdb "%vs_root%\Lib\site-packages\vapoursynth\plugins\"

REM The TEST id MUST coincide with the test uncommented inside the vpy
set "test=diagS8"
REM set "test="
if /I "%test%" NEQ "" (
    set "vpy_ending=_%test%"
) else (
    set "vpy_ending="
)
set "vpy=%vs_root%\test_000_Example_576p50%vpy_ending%.vpy"

set "log=%top_root%\run_log%vpy_ending%.txt"
set "findstr_log=%top_root%\run_findstr%vpy_ending%.txt"
set "mp4=%top_root%\test_000_Example_576p50_RESULT%vpy_ending%.mp4"

REM set "findstr_cmd=findstr /C:"[DSUM10-" /C:"[DSUM-SUMMARY]" /C:"[DSUM11" "%log%""
REM set "findstr_cmd=findstr /C:"[DSUM-SUMMARY]" /C:"[DSUM10-" /C:"[DSUM11" "%log%""
set "findstr_cmd=findstr /C:"[DSUM-HEALTH]" /C:"edit_version" /C:"D-SUM-12 frames_total" /C:"D-SUM-12 frames_cache_hit" /C:"D-SUM-12 frames_recovered_exact" /C:"D-SUM-12 frames_recovered_floor"  /C:"D-SUM-12 holes_identified" /C:"D-SUM-12 holes_filled"  /C:"D-SUM-09 return_transfer_attempted" /C:"D-SUM-09 return_transfer_succeeded"  /C:"D-SUM-10 frames_evicted" /C:"D-SUM-10 prune_events_triggered"  /C:"D-SUM-10 frames_recently_evicted_then_re_requested"  /C:"D-SUM-13 frames_recalculated_once" /C:"D-SUM-13 frames_recalculated_multiple_times"  "D:\TEST\run_log_diagS8.txt""

call :vspipe_only
REM call :vspipe_encode

echo.
echo Finished %test%
echo.
pause
goto :eof

:vspipe_only
del "%log%" >NUL 2>&1
del "%findstr_log%" >NUL 2>&1
REM echo. 2>>"%log%" 1>&2
REM echo ========================================== 2>>"%log%" 1>&2
REM echo vspipe info 2>>"%log%" 1>&2
REM echo ========================================== 2>>"%log%" 1>&2
REM echo  "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vpy%" 2>>"%log%" 1>&2
REM "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vpy%" 2>>"%log%" 1>&2
REM echo. 2>>"%log%" 1>&2
echo ========================================== 2>>"%log%" 1>&2
echo vspipe_only (Testing Pipe Output to NUL) 2>>"%log%" 1>&2
echo ========================================== 2>>"%log%" 1>&2
echo. 2>>"%log%" 1>&2
echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "%vpy%" NUL 2>>"%log%" 1>&2
@echo on
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "%vpy%" NUL 2>>"%log%" 1>&2
@echo off
echo. 2>>"%log%" 1>&2
dir "%log%"
echo. 2>>"%log%" 1>&2
echo Running %findstr_cmd% 2>>"%findstr_log%" 1>&2
echo Running %findstr_cmd%
echo. 2>>"%findstr_log%" 1>&2
%findstr_cmd% 2>>"%findstr_log%" 1>&2
echo. 2>>"%log%" 1>&2
echo.
echo type "%findstr_log%"
type "%findstr_log%"
echo.
goto :eof

:vspipe_encode
del "%log%" >NUL 2>&1
del "%findstr_log%" >NUL 2>&1
REM echo. 2>>"%log%" 1>&2
REM echo ========================================== 2>>"%log%" 1>&2
REM echo vspipe info 2>>"%log%" 1>&2
REM echo ========================================== 2>>"%log%" 1>&2
REM echo  "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vpy%" 2>>"%log%" 1>&2
REM "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "%vpy%" 2>>"%log%" 1>&2
REM echo. 2>>"%log%" 1>&2
echo ========================================== 2>>"%log%" 1>&2
echo "vspipe_encode Encoding 576p50" 2>>"%log%" 1>&2
echo ========================================== 2>>"%log%" 1>&2
echo. 2>>"%log%" 1>&2
echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "%vpy%" - pipe "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 18 -preset slow -pix_fmt yuv420p -movflags +faststart+write_colr -an -y "%mp4%" 2>>"%log%" 1>&2
@echo on
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "%vpy%" - | "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 18 -preset slow -pix_fmt yuv420p -movflags +faststart+write_colr -an -y "%mp4%" 2>>"%log%" 1>&2
@echo off
echo. 2>>"%log%" 1>&2
echo. 2>>"%log%" 1>&2
echo %findstr_cmd% 2>>"%log%" 1>&2
echo. 2>>"%log%" 1>&2
%findstr_cmd% 2>>"%findstr_log%" 1>&2
echo. 2>>"%log%" 1>&2
echo. 2>>"%log%" 1>&2
echo type "%findstr_log%"
type "%findstr_log%"
echo "%findstr_log%"
goto :eof
