/*	$OpenBSD: panic.c,v 1.10 2002/05/14 18:05:39 millert Exp $	*/
/*	$NetBSD: panic.c,v 1.2 1995/03/25 18:13:33 glass Exp $	*/

/*
 * panic.c - terminate fast in case of error
 * Copyright (c) 1993 by Thomas Koenig
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "at.h"
#include "panic.h"
#include "privs.h"

#ifndef lint
static const char rcsid[] = "$OpenBSD: panic.c,v 1.10 2002/05/14 18:05:39 millert Exp $";
#endif

/*
 * Something fatal has happened, print error message and exit.
 */
__dead void
panic(const char *a)
{
	(void)fprintf(stderr, "%s: %s\n", __progname, a);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(EXIT_FAILURE);
}

/*
 * Some operating system error; print error message and exit.
 */
__dead void
perr(const char *a)
{
	if (!force)
		perror(a);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(EXIT_FAILURE);
}

/*
 * Two-parameter version of perr().
 */
__dead void 
perr2(const char *a, const char *b)
{
	if (!force)
		(void)fputs(a, stderr);
	perr(b);
}

__dead void
usage(void)
{
	/* Print usage and exit.  */
	switch (program) {
	case AT:
	case CAT:
		(void)fprintf(stderr,
		    "usage: at [-bm] [-f file] [-q queue] -t time_arg\n"
		    "       at [-bm] [-f file] [-q queue] timespec\n"
		    "       at -c job [job ...]\n"
		    "       at -l [-q queue] [job ...]\n"
		    "       at -r job [job ...]\n");
		break;
	case ATQ:
		(void)fprintf(stderr,
		    "usage: atq [-cnv] [-q queue] [name...]\n");
		break;
	case ATRM:
		(void)fprintf(stderr,
		    "usage: atrm [-afi] [[job] [name] ...]\n");
		break;
	case BATCH:
		(void)fprintf(stderr,
		    "usage: batch [-m] [-f file] [-q queue] [timespec]\n");
		break;
	}
	exit(EXIT_FAILURE);
}
