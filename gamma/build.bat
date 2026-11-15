@echo off
chcp 65001 >nul
rem =====================================================================
rem  build.bat - OpenMiner (gamma) Windows 一键编译脚本
rem
rem  用法（在项目根目录、即本脚本所在目录执行）：
rem    build.bat                 默认 Release 构建
rem    build.bat Debug           Debug 构建
rem    build.bat Release         Release 构建
rem    build.bat RelWithDebInfo  带调试符号的 Release 构建
rem    build.bat MinGW Release   强制使用 MinGW-w64 (gcc) 构建
rem    build.bat clean           删除 build 目录后退出
rem
rem  生成器说明：
rem    本项目 external/ 下的第三方库（bgfx 等）已针对 MinGW-w64 / g++ 16
rem    打过补丁，因此默认使用 MinGW-w64 (g++) 构建。
rem    如需其它生成器，可用环境变量 CMAKE_GEN 手动指定，例如：
rem      set CMAKE_GEN=Ninja && build.bat
rem
rem  可选环境变量：
rem    CMAKE_GEN   手动指定 CMake 生成器（跳过自动探测）
rem    JOBS        并行编译任务数（默认自动使用全部核心）
rem    DEPLOY_DIR  部署目录，编译成功后自动把 exe 及依赖 DLL 复制到该目录。
rem                默认为上一级目录下的 Minecraft 文件夹（即 ..\Minecraft）；
rem                设为 none 可禁用部署，例如：set DEPLOY_DIR=none && build.bat
rem    DEPLOY_NAME 部署的客户端 exe 文件名，默认 Minecraft.exe
rem                （游戏目录里双击运行的就是这个名字）
rem
rem  其他说明：
rem    - 若 build 目录里的 CMake 缓存来自改名前的源码目录，或与当前所选
rem      生成器不一致，脚本会自动删除旧构建目录并重新配置，无需手动 clean。
rem  产物：build\openminer.exe（客户端）、build\openminer_server.exe（服务端）
rem        编译完成后自动部署到 Minecraft 游戏目录（config/saves/mods/resources 所在处）
rem =====================================================================

setlocal enableextensions enabledelayedexpansion

rem ---- 切换到脚本所在目录（保证从任意位置双击/调用都能工作）----
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "USE_MINGW=0"
if "%DEPLOY_DIR%"=="" set "DEPLOY_DIR=%~dp0..\Minecraft"
if "%DEPLOY_NAME%"=="" set "DEPLOY_NAME=Minecraft.exe"
if /i not "%DEPLOY_DIR%"=="none" for %%I in ("%DEPLOY_DIR%") do set "DEPLOY_DIR=%%~fI"

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
echo  部署目录 : %DEPLOY_DIR%
echo  部署名称 : %DEPLOY_NAME%
echo =====================================================
echo.

rem ---- 若 PATH 里缺少工具链，尝试常见的 MinGW-w64 安装位置 ----
set "NEED_TOOLCHAIN_PATH="
where cmake >nul 2>nul
if errorlevel 1 set "NEED_TOOLCHAIN_PATH=1"
where g++ >nul 2>nul
if errorlevel 1 set "NEED_TOOLCHAIN_PATH=1"
if defined NEED_TOOLCHAIN_PATH (
    set "TOOLCHAIN_HINT="
    for %%P in ("C:\mingw64\bin" "C:\msys64\mingw64\bin" "C:\msys64\ucrt64\bin" "C:\TDM-GCC-64\bin") do (
        if not defined TOOLCHAIN_HINT if exist "%%~P\g++.exe" set "TOOLCHAIN_HINT=%%~P"
    )
    if defined TOOLCHAIN_HINT (
        set "PATH=!TOOLCHAIN_HINT!;!PATH!"
        echo [INFO] 已把 !TOOLCHAIN_HINT! 加入 PATH。
    )
)

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

rem ---- 检测编译器 ----
set "HAS_GXX=0"
where g++ >nul 2>nul
if not errorlevel 1 set "HAS_GXX=1"

rem ---- 确定生成器 ----
set "GENERATOR="
if not "%CMAKE_GEN%"=="" (
    set "GENERATOR=%CMAKE_GEN%"
    echo [INFO] 使用 CMAKE_GEN 指定的生成器：%CMAKE_GEN%
    goto gen_done
)

if "%HAS_GXX%"=="1" (
    set "GENERATOR=MinGW Makefiles"
    echo [INFO] 使用 MinGW-w64 ^(g++^) 构建
    goto gen_done
)

rem 没有 g++：给出明确提示。本项目的 external 依赖是按 MinGW/g++ 16 修补的，
rem 所以不自动切到 Visual Studio（cmake --help 里列出 VS 生成器不代表本机装了 VS）。
echo [错误] 未找到 MinGW-w64 的 g++。
echo        本项目 external/ 依赖已按 MinGW/g++ 16 修补，推荐使用 MinGW-w64 构建。
echo        请安装 MinGW-w64（如 MSYS2 UCRT64/MINGW64 或 winlibs.com 的包），
echo        并把它的 bin 目录加入 PATH，例如：
echo          C:\mingw64\bin  或  C:\msys64\mingw64\bin
echo        如果你确实想用其它生成器，可用环境变量手动指定，例如：
echo          set CMAKE_GEN=Ninja ^&^& build.bat
echo          set CMAKE_GEN=Visual Studio 17 2022 ^&^& build.bat
if "%USE_MINGW%"=="1" echo        ^(本次命令行已显式要求 MinGW^)
exit /b 1
:gen_done

rem ---- 自动清理过期的构建缓存 ----
rem 源码目录改名（例如 OpenMiner-src -> gamma）或切换生成器后，旧的 CMakeCache.txt
rem 会让配置直接报错（"CMakeCache.txt directory ... is different"），这里自动处理。
if not exist "%BUILD_DIR%\CMakeCache.txt" goto cache_ok
set "CACHE_HOME="
set "CACHE_GEN="
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_HOME_DIRECTORY:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"') do set "CACHE_HOME=%%B"
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_GENERATOR:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"') do set "CACHE_GEN=%%B"
if defined CACHE_HOME (
    set "CACHE_HOME=!CACHE_HOME:/=\!"
    if /i not "!CACHE_HOME!"=="%CD%" (
        echo [INFO] 检测到旧构建缓存来自其它源码目录：
        echo        缓存目录：!CACHE_HOME!
        echo        当前目录：%CD%
        echo [INFO] 自动删除 %BUILD_DIR% 后重新配置...
        rmdir /s /q "%BUILD_DIR%"
        goto cache_ok
    )
)
if defined CACHE_GEN (
    if /i not "!CACHE_GEN!"=="%GENERATOR%" (
        echo [INFO] 生成器已变更（!CACHE_GEN! -^> %GENERATOR%），自动删除 %BUILD_DIR% 后重新配置...
        rmdir /s /q "%BUILD_DIR%"
    )
)
:cache_ok

rem ---- 配置（Configure）----
echo.
echo [1/3] CMake 配置中（生成器：%GENERATOR%）...
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
echo 编译完成！
set "EXE_FOUND=0"
set "CLIENT_EXE="
set "SERVER_EXE="
if exist "%BUILD_DIR%\openminer.exe" (
    set "EXE_FOUND=1"
    set "CLIENT_EXE=%BUILD_DIR%\openminer.exe"
    echo        %BUILD_DIR%\openminer.exe
)
if exist "%BUILD_DIR%\openminer_server.exe" (
    set "EXE_FOUND=1"
    set "SERVER_EXE=%BUILD_DIR%\openminer_server.exe"
    echo        %BUILD_DIR%\openminer_server.exe
)
rem Visual Studio 生成器会把可执行文件输出到带构建类型的子目录
if exist "%BUILD_DIR%\%BUILD_TYPE%\openminer.exe" (
    set "EXE_FOUND=1"
    set "CLIENT_EXE=%BUILD_DIR%\%BUILD_TYPE%\openminer.exe"
    echo        %BUILD_DIR%\%BUILD_TYPE%\openminer.exe
)
if exist "%BUILD_DIR%\%BUILD_TYPE%\openminer_server.exe" (
    set "EXE_FOUND=1"
    set "SERVER_EXE=%BUILD_DIR%\%BUILD_TYPE%\openminer_server.exe"
    echo        %BUILD_DIR%\%BUILD_TYPE%\openminer_server.exe
)
if "%EXE_FOUND%"=="0" (
    echo [警告] 未在 %BUILD_DIR% 中找到 openminer.exe / openminer_server.exe，
    echo        若使用多配置生成器（Visual Studio），请在 IDE 中按 F5 运行或查看
    echo        %BUILD_DIR% 下对应配置子目录。
    goto deploy_done
)

rem ---- 部署到 Minecraft 游戏目录 ----
if /i "%DEPLOY_DIR%"=="none" goto deploy_done
echo.
echo [3/3] 部署中...
if not exist "%DEPLOY_DIR%" mkdir "%DEPLOY_DIR%"
if not exist "%DEPLOY_DIR%" (
    echo [警告] 无法创建部署目录 %DEPLOY_DIR%，请手动复制上方产物。
    goto deploy_done
)

rem 部署：先把旧 exe 改名（Windows 允许删除/重命名正在运行的 exe），
rem 再复制新文件过来。这样**不需要杀掉用户正在玩的游戏**。
rem
rem 之前的做法是直接 copy，遇到 exe 被占用就静默失败（只打一行警告），
rem 导致「明明构建成功但游戏里还是旧版本」，非常难排查。
rem 这里改成「改名 + 复制」，失败时明确提示。

rem 顺带清理客户端目录里的旧日志，避免下次排障时读到上一次的
if exist "%DEPLOY_DIR%\logs" del /q "%DEPLOY_DIR%\logs\*.log" >nul 2>nul

rem 部署失败标志：任何一步 copy 失败都不能让整个脚本仍返回 0，
rem 否则「明明构建失败但提示部署完成」会误导排查。
set "DEPLOY_FAILED="

if defined CLIENT_EXE if exist "%CLIENT_EXE%" (
    rem 先把旧 exe 改名（Windows 允许重命名正在运行的 exe），再复制新的过来。
    rem 这样既不会杀掉用户正在玩的游戏，也不会因为文件占用而部署失败。
    rem 先清掉可能残留的陈旧 .old，避免上一轮失败留下的更旧版本被回滚盖回来。
    if exist "%DEPLOY_DIR%\%DEPLOY_NAME%.old" del /q "%DEPLOY_DIR%\%DEPLOY_NAME%.old" >nul 2>nul
    if exist "%DEPLOY_DIR%\%DEPLOY_NAME%" (
        move /y "%DEPLOY_DIR%\%DEPLOY_NAME%" "%DEPLOY_DIR%\%DEPLOY_NAME%.old" >nul 2>nul
        if errorlevel 1 (
            echo [警告] 无法重命名旧的客户端 exe，可能正被占用。
            set "DEPLOY_FAILED=1"
        )
    )
    if not defined DEPLOY_FAILED (
        copy /y "%CLIENT_EXE%" "%DEPLOY_DIR%\%DEPLOY_NAME%" >nul
        if errorlevel 1 (
            echo [警告] 复制客户端到 %DEPLOY_DIR%\%DEPLOY_NAME% 失败。
            if exist "%DEPLOY_DIR%\%DEPLOY_NAME%.old" move /y "%DEPLOY_DIR%\%DEPLOY_NAME%.old" "%DEPLOY_DIR%\%DEPLOY_NAME%" >nul 2>nul
            set "DEPLOY_FAILED=1"
        ) else (
            if exist "%DEPLOY_DIR%\%DEPLOY_NAME%.old" del /q "%DEPLOY_DIR%\%DEPLOY_NAME%.old" >nul 2>nul
            echo [INFO] 已部署客户端 %DEPLOY_NAME% -^> %DEPLOY_DIR%
        )
    )
)
if defined SERVER_EXE if exist "%SERVER_EXE%" (
    if exist "%DEPLOY_DIR%\openminer_server.exe.old" del /q "%DEPLOY_DIR%\openminer_server.exe.old" >nul 2>nul
    if exist "%DEPLOY_DIR%\openminer_server.exe" (
        move /y "%DEPLOY_DIR%\openminer_server.exe" "%DEPLOY_DIR%\openminer_server.exe.old" >nul 2>nul
        if errorlevel 1 (
            echo [警告] 无法重命名旧的服务端 exe，可能正被占用。
            set "DEPLOY_FAILED=1"
        )
    )
    if not defined DEPLOY_FAILED (
        copy /y "%SERVER_EXE%" "%DEPLOY_DIR%\openminer_server.exe" >nul
        if errorlevel 1 (
            echo [警告] 复制服务端到 %DEPLOY_DIR%\openminer_server.exe 失败。
            if exist "%DEPLOY_DIR%\openminer_server.exe.old" move /y "%DEPLOY_DIR%\openminer_server.exe.old" "%DEPLOY_DIR%\openminer_server.exe" >nul 2>nul
            set "DEPLOY_FAILED=1"
        ) else (
            if exist "%DEPLOY_DIR%\openminer_server.exe.old" del /q "%DEPLOY_DIR%\openminer_server.exe.old" >nul 2>nul
            echo [INFO] 已部署服务端 openminer_server.exe -^> %DEPLOY_DIR%
        )
    )
)
rem MinGW 构建时把编译器运行时 DLL（libwinpthread/libgcc/libstdc++）一并带上，
rem 否则在没装 MinGW 运行库的机器上会报“找不到 DLL”；已存在则不覆盖
for %%D in (libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll) do (
    if not exist "%DEPLOY_DIR%\%%D" (
        for /f "delims=" %%F in ('where %%D 2^>nul') do copy /y "%%F" "%DEPLOY_DIR%\%%D" >nul 2>nul
    )
)
echo.
if defined DEPLOY_FAILED (
    echo [错误] 部署失败：请关闭正在运行的游戏后重新执行 build.bat。
    exit /b 1
)
echo [INFO] 部署完成，双击 %DEPLOY_DIR%\%DEPLOY_NAME% 即可开始游戏。
:deploy_done
endlocal
exit /b 0
