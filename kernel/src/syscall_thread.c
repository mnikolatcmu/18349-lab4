/** @file   syscall_thread.c
 *
 *  @brief  
 *
 *  @date   11/1/2026
 *
 *  @author Aravind Gunasekaran (agunasek)
 *         Julie Nikolatos (mnikolat)
 */

#include <stdint.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"

/** @brief      Initial XPSR value, all 0s except thumb bit. */
#define XPSR_INIT 0x1000000

/** @brief Interrupt return code to user mode using PSP.*/
#define LR_RETURN_TO_USER_PSP 0xFFFFFFFD
/** @brief Interrupt return code to kernel mode using MSP.*/
#define LR_RETURN_TO_KERNEL_MSP 0xFFFFFFF1

typedef enum {
  STATE_UNUSED, // no thread created in this slot/deleted
  STATE_RUNNABLE, // ready - waiting only for the CPU
  STATE_RUNNING, // on the CPU right now (exactly one thread)
  STATE_WAITING, // done for this period, or used up C
  STATE_BLOCKED, // final: may not take a mutex yet
  } threadState_t;

typedef struct tcb {
  void *savedMSP //where pendsv put the context for this thread
  uint32_t svc_status //were we in an svc when swapped
  static uint32_t static_prio //prio from sys_thread_create - static
  uint32_t scheduler_prio //prio compared by scheduler, changed by mutex eventually
  threadState_t state //state of this thread
  uint32_t period // period between release - deadline
  uint32_t ticks_this_per //ticks counted in current period
  uint32_t total_ticks //ticks spent running since created
  uint32_t next_per //system_ticks value for start of next period
  uint32_t num_mutexes //number of held mutexes by this tcb - final
} tcb_t;
  

/**
 * @brief      Heap high and low pointers.
 */
//@{
extern char
  __thread_u_stacks_low,
  __thread_u_stacks_top,
  __thread_k_stacks_low,
  __thread_k_stacks_top;
//@}

/**
 * @brief      Precalculated values for UB test.
 */
float ub_table[] = {
  0.000, 1.000, .8284, .7798, .7568,
  .7435, .7348, .7286, .7241, .7205,
  .7177, .7155, .7136, .7119, .7106,
  .7094, .7083, .7075, .7066, .7059,
  .7052, .7047, .7042, .7037, .7033,
  .7028, .7025, .7021, .7018, .7015,
  .7012, .7009
};

/**
 * @struct user_stack_frame
 *
 * @brief  Stack frame upon exception.
 */
typedef struct {
  uint32_t r0;   /** @brief Register value for r0 */
  uint32_t r1;   /** @brief Register value for r1 */
  uint32_t r2;   /** @brief Register value for r2 */
  uint32_t r3;   /** @brief Register value for r3 */
  uint32_t r12;  /** @brief Register value for r12 */
  uint32_t lr;   /** @brief Register value for lr*/
  uint32_t pc;   /** @brief Register value for pc */
  uint32_t xPSR; /** @brief Register value for xPSR */
} interrupt_stack_frame;

tcb_t *current_tcb;              //the currently running tcb
tcb_t *tcb_array[NUM_TCBS];      //array of all tcbs, length of the number of tcbs
bool scheduler_state;            //whether or not the scheduler is started
volatile uint32_t system_ticks;  //ticks since the scheduler started for sys_get_time()
uint32_t NUM_TCBS;               //number of max threads
uint32_t user_thread_count;      //number of user threads currently running
float total_util;                //totals for UB test at this moment


void *pendsv_c_handler(void *context_ptr){
  (void) context_ptr;
  return NULL;
}

int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  uint32_t max_mutexes
){
  (void) max_threads; (void) stack_size; (void) idle_fn; (void) max_mutexes;
  return -1;
}

int sys_thread_create(
  void *fn,
  uint32_t prio,
  uint32_t C,
  uint32_t T,
  void *vargp
){
  (void) fn; (void) prio; (void) C; (void) T; (void) vargp;
  return -1;
}

int sys_scheduler_start( uint32_t frequency ){
  (void) frequency;
  return -1;
}

uint32_t sys_get_priority(){
  return 0U;
}

uint32_t sys_get_time(){
  return 0U;
}

uint32_t sys_thread_time(){
  return 0U;
}

void sys_thread_kill(){
}

void sys_wait_until_next_period(){
}

kmutex_t *sys_mutex_init( uint32_t max_prio ) {
  (void) max_prio;
  return NULL;
}

void sys_mutex_lock( kmutex_t *mutex ) {
  (void) mutex;
}

void sys_mutex_unlock( kmutex_t *mutex ) {
  (void) mutex;
}
