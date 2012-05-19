/**********************************************************************\
*                Copyright (C) Michael Kerrisk, 2010.                  *
*                                                                      *
* This program is free software. You may use, modify, and redistribute *
* it under the terms of the GNU Affero General Public License as       *
* published by the Free Software Foundation, either version 3 or (at   *
* your option) any later version. This program is distributed without  *
* any warranty. See the file COPYING for details.                      *
\**********************************************************************/

/* t_vfork.c

   Demonstrate the use of vfork() to create a child process.
*/
#include <user.h>

int main(int argc, char *argv[])
{
	int istack = 222;

	switch (vfork()) {
	case -1:
		perror("vfork");

	case 0:		/* Child executes first, in parent's memory space */
		//sleep(3);                   
		/* Even if we sleep for a while, parent still is not scheduled */
		istack *= 3;	/* This change will be seen by parent */
		printf("Child executing: istack=%d\n", istack);
		exit(EXIT_SUCCESS);

	default:		/* Parent is blocked until child exits */
		printf("Parent executing: istack=%d\n", istack);
		printf("istack=%d\n", istack);
		exit(EXIT_SUCCESS);
	}
	return 0;
}
