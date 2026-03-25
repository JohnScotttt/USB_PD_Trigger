#include <ch32x035.h>
#include "interface/hid.h"
#include "usb_pd/phy.h"
#include "usb_pd/ccdet.h"
#include "usb_pd/sink.h"
#include "delay.h"
#include "vbus/sensor.h"
#include "control/keypad.h"
#include "control/status.h"
#include "control/cmd.h"
#include "memory/fram.h"
#include "log/logger.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();

    delay_init();

    uint32_t fram_capacity = fram_init();
    set_fram_capacity(fram_capacity);

    keypad_init();

    usb_init();
    
    usb_pd_sink_init();

    in_adc_init();

    status_init();

    while (1)
    {
        keypad_scan();
        cmd_process_next();
        usb_pd_sink_process();
    }
}
