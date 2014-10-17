/*	$OpenBSD: util.c,v 1.26 2014/10/17 20:16:13 millert Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 * Portions Copyright (c) 1983, 1995, 1996 Eric P. Allman (woof!)
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <paths.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <vis.h>
#include <err.h>
#include "finger.h"
#include "extern.h"

char	*estrdup(char *);
WHERE	*walloc(PERSON *pn);
void	find_idle_and_ttywrite(WHERE *);
void	userinfo(PERSON *, struct passwd *);

struct storage {
	struct storage *next;
	char a[1];
};

void
free_storage(struct storage *st)
{
	struct storage *nx;

	while (st != NULL) {
		nx = st->next;
		free(st);
		st = nx;
	}
}

void
find_idle_and_ttywrite(WHERE *w)
{
	struct stat sb;

	(void)snprintf(tbuf, sizeof(tbuf), "%s%s", _PATH_DEV, w->tty);
	if (stat(tbuf, &sb) < 0) {
		/* Don't bitch about it, just handle it... */
		w->idletime = 0;
		w->writable = 0;

		return;
	}
	w->idletime = now < sb.st_atime ? 0 : now - sb.st_atime;

#define	TALKABLE	0220		/* tty is writable if 220 mode */
	w->writable = ((sb.st_mode & TALKABLE) == TALKABLE);
}

char *
estrdup(char *s)
{
	char *p = strdup(s);
	if (!p)
		err(1, "strdup");
	return (p);
}

void
userinfo(PERSON *pn, struct passwd *pw)
{
	char *p;
	char *bp, name[1024];
	struct stat sb;

	pn->realname = pn->office = pn->officephone = pn->homephone = NULL;

	pn->uid = pw->pw_uid;
	pn->name = estrdup(pw->pw_name);
	pn->dir = estrdup(pw->pw_dir);
	pn->shell = estrdup(pw->pw_shell);

	(void)strlcpy(bp = tbuf, pw->pw_gecos, sizeof(tbuf));

	/* ampersands get replaced by the login name */
	if (!(p = strsep(&bp, ",")))
		return;
	expandusername(p, pw->pw_name, name, sizeof(name));
	pn->realname = estrdup(name);
	pn->office = ((p = strsep(&bp, ",")) && *p) ?
	    estrdup(p) : NULL;
	pn->officephone = ((p = strsep(&bp, ",")) && *p) ?
	    estrdup(p) : NULL;
	pn->homephone = ((p = strsep(&bp, ",")) && *p) ?
	    estrdup(p) : NULL;
	(void)snprintf(tbuf, sizeof(tbuf), "%s/%s", _PATH_MAILSPOOL,
	    pw->pw_name);
	pn->mailrecv = -1;		/* -1 == not_valid */
	if (stat(tbuf, &sb) < 0) {
		if (errno != ENOENT) {
			warn("%s", tbuf);
			return;
		}
	} else if (sb.st_size != 0) {
		pn->mailrecv = sb.st_mtime;
		pn->mailread = sb.st_atime;
	}
}

int
match(struct passwd *pw, char *user)
{
	char *p, *t;
	char name[1024];

	(void)strlcpy(p = tbuf, pw->pw_gecos, sizeof(tbuf));

	/* ampersands get replaced by the login name */
	if (!(p = strtok(p, ",")))
		return (0);
	expandusername(p, pw->pw_name, name, sizeof(name));
	for (t = name; (p = strtok(t, "\t ")) != NULL; t = NULL)
		if (!strcasecmp(p, user))
			return (1);
	return (0);
}

/* inspired by usr.sbin/sendmail/util.c::buildfname */
void
expandusername(char *gecos, char *login, char *buf, int buflen)
{
	char *p, *bp;

	/* why do we skip asterisks!?!? */
	if (*gecos == '*')
		gecos++;
	bp = buf;

	/* copy gecos, interpolating & to be full name */
	for (p = gecos; *p != '\0'; p++) {
		if (bp >= &buf[buflen - 1]) {
			/* buffer overflow - just use login name */
			strlcpy(buf, login, buflen);
			buf[buflen - 1] = '\0';
			return;
		}
		if (*p == '&') {
			/* interpolate full name */
			strlcpy(bp, login, buflen - (bp - buf));
			*bp = toupper((unsigned char)*bp);
			bp += strlen(bp);
		}
		else
			*bp++ = *p;
	}
	*bp = '\0';
}

void
enter_lastlog(PERSON *pn)
{
	WHERE *w;
	static int opened, fd;
	struct lastlog ll;
	char doit = 0;

	/* some systems may not maintain lastlog, don't report errors. */
	if (!opened) {
		fd = open(_PATH_LASTLOG, O_RDONLY);
		opened = 1;
	}
	if (fd == -1 ||
	    lseek(fd, (off_t)(pn->uid * sizeof(ll)), SEEK_SET) !=
	    (long)(pn->uid * sizeof(ll)) ||
	    read(fd, (char *)&ll, sizeof(ll)) != sizeof(ll)) {
			/* as if never logged in */
			ll.ll_line[0] = ll.ll_host[0] = '\0';
			ll.ll_time = 0;
		}
	if ((w = pn->whead) == NULL)
		doit = 1;
	else if (ll.ll_time != 0) {
		/* if last login is earlier than some current login */
		for (; !doit && w != NULL; w = w->next)
			if (w->info == LOGGEDIN && w->loginat < ll.ll_time)
				doit = 1;
		/*
		 * and if it's not any of the current logins
		 * can't use time comparison because there may be a small
		 * discrepency since login calls time() twice
		 */
		for (w = pn->whead; doit && w != NULL; w = w->next)
			if (w->info == LOGGEDIN &&
			    strncmp(w->tty, ll.ll_line, UT_LINESIZE) == 0)
				doit = 0;
	}
	if (doit) {
		w = walloc(pn);
		w->info = LASTLOG;
		bcopy(ll.ll_line, w->tty, UT_LINESIZE);
		w->tty[UT_LINESIZE] = 0;
		bcopy(ll.ll_host, w->host, UT_HOSTSIZE);
		w->host[UT_HOSTSIZE] = 0;
		w->loginat = ll.ll_time;
	}
}

void
enter_where(struct utmp *ut, PERSON *pn)
{
	WHERE *w = walloc(pn);

	w->info = LOGGEDIN;
	bcopy(ut->ut_line, w->tty, UT_LINESIZE);
	w->tty[UT_LINESIZE] = 0;
	bcopy(ut->ut_host, w->host, UT_HOSTSIZE);
	w->host[UT_HOSTSIZE] = 0;
	w->loginat = (time_t)ut->ut_time;
	find_idle_and_ttywrite(w);
}

PERSON *
enter_person(struct passwd *pw)
{
	PERSON *pn, **pp;

	for (pp = htab + hash(pw->pw_name);
	    *pp != NULL && strcmp((*pp)->name, pw->pw_name) != 0;
	    pp = &(*pp)->hlink)
		;
	if ((pn = *pp) == NULL) {
		pn = palloc();
		entries++;
		if (phead == NULL)
			phead = ptail = pn;
		else {
			ptail->next = pn;
			ptail = pn;
		}
		pn->next = NULL;
		pn->hlink = NULL;
		*pp = pn;
		userinfo(pn, pw);
		pn->whead = NULL;
	}
	return (pn);
}

PERSON *
find_person(char *name)
{
	PERSON *pn;

	/* name may be only UT_NAMESIZE long and not terminated */
	for (pn = htab[hash(name)];
	    pn != NULL && strncmp(pn->name, name, UT_NAMESIZE) != 0;
	    pn = pn->hlink)
		;
	return (pn);
}

int
hash(char *name)
{
	int h, i;

	h = 0;
	/* name may be only UT_NAMESIZE long and not terminated */
	for (i = UT_NAMESIZE; --i >= 0 && *name;)
		h = ((h << 2 | h >> (HBITS - 2)) ^ *name++) & HMASK;
	return (h);
}

PERSON *
palloc(void)
{
	PERSON *p;

	if ((p = (PERSON *)malloc((u_int) sizeof(PERSON))) == NULL)
		err(1, "malloc");
	return (p);
}

WHERE *
walloc(PERSON *pn)
{
	WHERE *w;

	if ((w = (WHERE *)malloc((u_int) sizeof(WHERE))) == NULL)
		err(1, "malloc");
	if (pn->whead == NULL)
		pn->whead = pn->wtail = w;
	else {
		pn->wtail->next = w;
		pn->wtail = w;
	}
	w->next = NULL;
	return (w);
}

char *
prphone(char *num)
{
	char *p;
	int len;
	static char pbuf[15];

	/* don't touch anything if the user has their own formatting */
	for (p = num; *p; ++p)
		if (!isdigit((unsigned char)*p))
			return (num);
	len = p - num;
	p = pbuf;
	switch (len) {
	case 11:			/* +0-123-456-7890 */
		*p++ = '+';
		*p++ = *num++;
		*p++ = '-';
		/* FALLTHROUGH */
	case 10:			/* 012-345-6789 */
		*p++ = *num++;
		*p++ = *num++;
		*p++ = *num++;
		*p++ = '-';
		/* FALLTHROUGH */
	case 7:				/* 012-3456 */
		*p++ = *num++;
		*p++ = *num++;
		*p++ = *num++;
		break;
	case 5:				/* x0-1234 */
	case 4:				/* x1234 */
		*p++ = 'x';
		*p++ = *num++;
		break;
	default:
		return (num);
	}
	if (len != 4) {
		*p++ = '-';
		*p++ = *num++;
	}
	*p++ = *num++;
	*p++ = *num++;
	*p++ = *num++;
	*p = '\0';
	return (pbuf);
}

/* Like strvis(), but use malloc() to get the space and return a pointer
 * to the beginning of the converted string, not the end.
 *
 * The caller is responsible for free()'ing the returned string.
 */
char *
vs(struct storage **exist, char *src)
{
	char *dst;
	struct storage *n;

	if ((n = malloc(sizeof(struct storage) + 4 * strlen(src) + 1)) == NULL)
		err(1, "malloc failed");
	n->next = *exist;
	*exist = n;

	dst = n->a;

	strvis(dst, src, VIS_SAFE|VIS_NOSLASH);
	return (dst);
}
