#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

namespace Mtk {

    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Success,
        Warn,
        Error
    };

    class Logger {
    public:
        static void Init();
        static void SetVerbose(bool verbose);
        static bool IsVerbose();

        static void Log(LogLevel level, const std::string& message);
        static void Trace(const std::string& message);
        static void Debug(const std::string& message);
        static void Info(const std::string& message);
        static void Success(const std::string& message);
        static void Warn(const std::string& message);
        static void Error(const std::string& message);

        // Hex dump helper for packet inspection
        static void HexDump(const std::string& direction, const uint8_t* data, size_t length);
        static void HexDump(const std::string& direction, const std::vector<uint8_t>& data);

    private:
        static bool s_verbose;
        static bool s_ansiSupported;
        static std::string GetTimestamp();
    };

} // namespace Mtk
