/* boot.s
 * Assembler startup file for the STM32
 * Julie Nikolatos (mnikolat)
 * Aravind Gunasekaran (agunasek)
 */

# The Cortex M4 is a thumb only processor
.cpu cortex-m4
.syntax unified
.section .ivt
.thumb

.extern systick_c_handler

.global ivt
ivt:
.word   __msp_stack_top     /* stack top address */
.word   _reset_             /* 1 Reset */
.word   _nmi_               /* 2 NMI */
.word   _hard_fault_        /* 3 Hard Fault */
.word   _mm_fault_          /* 4 MM Fault */
.word   _bus_fault_         /* 5 Bus Fault */
.word   _usage_fault_       /* 6 Usage Fault */
.word   spin                /* 7 RESERVED */
.word   spin                /* 8 RESERVED */
.word   spin                /* 9 RESERVED*/
.word   spin                /* 10 RESERVED */
.word   _svc_asm_handler_   /* 11 SV call */
.word   spin                /* 12 Debug reserved */
.word   spin                /* 13 RESERVED */
.word   _pend_sv_           /* 14 PendSV */
.word   systick_c_handler   /* 15 SysTick */
.word   spin                /* 16 IRQ0 Window Watchdog Interrupt */
.word   spin                /* 17 IRQ1 PVD */
.word   spin                /* 18 IRQ2 TAMPER */
.word   spin                /* 19 IRQ3 RTC   */
.word   spin                /* 20 IRQ4 FLASH   */
.word   spin                /* 21 IRQ5 RCC */
.word   spin                /* 22 IRQ6 EXTI0 */
.word   spin                /* 23 IRQ7 EXTI1  */
.word   spin                /* 24 IRQ8 EXTI2   */
.word   spin                /* 25 IRQ9 EXTI3 */
.word   spin                /* 26 IRQ10 EXTI4 */
.word   spin                /* 27 IRQ11 DMA1_Channel1   */
.word   spin                /* 28 IRQ12 DMA1_Channel2   */
.word   spin                /* 29 IRQ13 DMA1_Channel3 */
.word   spin                /* 30 IRQ14 DMA1_Channel4 */
.word   spin                /* 31 IRQ15 DMA1_Channel5   */
.word   spin                /* 32 IRQ16 DMA1_Channel6   */
.word   spin                /* 33 IRQ17 DMA1_Channel7 */
.word   spin                /* 34 IRQ18 ADC1_2 */
.word   spin                /* 35 IRQ19 CAN1_TX   */
.word   spin                /* 36 IRQ20 CAN1_TX0   */
.word   spin                /* 37 IRQ21 CAN1_RX1 */
.word   spin                /* 38 IRQ22 CAN1_SCE */
.word   spin                /* 39 IRQ23 EXTI9_5   */
.word   spin                /* 40 IRQ24 TIM1_BRK   */
.word   spin                /* 41 IRQ25 TIM1_UP */
.word   spin                /* 42 IRQ26 TIM1_TRG_COM */
.word   spin                /* 43 IRQ27 TIM1_CC   */
@.word   spin                /* 44 IRQ28 TIM2   */
.word   tim2Handler
.word   spin                /* 45 IRQ29 TIM3 */
.word   spin                /* 46 IRQ30 TIM5 */
.word   spin                /* 47 IRQ31 I2C1_EV   */
.word   spin                /* 48 IRQ32 I2C1_ER   */
.word   spin                /* 49 IRQ33 I2C2_EV */
.word   spin                /* 50 IRQ34 I2C2_ER */
.word   spin                /* 51 IRQ35 SPI1   */
.word   spin                /* 52 IRQ36 SPI2   */
.word   spin                /* 53 IRQ37 USART1 */
.word   USART2_IRQHandler   /* 54 IRQ38 USART2 */
.word   spin                /* 55 IRQ39 USART3   */
.word   spin                /* 56 IRQ40 EXTI15_10   */
.word   spin                /* 57 IRQ41 RTCAlarm */
.word   spin                /* 58 IRQ42 OTG_FS_WKUP */
.word   spin                /* 59 IRQ43 RESERVED   */
.word   spin                /* 60 IRQ44 RESERVED   */
.word   spin                /* 61 IRQ45 RESERVED */
.word   spin                /* 62 IRQ46 RESERVED */
.word   spin                /* 63 IRQ47 RESERVED   */
.word   spin                /* 64 IRQ48 RESERVED   */
.word   spin                /* 65 IRQ49 RESERVED */
.word   spin                /* 66 IRQ50 TIM5 */
.word   spin                /* 67 IRQ51 SPI3   */
.word   spin                /* 68 IRQ52 UART4   */
.word   spin                /* 69 IRQ53 UART5 */
.word   spin                /* 70 IRQ54 TIM6 */
.word   spin                /* 71 IRQ55 TIM7   */
.word   spin                /* 72 IRQ56 DMA2_Channel1   */
.word   spin                /* 73 IRQ57 DMA2_Channel2 */
.word   spin                /* 74 IRQ58 DMA2_Channel3 */
.word   spin                /* 75 IRQ59 DMA2_Channel4   */
.word   spin                /* 76 IRQ60 DMA2_Channel5   */
.word   spin                /* 77 IRQ61 ETH */
.word   spin                /* 78 IRQ62 ETH_WKUP */
.word   spin                /* 79 IRQ63 CAN2_TX   */
.word   spin                /* 80 IRQ64 CAN2_RX0  */
.word   spin                /* 81 IRQ65 CAN2_RX1 */
.word   spin                /* 82 IRQ66 CAN2_SCE */
.word   spin                /* 83 IRQ67 OTG_FS   */

.section .text

.thumb_func
spin:
  bkpt

.thumb_func
_nmi_ :
  bkpt

.thumb_func
_hard_fault_ :
  bkpt

.thumb_func
_mm_fault_:
  bkpt

.thumb_func
_bus_fault_ :
  bkpt

.thumb_func
_usage_fault_ :
  bkpt

@ pendsv handler, saves psp, r4-r11 and the exception return code on the
@ current thread's kernel stack. pendsv_c_handler returns the msp of the
@ next thread, and its context gets restored from there
.thumb_func
_pend_sv_ :
  mrs r0, psp
  push {r0, r4-r11, lr}
  mov r0, sp
  bl pendsv_c_handler
  mov sp, r0
  pop {r0, r4-r11, lr}
  msr psp, r0
  bx lr

.thumb_func
_spi1_handler:
  bkpt

.thumb_func
.global _reset_
_reset_:

@full span of data from k_data to _u_edata
  ldr r1, =_erodata
  ldr r2, =_k_data
  ldr r3, =_u_edata

@copy the data from flash to sram
copy_data:
  cmp r2, r3
  beq end_copy_data

  @load 1 byte
  ldrb r0, [r1], #1

  @store 1 byte
  strb r0, [r2], #1

  @check loop guard
  b copy_data
end_copy_data:

@check if at end of the data
@if not at end -> continue

ldr r4, =_k_bss
ldr r5, =_u_ebss
movs r0, #0

@ set bss section to 0
set_zero:
  cmp r4, r5
  beq end_set_zero

  @set current spot to 0
  strb r0, [r4]

  @index by 1 byte
  adds r4, #1
  
  @check loop guard
  b set_zero
end_set_zero:

@ The bkpt instruction puts a breakpoint that fires an exception that
@ can be caught if a debugger is attached.
  bl kernel_main
  bkpt

.thumb_func
.global _sys_tick_asm_
_sys_tick_asm_:
  bkpt

.thumb_func
.global _svc_asm_handler_
_svc_asm_handler_:
@  bkpt
  mrs r0, psp
  b svc_c_handler