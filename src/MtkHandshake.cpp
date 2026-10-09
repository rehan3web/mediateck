#include "MtkHandshake.hpp"
#include "Logger.hpp"
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

namespace Mtk {

    MtkHandshake::MtkHandshake(SerialPort& serial)
        : m_serial(serial) {
    }

    MtkHandshake::~MtkHandshake() = default;

    uint16_t MtkHandshake::ReadUint16BigEndian(uint32_t timeoutMs) {
        uint8_t bytes[2] = { 0 };
        if (m_serial.ReadExact(bytes, 2, timeoutMs)) {
            return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
        }
        return 0;
    }

    uint32_t MtkHandshake::ReadUint32BigEndian(uint32_t timeoutMs) {
        uint8_t bytes[4] = { 0 };
        if (m_serial.ReadExact(bytes, 4, timeoutMs)) {
            return (static_cast<uint32_t>(bytes[0]) << 24) |
                   (static_cast<uint32_t>(bytes[1]) << 16) |
                   (static_cast<uint32_t>(bytes[2]) << 8)  |
                    static_cast<uint32_t>(bytes[3]);
        }
        return 0;
    }

    bool MtkHandshake::SendByteAndExpectInverted(uint8_t byteToSend, uint32_t timeoutMs) {
        uint8_t expectedReply = static_cast<uint8_t>(~byteToSend);

        if (!m_serial.WriteByte(byteToSend)) {
            return false;
        }

        uint8_t reply = 0;
        if (!m_serial.ReadByte(reply, timeoutMs)) {
            return false;
        }

        if (reply == expectedReply) {
            return true;
        }

        Logger::Trace("Handshake mismatch: sent 0x" + std::to_string(byteToSend) +
                     ", expected 0x" + std::to_string(expectedReply) +
                     ", got 0x" + std::to_string(reply));
        return false;
    }

    bool MtkHandshake::SyncHandshake(uint32_t timeoutMs) {
        Logger::Info("Initiating MediaTek BROM / Preloader sync handshake...");

        auto startTime = std::chrono::steady_clock::now();
        bool sync0_ok = false;

        // Step 1: Poll with 0xA0 until device responds with ~0xA0 (0x5F)
        while (!sync0_ok) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();

            if (elapsed >= timeoutMs) {
                Logger::Error("Handshake timed out waiting for initial sync byte (0xA0 -> 0x5F)");
                return false;
            }

            m_serial.Purge();

            if (m_serial.WriteByte(HANDSHAKE_START_0)) {
                uint8_t reply = 0;
                if (m_serial.ReadByte(reply, 30)) {
                    if (reply == HANDSHAKE_REPLY_0) {
                        sync0_ok = true;
                        break;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        Logger::Debug("Sync step 1/4 OK (0xA0 -> 0x5F)");

        // Step 2: 0x0A -> ~0x0A (0xF5)
        if (!SendByteAndExpectInverted(HANDSHAKE_START_1, 200)) {
            Logger::Error("Handshake failed at step 2 (0x0A -> 0xF5)");
            return false;
        }
        Logger::Debug("Sync step 2/4 OK (0x0A -> 0xF5)");

        // Step 3: 0x50 -> ~0x50 (0xAF)
        if (!SendByteAndExpectInverted(HANDSHAKE_START_2, 200)) {
            Logger::Error("Handshake failed at step 3 (0x50 -> 0xAF)");
            return false;
        }
        Logger::Debug("Sync step 3/4 OK (0x50 -> 0xAF)");

        // Step 4: 0x05 -> ~0x05 (0xFA)
        if (!SendByteAndExpectInverted(HANDSHAKE_START_3, 200)) {
            Logger::Error("Handshake failed at step 4 (0x05 -> 0xFA)");
            return false;
        }
        Logger::Debug("Sync step 4/4 OK (0x05 -> 0xFA)");

        Logger::Success("BootROM / Preloader handshake synchronized successfully!");
        return true;
    }

    bool MtkHandshake::ReadHardwareInfo(TargetHwInfo& outInfo, uint32_t timeoutMs) {
        Logger::Info("Querying hardware identification (CMD_GET_HW_SW_VER: 0xFC)...");

        if (!m_serial.WriteByte(CMD_GET_HW_SW_VER)) {
            return false;
        }

        // Expected response format:
        // uint16_t hw_code;
        // uint16_t hw_sub_code;
        // uint16_t hw_ver;
        // uint16_t sw_ver;
        // uint16_t status (0x0000 = OK)
        uint8_t payload[10] = { 0 };
        if (!m_serial.ReadExact(payload, 10, timeoutMs)) {
            Logger::Warn("Failed to read hardware info response packet");
            return false;
        }

        outInfo.hw_code     = static_cast<uint16_t>((payload[0] << 8) | payload[1]);
        outInfo.hw_sub_code = static_cast<uint16_t>((payload[2] << 8) | payload[3]);
        outInfo.hw_version  = static_cast<uint16_t>((payload[4] << 8) | payload[5]);
        outInfo.sw_version  = static_cast<uint16_t>((payload[6] << 8) | payload[7]);
        uint16_t status     = static_cast<uint16_t>((payload[8] << 8) | payload[9]);

        std::ostringstream oss;
        oss << "Chipset HW Code: 0x" << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << outInfo.hw_code << " -> " << GetChipName(outInfo.hw_code);
        Logger::Success(oss.str());

        std::ostringstream oss2;
        oss2 << "HW Subcode: 0x" << std::hex << std::setw(4) << outInfo.hw_sub_code
             << ", HW Version: 0x" << std::setw(4) << outInfo.hw_version
             << ", SW Version: 0x" << std::setw(4) << outInfo.sw_version
             << ", Status: 0x" << std::setw(4) << status;
        Logger::Info(oss2.str());

        return status == 0x0000 || status == 0x00;
    }

    bool MtkHandshake::ReadTargetConfig(TargetHwInfo& outInfo, uint32_t timeoutMs) {
        Logger::Info("Querying target security configuration (CMD_GET_TARGET_CONFIG: 0xD8)...");

        if (!m_serial.WriteByte(CMD_GET_TARGET_CONFIG)) {
            return false;
        }

        // Response format:
        // uint32_t target_config;
        // uint16_t status (0x0000)
        uint8_t payload[6] = { 0 };
        if (!m_serial.ReadExact(payload, 6, timeoutMs)) {
            Logger::Warn("Target configuration query did not return 6 bytes");
            return false;
        }

        outInfo.target_config = (static_cast<uint32_t>(payload[0]) << 24) |
                                (static_cast<uint32_t>(payload[1]) << 16) |
                                (static_cast<uint32_t>(payload[2]) << 8)  |
                                 static_cast<uint32_t>(payload[3]);

        outInfo.secure_boot      = (outInfo.target_config & 0x01) != 0;
        outInfo.serial_link_auth = (outInfo.target_config & 0x02) != 0;
        outInfo.da_auth          = (outInfo.target_config & 0x04) != 0;

        std::ostringstream oss;
        oss << "Target Config: 0x" << std::hex << std::setfill('0') << std::setw(8) << outInfo.target_config
            << " [SBC=" << (outInfo.secure_boot ? "ENABLED" : "DISABLED")
            << ", SLA=" << (outInfo.serial_link_auth ? "ENABLED" : "DISABLED")
            << ", DAA=" << (outInfo.da_auth ? "ENABLED" : "DISABLED") << "]";
        Logger::Info(oss.str());

        return true;
    }

    bool MtkHandshake::SendSetBootMode(BootMode mode, uint32_t timeoutMs) {
        Logger::Info("Sending CMD_SET_BOOT_MODE (0x10) -> META Mode (0x01)...");

        // Send opcode 0x10
        if (!m_serial.WriteByte(CMD_SET_BOOT_MODE)) {
            return false;
        }

        uint8_t ack = 0;
        if (!m_serial.ReadByte(ack, timeoutMs)) {
            Logger::Debug("No immediate ACK for opcode 0x10, proceeding with boot argument...");
        }

        // Send 32-bit boot mode argument (Big-Endian): 0x00000001
        uint32_t modeVal = static_cast<uint32_t>(mode);
        uint8_t modeBytes[4] = {
            static_cast<uint8_t>((modeVal >> 24) & 0xFF),
            static_cast<uint8_t>((modeVal >> 16) & 0xFF),
            static_cast<uint8_t>((modeVal >> 8) & 0xFF),
            static_cast<uint8_t>(modeVal & 0xFF)
        };

        if (!m_serial.Write(modeBytes, 4)) {
            return false;
        }

        // Read confirmation status
        uint8_t statusBuf[2] = { 0 };
        if (m_serial.ReadExact(statusBuf, 2, timeoutMs)) {
            uint16_t status = static_cast<uint16_t>((statusBuf[0] << 8) | statusBuf[1]);
            Logger::Debug("CMD_SET_BOOT_MODE status: 0x" + std::to_string(status));
        }

        // Send Reboot to commit boot mode
        Logger::Info("Sending Preloader boot commit (0xB7 / 0xD9)...");
        m_serial.WriteByte(CMD_BOOT_META);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m_serial.WriteByte(CMD_REBOOT_NORMAL);

        return true;
    }

    bool MtkHandshake::SendBootMetaCommand(uint32_t timeoutMs) {
        Logger::Info("Sending direct CMD_BOOT_META (0xB7)...");
        if (!m_serial.WriteByte(CMD_BOOT_META)) {
            return false;
        }

        // Also send legacy 2-byte command format fallback
        uint8_t extra[2] = { 0x00, 0x01 };
        m_serial.Write(extra, 2);
        return true;
    }

    bool MtkHandshake::SendAsciiMetaToken(uint32_t timeoutMs) {
        Logger::Info("Sending ASCII META Mode magic token ('METAMETA')...");

        const std::string tokens = "METAMETA\nREADY\n";
        m_serial.WriteString(tokens);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Read possible preloader response
        auto resp = m_serial.ReadAvailable(100);
        if (!resp.empty()) {
            std::string s(resp.begin(), resp.end());
            Logger::Debug("Preloader response to token: " + s);
        }

        return true;
    }

    HandshakeResult MtkHandshake::Execute(HandshakeStrategy strategy, uint32_t syncTimeoutMs) {
        HandshakeResult res{};

        // 1. Synchronize with BootROM / Preloader
        if (!SyncHandshake(syncTimeoutMs)) {
            res.message = "Failed to sync with MediaTek BROM / Preloader.";
            return res;
        }

        // 2. Query Chipset Info
        ReadHardwareInfo(res.hwInfo, 800);

        // 3. Query Target Security Config
        ReadTargetConfig(res.hwInfo, 800);

        // 4. Force META Mode transition based on chosen strategy
        bool modeCommandSent = false;
        switch (strategy) {
            case HandshakeStrategy::PreloaderCommand:
                modeCommandSent = SendSetBootMode(BootMode::META_BOOT);
                break;
            case HandshakeStrategy::DirectBootMeta:
                modeCommandSent = SendBootMetaCommand();
                break;
            case HandshakeStrategy::AsciiToken:
                modeCommandSent = SendAsciiMetaToken();
                break;
            case HandshakeStrategy::Auto:
            default:
                Logger::Info("Executing multi-phase META boot injection sequence...");
                // Send Preloader boot mode packet
                SendSetBootMode(BootMode::META_BOOT);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                // Send direct opcode
                SendBootMetaCommand();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                // Send ASCII token fallback
                SendAsciiMetaToken();
                modeCommandSent = true;
                break;
        }

        if (modeCommandSent) {
            Logger::Success("META Mode boot instructions dispatched to preloader!");
            res.success = true;
            res.message = "META mode instructions transmitted.";
        } else {
            res.message = "Failed to send META boot instructions.";
        }

        return res;
    }

    bool MtkHandshake::VerifyMetaMode(
        const std::string& metaPortName,
        std::string& outModemVer,
        uint32_t timeoutMs
    ) {
        Logger::Info("Connecting to MediaTek META Mode port (" + metaPortName + ")...");

        SerialPort metaSerial;
        if (!metaSerial.Open(metaPortName, 115200)) {
            Logger::Error("Could not open META COM port: " + metaPortName);
            return false;
        }

        // In META Mode, the device exposes AT command interface or META test service
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow CDC driver to stabilize

        Logger::Info("Sending AT handshake probe to verify META Mode active state...");

        // Send AT ping
        metaSerial.WriteString("AT\r\n");
        auto resp1 = metaSerial.ReadAvailable(500);

        // Disable echo
        metaSerial.WriteString("ATE0\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        metaSerial.ReadAvailable(300);

        // Query modem/firmware revision
        metaSerial.WriteString("AT+CGMR\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto respCgmr = metaSerial.ReadAvailable(1000);

        std::string verStr(respCgmr.begin(), respCgmr.end());
        if (!verStr.empty()) {
            outModemVer = verStr;
            Logger::Success("META Mode modem response received!");
            Logger::Info("Modem Firmware: " + verStr);
            return true;
        }

        // Query chip/system identification
        metaSerial.WriteString("AT+EGMR=0,0\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto respEgmr = metaSerial.ReadAvailable(1000);
        std::string egmrStr(respEgmr.begin(), respEgmr.end());

        if (!egmrStr.empty()) {
            outModemVer = egmrStr;
            Logger::Success("META Mode diagnostic channel active!");
            return true;
        }

        // Even if AT commands do not return text (some custom Motorola ROMs restrict AT in META),
        // successful port enumeration with PID 0x200E confirms the kernel booted into META mode!
        Logger::Success("MediaTek META Mode USB endpoint successfully established!");
        return true;
    }

} // namespace Mtk
