/*
 *      start.s
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include <constants.h>

.section .text
.globl start
.globl interrupt_handlers
.globl idle
.globl set_gdt
.globl set_tss
.globl idt_load
.globl fpustate
.globl enter_user_mode
.globl enable_paging
.globl disable_paging
.globl getcr2

.globl ih_stack

start:
  mov $sys_stack,%rsp
  push %rbx # multiboot header
  call kmain

.align 4
mboot:
  .set MULTIBOOT_PAGE_ALIGN, 1<<0
  .set MULTIBOOT_MEMORY_INFO, 1<<1
  .set MULTIBOOT_AOUT_KLUDGE, 1<<16
  .set MULTIBOOT_HEADER_MAGIC, 0x1BADB002
  .set MULTIBOOT_HEADER_FLAGS, MULTIBOOT_PAGE_ALIGN|MULTIBOOT_MEMORY_INFO|MULTIBOOT_AOUT_KLUDGE
  .set MULTIBOOT_CHECKSUM, -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

  .int MULTIBOOT_HEADER_MAGIC
  .int MULTIBOOT_HEADER_FLAGS
  .int MULTIBOOT_CHECKSUM

  .int mboot 	# Location of this descriptor
  .int code 	# Start of kernel '.text' (code) section.
  .int bss 	# End of kernel '.data' section.
  .int end 	# End of kernel.
  .int start 	# Kernel entry point (initial EIP).

idle:
  jmp idle

set_gdt:
  mov 4(%rsp),%rax
  lgdt (%rax)
  mov $0x10,%ax
  mov %rax,%ds
  mov %rax,%es
  mov %rax,%fs
  mov %rax,%gs
  mov %rax,%ss
  ljmp $0x8, $(set_gdt_end)
set_gdt_end:
  ret

set_tss:
  mov 4(%rsp),%rax
  ltr %ax
  ret

idt_load:
  lidt (idtp)
  ret

interrupt_handlers:
  .int isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
  .int isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
  .int isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
  .int isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
  .int isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
  .int isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
  .int isr48

.macro isrs_noparam num
.globl isr\num
isr\num:
  cli
  push $0x0
  push $\num
  jmp call_interrupt_handler
.endm

.macro isrs_param num
.globl isr\num
isr\num:
  cli
  push $\num
  jmp call_interrupt_handler
.endm

# Exceptions
isrs_noparam 0         # Divide By Zero
isrs_noparam 1         # Debug
isrs_noparam 2         # Non Maskable Interrupt
isrs_noparam 3         # Int 3
isrs_noparam 4         # INTO
isrs_noparam 5         # Out of Bounds
isrs_noparam 6         # Invalid Opcode
isrs_noparam 7         # Coprocessor Not Available
isrs_param   8         # Double Fault
isrs_noparam 9         # Coprocessor Segment Overrun
isrs_param   10        # Bad TSS
isrs_param   11        # Segment Not Present
isrs_param   12        # Stack Fault
isrs_param   13        # General Protection Fault
isrs_param   14        # Page Fault
isrs_noparam 15        # Reserved
isrs_noparam 16        # Floating Point
isrs_noparam 17        # Alignment Check
isrs_noparam 18        # Machine Check
isrs_noparam 19        # Reserved
isrs_noparam 20        # Reserved
isrs_noparam 21        # Reserved
isrs_noparam 22        # Reserved
isrs_noparam 23        # Reserved
isrs_noparam 24        # Reserved
isrs_noparam 25        # Reserved
isrs_noparam 26        # Reserved
isrs_noparam 27        # Reserved
isrs_noparam 28        # Reserved
isrs_noparam 29        # Reserved
isrs_noparam 30        # Reserved
isrs_noparam 31        # Reserved

# IRQs
isrs_noparam 32        # IRQ 0
isrs_noparam 33        # IRQ 1
isrs_noparam 34        # IRQ 2
isrs_noparam 35        # IRQ 3
isrs_noparam 36        # IRQ 4
isrs_noparam 37        # IRQ 5
isrs_noparam 38        # IRQ 6
isrs_noparam 39        # IRQ 7
isrs_noparam 40        # IRQ 8
isrs_noparam 41        # IRQ 9
isrs_noparam 42        # IRQ 10
isrs_noparam 43        # IRQ 11
isrs_noparam 44        # IRQ 12
isrs_noparam 45        # IRQ 13
isrs_noparam 46        # IRQ 14
isrs_noparam 47        # IRQ 15

# Other interrupts
isrs_noparam 48        # System call

call_interrupt_handler:
  # Save the state of all CPU registers
  pusha
  push %ds
  push %es
  push %fs
  push %gs
  fsave fpustate
  subl $108,%rsp

  # Change the segment registers to those used for kernel mode
  mov $0x10,%ax
  mov %ax,%ds
  mov %ax,%es
  mov %ax,%fs
  mov %ax,%gs
  mov %rsp,%rax

  # Call interrupt_handler()
  push %rax
  mov $interrupt_handler,%rax
  call *%rax
  pop %rax

  # Restore register state
  frstor fpustate
  addl $108,%rsp
  pop %gs
  pop %fs
  pop %es
  pop %ds
  popa
  add $8,%rsp
  iret

# This is the function that the kernel uses to perform the initial switch from
# kernel mode into user mode. It also causes interrupts to be enabled (right
# at the end). After this function completes, the kernel will be in a fully
# "running" state, where it is rrsponding to interrupts and context switching
# between processes.
enter_user_mode:

  # Change the segment registers to refer to the data segment that we have set
  # up for user mode (which was done in setup_segmentation)
  mov $0x23, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  # Set up a sequence of 5 words on the top of the stack, which represent the
  # data that the iret instruction uses to restore the state of the processor
  # after an interrupt has completed. Even though we're not actually handling
  # an interrupt here, we use the iret instruction as a way of causing the
  # processor to switch between privilege levels. This is the same thing that
  # happens when a normal interrupt handler returns, except in this case we're
  # manufacturing the saved CPU state on the stack, which in the case of a real
  # interrupt would be put there by the processor itself.
  mov %rsp, %rax
  pushl $0x23              # Stack segment to restore
  pushl %rax               # Stack pointer to restore
  pushf                    # Save processor flags
  orl $0x200, 0(%rsp)      # Set bit indicating interrupts are enabled
  pushl $0x1B              # Code segment to restore (user mode)
  push $switch_to_user_end # Instruction pointer to restore (return address)

  # Now we execute the iret instruction. This will cause the CPU to pop the
  # 5 words from the stack that we just put there, and use these values to
  # restore its state, which we have set up as user mode with interrupts
  # enabled.
  iret

switch_to_user_end:        # The iret instruction will "return" here
  ret

enable_paging:
  # Get the parameter to this function from the stack, and store it in the CR3
  # register, which tells the processor which page directory to use
  movl 4(%rsp),%rax
  movl %rax,%cr3
  # Set the paging bit of the CR0 register, which tells the processor to enable
  # paging
  movl %cr0,%rax
  orl $0x80000000,%rax
  movl %rax,%cr0
  ret

disable_paging:
  # Clear the paging bit of the CR0 register
  movl %cr0,%rax
  andl $0x7FFFFFFF,%rax
  movl %rax,%cr0
  ret

# Returns the value of the CR2 register, which in the case of a page fault,
# indicates the address that the process was trying to access when the fault
# occurred.
getcr2:
  movl %cr2,%rax
  ret

.globl inb
inb:
  push %edx
  movl 8(%rsp),%edx
  movl $0,%rax
  inb %dx,%al
  pop %edx
  ret

.globl outb
outb:
  push %edx
  movl 12(%rsp),%rax
  movl 8(%rsp),%edx
  outb %al,%dx
  pop %edx
  ret

.section .bss
  .lcomm fpustate,108
  .lcomm sys_stack_top,65536
  .lcomm sys_stack,0
  .lcomm ih_stack_top,65536
  .lcomm ih_stack,0
