/*	$Id: main.c,v 1.21 2010/02/18 02:11:26 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mdoc.h"
#include "man.h"
#include "main.h"

#define	UNCONST(a)	((void *)(uintptr_t)(const void *)(a))

typedef	void		(*out_mdoc)(void *, const struct mdoc *);
typedef	void		(*out_man)(void *, const struct man *);
typedef	void		(*out_free)(void *);

struct	buf {
	char	 	 *buf;
	size_t		  sz;
};

enum	intt {
	INTT_AUTO,
	INTT_MDOC,
	INTT_MAN
};

enum	outt {
	OUTT_ASCII = 0,
	OUTT_TREE,
	OUTT_HTML,
	OUTT_XHTML,
	OUTT_LINT
};

struct	curparse {
	const char	 *file;		/* Current parse. */
	int		  fd;		/* Current parse. */
	int		  wflags;
#define	WARN_WALL	 (1 << 0)	/* All-warnings mask. */
#define	WARN_WERR	 (1 << 2)	/* Warnings->errors. */
	int		  fflags;
#define	IGN_SCOPE	 (1 << 0) 	/* Ignore scope errors. */
#define	NO_IGN_ESCAPE	 (1 << 1) 	/* Don't ignore bad escapes. */
#define	NO_IGN_MACRO	 (1 << 2) 	/* Don't ignore bad macros. */
#define	NO_IGN_CHARS	 (1 << 3)	/* Don't ignore bad chars. */
#define	IGN_ERRORS	 (1 << 4)	/* Ignore failed parse. */
	enum intt	  inttype;	/* Input parsers... */
	struct man	 *man;
	struct man	 *lastman;
	struct mdoc	 *mdoc;
	struct mdoc	 *lastmdoc;
	enum outt	  outtype;	/* Output devices... */
	out_mdoc	  outmdoc;
	out_man	  	  outman;
	out_free	  outfree;
	void		 *outdata;
	char		  outopts[BUFSIZ];
};

static	int		  foptions(int *, char *);
static	int		  toptions(enum outt *, char *);
static	int		  moptions(enum intt *, char *);
static	int		  woptions(int *, char *);
static	int		  merr(void *, int, int, const char *);
static	int		  mwarn(void *, int, int, const char *);
static	int		  ffile(struct buf *, struct buf *, 
				const char *, struct curparse *);
static	int		  fdesc(struct buf *, struct buf *,
				struct curparse *);
static	int		  pset(const char *, int, struct curparse *,
				struct man **, struct mdoc **);
static	struct man	 *man_init(struct curparse *);
static	struct mdoc	 *mdoc_init(struct curparse *);
static	void		  version(void) __attribute__((noreturn));
static	void		  usage(void) __attribute__((noreturn));

static	const char	 *progname;


int
main(int argc, char *argv[])
{
	int		 c, rc;
	struct buf	 ln, blk;
	struct curparse	 curp;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	memset(&curp, 0, sizeof(struct curparse));

	curp.inttype = INTT_AUTO;
	curp.outtype = OUTT_ASCII;

	/* LINTED */
	while (-1 != (c = getopt(argc, argv, "f:m:O:T:VW:")))
		switch (c) {
		case ('f'):
			if ( ! foptions(&curp.fflags, optarg))
				return(EXIT_FAILURE);
			break;
		case ('m'):
			if ( ! moptions(&curp.inttype, optarg))
				return(EXIT_FAILURE);
			break;
		case ('O'):
			(void)strlcat(curp.outopts, optarg, BUFSIZ);
			(void)strlcat(curp.outopts, ",", BUFSIZ);
			break;
		case ('T'):
			if ( ! toptions(&curp.outtype, optarg))
				return(EXIT_FAILURE);
			break;
		case ('W'):
			if ( ! woptions(&curp.wflags, optarg))
				return(EXIT_FAILURE);
			break;
		case ('V'):
			version();
			/* NOTREACHED */
		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	memset(&ln, 0, sizeof(struct buf));
	memset(&blk, 0, sizeof(struct buf));

	rc = 1;

	if (NULL == *argv) {
		curp.file = "<stdin>";
		curp.fd = STDIN_FILENO;

		c = fdesc(&blk, &ln, &curp);
		if ( ! (IGN_ERRORS & curp.fflags)) 
			rc = 1 == c ? 1 : 0;
		else
			rc = -1 == c ? 0 : 1;
	}

	while (rc && *argv) {
		c = ffile(&blk, &ln, *argv, &curp);
		if ( ! (IGN_ERRORS & curp.fflags)) 
			rc = 1 == c ? 1 : 0;
		else
			rc = -1 == c ? 0 : 1;

		argv++;
		if (*argv && rc) {
			if (curp.lastman)
				man_reset(curp.lastman);
			if (curp.lastmdoc)
				mdoc_reset(curp.lastmdoc);
			curp.lastman = NULL;
			curp.lastmdoc = NULL;
		}
	}

	if (blk.buf)
		free(blk.buf);
	if (ln.buf)
		free(ln.buf);
	if (curp.outfree)
		(*curp.outfree)(curp.outdata);
	if (curp.mdoc)
		mdoc_free(curp.mdoc);
	if (curp.man)
		man_free(curp.man);

	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
}


static void
version(void)
{

	(void)printf("%s %s\n", progname, VERSION);
	exit(EXIT_SUCCESS);
}


static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-V] [-foption...] "
			"[-mformat] [-Ooption] [-Toutput] "
			"[-Werr...]\n", progname);
	exit(EXIT_FAILURE);
}


static struct man *
man_init(struct curparse *curp)
{
	int		 pflags;
	struct man_cb	 mancb;

	mancb.man_err = merr;
	mancb.man_warn = mwarn;

	/* Defaults from mandoc.1. */

	pflags = MAN_IGN_MACRO | MAN_IGN_ESCAPE | MAN_IGN_CHARS;

	if (curp->fflags & NO_IGN_MACRO)
		pflags &= ~MAN_IGN_MACRO;
	if (curp->fflags & NO_IGN_CHARS)
		pflags &= ~MAN_IGN_CHARS;
	if (curp->fflags & NO_IGN_ESCAPE)
		pflags &= ~MAN_IGN_ESCAPE;

	return(man_alloc(curp, pflags, &mancb));
}


static struct mdoc *
mdoc_init(struct curparse *curp)
{
	int		 pflags;
	struct mdoc_cb	 mdoccb;

	mdoccb.mdoc_err = merr;
	mdoccb.mdoc_warn = mwarn;

	/* Defaults from mandoc.1. */

	pflags = MDOC_IGN_MACRO | MDOC_IGN_ESCAPE | MDOC_IGN_CHARS;

	if (curp->fflags & IGN_SCOPE)
		pflags |= MDOC_IGN_SCOPE;
	if (curp->fflags & NO_IGN_ESCAPE)
		pflags &= ~MDOC_IGN_ESCAPE;
	if (curp->fflags & NO_IGN_MACRO)
		pflags &= ~MDOC_IGN_MACRO;
	if (curp->fflags & NO_IGN_CHARS)
		pflags &= ~MDOC_IGN_CHARS;

	return(mdoc_alloc(curp, pflags, &mdoccb));
}


static int
ffile(struct buf *blk, struct buf *ln, 
		const char *file, struct curparse *curp)
{
	int		 c;

	curp->file = file;
	if (-1 == (curp->fd = open(curp->file, O_RDONLY, 0))) {
		perror(curp->file);
		return(-1);
	}

	c = fdesc(blk, ln, curp);

	if (-1 == close(curp->fd))
		perror(curp->file);

	return(c);
}


static int
fdesc(struct buf *blk, struct buf *ln, struct curparse *curp)
{
	size_t		 sz;
	ssize_t		 ssz;
	struct stat	 st;
	int		 j, i, pos, lnn, comment;
	struct man	*man;
	struct mdoc	*mdoc;

	sz = BUFSIZ;
	man = NULL;
	mdoc = NULL;

	/*
	 * Two buffers: ln and buf.  buf is the input buffer optimised
	 * here for each file's block size.  ln is a line buffer.  Both
	 * growable, hence passed in by ptr-ptr.
	 */

	if (-1 == fstat(curp->fd, &st))
		perror(curp->file);
	else if ((size_t)st.st_blksize > sz)
		sz = st.st_blksize;

	if (sz > blk->sz) {
		blk->buf = realloc(blk->buf, sz);
		if (NULL == blk->buf) {
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		blk->sz = sz;
	}

	/* Fill buf with file blocksize. */

	for (lnn = pos = comment = 0; ; ) {
		if (-1 == (ssz = read(curp->fd, blk->buf, sz))) {
			perror(curp->file);
			return(-1);
		} else if (0 == ssz) 
			break;

		/* Parse the read block into partial or full lines. */

		for (i = 0; i < (int)ssz; i++) {
			if (pos >= (int)ln->sz) {
				ln->sz += 256; /* Step-size. */
				ln->buf = realloc(ln->buf, ln->sz);
				if (NULL == ln->buf) {
					perror(NULL);
					return(EXIT_FAILURE);
				}
			}

			if ('\n' != blk->buf[i]) {
				if (comment)
					continue;
				ln->buf[pos++] = blk->buf[i];

				/* Handle in-line `\"' comments. */

				if (1 == pos || '\"' != ln->buf[pos - 1])
					continue;

				for (j = pos - 2; j >= 0; j--)
					if ('\\' != ln->buf[j])
						break;

				if ( ! ((pos - 2 - j) % 2))
					continue;

				comment = 1;
				pos -= 2;
				continue;
			} 

			/* Handle escaped `\\n' newlines. */

			if (pos > 0 && 0 == comment && 
					'\\' == ln->buf[pos - 1]) {
				for (j = pos - 1; j >= 0; j--)
					if ('\\' != ln->buf[j])
						break;
				if ( ! ((pos - j) % 2)) {
					pos--;
					lnn++;
					continue;
				}
			}

			ln->buf[pos] = 0;
			lnn++;

			/* If unset, assign parser in pset(). */

			if ( ! (man || mdoc) && ! pset(ln->buf, 
						pos, curp, &man, &mdoc))
				return(-1);

			pos = comment = 0;

			/* Pass down into parsers. */

			if (man && ! man_parseln(man, lnn, ln->buf))
				return(0);
			if (mdoc && ! mdoc_parseln(mdoc, lnn, ln->buf))
				return(0);
		}
	}

	/* NOTE a parser may not have been assigned, yet. */

	if ( ! (man || mdoc)) {
		fprintf(stderr, "%s: Not a manual\n", curp->file);
		return(0);
	}

	if (mdoc && ! mdoc_endparse(mdoc))
		return(0);
	if (man && ! man_endparse(man))
		return(0);

	/* If unset, allocate output dev now (if applicable). */

	if ( ! (curp->outman && curp->outmdoc)) {
		switch (curp->outtype) {
		case (OUTT_XHTML):
			curp->outdata = xhtml_alloc(curp->outopts);
			curp->outman = html_man;
			curp->outmdoc = html_mdoc;
			curp->outfree = html_free;
			break;
		case (OUTT_HTML):
			curp->outdata = html_alloc(curp->outopts);
			curp->outman = html_man;
			curp->outmdoc = html_mdoc;
			curp->outfree = html_free;
			break;
		case (OUTT_TREE):
			curp->outman = tree_man;
			curp->outmdoc = tree_mdoc;
			break;
		case (OUTT_LINT):
			break;
		default:
			curp->outdata = ascii_alloc();
			curp->outman = terminal_man;
			curp->outmdoc = terminal_mdoc;
			curp->outfree = terminal_free;
			break;
		}
	}

	/* Execute the out device, if it exists. */

	if (man && curp->outman)
		(*curp->outman)(curp->outdata, man);
	if (mdoc && curp->outmdoc)
		(*curp->outmdoc)(curp->outdata, mdoc);

	return(1);
}


static int
pset(const char *buf, int pos, struct curparse *curp,
		struct man **man, struct mdoc **mdoc)
{
	int		 i;

	/*
	 * Try to intuit which kind of manual parser should be used.  If
	 * passed in by command-line (-man, -mdoc), then use that
	 * explicitly.  If passed as -mandoc, then try to guess from the
	 * line: either skip dot-lines, use -mdoc when finding `.Dt', or
	 * default to -man, which is more lenient.
	 */

	if (buf[0] == '.') {
		for (i = 1; buf[i]; i++)
			if (' ' != buf[i] && '\t' != buf[i])
				break;
		if (0 == buf[i])
			return(1);
	}

	switch (curp->inttype) {
	case (INTT_MDOC):
		if (NULL == curp->mdoc) 
			curp->mdoc = mdoc_init(curp);
		if (NULL == (*mdoc = curp->mdoc))
			return(0);
		curp->lastmdoc = *mdoc;
		return(1);
	case (INTT_MAN):
		if (NULL == curp->man) 
			curp->man = man_init(curp);
		if (NULL == (*man = curp->man))
			return(0);
		curp->lastman = *man;
		return(1);
	default:
		break;
	}

	if (pos >= 3 && 0 == memcmp(buf, ".Dd", 3))  {
		if (NULL == curp->mdoc) 
			curp->mdoc = mdoc_init(curp);
		if (NULL == (*mdoc = curp->mdoc))
			return(0);
		curp->lastmdoc = *mdoc;
		return(1);
	} 

	if (NULL == curp->man) 
		curp->man = man_init(curp);
	if (NULL == (*man = curp->man))
		return(0);
	curp->lastman = *man;
	return(1);
}


static int
moptions(enum intt *tflags, char *arg)
{

	if (0 == strcmp(arg, "doc"))
		*tflags = INTT_MDOC;
	else if (0 == strcmp(arg, "andoc"))
		*tflags = INTT_AUTO;
	else if (0 == strcmp(arg, "an"))
		*tflags = INTT_MAN;
	else {
		fprintf(stderr, "%s: Bad argument\n", arg);
		return(0);
	}

	return(1);
}


static int
toptions(enum outt *tflags, char *arg)
{

	if (0 == strcmp(arg, "ascii"))
		*tflags = OUTT_ASCII;
	else if (0 == strcmp(arg, "lint"))
		*tflags = OUTT_LINT;
	else if (0 == strcmp(arg, "tree"))
		*tflags = OUTT_TREE;
	else if (0 == strcmp(arg, "html"))
		*tflags = OUTT_HTML;
	else if (0 == strcmp(arg, "xhtml"))
		*tflags = OUTT_XHTML;
	else {
		fprintf(stderr, "%s: Bad argument\n", arg);
		return(0);
	}

	return(1);
}


static int
foptions(int *fflags, char *arg)
{
	char		*v, *o;
	const char	*toks[8];

	toks[0] = "ign-scope";
	toks[1] = "no-ign-escape";
	toks[2] = "no-ign-macro";
	toks[3] = "no-ign-chars";
	toks[4] = "ign-errors";
	toks[5] = "strict";
	toks[6] = "ign-escape";
	toks[7] = NULL;

	while (*arg) {
		o = arg;
		switch (getsubopt(&arg, UNCONST(toks), &v)) {
		case (0):
			*fflags |= IGN_SCOPE;
			break;
		case (1):
			*fflags |= NO_IGN_ESCAPE;
			break;
		case (2):
			*fflags |= NO_IGN_MACRO;
			break;
		case (3):
			*fflags |= NO_IGN_CHARS;
			break;
		case (4):
			*fflags |= IGN_ERRORS;
			break;
		case (5):
			*fflags |= NO_IGN_ESCAPE | 
			 	   NO_IGN_MACRO | NO_IGN_CHARS;
			break;
		case (6):
			*fflags &= ~NO_IGN_ESCAPE;
			break;
		default:
			fprintf(stderr, "%s: Bad argument\n", o);
			return(0);
		}
	}

	return(1);
}


static int
woptions(int *wflags, char *arg)
{
	char		*v, *o;
	const char	*toks[3]; 

	toks[0] = "all";
	toks[1] = "error";
	toks[2] = NULL;

	while (*arg) {
		o = arg;
		switch (getsubopt(&arg, UNCONST(toks), &v)) {
		case (0):
			*wflags |= WARN_WALL;
			break;
		case (1):
			*wflags |= WARN_WERR;
			break;
		default:
			fprintf(stderr, "%s: Bad argument\n", o);
			return(0);
		}
	}

	return(1);
}


/* ARGSUSED */
static int
merr(void *arg, int line, int col, const char *msg)
{
	struct curparse *curp;

	curp = (struct curparse *)arg;

	(void)fprintf(stderr, "%s:%d:%d: error: %s\n", 
			curp->file, line, col + 1, msg);

	return(0);
}


static int
mwarn(void *arg, int line, int col, const char *msg)
{
	struct curparse *curp;

	curp = (struct curparse *)arg;

	if ( ! (curp->wflags & WARN_WALL))
		return(1);

	(void)fprintf(stderr, "%s:%d:%d: warning: %s\n", 
			curp->file, line, col + 1, msg);

	if ( ! (curp->wflags & WARN_WERR))
		return(1);
	
	return(0);
}

