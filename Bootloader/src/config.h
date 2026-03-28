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
