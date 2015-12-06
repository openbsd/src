//
// errors.c - error & help routines
//
// Written by Eryk Vershen
//

/*
 * Copyright 1996,1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// for *printf()
#include <stdio.h>
#include <errno.h>

// for exit()
#include <stdlib.h>
// for strrchr
#include <string.h>

// for va_start(), etc.
#include <stdarg.h>

#include "errors.h"


//
// Defines
//


//
// Types
//


//
// Global Constants
//


//
// Global Variables
//
extern char *__progname;


//
// Forward declarations
//


//
// Routines
//
void
do_help()
{
    printf("usage: %s [-hilrv] disk\n", __progname);
/*
	{"debug",	no_argument,		0,	'd'},
	{"abbr",	no_argument,		0,	'a'},
	{"fs",		no_argument,		0,	'f'},
	{"logical",	no_argument,		0,	kLogicalOption},
	{"compute_size", no_argument,		0,	'c'},
*/
}


void
usage(const char *kind)
{
    error(-1, "bad usage - %s\n", kind);
    hflag = 1;
}


//
// Print a message on standard error and exit with value.
// Values in the range of system error numbers will add
// the perror(3) message.
//
void
fatal(int value, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", __progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (value > 0 && value < sys_nerr) {
	fprintf(stderr, "  (%s)\n", sys_errlist[value]);
    } else {
	fprintf(stderr, "\n");
    }
    exit(value);
}


//
// Print a message on standard error.
// Values in the range of system error numbers will add
// the perror(3) message.
//
void
error(int value, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", __progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (value > 0 && value < sys_nerr) {
	fprintf(stderr, "  (%s)\n", sys_errlist[value]);
    } else {
	fprintf(stderr, "\n");
    }
}
