/*	$Id: main.c,v 1.46 2010/08/20 08:13:03 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010 Ingo Schwarze <schwarze@openbsd.org>
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
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "main.h"
#include "mdoc.h"
#include "man.h"
#include "roff.h"

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
	OUTT_LINT,
	OUTT_PS,
	OUTT_PDF
};

struct	curparse {
	const char	 *file;		/* Current parse. */
	int		  fd;		/* Current parse. */
	enum mandoclevel  wlevel;	/* Ignore messages below this. */
	int		  wstop;	/* Stop after a file with a warning. */
	enum intt	  inttype;	/* which parser to use */
	struct man	 *man;		/* man parser */
	struct mdoc	 *mdoc;		/* mdoc parser */
	struct roff	 *roff;		/* roff parser (!NULL) */
	struct regset	  regs;		/* roff registers */
	enum outt	  outtype; 	/* which output to use */
	out_mdoc	  outmdoc;	/* mdoc output ptr */
	out_man	  	  outman;	/* man output ptr */
	out_free	  outfree;	/* free output ptr */
	void		 *outdata;	/* data for output */
	char		  outopts[BUFSIZ]; /* buf of output opts */
};

static	const char * const	mandoclevels[MANDOCLEVEL_MAX] = {
	"SUCCESS",
	"RESERVED",
	"WARNING",
	"ERROR",
	"FATAL",
	"BADARG",
	"SYSERR"
};

static	const enum mandocerr	mandoclimits[MANDOCLEVEL_MAX] = {
	MANDOCERR_OK,
	MANDOCERR_WARNING,
	MANDOCERR_WARNING,
	MANDOCERR_ERROR,
	MANDOCERR_FATAL,
	MANDOCERR_MAX,
	MANDOCERR_MAX
};

static	const char * const	mandocerrs[MANDOCERR_MAX] = {
	"ok",

	"generic warning",

	"text should be uppercase",
	"sections out of conventional order",
	"section name repeats",
	"out of order prologue",
	"repeated prologue entry",
	"list type must come first",
	"bad standard",
	"bad library",
	"tab in non-literal context",
	"bad escape sequence",
	"unterminated quoted string",
	"argument requires the width argument",
	"superfluous width argument",
	"ignoring argument",
	"bad date argument",
	"bad width argument",
	"unknown manual section",
	"section not in conventional manual section",
	"end of line whitespace",
	"blocks badly nested",
	"scope open on exit",

	"generic error",

	"NAME section must come first",
	"bad Boolean value",
	"child violates parent syntax",
	"bad AT&T symbol",
	"list type repeated",
	"display type repeated",
	"argument repeated",
	"manual name not yet set",
	"obsolete macro ignored",
	"empty macro ignored",
	"macro not allowed in body",
	"macro not allowed in prologue",
	"bad character",
	"bad NAME section contents",
	"no blank lines",
	"no text in this context",
	"bad comment style",
	"unknown macro will be lost",
	"line scope broken",
	"argument count wrong",
	"request scope close w/none open",
	"scope already open",
	"macro requires line argument(s)",
	"macro requires body argument(s)",
	"macro requires argument(s)",
	"no title in document",
	"missing list type",
	"missing display type",
	"missing font type",
	"line argument(s) will be lost",
	"body argument(s) will be lost",

	"generic fatal error",

	"column syntax is inconsistent",
	"displays may not be nested",
	"unsupported display type",
	"blocks badly nested",
	"no such block is open",
	"scope broken, syntax violated",
	"line scope broken, syntax violated",
	"argument count wrong, violates syntax",
	"child violates parent syntax",
	"argument count wrong, violates syntax",
	"no document body",
	"no document prologue",
	"utsname system call failed",
	"static buffer exhausted",
};

static	void		  fdesc(struct curparse *);
static	void		  ffile(const char *, struct curparse *);
static	int		  moptions(enum intt *, char *);
static	int		  mmsg(enum mandocerr, void *, 
				int, int, const char *);
static	void		  pset(const char *, int, struct curparse *,
				struct man **, struct mdoc **);
static	int		  toptions(struct curparse *, char *);
static	void		  usage(void) __attribute__((noreturn));
static	void		  version(void) __attribute__((noreturn));
static	int		  woptions(struct curparse *, char *);

static	const char	 *progname;
static	enum mandoclevel  exit_status = MANDOCLEVEL_OK;

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
	curp.wlevel  = MANDOCLEVEL_FATAL;

	/* LINTED */
	while (-1 != (c = getopt(argc, argv, "m:O:T:VW:")))
		switch (c) {
		case ('m'):
			if ( ! moptions(&curp.inttype, optarg))
				return(MANDOCLEVEL_BADARG);
			break;
		case ('O'):
			(void)strlcat(curp.outopts, optarg, BUFSIZ);
			(void)strlcat(curp.outopts, ",", BUFSIZ);
			break;
		case ('T'):
			if ( ! toptions(&curp, optarg))
				return(MANDOCLEVEL_BADARG);
			break;
		case ('W'):
			if ( ! woptions(&curp, optarg))
				return(MANDOCLEVEL_BADARG);
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
		if (MANDOCLEVEL_OK != exit_status && curp.wstop)
			break;
		++argv;
	}

	if (curp.outfree)
		(*curp.outfree)(curp.outdata);
	if (curp.mdoc)
		mdoc_free(curp.mdoc);
	if (curp.man)
		man_free(curp.man);
	if (curp.roff)
		roff_free(curp.roff);

	return(exit_status);
}


static void
version(void)
{

	(void)printf("%s %s\n", progname, VERSION);
	exit(MANDOCLEVEL_OK);
}


static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-V] [-foption] "
			"[-mformat] [-Ooption] [-Toutput] "
			"[-Werr] [file...]\n", progname);
	exit(MANDOCLEVEL_BADARG);
}


static void
ffile(const char *file, struct curparse *curp)
{

	curp->file = file;
	if (-1 == (curp->fd = open(curp->file, O_RDONLY, 0))) {
		perror(curp->file);
		exit_status = MANDOCLEVEL_SYSERR;
		return;
	}

	fdesc(curp);

	if (-1 == close(curp->fd))
		perror(curp->file);
}


static void
resize_buf(struct buf *buf, size_t initial)
{

	buf->sz = buf->sz ? 2 * buf->sz : initial;
	buf->buf = realloc(buf->buf, buf->sz);
	if (NULL == buf->buf) {
		perror(NULL);
		exit(MANDOCLEVEL_SYSERR);
	}
}


static int
read_whole_file(struct curparse *curp, struct buf *fb, int *with_mmap)
{
	struct stat	 st;
	size_t		 off;
	ssize_t		 ssz;

	if (-1 == fstat(curp->fd, &st)) {
		perror(curp->file);
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
			return(0);
		}
		*with_mmap = 1;
		fb->sz = (size_t)st.st_size;
		fb->buf = mmap(NULL, fb->sz, PROT_READ, 
				MAP_FILE|MAP_SHARED, curp->fd, 0);
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
			resize_buf(fb, 65536);
		}
		ssz = read(curp->fd, fb->buf + (int)off, fb->sz - off);
		if (ssz == 0) {
			fb->sz = off;
			return(1);
		}
		if (ssz == -1) {
			perror(curp->file);
			break;
		}
		off += (size_t)ssz;
	}

	free(fb->buf);
	fb->buf = NULL;
	return(0);
}


static void
fdesc(struct curparse *curp)
{
	struct buf	 ln, blk;
	int		 i, pos, lnn, lnn_start, with_mmap, of;
	enum rofferr	 re;
	unsigned char	 c;
	struct man	*man;
	struct mdoc	*mdoc;
	struct roff	*roff;

	man = NULL;
	mdoc = NULL;
	roff = NULL;

	memset(&ln, 0, sizeof(struct buf));

	/*
	 * Two buffers: ln and buf.  buf is the input file and may be
	 * memory mapped.  ln is a line buffer and grows on-demand.
	 */

	if ( ! read_whole_file(curp, &blk, &with_mmap)) {
		exit_status = MANDOCLEVEL_SYSERR;
		return;
	}

	if (NULL == curp->roff) 
		curp->roff = roff_alloc(&curp->regs, curp, mmsg);
	assert(curp->roff);
	roff = curp->roff;

	for (i = 0, lnn = 1; i < (int)blk.sz;) {
		pos = 0;
		lnn_start = lnn;
		while (i < (int)blk.sz) {
			if ('\n' == blk.buf[i]) {
				++i;
				++lnn;
				break;
			}

			/* 
			 * Warn about bogus characters.  If you're using
			 * non-ASCII encoding, you're screwing your
			 * readers.  Since I'd rather this not happen,
			 * I'll be helpful and drop these characters so
			 * we don't display gibberish.  Note to manual
			 * writers: use special characters.
			 */

			c = (unsigned char) blk.buf[i];
			if ( ! (isascii(c) && (isgraph(c) || isblank(c)))) {
				mmsg(MANDOCERR_BADCHAR, curp, 
				    lnn_start, pos, "ignoring byte");
				i++;
				continue;
			}

			/* Trailing backslash is like a plain character. */
			if ('\\' != blk.buf[i] || i + 1 == (int)blk.sz) {
				if (pos >= (int)ln.sz)
					resize_buf(&ln, 256);
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
				resize_buf(&ln, 256);

			ln.buf[pos++] = blk.buf[i++];
			ln.buf[pos++] = blk.buf[i++];
		}

 		if (pos >= (int)ln.sz)
			resize_buf(&ln, 256);
		ln.buf[pos] = '\0';

		/*
		 * A significant amount of complexity is contained by
		 * the roff preprocessor.  It's line-oriented but can be
		 * expressed on one line, so we need at times to
		 * readjust our starting point and re-run it.  The roff
		 * preprocessor can also readjust the buffers with new
		 * data, so we pass them in wholesale.
		 */

		of = 0;
		do {
			re = roff_parseln(roff, lnn_start, 
					&ln.buf, &ln.sz, of, &of);
		} while (ROFF_RERUN == re);

		if (ROFF_IGN == re) {
			continue;
		} else if (ROFF_ERR == re) {
			assert(MANDOCLEVEL_FATAL <= exit_status);
			goto cleanup;
		}

		/*
		 * If input parsers have not been allocated, do so now.
		 * We keep these instanced betwen parsers, but set them
		 * locally per parse routine since we can use different
		 * parsers with each one.
		 */

		if ( ! (man || mdoc))
			pset(ln.buf + of, pos - of, curp, &man, &mdoc);

		/* Lastly, push down into the parsers themselves. */

		if (man && ! man_parseln(man, lnn_start, ln.buf, of)) {
			assert(MANDOCLEVEL_FATAL <= exit_status);
			goto cleanup;
		}
		if (mdoc && ! mdoc_parseln(mdoc, lnn_start, ln.buf, of)) {
			assert(MANDOCLEVEL_FATAL <= exit_status);
			goto cleanup;
		}
	}

	/* NOTE a parser may not have been assigned, yet. */

	if ( ! (man || mdoc)) {
		fprintf(stderr, "%s: Not a manual\n", curp->file);
		exit_status = MANDOCLEVEL_FATAL;
		goto cleanup;
	}

	/* Clean up the parse routine ASTs. */

	if (mdoc && ! mdoc_endparse(mdoc)) {
		assert(MANDOCLEVEL_FATAL <= exit_status);
		goto cleanup;
	}
	if (man && ! man_endparse(man)) {
		assert(MANDOCLEVEL_FATAL <= exit_status);
		goto cleanup;
	}
	if (roff && ! roff_endparse(roff)) {
		assert(MANDOCLEVEL_FATAL <= exit_status);
		goto cleanup;
	}

	/*
	 * With -Wstop and warnings or errors of at least
	 * the requested level, do not produce output.
	 */

	if (MANDOCLEVEL_OK != exit_status && curp->wstop)
		goto cleanup;

	/* If unset, allocate output dev now (if applicable). */

	if ( ! (curp->outman && curp->outmdoc)) {
		switch (curp->outtype) {
		case (OUTT_XHTML):
			curp->outdata = xhtml_alloc(curp->outopts);
			break;
		case (OUTT_HTML):
			curp->outdata = html_alloc(curp->outopts);
			break;
		case (OUTT_ASCII):
			curp->outdata = ascii_alloc(curp->outopts);
			curp->outfree = ascii_free;
			break;
		case (OUTT_PDF):
			curp->outdata = pdf_alloc(curp->outopts);
			curp->outfree = pspdf_free;
			break;
		case (OUTT_PS):
			curp->outdata = ps_alloc(curp->outopts);
			curp->outfree = pspdf_free;
			break;
		default:
			break;
		}

		switch (curp->outtype) {
		case (OUTT_HTML):
			/* FALLTHROUGH */
		case (OUTT_XHTML):
			curp->outman = html_man;
			curp->outmdoc = html_mdoc;
			curp->outfree = html_free;
			break;
		case (OUTT_TREE):
			curp->outman = tree_man;
			curp->outmdoc = tree_mdoc;
			break;
		case (OUTT_PDF):
			/* FALLTHROUGH */
		case (OUTT_ASCII):
			/* FALLTHROUGH */
		case (OUTT_PS):
			curp->outman = terminal_man;
			curp->outmdoc = terminal_mdoc;
			break;
		default:
			break;
		}
	}

	/* Execute the out device, if it exists. */

	if (man && curp->outman)
		(*curp->outman)(curp->outdata, man);
	if (mdoc && curp->outmdoc)
		(*curp->outmdoc)(curp->outdata, mdoc);

 cleanup:
	memset(&curp->regs, 0, sizeof(struct regset));
	if (mdoc)
		mdoc_reset(mdoc);
	if (man)
		man_reset(man);
	if (roff)
		roff_reset(roff);
	if (ln.buf)
		free(ln.buf);
	if (with_mmap)
		munmap(blk.buf, blk.sz);
	else
		free(blk.buf);

	return;
}


static void
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

	if ('.' == buf[0] || '\'' == buf[0]) {
		for (i = 1; buf[i]; i++)
			if (' ' != buf[i] && '\t' != buf[i])
				break;
		if ('\0' == buf[i])
			return;
	}

	switch (curp->inttype) {
	case (INTT_MDOC):
		if (NULL == curp->mdoc) 
			curp->mdoc = mdoc_alloc(&curp->regs, curp, mmsg);
		assert(curp->mdoc);
		*mdoc = curp->mdoc;
		return;
	case (INTT_MAN):
		if (NULL == curp->man) 
			curp->man = man_alloc(&curp->regs, curp, mmsg);
		assert(curp->man);
		*man = curp->man;
		return;
	default:
		break;
	}

	if (pos >= 3 && 0 == memcmp(buf, ".Dd", 3))  {
		if (NULL == curp->mdoc) 
			curp->mdoc = mdoc_alloc(&curp->regs, curp, mmsg);
		assert(curp->mdoc);
		*mdoc = curp->mdoc;
		return;
	} 

	if (NULL == curp->man) 
		curp->man = man_alloc(&curp->regs, curp, mmsg);
	assert(curp->man);
	*man = curp->man;
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
		curp->wlevel  = MANDOCLEVEL_WARNING;
	}
	else if (0 == strcmp(arg, "tree"))
		curp->outtype = OUTT_TREE;
	else if (0 == strcmp(arg, "html"))
		curp->outtype = OUTT_HTML;
	else if (0 == strcmp(arg, "xhtml"))
		curp->outtype = OUTT_XHTML;
	else if (0 == strcmp(arg, "ps"))
		curp->outtype = OUTT_PS;
	else if (0 == strcmp(arg, "pdf"))
		curp->outtype = OUTT_PDF;
	else {
		fprintf(stderr, "%s: Bad argument\n", arg);
		return(0);
	}

	return(1);
}


static int
woptions(struct curparse *curp, char *arg)
{
	char		*v, *o;
	const char	*toks[6]; 

	toks[0] = "stop";
	toks[1] = "all";
	toks[2] = "warning";
	toks[3] = "error";
	toks[4] = "fatal";
	toks[5] = NULL;

	while (*arg) {
		o = arg;
		switch (getsubopt(&arg, UNCONST(toks), &v)) {
		case (0):
			curp->wstop = 1;
			break;
		case (1):
			/* FALLTHROUGH */
		case (2):
			curp->wlevel = MANDOCLEVEL_WARNING;
			break;
		case (3):
			curp->wlevel = MANDOCLEVEL_ERROR;
			break;
		case (4):
			curp->wlevel = MANDOCLEVEL_FATAL;
			break;
		default:
			fprintf(stderr, "-W%s: Bad argument\n", o);
			return(0);
		}
	}

	return(1);
}


static int
mmsg(enum mandocerr t, void *arg, int ln, int col, const char *msg)
{
	struct curparse *cp;
	enum mandoclevel level;

	level = MANDOCLEVEL_FATAL;
	while (t < mandoclimits[level])
		level--;

	cp = (struct curparse *)arg;
	if (level < cp->wlevel)
		return(1);

	fprintf(stderr, "%s:%d:%d: %s: %s",
	    cp->file, ln, col + 1, mandoclevels[level], mandocerrs[t]);
	if (msg)
		fprintf(stderr, ": %s", msg);
	fputc('\n', stderr);

	if (exit_status < level)
		exit_status = level;
	
	return(level < MANDOCLEVEL_FATAL);
}
