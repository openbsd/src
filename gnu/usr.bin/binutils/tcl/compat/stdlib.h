/*
 * stdlib.h --
 *
 *	Declares facilities exported by the "stdlib" portion of
 *	the C library.  This file isn't complete in the ANSI-C
 *	sense;  it only declares things that are needed by Tcl.
 *	This file is needed even on many systems with their own
 *	stdlib.h (e.g. SunOS) because not all stdlib.h files
 *	declare all the procedures needed here (such as strtod).
 *
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) stdlib.h 1.10 96/02/15 14:43:54
 */

#ifndef _STDLIB
#define _STDLIB

#include <tcl.h>

extern void		abort _ANSI_ARGS_((void));
extern double		atof _ANSI_ARGS_((CONST char *string));
extern int		atoi _ANSI_ARGS_((CONST char *string));
extern long		atol _ANSI_ARGS_((CONST char *string));
extern char *		calloc _ANSI_ARGS_((unsigned int numElements,
			    unsigned int size));
extern void		exit _ANSI_ARGS_((int status));
extern int		free _ANSI_ARGS_((char *blockPtr));
extern char *		getenv _ANSI_ARGS_((CONST char *name));
extern char *		malloc _ANSI_ARGS_((unsigned int numBytes));
extern void		qsort _ANSI_ARGS_((VOID *base, int n, int size,
			    int (*compar)(CONST VOID *element1, CONST VOID
			    *element2)));
extern char *		realloc _ANSI_ARGS_((char *ptr, unsigned int numBytes));
extern double		strtod _ANSI_ARGS_((CONST char *string, char **endPtr));
extern long		strtol _ANSI_ARGS_((CONST char *string, char **endPtr,
			    int base));
extern unsigned long	strtoul _ANSI_ARGS_((CONST char *string,
			    char **endPtr, int base));

#endif /* _STDLIB */
