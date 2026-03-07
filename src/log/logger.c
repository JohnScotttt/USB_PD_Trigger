#include "logger.h"
#include "vbus/sensor.h"
#include "usb/hid.h"
#include "delay.h"
#include "control/status.h"

static volatile uint32_t          log_counter = 0;
static log_buf_t                  log_buffer  = {0};

// ============================================================================
// Ring Buffer
// ============================================================================

#define LOG_RING_INC(x) (((x) + 1) % LOG_BUFFER_SIZE)

static inline bool log_buf_is_full(void)
{
    return (LOG_RING_INC(log_buffer.head) == log_buffer.tail);
}

static inline bool log_buf_has_data(void)
{
    return (log_buffer.head != log_buffer.tail);
}

static inline void log_buf_clear(void)
{
    log_buffer.head = 0;
    log_buffer.tail = 0;
}

static inline uint32_t log_buf_get_overflow_count(void)
{
    return log_buffer.overflow_count;
}

static inline uint32_t log_buf_get_count(void)
{
    return (log_buffer.head >= log_buffer.tail)
               ? (log_buffer.head - log_buffer.tail)
               : (LOG_BUFFER_SIZE - log_buffer.tail + log_buffer.head);
}

static void log_buf_push(const log_entry_t *log)
{
    if (log == NULL)
    {
        return;
    }

    // 检查是否已满
    if (log_buf_is_full())
    {
        log_buffer.overflow_count++;
        // 覆盖最旧的消息 移动尾指针
        log_buffer.tail = LOG_RING_INC(log_buffer.tail);
    }

    // 写入消息
    memcpy(&log_buffer.log[log_buffer.head], log, sizeof(log_entry_t));

    // 更新头指针
    log_buffer.head = LOG_RING_INC(log_buffer.head);
}

static void log_buf_pop(void)
{
    // 检查是否为空
    if (!log_buf_has_data())
    {
        return;
    }

    // 更新尾指针
    log_buffer.tail = LOG_RING_INC(log_buffer.tail);
}

static bool log_buf_peek(const log_entry_t **log)
{
    if (log == NULL)
    {
        return false;
    }

    // 检查是否为空
    if (!log_buf_has_data())
    {
        return false;
    }

    // 返回指向内部缓冲区的指针
    *log = &log_buffer.log[log_buffer.tail];

    return true;
}

// ============================================================================
// Log Processing (-> HID)
// ============================================================================

void log_save_usb_pd_rx(uint8_t status, const uint8_t *data, uint16_t len)
{
    // 限制数据长度
    if (len > USB_PD_DATA_MAX_LEN - 1)
    {
        len = USB_PD_DATA_MAX_LEN - 1;
    }

    log_entry_t log = {0};
    log.type = USB_PD;
    log.counter = ++log_counter;
    log.timestamp_us = millis();
    log.data_len = len + 1;
    uint8_t sop = status & BMC_AUX_Mask;
    if (status & IF_RX_RESET)
    {
        log.data[0] = (sop == PD_RX_SOP1_HRST) ? HARD_RESET : CABLE_RESET;
    }
    else
    {
        log.data[0] = (sop == PD_RX_SOP0)      ? SOP
                    : (sop == PD_RX_SOP1_HRST) ? SOP1
                    : (sop == PD_RX_SOP2_CRST) ? SOP2
                                               : 0xFF;
    }

    memcpy(&log.data[1], data, len);

    log_buf_push(&log);
}

void log_save_usb_pd_tx(uint8_t sop, const uint8_t *data, uint16_t len)
{
    // 限制数据长度
    if (len > USB_PD_DATA_MAX_LEN - 1)
    {
        len = USB_PD_DATA_MAX_LEN - 1;
    }

    // 填充消息
    log_entry_t log = {0};
    log.type = USB_PD;
    log.counter = ++log_counter;
    log.timestamp_us = millis();
    log.data_len = len + 1;
    log.data[0] = (sop == UPD_HARD_RESET)  ? HARD_RESET
                : (sop == UPD_CABLE_RESET) ? CABLE_RESET
                : (sop == UPD_SOP0)        ? SOP
                : (sop == UPD_SOP1)        ? SOP1
                : (sop == UPD_SOP2)        ? SOP2
                                           : 0xFF;
    memcpy(&log.data[1], data, len);

    log_buf_push(&log);
}

void log_save_sys(system_type_t sys_evt_type)
{
    log_entry_t log = {0};
    log.type = SYSTEM;
    log.counter = ++log_counter;
    log.timestamp_us = millis();
    log.data_len = 1;
    log.data[0] = (uint8_t)sys_evt_type;

    log_buf_push(&log);
}

void log_save_usb_pd_connect_change(usb_pd_cc_channel_type_t old_channel, usb_pd_cc_channel_type_t new_channel)
{
    system_type_t sys_type = (old_channel == PD_CC1 && new_channel == PD_CC_NONE) ? DETACH_CC1
                           : (old_channel == PD_CC2 && new_channel == PD_CC_NONE) ? DETACH_CC2
                           : (old_channel == PD_CC_NONE && new_channel == PD_CC1) ? ATTACH_CC1
                           : (old_channel == PD_CC_NONE && new_channel == PD_CC2) ? ATTACH_CC2
                                                                                  : 0;

    // if (new_channel == PD_CC_NONE)
    // {
    //     log_reset();
    // }

    log_save_sys(sys_type);
}

void log_reset(void)
{
    log_buf_clear();
    // event_counter = 0;
}

// O3 优化数据会有异常
// __attribute__((optimize("O0")))
static bool hid_report_format(const log_entry_t *log, usb_hid_report_t *report)
{
    if (log == NULL || report == NULL)
    {
        return false;
    }
    // memset(report, 0, sizeof(hid_report_t));
    // __asm__ __volatile__("" ::: "memory");

    // 填充报告字段
    if (get_usb_hid_report_type() == HID_REPORT_TYPE_MINI)
    {
        report->mini_report.header = 0xFFA2; // Minimal data
        report->mini_report.type = log->type;
        report->mini_report.length = (log->data_len > USB_PD_DATA_MAX_LEN) ? USB_PD_DATA_MAX_LEN : log->data_len;
        memcpy(report->mini_report.data, log->data, report->mini_report.length);
        memset(report->mini_report.data + report->mini_report.length, 0, USB_PD_DATA_MAX_LEN - report->mini_report.length);
        memset(report->mini_report._reserved, 0, sizeof(report->mini_report._reserved));
        report->mini_report.length += 1;
    }
    else if (get_usb_hid_report_type() == HID_REPORT_TYPE_STD)
    {
        report->std_report.header = 0xFFA1; // Standard data
        report->std_report.time = log->timestamp_us;
        report->std_report.counter = log->counter;
        report->std_report.type = log->type;
        report->std_report.length = (log->data_len > USB_PD_DATA_MAX_LEN) ? USB_PD_DATA_MAX_LEN : log->data_len;
        memcpy(report->std_report.data, log->data, report->std_report.length);
        memset(report->std_report.data + report->std_report.length, 0, USB_PD_DATA_MAX_LEN - report->std_report.length);
        memset(report->std_report._reserved, 0, sizeof(report->std_report._reserved));
        report->std_report.length += 1;
    }

    return true;
}

static bool send(const log_entry_t *log)
{
    usb_hid_report_t report;

    if (!hid_report_format(log, &report))
    {
        return false;
    }
    
    return hid_send_report((uint8_t *)&report, sizeof(usb_hid_std_report_t));
}

void log_process_next(void)
{
    if (log_buf_has_data())
    {
        // 查看消息但不移除
        const log_entry_t *log = NULL;
        if (log_buf_peek(&log))
        {
            // 尝试发送
            if (send(log))
            {
                // 发送成功，移除消息
                log_buf_pop();
            }
            // 发送失败则保留在缓冲区，下次重试
        }
    }
}
