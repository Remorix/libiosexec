#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>

#include <libiosexec.h>

int main(int argc, char **argv)
{
	pid_t pid;
	char *args[2];
	int err;

	if (argc != 2)
		return 64;
	args[0] = argv[1];
	args[1] = NULL;
	err = ie_posix_spawn(&pid, argv[1], NULL, NULL, args, NULL);
	printf("%d %s\n", err, strerror(err));
	return 0;
}
