@echo off
REM usage: copy_cesium.bat <project folder>
REM Copies Cesium for Unreal from the engine's Marketplace plugins into the project, so it is compiled
REM from source for every target platform (the Fab install only carries Win64 binaries). Linux builds
REM of it need unreachable code reported as a warning; the patch goes away once Cesium removes it.
setlocal
call "%~dp0env.bat"
set "SRC="
for /d %%D in ("%UE_DIR%\Engine\Plugins\Marketplace\Cesium*") do set "SRC=%%~fD"
if not defined SRC (echo Cesium for Unreal is not installed in %UE_DIR% & exit /b 1)
robocopy "%SRC%" "%~f1\Plugins\CesiumForUnreal" /E /XD Binaries Intermediate Android-aarch64-Release Darwin-universal-Release iOS-arm64-Release /njh /njs /ndl /np /nfl >nul
powershell -NoProfile -Command "$f='%~f1\Plugins\CesiumForUnreal\Source\CesiumRuntime\CesiumRuntime.Build.cs'; $s=Get-Content -Raw $f; if ($s -notmatch 'UnreachableCodeWarningLevel') { $s=$s -replace '(public CesiumRuntime\(ReadOnlyTargetRules Target\) : base\(Target\)\s*\{)', ('$1' + [Environment]::NewLine + '        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Warning;'); Set-Content -NoNewline $f $s }"
powershell -NoProfile -Command "$f='%~f1\Plugins\CesiumForUnreal\CesiumForUnreal.uplugin'; (Get-Content -Raw $f) -replace '\"Installed\": true', '\"Installed\": false' | Set-Content -NoNewline $f"
exit /b 0
