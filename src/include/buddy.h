/*
 *      buddy.h
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#ifndef BUDDY_H
#define BUDDY_H

#define EMPTY 0xFFFFFFFF

typedef struct blockinfo {
	unsigned char sizem:7;
	unsigned char used:1;
} blockinfo;

typedef struct memarea {
	unsigned int lower;
	unsigned int upper;
	char *mem;
	blockinfo *blocks;
	unsigned int freelist[32];
} memarea;

void *buddy_alloc(memarea * ma, unsigned int nbytes);
void buddy_free(memarea * ma, void *ptr);
unsigned int buddy_nblocks(unsigned int sizepow2);
void buddy_init(memarea * ma, unsigned int sizepow2, char *membase,
		blockinfo * blocks);

#endif				/* BUDDY_H */
