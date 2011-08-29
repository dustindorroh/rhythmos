/*
 *      filedesc.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "kernel.h"

extern process *current_process;
extern process processes[MAX_PROCESSES];

/*
 * screen_write
 * 
 * This function is used as the write handler for file handles of type
 * FH_SCREEN. It replaces the old logic in syscall_write; it simply calls through
 * to write_to_screen.
 */
static ssize_t screen_write(filehandle * fh, const void *buf, size_t count)
{
	write_to_screen(buf, count);
	return count;
}

/*
 * screen_read
 * 
 * This function is used as the read handler for file handles of type FH_SCREEN.
 * Since it doesn't make sense to read from the screen, we simply return an error
 * code here indicating that an invalid operation has been requested.
 */
static ssize_t screen_read(filehandle * fh, void *buf, size_t count)
{
	return -EINVAL;
}

/*
 * screen_destroy
 * 
 * Frees the memory associated with a screen file handle. Called when all file
 * descriptors referring to the handle are closed, i.e. when the reference count
 * becomes 0.
 */
static void screen_destroy(filehandle * fh)
{
	kfree(fh);
}

/*
 * new_screen_handle
 * 
 * Creates a file handle that writes to the screen. This is necessary for processes
 * to make their output visible, and is the standard file handle that is installed
 * by default on the standard output and error streams for a new process. This
 * replaces the previous logic in syscall_write which called write_to_screen
 * directly, since the system call is now capable of writing to different types of
 * output, e.g. pipes.
 * 
 * The fields of the filehandle object are set up to contain pointers to the
 * screen_* functions defined above. The read, write, and close system calls
 * dispatch to these as appropriate.
 */
filehandle *new_screen_handle(void)
{
	filehandle *fh = (filehandle *) kmalloc(sizeof(filehandle));
	fh->type = FH_SCREEN;
	fh->refcount = 1;
	fh->write = screen_write;
	fh->read = screen_read;
	fh->destroy = screen_destroy;
	return fh;
}

/*
 * close_filehandle
 * 
 * Releases a reference to a file handle, possibly destroying it. The reason why
 * it is not destroyed immediately is that there could be multiple file descriptors
 * (possibly in different processes) referring to the same file handle. It is thus
 * only safe to destroy it once all of them have been released.
 */
void close_filehandle(filehandle * fh)
{
	assert(1 <= fh->refcount);
	fh->refcount--;
	if (0 == fh->refcount)
		fh->destroy(fh);
}

/*
 * syscall_close
 * 
 * Close a file descriptor. Does not necessarily get rid of the associated file
 * handle immediately, since there may be other file descriptors referring to the
 * same handle.
 */
int syscall_close(int fd)
{
	if ((0 > fd) || (MAX_FDS <= fd)
	    || (NULL == current_process->filedesc[fd]))
		return -EBADF;

	close_filehandle(current_process->filedesc[fd]);
	current_process->filedesc[fd] = NULL;
	return 0;
}

/*
 * syscall_dup2
 * 
 * "Copies" a file descriptor by making another reference to the same file handle,
 * but from a different file descriptor. Because both file descriptors return to
 * the same handle, any calls to functions like read or write will maintain the
 * shared state, e.g. file position.
 * 
 * A common use of this function is to replace the standard input/output/error
 * streams with other file handles (e.g. pipes) that a process has created, e.g.
 * when doing I/O redirection in a shell.
 */
int syscall_dup2(int oldfd, int newfd)
{
	if ((0 > oldfd) || (MAX_FDS <= oldfd))
		return -EBADF;
	if ((0 > newfd) || (MAX_FDS <= newfd))
		return -EBADF;

	filehandle *fh = current_process->filedesc[oldfd];
	if (NULL == fh)
		return -EBADF;

	if (oldfd == newfd)
		return newfd;

	if (NULL != current_process->filedesc[newfd])
		close_filehandle(current_process->filedesc[newfd]);
	current_process->filedesc[newfd] = fh;
	fh->refcount++;

	return newfd;
}
