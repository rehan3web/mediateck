@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   MediaTek META Mode Tool - Build Script
echo =======================================================

set OUT_DIR=bin
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

:: Check for cl.exe (MSVC)
where cl.exe >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [*] Found Microsoft Visual C++ Compiler (cl.exe)
    echo [*] Compiling mtk_meta_tool.exe...
    cl.exe /nologo /std:c++17 /O2 /EHsc /Iinclude src\main.cpp src\Logger.cpp src\SerialPort.cpp src\DeviceWatcher.cpp src\MtkHandshake.cpp src\DriverInstaller.cpp /link setupapi.lib advapi32.lib /OUT:%OUT_DIR%\mtk_meta_tool.exe
    if %ERRORLEVEL% equ 0 (
        echo [SUCCESS] Build succeeded! Executable located at: %OUT_DIR%\mtk_meta_tool.exe
        goto :done
    ) else (
        echo [ERROR] MSVC compilation failed.
        goto :fail
    )
)

:: Check for g++ (MinGW)
where g++.exe >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [*] Found MinGW GCC Compiler (g++.exe)
    echo [*] Compiling mtk_meta_tool.exe...
    g++.exe -std=c++17 -O2 -Iinclude src/main.cpp src/Logger.cpp src/SerialPort.cpp src/DeviceWatcher.cpp src/MtkHandshake.cpp src/DriverInstaller.cpp -lsetupapi -ladvapi32 -o %OUT_DIR%/mtk_meta_tool.exe
    if %ERRORLEVEL% equ 0 (
        echo [SUCCESS] Build succeeded! Executable located at: %OUT_DIR%\mtk_meta_tool.exe
        goto :done
    ) else (
        echo [ERROR] MinGW GCC compilation failed.
        goto :fail
    )
)

:: Check for clang++
where clang++.exe >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [*] Found Clang Compiler (clang++.exe)
    echo [*] Compiling mtk_meta_tool.exe...
    clang++ -std=c++17 -O2 -Iinclude src/main.cpp src/Logger.cpp src/SerialPort.cpp src/DeviceWatcher.cpp src/MtkHandshake.cpp src/DriverInstaller.cpp -lsetupapi -ladvapi32 -o %OUT_DIR%/mtk_meta_tool.exe
    if %ERRORLEVEL% equ 0 (
        echo [SUCCESS] Build succeeded! Executable located at: %OUT_DIR%\mtk_meta_tool.exe
        goto :done
    ) else (
        echo [ERROR] Clang compilation failed.
        goto :fail
    )
)

:: Check for cmake
where cmake.exe >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [*] Found CMake. Configuring build directory...
    cmake -B build -S .
    cmake --build build --config Release
    if %ERRORLEVEL% equ 0 (
        echo [SUCCESS] Build succeeded via CMake!
        goto :done
    )
)

echo [!] No C++ compiler found directly in PATH.
echo [!] To compile with Visual Studio, open 'x64 Native Tools Command Prompt for VS' and rerun build.bat.
echo [!] Or install MinGW-w64 (g++) and add it to your PATH.
goto :fail

:done
echo =======================================================
exit /b 0

:fail
echo =======================================================
exit /b 1
