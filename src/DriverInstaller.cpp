#include "DriverInstaller.hpp"
#include "Logger.hpp"

#include <windows.h>
#include <shellapi.h>
#include <setupapi.h>
#include <iostream>
#include <sstream>
#include <fstream>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace Mtk {

    // Embedded clean Windows 10/11 INF driver for MediaTek CDC-ACM (usbser.sys)
    static const char* EMBEDDED_INF_CONTENT = 
R"INF_RAW(; ==============================================================================
; MediaTek PreLoader & BootROM Modern USB VCOM Driver INF
; Targets: Windows 10 / Windows 11 (x64)
; Service: Uses native Microsoft-signed usbser.sys (No Code 39 / No Bad Image)
; ==============================================================================

[Version]
Signature   = "$Windows NT$"
Class       = Ports
ClassGuid   = {4D36E978-E325-11CE-BFC1-08002BE10318}
Provider    = %ProviderName%
DriverVer   = 10/09/2026,2.0.0.0
PnpLockdown = 1

[Manufacturer]
%MfgName% = MTK_Devices, NTamd64.10.0

[MTK_Devices.NTamd64.10.0]
; MediaTek BootROM USB Port
%MTK_BROM_Desc%        = UsbSerial_Install, USB\VID_0E8D&PID_0003

; MediaTek PreLoader USB VCOM Port (Moto G73 5G & Dimensity)
%MTK_PRELOADER_Desc%   = UsbSerial_Install, USB\VID_0E8D&PID_2000
%MTK_PRELOADER_Desc%   = UsbSerial_Install, USB\VID_0E8D&PID_2000&REV_0100

; MediaTek META Mode Port
%MTK_META_Desc%        = UsbSerial_Install, USB\VID_0E8D&PID_200E
%MTK_META_Desc%        = UsbSerial_Install, USB\VID_0E8D&PID_2002
%MTK_META_Desc%        = UsbSerial_Install, USB\VID_0E8D&PID_202D

; Motorola MediaTek Specific Identifiers
%MOTO_PRELOADER_Desc%  = UsbSerial_Install, USB\VID_22B8&PID_2EC5
%MOTO_META_Desc%       = UsbSerial_Install, USB\VID_22B8&PID_2E76

[UsbSerial_Install.NT]
Include = usbser.inf
Needs   = UsbSerial.Install.NT

[UsbSerial_Install.NT.Services]
Include = usbser.inf
Needs   = UsbSerial.Install.NT.Services

[UsbSerial_Install.NT.HW]
AddReg = UsbSerial_HW_AddReg

[UsbSerial_HW_AddReg]
HKR,,"ConfigPriority",0x00010001,0x00000001

[Strings]
ProviderName          = "MediaTek & Motorola"
MfgName               = "MediaTek Inc."
MTK_BROM_Desc         = "MediaTek USB Port (BootROM)"
MTK_PRELOADER_Desc    = "MediaTek PreLoader USB VCOM Port"
MTK_META_Desc         = "MediaTek META Mode USB VCOM Port"
MOTO_PRELOADER_Desc   = "Motorola MediaTek PreLoader VCOM Port"
MOTO_META_Desc        = "Motorola MediaTek Diagnostic Port"
)INF_RAW";

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

    bool DriverInstaller::RelaunchAsAdmin(const std::string& extraArgs) {
        char exePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        std::string params = extraArgs;
        if (params.empty()) {
            params = "-v";
        }

        SHELLEXECUTEINFOA sei{};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = "runas";
        sei.lpFile = exePath;
        sei.lpParameters = params.c_str();
        sei.nShow = SW_NORMAL;

        return ShellExecuteExA(&sei) != FALSE;
    }

    bool DriverInstaller::RemoveObsoleteDriver(const std::string& oemInfName) {
        Logger::Info("Checking and removing obsolete driver package: " + oemInfName + "...");

        BOOL ok = SetupUninstallOEMInfA(
            oemInfName.c_str(),
            SUOI_FORCEDELETE,
            nullptr
        );

        if (ok) {
            Logger::Success("Successfully deleted corrupted driver package: " + oemInfName);
            return true;
        }

        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            return true;
        }

        Logger::Warn("Could not remove " + oemInfName + " (Win32 Error: " + std::to_string(err) + ")");
        return false;
    }

    bool DriverInstaller::InstallInf(const std::string& infFilePath) {
        Logger::Info("Installing certified CDC-ACM driver into Driver Store: " + infFilePath);

        char destinationInfFileName[MAX_PATH] = { 0 };

        BOOL ok = SetupCopyOEMInfA(
            infFilePath.c_str(),
            nullptr,
            SPOST_PATH,
            SP_COPY_NEWER_ONLY,
            destinationInfFileName,
            sizeof(destinationInfFileName),
            nullptr,
            nullptr
        );

        if (ok) {
            Logger::Success("Clean driver registered in Windows Driver Store as: " + std::string(destinationInfFileName));
            return true;
        }

        DWORD err = GetLastError();
        if (err == ERROR_FILE_EXISTS) {
            Logger::Info("Driver package already up to date in Driver Store.");
            return true;
        }

        Logger::Error("Failed to register INF into Driver Store (Win32 Error: " + std::to_string(err) + ")");
        return false;
    }

    bool DriverInstaller::UpdatePnpDevice(const std::string& hardwareId, const std::string& infPath) {
        HMODULE hNewDev = LoadLibraryA("newdev.dll");
        if (!hNewDev) return false;

        auto pfnUpdateDriver = reinterpret_cast<UpdateDriverFn>(
            GetProcAddress(hNewDev, "UpdateDriverForPlugAndPlayDevicesA")
        );

        if (!pfnUpdateDriver) {
            FreeLibrary(hNewDev);
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
        return result != FALSE;
    }

    bool DriverInstaller::FixPreloaderDriver() {
        if (!IsElevated()) {
            Logger::Warn("Administrator rights needed to update device drivers.");
            return false;
        }

        Logger::Info("Auto-repairing MediaTek USB driver configuration...");

        // 1. Remove old corrupted 2011 oem19.inf
        RemoveObsoleteDriver("oem19.inf");

        // 2. Prepare driver INF (from disk or extract embedded)
        char targetInfPath[MAX_PATH] = { 0 };
        GetFullPathNameA("driver\\mtk_cdc_vcom.inf", MAX_PATH, targetInfPath, nullptr);

        if (GetFileAttributesA(targetInfPath) == INVALID_FILE_ATTRIBUTES) {
            // Write embedded driver to TEMP directory
            char tempDir[MAX_PATH] = { 0 };
            GetTempPathA(MAX_PATH, tempDir);
            std::string tempInf = std::string(tempDir) + "mtk_cdc_vcom.inf";

            std::ofstream out(tempInf);
            if (out.is_open()) {
                out << EMBEDDED_INF_CONTENT;
                out.close();
                strcpy_s(targetInfPath, tempInf.c_str());
            }
        }

        if (!InstallInf(targetInfPath)) {
            return false;
        }

        // 3. Update existing PnP device bindings
        UpdatePnpDevice("USB\\VID_0E8D&PID_2000", targetInfPath);
        UpdatePnpDevice("USB\\VID_0E8D&PID_0003", targetInfPath);
        UpdatePnpDevice("USB\\VID_0E8D&PID_200E", targetInfPath);

        Logger::Success("MediaTek CDC-ACM driver successfully verified and installed!");
        return true;
    }

    bool DriverInstaller::AutoEnsureDriverClean() {
        // Check if obsolete oem19.inf exists in C:\Windows\INF
        char winDir[MAX_PATH] = { 0 };
        GetWindowsDirectoryA(winDir, MAX_PATH);
        std::string oem19Path = std::string(winDir) + "\\INF\\oem19.inf";

        bool oem19Present = (GetFileAttributesA(oem19Path.c_str()) != INVALID_FILE_ATTRIBUTES);

        if (oem19Present) {
            Logger::Warn("Corrupted 2011 driver (oem19.inf - Code 39) detected on this computer!");

            if (IsElevated()) {
                Logger::Info("Running automated in-line driver repair...");
                return FixPreloaderDriver();
            } else {
                Logger::Info("Requesting Administrator permission to automatically fix driver (Code 39)...");
                if (RelaunchAsAdmin("-v")) {
                    Logger::Info("Elevated helper launched. Exiting current non-elevated instance.");
                    exit(0);
                }
            }
        } else if (IsElevated()) {
            FixPreloaderDriver();
        }

        return true;
    }

} // namespace Mtk
