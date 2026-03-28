#include "iap.h"
#include <string.h>

static iap_context_t ctx;
static uint8_t page_buf[FLASH_PAGE_SIZE + 128]; // Buffer for accumulating page data
static uint32_t page_buf_offset; // Current offset within page_buf relative to page boundary

// ============================================================================
// Flash programming helpers (reference: EVT/EXAM/IAP)
// ============================================================================

static void flash_program_page(uint32_t addr, uint32_t *buf)
{
    FLASH_BufReset();
    for (uint8_t i = 0; i < 64; i++)
    {
        FLASH_BufLoad(addr + 4 * i, buf[i]);
    }
    FLASH_ProgramPage_Fast(addr);
}

// ============================================================================
// CRC32 (standard polynomial 0xEDB88320)
// ============================================================================

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

uint32_t iap_calc_crc32(uint32_t addr, uint32_t len)
{
    uint32_t crc = 0;
    const uint8_t *p = (const uint8_t *)addr;
    crc = crc32_update(crc, p, len);
    return crc;
}

// ============================================================================
// IAP interface
// ============================================================================

void iap_init(void)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.state = IAP_STATE_IDLE;
    page_buf_offset = 0;
}

uint8_t iap_cmd_start(uint32_t fw_size, uint32_t fw_crc32)
{
    if (fw_size == 0 || fw_size > FLASH_APP_SIZE)
        return IAP_STATUS_ERROR;

    ctx.fw_size = fw_size;
    ctx.fw_crc32 = fw_crc32;
    ctx.bytes_received = 0;
    ctx.state = IAP_STATE_RECEIVING;
    page_buf_offset = 0;

    // Unlock flash for fast programming
    FLASH_Unlock_Fast();

    // Erase the APP area pages that will be used
    uint32_t pages = (fw_size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++)
    {
        FLASH_ErasePage_Fast(FLASH_APP_BASE + i * FLASH_PAGE_SIZE);
    }

    return IAP_STATUS_OK;
}

uint8_t iap_cmd_data(uint32_t offset, const uint8_t *data, uint8_t len)
{
    if (ctx.state != IAP_STATE_RECEIVING)
        return IAP_STATUS_ERROR;

    if (offset + len > ctx.fw_size)
        return IAP_STATUS_ERROR;

    // Calculate absolute flash address
    uint32_t flash_addr = FLASH_APP_BASE + offset;

    // Calculate page-aligned address and offset within page
    uint32_t page_addr = flash_addr & ~(FLASH_PAGE_SIZE - 1);
    uint32_t offset_in_page = flash_addr - page_addr;

    // Process data that may span page boundaries
    uint8_t remaining = len;
    const uint8_t *src = data;
    uint32_t cur_offset_in_page = offset_in_page;
    uint32_t cur_page_addr = page_addr;

    while (remaining > 0)
    {
        uint8_t chunk = remaining;
        if (cur_offset_in_page + chunk > FLASH_PAGE_SIZE)
            chunk = FLASH_PAGE_SIZE - cur_offset_in_page;

        // Read the current page from flash into buffer
        memcpy(page_buf, (const void *)cur_page_addr, FLASH_PAGE_SIZE);

        // Overlay new data
        memcpy(page_buf + cur_offset_in_page, src, chunk);

        // Erase and reprogram the page
        FLASH_ErasePage_Fast(cur_page_addr);
        flash_program_page(cur_page_addr, (uint32_t *)page_buf);

        src += chunk;
        remaining -= chunk;
        cur_page_addr += FLASH_PAGE_SIZE;
        cur_offset_in_page = 0;
    }

    ctx.bytes_received += len;
    return IAP_STATUS_OK;
}

uint8_t iap_cmd_finish(void)
{
    if (ctx.state != IAP_STATE_RECEIVING)
        return IAP_STATUS_ERROR;

    ctx.state = IAP_STATE_VERIFYING;

    // Calculate CRC32 over the written APP area
    uint32_t calc_crc = iap_calc_crc32(FLASH_APP_BASE, ctx.fw_size);

    if (calc_crc != ctx.fw_crc32)
    {
        ctx.state = IAP_STATE_ERROR;
        // Lock flash
        FLASH->CTLR |= ((uint32_t)0x00008000); // FLASH_Lock_Fast
        FLASH->CTLR |= ((uint32_t)0x00000080); // FLASH_Lock
        return IAP_STATUS_CRC_FAIL;
    }

    // CRC OK - clear CalAddr flag
    iap_clear_update_flag();

    // Lock flash
    FLASH->CTLR |= ((uint32_t)0x00008000); // FLASH_Lock_Fast
    FLASH->CTLR |= ((uint32_t)0x00000080); // FLASH_Lock

    ctx.state = IAP_STATE_DONE;
    return IAP_STATUS_OK;
}

iap_state_t iap_get_state(void)
{
    return ctx.state;
}

uint32_t iap_get_bytes_received(void)
{
    return ctx.bytes_received;
}

bool iap_is_app_valid(void)
{
    return (*(volatile uint32_t *)FLASH_APP_BASE) != 0xFFFFFFFF;
}

bool iap_is_update_requested(void)
{
    return (*(volatile uint32_t *)CAL_ADDR) == CHECK_NUM;
}

void iap_clear_update_flag(void)
{
    // Read the page containing CalAddr
    uint32_t page_addr = CAL_ADDR & ~(FLASH_PAGE_SIZE - 1);
    uint32_t buf[FLASH_PAGE_SIZE / 4];

    memcpy(buf, (const void *)page_addr, FLASH_PAGE_SIZE);

    // Clear the CalAddr value (set to 0xFFFFFFFF)
    uint32_t word_offset = (CAL_ADDR - page_addr) / 4;
    buf[word_offset] = 0xFFFFFFFF;

    // Erase and reprogram
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(page_addr);
    flash_program_page(page_addr, buf);
    FLASH->CTLR |= ((uint32_t)0x00008000); // FLASH_Lock_Fast
    FLASH->CTLR |= ((uint32_t)0x00000080); // FLASH_Lock
}

void iap_jump_to_app(void)
{
    // Disable USB peripheral
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, DISABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, DISABLE);

    // Reset GPIO to floating
    GPIO_DeInit(GPIOA);
    GPIO_DeInit(GPIOB);
    GPIO_DeInit(GPIOC);
    GPIO_AFIODeInit();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, DISABLE);

    // Jump to APP via Software interrupt
    NVIC_EnableIRQ(Software_IRQn);
    NVIC_SetPendingIRQ(Software_IRQn);
}

// SW_Handler: jump to APP entry point
void SW_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SW_Handler(void)
{
    __asm volatile("li  a6, 0x3000");
    __asm volatile("jr  a6");
}
