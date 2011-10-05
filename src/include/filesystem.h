/*
 *      filesystem.h
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#define TYPE_DIR 1
#define TYPE_FILE 2

#define MAX_FILENAME_LEN     255

typedef struct directory_entry {
	unsigned int size;	/* size of file/directory in bytes */
	unsigned int type;	/* either TYPE_DIR or TYPE_FILE */
	unsigned int location;	/* offset from start of file system */
	unsigned int mode;	/* permission bits */
	unsigned int mtime;	/* modification time */
	char name[MAX_FILENAME_LEN + 1];	/* file or directory name */
} __attribute__ ((packed)) directory_entry;

typedef struct directory {
	unsigned int count;
	directory_entry entries[0];
} __attribute__ ((packed)) directory;

int get_directory_entry(const char *filesystem, const char *path,
			directory_entry ** entry);
void relative_to_absolute(char *abs, const char *base,
			  const char *rel, int max);

/*
 * Multiboot info 
 */

typedef struct {
	unsigned int mod_start;
	unsigned int mod_end;
	char *string;
	unsigned int reserved[3];
} __attribute__ ((__packed__)) module;

typedef struct {
	unsigned int flags;
	unsigned int mem_lower;
	unsigned int mem_upper;
	unsigned int boot_device;
	char *cmdline;
	unsigned int mods_count;
	module *mods_addr;
} __attribute__ ((__packed__)) multiboot;

#endif				/* FILESYSTEM_H */
