@echo off
rem ===================== 2DMC beta1.8 OpenGL build =====================
rem usage: build.bat              -> GUI build (no console, release)
rem        build.bat console      -> console build (keeps stdout/stderr, for debugging)
rem        build.bat fast         -> GUI build with -O0 (quick syntax check)
rem NOTE: keep this file pure ASCII. cmd.exe parses .bat as ANSI and
rem       UTF-8 Chinese comments break the parser.
setlocal
cd /d "%~dp0"

set SRC=src\main.cpp ^
 src\platform\platform_win32.cpp ^
 src\render\atlas.cpp ^
 src\render\font_gdi.cpp ^
 src\render\png_load.cpp ^
 src\render\renderer_gl.cpp ^
 src\world\blocks.cpp ^
 src\world\world.cpp ^
 src\item\chest.cpp ^
 src\item\item.cpp ^
 src\item\recipes.cpp ^
 src\entity\entity.cpp ^
 src\game\game.cpp ^
 src\game\game_render.cpp ^
 src\game\ui.cpp ^
 src\game\save.cpp ^
 src\mods\mod_loader.cpp

set OUT=2DMC18.exe
set CXX=g++
set FLAGS=-std=c++17 -O1 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-function -Wno-unused-parameter -Isrc
rem Pin the character sets: without this the compiler may mis-detect the
rem UTF-8 source as ANSI and mangle Chinese literals.
set FLAGS=%FLAGS% -finput-charset=UTF-8 -fexec-charset=UTF-8 -fwide-exec-charset=UTF-16LE
set LIBS=-lopengl32 -lgdi32 -lwinmm -lgdiplus -lole32 -luuid -static

if /i "%~1"=="console" (
    echo [build] console debug variant ...
    %CXX% %SRC% -o %OUT% %FLAGS% -g %LIBS%
    goto :done
)
if /i "%~1"=="fast" (
    echo [build] fast syntax check ...
    %CXX% %SRC% -o %OUT% -std=c++17 -O0 -w -Isrc -finput-charset=UTF-8 -fexec-charset=UTF-8 -fwide-exec-charset=UTF-16LE %LIBS% -mwindows
    goto :done
)
echo [build] GUI variant ...
%CXX% %SRC% -o %OUT% %FLAGS% %LIBS% -mwindows

:done
if errorlevel 1 (
    echo [FAILED] compile error
    exit /b 1
)
echo [OK] %OUT%
endlocal
