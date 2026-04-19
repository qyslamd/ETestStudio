@echo off
setlocal
set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set BIN="%ROOT%\build\ninja-debug\bin"
set SCRIPT="%ROOT%\tests\lua_bytecode\test.lua"
echo Bytecode smoke: compile and run
"%BIN%\luac.exe" -o "%BIN%\test_bytecode.luac" %SCRIPT%
"%BIN%\lua.exe" "%BIN%\test_bytecode.luac"  
if errorlevel 1 (
  echo Bytecode smoke FAILED
  exit /b 1
)
echo Bytecode smoke OK
exit /b 0
