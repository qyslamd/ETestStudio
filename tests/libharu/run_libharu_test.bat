@echo off
echo Running libharu unit test...
cd /d %~dp0..\..\build\bin
test_libharu_basic.exe
echo.
echo Test completed, press any key to exit...
pause >nul
