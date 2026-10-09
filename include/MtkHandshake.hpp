#pragma once
#include "SerialPort.hpp"
#include "MtkDefines.hpp"
#include "DeviceWatcher.hpp"
#include <memory>
#include <string>

namespace Mtk {

    enum class HandshakeStrategy {
        Auto,             // Tries standard Preloader protocol, then ASCII token fallback
        PreloaderCommand, // Uses CMD_SET_BOOT_MODE (0x10) and CMD_BOOT_META (0xB7)
        DirectBootMeta,   // Uses CMD_BOOT_META opcode directly
        AsciiToken,       // Uses "METAMETA" / "READY" token sequence
    };

    struct HandshakeResult {
        bool success = false;
        TargetHwInfo hwInfo{};
        std::string metaPortName;
        std::string message;
        std::string modemVersion;
    };

    class MtkHandshake {
    public:
        explicit MtkHandshake(SerialPort& serial);
        ~MtkHandshake();

        // Perform full handshake and transition to META mode on the open port
        HandshakeResult Execute(
            HandshakeStrategy strategy = HandshakeStrategy::Auto,
            uint32_t syncTimeoutMs = 3000
        );

        // Verify and communicate with the newly enumerated META mode port
        bool VerifyMetaMode(
            const std::string& metaPortName,
            std::string& outModemVer,
            uint32_t timeoutMs = 5000
        );

        // Individual protocol primitives
        bool SyncHandshake(uint32_t timeoutMs = 2500);
        bool ReadHardwareInfo(TargetHwInfo& outInfo, uint32_t timeoutMs = 1000);
        bool ReadTargetConfig(TargetHwInfo& outInfo, uint32_t timeoutMs = 1000);
        bool SendSetBootMode(BootMode mode, uint32_t timeoutMs = 1000);
        bool SendBootMetaCommand(uint32_t timeoutMs = 1000);
        bool SendAsciiMetaToken(uint32_t timeoutMs = 1000);

    private:
        SerialPort& m_serial;

        bool SendByteAndExpectInverted(uint8_t byteToSend, uint32_t timeoutMs = 200);
        uint16_t ReadUint16BigEndian(uint32_t timeoutMs = 500);
        uint32_t ReadUint32BigEndian(uint32_t timeoutMs = 500);
    };

} // namespace Mtk
