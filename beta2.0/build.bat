@echo off
rem ===================== 2DMC beta2.0 graphical build =====================
rem usage: build.bat           -> GUI build (no console, release)
rem        build.bat console   -> console build (keeps stdout, for debugging)
rem NOTE: keep this file pure ASCII. cmd.exe parses .bat as ANSI and
rem       UTF-8 Chinese comments break the parser.
setlocal
cd /d "%~dp0"

set SRC=src\main.cpp ^
 src\platform\platform_win32.cpp ^
 src\render\atlas.cpp ^
 src\render\font_gdi.cpp ^
 src\render\renderer_gl.cpp ^
 src\world\tiles.cpp ^
 src\world\world.cpp ^
 src\entity\entity.cpp ^
 src\game\game.cpp ^
 src\game\game_render.cpp ^
 src\game\save.cpp

set OUT=2DMC.exe
set CXX=g++
set FLAGS=-std=c++17 -O2 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-function
rem Pin the character sets: without this the compiler may mis-detect the
rem UTF-8 source as ANSI and mangle Chinese literals.
set FLAGS=%FLAGS% -finput-charset=UTF-8 -fexec-charset=UTF-8 -fwide-exec-charset=UTF-16LE
set LIBS=-lopengl32 -lgdi32 -lwinmm -static

if /i "%~1"=="console" (
    echo [build] console debug variant ...
    %CXX% %SRC% -o %OUT% %FLAGS% %LIBS%
) else (
    echo [build] GUI variant ...
    %CXX% %SRC% -o %OUT% %FLAGS% %LIBS% -mwindows
)

if errorlevel 1 (
    echo [FAILED] compile error
    exit /b 1
)
echo [OK] %OUT%
endlocal