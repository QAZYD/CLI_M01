@echo off
cls

:: Check if the executable exists before trying to run it
if not exist main.exe (
    echo Error: main.exe not found! 
    echo Please run your compilation batch file first to build the program.
    echo.
    pause
    exit /b
)

echo Launching OS Emulator...
echo ===================================
main.exe
echo ===================================
echo Program terminated.
echo.

pause