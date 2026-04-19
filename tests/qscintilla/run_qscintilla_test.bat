@echo off
echo =======================================
echo Running QScintilla C++ Highlight Tests
echo =======================================
cd ..\..\build\ninja-debug\bin
qscintilla_highlight_test.exe
echo =======================================
echo Test completed!
pause
