@echo off
if exist "%~dp0..\..\Saved\Ultimate\Package\Windows\HSR.exe" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch_ultimate_practice.ps1" -Packaged
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch_ultimate_practice.ps1"
)
