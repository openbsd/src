/*	$OpenBSD: util.c,v 1.4 1996/07/13 11:01:35 mickey Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] =
    "@(#) Header: util.c,v 1.48 96/06/23 02:26:42 leres Exp (LBL)";
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <ctype.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stdio.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdlib.h>
#include <string.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <unistd.h>

#include "interface.h"

/*
 * Print out a filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_print(register const u_char *s, register const u_char *ep)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	putchar('"');
	while (ep == NULL || s < ep) {
		c = *s++;
		if (c == '\0') {
			ret = 0;
			break;
		}
		if (!isascii(c)) {
			c = toascii(c);
			putchar('M');
			putchar('-');
		}
		if (!isprint(c)) {
			c ^= 0x40;	/* DEL to ?, others to alpha */
			putchar('^');
		}
		putchar(c);
	}
	putchar('"');
	return(ret);
}

/*
 * Print out a counted filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_printn(register const u_char *s, register u_int n,
	  register const u_char *ep)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	putchar('"');
	while (ep == NULL || s < ep) {
		if (n-- <= 0) {
			ret = 0;
			break;
		}
		c = *s++;
		if (!isascii(c)) {
			c = toascii(c);
			putchar('M');
			putchar('-');
		}
		if (!isprint(c)) {
			c ^= 0x40;	/* DEL to ?, others to alpha */
			putchar('^');
		}
		putchar(c);
	}
	putchar('"');
	return(ret);
}

/*
 * Print the timestamp
 */
void
ts_print(register const struct timeval *tvp)
{
	register int s;

	if (tflag > 0) {
		/* Default */
		s = (tvp->tv_sec + thiszone) % 86400;
		(void)printf("%02d:%02d:%02d.%06u ",
		    s / 3600, (s % 3600) / 60, s % 60, (u_int32_t)tvp->tv_usec);
	} else if (tflag < 0) {
		/* Unix timeval style */
		(void)printf("%u.%06u ",
		    (u_int32_t)tvp->tv_sec, (u_int32_t)tvp->tv_usec);
	}
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 */
const char *
tok2str(register const struct tok *lp, register const char *fmt,
	register int v)
{
	static char buf[128];

	while (lp->s != NULL) {
		if (lp->v == v)
			return (lp->s);
		++lp;
	}
	if (fmt == NULL)
		fmt = "#%d";
	(void)sprintf(buf, fmt, v);
	return (buf);
}


/* VARARGS */
__dead void
#if __STDC__
error(const char *fmt, ...)
#else
error(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void)fprintf(stderr, "%s: ", program_name);
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	exit(1);
	/* NOTREACHED */
}

/* VARARGS */
void
#if __STDC__
warning(const char *fmt, ...)
#else
warning(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void)fprintf(stderr, "%s: warning: ", program_name);
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}

/*
 * Copy arg vector into a new buffer, concatenating arguments with spaces.
 */
char *
copy_argv(register char **argv)
{
	register char **p;
	register int len = 0;
	char *buf;
	char *src, *dst;

	p = argv;
	if (*p == 0)
		return 0;

	while (*p)
		len += strlen(*p++) + 1;

	buf = (char *)malloc(len);
	if (buf == NULL)
		error("copy_argv: malloc");

	p = argv;
	dst = buf;
	while ((src = *p++) != NULL) {
		while ((*dst++ = *src++) != '\0')
			;
		dst[-1] = ' ';
	}
	dst[-1] = '\0';

	return buf;
}

/* A replacement for strdup() that cuts down on malloc() overhead */
char *
savestr(register const char *str)
{
	register u_int size;
	register char *p;
	static char *strptr = NULL;
	static u_int strsize = 0;

	size = strlen(str) + 1;
	if (size > strsize) {
		strsize = 1024;
		if (strsize < size)
			strsize = size;
		strptr = (char *)malloc(strsize);
		if (strptr == NULL)
			error("savestr: malloc");
	}
	(void)strcpy(strptr, str);
	p = strptr;
	strptr += size;
	strsize -= size;
	return (p);
}


char *
read_infile(char *fname)
{
	struct stat buf;
	int fd;
	char *p;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		error("can't open '%s'", fname);

	if (fstat(fd, &buf) < 0)
		error("can't stat '%s'", fname);

	p = malloc((u_int)buf.st_size + 1);
	if (p == NULL)
		error("read_infile: malloc");
	if (read(fd, p, (int)buf.st_size) != buf.st_size)
		error("problem reading '%s'", fname);

	p[(int)buf.st_size] = '\0';

	return p;
}

int32_t
gmt2local()
{
	register int t;
#if !defined(HAVE_ALTZONE) && !defined(HAVE_TIMEZONE)
	struct timeval tv;
	struct timezone tz;
	register struct tm *tm;
#endif

	t = 0;
#if !defined(HAVE_ALTZONE) && !defined(HAVE_TIMEZONE)
	if (gettimeofday(&tv, &tz) < 0)
		error("gettimeofday");
	tm = localtime((time_t *)&tv.tv_sec);
#ifdef HAVE_TM_GMTOFF
	t = tm->tm_gmtoff;
#else
	t = tz.tz_minuteswest * -60;
	/* XXX Some systems need this, some auto offset tz_minuteswest... */
	if (tm->tm_isdst)
		t += 60 * 60;
#endif
#endif

#ifdef HAVE_TIMEZONE
	tzset();
	t = -timezone;
	if (daylight)
		t += 60 * 60;
#endif

#ifdef HAVE_ALTZONE
	tzset();
	t = -altzone;
#endif

	return (t);
}
