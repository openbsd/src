/*
 * $Id: accumlog.c,v 1.1 1998/06/03 08:57:05 beck Exp $
 * 
 * Copyright (c) 1998 Obtuse Systems Corporation <info@obtuse.com> 
 *  Copyright (c) 1998 Simon J. Gerraty <sjg@quick.com.au>
 *  From: accumlog.c,v 1.1 1998/03/29 07:47:02 sjg
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Obtuse Systems 
 *      Corporation and its contributors.
 * 4. Neither the name of the Obtuse Systems Corporation nor the names
 *    of its contributors may be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY OBTUSE SYSTEMS CORPORATION AND
 * CONTRIBUTORS ``AS IS''AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL OBTUSE SYSTEMS CORPORATION OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

/*
 * NAME:
 *	accumlog - append info to log entry
 *
 * SYNOPSIS:
 *	int	accumlog(level, fmt, ...)
 *
 * DESCRIPTION:
 *	If "fmt" is null we flush any accumulated log
 *	to syslog otherwise we just append it to an existing entry.
 *	
 * AUTHOR:
 *	Simon J. Gerraty <sjg@quick.com.au>
 */

/*
 *	@(#)Copyright (c) 1998 Simon J. Gerraty.
 *	
 *	This is free software.	It comes with NO WARRANTY.
 *	Permission to use, modify and distribute this source code 
 *	is granted subject to the following conditions.
 *	1/ that the above copyright notice and this notice 
 *	are preserved in all copies and that due credit be given 
 *	to the author.	
 *	2/ that any changes to this code are clearly commented 
 *	as such so that the author does not get blamed for bugs 
 *	other than his own.
 *	
 *	Please send copies of changes and bug-fixes to:
 *	sjg@quick.com.au
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include	<stdio.h>
#if defined(__STDC__) || defined(__cplusplus)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include	<syslog.h>
#include	<sys/types.h>
#include	<string.h>
#include <sysexits.h>  /* exit codes so smtpd/smtpfwdd can exit properly -BB */
#ifdef HAVE_MALLOC_H
# include	<malloc.h>
#else
extern char *malloc(), *realloc();
#endif

#ifdef MAIN
# define LOG_HUNK 10
#endif

#ifndef LOG_HUNK
# define LOG_HUNK 128
#endif
#ifndef MAX
# define MAX(a, b) (((a) < (b)) ? (b) : (a))
#endif

int
#ifdef __STDC__
accumlog(int level, const char *fmt, ...) {
#else
accumlog(va_alist)
	va_dcl	
{
	int level;
	char *fmt;
#endif
        va_list va;
	static char *log = 0;
	static int lsz = 0;
	static int lx = 0;
	int i, x, space = 0;
		
#ifdef __STDC__
        va_start(va, fmt);
#else
        va_start(va);
        level = va_arg(va, int);
        fmt = va_arg(va, char *);
#endif
	if (log == 0) {
		lsz = 2 * LOG_HUNK;
		if ((log = (char *) malloc(lsz)) == 0) {
			syslog(LOG_ERR, "accumlog: malloc(%d): %m", lsz);
			exit(EX_OSERR);
		}
	}
	if (fmt == 0) {
		if (lx > 0) {
			syslog(level, "%s", log);
			space = lx;
			lx = 0;
		}
		va_end(va);
		return space;		/* how much logged */
	}
	do {
		space = lsz - lx;
		x = vsnprintf(&log[lx], space, fmt, va);
		if (x < 0) {
			syslog(LOG_ERR, "accumlog: vsnprintf(\"%s\", ...): %m", fmt);
			lx = 0;			/* lose */
		}
		if (x > 0 && (i = x + (LOG_HUNK / 2)) > space) {
			lsz += MAX(i, LOG_HUNK);
			if ((log = realloc(log, lsz)) == 0) {
				syslog(LOG_ERR, "accumlog: realloc(%d): %m", lsz);
				exit(EX_OSERR);
			}
				
		}
	} while (x > 0 && x > space) ;

	if (x > 0) {
		lx += x;
		if (log[lx - 1] == '\n')
			lx--;
	}
	
        va_end(va);
	return lx;
}

#ifdef MAIN
int
main(argc, argv)
	int argc;
	char **argv;
{
	int i;
	
	openlog("accumlog", 0, LOG_LOCAL0);
	accumlog(LOG_INFO, "PID=%d\n", getpid()); /* should lose the \n */

	for (i = 1; i < argc; i++)
		accumlog(LOG_INFO, ", argv[%d]='%s'", i, argv[i]);
	accumlog(LOG_INFO, 0);
	exit(EX_OK);
}
#endif
