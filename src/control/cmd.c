#include "cmd.h"
#include "interface/host.h"
#include "status.h"
#include "usb_pd/event.h"
#include "usb_pd/phy.h"
#include "vbus/sensor.h"
#include "delay.h"

static void cmd_forward_pd(uint8_t pd_cmd_code, int32_t value)
{
    switch (pd_cmd_code)
    {
        case SELECT_PDO:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_SELECT_PDO, value});
            break;
        }
        case NEXT_PDO:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_NEXT_PDO, 0});
            break;
        }
        case PREV_PDO:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_PREV_PDO, 0});
            break;
        }
        case SET_VOLTAGE_MV:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_SET_VOLTAGE_MV, value});
            break;
        }
        case SET_CURRENT_MA:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_SET_CURRENT_MA, value});
            break;
        }
        case SET_POWER_CW:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_SET_POWER_CW, value});
            break;
        }
        case INCREASE_VOLTAGE_MV:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_ADJUST_VOLTAGE_MV, value});
            break;
        }
        case DECREASE_VOLTAGE_MV:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_ADJUST_VOLTAGE_MV, -value});
            break;
        }
        case INCREASE_CURRENT_MA:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_ADJUST_CURRENT_MA, value});
            break;
        }
        case DECREASE_CURRENT_MA:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_ADJUST_CURRENT_MA, -value});
            break;
        }
        case INCREASE_POWER_CW:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_ADJUST_POWER_CW, value});
            break;
        }
        case DECREASE_POWER_CW:
        {
            usb_pd_cmd_execute(&(usb_pd_cmd_t){USB_PD_CMD_ADJUST_POWER_CW, -value});
            break;
        }
        default:
            break;
    }
}

static void cmd_forward_sys(uint8_t sys_cmd_code)
{
    switch (sys_cmd_code)
    {
        case SYS_CMD_MCU_REBOOT:
        {
            NVIC_SystemReset();
            break;
        }
        case SYS_CMD_USB_PD_REBOOT:
        {
            uint16_t vbus_before = adc_get_vbus_in_mv();
            usb_pd_phy_send_hard_reset();
            delay_ms(200);
            uint16_t vbus_after = adc_get_vbus_in_mv();
            if (vbus_before - vbus_after < 500)
            {
                usb_pd_phy_cc_rd_enable(false);
                delay_ms(500);
                usb_pd_phy_cc_rd_enable(true);
            }
            break;
        }
        case SYS_CMD_GET_STATUS:
        {
            // TODO: 通过 HID 报告发送当前状态
            break;
        }
        case SYS_CMD_VBUS_ON:
        {
            vbus_out_enable(true);
            break;
        }
        case SYS_CMD_VBUS_OFF:
        {
            vbus_out_enable(false);
            break;
        }
        case SYS_CMD_SET_HID_PRIORITY:
        {
            set_usb_pd_msg_priority(IF_PRIORITY);
            break;
        }
        case SYS_CMD_SET_REPLY_PRIORITY:
        {
            set_usb_pd_msg_priority(REPLY_PRIORITY);
            break;
        }
        case SYS_CMD_SET_SINK_MODE_SPR:
        {
            set_usb_pd_sink_mode(SPR_MODE);
            delay_ms(10);
            usb_pd_phy_send_hard_reset();
            break;
        }
        case SYS_CMD_SET_SINK_MODE_EPR:
        {
            set_usb_pd_sink_mode(EPR_MODE);
            delay_ms(10);
            usb_pd_phy_send_hard_reset();
            break;
        }
        case SYS_CMD_SET_SINK_MODE_PROP:
        {
            set_usb_pd_sink_mode(PROP_MODE);
            delay_ms(10);
            usb_pd_phy_send_hard_reset();
            break;
        }
        case SYS_CMD_SET_HID_REPORT_STD:
        {
            set_usb_hid_report_type(HID_REPORT_TYPE_STD);
            break;
        }
        case SYS_CMD_SET_HID_REPORT_MINI:
        {
            set_usb_hid_report_type(HID_REPORT_TYPE_MINI);
            break;
        }
        case SYS_CMD_SET_VBUS_ALWAYS_OFF:
        {
            set_vbus_en_status(VBUS_ALWAYS_OFF);
            break;
        }
        case SYS_CMD_SET_VBUS_ALWAYS_ON:
        {
            set_vbus_en_status(VBUS_ALWAYS_ON);
            break;
        }
        case SYS_CMD_SET_VBUS_HOLD:
        {
            set_vbus_en_status(VBUS_HOLD);
            break;
        }
        case SYS_CMD_TRIGGER_HOLD_ON:
        {
            set_trigger_hold_status(TRIGGER_HOLD_ON);
            break;
        }
        case SYS_CMD_TRIGGER_HOLD_OFF:
        {
            set_trigger_hold_status(TRIGGER_HOLD_OFF);
            break;
        }
        default:
            break;
    }
}

void cmd_process_next(void)
{
    if (!host_rx_has_sys())
    {
        return;
    }

    const uint8_t *data = NULL;
    if (!host_rx_peek_sys(&data))
    {
        return;
    }

    uint8_t buf[HOST_RX_BUF_SIZE] = {0};
    memcpy(buf, data, HOST_RX_BUF_SIZE);
    host_rx_pop_sys();

    uint8_t d = (buf[0] == HOST_CMD_HEADER_0_STD)
              ? (HOST_CMD_STD_DATA_TYPE_OFFSET + 1)
              : (HOST_CMD_MINI_DATA_TYPE_OFFSET + 1);

    uint8_t length = buf[d - 2] - 2;
    if (length == 0)
    {
        return;
    }

    switch (buf[d])
    {
        case CMD_TYPE_USB_PD:
        {
            int32_t value = buf[d + 2] | (buf[d + 3] << 8) | (buf[d + 4] << 16) | (buf[d + 5] << 24);
            cmd_forward_pd(buf[d + 1], value);
            break;
        }
        case CMD_TYPE_SYS:
        {
            cmd_forward_sys(buf[d + 1]);
            break;
        }
        default:
            return;
    }
}