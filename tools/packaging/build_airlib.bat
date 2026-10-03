@echo off
REM Builds AirLib, rpclib and MavLinkCom (Release) for Windows with Visual Studio, like build.cmd
REM from a developer prompt. The Linux libraries come from build.sh --ue-toolchain (see README.md).
setlocal
call "%~dp0env.bat"
call "%VCVARS%" >nul || exit /b 1
cd /d "%HERCULES_ROOT%"
call build.cmd --Release
