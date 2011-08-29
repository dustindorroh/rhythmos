/*
 *      pwd.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include "constants.h"
#include "user.h"

int main(int argc, char **argv)
{
	char path[PATH_MAX];
	if (NULL == getcwd(path, PATH_MAX))
		perror("getcwd");
	else
		printf("%s\n", path);

	return 0;
}
