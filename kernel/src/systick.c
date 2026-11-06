/**
* @file systick.c
*
* @brief intialization, delay, get_ticks, and c handler for systick
*
* @date 10/8/2026
*
* @author Aravind Gunasekaran (agunasek)
*         Julie Nikolatos (mnikolat)
*/

#include <unistd.h>
#include <systick.h>

#define UNUSED __attribute__((unused))

//all of the systick registers - on page 246 of m4 programming manual
struct systick_reg_map {
    volatile uint32_t CTRL;   /**< Control & Status Register */
    volatile uint32_t LOAD;   /**<  Reload Val Register */
    volatile uint32_t VAL;    /**<  Current Val Register */
    volatile uint32_t CALIB;  /**<  Calibration Val Register */
};

/** @brief Base address for SYSTICK registers */
#define SYSTICK_BASE  (struct systick_reg_map *) 0xE000E010

/** @brief Clear value for current value in VAL reg */
#define SYS_CURR_CLR  0x00000000

/** @brief CLKSOURCE bit to enable processor clock */
#define CLKSOURCE 1 << 2

/** @brief TICKINT bit to enable processor clock */
#define TICKINT 1 << 1

/** @brief COUNTER bit to enable processor counter */
#define COUNTER_EN 1 

/** @brief number of times systick_c_handler is called
    returned by systick_get_ticks */
static volatile uint32_t systick_ticks = 0;


//in syscall_thread.c
void schedulerTick(void);

/**
 * @brief initializes systick: sets reload value, resets curr value, and enables counter
 *
 * @param frequency  how many times a second systick fires (Hz)
 */
void systick_init(uint32_t frequency) {
    //compute & program reload val
    struct systick_reg_map *sys = SYSTICK_BASE;
    sys->LOAD = (BASE_FREQ / frequency) - 1;
    //Clear current
    sys->VAL = SYS_CURR_CLR;
    //program control and status register
    sys->CTRL |= CLKSOURCE;
    sys->CTRL |= TICKINT;
    sys->CTRL |= COUNTER_EN;
    systick_ticks = 0;
}

/**
 * @brief delays the processor for ticks number of milliseconds
 */
void systick_delay(uint32_t ticks) {
    uint32_t start_ticks = systick_get_ticks();
    while ((systick_get_ticks() - start_ticks) < ticks);
    return;
}

/**
 * @brief returns total number of elapsed ticks after STM32 was booted
 *        total number of times systick_c_handler is called
 */
uint32_t systick_get_ticks() {
    return systick_ticks;
}

/**
 * @brief increments the variable systick_ticks and calls schedulerTick
 */
void systick_c_handler() {
    //put function address into interrupt vector table
    systick_ticks++;
    schedulerTick();
}