#pragma once

#include "def.h"

#define HOST_RX_BUF_SIZE   64
#define HOST_RX_FIFO_DEPTH 8

// Command packet format constants (shared by HID and UART)
//
// Standard format (header 0x51FF):
// Header(2B) | Time(4B) | Counter(4B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]        [2-5]      [6-9]          [10]              [11]        [12+]
//
// Mini format (header 0x52FF):
// Header(2B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]          [2]              [3]          [4+]
#define HOST_CMD_HEADER_0_STD          0x51
#define HOST_CMD_HEADER_0_MINI         0x52
#define HOST_CMD_HEADER_1              0xFF
#define HOST_CMD_STD_DATA_TYPE_OFFSET  11
#define HOST_CMD_MINI_DATA_TYPE_OFFSET 3
#define HOST_CMD_TYPE_PD               0x00
#define HOST_CMD_TYPE_SYS              0x01

typedef struct host_rx_buf_t
{
    uint8_t buf[HOST_RX_FIFO_DEPTH][HOST_RX_BUF_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} host_rx_buf_t;

// RX buffer API (shared PD / SYS queues fed by both HID and UART)
bool host_rx_has_pd(void);
bool host_rx_has_sys(void);
bool host_rx_peek_pd(const uint8_t **data);
bool host_rx_peek_sys(const uint8_t **data);
void host_rx_pop_pd(void);
void host_rx_pop_sys(void);
void host_rx_clear_pd(void);
void host_rx_clear_sys(void);

// Packet dispatch — called from HID OUT callback and UART IDLE ISR
void host_rx_dispatch(const uint8_t *buf, uint16_t len);

// TX — broadcast to both HID and UART channels
// Returns 1 when both channels have sent (or timed out).
#define HOST_TX_TIMEOUT_MS 20

uint8_t host_send_report(const uint8_t *data, uint16_t len);
