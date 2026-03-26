#include "host.h"
#include "hid.h"
#include "uart.h"
#include "delay.h"

// ============================================================================
// Critical section helpers — save/restore WCH INTSYSCR (CSR 0x800)
// so that calling from ISR context does not prematurely re-enable interrupts.
// WCH QingKe V4F uses CSR 0x800 with bit mask 0x88 for global interrupt
// control, NOT the standard RISC-V mstatus.MIE.
// ============================================================================

static inline uint32_t irq_save(void)
{
    uint32_t val;
    __asm__ volatile("csrr %0, 0x800" : "=r"(val));
    __asm__ volatile("csrc 0x800, %0" : : "r"(0x88));
    __asm__ volatile("fence.i");
    return val;
}

static inline void irq_restore(uint32_t val)
{
    __asm__ volatile("csrw 0x800, %0" : : "r"(val));
}

// ============================================================================
// Shared RX ring buffers (fed by both HID and UART)
// ============================================================================

static host_rx_buf_t pd_buf;
static host_rx_buf_t sys_buf;

#define HOST_RING_INC(x) (((x) + 1) % HOST_RX_FIFO_DEPTH)

static inline bool rx_fifo_is_full(const host_rx_buf_t *f)
{
    return HOST_RING_INC(f->head) == f->tail;
}

static inline bool rx_fifo_has_data(const host_rx_buf_t *f)
{
    return f->head != f->tail;
}

static void rx_fifo_push(host_rx_buf_t *f, const uint8_t *data, uint16_t len)
{
    uint32_t saved = irq_save();
    if (rx_fifo_is_full(f))
    {
        irq_restore(saved);
        return;
    }
    memset(f->buf[f->head], 0, HOST_RX_BUF_SIZE);
    memcpy(f->buf[f->head], data, len > HOST_RX_BUF_SIZE ? HOST_RX_BUF_SIZE : len);
    f->head = HOST_RING_INC(f->head);
    irq_restore(saved);
}

static void rx_fifo_pop(host_rx_buf_t *f)
{
    uint32_t saved = irq_save();
    if (rx_fifo_has_data(f))
    {
        f->tail = HOST_RING_INC(f->tail);
    }
    irq_restore(saved);
}

static void rx_fifo_clear(host_rx_buf_t *f)
{
    uint32_t saved = irq_save();
    f->head = 0;
    f->tail = 0;
    irq_restore(saved);
}

static bool rx_fifo_peek(const host_rx_buf_t *f, const uint8_t **data)
{
    uint32_t saved = irq_save();
    if (!rx_fifo_has_data(f))
    {
        irq_restore(saved);
        return false;
    }
    *data = f->buf[f->tail];
    irq_restore(saved);
    return true;
}

// ============================================================================
// Packet dispatch — validates header and routes to PD or SYS queue
// ============================================================================

void host_rx_dispatch(const uint8_t *buf, uint16_t len)
{
    if (len < HOST_CMD_MINI_DATA_TYPE_OFFSET + 1) return;
    if (buf[1] != HOST_CMD_HEADER_1) return;

    uint8_t dt_offset = 0;
    bool valid = false;

    if (buf[0] == HOST_CMD_HEADER_0_STD && len >= HOST_CMD_STD_DATA_TYPE_OFFSET + 1)
    {
        dt_offset = HOST_CMD_STD_DATA_TYPE_OFFSET;
        valid = true;
    }
    else if (buf[0] == HOST_CMD_HEADER_0_MINI)
    {
        dt_offset = HOST_CMD_MINI_DATA_TYPE_OFFSET;
        valid = true;
    }

    if (valid)
    {
        uint8_t data_type = buf[dt_offset];
        if (data_type == HOST_CMD_TYPE_PD)
            rx_fifo_push(&pd_buf, buf, len);
        else if (data_type == HOST_CMD_TYPE_SYS)
            rx_fifo_push(&sys_buf, buf, len);
    }
}

// ============================================================================
// Public RX API
// ============================================================================

bool host_rx_has_pd(void)
{
    return rx_fifo_has_data(&pd_buf);
}

bool host_rx_has_sys(void)
{
    return rx_fifo_has_data(&sys_buf);
}

bool host_rx_peek_pd(const uint8_t **data)
{
    return rx_fifo_peek(&pd_buf, data);
}

bool host_rx_peek_sys(const uint8_t **data)
{
    return rx_fifo_peek(&sys_buf, data);
}

void host_rx_pop_pd(void)
{
    rx_fifo_pop(&pd_buf);
}

void host_rx_pop_sys(void)
{
    rx_fifo_pop(&sys_buf);
}

void host_rx_clear_pd(void)
{
    rx_fifo_clear(&pd_buf);
}

void host_rx_clear_sys(void)
{
    rx_fifo_clear(&sys_buf);
}

// ============================================================================
// TX — broadcast to both HID and UART
//
// Tracks per-channel completion. Returns 1 only when both channels have
// successfully sent, or HOST_TX_TIMEOUT_MS has elapsed (give up remaining).
// Caller (logger) should retry with the SAME data until this returns 1.
// ============================================================================

static uint8_t  tx_hid_done;
static uint8_t  tx_uart_done;
static uint32_t tx_start_ms;
static uint8_t  tx_active;

uint8_t host_send_report(const uint8_t *data, uint16_t len)
{
    if (!tx_active)
    {
        tx_hid_done  = 0;
        tx_uart_done = 0;
        tx_start_ms  = millis();
        tx_active    = 1;
    }

    if (!tx_hid_done && hid_send_report(data, len))
        tx_hid_done = 1;

    if (!tx_uart_done && uart_send_report(data, len))
        tx_uart_done = 1;

    bool timeout = (millis() - tx_start_ms) >= HOST_TX_TIMEOUT_MS;

    if ((tx_hid_done && tx_uart_done) || timeout)
    {
        tx_active = 0;
        return 1;
    }

    return 0;
}
