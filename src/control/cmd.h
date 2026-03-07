#pragma once

#include "def.h"

typedef enum cmd_type_t
{
    CMD_TYPE_USB_PD,
    CMD_TYPE_SYS,
} cmd_type_t;

typedef enum usb_pd_cmd_code_type_t
{
    SELECT_PDO,
    NEXT_PDO,
    PREV_PDO,
    SET_VOLTAGE_MV,
    SET_CURRENT_MA,
    SET_POWER_CW,
    INCREASE_VOLTAGE_MV,
    DECREASE_VOLTAGE_MV,
    INCREASE_CURRENT_MA,
    DECREASE_CURRENT_MA,
    INCREASE_POWER_CW,
    DECREASE_POWER_CW,
} usb_pd_cmd_code_type_t;

typedef enum sys_cmd_code_type_t
{
    SYS_CMD_MCU_REBOOT          = 0x00,
    SYS_CMD_USB_PD_REBOOT       = 0x01,
    SYS_CMD_VBUS_ON             = 0x11,
    SYS_CMD_VBUS_OFF            = 0x12,
    SYS_CMD_SET_HID_PRIORITY    = 0x21,
    SYS_CMD_SET_REPLY_PRIORITY  = 0x22,
    SYS_CMD_SET_SINK_MODE_SPR   = 0x31,
    SYS_CMD_SET_SINK_MODE_EPR   = 0x32,
    SYS_CMD_SET_SINK_MODE_PROP  = 0x33,
    SYS_CMD_SET_HID_REPORT_STD  = 0x41,
    SYS_CMD_SET_HID_REPORT_MINI = 0x42,
} sys_cmd_code_type_t;

void cmd_process_next(void);