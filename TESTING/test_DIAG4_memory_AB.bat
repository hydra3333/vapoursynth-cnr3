@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM =====================================================================================
REM DIAG.4 MEMORY (D-SUM-02) A/B HARNESS  (designer-owned, W3D)
REM
REM PURPOSE: prove D-SUM-02 memory diagnostics are OBSERVE-ONLY (R-PROCESS-19), capture the
REM          memory tables for the designer eyeball, and assert the DIAG.4-specific facts
REM          (post-cleanup clear=ok; D-SUM-04 balance==0 in the same log; periodic cadence).
REM
REM RULING 2 -- INTERNAL COMPARISON (critical):
REM   RUN A = D-SUM-02 OFF build    (CNR3_DIAG_COMPUTE/PRINT_DSUM02_MEMORY commented out)
REM   RUN B = D-SUM-02 ON  build    (D-SUM-02 enabled)
REM   BOTH builds are the DIAG.4 tree and BOTH contain the ungated teardown output_cache.clear().
REM   They must differ ONLY in the D-SUM-02 gate. Do NOT compare against a pre-DIAG.4 binary.
REM   For the CHECK 2h D-SUM-04 cross-check, the ON build must also have the cache-side families
REM   (incl. D-SUM-04) enabled -- i.e. a normal diagnostics build with D-SUM-02 the only toggle.
REM
REM PASS (per scenario):
REM   CHECK 1  fc /b A.y4m B.y4m == byte-identical            <- THE observe-only exit gate
REM   CHECK 0E/0F  A has NO [DSUM02-*]; B HAS it              <- correct builds staged
REM   CHECK 2a/2b  B has [DSUM02-SNAPSHOT] and [DSUM02-SUMMARY]
REM   CHECK 2c  B has the baseline snapshot ("at cnr3_create (baseline)")
REM   CHECK 2d  B post-cleanup shows "after cache clear (clear=ok)"  (non-ok => FAIL)
REM   CHECK 2e  periodic cadence: S1 none (<1000 frames); S7/S8 have frame=1000
REM   The B .err logs + pt_mem_*/d04_* captures go to the designer for the table eyeball
REM   (column alignment, Other + Static blocks, deltas) and the D-SUM-04 balance==0 read.
REM =====================================================================================

set "rrr=-r 1"

REM --- paths (match the S-series / AB layout; adjust if yours differ) ---
set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "golden_source=%source_path%\000_Example_576p50.mp4"

REM --- the two staged builds: pop the matching cnr3.dll into these BEFORE running ---
REM     dll_diag4_memOFF_Release : DIAG.4 tree, D-SUM-02 gates OFF (clear present)
REM     dll_diag4_memON_Release  : DIAG.4 tree, D-SUM-02 gates ON  (clear present)
set "dll_folder_OFF=%source_path%\dll_diag4_memOFF_Release"
set "dll_folder_ON=%source_path%\dll_diag4_memON_Release"

cd /D "%vs_root%"

REM --- preflight ---
set "preflight_fail=0"
if not exist "%vspipe%"            ( echo PREFLIGHT FAIL: vspipe not found: "%vspipe%" 1>&2 & set "preflight_fail=1" )
if not exist "%golden_source%"     ( echo PREFLIGHT FAIL: golden source not found: "%golden_source%" 1>&2 & set "preflight_fail=1" )
if not exist "%runtime_dll_folder%\" ( echo PREFLIGHT FAIL: runtime plugin folder not found: "%runtime_dll_folder%" 1>&2 & set "preflight_fail=1" )
if not exist "%dll_folder_OFF%\cnr3.dll" ( echo PREFLIGHT FAIL: memOFF DLL not found: "%dll_folder_OFF%\cnr3.dll" 1>&2 & set "preflight_fail=1" )
if not exist "%dll_folder_ON%\cnr3.dll"  ( echo PREFLIGHT FAIL: memON DLL not found: "%dll_folder_ON%\cnr3.dll" 1>&2 & set "preflight_fail=1" )
for %%S in (S1 S7 S8) do (
  if not exist "%vs_root%\test_000_Example_576p50_%%S.vpy" ( echo PREFLIGHT FAIL: vpy not found: test_000_Example_576p50_%%S.vpy 1>&2 & set "preflight_fail=1" )
)
if "%preflight_fail%"=="1" ( echo PREFLIGHT RESULT: FAIL -- fix the paths above before running. 1>&2 & goto :fatal )
echo PREFLIGHT RESULT: PASS -- required files/folders exist. 1>&2

for %%S in (S1 S7 S8) do call :run_ab_pair %%S

@echo on
findstr /C:"[DSUM02-" "%source_path%\run_mem_B_S1.txt" > "%source_path%\pt_mem_S1.txt"
findstr /C:"[DSUM02-" "%source_path%\run_mem_B_S8.txt" > "%source_path%\pt_mem_S8.txt"
findstr /C:"D-SUM-04" "%source_path%\run_mem_B_S8.txt" > "%source_path%\d04_S8.txt"
@echo off
echo. 1>&2
echo ATTACH THESE TO THE DESIGNER CHAT: 1>&2
echo   "%source_path%\pt_mem_S1.txt"   (baseline + pre/post-cleanup + summary; no periodic) 1>&2
echo   "%source_path%\pt_mem_S8.txt"   (adds periodic frame=1000/2000/3000) 1>&2
echo   "%source_path%\d04_S8.txt"      (D-SUM-04 balance -- must be zero) 1>&2
echo. 1>&2
echo ==================================================================== 1>&2
echo DIAG.4 MEMORY A/B COMPLETE 1>&2
echo   PASS REQUIRES per scenario: CHECK 1 byte-identical (observe-only), 1>&2
echo   0E/0F correct builds, 2a-2e memory capture + clear=ok + cadence. 1>&2
echo   Designer verifies pt_mem_* table alignment/deltas + d04_S8 balance==0. 1>&2
echo ==================================================================== 1>&2
pause
exit /b 0

REM =====================================================================================
:run_ab_pair
set "scen=%~1"
set "vpy=%vs_root%\test_000_Example_576p50_%scen%.vpy"
set "a_y4m=%source_path%\mem_%scen%_A_off.y4m"
set "b_y4m=%source_path%\mem_%scen%_B_on.y4m"
set "a_err=%source_path%\run_mem_A_%scen%.txt"
set "b_err=%source_path%\run_mem_B_%scen%.txt"

echo. 1>&2
echo #################### SCENARIO %scen% #################### 1>&2
del /F "%a_y4m%" "%b_y4m%" "%a_err%" "%b_err%" 2>NUL

REM --- RUN A: D-SUM-02 OFF ---
call :install_dll "%dll_folder_OFF%\cnr3.dll" "%runtime_dll_folder%" "%scen% RUN A memOFF" || goto :fatal_copy
"%vspipe%" %rrr% --container y4m "%vpy%" "%a_y4m%"  2>>"%a_err%"
set "rc_a=%ERRORLEVEL%"
echo (RUN A %scen% memOFF exit=%rc_a%) >>"%a_err%"

REM --- RUN B: D-SUM-02 ON ---
call :install_dll "%dll_folder_ON%\cnr3.dll" "%runtime_dll_folder%" "%scen% RUN B memON" || goto :fatal_copy
"%vspipe%" %rrr% --container y4m "%vpy%" "%b_y4m%"  2>>"%b_err%"
set "rc_b=%ERRORLEVEL%"
echo (RUN B %scen% memON exit=%rc_b%) >>"%b_err%"

REM --- CHECK 0: exit codes + outputs ---
if "%rc_a%"=="0" ( echo RESULT %scen% 0A: PASS -- RUN A exit 0 1>&2 ) else ( echo RESULT %scen% 0A: FAIL -- RUN A exit=%rc_a% 1>&2 )
if "%rc_b%"=="0" ( echo RESULT %scen% 0B: PASS -- RUN B exit 0 1>&2 ) else ( echo RESULT %scen% 0B: FAIL -- RUN B exit=%rc_b% 1>&2 )
if exist "%a_y4m%" ( echo RESULT %scen% 0C: PASS -- A output exists 1>&2 ) else ( echo RESULT %scen% 0C: FAIL -- A output missing 1>&2 )
if exist "%b_y4m%" ( echo RESULT %scen% 0D: PASS -- B output exists 1>&2 ) else ( echo RESULT %scen% 0D: FAIL -- B output missing 1>&2 )

REM --- CHECK 0K: build identity via the [DSUM02- tag (A must NOT have it; B MUST) ---
findstr /C:"[DSUM02-" "%a_err%" >NUL && ( echo RESULT %scen% 0E: FAIL -- A emitted [DSUM02-*]; wrong ^(ON^) DLL staged as A 1>&2 ) || ( echo RESULT %scen% 0E: PASS -- A has no [DSUM02-*], memOFF confirmed 1>&2 )
findstr /C:"[DSUM02-" "%b_err%" >NUL && ( echo RESULT %scen% 0F: PASS -- B emitted [DSUM02-*], memON confirmed 1>&2 ) || ( echo RESULT %scen% 0F: FAIL -- B has no [DSUM02-*]; memory not built/printing 1>&2 )

REM --- CHECK 1: THE observe-only exit gate: A.y4m == B.y4m byte-identical ---
if not exist "%a_y4m%" ( echo RESULT %scen% 1: FAIL -- cannot compare, A missing 1>&2 & goto :ab_mem2 )
if not exist "%b_y4m%" ( echo RESULT %scen% 1: FAIL -- cannot compare, B missing 1>&2 & goto :ab_mem2 )
fc /b "%a_y4m%" "%b_y4m%" >NUL && ( echo RESULT %scen% 1: PASS -- memON output byte-identical to memOFF ^(OBSERVE-ONLY^) 1>&2 ) || ( echo RESULT %scen% 1: FAIL -- output differs; D-SUM-02 perturbed a returned frame 1>&2 )

:ab_mem2
REM --- CHECK 2a/2b: memory blocks present in B ---
findstr /C:"[DSUM02-SNAPSHOT]" "%b_err%" >NUL && ( echo RESULT %scen% 2a: PASS -- SNAPSHOT block present 1>&2 ) || ( echo RESULT %scen% 2a: FAIL -- no [DSUM02-SNAPSHOT] 1>&2 )
findstr /C:"[DSUM02-SUMMARY]" "%b_err%" >NUL && ( echo RESULT %scen% 2b: PASS -- SUMMARY block present 1>&2 ) || ( echo RESULT %scen% 2b: FAIL -- no [DSUM02-SUMMARY] 1>&2 )

REM --- CHECK 2c: baseline snapshot present ---
findstr /C:"at cnr3_create (baseline)" "%b_err%" >NUL && ( echo RESULT %scen% 2c: PASS -- baseline snapshot present 1>&2 ) || ( echo RESULT %scen% 2c: FAIL -- no baseline snapshot 1>&2 )

REM --- CHECK 2d: post-cleanup clear=ok (non-ok => FAIL) ---
findstr /C:"after cache clear (clear=ok)" "%b_err%" >NUL && (
  echo RESULT %scen% 2d: PASS -- post-cleanup clear=ok 1>&2
) || (
  findstr /C:"after cache clear (clear=" "%b_err%" >NUL && ( echo RESULT %scen% 2d: FAIL -- post-cleanup clear returned NON-OK ^(pin survived teardown?^) 1>&2 ) || ( echo RESULT %scen% 2d: FAIL -- no post-cleanup snapshot at all 1>&2 )
)

REM --- CHECK 2e: periodic cadence (S1 none; S7/S8 frame=1000) ---
if /I "%scen%"=="S1" (
  findstr /C:"[DSUM02-SNAPSHOT]" "%b_err%" | findstr /C:"frame=1000" >NUL && ( echo RESULT %scen% 2e: CHECK -- S1 unexpectedly shows a periodic frame ^(<1000 frames expected^) 1>&2 ) || ( echo RESULT %scen% 2e: PASS -- S1 no periodic ^(baseline+cleanup+summary only^) 1>&2 )
) else (
  findstr /C:"[DSUM02-SNAPSHOT]" "%b_err%" | findstr /C:"frame=1000" >NUL && ( echo RESULT %scen% 2e: PASS -- periodic fired ^(frame=1000 present^) 1>&2 ) || ( echo RESULT %scen% 2e: INFO -- no frame=1000 in %scen% ^(scenario may not reach 1000; observe-only still proven by CHECK 1^) 1>&2 )
)

echo (SCENARIO %scen%: send %b_err% + pt_mem_*/d04_* to designer for table + balance verification) 1>&2
goto :eof

REM =====================================================================================
:install_dll
set "src_dll=%~1"
set "dst_dir=%~2"
set "copy_label=%~3"
if not exist "%src_dll%" ( echo COPY FAIL -- %copy_label% source missing: "%src_dll%" 1>&2 & exit /b 1 )
if not exist "%dst_dir%\" ( echo COPY FAIL -- %copy_label% dest folder missing: "%dst_dir%" 1>&2 & exit /b 1 )
del /F "%dst_dir%\cnr3.dll" 2>NUL
COPY /Y /V "%src_dll%" "%dst_dir%\" >NUL
if errorlevel 1 ( echo COPY FAIL -- %copy_label% 1>&2 & exit /b 1 )
if not exist "%dst_dir%\cnr3.dll" ( echo COPY FAIL -- %copy_label% produced no cnr3.dll 1>&2 & exit /b 1 )
echo COPY PASS -- %copy_label% 1>&2
goto :eof

:fatal_copy
echo FATAL: DLL install failed; aborting. 1>&2
pause
exit /b 1

:fatal
echo FATAL: preflight failed; aborting. 1>&2
pause
exit /b 1
