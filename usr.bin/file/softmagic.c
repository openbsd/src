/*	$OpenBSD: softmagic.c,v 1.9 2003/03/11 21:26:26 ian Exp $	*/

/*
 * softmagic - interpret variable magic from /etc/magic
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

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>

#include "file.h"

#ifndef	lint
static char *moduleid = "$OpenBSD: softmagic.c,v 1.9 2003/03/11 21:26:26 ian Exp $";
#endif	/* lint */

static int match(unsigned char *, int);
static int mget(union VALUETYPE *, unsigned char *, struct magic *, int);
static int mcheck(union VALUETYPE *, struct magic *);
static int32_t mprint(union VALUETYPE *, struct magic *);
static void mdebug(int32_t, char *, int);
static int mconvert(union VALUETYPE *, struct magic *);

/*
 * softmagic - lookup one file in database 
 * (already read from /etc/magic by apprentice.c).
 * Passed the name and FILE * of one file to be typed.
 */
/*ARGSUSED1*/		/* nbytes passed for regularity, maybe need later */
int
softmagic(buf, nbytes)
unsigned char *buf;
int nbytes;
{
	if (match(buf, nbytes))
		return 1;

	return 0;
}

/*
 * Go through the whole list, stopping if you find a match.  Process all
 * the continuations of that match before returning.
 *
 * We support multi-level continuations:
 *
 *	At any time when processing a successful top-level match, there is a
 *	current continuation level; it represents the level of the last
 *	successfully matched continuation.
 *
 *	Continuations above that level are skipped as, if we see one, it
 *	means that the continuation that controls them - i.e, the
 *	lower-level continuation preceding them - failed to match.
 *
 *	Continuations below that level are processed as, if we see one,
 *	it means we've finished processing or skipping higher-level
 *	continuations under the control of a successful or unsuccessful
 *	lower-level continuation, and are now seeing the next lower-level
 *	continuation and should process it.  The current continuation
 *	level reverts to the level of the one we're seeing.
 *
 *	Continuations at the current level are processed as, if we see
 *	one, there's no lower-level continuation that may have failed.
 *
 *	If a continuation matches, we bump the current continuation level
 *	so that higher-level continuations are processed.
 */
static int
match(s, nbytes)
unsigned char	*s;
int nbytes;
{
	int magindex = 0;
	int cont_level = 0;
	int need_separator = 0;
	union VALUETYPE p;
	static int32_t *tmpoff = NULL;
	static size_t tmplen = 0;
	int32_t oldoff = 0;

	if (tmpoff == NULL)
		if ((tmpoff = (int32_t *) malloc(tmplen = 20)) == NULL)
			err(1, "malloc");

	for (magindex = 0; magindex < nmagic; magindex++) {
		/* if main entry matches, print it... */
		if (!mget(&p, s, &magic[magindex], nbytes) ||
		    !mcheck(&p, &magic[magindex])) {
			    /* 
			     * main entry didn't match,
			     * flush its continuations
			     */
			    while (magindex < nmagic &&
			    	   magic[magindex + 1].cont_level != 0)
			    	   magindex++;
			    continue;
		}

		tmpoff[cont_level] = mprint(&p, &magic[magindex]);
		/*
		 * If we printed something, we'll need to print
		 * a blank before we print something else.
		 */
		if (magic[magindex].desc[0])
			need_separator = 1;
		/* and any continuations that match */
		if (++cont_level >= tmplen)
			if ((tmpoff = (int32_t *) realloc(tmpoff,
						       tmplen += 20)) == NULL)
				err(1, "malloc");
		while (magic[magindex+1].cont_level != 0 && 
		       ++magindex < nmagic) {
			if (cont_level >= magic[magindex].cont_level) {
				if (cont_level > magic[magindex].cont_level) {
					/*
					 * We're at the end of the level
					 * "cont_level" continuations.
					 */
					cont_level = magic[magindex].cont_level;
				}
				if (magic[magindex].flag & ADD) {
					oldoff=magic[magindex].offset;
					magic[magindex].offset += tmpoff[cont_level-1];
				}
				if (mget(&p, s, &magic[magindex], nbytes) &&
				    mcheck(&p, &magic[magindex])) {
					/*
					 * This continuation matched.
					 * Print its message, with
					 * a blank before it if
					 * the previous item printed
					 * and this item isn't empty.
					 */
					/* space if previous printed */
					if (need_separator
					   && (magic[magindex].nospflag == 0)
					   && (magic[magindex].desc[0] != '\0')
					   ) {
						(void) putchar(' ');
						need_separator = 0;
					}
					tmpoff[cont_level] = mprint(&p, &magic[magindex]);
					if (magic[magindex].desc[0])
						need_separator = 1;

					/*
					 * If we see any continuations
					 * at a higher level,
					 * process them.
					 */
					if (++cont_level >= tmplen)
						if ((tmpoff = 
						    (int32_t *) realloc(tmpoff,
						    tmplen += 20)) == NULL)
							err(1, "malloc");
				}
				if (magic[magindex].flag & ADD) {
					 magic[magindex].offset = oldoff;
				}
			}
		}
		return 1;		/* all through */
	}
	return 0;			/* no match at all */
}

static int32_t
mprint(p, m)
union VALUETYPE *p;
struct magic *m;
{
	char *pp, *rt;
	uint32_t v;
	int32_t t=0 ;


  	switch (m->type) {
  	case BYTE:
		v = p->b;
		v = signextend(m, v) & m->mask;
		(void) printf(m->desc, (unsigned char) v);
		t = m->offset + sizeof(char);
		break;

  	case SHORT:
  	case BESHORT:
  	case LESHORT:
		v = p->h;
		v = signextend(m, v) & m->mask;
		(void) printf(m->desc, (unsigned short) v);
		t = m->offset + sizeof(short);
		break;

  	case LONG:
  	case BELONG:
  	case LELONG:
		v = p->l;
		v = signextend(m, v) & m->mask;
		(void) printf(m->desc, (uint32_t) v);
		t = m->offset + sizeof(int32_t);
  		break;

  	case STRING:
		if (m->reln == '=') {
			(void) printf(m->desc, m->value.s);
			t = m->offset + strlen(m->value.s);
		}
		else {
			if (*m->value.s == '\0') {
				char *cp = strchr(p->s,'\n');
				if (cp)
					*cp = '\0';
			}
			(void) printf(m->desc, p->s);
			t = m->offset + strlen(p->s);
		}
		break;

	case DATE:
	case BEDATE:
	case LEDATE:
		pp = ctime((time_t*) &p->l);
		if ((rt = strchr(pp, '\n')) != NULL)
			*rt = '\0';
		(void) printf(m->desc, pp);
		t = m->offset + sizeof(time_t);
		break;

	default:
		errx(1, "invalid m->type (%d) in mprint().", m->type);
		/*NOTREACHED*/
	}
	return(t);
}

/*
 * Convert the byte order of the data we are looking at
 */
static int
mconvert(p, m)
union VALUETYPE *p;
struct magic *m;
{
	switch (m->type) {
	case BYTE:
	case SHORT:
	case LONG:
	case DATE:
		return 1;
	case STRING:
		{
			char *ptr;

			/* Null terminate and eat the return */
			p->s[sizeof(p->s) - 1] = '\0';
			if ((ptr = strchr(p->s, '\n')) != NULL)
				*ptr = '\0';
			return 1;
		}
	case BESHORT:
		p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
		return 1;
	case BELONG:
	case BEDATE:
		p->l = (int32_t)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		return 1;
	case LESHORT:
		p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
		return 1;
	case LELONG:
	case LEDATE:
		p->l = (int32_t)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		return 1;
	default:
		errx(1, "invalid type %d in mconvert().", m->type);
		return 0;
	}
}


static void
mdebug(offset, str, len)
int32_t offset;
char *str;
int len;
{
	(void) fprintf(stderr, "mget @%d: ", offset);
	showstr(stderr, (char *) str, len);
	(void) fputc('\n', stderr);
	(void) fputc('\n', stderr);
}

static int
mget(p, s, m, nbytes)
union VALUETYPE* p;
unsigned char	*s;
struct magic *m;
int nbytes;
{
	int32_t offset = m->offset;

	if (offset + sizeof(union VALUETYPE) <= nbytes)
		memcpy(p, s + offset, sizeof(union VALUETYPE));
	else {
		/*
		 * the usefulness of padding with zeroes eludes me, it
		 * might even cause problems
		 */
		int32_t have = nbytes - offset;
		memset(p, 0, sizeof(union VALUETYPE));
		if (have > 0)
			memcpy(p, s + offset, have);
	}


	if (debug) {
		mdebug(offset, (char *) p, sizeof(union VALUETYPE));
		mdump(m);
	}

	if (!mconvert(p, m))
		return 0;

	if (m->flag & INDIR) {

		switch (m->in.type) {
		case BYTE:
			offset = p->b + m->in.offset;
			break;
		case SHORT:
			offset = p->h + m->in.offset;
			break;
		case LONG:
			offset = p->l + m->in.offset;
			break;
		}

		if (offset + sizeof(union VALUETYPE) > nbytes)
			return 0;

		memcpy(p, s + offset, sizeof(union VALUETYPE));

		if (debug) {
			mdebug(offset, (char *) p, sizeof(union VALUETYPE));
			mdump(m);
		}

		if (!mconvert(p, m))
			return 0;
	}
	return 1;
}

static int
mcheck(p, m)
union VALUETYPE* p;
struct magic *m;
{
	uint32_t l = m->value.l;
	uint32_t v;
	int matched;

	if ( (m->value.s[0] == 'x') && (m->value.s[1] == '\0') ) {
		fprintf(stderr, "BOINK");
		return 1;
	}


	switch (m->type) {
	case BYTE:
		v = p->b;
		break;

	case SHORT:
	case BESHORT:
	case LESHORT:
		v = p->h;
		break;

	case LONG:
	case BELONG:
	case LELONG:
	case DATE:
	case BEDATE:
	case LEDATE:
		v = p->l;
		break;

	case STRING:
		l = 0;
		/* What we want here is:
		 * v = strncmp(m->value.s, p->s, m->vallen);
		 * but ignoring any nulls.  bcmp doesn't give -/+/0
		 * and isn't universally available anyway.
		 */
		v = 0;
		{
			unsigned char *a = (unsigned char*)m->value.s;
			unsigned char *b = (unsigned char*)p->s;
			int len = m->vallen;

			while (--len >= 0)
				if ((v = *b++ - *a++) != '\0')
					break;
		}
		break;
	default:
		errx(1, "invalid type %d in mcheck().", m->type);
		return 0;/*NOTREACHED*/
	}

	v = signextend(m, v) & m->mask;

	switch (m->reln) {
	case 'x':
		if (debug)
			(void) fprintf(stderr, "%u == *any* = 1\n", v);
		matched = 1;
		break;

	case '!':
		matched = v != l;
		if (debug)
			(void) fprintf(stderr, "%u != %u = %d\n",
				       v, l, matched);
		break;

	case '=':
		matched = v == l;
		if (debug)
			(void) fprintf(stderr, "%u == %u = %d\n",
				       v, l, matched);
		break;

	case '>':
		if (m->flag & UNSIGNED) {
			matched = v > l;
			if (debug)
				(void) fprintf(stderr, "%u > %u = %d\n",
					       v, l, matched);
		}
		else {
			matched = (int32_t) v > (int32_t) l;
			if (debug)
				(void) fprintf(stderr, "%d > %d = %d\n",
					       v, l, matched);
		}
		break;

	case '<':
		if (m->flag & UNSIGNED) {
			matched = v < l;
			if (debug)
				(void) fprintf(stderr, "%u < %u = %d\n",
					       v, l, matched);
		}
		else {
			matched = (int32_t) v < (int32_t) l;
			if (debug)
				(void) fprintf(stderr, "%d < %d = %d\n",
					       v, l, matched);
		}
		break;

	case '&':
		matched = (v & l) == l;
		if (debug)
			(void) fprintf(stderr, "((%x & %x) == %x) = %d\n",
				       v, l, l, matched);
		break;

	case '^':
		matched = (v & l) != l;
		if (debug)
			(void) fprintf(stderr, "((%x & %x) != %x) = %d\n",
				       v, l, l, matched);
		break;

	default:
		matched = 0;
		errx(1, "mcheck: can't happen: invalid relation %d.", m->reln);
		break;/*NOTREACHED*/
	}

	return matched;
}
