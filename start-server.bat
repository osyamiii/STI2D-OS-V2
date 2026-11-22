@echo off
rem Starts the STI2D-OS portal: http://localhost:8000/web/ (and a phone address on the same Wi-Fi)
cd /d "%~dp0"
where py >nul 2>nul && (py -3 scripts\serve.py %1) || (python scripts\serve.py %1)
pause
