#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libiosexec.h>

extern char **environ;

int
main(int argc, char **argv)
{
	char *args[2];
	int err;

	if (argc != 2)
		return 64;

	args[0] = argv[1];
	args[1] = NULL;

	if (ie_execve(argv[1], args, environ) == 0)
		return 100;

	err = errno;
	printf("%d %s\n", err, strerror(err));
	return err == EACCES ? 0 : 1;
}
