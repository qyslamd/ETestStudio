@echo off
setlocal
set ROOT=%~dp0
set ROOT=%ROOT%..\..\
set ROOT=%ROOT:~0,-1%
set BIN=%ROOT%\build\ninja-debug\bin
set SCRIPT=%ROOT%\tests\lua_bytecode\test.lua
echo Bytecode smoke: compile and run
REM Ensure UTF-8 console for Lua output (ASCII should be stable for testing)
chcp 65001 > nul
"%BIN%\luac.exe" -o "%BIN%\test_bytecode.luac" "%SCRIPT%"
"%BIN%\lua.exe" "%BIN%\test_bytecode.luac"
if errorlevel 1 (
  echo Bytecode smoke FAILED
  exit /b 1
)
echo Bytecode smoke OK
exit /b 0
