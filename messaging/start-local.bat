@echo off
setlocal
cd /d "%~dp0\.."
if "%STI2D_INVITE_CODE%"=="" (
  echo Definis STI2D_INVITE_CODE avec un code d'invitation avant de lancer le relais.
  exit /b 1
)
if "%STI2D_MESSAGES_DB%"=="" set STI2D_MESSAGES_DB=%CD%\messaging\messages.sqlite3
if "%STI2D_MESSAGES_PORT%"=="" set STI2D_MESSAGES_PORT=8765
python -m uvicorn messaging.server.app:app --host 127.0.0.1 --port %STI2D_MESSAGES_PORT%
