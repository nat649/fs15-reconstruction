@echo off
if not exist "%~dp0build\FS15Launcher\FS15Reconstruction.exe" (
  echo Build the launcher first: scripts\build-launcher.ps1
  pause
  exit /b 1
)
start "" "%~dp0build\FS15Launcher\FS15Reconstruction.exe" --root "%~dp0."
