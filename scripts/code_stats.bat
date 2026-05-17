@echo off
setlocal enabledelayedexpansion

set CLOC="D:/trae_workspace/etest-demo/3rdparty/cloc-2.06/cloc-2.06.exe"

if not exist %CLOC% (
    echo Error: cloc.exe not found at %CLOC%
    exit /b 1
)

REM cloc options reference:
REM   --quiet               Suppress info messages, show final report only
REM   --csv / --json / --yaml / --xml
REM                         Output in CSV/JSON/YAML/XML format
REM   --by-file             Show per-file detail (omit for lang-only summary)
REM   --by-file-by-lang     Show both per-lang and per-file results
REM   --exclude-dir=<dirs>  Exclude directories (comma-separated)
REM   --exclude-ext=<exts>  Exclude extensions (comma-separated)
REM   --include-ext=<exts>  Only count specified extensions
REM   --exclude-lang=<langs> Exclude languages
REM   --report-file=<file>  Write results to file (--out is alias)
REM   --diff <set1> <set2>  Diff two versions
REM   --git                 Accept commit hashes or branch names
REM   --vcs=git             Use git ls-files (respects .gitignore)
REM   --progress            Show progress
REM   --sum-one             Show SUM line even for single file
REM   --by-percent X        Scale results by X percent
REM   Full list: cloc --help

echo ========================================
echo  etest-demo Code Line Count
echo ========================================
echo.


%CLOC% --exclude-dir=3rdparty,build,.git,cmake-build,out,Release,Debug ^
       --exclude-ext=md,txt,bat,ps1,json,ini,iss,svg ^
       "%~dp0..\src" "%~dp0..\tests" "%~dp0..\examples" 2>&1

echo.
echo Done!
endlocal
