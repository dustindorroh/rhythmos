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

int main(int argc, char** argv)
{

	/* Our process ID */
	pid_t pid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		exit(1);
	}
	/* If we got a good PID, then
	   we can exit the parent process. */
	if (pid == 0) {
		printf("child pid: %d\n",getpid());
	}
	if (pid > 0) {
		//printf("child pid: %d\n",pid);
		printf("parent pid: %d\n",getpid());
		exit(0);
	}
	
	/* Change the current working directory */
	if ((chdir("/")) < 0) {
		/* Log the failure */
		exit(1);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* redirect fd's 0,1,2 to /dev/null */ 
	/** @TODO - Create a null device **/
	//open ("/dev/null", O_RDWR);     /* stdin */
	//dup (0);                        /* stdout */
	//dup (0);                        /* stderror */

	/* 
	 * Daemon-specific initialization goes here 
	 */
	/* The Big Loop */
	while (1) {	/* Do some task here ... */ }
	exit(0);
}
