@echo off
REM test_W2_hot_zone_observation_AB.bat
REM
REM DESIGNER-OWNED acceptance harness for W.2 (CMS07.14 section 7.6: live hot-zone observation at arInitial).
REM Run by the COORDINATOR. The coder delivers ONLY the cnr3_arInitial.cpp patch + the build_config marker;
REM this harness is the designer's deliverable (adapted from the golden test_000_Example_576p50.* + the K.1F
REM core-cache-defeat lesson).
REM
REM W.2 is OBSERVE-ONLY (nothing in the live path consumes hot-zone state until W.3), so the proof is:
REM   (1) BRANCH COVERAGE: the temporary KDT line  [KDT] instance=%d N=%d HOT-ZONE-OBSERVED status=%s
REM       fires with the right N on each of the four branches; and
REM   (2) OUTPUT NEUTRALITY: produced bytes are byte-identical to the pre-W.2 (W.1) build.
REM
REM TWO RUNS over the SAME single combined scenario ([0,50,2000], seg 60, -r 1, no shuffle):
REM   RUN A = pre-W.2 baseline DLL (the committed W.1 build, marker CMS07-W.1-checkpoint-retention-trigger)
REM   RUN B = W.2 build DLL       (marker CMS07-W.2-hot-zone-observation-arInitial)
REM Then: fc /b A.y4m B.y4m  must report NO differences (neutrality); and B.err must show the four
REM HOT-ZONE-OBSERVED lines correlated with their branch lines (coverage).
REM
REM REQUIRES: both DLLs built with CNR3_KEYSTONE_DEV_TRACE defined (so the temporary HOT-ZONE-OBSERVED KDT
REM and the predecessor/cache-hit/recover branch lines are emitted). The frame-0 correlation line is
REM ADDITIONALLY guarded on CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF (see CHECK 2a) -- optional, INFO-only.
REM (The standard four-way cache-core selftest -- still 54/54 -- is run SEPARATELY by the coordinator; a
REM DLL-only change must not move the count. It is not part of this live harness.)
REM
REM REVISION (post prior-designer source cross-check, independently re-verified against cnr3_arAllFramesReady.cpp):
REM   (1) frame-0 branch line is DOUBLE-guarded (line 34) -> CHECK 2a-branch downgraded to INFO (not gating);
REM   (2) recovery token is "branch=RECOVER" (lines 318/371), not "RECOVERY/recover/floor" -> CHECK 2d-branch
REM       grep corrected (findstr is case-sensitive without /I). The four HOT-ZONE-OBSERVED obs checks and the
REM       byte-neutrality check are unchanged (they were already correct).

set "rrr=-r 1"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_W2_hot_zone_observation_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"

REM FILL THESE: the two build folders to compare (Debug or Release; use the same config for both runs).
set "dll_folder_preW2=D:\TEST\dll_W1_Release"
set "dll_folder_W2=D:\TEST\dll_W2_Release"

set "a_y4m=%source_path%\W2_runA_preW2_temp.y4m"
set "a_err=%source_path%\W2_runA_preW2_temp_stderr.txt"
set "b_y4m=%source_path%\W2_runB_w2_temp.y4m"
set "b_err=%source_path%\W2_runB_w2_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"

REM Pre-delete outputs so a stale file cannot masquerade as a produced frame.
del /F "%a_y4m%" "%b_y4m%" "%a_err%" "%b_err%" 2>NUL

REM === RUN A: pre-W.2 (W.1) baseline DLL ===
echo Installing pre-W.2 (W.1 baseline) DLL from: %dll_folder_preW2%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%dll_folder_preW2%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll"
"%vspipe%" %rrr% --container y4m "%vpy%" "%a_y4m%"  2>>"%a_err%"
set "rc_a=%ERRORLEVEL%"
echo (RUN A preW2 exit=%rc_a%) >>"%a_err%"

REM === RUN B: W.2 DLL ===
echo Installing W.2 DLL from: %dll_folder_W2%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%dll_folder_W2%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll"
"%vspipe%" %rrr% --container y4m "%vpy%" "%b_y4m%"  2>>"%b_err%"
set "rc_b=%ERRORLEVEL%"
echo (RUN B W2 exit=%rc_b%) >>"%b_err%"

echo.
echo ====================================================
echo CHECK 0 - both runs completed
echo ====================================================
if not "%rc_a%"=="0" ( echo RESULT 0A: FAIL -- RUN A did not exit 0 1>&2 ) else ( echo RESULT 0A: PASS -- RUN A exited 0 1>&2 )
if not "%rc_b%"=="0" ( echo RESULT 0B: FAIL -- RUN B did not exit 0 1>&2 ) else ( echo RESULT 0B: PASS -- RUN B exited 0 1>&2 )

echo.
echo ====================================================
echo CHECK 1 - OUTPUT NEUTRALITY (observe-only: W.2 bytes == pre-W.2 bytes)
echo ====================================================
fc /b "%a_y4m%" "%b_y4m%" >NUL && echo RESULT 1: PASS -- W.2 output byte-identical to pre-W.2 (observation changed no frame) 1>&2 || echo RESULT 1: FAIL -- W.2 output differs from pre-W.2: observation is NOT observe-only 1>&2

echo.
echo ====================================================
echo CHECK 2 - BRANCH COVERAGE (HOT-ZONE-OBSERVED fired, right N, every branch) -- on the W.2 run (B)
echo ====================================================
REM 2.0 sanity: the temporary KDT is present at all (build has CNR3_KEYSTONE_DEV_TRACE).
findstr /C:"HOT-ZONE-OBSERVED" "%b_err%" >NUL && echo RESULT 2.0: PASS -- HOT-ZONE-OBSERVED lines present 1>&2 || echo RESULT 2.0: FAIL/INCONCLUSIVE -- no HOT-ZONE-OBSERVED line (KDT not built? wire-in absent?) 1>&2

REM 2a frame-0 fresh start: observation at N=0 (load-bearing) AND the frame-0 branch line at N=0 (INFO only).
REM VERIFIED (cnr3_arAllFramesReady.cpp:34): the FRAME0-FRESH-START line is DOUBLE-guarded on
REM CNR3_KEYSTONE_DEV_TRACE *AND* CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF -- unlike the other three branch
REM lines (single-guarded). So the frame-0 correlation line is INFORMATIONAL: absent unless the build also
REM defines the frame-0 proof macro, and that absence is NOT a failure -- N=0 -> frame-0 is unambiguous by
REM construction (the scenario starts at 0), and the load-bearing proof is 2a-obs (single-guarded, always on).
findstr /C:"N=0 HOT-ZONE-OBSERVED" "%b_err%" >NUL && echo RESULT 2a-obs: PASS -- observation fired at N=0 1>&2 || echo RESULT 2a-obs: FAIL -- no observation at N=0 1>&2
findstr /C:"N=0 FRAME0-FRESH-START" "%b_err%" >NUL && echo RESULT 2a-branch: INFO -- frame-0 branch line present 1>&2 || echo RESULT 2a-branch: INFO -- frame-0 branch line ABSENT, NOT a failure (double-guarded on CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF; define it for the correlation line). N=0 -^> frame-0 is unambiguous; 2a-obs is the proof. 1>&2

REM 2b predecessor-present: observation at N=1 AND the predecessor-present branch line at N=1.
findstr /C:"N=1 HOT-ZONE-OBSERVED" "%b_err%" >NUL && echo RESULT 2b-obs: PASS -- observation fired at N=1 1>&2 || echo RESULT 2b-obs: FAIL -- no observation at N=1 1>&2
findstr /C:"N=1 branch=PREDECESSOR-PRESENT-COMPUTE" "%b_err%" >NUL && echo RESULT 2b-branch: PASS -- N=1 took predecessor-present 1>&2 || echo RESULT 2b-branch: CHECK -- confirm the N=1 branch line text vs arAllFramesReady 1>&2

REM 2c cache-hit: observation at N=50 AND a CACHE-HIT branch line at N=50 (re-entry forced by SetVideoCache mode=0).
findstr /C:"N=50 HOT-ZONE-OBSERVED" "%b_err%" >NUL && echo RESULT 2c-obs: PASS -- observation fired at N=50 1>&2 || echo RESULT 2c-obs: FAIL -- no observation at N=50 1>&2
findstr /C:"N=50 branch=CACHE-HIT" "%b_err%" >NUL && echo RESULT 2c-branch: PASS -- N=50 took cache-hit 1>&2 || echo RESULT 2c-branch: CHECK -- confirm N=50 cache-hit; if absent, the VS core cache may have intercepted (verify SetVideoCache mode=0 took effect) 1>&2

REM 2d recovery: observation at N=2000 AND a recovery branch line at N=2000.
REM VERIFIED token (cnr3_arAllFramesReady.cpp:318/371): "branch=RECOVER" (uppercase; recover_branch=<sub>),
REM single-guarded on CNR3_KEYSTONE_DEV_TRACE. findstr is case-sensitive without /I, so match it exactly.
findstr /C:"N=2000 HOT-ZONE-OBSERVED" "%b_err%" >NUL && echo RESULT 2d-obs: PASS -- observation fired at N=2000 1>&2 || echo RESULT 2d-obs: FAIL -- no observation at N=2000 1>&2
findstr /C:"N=2000" "%b_err%" | findstr /C:"branch=RECOVER" >NUL && echo RESULT 2d-branch: PASS -- N=2000 took recovery (branch=RECOVER) 1>&2 || echo RESULT 2d-branch: FAIL -- N=2000 did not show branch=RECOVER 1>&2

echo.
echo ====================================================
echo SUMMARY (read the per-check RESULT lines above; this line is a label, not a verdict):
echo   0 both-ran / 1 byte-NEUTRALITY (must PASS) / 2a-2d four-branch HOT-ZONE-OBSERVED coverage (must PASS)
echo   W.2 PASS REQUIRES: RESULT 1 PASS (neutrality) AND 2a-obs/2b-obs/2c-obs/2d-obs PASS (observation fired
echo   on all four branches with the right N). The 2b/2c/2d-branch lines confirm which branch each N took
echo   (verified tokens). 2a-branch (frame-0) is INFO-only (double-guarded); its absence is not a failure.
echo ====================================================
pause
