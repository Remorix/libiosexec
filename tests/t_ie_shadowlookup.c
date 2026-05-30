#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include <libiosexec.h>

int
main(void)
{
	struct passwd *pw;

	pw = ie_getpwuid(0);
	if (pw == NULL) {
		fprintf(stderr, "getpwuid root failed: %s\n", strerror(errno));
		return 1;
	}
	if (strcmp(pw->pw_passwd, "*") != 0)
		return 2;
	if (strcmp(pw->pw_gecos, "System Administrator") != 0)
		return 3;
	return 0;
}
