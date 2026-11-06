/** @file   syscall_thread.c
 *
 *  @brief  thread syscalls: init, create, scheduling, kill, and the pendsv c handler
 *
 *  @date   11/1/2026
 *
 *  @author Aravind Gunasekaran (agunasek)
 *         Julie Nikolatos (mnikolat)
 */

#include <stdint.h>
#include <stdbool.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include "mpu.h"
#include "arm.h"
#include "systick.h"

/** @brief total number of tcbs, 14 user threads + idle + default */
#define NUM_TCBS 16

/** @brief max number of user threads */
#define MAX_USER_THREADS 14

/** @brief tcb slot for the idle thread */
#define IDLE_INDEX 14

/** @brief tcb slot for the default thread (main) */
#define DEFAULT_INDEX 15

/** @brief max number of mutexes, so they fit in a 32 bit bitvector */
#define MAX_MUTEXES 32

/** @brief bytes in a word */
#define WORD_SIZE 4

/** @brief smallest stack size in bytes (1KB) */
#define MIN_STACK_BYTES 1024

/** @brief clears the thumb bit for the stacked pc */
#define PC_MASK 0xFFFFFFFE

/** @brief      Initial XPSR value, all 0s except thumb bit. */
#define XPSR_INIT 0x1000000

/** @brief Interrupt return code to user mode using PSP.*/
#define LR_RETURN_TO_USER_PSP 0xFFFFFFFD
/** @brief Interrupt return code to kernel mode using MSP.*/
#define LR_RETURN_TO_KERNEL_MSP 0xFFFFFFF1

/** @brief system handler control and state register */
#define SHCSR ((volatile uint32_t *)0xE000ED24)

/** @brief SHCSR bit that is set while an svc is waiting to be handled */
#define SVC_PENDING_BIT (1 << 15)

/** @brief all the states a thread can be in */
typedef enum {
  STATE_UNUSED,   /**< no thread created in this slot/deleted */
  STATE_RUNNABLE, /**< ready, waiting only for the CPU */
  STATE_RUNNING,  /**< on the CPU right now (exactly one thread) */
  STATE_WAITING,  /**< done for this period, or used up C */
  STATE_BLOCKED   /**< final: may not take a mutex yet */
} threadState_t;

/** @brief thread control block, everything the kernel keeps for one thread */
typedef struct tcb {
  void *savedMSP;           /**< where pendsv put the context for this thread */
  int svcStatus;            /**< were we in an svc when swapped */
  int svcPending;           /**< was our svc still waiting to run when swapped */
  uint32_t staticPrio;      /**< prio from sys_thread_create, never changes */
  uint32_t schedulerPrio;   /**< prio compared by scheduler, changed by mutex eventually */
  threadState_t state;      /**< state of this thread */
  uint32_t computeTime;     /**< C, max ticks this thread can run each period */
  uint32_t period;          /**< T, ticks between releases and the deadline */
  uint32_t ticksThisPeriod; /**< ticks counted in current period */
  uint32_t totalTicks;      /**< ticks spent running since created */
  uint32_t nextPeriod;      /**< systemTicks value for start of next period */
  uint32_t heldMutexes;     /**< bit i is set if this thread holds mutex i */
} tcb_t;

/**
 * @brief      Thread stack low and top addresses (user and kernel), from the linker.
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

/**
 * @brief  what the pendsv asm handler pushes on the kernel stack, in the
 *         same order as push {r0, r4-r11, lr}
 */
typedef struct {
  uint32_t psp; /**< user stack pointer of the thread (pushed from r0) */
  uint32_t r4;  /**< Register value for r4 */
  uint32_t r5;  /**< Register value for r5 */
  uint32_t r6;  /**< Register value for r6 */
  uint32_t r7;  /**< Register value for r7 */
  uint32_t r8;  /**< Register value for r8 */
  uint32_t r9;  /**< Register value for r9 */
  uint32_t r10; /**< Register value for r10 */
  uint32_t r11; /**< Register value for r11 */
  uint32_t lr;  /**< exception return code to go back with */
} savedContext_t;

/** @brief default idle function in svc_stubs.S */
extern void idle_default(void);

/** @brief thread_kill svc stub in svc_stubs.S, threads return into it */
extern void thread_kill(void);

/** @brief all the tcbs, indexed by priority */
tcb_t tcbArray[NUM_TCBS];

/** @brief the currently running tcb */
tcb_t *currentTcb;

/** @brief ticks since the scheduler started for sys_get_time() */
volatile uint32_t systemTicks;

/** @brief max number of user threads, from sys_thread_init */
uint32_t maxThreads;

/** @brief size of each thread stack in bytes (stack_size is in words) */
uint32_t stackBytes;

/** @brief max number of mutexes, from sys_thread_init */
uint32_t maxMutexes;

/** @brief number of user threads created and not killed */
uint32_t userThreadCount;

/** @brief total C/T of all user threads for the UB test */
float totalUtil;

/** @brief whether or not the scheduler is started */
bool schedulerStarted;

/**
 * @brief  round robin, picks the next runnable user thread after the current
 *         one and wraps around. picks idle if none can run
 *
 * @return the tcb to run next
 */
static tcb_t *findNextThread(void){
  uint32_t start = currentTcb - tcbArray;
  for (uint32_t k = 1; k <= maxThreads; k++){
    uint32_t i = (start + k) % maxThreads;
    if (tcbArray[i].state == STATE_RUNNABLE) return &tcbArray[i];
  }
  return &tcbArray[IDLE_INDEX];
}

/**
 * @brief  saves the current thread's context and svc status, then picks the
 *         next thread to run. if systick and an svc happen at the same time,
 *         pendsv runs before the svc, so the pending svc is saved with the
 *         thread too, otherwise it would run in the next thread
 *
 * @param context_ptr  msp of the current thread after the asm handler pushed
 *                     its psp, r4-r11 and exception return code
 *
 * @return saved msp of the next thread
 */
void *pendsv_c_handler(void *context_ptr){
  currentTcb->savedMSP = context_ptr;
  currentTcb->svcStatus = get_svc_status();
  currentTcb->svcPending = (*SHCSR & SVC_PENDING_BIT) ? 1 : 0;
  *SHCSR &= ~SVC_PENDING_BIT;
  if (currentTcb->state == STATE_RUNNING) currentTcb->state = STATE_RUNNABLE;

  tcb_t *next = findNextThread();
  next->state = STATE_RUNNING;
  currentTcb = next;
  set_svc_status(next->svcStatus);
  if (next->svcPending) *SHCSR |= SVC_PENDING_BIT;
  return next->savedMSP;
}

/**
 * @brief  called by systick_c_handler each tick. counts the tick and pends a
 *         context swap once the scheduler has started
 */
void schedulerTick(void){
  if (!schedulerStarted) return;
  systemTicks++;
  pend_pendsv();
}

/**
 * @brief  sets every field of a tcb to 0 and marks it unused
 *
 * @param tcb  the tcb to clear
 */
static void clearTcb(tcb_t *tcb){
  tcb->savedMSP = NULL;
  tcb->svcStatus = 0;
  tcb->svcPending = 0;
  tcb->staticPrio = 0;
  tcb->schedulerPrio = 0;
  tcb->state = STATE_UNUSED;
  tcb->computeTime = 0;
  tcb->period = 0;
  tcb->ticksThisPeriod = 0;
  tcb->totalTicks = 0;
  tcb->nextPeriod = 0;
  tcb->heldMutexes = 0;
}

/**
 * @brief  makes a fake context for a new thread, so the first time pendsv
 *         switches to it the thread starts running fn(arg)
 *
 * @param tcb       the thread's tcb, its savedMSP gets set here
 * @param slot      which stack slot to use in each region (0 is at the top)
 * @param fn        function the thread starts in
 * @param arg       argument for fn, goes in r0
 * @param returnTo  where fn goes when it returns
 */
static void buildThreadFrames(tcb_t *tcb, uint32_t slot, void *fn, void *arg, void *returnTo){
  uint32_t userTop = (uint32_t)&__thread_u_stacks_top - (slot * stackBytes);
  uint32_t kernelTop = (uint32_t)&__thread_k_stacks_top - (slot * stackBytes);

  interrupt_stack_frame *hwFrame = (interrupt_stack_frame *)(userTop - sizeof(interrupt_stack_frame));
  hwFrame->r0 = (uint32_t)arg;
  hwFrame->r1 = 0;
  hwFrame->r2 = 0;
  hwFrame->r3 = 0;
  hwFrame->r12 = 0;
  hwFrame->lr = (uint32_t)returnTo;
  hwFrame->pc = (uint32_t)fn & PC_MASK;
  hwFrame->xPSR = XPSR_INIT;

  savedContext_t *swFrame = (savedContext_t *)(kernelTop - sizeof(savedContext_t));
  swFrame->psp = (uint32_t)hwFrame;
  swFrame->r4 = 0;
  swFrame->r5 = 0;
  swFrame->r6 = 0;
  swFrame->r7 = 0;
  swFrame->r8 = 0;
  swFrame->r9 = 0;
  swFrame->r10 = 0;
  swFrame->r11 = 0;
  swFrame->lr = LR_RETURN_TO_USER_PSP;

  tcb->savedMSP = swFrame;
}

/**
 * @brief  checks the arguments, sizes the stacks, clears every tcb and sets
 *         up the idle and default threads
 *
 * @param max_threads  max number of user threads
 * @param stack_size   size of each stack in words
 * @param idle_fn      idle function, or NULL to use idle_default
 * @param max_mutexes  max number of mutexes
 *
 * @return 0 on success, -1 on failure
 */
int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  uint32_t max_mutexes
){
  if (schedulerStarted) return -1;
  if ((max_threads > MAX_USER_THREADS) || (max_mutexes > MAX_MUTEXES)) return -1;

  uint32_t userSpace = &__thread_u_stacks_top - &__thread_u_stacks_low;
  uint32_t kernelSpace = &__thread_k_stacks_top - &__thread_k_stacks_low;

  if ((stack_size == 0) || (stack_size > userSpace / WORD_SIZE)) return -1;
  uint32_t bytes = 1U << mm_log2ceil_size(stack_size * WORD_SIZE);
  if (bytes < MIN_STACK_BYTES) bytes = MIN_STACK_BYTES;

  if (((max_threads + 1) * bytes > userSpace) || ((max_threads + 1) * bytes > kernelSpace)) return -1;

  maxThreads = max_threads;
  stackBytes = bytes;
  maxMutexes = max_mutexes;
  systemTicks = 0;
  userThreadCount = 0;
  totalUtil = 0;

  for (int i = 0; i < NUM_TCBS; i++){
    clearTcb(&tcbArray[i]);
  }

  void *idleFn = (idle_fn != NULL) ? idle_fn : (void *)idle_default;
  tcb_t *idle = &tcbArray[IDLE_INDEX];
  buildThreadFrames(idle, max_threads, idleFn, NULL, (void *)idle_default);
  idle->staticPrio = IDLE_INDEX;
  idle->schedulerPrio = IDLE_INDEX;
  idle->state = STATE_RUNNABLE;

  tcb_t *defaultTcb = &tcbArray[DEFAULT_INDEX];
  defaultTcb->staticPrio = DEFAULT_INDEX;
  defaultTcb->schedulerPrio = DEFAULT_INDEX;
  defaultTcb->state = STATE_RUNNING;
  currentTcb = defaultTcb;

  return 0;
}

/**
 * @brief  sets up the tcb and stacks for a new thread if it passes the UB test
 *
 * @param fn     function the thread runs
 * @param prio   priority of the thread, also its tcb index
 * @param C      compute time in ticks
 * @param T      period in ticks
 * @param vargp  argument passed to fn
 *
 * @return 0 on success, -1 on failure
 */
int sys_thread_create(
  void *fn,
  uint32_t prio,
  uint32_t C,
  uint32_t T,
  void *vargp
){
  if (fn == NULL) return -1;
  if (prio >= maxThreads) return -1;
  if ((C == 0) || (C > T)) return -1;

  int interruptState = save_interrupt_state_and_disable();

  tcb_t *tcb = &tcbArray[prio];
  if (tcb->state != STATE_UNUSED){
    restore_interrupt_state(interruptState);
    return -1;
  }

  uint32_t n = userThreadCount + 1;
  float util = totalUtil + ((float)C / (float)T);
  if (util > ub_table[n]){
    restore_interrupt_state(interruptState);
    return -1;
  }

  buildThreadFrames(tcb, prio, fn, vargp, (void *)thread_kill);
  tcb->svcStatus = 0;
  tcb->svcPending = 0;
  tcb->staticPrio = prio;
  tcb->schedulerPrio = prio;
  tcb->computeTime = C;
  tcb->period = T;
  tcb->ticksThisPeriod = 0;
  tcb->totalTicks = 0;
  tcb->heldMutexes = 0;
  if (schedulerStarted) tcb->nextPeriod = systemTicks + T;
  else tcb->nextPeriod = T;
  tcb->state = STATE_RUNNABLE;

  userThreadCount = n;
  totalUtil = util;

  if (schedulerStarted) pend_pendsv();

  restore_interrupt_state(interruptState);
  return 0;
}

/**
 * @brief  starts systick at the given frequency and pends pendsv to switch
 *         out of main. main comes back here once every user thread is done
 *
 * @param frequency  systick frequency in Hz
 *
 * @return 0 on success, -1 on failure
 */
int sys_scheduler_start( uint32_t frequency ){
  if ((userThreadCount == 0) || (frequency == 0) || (frequency > BASE_FREQ)) return -1;
  if (schedulerStarted) return -1;

  int interruptState = save_interrupt_state_and_disable();
  systemTicks = 0;
  systick_init(frequency);
  schedulerStarted = true;
  pend_pendsv();
  restore_interrupt_state(interruptState);

  return 0;
}

/**
 * @brief  returns the priority of the running thread
 *
 * @return the running thread's scheduler priority
 */
uint32_t sys_get_priority(){
  return currentTcb->schedulerPrio;
}

/**
 * @brief  returns the time since the scheduler started
 *
 * @return number of ticks
 */
uint32_t sys_get_time(){
  return systemTicks;
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