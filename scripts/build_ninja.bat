@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

:: 调用 Visual Studio 的 x64 Native Tools Command Prompt 环境变量设置脚本
call "%ETest_VS2019_x64_Native_Bat%"

:: 配置CMake环境变量
set PATH=%PATH%;"%ETest_CMake_Path%"

set PROJECT_ROOT=%~dp0
cd /d "%PROJECT_ROOT%.."

:: 解析构建类型参数
set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=debug"
if /i "%BUILD_TYPE%"=="debug" (
    set "PRESET=ninja-debug"
    set "BUILD_DIR=build\ninja-debug"
)
if /i "%BUILD_TYPE%"=="relwithdebinfo" (
    set "PRESET=ninja-relwithdebinfo"
    set "BUILD_DIR=build\ninja-relwithdebinfo"
)
if /i "%BUILD_TYPE%"=="release" (
    set "PRESET=ninja-release"
    set "BUILD_DIR=build\ninja-release"
)

if not defined PRESET (
    echo 未知构建类型: %BUILD_TYPE%
    echo 用法: build_ninja.bat [debug^|relwithdebinfo^|release]
    exit /b 1
)

echo 构建类型: %BUILD_TYPE% (%PRESET%)

:: 配置CMake项目
cmake -S . --preset %PRESET%

if errorlevel 1 (
    echo "CMake 配置失败"
    exit /b 1
)

echo 配置成功

:: 构建项目
cmake --build %BUILD_DIR%
if errorlevel 1 (
    echo "构建失败"
    exit /b 1
)

echo 构建成功!