@echo off
REM test_D2_once_only_harness_AB.bat
REM
REM D.2 acceptance harness. Proves LIVE BRANCH-(d) exact-anchor MULTI-hole recovery (k=2) + the
REM bounded-window REFUSAL (no in-window anchor). Synthetic D.2 chain (distinct from D.1/K.1E.3):
REM   source[0] Y128 U72  V184 -> output[0]=source[0]      (fresh-start anchor at N-3)
REM   source[1] Y128 U208 V40  -> output[1]=Y128 U148 V96  (hole-1, filled)
REM   source[2] Y128 U176 V72  -> output[2]=Y128 U149 V95  (hole-2, filled)
REM   source[3] Y128 U128 V144 -> output[3]=Y128 U148 V100 (TARGET = D.2 GOLDEN)
REM
REM RUNS:
REM   RUN A  mode=recovery,   --start 0 --end 1 -> positions 0 then 3 (frames 1,2 SKIPPED). THE PROOF:
REM          branch=RECOVER anchor=0 hole_count=2 holes=[1,2] source_requests=[1,2,3] both computed,
REM          pin_list_size=3 pin_balance=0; recovered output[3]=148/100.
REM   RUN B  mode=holebytes,  --start 0 --end 3 -> positions 0,3 (recover, fills holes 1,2 into cache),
REM          then positions 1,2 re-requested -> CACHE-HIT returns filled holes 148/96 and 149/95.
REM   RUN C  mode=refusal,    --start 0 --end 1 -> positions 0 then 52 (frames 1..51 SKIPPED).
REM          nearest anchor=0 is 52 back > B=50 -> clean REFUSAL reason=no-in-window-anchor.
REM   RUN D  mode=d1regress,  --start 0 --end 1 -> D.1 single-hole (k==1) on the D.1 chain -> 147/109.
REM   RUN E  mode=processing, --start 0 --end 3 -> 0,1,2,3 first/uncached. NEGATIVE control: frame 3
REM          has output[2] present -> PREDECESSOR-PRESENT, NOT recovery.
REM   RUN F  mode=passthrough,--start 0 --end 3 -> source bypass (no [KDT]).
REM
REM CORE-CACHE DEFEAT: the .vpy applies std.SetVideoCache(CNR3_node, mode=0).
REM MANDATORY: bytes-match ALONE is not a pass. Recovery MUST show branch=RECOVER hole_count=2; the
REM refusal MUST show REFUSED branch=no-in-window-anchor. The bounded-window-exceeded nature is proven
REM BY CONSTRUCTION (RUN C establishes output[0] then requests 52), NOT by a code-emitted label.

set "rrr=-r 1"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_D2_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
REM Flip to x64\Release to prove the Release DLL:
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

set "checker=%vs_root%\test_K1F_check_y4m_constant_plane.py"

REM Output files
set "a_y4m=%source_path%\D2_runA_recovery_temp.y4m"
set "a_err=%source_path%\D2_runA_recovery_temp_stderr.txt"
set "b_y4m=%source_path%\D2_runB_holebytes_temp.y4m"
set "b_err=%source_path%\D2_runB_holebytes_temp_stderr.txt"
set "c_y4m=%source_path%\D2_runC_refusal_temp.y4m"
set "c_err=%source_path%\D2_runC_refusal_temp_stderr.txt"
set "d_y4m=%source_path%\D2_runD_d1regress_temp.y4m"
set "d_err=%source_path%\D2_runD_d1regress_temp_stderr.txt"
set "e_y4m=%source_path%\D2_runE_processing_temp.y4m"
set "e_err=%source_path%\D2_runE_processing_temp_stderr.txt"
set "f_y4m=%source_path%\D2_runF_passthrough_temp.y4m"
set "f_err=%source_path%\D2_runF_passthrough_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"

echo Installing built DLL from: %built_dll_folder%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll"

del /F "%a_y4m%" "%b_y4m%" "%c_y4m%" "%d_y4m%" "%e_y4m%" "%f_y4m%" 2>NUL
del /F "%a_err%" "%b_err%" "%c_err%" "%d_err%" "%e_err%" "%f_err%" 2>NUL

REM === RUN A: multi-hole recovery (THE PROOF) -> positions 0,3 ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="recovery" --container y4m "%vpy%" "%a_y4m%"  2>>"%a_err%"
set "rc_a=%ERRORLEVEL%"
echo (RUN A recovery exit=%rc_a%) >>"%a_err%"

REM === RUN B: hole bytes via cache-hit -> positions 0,3,1,2 ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="holebytes" --container y4m "%vpy%" "%b_y4m%"  2>>"%b_err%"
set "rc_b=%ERRORLEVEL%"
echo (RUN B holebytes exit=%rc_b%) >>"%b_err%"

REM === RUN C: bounded-window refusal -> positions 0,52 (EXPECTED to error/refuse) ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="refusal" --container y4m "%vpy%" "%c_y4m%"  2>>"%c_err%"
set "rc_c=%ERRORLEVEL%"
echo (RUN C refusal exit=%rc_c%) >>"%c_err%"

REM === RUN D: D.1 k==1 regression -> positions 0,2 on the D.1 chain ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="d1regress" --container y4m "%vpy%" "%d_y4m%"  2>>"%d_err%"
set "rc_d=%ERRORLEVEL%"
echo (RUN D d1regress exit=%rc_d%) >>"%d_err%"

REM === RUN E: negative control -> 0,1,2,3 sequential ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="processing" --container y4m "%vpy%" "%e_y4m%"  2>>"%e_err%"
set "rc_e=%ERRORLEVEL%"
echo (RUN E processing exit=%rc_e%) >>"%e_err%"

REM === RUN F: passthrough ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="passthrough" --container y4m "%vpy%" "%f_y4m%"  2>>"%f_err%"
set "rc_f=%ERRORLEVEL%"
echo (RUN F passthrough exit=%rc_f%) >>"%f_err%"

echo.
echo ====================================================
echo CHECK A - multi-hole RECOVERY on frame 3 (THE D.2 PROOF)
echo ====================================================
echo RUN A recovery exit code = %rc_a% 1>&2
if not "%rc_a%"=="0" ( echo RESULT A0: FAIL -- RUN A did not exit 0 1>&2 ) else ( echo RESULT A0: PASS -- RUN A exited 0 1>&2 )
findstr /C:"branch=RECOVER" "%a_err%" >NUL && echo RESULT A1: PASS -- branch=RECOVER present 1>&2 || echo RESULT A1: FAIL/INCONCLUSIVE -- no RECOVER KDT; NOT a D.2 pass 1>&2
findstr /C:"recover_branch=exact-anchor" "%a_err%" | findstr /C:"anchor=0" /C:"hole_count=2" >NUL && echo RESULT A2: PASS -- exact-anchor anchor=0 hole_count=2 1>&2 || echo RESULT A2: CHECK -- inspect anchor/hole_count 1>&2
findstr /C:"holes=[1,2]" "%a_err%" >NUL && echo RESULT A3: PASS -- holes=[1,2] 1>&2 || echo RESULT A3: CHECK -- inspect holes list 1>&2
findstr /C:"source_requests=[1,2,3]" "%a_err%" >NUL && echo RESULT A4: PASS -- source_requests=[1,2,3] 1>&2 || echo RESULT A4: CHECK -- inspect source_requests 1>&2
findstr /C:"hole=1 outcome=computed" "%a_err%" >NUL && echo RESULT A5: PASS -- hole 1 computed 1>&2 || echo RESULT A5: CHECK -- inspect hole 1 outcome 1>&2
findstr /C:"hole=2 outcome=computed" "%a_err%" >NUL && echo RESULT A6: PASS -- hole 2 computed 1>&2 || echo RESULT A6: CHECK -- inspect hole 2 outcome 1>&2
findstr /C:"pin_list_size=3" "%a_err%" >NUL && echo RESULT A7: PASS -- pin_list_size=3 (anchor + 2 holes) 1>&2 || echo RESULT A7: CHECK -- inspect pin_list_size 1>&2
findstr /C:"branch=RECOVER" "%a_err%" | findstr /C:"pin_balance=0" >NUL && echo RESULT A8: PASS -- pin_balance=0 1>&2 || echo RESULT A8: CHECK -- inspect pin_balance 1>&2
findstr /C:"N=0 FRAME0-FRESH-START" "%a_err%" >NUL && echo RESULT A9: PASS -- FRAME0-FRESH-START present (anchor established) 1>&2 || echo RESULT A9: FAIL/INCONCLUSIVE -- anchor frame 0 not established 1>&2
REM recovered frame 3 lands at output index 1 (positions rendered: 0,3)
"%python_exe%" "%checker%" "%a_y4m%" 128 148 100 1 1>&2

echo.
echo ====================================================
echo CHECK B - FILLED HOLE BYTES via cache-hit (the robust correctness proof)
echo ====================================================
echo RUN B holebytes exit code = %rc_b% 1>&2
if not "%rc_b%"=="0" ( echo RESULT B0: FAIL -- RUN B did not exit 0 1>&2 ) else ( echo RESULT B0: PASS -- RUN B exited 0 1>&2 )
findstr /C:"branch=CACHE-HIT" "%b_err%" >NUL && echo RESULT B1: PASS -- cache-hit(s) present for re-requested holes 1>&2 || echo RESULT B1: FAIL/INCONCLUSIVE -- no CACHE-HIT KDT 1>&2
REM rendered positions: 0,3,1,2  -> output indices 2=hole1(filled output[1]), 3=hole2(filled output[2])
"%python_exe%" "%checker%" "%b_y4m%" 128 148 96 2 1>&2
"%python_exe%" "%checker%" "%b_y4m%" 128 149 95 3 1>&2

echo.
echo ====================================================
echo CHECK C - bounded-window REFUSAL (THE NEW D.2 PROOF)
echo ====================================================
echo RUN C refusal exit code = %rc_c% 1>&2
findstr /C:"REFUSED branch=no-in-window-anchor" "%c_err%" >NUL && echo RESULT C1: PASS -- clean refusal reason=no-in-window-anchor 1>&2 || echo RESULT C1: FAIL/INCONCLUSIVE -- expected REFUSED branch=no-in-window-anchor 1>&2
findstr /C:"N=0 FRAME0-FRESH-START" "%c_err%" >NUL && echo RESULT C2: PASS -- output[0] established (so anchor exists, just out of window -> bounded-window case by construction) 1>&2 || echo RESULT C2: CHECK -- frame 0 fresh-start not seen 1>&2
findstr /C:"branch=RECOVER" "%c_err%" >NUL && echo RESULT C3: FAIL -- recovery fired when it should have refused 1>&2 || echo RESULT C3: PASS -- no recovery fired (correctly refused) 1>&2
if not exist "%c_y4m%" ( echo RESULT C4: PASS -- no partial output frame produced 1>&2 ) else ( echo RESULT C4: CHECK -- an output file was produced; inspect whether any frame was emitted 1>&2 )

echo.
echo ====================================================
echo CHECK D - D.1 single-hole (k==1) regression (accept gate changed)
echo ====================================================
echo RUN D d1regress exit code = %rc_d% 1>&2
if not "%rc_d%"=="0" ( echo RESULT D0: FAIL -- RUN D did not exit 0 1>&2 ) else ( echo RESULT D0: PASS -- RUN D exited 0 1>&2 )
findstr /C:"branch=RECOVER" "%d_err%" | findstr /C:"anchor=0" /C:"hole_count=1" >NUL && echo RESULT D1: PASS -- D.1 recovery anchor=0 hole_count=1 1>&2 || echo RESULT D1: CHECK -- inspect D.1 recovery KDT 1>&2
findstr /C:"holes=[1]" "%d_err%" >NUL && echo RESULT D2: PASS -- holes=[1] 1>&2 || echo RESULT D2: CHECK -- inspect holes 1>&2
"%python_exe%" "%checker%" "%d_y4m%" 128 147 109 1 1>&2

echo.
echo ====================================================
echo CHECK E - negative control (predecessor-present, NOT recovery)
echo ====================================================
echo RUN E processing exit code = %rc_e% 1>&2
if not "%rc_e%"=="0" ( echo RESULT E0: FAIL -- RUN E did not exit 0 1>&2 ) else ( echo RESULT E0: PASS -- RUN E exited 0 1>&2 )
findstr /C:"N=3 branch=PREDECESSOR-PRESENT-COMPUTE" "%e_err%" >NUL && echo RESULT E1: PASS -- frame 3 took predecessor-present 1>&2 || echo RESULT E1: CHECK -- frame 3 branch 1>&2
findstr /C:"branch=RECOVER" "%e_err%" >NUL && echo RESULT E2: FAIL -- NEGATIVE control violated: recovery fired with predecessor present 1>&2 || echo RESULT E2: PASS -- no recovery when predecessor present 1>&2
"%python_exe%" "%checker%" "%e_y4m%" 128 148 96 1 1>&2
"%python_exe%" "%checker%" "%e_y4m%" 128 149 95 2 1>&2
"%python_exe%" "%checker%" "%e_y4m%" 128 148 100 3 1>&2

echo.
echo ====================================================
echo CHECK F - passthrough negative (no [KDT])
echo ====================================================
echo RUN F passthrough exit code = %rc_f% 1>&2
findstr /C:"[KDT]" "%f_err%" >NUL && echo RESULT F: FAIL -- [KDT] present in passthrough bypass 1>&2 || echo RESULT F: PASS -- no [KDT] in passthrough bypass 1>&2

echo.
echo ====================================================
echo SUMMARY (read per-check RESULT lines; this is a label, not a verdict):
echo   A multi-hole RECOVER (THE PROOF) / B filled-hole bytes / C bounded-window REFUSAL (NEW) /
echo   D D.1 k==1 regression / E negative control / F passthrough-clean
echo   D.2 PASS REQUIRES: A1-A9 + recovered 148/100; B filled holes 148/96 and 149/95;
echo   C REFUSED branch=no-in-window-anchor (no RECOVER, no partial); D 147/109; E predecessor-present
echo   (no RECOVER); F clean. bytes WITHOUT the matching KDT line = INCONCLUSIVE.
echo ====================================================
pause
