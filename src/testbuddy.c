/*
 *      testbuddy.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "constants.h"
#include "buddy.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define DELAYMS 0

void kprintf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

void print_free_mem(memarea * ma)
{
	unsigned int m;
	for (m = ma->lower; m <= ma->upper; m++) {
		unsigned int block = ma->freelist[m];
		while (EMPTY != block) {
			printf("free %u: 0x%x-0x%x\n", m, block,
			       block + (1 << m));
			block = *(unsigned int *)(ma->mem + block);
		}
	}
}

void print_mem_line(memarea * ma, int length)
{
	char line[length + 1];
	memset(line, '*', length);
	line[length] = '\0';

	unsigned int m;
	for (m = ma->lower; m <= ma->upper; m++) {
		unsigned int block = ma->freelist[m];
		while (EMPTY != block) {
			double dstart =
			    ((double)block) / ((double)(1 << ma->upper));
			double dend =
			    ((double)(block + (1 << m))) /
			    ((double)(1 << ma->upper));
			assert((dstart >= 0.0) && (dend <= 1.0));
			int istart = dstart * length;
			int iend = dend * length;
			memset(&line[istart], '.', iend - istart);
			block = *(unsigned int *)(ma->mem + block);
		}
	}
	printf("%s\n", line);
}

int main()
{
	setbuf(stdout, NULL);
	memarea ma;
	unsigned int sizepow2 = 25;	/* 32 Mb */
	blockinfo *blocks = malloc(buddy_nblocks(sizepow2) * sizeof(blockinfo));
	char *membase = malloc(1 << sizepow2);
	memset(membase, 0, 1 << sizepow2);

	buddy_init(&ma, sizepow2, membase, blocks);

	srand(1);

	int iterations = 40000;
	void *allocated[iterations];
	int got = 0;
	int malloc_probability = 50;

	int i;
	for (i = 0; i < iterations; i++) {
		if (((rand() % 100) < malloc_probability) || (0 == got)) {
			int nbytes = rand() % (2 * 1024 * 1024);
			allocated[got] = buddy_alloc(&ma, nbytes);
			if (NULL != allocated[got])
				got++;
		} else {
			int index = rand() % got;
			buddy_free(&ma, allocated[index]);
			memmove(&allocated[index], &allocated[index + 1],
				(got - index - 1) * sizeof(void *));
			got--;
		}
		print_mem_line(&ma, 128);

		if (0 < DELAYMS) {
			struct timespec st;
			st.tv_sec = DELAYMS / 1000;
			st.tv_nsec = (DELAYMS % 1000) * 1000000;
			nanosleep(&st, NULL);
		}
	}

	return 0;
}
