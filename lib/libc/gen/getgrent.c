/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1994, Jason Downs. All Rights Reserved.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: getgrent.c,v 1.21 2004/06/07 21:11:23 marc Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>
#include <errno.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"
#endif
#include "thread_private.h"

/* This global storage is locked for the non-rentrant functions */
_THREAD_PRIVATE_KEY(gr_storage);
static struct group_storage {
#define	MAXGRP		200
	char *members[MAXGRP];
#define	MAXLINELENGTH	1024
	char line[MAXLINELENGTH];
} gr_storage;
#define GETGR_R_SIZE_MAX	(1024+200*sizeof(char*))

/* File pointers are locked with the 'gr' mutex */
_THREAD_PRIVATE_KEY(gr);
static FILE *_gr_fp;
static struct group _gr_group;
static int _gr_stayopen;
static int grscan(int, gid_t, const char *, struct group *, struct group_storage *);
static int start_gr(void);
static void endgrent_basic(void);

static struct group *getgrnam_gs(const char *, struct group *,
	struct group_storage *);
static struct group *getgrgid_gs(gid_t, struct group *,
	struct group_storage *);

#ifdef YP
enum _ypmode { YPMODE_NONE, YPMODE_FULL, YPMODE_NAME };
static enum _ypmode __ypmode;
static char	*__ypcurrent, *__ypdomain;
static int	__ypcurrentlen;
#endif

struct group *
getgrent(void)
{
	struct group *p_gr = (struct group*)_THREAD_PRIVATE(gr, _gr_group, NULL);
	struct group_storage *gs = (struct group_storage *)_THREAD_PRIVATE(gr_storage,
	    gr_storage, NULL);

	_THREAD_PRIVATE_MUTEX_LOCK(gr);
	if ((!_gr_fp && !start_gr()) || !grscan(0, 0, NULL, p_gr, gs))
		p_gr = NULL;
	_THREAD_PRIVATE_MUTEX_UNLOCK(gr);
	return (p_gr);
}

static struct group *
getgrnam_gs(const char *name, struct group *p_gr, struct group_storage *gs)
{
	int rval;

	_THREAD_PRIVATE_MUTEX_LOCK(gr);
	if (!start_gr())
		rval = 0;
	else {
		rval = grscan(1, 0, name, p_gr, gs);
		if (!_gr_stayopen)
			endgrent_basic();
	}
	_THREAD_PRIVATE_MUTEX_UNLOCK(gr);
	return(rval ? p_gr : NULL);
}

struct group *
getgrnam(const char *name)
{
	struct group *p_gr = (struct group*)_THREAD_PRIVATE(gr,_gr_group,NULL);
	struct group_storage *gs = (struct group_storage *)_THREAD_PRIVATE(gr_storage,
	    gr_storage, NULL);

	return getgrnam_gs(name, p_gr, gs);
}

int
getgrnam_r(const char *name, struct group *grp, char *buffer,
	size_t bufsize, struct group **result)
{
	int errnosave;
	int ret;

	if (bufsize < GETGR_R_SIZE_MAX)
		return ERANGE;
	errnosave = errno;
	*result = getgrnam_gs(name, grp, (struct group_storage *)buffer);
	if (*result == NULL)
		ret = errno;
	else
		ret = 0;
	errno = errnosave;
	return ret;
}

static struct group *
getgrgid_gs(gid_t gid, struct group *p_gr, struct group_storage *gs)
{
	int rval;

	_THREAD_PRIVATE_MUTEX_LOCK(gr);
	if (!start_gr())
		rval = 0;
	else {
		rval = grscan(1, gid, NULL, p_gr, gs);
		if (!_gr_stayopen)
			endgrent_basic();
	}
	_THREAD_PRIVATE_MUTEX_UNLOCK(gr);
	return(rval ? p_gr : NULL);
}

struct group *
getgrgid(gid_t gid)
{
	struct group *p_gr = (struct group*)_THREAD_PRIVATE(gr, _gr_group, NULL);
	struct group_storage *gs = (struct group_storage *)_THREAD_PRIVATE(gr_storage,
	    gr_storage, NULL);

	return getgrgid_gs(gid, p_gr, gs);
}

int
getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t bufsize,
	struct group **result)
{
	int errnosave;
	int ret;

	if (bufsize < GETGR_R_SIZE_MAX)
		return ERANGE;
	errnosave = errno;
	*result = getgrgid_gs(gid, grp, (struct group_storage *)buffer);
	if (*result == NULL)
		ret = errno;
	else
		ret = 0;
	errno = errnosave;
	return ret;
}

static int
start_gr(void)
{
	if (_gr_fp) {
		rewind(_gr_fp);
#ifdef YP
		__ypmode = YPMODE_NONE;
		if (__ypcurrent)
			free(__ypcurrent);
		__ypcurrent = NULL;
#endif
		return(1);
	}
	return((_gr_fp = fopen(_PATH_GROUP, "r")) ? 1 : 0);
}

void
setgrent(void)
{
	(void) setgroupent(0);
}

int
setgroupent(int stayopen)
{
	int retval;

	_THREAD_PRIVATE_MUTEX_LOCK(gr);
	if (!start_gr())
		retval = 0;
	else {
		_gr_stayopen = stayopen;
		retval = 1;
	}
	_THREAD_PRIVATE_MUTEX_UNLOCK(gr);
	return (retval);
}

static
void
endgrent_basic(void)
{
	if (_gr_fp) {
		(void)fclose(_gr_fp);
		_gr_fp = NULL;
#ifdef YP
		__ypmode = YPMODE_NONE;
		if (__ypcurrent)
			free(__ypcurrent);
		__ypcurrent = NULL;
#endif
	}
}

void
endgrent(void)
{
	_THREAD_PRIVATE_MUTEX_LOCK(gr);
	endgrent_basic();
	_THREAD_PRIVATE_MUTEX_UNLOCK(gr);
}

static int
grscan(int search, gid_t gid, const char *name, struct group *p_gr,
    struct group_storage *gs)
{
	register char *cp, **m;
	char *bp, *endp;
	u_long ul;
#ifdef YP
	char *key, *data;
	int keylen, datalen;
	int r;
	char *grname = (char *)NULL;
#endif
	char **members;
	char *line;

	if (gs == NULL)
		return 0;
	members = gs->members;
	line = gs->line;

	for (;;) {
#ifdef YP
		if (__ypmode != YPMODE_NONE) {

			if (!__ypdomain) {
				if (yp_get_default_domain(&__ypdomain)) {
					__ypmode = YPMODE_NONE;
					if (grname != (char *)NULL) {
						free(grname);
						grname = (char *)NULL;
					}
					continue;
				}
			}
			switch (__ypmode) {
			case YPMODE_FULL:
				if (__ypcurrent) {
					r = yp_next(__ypdomain, "group.byname",
					    __ypcurrent, __ypcurrentlen,
					    &key, &keylen, &data, &datalen);
					free(__ypcurrent);
					if (r != 0) {
						__ypcurrent = NULL;
						__ypmode = YPMODE_NONE;
						free(data);
						continue;
					}
					__ypcurrent = key;
					__ypcurrentlen = keylen;
					bcopy(data, line, datalen);
					free(data);
				} else {
					r = yp_first(__ypdomain, "group.byname",
					    &__ypcurrent, &__ypcurrentlen,
					    &data, &datalen);
					if (r != 0) {
						__ypmode = YPMODE_NONE;
						free(data);
						continue;
					}
					bcopy(data, line, datalen);
					free(data);
				}
				break;
			case YPMODE_NAME:
				if (grname != (char *)NULL) {
					r = yp_match(__ypdomain, "group.byname",
					    grname, strlen(grname),
					    &data, &datalen);
					__ypmode = YPMODE_NONE;
					free(grname);
					grname = (char *)NULL;
					if (r != 0) {
						free(data);
						continue;
					}
					bcopy(data, line, datalen);
					free(data);
				} else {
					__ypmode = YPMODE_NONE;	/* ??? */
					continue;
				}
				break;
			case YPMODE_NONE:
				/* NOTREACHED */
				break;
			}
			line[datalen] = '\0';
			bp = line;
			goto parse;
		}
#endif
		if (!fgets(line, sizeof(gs->line), _gr_fp))
			return(0);
		bp = line;
		/* skip lines that are too big */
		if (!strchr(line, '\n')) {
			int ch;

			while ((ch = getc(_gr_fp)) != '\n' && ch != EOF)
				;
			continue;
		}
#ifdef YP
		if (line[0] == '+') {
			switch (line[1]) {
			case ':':
			case '\0':
			case '\n':
				if (_yp_check(NULL)) {
					if (!search) {
						__ypmode = YPMODE_FULL;
						continue;
					}
					if (!__ypdomain &&
					    yp_get_default_domain(&__ypdomain))
						continue;
					if (name) {
						r = yp_match(__ypdomain,
						    "group.byname",
						    name, strlen(name),
						    &data, &datalen);
					} else {
						char buf[20];

						snprintf(buf, sizeof buf,
						    "%u", gid);
						r = yp_match(__ypdomain,
						    "group.bygid",
						    buf, strlen(buf),
						    &data, &datalen);
					}
					if (r != 0)
						continue;
					bcopy(data, line, datalen);
					free(data);
					line[datalen] = '\0';
					bp = line;
					p_gr->gr_name = strsep(&bp, ":\n");
					p_gr->gr_passwd =
						strsep(&bp, ":\n");
					if (!(cp = strsep(&bp, ":\n")))
						continue;
					if (name) {
						ul = strtoul(cp, &endp, 10);
						if (*endp != '\0' ||
						    endp == cp || ul >= GID_MAX)
							continue;
						p_gr->gr_gid = ul;
					} else
						p_gr->gr_gid = gid;
					goto found_it;
				}
				break;
			default:
				if (_yp_check(NULL)) {
					register char *tptr;

					tptr = strsep(&bp, ":\n");
					if (search && name && strcmp(tptr, name))
						continue;
					__ypmode = YPMODE_NAME;
					grname = strdup(tptr + 1);
					continue;
				}
				break;
			}
		}
parse:
#endif
		p_gr->gr_name = strsep(&bp, ":\n");
		if (search && name && strcmp(p_gr->gr_name, name))
			continue;
		p_gr->gr_passwd = strsep(&bp, ":\n");
		if (!(cp = strsep(&bp, ":\n")))
			continue;
		ul = strtoul(cp, &endp, 10);
		if (endp == cp || *endp != '\0' || ul >= GID_MAX)
			continue;
		p_gr->gr_gid = ul;
		if (search && name == NULL && p_gr->gr_gid != gid)
			continue;
	found_it:
		cp = NULL;
		if (bp == NULL)
			continue;
		for (m = p_gr->gr_mem = members;; bp++) {
			if (m == &members[MAXGRP - 1])
				break;
			if (*bp == ',') {
				if (cp) {
					*bp = '\0';
					*m++ = cp;
					cp = NULL;
				}
			} else if (*bp == '\0' || *bp == '\n' || *bp == ' ') {
				if (cp) {
					*bp = '\0';
					*m++ = cp;
				}
				break;
			} else if (cp == NULL)
				cp = bp;
		}
		*m = NULL;
		return(1);
	}
	/* NOTREACHED */
}
