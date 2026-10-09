@echo off
:: Check for administrative rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo =====================================================================
    echo   [IMPORTANT] Please Run this script as Administrator!
    echo   Right-click fix_driver.bat and select "Run as administrator"
    echo =====================================================================
    echo.
    pause
    exit /b 1
)

echo =====================================================================
echo   Fixing MediaTek PreLoader Driver Error (Code 39 Bad Image)
echo =====================================================================
echo.
echo Removing outdated 2011 driver package (oem19.inf)...
pnputil /delete-driver oem19.inf /uninstall /force

echo.
echo =====================================================================
echo   SUCCESS! 
echo   The corrupted 2011 driver has been removed.
echo   Windows will now automatically use the official, stable 
echo   Microsoft USB Serial Device driver (usbser.sys) for your phone!
echo =====================================================================
echo.
pause
