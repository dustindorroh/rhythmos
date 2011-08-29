/*
 *      filesystem.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "constants.h"
#ifdef USERLAND
#include <errno.h>
#include <string.h>
#include <stdio.h>
void kprintf(const char *format, ...);
#else
#include "kernel.h"
#endif
#include "filesystem.h"

/*
 * lookup
 * 
 * Scans through a directory object for the specified filename. If the file does
 * not exist, the function returns NULL.
 */
static directory_entry *lookup(directory * dir, const char *name, int len)
{
	unsigned int i;
	for (i = 0; i < dir->count; i++) {
		if ((strlen(dir->entries[i].name) == len) &&
		    !strncmp(dir->entries[i].name, name, len))
			return &dir->entries[i];
	}
	return NULL;
}

/*
 * get_directory_entry
 * 
 * Finds the directory_entry objects within a filesystem corresponding to the
 * specified path. The filesystem parameter should refere to the address in memory
 * at which a filesystem image begins. In the case of the kernel, this corresponds
 * to the RAM disk loaded by grub, the address of which is passed to main in the
 * multiboot structure. In the case of fstool, this is a buffer within memory into
 * which the image file has been read.
 * 
 * The specified path must be absolute, i.e. beginning with /. To obtain a
 * directory entry corresponding to a relative path, you must first convert it to
 * an absolute path using relative_to_absolute.
 * 
 * Upon success, this function returns 0, and stores a pointer to the
 * directory_entry object at the location specified by the entry parameter. On
 * failure, it returns a negative error code.
 */
int
get_directory_entry(const char *filesystem, const char *path,
		    directory_entry ** entry)
{
	*entry = (directory_entry *) filesystem;
	directory *dir = (directory *) (filesystem + (*entry)->location);
	unsigned int len = strlen(path);
	unsigned int start = 0;
	unsigned int pos;

	/*
	 * Scan through each of the /-separated components of the path name 
	 */
	for (pos = 0; pos <= len; pos++) {
		if (('/' == path[pos]) || (pos == len)) {
			if (pos > start) {
				if (NULL ==
				    (*entry =
				     lookup(dir, &path[start], pos - start))) {
					/*
					 * File not found 
					 */
					return -ENOENT;
				} else if (TYPE_DIR == (*entry)->type) {
					/*
					 * Assign the dir pointer to this directory, in case we need to
					 * traverse any more path components 
					 */
					dir =
					    (directory *) (filesystem +
							   (*entry)->location);
				} else if (pos < len) {
					/*
					 * It's a file, but there are more path components 
					 */
					return -ENOENT;
				}
				/*
				 * Otherwise we're ok, and will either return now or process the
				 * next component 
				 */
			}
			start = pos + 1;
		}
	}
	return 0;
}

/*
 * relative_to_absolute
 * 
 * Convert a relative path to an absolute path, according to some particular base
 * directory. For example, if the base path is /home/peter and the relative path
 * is ../cruz, then the resulting absolute path will be /home/cruz.
 * 
 * The abs parameter is an output parameter which must pointer to an array in which
 * the caller wishes the absolute path name to be stored, up to a maximum of max
 * bytes.
 */
void relative_to_absolute(char *abs, const char *base, const char *rel, int max)
{

	if ('/' == rel[0]) {
		/*
		 * Relative path is actually absolute; forget about base 
		 */
		snprintf(abs, max, "/");
	} else {
		/*
		 * Initialise our path as base 
		 */
		snprintf(abs, max, "%s", base);
	}

	/*
	 * Go through the relative path string and process each /-separated component.
	 * Each component can considered to be a "command" which instructs us what to
	 * do to the path string at that point. "." means current directory, so we
	 * just leave the path as is. ".." means remove the last component (if any)
	 * from the path string. Anything else means add a new component. 
	 */
	int rel_len = strlen(rel);
	int start = 0;
	int pos;
	int abs_len = strlen(abs);

	for (pos = 0; pos <= rel_len; pos++) {
		if (('/' == rel[pos]) || (pos == rel_len)) {
			if (pos > start) {
				int namelen = pos - start;
				const char *component = &rel[start];

				if ((1 == namelen)
				    && !strncmp(component, ".", namelen)) {
					/*
					 * no action 
					 */
				} else if ((2 == namelen)
					   && !strncmp(component, "..",
						       namelen)) {
					/*
					 * remove the last directory 
					 */
					while ((1 < abs_len)
					       && ('/' != abs[abs_len - 1]))
						abs[--abs_len] = '\0';
					/*
					 * remove the / at the end of the path, unless it's the root 
					 */
					if (1 < abs_len)
						abs[--abs_len] = '\0';
				} else {
					/*
					 * add the directory name 
					 */
					int rpos = start;
					if (((abs_len < max - 1))
					    && ('/' != abs[abs_len - 1]))
						abs[abs_len++] = '/';	/* extra / if not the root */
					while ((abs_len < max - 1)
					       && (rpos < pos))
						abs[abs_len++] = rel[rpos++];
					abs[abs_len] = '\0';
				}
			}
			/*
			 * Next component starts directly after this character 
			 */
			start = pos + 1;
		}
	}
}
