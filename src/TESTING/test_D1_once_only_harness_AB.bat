@echo off
REM test_D1_once_only_harness_AB.bat
REM
REM D.1 acceptance harness. Proves LIVE BRANCH-(d) EXACT-ANCHOR SINGLE-HOLE RECOVERY.
REM Runs over a SYNTHETIC deterministic clip (D.1 golden chain, distinct from K.1E.3):
REM   source[0] Y128 U80  V176 -> output[0]=source[0]      (fresh start = anchor)
REM   source[1] Y128 U208 V48  -> output[1]=Y128 U145 V111 (the HOLE, filled by recovery)
REM   source[2] Y128 U176 V80  -> output[2]=Y128 U147 V109 (the TARGET = D.1 GOLDEN)
REM
REM RUNS:
REM   RUN 1  mode=processing,  --start 0 --end 2 -> first/uncached compute of 0,1,2.
REM          BRANCH-C regression (contract changed): 1->145/111, 2->147/109 via predecessor-present.
REM          NEGATIVE control: frame 2 here has output[1] PRESENT -> must take PREDECESSOR-PRESENT,
REM          NOT recovery (no branch=RECOVER allowed in this run).
REM   RUN 2  mode=cachehit,    --start 0 --end 3 -> positions 0,1,2 compute, position 3 re-requests
REM          frame 2 -> CACHE-HIT on cached output[2] (must return 147/109; K.1F regression).
REM   RUN 3  mode=recovery,    --start 0 --end 1 -> positions 0 then 2 (frame 1 SKIPPED).
REM          position 0 fresh-start (anchor). position 1 requests output[2] with output[1] ABSENT,
REM          output[0] PRESENT -> branch=RECOVER anchor=0 hole_count=1 holes=[1] -> 147/109 (THE PROOF).
REM   RUN 4  mode=passthrough, --start 0 --end 2 -> source bypass (sanity: no [KDT] at all).
REM
REM CORE-CACHE DEFEAT: the .vpy applies std.SetVideoCache(CNR3_node, mode=0) so the skipped/re-requested
REM frame re-enters CNR3::getFrame instead of being served by the VS core cache (R76 mode=0 = always
REM disable). Without this the recovery/cache-hit could be a FALSE PASS.
REM
REM MANDATORY PASS LOGIC: bytes-match ALONE is not a pass. The recovery frame MUST show a
REM branch=RECOVER [KDT] line (recover_branch=exact-anchor anchor=0 hole_count=1 holes=[1]
REM source_requests=[1,2] hole=1 outcome=computed pin_balance=0 p11c_called=0 scene_change_deferred=1).
REM FAIL-CLOSED ORDERING: FRAME0-FRESH-START for N=0 must appear BEFORE branch=RECOVER N=2 anchor=0.

set "rrr=-r 1"

set "mode_passthrough=passthrough"
set "mode_processing=processing"
set "mode_cachehit=cachehit"
set "mode_recovery=recovery"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_D1_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
REM Flip to x64\Release to prove the Release DLL:
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

set "checker=%vs_root%\test_K1F_check_y4m_constant_plane.py"

REM Golden values (D.1 chain)
set "golden1_u=145"
set "golden1_v=111"
set "golden2_u=147"
set "golden2_v=109"

REM Output files
set "r1_y4m=%source_path%\D1_run1_processing_temp.y4m"
set "r1_err=%source_path%\D1_run1_processing_temp_stderr.txt"
set "r2_y4m=%source_path%\D1_run2_cachehit_temp.y4m"
set "r2_err=%source_path%\D1_run2_cachehit_temp_stderr.txt"
set "r3_y4m=%source_path%\D1_run3_recovery_temp.y4m"
set "r3_err=%source_path%\D1_run3_recovery_temp_stderr.txt"
set "r4_y4m=%source_path%\D1_run4_passthrough_temp.y4m"
set "r4_err=%source_path%\D1_run4_passthrough_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"

echo Installing built DLL from: %built_dll_folder%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll"

REM Pre-delete outputs so a stale file cannot masquerade as a produced frame.
del /F "%r1_y4m%" "%r2_y4m%" "%r3_y4m%" "%r4_y4m%" 2>NUL
del /F "%r1_err%" "%r2_err%" "%r3_err%" "%r4_err%" 2>NUL

REM === RUN 1: processing 0..2 (branch-c regression + negative control) ===
"%vspipe%" %rrr% --start 0 --end 2 --arg mode="%mode_processing%" --container y4m "%vpy%" "%r1_y4m%"  2>>"%r1_err%"
set "rc_r1=%ERRORLEVEL%"
echo (RUN1 processing exit=%rc_r1%) >>"%r1_err%"

REM === RUN 2: cachehit 0..3 (position 3 = CACHE-HIT on output[2]) ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="%mode_cachehit%" --container y4m "%vpy%" "%r2_y4m%"  2>>"%r2_err%"
set "rc_r2=%ERRORLEVEL%"
echo (RUN2 cachehit exit=%rc_r2%) >>"%r2_err%"

REM === RUN 3: recovery 0..1 (position 1 = RECOVER on output[2], hole=1, anchor=0) ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="%mode_recovery%" --container y4m "%vpy%" "%r3_y4m%"  2>>"%r3_err%"
set "rc_r3=%ERRORLEVEL%"
echo (RUN3 recovery exit=%rc_r3%) >>"%r3_err%"

REM === RUN 4: passthrough 0..2 (sanity, no [KDT]) ===
"%vspipe%" %rrr% --start 0 --end 2 --arg mode="%mode_passthrough%" --container y4m "%vpy%" "%r4_y4m%"  2>>"%r4_err%"
set "rc_r4=%ERRORLEVEL%"
echo (RUN4 passthrough exit=%rc_r4%) >>"%r4_err%"

echo.
echo ====================================================
echo CHECK 1 - RUN1 branch-c regression + negative control (first/uncached must COMPUTE via predecessor-present)
echo ====================================================
echo RUN1 processing exit code = %rc_r1% 1>&2
if not "%rc_r1%"=="0" ( echo RESULT 1A: FAIL -- RUN1 did not exit 0 1>&2 ) else ( echo RESULT 1A: PASS -- RUN1 exited 0 1>&2 )
findstr /C:"N=1 branch=PREDECESSOR-PRESENT-COMPUTE" "%r1_err%" >NUL && echo RESULT 1B: PASS -- frame 1 predecessor-present compute 1>&2 || echo RESULT 1B: FAIL -- frame 1 did not show predecessor-present compute 1>&2
findstr /C:"N=2 branch=PREDECESSOR-PRESENT-COMPUTE" "%r1_err%" >NUL && echo RESULT 1C: PASS -- frame 2 predecessor-present compute 1>&2 || echo RESULT 1C: FAIL -- frame 2 did not show predecessor-present compute 1>&2
findstr /C:"branch=RECOVER" "%r1_err%" >NUL && echo RESULT 1D: FAIL -- NEGATIVE control violated: a request with predecessor present took RECOVER 1>&2 || echo RESULT 1D: PASS -- no recovery when predecessor present (took predecessor-present) 1>&2
findstr /C:"branch=CACHE-HIT" "%r1_err%" >NUL && echo RESULT 1E: FAIL -- NEGATIVE control violated: a first/uncached request took CACHE-HIT 1>&2 || echo RESULT 1E: PASS -- no cache-hit on first/uncached requests 1>&2
"%python_exe%" "%checker%" "%r1_y4m%" 128 %golden1_u% %golden1_v% 1 1>&2
"%python_exe%" "%checker%" "%r1_y4m%" 128 %golden2_u% %golden2_v% 2 1>&2

echo.
echo ====================================================
echo CHECK 2 - RUN2 CACHE-HIT on frame 2 (K.1F regression; routing changed)
echo ====================================================
echo RUN2 cachehit exit code = %rc_r2% 1>&2
if not "%rc_r2%"=="0" ( echo RESULT 2A: FAIL -- RUN2 did not exit 0 1>&2 ) else ( echo RESULT 2A: PASS -- RUN2 exited 0 1>&2 )
findstr /C:"branch=CACHE-HIT" "%r2_err%" >NUL && echo RESULT 2B: PASS -- branch=CACHE-HIT present (getFrame re-entered) 1>&2 || echo RESULT 2C: FAIL/INCONCLUSIVE -- no CACHE-HIT KDT; NOT a pass 1>&2
findstr /C:"branch=CACHE-HIT" "%r2_err%" | findstr /C:"pixel_compute=0" /C:"p11b_called=0" /C:"p11c_called=0" >NUL && echo RESULT 2D: PASS -- cache-hit performed no compute 1>&2 || echo RESULT 2D: CHECK -- inspect cache-hit KDT compute flags 1>&2
"%python_exe%" "%checker%" "%r2_y4m%" 128 %golden2_u% %golden2_v% 3 1>&2

echo.
echo ====================================================
echo CHECK 3 - RUN3 RECOVERY on frame 2 (THE D.1 PROOF)
echo ====================================================
echo RUN3 recovery exit code = %rc_r3% 1>&2
if not "%rc_r3%"=="0" ( echo RESULT 3A: FAIL -- RUN3 did not exit 0 1>&2 ) else ( echo RESULT 3A: PASS -- RUN3 exited 0 1>&2 )
REM Mandatory: the RECOVER KDT line MUST be present (definitive proof branch-d fired).
findstr /C:"branch=RECOVER" "%r3_err%" >NUL && echo RESULT 3B: PASS -- branch=RECOVER present (recovery fired, core cache defeated) 1>&2 || echo RESULT 3B: FAIL/INCONCLUSIVE -- no RECOVER KDT; NOT a D.1 pass 1>&2
findstr /C:"recover_branch=exact-anchor" "%r3_err%" | findstr /C:"anchor=0" /C:"hole_count=1" >NUL && echo RESULT 3C: PASS -- exact-anchor anchor=0 hole_count=1 1>&2 || echo RESULT 3C: CHECK -- inspect recover_branch/anchor/hole_count 1>&2
findstr /C:"holes=[1]" "%r3_err%" >NUL && echo RESULT 3D: PASS -- holes=[1] 1>&2 || echo RESULT 3D: CHECK -- inspect holes list 1>&2
findstr /C:"source_requests=[1,2]" "%r3_err%" >NUL && echo RESULT 3E: PASS -- source_requests=[1,2] 1>&2 || echo RESULT 3E: CHECK -- inspect source_requests 1>&2
findstr /C:"hole=1 outcome=computed" "%r3_err%" >NUL && echo RESULT 3F: PASS -- hole 1 outcome=computed 1>&2 || echo RESULT 3F: CHECK -- inspect hole outcome 1>&2
findstr /C:"branch=RECOVER" "%r3_err%" | findstr /C:"pin_balance=0" >NUL && echo RESULT 3G: PASS -- recovery pin_balance=0 1>&2 || echo RESULT 3G: CHECK -- inspect pin_balance 1>&2
findstr /C:"branch=RECOVER" "%r3_err%" | findstr /C:"p11c_called=0 scene_change_deferred=1" >NUL && echo RESULT 3H: PASS -- P.11C deferred (consistent with branch-c) 1>&2 || echo RESULT 3H: CHECK -- inspect p11c/scene_change flags 1>&2
REM FAIL-CLOSED ORDERING: frame-0 fresh-start must appear before the recovery for frame 2.
findstr /C:"N=0 FRAME0-FRESH-START" "%r3_err%" >NUL && echo RESULT 3I: PASS -- FRAME0-FRESH-START present (anchor established) 1>&2 || echo RESULT 3I: FAIL/INCONCLUSIVE -- anchor frame 0 not established; ordering not proven 1>&2
REM Byte identity: frame at output index 1 (the recovered frame 2) must equal golden 147/109.
"%python_exe%" "%checker%" "%r3_y4m%" 128 %golden2_u% %golden2_v% 1 1>&2

echo.
echo ====================================================
echo CHECK 4 - RUN4 passthrough negative (no [KDT] at all)
echo ====================================================
echo RUN4 passthrough exit code = %rc_r4% 1>&2
findstr /C:"[KDT]" "%r4_err%" >NUL && echo RESULT 4: FAIL -- [KDT] present in passthrough bypass 1>&2 || echo RESULT 4: PASS -- no [KDT] in passthrough bypass 1>&2

echo.
echo ====================================================
echo SUMMARY (read the per-check RESULT lines above; this line is a label, not a verdict):
echo   1 branch-c regression+negative / 2 CACHE-HIT regression / 3 RECOVERY frame2 (THE PROOF) / 4 passthrough-clean
echo   D.1 PASS REQUIRES: RESULT 3B PASS (branch=RECOVER) AND 3C-3I PASS AND recovered-frame bytes 147/109,
echo   AND branch-c regression (1B/1C) AND negative control (1D/1E) AND K.1F cache-hit (2B/2D) all PASS.
echo   bytes-match WITHOUT a RECOVER KDT line = core cache intercepted = INCONCLUSIVE = NOT a pass.
echo ====================================================
pause
