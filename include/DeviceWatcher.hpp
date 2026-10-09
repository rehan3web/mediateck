#pragma once
#include "MtkDefines.hpp"
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace Mtk {

    struct DeviceInfo {
        std::string portName;     // e.g. "COM4"
        std::string friendlyName; // e.g. "MediaTek PreLoader USB VCOM Port (COM4)"
        std::string hardwareId;   // e.g. "USB\\VID_0E8D&PID_2000&REV_0100"
        uint16_t vid = 0;
        uint16_t pid = 0;
        DevicePortType type = DevicePortType::None;

        bool IsMtkDevice() const {
            return vid == MTK_USB_VID;
        }
    };

    class DeviceWatcher {
    public:
        DeviceWatcher();
        ~DeviceWatcher();

        // Enumerate all currently present COM ports matching MediaTek VID/PID
        static std::vector<DeviceInfo> EnumerateMtkPorts();

        // Enumerate all active serial COM ports on the system
        static std::vector<DeviceInfo> EnumerateAllPorts();

        // Block and wait for a MediaTek Preloader or BROM device to appear
        // Returns the detected device info, or an empty device info on timeout
        DeviceInfo WaitForMtkDevice(
            uint32_t timeoutSeconds = 60,
            std::function<void(uint32_t secondsElapsed)> onTick = nullptr
        );

        // Wait specifically for a META mode port to enumerate after handshake
        DeviceInfo WaitForMetaPort(
            uint32_t timeoutSeconds = 30,
            std::function<void(uint32_t secondsElapsed)> onTick = nullptr
        );

    private:
        static DevicePortType ClassifyPort(uint16_t vid, uint16_t pid, const std::string& name);
        static bool ParseVidPid(const std::string& hwId, uint16_t& outVid, uint16_t& outPid);
        static std::string ExtractComPortFromFriendlyName(const std::string& friendlyName);
    };

} // namespace Mtk
