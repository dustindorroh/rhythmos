/*
 *      unixproc.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include <kernel.h>
#include <filesystem.h>

extern char *filesystem;
extern process *current_process;
extern process processes[MAX_PROCESSES];
extern processlist ready;

/*
 * map_and_copy
 * 
 * Maps a series of physical pages into a process's address space, copying the data
 * from the corresponding pages in another process.
 * 
 * This operates similarly to map_new_pages. The difference is that instead of
 * the pages being empty, their contents is copied from existing pages that are
 * already mapped by another process. In order to obtain the latter, we perform a
 * lookup on the source process's page directory to get the physical address of
 * each page, and then use that to obtain the data to store in the newly-allocated
 * physical pages of the destination process.
 * 
 * This is used by the fork system call, which needs to duplicate all aspects of
 * a process's state. It uses this function to copy the text, data, and stack
 * segments of the parent process.
 */
static void
map_and_copy(page_dir src_dir, page_dir dest_dir,
	     unsigned int start, unsigned int end)
{
	assert(0 == start % PAGE_SIZE);
	assert(0 == end % PAGE_SIZE);
	unsigned int addr;
	for (addr = start; addr < end; addr += PAGE_SIZE) {
		/*
		 * Map new page 
		 */
		unsigned int page = (unsigned int)alloc_page();
		map_page(dest_dir, addr, page, PAGE_USER, PAGE_READ_WRITE);

		/*
		 * Copy from source 
		 */
		unsigned int src_phys;
		int sl = lookup_page(src_dir, addr, &src_phys);
		assert(sl);
		memmove((void *)page, (void *)src_phys, PAGE_SIZE);
	}
}

/*
 * syscall_fork
 * 
 * Implements the fork system call, which makes a copy of an existing process.
 * After this call has completed, the two processes will have exactly the same
 * state, with the exception of their process ids and the return value from the
 * fork call. The return value that the parent process sees will be the (non-zero)
 * process id of the child. The child will see a return value of 0. Based on this,
 * each of the two processes can go their separate ways, with the child process
 * typically taking completely different code
 * path, such as a call to exec.
 * 
 * This call is an alternative to start_process. fork is the standard way to create
 * processes under UNIX, and in most cases is the *only* way for new processes to
 * be started, at least from user-space.
 * 
 * The *full* state of a process must be copied here, including all fields of the
 * process object, and all of the memory associated with the process in its various
 * segments (text, data, and stack).
 */
pid_t syscall_fork(regs * r)
{
	/*
	 * Find a free process identifier 
	 */
	pid_t child_pid = get_free_pid();
	if (0 > child_pid)
		return -EAGAIN;

	process *parent = current_process;
	process *child = &processes[child_pid];
	memset(child, 0, sizeof(process));
	child->pid = child_pid;
	child->parent_pid = parent->pid;
	child->exists = 1;

	/*
	 * Create a page directory for the new process, and set the segment ranges.
	 * Note that we have to temporarily disable paging here, because we need to
	 * deal with physical memory when allocating a page directory and setting up
	 * the mappings. 
	 */
	disable_paging();
	child->pdir = (page_dir) alloc_page();

	child->text_start = parent->text_start;
	child->text_end = parent->text_end;
	child->data_start = parent->data_start;
	child->data_end = parent->data_end;
	child->stack_start = parent->stack_start;
	child->stack_end = parent->stack_end;

	/*
	 * Identity map memory 0-6Mb 
	 */
	unsigned int addr;
	for (addr = 0 * MB; addr < 6 * MB; addr += PAGE_SIZE)
		map_page(child->pdir, addr, addr, PAGE_USER, PAGE_READ_ONLY);

	/*
	 * Copy parent's text, data, and stack segments to child 
	 */
	map_and_copy(parent->pdir, child->pdir, child->text_start,
		     child->text_end);
	map_and_copy(parent->pdir, child->pdir, child->data_start,
		     child->data_end);
	map_and_copy(parent->pdir, child->pdir, child->stack_start,
		     child->stack_end);

	enable_paging(current_process->pdir);

	/*
	 * Copy file handles. The reference count is increased on each of them, so
	 * that we can keep track of how many file descriptors refere to each file
	 * handle. This information is needed so we know when to destroy a file handle
	 * (i.e. when its reference count reaches 0). 
	 */
	int i;
	for (i = 0; i < MAX_FDS; i++) {
		if (NULL != parent->filedesc[i]) {
			child->filedesc[i] = parent->filedesc[i];
			child->filedesc[i]->refcount++;
		}
	}
	/*
	 * Copy the saved CPU registers of the current process, which determines its
	 * execution state (instruction pointer, stack pointer etc.) 
	 */
	child->saved_regs = *r;
	child->saved_regs.eax = 0;	/* child's return value from fork */

	memmove(child->cwd, parent->cwd, PATH_MAX);

	/*
	 * Place the process on the ready list, so that it can begin execution on a
	 * subsequent context switch 
	 */
	child->ready = 1;
	list_add(&ready, child);

	/*
	 * Return the child's process id... note that this value will only go to the
	 * parent, since we set the child's return value from fork above 
	 */
	return child_pid;
}

/* 
 * syscall_vfork
 * 
 * The vfork() function differs from fork() only in that the child process can 
 * share code and data with the calling process (parent process). This speeds 
 * cloning activity significantly at a risk to the integrity of the parent 
 * process if vfork() is misused.
 * 
 * 
 * The use of vfork() for any purpose except as a prelude to an immediate call 
 * to a function from the exec family, or to _exit(), is not advised.
 * 
 * The vfork() function can be used to create new processes without fully 
 * copying the address space of the old process. If a forked process is simply 
 * going to call exec, the data space copied from the parent to the child by 
 * fork() is not used. This is particularly inefficient in a paged environment, 
 * making vfork() particularly useful. Depending upon the size of the parent's 
 * data space, vfork() can give a significant performance improvement over 
 * fork().
 * 
 * The vfork() function can normally be used just like fork(). It does not work,
 * however, to return while running in the child's context from the caller of 
 * vfork() since the eventual return from vfork() would then return to a no 
 * longer existent stack frame. Be careful, also, to call _exit() rather than 
 * exit() if you cannot exec, since exit() flushes and closes standard I/O 
 * channels, thereby damaging the parent process' standard I/O data structures. 
 * (Even with fork(), it is wrong to call exit(), since buffered data would then
 * be flushed twice.)
 * 
 */

pid_t syscall_vfork(regs * r)
{
	/*
	 * Find a free process identifier 
	 */
	pid_t child_pid = get_free_pid();
	if (0 > child_pid)
		return -EAGAIN;

	process *parent = current_process;
	process *child = &processes[child_pid];
	memset(child, 0, sizeof(process));
	child->pid = child_pid;
	child->parent_pid = parent->pid;
	child->exists = 1;

	parent->waiting_on = -1;

	/*
	 * Create a page directory for the new process, and set the segment ranges.
	 * Note that we have to temporarily disable paging here, because we need to
	 * deal with physical memory when allocating a page directory and setting up
	 * the mappings. 
	 */
	disable_paging();
	child->pdir = (page_dir) alloc_page();

	child->text_start = parent->text_start;
	child->text_end = parent->text_end;
	child->data_start = parent->data_start;
	child->data_end = parent->data_end;
	child->stack_start = parent->stack_start;
	child->stack_end = parent->stack_end;

	/*
	 * Identity map memory 0-6Mb 
	 */
	unsigned int addr;
	for (addr = 0 * MB; addr < 6 * MB; addr += PAGE_SIZE)
		map_page(child->pdir, addr, addr, PAGE_USER, PAGE_READ_ONLY);

	/*
	 * Copy parent's text, data, and stack segments to child 
	 */
	map_and_copy(parent->pdir, child->pdir, child->text_start,
		     child->text_end);
	map_and_copy(parent->pdir, child->pdir, child->data_start,
		     child->data_end);
	map_and_copy(parent->pdir, child->pdir, child->stack_start,
		     child->stack_end);

	enable_paging(current_process->pdir);

	/*
	 * Copy file handles. The reference count is increased on each of them, so
	 * that we can keep track of how many file descriptors refere to each file
	 * handle. This information is needed so we know when to destroy a file handle
	 * (i.e. when its reference count reaches 0). 
	 */
	int i;
	for (i = 0; i < MAX_FDS; i++) {
		if (NULL != parent->filedesc[i]) {
			child->filedesc[i] = parent->filedesc[i];
			child->filedesc[i]->refcount++;
		}
	}
	/*
	 * Copy the saved CPU registers of the current process, which determines its
	 * execution state (instruction pointer, stack pointer etc.) 
	 */
	child->saved_regs = *r;
	child->saved_regs.eax = 0;	/* child's return value from vfork */

	//memmove(child->cwd, parent->cwd, PATH_MAX);

	/*
	 * Place the process on the ready list, so that it can begin execution on a
	 * subsequent context switch 
	 */
	child->ready = 1;
	list_add(&ready, child);

	/*
	 * Return the child's process id... note that this value will only go to the
	 * parent, since we set the child's return value from fork above 
	 */

	kprintf("vfork debug output\n");
	kprintf("child->pid: %i,%i\n", child->pid, parent->pid);
	kprintf("exists: %i,%i\n", child->exists, parent->exists);
	kprintf("ready: %i,%i\n", child->ready, parent->ready);
	kprintf("pdir: %i,%i\n", child->pdir, parent->pdir);
	kprintf("last_errno:%i,%i\n", child->last_errno, parent->last_errno);
	kprintf("stack_start: %u,%u\n", child->stack_start,
		parent->stack_start);
	kprintf("stack_end: %u,%u\n", child->stack_end, parent->stack_end);
	kprintf("data_start: %u,%u\n", child->data_start, parent->data_start);
	kprintf("data_end: %u,%u\n", child->data_end, parent->data_end);
	kprintf("text_start: %u,%u\n", child->text_start, parent->text_start);
	kprintf("text_end: %u,%u\n", child->text_end, parent->text_end);

	return child_pid;
}

/*
 * syscall_execve
 * 
 * Implements the execve system call. This effectively does a "brain transplant"
 * on a process by arranging for it to run a different program to what it was
 * previously. This is achieved by loading the new program from the file system,
 * placing a copy in the process's text segment, and changing the instruction
 * pointer of the process to point to the first instruction of the loaded
 * executable file.
 * 
 * In addition to loading a new program, this call is also responsible for passing
 * command line arguments to the new program. This is done by using the memory
 * at the bottom of the stack (actually the highest address, since the stack grows
 * downwards), in which the array of strings corresponding to argv is placed. The
 * argv and argc values are placed at the top of the stack, so that the main
 * function will be able to access them as arguments.
 */
int
syscall_execve(const char *filename, char *const argv[],
	       char *const envp[], regs * r)
{
	process *proc = current_process;

	/*
	 * Verify that the filename and all of the pointers within argc arg valid
	 * (i.e. completely reside in the process's address space) 
	 */
	if (!valid_string(filename))
		return -EFAULT;

	unsigned int argno = 0;
	if (NULL != argv) {
		while (1) {
			if (!valid_pointer(argv, (argno + 1) * sizeof(char *)))
				return -EFAULT;
			if (NULL == argv[argno])
				break;
			if (!valid_string(argv[argno]))
				return -EFAULT;
			argno++;
		}
	}

	/*
	 * Check that the specified executable file actually exists, and find out its
	 * location within the file system 
	 */
	directory_entry *entry;
	int res;
	if (0 > (res = get_directory_entry(filesystem, filename, &entry)))
		return res;
	if (TYPE_DIR == entry->type)
		return -EISDIR;

	/*
	 * Calculate number of arguments and amount of space needed to store them 
	 */
	unsigned int argc = 0;
	unsigned int argslen = 0;
	for (argc = 0; argv && argv[argc]; argc++)
		argslen += strlen(argv[argc]) + 1;

	/*
	 * Allocate a temporary buffer in which to store the argument data. This will
	 * later be copied to the process's stack. The reason we can't do this
	 * in-place is that doing so would risk overwriting some of the data we are
	 * copying from. 
	 */
	unsigned int argdata_size =
	    argslen + argc * sizeof(char *) + 2 * sizeof(int);
	char *argdata = kmalloc(argdata_size);
	char **newargv = (char **)(argdata + 8);

	/*
	 * Work backwards through the allocated buffer, copying in the strings one-
	 * by-one 
	 */
	unsigned int pos = argdata_size;
	for (argno = 0; argno < argc; argno++) {
		unsigned int nbytes = strlen(argv[argno]) + 1;
		pos -= nbytes;
		memmove(&argdata[pos], argv[argno], nbytes);
		newargv[argno] =
		    (char *)(PROCESS_STACK_BASE - argdata_size + pos);
	}

	/*
	 * Set argc and argv as the top two words on the stack. These are the values
	 * that main will see passed in as its parameters. 
	 */
	*(unsigned int *)(argdata + 0) = argc;
	*(unsigned int *)(argdata + 4) = PROCESS_STACK_BASE - argdata_size + 8;

	/*
	 * Unmap the existing text segment 
	 */
	disable_paging();
	unsigned int addr;
	for (addr = proc->text_start; addr < proc->text_end; addr += PAGE_SIZE)
		unmap_and_free_page(proc->pdir, addr);
	for (addr = proc->data_start; addr < proc->data_end; addr += PAGE_SIZE)
		unmap_and_free_page(proc->pdir, addr);

	/*
	 * Resize text and data segments to 0 bytes each 
	 */
	proc->text_start = PROCESS_TEXT_BASE;
	proc->text_end = PROCESS_TEXT_BASE;
	proc->data_start = PROCESS_DATA_BASE;
	proc->data_end = PROCESS_DATA_BASE;

	/*
	 * Load in the text segment from the executable file 
	 */
	char *data = filesystem + entry->location;
	for (pos = 0; pos < entry->size; pos += PAGE_SIZE) {
		proc->text_end = proc->text_start + pos;
		void *page = alloc_page();
		if (PAGE_SIZE <= entry->size - pos)
			memmove(page, &data[pos], PAGE_SIZE);
		else
			memmove(page, &data[pos], entry->size - pos);
		map_page(proc->pdir, proc->text_end, (unsigned int)page,
			 PAGE_USER, PAGE_READ_WRITE);
	}
	proc->text_end = proc->text_start + pos;
	enable_paging(current_process->pdir);

	/*
	 * Copy the command line argument data we set up above to the process's
	 * stack 
	 */
	memmove((void *)(PROCESS_STACK_BASE - argdata_size), argdata,
		argdata_size);
	kfree(argdata);

	/*
	 * Set up the process's saved register state so that when it resumes
	 * execution, it will start from the beginning of the loaded code. 
	 */
	init_regs(r, PROCESS_STACK_BASE - argdata_size,
		  (void *)current_process->text_start);

	return 0;
}

/*
 * syscall_waitpid
 * 
 * Wait for a process to complete. The specified process id must be a child of the
 * current process. If it has already finished, then the exit code will be passed
 * back in the supplied status parameter. Otherwise, the current process will block
 * until the child process finally completes.
 * 
 * The logic for handling the resumption of a process that is blocked on this call
 * is implemented in kill_process.
 */
pid_t syscall_waitpid(pid_t pid, int *status, int options)
{
	if ((0 > pid) || (MAX_PROCESSES <= pid))
		return -ECHILD;
	current_process->waiting_on = -1;

	/*
	 * Check that the child exists and is in fact a child of this process 
	 */
	process *child = &processes[pid];
	if (!child->exists || (child->parent_pid != current_process->pid))
		return -ECHILD;

	if (child->exited) {
		/*
		 * Child has already finished executing; just return its exit code, and
		 * release the slot in the process table 
		 */
		if (NULL != status)
			*status = child->exit_status;
		child->exists = 0;
		return pid;
	} else {
		/*
		 * Child is still running... block the calling process 
		 */
		current_process->waiting_on = pid;
		suspend_process(current_process);
		return -ESUSPEND;
	}
}
