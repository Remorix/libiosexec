/*	$OpenBSD: getpwent.c,v 1.64 2021/12/07 18:13:45 deraadt Exp $ */
/*
 * Copyright (c) 2008 Theo de Raadt
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1994, 1995, 1996, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/param.h>

#include "libiosexec.h"
#include "libiosexec_private.h"

#define DEF_WEAK(a)

#define __THREAD_NAME(name)     __CONCAT(_thread_tagname_,name)
#define _THREAD_PRIVATE_KEY(name)                                       \
        static void *__THREAD_NAME(name)

#define _THREAD_PRIVATE_MUTEX_LOCK(name)                do {} while (0)
#define _THREAD_PRIVATE_MUTEX_UNLOCK(name)              do {} while (0)

#define _PW_BUF_LEN 1024
#define _PW_LINE_LEN 1024

struct pw_storage {
	struct passwd pw;
	uid_t uid;
	char name[_PW_NAME_LEN + 1];
	char pwbuf[_PW_BUF_LEN];
};

_THREAD_PRIVATE_KEY(pw);

/* mmap'd password storage */
static struct pw_storage *_pw_storage = MAP_FAILED;
static FILE *_pw_fp;
static int _pw_stayopen;
static bool _pw_master_format;

static struct passwd *__get_pw_buf(char **bufp, size_t *buflenp, uid_t uid,
    const char *name);
static int parse_passwd_line(char *line, struct passwd *pw, char *buf,
    size_t buflen, bool master_format, bool *has_shadow);
static int read_passwd_file(FILE *fp, struct passwd *pw, char *buf,
    size_t buflen, bool master_format, bool match_uid, uid_t uid,
    const char *name, bool *has_shadow);
static FILE *open_passwd_file(bool shadow, bool *master_format);

static struct passwd *
__get_pw_buf(char **bufp, size_t *buflenp, uid_t uid, const char *name)
{
	bool remap = true;

	if (_pw_storage != MAP_FAILED) {
		if (name != NULL) {
			if (strcmp(_pw_storage->name, name) == 0)
				remap = false;
		} else if (uid != (uid_t)-1) {
			if (_pw_storage->uid == uid)
				remap = false;
		}
		if (remap)
			munmap(_pw_storage, sizeof(*_pw_storage));
	}

	if (remap) {
		_pw_storage = mmap(NULL, sizeof(*_pw_storage),
		    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (_pw_storage == MAP_FAILED)
			return NULL;
		if (name != NULL)
			strlcpy(_pw_storage->name, name, sizeof(_pw_storage->name));
		else
			_pw_storage->name[0] = '\0';
		_pw_storage->uid = uid;
	}

	*bufp = _pw_storage->pwbuf;
	*buflenp = sizeof(_pw_storage->pwbuf);
	return &_pw_storage->pw;
}

static int
copy_field(char **dst, char **cursor, size_t *remaining, const char *src)
{
	size_t len = strlen(src) + 1;

	if (len > *remaining) {
		errno = ERANGE;
		return 0;
	}

	memcpy(*cursor, src, len);
	*dst = *cursor;
	*cursor += len;
	*remaining -= len;
	return 1;
}

static int
parse_number(const char *field, long long min, long long max, long long *value)
{
	char *end = NULL;
	long long parsed;

	errno = 0;
	parsed = strtoll(field, &end, 10);
	if (end == field || *end != '\0' || errno != 0)
		return 0;
	if (parsed < min || parsed > max)
		return 0;
	*value = parsed;
	return 1;
}

static int
parse_passwd_line(char *line, struct passwd *pw, char *buf, size_t buflen,
    bool master_format, bool *has_shadow)
{
	char *fields[10] = {0};
	char *cursor = buf;
	size_t remaining = buflen;
	char *bp = line;
	int expected = master_format ? 10 : 7;
	int i;
	long long num;

	for (i = 0; i < expected; i++) {
		fields[i] = strsep(&bp, ":\n");
		if (fields[i] == NULL)
			return 0;
	}
	if (bp != NULL && *bp != '\0')
		return 0;

	if (!copy_field(&pw->pw_name, &cursor, &remaining, fields[0]))
		return 0;
	if (!copy_field(&pw->pw_passwd, &cursor, &remaining, fields[1]))
		return 0;
	if (!parse_number(fields[2], 0, UINT_MAX, &num))
		return 0;
	pw->pw_uid = (uid_t)num;
	if (!parse_number(fields[3], INT_MIN, INT_MAX, &num))
		return 0;
	pw->pw_gid = (gid_t)num;

	pw->pw_change = 0;
	pw->pw_class = (char *)"";
	pw->pw_expire = 0;
	if (master_format) {
		if (!copy_field(&pw->pw_class, &cursor, &remaining, fields[4]))
			return 0;
		if (!parse_number(fields[5], LLONG_MIN, LLONG_MAX, &num))
			return 0;
		pw->pw_change = (time_t)num;
		if (!parse_number(fields[6], LLONG_MIN, LLONG_MAX, &num))
			return 0;
		pw->pw_expire = (time_t)num;
		if (!copy_field(&pw->pw_gecos, &cursor, &remaining, fields[7]))
			return 0;
		if (!copy_field(&pw->pw_dir, &cursor, &remaining, fields[8]))
			return 0;
		if (!copy_field(&pw->pw_shell, &cursor, &remaining, fields[9]))
			return 0;
	} else {
		if (!copy_field(&pw->pw_gecos, &cursor, &remaining, fields[4]))
			return 0;
		if (!copy_field(&pw->pw_dir, &cursor, &remaining, fields[5]))
			return 0;
		if (!copy_field(&pw->pw_shell, &cursor, &remaining, fields[6]))
			return 0;
	}

	if (has_shadow != NULL)
		*has_shadow = master_format;
	return 1;
}

static FILE *
open_passwd_file(bool shadow, bool *master_format)
{
	FILE *fp;

	if (shadow) {
		fp = fopen(_PATH_MASTERPASSWD, "re");
		if (fp != NULL) {
			*master_format = true;
			return fp;
		}
	}

	fp = fopen(_PATH_PASSWD, "re");
	if (fp != NULL) {
		*master_format = false;
		return fp;
	}

	return NULL;
}

static int
read_passwd_file(FILE *fp, struct passwd *pw, char *buf, size_t buflen,
    bool master_format, bool match_uid, uid_t uid, const char *name,
    bool *has_shadow)
{
	char line[_PW_LINE_LEN];

	while (fgets(line, sizeof(line), fp) != NULL) {
		if (line[0] == '#' || line[0] == '\n')
			continue;
		if (strchr(line, '\n') == NULL) {
			int ch;

			while ((ch = getc_unlocked(fp)) != '\n' && ch != EOF)
				;
			continue;
		}
		if (!parse_passwd_line(line, pw, buf, buflen, master_format, has_shadow))
			continue;
		if (match_uid) {
			if (pw->pw_uid == uid)
				return 1;
		} else if (name == NULL || strcmp(pw->pw_name, name) == 0) {
			return 1;
		}
	}

	return 0;
}

struct passwd *
ie_getpwent(void)
{
	struct passwd *pw;
	char *pwbuf;
	size_t buflen;
	bool master_format = false;
	bool has_shadow = false;
	struct passwd *ret = NULL;
	int saved_errno;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	saved_errno = errno;

	if (_pw_fp == NULL) {
		_pw_fp = open_passwd_file(geteuid() == 0, &master_format);
		if (_pw_fp == NULL)
			goto done;
		_pw_master_format = master_format;
	} else {
		master_format = _pw_master_format;
	}

	if ((pw = __get_pw_buf(&pwbuf, &buflen, -1, NULL)) == NULL)
		goto done;

	if (read_passwd_file(_pw_fp, pw, pwbuf, buflen, master_format, false,
	    (uid_t)-1, NULL, &has_shadow))
		ret = pw;
	done:
	if (ret == NULL)
		errno = saved_errno;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return ret;
}

static int
getpwnam_internal(const char *name, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp, bool shadow, bool reentrant)
{
	struct passwd *pwret = NULL;
	bool master_format = false;
	bool has_shadow = false;
	int my_errno = 0;
	int saved_errno;
	FILE *fp;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	saved_errno = errno;
	errno = 0;
	fp = open_passwd_file(shadow, &master_format);
	if (fp == NULL)
		goto fail;

	if (!reentrant) {
		if ((pw = __get_pw_buf(&buf, &buflen, -1, name)) == NULL) {
			fclose(fp);
			goto fail;
		}
	}

	if (read_passwd_file(fp, pw, buf, buflen, master_format, false, (uid_t)-1,
	    name, &has_shadow))
		pwret = pw;
	else if (errno == 0)
		errno = ENOENT;

	fclose(fp);
fail:
	if (pwretp)
		*pwretp = pwret;
	if (pwret == NULL)
		my_errno = errno;
	errno = saved_errno;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return my_errno;
}

int
ie_getpwnam_r(const char *name, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp)
{
	return getpwnam_internal(name, pw, buf, buflen, pwretp,
	    geteuid() == 0, true);
}
DEF_WEAK(getpwnam_r);

struct passwd *
ie_getpwnam(const char *name)
{
	struct passwd *pw = NULL;
	int my_errno;

	my_errno = getpwnam_internal(name, NULL, NULL, 0, &pw, geteuid() == 0,
	    false);
	if (my_errno) {
		pw = NULL;
		errno = my_errno;
	}
	return pw;
}

static int
getpwuid_internal(uid_t uid, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp, bool shadow, bool reentrant)
{
	struct passwd *pwret = NULL;
	bool master_format = false;
	bool has_shadow = false;
	int my_errno = 0;
	int saved_errno;
	FILE *fp;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	saved_errno = errno;
	errno = 0;
	fp = open_passwd_file(shadow, &master_format);
	if (fp == NULL)
		goto fail;

	if (!reentrant) {
		if ((pw = __get_pw_buf(&buf, &buflen, uid, NULL)) == NULL) {
			fclose(fp);
			goto fail;
		}
	}

	if (read_passwd_file(fp, pw, buf, buflen, master_format, true, uid, NULL,
	    &has_shadow))
		pwret = pw;
	else if (errno == 0)
		errno = ENOENT;

	fclose(fp);
fail:
	if (pwretp)
		*pwretp = pwret;
	if (pwret == NULL)
		my_errno = errno;
	errno = saved_errno;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return my_errno;
}

int
ie_getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp)
{
	return getpwuid_internal(uid, pw, buf, buflen, pwretp, geteuid() == 0,
	    true);
}
DEF_WEAK(ie_getpwuid_r);

struct passwd *
ie_getpwuid(uid_t uid)
{
	struct passwd *pw = NULL;
	int my_errno;

	my_errno = getpwuid_internal(uid, NULL, NULL, 0, &pw, geteuid() == 0,
	    false);
	if (my_errno) {
		pw = NULL;
		errno = my_errno;
	}
	return pw;
}

int
ie_setpassent(int stayopen)
{
	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	if (_pw_fp != NULL)
		rewind(_pw_fp);
	_pw_stayopen = stayopen;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return 1;
}
DEF_WEAK(ie_setpassent);

void
ie_setpwent(void)
{
	(void)ie_setpassent(0);
}

void
ie_endpwent(void)
{
	int saved_errno;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	saved_errno = errno;
	if (_pw_fp != NULL) {
		fclose(_pw_fp);
		_pw_fp = NULL;
	}
	errno = saved_errno;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
}
