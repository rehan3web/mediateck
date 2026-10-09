#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

namespace Mtk {

    class SerialPort {
    public:
        SerialPort();
        ~SerialPort();

        // Non-copyable
        SerialPort(const SerialPort&) = delete;
        SerialPort& operator=(const SerialPort&) = delete;

        // Open a COM port (e.g., "COM3" or "\\\\.\\COM3")
        bool Open(const std::string& portName, uint32_t baudRate = 115200);
        void Close();
        bool IsOpen() const;

        // Transmission
        bool Write(const uint8_t* data, size_t length);
        bool Write(const std::vector<uint8_t>& data);
        bool WriteByte(uint8_t byte);
        bool WriteString(const std::string& str);

        // Reception
        size_t Read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 100);
        bool ReadExact(uint8_t* buffer, size_t expectedLen, uint32_t timeoutMs = 500);
        bool ReadByte(uint8_t& outByte, uint32_t timeoutMs = 100);
        std::vector<uint8_t> ReadAvailable(uint32_t timeoutMs = 50);

        // Control lines & buffer management
        void Purge();
        bool SetDTR(bool enable);
        bool SetRTS(bool enable);
        bool SetBaudRate(uint32_t baudRate);

        std::string GetPortName() const { return m_portName; }

    private:
        HANDLE m_hPort;
        std::string m_portName;
        uint32_t m_currentBaudRate;

        bool ConfigurePort(uint32_t baudRate);
        bool SetTimeouts(uint32_t readTimeoutMs, uint32_t writeTimeoutMs);
    };

} // namespace Mtk
