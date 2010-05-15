/*	$Id: main.c,v 1.29 2010/05/15 22:00:22 schwarze Exp $ */
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
#include <sys/types.h>
#include <sys/mman.h>
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
#define	FL_IGN_SCOPE	 (1 << 0) 	/* Ignore scope errors. */
#define	FL_NIGN_ESCAPE	 (1 << 1) 	/* Don't ignore bad escapes. */
#define	FL_NIGN_MACRO	 (1 << 2) 	/* Don't ignore bad macros. */
#define	FL_IGN_ERRORS	 (1 << 4)	/* Ignore failed parse. */
#define	FL_STRICT	  FL_NIGN_ESCAPE | \
			  FL_NIGN_MACRO
	enum intt	  inttype;	/* Input parsers... */
	struct man	 *man;
	struct mdoc	 *mdoc;
	enum outt	  outtype;	/* Output devices... */
	out_mdoc	  outmdoc;
	out_man	  	  outman;
	out_free	  outfree;
	void		 *outdata;
	char		  outopts[BUFSIZ];
};

static	void		  fdesc(struct curparse *);
static	void		  ffile(const char *, struct curparse *);
static	int		  foptions(int *, char *);
static	struct man	 *man_init(struct curparse *);
static	struct mdoc	 *mdoc_init(struct curparse *);
static	int		  merr(void *, int, int, const char *);
static	int		  moptions(enum intt *, char *);
static	int		  mwarn(void *, int, int, const char *);
static	int		  pset(const char *, int, struct curparse *,
				struct man **, struct mdoc **);
static	int		  toptions(struct curparse *, char *);
static	void		  usage(void) __attribute__((noreturn));
static	void		  version(void) __attribute__((noreturn));
static	int		  woptions(int *, char *);

static	const char	 *progname;
static 	int		  with_error;
static	int		  with_warning;

int
main(int argc, char *argv[])
{
	int		 c;
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
			if ( ! toptions(&curp, optarg))
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

	if (NULL == *argv) {
		curp.file = "<stdin>";
		curp.fd = STDIN_FILENO;

		fdesc(&curp);
	}

	while (*argv) {
		ffile(*argv, &curp);

		if (with_error && !(curp.fflags & FL_IGN_ERRORS))
			break;
		++argv;
	}

	if (curp.outfree)
		(*curp.outfree)(curp.outdata);

	return((with_warning || with_error) ? 
			EXIT_FAILURE :  EXIT_SUCCESS);
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

	(void)fprintf(stderr, "usage: %s [-V] [-foption] "
			"[-mformat] [-Ooption] [-Toutput] "
			"[-Werr] [file...]\n", progname);
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

	pflags = MAN_IGN_MACRO | MAN_IGN_ESCAPE;

	if (curp->fflags & FL_NIGN_MACRO)
		pflags &= ~MAN_IGN_MACRO;
	if (curp->fflags & FL_NIGN_ESCAPE)
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

	pflags = MDOC_IGN_MACRO | MDOC_IGN_ESCAPE;

	if (curp->fflags & FL_IGN_SCOPE)
		pflags |= MDOC_IGN_SCOPE;
	if (curp->fflags & FL_NIGN_ESCAPE)
		pflags &= ~MDOC_IGN_ESCAPE;
	if (curp->fflags & FL_NIGN_MACRO)
		pflags &= ~MDOC_IGN_MACRO;

	return(mdoc_alloc(curp, pflags, &mdoccb));
}


static void
ffile(const char *file, struct curparse *curp)
{

	curp->file = file;
	if (-1 == (curp->fd = open(curp->file, O_RDONLY, 0))) {
		perror(curp->file);
		with_error = 1;
		return;
	}

	fdesc(curp);

	if (-1 == close(curp->fd))
		perror(curp->file);
}


static int
resize_buf(struct buf *buf, size_t initial)
{
	void *tmp;
	size_t sz;

	if (buf->sz == 0)
		sz = initial;
	else
		sz = 2 * buf->sz;
	tmp = realloc(buf->buf, sz);
	if (NULL == tmp) {
		perror(NULL);
		return(0);
	}
	buf->buf = tmp;
	buf->sz = sz;
	return(1);
}


static int
read_whole_file(struct curparse *curp, struct buf *fb, int *with_mmap)
{
	struct stat	 st;
	size_t		 off;
	ssize_t		 ssz;

	if (-1 == fstat(curp->fd, &st)) {
		perror(curp->file);
		with_error = 1;
		return(0);
	}

	/*
	 * If we're a regular file, try just reading in the whole entry
	 * via mmap().  This is faster than reading it into blocks, and
	 * since each file is only a few bytes to begin with, I'm not
	 * concerned that this is going to tank any machines.
	 */

	if (S_ISREG(st.st_mode)) {
		if (st.st_size >= (1U << 31)) {
			fprintf(stderr, "%s: input too large\n", 
					curp->file);
			with_error = 1;
			return(0);
		}
		*with_mmap = 1;
		fb->sz = st.st_size;
		fb->buf = mmap(NULL, fb->sz, PROT_READ, 
				MAP_FILE, curp->fd, 0);
		if (fb->buf != MAP_FAILED)
			return(1);
	}

	/*
	 * If this isn't a regular file (like, say, stdin), then we must
	 * go the old way and just read things in bit by bit.
	 */

	*with_mmap = 0;
	off = 0;
	fb->sz = 0;
	fb->buf = NULL;
	for (;;) {
		if (off == fb->sz) {
			if (fb->sz == (1U << 31)) {
				fprintf(stderr, "%s: input too large\n", 
						curp->file);
				break;
			}
			if (! resize_buf(fb, 65536))
				break;
		}
		ssz = read(curp->fd, fb->buf + off, fb->sz - off);
		if (ssz == 0) {
			fb->sz = off;
			return(1);
		}
		if (ssz == -1) {
			perror(curp->file);
			break;
		}
		off += ssz;
	}

	free(fb->buf);
	fb->buf = NULL;
	with_error = 1;
	return(0);
}


static void
fdesc(struct curparse *curp)
{
	struct buf	 ln, blk;
	int		 i, pos, lnn, lnn_start, with_mmap;
	struct man	*man;
	struct mdoc	*mdoc;

	man = NULL;
	mdoc = NULL;
	memset(&ln, 0, sizeof(struct buf));

	/*
	 * Two buffers: ln and buf.  buf is the input file and may be
	 * memory mapped.  ln is a line buffer and grows on-demand.
	 */

	if (!read_whole_file(curp, &blk, &with_mmap))
		return;

	for (i = 0, lnn = 1; i < (int)blk.sz;) {
		pos = 0;
		lnn_start = lnn;
		while (i < (int)blk.sz) {
			if ('\n' == blk.buf[i]) {
				++i;
				++lnn;
				break;
			}
			/* Trailing backslash is like a plain character. */
			if ('\\' != blk.buf[i] || i + 1 == (int)blk.sz) {
				if (pos >= (int)ln.sz)
					if (! resize_buf(&ln, 256))
						goto bailout;
				ln.buf[pos++] = blk.buf[i++];
				continue;
			}
			/* Found an escape and at least one other character. */
			if ('\n' == blk.buf[i + 1]) {
				/* Escaped newlines are skipped over */
				i += 2;
				++lnn;
				continue;
			}
			if ('"' == blk.buf[i + 1]) {
				i += 2;
				/* Comment, skip to end of line */
				for (; i < (int)blk.sz; ++i) {
					if ('\n' == blk.buf[i]) {
						++i;
						++lnn;
						break;
					}
				}
				/* Backout trailing whitespaces */
				for (; pos > 0; --pos) {
					if (ln.buf[pos - 1] != ' ')
						break;
					if (pos > 2 && ln.buf[pos - 2] == '\\')
						break;
				}
				break;
			}
			/* Some other escape sequence, copy and continue. */
			if (pos + 1 >= (int)ln.sz)
				if (! resize_buf(&ln, 256))
					goto bailout;

			ln.buf[pos++] = blk.buf[i++];
			ln.buf[pos++] = blk.buf[i++];
		}

 		if (pos >= (int)ln.sz)
			if (! resize_buf(&ln, 256))
				goto bailout;
		ln.buf[pos] = 0;

		/* If unset, assign parser in pset(). */

		if ( ! (man || mdoc) && ! pset(ln.buf, pos, curp, &man, &mdoc))
			goto bailout;

		/* Pass down into parsers. */

		if (man && ! man_parseln(man, lnn, ln.buf))
			goto bailout;
		if (mdoc && ! mdoc_parseln(mdoc, lnn, ln.buf))
			goto bailout;
	}

	/* NOTE a parser may not have been assigned, yet. */

	if ( ! (man || mdoc)) {
		fprintf(stderr, "%s: Not a manual\n", curp->file);
		goto bailout;
	}

	if (mdoc && ! mdoc_endparse(mdoc))
		goto bailout;
	if (man && ! man_endparse(man))
		goto bailout;

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
			curp->outdata = ascii_alloc(80);
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

 cleanup:
	if (curp->mdoc) {
		mdoc_free(curp->mdoc);
		curp->mdoc = NULL;
	}
	if (curp->man) {
		man_free(curp->man);
		curp->man = NULL;
	}
	if (ln.buf)
		free(ln.buf);
	if (with_mmap)
		munmap(blk.buf, blk.sz);
	else
		free(blk.buf);
	return;

 bailout:
	with_error = 1;
	goto cleanup;
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
		return(1);
	case (INTT_MAN):
		if (NULL == curp->man) 
			curp->man = man_init(curp);
		if (NULL == (*man = curp->man))
			return(0);
		return(1);
	default:
		break;
	}

	if (pos >= 3 && 0 == memcmp(buf, ".Dd", 3))  {
		if (NULL == curp->mdoc) 
			curp->mdoc = mdoc_init(curp);
		if (NULL == (*mdoc = curp->mdoc))
			return(0);
		return(1);
	} 

	if (NULL == curp->man) 
		curp->man = man_init(curp);
	if (NULL == (*man = curp->man))
		return(0);
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
toptions(struct curparse *curp, char *arg)
{

	if (0 == strcmp(arg, "ascii"))
		curp->outtype = OUTT_ASCII;
	else if (0 == strcmp(arg, "lint")) {
		curp->outtype = OUTT_LINT;
		curp->wflags |= WARN_WALL;
		curp->fflags |= FL_STRICT;
	}
	else if (0 == strcmp(arg, "tree"))
		curp->outtype = OUTT_TREE;
	else if (0 == strcmp(arg, "html"))
		curp->outtype = OUTT_HTML;
	else if (0 == strcmp(arg, "xhtml"))
		curp->outtype = OUTT_XHTML;
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
	toks[3] = "ign-errors";
	toks[4] = "strict";
	toks[5] = "ign-escape";
	toks[6] = NULL;

	while (*arg) {
		o = arg;
		switch (getsubopt(&arg, UNCONST(toks), &v)) {
		case (0):
			*fflags |= FL_IGN_SCOPE;
			break;
		case (1):
			*fflags |= FL_NIGN_ESCAPE;
			break;
		case (2):
			*fflags |= FL_NIGN_MACRO;
			break;
		case (3):
			*fflags |= FL_IGN_ERRORS;
			break;
		case (4):
			*fflags |= FL_STRICT;
			break;
		case (5):
			*fflags &= ~FL_NIGN_ESCAPE;
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

	with_error = 1;

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

	with_warning = 1;
	if (curp->wflags & WARN_WERR) {
		with_error = 1;
		return(0);
	}

	return(1);
}

