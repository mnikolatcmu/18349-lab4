/**
 * @file   swi_stubs.S
 *
 * @brief  Stub functions for newlib and 349libc syscalls
 *
 * @date
 * @author
 */

.cpu cortex-m4
.syntax unified
.section .swi_stub
.thumb

#include "../../kernel/include/svc_num.h"

.global _sbrk
_sbrk:
  svc #SVC_SBRK
  bx lr

.global _write
_write:
  svc #SVC_WRITE
  bx lr

.global _close
_close:
  svc #SVC_CLOSE
  bx lr

.global _fstat
_fstat:
  svc #SVC_FSTAT
  bx lr

.global _isatty
_isatty:
  svc #SVC_ISATTY
  bx lr

.global _lseek
_lseek:
  svc #SVC_LSEEK
  bx lr

.global _read
_read:
  svc #SVC_READ
  bx lr

.global _exit
_exit:
  svc #SVC_EXIT
  bx lr

.type thread_init, %function
.global thread_init
thread_init:
  svc #SVC_THR_INIT
  bx lr

.type thread_create, %function
.global thread_create
thread_create:
  svc #SVC_THR_CREATE
  bx lr

.type thread_kill, %function
.global thread_kill
thread_kill:
  svc #SVC_THR_KILL
  bx lr

.type scheduler_start, %function
.global scheduler_start
scheduler_start:
  svc #SVC_SCHD_START
  bx lr

.type mutex_init, %function
.global mutex_init
mutex_init:
  svc #SVC_MUT_INIT
  bx lr

.type mutex_lock, %function
.global mutex_lock
mutex_lock:
  svc #SVC_MUT_LOK
  bx lr

.type mutex_unlock, %function
.global mutex_unlock
mutex_unlock:
  svc #SVC_MUT_ULK
  bx lr

.type wait_until_next_period, %function
.global wait_until_next_period
wait_until_next_period:
  svc #SVC_WAIT
  bx lr

.global get_time
get_time:
  svc #SVC_TIME
  bx lr

.global get_priority
get_priority:
  svc #SVC_PRIORITY
  bx lr

.global thread_time
thread_time:
  svc #SVC_THR_TIME
  bx lr

.global servo_enable
servo_enable:
  svc #SVC_SERVO_ENABLE
  bx lr

.global servo_set
servo_set:
  svc #SVC_SERVO_SET
  bx lr

/* The following stubs are not required to be implemented */

.global _start
_start:
  bkpt

.global _kill
_kill:
  bkpt

.global _gettimeofday
_gettimeofday:
  bkpt

.global _times
_times:
  bkpt

.global _getpid
_getpid:
  bkpt

@ default idle thread sleeps until the next interrupt
.type idle_default, %function
.global idle_default
idle_default:
  wfi
  b idle_default