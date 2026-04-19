@echo off
setlocal
set ROOT=%~dp0
set ROOT=%ROOT%..\..\
set ROOT=%ROOT:~0,-1%
set BIN="%ROOT%\\build\\ninja-debug\\bin"
set SCRIPT="%ROOT%\\tests\\lua_bytecode\\test.lua"
echo Running Lua smoke test: compile to bytecode and execute
REM Ensure UTF-8 console for Lua output (ASCII should be stable for testing)
chcp 65001 > nul
"%BIN%\\luac.exe" -o "%BIN%\\test_bytecode.luac" %SCRIPT%
"%BIN%\\lua.exe" "%BIN%\\test_bytecode.luac%"
if errorlevel 1 (
  echo Lua bytecode smoke test FAILED
  exit /b 1
) else (
  echo Lua bytecode smoke test PASSED
  exit /b 0
)
