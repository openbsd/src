/*	$OpenBSD: panic.c,v 1.9 2002/05/13 16:12:07 millert Exp $	*/
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

/* System Headers */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Local headers */

#include "panic.h"
#include "at.h"
#include "privs.h"

/* File scope variables */

#ifndef lint
static const char rcsid[] = "$OpenBSD: panic.c,v 1.9 2002/05/13 16:12:07 millert Exp $";
#endif

/* External variables */

/* Global functions */

__dead void
panic(const char *a)
{
	/*
	 * Something fatal has happened, print error message and exit.
	 */
	(void)fprintf(stderr, "%s: %s\n", __progname, a);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(EXIT_FAILURE);
}

__dead void
perr(const char *a)
{
	/*
	 * Some operating system error; print error message and exit.
	 */
	perror(a);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(EXIT_FAILURE);
}

__dead void 
perr2(const char *a, const char *b)
{
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
		    "Usage: at [-blmrv] [-f file] [-q queue] -t time_arg\n"
		    "       at [-blmrv] [-f file] [-q queue] timespec\n"
		    "       at -c job [job ...]\n");
		break;
	case ATQ:
		(void)fprintf(stderr, "Usage: atq [-q queue] [-v]\n");
		break;
	case ATRM:
		(void)fprintf(stderr, "Usage: atrm job [job ...]\n");
		break;
	case BATCH:
		(void)fprintf(stderr,
		    "Usage: batch [-mv] [-f file] [-q queue] [timespec]\n");
		break;
	}
	exit(EXIT_FAILURE);
}
