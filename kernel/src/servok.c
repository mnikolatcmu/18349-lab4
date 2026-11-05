/**
 * @file servok.c
 *
 * @brief Initialize, enable, and set servo through gpio
 *
 * @date   10/29/2026
 *
 * @author  Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

#include <unistd.h>
#include <syscall.h>
#include <servok.h>
#include <gpio.h>
#include <timer.h>

//Defines for servos, the two channels
#define SERVO_CHANNEL_1 1
#define SERVO_CHANNEL_2 2
//maximum angle for returning -1
#define MAX_ANGLE 180
//minimum pulse width
#define MIN_PULSE_US 600
//pulse width addition for each degree
#define US_PER_DEGREE 10
//default initialization angle
#define DEFAULT_ANGLE 90
//prescaler for servo
#define SERVO_PRESCALER 16

//ch0/1 values that are edited by functions below
int pwm_channel0 = MIN_PULSE_US + (DEFAULT_ANGLE * US_PER_DEGREE);
int pwm_channel1 = MIN_PULSE_US + (DEFAULT_ANGLE * US_PER_DEGREE);
int ch0_en = 0;
int ch1_en = 0;

//Initialize servos & timer for servos
void servo_init(){
  gpio_init(PWM0_PORT, PWM0_PIN, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
  gpio_init(PWM1_PORT, PWM1_PIN, MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
  gpio_clr(PWM0_PORT, PWM0_PIN);
  gpio_clr(PWM1_PORT, PWM1_PIN);
  timer_init(TIMER_2, SERVO_PRESCALER, PWM_SLOT_US);
}
/**
* @brief Enable or disable servo motor control
*
* @param channel channel to enable or disable
* @param enabled true to enable, false to disable
*
* @return 0 on success or -1 on failure
*/
int sys_servo_enable(uint8_t channel, uint8_t enabled){
  if (channel == SERVO_CHANNEL_1){
    ch0_en = enabled;
    if (enabled == 0) gpio_clr(PWM0_PORT, PWM0_PIN);
    return 0;
  }
  if (channel == SERVO_CHANNEL_2){
    ch1_en = enabled;
    if (enabled == 0) gpio_clr(PWM1_PORT, PWM1_PIN);
    return 0;
  }
  return -1;

}

/**
* @brief Set a servo motor to a given position
*
* @param channel channel to control
* @param angle servo angle in degrees (0-180)
*
* @return 0 on success or -1 on failure
*/
int sys_servo_set(uint8_t channel, uint8_t angle){
  if (angle > MAX_ANGLE) return -1;
  if (channel == SERVO_CHANNEL_1){
    if (!ch0_en) return -1;
    pwm_channel0 = MIN_PULSE_US + (angle * US_PER_DEGREE);
    return 0;
  }
  if (channel == SERVO_CHANNEL_2){
    if (!ch1_en) return -1;
    pwm_channel1 = MIN_PULSE_US + (angle * US_PER_DEGREE);
    return 0;
  }
  return -1;
}