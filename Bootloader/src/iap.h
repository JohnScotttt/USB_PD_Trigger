#pragma once

#include <ch32x035.h>
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// IAP command codes (custom protocol)
#define IAP_CMD_START   0x80    // Begin IAP: [fw_size(4B)][crc32(4B)]
#define IAP_CMD_DATA    0x81    // Data packet: [offset(4B)][len(1B)][data(<=57B)]
#define IAP_CMD_FINISH  0x82    // Finish: trigger CRC verify, clear CalAddr
#define IAP_CMD_STATUS  0x83    // Query status/progress

// IAP status codes
#define IAP_STATUS_OK       0x00
#define IAP_STATUS_ERROR    0x01
#define IAP_STATUS_BUSY     0x02
#define IAP_STATUS_CRC_FAIL 0x03

// IAP state machine
typedef enum {
    IAP_STATE_IDLE = 0,
    IAP_STATE_RECEIVING,
    IAP_STATE_VERIFYING,
    IAP_STATE_DONE,
    IAP_STATE_ERROR,
} iap_state_t;

typedef struct {
    iap_state_t state;
    uint32_t    fw_size;        // Expected firmware size
    uint32_t    fw_crc32;       // Expected CRC32
    uint32_t    bytes_received; // Total bytes programmed so far
} iap_context_t;

// Initialize IAP context
void iap_init(void);

// Process IAP_CMD_START: prepare for firmware download
uint8_t iap_cmd_start(uint32_t fw_size, uint32_t fw_crc32);

// Process IAP_CMD_DATA: program data to flash
uint8_t iap_cmd_data(uint32_t offset, const uint8_t *data, uint8_t len);

// Process IAP_CMD_FINISH: verify CRC and finalize
uint8_t iap_cmd_finish(void);

// Get current IAP state
iap_state_t iap_get_state(void);

// Get bytes received
uint32_t iap_get_bytes_received(void);

// Check if APP area is valid (first word not 0xFFFFFFFF)
bool iap_is_app_valid(void);

// Check if CalAddr contains CheckNum (bootloader requested by APP)
bool iap_is_update_requested(void);

// Clear CalAddr flag
void iap_clear_update_flag(void);

// CRC32 calculation over flash region
uint32_t iap_calc_crc32(uint32_t addr, uint32_t len);

// Jump to APP via Software interrupt
void iap_jump_to_app(void);
