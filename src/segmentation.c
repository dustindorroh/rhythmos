/*
 *      segmentation.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include "kernel.h"

/*
 * This file contains routines to set up the CPU's segment table so that we
 * can access memory properly in kernel and user mode. You don't need to
 * understand this part of the kernel for the purposes of the course and
 * assignments, but it's necessary in order for the kernel and the different
 * privelege levels to work properly.
 * 
 * Parts of the code in this file were derived from:
 * http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html
 * http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
 * These pages provide a more in-depth explanation of the setup code. 
 */

#define OPSIZE_16BIT         0
#define OPSIZE_32BIT         1

#define GRANULARITY_1BYTE    0
#define GRANULARITY_4KB      1

#define DESC_SYSTEM          0
#define DESC_CODEDATA        1

#define SEG_READ_ONLY        0
#define SEG_READ_WRITE       2
#define SEG_EXECUTE_ONLY     8
#define SEG_EXECUTE_READ     10
#define SEG_TSS              9

typedef struct {
	unsigned short limit_low;
	unsigned short base_low;
	unsigned char base_middle;
	unsigned char seg_type:4;
	unsigned char desc_type:1;
	unsigned char dpl:2;
	unsigned char present:1;
	unsigned char limit_high:4;
	unsigned char avl:1;
	unsigned char op64:1;
	unsigned char opsize:1;
	unsigned char granularity:1;
	unsigned char base_high;
} __attribute__ ((packed)) gdt_entry;

typedef struct {
	unsigned short limit;
	unsigned int base;
} __attribute__ ((packed)) gdt_ptr;

struct {
	unsigned short prevtask, r_prevtask;
	unsigned int esp0;
	unsigned short ss0, r_ss0;
	unsigned int esp1;
	unsigned short ss1, r_ss1;
	unsigned int esp2;
	unsigned short ss2, r_ss2;
	unsigned int cr3;
	unsigned int eip;
	unsigned int eflags;
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;
	unsigned short es, r_es;
	unsigned short cs, r_cs;
	unsigned short ss, r_ss;
	unsigned short ds, r_ds;
	unsigned short fs, r_fs;
	unsigned short gs, r_gs;
	unsigned short ldt, r_ldt;
	unsigned short r_iombase, iombase;
} __attribute__ ((packed)) tss;

#define NUM_SEGMENTS 6

gdt_entry gdt[NUM_SEGMENTS];
gdt_ptr gp;

static void
gdt_set_gate(unsigned int num, unsigned int base,
	     unsigned int limit, unsigned char dpl,
	     unsigned char desc_type, unsigned char seg_type)
{
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;

	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].limit_high = (limit >> 16) & 0x0F;

	gdt[num].present = 1;
	gdt[num].dpl = dpl;
	gdt[num].desc_type = desc_type;
	gdt[num].seg_type = seg_type;

	gdt[num].granularity = GRANULARITY_4KB;
	gdt[num].opsize = OPSIZE_32BIT;
	gdt[num].op64 = 0;
	gdt[num].avl = 0;

	if (0 == num) {
		gdt[num].present = 0;
		gdt[num].granularity = 0;
		gdt[num].opsize = 0;
	}
}

extern unsigned int ih_stack;

void setup_segmentation(void)
{
	gp.limit = sizeof(gdt) - 1;
	gp.base = (unsigned int)&gdt;

	/*
	 * First GDT entry must be empty 
	 */
	gdt_set_gate(0, 0, 0, 0, 0, 0);

	/*
	 * Create two segments (one for code, the other for data) that occupy the
	 * full address range (0-4gb) 
	 */
	gdt_set_gate(1, 0, 0xFFFFFFFF, RING_0, DESC_CODEDATA, SEG_EXECUTE_READ);
	gdt_set_gate(2, 0, 0xFFFFFFFF, RING_0, DESC_CODEDATA, SEG_READ_WRITE);

	/*
	 * Create two more segments as above, with the exception that code in these
	 * segments runs in ring 3 (user mode) instead of ring 0 (supervisor mode) 
	 */
	gdt_set_gate(3, 0, 0xFFFFFFFF, RING_3, DESC_CODEDATA, SEG_EXECUTE_READ);
	gdt_set_gate(4, 0, 0xFFFFFFFF, RING_3, DESC_CODEDATA, SEG_READ_WRITE);

	/*
	 * Create a task state segment (TSS) which contains the stack address for
	 * interrupt handlers, which use a different stack to processes 
	 */
	unsigned int addr = (unsigned int)&tss;
	gdt_set_gate(5, addr, addr + sizeof(tss) - 1, RING_3, DESC_SYSTEM,
		     SEG_TSS);
	gdt[5].granularity = 0;
	gdt[5].opsize = 0;

	/*
	 * Zero the TSS 
	 */
	unsigned int i;
	for (i = 0; i < sizeof(tss) / 4; i++)
		((unsigned int *)&tss)[i] = 0;

	/*
	 * Set segment fields and stack address 
	 */
	tss.ss0 = KERNEL_DATA_SEGMENT;
	tss.esp0 = (unsigned int)&ih_stack;
	tss.cs = KERNEL_CODE_SEGMENT | RING_3;
	tss.ss = KERNEL_DATA_SEGMENT | RING_3;
	tss.ds = KERNEL_DATA_SEGMENT | RING_3;
	tss.es = KERNEL_DATA_SEGMENT | RING_3;
	tss.fs = KERNEL_DATA_SEGMENT | RING_3;
	tss.gs = KERNEL_DATA_SEGMENT | RING_3;

	/*
	 * Tell the processor to read the new GDT and TSS 
	 */
	set_gdt(&gp);
	set_tss(TSS_SEGMENT | RING_3);
}
