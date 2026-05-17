#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

bool
is_executable_file(const char *path)
{
	struct stat st;

	if (stat(path, &st) == -1)
		return false;
	if (!S_ISREG(st.st_mode))
		return false;
	return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
}

bool
is_shell_script(const char *path)
{
	int fd = 0;
	ssize_t sz = 0;
	char buf[2] = {0};

	if (access(path, R_OK) == -1)
		return false;

	if (!is_executable_file(path))
		return false;

	if ((fd = open(path, O_RDONLY)) == -1)
		return false;

	if ((sz = read(fd, buf, 2)) != 2) {
		close(fd);
		return false;
	}

	if (buf[0] == '#' && buf[1] == '!') {
		close(fd);
		return true;
	}

	close(fd);
	return false;
}
