/* $RCSfile: util.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:29 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	util.c,v $
 */

#include "EXTERN.h"
#include "a2p.h"
#include "INTERN.h"
#include "util.h"

#define FLUSH

static char nomem[] = "Out of memory!\n";

/* paranoid version of malloc */


Malloc_t
safemalloc(size)
MEM_SIZE size;
{
    char *ptr;
    Malloc_t malloc();

    ptr = (char *) malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%x: (%05d) malloc %d bytes\n",ptr,an++,size);
#endif
    if (ptr != Nullch)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
}

/* paranoid version of realloc */

Malloc_t
saferealloc(where,size)
char *where;
MEM_SIZE size;
{
    char *ptr;
    Malloc_t realloc();

    ptr = (char *)
		realloc(where,size?size:1);	/* realloc(0) is NASTY on our system */
#ifdef DEBUGGING
    if (debug & 128) {
	fprintf(stderr,"0x%x: (%05d) rfree\n",where,an++);
	fprintf(stderr,"0x%x: (%05d) realloc %d bytes\n",ptr,an++,size);
    }
#endif
    if (ptr != Nullch)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
}

/* safe version of free */

void
safefree(where)
char *where;
{
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%x: (%05d) free\n",where,an++);
#endif
    free(where);
}

/* safe version of string copy */

char *
safecpy(to,from,len)
char *to;
register char *from;
register int len;
{
    register char *dest = to;

    if (from != Nullch) 
	for (len--; len && (*dest++ = *from++); len--) ;
    *dest = '\0';
    return to;
}

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
cpytill(to,from,delim)
register char *to, *from;
register int delim;
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
cpy2(to,from,delim)
register char *to, *from;
register int delim;
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
instr(big, little)
char *big, *little;

{
    register char *t, *s, *x;

    for (t = big; *t; t++) {
	for (x=t,s=little; *s; x++,s++) {
	    if (!*x)
		return Nullch;
	    if (*s != *x)
		break;
	}
	if (!*s)
	    return t;
    }
    return Nullch;
}

/* copy a string to a safe spot */

char *
savestr(str)
char *str;
{
    register char *newaddr = safemalloc((MEM_SIZE)(strlen(str)+1));

    (void)strcpy(newaddr,str);
    return newaddr;
}

/* grow a static string to at least a certain length */

void
growstr(strptr,curlen,newlen)
char **strptr;
int *curlen;
int newlen;
{
    if (newlen > *curlen) {		/* need more room? */
	if (*curlen)
	    *strptr = saferealloc(*strptr,(MEM_SIZE)newlen);
	else
	    *strptr = safemalloc((MEM_SIZE)newlen);
	*curlen = newlen;
    }
}

/*VARARGS1*/
void
croak(pat,a1,a2,a3,a4)
char *pat;
int a1,a2,a3,a4;
{
    fprintf(stderr,pat,a1,a2,a3,a4);
    exit(1);
}

/*VARARGS1*/
void
fatal(pat,a1,a2,a3,a4)
char *pat;
int a1,a2,a3,a4;
{
    fprintf(stderr,pat,a1,a2,a3,a4);
    exit(1);
}

/*VARARGS1*/
void
warn(pat,a1,a2,a3,a4)
char *pat;
int a1,a2,a3,a4;
{
    fprintf(stderr,pat,a1,a2,a3,a4);
}

