/*
 *      buddy.c
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 */
#include <constants.h>
#ifdef USERLAND
#include <stdio.h>
#include <string.h>
#include <assert.h>
void kprintf(const char *format, ...);
#else
#include <kernel.h>
#endif				/* USERLAND */
#include <buddy.h>

/* Helper macros for accessing the blocks array */
#define get_sizem(_b)    (ma->blocks[(_b)/(1 << ma->lower)].sizem)
#define is_used(_b)      (ma->blocks[(_b)/(1 << ma->lower)].used)
#define set_sizem(_b,_s) ma->blocks[(_b)/(1 << ma->lower)].sizem = (_s)
#define set_used(_b,_u)  ma->blocks[(_b)/(1 << ma->lower)].used = (_u)

/* Minimum granularity for keeping track of blocks. 
 * This affects the size of the blocks array in the memarea structure. See
 * the description of buddy_init for further details. Note that this must
 * be at least 2 (i.e. 2^2 = 4 bytes),since we store 4 byte pointers in
 * unused blocks for the free list links.  
 */
#define DEFAULT_LOWER 8		/* 256 bytes */

/* mforsize - measure for the block size
 * 
 * Determine the actual block size to be used for a particular requested size. This
 * is called when the buddy allocator receives a new allocation request, in order
 * to figure out how much memory to actually allocate. The block size must be a
 * power of 2, and at least as big as the smallest level of granularity, which is
 * 256 bytes. This function returns the base-2 log of the size, i.e. a value k
 * such that 2^k = blocksize.
 */
static unsigned int mforsize(memarea * ma, unsigned int nbytes)
{
	unsigned int pow2;
	unsigned int m = 0;
	for (pow2 = 1; pow2 <= nbytes; pow2 *= 2)
		m++;
	return (m >= ma->lower) ? m : ma->lower;
}

/* get_buddy - get the buddy address for the measured size
 * 
 * Calculate the address of the buddy of a block, for a given size. The reason why
 * we need to know the size is that a given block address can potentially have
 * multiple buddies - e.g. a block at 2mb could have a buddy at 0mb, or 2.5mb, or
 * 2.25 mb etc. This is also the reason why we keep track of block sizes in the
 * blockinfo array.
 */
static unsigned int get_buddy(memarea * ma, unsigned int block)
{
	unsigned int m = get_sizem(block);
	assert((ma->lower <= m) && (ma->upper >= m));
	if (block & (1 << m))
		return block & ~(1 << m);
	else
		return block | (1 << m);
}

/* add_to_freelist - add a block the the free list.
 * 
 * Add the specified block to the free list of the given size. The buddy allocator
 * maintains separate free lists for each size, and the blocks in each free list
 * are of the corresponding size. This enables a free block of a particular size to
 * be obtained quickly by just removing the first element from the list.
 */
static void add_to_freelist(memarea * ma, unsigned int m, unsigned int block)
{
	assert(block + (1 << m) <= (1 << ma->upper));
	*(unsigned int *)(ma->mem + block) = ma->freelist[m];
	ma->freelist[m] = block;
}

/* remove_from_freelist
 * 
 * Remove the specified block from the free list of a particular size. This
 * function must only be called for blocks that definitely exist in the specified
 * free list.
 */
static void
remove_from_freelist(memarea * ma, unsigned int m, unsigned int block)
{
	int found = 0;
	unsigned int *ptr = &ma->freelist[m];
	while (EMPTY != *ptr) {
		if (*ptr == block) {
			*ptr = *(unsigned int *)(ma->mem + *ptr);
			found = 1;
			break;
		} else {
			ptr = (unsigned int *)(ma->mem + *ptr);
		}
	}
	assert(found);
}

/*! buddy_alloc
 * 
 * Allocate a block of a certain size within the requested memory area. This first
 * rounds up the size to the nearest power of two, and then searches the
 * appropriate free list to see if there any available blocks of that size. If
 * there aren't, it looks at the free lists successfully larger block sizes until
 * it finds a free block. This block is then split repeatedly until a block of the
 * required size is obtained.
 * 
 * The memory returned by this function is always within the range associated with
 * the memarea structure, which ranges from ma->mem to ma->mem + 2^ma->upper.
 * Freeing this memory must only be done by calling buddy_free with the same
 * memarea structure.
 * 
 * This function is not normally called directly; it is instead usually called
 * through the wrapper functions kmalloc (for kernel memory) and malloc (for
 * process memory).
 */
void *buddy_alloc(memarea * ma, unsigned int nbytes)
{
	unsigned int m = mforsize(ma, nbytes);

	/*
	 * Check if the free list is empty; and if so, find a larger block to split 
	 */
	if (EMPTY == ma->freelist[m]) {

		/*
		 * Find the first free block of size > 2^m 
		 */
		int cm;
		for (cm = m + 1; cm <= ma->upper; cm++) {
			if (EMPTY != ma->freelist[cm])
				break;
		}
		if (cm > ma->upper) {
			kprintf("Memory exhausted\n");
#ifndef USERLAND
			assert(0);
#endif				/* USERLAND */
			return NULL;
		}

		/*
		 * We found a free block; keep splitting it in two until we have a block
		 * of the right size 
		 */
		for (; cm > m; cm--) {
			/*
			 * Remove block from free list 
			 */
			unsigned int block = ma->freelist[cm];
			assert(!is_used(block));
			assert(get_sizem(block) == cm);
			remove_from_freelist(ma, cm, block);

			/*
			 * Split the block in two 
			 */
			unsigned int half1 = block;
			unsigned int half2 = block + (1 << (cm - 1));
			set_sizem(half1, cm - 1);
			set_sizem(half2, cm - 1);
			assert(!is_used(half1));
			assert(!is_used(half2));

			/*
			 * Add both halves to the free list 
			 */
			add_to_freelist(ma, cm - 1, half2);
			add_to_freelist(ma, cm - 1, half1);
		}
	}

	/*
	 * Take the next correctly sized block from the free list 
	 */
	unsigned int block = ma->freelist[m];
	assert(EMPTY != block);
	assert(!is_used(block));
	assert(get_sizem(block) == m);
	set_used(block, 1);
	remove_from_freelist(ma, m, block);

	return (void *)(block + (unsigned int)ma->mem);
}

/* buddy_free
 * 
 * Release a region of memory that has previously been allocated by buddy_alloc.
 * Once this function has been called with a particular pointer, that pointer
 * becomes invalid, and should not be used again. It is up to the caller to ensure
 * that this restriction is complied with.
 * 
 * After marking the block as free, this function attempts to coalesce the blocks
 * buddy, if the latter is free. This process can continue multiple times, until
 * no more coalescing is possible. These merges can result in larger block sizes
 * becoming available for subsequent allocation.
 */
void buddy_free(memarea * ma, void *ptr)
{
	/*
	 * Allow the function to be called with a NULL pointer, in which case we don't
	 * actually need to do anything 
	 */
	if (NULL == ptr)
		return;

	/*
	 * Verify that the memory to free is within the correct memory range 
	 */
	assert(((char *)ptr >= ma->mem)
	       && (char *)ptr < ma->mem + (1 << ma->upper));

	/*
	 * Get offset of block relative to start of heap 
	 */
	unsigned int block = ((char *)ptr) - ma->mem;
	unsigned int m = get_sizem(block);
	assert((ma->lower <= m) && (ma->upper >= m));
	assert(is_used(block));

	/*
	 * Mark this block as unused 
	 */
	set_used(block, 0);

	/*
	 * Check if the buddy is free, and is so, merge this block and the buddy
	 * into a larger free block, repeating as many times as possible 
	 */
	unsigned int buddy = get_buddy(ma, block);
	while ((m < ma->upper) && !is_used(buddy) && (get_sizem(buddy) == m)) {
		remove_from_freelist(ma, m, buddy);
		if (block > buddy) {
			unsigned int tmp = block;
			block = buddy;
			buddy = tmp;
		}
		set_sizem(block, m + 1);
		set_sizem(buddy, 0);
		m++;

		buddy = get_buddy(ma, block);
	}

	/*
	 * Place the (possibly merged) block on the free list 
	 */
	add_to_freelist(ma, m, block);
}

/* buddy_nblocks
 * 
 * Compute the number of blockinfo structures necessary to keep track of a region
 * of memory whose size (in powers of two) is given by sizepow2.
 */
unsigned int buddy_nblocks(unsigned int sizepow2)
{
	return (1 << (sizepow2 - DEFAULT_LOWER));
}

/* buddy_init
 * 
 * Initialise a memarea structure that is used by the buddy allocator to manage a
 * specific region of memory. The size of the region is specified as a power of
 * two, since the buddy allocation algorithm relies on the memory it is managing
 * being this size.
 * 
 * The blocks parameter is an array of blockinfo structures that are used to record
 * which parts of memory are used, and what block sizes they are part of. The
 * number of elements that this array must have depends on the size of the memory
 * being managed, and and can be calculated by calling the buddy_nblocks function.
 * 
 * The lower field of the memarea structure determines the granularity at which
 * memory is managed, which in turn affects the number of blockinfo structures that
 * are used. Smaller blocks potentially reduce the amount of wasted memory, but
 * require more memory to be set aside for the blocks array. We make a compromise
 * of 256 bytes of memory for every block element (1 byte). which means that this
 * is the smallest amount of memory that can be allocated. This value is defined
 * by the DEFAULT_LOWER macro near the top of this file.
 */
void buddy_init(memarea * ma, unsigned int sizepow2, char *membase,
		blockinfo * blocks)
{
	memset(ma, 0, sizeof(memarea));
	ma->lower = DEFAULT_LOWER;
	ma->upper = sizepow2;
	ma->mem = membase;
	ma->blocks = blocks;
	unsigned int nblocks = (1 << (ma->upper - ma->lower));
	memset(ma->blocks, 0, nblocks * sizeof(blockinfo));
	memset(ma->freelist, 0xFF, 32 * sizeof(unsigned int));
	ma->blocks[0].sizem = ma->upper;
	ma->freelist[ma->upper] = 0;
	*(unsigned int *)(ma->mem + 0) = EMPTY;
}

#ifndef USERLAND
/* init_userspace_malloc - maintain the connections that malloc depends on.
 * 
 * Sets up the data segment of a process for use by malloc. The initial size of
 * the heap is specified here as a power of two, such that if this value is k then
 * the total heap size is 2^k. The data segment needs to hold this much memory,
 * plus some additional memory for the bookkepping data required by the buddy
 * allocation algorithm. This consists of a memarea structure, which is a fixed
 * size, as well as an array of blockinfo objects, which is a variable size based
 * on the size of the heap.
 * 
 * The brk system call is used by the process to request that the kernel modify
 * the end of the process's data segment to the specified address. This address is
 * calculated by taking the start of the data segment, and adding the total amount
 * of memory required for the heap as well as the bookkeeping data. Once this
 * memory has been obtained by the process, it can calculate the address of its
 * memarea structure and the blockinfo array, which are placed directly after the
 * heap. These are passed to buddy_init to set up the bookkeeping data so that
 * subsequent calls to buddy_alloc and buddy_free (called by malloc and free,
 * respectively) will work.
 * 
 * This function must be called at process startup, before main begins.
 */
void init_userspace_malloc()
{
	/*
	 * Calculate the heap size in bytes, as well as the amount of memory required
	 * for bookkeeping purposes 
	 */
	unsigned int heap_sizep2 = 20;	/* 1Mb */
	unsigned int heap_size = (1 << heap_sizep2);
	unsigned int memarea_size = sizeof(memarea);
	unsigned int blocks_size =
	    buddy_nblocks(heap_sizep2) * sizeof(blockinfo);
	unsigned int total_size = heap_size + memarea_size + blocks_size;
	unsigned int data_end = PROCESS_DATA_BASE + total_size;

	/*
	 * Request this much memory from the kernel 
	 */
	if (0 != brk((void *)data_end)) {
		printf("brk failed\n");
		exit(1);
	}

	/*
	 * Initialise the bookkeeping data 
	 */
	unsigned int memarea_start = PROCESS_DATA_BASE;
	unsigned int heap_start = memarea_start + memarea_size;
	unsigned int blocks_start = heap_start + heap_size;
	memarea *ma = (memarea *) memarea_start;
	char *heap = (char *)heap_start;
	blockinfo *blocks = (blockinfo *) blocks_start;

	buddy_init(ma, heap_sizep2, heap, blocks);
}

/* malloc - handles the memory allocation requests coming in.
 * 
 * This is just a wrapper around buddy_alloc, which passes in the
 * process's own memarea struct, to tell the allocation algorithm
 * which memory range and set of bookkeeping data to use. This differs
 * from kmalloc in that the latter allocates memory in the kernel's
 * private area, and can only be used in kernel mode.
 */
void *malloc(unsigned int nbytes)
{
	if (!in_user_mode())
		assert(!"malloc should not be called from kernel mode");
	memarea *ma = (memarea *) PROCESS_DATA_BASE;
	void *ptr = buddy_alloc(ma, nbytes);
	if (!ptr)
		assert(!"Out of memory");
	return ptr;
}

/** 
 * realloc - function changes the size of the memory block 
 * @ptr : pointer to the memory block
 * @size: size to change the memory block too.
 *
 * The realloc() function changes the size of the memory block pointed
 * to by ptr to size bytes.  The contents will be unchanged in the
 * range from the start of the region up to the minimum of the old and
 * new sizes.  If the new size is larger than the old size, the added
 * memory will not be initialized.  If ptr is NULL, then the call is
 * equivalent to malloc(size), for all values of size; if size is
 * equal to zero, and ptr is not NULL, then the call is equivalent to
 * free(ptr).  Unless ptr is NULL, it must have been returned by an
 * earlier call to malloc(), calloc() or realloc().  If the area
 * pointed to was moved, a free(ptr) is done.
 */
void *realloc(void *ptr, size_t size)
{
	if (!in_user_mode())
		assert(!"realloc should not be called from kernel mode");

	/* If ptr is NULL, then the call is equivalent to malloc(size) */
	if (ptr == NULL) {
		printf("Warning ptr is NULL: calling malloc(%d)\n", size);
		return malloc(size);
	} else if (size == 0) {
		printf("Warning size = 0: calling free(ptr)\n");
		free(ptr);
		return NULL;
	}
	///* If size is 0, then the call is equivalent to malloc(size) */       
	//if (size == 0 && (ptr != NULL)) {
	//free(ptr);
	//return NULL;
	//}
}

//static void *debug_realloc (void *block, size_t size) {
  //if (size == 0) {
    //freeblock(block);
    //return NULL;
  //}
  //else if (memdebug_total+size > memdebug_memlimit)
    //return NULL;  /* to test memory allocation errors */
  //else {
    //size_t realsize = HEADER+size+MARKSIZE;
    //char *newblock = (char *)(malloc)(realsize);  /* alloc a new block */
    //int i;
    //if (realsize < size) return NULL;  /* overflow! */
    //if (newblock == NULL) return NULL;
    //if (block) {
      //size_t oldsize = *blocksize(block);
      //if (oldsize > size) oldsize = size;
      //memcpy(newblock+HEADER, block, oldsize);
      //freeblock(block);  /* erase (and check) old copy */
    //}
    //memdebug_total += size;
    //if (memdebug_total > memdebug_maxmem) memdebug_maxmem = memdebug_total;
    //memdebug_numblocks++;
    //*(unsigned long *)newblock = size;
    //for (i=0;i<MARKSIZE;i++)
      //*(newblock+HEADER+size+i) = (char)(MARK+i);
    //return newblock+HEADER;
  //}
//}

/* free
 * 
 * Wrapper around buddy_free, using the process's memarea struct as for malloc
 */
void free(void *ptr)
{
	if (!in_user_mode())
		assert(!"free should not be called from kernel mode");
	memarea *ma = (memarea *) PROCESS_DATA_BASE;
	buddy_free(ma, ptr);
}

memarea kernel_memarea;
blockinfo kernel_blocks[(1 << (KERNEL_MEM_SIZEPOW2 - DEFAULT_LOWER))];

/* kmalloc_init
 * 
 * Sets up the bookkeeping data for the region of kernel memory set aside for the
 * buddy allocator.
 */
void kmalloc_init(void)
{
	buddy_init(&kernel_memarea, KERNEL_MEM_SIZEPOW2,
		   (char *)KERNEL_MEM_BASE, kernel_blocks);
}

/* kmalloc
 * 
 * Wrapper around buddy_alloc for kernel memory; works similarly to the malloc
 * wrapper
 */
void *kmalloc(unsigned int nbytes)
{
	void *ptr = buddy_alloc(&kernel_memarea, nbytes);
	if (!ptr)
		assert(!"Out of kernel memory");
	return ptr;
}

/* kmalloc
 * 
 * Wrapper around buddy_free for kernel memory
 */
void kfree(void *ptr)
{
	buddy_free(&kernel_memarea, ptr);
}

#endif				/* USERLAND */
