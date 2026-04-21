@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul

set PROJECT_ROOT=%~dp0
set PROJECT_ROOT=%PROJECT_ROOT%..

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
echo 清理完成，可以执行build_ninja.bat