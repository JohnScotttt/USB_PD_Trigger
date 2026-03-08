#pragma once

#include "def.h"

#define UART_STATE_IDLE 0
#define UART_STATE_BUSY 1

#define UART_RX_BUF_SIZE  64
#define UART_TX_BUF_SIZE  64
#define UART_RX_FIFO_DEPTH 8

// UART command packet — Standard format (header 0x51FF)
// Header(2B) | Time(4B) | Counter(4B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]        [2-5]      [6-9]          [10]              [11]        [12+]
//
// UART command packet — Mini format (header 0x52FF)
// Header(2B) | FullLength(1B) | DataType(1B) | Data
//  [0-1]          [2]              [3]          [4+]
#define UART_CMD_HEADER_0_STD          0x51
#define UART_CMD_HEADER_0_MINI         0x52
#define UART_CMD_HEADER_1              0xFF
#define UART_CMD_STD_DATA_TYPE_OFFSET  11
#define UART_CMD_MINI_DATA_TYPE_OFFSET 3
#define UART_CMD_TYPE_PD               0x00
#define UART_CMD_TYPE_SYS              0x01

typedef struct uart_rx_buf_t
{
    uint8_t buf[UART_RX_FIFO_DEPTH][UART_RX_BUF_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} uart_rx_buf_t;

void uart_init(void);

uint8_t uart_send_report(const uint8_t *data, uint16_t len);

bool uart_rx_buf_has_pd(void);
bool uart_rx_buf_has_sys(void);
void uart_rx_buf_pop_pd(void);
void uart_rx_buf_pop_sys(void);
void uart_rx_buf_clear_pd(void);
void uart_rx_buf_clear_sys(void);
bool uart_rx_buf_peek_pd(const uint8_t **data);
bool uart_rx_buf_peek_sys(const uint8_t **data);