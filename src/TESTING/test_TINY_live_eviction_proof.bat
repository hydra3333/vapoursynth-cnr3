@echo off
setlocal EnableExtensions
REM test_TINY_live_eviction_proof.bat
REM
REM DESIGNER-OWNED (W3D) live proof harness for the TINY-100 diagnostic cache scaffold
REM (CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY). SINGLE run of the TINY DLL (not an A/B compare like W.3).
REM
REM PURPOSE: prove the tiny profile actually EVICTS on a SHORT (~260 frame) run -- the payoff of the
REM scaffold. Eviction CORRECTNESS was already proven by the W.3 arc (four-way 55/55 + the A/B harness)
REM at production scale; the tiny profile runs the SAME eviction code with smaller constants, so here we
REM only observe the triggers firing and victims detaching.
REM
REM PROVES, per v0.2 acceptance:
REM   IDENTITY : the loaded DLL is the TINY build (profile=tiny-100 if the KDT line carries it; ELSE by
REM              inference -- cap_trigger=1 on this short 576p50 run, which a NORMAL DLL cannot produce).
REM   CHECK 1  : cap_trigger=1 observed (capacity eviction fired), preferably on a KDT line that also
REM              shows detached=1..8 (non-vacuity: the trigger actually detached victims).
REM   CHECK 2  : ckpt_trigger=1 observed (checkpoint-retention eviction fired), preferably same-line detached.
REM   CHECK 3  : (INFO) branch=RECOVER observed -- recovery exercised under eviction.
REM   NOTE     : do NOT require slot_count to return below the active ceiling 100. v0.2: one bounded prune
REM              pass per store means the cache may sit at 101..110 between stores. That is expected
REM              hysteresis, NOT a failure. The acceptance is trigger-fired + victims-detached.
REM
REM REQUIRES: the TINY DLL built with BOTH CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY and CNR3_KEYSTONE_DEV_TRACE.
REM   Run the tiny four-way cache-core selftest SEPARATELY first (expect exit 0, cache_profile: tiny-100,
REM   13 visible SKIPPED lines); this live harness is the step AFTER that.

set "rrr=-r 1"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_TINY_live_eviction_proof.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "golden_source=%source_path%\000_Example_576p50.mp4"

REM ---------------------------------------------------------------------------------------
REM The TINY build folder (Debug or Release; either is fine -- integer path is config-agnostic).
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"
set "dll_folder_TINY=D:\TEST\dll_TINY_Debug"
set "selftest=%built_dll_folder%\cnr3_cache_core_selftest.exe"
set "selftest_log=%source_path%\TINY_live_selftest_stderr.txt"
REM ---------------------------------------------------------------------------------------

set "y4m=%source_path%\TINY_live_temp.y4m"
set "err=%source_path%\TINY_live_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%" || goto :fatal_cd

echo.
echo ====================================================
echo PREFLIGHT - paths and DLLs
echo ====================================================
set "preflight_fail=0"
if not exist "%vspipe%"            ( echo PREFLIGHT FAIL: vspipe not found: "%vspipe%" 1>&2 & set "preflight_fail=1" )
if not exist "%vpy%"               ( echo PREFLIGHT FAIL: vpy not found: "%vpy%" 1>&2 & set "preflight_fail=1" )
if not exist "%golden_source%"     ( echo PREFLIGHT FAIL: golden source not found: "%golden_source%" 1>&2 & set "preflight_fail=1" )
if not exist "%runtime_dll_folder%\" ( echo PREFLIGHT FAIL: runtime DLL folder not found: "%runtime_dll_folder%" 1>&2 & set "preflight_fail=1" )
if not exist "%built_dll_folder%\cnr3.dll" ( echo PREFLIGHT FAIL: built TINY DLL not found: "%built_dll_folder%\cnr3.dll" 1>&2 & set "preflight_fail=1" )
if not exist "%selftest%" ( echo PREFLIGHT FAIL: tiny selftest exe not found: "%selftest%" 1>&2 & set "preflight_fail=1" )
if not exist "%dll_folder_TINY%\"  ( echo PREFLIGHT FAIL: TINY staging folder not found: "%dll_folder_TINY%" 1>&2 & set "preflight_fail=1" )
if not "%preflight_fail%"=="0" ( echo PREFLIGHT RESULT: FAIL -- fix the paths above. 1>&2 & pause & exit /b 2 )
echo PREFLIGHT RESULT: PASS -- required files/folders exist. 1>&2

echo.
echo ====================================================
echo PRECONDITION - tiny selftest proves the build is TINY before the live run
echo ====================================================
REM Prove the build is the tiny profile up front (folded from the coder smoke harness): run the
REM cache-core selftest exe (standalone; no DLL install needed) and require exit 0 AND the tiny-100
REM profile marker in its heading. If this fails, the build is NOT tiny -- do not trust the live run.
del /F "%selftest_log%" 2>NUL
"%selftest%" 1>NUL 2>>"%selftest_log%"
set "st_rc=%ERRORLEVEL%"
if not "%st_rc%"=="0" ( echo RESULT P0: FAIL -- tiny selftest did not exit 0 ^(rc=%st_rc%^); build not usable 1>&2 & pause & exit /b 5 ) else ( echo RESULT P0: PASS -- tiny selftest exited 0 1>&2 )
findstr /C:"cache_profile: tiny-100" "%selftest_log%" >NUL && echo RESULT P1: PASS -- selftest heading reports cache_profile: tiny-100 1>&2 || ( echo RESULT P1: FAIL -- selftest did NOT report tiny-100; this is a NORMAL build, aborting before the live run 1>&2 & pause & exit /b 5 )

echo.
echo COPYING the built TINY cnr3.dll to staging from: %built_dll_folder%
call :install_dll "%built_dll_folder%\cnr3.dll" "%dll_folder_TINY%" "TINY staging copy" || goto :fatal_copy

REM Pre-delete outputs so a stale file cannot masquerade as a produced frame.
del /F "%y4m%" "%err%" 2>NUL

echo.
echo === RUN: TINY DLL (single run) ===
echo Installing TINY DLL from: %dll_folder_TINY%
call :install_dll "%dll_folder_TINY%\cnr3.dll" "%runtime_dll_folder%" "TINY runtime install" || goto :fatal_copy
dir /tw "%runtime_dll_folder%\cnr3.dll"
"%vspipe%" %rrr% --container y4m "%vpy%" "%y4m%"  2>>"%err%"
set "rc=%ERRORLEVEL%"
echo ^(TINY live run exit=%rc%^) >>"%err%"

echo.
echo ====================================================
echo CHECK 0 - run completed
echo ====================================================
if not "%rc%"=="0" ( echo RESULT 0A: FAIL -- TINY live run did not exit 0 ^(rc=%rc%^) 1>&2 ) else ( echo RESULT 0A: PASS -- TINY live run exited 0 1>&2 )
if exist "%y4m%" ( echo RESULT 0B: PASS -- output file exists 1>&2 ) else ( echo RESULT 0B: FAIL -- output file missing 1>&2 )
REM 0C sanity: the KDT line is present at all (build has CNR3_KEYSTONE_DEV_TRACE, helper wired).
findstr /C:"cap_trigger=" "%err%" >NUL && echo RESULT 0C: PASS -- W.3 [KDT] store-prune lines present 1>&2 || echo RESULT 0C: FAIL/INCONCLUSIVE -- no [KDT] cap_trigger line ^(KDT not built? helper not wired?^) 1>&2

echo.
echo ====================================================
echo CHECK I - TINY IDENTITY (which profile is loaded)
echo ====================================================
REM Primary: the live KDT line carries profile=tiny-100 (the coder added profile=%%s to the live KDT
REM line, so this is now the DIRECT identity proof, not inference).
findstr /C:"profile=tiny-100" "%err%" >NUL && ( echo RESULT I: PASS ^(direct^) -- KDT reports profile=tiny-100 1>&2 & goto :identity_done )
REM Guard against a normal DLL wrongly staged: if the live line explicitly says profile=normal, FAIL.
findstr /C:"profile=normal" "%err%" >NUL && ( echo RESULT I: FAIL -- KDT reports profile=normal; the TINY DLL was NOT loaded 1>&2 & goto :identity_done )
REM Fallback (marker not emitted by the live DLL yet): cap_trigger=1 on this ~260-frame 576p50 run is
REM itself proof of the tiny profile -- a NORMAL DLL (ceiling 1000, trigger 1100) cannot fire it this soon.
findstr /C:"cap_trigger=1" "%err%" >NUL && echo RESULT I: PASS ^(inferred^) -- cap_trigger=1 on a short 576p50 run; a normal DLL cannot fire capacity this soon, so this is the TINY profile 1>&2 || echo RESULT I: FAIL/INCONCLUSIVE -- no profile marker and no cap_trigger=1; cannot confirm the TINY DLL is loaded ^(wrong DLL? run too short? not dev-trace?^) 1>&2
:identity_done

echo.
echo ====================================================
echo CHECK 1 - CAPACITY eviction fired ^(+ non-vacuity^)
echo ====================================================
findstr /C:"cap_trigger=1" "%err%" >NUL && echo RESULT 1a: PASS -- capacity trigger fired ^(cap_trigger=1^) 1>&2 || echo RESULT 1a: FAIL -- capacity trigger never fired; lengthen CAPACITY_RUN_LENGTH past the tiny overflow_trigger ^(110^) 1>&2
findstr /C:"cap_trigger=1" "%err%" | findstr /R /C:"detached=[1-8]" >NUL && echo RESULT 1b: PASS -- a capacity-triggered prune detached victims ^(non-vacuous^) 1>&2 || echo RESULT 1b: CHECK -- cap_trigger=1 seen but no cap line with detached=1..8; inspect %err% 1>&2

echo.
echo ====================================================
echo CHECK 2 - CHECKPOINT eviction fired ^(+ non-vacuity^)
echo ====================================================
findstr /C:"ckpt_trigger=1" "%err%" >NUL && echo RESULT 2a: PASS -- checkpoint trigger fired ^(ckpt_trigger=1^) 1>&2 || echo RESULT 2a: FAIL -- checkpoint trigger never fired; ensure the run exceeds ~12 checkpoints ^(>= ~36 frames at tiny interval 3^) 1>&2
findstr /C:"ckpt_trigger=1" "%err%" | findstr /R /C:"detached=[1-8]" >NUL && echo RESULT 2b: PASS -- a checkpoint-triggered prune detached victims ^(non-vacuous^) 1>&2 || echo RESULT 2b: CHECK -- ckpt_trigger=1 seen but no checkpoint line with detached=1..8; inspect %err% 1>&2

echo.
echo ====================================================
echo CHECK 3 - RECOVERY exercised under eviction ^(INFO^)
echo ====================================================
findstr /C:"branch=RECOVER" "%err%" >NUL && echo RESULT 3: PASS -- recovery branch ran under eviction 1>&2 || echo RESULT 3: INFO -- no branch=RECOVER seen; confirm the recovery jumps reached cold frames 1>&2

echo.
echo ====================================================
echo SUMMARY ^(read the per-check RESULT lines above^):
echo   TINY LIVE PASS REQUIRES: P0/P1 PASS ^(build proven tiny^), 0A/0B/0C PASS, CHECK I PASS ^(identity^),
echo   CHECK 1a PASS and CHECK 2a PASS ^(both triggers fired^). 1b/2b are the stronger same-line
echo   non-vacuity checks; CHECK 3 is INFO. Do NOT require slot_count to return below 100 ^(hysteresis^).
echo ====================================================
pause
exit /b 0

:install_dll
set "src_dll=%~1"
set "dst_dir=%~2"
set "copy_label=%~3"
if not exist "%src_dll%" ( echo COPY FAIL -- %copy_label% source missing: "%src_dll%" 1>&2 & exit /b 1 )
if not exist "%dst_dir%\" ( echo COPY FAIL -- %copy_label% destination folder missing: "%dst_dir%" 1>&2 & exit /b 1 )
del /F "%dst_dir%\cnr3.dll" 2>NUL
COPY /Y /V "%src_dll%" "%dst_dir%\" >NUL
if errorlevel 1 ( echo COPY FAIL -- %copy_label% from "%src_dll%" to "%dst_dir%" 1>&2 & exit /b 1 )
if not exist "%dst_dir%\cnr3.dll" ( echo COPY FAIL -- %copy_label% did not produce "%dst_dir%\cnr3.dll" 1>&2 & exit /b 1 )
echo COPY PASS -- %copy_label% 1>&2
exit /b 0

:fatal_cd
echo FATAL: could not cd to "%vs_root%" 1>&2
pause
exit /b 1

:fatal_copy
echo FATAL: DLL install failed; harness did not run or did not complete. 1>&2
pause
exit /b 1
