#pragma once

#include <ch32x035.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define CC_GPIO_PORT        GPIOC
#define CC_GPIO_CLK         RCC_APB2Periph_GPIOC
#define CC1_GPIO_PIN        GPIO_Pin_14
#define CC2_GPIO_PIN        GPIO_Pin_15

#define CC_EN_GPIO_PORT     GPIOB
#define CC_EN_GPIO_CLK      RCC_APB2Periph_GPIOB
#define CC_EN_GPIO_PIN      GPIO_Pin_12

#define CC_RA_EN_GPIO_PORT  GPIOB
#define CC_RA_EN_GPIO_CLK   RCC_APB2Periph_GPIOB
#define CC1_RA_EN_GPIO_PIN  GPIO_Pin_1
#define CC2_RA_EN_GPIO_PIN  GPIO_Pin_0

#define SW_GPIO_PORT        GPIOB
#define SW_GPIO_CLK         RCC_APB2Periph_GPIOB
#define SW_L_GPIO_PIN       GPIO_Pin_3
#define SW_R_GPIO_PIN       GPIO_Pin_11

#define ADC_GPIO_PORT       GPIOA
#define ADC_GPIO_CLK        RCC_APB2Periph_GPIOA
#define ADC_GPIO_PIN        GPIO_Pin_0
#define ADC_CHANNEL         ADC_Channel_0

#define VBUS_EN_GPIO_PORT   GPIOA
#define VBUS_EN_GPIO_CLK    RCC_APB2Periph_GPIOA
#define VBUS_EN_GPIO_PIN    GPIO_Pin_1

#define PC_USB_GPIO_PORT     GPIOC
#define PC_USB_GPIO_CLK      RCC_APB2Periph_GPIOC
#define PC_USB_DP_GPIO_PIN   GPIO_Pin_17
#define PC_USB_DM_GPIO_PIN   GPIO_Pin_16

#define UART_GPIO_PORT       GPIOA
#define UART_GPIO_CLK        RCC_APB2Periph_GPIOA
#define UART_TX_GPIO_PIN     GPIO_Pin_2
#define UART_RX_GPIO_PIN     GPIO_Pin_3

#define SPI_GPIO_PORT        GPIOA
#define SPI_GPIO_CLK         RCC_APB2Periph_GPIOA
#define CS_GPIO_PIN          GPIO_Pin_4
#define SCK_GPIO_PIN         GPIO_Pin_5
#define MISO_GPIO_PIN        GPIO_Pin_6
#define MOSI_GPIO_PIN        GPIO_Pin_7

#define SET_CC1_ACTIVE() (USBPD->CONFIG &= ~CC_SEL)
#define SET_CC2_ACTIVE() (USBPD->CONFIG |= CC_SEL)
#define IS_CC1_ACTIVE()  ((USBPD->CONFIG & CC_SEL) == 0)

#define USB_PD_DATA_MAX_LEN 35 // 1 + 2 + 4 * 7 + 4=35

typedef enum usb_pd_cc_channel_type_t
{
    PD_CC_NONE = 0,
    PD_CC1 = 1,
    PD_CC2 = 2,
} usb_pd_cc_channel_type_t;

typedef enum data_type_t
{
    USB_PD,
    SYSTEM,
    MESSAGE,
} data_type_t;

typedef enum sop_type_t
{
    SOP,
    SOP1,
    SOP2,
    SOP1_DEBUG,
    SOP2_DEBUG,
    HARD_RESET,
    CABLE_RESET,
} sop_type_t;

typedef enum system_type_t
{
    ATTACH_CC1 = 0x11,
    DETACH_CC1 = 0x12,
    ATTACH_CC2 = 0x21,
    DETACH_CC2 = 0x22,
    VBUS_ON = 0x31,
    VBUS_OFF = 0x32,
    CRC_ERROR = 0xEE,
} system_type_t;