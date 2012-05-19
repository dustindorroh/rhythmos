/*
 *      main.c
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 */

#include <kernel.h>
#include <filesystem.h>
#include <keyboard.h>

screenchar *screen = (screenchar *) VIDEO_MEMORY;

unsigned int xpos = 0;
unsigned int ypos = 0;

unsigned int shift_pressed = 0;
unsigned int timer_ticks = 0;

char *filesystem;
pipe_buffer *input_pipe = NULL;
extern process processes[MAX_PROCESSES];
/*
 * scroll
 * 
 * Move lines 1-24 up by one line, and clear the last line.
 */
static void scroll()		// @TODO: Move to video.c
{
	unsigned int y;
	unsigned int x;

	/*
	 * Move all lines up one 
	 */
	for (y = 0; y < SCREEN_HEIGHT - 1; y++)
		for (x = 0; x < SCREEN_WIDTH; x++) {
			screen[y * SCREEN_WIDTH + x] =
			    screen[(y + 1) * SCREEN_WIDTH + x];
		}

	/*
	 * Clear the last line 
	 */
	for (x = 0; x < SCREEN_WIDTH; x++)
		screen[y * SCREEN_WIDTH + x].c = ' ';
	ypos--;
}

/*
 * write_to_screen
 * 
 * Prints a series of characters to the screen, by writing directly to
 * video memory. The position of the cursor is updated accordingly. The
 * screen is scrolled if the text goes beyond the end of the last line.
 */
void write_to_screen(const char *data, unsigned int count)	// @TODO: Move to video.c
{
	unsigned int i;
	for (i = 0; i < count; i++) {
		char c = data[i];

		if ('\n' == c) {
			/* newline */
			xpos = 0;
			ypos++;
		} else if ('\t' == c) {
			/* tab  */
			int add = 8 - (xpos % 8);
			xpos += add;
		} else if (c == BACKSPACE && xpos) {
			/* Handle a backspace, by moving the cursor back one space
			 * untill the cursor is against the edge */

			xpos--;	/* Back the cursor up then display if */
			screen[ypos * SCREEN_WIDTH + xpos].c = (int)NULL;

		} else {
			/* normal character */
			screen[ypos * SCREEN_WIDTH + xpos].c = c;
			xpos++;
		}
		if (xpos >= SCREEN_WIDTH) {
			xpos = 0;
			ypos++;
		}
		if (ypos >= SCREEN_HEIGHT) {
			scroll();
		}
	}

	move_cursor(xpos, ypos);
}

/*
 * timer_handler
 * 
 * This function is called every time a timer interrupt occurs, which
 * happens 50 times per second. You can implement any logic here that
 * needs to happen at regular intervals (e.g. a context switch to the
 * next process).
 */
void timer_handler(regs * r)
{
	timer_ticks++;

	context_switch(r);
}

/*
 * process_a
 * 
 * Simple test process. Just repeatedly prints out a string, reporting
 * the number of iterations done so far.
 */
void process_a()
{
	int iterations = 0;
	while (1) {
		int i;
		for (i = 0; i < 5000000; i++) ;
		printf("I am process A (pid %d), iterations = %d\n", getpid(),
		       iterations);
		iterations++;
	}
}

/*
 * process_b
 * 
 * Simple test process. Just repeatedly prints out a string, reporting
 * the number of iterations done so far.
 */
void process_b()
{
	int iterations = 0;
	while (1) {
		int i;
		for (i = 0; i < 5000000; i++) ;
		printf("I am process B (pid %d), iterations = %d\n", getpid(),
		       iterations);
		iterations++;
	}
}

/*
 * uppercase
 * 
 * A filter which converts all of the data passed to its standard input
 * (fd 0) to uppercase, and then sends the result to its standard output
 * (fd 1).
 */
void uppercase()
{
	while (1) {
		char buf[BUFSIZE + 1];
		int r;
		while (0 < (r = read(0, buf, BUFSIZE))) {
			int i;
			for (i = 0; i < r; i++) {
				if (('a' <= buf[i]) && ('z' >= buf[i]))
					buf[i] += 'A' - 'a';
			}
			write(STDOUT_FILENO, buf, r);
		}
	}
	exit(0);
}

/*
 * number_lines
 * 
 * A filter which prints the line number next to each line. This works
 * in a similar manner to uppercase; it reads data from standard input,
 * manipulates it, and then writes the modified data to standard output.
 */
void number_lines()
{
	while (1) {
		char buf[BUFSIZE + 1];
		int r;
		int lineno = 0;
		printf("%3d ", lineno);
		while (0 < (r = read(0, buf, BUFSIZE))) {
			int i;
			for (i = 0; i < r; i++) {
				write(STDOUT_FILENO, &buf[i], 1);
				if ('\n' == buf[i]) {
					lineno++;
					printf("%3d ", lineno);
				}
			}
		}
	}
	exit(0);
}

/*
 * launch_shell
 * 
 * The first process launched at kernel startup. All this does is simply
 * execute the shell. It is now the only process created via
 * start_process; everything else starts via fork.
 * 
 * The reason for using a string on the stack here is that execve checks
 * to see if the string is located within the process's address
 * space. Since all processes load their executable image from the
 * filesystem, the checks performed by valid_pointer have been changed
 * in this version of the kernel to forbid processes from passing in
 * pointers residing in the kernel's memory, which would be the case if
 * we passed in a pointer to a string that was compiled directly into
 * the kernel's image.
 */
void launch_shell()
{
	char program[100];
	snprintf(program, 100, "/bin/sh");
	execve(program, NULL, NULL);
	printf("/bin/sh: execve failed: %d\n", errno);
	exit(1);
}

/*
 * kmain
 * 
 * This is the first thing that executes when the kernel starts. Any
 * initialisation e.g. interrupt handlers must be done at the start of
 * the function. This function should never return.
 */
void kmain(multiboot * mb)
{
	setup_segmentation();
	setup_interrupts();
	kmalloc_init();

	/*
	 * Clear the screen 
	 */
	unsigned int x;
	unsigned int y;
	for (y = 0; y < SCREEN_HEIGHT; y++)
		for (x = 0; x < SCREEN_WIDTH; x++)
			screen[y * SCREEN_WIDTH + x].c = ' ';

	/*
	 * Place the cursor on line 0 to start with 
	 */
	move_cursor(xpos, ypos);

	kprintf("%s\n%s\n%s\n\n\n\n", VERSION, COPYRIGHT, DISCLAIMER);

	assert(1 == mb->mods_count);
	assert(mb->mods_addr[0].mod_end < 2 * MB);
	filesystem = (char *)mb->mods_addr[0].mod_start;
	/*
	 * Check here for the size of the RAM disk. Because we use a
	 * hard-coded value of 2MB for the start of the kernel's private
	 * data area, we can't safely work with filesystems that extend
	 * into this area. This is really just a hack to avoid the
	 * additional complexity of computing the right place to start
	 * the kernel and page memory regions, but suffices for our
	 * purposes.
	 */
	if (mb->mods_addr[0].mod_end >= 2 * MB)
		assert
		    (!"Filesystem goes beyond 2Mb limit. Please use smaller filesystem.");

	pid_t pid = start_process(launch_shell);
	input_pipe = processes[pid].filedesc[STDIN_FILENO]->p;

	/*
	 * Go in to user mode and enable interrupts
	 */
	enter_user_mode();

	/*
	 * Loop indenitely... we should never return from this function
	 */

	/*
	 * Pretty soon a context switch will occur, and the processor
	 * will jump out of this loop and start executing the first
	 * scheduled process
	 */
	while (1) ;
}
