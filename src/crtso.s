/*
 *      crtso.s
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "constants.h"
.section .text

.globl start
start:
  call init_userspace_malloc
  call main
  pushl %eax
  pushl $0
  mov $SYSCALL_EXIT,%eax
  int $INTERRUPT_SYSCALL
idle:
  jmp idle

.globl __assert
__assert:
  jmp user_mode_assert

.globl write_to_screen
write_to_screen:
  ret
