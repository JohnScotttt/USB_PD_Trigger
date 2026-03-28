#include <ch32x035.h>
#include "usb/hid.h"
#include "iap.h"
#include "config.h"
#include "delay.h"

// Parse and dispatch IAP commands from HID
static void iap_process_cmd(const uint8_t *buf)
{
    // Mini format: [0x52][0xFF][FullLength][DataType=0x01][Category=0x01][CMD][Params...]
    //               [0]   [1]     [2]          [3]           [4]         [5]   [6+]
    uint8_t cmd = buf[5];
    uint8_t resp[HIDRAW_IN_EP_SIZE] = {0};

    // Build response header
    resp[0] = HID_RESP_HEADER_0;
    resp[1] = HID_RESP_HEADER_1;
    resp[2] = cmd; // Echo command

    switch (cmd)
    {
    case IAP_CMD_START:
    {
        // Params: [fw_size(4B LE)][crc32(4B LE)]
        uint32_t fw_size = buf[6] | (buf[7] << 8) | (buf[8] << 16) | (buf[9] << 24);
        uint32_t fw_crc  = buf[10] | (buf[11] << 8) | (buf[12] << 16) | (buf[13] << 24);
        uint8_t status = iap_cmd_start(fw_size, fw_crc);
        resp[3] = status;
        break;
    }
    case IAP_CMD_DATA:
    {
        // Params: [offset(4B LE)][len(1B)][data...]
        uint32_t offset = buf[6] | (buf[7] << 8) | (buf[8] << 16) | (buf[9] << 24);
        uint8_t len = buf[10];
        if (len > 57) len = 57; // Max data per packet
        uint8_t status = iap_cmd_data(offset, &buf[11], len);
        resp[3] = status;
        // Include bytes_received in response
        uint32_t received = iap_get_bytes_received();
        resp[4] = (received >> 0) & 0xFF;
        resp[5] = (received >> 8) & 0xFF;
        resp[6] = (received >> 16) & 0xFF;
        resp[7] = (received >> 24) & 0xFF;
        break;
    }
    case IAP_CMD_FINISH:
    {
        uint8_t status = iap_cmd_finish();
        resp[3] = status;
        break;
    }
    case IAP_CMD_STATUS:
    {
        resp[3] = IAP_STATUS_OK;
        resp[4] = (uint8_t)iap_get_state();
        uint32_t received = iap_get_bytes_received();
        resp[5] = (received >> 0) & 0xFF;
        resp[6] = (received >> 8) & 0xFF;
        resp[7] = (received >> 16) & 0xFF;
        resp[8] = (received >> 24) & 0xFF;
        break;
    }
    default:
        resp[3] = IAP_STATUS_ERROR;
        break;
    }

    hid_send_report(resp, HIDRAW_IN_EP_SIZE);
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    delay_init();

    // Check if APP is valid and no update requested
    if (iap_is_app_valid() && !iap_is_update_requested())
    {
        // TODO: Check button press here (reserved interface)
        // if (button_pressed()) { goto stay_in_bootloader; }

        // Jump to APP
        delay_ms(50);
        iap_jump_to_app();
        while (1);
    }

    // Stay in Bootloader - initialize USB and IAP
    iap_init();
    usb_init();

    while (1)
    {
        if (hid_has_iap_cmd())
        {
            const uint8_t *cmd = hid_get_iap_cmd();
            uint8_t buf[HIDRAW_OUT_EP_SIZE];
            memcpy(buf, cmd, HIDRAW_OUT_EP_SIZE);
            hid_pop_iap_cmd();

            iap_process_cmd(buf);
        }

        // After IAP done, auto-reset to jump to APP
        if (iap_get_state() == IAP_STATE_DONE)
        {
            delay_ms(100);
            NVIC_SystemReset();
        }
    }
}
