@echo off
chcp 65001 >nul
rem =====================================================================
rem  build.bat - OpenMiner (gamma) Windows 一键编译脚本
rem
rem  用法（在项目根目录、即本脚本所在目录执行）：
rem    build.bat                 默认使用 Visual Studio 生成器，Release 构建
rem    build.bat Debug           Debug 构建
rem    build.bat Release         Release 构建
rem    build.bat RelWithDebInfo  带调试符号的 Release 构建
rem    build.bat MinGW Release   使用 MinGW-w64 (gcc) 构建 Release
rem                （自定义生成器请用环境变量：set CMAKE_GEN=Ninja && build.bat）
rem    build.bat clean           删除 build 目录后退出
rem
rem  可选环境变量：
rem    CMAKE_GEN   手动指定 CMake 生成器，例如：
rem                set CMAKE_GEN=Visual Studio 17 2022 && build.bat
rem    JOBS        并行编译任务数（MinGW/Ninja 时使用），默认自动检测
rem
rem  产物：build\openminer.exe（客户端）、build\openminer_server.exe（服务端）
rem =====================================================================

setlocal

rem ---- 切换到脚本所在目录（保证从任意位置双击/调用都能工作）----
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "USE_MINGW=0"

rem ---- 解析参数 ----
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="clean" (
    echo [INFO] 正在删除 %BUILD_DIR% ...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo [INFO] 清理完成。
    exit /b 0
)
if /i "%~1"=="debug"          set "BUILD_TYPE=Debug"
if /i "%~1"=="release"        set "BUILD_TYPE=Release"
if /i "%~1"=="relwithdebinfo" set "BUILD_TYPE=RelWithDebInfo"
if /i "%~1"=="minsizerel"     set "BUILD_TYPE=MinSizeRel"
if /i "%~1"=="mingw"          set "USE_MINGW=1"
shift
goto parse_args
:args_done

echo.
echo ================= OpenMiner 编译脚本 =================
echo  构建类型 : %BUILD_TYPE%
echo  构建目录 : %CD%\%BUILD_DIR%
echo =====================================================
echo.

rem ---- 检查 CMake ----
where cmake >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 cmake，请先安装 CMake ^(>= 3.12^) 并加入 PATH。
    echo        下载：https://cmake.org/download/
    exit /b 1
)

rem ---- 检查 git 子模块是否已初始化（zlib/lua/bgfx 等依赖为子模块）----
if not exist "external\zlib\CMakeLists.txt" (
    where git >nul 2>nul
    if errorlevel 1 (
        echo [错误] external\ 下的子模块为空且未找到 git。
        echo        请安装 Git 或在完整仓库副本中重新运行本脚本。
        exit /b 1
    )
    echo [INFO] 正在拉取 git 子模块（首次编译需要联网，可能需要几分钟）...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo [错误] git submodule update --init --recursive 失败，请检查网络后重试。
        exit /b 1
    )
)

rem ---- 确定生成器与工具链 ----
set "GENERATOR="
if not "%CMAKE_GEN%"=="" (
    set "GENERATOR=%CMAKE_GEN%"
    echo [INFO] 使用用户指定的生成器：%GENERATOR%
    goto gen_done
)

if "%USE_MINGW%"=="1" (
    rem 显式要求 MinGW：校验 g++ 可用
    where g++ >nul 2>nul
    if errorlevel 1 (
        echo [错误] 指定了 MinGW 但 PATH 中找不到 g++。
        echo        请安装 MinGW-w64（如 MSYS2 UCRT64/MINGW64）并加入 PATH，例如：
        echo          C:\msys64\mingw64\bin  或  C:\TDM-GCC-64\bin
        exit /b 1
    )
    set "GENERATOR=MinGW Makefiles"
    goto gen_done
)

rem 未显式指定：优先探测 Visual Studio，其次 MinGW
where g++ >nul 2>nul && set "HAS_GXX=1" || set "HAS_GXX=0"

cmake --help 2>nul | findstr /c:"Visual Studio 17 2022" >nul
if not errorlevel 1 (
    set "GENERATOR=Visual Studio 17 2022"
    echo [INFO] 检测到 Visual Studio 2022
    goto gen_done
)
cmake --help 2>nul | findstr /c:"Visual Studio 16 2019" >nul
if not errorlevel 1 (
    set "GENERATOR=Visual Studio 16 2019"
    echo [INFO] 检测到 Visual Studio 2019
    goto gen_done
)
if "%HAS_GXX%"=="1" (
    set "GENERATOR=MinGW Makefiles"
    echo [INFO] 未找到 Visual Studio，改用 MinGW-w64 ^(g++^)
    goto gen_done
)

echo [错误] 既未找到 Visual Studio（2019/2022），也未找到 MinGW-w64 的 g++。
echo        请安装其中之一，或用 CMAKE_GEN 环境变量手动指定生成器，例如：
echo          set CMAKE_GEN=Ninja ^&^& build.bat
exit /b 1
:gen_done

rem ---- 配置（Configure）----
echo.
echo [1/3] CMake 配置中...
cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -G "%GENERATOR%"
if errorlevel 1 (
    echo.
    echo [错误] CMake 配置失败，请检查上方输出。
    echo        提示：修改过 CMakeLists 或切换过编译器时，可先执行 "build.bat clean"。
    exit /b 1
)

rem ---- 编译（Build）----
echo.
echo [2/3] 编译中（首次编译耗时较长，请耐心等待）...
if defined JOBS (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel %JOBS%
) else (
    cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel
)
if errorlevel 1 (
    echo.
    echo [错误] 编译失败，请检查上方编译器报错信息。
    exit /b 1
)

rem ---- 结果汇总 ----
echo.
echo [3/3] 编译完成！

set "EXE_FOUND=0"
if exist "%BUILD_DIR%\openminer.exe" (
    set "EXE_FOUND=1"
    echo        %BUILD_DIR%\openminer.exe
)
if exist "%BUILD_DIR%\openminer_server.exe" (
    set "EXE_FOUND=1"
    echo        %BUILD_DIR%\openminer_server.exe
)
rem Visual Studio 生成器会把可执行文件输出到带构建类型的子目录
if exist "%BUILD_DIR%\%BUILD_TYPE%\openminer.exe" (
    set "EXE_FOUND=1"
    echo        %BUILD_DIR%\%BUILD_TYPE%\openminer.exe
)
if exist "%BUILD_DIR%\%BUILD_TYPE%\openminer_server.exe" (
    set "EXE_FOUND=1"
    echo        %BUILD_DIR%\%BUILD_TYPE%\openminer_server.exe
)

if "%EXE_FOUND%"=="1" (
    echo.
    echo 编译产物已生成，直接运行上方列出的 exe 即可启动游戏。
) else (
    echo.
    echo [警告] 未在 %BUILD_DIR% 中找到 openminer.exe / openminer_server.exe，
    echo        若使用多配置生成器（Visual Studio），请在 IDE 中按 F5 运行或查看
    echo        %BUILD_DIR% 下对应配置子目录。
)
endlocal
exit /b 0
