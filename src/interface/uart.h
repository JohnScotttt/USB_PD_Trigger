#pragma once

#include "def.h"

#define UART_STATE_IDLE 0
#define UART_STATE_BUSY 1

#define UART_RX_BUF_SIZE  64
#define UART_TX_BUF_SIZE  64

void uart_init(void);

uint8_t uart_send_report(const uint8_t *data, uint16_t len);