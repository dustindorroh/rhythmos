/*
 *      page.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include "kernel.h"

/*
 * Page table management
 * 
 * This file contains a set of functions for allocating pages to both the kernel
 * and user processes. It is responsible for managing all of the physical memory
 * above PAGE_START (defined in constants.h). 
 */

/*
 * Address of first piece of unused memory 
 */
unsigned int page_end = PAGE_START;

/*
 * Linked list of free pages 
 */
typedef struct freelist {
	struct freelist *next;
} freelist;
freelist *page_free = NULL;

/*
 * Total number of pages allocated 
 */
unsigned int npages = 0;

/*
 * alloc_page
 * 
 * Allocate a new page. This uses a simple free list allocation scheme, which is
 * simple to implement since all pages are the same size. The page_free variable
 * points to the head of a linked list of free pages, and if this is non-null, then
 * we simply take the first page in the list and make page_free point to the next
 * page in the list. Alternatively, if the free list is empty, we just take a page
 * from the next available address, specified by page_end.
 * 
 * At kernel boot time, the free list is empty, and page_end points to the first
 * memory address available for page allocation, which is PAGE_START.
 */
void *alloc_page(void)
{
	void *address;
	npages++;

	/*
	 * Find the next free page 
	 */
	if (NULL == page_free) {
		/*
		 * Free list is empty; choose next available address 
		 */
		address = (void *)page_end;
		page_end += PAGE_SIZE;
	} else {
		/*
		 * Free list is non-empty; take the first entry in the list 
		 */
		address = (void *)page_free;
		page_free = page_free->next;
	}

	/*
	 * Zero page 
	 */
	unsigned int i;
	for (i = 0; i < 1024; i++)
		((unsigned int *)address)[i] = 0;
	return address;
}

/*
 * free_page
 * 
 * Indicates that a page is no longer needed, and returns it to the free list. The
 * page will become available for use by subsequent calls to alloc_page().
 */
void free_page(void *page)
{
	assert(0 < npages);
	npages--;
	freelist *entry = ((freelist *) page);
	entry->next = page_free;
	page_free = entry;
}

/*
 * map_page
 * 
 * Adds an entry to a page directory for the specified logical to physical mapping.
 * When paging is enabled and this page directory is the current one, any memory
 * accesses that occur within the page that begins at the logical address will
 * actually occur at the corresponding offset from the physical address. Thus, the
 * actual address accessed depends on which page directory is currently active
 * (there is one per process). Both the logical and physical addresses must be page
 * aligned, i.e. a multiple of 4096 bytes. With paging enabled, it is *only*
 * possible to access memory for which a mapping has been set up by this function,
 * and all memory accesses use the logical addresses.
 * 
 * Since x86 structures page mappings into two levels (page directories and page
 * tables), this function first checks if there is an entry in the page directory
 * for the page table that this logical address resides within. If not, it creates
 * one. Then, it sets the appropriate entry in the page table to the specified
 * physical address.
 * 
 * The page tables also support the use of permission bits that specify under what
 * conditions a page can be read from or written to. The access parameter must be
 * either PAGE_USER, specifying that user mode code may access the page, or
 * PAGE_SUPERVISOR, indicating that only kernel mode code may access it. The
 * readwrite parameter is either PAGE_READ_WRITE or PAGE_READ_ONLY, which specifies
 * whether user code can write to the page or not. Code running in kernel mode can
 * always write to the page regardless of whether this bit is set or not.
 */
void
map_page(page_dir pdir, unsigned int logical, unsigned int physical,
	 unsigned int access, unsigned int readwrite)
{
	assert(0 == logical % PAGE_SIZE);	/* ensure it's page-aligned */
	assert(0 == physical % PAGE_SIZE);	/* ensure it's page-aligned */
	unsigned int pageno = logical / PAGE_SIZE;	/* page # of logical address */
	unsigned int dirindex = pageno / 1024;	/* index into page directory */
	unsigned int tblindex = pageno % 1024;	/* index into page table */

	/*
	 * Get page directory entry, creating if necessary. The permission bits here
	 * just act as a filter for the entries in the page table, so we can just
	 * specify full user access here; it's the permission bits in each page table
	 * entry that really count. 
	 */
	if (!(pdir[dirindex] & PAGE_PRESENT)) {
		unsigned int dirpage = (unsigned int)alloc_page();
		pdir[dirindex] =
		    dirpage | PAGE_PRESENT | PAGE_USER | PAGE_READ_WRITE;
	}

	/*
	 * Add/replace the page table entry. The value we set combines the top 20 bits
	 * of the page address with the permission bits, and PAGE_PRESENT to indicate
	 * that this mapping exists. The page address always has its bottom 12 bits as
	 * 0, since it is a multiple of 2^12 = 4096. 
	 */
	page_table ptable = (page_table) (pdir[dirindex] & PAGE_ADDRESS_MASK);
	ptable[tblindex] = physical | PAGE_PRESENT | access | readwrite;
}

/*
 * lookup_page
 * 
 * Find the physical address associated with the specified logical address in a
 * process's page directory. If the page is not mapped, 0 is returned.
 */
int lookup_page(page_dir pdir, unsigned int logical, unsigned int *phys)
{
	assert(0 == logical % PAGE_SIZE);	/* ensure it's page-aligned */
	unsigned int pageno = logical / PAGE_SIZE;	/* page # of logical address */
	unsigned int dirindex = pageno / 1024;	/* index into page directory */
	unsigned int tblindex = pageno % 1024;	/* index into page table */

	if (!(pdir[dirindex] & PAGE_PRESENT))
		return 0;

	page_table ptable = (page_table) (pdir[dirindex] & PAGE_ADDRESS_MASK);
	if (!(ptable[tblindex] & PAGE_PRESENT))
		return 0;
	*phys = (ptable[tblindex] & PAGE_ADDRESS_MASK);
	return 1;
}

/*
 * unmap_and_free_page
 * 
 * Remove a page mapping, and free the physical page associated with it.
 */
void unmap_and_free_page(page_dir pdir, unsigned int logical)
{
	assert(0 == logical % PAGE_SIZE);	/* ensure it's page-aligned */
	unsigned int pageno = logical / PAGE_SIZE;	/* page # of logical address */
	unsigned int dirindex = pageno / 1024;	/* index into page directory */
	unsigned int tblindex = pageno % 1024;	/* index into page table */

	if (!(pdir[dirindex] & PAGE_PRESENT))
		return;

	page_table ptable = (page_table) (pdir[dirindex] & PAGE_ADDRESS_MASK);
	if (!(ptable[tblindex] & PAGE_PRESENT))
		return;
	unsigned int page = (ptable[tblindex] & PAGE_ADDRESS_MASK);
	free_page((void *)page);
	ptable[tblindex] = 0;
}

/*
 * identity_map
 * 
 * Set up a page table entry that maps the specified logical address to the same
 * physical address. This is needed for certain important regions of physical
 * memory to remain accessible when paging is enabled, in particular the kernel
 * memory that system calls need to access.
 */
void
identity_map(page_dir pdir, unsigned int start, unsigned int end,
	     unsigned int access, unsigned int readwrite)
{
	unsigned int addr;
	for (addr = start; addr < end; addr += PAGE_SIZE)
		map_page(pdir, addr, addr, access, readwrite);
}

/*
 * map_new_pages
 * 
 * Allocates a certain number of new pages, and sets up mappings to them starting
 * at the specified base address.
 */
void map_new_pages(page_dir pdir, unsigned int base, unsigned int npages)
{
	assert(0 == base % PAGE_SIZE);
	unsigned int i;
	for (i = 0; i < npages; i++) {
		unsigned int page = (unsigned int)alloc_page();
		map_page(pdir, base + i * PAGE_SIZE, page, PAGE_USER,
			 PAGE_READ_WRITE);
	}
}

/*
 * free_page_dir
 * 
 * Frees the memory associated with a page directory and its page tables. This does
 * *not* free the pages referred to by the page table entries, since some of them
 * may be in parts of memory that are not managed by the page allocator, e.g. the
 * code or data used by the kernel.
 */
void free_page_dir(page_dir pdir)
{
	unsigned int dirindex;
	for (dirindex = 0; dirindex < 1024; dirindex++) {
		if (pdir[dirindex] & PAGE_PRESENT) {
			unsigned int page_addr =
			    pdir[dirindex] & PAGE_ADDRESS_MASK;
			free_page((void *)page_addr);
		}
	}
	free_page(pdir);
}
