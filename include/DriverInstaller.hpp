#pragma once
#include <string>
#include <vector>

namespace Mtk {

    class DriverInstaller {
    public:
        // Automatically fixes Code 39 by removing obsolete oem19.inf
        // and installing the clean Microsoft CDC-ACM INF driver package
        static bool FixPreloaderDriver();

        // Install an INF file into the Windows Driver Store
        static bool InstallInf(const std::string& infFilePath);

        // Remove an obsolete driver package (e.g. "oem19.inf")
        static bool RemoveObsoleteDriver(const std::string& oemInfName);

        // Update active PnP devices to use the new driver
        static bool UpdatePnpDevice(const std::string& hardwareId, const std::string& infPath);

        // Check if the current process is running with Administrator privileges
        static bool IsElevated();
    };

} // namespace Mtk
