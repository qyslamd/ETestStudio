@echo off
setlocal

set "PATH=D:/Qt/Qt5.14.2/5.14.2/msvc2017_64/bin;%PATH%"
set "QT_PLUGIN_PATH=D:/Qt/Qt5.14.2/5.14.2/msvc2017_64/plugins"

chcp 65001 >nul

D:/trae_workspace/etest-demo/build/ninja-debug/bin/eye-tracking-demo.exe
