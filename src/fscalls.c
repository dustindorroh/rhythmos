/*
 *      fscalls.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include "kernel.h"
#include "filesystem.h"

extern char *filesystem;
extern process *current_process;

/*
 * syscall_stat
 * 
 * Implementation of the stat system call, which obtains information about a
 * particular file or directory. You should use this as a guideline for how to
 * implement the other system calls in assignment 3.
 */
int syscall_stat(const char *path, struct stat *buf)
{
	/*
	 * Ensure the supplied path name and buffer are valid pointers 
	 */
	if (!valid_string(path))
		return -EFAULT;
	if (!valid_pointer(buf, sizeof(struct stat)))
		return -EFAULT;

	/*
	 * Get the directory_entry object for this path from the filesystem 
	 */
	directory_entry *entry;
	int r;

	char abs[PATH_MAX];
	relative_to_absolute(abs, current_process->cwd, path, PATH_MAX);
	if (0 != (r = get_directory_entry(filesystem, abs, &entry)))
		return r;

	/*
	 * Set all fields of the stat buffer 
	 */
	buf->st_mode = entry->mode;
	buf->st_uid = 0;
	buf->st_gid = 0;
	buf->st_size = entry->size;
	buf->st_mtime = entry->mtime;

	return 0;
}

static ssize_t file_write(filehandle * fh, const void *buf, size_t count)
{
	return -EBADF;
}

static ssize_t file_read(filehandle * fh, void *buf, size_t count)
{
	if (FH_DIR == fh->type) {
		return -EISDIR;
	} else {
		if (fh->pos >= fh->entry->size)
			return 0;
		if (fh->pos + count > fh->entry->size)
			count = fh->entry->size - fh->pos;
		memmove(buf, filesystem + fh->entry->location + fh->pos, count);
		fh->pos += count;
		return count;
	}
}

static void file_destroy(filehandle * fh)
{
	kfree(fh);
}

static filehandle *new_file(int type)
{
	filehandle *fh = (filehandle *) kmalloc(sizeof(filehandle));
	fh->type = type;
	fh->refcount = 1;
	fh->write = file_write;
	fh->read = file_read;
	fh->destroy = file_destroy;
	return fh;
}

int syscall_open(const char *pathname, int flags)
{
	if (!valid_string(pathname))
		return -EFAULT;

	int fd = -1;
	for (fd = 0; fd < MAX_FDS; fd++) {
		if (NULL == current_process->filedesc[fd])
			break;
	}
	if (MAX_FDS == fd)
		return -EMFILE;

	char abspath[PATH_MAX];
	relative_to_absolute(abspath, current_process->cwd, pathname, PATH_MAX);

	directory_entry *entry;
	int r;
	if (0 != (r = get_directory_entry(filesystem, abspath, &entry)))
		return r;

	if ((flags == OPEN_DIRECTORY) && (entry->type != TYPE_DIR))
		return -ENOTDIR;
	else if ((flags != OPEN_DIRECTORY) && (entry->type == TYPE_DIR))
		return -EISDIR;

	filehandle *fh;
	if (TYPE_DIR == entry->type)
		fh = new_file(FH_DIR);
	else
		fh = new_file(FH_FILE);
	fh->entry = entry;
	fh->pos = 0;
	fh->entryno = 0;
	current_process->filedesc[fd] = fh;
	return fd;
}

int syscall_getdent(int fd, struct dirent *entry)
{
	if (!valid_pointer(entry, sizeof(struct dirent)))
		return -EFAULT;

	if ((0 > fd) || (MAX_FDS <= fd)
	    || (NULL == current_process->filedesc[fd]))
		return -EBADF;

	filehandle *fh = current_process->filedesc[fd];
	if (FH_DIR == fh->type) {
		directory *dir =
		    (directory *) (filesystem + fh->entry->location);
		if (fh->entryno >= dir->count)
			return 0;
		entry->d_ino = 0;
		snprintf(entry->d_name, NAME_MAX + 1,
			 dir->entries[fh->entryno].name);
		fh->entryno++;
		return 1;
	} else {
		return -ENOTDIR;
	}
	return 0;
}

int syscall_chdir(const char *path)
{
	if (!valid_string(path))
		return -EFAULT;

	char newcwd[PATH_MAX];
	relative_to_absolute(newcwd, current_process->cwd, path, PATH_MAX);
	int r;
	directory_entry *entry;
	if (0 != (r = get_directory_entry(filesystem, newcwd, &entry)))
		return r;
	if (TYPE_DIR != entry->type)
		return -ENOTDIR;
	memmove(current_process->cwd, newcwd, PATH_MAX);
	return 0;
}

char *syscall_getcwd(char *buf, size_t size)
{
	if (!valid_pointer(buf, size))
		return NULL;
	snprintf(buf, size, "%s", current_process->cwd);
	return buf;
}
