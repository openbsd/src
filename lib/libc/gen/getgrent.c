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
static char rcsid[] = "$OpenBSD: getgrent.c,v 1.8 1997/12/19 09:42:22 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"
#endif

static FILE *_gr_fp;
static struct group _gr_group;
static int _gr_stayopen;
static int grscan __P((int, gid_t, const char *));
static int start_gr __P((void));

#define	MAXGRP		200
static char *members[MAXGRP];
#define	MAXLINELENGTH	1024
static char line[MAXLINELENGTH];

#ifdef YP
enum _ypmode { YPMODE_NONE, YPMODE_FULL, YPMODE_NAME };
static enum _ypmode __ypmode;
static char	*__ypcurrent, *__ypdomain;
static int	__ypcurrentlen;
#endif

struct group *
getgrent()
{
	if ((!_gr_fp && !start_gr()) || !grscan(0, 0, NULL))
		return(NULL);
	return(&_gr_group);
}

struct group *
getgrnam(name)
	const char *name;
{
	int rval;

	if (!start_gr())
		return(NULL);
	rval = grscan(1, 0, name);
	if (!_gr_stayopen)
		endgrent();
	return(rval ? &_gr_group : NULL);
}

struct group *
#ifdef __STDC__
getgrgid(gid_t gid)
#else
getgrgid(gid)
	gid_t gid;
#endif
{
	int rval;

	if (!start_gr())
		return(NULL);
	rval = grscan(1, gid, NULL);
	if (!_gr_stayopen)
		endgrent();
	return(rval ? &_gr_group : NULL);
}

static int
start_gr()
{
	if (_gr_fp) {
		rewind(_gr_fp);
#ifdef YP
		__ypmode = YPMODE_NONE;
		if(__ypcurrent)
			free(__ypcurrent);
		__ypcurrent = NULL;
#endif
		return(1);
	}
	return((_gr_fp = fopen(_PATH_GROUP, "r")) ? 1 : 0);
}

void
setgrent()
{
	(void) setgroupent(0);
}

int
setgroupent(stayopen)
	int stayopen;
{
	if (!start_gr())
		return(0);
	_gr_stayopen = stayopen;
	return(1);
}

void
endgrent()
{
	if (_gr_fp) {
		(void)fclose(_gr_fp);
		_gr_fp = NULL;
#ifdef YP
		__ypmode = YPMODE_NONE;
		if(__ypcurrent)
			free(__ypcurrent);
		__ypcurrent = NULL;
#endif
	}
}

static int
grscan(search, gid, name)
	register int search;
	register gid_t gid;
	register const char *name;
{
	register char *cp, **m;
	char *bp;
#ifdef YP
	char *key, *data;
	int keylen, datalen;
	int r;
	char *grname = (char *)NULL;
#endif

	for (;;) {
#ifdef YP
		if(__ypmode != YPMODE_NONE) {

			if(!__ypdomain) {
				if(yp_get_default_domain(&__ypdomain)) {
					__ypmode = YPMODE_NONE;
					if(grname != (char *)NULL) {
						free(grname);
						grname = (char *)NULL;
					}
					continue;
				}
			}
			switch(__ypmode) {
			case YPMODE_FULL:
				if(__ypcurrent) {
					r = yp_next(__ypdomain, "group.byname",
						__ypcurrent, __ypcurrentlen,
						&key, &keylen, &data, &datalen);
					free(__ypcurrent);
					if(r != 0) {
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
					if(r != 0) {
						__ypmode = YPMODE_NONE;
						free(data);
						continue;
					}
					bcopy(data, line, datalen);
					free(data);
				}
				break;
			case YPMODE_NAME:
				if(grname != (char *)NULL) {
					r = yp_match(__ypdomain, "group.byname",
						grname, strlen(grname),
						&data, &datalen);
					__ypmode = YPMODE_NONE;
					free(grname);
					grname = (char *)NULL;
					if(r != 0) {
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
			}
			line[datalen] = '\0';
			bp = line;
			goto parse;
		}
#endif
		if (!fgets(line, sizeof(line), _gr_fp))
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
			switch(line[1]) {
			case ':':
			case '\0':
			case '\n':
				if(_yp_check(NULL)) {
					if (!search) {
						__ypmode = YPMODE_FULL;
						continue;
					}
					if(!__ypdomain &&
					   yp_get_default_domain(&__ypdomain))
						continue;
					if (name) {
						r = yp_match(__ypdomain,
							     "group.byname",
							     name, strlen(name),
							     &data, &datalen);
					} else {
						char buf[20];
						sprintf(buf, "%u", gid);
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
					_gr_group.gr_name = strsep(&bp, ":\n");
					_gr_group.gr_passwd =
						strsep(&bp, ":\n");
					if (!(cp = strsep(&bp, ":\n")))
						continue;
					_gr_group.gr_gid =
						name ? atoi(cp) : gid;
					goto found_it;
				}
				break;
			default:
				if(_yp_check(NULL)) {
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
		_gr_group.gr_name = strsep(&bp, ":\n");
		if (search && name && strcmp(_gr_group.gr_name, name))
			continue;
		_gr_group.gr_passwd = strsep(&bp, ":\n");
		if (!(cp = strsep(&bp, ":\n")))
			continue;
		_gr_group.gr_gid = atoi(cp);
		if (search && name == NULL && _gr_group.gr_gid != gid)
			continue;
	found_it:
		cp = NULL;
		if (bp == NULL)
			continue;
		for (m = _gr_group.gr_mem = members;; bp++) {
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
