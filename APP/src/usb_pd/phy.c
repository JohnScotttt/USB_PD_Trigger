#include "phy.h"
#include "delay.h"
#include "usb/hid.h"

static uint8_t usb_pd_rx_buffer[USB_PD_DATA_MAX_LEN] __attribute__((aligned(4)));
static uint8_t usb_pd_tx_buffer[USB_PD_DATA_MAX_LEN] __attribute__((aligned(4)));
static uint8_t usb_pd_tx_sop = 0;
static uint8_t usb_pd_tx_len = 0;

static usb_pd_phy_callbacks_t phy_callbacks = {0};

static void usb_pd_phy_set_rx(void);
static void usb_pd_phy_set_tx(uint8_t tx_len, uint8_t sop);

void usb_pd_phy_init(const usb_pd_phy_callbacks_t *callbacks)
{
    if (callbacks)
    {
        phy_callbacks = *callbacks;
    }

    GPIO_InitTypeDef GPIO_InitStructure_CC = {0};
    RCC_APB2PeriphClockCmd(CC_GPIO_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBPD, ENABLE);
    GPIO_InitStructure_CC.GPIO_Pin = PIN_CC1 | PIN_CC2;
    GPIO_InitStructure_CC.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure_CC.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CC_GPIO_PORT, &GPIO_InitStructure_CC);

    GPIO_InitTypeDef GPIO_InitStructure_CC_EN = {0};
    RCC_APB2PeriphClockCmd(CC_EN_GPIO_CLK, ENABLE);
    GPIO_InitStructure_CC_EN.GPIO_Pin = CC_EN_GPIO_PIN;
    GPIO_InitStructure_CC_EN.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure_CC_EN.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CC_EN_GPIO_PORT, &GPIO_InitStructure_CC_EN);

    GPIO_InitTypeDef GPIO_InitStructure_CC_RA_EN = {0};
    RCC_APB2PeriphClockCmd(CC_RA_EN_GPIO_CLK, ENABLE);
    GPIO_InitStructure_CC_RA_EN.GPIO_Pin = CC1_RA_EN_GPIO_PIN | CC2_RA_EN_GPIO_PIN;
    GPIO_InitStructure_CC_RA_EN.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure_CC_RA_EN.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CC_RA_EN_GPIO_PORT, &GPIO_InitStructure_CC_RA_EN);

    usb_pd_phy_cc_rd_enable(true);

    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->PORT_CC1 |= CC_CMP_45;
    USBPD->PORT_CC2 |= CC_CMP_45;
    USBPD->PORT_CC1 &= ~CC_LVE;
    USBPD->PORT_CC2 &= ~CC_LVE;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;

    usb_pd_phy_set_rx();

    NVIC_SetPriority(USBPD_IRQn, 0);
    NVIC_EnableIRQ(USBPD_IRQn);
}

void usb_pd_phy_cc_rd_enable(bool status)
{
    if (status)
    {
        GPIO_SetBits(CC_EN_GPIO_PORT, CC_EN_GPIO_PIN);
    }
    else
    {
        GPIO_ResetBits(CC_EN_GPIO_PORT, CC_EN_GPIO_PIN);
    }
}

void usb_pd_phy_cc_ra_enable(usb_pd_cc_channel_type_t cc_channel, bool status)
{
    if (status)
    {
        if (cc_channel == PD_CC1)
        {
            GPIO_SetBits(CC_RA_EN_GPIO_PORT, CC1_RA_EN_GPIO_PIN);
        }
        else if (cc_channel == PD_CC2)
        {
            GPIO_SetBits(CC_RA_EN_GPIO_PORT, CC2_RA_EN_GPIO_PIN);
        }
    }
    else
    {
        if (cc_channel == PD_CC1)
        {
            GPIO_ResetBits(CC_RA_EN_GPIO_PORT, CC1_RA_EN_GPIO_PIN);
        }
        else if (cc_channel == PD_CC2)
        {
            GPIO_ResetBits(CC_RA_EN_GPIO_PORT, CC2_RA_EN_GPIO_PIN);
        }
    }
}

uint16_t usb_pd_phy_get_cc_voltage(usb_pd_cc_channel_type_t cc_channel, bool is_connected)
{
    const uint16_t cc_cmp_list[] = {CC_CMP_22, CC_CMP_45, CC_CMP_55, CC_CMP_66, CC_CMP_95, CC_CMP_123};
    const uint16_t cc_vol_list[] = {220, 450, 550, 660, 950, 1230};

    uint32_t CC_GPIO_PIN;
    volatile uint16_t *PORT_CC;

    switch (cc_channel)
    {
    case PD_CC1:
        CC_GPIO_PIN = CC1_GPIO_PIN;
        PORT_CC = &USBPD->PORT_CC1;
        break;
    case PD_CC2:
        CC_GPIO_PIN = CC2_GPIO_PIN;
        PORT_CC = &USBPD->PORT_CC2;
        break;
    default:
        return 0;
    }

    if (GPIO_ReadInputDataBit(CC_GPIO_PORT, CC_GPIO_PIN))
    {
        return 2200;
    }

    if (is_connected)
    {
        return (*PORT_CC & PA_CC_AI) ? 450 : 0;
    }
    else
    {
        uint16_t cc_vol = 0;
        for (uint8_t i = 0; i < sizeof(cc_cmp_list) / sizeof(cc_cmp_list[0]); i++)
        {
            *PORT_CC &= ~(CC_CMP_Mask | PA_CC_AI);
            *PORT_CC |= cc_cmp_list[i];
            delay_us(2);
            if (*PORT_CC & PA_CC_AI)
            {
                cc_vol = cc_vol_list[i];
            }
            else
            {
                break;
            }
        }
        *PORT_CC &= ~(CC_CMP_Mask | PA_CC_AI);
        *PORT_CC |= CC_CMP_45;
        return cc_vol;
    }

    return 0;
}

void usb_pd_phy_set_active_cc(usb_pd_cc_channel_type_t cc_channel)
{
    switch (cc_channel)
    {
    case PD_CC1:
        SET_CC1_ACTIVE();
        break;
    case PD_CC2:
        SET_CC2_ACTIVE();
        break;
    case PD_CC_NONE:
    default:
        break;
    }
}

static void usb_pd_phy_set_rx(void)
{
    // 设置 CC 正常 VDD 电压驱动输出
    USBPD->PORT_CC1 &= ~CC_LVE;
    USBPD->PORT_CC2 &= ~CC_LVE;

    // 清除所有中断标志位
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;

    // 中断使能
    USBPD->CONFIG |= IE_TX_END | IE_RX_ACT | IE_RX_RESET;
    // DMA 使能
    USBPD->CONFIG |= PD_DMA_EN;

    // 设置为接收模式
    USBPD->DMA = (uint32_t)usb_pd_rx_buffer; // DMA Buffer
    USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;     // BMC 接收采样时钟计数器

    // 开始接收
    USBPD->CONTROL &= ~PD_TX_EN; // PD 接收使能
    USBPD->CONTROL |= BMC_START; // BMC 开始信号
}

static void usb_pd_phy_set_tx(uint8_t tx_len, uint8_t sop)
{
    usb_pd_tx_sop = sop;
    usb_pd_tx_len = tx_len;

    // 设置 CC 低电压驱动输出
    if (IS_CC1_ACTIVE())
    {
        USBPD->PORT_CC1 |= CC_LVE;
    }
    else
    {
        USBPD->PORT_CC2 |= CC_LVE;
    }

    // 清除所有中断标志位
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;

    // 中断使能
    USBPD->CONFIG |= IE_TX_END | IE_RX_ACT | IE_RX_RESET;
    // DMA 使能
    USBPD->CONFIG |= PD_DMA_EN;

    // 设置为发送模式
    USBPD->TX_SEL = sop;
    USBPD->DMA = (uint32_t)usb_pd_tx_buffer; // DMA Buffer
    USBPD->BMC_CLK_CNT = UPD_TMR_TX_48M;     // BMC 发送采样时钟计数器
    USBPD->BMC_TX_SZ = tx_len;

    // 发送前保存事件
    // if (phy_callbacks.on_rx_end)
    // {
    //     phy_callbacks.on_rx_end(USBPD->STATUS, usb_pd_tx_buffer, tx_len);
    // }

    // 开始发送
    USBPD->STATUS |= IF_TX_END;  //
    USBPD->CONTROL |= PD_TX_EN;  // PD 发送使能
    USBPD->CONTROL |= BMC_START; // BMC 开始信号

    // wait tx end
    while (((USBPD->STATUS & IF_TX_END) == 0) && (USBPD->CONTROL & PD_TX_EN))
        ;
}

void usb_pd_phy_send_data(const uint8_t *data, uint8_t data_len, uint8_t sop)
{
    if (data_len > USB_PD_DATA_MAX_LEN - 1)
    {
        data_len = USB_PD_DATA_MAX_LEN - 1;
    }
    memcpy(usb_pd_tx_buffer, data, data_len);

    usb_pd_phy_set_tx(data_len, sop);
}

void usb_pd_phy_send_hard_reset(void)
{
    usb_pd_phy_set_tx(0, UPD_HARD_RESET);
}

void USBPD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast"))) __attribute__((section(".highcode")));
void USBPD_IRQHandler(void)
{
    uint8_t status = 0;
    // 接收完成中断标志
    if (USBPD->STATUS & IF_RX_ACT)
    {
        status = USBPD->STATUS;
        USBPD->STATUS |= IF_RX_ACT;

        uint8_t sop = status & BMC_AUX_Mask;
        if ((sop == BMC_AUX_SOP0 || sop == BMC_AUX_SOP1_HRST || sop == BMC_AUX_SOP2_CRST || sop == BMC_AUX_INVALID) && USBPD->BMC_BYTE_CNT >= 2)
        {
            if (phy_callbacks.on_rx_end)
            {
                phy_callbacks.on_rx_end(status, usb_pd_rx_buffer, USBPD->BMC_BYTE_CNT);
            }
        }
    }

    // 发送完成中断标志
    if (USBPD->STATUS & IF_TX_END)
    {
        status = USBPD->STATUS;
        USBPD->STATUS |= IF_TX_END;

        // 恢复为接收模式
        usb_pd_phy_set_rx();

        if (phy_callbacks.on_tx_end)
        {
            phy_callbacks.on_tx_end(usb_pd_tx_sop, usb_pd_tx_buffer, usb_pd_tx_len);
        }
    }

    // 接收复位中断标志
    if (USBPD->STATUS & IF_RX_RESET)
    {
        status = USBPD->STATUS;
        USBPD->STATUS |= IF_RX_RESET;

        if (phy_callbacks.on_rx_end)
        {
            phy_callbacks.on_rx_end(status, 0, 0);
        }
    }
}
