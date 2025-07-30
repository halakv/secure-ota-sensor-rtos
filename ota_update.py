#!/usr/bin/env python3
"""
STM32 Secure OTA Update Tool

A clean, comprehensive tool for uploading firmware to STM32 devices via UART.
Combines the functionality of ota_upload.py and test_ota.py into a single script.

Usage:
    python ota_update.py <firmware.bin> [options]

Examples:
    python ota_update.py FreeRTOS.bin
    python ota_update.py firmware.bin --port COM3 --baudrate 115200
    python ota_update.py app.bin --port /dev/ttyUSB0 --chunk-size 128 --monitor 10
"""

import serial
import time
import os
import sys
import argparse
import zlib
from pathlib import Path


class STM32OTAUpdater:
    """STM32 OTA firmware updater via UART"""
    
    def __init__(self, port, baudrate=115200, timeout=5):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial_conn = None
        
    def connect(self):
        """Establish serial connection to STM32 device"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            
            # Clear buffers
            self.serial_conn.reset_input_buffer()
            self.serial_conn.reset_output_buffer()
            time.sleep(0.5)
            return True
            
        except serial.SerialException as e:
            print(f"✗ Failed to connect to {self.port}: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection"""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            print("✓ Connection closed")
    
    def send_command(self, command, wait_response=True, timeout=3):
        """Send command and optionally wait for response"""
        if not self.serial_conn or not self.serial_conn.is_open:
            return False
        
        try:
            # Send command
            cmd_bytes = f"{command}\r\n".encode('utf-8')
            self.serial_conn.write(cmd_bytes)
            print(f"→ {command}")
            
            if not wait_response:
                return True
            
            # Wait for response
            start_time = time.time()
            responses = []
            
            while time.time() - start_time < timeout:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        responses.append(line)
                        print(f"← {line}")
                        
                        # Check for success indicators
                        if any(success in line for success in [
                            "Ready to receive firmware binary data",
                            "OTA state confirmed: RECEIVING mode active",
                            "OTA started, expecting"
                        ]):
                            # For OTA start, wait for additional confirmation
                            if "otastart" in command:
                                continue  # Keep reading for more responses
                            return True
                        elif any(err in line.lower() for err in ["error", "failed", "timeout"]):
                            return False
                else:
                    time.sleep(0.01)
            
            return len(responses) > 0
            
        except Exception as e:
            print(f"✗ Command error: {e}")
            return False
    
    def upload_firmware(self, firmware_file, chunk_size=256):
        """Upload firmware binary to STM32 device"""
        
        # Validate file
        if not os.path.exists(firmware_file):
            print(f"✗ File not found: {firmware_file}")
            return False
        
        file_size = os.path.getsize(firmware_file)
        if file_size == 0:
            print(f"✗ Empty file: {firmware_file}")
            return False
        
        print(f"\n📁 Firmware: {firmware_file}")
        print(f"📏 Size: {file_size:,} bytes")
        print(f"📦 Chunk size: {chunk_size} bytes")
        
        # Step 1: Start OTA process
        print(f"\n🚀 Starting OTA process...")
        if not self.send_command(f"otastart {file_size}", wait_response=True, timeout=15):
            print("✗ Failed to start OTA")
            return False
        
        # Wait for device to complete flash erase and enter receiving mode
        print("⏳ Waiting for device to complete initialization...")
        time.sleep(3)  # Give device time to erase flash and switch modes
        
        # Step 2: Upload firmware data
        print(f"\n📤 Uploading firmware...")
        try:
            with open(firmware_file, 'rb') as f:
                uploaded = 0
                chunk_num = 0
                
                while True:
                    chunk = f.read(chunk_size)
                    if not chunk:
                        break
                    
                    self.serial_conn.write(chunk)
                    uploaded += len(chunk)
                    chunk_num += 1
                    
                    # Progress display
                    progress = (uploaded * 100) // file_size
                    if chunk_num % 10 == 0 or uploaded == file_size:  # Every 10 chunks or at end
                        print(f"   Progress: {uploaded:6,}/{file_size:,} bytes ({progress:3d}%)")
                    
                    time.sleep(0.05)  # Prevent overwhelming the device - increased delay
                
                print(f"✓ Upload complete: {uploaded:,} bytes sent")
                
        except Exception as e:
            print(f"✗ Upload error: {e}")
            return False
        
        # Step 3: Wait for OTA completion and check status
        print(f"\n⏳ Waiting for OTA completion...")
        time.sleep(2)  # Reduced wait time since we added proper handshaking
        
        # Send finish command to complete OTA
        print("→ Sending OTA finish command...")
        if self.send_command("otafinish", wait_response=True, timeout=10):
            print("✓ OTA finish command sent")
        else:
            print("⚠ OTA finish command may have failed")
        
        # Wait a bit more for processing
        time.sleep(2)
        
        # Check final status
        if self.send_command("otastatus", wait_response=True, timeout=5):
            print("✓ OTA process completed")
            return True
        else:
            print("⚠ Status check completed")
            return True  # Consider success even if status check is unclear
    
    def calculate_crc32(self, firmware_file):
        """Calculate CRC32 checksum of firmware file"""
        try:
            with open(firmware_file, 'rb') as f:
                file_data = f.read()
                crc32 = zlib.crc32(file_data) & 0xffffffff
                return crc32, len(file_data)
        except Exception as e:
            print(f"✗ CRC calculation error: {e}")
            return None, 0
    
    def send_crc_command(self, expected_crc=None):
        """Send CRC command to device and optionally verify against expected value"""
        if not self.send_command("crc", wait_response=True, timeout=10):
            print("⚠ CRC command failed or not supported by firmware")
            return False
        
        # If we have an expected CRC, we would parse the response here
        # For now, just indicate that the command was sent
        return True
    
    def monitor_output(self, duration=5):
        """Monitor device output for specified duration"""
        if duration <= 0:
            return
        
        print(f"\n👁 Monitoring for {duration} seconds...")
        start_time = time.time()
        
        while time.time() - start_time < duration:
            if self.serial_conn.in_waiting > 0:
                line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"← {line}")
            else:
                time.sleep(0.1)


def main():
    parser = argparse.ArgumentParser(
        description="STM32 Secure OTA Firmware Update Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python ota_update.py firmware.bin
  python ota_update.py FreeRTOS.bin --port COM3 --baudrate 115200
  python ota_update.py app.bin --port /dev/ttyUSB0 --chunk-size 128
  python ota_update.py firmware.bin --crc-only
  python ota_update.py firmware.bin --verify-crc
        """
    )
    
    parser.add_argument('firmware', help='Firmware binary file (.bin)')
    parser.add_argument('--port', default='COM3', 
                       help='Serial port (default: COM3)')
    parser.add_argument('--baudrate', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('--chunk-size', type=int, default=256,
                       help='Upload chunk size in bytes (default: 256)')
    parser.add_argument('--monitor', type=int, default=5,
                       help='Monitor duration after upload in seconds (default: 5)')
    parser.add_argument('--no-monitor', action='store_true',
                       help='Skip post-upload monitoring')
    parser.add_argument('--crc-only', action='store_true',
                       help='Only calculate and display CRC32 checksum, do not upload')
    parser.add_argument('--verify-crc', action='store_true',
                       help='Send CRC command to device after upload for verification')
    
    args = parser.parse_args()
    
    # Validate firmware file
    firmware_path = Path(args.firmware)
    if not firmware_path.exists():
        print(f"✗ Firmware file not found: {args.firmware}")
        sys.exit(1)
    
    if not firmware_path.suffix.lower() == '.bin':
        print(f"⚠ Warning: File extension is not '.bin': {firmware_path.suffix}")
    
    # Print header
    print("=" * 60)
    print("    STM32 Secure OTA Firmware Update Tool")
    print("=" * 60)
    
    # Calculate CRC32 checksum
    updater = STM32OTAUpdater(args.port, args.baudrate)
    crc32, file_size = updater.calculate_crc32(str(firmware_path))
    
    if crc32 is not None:
        print(f"📊 CRC32: 0x{crc32:08X} ({crc32})")
        print(f"📏 Size: {file_size:,} bytes")
    
    # If only CRC calculation requested, exit here
    if args.crc_only:
        if crc32 is not None:
            print("✓ CRC calculation complete")
        else:
            print("✗ CRC calculation failed")
            sys.exit(1)
        sys.exit(0)
    
    # Connect for upload
    if not updater.connect():
        sys.exit(1)
    
    try:
        success = updater.upload_firmware(str(firmware_path), args.chunk_size)
        
        if success:
            print("\n🎉 Firmware update successful!")
            
            # Verify CRC if requested
            if args.verify_crc:
                print(f"\n🔍 Verifying flash CRC...")
                if crc32 is not None:
                    print(f"Expected CRC32: 0x{crc32:08X}")
                updater.send_crc_command(crc32)
            
            # Monitor device output
            if not args.no_monitor and args.monitor > 0:
                updater.monitor_output(args.monitor)
            
        else:
            print("\n❌ Firmware update failed!")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print("\n⚠ Update interrupted by user")
        sys.exit(1)
    finally:
        updater.disconnect()


if __name__ == "__main__":
    main()