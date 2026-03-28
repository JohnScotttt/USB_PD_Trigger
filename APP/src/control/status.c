#include "status.h"
#include "keypad.h"
#include "log/logger.h"
#include "memory/fram.h"

static usb_pd_msg_priority_type_t usb_pd_msg_priority = REPLY_PRIORITY;
static usb_pd_sink_mode_type_t usb_pd_sink_mode = EPR_MODE;
static usb_hid_report_type_t usb_hid_report_type = HID_REPORT_TYPE_STD;
static vbus_en_status_type_t vbus_en_status = VBUS_ALWAYS_OFF;
static trigger_hold_status_type_t trigger_hold_status = TRIGGER_HOLD_OFF;
static uint32_t fram_capacity = 0;

static void vbus_on(void)
{
    GPIO_SetBits(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN);
    if (vbus_en_status == VBUS_HOLD_OFF && fram_capacity > 0)
    {
        vbus_en_status = VBUS_HOLD_ON;
        uint8_t buf = (uint8_t)vbus_en_status;
        fram_write(VBUS_EN_STATUS_OFFSET, &buf, 1);
    }
    log_save_sys(VBUS_ON);
}

static void vbus_off(void)
{
    GPIO_ResetBits(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN);
    if (vbus_en_status == VBUS_HOLD_ON && fram_capacity > 0)
    {
        vbus_en_status = VBUS_HOLD_OFF;
        uint8_t buf = (uint8_t)vbus_en_status;
        fram_write(VBUS_EN_STATUS_OFFSET, &buf, 1);
    }
    log_save_sys(VBUS_OFF);
}

static void vbus_en_toggle(void)
{
    if (GPIO_ReadOutputDataBit(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN))
    {
        vbus_off();
    }
    else
    {
        vbus_on();
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

    uint8_t buf = 0;
    if (fram_capacity > 0)
    {
        fram_read(USB_PD_MSG_PRIORITY_OFFSET, &buf, 1);
        usb_pd_msg_priority = buf;
        fram_read(USB_PD_SINK_MODE_OFFSET, &buf, 1);
        usb_pd_sink_mode = buf;
        fram_read(USB_PD_HID_REPORT_TYPE_OFFSET, &buf, 1);
        usb_hid_report_type = buf;
        fram_read(VBUS_EN_STATUS_OFFSET, &buf, 1);
        vbus_en_status = buf;
        fram_read(TRIGGER_HOLD_STATUS_OFFSET, &buf, 1);
        trigger_hold_status = buf;
    }

    if (vbus_en_status == VBUS_HOLD_ON || vbus_en_status == VBUS_ALWAYS_ON)
    {
        vbus_on();
    }
    else if (vbus_en_status == VBUS_HOLD_OFF || vbus_en_status == VBUS_ALWAYS_OFF)
    {
        vbus_off();
    }
}

void set_usb_pd_msg_priority(usb_pd_msg_priority_type_t priority)
{
    usb_pd_msg_priority = priority;
    if (fram_capacity > 0)
    {
        uint8_t buf = (uint8_t)priority;
        fram_write(USB_PD_MSG_PRIORITY_OFFSET, &buf, 1);
    }
}

usb_pd_msg_priority_type_t get_usb_pd_msg_priority(void)
{
    return usb_pd_msg_priority;
}

void set_usb_pd_sink_mode(usb_pd_sink_mode_type_t mode)
{
    usb_pd_sink_mode = mode;
    if (fram_capacity > 0)
    {
        uint8_t buf = (uint8_t)mode;
        fram_write(USB_PD_SINK_MODE_OFFSET, &buf, 1);
    }
}

usb_pd_sink_mode_type_t get_usb_pd_sink_mode(void)
{
    return usb_pd_sink_mode;
}

void set_usb_hid_report_type(usb_hid_report_type_t type)
{
    usb_hid_report_type = type;
    if (fram_capacity > 0)
    {
        uint8_t buf = (uint8_t)type;
        fram_write(USB_PD_HID_REPORT_TYPE_OFFSET, &buf, 1);
    }
}

usb_hid_report_type_t get_usb_hid_report_type(void)
{
    return usb_hid_report_type;
}

void vbus_out_enable(bool enable)
{
    if (enable)
    {
        vbus_on();
    }
    else
    {
        vbus_off();
    }
}

void set_vbus_en_status(vbus_en_status_type_t status)
{
    vbus_en_status = status;
    if (status == VBUS_HOLD && fram_capacity > 0)
    {
        uint8_t buf = GPIO_ReadOutputDataBit(VBUS_EN_GPIO_PORT, VBUS_EN_GPIO_PIN) 
                    ? (uint8_t)VBUS_HOLD_ON : (uint8_t)VBUS_HOLD_OFF;
        vbus_en_status = buf;
        fram_write(VBUS_EN_STATUS_OFFSET, &buf, 1);
    }
    else if (fram_capacity > 0)
    {
        uint8_t buf = (uint8_t)status;
        fram_write(VBUS_EN_STATUS_OFFSET, &buf, 1);
    }
}

vbus_en_status_type_t get_vbus_en_status(void)
{
    return vbus_en_status;
}

void set_fram_capacity(uint32_t capacity)
{
    fram_capacity = capacity;
}

uint32_t get_fram_capacity(void)
{
    return fram_capacity;
}

void set_trigger_hold_status(trigger_hold_status_type_t status)
{
    trigger_hold_status = status;
    if (fram_capacity > 0)
    {
        uint8_t buf = (uint8_t)status;
        fram_write(TRIGGER_HOLD_STATUS_OFFSET, &buf, 1);
    }
}

trigger_hold_status_type_t get_trigger_hold_status(void)
{
    return trigger_hold_status;
}