/*
 *      echo.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

//#include <stdio.h>
#include "constants.h"
#include "user.h"

int main(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		printf("%s", argv[i]);
		printf(" ");
	}
	printf("\n");
	exit(0);
}
