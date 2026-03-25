#pragma once

#include "def.h"

typedef enum usb_pd_msg_priority_type_t
{
    IF_PRIORITY,
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

typedef enum vbus_en_status_type_t
{
    VBUS_ALWAYS_OFF,
    VBUS_ALWAYS_ON,
    VBUS_HOLD,
    VBUS_HOLD_OFF,
    VBUS_HOLD_ON,
} vbus_en_status_type_t;

typedef enum trigger_hold_status_type_t
{
    TRIGGER_HOLD_OFF,
    TRIGGER_HOLD_ON,
} trigger_hold_status_type_t;

void status_init(void);

void set_usb_pd_msg_priority(usb_pd_msg_priority_type_t priority);
usb_pd_msg_priority_type_t get_usb_pd_msg_priority(void);

void set_usb_pd_sink_mode(usb_pd_sink_mode_type_t mode);
usb_pd_sink_mode_type_t get_usb_pd_sink_mode(void);

void set_usb_hid_report_type(usb_hid_report_type_t type);
usb_hid_report_type_t get_usb_hid_report_type(void);

void vbus_out_enable(bool enable);
void set_vbus_en_status(vbus_en_status_type_t status);
vbus_en_status_type_t get_vbus_en_status(void);

void set_fram_capacity(uint32_t capacity);
uint32_t get_fram_capacity(void);

void set_trigger_hold_status(trigger_hold_status_type_t status);
trigger_hold_status_type_t get_trigger_hold_status(void);