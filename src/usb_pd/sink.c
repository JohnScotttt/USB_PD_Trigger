#include "sink.h"
#include "log/logger.h"
#include "event.h"
#include "ccdet.h"
#include "phy.h"
#include "interface/hid.h"

#include "delay.h"
#include "config.h"

/* 连接检测层回调 */
static void usb_pd_sink_connection_changed_callback(usb_pd_cc_channel_type_t old_channel, usb_pd_cc_channel_type_t new_channel);

static void usb_pd_save_rx(uint8_t status, const uint8_t *data, uint16_t len)
{
    usb_pd_event_save_rx(status, data, len);
    log_save_usb_pd_rx(status, data, len);
}

static void usb_pd_save_tx(uint8_t sop, const uint8_t *data, uint16_t len)
{
    log_save_usb_pd_tx(sop, data, len);
}

void usb_pd_sink_init(void)
{
    // 初始化 PHY 层
    usb_pd_phy_callbacks_t usb_pd_phy_callbacks = {
        .on_rx_end = usb_pd_save_rx,
        .on_tx_end = usb_pd_save_tx,
    };
    usb_pd_phy_init(&usb_pd_phy_callbacks);

    // 初始化连接检测层
    usb_pd_ccdet_callbacks_t usb_pd_ccdet_callbacks = {
        .on_connection_changed = usb_pd_sink_connection_changed_callback,
    };
    usb_pd_ccdet_init(&usb_pd_ccdet_callbacks);
}

static void usb_pd_sink_connection_changed_callback(usb_pd_cc_channel_type_t old_channel, usb_pd_cc_channel_type_t new_channel)
{
    usb_pd_event_save_connect_change();
    log_save_usb_pd_connect_change(old_channel, new_channel);
    hid_rx_buf_clear_pd();
}

void usb_pd_sink_process(void)
{
    usb_pd_ccdet_auto_detect();
    usb_pd_event_process_next();
    log_process_next();
}