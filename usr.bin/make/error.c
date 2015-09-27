/*	$OpenBSD: error.c,v 1.25 2015/09/27 16:58:16 guenther Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "defines.h"
#include "error.h"
#include "job.h"
#include "targ.h"
#include "var.h"
#ifndef LOCATION_TYPE
#include "location.h"
#endif

#include "lowparse.h"
#include "dump.h"

int fatal_errors = 0;

static void ParseVErrorInternal(const Location *, int, const char *, va_list)
			__attribute__((__format__ (printf, 3, 0)));
/*-
 * Error --
 *	Print an error message given its format.
 */
void
Error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
}

/*-
 * Fatal --
 *	Produce a Fatal error message. If jobs are running, waits for them
 *	to finish.
 *
 * Side Effects:
 *	The program exits
 */
void
Fatal(const char *fmt, ...)
{
	va_list ap;

	Job_Wait();

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");

	if (DEBUG(GRAPH2))
		post_mortem();
	exit(2);		/* Not 1 so -q can distinguish error */
}

/*
 * Punt --
 *	Major exception once jobs are being created. Kills all jobs, prints
 *	a message and exits.
 *
 * Side Effects:
 *	All children are killed indiscriminately and the program Lib_Exits
 */
void
Punt(const char *fmt, ...)
{
	if (fmt) {
		va_list ap;

		va_start(ap, fmt);
		(void)fprintf(stderr, "make: ");
		(void)vfprintf(stderr, fmt, ap);
		va_end(ap);
		(void)fprintf(stderr, "\n");
	}

	Job_AbortAll();
	if (DEBUG(GRAPH2))
		post_mortem();
	exit(2);		/* Not 1, so -q can distinguish error */
}

/*
 * Finish --
 *	Called when aborting due to errors in command or fatal signal
 *
 * Side Effects:
 *	The program exits
 */
void
Finish()
{
	Job_Wait();
	print_errors();
	if (DEBUG(GRAPH2))
		post_mortem();
	exit(2);		/* Not 1 so -q can distinguish error */
}


/*-
 * ParseVErrorInternal	--
 *	Error message abort function for parsing. Prints out the context
 *	of the error (line number and file) as well as the message with
 *	two optional arguments.
 *
 * Side Effects:
 *	"fatals" is incremented if the level is PARSE_FATAL.
 */
static void
ParseVErrorInternal(const Location *origin, int type, const char *fmt, 
    va_list ap)
{
	static bool first = true;
	fprintf(stderr, "*** %s",
	    type == PARSE_WARNING ? "Warning" : "Parse error");
	if (first) {
		fprintf(stderr, " in %s: ", Var_Value(".CURDIR"));
		first = false;
	} else
		fprintf(stderr, ": ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (origin->fname)
	    	fprintf(stderr, " (%s:%lu)", origin->fname, origin->lineno);
	fprintf(stderr, "\n");
	if (type == PARSE_FATAL)
		fatal_errors ++;
}

/*-
 * Parse_Error	--
 *	External interface to ParseVErrorInternal; uses the default filename
 *	Line number.
 */
void
Parse_Error(int type, const char *fmt, ...)
{
	va_list ap;
	Location l;

	va_start(ap, fmt);
	Parse_FillLocation(&l);
	ParseVErrorInternal(&l, type, fmt, ap);
	va_end(ap);
}
