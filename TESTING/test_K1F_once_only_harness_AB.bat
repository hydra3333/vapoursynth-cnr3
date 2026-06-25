@echo off
REM test_K1F_once_only_harness_AB.bat
REM
REM K.1F acceptance harness. Proves LIVE DIRECT CACHED-OUTPUT RETURN (branch-b).
REM Runs over a SYNTHETIC deterministic clip (same golden chain as K.1E.3):
REM   source[0] Y128 U96  V160 -> output[0]=source[0]   (fresh start)
REM   source[1] Y128 U224 V32  -> output[1]=Y128 U161 V95
REM   source[2] Y128 U192 V64  -> output[2]=Y128 U163 V93
REM
REM RUNS:
REM   RUN 1  mode=processing, --start 0 --end 2  -> first/uncached compute of 0,1,2
REM          (K.1E.3 REGRESSION: 1->161/95, 2->163/93; NEGATIVE control: first request must COMPUTE)
REM   RUN 2  mode=cachehit,  --start 0 --end 3   -> positions 0,1,2 compute, position 3 re-requests
REM          frame 2 -> CACHE-HIT on cached output[2] (must return 163/93 with branch=CACHE-HIT)
REM   RUN 3  mode=repeat0,   --start 0 --end 1   -> position 0 fresh-start, position 1 re-requests
REM          frame 0 -> CACHE-HIT (proves present-N dispatch precedes the n==0 fresh-start gate)
REM   RUN 4  mode=passthrough, --start 0 --end 2 -> source bypass (sanity: no [KDT] at all)
REM
REM CORE-CACHE DEFEAT: the .vpy applies std.SetVideoCache(CNR3_node, mode=0) so the re-request
REM re-enters CNR3::getFrame instead of being served by the VS core cache (R76 mode=0 = always
REM disable). Without this the cache-hit could be a FALSE PASS.
REM
REM MANDATORY PASS LOGIC: bytes-match ALONE is not a pass. The re-requested frame MUST show a
REM branch=CACHE-HIT [KDT] line with pixel_compute=0 p11b_called=0 p11c_called=0 and balanced
REM ledgers. bytes-match WITHOUT a CACHE-HIT [KDT] = core cache intercepted = INCONCLUSIVE (FAIL).

set "rrr=-r 1"

set "mode_passthrough=passthrough"
set "mode_processing=processing"
set "mode_cachehit=cachehit"
set "mode_repeat0=repeat0"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_K1F_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
REM Flip to x64\Release to prove the Release DLL:
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

set "checker=%vs_root%\test_K1F_check_y4m_constant_plane.py"

REM Golden values
set "golden1_u=161"
set "golden1_v=95"
set "golden2_u=163"
set "golden2_v=93"

REM Output files
set "r1_y4m=%source_path%\K1F_run1_processing_temp.y4m"
set "r1_err=%source_path%\K1F_run1_processing_temp_stderr.txt"
set "r2_y4m=%source_path%\K1F_run2_cachehit_temp.y4m"
set "r2_err=%source_path%\K1F_run2_cachehit_temp_stderr.txt"
set "r3_y4m=%source_path%\K1F_run3_repeat0_temp.y4m"
set "r3_err=%source_path%\K1F_run3_repeat0_temp_stderr.txt"
set "r4_y4m=%source_path%\K1F_run4_passthrough_temp.y4m"
set "r4_err=%source_path%\K1F_run4_passthrough_temp_stderr.txt"

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

REM === RUN 1: processing 0..2 (regression + negative control) ===
"%vspipe%" %rrr% --start 0 --end 2 --arg mode="%mode_processing%" --container y4m "%vpy%" "%r1_y4m%"  2>>"%r1_err%"
set "rc_r1=%ERRORLEVEL%"
echo (RUN1 processing exit=%rc_r1%) >>"%r1_err%"

REM === RUN 2: cachehit 0..3 (position 3 = CACHE-HIT on output[2]) ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="%mode_cachehit%" --container y4m "%vpy%" "%r2_y4m%"  2>>"%r2_err%"
set "rc_r2=%ERRORLEVEL%"
echo (RUN2 cachehit exit=%rc_r2%) >>"%r2_err%"

REM === RUN 3: repeat0 0..1 (position 1 = CACHE-HIT on output[0]) ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="%mode_repeat0%" --container y4m "%vpy%" "%r3_y4m%"  2>>"%r3_err%"
set "rc_r3=%ERRORLEVEL%"
echo (RUN3 repeat0 exit=%rc_r3%) >>"%r3_err%"

REM === RUN 4: passthrough 0..2 (sanity, no [KDT]) ===
"%vspipe%" %rrr% --start 0 --end 2 --arg mode="%mode_passthrough%" --container y4m "%vpy%" "%r4_y4m%"  2>>"%r4_err%"
set "rc_r4=%ERRORLEVEL%"
echo (RUN4 passthrough exit=%rc_r4%) >>"%r4_err%"

echo.
echo ====================================================
echo CHECK 1 - RUN1 regression + negative control (first/uncached must COMPUTE)
echo ====================================================
echo RUN1 processing exit code = %rc_r1% 1>&2
if not "%rc_r1%"=="0" ( echo RESULT 1A: FAIL -- RUN1 did not exit 0 1>&2 ) else ( echo RESULT 1A: PASS -- RUN1 exited 0 1>&2 )
findstr /C:"N=1 branch=PREDECESSOR-PRESENT-COMPUTE" "%r1_err%" >NUL && echo RESULT 1B: PASS -- frame 1 COMPUTED (not cache-hit) 1>&2 || echo RESULT 1B: FAIL -- frame 1 did not show predecessor-present compute 1>&2
findstr /C:"N=2 branch=PREDECESSOR-PRESENT-COMPUTE" "%r1_err%" >NUL && echo RESULT 1C: PASS -- frame 2 COMPUTED (not cache-hit) 1>&2 || echo RESULT 1C: FAIL -- frame 2 did not show predecessor-present compute 1>&2
findstr /C:"branch=CACHE-HIT" "%r1_err%" >NUL && echo RESULT 1D: FAIL -- NEGATIVE control violated: a first/uncached request took CACHE-HIT 1>&2 || echo RESULT 1D: PASS -- no cache-hit on first/uncached requests 1>&2
"%python_exe%" "%checker%" "%r1_y4m%" 128 %golden1_u% %golden1_v% 1 1>&2
"%python_exe%" "%checker%" "%r1_y4m%" 128 %golden2_u% %golden2_v% 2 1>&2

echo.
echo ====================================================
echo CHECK 2 - RUN2 CACHE-HIT on frame 2 (THE K.1F PROOF)
echo ====================================================
echo RUN2 cachehit exit code = %rc_r2% 1>&2
if not "%rc_r2%"=="0" ( echo RESULT 2A: FAIL -- RUN2 did not exit 0 1>&2 ) else ( echo RESULT 2A: PASS -- RUN2 exited 0 1>&2 )
REM Mandatory: the CACHE-HIT KDT line MUST be present (definitive proof getFrame re-entered).
findstr /C:"branch=CACHE-HIT" "%r2_err%" >NUL && echo RESULT 2B: PASS -- branch=CACHE-HIT present (getFrame re-entered, core cache defeated) 1>&2 || echo RESULT 2C: FAIL/INCONCLUSIVE -- no CACHE-HIT KDT: core cache likely intercepted; NOT a K.1F pass 1>&2
findstr /C:"branch=CACHE-HIT" "%r2_err%" | findstr /C:"pixel_compute=0" /C:"p11b_called=0" /C:"p11c_called=0" >NUL && echo RESULT 2D: PASS -- cache-hit performed no compute 1>&2 || echo RESULT 2D: CHECK -- inspect cache-hit KDT for pixel_compute/p11b/p11c =0 1>&2
findstr /C:"cache_hit_pin_balance=0" "%r2_err%" >NUL && echo RESULT 2E: PASS -- cache-hit pin balance zero 1>&2 || echo RESULT 2E: CHECK -- inspect cache_hit_pin_balance 1>&2
findstr /C:"source_trigger_released=1" "%r2_err%" >NUL && echo RESULT 2F: PASS -- trigger source retrieved and released 1>&2 || echo RESULT 2F: CHECK -- inspect source_trigger_released 1>&2
REM Byte identity: frame at output index 3 (the re-requested frame 2) must equal golden 163/93.
"%python_exe%" "%checker%" "%r2_y4m%" 128 %golden2_u% %golden2_v% 3 1>&2

echo.
echo ====================================================
echo CHECK 3 - RUN3 repeated frame 0 (present-N dispatch precedes n==0 gate)
echo ====================================================
echo RUN3 repeat0 exit code = %rc_r3% 1>&2
if not "%rc_r3%"=="0" ( echo RESULT 3A: FAIL -- RUN3 did not exit 0 1>&2 ) else ( echo RESULT 3A: PASS -- RUN3 exited 0 1>&2 )
findstr /C:"N=0 FRAME0-FRESH-START" "%r3_err%" >NUL && echo RESULT 3B: PASS -- first frame 0 was FRESH-START 1>&2 || echo RESULT 3B: CHECK -- inspect first frame-0 branch 1>&2
findstr /C:"branch=CACHE-HIT" "%r3_err%" >NUL && echo RESULT 3C: PASS -- second frame 0 was CACHE-HIT (present-N precedes n==0 gate) 1>&2 || echo RESULT 3C: FAIL/INCONCLUSIVE -- no CACHE-HIT on repeated frame 0 1>&2

echo.
echo ====================================================
echo CHECK 4 - RUN4 passthrough negative (no [KDT] at all)
echo ====================================================
echo RUN4 passthrough exit code = %rc_r4% 1>&2
findstr /C:"[KDT]" "%r4_err%" >NUL && echo RESULT 4: FAIL -- [KDT] present in passthrough bypass 1>&2 || echo RESULT 4: PASS -- no [KDT] in passthrough bypass 1>&2

echo.
echo ====================================================
echo SUMMARY (read the per-check RESULT lines above; this line is a label, not a verdict):
echo   1 regression+negative / 2 CACHE-HIT frame2 (THE PROOF) / 3 repeated-frame0 dispatch / 4 passthrough-clean
echo   K.1F PASS REQUIRES: RESULT 2B PASS (branch=CACHE-HIT present) AND 2D/2E/2F PASS AND frame-2 bytes 163/93.
echo   bytes-match WITHOUT a CACHE-HIT KDT line = core cache intercepted = INCONCLUSIVE = NOT a pass.
echo ====================================================
pause
