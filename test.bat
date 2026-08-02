@echo off

g++ test_all.cpp ^
MemoryManager.cpp ^
ProcessControl.cpp ^
SymbolTable.cpp ^
CommandGenerator.cpp ^
configManager.cpp ^
ICommand.cpp ^
ScreenSpawnerCommand.cpp ^
InitializeCommand.cpp ^
-I. -IcoreDependencies -o test_all

if %errorlevel% neq 0 (
    echo Build Failed
    pause
    exit /b
)

echo Build Successful! Running test_all.exe...
echo.

test_all.exe

pause