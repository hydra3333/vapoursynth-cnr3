@echo off
REM test_P11C4_branch_d_recovery_scene_detection.bat
REM
REM CNR3 P.11C.4 LIVE BRANCH-(d) RECOVERY SCENE-DETECTION proof. Adapted from the D3 harness.
REM Proves per-hole + target scene detection during floor-fresh-start recovery (floor=0, holes=[1,2],
REM target=3), at the live default scdthr=10.0 (derived threshold 5606), 16x16 / 8x8 chroma grid.
REM
REM CORE-CACHE DEFEAT: the .vpy applies std.SetVideoCache(CNR3_node, mode=0).
REM MANDATORY: bytes-match ALONE is not a pass. The recovery KDT line MUST show
REM recover_branch=floor-fresh-start + the per-hole/target scene fields.
REM
REM Flip built_dll_folder between x64\Debug and x64\Release to prove BOTH configs.

set "rrr=-r 1"
set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_P11C4_branch_d_recovery_scene_detection.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
REM Flip to x64\Release to prove the Release DLL:
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"
set "checker=%vs_root%\test_K1F_check_y4m_constant_plane.py"

set "ctrl_y4m=%source_path%\P11C4_dcontrol_temp.y4m"
set "ctrl_err=%source_path%\P11C4_dcontrol_stderr.txt"
set "hole_y4m=%source_path%\P11C4_dcutathole_temp.y4m"
set "hole_err=%source_path%\P11C4_dcutathole_stderr.txt"
set "tgt_y4m=%source_path%\P11C4_dcutattarget_temp.y4m"
set "tgt_err=%source_path%\P11C4_dcutattarget_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"

echo Installing built DLL from: %built_dll_folder%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll"

del /F "%ctrl_y4m%" "%hole_y4m%" "%tgt_y4m%" 2>NUL
del /F "%ctrl_err%" "%hole_err%" "%tgt_err%" 2>NUL

REM ============ RUN 1: D-control (no cut) ============
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="dcontrol" --container y4m "%vpy%" "%ctrl_y4m%" 2>>"%ctrl_err%"
echo (dcontrol exit=%ERRORLEVEL%) >>"%ctrl_err%"

REM ============ RUN 2: D-cut-at-hole (cut at frame 2) ============
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="dcutathole" --container y4m "%vpy%" "%hole_y4m%" 2>>"%hole_err%"
echo (dcutathole exit=%ERRORLEVEL%) >>"%hole_err%"

REM ============ RUN 3: D-cut-at-target (cut at frame 3) ============
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="dcutattarget" --container y4m "%vpy%" "%tgt_y4m%" 2>>"%tgt_err%"
echo (dcutattarget exit=%ERRORLEVEL%) >>"%tgt_err%"

echo.
echo ====================================================
echo CHECK 1 - D-control : recovery runs, NO cut anywhere
echo ====================================================
findstr /C:"recover_branch=floor-fresh-start" "%ctrl_err%" >NUL && echo R1: PASS recover_branch=floor-fresh-start 1>&2 || echo R1: FAIL not floor-fresh-start 1>&2
findstr /C:"floor_scene_change_not_applicable=1" "%ctrl_err%" >NUL && echo R2: PASS floor not_applicable 1>&2 || echo R2: FAIL floor not flagged not_applicable 1>&2
findstr /C:"holes=[1,2]" "%ctrl_err%" >NUL && echo R3: PASS holes=[1,2] 1>&2 || echo R3: CHECK holes 1>&2
findstr /C:"hole_scene_change_detected=1" "%ctrl_err%" >NUL && echo R4: FAIL a hole wrongly detected a cut 1>&2 || echo R4: PASS no hole detected a cut 1>&2
findstr /C:"target_scene_change_detected=1" "%ctrl_err%" >NUL && echo R5: FAIL target wrongly detected a cut 1>&2 || echo R5: PASS target detected no cut 1>&2
findstr /C:"hole_scene_change_diff_total=256" "%ctrl_err%" >NUL && echo R6: PASS a hole shows diff_total=256 (no-cut) 1>&2 || echo R6: CHECK hole diff_total 1>&2
findstr /C:"target_scene_change_diff_total=256" "%ctrl_err%" >NUL && echo R7: PASS target diff_total=256 (no-cut) 1>&2 || echo R7: CHECK target diff_total 1>&2
findstr /C:"target_store_as_checkpoint=0" "%ctrl_err%" >NUL && echo R8: PASS target store_as_checkpoint=0 1>&2 || echo R8: CHECK target store 1>&2
echo --- [KDT] (dcontrol) full lines --- 1>&2
findstr /C:"[KDT]" "%ctrl_err%" 1>&2

echo.
echo ====================================================
echo CHECK 2 - D-cut-at-hole : cut at hole 2 resets + checkpoints
echo ====================================================
findstr /C:"recover_branch=floor-fresh-start" "%hole_err%" >NUL && echo R1: PASS floor-fresh-start 1>&2 || echo R1: FAIL 1>&2
findstr /C:"hole=2" "%hole_err%" | findstr /C:"hole_scene_change_detected=1" >NUL && echo R2: PASS hole2 detected=1 1>&2 || echo R2: CHECK hole2 detection (inspect line) 1>&2
findstr /C:"hole_scene_change_diff_total=6160" "%hole_err%" >NUL && echo R3: PASS hole2 diff_total=6160 1>&2 || echo R3: CHECK hole2 diff_total 1>&2
findstr /C:"hole_scene_change_samples_examined=10" "%hole_err%" >NUL && echo R4: PASS hole2 samples=10 1>&2 || echo R4: CHECK hole2 samples 1>&2
findstr /C:"hole_resulting_slot_is_checkpoint=1" "%hole_err%" >NUL && echo R5: PASS hole2 ACTUAL checkpoint=1 1>&2 || echo R5: CHECK hole2 resulting checkpoint 1>&2
findstr /C:"hole_scene_change_reset_output_used=1" "%hole_err%" >NUL && echo R5b: PASS hole2 reset_output_used=1 1>&2 || echo R5b: CHECK hole2 reset 1>&2
findstr /C:"hole_store_as_checkpoint=1" "%hole_err%" >NUL && echo R5c: PASS hole2 store_as_checkpoint=1 1>&2 || echo R5c: CHECK hole2 store 1>&2
findstr /C:"target_scene_change_detected=1" "%hole_err%" >NUL && echo R6: FAIL target wrongly cut 1>&2 || echo R6: PASS target no cut after hole reset 1>&2
echo --- [KDT] (dcutathole) --- 1>&2
findstr /C:"[KDT]" "%hole_err%" 1>&2
echo --- BYTE: rendered idx0 = target3 = BLEND(out2 160/80, src 100/120); LOCKED golden 255/151/83 --- 1>&2
echo     (sanity: U=151 between 100..160 and != both; V=83 between 80..120 and != both -> a blend, not reset) 1>&2
"%python_exe%" "%checker%" "%hole_y4m%" 255 151 83 0 1>&2

echo.
echo ====================================================
echo CHECK 3 - D-cut-at-target : cut at target resets + checkpoint(expected)
echo ====================================================
findstr /C:"recover_branch=floor-fresh-start" "%tgt_err%" >NUL && echo R1: PASS floor-fresh-start 1>&2 || echo R1: FAIL 1>&2
findstr /C:"hole_scene_change_detected=1" "%tgt_err%" >NUL && echo R2: FAIL a hole wrongly cut 1>&2 || echo R2: PASS holes no cut 1>&2
findstr /C:"target_scene_change_detected=1" "%tgt_err%" >NUL && echo R3: PASS target detected=1 1>&2 || echo R3: CHECK target detection 1>&2
findstr /C:"target_scene_change_diff_total=6120" "%tgt_err%" >NUL && echo R4: PASS target diff_total=6120 1>&2 || echo R4: CHECK target diff_total 1>&2
findstr /C:"target_scene_change_samples_examined=10" "%tgt_err%" >NUL && echo R5: PASS target samples=10 1>&2 || echo R5: CHECK target samples 1>&2
findstr /C:"target_resulting_slot_is_checkpoint_expected=1" "%tgt_err%" >NUL && echo R6: PASS target checkpoint_expected=1 1>&2 || echo R6: CHECK target expected checkpoint 1>&2
findstr /C:"target_scene_change_reset_output_used=1" "%tgt_err%" >NUL && echo R7: PASS target reset_output_used=1 1>&2 || echo R7: CHECK target reset 1>&2
echo --- [KDT] (dcutattarget) --- 1>&2
findstr /C:"[KDT]" "%tgt_err%" 1>&2
echo --- BYTE: rendered idx0 = target3 = RESET = current source chroma EXACTLY --- 1>&2
"%python_exe%" "%checker%" "%tgt_y4m%" 255 160 80 0 1>&2

echo.
echo ====================================================
echo P.11C.4 PASS REQUIRES: recover_branch=floor-fresh-start + per-hole/target scene fields matching the
echo goldens (dcontrol all detected=0; dcutathole hole2 detected=1/6160/10/checkpoint=1; dcutattarget
echo target detected=1/6120/10/checkpoint_expected=1) on BOTH Debug and Release. Re-run with built_dll_folder
echo flipped to x64\Release. Bytes-match without the KDT line = INCONCLUSIVE.
echo ====================================================
pause
goto :eof
