/*	$OpenBSD: process.c,v 1.33 2017/12/13 16:06:34 millert Exp $	*/

/*-
 * Copyright (c) 1992 Diomidis Spinellis.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis of Imperial College, University of London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "extern.h"

static SPACE HS, PS, SS;
#define	pd		PS.deleted
#define	ps		PS.space
#define	psl		PS.len
#define	psanl		PS.append_newline
#define	hs		HS.space
#define	hsl		HS.len

static inline int	 applies(struct s_command *);
static void		 flush_appends(void);
static void		 lputs(char *);
static inline int	 regexec_e(regex_t *, const char *, int, int, size_t,
			     size_t);
static void		 regsub(SPACE *, char *, char *);
static int		 substitute(struct s_command *);

struct s_appends *appends;	/* Array of pointers to strings to append. */
static size_t appendx;		/* Index into appends array. */
size_t appendnum;		/* Size of appends array. */

static int lastaddr;		/* Set by applies if last address of a range. */
static int sdone;		/* If any substitutes since last line input. */
				/* Iov structure for 'w' commands. */
static regex_t *defpreg;
size_t maxnsub;
regmatch_t *match;

#define OUT() do {\
	fwrite(ps, 1, psl, outfile);\
	if (psanl) fputc('\n', outfile);\
} while (0)

void
process(void)
{
	struct s_command *cp;
	SPACE tspace;
	size_t len, oldpsl;
	char *p;

	for (linenum = 0; mf_fgets(&PS, REPLACE);) {
		pd = 0;
top:
		cp = prog;
redirect:
		while (cp != NULL) {
			if (!applies(cp)) {
				cp = cp->next;
				continue;
			}
			switch (cp->code) {
			case '{':
				cp = cp->u.c;
				goto redirect;
			case 'a':
				if (appendx >= appendnum) {
					appends = xreallocarray(appends,
					    appendnum,
					    2 * sizeof(struct s_appends));
					appendnum *= 2;
				}
				appends[appendx].type = AP_STRING;
				appends[appendx].s = cp->t;
				appends[appendx].len = strlen(cp->t);
				appendx++;
				break;
			case 'b':
				cp = cp->u.c;
				goto redirect;
			case 'c':
				pd = 1;
				psl = 0;
				if (cp->a2 == NULL || lastaddr || lastline())
					(void)fprintf(outfile, "%s", cp->t);
				break;
			case 'd':
				pd = 1;
				goto new;
			case 'D':
				if (pd)
					goto new;
				if (psl == 0 ||
				    (p = memchr(ps, '\n', psl)) == NULL) {
					pd = 1;
					goto new;
				} else {
					psl -= (p + 1) - ps;
					memmove(ps, p + 1, psl);
					goto top;
				}
			case 'g':
				cspace(&PS, hs, hsl, REPLACE);
				break;
			case 'G':
				cspace(&PS, "\n", 1, 0);
				cspace(&PS, hs, hsl, 0);
				break;
			case 'h':
				cspace(&HS, ps, psl, REPLACE);
				break;
			case 'H':
				cspace(&HS, "\n", 1, 0);
				cspace(&HS, ps, psl, 0);
				break;
			case 'i':
				(void)fprintf(outfile, "%s", cp->t);
				break;
			case 'l':
				lputs(ps);
				break;
			case 'n':
				if (!nflag && !pd)
					OUT();
				flush_appends();
				if (!mf_fgets(&PS, REPLACE))
					exit(0);
				pd = 0;
				break;
			case 'N':
				flush_appends();
				cspace(&PS, "\n", 1, 0);
				if (!mf_fgets(&PS, 0))
					exit(0);
				break;
			case 'p':
				if (pd)
					break;
				OUT();
				break;
			case 'P':
				if (pd)
					break;
				if ((p = memchr(ps, '\n', psl)) != NULL) {
					oldpsl = psl;
					psl = p - ps;
					psanl = 1;
					OUT();
					psl = oldpsl;
				} else {
					OUT();
				}
				break;
			case 'q':
				if (!nflag && !pd)
					OUT();
				flush_appends();
				exit(0);
			case 'r':
				if (appendx >= appendnum) {
					appends = xreallocarray(appends,
					    appendnum,
					    2 * sizeof(struct s_appends));
					appendnum *= 2;
				}
				appends[appendx].type = AP_FILE;
				appends[appendx].s = cp->t;
				appends[appendx].len = strlen(cp->t);
				appendx++;
				break;
			case 's':
				sdone |= substitute(cp);
				break;
			case 't':
				if (sdone) {
					sdone = 0;
					cp = cp->u.c;
					goto redirect;
				}
				break;
			case 'w':
				if (pd)
					break;
				if (cp->u.fd == -1 && (cp->u.fd = open(cp->t,
				    O_WRONLY|O_APPEND|O_CREAT|O_TRUNC,
				    DEFFILEMODE)) == -1)
					error(FATAL, "%s: %s",
					    cp->t, strerror(errno));
				if ((size_t)write(cp->u.fd, ps, psl) != psl ||
				    write(cp->u.fd, "\n", 1) != 1)
					error(FATAL, "%s: %s",
					    cp->t, strerror(errno));
				break;
			case 'x':
				if (hs == NULL)
					cspace(&HS, "", 0, REPLACE);
				tspace = PS;
				PS = HS;
				psanl = tspace.append_newline;
				HS = tspace;
				break;
			case 'y':
				if (pd || psl == 0)
					break;
				for (p = ps, len = psl; len--; ++p)
					*p = cp->u.y[(unsigned char)*p];
				break;
			case ':':
			case '}':
				break;
			case '=':
				(void)fprintf(outfile, "%lu\n", linenum);
			}
			cp = cp->next;
		} /* for all cp */

new:		if (!nflag && !pd)
			OUT();
		flush_appends();
	} /* for all lines */
}

/*
 * TRUE if the address passed matches the current program state
 * (lastline, linenumber, ps).
 */
#define	MATCH(a)						\
	(a)->type == AT_RE ? regexec_e((a)->u.r, ps, 0, 1, 0, psl) :	\
	    (a)->type == AT_LINE ? linenum == (a)->u.l : lastline()

/*
 * Return TRUE if the command applies to the current line.  Sets the inrange
 * flag to process ranges.  Interprets the non-select (``!'') flag.
 */
static inline int
applies(struct s_command *cp)
{
	int r;

	lastaddr = 0;
	if (cp->a1 == NULL && cp->a2 == NULL)
		r = 1;
	else if (cp->a2)
		if (cp->inrange) {
			if (MATCH(cp->a2)) {
				cp->inrange = 0;
				lastaddr = 1;
			}
			r = 1;
		} else if (MATCH(cp->a1)) {
			/*
			 * If the second address is a number less than or
			 * equal to the line number first selected, only
			 * one line shall be selected.
			 *	-- POSIX 1003.2
			 */
			if (cp->a2->type == AT_LINE &&
			    linenum >= cp->a2->u.l)
				lastaddr = 1;
			else
				cp->inrange = 1;
			r = 1;
		} else
			r = 0;
	else
		r = MATCH(cp->a1);
	return (cp->nonsel ? !r : r);
}

/*
 * Reset all inrange markers.
 */
void
resetranges(void)
{
	struct s_command *cp;

	for (cp = prog; cp; cp = cp->code == '{' ? cp->u.c : cp->next)
		if (cp->a2)
			cp->inrange = 0;
}

/*
 * substitute --
 *	Do substitutions in the pattern space.  Currently, we build a
 *	copy of the new pattern space in the substitute space structure
 *	and then swap them.
 */
static int
substitute(struct s_command *cp)
{
	SPACE tspace;
	regex_t *re;
	regoff_t slen;
	int n, lastempty;
	regoff_t le = 0;
	char *s;

	s = ps;
	re = cp->u.s->re;
	if (re == NULL) {
		if (defpreg != NULL && cp->u.s->maxbref > defpreg->re_nsub) {
			linenum = cp->u.s->linenum;
			error(COMPILE, "\\%d not defined in the RE",
			    cp->u.s->maxbref);
		}
	}
	if (!regexec_e(re, ps, 0, 0, 0, psl))
		return (0);

	SS.len = 0;				/* Clean substitute space. */
	slen = psl;
	n = cp->u.s->n;
	lastempty = 1;

	do {
		/* Copy the leading retained string. */
		if (n <= 1 && (match[0].rm_so > le))
			cspace(&SS, s, match[0].rm_so - le, APPEND);

		/* Skip zero-length matches right after other matches. */
		if (lastempty || (match[0].rm_so - le) ||
		    match[0].rm_so != match[0].rm_eo) {
			if (n <= 1) {
				/* Want this match: append replacement. */
				regsub(&SS, ps, cp->u.s->new);
				if (n == 1)
					n = -1;
			} else {
				/* Want a later match: append original. */
				if (match[0].rm_eo - le)
					cspace(&SS, s, match[0].rm_eo - le,
					    APPEND);
				n--;
			}
		}

		/* Move past this match. */
		s = ps + match[0].rm_eo;
		slen = psl - match[0].rm_eo;
		le = match[0].rm_eo;

		/*
		 * After a zero-length match, advance one byte,
		 * and at the end of the line, terminate.
		 */
		if (match[0].rm_so == match[0].rm_eo) {
			if (*s == '\0' || *s == '\n')
				slen = -1;
			else
				slen--;
			if (*s != '\0') {
				cspace(&SS, s++, 1, APPEND);
				le++;
			}
			lastempty = 1;
		} else
			lastempty = 0;

	} while (n >= 0 && slen >= 0 &&
	    regexec_e(re, ps, REG_NOTBOL, 0, le, psl));

	/* Did not find the requested number of matches. */
	if (n > 0)
		return (0);

	/* Copy the trailing retained string. */
	if (slen > 0)
		cspace(&SS, s, slen, APPEND);

	/*
	 * Swap the substitute space and the pattern space, and make sure
	 * that any leftover pointers into stdio memory get lost.
	 */
	tspace = PS;
	PS = SS;
	psanl = tspace.append_newline;
	SS = tspace;
	SS.space = SS.back;

	/* Handle the 'p' flag. */
	if (cp->u.s->p)
		OUT();

	/* Handle the 'w' flag. */
	if (cp->u.s->wfile && !pd) {
		if (cp->u.s->wfd == -1 && (cp->u.s->wfd = open(cp->u.s->wfile,
		    O_WRONLY|O_APPEND|O_CREAT|O_TRUNC, DEFFILEMODE)) == -1)
			error(FATAL, "%s: %s", cp->u.s->wfile, strerror(errno));
		if ((size_t)write(cp->u.s->wfd, ps, psl) != psl ||
		    write(cp->u.s->wfd, "\n", 1) != 1)
			error(FATAL, "%s: %s", cp->u.s->wfile, strerror(errno));
	}
	return (1);
}

/*
 * Flush append requests.  Always called before reading a line,
 * therefore it also resets the substitution done (sdone) flag.
 */
static void
flush_appends(void)
{
	FILE *f;
	size_t count, idx;
	char buf[8 * 1024];

	for (idx = 0; idx < appendx; idx++)
		switch (appends[idx].type) {
		case AP_STRING:
			fwrite(appends[idx].s, sizeof(char), appends[idx].len,
			    outfile);
			break;
		case AP_FILE:
			/*
			 * Read files probably shouldn't be cached.  Since
			 * it's not an error to read a non-existent file,
			 * it's possible that another program is interacting
			 * with the sed script through the file system.  It
			 * would be truly bizarre, but possible.  It's probably
			 * not that big a performance win, anyhow.
			 */
			if ((f = fopen(appends[idx].s, "r")) == NULL)
				break;
			while ((count = fread(buf, sizeof(char), sizeof(buf), f)))
				(void)fwrite(buf, sizeof(char), count, outfile);
			(void)fclose(f);
			break;
		}
	if (ferror(outfile))
		error(FATAL, "%s: %s", outfname, strerror(errno ? errno : EIO));
	appendx = sdone = 0;
}

static void
lputs(char *s)
{
	int count;
	extern int termwidth;
	const char *escapes;
	char *p;

	for (count = 0; *s; ++s) {
		if (count >= termwidth) {
			(void)fprintf(outfile, "\\\n");
			count = 0;
		}
		if (isascii((unsigned char)*s) && isprint((unsigned char)*s)
		    && *s != '\\') {
			(void)fputc(*s, outfile);
			count++;
		} else if (*s == '\n') {
			(void)fputc('$', outfile);
			(void)fputc('\n', outfile);
			count = 0;
		} else {
			escapes = "\\\a\b\f\r\t\v";
			(void)fputc('\\', outfile);
			if ((p = strchr(escapes, *s))) {
				(void)fputc("\\abfrtv"[p - escapes], outfile);
				count += 2;
			} else {
				(void)fprintf(outfile, "%03o", *(u_char *)s);
				count += 4;
			}
		}
	}
	(void)fputc('$', outfile);
	(void)fputc('\n', outfile);
	if (ferror(outfile))
		error(FATAL, "%s: %s", outfname, strerror(errno ? errno : EIO));
}

static inline int
regexec_e(regex_t *preg, const char *string, int eflags,
    int nomatch, size_t start, size_t stop)
{
	int eval;

	if (preg == NULL) {
		if (defpreg == NULL)
			error(FATAL, "first RE may not be empty");
	} else
		defpreg = preg;

	/* Set anchors */
	match[0].rm_so = start;
	match[0].rm_eo = stop;

	eval = regexec(defpreg, string,
	    nomatch ? 0 : maxnsub + 1, match, eflags | REG_STARTEND);
	switch (eval) {
	case 0:
		return (1);
	case REG_NOMATCH:
		return (0);
	}
	error(FATAL, "RE error: %s", strregerror(eval, defpreg));
}

/*
 * regsub - perform substitutions after a regexp match
 * Based on a routine by Henry Spencer
 */
static void
regsub(SPACE *sp, char *string, char *src)
{
	int len, no;
	char c, *dst;

#define	NEEDSP(reqlen)							\
	if (sp->len + (reqlen) + 1 >= sp->blen) {			\
		size_t newlen = sp->blen + (reqlen) + 1024;		\
		sp->space = sp->back = xrealloc(sp->back, newlen);	\
		sp->blen = newlen;					\
		dst = sp->space + sp->len;				\
	}

	dst = sp->space + sp->len;
	while ((c = *src++) != '\0') {
		if (c == '&')
			no = 0;
		else if (c == '\\' && isdigit((unsigned char)*src))
			no = *src++ - '0';
		else
			no = -1;
		if (no < 0) {		/* Ordinary character. */
			if (c == '\\' && (*src == '\\' || *src == '&'))
				c = *src++;
			NEEDSP(1);
			*dst++ = c;
			++sp->len;
		} else if (match[no].rm_so != -1 && match[no].rm_eo != -1) {
			len = match[no].rm_eo - match[no].rm_so;
			NEEDSP(len);
			memmove(dst, string + match[no].rm_so, len);
			dst += len;
			sp->len += len;
		}
	}
	NEEDSP(1);
	*dst = '\0';
}

/*
 * aspace --
 *	Append the source space to the destination space, allocating new
 *	space as necessary.
 */
void
cspace(SPACE *sp, const char *p, size_t len, enum e_spflag spflag)
{
	size_t tlen;

	/* Make sure SPACE has enough memory and ramp up quickly. */
	tlen = sp->len + len + 1;
	if (tlen > sp->blen) {
		size_t newlen = tlen + 1024;
		sp->space = sp->back = xrealloc(sp->back, newlen);
		sp->blen = newlen;
	}

	if (spflag == REPLACE)
		sp->len = 0;

	memmove(sp->space + sp->len, p, len);

	sp->space[sp->len += len] = '\0';
}

/*
 * Close all cached opened files and report any errors
 */
void
cfclose(struct s_command *cp, struct s_command *end)
{

	for (; cp != end; cp = cp->next)
		switch (cp->code) {
		case 's':
			if (cp->u.s->wfd != -1 && close(cp->u.s->wfd))
				error(FATAL,
				    "%s: %s", cp->u.s->wfile, strerror(errno));
			cp->u.s->wfd = -1;
			break;
		case 'w':
			if (cp->u.fd != -1 && close(cp->u.fd))
				error(FATAL, "%s: %s", cp->t, strerror(errno));
			cp->u.fd = -1;
			break;
		case '{':
			cfclose(cp->u.c, cp->next);
			break;
		}
}
