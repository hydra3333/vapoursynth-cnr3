@echo off
setlocal EnableExtensions
REM
REM DESIGNER-OWNED acceptance harness for W.3 ^(CMS07.14 section 7.5: combined live store-and-prune helper^).
REM Run by the COORDINATOR after the W.3 four-way cache-core selftest is PASS. This harness validates
REM LIVE getFrame behaviour under real frame flow -- the layer the cache-core selftest cannot reach --
REM and is the final gate before commit.
REM
REM RUN A = W.2 baseline DLL, non-evicting reference.
REM RUN B = W.3 DLL, evicting build under test.
REM
REM W.3 PASS REQUIRES:
REM   0A/0B PASS: both runs exit 0
REM   0C/0D PASS: both output files exist
REM   0E/0F PASS: DLL identity evidence holds ^(A has no W.3 KDT cap_trigger line; B has it^)
REM   1 PASS:     A.y4m and B.y4m are byte-identical
REM   2a/2b/2c PASS: capacity trigger, checkpoint trigger, and detached victims are observed in RUN B
REM
REM PROOF SHAPE ^(see the .vpy header for the full rationale and the trigger arithmetic^):
REM   Two runs over the SAME scenario ^(one long sequential segment that crosses BOTH triggers, plus two
REM   recovery segments^). -r 1, no shuffle.
REM     RUN A = W.2 baseline DLL ^(marker CMS07-W.2-hot-zone-observation-arInitial^) -- never evicts.
REM     RUN B = W.3 DLL          ^(marker CMS07-W.3-combined-live-store-prune-helper^) -- evicts.
REM   W.3 PASS REQUIRES ALL OF:
REM     CHECK 1  RETURNED-FRAME CORRECTNESS: fc /b A.y4m B.y4m == no differences
REM              ^(eviction removed only no-longer-needed slots; it corrupted no returned frame^)
REM     CHECK 2  EVICTION FIRED ^(RUN B KDT^): cap_trigger=1 present AND ckpt_trigger=1 present AND a
REM              detached=^<n^> with n^>0 present  ^(so CHECK 1 is NOT a vacuous identity^)
REM     CHECK 3  RECOVERY EXERCISED ^(RUN B KDT^): AS2 combined-helper kinds appear and recovery ran
REM     CHECK 0  both runs exit 0
REM   CHECK 2 is the one that gives CHECK 1 its teeth: identical bytes only prove correctness if eviction
REM   actually happened. If CHECK 2 fails, treat CHECK 1 as INCONCLUSIVE ^(the scenario did not cross a
REM   trigger -- lengthen CAPACITY_RUN_LENGTH in the .vpy^).
REM
REM NOTE ON RUNTIME/MEMORY: ^~1360 frames through CNR3 with mode=0 ^(a few minutes^). RUN A ^(W.2, no eviction^)
REM   holds the whole run in the output cache ^(^~1400 * ^~0.6 MB ^~= 0.9 GB^) -- well within a 32 GB machine.
REM
REM Notes:
REM   - The CNR3 DLL normally does not emit CNR3_EDIT_VERSION during live getFrame, so this harness uses
REM     the W.3-only KDT token "cap_trigger=" as runtime identity evidence.
REM   - CHECK 2d/2e are stronger same-line non-vacuity checks. They are CHECK-grade on first run:
REM     inspect RUN B KDT if either reports CHECK.
REM   - Use matching W.2/W.3 configurations where possible: Debug with Debug, or Release with Release.

set "rrr=-r 1"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_W3_combined_store_prune_eviction_AB_v07.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
set "golden_source=%source_path%\000_Example_576p50.mp4"

REM ---------------------------------------------------------------------------------------
REM Choose only ONE matching config Trio.

REM Debug Trio:
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"
set "dll_folder_W2=D:\TEST\dll_W2_Debug"
set "dll_folder_W3=D:\TEST\dll_W3_Debug"

REM Release Trio:
REM set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release"
REM set "dll_folder_W2=D:\TEST\dll_W2_Release"
REM set "dll_folder_W3=D:\TEST\dll_W3_Release"
REM ---------------------------------------------------------------------------------------

set "a_y4m=%source_path%\W3_runA_w2base_temp.y4m"
set "a_err=%source_path%\W3_runA_w2base_temp_stderr.txt"
set "b_y4m=%source_path%\W3_runB_w3_temp.y4m"
set "b_err=%source_path%\W3_runB_w3_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%" || goto :fatal_cd

echo.
echo ====================================================
echo PREFLIGHT - paths and DLLs
echo ====================================================
set "preflight_fail=0"
if not exist "%vspipe%" (
  echo PREFLIGHT FAIL: vspipe not found: "%vspipe%" 1>&2
  set "preflight_fail=1"
)
if not exist "%vpy%" (
  echo PREFLIGHT FAIL: vpy not found: "%vpy%" 1>&2
  set "preflight_fail=1"
)
if not exist "%golden_source%" (
  echo PREFLIGHT FAIL: golden source not found: "%golden_source%" 1>&2
  set "preflight_fail=1"
)
if not exist "%runtime_dll_folder%\" (
  echo PREFLIGHT FAIL: runtime DLL folder not found: "%runtime_dll_folder%" 1>&2
  set "preflight_fail=1"
)
if not exist "%built_dll_folder%\cnr3.dll" (
  echo PREFLIGHT FAIL: built W.3 DLL not found: "%built_dll_folder%\cnr3.dll" 1>&2
  set "preflight_fail=1"
)
if not exist "%dll_folder_W2%\cnr3.dll" (
  echo PREFLIGHT FAIL: W.2 DLL not found: "%dll_folder_W2%\cnr3.dll" 1>&2
  set "preflight_fail=1"
)
if not exist "%dll_folder_W3%\" (
  echo PREFLIGHT FAIL: W.3 staging folder not found: "%dll_folder_W3%" 1>&2
  set "preflight_fail=1"
)
if not "%preflight_fail%"=="0" (
  echo PREFLIGHT RESULT: FAIL -- fix the paths above before running the harness. 1>&2
  pause
  exit /b 2
)
echo PREFLIGHT RESULT: PASS -- required files/folders exist. 1>&2

echo.
echo COPYING THE ACTUAL built W.3 cnr3.dll to the staging area from: %built_dll_folder%
call :install_dll "%built_dll_folder%\cnr3.dll" "%dll_folder_W3%" "W.3 staging copy" || goto :fatal_copy

REM Pre-delete outputs so a stale file cannot masquerade as a produced frame.
del /F "%a_y4m%" "%b_y4m%" "%a_err%" "%b_err%" 2>NUL

REM === RUN A: W.2 baseline DLL ^(non-evicting reference^) ===
echo.
echo === RUN A: W.2 baseline DLL ===
echo Installing W.2 baseline DLL from: %dll_folder_W2%
call :install_dll "%dll_folder_W2%\cnr3.dll" "%runtime_dll_folder%" "RUN A W.2 runtime install" || goto :fatal_copy
dir /tw "%runtime_dll_folder%\cnr3.dll"
echo "%vspipe%" %rrr% --container y4m "%vpy%" "%a_y4m%"
"%vspipe%" %rrr% --container y4m "%vpy%" "%a_y4m%"  2>>"%a_err%"
set "rc_a=%ERRORLEVEL%"
echo ^(RUN A W2-baseline exit=%rc_a%^) >>"%a_err%"
echo ^(RUN A W2-baseline exit=%rc_a%^)

REM === RUN B: W.3 DLL ^(evicts^) ===
echo.
echo === RUN B: W.3 DLL ===
echo Installing W.3 DLL from: %dll_folder_W3%
call :install_dll "%dll_folder_W3%\cnr3.dll" "%runtime_dll_folder%" "RUN B W.3 runtime install" || goto :fatal_copy
dir /tw "%runtime_dll_folder%\cnr3.dll"
echo "%vspipe%" %rrr% --container y4m "%vpy%" "%b_y4m%"
"%vspipe%" %rrr% --container y4m "%vpy%" "%b_y4m%"  2>>"%b_err%"
set "rc_b=%ERRORLEVEL%"
echo ^(RUN B W3 exit=%rc_b%^) >>"%b_err%"
echo ^(RUN B W3 exit=%rc_b%^)

echo.
echo ====================================================
echo CHECK 0 - both runs completed
echo ====================================================
if not "%rc_a%"=="0" ( echo RESULT 0A: FAIL -- RUN A ^(W.2 baseline^) did not exit 0 1>&2 ) else ( echo RESULT 0A: PASS -- RUN A exited 0 1>&2 )
if not "%rc_b%"=="0" ( echo RESULT 0B: FAIL -- RUN B ^(W.3^) did not exit 0 1>&2 ) else ( echo RESULT 0B: PASS -- RUN B exited 0 1>&2 )

echo.
echo ====================================================
echo CHECK 0Y - output files exist
echo ====================================================
if exist "%a_y4m%" ( echo RESULT 0C: PASS -- RUN A output file exists 1>&2 ) else ( echo RESULT 0C: FAIL -- RUN A output file missing 1>&2 )
if exist "%b_y4m%" ( echo RESULT 0D: PASS -- RUN B output file exists 1>&2 ) else ( echo RESULT 0D: FAIL -- RUN B output file missing 1>&2 )

echo.
echo ====================================================
echo CHECK 0K - runtime DLL identity evidence
echo ====================================================
REM CNR3_EDIT_VERSION is emitted by the selftest binary, not necessarily by the live DLL during getFrame.
REM Use the W.3-only KDT store-prune token: RUN A must not emit cap_trigger=; RUN B must emit it.
findstr /C:"cap_trigger=" "%a_err%" >NUL && echo RESULT 0E: FAIL -- RUN A emitted W.3-only cap_trigger KDT; wrong baseline DLL likely loaded 1>&2 || echo RESULT 0E: PASS -- RUN A did not emit W.3 KDT, consistent with W.2 baseline 1>&2
findstr /C:"cap_trigger=" "%b_err%" >NUL && echo RESULT 0F: PASS -- RUN B emitted W.3 KDT, W.3 DLL confirmed 1>&2 || echo RESULT 0F: FAIL -- RUN B did not emit W.3 KDT; W.3 DLL not loaded or KDT not built 1>&2

echo.
echo ====================================================
echo CHECK 1 - RETURNED-FRAME CORRECTNESS ^(W.3 evicting output == W.2 non-evicting output^)
echo ====================================================
if not exist "%a_y4m%" goto :check1_missing_a
if not exist "%b_y4m%" goto :check1_missing_b
fc /b "%a_y4m%" "%b_y4m%" >NUL && echo RESULT 1: PASS -- W.3 returned frames byte-identical to W.2 ^(eviction corrupted no returned frame^) 1>&2 || echo RESULT 1: FAIL -- W.3 output differs from W.2: eviction removed a still-needed slot or corrupted a returned frame 1>&2
goto :after_check1
:check1_missing_a
echo RESULT 1: FAIL -- cannot compare: RUN A output missing 1>&2
goto :after_check1
:check1_missing_b
echo RESULT 1: FAIL -- cannot compare: RUN B output missing 1>&2
:after_check1

echo.
echo ====================================================
echo CHECK 2 - EVICTION OBSERVABLY FIRED ^(RUN B^) -- gives CHECK 1 its teeth
echo ====================================================
REM 2.0 sanity: the W.3 KDT line is present at all ^(build has CNR3_KEYSTONE_DEV_TRACE, helper wired^).
findstr /C:"cap_trigger=" "%b_err%" >NUL && echo RESULT 2.0: PASS -- W.3 [KDT] store-prune lines present 1>&2 || echo RESULT 2.0: FAIL/INCONCLUSIVE -- no W.3 [KDT] line ^(KDT not built? helper not wired?^) 1>&2

REM 2a CAPACITY trigger fired at least once.
findstr /C:"cap_trigger=1" "%b_err%" >NUL && echo RESULT 2a: PASS -- capacity trigger fired ^(cap_trigger=1^) 1>&2 || echo RESULT 2a: FAIL -- capacity trigger never fired; lengthen CAPACITY_RUN_LENGTH past your overflow_trigger 1>&2

REM 2b CHECKPOINT trigger fired at least once.
findstr /C:"ckpt_trigger=1" "%b_err%" >NUL && echo RESULT 2b: PASS -- checkpoint trigger fired ^(ckpt_trigger=1^) 1>&2 || echo RESULT 2b: FAIL -- checkpoint trigger never fired; ensure the run exceeds 48 checkpoints ^(^>= about 490 frames^) 1>&2

REM 2c at least one prune actually DETACHED victims ^(detached=^<n^>, n^>0^).
findstr /R /C:"detached=[1-8]" "%b_err%" >NUL && echo RESULT 2c: PASS -- at least one prune detached victims ^(detached^>0^) 1>&2 || echo RESULT 2c: FAIL -- no prune detached any victim; eviction did not actually remove a slot 1>&2

REM 2d/2e stronger non-vacuity checks: require each trigger to appear on a line that also detached victims.
findstr /C:"cap_trigger=1" "%b_err%" | findstr /R /C:"detached=[1-8]" >NUL && echo RESULT 2d: PASS -- capacity-triggered prune detached victims 1>&2 || echo RESULT 2d: CHECK -- cap_trigger=1 was seen, but no cap-trigger line with detached^>0 was found 1>&2
findstr /C:"ckpt_trigger=1" "%b_err%" | findstr /R /C:"detached=[1-8]" >NUL && echo RESULT 2e: PASS -- checkpoint-triggered prune detached victims 1>&2 || echo RESULT 2e: CHECK -- ckpt_trigger=1 was seen, but no checkpoint-trigger line with detached^>0 was found 1>&2

echo.
echo ====================================================
echo CHECK 3 - RECOVERY EXERCISED UNDER EVICTION ^(RUN B^) + combined-helper kind coverage
echo ====================================================
REM 3a recovery branch ran.
findstr /C:"branch=RECOVER" "%b_err%" >NUL && echo RESULT 3a: PASS -- recovery branch ran 1>&2 || echo RESULT 3a: CHECK -- no branch=RECOVER seen; confirm the recovery jumps reached cold frames 1>&2

REM 3b AS2 combined-helper kinds appear ^(floor / hole^). INFO-grade unless Part 2 requires deterministic AS2.
findstr /C:"kind=as2_consumer" "%b_err%" >NUL && echo RESULT 3b: PASS -- AS2 combined-helper kind^(s^) exercised 1>&2 || echo RESULT 3b: INFO -- no as2_consumer kind seen; recovery may have taken production-fresh-start only. Inspect %b_err% kind= fields 1>&2

REM 3c production kind appears ^(sequential + frame-0 + target stores^).
findstr /C:"kind=production" "%b_err%" >NUL && echo RESULT 3c: PASS -- production combined-helper kind exercised 1>&2 || echo RESULT 3c: FAIL -- no production kind seen; sequential run did not route through helper 1>&2

echo.
echo ====================================================
echo SUMMARY ^(read the per-check RESULT lines above; this line is a label, not a verdict^):
echo   W.3 PASS REQUIRES: 0A/0B PASS, 0C/0D PASS, 0E/0F PASS, CHECK 1 PASS,
echo   AND CHECK 2a/2b/2c PASS ^(capacity + checkpoint triggers fired and victims were detached^).
echo   CHECK 1 without CHECK 2 is INCONCLUSIVE ^(a vacuous identity^).
echo   CHECK 2d/2e are stronger same-line non-vacuity checks: inspect KDT before accepting the gate if either is CHECK.
echo   CHECK 3a/3c PASS expected; CHECK 3b is INFO-grade combined-helper-kind coverage.
echo ====================================================
pause
exit /b 0

:install_dll
echo Entered :install_dll '%1' '%2' '%3'  '%4'
set "src_dll=%~1"
set "dst_dir=%~2"
set "copy_label=%~3"
if not exist "%src_dll%" (
  echo COPY FAIL -- %copy_label% source missing: "%src_dll%" 1>&2
  echo COPY FAIL -- %copy_label% source missing: "%src_dll%"
  exit /b 1
)
if not exist "%dst_dir%\" (
  echo COPY FAIL -- %copy_label% destination folder missing: "%dst_dir%" 1>&2
  echo COPY FAIL -- %copy_label% destination folder missing: "%dst_dir%"
  exit /b 1
)
del /F "%dst_dir%\cnr3.dll" 2>NUL
COPY /Y /V "%src_dll%" "%dst_dir%\" >NUL
if errorlevel 1 (
  echo COPY FAIL -- %copy_label% from "%src_dll%" to "%dst_dir%" 1>&2
  echo COPY FAIL -- %copy_label% from "%src_dll%" to "%dst_dir%"
  exit /b 1
)
if not exist "%dst_dir%\cnr3.dll" (
  echo COPY FAIL -- %copy_label% did not produce "%dst_dir%\cnr3.dll" 1>&2
  echo COPY FAIL -- %copy_label% did not produce "%dst_dir%\cnr3.dll"
  exit /b 1
)
echo COPY PASS -- %copy_label% 1>&2
echo COPY PASS -- %copy_label%
echo Exiting :install_dll '%1' '%2' '%3' '%4'
exit /b 0

:fatal_cd
echo FATAL: could not cd to "%vs_root%" 1>&2
pause
exit /b 1

:fatal_copy
echo FATAL: DLL install failed; harness did not run or did not complete. 1>&2
pause
exit /b 1
