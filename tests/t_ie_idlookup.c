#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include <libiosexec.h>

int main(void)
{
	struct passwd *pw;
	struct group *gr;

	pw = ie_getpwnam("root");
	if (pw == NULL) {
		fprintf(stderr, "getpwnam root failed: %s\n", strerror(errno));
		return 1;
	}
	if (pw->pw_uid != 0 || strcmp(pw->pw_shell, "/var/jb/bin/sh") != 0)
		return 2;

	pw = ie_getpwuid(501);
	if (pw == NULL || strcmp(pw->pw_name, "mobile") != 0)
		return 3;

	gr = ie_getgrnam("nobody");
	if (gr == NULL || gr->gr_gid != (gid_t)-2)
		return 4;

	gr = ie_getgrgid((gid_t)-1);
	if (gr == NULL || strcmp(gr->gr_name, "nogroup") != 0)
		return 5;

	return 0;
}
