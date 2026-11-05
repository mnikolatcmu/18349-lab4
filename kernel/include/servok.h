/**
 * @file servok.h
 *
 * @brief 3 functions accessable to control servos, 
 *        defines pwn channels and inputs
 *
 * @date 10/28/2026
 *
 * @author Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

#ifndef _SERVOK_H_
#define _SERVOK_H_

extern int pwm_channel0;
extern int pwm_channel1;
extern int ch0_en;
extern int ch1_en;

#define PWM0_PORT GPIO_A
#define PWM0_PIN 8
#define PWM1_PORT GPIO_B
#define PWM1_PIN 10

#define PWM_SLOT_US 10000

void servo_init();

int sys_servo_enable(uint8_t channel, uint8_t enabled);

int sys_servo_set(uint8_t channel, uint8_t angle);

#endif /* _SERVOK_H_ */
