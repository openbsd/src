/*	$OpenBSD: vprintf.c,v 1.4 1997/08/31 06:57:26 deraadt Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*
 * varargs versions of printf routines
 *
 **********************************************************************
 */

#include <stdio.h>
#include <varargs.h>

#ifdef _IOSTRG
#define STRFLAG		(_IOSTRG|_IOWRT)	/* no _IOWRT: avoid stdio bug */
#else
#define STRFLAG		(_IOREAD)		/* XXX: Assume svr4 stdio */
#endif

#ifdef DOPRINT_VA
/* 
 *  system provides _doprnt_va routine
 */
#define	_doprnt	_doprnt_va
#else
/*
 * system provides _doprnt routine
 */
#define _doprnt_va _doprnt
#endif


#ifdef NEED_VPRINTF
int
vprintf(fmt, args)
	char *fmt;
	va_list args;
{
	_doprnt(fmt, args, stdout);
	return (ferror(stdout) ? EOF : 0);
}

int
vfprintf(f, fmt, args)
	FILE *f;
	char *fmt;
	va_list args;
{
	_doprnt(fmt, args, f);
	return (ferror(f) ? EOF : 0);
}

int
vsprintf(s, fmt, args)
	char *s, *fmt;
	va_list args;
{
	FILE fakebuf;

	fakebuf._flag = STRFLAG;
	fakebuf._ptr = s;
	fakebuf._cnt = 32767;
	_doprnt(fmt, args, &fakebuf);
	putc('\0', &fakebuf);
	return (strlen(s));
}
#endif	/* NEED_VPRINTF */

#if	defined(NEED_VSNPRINTF) || defined(NEED_VPRINTF)
int
vsnprintf(s, n, fmt, args)
	char *s, *fmt;
	va_list args;
{
	FILE fakebuf;
#ifdef HAS_VFPRINTF
	int ret;
#endif

	fakebuf._flag = STRFLAG;
	fakebuf._ptr = s;
	fakebuf._cnt = n-1;
	fakebuf._file = -1;
#ifdef HAS_VFPRINTF
        ret = vfprintf(&fakebuf, fmt, args);
#else
	_doprnt(fmt, args, &fakebuf);
	fakebuf._cnt++;
#endif
	putc('\0', &fakebuf);
	if (fakebuf._cnt<0)
	    fakebuf._cnt = 0;
#ifdef HAS_VFPRINTF
	return(ret);
#else
	return (n-fakebuf._cnt-1);
#endif
}
#endif	/* NEED_VPRINTF || NEED_VSNPRINTF */
