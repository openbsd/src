/* $OpenBSD: pwarnx.c,v 1.4 2003/08/21 20:24:57 espie Exp $ */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "lib.h"

static char pkgname[60];

void
set_pkg(name)
    char *name;
{

    char *name2;

    if (name != NULL) {
    	size_t len;
	
	len = strlen(name);
	while (len != 0 && name[len-1] == '/')
		name[--len] = '\0';

    	name2 = strrchr(name, '/');
	if (name2 != NULL) 
		name = name2+1;
	strlcpy(pkgname, name, sizeof pkgname);
	name2 = strstr(pkgname, ".tgz");
	if (name2 != NULL && name2[4] == '\0')
		*name2 = '\0';
    } else
    	pkgname[0] = '\0';
}


extern char *__progname;

void 
pwarnx(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (pkgname[0] == '\0')
	    (void)fprintf(stderr, "%s: ", __progname);
	else
	    (void)fprintf(stderr, "%s(%s): ", __progname, pkgname);
	if (fmt != NULL)
		(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
	va_end(ap);
}

void 
pwarn(const char *fmt, ...)
{
	va_list ap;
	int sverrno;

	sverrno = errno;
	va_start(ap, fmt);

	if (pkgname[0] == '\0')
	    (void)fprintf(stderr, "%s: ", __progname);
	else
	    (void)fprintf(stderr, "%s(%s): ", __progname, pkgname);
	if (fmt != NULL) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(sverrno));
	(void)fprintf(stderr, "\n");
	va_end(ap);
}
