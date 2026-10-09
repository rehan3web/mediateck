#include "Logger.hpp"
#include <windows.h>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace Mtk {

    bool Logger::s_verbose = false;
    bool Logger::s_ansiSupported = false;

    void Logger::Init() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (SetConsoleMode(hOut, dwMode)) {
                    s_ansiSupported = true;
                }
            }
        }
    }

    void Logger::SetVerbose(bool verbose) {
        s_verbose = verbose;
    }

    bool Logger::IsVerbose() {
        return s_verbose;
    }

    std::string Logger::GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm bt{};
        localtime_s(&bt, &timer);

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << bt.tm_hour << ":"
            << std::setw(2) << bt.tm_min << ":"
            << std::setw(2) << bt.tm_sec << "."
            << std::setw(3) << ms.count();
        return oss.str();
    }

    void Logger::Log(LogLevel level, const std::string& message) {
        if ((level == LogLevel::Trace || level == LogLevel::Debug) && !s_verbose) {
            return;
        }

        std::string ts = GetTimestamp();

        if (s_ansiSupported) {
            std::string prefix;
            std::string color;
            std::string reset = "\033[0m";

            switch (level) {
                case LogLevel::Trace:
                    prefix = "[TRACE]";
                    color = "\033[90m"; // Dark gray
                    break;
                case LogLevel::Debug:
                    prefix = "[DEBUG]";
                    color = "\033[36m"; // Cyan
                    break;
                case LogLevel::Info:
                    prefix = "[INFO ]";
                    color = "\033[97m"; // White
                    break;
                case LogLevel::Success:
                    prefix = "[SUCCESS]";
                    color = "\033[92m"; // Bright green
                    break;
                case LogLevel::Warn:
                    prefix = "[WARN ]";
                    color = "\033[93m"; // Bright yellow
                    break;
                case LogLevel::Error:
                    prefix = "[ERROR]";
                    color = "\033[91m"; // Bright red
                    break;
            }

            std::cout << "\033[90m[" << ts << "]\033[0m "
                      << color << prefix << " " << message << reset << std::endl;
        } else {
            const char* prefix = "[INFO ]";
            switch (level) {
                case LogLevel::Trace:   prefix = "[TRACE]"; break;
                case LogLevel::Debug:   prefix = "[DEBUG]"; break;
                case LogLevel::Info:    prefix = "[INFO ]"; break;
                case LogLevel::Success: prefix = "[SUCCESS]"; break;
                case LogLevel::Warn:    prefix = "[WARN ]"; break;
                case LogLevel::Error:   prefix = "[ERROR]"; break;
            }
            std::cout << "[" << ts << "] " << prefix << " " << message << std::endl;
        }
    }

    void Logger::Trace(const std::string& message) {
        Log(LogLevel::Trace, message);
    }

    void Logger::Debug(const std::string& message) {
        Log(LogLevel::Debug, message);
    }

    void Logger::Info(const std::string& message) {
        Log(LogLevel::Info, message);
    }

    void Logger::Success(const std::string& message) {
        Log(LogLevel::Success, message);
    }

    void Logger::Warn(const std::string& message) {
        Log(LogLevel::Warn, message);
    }

    void Logger::Error(const std::string& message) {
        Log(LogLevel::Error, message);
    }

    void Logger::HexDump(const std::string& direction, const uint8_t* data, size_t length) {
        if (!s_verbose || length == 0) return;

        std::ostringstream oss;
        oss << "[" << direction << "] (" << length << " bytes): ";
        for (size_t i = 0; i < length; ++i) {
            oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                << static_cast<int>(data[i]) << " ";
        }

        // ASCII preview
        oss << " | ";
        for (size_t i = 0; i < length; ++i) {
            char c = static_cast<char>(data[i]);
            if (c >= 32 && c <= 126) {
                oss << c;
            } else {
                oss << '.';
            }
        }

        Debug(oss.str());
    }

    void Logger::HexDump(const std::string& direction, const std::vector<uint8_t>& data) {
        HexDump(direction, data.data(), data.size());
    }

} // namespace Mtk
