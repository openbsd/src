/*	$OpenPackages$ */
/*	$OpenBSD: error.c,v 1.15 2007/11/03 10:41:48 espie Exp $ */

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

#include "lowparse.h"

int fatal_errors = 0;
bool supervise_jobs = false;

static void ParseVErrorInternal(const char *, unsigned long, int, const char *, va_list);
/*-
 * Error --
 *	Print an error message given its format.
 */
/* VARARGS */
void
Error(char *fmt, ...)
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
/* VARARGS */
void
Fatal(char *fmt, ...)
{
	va_list ap;

	if (supervise_jobs)
		Job_Wait();

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");

	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
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
/* VARARGS */
void
Punt(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)fprintf(stderr, "make: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");

	DieHorribly();
}

/*-
 * DieHorribly --
 *	Exit without giving a message.
 *
 * Side Effects:
 *	A big one...
 */
void
DieHorribly(void)
{
	Job_AbortAll();
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	exit(2);		/* Not 1, so -q can distinguish error */
}

/*
 * Finish --
 *	Called when aborting due to errors in child shell to signal
 *	abnormal exit.
 *
 * Side Effects:
 *	The program exits
 */
void
Finish(int errors) /* number of errors encountered in Make_Make */
{
	Job_Wait();
	if (errors != 0) {
		Error("make pid #%ld: %d error%s in directory %s:", (long)getpid(), 
		    errors, errors == 1 ? "" : "s", 
		    Var_Value(".CURDIR"));
	}
	print_errors();
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
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
/* VARARGS */
static void
ParseVErrorInternal(const char *cfname, unsigned long clineno, int type,
	const char *fmt, va_list ap)
{
	if (cfname)
	    (void)fprintf(stderr, "\"%s\", line %lu: ", cfname, clineno);
	if (type == PARSE_WARNING)
		(void)fprintf(stderr, "warning: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	if (type == PARSE_FATAL)
		fatal_errors ++;
}

/*-
 * Parse_Error	--
 *	External interface to ParseVErrorInternal; uses the default filename
 *	Line number.
 */
/* VARARGS */
void
Parse_Error(int type, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ParseVErrorInternal(Parse_Getfilename(), Parse_Getlineno(), type,
	    fmt, ap);
	va_end(ap);
}

