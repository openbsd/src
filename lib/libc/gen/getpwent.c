/*	$OpenBSD: getpwent.c,v 1.43 2011/09/05 03:52:24 guenther Exp $ */
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

#include <sys/param.h>
#include <fcntl.h>
#include <db.h>
#include <syslog.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netgroup.h>
#ifdef YP
#include <machine/param.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"
#include "ypexclude.h"
#endif
#include "thread_private.h"

_THREAD_PRIVATE_KEY(pw);

static DB *_pw_db;			/* password database */

/* Following are used only by setpwent(), getpwent(), and endpwent() */
static struct passwd _pw_passwd;	/* password structure */
static char _pw_string[_PW_BUF_LEN];	/* string pointed to by _pw_passwd */
static int _pw_keynum;			/* key counter */
static int _pw_stayopen;		/* keep fd's open */
static int _pw_flags;			/* password flags */

static int __hashpw(DBT *, char *buf, size_t buflen, struct passwd *, int *);
static int __initdb();
static struct passwd *_pwhashbyname(const char *name, char *buf,
	size_t buflen, struct passwd *pw, int *);
static struct passwd *_pwhashbyuid(uid_t uid, char *buf,
	size_t buflen, struct passwd *pw, int *);

#ifdef YP
static char	*__ypdomain;

/* Following are used only by setpwent(), getpwent(), and endpwent() */
enum _ypmode { YPMODE_NONE, YPMODE_FULL, YPMODE_USER, YPMODE_NETGRP };
static enum	_ypmode __ypmode;
static char	*__ypcurrent;
static int	__ypcurrentlen;
static int	__yp_pw_flags;
static struct passwd *__ypproto;
static char	__ypline[_PW_BUF_LEN];
static int	__getpwent_has_yppw = -1;
static struct _ypexclude *__ypexhead;

static int __has_yppw();
static int __has_ypmaster(void);
static void __ypproto_set(struct passwd *, long *, int, int *);
static int __ypparse(struct passwd *pw, char *s, int);

#define LOOKUP_BYNAME 0
#define LOOKUP_BYUID 1
static struct passwd *__yppwlookup(int, char *, uid_t, struct passwd *,
    char *, size_t, int *);

/* macro for deciding which YP maps to use. */
#define PASSWD_BYNAME \
	(__has_ypmaster() ? "master.passwd.byname" : "passwd.byname")
#define PASSWD_BYUID \
	(__has_ypmaster() ? "master.passwd.byuid" : "passwd.byuid")

static void
__ypproto_set(struct passwd *pw, long *buf, int flags, int *yp_pw_flagsp)
{
	char *ptr;

	/* make this the new prototype */
	ptr = (char *)buf;

	/* first allocate the struct. */
	__ypproto = (struct passwd *)ptr;
	ptr += sizeof(struct passwd);

	/* name */
	if (pw->pw_name && (pw->pw_name)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_name, ptr, strlen(pw->pw_name) + 1);
		__ypproto->pw_name = ptr;
		ptr += (strlen(pw->pw_name) + 1);
	} else
		__ypproto->pw_name = NULL;

	/* password */
	if (pw->pw_passwd && (pw->pw_passwd)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_passwd, ptr, strlen(pw->pw_passwd) + 1);
		__ypproto->pw_passwd = ptr;
		ptr += (strlen(pw->pw_passwd) + 1);
	} else
		__ypproto->pw_passwd = NULL;

	/* uid */
	__ypproto->pw_uid = pw->pw_uid;

	/* gid */
	__ypproto->pw_gid = pw->pw_gid;

	/* change (ignored anyway) */
	__ypproto->pw_change = pw->pw_change;

	/* class (ignored anyway) */
	__ypproto->pw_class = "";

	/* gecos */
	if (pw->pw_gecos && (pw->pw_gecos)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_gecos, ptr, strlen(pw->pw_gecos) + 1);
		__ypproto->pw_gecos = ptr;
		ptr += (strlen(pw->pw_gecos) + 1);
	} else
		__ypproto->pw_gecos = NULL;

	/* dir */
	if (pw->pw_dir && (pw->pw_dir)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_dir, ptr, strlen(pw->pw_dir) + 1);
		__ypproto->pw_dir = ptr;
		ptr += (strlen(pw->pw_dir) + 1);
	} else
		__ypproto->pw_dir = NULL;

	/* shell */
	if (pw->pw_shell && (pw->pw_shell)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_shell, ptr, strlen(pw->pw_shell) + 1);
		__ypproto->pw_shell = ptr;
		ptr += (strlen(pw->pw_shell) + 1);
	} else
		__ypproto->pw_shell = NULL;

	/* expire (ignored anyway) */
	__ypproto->pw_expire = pw->pw_expire;

	/* flags */
	*yp_pw_flagsp = flags;
}

static int
__ypparse(struct passwd *pw, char *s, int yp_pw_flags)
{
	char *bp, *cp, *endp;
	u_long ul;
	int count = 0;

	/* count the colons. */
	bp = s;
	while (*bp != '\0') {
		if (*bp++ == ':')
			count++;
	}

	/* since this is currently using strsep(), parse it first */
	bp = s;
	pw->pw_name = strsep(&bp, ":\n");
	pw->pw_passwd = strsep(&bp, ":\n");
	if (!(cp = strsep(&bp, ":\n")))
		return (1);
	ul = strtoul(cp, &endp, 10);
	if (endp == cp || *endp != '\0' || ul >= UID_MAX)
		return (1);
	pw->pw_uid = (uid_t)ul;
	if (!(cp = strsep(&bp, ":\n")))
		return (1);
	ul = strtoul(cp, &endp, 10);
	if (endp == cp || *endp != '\0' || ul >= GID_MAX)
		return (1);
	pw->pw_gid = (gid_t)ul;
	if (count == 9) {
		long l;

		/* If the ypserv gave us all the fields, use them. */
		pw->pw_class = strsep(&bp, ":\n");
		if (!(cp = strsep(&bp, ":\n")))
			return (1);
		l = strtol(cp, &endp, 10);
		if (endp == cp || *endp != '\0' || l >= INT_MAX || l <= INT_MIN)
			return (1);
		pw->pw_change = (time_t)l;
		if (!(cp = strsep(&bp, ":\n")))
			return (1);
		l = strtol(cp, &endp, 10);
		if (endp == cp || *endp != '\0' || l >= INT_MAX || l <= INT_MIN)
			return (1);
		pw->pw_expire = (time_t)l;
	} else {
		/* ..else it is a normal ypserv. */
		pw->pw_class = "";
		pw->pw_change = 0;
		pw->pw_expire = 0;
	}
	pw->pw_gecos = strsep(&bp, ":\n");
	pw->pw_dir = strsep(&bp, ":\n");
	pw->pw_shell = strsep(&bp, ":\n");

	/* now let the prototype override, if set. */
	if (__ypproto) {
		if (!(yp_pw_flags & _PASSWORD_NOUID))
			pw->pw_uid = __ypproto->pw_uid;
		if (!(yp_pw_flags & _PASSWORD_NOGID))
			pw->pw_gid = __ypproto->pw_gid;
		if (__ypproto->pw_gecos)
			pw->pw_gecos = __ypproto->pw_gecos;
		if (__ypproto->pw_dir)
			pw->pw_dir = __ypproto->pw_dir;
		if (__ypproto->pw_shell)
			pw->pw_shell = __ypproto->pw_shell;
	}
	return (0);
}
#endif

struct passwd *
getpwent(void)
{
#ifdef YP
	static char *name = NULL;
	char *map;
#endif
	char bf[1 + sizeof(_pw_keynum)];
	struct passwd *pw = NULL;
	DBT key;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	if (!_pw_db && !__initdb())
		goto done;

#ifdef YP
	map = PASSWD_BYNAME;

	if (__getpwent_has_yppw == -1)
		__getpwent_has_yppw = __has_yppw();

again:
	if (__getpwent_has_yppw && (__ypmode != YPMODE_NONE)) {
		const char *user, *host, *dom;
		int keylen, datalen, r, s;
		char *key, *data = NULL;

		if (!__ypdomain) {
			if (_yp_check(&__ypdomain) == 0) {
				__ypmode = YPMODE_NONE;
				goto again;
			}
		}
		switch (__ypmode) {
		case YPMODE_FULL:
			if (__ypcurrent) {
				r = yp_next(__ypdomain, map,
				    __ypcurrent, __ypcurrentlen,
				    &key, &keylen, &data, &datalen);
				free(__ypcurrent);
				__ypcurrent = NULL;
				if (r != 0) {
					__ypmode = YPMODE_NONE;
					if (data)
						free(data);
					goto again;
				}
				__ypcurrent = key;
				__ypcurrentlen = keylen;
			} else {
				r = yp_first(__ypdomain, map,
				    &__ypcurrent, &__ypcurrentlen,
				    &data, &datalen);
				if (r != 0 ||
				    __ypcurrentlen > sizeof(__ypline)) {
					__ypmode = YPMODE_NONE;
					if (data)
						free(data);
					goto again;
				}
			}
			bcopy(data, __ypline, datalen);
			free(data);
			break;
		case YPMODE_NETGRP:
			s = getnetgrent(&host, &user, &dom);
			if (s == 0) {	/* end of group */
				endnetgrent();
				__ypmode = YPMODE_NONE;
				goto again;
			}
			if (user && *user) {
				r = yp_match(__ypdomain, map,
				    user, strlen(user), &data, &datalen);
			} else
				goto again;
			if (r != 0 ||
			    __ypcurrentlen > sizeof(__ypline)) {
				/*
				 * if the netgroup is invalid, keep looking
				 * as there may be valid users later on.
				 */
				if (data)
					free(data);
				goto again;
			}
			bcopy(data, __ypline, datalen);
			free(data);
			break;
		case YPMODE_USER:
			if (name) {
				r = yp_match(__ypdomain, map,
				    name, strlen(name), &data, &datalen);
				__ypmode = YPMODE_NONE;
				free(name);
				name = NULL;
				if (r != 0 ||
				    __ypcurrentlen > sizeof(__ypline)) {
					if (data)
						free(data);
					goto again;
				}
				bcopy(data, __ypline, datalen);
				free(data);
			} else {		/* XXX */
				__ypmode = YPMODE_NONE;
				goto again;
			}
			break;
		case YPMODE_NONE:
			/* NOTREACHED */
			break;
		}

		__ypline[datalen] = '\0';
		if (__ypparse(&_pw_passwd, __ypline, __yp_pw_flags))
			goto again;
		pw = &_pw_passwd;
		goto done;
	}
#endif

	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	bcopy((char *)&_pw_keynum, &bf[1], sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = 1 + sizeof(_pw_keynum);
	if (__hashpw(&key, _pw_string, sizeof _pw_string,
	    &_pw_passwd, &_pw_flags)) {
#ifdef YP
		static long __yppbuf[_PW_BUF_LEN / sizeof(long)];
		const char *user, *host, *dom;

		/* if we don't have YP at all, don't bother. */
		if (__getpwent_has_yppw) {
			if (_pw_passwd.pw_name[0] == '+') {
				/* set the mode */
				switch (_pw_passwd.pw_name[1]) {
				case '\0':
					__ypmode = YPMODE_FULL;
					break;
				case '@':
					__ypmode = YPMODE_NETGRP;
					setnetgrent(_pw_passwd.pw_name + 2);
					break;
				default:
					__ypmode = YPMODE_USER;
					name = strdup(_pw_passwd.pw_name + 1);
					break;
				}

				__ypproto_set(&_pw_passwd, __yppbuf,
				    _pw_flags, &__yp_pw_flags);
				goto again;
			} else if (_pw_passwd.pw_name[0] == '-') {
				/* an attempted exclusion */
				switch (_pw_passwd.pw_name[1]) {
				case '\0':
					break;
				case '@':
					setnetgrent(_pw_passwd.pw_name + 2);
					while (getnetgrent(&host, &user, &dom)) {
						if (user && *user)
							__ypexclude_add(&__ypexhead,
							    user);
					}
					endnetgrent();
					break;
				default:
					__ypexclude_add(&__ypexhead,
					    _pw_passwd.pw_name + 1);
					break;
				}
				goto again;
			}
		}
#endif
		pw = &_pw_passwd;
		goto done;
	}

done:
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return (pw);
}

#ifdef YP
/*
 * See if the YP token is in the database.  Only works if pwd_mkdb knows
 * about the token.
 */
static int
__has_yppw(void)
{
	DBT key, data, pkey, pdata;
	char bf[2];

	key.data = (u_char *)_PW_YPTOKEN;
	key.size = strlen(_PW_YPTOKEN);

	/* Pre-token database support. */
	bf[0] = _PW_KEYBYNAME;
	bf[1] = '+';
	pkey.data = (u_char *)bf;
	pkey.size = sizeof(bf);

	if ((_pw_db->get)(_pw_db, &key, &data, 0) &&
	    (_pw_db->get)(_pw_db, &pkey, &pdata, 0))
		return (0);	/* No YP. */
	return (1);
}

/*
 * See if there's a master.passwd map.
 */
static int
__has_ypmaster(void)
{
	int keylen, resultlen;
	char *key, *result;
	static int checked = -1;
	static uid_t saved_uid, saved_euid;
	uid_t uid = getuid(), euid = geteuid();

	/*
	 * Do not recheck IFF the saved UID and the saved
	 * EUID are the same. In all other cases, recheck.
	 */
	if (checked != -1 && saved_uid == uid && saved_euid == euid)
		return (checked);

	if (euid != 0) {
		saved_uid = uid;
		saved_euid = euid;
		checked = 0;
		return (checked);
	}

	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0) {
			saved_uid = uid;
			saved_euid = euid;
			checked = 0;
			return (checked);	/* No domain. */
		}
	}

	if (yp_first(__ypdomain, "master.passwd.byname",
	    &key, &keylen, &result, &resultlen)) {
		saved_uid = uid;
		saved_euid = euid;
		checked = 0;
		return (checked);
	}
	free(result);
	if (key)
		free(key);

	saved_uid = uid;
	saved_euid = euid;
	checked = 1;
	return (checked);
}

static struct passwd *
__yppwlookup(int lookup, char *name, uid_t uid, struct passwd *pw,
    char *buf, size_t buflen, int *flagsp)
{
	char bf[1 + _PW_NAME_LEN], *ypcurrent = NULL, *map = NULL;
	int yp_pw_flags = 0, ypcurrentlen, r, s = -1, pw_keynum;
	static long yppbuf[_PW_BUF_LEN / sizeof(long)];
	struct _ypexclude *ypexhead = NULL;
	const char *host, *user, *dom;
	DBT key;

	for (pw_keynum = 1; pw_keynum; pw_keynum++) {
		bf[0] = _PW_KEYBYNUM;
		bcopy((char *)&pw_keynum, &bf[1], sizeof(pw_keynum));
		key.data = (u_char *)bf;
		key.size = 1 + sizeof(pw_keynum);
		if (__hashpw(&key, buf, buflen, pw, flagsp) == 0)
			break;
		switch (pw->pw_name[0]) {
		case '+':
			if (!__ypdomain) {
				if (_yp_check(&__ypdomain) == 0)
					continue;
			}
			__ypproto_set(pw, yppbuf, *flagsp, &yp_pw_flags);
			if (!map) {
				if (lookup == LOOKUP_BYNAME) {
					map = PASSWD_BYNAME;
					name = strdup(name);
				} else {
					map = PASSWD_BYUID;
					asprintf(&name, "%u", uid);
				}
			}

			switch (pw->pw_name[1]) {
			case '\0':
				if (ypcurrent) {
					free(ypcurrent);
					ypcurrent = NULL;
				}
				r = yp_match(__ypdomain, map,
				    name, strlen(name),
				    &ypcurrent, &ypcurrentlen);
				if (r != 0 || ypcurrentlen > buflen) {
					if (ypcurrent)
						free(ypcurrent);
					ypcurrent = NULL;
					continue;
				}
				break;
			case '@':
pwnam_netgrp:
				if (ypcurrent) {
					free(ypcurrent);
					ypcurrent = NULL;
				}
				if (s == -1)	/* first time */
					setnetgrent(pw->pw_name + 2);
				s = getnetgrent(&host, &user, &dom);
				if (s == 0) {	/* end of group */
					endnetgrent();
					s = -1;
					continue;
				} else {
					if (user && *user) {
						r = yp_match(__ypdomain, map,
						    user, strlen(user),
						    &ypcurrent, &ypcurrentlen);
					} else
						goto pwnam_netgrp;
					if (r != 0 || ypcurrentlen > buflen) {
						if (ypcurrent)
							free(ypcurrent);
						ypcurrent = NULL;
						/*
						 * just because this
						 * user is bad, doesn't
						 * mean they all are.
						 */
						goto pwnam_netgrp;
					}
				}
				break;
			default:
				if (ypcurrent) {
					free(ypcurrent);
					ypcurrent = NULL;
				}
				user = pw->pw_name + 1;
				r = yp_match(__ypdomain, map,
				    user, strlen(user),
				    &ypcurrent, &ypcurrentlen);
				if (r != 0 || ypcurrentlen > buflen) {
					if (ypcurrent)
						free(ypcurrent);
					ypcurrent = NULL;
					continue;
				}
				break;
			}
			bcopy(ypcurrent, buf, ypcurrentlen);
			buf[ypcurrentlen] = '\0';
			if (__ypparse(pw, buf, yp_pw_flags) ||
			    __ypexclude_is(&ypexhead, pw->pw_name)) {
				if (s == 1)	/* inside netgrp */
					goto pwnam_netgrp;
				continue;
			}
			break;
		case '-':
			/* attempted exclusion */
			switch (pw->pw_name[1]) {
			case '\0':
				break;
			case '@':
				setnetgrent(pw->pw_name + 2);
				while (getnetgrent(&host, &user, &dom)) {
					if (user && *user)
						__ypexclude_add(&ypexhead, user);
				}
				endnetgrent();
				break;
			default:
				__ypexclude_add(&ypexhead, pw->pw_name + 1);
				break;
			}
			break;
		}
		if ((lookup == LOOKUP_BYUID && pw->pw_uid == uid) ||
		    (lookup == LOOKUP_BYNAME && strcmp(pw->pw_name, name) == 0))
			goto done;
		if (s == 1)	/* inside netgrp */
			goto pwnam_netgrp;
		continue;
	}
	pw = NULL;
done:
	__ypexclude_free(&ypexhead);
	__ypproto = NULL;
	if (ypcurrent)
		free(ypcurrent);
	ypcurrent = NULL;
	if (map)
		free(name);
	return (pw);
}
#endif /* YP */

static struct passwd *
_pwhashbyname(const char *name, char *buf, size_t buflen, struct passwd *pw,
    int *flagsp)
{
	char bf[1 + _PW_NAME_LEN];
	int len, r;
	DBT key;

	len = strlen(name);
	if (len > _PW_NAME_LEN)
		return (NULL);
	bf[0] = _PW_KEYBYNAME;
	bcopy(name, &bf[1], MIN(len, _PW_NAME_LEN));
	key.data = (u_char *)bf;
	key.size = 1 + MIN(len, _PW_NAME_LEN);
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

int
getpwnam_r(const char *name, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp)
{
	struct passwd *pwret = NULL;
	int flags = 0, *flagsp;
	DB *savedb;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	savedb = _pw_db;
	if (!_pw_db && !__initdb())
		goto fail;

	if (pw == &_pw_passwd)
		flagsp = &_pw_flags;
	else
		flagsp = &flags;

#ifdef YP
	if (__has_yppw())
		pwret = __yppwlookup(LOOKUP_BYNAME, (char *)name, 0, pw,
		    buf, buflen, flagsp);
#endif /* YP */
	if (!pwret)
		pwret = _pwhashbyname(name, buf, buflen, pw, flagsp);

	if (savedb != _pw_db || !_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = NULL;
	}
fail:
	if (pwretp)
		*pwretp = pwret;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return (pwret ? 0 : 1);
}

struct passwd *
getpwnam(const char *name)
{
	struct passwd *pw = NULL;

	if (getpwnam_r(name, &_pw_passwd, _pw_string, sizeof _pw_string, &pw))
		pw = NULL;
	return (pw);
}

int
getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t buflen,
    struct passwd **pwretp)
{
	struct passwd *pwret = NULL;
	int flags = 0, *flagsp;
	DB *savedb;

	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	savedb = _pw_db;
	if (!_pw_db && !__initdb())
		goto fail;

	if (pw == &_pw_passwd)
		flagsp = &_pw_flags;
	else
		flagsp = &flags;

#ifdef YP
	if (__has_yppw())
		pwret = __yppwlookup(LOOKUP_BYUID, NULL, uid, pw,
		    buf, buflen, flagsp);
#endif /* YP */
	if (!pwret)
		pwret = _pwhashbyuid(uid, buf, buflen, pw, flagsp);

	if (savedb != _pw_db || !_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = NULL;
	}
fail:
	if (pwretp)
		*pwretp = pwret;
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
	return (pwret ? 0 : 1);
}

struct passwd *
getpwuid(uid_t uid)
{
	struct passwd *pw = NULL;

	if (getpwuid_r(uid, &_pw_passwd, _pw_string, sizeof _pw_string, &pw))
		pw = NULL;
	return (pw);
}

int
setpassent(int stayopen)
{
	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	_pw_keynum = 0;
	_pw_stayopen = stayopen;
#ifdef YP
	__ypmode = YPMODE_NONE;
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	__ypexclude_free(&__ypexhead);
	__ypproto = NULL;
#endif
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
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
	_THREAD_PRIVATE_MUTEX_LOCK(pw);
	_pw_keynum = 0;
	if (_pw_db) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = NULL;
	}
#ifdef YP
	__ypmode = YPMODE_NONE;
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	__ypexclude_free(&__ypexhead);
	__ypproto = NULL;
#endif
	_THREAD_PRIVATE_MUTEX_UNLOCK(pw);
}

static int
__initdb(void)
{
	static int warned;
	int saved_errno = errno;

#ifdef YP
	__ypmode = YPMODE_NONE;
	__getpwent_has_yppw = -1;
#endif
	if ((_pw_db = dbopen(_PATH_SMP_DB, O_RDONLY, 0, DB_HASH, NULL)) ||
	    (_pw_db = dbopen(_PATH_MP_DB, O_RDONLY, 0, DB_HASH, NULL))) {
		errno = saved_errno;
		return (1);
	}
	if (!warned)
		syslog(LOG_ERR, "%s: %m", _PATH_MP_DB);
	warned = 1;
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
	if (data.size > buflen)
		return (0);

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
