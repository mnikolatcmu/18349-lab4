/**
 * @file  timer.c
 *
 * @brief  implements timers with handler, init, clear, and disable
 *
 * @date   10/26/2026
 *
 * @author  Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <unistd.h>
 #include <timer.h>
 #include <rcc.h>
 #include <nvic.h>
 #include <gpio.h>
 #include <servok.h>
 
 
 #define UNUSED __attribute__((unused))
 
 #define FIRST_TIMER 2
 #define LAST_TIMER 5
 
 #define TIM_CR1_CEN (1 << 0)
 #define TIM_DIER_UIE (1 << 0)
 #define TIM_SR_UIF (1 << 0)
 #define TIM_EGR_UG (1 << 0)
 
 /** @brief tim2_5 */
 struct tim2_5 {
   volatile uint32_t cr1; /**< 00 Control Register 1 */
   volatile uint32_t cr2; /**< 04 Control Register 2 */
   volatile uint32_t smcr; /**< 08 Slave Mode Control */
   volatile uint32_t dier; /**< 0C DMA/Interrupt Enable */
   volatile uint32_t sr; /**< 10 Status Register */
   volatile uint32_t egr; /**< 14 Event Generation */
   volatile uint32_t ccmr[2]; /**< 18-1C Capture/Compare Mode */
   volatile uint32_t ccer; /**< 20 Capture/Compare Enable */
   volatile uint32_t cnt; /**< 24 Counter Register */
   volatile uint32_t psc; /**< 28 Prescaler Register */
   volatile uint32_t arr; /**< 2C Auto-Reload Register */
   volatile uint32_t reserved_1; /**< 30 */
   volatile uint32_t ccr[4]; /**< 34-40 Capture/Compare */
   volatile uint32_t reserved_2; /**< 44 */
   volatile uint32_t dcr; /**< 48 DMA Control Register */
   volatile uint32_t dmar; /**< 4C DMA address for full transfer Register */
   volatile uint32_t or; /**< 50 Option Register */
 };
 
 struct tim2_5* const timer_base[] = {(void *)0x0,   // N/A - Don't fill out
                                      (void *)0x0,   // N/A - Don't fill out
                                      (void *)0x40000000, //TIMER 2
                                      (void *)0x40000400, //TIMER 3
                                      (void *)0x40000800, //TIMER 4
                                      (void *)0x40000C00}; //TIMER 5
 
 static const uint8_t timerIrq[] = {0, 0, TIM2_INT_NUM, TIM3_INT_NUM, TIM4_INT_NUM, TIM5_INT_NUM};
 
 //static int ledOn = 0;
 static int pwmState = 0;
 
 
 //void timer_init(UNUSED int timer, UNUSED uint32_t prescalar, UNUSED uint32_t period) {
 //
 //}
 void timer_init(int timer, uint32_t prescalar, uint32_t period) {
   if ((timer < FIRST_TIMER) || (timer > LAST_TIMER)) return;
   struct rcc_reg_map *rcc = RCC_BASE;
   struct tim2_5 *tim = timer_base[timer];
   rcc->apb1_enr |= (1 << (timer - FIRST_TIMER));
   tim->cr1 &= ~(TIM_CR1_CEN);
   tim->psc = prescalar - 1;
   tim->arr = period - 1;
   tim->cnt = 0;
   tim->egr = TIM_EGR_UG;
   tim->sr &= ~(TIM_SR_UIF);
   tim->dier |= TIM_DIER_UIE;
   nvic_irq(timerIrq[timer], IRQ_ENABLE);
   tim->cr1 |= TIM_CR1_CEN;
 }
 
 //void timer_disable(UNUSED int timer) {
 //
 //}
 void timer_disable(int timer) {
   if ((timer < FIRST_TIMER) || (timer > LAST_TIMER)) return;
   struct rcc_reg_map *rcc = RCC_BASE;
   struct tim2_5 *tim = timer_base[timer];
   tim->cr1 &= ~(TIM_CR1_CEN);
   tim->dier &= ~(TIM_DIER_UIE);
   tim->sr &= ~(TIM_SR_UIF);
   nvic_irq(timerIrq[timer], IRQ_DISABLE);
   rcc->apb1_enr &= ~(1 << (timer - FIRST_TIMER));
 }
 
 //void timer_clear_interrupt_bit(UNUSED int timer) {
 //
 //}
 void timer_clear_interrupt_bit(int timer) {
   if ((timer < FIRST_TIMER) || (timer > LAST_TIMER)) return;
   timer_base[timer]->sr &= ~(TIM_SR_UIF);
 }
 
 //handles either timer for leds or timer for servos
 void tim2Handler(){
   //Timer implementation for LEDs - no longer needed for final
   /*timer_clear_interrupt_bit(TIMER_2);
   if (ledOn == 0){
     gpio_set(LED_PORT, LED_PIN);
     ledOn = 1;
   }
   else {
     gpio_clr(LED_PORT, LED_PIN);
     ledOn = 0;
   }
     */
   struct tim2_5 *tim = timer_base[TIMER_2];
   timer_clear_interrupt_bit(TIMER_2);
   if (pwmState == 0){
     if (ch0_en) gpio_set(PWM0_PORT, PWM0_PIN);
     tim->arr = pwm_channel0 - 1;
     pwmState = 1;
   }
   else if (pwmState == 1){
     gpio_clr(PWM0_PORT, PWM0_PIN);
     tim->arr = PWM_SLOT_US - pwm_channel0 - 1;
     pwmState = 2;
   }
   else if (pwmState == 2){
     if (ch1_en) gpio_set(PWM1_PORT, PWM1_PIN);
     tim->arr = pwm_channel1 - 1;
     pwmState = 3;
   }
   else {
     gpio_clr(PWM1_PORT, PWM1_PIN);
     tim->arr = PWM_SLOT_US - pwm_channel1 - 1;
     pwmState = 0;
   }
 }