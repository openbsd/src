/* 
 * getcwd.c --
 *
 *	This file provides an implementation of the getcwd procedure
 *	that uses getwd, for systems with getwd but without getcwd.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) getcwd.c 1.5 96/02/15 12:08:20
 */

#include "tclInt.h"
#include "tclPort.h"

extern char *getwd _ANSI_ARGS_((char *pathname));

char *
getcwd(buf, size)
    char *buf;			/* Where to put path for current directory. */
    size_t size;		/* Number of bytes at buf. */
{
    char realBuffer[MAXPATHLEN+1];
    int length;

    if (getwd(realBuffer) == NULL) {
	/*
	 * There's not much we can do besides guess at an errno to
	 * use for the result (the error message in realBuffer isn't
	 * much use...).
	 */

	errno = EACCES;
	return NULL;
    }
    length = strlen(realBuffer);
    if (length >= size) {
	errno = ERANGE;
	return NULL;
    }
    strcpy(buf, realBuffer);
    return buf;
}

