/*
 *      interrupts.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "kernel.h"

/*
 * The functions in this file are mainly concerned with setting up interrupt
 * handlers, which are callback functions that are invoked whenever certain
 * events happen, e.g. a page fault exception or a key press. You don't need to
 * understand the hardware-specific stuff this is doing for the purposes of the
 * course/assignments, but it's necessary for this to be here so that the kernel
 * can handle interrupts properly.
 * 
 * Parts of the code in this file were derived from:
 * http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html
 * http://www.jamesmolloy.co.uk/tutorial_html/5.-IRQs%20and%20the%20PIT.html
 * These pages provide a more in-depth explanation of the setup code. 
 */
#define DOWN 0x0600
#define LEFT 0x0601 
#define RIGHT 0x0602 
#define UP 0x0603
#define DELETE 0x7f
#define BACKSPACE 0x08
extern process *current_process;

extern screenchar *screen;
extern unsigned int fpustate[27];

/*
 * US keyboard layout 
 */
char kbdmap[128] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', BACKSPACE,
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
	'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/*
 * US keyboard layout with SHIFT pressed 
 */
char kbdmap_shift[128] = {
	0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', BACKSPACE,
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
	0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
	'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

};


typedef struct {
	unsigned short base_lo;
	unsigned short sel;
	unsigned char always0;
	unsigned char flags;
	unsigned short base_hi;
} __attribute__ ((packed)) idt_entry;

typedef struct {
	unsigned short limit;
	unsigned int base;
} __attribute__ ((packed)) idt_ptr;

idt_entry idt[256];
idt_ptr idtp;

extern void idt_load();

static void
idt_set_gate(unsigned char num, unsigned long base,
	     unsigned short sel, unsigned char flags)
{
	idt[num].base_lo = (base & 0xFFFF);
	idt[num].base_hi = (base >> 16) & 0xFFFF;
	idt[num].sel = sel;
	idt[num].always0 = 0;
	idt[num].flags = flags;
}

extern unsigned int interrupt_handlers[49];

void setup_interrupts(void)
{
	unsigned int i;

	/*
	 * Setup interrupt descriptor table (IDT), and clear all entries 
	 */
	idtp.limit = (sizeof(idt_entry) * 256) - 1;
	idtp.base = (unsigned int)&idt;
	for (i = 0; i < sizeof(idt_entry) * 256; i++)
		*(i + (char *)idt) = '\0';
	idt_load();

	/* Set up mappings between IRQ lines and interrupt no.s  */
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

	/* Setup IRQ and interrupt handlers */
	for (i = 0; i < 48; i++)
		idt_set_gate(i, interrupt_handlers[i], 0x08, 0x8E);
	
	/* Interrupt 48 is system call - allow it to be called from ring 3 */ 
	idt_set_gate(i, interrupt_handlers[i], 0x08, 0xEE);

	/* Configure the system clock to send timer interrupts at 50Hz */
	int div = ISR_FREQ / TICKS_PER_SECOND;

	/* Send the command byte. */
	outb(0x43, 0x36);
	
	outb(0x40, LOWER_BYTE(div));
	outb(0x40, UPPER_BYTE(div));

	/*
	 * We don't enable interrupts yet; this will be done once the other kernel
	 * initialisation is complete 
	 */
}

char *exception_messages[32] = {
	"Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
	"Into Detected Overflow", "Out of Bounds", "Invalid Opcode",
	"No Coprocessor",
	"Double Fault", "Coprocessor Segment Overrun", "Bad TSS",
	"Segment Not Present",
	"Stack Fault", "General Protection Fault", "Page Fault",
	"Unknown Interrupt",
	"Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

static void addstr(unsigned int *x, const char *str)
{
	while (*str) {
		screen[*x].c = *str;
		screen[*x].fg = 15;
		screen[*x].bg = 0;
		(*x)++;
		str++;
	}
}

void fatal(const char *str)
{
	unsigned int x = 0;
	addstr(&x, "Fatal error: ");
	addstr(&x, str);
	while (1) {
	}
}

void __assert(const char *str, const char *function)
{
	if (in_user_mode())
		user_mode_assert(str, function);
	unsigned int x = 0;
	addstr(&x, "Assertion failure in ");
	addstr(&x, function);
	addstr(&x, ": ");
	addstr(&x, str);
	while (1) {
	}
}

void print_regs(regs * r)
{
	kprintf("edi %10p        gs      %10p\n", r->edi, r->gs);
	kprintf("esi %10p        fs      %10p\n", r->esi, r->fs);
	kprintf("ebp %10p        es      %10p\n", r->ebp, r->es);
	kprintf("esp %10p        ds      %10p\n", r->esp, r->ds);
	kprintf("ebx %10p        eip     %10p\n", r->ebx, r->eip);
	kprintf("edx %10p        cs      %10p\n", r->edx, r->cs);
	kprintf("ecx %10p        eflags  %10p\n", r->ecx, r->eflags);
	kprintf("eax %10p        useresp %10p\n", r->eax, r->useresp);
	kprintf("                      ss      %10p\n", r->ss);
	kprintf("cr2 = %p\n", getcr2());
}

void interrupt_handler(regs * r)
{
	unsigned int int_no = r->int_no;
	unsigned int i;

	/*
	 * Copy FPU state from temporary buffer 
	 */
	for (i = 0; i < 27; i++)
		r->fstate[i] = fpustate[i];

	/*
	 * Handle interrupt 
	 */
	switch (int_no) {
	case INTERRUPT_TIMER:
		timer_handler(r);
		break;
	case INTERRUPT_KEYBOARD:
		{
			unsigned char scancode = inb(0x60);
			keyboard_handler(r, scancode);
			break;
		}
	case INTERRUPT_SYSCALL:
		syscall(r);
		break;
	default:
		if ((14 == int_no) && (NULL != current_process) &&
		    !current_process->in_syscall) {
			kprintf
			    ("Process %d: page fault exception at address %p\n",
			     current_process->pid, getcr2());
			kill_process(current_process);
			context_switch(r);
		} else if (MAX_EXCEPTION >= int_no) {
			unsigned int x = 0;
			print_regs(r);
			addstr(&x, exception_messages[r->int_no]);
			addstr(&x, " exception. System halted!");
			asm volatile ("hlt;");
		}
		break;
	}

	/*
	 * If the interrupt number is in the range 32-47, then it corresponds to an
	 * IRQ (interrupt request), e.g. timer event. We need to send out
	 * commands to one or both of the PICs (programmable interrupt controllers) to
	 * indicate that we have finished handling the interrupt. 
	 */
	if ((int_no >= 32) && (int_no < 48)) {
		if (int_no >= 40)
			outb(0xA0, 0x20);
		outb(0x20, 0x20);
	}

	/*
	 * Copy FPU state back to temporary buffer 
	 */
	for (i = 0; i < 27; i++)
		fpustate[i] = r->fstate[i];
}

/*
 * move_cursor
 * 
 * Changes the position of the cursor in the video card's registers. This involves
 * send out a sequence of commands on certain ports that get directed to the
 * video card.
 */
void move_cursor(int x, int y)
{
	unsigned int val = y * 80 + x;
	outb(0x3D4, 14);
	outb(0x3D5, val >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, val);
}
