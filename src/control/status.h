#pragma once

#include "def.h"

typedef enum usb_pd_msg_priority_type_t
{
    HID_PRIORITY,
    REPLY_PRIORITY,
} usb_pd_msg_priority_type_t;

typedef enum usb_pd_sink_mode_type_t
{
    SPR_MODE,
    EPR_MODE,
    PROP_MODE,
} usb_pd_sink_mode_type_t;

typedef enum usb_hid_report_type_t
{
    HID_REPORT_TYPE_STD,
    HID_REPORT_TYPE_MINI,
} usb_hid_report_type_t;

void status_init(void);

void set_usb_pd_msg_priority(usb_pd_msg_priority_type_t priority);
usb_pd_msg_priority_type_t get_usb_pd_msg_priority(void);

void set_usb_pd_sink_mode(usb_pd_sink_mode_type_t mode);
usb_pd_sink_mode_type_t get_usb_pd_sink_mode(void);

void set_usb_hid_report_type(usb_hid_report_type_t type);
usb_hid_report_type_t get_usb_hid_report_type(void);

void vbus_out_enable(bool enable);