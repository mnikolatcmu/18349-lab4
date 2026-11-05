/** @file   syscall_thread.c
 *
 *  @brief  thread syscalls - init, create, scheduling, kill, and the pendsv c handler
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

/** @brief total number of tcbs - 14 user threads + idle + default */
#define NUM_TCBS 16

/** @brief max number of user threads */
#define MAX_USER_THREADS 14

/** @brief tcb slot for the idle thread - lowest prio after the user threads */
#define IDLE_INDEX 14

/** @brief tcb slot for the default thread (main) - lowest prio of all */
#define DEFAULT_INDEX 15

/** @brief max number of mutexes, so they fit in a 32 bit bitvector */
#define MAX_MUTEXES 32

/** @brief bytes in a word - stack_size is given in words */
#define WORD_SIZE 4

/** @brief smallest stack we hand out, 1KB */
#define MIN_STACK_BYTES 1024

/** @brief clears the thumb bit - the stacked pc has to have bit 0 as 0 */
#define PC_MASK 0xFFFFFFFE

/** @brief      Initial XPSR value, all 0s except thumb bit. */
#define XPSR_INIT 0x1000000

/** @brief Interrupt return code to user mode using PSP.*/
#define LR_RETURN_TO_USER_PSP 0xFFFFFFFD
/** @brief Interrupt return code to kernel mode using MSP.*/
#define LR_RETURN_TO_KERNEL_MSP 0xFFFFFFF1

/** @brief all the states a thread can be in */
typedef enum {
  STATE_UNUSED,   /**< no thread created in this slot/deleted */
  STATE_RUNNABLE, /**< ready - waiting only for the CPU */
  STATE_RUNNING,  /**< on the CPU right now (exactly one thread) */
  STATE_WAITING,  /**< done for this period, or used up C */
  STATE_BLOCKED   /**< final: may not take a mutex yet */
} threadState_t;

/** @brief thread control block - everything the kernel keeps for one thread */
typedef struct tcb {
  void *savedMSP;           /**< where pendsv put the context for this thread */
  int svcStatus;            /**< were we in an svc when swapped */
  uint32_t staticPrio;      /**< prio from sys_thread_create - never changes */
  uint32_t schedulerPrio;   /**< prio compared by scheduler, changed by mutex eventually */
  threadState_t state;      /**< state of this thread */
  uint32_t computeTime;     /**< C - max ticks this thread can run each period */
  uint32_t period;          /**< T - ticks between releases, also the deadline */
  uint32_t ticksThisPeriod; /**< ticks counted in current period */
  uint32_t totalTicks;      /**< ticks spent running since created */
  uint32_t nextPeriod;      /**< systemTicks value for start of next period */
  uint32_t heldMutexes;     /**< final - bit i is set if this thread holds mutex i */
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
 * @brief  what the pendsv asm handler pushes on the kernel stack -
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

/** @brief default idle function in svc_stubs.S - just sleeps */
extern void idle_default(void);

/** @brief all the tcbs - index is the thread's priority */
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


void *pendsv_c_handler(void *context_ptr){
  (void) context_ptr;
  return NULL;
}

/**
 * @brief  sets every field of a tcb back to 0 and marks it unused
 *
 * @param tcb  the tcb to clear
 */
static void clearTcb(tcb_t *tcb){
  tcb->savedMSP = NULL;
  tcb->svcStatus = 0;
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
 * @brief  makes a fake context for a thread that hasn't run yet. the first
 *         time pendsv switches to it, it "returns" into fn(arg)
 *
 * @param tcb       the thread's tcb, its savedMSP gets set here
 * @param slot      which stack slot to use in each region (0 is at the top)
 * @param fn        function the thread starts in
 * @param arg       argument for fn, goes in r0
 * @param returnTo  where fn goes when it returns
 */
static void buildThreadFrames(tcb_t *tcb, uint32_t slot, void *fn, void *arg, void *returnTo){
  //top of this thread's user and kernel stacks - stacks grow down
  //(done as numbers since the linker symbols are just 1 char to the compiler)
  uint32_t userTop = (uint32_t)&__thread_u_stacks_top - (slot * stackBytes);
  uint32_t kernelTop = (uint32_t)&__thread_k_stacks_top - (slot * stackBytes);

  //hardware frame on the user stack - the cpu pops this on exception return
  interrupt_stack_frame *hwFrame = (interrupt_stack_frame *)(userTop - sizeof(interrupt_stack_frame));
  hwFrame->r0 = (uint32_t)arg;
  hwFrame->r1 = 0;
  hwFrame->r2 = 0;
  hwFrame->r3 = 0;
  hwFrame->r12 = 0;
  hwFrame->lr = (uint32_t)returnTo;
  hwFrame->pc = (uint32_t)fn & PC_MASK;
  hwFrame->xPSR = XPSR_INIT;

  //software frame on the kernel stack - the pendsv asm handler pops this
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

int sys_thread_init(
  uint32_t max_threads,
  uint32_t stack_size,
  void *idle_fn,
  uint32_t max_mutexes
){
  //can't redo the setup while threads are running
  if (schedulerStarted) return -1;

  //check our variables make sense - idle_fn can be NULL
  if ((max_threads > MAX_USER_THREADS) || (max_mutexes > MAX_MUTEXES)) return -1;

  //space in each stack region (32KB each)
  uint32_t userSpace = &__thread_u_stacks_top - &__thread_u_stacks_low;
  uint32_t kernelSpace = &__thread_k_stacks_top - &__thread_k_stacks_low;

  //words -> bytes, rounded up to a power of 2 and at least 1KB
  if ((stack_size == 0) || (stack_size > userSpace / WORD_SIZE)) return -1;
  uint32_t bytes = 1U << mm_log2ceil_size(stack_size * WORD_SIZE);
  if (bytes < MIN_STACK_BYTES) bytes = MIN_STACK_BYTES;

  //every user thread + the idle thread needs a slot in both regions
  if (((max_threads + 1) * bytes > userSpace) || ((max_threads + 1) * bytes > kernelSpace)) return -1;

  //save the settings and reset the counters
  maxThreads = max_threads;
  stackBytes = bytes;
  maxMutexes = max_mutexes;
  systemTicks = 0;
  userThreadCount = 0;
  totalUtil = 0;

  //every tcb starts unused
  for (int i = 0; i < NUM_TCBS; i++){
    clearTcb(&tcbArray[i]);
  }

  //idle thread - slot right after the user threads, always runnable
  //if idle_fn returns it falls into idle_default
  void *idleFn = (idle_fn != NULL) ? idle_fn : (void *)idle_default;
  tcb_t *idle = &tcbArray[IDLE_INDEX];
  buildThreadFrames(idle, max_threads, idleFn, NULL, (void *)idle_default);
  idle->staticPrio = IDLE_INDEX;
  idle->schedulerPrio = IDLE_INDEX;
  idle->state = STATE_RUNNABLE;

  //default thread (main) - already running on its own stacks
  //pendsv fills in its savedMSP the first time main gets swapped out
  tcb_t *defaultTcb = &tcbArray[DEFAULT_INDEX];
  defaultTcb->staticPrio = DEFAULT_INDEX;
  defaultTcb->schedulerPrio = DEFAULT_INDEX;
  defaultTcb->state = STATE_RUNNING;
  currentTcb = defaultTcb;

  return 0;
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