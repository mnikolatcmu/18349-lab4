.cpu cortex-m4
.syntax unified
.thumb

.section .text

.thumb_func
.global enter_user_mode
enter_user_mode:
  ldr r0, =__psp_stack_top
  msr psp, r0
  b crt0
  b _crt0
  bkpt