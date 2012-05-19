/*
 *      ls.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include <user.h>

int main(int argc, char **argv)
{
	char *path;
	if (2 <= argc)
		path = argv[1];
	else
		path = ".";

	DIR *dir;
	if (NULL == (dir = opendir(path))) {
		perror(path);
		return -1;
	}

	struct dirent *entry;
	while (NULL != (entry = readdir(dir))) {
		char fullpath[PATH_MAX];
		snprintf(fullpath, PATH_MAX, "%s/%s", path, entry->d_name);
		struct stat statbuf;
		if (0 > stat(fullpath, &statbuf)) {
			perror(fullpath);
		} else {
			printf("%c%c%c%c%c%c%c%c%c%c ",
			       (statbuf.st_mode & S_IFDIR) ? 'd' : '-',
			       (statbuf.st_mode & S_IRUSR) ? 'r' : '-',
			       (statbuf.st_mode & S_IWUSR) ? 'w' : '-',
			       (statbuf.st_mode & S_IXUSR) ? 'x' : '-',
			       (statbuf.st_mode & S_IRGRP) ? 'r' : '-',
			       (statbuf.st_mode & S_IWGRP) ? 'w' : '-',
			       (statbuf.st_mode & S_IXGRP) ? 'x' : '-',
			       (statbuf.st_mode & S_IROTH) ? 'r' : '-',
			       (statbuf.st_mode & S_IWOTH) ? 'w' : '-',
			       (statbuf.st_mode & S_IXOTH) ? 'x' : '-');

			if (statbuf.st_mode & S_IFDIR)
				printf("DIR  %-8d %s/\n", statbuf.st_size,
				       entry->d_name);
			else
				printf("FILE %-8d %s\n", statbuf.st_size,
				       entry->d_name);
		}
	}

	closedir(dir);

	return 0;
}
