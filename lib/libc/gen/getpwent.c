/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1994, 1995, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: getpwent.c,v 1.5 1996/09/15 10:09:11 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <fcntl.h>
#include <db.h>
#include <syslog.h>
#include <pwd.h>
#include <utmp.h>
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
#endif

static struct passwd _pw_passwd;	/* password structure */
static DB *_pw_db;			/* password database */
static int _pw_keynum;			/* key counter */
static int _pw_stayopen;		/* keep fd's open */
static int _pw_flags;			/* password flags */
static int __hashpw __P((DBT *));
static int __initdb __P((void));

const char __yp_token[] = "__YP!";	/* Let pwd_mkdb pull this in. */

#ifdef YP
enum _ypmode { YPMODE_NONE, YPMODE_FULL, YPMODE_USER, YPMODE_NETGRP };
static enum _ypmode __ypmode;

static char     *__ypcurrent, *__ypdomain;
static int      __ypcurrentlen;
static struct passwd *__ypproto = (struct passwd *)NULL;
static int	__ypflags;
static char	line[1024];
static long	prbuf[1024 / sizeof(long)];
static DB *__ypexclude = (DB *)NULL;

static int __has_yppw __P((void));
static int __ypexclude_add __P((const char *));
static int __ypexclude_is __P((const char *));
static void __ypproto_set __P((void));

static int
__ypexclude_add(name)
const char *name;
{
	DBT key, data;

	/* initialize the exclusion table if needed. */
	if(__ypexclude == (DB *)NULL) {
		__ypexclude = dbopen(NULL, O_RDWR, 600, DB_HASH, NULL);
		if(__ypexclude == (DB *)NULL)
			return(1);
	}

	/* set up the key */
	key.data = (char *)name;
	key.size = strlen(name);

	/* data is nothing. */
	data.data = NULL;
	data.size = 0;

	/* store it */
	if((__ypexclude->put)(__ypexclude, &key, &data, 0) == -1)
		return(1);
	
	return(0);
}

static int
__ypexclude_is(name)
const char *name;
{
	DBT key, data;

	if(__ypexclude == (DB *)NULL)
		return(0);	/* nothing excluded */

	/* set up the key */
	key.data = (char *)name;
	key.size = strlen(name);

	if((__ypexclude->get)(__ypexclude, &key, &data, 0) == 0)
		return(1);	/* excluded */
	
	return(0);
}

static void
__ypproto_set()
{
	register char *ptr;
	register struct passwd *pw = &_pw_passwd;

	/* make this the new prototype */
	ptr = (char *)prbuf;

	/* first allocate the struct. */
	__ypproto = (struct passwd *)ptr;
	ptr += sizeof(struct passwd);

	/* name */
	if(pw->pw_name && (pw->pw_name)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_name, ptr, strlen(pw->pw_name) + 1);
		__ypproto->pw_name = ptr;
		ptr += (strlen(pw->pw_name) + 1);
	} else
		__ypproto->pw_name = (char *)NULL;
	
	/* password */
	if(pw->pw_passwd && (pw->pw_passwd)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_passwd, ptr, strlen(pw->pw_passwd) + 1);
		__ypproto->pw_passwd = ptr;
		ptr += (strlen(pw->pw_passwd) + 1);
	} else
		__ypproto->pw_passwd = (char *)NULL;

	/* uid */
	__ypproto->pw_uid = pw->pw_uid;

	/* gid */
	__ypproto->pw_gid = pw->pw_gid;

	/* change (ignored anyway) */
	__ypproto->pw_change = pw->pw_change;

	/* class (ignored anyway) */
	__ypproto->pw_class = "";

	/* gecos */
	if(pw->pw_gecos && (pw->pw_gecos)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_gecos, ptr, strlen(pw->pw_gecos) + 1);
		__ypproto->pw_gecos = ptr;
		ptr += (strlen(pw->pw_gecos) + 1);
	} else
		__ypproto->pw_gecos = (char *)NULL;
	
	/* dir */
	if(pw->pw_dir && (pw->pw_dir)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_dir, ptr, strlen(pw->pw_dir) + 1);
		__ypproto->pw_dir = ptr;
		ptr += (strlen(pw->pw_dir) + 1);
	} else
		__ypproto->pw_dir = (char *)NULL;

	/* shell */
	if(pw->pw_shell && (pw->pw_shell)[0]) {
		ptr = (char *)ALIGN(ptr);
		bcopy(pw->pw_shell, ptr, strlen(pw->pw_shell) + 1);
		__ypproto->pw_shell = ptr;
		ptr += (strlen(pw->pw_shell) + 1);
	} else
		__ypproto->pw_shell = (char *)NULL;

	/* expire (ignored anyway) */
	__ypproto->pw_expire = pw->pw_expire;

	/* flags */
	__ypflags = _pw_flags;
}

static int
__ypparse(pw, s)
struct passwd *pw;
char *s;
{
	char *bp, *cp;

	/* since this is currently using strsep(), parse it first */
	bp = s;
	pw->pw_name = strsep(&bp, ":\n");
	pw->pw_passwd = strsep(&bp, ":\n");
	if (!(cp = strsep(&bp, ":\n")))
		return 1;
	pw->pw_uid = atoi(cp);
	if (!(cp = strsep(&bp, ":\n")))
		return 1;
	pw->pw_gid = atoi(cp);
	pw->pw_change = 0;
	pw->pw_class = "";
	pw->pw_gecos = strsep(&bp, ":\n");
	pw->pw_dir = strsep(&bp, ":\n");
	pw->pw_shell = strsep(&bp, ":\n");
	pw->pw_expire = 0;

	/* now let the prototype override, if set. */
	if(__ypproto != (struct passwd *)NULL) {
#ifdef YP_OVERRIDE_PASSWD
		if(__ypproto->pw_passwd != (char *)NULL)
			pw->pw_passwd = __ypproto->pw_passwd;
#endif
		if(!(__ypflags & _PASSWORD_NOUID))
			pw->pw_uid = __ypproto->pw_uid;
		if(!(__ypflags & _PASSWORD_NOGID))
			pw->pw_gid = __ypproto->pw_gid;
		if(__ypproto->pw_gecos != (char *)NULL)
			pw->pw_gecos = __ypproto->pw_gecos;
		if(__ypproto->pw_dir != (char *)NULL)
			pw->pw_dir = __ypproto->pw_dir;
		if(__ypproto->pw_shell != (char *)NULL)
			pw->pw_shell = __ypproto->pw_shell;
	}
	return 0;
}
#endif

struct passwd *
getpwent()
{
	DBT key;
	char bf[sizeof(_pw_keynum) + 1];
#ifdef YP
	static char *name = (char *)NULL;
	const char *user, *host, *dom;
	int has_yppw;
#endif

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

#ifdef YP
	has_yppw = __has_yppw();

again:
	if(has_yppw && (__ypmode != YPMODE_NONE)) {
		char *key, *data;
		int keylen, datalen;
		int r, s;

		if(!__ypdomain) {
			if( _yp_check(&__ypdomain) == 0) {
				__ypmode = YPMODE_NONE;
				goto again;
			}
		}
		switch(__ypmode) {
		case YPMODE_FULL:
			if(__ypcurrent) {
				r = yp_next(__ypdomain, "passwd.byname",
					__ypcurrent, __ypcurrentlen,
					&key, &keylen, &data, &datalen);
				free(__ypcurrent);
				if(r != 0) {
					__ypcurrent = NULL;
					__ypmode = YPMODE_NONE;
					if(data)
						free(data);
					data = NULL;
					goto again;
				}
				__ypcurrent = key;
				__ypcurrentlen = keylen;
				bcopy(data, line, datalen);
				free(data);
				data = NULL;
			} else {
				r = yp_first(__ypdomain, "passwd.byname",
					&__ypcurrent, &__ypcurrentlen,
					&data, &datalen);
				if(r != 0) {
					__ypmode = YPMODE_NONE;
					if(data)
						free(data);
					goto again;
				}
				bcopy(data, line, datalen);
				free(data);
				data = NULL;
			}
			break;
		case YPMODE_NETGRP:
			s = getnetgrent(&host, &user, &dom);
			if(s == 0) {	/* end of group */
				endnetgrent();
				__ypmode = YPMODE_NONE;
				goto again;
			}
			if(user && *user) {
				r = yp_match(__ypdomain, "passwd.byname",
					user, strlen(user),
					&data, &datalen);
			} else
				goto again;
			if(r != 0) {
				/*
				 * if the netgroup is invalid, keep looking
				 * as there may be valid users later on.
				 */
				if(data)
					free(data);
				goto again;
			}
			bcopy(data, line, datalen);
			free(data);
			data = (char *)NULL;
			break;
		case YPMODE_USER:
			if(name != (char *)NULL) {
				r = yp_match(__ypdomain, "passwd.byname",
					name, strlen(name),
					&data, &datalen);
				__ypmode = YPMODE_NONE;
				free(name);
				name = (char *)NULL;
				if(r != 0) {
					if(data)
						free(data);
					goto again;
				}
				bcopy(data, line, datalen);
				free(data);
				data = (char *)NULL;
			} else {		/* XXX */
				__ypmode = YPMODE_NONE;
				goto again;
			}
			break;
		}

		line[datalen] = '\0';
		if (__ypparse(&_pw_passwd, line))
			goto again;
		return &_pw_passwd;
	}
#endif

	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(_pw_keynum) + 1;
	if(__hashpw(&key)) {
#ifdef YP
		/* if we don't have YP at all, don't bother. */
		if(has_yppw) {
			if(_pw_passwd.pw_name[0] == '+') {
				/* set the mode */
				switch(_pw_passwd.pw_name[1]) {
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

				/* save the prototype */
				__ypproto_set();
				goto again;
			} else if(_pw_passwd.pw_name[0] == '-') {
				/* an attempted exclusion */
				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					break;
				case '@':
					setnetgrent(_pw_passwd.pw_name + 2);
					while(getnetgrent(&host, &user, &dom)) {
						if(user && *user)
							__ypexclude_add(user);
					}
					endnetgrent();
					break;
				default:
					__ypexclude_add(_pw_passwd.pw_name + 1);
					break;
				}
				goto again;
			}
		}
#endif
		return &_pw_passwd;
	}
	return (struct passwd *)NULL;
}

#ifdef YP

/*
 * See if the YP token is in the database.  Only works if pwd_mkdb knows
 * about the token.
 */
static int
__has_yppw()
{
	DBT key, data;
	DBT pkey, pdata;
	int len;
	char bf[UT_NAMESIZE];

	key.data = (u_char *)__yp_token;
	key.size = strlen(__yp_token);

	/* Pre-token database support. */
	bf[0] = _PW_KEYBYNAME;
	len = strlen("+");
	bcopy("+", bf + 1, MIN(len, UT_NAMESIZE));
	pkey.data = (u_char *)bf;
	pkey.size = len + 1;

	if ((_pw_db->get)(_pw_db, &key, &data, 0)
	    && (_pw_db->get)(_pw_db, &pkey, &pdata, 0))
		return(0);	/* No YP. */
	return(1);
}
#endif

struct passwd *
getpwnam(name)
	const char *name;
{
	DBT key;
	int len, rval;
	char bf[UT_NAMESIZE + 1];

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

#ifdef YP
	/*
	 * If YP is active, we must sequence through the passwd file
	 * in sequence.
	 */
	if (__has_yppw()) {
		int r;
		int s = -1;
		const char *host, *user, *dom;

		for(_pw_keynum=1; _pw_keynum; _pw_keynum++) {
			bf[0] = _PW_KEYBYNUM;
			bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
			key.data = (u_char *)bf;
			key.size = sizeof(_pw_keynum) + 1;
			if(__hashpw(&key) == 0)
				break;
			switch(_pw_passwd.pw_name[0]) {
			case '+':
				if(!__ypdomain) {
					if(_yp_check(&__ypdomain) == 0) {
						continue;
					}
				}
				/* save the prototype */
				__ypproto_set();

				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					if(__ypcurrent) {
						free(__ypcurrent);
						__ypcurrent = NULL;
					}
					r = yp_match(__ypdomain,
						"passwd.byname",
						name, strlen(name),
						&__ypcurrent, &__ypcurrentlen);
					if(r != 0) {
						if(__ypcurrent)
							free(__ypcurrent);
						__ypcurrent = NULL;
						continue;
					}
					break;
				case '@':
pwnam_netgrp:
					if(__ypcurrent) {
						free(__ypcurrent);
						__ypcurrent = NULL;
					}
					if(s == -1)	/* first time */
						setnetgrent(_pw_passwd.pw_name + 2);
					s = getnetgrent(&host, &user, &dom);
					if(s == 0) {	/* end of group */
						endnetgrent();
						s = -1;
						continue;
					} else {
						if(user && *user) {
							r = yp_match(__ypdomain,
							    "passwd.byname",
							    user, strlen(user),
							    &__ypcurrent,
							    &__ypcurrentlen);
						} else
							goto pwnam_netgrp;
						if(r != 0) {
							if(__ypcurrent)
							    free(__ypcurrent);
							__ypcurrent = NULL;
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
					if(__ypcurrent) {
						free(__ypcurrent);
						__ypcurrent = NULL;
					}
					user = _pw_passwd.pw_name + 1;
					r = yp_match(__ypdomain,
						"passwd.byname",
						user, strlen(user),
						&__ypcurrent,
						&__ypcurrentlen);
					if(r != 0) {
						if(__ypcurrent)
							free(__ypcurrent);
						__ypcurrent = NULL;
						continue;
					}
					break;
				}
				bcopy(__ypcurrent, line, __ypcurrentlen);
				line[__ypcurrentlen] = '\0';
				if(__ypparse(&_pw_passwd, line)
				   || __ypexclude_is(_pw_passwd.pw_name)) {
					if(s == 1)	/* inside netgrp */
						goto pwnam_netgrp;
					continue;
				}
				break;
			case '-':
				/* attempted exclusion */
				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					break;
				case '@':
					setnetgrent(_pw_passwd.pw_name + 2);
					while(getnetgrent(&host, &user, &dom)) {
						if(user && *user)
							__ypexclude_add(user);
					}
					endnetgrent();
					break;
				default:
					__ypexclude_add(_pw_passwd.pw_name + 1);
					break;
				}
				break;
			}
			if(strcmp(_pw_passwd.pw_name, name) == 0) {
				if (!_pw_stayopen) {
					(void)(_pw_db->close)(_pw_db);
					_pw_db = (DB *)NULL;
				}
				if(__ypexclude != (DB *)NULL) {
					(void)(__ypexclude->close)(__ypexclude);
					__ypexclude = (DB *)NULL;
				}
				__ypproto = (struct passwd *)NULL;
				return &_pw_passwd;
			}
			if(s == 1)	/* inside netgrp */
				goto pwnam_netgrp;
			continue;
		}
		if (!_pw_stayopen) {
			(void)(_pw_db->close)(_pw_db);
			_pw_db = (DB *)NULL;
		}
		if(__ypexclude != (DB *)NULL) {
			(void)(__ypexclude->close)(__ypexclude);
			__ypexclude = (DB *)NULL;
		}
		__ypproto = (struct passwd *)NULL;
		return (struct passwd *)NULL;
	}
#endif /* YP */

	bf[0] = _PW_KEYBYNAME;
	len = strlen(name);
	bcopy(name, bf + 1, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;
	rval = __hashpw(&key);

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
	return(rval ? &_pw_passwd : (struct passwd *)NULL);
}

struct passwd *
#ifdef __STDC__
getpwuid(uid_t uid)
#else
getpwuid(uid)
	int uid;
#endif
{
	DBT key;
	char bf[sizeof(_pw_keynum) + 1];
	int keyuid, rval;

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

#ifdef YP
	/*
	 * If YP is active, we must sequence through the passwd file
	 * in sequence.
	 */
	if (__has_yppw()) {
		char uidbuf[20];
		int r;
		int s = -1;
		const char *host, *user, *dom;

		sprintf(uidbuf, "%d", uid);
		for(_pw_keynum=1; _pw_keynum; _pw_keynum++) {
			bf[0] = _PW_KEYBYNUM;
			bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
			key.data = (u_char *)bf;
			key.size = sizeof(_pw_keynum) + 1;
			if(__hashpw(&key) == 0)
				break;
			switch(_pw_passwd.pw_name[0]) {
			case '+':
				if(!__ypdomain) {
					if(_yp_check(&__ypdomain) == 0) {
						continue;
					}
				}
				/* save the prototype */
				__ypproto_set();

				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					if(__ypcurrent) {
						free(__ypcurrent);
						__ypcurrent = NULL;
					}
					r = yp_match(__ypdomain, "passwd.byuid",
						uidbuf, strlen(uidbuf),
						&__ypcurrent, &__ypcurrentlen);
					if(r != 0) {
						if(__ypcurrent)
							free(__ypcurrent);
						__ypcurrent = NULL;
						continue;
					}
					break;
				case '@':
pwuid_netgrp:
					if(__ypcurrent) {
						free(__ypcurrent);
						__ypcurrent = NULL;
					}
					if(s == -1)	/* first time */
						setnetgrent(_pw_passwd.pw_name + 2);
					s = getnetgrent(&host, &user, &dom);
					if(s == 0) {	/* end of group */
						endnetgrent();
						s = -1;
						continue;
					} else {
						if(user && *user) {
							r = yp_match(__ypdomain,
							    "passwd.byname",
							    user, strlen(user),
							    &__ypcurrent,
							    &__ypcurrentlen);
						} else
							goto pwuid_netgrp;
						if(r != 0) {
							if(__ypcurrent)
							    free(__ypcurrent);
							__ypcurrent = NULL;
							/*
                                                         * just because this
							 * user is bad, doesn't
							 * mean they all are.
							 */
							goto pwuid_netgrp;
						}
					}
					break;
				default:
					if(__ypcurrent) {
						free(__ypcurrent);
						__ypcurrent = NULL;
					}
					user = _pw_passwd.pw_name + 1;
					r = yp_match(__ypdomain,
						"passwd.byname",
						user, strlen(user),
						&__ypcurrent,
						&__ypcurrentlen);
					if(r != 0) {
						if(__ypcurrent)
							free(__ypcurrent);
						__ypcurrent = NULL;
						continue;
					}
					break;
				}
				bcopy(__ypcurrent, line, __ypcurrentlen);
				line[__ypcurrentlen] = '\0';
				if(__ypparse(&_pw_passwd, line)
				   || __ypexclude_is(_pw_passwd.pw_name)) {
					if(s == 1)	/* inside netgroup */
						goto pwuid_netgrp;
					continue;
				}
				break;
			case '-':
				/* attempted exclusion */
				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					break;
				case '@':
					setnetgrent(_pw_passwd.pw_name + 2);
					while(getnetgrent(&host, &user, &dom)) {
						if(user && *user)
							__ypexclude_add(user);
					}
					endnetgrent();
					break;
				default:
					__ypexclude_add(_pw_passwd.pw_name + 1);
					break;
				}
				break;
			}
			if( _pw_passwd.pw_uid == uid) {
				if (!_pw_stayopen) {
					(void)(_pw_db->close)(_pw_db);
					_pw_db = (DB *)NULL;
				}
				if (__ypexclude != (DB *)NULL) {
					(void)(__ypexclude->close)(__ypexclude);
					__ypexclude = (DB *)NULL;
				}
				__ypproto = NULL;
				return &_pw_passwd;
			}
			if(s == 1)	/* inside netgroup */
				goto pwuid_netgrp;
			continue;
		}
		if (!_pw_stayopen) {
			(void)(_pw_db->close)(_pw_db);
			_pw_db = (DB *)NULL;
		}
		if(__ypexclude != (DB *)NULL) {
			(void)(__ypexclude->close)(__ypexclude);
			__ypexclude = (DB *)NULL;
		}
		__ypproto = (struct passwd *)NULL;
		return (struct passwd *)NULL;
	}
#endif /* YP */

	bf[0] = _PW_KEYBYUID;
	keyuid = uid;
	bcopy(&keyuid, bf + 1, sizeof(keyuid));
	key.data = (u_char *)bf;
	key.size = sizeof(keyuid) + 1;
	rval = __hashpw(&key);

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
	return(rval ? &_pw_passwd : (struct passwd *)NULL);
}

int
setpassent(stayopen)
	int stayopen;
{
	_pw_keynum = 0;
	_pw_stayopen = stayopen;
#ifdef YP
	__ypmode = YPMODE_NONE;
	if(__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	if(__ypexclude != (DB *)NULL) {
		(void)(__ypexclude->close)(__ypexclude);
		__ypexclude = (DB *)NULL;
	}
	__ypproto = (struct passwd *)NULL;
#endif
	return(1);
}

void
setpwent()
{
	(void) setpassent(0);
}

void
endpwent()
{
	_pw_keynum = 0;
	if (_pw_db) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
#ifdef YP
	__ypmode = YPMODE_NONE;
	if(__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	if(__ypexclude != (DB *)NULL) {
		(void)(__ypexclude->close)(__ypexclude);
		__ypexclude = (DB *)NULL;
	}
	__ypproto = (struct passwd *)NULL;
#endif
}

static int
__initdb()
{
	static int warned;
	char *p;

#ifdef YP
	__ypmode = YPMODE_NONE;
#endif
	p = (geteuid()) ? _PATH_MP_DB : _PATH_SMP_DB;
	_pw_db = dbopen(p, O_RDONLY, 0, DB_HASH, NULL);
	if (_pw_db)
		return(1);
	if (!warned)
		syslog(LOG_ERR, "%s: %m", p);
	warned = 1;
	return(0);
}

static int
__hashpw(key)
	DBT *key;
{
	register char *p, *t;
	static u_int max;
	static char *line;
	DBT data;

	if ((_pw_db->get)(_pw_db, key, &data, 0))
		return(0);
	p = (char *)data.data;
	if (data.size > max && !(line = realloc(line, (max += 1024))))
		return(0);

	t = line;
#define	EXPAND(e)	e = t; while ((*t++ = *p++));
	EXPAND(_pw_passwd.pw_name);
	EXPAND(_pw_passwd.pw_passwd);
	bcopy(p, (char *)&_pw_passwd.pw_uid, sizeof(int));
	p += sizeof(int);
	bcopy(p, (char *)&_pw_passwd.pw_gid, sizeof(int));
	p += sizeof(int);
	bcopy(p, (char *)&_pw_passwd.pw_change, sizeof(time_t));
	p += sizeof(time_t);
	EXPAND(_pw_passwd.pw_class);
	EXPAND(_pw_passwd.pw_gecos);
	EXPAND(_pw_passwd.pw_dir);
	EXPAND(_pw_passwd.pw_shell);
	bcopy(p, (char *)&_pw_passwd.pw_expire, sizeof(time_t));
	p += sizeof(time_t);

	/* See if there's any data left.  If so, read in flags. */
	if (data.size > (p - (char *)data.data)) {
		bcopy(p, (char *)&_pw_flags, sizeof(int));
		p += sizeof(int);
	} else
		_pw_flags = _PASSWORD_NOUID|_PASSWORD_NOGID;	/* default */

	return(1);
}
