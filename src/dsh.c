/*
 *      dsh.c
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 */

#include "constants.h"
#include "user.h"
#include "keyboard.h"

#define MAX_ARGS 32

int parse_command(char *line, char **argv, int max)
{
	int len = strlen(line);
	int pos;
	int argc = 0;
	int start = 0;

	/* Scan through the input to extract space-separated tokens  */
	for (pos = 0; pos <= len; pos++) {
		if (('\n' == line[pos]) || (' ' == line[pos]) || (pos == len)) {
			/*
			 * We found a token; add it to the argv array 
			 */
			line[pos] = '\0';
			if ((pos > start) && (argc + 1 < max))
				argv[argc++] = &line[start];
			start = pos + 1;
		}
	}

	return argc;
}

void run_program(const char *filename, char **argv)
{
	/* Fork off a child process  */
	pid_t pid = fork();
	if (0 > pid) {
		perror("fork");
		return;
	} else if (0 == pid) {
		/* in child process */
		execve(filename, argv, NULL);
		perror("execve");
		exit(1);
	} else {
		/* in parent process */
		if (0 > waitpid(pid, NULL, 0))
			perror("waitpid");
	}
}

void process_line(char *line, int *done)
{
	int pos;
	int tpos = 0;
	int len = strlen(line);
	char * tline = (char *)malloc(BUFSIZE);
	char *argv[MAX_ARGS + 1];
	
	// = malloc(rlen * sizeof(char));

	for (pos = 0; pos <= len; pos++) {
		if((BACKSPACE == line[pos+1]) && (BACKSPACE == line[pos+2])) {
			pos--;
			pos--;
			pos--;
			tline[tpos] = line[pos];
		} else {
			tline[tpos] = line[pos];
		}
		tpos++;
	}

	memset(line, 0, len * sizeof(char));
	memmove(line,tline,tpos);
	
	tline = (char *)malloc(BUFSIZE);
	len = strlen(line);
	tpos=0;
	
	for (pos = 0; pos <= len; pos++) {
		if(BACKSPACE == line[pos+1]) {
			pos++;
			pos++;
			tline[tpos] = line[pos];
		} else {
			tline[tpos] = line[pos];
		}
		tpos++;
	}

	memset(line, 0, len * sizeof(char));
	memmove(line,tline,tpos);

	
	/* Extract the space-separated tokens from the command line  */
	int argc = parse_command(line, argv, MAX_ARGS);
	argv[argc] = NULL;	/* so we can pass it to execve() */

	if (0 == argc)
		return;

	/* Check which command was requested and take appropriate action  */
	if (!strcmp(argv[0], "cd")) {
		/* Change directory  */
		if (2 > argc)
			printf("Usage: cd <path>\n");
		else if (0 > chdir(argv[1]))
			perror(argv[1]);
	} else if (!strcmp(argv[0], "pwwd")) {
		/* Print current working directory */
		char path[PATH_MAX];
		if (NULL == getcwd(path, PATH_MAX))
			perror("getcwd");
		else
			printf("%s\n", path);
	} else if (!strcmp(argv[0], "exit")) {
		/* Exit shell */
		*done = 1;
	} else {
		/* Parse for shell variables 
		 * (ie tokens that start with `$') */
		int i;
		for(i = 0; i < argc; i++) {
			if (argv[i][0] == '$') {
				printf("SHELL VARIABLE DETECTED: %s",argv[i]);
				if (argv[i][1] == '$') {
					printf("%d", getpid());
				}
			}
		}
		/* Run a program */
		char cmdpath[PATH_MAX];
		struct stat statbuf;
		snprintf(cmdpath, PATH_MAX, "/bin/%s", argv[0]);
		if (0 == stat(cmdpath, &statbuf)) {
			run_program(cmdpath, argv);
			return;
		}
		printf("%s: command not found\n", argv[0]);
	}
	free(tline);
}

void prompt(char * username, char * hostname)
{
	/* Print current working directory */
	char path[PATH_MAX];
	if (NULL == getcwd(path, PATH_MAX)) {
		perror("getcwd");
		puts("$ ");
	} else if (username != NULL && hostname != NULL) {
		printf("%s@%s:%s $ ",username,hostname, path);
	} else {
		printf("rmos:%s$ ", path);
	}
}

int main(int argc, char **argv)
{
	char input[2 * BUFSIZE + 1];
	int pos = 0;
	int linestart = 0;
	printf("Dubug Shell started, pid = %d\n", getpid());
	prompt(NULL,NULL);
	int r;
	int done = 0;

	/* Read some text from standard input */
	while (!done && (0 < (r = read(STDIN_FILENO, &input[pos], BUFSIZE)))) {
		input[pos + r] = '\0';
		int end = pos + r;

		/* Echo the output back to the screen */
		write(STDOUT_FILENO, &input[pos], r);

		/*
		 * Scan through the input data to find \n characters, indicating
		 * end-of-line. For each complete line, we call the process_line function
		 * to extract the command name and arguments 
		 */
		while (pos < end) {
			if ('\n' == input[pos]) {
				input[pos] = '\0';
				process_line(&input[linestart], &done);
				
				prompt(NULL,NULL);
				linestart = pos + 1;
			}
			pos++;
		}

		/*
		 * If there's any remaining data in the buffer that does not yet constitute
		 * a complete line, then keep it, but shift it up to the beginning of the
		 * buffer. Data read on the next iteration will be placed after this. 
		 */
		memmove(&input[0], &input[linestart], pos - linestart);
		pos -= linestart;
		linestart = 0;

		/* Since we have a fixed size buffer, ignore any lines that won't fit  */
		if (pos >= BUFSIZE) {
			printf("Line too long; ignored");
			pos = 0;
		}
	}
	if (0 > r)
		perror("read");

	return 0;
}
