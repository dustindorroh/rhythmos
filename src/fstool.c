/*
 *      fstool.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include <constants.h>
#include <filesystem.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
	unsigned int alloc;
	unsigned int size;
	char *data;
} output;

#define OUTPUT_INIT { alloc: 1, size: 0, data: (char*)malloc(1) };
#define BUFSIZE 1024

void kprintf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

int read_file(const char *filename, char **data)
{
	int fd = open(filename, O_RDONLY);
	if (0 > fd) {
		perror(filename);
		exit(1);
	}
	int allocated = BUFSIZE;
	int size = 0;
	*data = (char *)malloc(allocated);
	int r;
	while (0 < (r = read(fd, &(*data)[size], BUFSIZE))) {
		size += r;
		allocated += r;
		*data = (char *)realloc(*data, allocated);
	}
	return size;
}

void output_append(output * b, const void *data, unsigned int size)
{
	if (b->size + size > b->alloc) {
		while (b->size + size > b->alloc)
			b->alloc *= 2;
		b->data = (char *)realloc(b->data, b->alloc);
	}
	memmove(&b->data[b->size], data, size);
	b->size += size;
}

void process_file(output * out, const char *path)
{
	int fd = open(path, O_RDONLY);
	if (0 > fd) {
		perror(path);
		exit(-1);
	}
	int r;
	char tmp[BUFSIZE];
	while (0 < (r = read(fd, tmp, BUFSIZE))) {
		output_append(out, tmp, r);
	}
	close(fd);
}

int ignore_file(const char *name)
{
	return (!strcmp(name, ".") ||
		!strcmp(name, "..") || !strcmp(name, ".svn"));
}

void process_dir(output * out, const char *path)
{
	DIR *dir;
	if (NULL == (dir = opendir(path))) {
		perror(path);
		exit(-1);
	}

	unsigned int alloc = sizeof(directory);
	directory *newdirect = (directory *) malloc(alloc);
	newdirect->count = 0;

	struct dirent *entry;
	while (NULL != (entry = readdir(dir))) {
		if (ignore_file(entry->d_name))
			continue;

		char fullpath[PATH_MAX];
		snprintf(fullpath, PATH_MAX, "%s/%s", path, entry->d_name);
		struct stat statbuf;
		if (0 > stat(fullpath, &statbuf)) {
			perror(fullpath);
			exit(-1);
		}

		if (S_ISDIR(statbuf.st_mode) || S_ISREG(statbuf.st_mode)) {
			alloc += sizeof(directory_entry);
			newdirect = (directory *) realloc(newdirect, alloc);
			directory_entry *ent =
			    &newdirect->entries[newdirect->count++];
			ent->size = statbuf.st_size;
			ent->type =
			    S_ISDIR(statbuf.st_mode) ? TYPE_DIR : TYPE_FILE;
			ent->location = 0;	/* fill in later */
			ent->mode = statbuf.st_mode;
			ent->mtime = statbuf.st_mtime;
			strncpy(ent->name, entry->d_name, MAX_FILENAME_LEN);
		}
	}
	closedir(dir);

	unsigned int diroffset = out->size;
	output_append(out, newdirect, alloc);

	unsigned int i;
	for (i = 0; i < newdirect->count; i++) {
		char fullpath[PATH_MAX];
		snprintf(fullpath, PATH_MAX, "%s/%s", path,
			 newdirect->entries[i].name);
		printf("%s\n", fullpath);
		newdirect->entries[i].location = out->size;
		if (TYPE_DIR == newdirect->entries[i].type) {
			process_dir(out, fullpath);
		} else {
			process_file(out, fullpath);
		}
	}

	memcpy(&out->data[diroffset], newdirect, alloc);
	free(newdirect);

	/*
	 * free(out.data); 
	 */
}

void dump_dir(const char *path, const directory_entry * entry, const char *data)
{
	const directory *dir = (const directory *)(data + entry->location);
	unsigned int i;
	for (i = 0; i < dir->count; i++) {
		char fullpath[PATH_MAX];
		snprintf(fullpath, PATH_MAX, "%s/%s", path,
			 dir->entries[i].name);
		printf("%c %-8d %s\n",
		       (TYPE_DIR == dir->entries[i].type) ? 'D' : 'F',
		       dir->entries[i].size, fullpath);
		if (TYPE_DIR == dir->entries[i].type) {
			dump_dir(fullpath, &dir->entries[i], data);
		}
	}
}

int
getdir(const char *data, unsigned int size, const char *path,
       directory_entry ** out)
{
	int r;
	if (0 > (r = get_directory_entry(data, path, out)))
		return r;
	if (TYPE_DIR != (*out)->type)
		return -ENOTDIR;
	return 0;
}

int
getfile(const char *data, unsigned int size, const char *path,
	directory_entry ** out)
{
	int r;
	if (0 > (r = get_directory_entry(data, path, out)))
		return r;
	if (TYPE_FILE != (*out)->type)
		return -EISDIR;
	return 0;
}

void dump_image(const char *data, unsigned int size)
{
	dump_dir("", (const directory_entry *)data, data);
}

int parse_command(char *line, char **argv, int max)
{
	int len = strlen(line);
	int pos;
	int argc = 0;
	int start = 0;

	for (pos = 0; pos <= len; pos++) {
		if (('\n' == line[pos]) || (' ' == line[pos]) || (pos == len)) {
			line[pos] = '\0';
			if ((pos > start) && (argc + 1 < max))
				argv[argc++] = &line[start];
			start = pos + 1;
		}
	}

	return argc;
}

void shell(const char *data, unsigned int size)
{
	char cwd[PATH_MAX];
	char line[1024];

	snprintf(cwd, PATH_MAX, "/");

	printf("%s > ", cwd);
	while (NULL != fgets(line, 1024, stdin)) {
		char abspath[PATH_MAX];
		directory_entry *entry = NULL;
		int r;

		char *argv[8];
		int argc = parse_command(line, argv, 8);

		/*
		 * Command: ls [path] - list directory contents 
		 */
		if ((1 <= argc) && !strcmp(argv[0], "ls")) {

			/*
			 * Argument is optional - use the current directory as default 
			 */
			if (2 <= argc)
				relative_to_absolute(abspath, cwd, argv[1],
						     PATH_MAX);
			else
				snprintf(abspath, PATH_MAX, "%s", cwd);

			if (0 > (r = getdir(data, size, abspath, &entry))) {
				printf("%s\n", strerror(-r));
			} else {
				const directory *dir =
				    (const directory *)(data + entry->location);
				unsigned int i;
				for (i = 0; i < dir->count; i++) {
					printf("%c%c%c%c%c%c%c%c%c%c ",
					       TYPE_DIR ==
					       dir->entries[i].type ? 'd' : '-',
					       (dir->entries[i].
						mode & 0400) ? 'r' : '-',
					       (dir->entries[i].
						mode & 0200) ? 'w' : '-',
					       (dir->entries[i].
						mode & 0100) ? 'x' : '-',
					       (dir->entries[i].
						mode & 040) ? 'r' : '-',
					       (dir->entries[i].
						mode & 020) ? 'w' : '-',
					       (dir->entries[i].
						mode & 010) ? 'x' : '-',
					       (dir->entries[i].
						mode & 04) ? 'r' : '-',
					       (dir->entries[i].
						mode & 02) ? 'w' : '-',
					       (dir->entries[i].
						mode & 01) ? 'x' : '-');

					/*
					 * printf("%8d ",dir->entries[i].mtime); 
					 */

					if (TYPE_DIR == dir->entries[i].type)
						printf("DIR  %-8d %s/\n",
						       dir->entries[i].size,
						       dir->entries[i].name);
					else
						printf("FILE %-8d %s\n",
						       dir->entries[i].size,
						       dir->entries[i].name);
				}
			}
		}

		/*
		 * Command: cat <filename> - print the contents of a file 
		 */
		else if ((2 == argc) && !strcmp(argv[0], "cat")) {
			relative_to_absolute(abspath, cwd, argv[1], PATH_MAX);
			if (0 > (r = getfile(data, size, abspath, &entry))) {
				printf("%s\n", strerror(-r));
			} else {
				fwrite(data + entry->location, 1, entry->size,
				       stdout);
			}
		}

		/*
		 * Command: cd <path> - change current working directory 
		 */
		else if ((2 == argc) && !strcmp(argv[0], "cd")) {
			/*
			 * Make sure the directory exists 
			 */
			relative_to_absolute(abspath, cwd, argv[1], PATH_MAX);
			if (0 > (r = getdir(data, size, abspath, &entry))) {
				printf("%s\n", strerror(-r));
			} else {
				/*
				 * Do the appropriate path manipulation 
				 */
				snprintf(cwd, PATH_MAX, "%s", abspath);
			}
		}

		/*
		 * Command: pwd - print current working directory 
		 */
		else if ((1 == argc) && !strcmp(argv[0], "pwd")) {
			printf("%s\n", cwd);
		}

		/*
		 * Command: q - quit the shell 
		 */
		else if ((1 == argc) && !strcmp(argv[0], "q")) {
			return;
		}

		/*
		 * Command: help - display list of commands 
		 */
		else if ((1 == argc) && !strcmp(argv[0], "help")) {
			printf("Commands:\n");
			printf
			    ("ls [path]  - list directory contents (or . if no path)\n");
			printf("cat <path> - display contents of a file\n");
			printf
			    ("cd <path>  - change current working directory\n");
			printf
			    ("pwd        - print current working directory\n");
			printf("q          - quit fstool\n");
		}

		/*
		 * Unknown command or incorrect no. of arguments 
		 */
		else {
			printf("Invalid command\n");
		}
		printf("%s > ", cwd);
	}
}

int main(int argc, char **argv)
{
	char *action = argv[1];

	if ((4 == argc) && !strcmp(action, "-build")) {
		/*
		 * build 
		 */
		char *image = argv[2];
		char *dir = argv[3];

		output out = OUTPUT_INIT;
		directory_entry root;
		root.size = 0;
		root.type = TYPE_DIR;
		root.location = sizeof(root);
		snprintf(root.name, MAX_FILENAME_LEN, "ROOT");
		output_append(&out, &root, sizeof(root));
		process_dir(&out, dir);
		root.size = out.size;

		int fd = open(image, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if (0 > fd) {
			perror(image);
			exit(-1);
		}
		write(fd, out.data, out.size);
		close(fd);
		free(out.data);
	} else if ((3 == argc) && !strcmp(action, "-dump")) {
		char *image = argv[2];
		char *data = NULL;
		int size = read_file(image, &data);
		dump_image(data, size);
	} else if ((4 == argc) && (!strcmp(action, "-get"))) {
		char *image = argv[2];
		char *path = argv[3];
		char *data = NULL;
		read_file(image, &data);
		directory_entry *entry = NULL;
		int r = get_directory_entry(data, path, &entry);
		if (0 > r) {
			printf("Error: %s\n", strerror(-r));
		} else {
			printf("Type %d Size %d Location %d\n",
			       entry->type, entry->size, entry->location);
		}
	} else if ((3 == argc) && (!strcmp(action, "-shell"))) {
		char *image = argv[2];
		char *data = NULL;
		int size = read_file(image, &data);
		shell(data, size);
	} else {
		fprintf(stderr,
			"Usage: buildfs -build|-dump|-get <image> <dir>\n");
		exit(-1);
	}

	return 0;
}
