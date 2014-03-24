/*    util.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999,
 *    2000, 2001, 2005 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#include "EXTERN.h"
#include "a2p.h"
#include "INTERN.h"
#include "util.h"

#include <stdarg.h>
#define FLUSH

static const char nomem[] = "Out of memory!\n";

/* paranoid version of malloc */


Malloc_t
safemalloc(MEM_SIZE size)
{
    Malloc_t ptr;

    /* malloc(0) is NASTY on some systems */
    ptr = malloc(size ? size : 1);
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%lx: (%05d) malloc %ld bytes\n",(unsigned long)ptr,
    	    	an++,(long)size);
#endif
    if (ptr != NULL)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
    return 0;
}

/* paranoid version of realloc */

Malloc_t
saferealloc(Malloc_t where, MEM_SIZE size)
{
    Malloc_t ptr;

    /* realloc(0) is NASTY on some systems */
    ptr = realloc(where, size ? size : 1);
#ifdef DEBUGGING
    if (debug & 128) {
	fprintf(stderr,"0x%lx: (%05d) rfree\n",(unsigned long)where,an++);
	fprintf(stderr,"0x%lx: (%05d) realloc %ld bytes\n",(unsigned long)ptr,an++,(long)size);
    }
#endif
    if (ptr != NULL)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
    return 0;
}

/* safe version of free */

Free_t
safefree(Malloc_t where)
{
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%lx: (%05d) free\n",(unsigned long)where,an++);
#endif
    free(where);
}

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
cpytill(char *to, char *from, int delim)
{
    for (; *from; from++,to++) {
	if (*from == '\\') {
	    if (from[1] == delim)
		from++;
	    else if (from[1] == '\\')
		*to++ = *from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    return from;
}


char *
cpy2(char *to, char *from, int delim)
{
    for (; *from; from++,to++) {
	if (*from == '\\')
	    *to++ = *from++;
	else if (*from == '$')
	    *to++ = '\\';
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    return from;
}

/* return ptr to little string in big string, NULL if not found */

char *
instr(char *big, const char *little)
{
    char *t, *x;
    const char *s;

    for (t = big; *t; t++) {
	for (x=t,s=little; *s; x++,s++) {
	    if (!*x)
		return NULL;
	    if (*s != *x)
		break;
	}
	if (!*s)
	    return t;
    }
    return NULL;
}

/* copy a string to a safe spot */

char *
savestr(const char *str)
{
    char * const newaddr = (char *) safemalloc((MEM_SIZE)(strlen(str)+1));

    (void)strcpy(newaddr,str);
    return newaddr;
}

/* grow a static string to at least a certain length */

void
growstr(char **strptr, int *curlen, int newlen)
{
    if (newlen > *curlen) {		/* need more room? */
	if (*curlen)
	    *strptr = (char *) saferealloc(*strptr,(MEM_SIZE)newlen);
	else
	    *strptr = (char *) safemalloc((MEM_SIZE)newlen);
	*curlen = newlen;
    }
}

void
fatal(const char *pat,...)
{
#if defined(HAS_VPRINTF)
    va_list args;

    va_start(args, pat);
    vfprintf(stderr,pat,args);
    va_end(args);
#else
    fprintf(stderr,pat,a1,a2,a3,a4);
#endif
    exit(1);
}

#if defined(DARWIN)
__private_extern__	/* warn() conflicts with libc */
#endif
void
warn(const char *pat,...)
{
#if defined(HAS_VPRINTF)
    va_list args;

    va_start(args, pat);
    vfprintf(stderr,pat,args);
    va_end(args);
#else
    fprintf(stderr,pat,a1,a2,a3,a4);
#endif
}

