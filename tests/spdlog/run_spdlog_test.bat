@echo off
echo Running spdlog unit test...
cd /d %~dp0..\..\build\bin
test_spdlog_basic.exe
echo.
echo Test completed, press any key to exit...
pause >nul
