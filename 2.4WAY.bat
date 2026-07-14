@echo off
setlocal enableextensions enabledelayedexpansion

set "build_folder_root=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"
set "solution_file=cnr3.slnx"

REM ----------------------------------------------------------------------------------------
@echo on
cls
REM Run the 4-way test
pushd "%build_folder_root%"

x64\Debug\cnr3_cache_core_selftest.exe 1>NUL
echo Debug normal exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe 1>NUL
echo Release normal exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe --force-fail-for-harness-proof 1>NUL
echo Release forced-fail exit_code=%ERRORLEVEL%

x64\Release\cnr3_cache_core_selftest.exe --verbose 1>NUL
echo Release verbose exit_code=%ERRORLEVEL%

popd
@echo off
REM ----------------------------------------------------------------------------------------

pause
goto :eof
