@echo off
REM test_D3_once_only_harness_AB.bat
REM
REM D.3 acceptance harness. Proves LIVE BRANCH-(d) FLOOR-FRESH-START recovery. When the bounded
REM descending search finds NO in-window anchor, recovery fresh-starts the floor max(0,N-B) then walks
REM forward. Synthetic D.3 chain (distinct from D.1/D.2):
REM   source[0] Y128 U56  V176 -> output[0]=source[0]       (FLOOR fresh-start, chroma unchanged)
REM   source[1] Y128 U200 V48  -> output[1]=Y128 U144 V111  (hole-1, filled)
REM   source[2] Y128 U168 V80  -> output[2]=Y128 U145 V109  (hole-2, filled)
REM   source[3] Y128 U120 V152 -> output[3]=Y128 U144 V113  (TARGET = D.3 GOLDEN)
REM
REM RUNS:
REM   RUN A  mode=floorstart, --start 0 --end 0 -> SINGLE cold output[3]. THE PROOF:
REM          branch=RECOVER recover_branch=floor-fresh-start floor=0 floor_outcome=computed
REM          hole_count=2 holes=[1,2] source_requests=[0,1,2,3] both holes computed pin_list_size=3
REM          pin_balance=0; recovered output[3]=144/113.
REM   RUN B  mode=floorbytes, --start 0 --end 3 -> floor-start then cache-hit 0,1,2 -> floor 56/176
REM          (fresh-start proof), holes 144/111 and 145/109.
REM   RUN C  mode=d2regress,  --start 0 --end 1 -> D.2 exact-anchor k==2: branch=RECOVER
REM          recover_branch=exact-anchor (NOT floor-fresh-start), output[3]=148/100.
REM   RUN D  mode=d1regress,  --start 0 --end 1 -> D.1 single-hole k==1: output[2]=147/109.
REM   RUN E  mode=processing, --start 0 --end 3 -> 0,1,2,3 sequential. NEGATIVE control: frame 3
REM          PREDECESSOR-PRESENT, not recovery.
REM   RUN G  mode=passthrough,--start 0 --end 3 -> source bypass (no [KDT]).
REM
REM (RUN F structural-refusal is a design-note/boundary check only; not forced with test-only hooks,
REM  per coder review. After D.3 the bounded-window case floor-starts; genuine refusal is structural.)
REM
REM CORE-CACHE DEFEAT: the .vpy applies std.SetVideoCache(CNR3_node, mode=0).
REM MANDATORY: bytes-match ALONE is not a pass. Floor-start MUST show recover_branch=floor-fresh-start
REM floor=0 floor_outcome=computed.

set "rrr=-r 1"

set "source_path=D:\TEST"
set "vs_root=%source_path%\Vapoursynth_x64_R76"
set "vpy=%vs_root%\test_D3_once_only_harness_AB.vpy"
set "vspipe=%vs_root%\lib\site-packages\vapoursynth\vspipe.exe"
set "python_exe=%vs_root%\python.exe"
set "runtime_dll_folder=%vs_root%\Lib\site-packages\vapoursynth\plugins"
REM Flip to x64\Release to prove the Release DLL:
set "built_dll_folder=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug"

set "checker=%vs_root%\test_K1F_check_y4m_constant_plane.py"

set "a_y4m=%source_path%\D3_runA_floorstart_temp.y4m"
set "a_err=%source_path%\D3_runA_floorstart_temp_stderr.txt"
set "b_y4m=%source_path%\D3_runB_floorbytes_temp.y4m"
set "b_err=%source_path%\D3_runB_floorbytes_temp_stderr.txt"
set "c_y4m=%source_path%\D3_runC_d2regress_temp.y4m"
set "c_err=%source_path%\D3_runC_d2regress_temp_stderr.txt"
set "d_y4m=%source_path%\D3_runD_d1regress_temp.y4m"
set "d_err=%source_path%\D3_runD_d1regress_temp_stderr.txt"
set "e_y4m=%source_path%\D3_runE_processing_temp.y4m"
set "e_err=%source_path%\D3_runE_processing_temp_stderr.txt"
set "g_y4m=%source_path%\D3_runG_passthrough_temp.y4m"
set "g_err=%source_path%\D3_runG_passthrough_temp_stderr.txt"

echo.
echo CD /D "%vs_root%"
CD /D "%vs_root%"

echo Installing built DLL from: %built_dll_folder%
del /F "%runtime_dll_folder%\cnr3.dll" 2>NUL
COPY /Y /V "%built_dll_folder%\cnr3.dll" "%runtime_dll_folder%\"
dir /tw "%runtime_dll_folder%\cnr3.dll"

del /F "%a_y4m%" "%b_y4m%" "%c_y4m%" "%d_y4m%" "%e_y4m%" "%g_y4m%" 2>NUL
del /F "%a_err%" "%b_err%" "%c_err%" "%d_err%" "%e_err%" "%g_err%" 2>NUL

REM === RUN A: floor-fresh-start (THE PROOF) -> single cold output[3] ===
"%vspipe%" %rrr% --start 0 --end 0 --arg mode="floorstart" --container y4m "%vpy%" "%a_y4m%"  2>>"%a_err%"
set "rc_a=%ERRORLEVEL%"
echo (RUN A floorstart exit=%rc_a%) >>"%a_err%"

REM === RUN B: floor + hole bytes via cache-hit -> 3,0,1,2 ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="floorbytes" --container y4m "%vpy%" "%b_y4m%"  2>>"%b_err%"
set "rc_b=%ERRORLEVEL%"
echo (RUN B floorbytes exit=%rc_b%) >>"%b_err%"

REM === RUN C: D.2 exact-anchor k==2 regression -> 0,3 on D.2 chain ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="d2regress" --container y4m "%vpy%" "%c_y4m%"  2>>"%c_err%"
set "rc_c=%ERRORLEVEL%"
echo (RUN C d2regress exit=%rc_c%) >>"%c_err%"

REM === RUN D: D.1 single-hole regression -> 0,2 on D.1 chain ===
"%vspipe%" %rrr% --start 0 --end 1 --arg mode="d1regress" --container y4m "%vpy%" "%d_y4m%"  2>>"%d_err%"
set "rc_d=%ERRORLEVEL%"
echo (RUN D d1regress exit=%rc_d%) >>"%d_err%"

REM === RUN E: negative control -> 0,1,2,3 sequential ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="processing" --container y4m "%vpy%" "%e_y4m%"  2>>"%e_err%"
set "rc_e=%ERRORLEVEL%"
echo (RUN E processing exit=%rc_e%) >>"%e_err%"

REM === RUN G: passthrough ===
"%vspipe%" %rrr% --start 0 --end 3 --arg mode="passthrough" --container y4m "%vpy%" "%g_y4m%"  2>>"%g_err%"
set "rc_g=%ERRORLEVEL%"
echo (RUN G passthrough exit=%rc_g%) >>"%g_err%"

echo.
echo ====================================================
echo CHECK A - FLOOR-FRESH-START on cold output[3] (THE D.3 PROOF)
echo ====================================================
echo RUN A floorstart exit code = %rc_a% 1>&2
if not "%rc_a%"=="0" ( echo RESULT A0: FAIL -- RUN A did not exit 0 1>&2 ) else ( echo RESULT A0: PASS -- RUN A exited 0 1>&2 )
findstr /C:"branch=RECOVER" "%a_err%" >NUL && echo RESULT A1: PASS -- branch=RECOVER present 1>&2 || echo RESULT A1: FAIL/INCONCLUSIVE -- no RECOVER KDT; NOT a D.3 pass 1>&2
findstr /C:"recover_branch=floor-fresh-start" "%a_err%" >NUL && echo RESULT A2: PASS -- recover_branch=floor-fresh-start 1>&2 || echo RESULT A2: FAIL -- not the floor-fresh-start branch 1>&2
findstr /C:"floor=0" "%a_err%" | findstr /C:"floor_outcome=computed" >NUL && echo RESULT A3: PASS -- floor=0 floor_outcome=computed 1>&2 || echo RESULT A3: CHECK -- inspect floor / floor_outcome 1>&2
findstr /C:"hole_count=2" "%a_err%" >NUL && echo RESULT A4: PASS -- hole_count=2 1>&2 || echo RESULT A4: CHECK -- inspect hole_count 1>&2
findstr /C:"holes=[1,2]" "%a_err%" >NUL && echo RESULT A5: PASS -- holes=[1,2] 1>&2 || echo RESULT A5: CHECK -- inspect holes 1>&2
findstr /C:"source_requests=[0,1,2,3]" "%a_err%" >NUL && echo RESULT A6: PASS -- source_requests=[0,1,2,3] 1>&2 || echo RESULT A6: CHECK -- inspect source_requests 1>&2
findstr /C:"hole=1 outcome=computed" "%a_err%" >NUL && echo RESULT A7: PASS -- hole 1 computed 1>&2 || echo RESULT A7: CHECK -- inspect hole 1 1>&2
findstr /C:"hole=2 outcome=computed" "%a_err%" >NUL && echo RESULT A8: PASS -- hole 2 computed 1>&2 || echo RESULT A8: CHECK -- inspect hole 2 1>&2
findstr /C:"pin_list_size=3" "%a_err%" >NUL && echo RESULT A9: PASS -- pin_list_size=3 (floor + 2 holes) 1>&2 || echo RESULT A9: CHECK -- inspect pin_list_size 1>&2
findstr /C:"branch=RECOVER" "%a_err%" | findstr /C:"pin_balance=0" >NUL && echo RESULT A10: PASS -- pin_balance=0 1>&2 || echo RESULT A10: CHECK -- inspect pin_balance 1>&2
REM cold output[3] is the only rendered frame -> y4m index 0
"%python_exe%" "%checker%" "%a_y4m%" 128 144 113 0 1>&2

echo.
echo ====================================================
echo CHECK B - FLOOR byte (fresh-start proof) + HOLE bytes via cache-hit
echo ====================================================
echo RUN B floorbytes exit code = %rc_b% 1>&2
if not "%rc_b%"=="0" ( echo RESULT B0: FAIL -- RUN B did not exit 0 1>&2 ) else ( echo RESULT B0: PASS -- RUN B exited 0 1>&2 )
findstr /C:"branch=CACHE-HIT" "%b_err%" >NUL && echo RESULT B1: PASS -- cache-hit(s) present for re-requested floor/holes 1>&2 || echo RESULT B1: FAIL/INCONCLUSIVE -- no CACHE-HIT KDT 1>&2
REM rendered positions: 3,0,1,2 -> y4m idx0=target3, idx1=floor0(56/176), idx2=hole1(144/111), idx3=hole2(145/109)
"%python_exe%" "%checker%" "%b_y4m%" 128 56 176 1 1>&2
"%python_exe%" "%checker%" "%b_y4m%" 128 144 111 2 1>&2
"%python_exe%" "%checker%" "%b_y4m%" 128 145 109 3 1>&2

echo.
echo ====================================================
echo CHECK C - D.2 exact-anchor k==2 regression (NOT floor-fresh-start)
echo ====================================================
echo RUN C d2regress exit code = %rc_c% 1>&2
if not "%rc_c%"=="0" ( echo RESULT C0: FAIL -- RUN C did not exit 0 1>&2 ) else ( echo RESULT C0: PASS -- RUN C exited 0 1>&2 )
findstr /C:"recover_branch=exact-anchor" "%c_err%" >NUL && echo RESULT C1: PASS -- exact-anchor branch (not floor) 1>&2 || echo RESULT C1: FAIL -- D.2 path not exact-anchor 1>&2
findstr /C:"recover_branch=floor-fresh-start" "%c_err%" >NUL && echo RESULT C2: FAIL -- D.2 case wrongly took floor-fresh-start 1>&2 || echo RESULT C2: PASS -- floor-fresh-start NOT taken for D.2 case 1>&2
findstr /C:"holes=[1,2]" "%c_err%" >NUL && echo RESULT C3: PASS -- holes=[1,2] 1>&2 || echo RESULT C3: CHECK -- inspect holes 1>&2
"%python_exe%" "%checker%" "%c_y4m%" 128 148 100 1 1>&2

echo.
echo ====================================================
echo CHECK D - D.1 single-hole (k==1) regression
echo ====================================================
echo RUN D d1regress exit code = %rc_d% 1>&2
if not "%rc_d%"=="0" ( echo RESULT D0: FAIL -- RUN D did not exit 0 1>&2 ) else ( echo RESULT D0: PASS -- RUN D exited 0 1>&2 )
findstr /C:"recover_branch=exact-anchor" "%d_err%" | findstr /C:"hole_count=1" >NUL && echo RESULT D1: PASS -- D.1 exact-anchor hole_count=1 1>&2 || echo RESULT D1: CHECK -- inspect D.1 KDT 1>&2
"%python_exe%" "%checker%" "%d_y4m%" 128 147 109 1 1>&2

echo.
echo ====================================================
echo CHECK E - negative control (predecessor-present, NOT recovery)
echo ====================================================
echo RUN E processing exit code = %rc_e% 1>&2
if not "%rc_e%"=="0" ( echo RESULT E0: FAIL -- RUN E did not exit 0 1>&2 ) else ( echo RESULT E0: PASS -- RUN E exited 0 1>&2 )
findstr /C:"N=3 branch=PREDECESSOR-PRESENT-COMPUTE" "%e_err%" >NUL && echo RESULT E1: PASS -- frame 3 took predecessor-present 1>&2 || echo RESULT E1: CHECK -- frame 3 branch 1>&2
findstr /C:"branch=RECOVER" "%e_err%" >NUL && echo RESULT E2: FAIL -- NEGATIVE control violated: recovery fired with predecessor present 1>&2 || echo RESULT E2: PASS -- no recovery when predecessor present 1>&2
"%python_exe%" "%checker%" "%e_y4m%" 128 144 111 1 1>&2
"%python_exe%" "%checker%" "%e_y4m%" 128 145 109 2 1>&2
"%python_exe%" "%checker%" "%e_y4m%" 128 144 113 3 1>&2

echo.
echo ====================================================
echo CHECK G - passthrough negative (no [KDT])
echo ====================================================
echo RUN G passthrough exit code = %rc_g% 1>&2
findstr /C:"[KDT]" "%g_err%" >NUL && echo RESULT G: FAIL -- [KDT] present in passthrough bypass 1>&2 || echo RESULT G: PASS -- no [KDT] in passthrough bypass 1>&2

echo.
echo ====================================================
echo SUMMARY (read per-check RESULT lines; this is a label, not a verdict):
echo   A FLOOR-FRESH-START (THE PROOF) / B floor-byte + hole bytes / C D.2 exact-anchor regression /
echo   D D.1 regression / E negative control / G passthrough-clean
echo   D.3 PASS REQUIRES: A1-A10 + recovered 144/113; B floor 56/176 (fresh-start) + holes 144/111 &
echo   145/109; C recover_branch=exact-anchor (NOT floor) + 148/100; D 147/109; E predecessor-present
echo   (no RECOVER); G clean. bytes WITHOUT the matching KDT line = INCONCLUSIVE.
echo ====================================================
pause
