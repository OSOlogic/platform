@echo off
REM Double-click to install & launch the OSOLogic sandbox (needs Docker Desktop).
powershell -NoProfile -ExecutionPolicy Bypass -Command "irm https://osologic.com/get.ps1 | iex"
pause
