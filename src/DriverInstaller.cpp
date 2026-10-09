#include "DriverInstaller.hpp"
#include "Logger.hpp"

#include <windows.h>
#include <setupapi.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace Mtk {

    typedef BOOL (WINAPI *UpdateDriverFn)(
        HWND hwndParent,
        LPCSTR HardwareId,
        LPCSTR FullInfPath,
        DWORD InstallFlags,
        PBOOL bRebootRequired
    );

    bool DriverInstaller::IsElevated() {
        BOOL elevated = FALSE;
        HANDLE hToken = nullptr;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION elevation{};
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
                elevated = elevation.TokenIsElevated;
            }
            CloseHandle(hToken);
        }
        return elevated != FALSE;
    }

    bool DriverInstaller::RemoveObsoleteDriver(const std::string& oemInfName) {
        Logger::Info("Attempting to uninstall obsolete driver package: " + oemInfName + "...");

        // SetupUninstallOEMInfA removes the driver from C:\Windows\INF
        BOOL ok = SetupUninstallOEMInfA(
            oemInfName.c_str(),
            SUOI_FORCEDELETE,
            nullptr
        );

        if (ok) {
            Logger::Success("Successfully deleted obsolete driver: " + oemInfName);
            return true;
        }

        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            Logger::Info("Driver " + oemInfName + " was already removed or does not exist.");
            return true;
        }

        Logger::Warn("SetupUninstallOEMInf returned error " + std::to_string(err) + " (Run as Administrator required)");
        return false;
    }

    bool DriverInstaller::InstallInf(const std::string& infFilePath) {
        Logger::Info("Installing modern MediaTek CDC-ACM driver into Windows Driver Store: " + infFilePath);

        char destinationInfFileName[MAX_PATH] = { 0 };

        // Copy INF into %SystemRoot%\INF and register it
        BOOL ok = SetupCopyOEMInfA(
            infFilePath.c_str(),
            nullptr,               // OEM source location (null = current inf directory)
            SPOST_PATH,            // OEM source media type
            SP_COPY_NEWER_ONLY,    // Copy style
            destinationInfFileName,
            sizeof(destinationInfFileName),
            nullptr,
            nullptr
        );

        if (ok) {
            Logger::Success("Driver registered in Driver Store as: " + std::string(destinationInfFileName));
            return true;
        }

        DWORD err = GetLastError();
        if (err == ERROR_FILE_EXISTS) {
            Logger::Info("Driver package is already up-to-date in Driver Store.");
            return true;
        }

        Logger::Error("Failed to copy INF to Driver Store (Win32 Error: " + std::to_string(err) + ")");
        return false;
    }

    bool DriverInstaller::UpdatePnpDevice(const std::string& hardwareId, const std::string& infPath) {
        Logger::Info("Binding hardware ID [" + hardwareId + "] to driver: " + infPath);

        HMODULE hNewDev = LoadLibraryA("newdev.dll");
        if (!hNewDev) {
            Logger::Error("Could not load newdev.dll");
            return false;
        }

        auto pfnUpdateDriver = reinterpret_cast<UpdateDriverFn>(
            GetProcAddress(hNewDev, "UpdateDriverForPlugAndPlayDevicesA")
        );

        if (!pfnUpdateDriver) {
            FreeLibrary(hNewDev);
            Logger::Error("Could not find UpdateDriverForPlugAndPlayDevicesA in newdev.dll");
            return false;
        }

        BOOL rebootRequired = FALSE;
        BOOL result = pfnUpdateDriver(
            nullptr,
            hardwareId.c_str(),
            infPath.c_str(),
            1, // INSTALLFLAG_FORCE
            &rebootRequired
        );

        FreeLibrary(hNewDev);

        if (result) {
            Logger::Success("Hardware [" + hardwareId + "] successfully updated to clean driver!");
            return true;
        }

        DWORD err = GetLastError();
        Logger::Debug("UpdateDriverForPlugAndPlayDevices returned error: " + std::to_string(err));
        return false;
    }

    bool DriverInstaller::FixPreloaderDriver() {
        std::cout << "\n================================================================================" << std::endl;
        std::cout << "          MediaTek PreLoader Driver Auto-Fix Utility (C++)" << std::endl;
        std::cout << "================================================================================" << std::endl;

        if (!IsElevated()) {
            Logger::Error("Administrator privileges required to install or remove Windows drivers.");
            Logger::Warn("Please run this tool or Command Prompt as Administrator!");
            return false;
        }

        // Step 1: Remove corrupted 2011 oem19.inf
        RemoveObsoleteDriver("oem19.inf");

        // Step 2: Install clean mtk_cdc_vcom.inf
        char fullInfPath[MAX_PATH] = { 0 };
        GetFullPathNameA("driver\\mtk_cdc_vcom.inf", MAX_PATH, fullInfPath, nullptr);

        if (GetFileAttributesA(fullInfPath) == INVALID_FILE_ATTRIBUTES) {
            Logger::Error("Could not locate driver file: " + std::string(fullInfPath));
            return false;
        }

        if (!InstallInf(fullInfPath)) {
            return false;
        }

        // Step 3: Update existing devices
        UpdatePnpDevice("USB\\VID_0E8D&PID_2000", fullInfPath);
        UpdatePnpDevice("USB\\VID_0E8D&PID_0003", fullInfPath);
        UpdatePnpDevice("USB\\VID_0E8D&PID_200E", fullInfPath);

        Logger::Success("Driver repair completed successfully! Code 39 resolved.");
        return true;
    }

} // namespace Mtk
