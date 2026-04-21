@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

:: 调用 Visual Studio 的 x64 Native Tools Command Prompt 环境变量设置脚本
call "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

:: 配置CMake环境变量
set PATH=%PATH%;"D:\Program Files\CMake\bin"

::  ==============================
:: 临时修改
set PROJECT_ROOT=%~dp0
set PROJECT_ROOT=%PROJECT_ROOT:~0,-1%

:: 删除旧的libpng源码目录
if exist "%PROJECT_ROOT%\3rdparty\libpng-1.6.43" (
    rmdir /s /q "%PROJECT_ROOT%\3rdparty\libpng-1.6.43"
    echo 已删除旧libpng目录
)

:: 解压原始干净的libpng源码包
cd /d "%PROJECT_ROOT%\3rdparty"
tar zxf libpng-1.6.43.tar.gz
echo 已重新解压干净的libpng源码

:: 回到项目根目录
cd /d "%PROJECT_ROOT%"


:: 配置CMake项目
cmake -S . --preset ninja-debug 
   
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