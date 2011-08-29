/*
 *      cat.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */
#include "constants.h"
#include "user.h"

int main(int argc, char **argv)
{
	if (2 > argc) {
		printf("Usage: cat <filename>\n");
		return -1;
	}

	char *path = argv[1];
	int fd;
	if (0 > (fd = open(path, 0))) {
		perror(argv[1]);
		return -1;
	} else {
		int bsize = 20;
		char buf[bsize];
		int r;
		while (0 < (r = read(fd, buf, bsize))) {
			write(STDOUT_FILENO, buf, r);
		}
		if (0 > r) {
			perror("read");
			return -1;
		}
		close(fd);
	}
	return 0;
}
