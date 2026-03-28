#pragma once

#include <ch32x035.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HID_STATE_IDLE 0
#define HID_STATE_BUSY 1

// hidraw in endpoint
#define HIDRAW_IN_EP       0x81
#define HIDRAW_IN_EP_SIZE  64
#define HIDRAW_IN_INTERVAL 1

// hidraw out endpoint
#define HIDRAW_OUT_EP          0x02
#define HIDRAW_OUT_EP_SIZE     64
#define HIDRAW_OUT_EP_INTERVAL 10

// Bootloader uses a different PID
#define USBD_VID           0xA016
#define USBD_PID           0x0405
#define USBD_MAX_POWER     50
#define USBD_LANGID_STRING 0x0409

// HID command packet - Mini format (header 0x52FF)
// Header(2B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]          [2]              [3]          [4+]
#define HID_CMD_HEADER_0_MINI         0x52
#define HID_CMD_HEADER_1              0xFF
#define HID_CMD_MINI_DATA_TYPE_OFFSET 3
#define HID_CMD_TYPE_SYS              0x01

// Response header
#define HID_RESP_HEADER_0             0xA2
#define HID_RESP_HEADER_1             0xFF

void usb_init(void);
uint8_t hid_send_report(const uint8_t *data, uint16_t len);

// IAP command processing callback - called from HID OUT callback
// Returns true if a new command packet is available
bool hid_has_iap_cmd(void);
const uint8_t *hid_get_iap_cmd(void);
void hid_pop_iap_cmd(void);
