/*      
 * 		daemon.c - simple example of how to daemonize a process.
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 *      
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <user.h>

typedef struct init {
	pid_t pid;
	char **programs;
} init_t;

int daemon_init(void)
{
	/* Fork off the parent process */
	pid_t pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		printf("child pid: %d\n", getpid());
	}
	if (pid > 0) {
		printf("parent pid: %d\n", getpid());
		exit(EXIT_SUCCESS);
	}
	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
#ifndef DEBUG
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
#endif

	return pid;
}

int main(int argc, char **argv)
{

	pid_t init = daemon_init();

	/* The Big Loop */
	while (1) {		/* Do some task here ... */
	}
	exit(EXIT_FAILURE);
}
