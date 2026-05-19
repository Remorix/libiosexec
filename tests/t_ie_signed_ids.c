#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>

#include "libiosexec.h"

int
main(void)
{
	struct group *gr;
	struct passwd *pw;

	gr = ie_getgrnam("nobody");
	if (gr == NULL)
		errx(1, "ie_getgrnam(\"nobody\") returned NULL");
	if (gr->gr_gid != (gid_t)-2)
		errx(1, "nobody gid was %u", gr->gr_gid);

	gr = ie_getgrgid((gid_t)-1);
	if (gr == NULL)
		errx(1, "ie_getgrgid(-1) returned NULL");
	if (strcmp(gr->gr_name, "nogroup") != 0)
		errx(1, "gid -1 resolved to %s", gr->gr_name);

	pw = ie_getpwuid((uid_t)-2);
	if (pw == NULL)
		errx(1, "ie_getpwuid(-2) returned NULL");
	if (strcmp(pw->pw_name, "nobody") != 0)
		errx(1, "uid -2 resolved to %s", pw->pw_name);

	return 0;
}
