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
#define DEBUG
typedef struct init {
	pid_t pid;
	const char *program;
	int keep_alive;
} init_t;

int daemonize(void)
{
	/* Fork off the parent process */
	pid_t pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {;
	}
	if (pid > 0) {
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

void run_program(const char *filename, char **argv)
{
	/* Fork off a child process  */
	pid_t pid = fork();
	if (0 > pid) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (0 == pid) {	/* child process */
		execve(filename, argv, NULL);
		perror("execve");
		exit(EXIT_FAILURE);
	} else {		/* parent process */
		if (0 > waitpid(pid, NULL, 0))
			perror("waitpid");
	}
}

int main(int argc, char **argv)
{

	init_t daemon;

	daemon.pid = daemonize();
	daemon.program = "dsh";
	daemon.keep_alive = 1;

	/* The Big Loop */
	while (1) {
		run_program(daemon.program[0], NULL);
	}
	exit(EXIT_FAILURE);

}
