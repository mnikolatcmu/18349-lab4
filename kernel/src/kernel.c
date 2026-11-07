/**
 * @file kernel.c
 *
 * @brief  kernel entry, sets up uart, the lcd and the servos, then jumps
 *         to the user program
 *
 * @date   11/5/2026
 *
 * @author  Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

#include <stdlib.h>
#include <string.h>

#include <gpio.h>

#include <systick.h>
#include <lcd_driver.h>
#include <keypad_driver.h>


#include <timer.h>
#include <nvic.h>

#include "arm.h"
#include "kernel.h"
#include "timer.h"
#include "i2c.h"
#include "lcd_driver.h"
#include "printk.h"
#include "uart_polling.h"
#include "uart.h"
#include "stdint.h"
#include "syscall.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "arm.h"
#include "servok.h"


/** @brief USARTDIV for 115200 baud with the 16MHz clock */
#define BAUD_RATE_115200 0x8B

/** @brief i2c clock for the lcd in kHz (standard mode) */
#define I2C_CLK_KHZ 100

/** @brief systick frequency before the scheduler starts, 1kHz so systick_delay counts ms */
#define BOOT_TICK_FREQ 1000

/**
 * @brief  sets up uart, the lcd and the servos, then enters user mode.
 *         systick starts at 1kHz here since the lcd init uses systick_delay
 *
 * @return never returns
 */
int kernel_main() {
  //Do not remove this function
    init_349();

    uart_init(BAUD_RATE_115200);

    systick_init(BOOT_TICK_FREQ);
    i2c_master_init(I2C_CLK_KHZ);
    //lcd_driver_init();

    //servo_init();

    enter_user_mode();

    while(1){
    }


    return 0;
}