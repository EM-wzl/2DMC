@echo off
chcp 65001 >nul
rem =====================================================================
rem  build.bat - 2DMC 自动构建入口
rem
rem  自动寻找 gamma 文件夹并调用其构建脚本，编译产物自动部署为
rem  Minecraft\Minecraft.exe（游戏运行目录）。
rem
rem  放置位置（任意其一即可自动定位）：
rem    * 2DMC 仓库根目录（本脚本的默认位置）
rem    * 2DMC 的上一级目录（如 D:\cpp）
rem    * gamma 目录或其任意子目录内
rem  也可用环境变量强制指定：set GAMMA_DIR=D:\cpp\2DMC\gamma
rem
rem  用法（参数原样传给 gamma\build.bat）：
rem    build.bat                 默认 Release 构建
rem    build.bat Debug           Debug 构建
rem    build.bat RelWithDebInfo  带调试符号的 Release
rem    build.bat MinGW Release   强制 MinGW-w64 (g++) 构建
rem    build.bat clean           删除 gamma\build 构建目录
rem    build.bat /?              显示本说明
rem  可选环境变量：CMAKE_GEN / JOBS / DEPLOY_DIR / DEPLOY_NAME / GAMMA_DIR
rem  （含义见 gamma\build.bat 头部说明）
rem =====================================================================

setlocal enabledelayedexpansion
set "SD=%~dp0"
if "%SD:~-1%"=="\" set "SD=%SD:~0,-1%"
rem ---- 补全受限环境下缺失的用户环境变量（ccache 等工具需要 LOCALAPPDATA）----
if not defined LOCALAPPDATA if defined USERPROFILE set "LOCALAPPDATA=%USERPROFILE%\AppData\Local"
if not defined APPDATA if defined USERPROFILE set "APPDATA=%USERPROFILE%\AppData\Roaming"

if "%~1"=="/?" (
	echo 自动寻找 gamma 文件夹并编译部署为 Minecraft\Minecraft.exe
	echo 用法: build.bat [Debug^|Release^|RelWithDebInfo^|MinSizeRel^|MinGW^|clean] [/?]
	echo 环境变量: GAMMA_DIR 强制指定 gamma 目录；CMAKE_GEN/JOBS/DEPLOY_DIR/DEPLOY_NAME 见 gamma\build.bat
	exit /b 0
)

set "GAMMA="

rem ---- 0) 环境变量直接指定（优先级最高）----
if defined GAMMA_DIR if exist "%GAMMA_DIR%\build.bat" if exist "%GAMMA_DIR%\CMakeLists.txt" set "GAMMA=%GAMMA_DIR%"

rem ---- 1) 本目录就是 gamma ----
if not defined GAMMA if exist "%SD%\CMakeLists.txt" if exist "%SD%\build.bat" if exist "%SD%\source" set "GAMMA=%SD%"

rem ---- 2) 本目录下（仓库根的常见情况）----
if not defined GAMMA if exist "%SD%\gamma\build.bat" if exist "%SD%\gamma\CMakeLists.txt" set "GAMMA=%SD%\gamma"

rem ---- 3) 向下一层 / 两层找（脚本放在仓库上一级目录时）----
if not defined GAMMA (
	for /d %%D in ("%SD%\*") do (
		if not defined GAMMA if exist "%%D\gamma\build.bat" if exist "%%D\gamma\CMakeLists.txt" set "GAMMA=%%D\gamma"
	)
)
if not defined GAMMA (
	for /d %%D in ("%SD%\*") do for /d %%E in ("%%D\*") do (
		if not defined GAMMA if exist "%%E\gamma\build.bat" if exist "%%E\gamma\CMakeLists.txt" set "GAMMA=%%E\gamma"
	)
)

rem ---- 4) 向上找最多 6 级（脚本放在 gamma 内部或仓库子目录时）----
if not defined GAMMA (
	set "UP=%SD%"
	for /l %%I in (1,1,6) do (
		if not defined GAMMA (
			pushd "!UP!" 2>nul
			if not errorlevel 1 (
				if exist "build.bat" if exist "CMakeLists.txt" if exist "source" set "GAMMA=!CD!"
				if not defined GAMMA if exist "gamma\build.bat" if exist "gamma\CMakeLists.txt" set "GAMMA=!CD!\gamma"
			)
			popd
			set "UP=!UP!\.."
		)
	)
)

rem ---- 校验并规范化路径 ----
if not defined GAMMA (
	echo [错误] 未找到 gamma 文件夹（需同时包含 build.bat 与 CMakeLists.txt）。
	echo        请把本脚本放在 2DMC 根目录 / 上一级目录 / gamma 内，
	echo        或用环境变量指定：set GAMMA_DIR=完整路径
	exit /b 1
)
for %%I in ("!GAMMA!") do set "GAMMA=%%~fI"
echo [INFO] 已定位 gamma 目录：%GAMMA%

rem ---- 调用 gamma 自带构建脚本（参数原样透传）----
pushd "%GAMMA%"
call build.bat %*
set "RC=%ERRORLEVEL%"
popd
if not "%RC%"=="0" (
	echo [错误] gamma 构建失败（退出码 %RC%）。
	exit /b %RC%
)

rem ---- 提示最终游戏位置 ----
for %%I in ("%GAMMA%\..\Minecraft\Minecraft.exe") do (
	if exist "%%~fI" echo [INFO] 游戏已就绪：%%~fI
)
endlocal
exit /b 0
