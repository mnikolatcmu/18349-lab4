/**
 * @file svc_handler.c
 *
 * @brief  handles the lab 3 and lab 4 svcs, called in assmbly file
 *
 * @date   10/26/2026
 *
 * @author  Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <stdint.h>
 #include <sys/stat.h>
 #include <debug.h>
 #include <svc_num.h>
 #include <syscall.h>
 #include <servok.h>
 #include <syscall_thread.h>
 #include <syscall_mutex.h>

 #define UNUSED __attribute__((unused))

 #define STACKED_PC 6
 #define SVC_INSTR_SIZE 2
 #define SVC_NUM_MASK 0xFF

 /** @brief index of xPSR in the stacked frame */
 #define STACKED_XPSR 7

 /** @brief index of the 5th arg, the first word above the stacked frame */
 #define FIFTH_ARG 8

 /** @brief xPSR bit 9, set when the hardware added a padding word to the frame */
 #define XPSR_PADDING (1 << 9)

 /** @brief handles different svc nums
 *   @param psp pointer to psp stack top
 **/
 void svc_c_handler(uint32_t *psp) {
   uint16_t *svcInstr = (uint16_t *)(psp[STACKED_PC] - SVC_INSTR_SIZE);
   int svc_number = *svcInstr & SVC_NUM_MASK;
   int r = 0;
   switch (svc_number) {
     case SVC_SBRK: r = (int)sys_sbrk((int)psp[0]); break;
     case SVC_WRITE: r = sys_write((int)psp[0], (char *)psp[1], (int)psp[2]); break;
     case SVC_READ: r = sys_read((int)psp[0], (char *)psp[1], (int)psp[2]); break;
     case SVC_EXIT: sys_exit((int)psp[0]); break;
     case SVC_SERVO_ENABLE: r = sys_servo_enable((uint8_t)psp[0], (uint8_t)psp[1]); break;
     case SVC_SERVO_SET: r = sys_servo_set((uint8_t)psp[0], (uint8_t)psp[1]); break;

     //tell newlib stdout is a terminal so printf is line buffered
     case SVC_FSTAT: ((struct stat *)psp[1])->st_mode = S_IFCHR; r = 0; break;
     case SVC_ISATTY: r = 1; break;

     case SVC_THR_INIT: r = sys_thread_init(psp[0], psp[1], (void *)psp[2], psp[3]); break;
     case SVC_THR_CREATE: {
       uint32_t extraWord = (psp[STACKED_XPSR] & XPSR_PADDING) ? 1 : 0;
       void *vargp = (void *)psp[FIFTH_ARG + extraWord];
       r = sys_thread_create((void *)psp[0], psp[1], psp[2], psp[3], vargp);
       break;
     }
     case SVC_THR_KILL: sys_thread_kill(psp); break;
     case SVC_SCHD_START: r = sys_scheduler_start(psp[0]); break;
     case SVC_MUT_INIT: r = (int)sys_mutex_init(psp[0]); break;
     case SVC_MUT_LOK: sys_mutex_lock((kmutex_t *)psp[0]); break;
     case SVC_MUT_ULK: sys_mutex_unlock((kmutex_t *)psp[0]); break;
     case SVC_WAIT: sys_wait_until_next_period(); break;
     case SVC_TIME: r = (int)sys_get_time(); break;
     case SVC_PRIORITY: r = (int)sys_get_priority(); break;
     case SVC_THR_TIME: r = (int)sys_thread_time(); break;
     default: r = -1; break;
   }
   psp[0] = r;
 }