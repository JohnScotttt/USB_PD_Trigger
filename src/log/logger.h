#pragma once

#include "def.h"

// 主要用于处理写入间隔小于 USB FS HID 端点轮询间隔（1 ms）的缓冲，防止瞬时大量写入丢包
#define LOG_BUFFER_SIZE 16

typedef struct log_entry_t //__attribute__((packed))
{
    uint32_t timestamp_us;            // 运行时间
    uint32_t counter;                 // 已接收消息数量
    data_type_t type;                 // 消息类型
    uint8_t data_len;                 // 消息长度
    uint8_t data[USB_PD_DATA_MAX_LEN]; // 消息数据
} log_entry_t;

typedef struct log_buf_t
{
    volatile uint32_t overflow_count;  // 溢出计数
    volatile uint16_t head;            // 写指针
    volatile uint16_t tail;            // 读指针
    log_entry_t log[LOG_BUFFER_SIZE];  // 消息数组
} log_buf_t;

typedef struct __attribute__((packed)) usb_hid_std_report_t
{
    uint16_t header;                     // 2 byte
    uint32_t time;                       // 4 byte
    uint32_t counter;                    // 4 byte
    uint8_t length;                      // 1 byte
    uint8_t type;                        // 1 byte
    uint8_t data[USB_PD_DATA_MAX_LEN];   // 35 byte
    uint8_t _reserved[17];               // 17 byte
} usb_hid_std_report_t;                  // 2+4+4+1+1+35+17 = 64 byte

typedef struct __attribute__((packed)) usb_hid_mini_report_t
{
    uint16_t header;                     // 2 byte
    uint8_t length;                      // 1 byte
    uint8_t type;                        // 1 byte
    uint8_t data[USB_PD_DATA_MAX_LEN];   // 35 byte
    uint8_t _reserved[25];               // 25 byte
} usb_hid_mini_report_t;                 // 2+1+1+35+25 = 64 byte

typedef union usb_hid_report_t
{
    usb_hid_std_report_t std_report;
    usb_hid_mini_report_t mini_report;
} usb_hid_report_t;

void log_save_usb_pd_rx(uint8_t status, const uint8_t *data, uint16_t len);
void log_save_usb_pd_tx(uint8_t sop, const uint8_t *data, uint16_t len);
void log_save_sys(system_type_t sys_type);
void log_save_usb_pd_connect_change(usb_pd_cc_channel_type_t old_channel, usb_pd_cc_channel_type_t new_channel);

void log_reset(void);

void log_process_next(void);
