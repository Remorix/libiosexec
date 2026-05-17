#include <stdio.h>
#include <string.h>

#include <libiosexec.h>

int
main(void)
{
	char *shell;

	ie_setusershell();

	shell = ie_getusershell();
	if (shell == NULL || strcmp(shell, "/var/jb/bin/sh") != 0)
		return 1;

	shell = ie_getusershell();
	if (shell == NULL || strcmp(shell, "/var/jb/bin/csh") != 0)
		return 2;

	shell = ie_getusershell();
	if (shell == NULL || strcmp(shell, "/var/jb/bin/zsh") != 0)
		return 3;

	if (ie_getusershell() != NULL)
		return 4;

	ie_endusershell();
	return 0;
}
