/*
 *      kill.c
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 */

#include <user.h>

int main(int argc, char **argv)
{
	int i;

	if (argc < 2) {
		puts("Usage: kill [pid] ...");
		exit(1);
	}

	for (i = 1; i <= argc; i++) {
		if (-1 == kill(atoi(argv[i]))) {
			perror("kill");
			exit(1);
		}
	}
	exit(0);
}
