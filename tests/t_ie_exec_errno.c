#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libiosexec.h>

extern char **environ;

int main(int argc, char **argv)
{
	char *args[2];

	if (argc != 2)
		return 64;
	args[0] = argv[1];
	args[1] = NULL;
	(void)ie_execve(argv[1], args, environ);
	printf("%d %s\n", errno, strerror(errno));
	return 0;
}
