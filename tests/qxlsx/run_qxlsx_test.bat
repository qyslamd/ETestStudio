@echo off
set "PATH=D:\Qt\Qt5.12.12\5.12.12\msvc2017_64\lib\cmake\Qt5/../../../bin;%PATH%"
set "QT_PLUGIN_PATH=D:\Qt\Qt5.12.12\5.12.12\msvc2017_64\lib\cmake\Qt5/../../../plugins"
set "BUILD_TYPE=ninja-debug"

echo Running QXlsx unit test...
cd /d %~dp0..\..\build\%BUILD_TYPE%\bin
test_qxlsx_basic.exe
echo.
echo Test completed, press any key to exit...
pause >nul
