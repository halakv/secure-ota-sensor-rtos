# STM32 Secure OTA + RTOS Sensor Firmware

This project implements a secure boot + OTA update firmware system with FreeRTOS task management for a simulated sensor pipeline on the STM32F446RE microcontroller.

## Features

### Core Functionality
- **Bootloader** - Boot from valid firmware after OTA
- **OTA Firmware Updates** - UART-based Over-The-Air firmware updates with failover + rollback
- **FreeRTOS Task Management** - Concurrent tasks: Sensor, Logger, OTA Handler, Watchdog
- **Flash Memory Management** - Dual-slot architecture for safe firmware updates
- **UART Communication** - Command-line interface and logging via USART2

### OTA Update Features
- **Real-time progress tracking** - Monitor upload progress and status
- **Robust error handling** - Automatic retry and error recovery mechanisms
- **Dual-slot architecture** - Safe fallback to previous firmware version
- **Flash verification** - Checksum validation during write operations
- **Command-line interface** - Simple CLI for device interaction and control

## Hardware Requirements

- **STM32F446RE Nucleo board** - Main microcontroller platform
- **USB cable** - For UART communication and power
- **Onboard LED (PA5)** - Status indication
- **UART2 pins (PA2/PA3)** - Serial communication interface

## Software Requirements

- **STM32CubeIDE 1.16.0** - Development environment for firmware compilation
- **Python 3.x with pyserial** - For OTA upload scripts
- **FreeRTOS CMSIS-V2** - Real-time operating system
- **STM32 HAL + LL drivers** - Hardware abstraction layer

## Quick Start Guide

### 1. Build the Firmware

```bash
cd FreeRTOS/Debug
make all
```

This generates `FreeRTOS.bin` in the Debug folder.

### 2. Flash Initial Firmware

Flash the compiled firmware to the STM32 device using STM32CubeProgrammer or your preferred flashing tool.

### 3. Upload New Firmware via OTA

#### Using the OTA Update Tool (Recommended)

```bash
# Basic usage
python ota_update.py FreeRTOS.bin

# With custom settings  
python ota_update.py FreeRTOS.bin --port COM3 --baudrate 115200 --chunk-size 256

# Linux/macOS
python ota_update.py firmware.bin --port /dev/ttyUSB0 --monitor 10

# CRC checksum only (no upload)
python ota_update.py firmware.bin --crc-only

# Upload with CRC verification
python ota_update.py firmware.bin --verify-crc
```

#### Command Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--port` | COM3 | Serial port (COM3, /dev/ttyUSB0, etc.) |
| `--baudrate` | 115200 | Communication baud rate |
| `--chunk-size` | 256 | Upload chunk size in bytes |
| `--monitor` | 5 | Post-upload monitoring duration (seconds) |
| `--no-monitor` | - | Skip post-upload device monitoring |
| `--crc-only` | - | Only calculate and display CRC32, do not upload |
| `--verify-crc` | - | Send CRC command to device after upload |

## Device Commands

Connect to the device via serial terminal (115200 baud) to use these commands:

| Command | Description | Example |
|---------|-------------|---------|
| `version` | Show firmware version and build info | `version` |
| `otastart <size>` | Start OTA with expected file size | `otastart 49332` |
| `otastatus` | Check OTA progress and current state | `otastatus` |
| `otafinish` | Manually finish OTA process | `otafinish` |
| `crc` | Calculate and display flash memory CRC32 | `crc` |
| `reboot` | Restart the device | `reboot` |
| `data` | Show current sensor data readings | `data` |

## Boot Process Flow
```
1. Metadata Validation - Check if metadata structure is valid
2. Target Selection - Determine active slot (A or B)
3. CRC Verification - Calculate and compare CRC32 of target firmware
4. Fallback Logic - If CRC fails, attempt to boot from Slot A
5. Application Jump - Transfer control to validated firmware
```

## OTA Update Process

```
1. Send: otastart <firmware_size>
   Device Response: "Ready to receive firmware binary data..."

2. Send: <binary_firmware_data>
   Device: Receives data in chunks, writes to flash memory

3. Device: Automatically processes completion when all data received
   Device Response: "Firmware update completed. Total bytes: XXXX"

4. Device reboots automatically to use new firmware
```

### Boot Process After OTA
```
1. Metadata Check - Bootloader reads updated metadata
2. Slot Selection - Boots from newly specified active slot
3. CRC Verification - Validates firmware integrity using stored CRC
4. Fallback Protection - Falls back to previous slot if CRC fails
```
## Memory Layout

```
Flash Memory (512KB total) - STM32F446RE Sector Layout:
┌─────────────────────────────────────────────────────────────────┐
│ Sector 0:      0x08000000 - 0x08003FFF (16KB) - Bootloader     │
│ Sector 1:      0x08004000 - 0x08007FFF (16KB) - Bootloader     │
│ Sector 2:      0x08008000 - 0x0800BFFF (16KB) - Reserved       │
│ Sector 3:      0x0800C000 - 0x0800FFFF (16KB) - Metadata       │
│ Sector 4:      0x08010000 - 0x0801FFFF (64KB) - Slot A Part 1  │
│ Sector 5:      0x08020000 - 0x0803FFFF (128KB) - Slot A Part 2 │
│ Sector 6:      0x08040000 - 0x0805FFFF (128KB) - Slot B Part 1 │
│ Sector 7:      0x08060000 - 0x0807FFFF (128KB) - Slot B Part 2 │
└─────────────────────────────────────────────────────────────────┘

Memory Layout:
│ Bootloader:    0x08000000 - 0x08007FFF (32KB, Sectors 0-1)     │
│ Reserved:      0x08008000 - 0x0800BFFF (16KB, Sector 2)        │
│ Metadata:      0x0800C000 - 0x0800FFFF (16KB, Sector 3)        │
│ Slot A (App):  0x08010000 - 0x0803FFFF (192KB, Sectors 4-5)    │  
│ Slot B (OTA):  0x08040000 - 0x0807FFFF (256KB, Sectors 6-7)    │
```

## Security Features

- **Dual-slot architecture** - Safe fallback to previous firmware on update failure
- **Flash verification** - Checksum validation during flash write operations
- **Secure bootloader** - Metadata validation before application jump
- **Error recovery** - Automatic rollback mechanism on failed updates
- **State management** - Thread-safe OTA state tracking and validation

## Troubleshooting

### Upload Issues

1. **Check file size** - Ensure `otastart` size parameter matches actual file size
2. **Verify port** - Confirm correct COM port (Windows) or /dev/ttyUSB* (Linux/macOS)
3. **Check permissions** - Ensure user has access to serial port
4. **Reset device** - Power cycle STM32 if device becomes unresponsive

### Device Not Responding

1. **Check connections** - Verify USB cable and STM32 board connection
2. **Verify baud rate** - Default communication rate is 115200
3. **Try different terminal** - Test with PuTTY, Tera Term, or screen command
4. **Flash recovery** - Re-flash bootloader if device is completely unresponsive

### OTA Status States

- **IDLE** - Ready for new OTA operation
- **RECEIVING** - Currently receiving firmware data via UART
- **COMPLETE** - All data received successfully, ready for completion

## Example OTA Session

```
$ python ota_update.py FreeRTOS.bin --port COM3 --verify-crc
============================================================
    STM32 Secure OTA Firmware Update Tool
============================================================
📊 CRC32: 0x12A5B678 (312874616)
📏 Size: 49,332 bytes
✓ Connected to COM3 at 115200 baud

📁 Firmware: FreeRTOS.bin
📏 Size: 49,332 bytes
📦 Chunk size: 256 bytes

🚀 Starting OTA process...
→ otastart 49332
← OTA started, expecting 49332 bytes
← Ready to receive firmware binary data...

📤 Uploading firmware...
   Progress:  2,560/49,332 bytes (  5%)
   Progress:  5,120/49,332 bytes ( 10%)
   ...
   Progress: 49,332/49,332 bytes (100%)
✓ Upload complete: 49,332 bytes sent

⏳ Checking OTA status...
→ otastatus
← OTA State: COMPLETE
← Expected: 49332 bytes, Received: 49332 bytes
← Firmware update completed. Total bytes: 49332
✓ OTA process completed

🎉 Firmware update successful!

🔍 Verifying flash CRC...
Expected CRC32: 0x12A5B678
→ crc
← Flash CRC32: 0x12A5B678
← CRC verification: PASS

👁 Monitoring for 5 seconds...
← [BOOT] Starting new firmware v1.0.3
← [SENSOR] Task initialized
← [LOGGER] System ready
✓ Connection closed
```

## Development Notes

- **Optimal chunk size** - 256 bytes recommended for STM32F446RE flash writing
- **Queue management** - OTA queue handles 5 messages of 260 bytes each
- **Thread safety** - All OTA operations use thread-safe state management
- **RTOS integration** - FreeRTOS tasks enable concurrent sensor and OTA operations
- **Error handling** - Comprehensive error checking and recovery mechanisms

## Building and Compilation

The project uses STM32CubeIDE with the following key configurations:
- **Target**: STM32F446RE
- **RTOS**: FreeRTOS with CMSIS-V2
- **Optimization**: -Os (size optimization)
- **Debug**: SWD interface support

For detailed build instructions and configuration options, refer to the STM32CubeIDE project files.

---

**Note**: This firmware implements defensive security measures and is designed for legitimate OTA update scenarios. The system includes built-in protections against malicious firmware and unauthorized access attempts.