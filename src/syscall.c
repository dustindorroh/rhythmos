/*
 *      syscall.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include <kernel.h>
#include <filesystem.h>

/*
 * Prototypes for system call handlers defined in other files 
 */

/*
 * pipe.c 
 */
int syscall_close(int fd);
int syscall_pipe(int filedes[2]);
int syscall_dup2(int oldfd, int newfd);

/*
 * unixproc.c 
 */
pid_t syscall_fork(regs * r);
int syscall_execve(const char *filename, char *const argv[],
		   char *const envp[], regs * r);
pid_t syscall_waitpid(pid_t pid, int *status, int options);

/*
 * fscalls.c 
 */
int syscall_stat(const char *path, struct stat *buf);
int syscall_open(const char *pathname, int flags);
int syscall_getdent(int fd, struct dirent *entry);
int syscall_chdir(const char *path);
char *syscall_getcwd(char *buf, size_t size);

extern process *current_process;
process processes[MAX_PROCESSES];

/**
 * valid_pointer
 * 
 * Checks to see if a pointer supplied to a system call is valid, i.e. resides
 * within the current process's user-accessible address space. This is a security
 * measure to stop processes from trying to subvert the restrictions of user mode
 * by tricking the kernel into reading from or writing to an area of memory that
 * the process would not normally have access to.
 * 
 * Any system call which has an invalid pointer supplied to it is supposed to
 * return -EFAULT.
 */
int valid_pointer(const void *ptr, unsigned int size)
{
	unsigned int start_address = (unsigned int)ptr;
	unsigned int end_address = start_address + size;

	if (0 == size)
		return 1;

	if (end_address < start_address)
		return 0;

	/*
	 * Within stack segment? 
	 */
	if ((start_address >= current_process->stack_start) &&
	    (end_address <= current_process->stack_end))
		return 1;

	/*
	 * Within data segment? 
	 */
	if ((start_address >= current_process->data_start) &&
	    (end_address <= current_process->data_end))
		return 1;

	/*
	 * Within text segment? 
	 */
	if ((start_address >= current_process->text_start) &&
	    (end_address <= current_process->text_end))
		return 1;

	/*
	 * Pointer is invalid 
	 */
	return 0;
}

/**
 * valid_string
 * 
 * Similar to valid_pointer. In the case of strings, we can simply check for a
 * particular length, since they are just arrays of characters terminated by '\0'.
 * So instead we have to scan through the string, checking that each part is valid
 * until we encounter the NULL terminator.
 */
int valid_string(const char *str)
{
	unsigned int len = 0;
	while (valid_pointer(str, len + 1)) {
		if ('\0' == str[len])
			return 1;
		len++;
	}
	return 0;
}

/**
 * syscall_getpid
 * 
 * Returns the identifier of the calling process
 */
static pid_t syscall_getpid()
{
	return current_process->pid;
}

/**
 * syscall_exit
 * 
 * Terminates the calling process. The specified exit status is recorded in the
 * exit_status field of the process, which can later be retrieved by the parent
 * process using waitpid.
 */
static int syscall_exit(int status)
{
	disable_paging();
	current_process->exit_status = status;
	kill_process(current_process);
	return -ESUSPEND;
}

/**
 * syscall_write
 * 
 * Writes the specified data to a file handle. What actually happens to this data
 * will depend on the type of file handle - it could be printed to the screen,
 * written to a pipe, written to a file etc. This function uses the filehandle
 * object corresponding to the specified file descriptor to obtain a pointer to
 * the handle's write function, to which it then dispatches the call.
 */
static ssize_t syscall_write(int fd, const void *buf, size_t count)
{
	if (!valid_pointer(buf, count))
		return -EFAULT;
	if ((0 > fd) || (MAX_FDS <= fd)
	    || (NULL == current_process->filedesc[fd]))
		return -EBADF;

	filehandle *fh = current_process->filedesc[fd];
	return fh->write(fh, buf, count);
}

/**
 * syscall_read
 * 
 * Read some data from a file descriptor. As for write, the way in which the data
 * is read depends on what type of file handle the descriptor refers to. This
 * function simply dispatches to the handle's read function.
 */
static ssize_t syscall_read(int fd, void *buf, size_t count)
{
	if (!valid_pointer(buf, count))
		return -EFAULT;
	if ((0 > fd) || (MAX_FDS <= fd)
	    || (NULL == current_process->filedesc[fd]))
		return -EBADF;

	filehandle *fh = current_process->filedesc[fd];
	return fh->read(fh, buf, count);
}

/**
 * syscall_geterrno
 * 
 * Retrieve the error code of the last system call. This is called whenever a
 * process access errno, which is defined in user.h as a call to this function.
 * 
 * Some systems take an alternative approach by making errno a global variable
 * within the process. We use a system call instead, since global variables require
 * each process to have a private text segment, which is not the case for most
 * versions of our kernel.
 */
static pid_t syscall_geterrno(void)
{
	return current_process->last_errno;
}

/**
 * syscall_brk
 * 
 * Called by a process when it wishes to extend the size of its data segment. The
 * memory allocation scheme used by processes in our kernel is based on a fixed
 * heap size, and thus each process calls brk only once, right at the beginning of
 * its execution. More advanced memory allocation schemes would allow the heap to
 * be extended if more memory is needed, and would call this function whenever they
 * need more pages mapped into their data segment for use by malloc.
 */
static int syscall_brk(void *end_data_segment)
{
	unsigned int oldend = current_process->data_end;
	unsigned int newend = (unsigned int)end_data_segment;

	/*
	 * Don't need to do anything if we already have enough memory 
	 */
	if (newend <= oldend)
		return 0;

	/*
	 * Round up to a multiple of PAGE_SIZE 
	 */
	if (0 != newend % PAGE_SIZE)
		newend = ((newend / PAGE_SIZE) + 1) * PAGE_SIZE;

	disable_paging();
	map_new_pages(current_process->pdir, oldend,
		      (newend - oldend) / PAGE_SIZE);
	enable_paging(current_process->pdir);
	current_process->data_end = newend;
	return 0;
}

int syscall_kill(pid_t pid)
{
	int r = 0;
	if ((0 > pid) || (MAX_PROCESSES <= pid) || (!processes[pid].exists))
		return -ESRCH;

	if (pid == current_process->pid)
		r = -ESUSPEND;	/* force context switch */

	kill_process(&processes[pid]);
	return r;
}

static inline void __attribute__ ((noreturn))
syscall_halt (void)
{
  while (1)
    {
      asm volatile ("hlt");
    }
}

/**
 * syscall_send - Sends a message to the specified process.
 * @tag: is an application-defined value indicating the type of the message
 * @data: contents of the message
 * @size: size of the message.
 * 
 * The maximum size is 1024 bytes. 
 * send returns 0 on success, or -1 on error (setting errno appropriately). 
 * It should never block. 
 */
int syscall_send(pid_t to, unsigned int tag, const void *data, size_t size)
{
	if (!valid_pointer(data, size))
		return -EFAULT;

	if ((0 > to) || (MAX_PROCESSES <= to) || !processes[to].exists)
		return -ESRCH;

	if ((0 > size) || (MAX_MESSAGE_SIZE < size))
		return -EINVAL;

	process *dest = &processes[to];

	if (NULL == dest->mailbox) {
		dest->mailbox_alloc = 8;
		dest->mailbox_size = 1;
		dest->mailbox =
		    (message *) kmalloc(dest->mailbox_alloc * sizeof(message));
	} else if (dest->mailbox_size < dest->mailbox_alloc) {
		dest->mailbox_size++;
	} else {
		return -ENOMEM;
	}

	message *msg = &dest->mailbox[dest->mailbox_size - 1];
	msg->from = current_process->pid;
	msg->tag = tag;
	msg->size = size;
	memcpy(msg->data, data, size);

	if (dest->receive_blocked) {
		resume_process(dest);
		dest->receive_blocked = 0;
	}

	return 0;
}

/**
 * syscall_receive - Receives a message sent to the current process.
 * @message: If a message is available immediately the sender, tag, size, and 
 * 		data are stored in the message struct. Else it depends on the value
 * 		of the block.
 * @block: If true, the calling process is suspended until a message arrives. 
 * 		If false, the function returns immediately with a result of -1, and 
 * 		errno set to EAGAIN.
 */
int syscall_receive(message * msg, int block)
{
	if (!valid_pointer(msg, sizeof(message)))
		return -EFAULT;

	if (current_process->mailbox_size > 0) {
		memcpy(msg, &current_process->mailbox[0], sizeof(message));
		memmove(&current_process->mailbox[0],
			&current_process->mailbox[1],
			(current_process->mailbox_size - 1) * sizeof(message));
		current_process->mailbox_size--;
		return 0;
	} else if (block) {
		current_process->receive_blocked = 1;
		suspend_process(current_process);
		return -ESUSPEND;
	} else {
		return -EAGAIN;
	}
}

/**
 * syscall
 * 
 * Main dispatch routine for system calls. This is called from within
 * interrupt_handler whenever interrupt 48 (INTERRUPT_SYSCALL) is raised by a
 * process. It works out the location in memory of the arguments to the system
 * call, and then based on which call was requested, invokes the appropriate
 * function. Any new system calls that are added to the kernel should have an entry
 * in the switch statement which calls them.
 */
void syscall(regs * r)
{
	/*
	 * Get system call no. from the EAX register, which is set by the code for
	 * each system call within calls.s 
	 */
	unsigned int call_no = r->eax;

	/*
	 * Find out where the top of the process's stack is in memory. The arguments
	 * to this call are 4 bytes below this in the stack (right underneath the
	 * return address). In our kernel, all parameters to system calls are 32 bits
	 * wide, so we can just treat this address as the start of an array of
	 * integers. Where necessary these may be cast to pointers. 
	 */
	unsigned int useresp = r->useresp;
	int *args = (int *)(useresp + 4);

	int res = -1;
	process *old_current = current_process;

	assert(current_process);
	current_process->in_syscall = call_no;

	/*
	 * Dispatch to the appropriate handler function 
	 */
	switch (call_no) {
	case SYSCALL_GETPID:
		res = syscall_getpid();
		break;
	case SYSCALL_EXIT:
		res = syscall_exit(args[0]);
		break;
	case SYSCALL_WRITE:
		res = syscall_write(args[0], (const void *)args[1], args[2]);
		break;
	case SYSCALL_READ:
		res = syscall_read(args[0], (char *)args[1], args[2]);
		break;
	case SYSCALL_GETERRNO:
		res = syscall_geterrno();
		break;
	case SYSCALL_BRK:
		res = syscall_brk((void *)args[0]);
		break;
	case SYSCALL_SEND:
		res = syscall_send(args[0], args[1], (void *)args[2], args[3]);
		break;
	case SYSCALL_RECEIVE:
		res = syscall_receive((message *) args[0], args[1]);
		break;
	case SYSCALL_CLOSE:
		res = syscall_close(args[0]);
		break;
	case SYSCALL_PIPE:
		res = syscall_pipe((int *)args[0]);
		break;
	case SYSCALL_DUP2:
		res = syscall_dup2(args[0], args[1]);
		break;
	case SYSCALL_FORK:
		res = syscall_fork(r);
		break;
	case SYSCALL_EXECVE:
		res = syscall_execve((char *)args[0], (char *const *)args[1],
				     (char *const *)args[2], r);
		break;
	case SYSCALL_WAITPID:
		res = syscall_waitpid(args[0], (int *)args[1], args[2]);
		break;
	case SYSCALL_STAT:
		res = syscall_stat((char *)args[0], (struct stat *)args[1]);
		break;
	case SYSCALL_OPEN:
		res = syscall_open((char *)args[0], args[1]);
		break;
	case SYSCALL_GETDENT:
		res = syscall_getdent(args[0], (struct dirent *)args[1]);
		break;
	case SYSCALL_CHDIR:
		res = syscall_chdir((char *)args[0]);
		break;
	case SYSCALL_GETCWD:
		res = (int)syscall_getcwd((char *)args[0], args[1]);
		break;
	case SYSCALL_KILL:
		res = syscall_kill(args[0]);
		break;
	case SYSCALL_HALT:
		syscall_halt();
		break;
	default:
		kprintf("Warning: Call to unimplemented system call %d\n",
			call_no);
		res = -ENOSYS;
		break;
	}

	/*
	 * Store the errno value, in case the process subsequently calls geterrno() 
	 */
	if ((SYSCALL_GETERRNO != call_no) && (-ESUSPEND != res) &&
	    (NULL != current_process)) {
		if (res < 0) {
			current_process->last_errno = -res;
			res = -1;
		} else {
			current_process->last_errno = 0;
		}
	}

	/*
	 * -ESUSPEND means the process was suspended or killed. This means we can't
	 * just switch back to it directly, since the process is not in the ready
	 * queue. Instead, we perform a context switch, which will go to another
	 * running process, or an idle loop if there are no other processes ready. 
	 */
	if (-ESUSPEND == res) {
		context_switch(r);
	} else {
		/*
		 * System call has completed 
		 */
		old_current->in_syscall = 0;
		r->eax = res;

		/*
		 * We might have changed to a different process; if this is the case,
		 * perform a context switch 
		 */
		if (old_current != current_process)
			context_switch(r);
	}
}
