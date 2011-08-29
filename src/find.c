/*
 *      find.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "constants.h"
#include "user.h"

int find(const char *path)
{
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
		printf("%s\n", fullpath);
		if (0 > stat(fullpath, &statbuf)) {
			perror(fullpath);
		} else {
			if (statbuf.st_mode & S_IFDIR) {
				if (0 != find(fullpath))
					return -1;
			}
		}
	}

	closedir(dir);
	return 0;
}

int main(int argc, char **argv)
{
	char *path;
	if (2 <= argc)
		path = argv[1];
	else
		path = ".";

	return find(path);
}
