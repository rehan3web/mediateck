#include "MtkDefines.hpp"
#include "SerialPort.hpp"
#include "DeviceWatcher.hpp"
#include "MtkHandshake.hpp"
#include "Logger.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace Mtk;

void PrintBanner() {
    std::cout << R"(
================================================================================
   __  __ _____ _  __   __  __ ______ _______       __  __  ____  _____  ______ 
  |  \/  |_   _| |/ /  |  \/  |  ____|__   __|/\   |  \/  |/ __ \|  __ \|  ____|
  | \  / | | | | ' /   | \  / | |__     | |  /  \  | \  / | |  | | |  | | |__   
  | |\/| | | | |  <    | |\/| |  __|    | | / /\ \ | |\/| | |  | | |  | |  __|  
  | |  | |_| |_| . \   | |  | | |____   | |/ ____ \| |  | | |__| | |__| | |____ 
  |_|  |_|_____|_|\_\  |_|  |_|______|  |_/_/    \_\_|  |_|\____/|_____/|______|
================================================================================
  Target Platform: Motorola Moto G73 5G (MediaTek Dimensity 930 / MT6855)
  Purpose: Automatic USB Detection & Low-Level META Mode Boot Transition
================================================================================
)" << std::endl;
}

void PrintUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  -p, --port <COMx>        Manually specify target COM port (e.g. COM4)\n"
              << "  -b, --baud <rate>        Set baud rate (default: 115200, fast: 921600)\n"
              << "  -s, --strategy <type>    META strategy: auto (default), cmd, direct, token\n"
              << "  -t, --timeout <sec>      Timeout in seconds waiting for device (default: 60)\n"
              << "  -l, --list               List all currently detected COM / MTK ports and exit\n"
              << "  -c, --continuous         Run continuously, waiting for devices repeatedly\n"
              << "  -v, --verbose            Enable verbose packet-level hex dumps and trace\n"
              << "  -h, --help               Display this help documentation\n\n"
              << "Standard Workflow:\n"
              << "  1. Turn OFF your Motorola G73 5G completely.\n"
              << "  2. Run this tool (default auto-detection mode).\n"
              << "  3. Connect the phone to PC via USB-C cable (without holding buttons, or hold Vol- if needed).\n"
              << "  4. The tool catches the Preloader within its 2-second window and commands META boot.\n"
              << "  5. The phone reboots directly into MediaTek META Mode.\n"
              << std::endl;
}

void ListPorts() {
    Logger::Info("Scanning system for COM ports...");
    auto allPorts = DeviceWatcher::EnumerateAllPorts();

    if (allPorts.empty()) {
        Logger::Warn("No active COM ports detected on the system.");
        return;
    }

    std::cout << "\n--------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(10) << "Port"
              << std::setw(12) << "VID:PID"
              << std::setw(16) << "Type"
              << "Friendly Name / Description\n";
    std::cout << "--------------------------------------------------------------------------------\n";

    for (const auto& dev : allPorts) {
        std::string vidPid = "-";
        if (dev.vid != 0 || dev.pid != 0) {
            std::ostringstream ss;
            ss << std::hex << std::uppercase << std::setfill('0')
               << std::setw(4) << dev.vid << ":" << std::setw(4) << dev.pid;
            vidPid = ss.str();
        }

        std::string typeStr = "Generic COM";
        switch (dev.type) {
            case DevicePortType::BromPort:      typeStr = "MTK BootROM"; break;
            case DevicePortType::PreloaderPort: typeStr = "MTK PreLoader"; break;
            case DevicePortType::MetaPort:      typeStr = "MTK META Mode"; break;
            case DevicePortType::OtherMtkPort:  typeStr = "MTK Unknown"; break;
            default: break;
        }

        std::cout << std::left << std::setw(10) << dev.portName
                  << std::setw(12) << vidPid
                  << std::setw(16) << typeStr
                  << dev.friendlyName << "\n";
    }
    std::cout << "--------------------------------------------------------------------------------\n\n";
}

int main(int argc, char* argv[]) {
    Logger::Init();
    PrintBanner();

    std::string specifiedPort = "";
    uint32_t baudRate = 115200;
    uint32_t timeoutSec = 60;
    HandshakeStrategy strategy = HandshakeStrategy::Auto;
    bool continuous = false;

    // Command-line parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "-l" || arg == "--list") {
            ListPorts();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            Logger::SetVerbose(true);
        } else if (arg == "-c" || arg == "--continuous") {
            continuous = true;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            specifiedPort = argv[++i];
        } else if ((arg == "-b" || arg == "--baud") && i + 1 < argc) {
            baudRate = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if ((arg == "-t" || arg == "--timeout") && i + 1 < argc) {
            timeoutSec = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if ((arg == "-s" || arg == "--strategy") && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "cmd") strategy = HandshakeStrategy::PreloaderCommand;
            else if (s == "direct") strategy = HandshakeStrategy::DirectBootMeta;
            else if (s == "token") strategy = HandshakeStrategy::AsciiToken;
            else strategy = HandshakeStrategy::Auto;
        }
    }

    if (Logger::IsVerbose()) {
        Logger::Info("Verbose packet logging ENABLED.");
    }

    do {
        std::string targetPort = specifiedPort;

        if (targetPort.empty()) {
            std::cout << "\n>>> [ACTION REQUIRED] <<<\n";
            std::cout << "  1. Ensure Motorola G73 5G is powered OFF completely.\n";
            std::cout << "  2. Connect the phone to this computer using a USB-C data cable.\n";
            std::cout << "  3. Tool will detect the MediaTek BootROM/Preloader automatically...\n\n";

            Logger::Info("Waiting for Motorola G73 5G (MTK Preloader/BROM port) [Timeout: " +
                        std::to_string(timeoutSec) + "s]...");

            DeviceWatcher watcher;
            DeviceInfo detectedDev = watcher.WaitForMtkDevice(timeoutSec, [](uint32_t sec) {
                if (sec > 0 && sec % 10 == 0) {
                    Logger::Info("Still waiting for USB device connection (" + std::to_string(sec) + "s elapsed)...");
                }
            });

            if (detectedDev.portName.empty()) {
                Logger::Error("Timed out waiting for MediaTek device. Please verify cable, power state, and drivers.");
                if (!continuous) return 1;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            targetPort = detectedDev.portName;
            Logger::Success("MediaTek device detected on port: " + targetPort +
                           " (" + detectedDev.friendlyName + ")");
        } else {
            Logger::Info("Using manually specified target port: " + targetPort);
        }

        // Open serial connection immediately
        SerialPort serial;
        if (!serial.Open(targetPort, baudRate)) {
            Logger::Error("Failed to open " + targetPort + ". Device may have detached or port is in use.");
            if (!continuous) return 1;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // Run handshake and META boot transition
        MtkHandshake handshake(serial);
        HandshakeResult res = handshake.Execute(strategy, 3000);

        // Close preloader port so device can reboot and re-enumerate
        serial.Close();

        if (!res.success) {
            Logger::Error("META Mode transition failed: " + res.message);
            if (!continuous) return 1;
            continue;
        }

        Logger::Info("Preloader disconnected. Phone is transitioning to META Mode...");
        Logger::Info("Monitoring USB bus for MediaTek META Mode port (PID 0x200E / 0x2002 / 0x202D)...");

        DeviceWatcher watcher;
        DeviceInfo metaDev = watcher.WaitForMetaPort(25, [](uint32_t sec) {
            if (sec > 0 && sec % 5 == 0) {
                Logger::Info("Waiting for META port to enumerate (" + std::to_string(sec) + "s)...");
            }
        });

        if (!metaDev.portName.empty()) {
            Logger::Success("META Mode port successfully enumerated on: " + metaDev.portName +
                           " (" + metaDev.friendlyName + ")");

            // Verify communication with META diagnostic channel
            std::string modemInfo;
            if (handshake.VerifyMetaMode(metaDev.portName, modemInfo, 5000)) {
                Logger::Success("************************************************************");
                Logger::Success("  Motorola G73 5G is now SUCCESSFULLY in MediaTek META Mode! ");
                Logger::Success("************************************************************");
                Logger::Info("Port: " + metaDev.portName);
                if (!modemInfo.empty()) {
                    Logger::Info("Diagnostic Response: " + modemInfo);
                }
                Logger::Info("You may now perform NVRAM calibration, RF testing, or diagnostics.");
            }
        } else {
            Logger::Warn("Device did not enumerate META VCOM port within 25 seconds.");
            Logger::Warn("Check Device Manager to verify if MediaTek META / USB VCOM drivers are installed.");
        }

        if (continuous) {
            Logger::Info("Continuous mode active. Waiting 5 seconds before next detection cycle...\n");
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

    } while (continuous);

    return 0;
}
