/*	$OpenBSD: yplog.c,v 1.3 1996/05/30 09:53:03 deraadt Exp $ */

/*
 * Copyright (c) 1996 Charles D. Cranor
 * All rights reserved.
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
 *      This product includes software developed by Charles D. Cranor.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * yplog.c: replacement yplog routines for 
 * Mats O Jansson's ypserv program, as added by
 * Chuck Cranor.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "yplog.h"

static FILE	*logfp = NULL;		/* the log file */

/*
 * yplog(): like a printf, but to the log file.   does the flush
 * and data for you.
 */

void
#if __STDC__
yplog(const char *fmt, ...)
#else
yplog(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vyplog(fmt, ap);
	va_end(ap);
}

/*
 * vyplog() support routine for yplog()
 */

void
vyplog(fmt, ap)
	register const char *fmt;
	va_list ap;
{
        time_t t;

	if (!logfp) return;
	(void)time(&t);
	fprintf(logfp,"%.15s ", ctime(&t) + 4);
	vfprintf(logfp, fmt, ap);
	fprintf(logfp,"\n");
	fflush(logfp);
}

/*
 * open log
 */

void
ypopenlog()
{
	logfp = fopen("/var/yp/ypserv.log", "a");
	if (!logfp) return;
	yplog("yplog opened");
}

/*
 * close log
 */

void
ypcloselog()
{
	if (logfp) {
		yplog("yplog closed");
		fclose(logfp);
		logfp = NULL;
	}
}
