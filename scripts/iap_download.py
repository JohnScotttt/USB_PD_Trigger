#!/usr/bin/env python3
"""
USB PD Trigger IAP Firmware Download Tool

Usage:
    python iap_download.py <firmware.bin>
    python iap_download.py <firmware.bin> --enter-bootloader

Requirements:
    pip install hid
"""

import argparse
import struct
import sys
import time
import zlib

try:
    import hid
except ImportError:
    print("Error: hid not installed. Run: pip install hid")
    sys.exit(1)

# USB identifiers
VID = 0xA016
PID_APP = 0x0404
PID_BOOTLOADER = 0x0405

# IAP protocol constants (Mini format)
HID_HEADER_0_MINI = 0x52
HID_HEADER_1 = 0xFF
HID_DATA_TYPE_SYS = 0x01
HID_CATEGORY_SYS = 0x01

# IAP command codes
IAP_CMD_START = 0x80
IAP_CMD_DATA = 0x81
IAP_CMD_FINISH = 0x82
IAP_CMD_STATUS = 0x83

# APP command for entering bootloader
SYS_CMD_ENTER_BOOTLOADER = 0x71

# Response header
RESP_HEADER_0 = 0xA2
RESP_HEADER_1 = 0xFF

# Status codes
IAP_STATUS_OK = 0x00
IAP_STATUS_ERROR = 0x01
IAP_STATUS_BUSY = 0x02
IAP_STATUS_CRC_FAIL = 0x03

# Max firmware data bytes per DATA packet
# Packet: [Header(2)][FullLen(1)][DataType(1)][Category(1)][CMD(1)][offset(4)][len(1)][data(<=53)]
MAX_DATA_PER_PACKET = 53

REPORT_SIZE = 64


def build_iap_packet(cmd: int, params: bytes = b"") -> bytes:
    """Build a Mini-format HID packet for IAP command.

    Returns 65 bytes: [ReportID=0x00][64 bytes HID data].
    hid.device.write() requires the first byte to be the Report ID.
    """
    # [0x00][0x52][0xFF][FullLength][DataType=0x01][Category=0x01][CMD][Params...]
    full_length = 2 + 1 + len(params)  # DataType + Category + CMD + Params
    pkt = bytearray(1 + REPORT_SIZE)   # 1 byte report ID + 64 bytes data
    pkt[0] = 0x00  # Report ID (none)
    pkt[1] = HID_HEADER_0_MINI
    pkt[2] = HID_HEADER_1
    pkt[3] = full_length
    pkt[4] = HID_DATA_TYPE_SYS
    pkt[5] = HID_CATEGORY_SYS
    pkt[6] = cmd
    pkt[7:7 + len(params)] = params
    return bytes(pkt)


def build_enter_bootloader_packet() -> bytes:
    """Build Mini-format packet to command APP to enter bootloader."""
    return build_iap_packet(SYS_CMD_ENTER_BOOTLOADER)


def calc_crc32(data: bytes) -> int:
    """Calculate CRC32 matching the firmware's implementation."""
    return zlib.crc32(data) & 0xFFFFFFFF


def open_device(vid: int, pid: int, retries: int = 10, delay: float = 1.0):
    """Open HID device with retries."""
    for i in range(retries):
        devs = hid.enumerate(vid, pid)
        if devs:
            dev = hid.device()
            dev.open(vid, pid)
            return dev
        if i < retries - 1:
            print(f"  Waiting for device (PID=0x{pid:04X})... ({i + 1}/{retries})")
            time.sleep(delay)
    return None


def send_and_receive(dev, packet: bytes, timeout_ms: int = 5000) -> bytes:
    """Send HID report and wait for response."""
    dev.write(packet)
    resp = dev.read(REPORT_SIZE, timeout_ms)
    if not resp:
        raise TimeoutError("No response from device")
    return bytes(resp)


def enter_bootloader():
    """Send enter-bootloader command to APP."""
    print("Connecting to APP (PID=0x{:04X})...".format(PID_APP))
    dev = open_device(VID, PID_APP, retries=5, delay=1.0)
    if dev is None:
        print("Error: APP device not found")
        return False

    print("Sending enter-bootloader command...")
    pkt = build_enter_bootloader_packet()
    dev.write(pkt)
    dev.close()
    print("Command sent. Device will reset into bootloader.")
    time.sleep(2)
    return True


def download_firmware(firmware_path: str):
    """Download firmware to device via IAP."""
    # Read firmware file
    with open(firmware_path, "rb") as f:
        fw_data = f.read()

    fw_size = len(fw_data)
    fw_crc = calc_crc32(fw_data)
    print(f"Firmware: {firmware_path}")
    print(f"  Size: {fw_size} bytes ({fw_size / 1024:.1f} KB)")
    print(f"  CRC32: 0x{fw_crc:08X}")

    if fw_size > 0xC800:  # 50KB
        print("Error: Firmware too large (max 50KB)")
        return False

    # Connect to bootloader
    print(f"\nConnecting to Bootloader (PID=0x{PID_BOOTLOADER:04X})...")
    dev = open_device(VID, PID_BOOTLOADER, retries=15, delay=1.0)
    if dev is None:
        print("Error: Bootloader device not found")
        return False

    print("Connected!")

    try:
        # Step 1: IAP_CMD_START
        print("\n[1/3] Starting IAP...")
        params = struct.pack("<II", fw_size, fw_crc)
        pkt = build_iap_packet(IAP_CMD_START, params)
        resp = send_and_receive(dev, pkt, timeout_ms=10000)

        if resp[0] != RESP_HEADER_0 or resp[1] != RESP_HEADER_1:
            print("Error: Invalid response header")
            return False
        if resp[3] != IAP_STATUS_OK:
            print(f"Error: IAP_CMD_START failed (status=0x{resp[3]:02X})")
            return False
        print("  Flash erased, ready for data.")

        # Step 2: IAP_CMD_DATA - send firmware in chunks
        print(f"\n[2/3] Downloading firmware ({fw_size} bytes)...")
        offset = 0
        total = fw_size
        last_pct = -1

        while offset < total:
            chunk_size = min(MAX_DATA_PER_PACKET, total - offset)
            chunk = fw_data[offset:offset + chunk_size]

            params = struct.pack("<IB", offset, chunk_size) + chunk
            pkt = build_iap_packet(IAP_CMD_DATA, params)
            resp = send_and_receive(dev, pkt, timeout_ms=5000)

            if resp[3] != IAP_STATUS_OK:
                print(f"\nError: IAP_CMD_DATA failed at offset 0x{offset:06X} (status=0x{resp[3]:02X})")
                return False

            offset += chunk_size
            pct = offset * 100 // total
            if pct != last_pct:
                bar_len = 40
                filled = bar_len * offset // total
                bar = "=" * filled + "-" * (bar_len - filled)
                print(f"\r  [{bar}] {pct:3d}% ({offset}/{total})", end="", flush=True)
                last_pct = pct

        print()  # newline after progress bar

        # Step 3: IAP_CMD_FINISH
        print("\n[3/3] Verifying CRC32...")
        pkt = build_iap_packet(IAP_CMD_FINISH)
        resp = send_and_receive(dev, pkt, timeout_ms=10000)

        if resp[3] == IAP_STATUS_OK:
            print("  CRC32 verified OK!")
            print("\nFirmware download complete! Device will reset to APP.")
            return True
        elif resp[3] == IAP_STATUS_CRC_FAIL:
            print("  Error: CRC32 verification failed!")
            print("  The firmware may be corrupted. Please retry.")
            return False
        else:
            print(f"  Error: IAP_CMD_FINISH failed (status=0x{resp[3]:02X})")
            return False

    except TimeoutError as e:
        print(f"\nError: {e}")
        return False
    except Exception as e:
        print(f"\nError: {e}")
        return False
    finally:
        dev.close()


def main():
    parser = argparse.ArgumentParser(description="USB PD Trigger IAP Firmware Download Tool")
    parser.add_argument("firmware", help="Path to firmware .bin file")
    parser.add_argument("--enter-bootloader", action="store_true",
                        help="Send enter-bootloader command to APP first")

    args = parser.parse_args()

    if args.enter_bootloader:
        if not enter_bootloader():
            sys.exit(1)

    success = download_firmware(args.firmware)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
