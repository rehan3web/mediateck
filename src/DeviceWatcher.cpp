#include "DeviceWatcher.hpp"
#include "Logger.hpp"
#include <windows.h>
#include <setupapi.h>
#include <algorithm>
#include <sstream>
#include <thread>
#include <regex>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace Mtk {

    // GUID for Ports (COM & LPT) setup class: {4D36E978-E325-11CE-BFC1-08002BE10318}
    static const GUID GUID_DEVCLASS_PORTS_CONST = {
        0x4D36E978, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 }
    };

    DeviceWatcher::DeviceWatcher() = default;
    DeviceWatcher::~DeviceWatcher() = default;

    DevicePortType DeviceWatcher::ClassifyPort(uint16_t vid, uint16_t pid, const std::string& name) {
        if (vid != MTK_USB_VID) {
            return DevicePortType::None;
        }

        switch (pid) {
            case PID_BROM_USB_PORT:
                return DevicePortType::BromPort;
            case PID_PRELOADER_VCOM:
                return DevicePortType::PreloaderPort;
            case PID_META_PORT_DEFAULT:
            case PID_META_PORT_ALT1:
            case PID_META_PORT_ALT2:
            case PID_META_PORT_ALT3:
                return DevicePortType::MetaPort;
            default:
                break;
        }

        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("preloader") != std::string::npos) {
            return DevicePortType::PreloaderPort;
        }
        if (lower.find("meta") != std::string::npos) {
            return DevicePortType::MetaPort;
        }
        if (lower.find("usb port") != std::string::npos || lower.find("brom") != std::string::npos) {
            return DevicePortType::BromPort;
        }

        return DevicePortType::OtherMtkPort;
    }

    bool DeviceWatcher::ParseVidPid(const std::string& hwId, uint16_t& outVid, uint16_t& outPid) {
        std::string upper = hwId;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        size_t vidPos = upper.find("VID_");
        size_t pidPos = upper.find("PID_");

        if (vidPos == std::string::npos || pidPos == std::string::npos) {
            return false;
        }

        try {
            outVid = static_cast<uint16_t>(std::stoul(upper.substr(vidPos + 4, 4), nullptr, 16));
            outPid = static_cast<uint16_t>(std::stoul(upper.substr(pidPos + 4, 4), nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string DeviceWatcher::ExtractComPortFromFriendlyName(const std::string& friendlyName) {
        // Look for pattern "(COMx)"
        size_t openParen = friendlyName.rfind("(COM");
        if (openParen != std::string::npos) {
            size_t closeParen = friendlyName.find(")", openParen);
            if (closeParen != std::string::npos) {
                return friendlyName.substr(openParen + 1, closeParen - openParen - 1);
            }
        }
        return "";
    }

    std::vector<DeviceInfo> DeviceWatcher::EnumerateAllPorts() {
        std::vector<DeviceInfo> devices;

        HDEVINFO hDevInfo = SetupDiGetClassDevsA(
            &GUID_DEVCLASS_PORTS_CONST,
            nullptr,
            nullptr,
            DIGCF_PRESENT
        );

        if (hDevInfo == INVALID_HANDLE_VALUE) {
            return devices;
        }

        SP_DEVINFO_DATA devInfoData{};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
            char buffer[512] = { 0 };
            DeviceInfo dev{};

            // Query friendly name (e.g. "MediaTek PreLoader USB VCOM Port (COM4)")
            if (SetupDiGetDeviceRegistryPropertyA(
                    hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME,
                    nullptr, reinterpret_cast<PBYTE>(buffer), sizeof(buffer), nullptr)) {
                dev.friendlyName = buffer;
            }

            // Query hardware ID (e.g. "USB\VID_0E8D&PID_2000...")
            buffer[0] = '\0';
            if (SetupDiGetDeviceRegistryPropertyA(
                    hDevInfo, &devInfoData, SPDRP_HARDWAREID,
                    nullptr, reinterpret_cast<PBYTE>(buffer), sizeof(buffer), nullptr)) {
                dev.hardwareId = buffer;
            }

            // Query port name directly from device parameters registry key
            HKEY hDevKey = SetupDiOpenDevRegKey(
                hDevInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

            if (hDevKey != INVALID_HANDLE_VALUE) {
                char portVal[64] = { 0 };
                DWORD portValSize = sizeof(portVal);
                DWORD type = 0;
                if (RegQueryValueExA(hDevKey, "PortName", nullptr, &type,
                                     reinterpret_cast<LPBYTE>(portVal), &portValSize) == ERROR_SUCCESS) {
                    dev.portName = portVal;
                }
                RegCloseKey(hDevKey);
            }

            // Fallback: extract from friendly name if registry query didn't succeed
            if (dev.portName.empty() && !dev.friendlyName.empty()) {
                dev.portName = ExtractComPortFromFriendlyName(dev.friendlyName);
            }

            // Parse VID / PID
            if (!dev.hardwareId.empty()) {
                ParseVidPid(dev.hardwareId, dev.vid, dev.pid);
            }

            dev.type = ClassifyPort(dev.vid, dev.pid, dev.friendlyName);

            if (!dev.portName.empty()) {
                devices.push_back(dev);
            }
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
        return devices;
    }

    std::vector<DeviceInfo> DeviceWatcher::EnumerateMtkPorts() {
        std::vector<DeviceInfo> all = EnumerateAllPorts();
        std::vector<DeviceInfo> mtk;
        for (const auto& dev : all) {
            if (dev.IsMtkDevice()) {
                mtk.push_back(dev);
            }
        }
        return mtk;
    }

    DeviceInfo DeviceWatcher::WaitForMtkDevice(
        uint32_t timeoutSeconds,
        std::function<void(uint32_t secondsElapsed)> onTick
    ) {
        auto startTime = std::chrono::steady_clock::now();
        uint32_t lastReportedSec = 0;

        while (true) {
            auto elapsedSec = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count()
            );

            if (elapsedSec > timeoutSeconds) {
                break;
            }

            if (elapsedSec != lastReportedSec) {
                lastReportedSec = elapsedSec;
                if (onTick) {
                    onTick(elapsedSec);
                }
            }

            std::vector<DeviceInfo> ports = EnumerateMtkPorts();
            for (const auto& dev : ports) {
                // We are looking for BROM or Preloader port
                if (dev.type == DevicePortType::BromPort ||
                    dev.type == DevicePortType::PreloaderPort ||
                    dev.type == DevicePortType::OtherMtkPort) {
                    return dev;
                }
            }

            // 15ms sleep provides low latency without hogging CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }

        return DeviceInfo{};
    }

    DeviceInfo DeviceWatcher::WaitForMetaPort(
        uint32_t timeoutSeconds,
        std::function<void(uint32_t secondsElapsed)> onTick
    ) {
        auto startTime = std::chrono::steady_clock::now();
        uint32_t lastReportedSec = 0;

        while (true) {
            auto elapsedSec = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count()
            );

            if (elapsedSec > timeoutSeconds) {
                break;
            }

            if (elapsedSec != lastReportedSec) {
                lastReportedSec = elapsedSec;
                if (onTick) {
                    onTick(elapsedSec);
                }
            }

            std::vector<DeviceInfo> ports = EnumerateMtkPorts();
            for (const auto& dev : ports) {
                if (dev.type == DevicePortType::MetaPort) {
                    return dev;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        return DeviceInfo{};
    }

} // namespace Mtk
