@echo off

g++ main.cpp  ^
ProcessControl.cpp  ^
CommandProcessor.cpp ^
configManager.cpp ^
FCFSScheduler.cpp ^
RRScheduler.cpp ^
SymbolTable.cpp ^
IETTHeard.cpp ^
CommandGenerator.cpp ^
-I. -o main

if %errorlevel% neq 0 (
    echo Build Failed
    pause
    exit /b
)

echo Build Successful
main.exe

pause