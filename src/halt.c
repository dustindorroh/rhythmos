/*
 *      halt.c
 *      
 *      Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
 */



#include <user.h>

int main(int argc, char ** argv)
{
	puts("Halting...");
	halt();
	exit(0);
}
