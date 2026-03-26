#pragma once

#include "def.h"

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

#define USBD_VID           0xA016
#define USBD_PID           0x0404
#define USBD_MAX_POWER     50
#define USBD_LANGID_STRING 0x0409

void usb_init(void);

uint8_t hid_send_report(const uint8_t *data, uint16_t len);
