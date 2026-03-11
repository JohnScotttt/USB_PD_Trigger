#include "fram.h"
#include "uid.h"
#include "log/logger.h"

uint32_t fram_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure = {0};

    RCC_APB2PeriphClockCmd(SPI_GPIO_CLK | RCC_APB2Periph_SPI1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = CS_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_GPIO_PORT, &GPIO_InitStructure);
    
    GPIO_SetBits(SPI_GPIO_PORT, CS_GPIO_PIN);

    GPIO_InitStructure.GPIO_Pin = SCK_GPIO_PIN | MOSI_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = MISO_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SPI_GPIO_PORT, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; // 双线全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                      // 主机模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                  // 8位数据帧
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;                         // SPI 模式 0: 空闲时钟为低电平 [cite: 221]
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;                       // SPI 模式 0: 第一个边沿采样 [cite: 221]
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                          // 软件控制 CS 引脚
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; 
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                 // MSB 先行 
    SPI_InitStructure.SPI_CRCPolynomial = 7;                           // 默认 CRC 值（即使不用也要填）
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE);

    return fram_get_capacity();
}

static uint8_t spi_rw_byte(uint8_t data) 
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

void fram_write(uint16_t addr, uint8_t *buf, uint16_t len) 
{
    FRAM_CS_LOW();
    spi_rw_byte(FRAM_WREN);
    FRAM_CS_HIGH();

    FRAM_CS_LOW();
    spi_rw_byte(FRAM_WRITE);
    
    spi_rw_byte((uint8_t)((addr & 0x1FFF) >> 8));
    spi_rw_byte((uint8_t)(addr & 0xFF));

    for (uint16_t i = 0; i < len; i++) 
    {
        spi_rw_byte(buf[i]);
    }

    FRAM_CS_HIGH();
}

void fram_read(uint16_t addr, uint8_t *buf, uint16_t len) 
{
    FRAM_CS_LOW();

    spi_rw_byte(FRAM_READ);
    spi_rw_byte((uint8_t)((addr & 0x1FFF) >> 8));
    spi_rw_byte((uint8_t)(addr & 0xFF));

    for (uint16_t i = 0; i < len; i++) 
    {
        buf[i] = spi_rw_byte(0xFF);
    }

    FRAM_CS_HIGH();
}

void fram_whole_erase(void) 
{
    // 8K x 8bit = 8192 byte
    uint8_t zero_buf[256] = {0};
    for (uint16_t addr = 0; addr < 8192; addr += 256) 
    {
        fram_write(addr, zero_buf, 256);
    }
}

uint32_t fram_get_capacity(void) 
{
    uint8_t  backup_0;
    uint8_t  test_val;
    uint32_t detected_size = 0;
    
    fram_read(0x0000, &backup_0, 1);
    test_val = 0xAA;
    fram_write(0x0000, &test_val, 1);
    
    uint8_t verify_val;
    fram_read(0x0000, &verify_val, 1);
    if (verify_val != 0xAA) 
    {
        return 0; 
    }

    uint32_t test_points[] = {2048, 4096, 8192, 16384, 32768, 65536};
    
    for (int i = 0; i < 6; i++) 
    {
        uint32_t offset = test_points[i];
        uint8_t  val_at_offset = 0x55;
        
        fram_write((uint16_t)offset, &val_at_offset, 1);
        
        uint8_t check_zero;
        fram_read(0x0000, &check_zero, 1);
        
        if (check_zero == 0x55) 
        {
            detected_size = offset;
            break;
        }
    }
    
    if (detected_size == 0) 
    {
        detected_size = 65536; 
    }

    fram_write(0x0000, &backup_0, 1);
    return detected_size;
}

void fram_cheak(void)
{
    uint8_t buf[12] = {0};
    uint32_t uid_1 = 0;
    uint32_t uid_2 = 0;
    uint32_t uid_3 = 0;
    fram_read(0, buf, 12);
    memcpy(&uid_1, buf, 4);
    memcpy(&uid_2, buf + 4, 4);
    memcpy(&uid_3, buf + 8, 4);
    if (uid_1 != CHIP_UID_1 || uid_2 != CHIP_UID_2 || uid_3 != CHIP_UID_3)
    {
        fram_whole_erase();
        uid_1 = CHIP_UID_1;
        uid_2 = CHIP_UID_2;
        uid_3 = CHIP_UID_3;
        memcpy(buf, (uint8_t *)&uid_1, 4);
        memcpy(buf + 4, (uint8_t *)&uid_2, 4);
        memcpy(buf + 8, (uint8_t *)&uid_3, 4);
        fram_write(0, buf, 12);
    }
}