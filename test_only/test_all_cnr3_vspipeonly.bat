@echo on

REM echo type "D:\TEST\Vapoursynth_x64_R76\test_cnr3_realclip.vpy"
REM type "D:\TEST\Vapoursynth_x64_R76\test_cnr3_realclip.vpy"

set "LOG=test_all_cnr3_vspipeonly.log"
set "vspipe=D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe"

REM set "rrr="
set "rrr=-r 1"

echo.>"%LOG%" 2>&1

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_000_Example_576i.vpy"
REM call :do_it "%vpy%"

set "vpy=D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p25.vpy"
call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_000_Example_576p50.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3-8bit.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3-stronger_repteated_output_frame_access_test.vpy"
REM echo TYPE "%vpy%" >>"%LOG%" 2>&1
REM TYPE "%vpy%" >>"%LOG%" 2>&1
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3-big_buck_bunny_480p24_30s.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3-elephants_dream_480p24_30s.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\big_buck_bunny_480p24_120s.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\elephants_dream_480p24_120s.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\Test_cnr3_realclip_x10.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3_realclip.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3-16bit.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3-8bit_luma_constant_chroma_changes.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3_scenechange_16bit.vpy"
REM call :do_it "%vpy%"

REM set "vpy=D:\TEST\Vapoursynth_x64_R76\test_cnr3_scenechange_8bit.vpy"
REM call :do_it "%vpy%"

pause
exit

:do_it
echo. >>"%LOG%" 2>&1
echo ----------------------------------------------------------------------------------------- >>"%LOG%" 2>&1
echo. >>"%LOG%" 2>&1
REM echo TYPE "%~1" >>"%LOG%" 2>&1
REM TYPE "%~1" >>"%LOG%" 2>&1
echo. >>"%LOG%" 2>&1
REM echo "%vspipe%" %rrr% --info "%~1" >>"%LOG%" 2>&1
REM "%vspipe%" %rrr% --info "%~1" >>"%LOG%" 2>&1
echo. >>"%LOG%" 2>&1
echo. >>"%LOG%" 2>&1
echo "%vspipe%" %rrr%  "%~1" NUL >>"%LOG%" 2>&1
"%vspipe%" %rrr%  "%~1" NUL >>"%LOG%" 2>&1
echo. >>"%LOG%" 2>&1
echo ----------------------------------------------------------------------------------------- >>"%LOG%" 2>&1
echo. >>"%LOG%" 2>&1
goto :eof


"C:\SOFTWARE\ffmpeg\ffmpeg.exe" -i big_buck_bunny_480p24.y4m -t 30 -c:v libx264 -pix_fmt yuv420p -crf 0 -y big_buck_bunny_480p24_30s.mp4

"C:\SOFTWARE\ffmpeg\ffmpeg.exe" -i big_buck_bunny_480p24.y4m -t 60 -c:v libx264 -pix_fmt yuv420p -crf 0 -y big_buck_bunny_480p24_60s.mp4

"C:\SOFTWARE\ffmpeg\ffmpeg.exe" -i big_buck_bunny_480p24.y4m -t 120 -c:v libx264 -pix_fmt yuv420p -crf 0 -y big_buck_bunny_480p24_120s.mp4

"C:\SOFTWARE\ffmpeg\ffmpeg.exe" -i elephants_dream_480p24.y4m -t 30 -c:v libx264 -pix_fmt yuv420p -crf 0 -y elephants_dream_480p24_30s.mp4

"C:\SOFTWARE\ffmpeg\ffmpeg.exe" -i elephants_dream_480p24.y4m -t 60 -c:v libx264 -pix_fmt yuv420p -crf 0 -y elephants_dream_480p24_60s.mp4

"C:\SOFTWARE\ffmpeg\ffmpeg.exe" -i elephants_dream_480p24.y4m -t 120 -c:v libx264 -pix_fmt yuv420p -crf 0 -y elephants_dream_480p24_120s.mp4

