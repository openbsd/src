/*
 * string.h --
 *
 *	Declarations of ANSI C library procedures for string handling.
 *
 * Copyright (c) 1991-1993 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) string.h 1.13 96/04/09 22:14:53
 */

#ifndef _STRING
#define _STRING

#include <tcl.h>

/*
 * The following #include is needed to define size_t. (This used to
 * include sys/stdtypes.h but that doesn't exist on older versions
 * of SunOS, e.g. 4.0.2, so I'm trying sys/types.h now.... hopefully
 * it exists everywhere)
 */

#ifndef MAC_TCL
#include <sys/types.h>
#endif

extern char *		memchr _ANSI_ARGS_((CONST VOID *s, int c, size_t n));
extern int		memcmp _ANSI_ARGS_((CONST VOID *s1, CONST VOID *s2,
			    size_t n));
extern char *		memcpy _ANSI_ARGS_((VOID *t, CONST VOID *f, size_t n));
extern char *		memmove _ANSI_ARGS_((VOID *t, CONST VOID *f,
			    size_t n));
extern char *		memset _ANSI_ARGS_((VOID *s, int c, size_t n));

extern int		strcasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2));
extern char *		strcat _ANSI_ARGS_((char *dst, CONST char *src));
extern char *		strchr _ANSI_ARGS_((CONST char *string, int c));
extern int		strcmp _ANSI_ARGS_((CONST char *s1, CONST char *s2));
extern char *		strcpy _ANSI_ARGS_((char *dst, CONST char *src));
extern size_t		strcspn _ANSI_ARGS_((CONST char *string,
			    CONST char *chars));
extern char *		strdup _ANSI_ARGS_((CONST char *string));
extern char *		strerror _ANSI_ARGS_((int error));
extern size_t		strlen _ANSI_ARGS_((CONST char *string));
extern int		strncasecmp _ANSI_ARGS_((CONST char *s1,
			    CONST char *s2, size_t n));
extern char *		strncat _ANSI_ARGS_((char *dst, CONST char *src,
			    size_t numChars));
extern int		strncmp _ANSI_ARGS_((CONST char *s1, CONST char *s2,
			    size_t nChars));
extern char *		strncpy _ANSI_ARGS_((char *dst, CONST char *src,
			    size_t numChars));
extern char *		strpbrk _ANSI_ARGS_((CONST char *string, char *chars));
extern char *		strrchr _ANSI_ARGS_((CONST char *string, int c));
extern size_t		strspn _ANSI_ARGS_((CONST char *string,
			    CONST char *chars));
extern char *		strstr _ANSI_ARGS_((CONST char *string,
			    CONST char *substring));
extern char *		strtok _ANSI_ARGS_((CONST char *s, CONST char *delim));

#endif /* _STRING */
