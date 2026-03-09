/*	$OpenBSD: getpwent.c,v 1.3 2026/03/09 12:56:12 deraadt Exp $ */
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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
#include <fcntl.h>
#include <db.h>
#include <syslog.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netgroup.h>

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

struct pw_storage {
	struct passwd pw;
	uid_t uid;
	char name[_PW_NAME_LEN + 1];
	char pwbuf[_PW_BUF_LEN];
};


static DB *_pw_db;			/* password database */

/* mmap'd password storage */
static struct pw_storage *_pw_storage = MAP_FAILED;

/* Following are used only by setpwent(), getpwent(), and endpwent() */
static int _pw_keynum;			/* key counter */
static int _pw_stayopen;		/* keep fd's open */
static int _pw_flags;			/* password flags */

static int __hashpw(DBT *, char *buf, size_t buflen, struct passwd *, int *);
static int __initdb(int);
static struct passwd *_pwhashbyname(const char *name, char *buf,
	size_t buflen, struct passwd *pw, int *);
static struct passwd *_pwhashbyuid(uid_t uid, char *buf,
	size_t buflen, struct passwd *pw, int *);

static struct passwd *
__get_pw_buf(char **bufp, size_t *buflenp, uid_t uid, const char *name)
{
	bool remap = true;

	/* Unmap the old buffer unless we are looking up the same uid/name */
	if (_pw_storage != MAP_FAILED) {
		if (name != NULL) {
			if (strcmp(_pw_storage->name, name) == 0) {
#ifdef PWDEBUG
				struct syslog_data sdata = SYSLOG_DATA_INIT;
				syslog_r(LOG_CRIT | LOG_CONS, &sdata,
				    "repeated passwd lookup of user \"%s\"",
				    name);
#endif
				remap = false;
			}
		} else if (uid != (uid_t)-1) {
			if (_pw_storage->uid == uid) {
#ifdef PWDEBUG
				struct syslog_data sdata = SYSLOG_DATA_INIT;
				syslog_r(LOG_CRIT | LOG_CONS, &sdata,
				    "repeated passwd lookup of uid %u",
				    uid);
#endif
				remap = false;
			}
		}
		if (remap)
			munmap(_pw_storage, sizeof(*_pw_storage));
	}

	if (remap) {
		_pw_storage = mmap(NULL, sizeof(*_pw_storage),
		    PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		if (_pw_storage == MAP_FAILED)
			return NULL;
		if (name != NULL)
			strlcpy(_pw_storage->name, name, sizeof(_pw_storage->name));
		_pw_storage->uid = uid;
	}

	*bufp = _pw_storage->pwbuf;
	*buflenp = sizeof(_pw_storage->pwbuf);
	return &_pw_storage->pw;
}

struct passwd *
getpwent(void)
{
	char bf[1 + sizeof(_pw_keynum)];
	struct passwd *pw, *ret = NULL;
	char *pwbuf;
	size_t buflen;
	DBT key;

	if (!_pw_db && !__initdb(0))
		goto done;

	/* Allocate space for struct and strings, unmapping the old. */
	if ((pw = __get_pw_buf(&pwbuf, &buflen, -1, NULL)) == NULL)
		goto done;

	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	bcopy((char *)&_pw_keynum, &bf[1], sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = 1 + sizeof(_pw_keynum);
	if (__hashpw(&key, pwbuf, buflen, pw, &_pw_flags)) {
		ret = pw;
		goto done;
	}
done:
	return (ret);
}

static struct passwd *
_pwhashbyname(const char *name, char *buf, size_t buflen, struct passwd *pw,
    int *flagsp)
{
	char bf[1 + _PW_NAME_LEN];
	size_t len;
	DBT key;
	int r;

	len = strlen(name);
	if (len > _PW_NAME_LEN)
		return (NULL);
	bf[0] = _PW_KEYBYNAME;
	bcopy(name, &bf[1], MINIMUM(len, _PW_NAME_LEN));
	key.data = (u_char *)bf;
	key.size = 1 + MINIMUM(len, _PW_NAME_LEN);
	r = __hashpw(&key, buf, buflen, pw, flagsp);
	if (r)
		return (pw);
	return (NULL);
}

static struct passwd *
_pwhashbyuid(uid_t uid, char *buf, size_t buflen, struct passwd *pw,
    int *flagsp)
{
	char bf[1 + sizeof(int)];
	DBT key;
	int r;

	bf[0] = _PW_KEYBYUID;
	bcopy(&uid, &bf[1], sizeof(uid));
	key.data = (u_char *)bf;
	key.size = 1 + sizeof(uid);
	r = __hashpw(&key, buf, buflen, pw, flagsp);
	if (r)
		return (pw);
	return (NULL);
}

static int
getpwnam_internal(const char *name, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp, bool shadow, bool reentrant)
{
	struct passwd *pwret = NULL;
	int flags = 0, *flagsp = &flags;
	int my_errno = 0;
	int saved_errno, tmp_errno;

	saved_errno = errno;
	errno = 0;
	if (!_pw_db && !__initdb(shadow))
		goto fail;

	if (!reentrant) {
		/* Allocate space for struct and strings, unmapping the old. */
		if ((pw = __get_pw_buf(&buf, &buflen, -1, name)) == NULL)
			goto fail;
		flagsp = &_pw_flags;
	}

	if (!pwret)
		pwret = _pwhashbyname(name, buf, buflen, pw, flagsp);

	if (!_pw_stayopen) {
		tmp_errno = errno;
		(void)(_pw_db->close)(_pw_db);
		_pw_db = NULL;
		errno = tmp_errno;
	}
fail:
	if (pwretp)
		*pwretp = pwret;
	if (pwret == NULL)
		my_errno = errno;
	errno = saved_errno;
	return (my_errno);
}

int
getpwnam_r(const char *name, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp)
{
	return getpwnam_internal(name, pw, buf, buflen, pwretp, false, true);
}

struct passwd *
getpwnam(const char *name)
{
	struct passwd *pw = NULL;
	int my_errno;

	my_errno = getpwnam_internal(name, NULL, NULL, 0, &pw, false, false);
	if (my_errno) {
		pw = NULL;
		errno = my_errno;
	}
	return (pw);
}

struct passwd *
getpwnam_shadow(const char *name)
{
	struct passwd *pw = NULL;
	int my_errno;

	my_errno = getpwnam_internal(name, NULL, NULL, 0, &pw, true, false);
	if (my_errno) {
		pw = NULL;
		errno = my_errno;
	}
	return (pw);
}

static int
getpwuid_internal(uid_t uid, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp, bool shadow, bool reentrant)
{
	struct passwd *pwret = NULL;
	int flags = 0, *flagsp = &flags;
	int my_errno = 0;
	int saved_errno, tmp_errno;

	saved_errno = errno;
	errno = 0;
	if (!_pw_db && !__initdb(shadow))
		goto fail;

	if (!reentrant) {
		/* Allocate space for struct and strings, unmapping the old. */
		if ((pw = __get_pw_buf(&buf, &buflen, uid, NULL)) == NULL)
			goto fail;
		flagsp = &_pw_flags;
	}

	if (!pwret)
		pwret = _pwhashbyuid(uid, buf, buflen, pw, flagsp);

	if (!_pw_stayopen) {
		tmp_errno = errno;
		(void)(_pw_db->close)(_pw_db);
		_pw_db = NULL;
		errno = tmp_errno;
	}
fail:
	if (pwretp)
		*pwretp = pwret;
	if (pwret == NULL)
		my_errno = errno;
	errno = saved_errno;
	return (my_errno);
}


int
getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp)
{
	return getpwuid_internal(uid, pw, buf, buflen, pwretp, false, true);
}

struct passwd *
getpwuid(uid_t uid)
{
	struct passwd *pw = NULL;
	int my_errno;

	my_errno = getpwuid_internal(uid, NULL, NULL, 0, &pw, false, false);
	if (my_errno) {
		pw = NULL;
		errno = my_errno;
	}
	return (pw);
}

struct passwd *
getpwuid_shadow(uid_t uid)
{
	struct passwd *pw = NULL;
	int my_errno;

	my_errno = getpwuid_internal(uid, NULL, NULL, 0, &pw, true, false);
	if (my_errno) {
		pw = NULL;
		errno = my_errno;
	}
	return (pw);
}

int
setpassent(int stayopen)
{
	_pw_keynum = 0;
	_pw_stayopen = stayopen;
	return (1);
}

void
setpwent(void)
{
	(void) setpassent(0);
}

void
endpwent(void)
{
	int saved_errno;

	saved_errno = errno;
	_pw_keynum = 0;
	if (_pw_db) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = NULL;
	}
	errno = saved_errno;
}

static int
__initdb(int shadow)
{
	static int warned;
	int saved_errno = errno;

	if (shadow)
		_pw_db = dbopen(_PATH_SMP_DB, O_RDONLY, 0, DB_HASH, NULL);
	if (!_pw_db)
		_pw_db = dbopen(_PATH_MP_DB, O_RDONLY, 0, DB_HASH, NULL);

	if (_pw_db) {
		errno = saved_errno;
		return (1);
	}
	if (!warned) {
		saved_errno = errno;
		errno = saved_errno;
		warned = 1;
	}
	return (0);
}

static int
__hashpw(DBT *key, char *buf, size_t buflen, struct passwd *pw,
    int *flagsp)
{
	char *p, *t;
	DBT data;

	if ((_pw_db->get)(_pw_db, key, &data, 0))
		return (0);
	p = (char *)data.data;
	if (data.size > buflen) {
		errno = ERANGE;
		return (0);
	}

	t = buf;
#define	EXPAND(e)	e = t; while ((*t++ = *p++));
	EXPAND(pw->pw_name);
	EXPAND(pw->pw_passwd);
	bcopy(p, (char *)&pw->pw_uid, sizeof(int));
	p += sizeof(int);
	bcopy(p, (char *)&pw->pw_gid, sizeof(int));
	p += sizeof(int);
	bcopy(p, (char *)&pw->pw_change, sizeof(time_t));
	p += sizeof(time_t);
	EXPAND(pw->pw_class);
	EXPAND(pw->pw_gecos);
	EXPAND(pw->pw_dir);
	EXPAND(pw->pw_shell);
	bcopy(p, (char *)&pw->pw_expire, sizeof(time_t));
	p += sizeof(time_t);

	/* See if there's any data left.  If so, read in flags. */
	if (data.size > (p - (char *)data.data)) {
		bcopy(p, (char *)flagsp, sizeof(int));
		p += sizeof(int);
	} else
		*flagsp = _PASSWORD_NOUID|_PASSWORD_NOGID;	/* default */
	return (1);
}
