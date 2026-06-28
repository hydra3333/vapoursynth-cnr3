@echo off
REM ============================================================================
REM CNR3 P.11C.3 -- BRANCH-C SCENE-DETECTION LIVE PROOF (driver)
REM Runs the .vpy twice (control + cut) at the LIVE DEFAULT scdthr=10.0 (threshold 1402),
REM piping to NUL, capturing the [KDT] lines from stderr for assertion.
REM Place this .bat and the .vpy next to the portable VapourSynth, like the existing harness.
REM ============================================================================

set "rrr=-r 1"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_D3_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
REM Flip to x64\Release to prove the Release DLL:
REM set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release"
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"

echo Installing built DLL from: %built_dll_folder%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll

set "VSPIPE=D:\TEST\Vapoursynth_x64_R76\lib\site-packages\vapoursynth\vspipe.exe"
set "VPY=%~dp0test_P11C3_branch_c_scene_detection.vpy"
set "KDTLOG_CONTROL=%~dp0P11C3_control_kdt.log"
set "KDTLOG_CUT=%~dp0P11C3_cut_kdt.log"

echo. 1>&2
echo ========================================== 1>&2
echo P.11C.3 branch-c proof : C-control (no cut) 1>&2
echo expect: threshold=1402 diff_total=64 samples=16 detected=0 reset=0 blend=1 store=0 1>&2
echo ========================================== 1>&2
set "CNR3_P11C3_CASE=control"
"%VSPIPE%" -r 1 --container y4m "%VPY%" NUL 2> "%KDTLOG_CONTROL%"
echo control exit_code=%ERRORLEVEL% 1>&2
echo --- [KDT] lines (control) --- 1>&2
findstr /C:"[KDT]" "%KDTLOG_CONTROL%" 1>&2

echo. 1>&2
echo ========================================== 1>&2
echo P.11C.3 branch-c proof : C-cut (hard cut) 1>&2
echo expect: threshold=1402 diff_total=2040 samples=2 detected=1 reset=1 blend=0 store=1 1>&2
echo ========================================== 1>&2
set "CNR3_P11C3_CASE=cut"
"%VSPIPE%" -r 1 --container y4m "%VPY%" NUL 2> "%KDTLOG_CUT%"
echo cut exit_code=%ERRORLEVEL% 1>&2
echo --- [KDT] lines (cut) --- 1>&2
findstr /C:"[KDT]" "%KDTLOG_CUT%" 1>&2

echo. 1>&2
echo ========================================== 1>&2
echo Manual assertion checklist (compare the [KDT] N=1 PREDECESSOR-PRESENT-COMPUTE lines): 1>&2
echo   C-control: scene_change_threshold=1402 scene_change_diff_total=64 1>&2
echo              scene_change_samples_examined=16 scene_change_detected=0 1>&2
echo              scene_change_reset_output_used=0 recursive_chroma_blend_used=1 1>&2
echo              store_as_checkpoint=0 resulting_slot_is_checkpoint_expected=0 1>&2
echo   C-cut:     scene_change_threshold=1402 scene_change_diff_total=2040 1>&2
echo              scene_change_samples_examined=2 scene_change_detected=1 1>&2
echo              scene_change_reset_output_used=1 recursive_chroma_blend_used=0 1>&2
echo              store_as_checkpoint=1 resulting_slot_is_checkpoint_expected=1 1>&2
echo. 1>&2
echo   Byte checks (inspect frame 1 output chroma, e.g. via a y4m dump or a small reader): 1>&2
echo     C-control: 100 ^< outU ^< 160 AND 80 ^< outV ^< 120  (blend, != current) 1>&2
echo     C-cut:     outU==160 AND outV==80                     (reset copy == current) 1>&2
echo ========================================== 1>&2

pause
goto :eof
