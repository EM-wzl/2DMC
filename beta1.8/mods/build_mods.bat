@echo off
rem ===================== build mods in this folder =====================
rem Compiles every .cpp here into a same-named .dll in this folder (game's mods/).
rem Usage: double-click this file, or run  mods\build_mods.bat
rem NOTE: keep this file pure ASCII (cmd.exe parses .bat as ANSI).
setlocal
cd /d "%~dp0"

set CXX=g++
set FLAGS=-std=c++17 -O2 -shared -static -static-libgcc -static-libstdc++ ^
 -I..\src\mods -finput-charset=UTF-8 -fexec-charset=UTF-8 -fwide-exec-charset=UTF-16LE

for %%F in (*.cpp) do (
    echo [mod] compiling %%F
    %CXX% "%%F" -o "%%~nF.dll" %FLAGS%
    if errorlevel 1 (
        echo [FAILED] %%F
    ) else (
        echo [OK] %%~nF.dll
    )
)
echo.
echo done. Start the game, open the MODS page and enable what you need.
endlocal
