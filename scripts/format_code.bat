@echo off
setlocal enabledelayedexpansion

set CLANG_FORMAT="D:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/bin/clang-format.exe"

if not exist %CLANG_FORMAT% (
    echo Error: clang-format.exe not found at %CLANG_FORMAT%
    exit /b 1
)

set EXCLUDE_DIRS=build cmake-build out Release Debug

echo Formatting .cpp and .h files...

for /r %%f in (*.cpp *.h) do (
    set "FULLPATH=%%~ff"
    set "SHOULD_SKIP=0"
    for %%d in (%EXCLUDE_DIRS%) do (
        echo !FULLPATH! | findstr /i /c:"\\%%d\\" >nul
        if !errorlevel!==0 set "SHOULD_SKIP=1"
    )
    if !SHOULD_SKIP!==0 (
        echo Formatting: %%f
        %CLANG_FORMAT% -i "%%f"
    ) else (
        echo Skipping: %%f
    )
)

echo Done!
endlocal
