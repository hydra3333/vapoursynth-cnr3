@echo off
setlocal enableextensions enabledelayedexpansion

set "build_folder_root=E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3"
set "solution_file=cnr3.slnx"

REM ----------------------------------------------------------------------------------------
ECHO Build Release and Debug Projects in Solution %solution_file%

call :build_projects_in_solution "Debug" "%build_folder_root%" "%solution_file%"

call :build_projects_in_solution "Release" "%build_folder_root%" "%solution_file%"

echo.
echo dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug\cnr3.*"
dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Debug\cnr3.*"
echo.
echo dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release\cnr3.*"
dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release\cnr3.*"
echo.
pause
REM ----------------------------------------------------------------------------------------
goto :eof

:build_projects_in_solution
REM p1 = Release or Debug type of build
REM p2 = build root folder to PUSHD into eg E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3
REM p3 = solution filename eg cnr3.slnx
REM find msbuild and set variable MSBUILD to its location
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
REM
@echo on
REM Build all %~1 Projects within solution %~3
pushd "%~2"
"%MSBUILD%" %~3 /m /t:Clean;Build /p:Configuration=%~1 /p:Platform=x64
set "EL=%ERRORLEVEL%"
@echo off
popd
if %EL% NEQ 0 (
    echo.
    echo  ERRORS detected when building %~1 projects in solution %~3
    echo.
    pause
    exit
)
REM echo dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%~1\cnr3.*"
REM dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%~1\cnr3.*"
goto :eof
