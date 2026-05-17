#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>

#include "libiosexec.h"

typedef char *(*compat_getusershell_fn)(void);
typedef void (*compat_setusershell_fn)(void);
typedef void (*compat_endusershell_fn)(void);

#if LIBIOSEXEC_PREFIXED_ROOT == 1
#define LIBRECOMPAT_DYLIB_PATH "/var/jb/usr/lib/librecompat.0.dylib"
#else
#define LIBRECOMPAT_DYLIB_PATH "/usr/lib/librecompat.0.dylib"
#endif

static void *
open_librecompat_handle(void)
{
	void *handle;

	handle = dlopen(LIBRECOMPAT_DYLIB_PATH, RTLD_LAZY | RTLD_NOLOAD);
	if (handle != NULL)
		return handle;

	handle = dlopen(LIBRECOMPAT_DYLIB_PATH, RTLD_LAZY);
	if (handle != NULL)
		return handle;

	return NULL;
}

static compat_getusershell_fn
resolve_compat_getusershell(void)
{
	void *handle;

	handle = open_librecompat_handle();
	if (handle == NULL)
		return NULL;
	return (compat_getusershell_fn)dlsym(handle, "compat_getusershell");
}

static compat_setusershell_fn
resolve_compat_setusershell(void)
{
	void *handle;

	handle = open_librecompat_handle();
	if (handle == NULL)
		return NULL;
	return (compat_setusershell_fn)dlsym(handle, "compat_setusershell");
}

static compat_endusershell_fn
resolve_compat_endusershell(void)
{
	void *handle;

	handle = open_librecompat_handle();
	if (handle == NULL)
		return NULL;
	return (compat_endusershell_fn)dlsym(handle, "compat_endusershell");
}

char *
ie_getusershell(void)
{
	compat_getusershell_fn fn;

	fn = resolve_compat_getusershell();
	if (fn == NULL) {
		errno = ENOSYS;
		return NULL;
	}

	return fn();
}

void
ie_setusershell(void)
{
	compat_setusershell_fn fn;

	fn = resolve_compat_setusershell();
	if (fn != NULL)
		fn();
}

void
ie_endusershell(void)
{
	compat_endusershell_fn fn;

	fn = resolve_compat_endusershell();
	if (fn != NULL)
		fn();
}
