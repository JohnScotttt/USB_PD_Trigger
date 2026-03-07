#pragma once

#include "def.h"

typedef struct usb_pd_ccdet_callbacks_t
{
    void (*on_connection_changed)(usb_pd_cc_channel_type_t old_channel, usb_pd_cc_channel_type_t new_channel);
} usb_pd_ccdet_callbacks_t;

void usb_pd_ccdet_init(const usb_pd_ccdet_callbacks_t *callbacks);

void usb_pd_ccdet_select_cc(usb_pd_cc_channel_type_t cc_channel);
void usb_pd_ccdet_auto_detect(void);
