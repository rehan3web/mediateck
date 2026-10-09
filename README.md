# MediaTek META Mode Injector for Motorola Moto G73 5G

A high-performance, low-level C++ Windows tool engineered to automatically detect a powered-off **Motorola Moto G73 5G** (MediaTek Dimensity 930 / MT6855) upon USB connection and force it into **MediaTek META Mode** (Mobile Engineering Testing Architecture).

---

## 1. Architectural Overview & Boot Flow

### Target Hardware Profile
- **Device:** Motorola Moto G73 5G
- **SoC:** MediaTek Dimensity 930 (MT6855)
- **Cores:** 2x Arm Cortex-A78 + 6x Arm Cortex-A55
- **USB Vendor ID:** `0x0E8D` (MediaTek Inc.)

### The MediaTek Preloader & BootROM Window
When a MediaTek smartphone is powered off and connected to a host computer via USB:
1. **BootROM (BROM)** initializes and senses VBUS.
2. It transitions to **Preloader** execution.
3. The Preloader initializes the USB CDC-ACM controller and temporarily opens a Virtual COM port (`PID 0x2000` or `PID 0x0003`).
4. **The Critical Window:** This virtual COM port remains open for only **1.5 to 3.0 seconds**. If no host acknowledges the sync handshake within this interval, the preloader aborts download mode and redirects the boot chain to normal Android boot or battery charging animation mode.
5. **The META Mode Transition:** If the host sends the MediaTek handshake sequence (`0xA0 0x0A 0x50 0x05`) within the window and injects the boot mode parameter (`0x00000001` - `META_BOOT`), the Preloader configures the hardware boot argument register (`androidboot.mode=meta`), detaches from USB, reboots the system, and boots the kernel into META Mode.
6. The phone re-enumerates on USB as a **MediaTek META Mode Port** (`PID 0x200E`, `0x2002`, or `0x202D`), exposing the modem and diagnostic services (`meta_tst`).

---

## 2. Tool Architecture & Modules

The software is implemented in modern C++17 with direct Win32 and SetupAPI calls for maximum speed and deterministic timing:

- **[MtkDefines.hpp](file:///c:/Users/HP/Downloads/Meta/include/MtkDefines.hpp)**: Hardware definitions, opcodes, boot flags, and Dimensity SoC tables.
- **[SerialPort.hpp](file:///c:/Users/HP/Downloads/Meta/include/SerialPort.hpp) / [SerialPort.cpp](file:///c:/Users/HP/Downloads/Meta/src/SerialPort.cpp)**: Low-level Win32 serial abstraction using `CreateFileA`, `DCB`, `COMMTIMEOUTS`, and RTS/DTR line management with sub-millisecond precision.
- **[DeviceWatcher.hpp](file:///c:/Users/HP/Downloads/Meta/include/DeviceWatcher.hpp) / [DeviceWatcher.cpp](file:///c:/Users/HP/Downloads/Meta/src/DeviceWatcher.cpp)**: High-speed (15ms polling) Windows SetupAPI monitor using `GUID_DEVCLASS_PORTS` to instantly intercept MediaTek USB devices upon insertion.
- **[MtkHandshake.hpp](file:///c:/Users/HP/Downloads/Meta/include/MtkHandshake.hpp) / [MtkHandshake.cpp](file:///c:/Users/HP/Downloads/Meta/src/MtkHandshake.cpp)**: The core protocol engine:
  - Synchronous 4-byte sync handshake (`0xA0` -> `0x5F`, `0x0A` -> `0xF5`, `0x50` -> `0xAF`, `0x05` -> `0xFA`).
  - Chipset interrogation (`0xFC` -> HW Code `0x0855`, HW Subcode, SW Version).
  - Target security check (`0xD8` -> SBC, SLA, DAA status).
  - Multi-phase boot injection:
    - Preloader Command `CMD_SET_BOOT_MODE` (`0x10`) -> `0x00000001`.
    - Direct boot opcode `CMD_BOOT_META` (`0xB7`).
    - ASCII magic token fallback (`"METAMETA\n"`).
  - Post-transition META port re-enumeration watcher (`PID 0x200E`).
  - Diagnostic channel verification via AT commands.
- **[Logger.hpp](file:///c:/Users/HP/Downloads/Meta/include/Logger.hpp) / [Logger.cpp](file:///c:/Users/HP/Downloads/Meta/src/Logger.cpp)**: Formatted console output with ANSI colors, timestamps, and packet hex dumps.
- **[main.cpp](file:///c:/Users/HP/Downloads/Meta/src/main.cpp)**: Interactive CLI with options for manual ports, custom baud rates, verbose traces, and continuous listening.

---

## 3. How to Build

### Option A: Using Visual Studio / MSVC (`build.bat`)
1. Open **x64 Native Tools Command Prompt for VS**.
2. Navigate to the project directory:
   ```cmd
   cd c:\Users\HP\Downloads\Meta
   ```
3. Run:
   ```cmd
   build.bat
   ```
   The binary will be generated at `bin\mtk_meta_tool.exe`.

### Option B: Using MinGW GCC (`g++`)
```bash
g++ -std=c++17 -O2 -Iinclude src/main.cpp src/Logger.cpp src/SerialPort.cpp src/DeviceWatcher.cpp src/MtkHandshake.cpp -lsetupapi -ladvapi32 -o bin/mtk_meta_tool.exe
```
Or simply run:
```bash
make
```

### Option C: Using CMake
```bash
cmake -B build -S .
cmake --build build --config Release
```

---

## 4. Usage Instructions

### Step-by-Step Procedure for Motorola Moto G73 5G

1. **Power Off the Device:**
   - Power off the Motorola G73 5G completely.
   - Wait 5 to 10 seconds until the screen and vibration motors are completely off.
   - Do **NOT** turn it back on.

2. **Launch the Tool:**
   ```cmd
   bin\mtk_meta_tool.exe -v
   ```
   The tool will display the banner and begin scanning the USB bus for the MediaTek PreLoader or BootROM.

3. **Connect the USB Cable:**
   - Connect the USB-C cable between the PC and the phone.
   - **Method 1 (Standard):** Simply plug in the USB cable without pressing any buttons. (Catches Preloader `PID 0x2000`).
   - **Method 2 (BROM Mode):** If the Preloader window passes too quickly on your device, hold the **Volume Down** button and plug in the USB cable. (Forces BootROM `PID 0x0003`).

4. **Automatic Handshake & META Transition:**
   - The tool immediately latches onto the COM port within milliseconds.
   - Exchanging handshake sync bytes:
     ```
     [INFO ] Initiating MediaTek BROM / Preloader sync handshake...
     [DEBUG] Sync step 1/4 OK (0xA0 -> 0x5F)
     [DEBUG] Sync step 2/4 OK (0x0A -> 0xF5)
     [DEBUG] Sync step 3/4 OK (0x50 -> 0xAF)
     [DEBUG] Sync step 4/4 OK (0x05 -> 0xFA)
     [SUCCESS] BootROM / Preloader handshake synchronized successfully!
     [SUCCESS] Chipset HW Code: 0x0855 -> Dimensity 930 / MT6855 (Motorola G73 5G)
     [INFO ] Target Config: 0x00000007 [SBC=ENABLED, SLA=ENABLED, DAA=ENABLED]
     [INFO ] Sending CMD_SET_BOOT_MODE (0x10) -> META Mode (0x01)...
     [SUCCESS] META Mode boot instructions dispatched to preloader!
     ```
   - The phone reboots into META Mode and the display typically shows a small `=> META MODE` or yellow/white indicator in the bottom corner.
   - The tool detects the new META COM port (`PID 0x200E` / `PID 0x2002`) and performs diagnostic verification.

---

## 5. Command-Line Reference

| Flag | Description | Default |
|------|-------------|---------|
| `-p, --port <COMx>` | Manually target a specific COM port | Auto-detect |
| `-b, --baud <rate>` | Set communication baud rate | `115200` |
| `-s, --strategy <type>` | META injection strategy (`auto`, `cmd`, `direct`, `token`) | `auto` |
| `-t, --timeout <sec>` | Timeout waiting for phone connection | `60` seconds |
| `-l, --list` | List all active COM & MediaTek ports | - |
| `-c, --continuous` | Keep running in a loop for multiple devices | Disabled |
| `-v, --verbose` | Enable packet-level hex dumps (`[TX]`, `[RX]`) | Disabled |
| `-h, --help` | Display command usage and syntax | - |

---

## 6. Troubleshooting & Driver Requirements

- **Device Manager Shows "MediaTek PreLoader USB VCOM Port" with a Yellow Exclamation Mark:**
  - Install the signed MediaTek USB VCOM Driver or MediaTek SP Driver.
- **Port Disappears Before Tool Can Connect:**
  - Start `mtk_meta_tool.exe` **before** plugging the USB cable into the phone. The tool's 15ms detection loop will catch the port as soon as Windows enumerates it.
- **Phone Reboots into Charging Mode:**
  - Ensure the phone was turned off cleanly. If it transitions to charging, disconnect the cable, turn off the phone, wait 5 seconds, and reconnect while holding `Volume Down`.
