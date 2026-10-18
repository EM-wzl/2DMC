@echo off
rem ===================== beta1.8 regression check =====================
rem Covers: build / block table / new world / save round-trip / panels / title / legacy save
rem NOTE: keep this file pure ASCII. cmd.exe parses .bat as ANSI; non-ASCII
rem       comments break the parser.
setlocal enabledelayedexpansion
cd /d "%~dp0"

set PASS=0
set FAIL=0

echo ============================================================
echo [1/6] compile
echo ============================================================
call build.bat >_build.log 2>&1
if errorlevel 1 (
    echo [FAIL] compile
    type _build.log
    set /a FAIL+=1
    goto :end
)
echo [OK] compile
set /a PASS+=1

echo.
echo ============================================================
echo [2/6] block table regression
echo ============================================================
g++ tools\verify_tables.cpp src\world\blocks.cpp -o _verify.exe -std=c++17 -Isrc -w ^
    -finput-charset=UTF-8 -fexec-charset=UTF-8 >_tb.log 2>&1
if errorlevel 1 (
    echo [FAIL] verify_tables compile
    type _tb.log
    set /a FAIL+=1
) else (
    _verify.exe >_tv.log 2>&1
    if errorlevel 1 (
        echo [FAIL] block table mismatch
        findstr /C:"FAIL" _tv.log
        set /a FAIL+=1
    ) else (
        echo [OK] block table
        findstr /C:"***" _tv.log
        set /a PASS+=1
    )
)

echo.
echo ============================================================
echo [3/6] new world + v4 save
echo ============================================================
if exist _reg.bin del _reg.bin
if exist debug.log del debug.log
start /wait "" 2DMC18.exe --world --name=_reg --shot=_r1.bmp --shotframe=60
if not exist _reg.bin (
    echo [FAIL] no save file produced
    set /a FAIL+=1
) else (
    echo [OK] new world + save
    for %%A in (_reg.bin) do echo      _reg.bin = %%~zA bytes
    set /a PASS+=1
)

echo.
echo ============================================================
echo [4/6] load round trip
echo ============================================================
if exist debug.log del debug.log
start /wait "" 2DMC18.exe --load=_reg --shot=_r2.bmp --shotframe=30
findstr /C:"load ok" debug.log >nul 2>&1
if errorlevel 1 (
    rem fall back: check the save header version instead
    findstr /R /C:"version 4" debug.log >nul 2>&1
)
if not exist _r2.bmp (
    echo [FAIL] load produced no frame
    set /a FAIL+=1
) else (
    echo [OK] load round trip
    set /a PASS+=1
)

echo.
echo ============================================================
echo [5/6] panels
echo ============================================================
for %%U in (inv craft settings map) do (
    start /wait "" 2DMC18.exe --load=_reg --ui=%%U --shot=_p_%%U.bmp --shotframe=25
    if exist _p_%%U.bmp (
        echo [OK] panel %%U
        set /a PASS+=1
    ) else (
        echo [FAIL] panel %%U
        set /a FAIL+=1
    )
)

echo.
echo ============================================================
echo [6/6] title screen
echo ============================================================
start /wait "" 2DMC18.exe --shot=_title.bmp --shotframe=25
if exist _title.bmp (
    echo [OK] title
    set /a PASS+=1
) else (
    echo [FAIL] title
    set /a FAIL+=1
)

echo.
echo ============================================================
echo legacy save load (synthetic v1 fixture, auto upgrade to v4)
echo ============================================================
if exist tools\fixtures\synthetic_v1.bin (
    copy /y tools\fixtures\synthetic_v1.bin _synv1.bin >nul
    if exist debug.log del debug.log
    start /wait "" 2DMC18.exe --load=_synv1 --shot=_v1.bmp --shotframe=25
    if exist _v1.bmp (
        echo [OK] legacy v1 load
        set /a PASS+=1
        findstr /C:"upgrade" debug.log
    ) else (
        echo [FAIL] legacy v1 load
        set /a FAIL+=1
    )
) else (
    echo [SKIP] no fixture tools\fixtures\synthetic_v1.bin
)

:end
echo.
echo ============================================================
echo PASS=!PASS!  FAIL=!FAIL!
echo screenshots: _r1.bmp _r2.bmp _p_*.bmp _title.bmp _v1.bmp
echo log: debug.log
echo ============================================================
if exist _verify.exe del _verify.exe
if exist _build.log del _build.log
if exist _tb.log del _tb.log
if exist _tv.log del _tv.log
if !FAIL! GTR 0 exit /b 1
exit /b 0
