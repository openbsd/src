/*	$OpenBSD: print.c,v 1.9 2003/03/11 21:26:26 ian Exp $	*/

/*
 * print.c - debugging printout routines
 *
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <err.h>
#include "file.h"

#ifndef lint
static char *moduleid = "$OpenBSD: print.c,v 1.9 2003/03/11 21:26:26 ian Exp $";
#endif  /* lint */

#define SZOF(a)	(sizeof(a) / sizeof(a[0]))

void
mdump(m)
struct magic *m;
{
	static char *typ[] = {   "invalid", "byte", "short", "invalid",
				 "long", "string", "date", "beshort",
				 "belong", "bedate", "leshort", "lelong",
				 "ledate" };
	(void) fputc('[', stderr);
	(void) fprintf(stderr, ">>>>>>>> %d" + 8 - (m->cont_level & 7),
		       m->offset);

	if (m->flag & INDIR)
		(void) fprintf(stderr, "(%s,%d),",
			       (m->in.type >= 0 && m->in.type < SZOF(typ)) ? 
					typ[(unsigned char) m->in.type] :
					"*bad*",
			       m->in.offset);

	(void) fprintf(stderr, " %s%s", (m->flag & UNSIGNED) ? "u" : "",
		       (m->type >= 0 && m->type < SZOF(typ)) ? 
				typ[(unsigned char) m->type] : 
				"*bad*");
	if (m->mask != ~0)
		(void) fprintf(stderr, " & %.8x", m->mask);

	(void) fprintf(stderr, ",%c", m->reln);

	if (m->reln != 'x') {
	    switch (m->type) {
	    case BYTE:
	    case SHORT:
	    case LONG:
	    case LESHORT:
	    case LELONG:
	    case BESHORT:
	    case BELONG:
		    (void) fprintf(stderr, "%d", m->value.l);
		    break;
	    case STRING:
		    showstr(stderr, m->value.s, -1);
		    break;
	    case DATE:
	    case LEDATE:
	    case BEDATE:
		    {
			    char *rt, *pp = ctime((time_t*) &m->value.l);
			    if ((rt = strchr(pp, '\n')) != NULL)
				    *rt = '\0';
			    (void) fprintf(stderr, "%s,", pp);
			    if (rt)
				    *rt = '\n';
		    }
		    break;
	    default:
		    (void) fputs("*bad*", stderr);
		    break;
	    }
	}
	(void) fprintf(stderr, ",\"%s\"]\n", m->desc);
}

/*
 * This "error" is here so we don't have to change all occurrences of
 * error() to err(1,...) when importing new versions from Christos.
 */
void error(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verr(1, fmt, va);
}

/*
 * ckfputs - futs, but with error checking
 * ckfprintf - fprintf, but with error checking
 */
void
ckfputs(str, fil) 	
    const char *str;
    FILE *fil;
{
	if (fputs(str,fil) == EOF)
		err(1, "write failed");
}

/*VARARGS*/
void
ckfprintf(FILE *f, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	(void) vfprintf(f, fmt, va);
	if (ferror(f))
		err(1, "write failed");
	va_end(va);
}
