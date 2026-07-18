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

echo mkdir "D:\TEST\DLL_fmParallelRequests\"
mkdir "D:\TEST\DLL_fmParallelRequests\"
echo copy /y "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release\cnr3.*" "D:\TEST\DLL_fmParallelRequests\"
copy /y "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\Release\cnr3.*" "D:\TEST\DLL_fmParallelRequests\"
echo.

REM set "build_type=Debug"
set "build_type=Release"
set "top_root=D:\TEST"
set "vs_root=%top_root%\Vapoursynth_x64_R76"
echo copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.dll "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.dll "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
echo copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.pdb "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
copy /y E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%build_type%\cnr3.pdb "%vs_root%\Lib\site-packages\vapoursynth\plugins\"
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
    exit /b %EL%
)
REM echo dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%~1\cnr3.*"
REM dir /b /s "E:\SOFTWARE-Win11\MULTIMEDIA\vapoursynth-cnr3\github\vs\cnr3\x64\%~1\cnr3.*"
goto :eof
