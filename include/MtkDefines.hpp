#pragma once
#include <cstdint>
#include <string>

namespace Mtk {

    // ============================================================================
    // USB Identification (MediaTek Inc.)
    // ============================================================================
    constexpr uint16_t MTK_USB_VID = 0x0E8D;

    // Common MediaTek USB Product IDs
    constexpr uint16_t PID_BROM_USB_PORT       = 0x0003; // MediaTek USB Port (BootROM)
    constexpr uint16_t PID_PRELOADER_VCOM      = 0x2000; // MediaTek PreLoader USB VCOM Port
    constexpr uint16_t PID_META_PORT_DEFAULT   = 0x200E; // MediaTek USB VCOM (Android) / META Port
    constexpr uint16_t PID_META_PORT_ALT1      = 0x2002; // MediaTek DA USB VCOM Port
    constexpr uint16_t PID_META_PORT_ALT2      = 0x2006; // MediaTek USB Modem Port
    constexpr uint16_t PID_META_PORT_ALT3      = 0x202D; // MediaTek High-Speed VCOM Port (META)

    // ============================================================================
    // MediaTek BootROM / Preloader Handshake Sync Bytes
    // ============================================================================
    constexpr uint8_t HANDSHAKE_START_0 = 0xA0;
    constexpr uint8_t HANDSHAKE_START_1 = 0x0A;
    constexpr uint8_t HANDSHAKE_START_2 = 0x50;
    constexpr uint8_t HANDSHAKE_START_3 = 0x05;

    // Negated reply bytes: (~byte) & 0xFF
    constexpr uint8_t HANDSHAKE_REPLY_0 = 0x5F; // ~0xA0
    constexpr uint8_t HANDSHAKE_REPLY_1 = 0xF5; // ~0x0A
    constexpr uint8_t HANDSHAKE_REPLY_2 = 0xAF; // ~0x50
    constexpr uint8_t HANDSHAKE_REPLY_3 = 0xFA; // ~0x05

    // ============================================================================
    // MediaTek BROM / Preloader Command Opcodes
    // ============================================================================
    constexpr uint8_t CMD_GET_HW_SW_VER       = 0xFC; // Query HW code, sub-code, HW/SW version
    constexpr uint8_t CMD_GET_HW_CODE         = 0xFD; // Query HW code only
    constexpr uint8_t CMD_GET_TARGET_CONFIG   = 0xD8; // Query target security config (SLA/DAA/SBC)
    constexpr uint8_t CMD_LEGACY_READ         = 0xA1; // 16-bit register read
    constexpr uint8_t CMD_LEGACY_WRITE        = 0xA2; // 16-bit register write
    constexpr uint8_t CMD_READ32              = 0xD1; // 32-bit register read
    constexpr uint8_t CMD_WRITE32             = 0xD4; // 32-bit register write
    constexpr uint8_t CMD_JUMP_DA             = 0xD5; // Jump to Download Agent
    constexpr uint8_t CMD_SEND_DA             = 0xD7; // Send Download Agent binary
    constexpr uint8_t CMD_SET_BOOT_MODE       = 0x10; // Set target boot mode in Preloader
    constexpr uint8_t CMD_BOOT_META           = 0xB7; // Preloader: Boot directly to META mode
    constexpr uint8_t CMD_REBOOT_NORMAL       = 0xD9; // Reboot target
    constexpr uint8_t CMD_SWITCH_USB_MODE     = 0x72; // Switch USB communication mode

    // Status & Acknowledgement codes
    constexpr uint8_t STATUS_ACK              = 0x00;
    constexpr uint8_t STATUS_NACK             = 0xFF;
    constexpr uint8_t STATUS_SYNC_OK          = 0x5A;

    // ============================================================================
    // Preloader Boot Modes
    // ============================================================================
    enum class BootMode : uint32_t {
        NORMAL_BOOT    = 0x00000000,
        META_BOOT      = 0x00000001, // Engineering / Calibration / NVRAM testing mode
        RECOVERY_BOOT  = 0x00000002,
        FASTBOOT       = 0x00000003,
        FACTORY_BOOT   = 0x00000004,
        ADVMETA_BOOT   = 0x00000005, // Advanced META Mode
        ATE_BOOT       = 0x00000006
    };

    // Device port state
    enum class DevicePortType {
        None,
        BromPort,       // VID 0x0E8D, PID 0x0003
        PreloaderPort,  // VID 0x0E8D, PID 0x2000
        MetaPort,       // VID 0x0E8D, PID 0x200E / 0x2002 / 0x202D
        OtherMtkPort
    };

    // Hardware Identification structure
    struct TargetHwInfo {
        uint16_t hw_code       = 0;
        uint16_t hw_sub_code   = 0;
        uint16_t hw_version    = 0;
        uint16_t sw_version    = 0;
        uint32_t target_config = 0;
        bool secure_boot       = false;
        bool serial_link_auth  = false;
        bool da_auth           = false;
    };

    // Returns friendly name for MediaTek chipset based on HW Code
    inline std::string GetChipName(uint16_t hwCode) {
        switch (hwCode) {
            case 0x0855: return "Dimensity 930 / MT6855 (Motorola G73 5G)";
            case 0x0816: return "Dimensity 930 / MT6855 Variant";
            case 0x0788: return "Dimensity 700 / MT6833";
            case 0x0813: return "Dimensity 810 / MT6833P";
            case 0x0886: return "Dimensity 1080 / MT6877V";
            case 0x0989: return "Dimensity 1200 / MT6893";
            case 0x0922: return "Dimensity 8000/8100 / MT6895";
            case 0x0676: return "Helio G80/G85 / MT6768";
            case 0x0677: return "Helio G90/G95 / MT6785";
            case 0x0699: return "Helio G99 / MT6789";
            case 0x0658: return "Helio P60 / MT6771";
            case 0x0675: return "Helio P65 / MT6765";
            default: {
                char buf[32];
                snprintf(buf, sizeof(buf), "MediaTek SoC [0x%04X]", hwCode);
                return std::string(buf);
            }
        }
    }

} // namespace Mtk
