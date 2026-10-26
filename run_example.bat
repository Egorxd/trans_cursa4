@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1"
  if errorlevel 1 exit /b 1
)
"bin\translator.exe" "examples\input.txt" -o "examples\output.py"
if errorlevel 1 exit /b 1
".venv\Scripts\python.exe" "tools\show_example.py"
if errorlevel 1 exit /b 1
pause
