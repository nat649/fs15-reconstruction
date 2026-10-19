@echo off
setlocal
cd /d "%~dp0"
if not exist "build\ge6-objects-build\src\ge6_objects\Release\FS15GE6Objects.exe" (
    echo Build first with powershell -File scripts\build-ge6-objects.ps1
    pause
    exit /b 1
)
"build\ge6-objects-build\src\ge6_objects\Release\FS15GE6Objects.exe" --scene "build\ge6-objects\map01\scene.ge6scene" %*
if errorlevel 1 pause
