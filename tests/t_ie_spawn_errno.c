#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>

#include <libiosexec.h>

extern char **environ;

int
main(int argc, char **argv)
{
	char *args[2];
	pid_t pid;
	int err;

	if (argc != 2)
		return 64;

	args[0] = argv[1];
	args[1] = NULL;

	err = ie_posix_spawn(&pid, argv[1], NULL, NULL, args, environ);
	printf("%d %s\n", err, strerror(err));
	return err == EACCES ? 0 : 1;
}
