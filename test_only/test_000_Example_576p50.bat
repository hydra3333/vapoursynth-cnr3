@echo off
REM set "rrr="
set "rrr=-r 1"
REM echo.

call :vspipe_only
REM call :vspipe_encode
goto :eof

:vspipe_only
echo. 1>&2
echo ========================================== 1>&2
echo vspipe info 1>&2
echo ========================================== 1>&2
echo  "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" 1>&2
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" 1>&2
echo. 1>&2
echo ========================================== 1>&2
echo vspipe_only (Testing Pipe Output to NUL) 1>&2
echo ========================================== 1>&2
echo. 1>&2
echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" NUL 1>&2
@echo on
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" NUL 1>&2
@echo off
echo. 1>&2
goto :eof

:vspipe_encode
echo. 1>&2
echo ========================================== 1>&2
echo vspipe info 1>&2
echo ========================================== 1>&2
echo  "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" 1>&2
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" --info "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" 1>&2
echo. 1>&2
echo ========================================== 1>&2
echo "vspipe_encode Encoding 576p50" 1>&2
echo ========================================== 1>&2
echo. 1>&2
echo "D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" - pipe "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 18 -preset slow -pix_fmt yuv420p -movflags +faststart+write_colr -an -y "test_000_Example_576p50.MP4" 1>&2
@echo on
"D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe" %rrr% --container y4m "D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy" - | "C:\SOFTWARE\Vapoursynth-x64\ffmpeg.exe" -hide_banner -v info -nostats -f yuv4mpegpipe -i pipe: -probesize 100M -analyzeduration 100M -fps_mode passthrough -c:v libx264 -crf 18 -preset slow -pix_fmt yuv420p -movflags +faststart+write_colr -an -y "test_000_Example_576p50.MP4" 1>&2
@echo off
echo. 1>&2
goto :eof
