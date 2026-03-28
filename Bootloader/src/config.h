// ========================
//         引脚配置
// ------------------------
//        PC_DP: PC17 *
//        PC_DM: PC16 *
// ========================

// =====================================
//           Data Packet Formats
// -------------------------------------
// |              Field | Minimal data |
// |        Header (2B) |       0x_2FF |
// |   Full Length (1B) |            ✓ |
// |     Data Type (1B) |            ✓ |
// |               Data |            ✓ |
// =====================================
//
// Header: 0x[ST]FF, where S = Source, T = packet Type
//   Source: A = Device, 5 = Host
//   Type:   2 = Minimal
//   e.g. 0xA2FF = Device Minimal, 0x52FF = Host Minimal
//
// Full Length: The length from the byte immediately following the "Full Length" field to the end of the packet.
//
// ===========================================================
//     Host → Bootloader Request (Header: 0x52FF)
// -----------------------------------------------------------
// |  Byte |                              Field |     Value  |
// |   0-1 |                      Header (Mini) |    0x52FF  |
// |     2 |                       Full Length  |   varies   |
// |     3 |               Data Type (System)   |     0x01   |
// |     4 |             Category (System)      |     0x01   |
// |     5 |                           Command  |   see below|
// |    6+ |                        Parameters  |   varies   |
// ===========================================================
//
// ===========================================================
//     Bootloader → Host Response (Header: 0xA2FF)
// -----------------------------------------------------------
// |  Byte |                              Field |     Value  |
// |   0-1 |                      Header (Mini) |    0xA2FF  |
// |     2 |                       Command Echo |   0x80-83  |
// |     3 |                       Status Code  |   see below|
// |    4+ |                   Response Data    |   varies   |
// ===========================================================
//
// ====================================      ==============================
//       IAP Command Codes                      IAP Status Codes
// ------------------------------------      ------------------------------
// |  Hex |               Description |      |  Hex |         Description |
// | 0x80 |             IAP_CMD_START |      | 0x00 |      IAP_STATUS_OK  |
// | 0x81 |              IAP_CMD_DATA |      | 0x01 |   IAP_STATUS_ERROR  |
// | 0x82 |            IAP_CMD_FINISH |      | 0x02 |    IAP_STATUS_BUSY  |
// | 0x83 |            IAP_CMD_STATUS |      | 0x03 | IAP_STATUS_CRC_FAIL |
// ====================================      ==============================
//
// ========================================================================
//                         IAP Command Details
// ------------------------------------------------------------------------
//  CMD_START  (0x80): Begin firmware update
//    Request Params:  [fw_size (4B LE)] [crc32 (4B LE)]
//    Response:        [status (1B)]
//
//  CMD_DATA   (0x81): Write firmware data chunk
//    Request Params:  [offset (4B LE)] [len (1B)] [data (<=53B)]
//    Response:        [status (1B)] [bytes_received (4B LE)]
//
//  CMD_FINISH (0x82): Verify CRC32 and finalize
//    Request Params:  (none)
//    Response:        [status (1B)]
//
//  CMD_STATUS (0x83): Query IAP progress
//    Request Params:  (none)
//    Response:        [status (1B)] [state (1B)] [bytes_received (4B LE)]
// ========================================================================

#pragma once

// Bootloader version
#define CONFIG_BL_VERSION_MAJOR 1
#define CONFIG_BL_VERSION_MINOR 0
#define CONFIG_BL_VERSION_PATCH 0

// Hardware version (same as APP)
#define CONFIG_HW_VERSION_MAJOR 1
#define CONFIG_HW_VERSION_MINOR 6

// Memory layout (0x08000000 base for flash programming APIs)
#define FLASH_BOOTLOADER_BASE   0x08000000
#define FLASH_BOOTLOADER_SIZE   0x3000      // 12KB
#define FLASH_APP_BASE          0x08003000
#define FLASH_APP_SIZE          0xC800      // 50KB
#define FLASH_TOTAL_SIZE        0xF800      // 62KB

// CalAddr: flag location for entering bootloader (last 4 bytes before flash end)
#define CAL_ADDR                (0x0800F800 - 4)  // 0x0800F7FC
#define CHECK_NUM               0x5AA55AA5

// Flash page size for CH32X035 fast programming
#define FLASH_PAGE_SIZE         256
