@echo off
rem builds + runs the Part A tests.
rem run this from a "x64 Native Tools Command Prompt for VS 2022"
rem (that's the one that has cl.exe on the path)

cd /d "%~dp0"

echo building planner_tests...
cl /nologo /EHsc /std:c++17 /W4 /Fe:planner_tests.exe planner_tests.cpp ..\sim\WorldState.cpp ..\sim\Action.cpp ..\sim\Planner.cpp
if errorlevel 1 exit /b 1

echo building console_run...
cl /nologo /EHsc /std:c++17 /W4 /Fe:console_run.exe console_main.cpp ..\sim\WorldState.cpp ..\sim\Action.cpp ..\sim\Planner.cpp ..\sim\Simulation.cpp
if errorlevel 1 exit /b 1

del *.obj >nul 2>nul

echo.
echo ==== planner tests ====
planner_tests.exe
if errorlevel 1 exit /b 1

echo.
echo ==== 60 second headless run ====
console_run.exe
if errorlevel 1 exit /b 1

echo.
echo everything passed.
