#include "ccdet.h"
#include "phy.h"
#include "vbus/sensor.h"
#include "delay.h"

/* CC 连接状态 */
static usb_pd_cc_channel_type_t active_cc_channel = PD_CC_NONE;

/* 连接检测层回调函数 */
static usb_pd_ccdet_callbacks_t ccdet_callbacks = {0};

void usb_pd_ccdet_init(const usb_pd_ccdet_callbacks_t *callbacks)
{
    // 保存回调函数
    if (callbacks)
    {
        ccdet_callbacks = *callbacks;
    }
}

void usb_pd_ccdet_select_cc(usb_pd_cc_channel_type_t cc_channel)
{
    if (cc_channel == active_cc_channel)
    {
        return;
    }

    usb_pd_cc_channel_type_t old_channel = active_cc_channel;
    active_cc_channel = cc_channel;

    usb_pd_phy_set_active_cc(cc_channel);

    // 通知连接状态变化
    if (ccdet_callbacks.on_connection_changed)
    {
        ccdet_callbacks.on_connection_changed(old_channel, active_cc_channel);
    }
}

void usb_pd_ccdet_auto_detect(void)
{
    static uint16_t connection_count = 0;
    bool cond;

    // 读取 VBUS 电压
    uint16_t vbus_volt = adc_get_vbus_in_mv();

    // CC 电压，需要时再读取
    uint16_t cc1_volt, cc2_volt;

    switch (active_cc_channel)
    {
    case PD_CC1:
    case PD_CC2:
        // 已连接状态下，只根据 VBUS 电压判断
        cond = vbus_volt > 2500;
        // 检测到未连接计数
        connection_count = cond ? 0 : connection_count + 1;
        // 如果连续检测到未连接，则认为连接断开
        if (!cond && connection_count >= 2)
        {
            usb_pd_ccdet_select_cc(PD_CC_NONE);
        }
        break;

    case PD_CC_NONE:
        // 未连接状态下，当 VBUS 有电压时按照 CC 引脚电压判断连接，VBUS 无电压时判定为未连接
        if (vbus_volt > 3000)
        {
            // 读取 CC 电压
            cc1_volt = usb_pd_phy_get_cc_voltage(PD_CC1, false);
            cc2_volt = usb_pd_phy_get_cc_voltage(PD_CC2, false);
            // 根据电压判断 CC1 CC2
            cond = ((cc1_volt >= 450 && cc1_volt <= 1230) && (cc2_volt >= 2200 || cc2_volt <= 220)) ||
                   ((cc2_volt >= 450 && cc2_volt <= 1230) && (cc1_volt >= 2200 || cc1_volt <= 220));
        }
        else
        {
            cond = false;
        }
        // 检测到已连接计数
        connection_count = cond ? connection_count + 1 : 0;
        // 如果连续检测到已连接，则认为连接成功
        if (cond && connection_count >= 3)
        {
            usb_pd_cc_channel_type_t cc_channel = PD_CC_NONE;
            // 读取 CC 电压
            cc1_volt = usb_pd_phy_get_cc_voltage(PD_CC1, false);
            cc2_volt = usb_pd_phy_get_cc_voltage(PD_CC2, false);
            // 根据电压判断 CC1 CC2
            if ((cc1_volt >= 450 && cc1_volt <= 1230) && cc2_volt == 2200)
            {
                cc_channel = PD_CC1;
            }
            else if ((cc2_volt >= 450 && cc2_volt <= 1230) && cc1_volt == 2200)
            {
                cc_channel = PD_CC2;
            }
            else
            {
                cc_channel = cc1_volt > cc2_volt ? PD_CC1 : PD_CC2;
            }
            usb_pd_ccdet_select_cc(cc_channel);
        }
        break;
    }
}
