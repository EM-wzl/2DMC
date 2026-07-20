@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0base.exe" (
  echo 未找到 "%~dp0base.exe"
  pause
  exit /b 1
)
"%SystemRoot%\System32\conhost.exe" "%SystemRoot%\System32\cmd.exe" /k ""%~dp0base.exe""
endlocal
