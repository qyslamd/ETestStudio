@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

:: 调用 Visual Studio 的 x64 Native Tools Command Prompt 环境变量设置脚本
call "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

:: 配置CMake环境变量
set PATH=%PATH%;"D:\Program Files\CMake\bin"

:: 配置CMake项目
cmake -S . ^
    --preset ninja-debug ^
    -B build\ninja-debug
   
if errorlevel 1 (
    echo "CMake 配置失败"
    exit /b 1
)

echo 配置成功

:: 构建项目
cmake --build build\ninja-debug
if errorlevel 1 (
    echo "构建失败"
    exit /b 1
)

echo 构建成功!