/**
 * @file syscall.c
 *
 * @brief      
 *
 * @date   10/26/2026
 *
 * @author  Aravind Gunasekaran (agunasek) and Julie Nikolatos (mnikolat)
 */

 #include <unistd.h>
 #include <stdio.h>
 #include <syscall.h>
 #include <uart.h>
 #include <printk.h>
 #include <lcd_driver.h>
 #include <arm.h>
 
 #define UNUSED __attribute__((unused))
 
 #define EOT_CHAR 4
 #define BACKSPACE_CHAR '\b'
 #define DELETE_CHAR 127
 
 extern char __heap_low;
 extern char __heap_top;
 
 static char *heapBreak = &__heap_low;
 
 //void *sys_sbrk(UNUSED int incr){
 //  return NULL;
 //}
 void *sys_sbrk(int incr){
   char *prevBreak = heapBreak;
   if ((heapBreak + incr > &__heap_top) || (heapBreak + incr < &__heap_low)) return (void *)-1;
   heapBreak = heapBreak + incr;
   return prevBreak;
 }
 
 //int sys_write(UNUSED int file, UNUSED char *ptr, UNUSED int len){
 //  return -1;
 //}
 int sys_write(int file, char *ptr, int len){
   if (file != STDOUT_FILENO) return -1;
   for (int i = 0; i < len; i++){
     while (uart_put_byte(ptr[i]) == -1);
   }
   return len;
 }
 
 //int sys_read(UNUSED int file, UNUSED char *ptr, UNUSED int len){
 //  return -1;
 //}
 int sys_read(int file, char *ptr, int len){
   static int lastWasReturn = 0;
   char c;
   int count = 0;
   if (file != STDIN_FILENO) return -1;
   while (count < len){
     if (uart_get_byte(&c) == -1) continue;
     if ((c == '\n') && (lastWasReturn == 1)){
       lastWasReturn = 0;
       continue;
     }
     lastWasReturn = 0;
     if (c == EOT_CHAR){
       return count;
     }
     else if ((c == BACKSPACE_CHAR) || (c == DELETE_CHAR)){
       if (count > 0){
         count--;
         sys_write(STDOUT_FILENO, "\b \b", 3);
       }
     }
     else if ((c == '\r') || (c == '\n')){
       if (c == '\r') lastWasReturn = 1;
       ptr[count] = '\n';
       count++;
       sys_write(STDOUT_FILENO, "\n", 1);
       return count;
     }
     else {
       ptr[count] = c;
       count++;
       sys_write(STDOUT_FILENO, &c, 1);
     }
   }
   return count;
 }
 
 /**
  * @brief turns status into char*, prints to terminal and lcd
  *        flushes uart and disables interupts - sleeps
  */
 void sys_exit(int status){
 //  char str[4];
   char str[12];
   snprintf(str, sizeof(str), "%d", status);
   printk(str);
   uart_flush();
   lcd_print(str);
   disable_interrupts();
   while (1){
     wait_for_interrupt();
   }
 }