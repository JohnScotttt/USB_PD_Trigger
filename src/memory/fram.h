#pragma once

#include "def.h"

#define FRAM_WREN   0x06    //写入使能，即设置WEL位       在写操作（WRSR和WRITE）之前，必须先发本命令
#define FRAM_WRDI   0x04    //写入禁止，即清除WEL位       本操作会禁止写操作（WRSR和WRITE），此后即使发送（WRSR和WRITE）也无效
#define FRAM_RDSR   0x05    //读状态寄存器
#define FRAM_WRSR   0x01    //写状态寄存器，前序操作为FRAM_WREN
#define FRAM_READ   0x03    //读
#define FRAM_WRITE  0x02    //写，前序操作为FRAM_WREN

#define FRAM_CS_LOW()   GPIO_ResetBits(SPI_GPIO_PORT, CS_GPIO_PIN)
#define FRAM_CS_HIGH()  GPIO_SetBits(SPI_GPIO_PORT, CS_GPIO_PIN)

#define HARDWARE_OFFSET 12
#define FIRMWARE_OFFSET 15
#define USB_PD_MSG_PRIORITY_OFFSET 32
#define USB_PD_SINK_MODE_OFFSET 33
#define USB_PD_HID_REPORT_TYPE_OFFSET 34

uint32_t fram_init(void);
void fram_write(uint16_t addr, uint8_t *buf, uint16_t len);
void fram_read(uint16_t addr, uint8_t *buf, uint16_t len);

void fram_whole_erase(void);
uint32_t fram_get_capacity(void);
void fram_cheak(void);