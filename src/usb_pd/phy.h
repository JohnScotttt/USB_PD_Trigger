#pragma once

#include "def.h"

typedef struct usb_pd_phy_callbacks_t
{
    void (*on_rx_end)(uint8_t status, const uint8_t *data, uint16_t len);  // 包含复位中断
    void (*on_tx_end)(uint8_t sop, const uint8_t *data, uint16_t len);
} usb_pd_phy_callbacks_t;

void usb_pd_phy_init(const usb_pd_phy_callbacks_t *callbacks);

void usb_pd_phy_cc_rd_enable(bool status);
void usb_pd_phy_cc_ra_enable(usb_pd_cc_channel_type_t cc_channel, bool status);

uint16_t usb_pd_phy_get_cc_voltage(usb_pd_cc_channel_type_t cc_channel, bool is_connected);

void usb_pd_phy_set_active_cc(usb_pd_cc_channel_type_t cc_channel);
void usb_pd_phy_send_data(const uint8_t *data, uint8_t data_len, uint8_t sop);
void usb_pd_phy_send_hard_reset(void);