/**
 * @file svc_handler.c
 *
 * @brief  handles 6 cases of svc, called in assmbly file
 *
 * @date   10/26/2026
 *
 * @author  Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <stdint.h>
 #include <debug.h>
 #include <svc_num.h>
 #include <syscall.h>
 
 #define UNUSED __attribute__((unused))
 
 #define STACKED_PC 6
 #define SVC_INSTR_SIZE 2
 #define SVC_NUM_MASK 0xFF
 
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
     default: r = -1; break;
   }
   psp[0] = r;
 }