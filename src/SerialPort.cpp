#include "SerialPort.hpp"
#include "Logger.hpp"
#include <chrono>

namespace Mtk {

    SerialPort::SerialPort()
        : m_hPort(INVALID_HANDLE_VALUE),
          m_currentBaudRate(115200) {
    }

    SerialPort::~SerialPort() {
        Close();
    }

    bool SerialPort::Open(const std::string& portName, uint32_t baudRate) {
        Close();

        std::string winPath = portName;
        if (winPath.find("\\\\.\\") != 0) {
            winPath = "\\\\.\\" + winPath;
        }

        m_portName = portName;

        m_hPort = CreateFileA(
            winPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,                      // Exclusive access
            nullptr,                // Default security
            OPEN_EXISTING,
            0,                      // Non-overlapped for tight deterministic timing
            nullptr
        );

        if (m_hPort == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            Logger::Debug("Failed to open " + portName + " (Win32 Error: " + std::to_string(err) + ")");
            return false;
        }

        if (!ConfigurePort(baudRate)) {
            Close();
            return false;
        }

        Purge();
        SetDTR(true);
        SetRTS(true);

        Logger::Debug("Port " + portName + " successfully opened at " + std::to_string(baudRate) + " baud");
        return true;
    }

    void SerialPort::Close() {
        if (m_hPort != INVALID_HANDLE_VALUE) {
            PurgeComm(m_hPort, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
            CloseHandle(m_hPort);
            m_hPort = INVALID_HANDLE_VALUE;
        }
    }

    bool SerialPort::IsOpen() const {
        return m_hPort != INVALID_HANDLE_VALUE;
    }

    bool SerialPort::ConfigurePort(uint32_t baudRate) {
        if (!IsOpen()) return false;

        DCB dcb{};
        dcb.DCBlength = sizeof(DCB);

        if (!GetCommState(m_hPort, &dcb)) {
            Logger::Error("Failed to get COM port state");
            return false;
        }

        m_currentBaudRate = baudRate;
        dcb.BaudRate = baudRate;
        dcb.ByteSize = 8;
        dcb.Parity   = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary  = TRUE;
        dcb.fParity  = FALSE;

        // Disable hardware and software flow control
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl  = DTR_CONTROL_ENABLE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fTXContinueOnXoff = FALSE;
        dcb.fOutX = FALSE;
        dcb.fInX  = FALSE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fAbortOnError = FALSE;

        if (!SetCommState(m_hPort, &dcb)) {
            Logger::Error("Failed to set COM port state (DCB)");
            return false;
        }

        // Set fast default timeouts: 50ms read, 50ms write
        return SetTimeouts(50, 50);
    }

    bool SerialPort::SetTimeouts(uint32_t readTimeoutMs, uint32_t writeTimeoutMs) {
        if (!IsOpen()) return false;

        COMMTIMEOUTS timeouts{};
        // Total timeout = ReadTotalTimeoutMultiplier * numberOfBytes + ReadTotalTimeoutConstant
        timeouts.ReadIntervalTimeout = MAXDWORD; // Return immediately if bytes exist
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = readTimeoutMs;

        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = writeTimeoutMs;

        return SetCommTimeouts(m_hPort, &timeouts) != FALSE;
    }

    void SerialPort::Purge() {
        if (IsOpen()) {
            PurgeComm(m_hPort, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
        }
    }

    bool SerialPort::SetDTR(bool enable) {
        if (!IsOpen()) return false;
        return EscapeCommFunction(m_hPort, enable ? SETDTR : CLRDTR) != FALSE;
    }

    bool SerialPort::SetRTS(bool enable) {
        if (!IsOpen()) return false;
        return EscapeCommFunction(m_hPort, enable ? SETRTS : CLRRTS) != FALSE;
    }

    bool SerialPort::SetBaudRate(uint32_t baudRate) {
        return ConfigurePort(baudRate);
    }

    bool SerialPort::Write(const uint8_t* data, size_t length) {
        if (!IsOpen() || length == 0) return false;

        DWORD bytesWritten = 0;
        BOOL result = WriteFile(m_hPort, data, static_cast<DWORD>(length), &bytesWritten, nullptr);

        if (result && bytesWritten == length) {
            Logger::HexDump("TX", data, length);
            return true;
        }

        Logger::Error("Write failed or partial write: " + std::to_string(bytesWritten) + "/" + std::to_string(length));
        return false;
    }

    bool SerialPort::Write(const std::vector<uint8_t>& data) {
        return Write(data.data(), data.size());
    }

    bool SerialPort::WriteByte(uint8_t byte) {
        return Write(&byte, 1);
    }

    bool SerialPort::WriteString(const std::string& str) {
        return Write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    size_t SerialPort::Read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) {
        if (!IsOpen() || maxLen == 0) return 0;

        SetTimeouts(timeoutMs, 50);

        DWORD bytesRead = 0;
        BOOL result = ReadFile(m_hPort, buffer, static_cast<DWORD>(maxLen), &bytesRead, nullptr);

        if (result && bytesRead > 0) {
            Logger::HexDump("RX", buffer, bytesRead);
            return static_cast<size_t>(bytesRead);
        }

        return 0;
    }

    bool SerialPort::ReadExact(uint8_t* buffer, size_t expectedLen, uint32_t timeoutMs) {
        if (!IsOpen() || expectedLen == 0) return false;

        size_t totalRead = 0;
        auto startTime = std::chrono::steady_clock::now();

        while (totalRead < expectedLen) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();

            if (elapsed >= timeoutMs) {
                break;
            }

            uint32_t remainingTimeout = static_cast<uint32_t>(timeoutMs - elapsed);
            SetTimeouts(remainingTimeout, 50);

            DWORD bytesRead = 0;
            BOOL ok = ReadFile(
                m_hPort,
                buffer + totalRead,
                static_cast<DWORD>(expectedLen - totalRead),
                &bytesRead,
                nullptr
            );

            if (ok && bytesRead > 0) {
                totalRead += bytesRead;
            } else {
                Sleep(1); // Small yield
            }
        }

        if (totalRead > 0) {
            Logger::HexDump("RX", buffer, totalRead);
        }

        return totalRead == expectedLen;
    }

    bool SerialPort::ReadByte(uint8_t& outByte, uint32_t timeoutMs) {
        return ReadExact(&outByte, 1, timeoutMs);
    }

    std::vector<uint8_t> SerialPort::ReadAvailable(uint32_t timeoutMs) {
        std::vector<uint8_t> result;
        uint8_t chunk[256];
        size_t n = Read(chunk, sizeof(chunk), timeoutMs);
        if (n > 0) {
            result.assign(chunk, chunk + n);
        }
        return result;
    }

} // namespace Mtk
