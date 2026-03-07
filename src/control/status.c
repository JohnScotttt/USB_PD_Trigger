#include "status.h"
#include "keypad.h"
#include "log/logger.h"

static void vbus_en_toggle(void)
{
    if (GPIO_ReadOutputDataBit(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN))
    {
        log_save_sys(VBUS_OFF);
        GPIO_ResetBits(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN);
    }
    else
    {
        log_save_sys(VBUS_ON);
        GPIO_SetBits(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN);
    }
}

void status_init(void)
{
    RCC_APB2PeriphClockCmd(VBUS_EN_GPIO_CLK, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin   = VBUS_EN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(VBUS_EN_GPIO_PORT, &GPIO_InitStructure);
    keypad_set_both_press_callback(vbus_en_toggle);
}

// ============================================================================
// 状态管理
// ============================================================================

static uint8_t usb_pd_msg_priority = REPLY_PRIORITY;
static usb_pd_sink_mode_type_t usb_pd_sink_mode = EPR_MODE;
static usb_hid_report_type_t usb_hid_report_type = HID_REPORT_TYPE_STD;

void set_usb_pd_msg_priority(usb_pd_msg_priority_type_t priority)
{
    usb_pd_msg_priority = priority;
}

usb_pd_msg_priority_type_t get_usb_pd_msg_priority(void)
{
    return usb_pd_msg_priority;
}

void set_usb_pd_sink_mode(usb_pd_sink_mode_type_t mode)
{
    usb_pd_sink_mode = mode;
}

usb_pd_sink_mode_type_t get_usb_pd_sink_mode(void)
{
    return usb_pd_sink_mode;
}

void set_usb_hid_report_type(usb_hid_report_type_t type)
{
    usb_hid_report_type = type;
}

usb_hid_report_type_t get_usb_hid_report_type(void)
{
    return usb_hid_report_type;
}

void vbus_out_enable(bool enable)
{
    if (enable)
    {
        GPIO_SetBits(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN);
        log_save_sys(VBUS_ON);
    }
    else
    {
        GPIO_ResetBits(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN);
        log_save_sys(VBUS_OFF);
    }
}