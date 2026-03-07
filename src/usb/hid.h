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

// HID RX FIFO depth (per queue)
#define HID_RX_FIFO_DEPTH 8

// HID command packet — Standard format (header 0x51FF)
// Header(2B) | Time(4B) | Counter(4B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]        [2-5]      [6-9]          [10]              [11]        [12+]
//
// HID command packet — Mini format (header 0x52FF)
// Header(2B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]          [2]              [3]          [4+]
#define HID_CMD_HEADER_0_STD          0x51
#define HID_CMD_HEADER_0_MINI         0x52
#define HID_CMD_HEADER_1              0xFF
#define HID_CMD_STD_DATA_TYPE_OFFSET  11
#define HID_CMD_MINI_DATA_TYPE_OFFSET 3
#define HID_CMD_TYPE_PD               0x00
#define HID_CMD_TYPE_SYS              0x01

typedef struct hid_rx_buf_t
{
    uint8_t buf[HID_RX_FIFO_DEPTH][HIDRAW_OUT_EP_SIZE];
    volatile uint8_t head; // written by ISR (producer)
    volatile uint8_t tail; // written by main loop (consumer)
} hid_rx_buf_t;

void usb_init(void);

uint8_t hid_send_report(const uint8_t *data, uint16_t len);

// Buffer-based receive API (dual queue: PD / System)
bool hid_rx_buf_has_pd(void);
bool hid_rx_buf_has_sys(void);
void hid_rx_buf_pop_pd(void);
void hid_rx_buf_pop_sys(void);
void hid_rx_buf_clear_pd(void);
void hid_rx_buf_clear_sys(void);
bool hid_rx_buf_peek_pd(const uint8_t **data);
bool hid_rx_buf_peek_sys(const uint8_t **data);
