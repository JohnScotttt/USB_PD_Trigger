;/********************************** (C) COPYRIGHT *******************************
;* File Name          : startup_ch32x035.s
;* Description        : Bootloader startup - includes SW_Handler for jumping to APP.
;*******************************************************************************/

	.section	.init,"ax",@progbits
	.global	_start
	.align	1
_start:
	j	handle_reset

    .section    .vector,"ax",@progbits
    .align  1
_vector_base:
    .option norvc;
    .word   _start
    .word   0
    .word   NMI_Handler                /* NMI */
    .word   HardFault_Handler          /* Hard Fault */
    .word   0
    .word   Ecall_M_Mode_Handler       /* Ecall M Mode */
    .word   0
    .word   0
    .word   Ecall_U_Mode_Handler       /* Ecall U Mode */
    .word   Break_Point_Handler        /* Break Point */
    .word   0
    .word   0
    .word   SysTick_Handler            /* SysTick */
    .word   0
    .word   SW_Handler                 /* SW */
    .word   0
    /* External Interrupts */
    .word   WWDG_IRQHandler            /* Window Watchdog */
    .word   PVD_IRQHandler             /* PVD through EXTI Line detect */
    .word   FLASH_IRQHandler           /* Flash */
    .word   0
    .word   EXTI7_0_IRQHandler         /* EXTI Line 7..0 */
    .word   AWU_IRQHandler             /* Auto Wake up */
    .word   DMA1_Channel1_IRQHandler   /* DMA1 Channel 1 */
    .word   DMA1_Channel2_IRQHandler   /* DMA1 Channel 2 */
    .word   DMA1_Channel3_IRQHandler   /* DMA1 Channel 3 */
    .word   DMA1_Channel4_IRQHandler   /* DMA1 Channel 4 */
    .word   DMA1_Channel5_IRQHandler   /* DMA1 Channel 5 */
    .word   DMA1_Channel6_IRQHandler   /* DMA1 Channel 6 */
    .word   DMA1_Channel7_IRQHandler   /* DMA1 Channel 7 */
    .word   ADC1_IRQHandler            /* ADC1 */
    .word   I2C1_EV_IRQHandler         /* I2C1 Event */
    .word   I2C1_ER_IRQHandler         /* I2C1 Error */
    .word   USART1_IRQHandler          /* USART1 */
    .word   SPI1_IRQHandler            /* SPI1 */
    .word   TIM1_BRK_IRQHandler        /* TIM1 Break */
    .word   TIM1_UP_IRQHandler         /* TIM1 Update */
    .word   TIM1_TRG_COM_IRQHandler    /* TIM1 Trigger and Commutation */
    .word   TIM1_CC_IRQHandler         /* TIM1 Capture Compare */
    .word   TIM2_UP_IRQHandler         /* TIM2 Update */
    .word   USART2_IRQHandler          /* USART2 */
    .word   EXTI15_8_IRQHandler        /* EXTI Line 15..8 */
    .word   EXTI25_16_IRQHandler       /* EXTI Line 25..16 */
    .word   USART3_IRQHandler          /* USART3 */
    .word   USART4_IRQHandler          /* USART4 */
    .word   DMA1_Channel8_IRQHandler   /* DMA1 Channel8 */
    .word   USBFS_IRQHandler           /* USBFS Break */
    .word   USBFSWakeUp_IRQHandler     /* USBFS Wake up from suspend */
    .word   PIOC_IRQHandler            /* PIOC */
    .word   OPA_IRQHandler             /* OPA */
    .word   USBPD_IRQHandler           /* USBPD */
    .word   USBPDWakeUp_IRQHandler     /* USBPD Wake up */
    .word   TIM2_CC_IRQHandler         /* TIM2 Capture Compare */
    .word   TIM2_TRG_COM_IRQHandler    /* TIM2 Trigger and Commutation */
    .word   TIM2_BRK_IRQHandler        /* TIM2 Break */
    .word   TIM3_IRQHandler            /* TIM3 */

    .option rvc;
    .section    .text.vector_handler, "ax", @progbits
    .weak   NMI_Handler
    .weak   HardFault_Handler
    .weak   Ecall_M_Mode_Handler
    .weak   Ecall_U_Mode_Handler
    .weak   Break_Point_Handler
    .weak   SysTick_Handler
    .weak   WWDG_IRQHandler
    .weak   PVD_IRQHandler
    .weak   FLASH_IRQHandler
    .weak   EXTI7_0_IRQHandler
    .weak   AWU_IRQHandler
    .weak   DMA1_Channel1_IRQHandler
    .weak   DMA1_Channel2_IRQHandler
    .weak   DMA1_Channel3_IRQHandler
    .weak   DMA1_Channel4_IRQHandler
    .weak   DMA1_Channel5_IRQHandler
    .weak   DMA1_Channel6_IRQHandler
    .weak   DMA1_Channel7_IRQHandler
    .weak   ADC1_IRQHandler
    .weak   I2C1_EV_IRQHandler
    .weak   I2C1_ER_IRQHandler
    .weak   USART1_IRQHandler
    .weak   SPI1_IRQHandler
    .weak   TIM1_BRK_IRQHandler
    .weak   TIM1_UP_IRQHandler
    .weak   TIM1_TRG_COM_IRQHandler
    .weak   TIM1_CC_IRQHandler
    .weak   TIM2_UP_IRQHandler
    .weak   USART2_IRQHandler
    .weak   EXTI15_8_IRQHandler
    .weak   EXTI25_16_IRQHandler
    .weak   USART3_IRQHandler
    .weak   USART4_IRQHandler
    .weak   DMA1_Channel8_IRQHandler
    .weak   USBFS_IRQHandler
    .weak   USBFSWakeUp_IRQHandler
    .weak   PIOC_IRQHandler
    .weak   OPA_IRQHandler
    .weak   USBPD_IRQHandler
    .weak   USBPDWakeUp_IRQHandler
    .weak   TIM2_CC_IRQHandler
    .weak   TIM2_TRG_COM_IRQHandler
    .weak   TIM2_BRK_IRQHandler
    .weak   TIM3_IRQHandler

NMI_Handler:
HardFault_Handler:
Ecall_M_Mode_Handler:
Ecall_U_Mode_Handler:
Break_Point_Handler:
SysTick_Handler:
WWDG_IRQHandler:
PVD_IRQHandler:
FLASH_IRQHandler:
EXTI7_0_IRQHandler:
AWU_IRQHandler:
DMA1_Channel1_IRQHandler:
DMA1_Channel2_IRQHandler:
DMA1_Channel3_IRQHandler:
DMA1_Channel4_IRQHandler:
DMA1_Channel5_IRQHandler:
DMA1_Channel6_IRQHandler:
DMA1_Channel7_IRQHandler:
ADC1_IRQHandler:
I2C1_EV_IRQHandler:
I2C1_ER_IRQHandler:
USART1_IRQHandler:
SPI1_IRQHandler:
TIM1_BRK_IRQHandler:
TIM1_UP_IRQHandler:
TIM1_TRG_COM_IRQHandler:
TIM1_CC_IRQHandler:
TIM2_UP_IRQHandler:
USART2_IRQHandler:
EXTI15_8_IRQHandler:
EXTI25_16_IRQHandler:
USART3_IRQHandler:
USART4_IRQHandler:
DMA1_Channel8_IRQHandler:
USBFS_IRQHandler:
USBFSWakeUp_IRQHandler:
PIOC_IRQHandler:
OPA_IRQHandler:
USBPD_IRQHandler:
USBPDWakeUp_IRQHandler:
TIM2_CC_IRQHandler:
TIM2_TRG_COM_IRQHandler:
TIM2_BRK_IRQHandler:
TIM3_IRQHandler:
1:
	j 1b

	.section	.text.handle_reset,"ax",@progbits
	.weak	handle_reset
	.align	1
handle_reset:
.option push
.option	norelax
	la gp, __global_pointer$
.option	pop
1:
	la sp, _eusrstack
	/* Load highcode code  section from flash to RAM */
2:
    la a0, _highcode_lma
    la a1, _highcode_vma_start
    la a2, _highcode_vma_end
    bgeu a1, a2, 2f
1:
    lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:
/* Load data section from flash to RAM */
	la a0, _data_lma
	la a1, _data_vma
	la a2, _edata
	bgeu a1, a2, 2f
1:
	lw t0, (a0)
	sw t0, (a1)
	addi a0, a0, 4
	addi a1, a1, 4
	bltu a1, a2, 1b
2:
/* Clear bss section */
	la a0, _sbss
	la a1, _ebss
	bgeu a0, a1, 2f
1:
	sw zero, (a0)
	addi a0, a0, 4
	bltu a0, a1, 1b
2:
/* Configure pipelining and instruction prediction */
    li t0, 0x1f
    csrw 0xbc0, t0
/* Enable interrupt nesting and hardware stack */
	li t0, 0x3
	csrw 0x804, t0
/* Enable global interrupt and configure privileged mode */
   	li t0, 0x88
   	csrw mstatus, t0
/* Configure the interrupt vector table recognition mode and entry address mode */
 	la t0, _vector_base
    ori t0, t0, 3
	csrw mtvec, t0

    jal  SystemInit
	la t0, main
	csrw mepc, t0
	mret
