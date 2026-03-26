#include "uart.h"
#include "host.h"

#define UART_PERIPH      USART2
#define UART_PERIPH_CLK  RCC_APB1Periph_USART2
#define UART_BAUDRATE    115200

#define UART_TX_DMA_CH   DMA1_Channel7
#define UART_RX_DMA_CH   DMA1_Channel6

static inline uint32_t irq_save(void)
{
    uint32_t val;
    __asm__ volatile("csrr %0, 0x800" : "=r"(val));
    __asm__ volatile("csrc 0x800, %0" : : "r"(0x88));
    __asm__ volatile("fence.i");
    return val;
}

static inline void irq_restore(uint32_t val)
{
    __asm__ volatile("csrw 0x800, %0" : : "r"(val));
}

// ============================================================================
// TX: DMA
// ============================================================================

static uint8_t uart_initialized = 0;

static volatile uint8_t tx_state = UART_STATE_IDLE;
static uint8_t tx_buf[UART_TX_BUF_SIZE];

// ============================================================================
// RX: USART IDLE interrupt + DMA
// ============================================================================

static uint8_t rx_dma_buf[UART_RX_BUF_SIZE];

// ============================================================================
// USART2 IDLE interrupt handler — frame complete
// ============================================================================

void __attribute__((interrupt("WCH-Interrupt-fast"))) USART2_IRQHandler(void)
{
    if (USART_GetITStatus(UART_PERIPH, USART_IT_IDLE) != RESET)
    {
        // Clear IDLE flag: read SR then DR
        (void)UART_PERIPH->STATR;
        (void)UART_PERIPH->DATAR;

        DMA_Cmd(UART_RX_DMA_CH, DISABLE);

        uint16_t received = UART_RX_BUF_SIZE - DMA_GetCurrDataCounter(UART_RX_DMA_CH);
        if (received > 0)
        {
            host_rx_dispatch(rx_dma_buf, received);
        }

        // Reset DMA for next frame
        DMA_SetCurrDataCounter(UART_RX_DMA_CH, UART_RX_BUF_SIZE);
        DMA_Cmd(UART_RX_DMA_CH, ENABLE);
    }
}

// ============================================================================
// DMA TX complete interrupt
// ============================================================================

void __attribute__((interrupt("WCH-Interrupt-fast"))) DMA1_Channel7_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC7))
    {
        DMA_ClearITPendingBit(DMA1_IT_GL7);
        DMA_Cmd(UART_TX_DMA_CH, DISABLE);
        tx_state = UART_STATE_IDLE;
    }
}

// ============================================================================
// Init
// ============================================================================

void uart_init(void)
{
    // GPIO clock & USART clock
    RCC_APB2PeriphClockCmd(UART_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(UART_PERIPH_CLK, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // GPIO
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin   = UART_TX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = UART_RX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(UART_GPIO_PORT, &GPIO_InitStructure);

    // USART
    USART_InitTypeDef USART_InitStructure = {0};
    USART_InitStructure.USART_BaudRate             = UART_BAUDRATE;
    USART_InitStructure.USART_WordLength           = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits             = USART_StopBits_1;
    USART_InitStructure.USART_Parity               = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                 = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(UART_PERIPH, &USART_InitStructure);

    // DMA TX (Channel7) — configured per-transfer, just set fixed fields here
    DMA_DeInit(UART_TX_DMA_CH);
    DMA_InitTypeDef DMA_InitStructure = {0};
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&UART_PERIPH->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)tx_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize         = 0;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(UART_TX_DMA_CH, &DMA_InitStructure);
    DMA_ITConfig(UART_TX_DMA_CH, DMA_IT_TC, ENABLE);

    // DMA RX (Channel6) — circular idle-line framing
    DMA_DeInit(UART_RX_DMA_CH);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&UART_PERIPH->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)rx_dma_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = UART_RX_BUF_SIZE;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_Init(UART_RX_DMA_CH, &DMA_InitStructure);
    DMA_Cmd(UART_RX_DMA_CH, ENABLE);

    // Enable USART DMA requests
    USART_DMACmd(UART_PERIPH, USART_DMAReq_Tx, ENABLE);
    USART_DMACmd(UART_PERIPH, USART_DMAReq_Rx, ENABLE);

    // Enable USART IDLE interrupt for RX frame detection
    USART_ITConfig(UART_PERIPH, USART_IT_IDLE, ENABLE);

    // NVIC — DMA TX complete
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // NVIC — USART2 IDLE
    NVIC_InitStructure.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(UART_PERIPH, ENABLE);

    uart_initialized = 1;
}

// ============================================================================
// TX API — non-blocking DMA send
// ============================================================================

uint8_t uart_send_report(const uint8_t *data, uint16_t len)
{
    if (!uart_initialized)
        return 1;

    uint32_t saved = irq_save();
    if (tx_state != UART_STATE_IDLE)
    {
        irq_restore(saved);
        return 0;
    }
    tx_state = UART_STATE_BUSY;
    irq_restore(saved);

    if (data == NULL || len == 0 || len > UART_TX_BUF_SIZE)
    {
        tx_state = UART_STATE_IDLE;
        return 0;
    }

    memcpy(tx_buf, data, len);

    DMA_Cmd(UART_TX_DMA_CH, DISABLE);
    DMA_SetCurrDataCounter(UART_TX_DMA_CH, len);
    DMA_Cmd(UART_TX_DMA_CH, ENABLE);

    return 1;
}