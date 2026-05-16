@echo off
setlocal

set "PATH=D:/Qt/Qt5.12.12/5.12.12/msvc2017_64/bin;%PATH%"
set "QT_PLUGIN_PATH=D:/Qt/Qt5.12.12/5.12.12/msvc2017_64/plugins"

chcp 65001 >nul

D:/trae_workspace/etest-demo/build/ninja-debug/bin/topology-demo.exe
