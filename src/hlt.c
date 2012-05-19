/* 
 *      hlt.c
 *      
 *      Copyright 2012 Dustin Dorroh <dustindorroh@gmail.com>
 */

#include <user.h>

int main(int argc, char **argv)
{
	puts("asm volatile (\"hlt\");");
	asm volatile ("hlt");
	exit(0);
}
