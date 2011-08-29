/*
 *      pipe.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include "kernel.h"

extern process *current_process;
extern process processes[MAX_PROCESSES];

/*
 * new_pipe
 * 
 * Create a new pipe object. This represents a pipe that is shared between both
 * the reading file handle and the writing file handle.
 */
pipe_buffer *new_pipe(void)
{
	pipe_buffer *b = kmalloc(sizeof(pipe_buffer));
	b->reading = 1;
	b->writing = 1;
	b->readpid = -1;
	b->alloc = BUFSIZE;
	b->len = 0;
	b->data = kmalloc(BUFSIZE);
	return b;
}

/*
 * wake_up_reader
 * 
 * Wakes up a suspended process that was blocked on a read call that referenced
 * this pipe. This is called whenever some data is written to the pipe, or the pipe
 * is closed for writing. Once the process is resumed, the read system call will be
 * executed again, and either return some data to the calling process or indicate
 * end-of-file.
 */
static void wake_up_reader(pipe_buffer * p)
{
	if (0 <= p->readpid) {
		resume_process(&processes[p->readpid]);
		p->readpid = -1;
	}
}

/*
 * write_to_pipe
 * 
 * Add some data to a pipe. If another process was blocked on a read call to this
 * pipe, it will subsequently be woken up and given the data it requested.
 * 
 * In our implementation, there is no limit on the amount of data that can be
 * stored in the pipe at any given time, though most operating systems do actually
 * place a limit on this, and force the writer to block until enough space is
 * available.
 * 
 * The reason for defining write_to_pipe as a separate function to
 * pipe_writer_write is so that it can be called from other parts of the kernel in
 * situations where the kernel itself is writing to the pipe, instead of another
 * process. An example of this can be seen in keyboard_handler, which writes
 * charcters entered by the user to a pipe that is connected to standard input of
 * the first process.
 */
void write_to_pipe(pipe_buffer * b, const void *buf, size_t count)
{
	if (b->len + count > b->alloc) {
		/*
		 * More data needs to be written that we have room for; resize the array 
		 */
		while (b->len + count > b->alloc)
			b->alloc *= 2;
		/*
		 * This would be slightly simpler if we had realloc(), but we don't 
		 */
		char *newdata = kmalloc(b->alloc);
		memmove(newdata, b->data, b->len);
		kfree(b->data);
		b->data = newdata;
	}

	/*
	 * Add the data to the pipe 
	 */
	memmove(&b->data[b->len], buf, count);
	b->len += count;

	/*
	 * If there was another process blocked on a read call for this pipe, wake
	 * it up 
	 */
	wake_up_reader(b);
}

/*
 * check_buffer_free
 * 
 * Free a pipe_buffer object if possible. This can only be done if it has been
 * closed for both reading *and* writing. This function is called whenever either
 * end of the pipe is closed.
 */
static void check_buffer_free(pipe_buffer * b)
{
	if (!b->reading && !b->writing) {
		assert(-1 == b->readpid);
		kfree(b->data);
		kfree(b);
	}
}

/*
 * pipe_writer_write
 * 
 * This function is used as the write handler for file handles of type
 * FH_PIPE_WRITER, and is invoked from the write system call. All this needs to do
 * is to call through to the write_to_pipe function to add the data.
 */
static ssize_t pipe_writer_write(filehandle * fh, const void *buf, size_t count)
{
	write_to_pipe(fh->p, buf, count);
	return count;
}

/*
 * pipe_writer_read
 * 
 * This function is used as the read handler for file handles of type
 * FH_PIPE_WRITER, and is invoked from the read system call.. Since these types
 * of file handles are write-only, we simply return an error code here indicating
 * that an invalid operation has been requested on the file handle.
 */
static ssize_t pipe_writer_read(filehandle * fh, void *buf, size_t count)
{
	return -EINVAL;
}

/*
 * pipe_writer_destroy
 * 
 * Destroys a pipe writer file handle. This will also destroy the pipe itself, if
 * it is no longer being read from.
 * 
 * If there is another process blocked on a read call for the pipe at the time
 * that this function is called, then it will be resumed so that the read call can
 * try again. If the pipe was empty, then the read call will immediately see that
 * it is no longer being written to, and return 0 to the process. If there was data
 * left in the pipe, the reading process will still be able to get that data, but
 * of course no more will be placed in the buffer and it will get a 0 on a later
 * call to read.
 */
static void pipe_writer_destroy(filehandle * fh)
{
	assert(fh->p->writing);
	fh->p->writing = 0;
	wake_up_reader(fh->p);
	check_buffer_free(fh->p);
	kfree(fh);
}

/*
 * new_pipe_writer
 * 
 * Creates a new file handle that can be used for writing to a pipe. This sets up
 * function pointers within the filehandle object that refer to the pipe_writer_*
 * functions defined above. These functions allow writing to the pipe, but not
 * reading from it.
 * 
 * Once this file handle has been created, it can be added to a process's file
 * descriptor table. This enables the process to write to the pipe by invoking the
 * write system call, and passing in the file descriptor that refers to this
 * handle. The process may *not* read from the pipe using this file handle, since
 * this is intended for write-only access.
 * 
 * For each pipe, there must be only a single file handle used for writing to it.
 * However, the file handle may be referenced from multiple file descriptors in
 * different processes.
 */
filehandle *new_pipe_writer(pipe_buffer * p)
{
	filehandle *fh = (filehandle *) kmalloc(sizeof(filehandle));
	fh->type = FH_PIPE_WRITER;
	fh->p = p;
	fh->refcount = 1;
	fh->write = pipe_writer_write;
	fh->read = pipe_writer_read;
	fh->destroy = pipe_writer_destroy;
	return fh;
}

/*
 * pipe_reader_write
 * 
 * This function is used as the write handler for file handles of type
 * FH_PIPE_READER, and is invoked from the write system call.
 * 
 * Since these types of file handles are read-only, we simply return an error code
 * here indicating that an invalid operation has been requested on the file handle.
 */
static ssize_t pipe_reader_write(filehandle * fh, const void *buf, size_t count)
{
	return -EINVAL;
}

/*
 * pipe_reader_read
 * 
 * This function is used as the read handler for file handles of type
 * FH_PIPE_READER, and is invoked from the read system call.
 * 
 * If there is data available in the pipe, then we can just return this to the
 * process and return immediately. An immedaite return is also possible if the pipe
 * is empty, but it is no longer being written to; this situation corresponds to
 * the end of file.
 * 
 * If neither of these two situations arise, then we must block here, since the
 * system call can't actually complete until either some more data becomes
 * available, or the pipe is closed for writing. In this case, we suspend the
 * current process. When the pipe is later written to or closed for writing, the
 * wake_up_reader function will resume the process, causing this function to be
 * called again. The second time round, either one of the first two cases will
 * match, and the system call can then return.
 */
static ssize_t pipe_reader_read(filehandle * fh, void *buf, size_t count)
{
	/*
	 * Don't allow multiple processes to read from the same pipe at the same
	 * time 
	 */
	if (-1 != fh->p->readpid)
		return -EBADF;

	if (0 < fh->p->len) {
		/*
		 * Data available - can return immediately 
		 */
		int copy = (count < fh->p->len) ? count : fh->p->len;
		memmove(buf, fh->p->data, copy);
		if (fh->p->len > copy)
			memmove(&fh->p->data[0], &fh->p->data[copy],
				fh->p->len - copy);
		fh->p->len -= copy;
		return copy;
	} else if (!fh->p->writing) {
		/*
		 * Pipe has been closed for writing - return end-of-file indicator 
		 */
		return 0;
	} else {
		/*
		 * No more data available yet - suspend process 
		 */
		fh->p->readpid = current_process->pid;
		suspend_process(current_process);
		return -ESUSPEND;
	}
}

/*
 * pipe_reader_destroy
 * 
 * Destroys a pipe reader file handle. This will also destroy the pipe itself, if
 * it is no longer being written to.
 */
static void pipe_reader_destroy(filehandle * fh)
{
	assert(fh->p->reading);
	fh->p->reading = 0;
	check_buffer_free(fh->p);
	kfree(fh);
}

/*
 * new_pipe_reader
 * 
 * Creates a file handle which can be used to read from a pipe. When this is added
 * to a process's file descriptor table, the process may read from the pipe by
 * invoking the read system call, and passing in the appropriate file descriptor.
 * 
 * For each pipe, there must be only a single file handle used for reading from it.
 * This may be referenced from multiple file descriptors though.
 */
filehandle *new_pipe_reader(pipe_buffer * p)
{
	filehandle *fh = (filehandle *) kmalloc(sizeof(filehandle));
	fh->type = FH_PIPE_READER;
	fh->p = p;
	fh->refcount = 1;
	fh->write = pipe_reader_write;
	fh->read = pipe_reader_read;
	fh->destroy = pipe_reader_destroy;
	return fh;
}

/*
 * syscall_pipe
 * 
 * Implements the pipe system call. This creates a pipe, as well as two file
 * handles. The first file handle can be used for reading from the pipe, and the
 * second for writing to it. Both handles are then associated with file descriptors
 * in the current process's file descriptor table.
 * 
 * The most common use of this function is when a process is about to fork. A pipe
 * is created to allow the parent and child process to communicate. When the fork
 * occurs, the child inherits references to the pipe, and both sides release either
 * the reading or writing file descriptor, depending on the direction in which the
 * data is to flow. The pipe can then be used to transfer data from the parent to
 * the child, or vice-versa.
 */
int syscall_pipe(int filedes[2])
{
	/*
	 * Ensure the supplied array is within the process's address space 
	 */
	if (!valid_pointer(filedes, 2 * sizeof(int)))
		return -EFAULT;

	/*
	 * Find two unused file descriptors 
	 */
	int readfd = -1;
	int writefd = -1;
	int i;
	for (i = 0; (i < MAX_FDS) && (-1 == writefd); i++) {
		if (NULL == current_process->filedesc[i]) {
			if (-1 == readfd)
				readfd = i;
			else if (-1 == writefd)
				writefd = i;
		}
	}

	/*
	 * If we were unable to allocate the file descriptors, the call cannot
	 * complete successfully. Return an error to the process indicating that there
	 * aren't enough file descriptors available. 
	 */
	if ((-1 == readfd) || (-1 == writefd))
		return -EMFILE;

	/*
	 * Create a new pipe 
	 */
	pipe_buffer *b = new_pipe();

	/*
	 * Create the file handles and place references to them in the process's
	 * file descriptor table 
	 */
	current_process->filedesc[readfd] = new_pipe_reader(b);
	current_process->filedesc[writefd] = new_pipe_writer(b);

	/*
	 * Store the file descriptors in the output array, so the process knows which
	 * ones to use for reading and writing to the pipe 
	 */
	filedes[0] = readfd;
	filedes[1] = writefd;

	return 0;
}
