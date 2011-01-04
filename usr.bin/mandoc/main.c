/*	$Id: main.c,v 1.64 2011/01/04 22:28:17 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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

#define	REPARSE_LIMIT	1000
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
	int		  line;		/* Line number in the file. */
	enum mandoclevel  wlevel;	/* Ignore messages below this. */
	int		  wstop;	/* Stop after a file with a warning. */
	enum intt	  inttype;	/* which parser to use */
	struct man	 *pman;		/* persistent man parser */
	struct mdoc	 *pmdoc;	/* persistent mdoc parser */
	struct man	 *man;		/* man parser */
	struct mdoc	 *mdoc;		/* mdoc parser */
	struct roff	 *roff;		/* roff parser (!NULL) */
	struct regset	  regs;		/* roff registers */
	int		  reparse_count; /* finite interpolation stack */
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

	/* related to the prologue */
	"no title in document",
	"document title should be all caps",
	"unknown manual section",
	"cannot parse date argument",
	"prologue macros out of order",
	"duplicate prologue macro",
	"macro not allowed in prologue",
	"macro not allowed in body",

	/* related to document structure */
	".so is fragile, better use ln(1)",
	"NAME section must come first",
	"bad NAME section contents",
	"manual name not yet set",
	"sections out of conventional order",
	"duplicate section name",
	"section not in conventional manual section",

	/* related to macros and nesting */
	"skipping obsolete macro",
	"skipping paragraph macro",
	"blocks badly nested",
	"child violates parent syntax",
	"nested displays are not portable",
	"already in literal mode",

	/* related to missing macro arguments */
	"skipping empty macro",
	"argument count wrong",
	"missing display type",
	"list type must come first",
	"tag lists require a width argument",
	"missing font type",

	/* related to bad macro arguments */
	"skipping argument",
	"duplicate argument",
	"duplicate display type",
	"duplicate list type",
	"unknown AT&T UNIX version",
	"bad Boolean value",
	"unknown font",
	"unknown standard specifier",
	"bad width argument",

	/* related to plain text */
	"blank line in non-literal context",
	"tab in non-literal context",
	"end of line whitespace",
	"bad comment style",
	"unknown escape sequence",
	"unterminated quoted string",
	
	/* related to tables */
	"extra data cells",

	"generic error",

	/* related to tables */
	"bad table syntax",
	"bad table option",
	"bad table layout",
	"no table layout cells specified",
	"no table data cells specified",
	"ignore data in cell",
	"data block still open",

	"input stack limit exceeded, infinite loop?",
	"skipping bad character",
	"skipping text before the first section header",
	"skipping unknown macro",
	"NOT IMPLEMENTED: skipping request",
	"line scope broken",
	"argument count wrong",
	"skipping end of block that is not open",
	"missing end of block",
	"scope open on exit",
	"uname(3) system call failed",
	"macro requires line argument(s)",
	"macro requires body argument(s)",
	"macro requires argument(s)",
	"missing list type",
	"line argument(s) will be lost",
	"body argument(s) will be lost",

	"generic fatal error",

	"column syntax is inconsistent",
	"NOT IMPLEMENTED: .Bd -file",
	"line scope broken, syntax violated",
	"argument count wrong, violates syntax",
	"child violates parent syntax",
	"argument count wrong, violates syntax",
	"NOT IMPLEMENTED: .so with absolute path or \"..\"",
	"no document body",
	"no document prologue",
	"static buffer exhausted",
};

static	void		  parsebuf(struct curparse *, struct buf, int);
static	void		  pdesc(struct curparse *);
static	void		  fdesc(struct curparse *);
static	void		  ffile(const char *, struct curparse *);
static	int		  pfile(const char *, struct curparse *);
static	int		  moptions(enum intt *, char *);
static	int		  mmsg(enum mandocerr, void *, 
				int, int, const char *);
static	void		  pset(const char *, int, struct curparse *);
static	int		  toptions(struct curparse *, char *);
static	void		  usage(void) __attribute__((noreturn));
static	void		  version(void) __attribute__((noreturn));
static	int		  woptions(struct curparse *, char *);

static	const char	 *progname;
static	enum mandoclevel  file_status = MANDOCLEVEL_OK;
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
				return((int)MANDOCLEVEL_BADARG);
			break;
		case ('O'):
			(void)strlcat(curp.outopts, optarg, BUFSIZ);
			(void)strlcat(curp.outopts, ",", BUFSIZ);
			break;
		case ('T'):
			if ( ! toptions(&curp, optarg))
				return((int)MANDOCLEVEL_BADARG);
			break;
		case ('W'):
			if ( ! woptions(&curp, optarg))
				return((int)MANDOCLEVEL_BADARG);
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
	if (curp.pmdoc)
		mdoc_free(curp.pmdoc);
	if (curp.pman)
		man_free(curp.pman);
	if (curp.roff)
		roff_free(curp.roff);

	return((int)exit_status);
}


static void
version(void)
{

	(void)printf("%s %s\n", progname, VERSION);
	exit((int)MANDOCLEVEL_OK);
}


static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s "
			"[-V] "
			"[-foption] "
			"[-mformat] "
			"[-Ooption] "
			"[-Toutput] "
			"[-Werr] "
			"[file...]\n", 
			progname);

	exit((int)MANDOCLEVEL_BADARG);
}

static void
ffile(const char *file, struct curparse *curp)
{

	/*
	 * Called once per input file.  Get the file ready for reading,
	 * pass it through to the parser-driver, then close it out.
	 * XXX: don't do anything special as this is only called for
	 * files; stdin goes directly to fdesc().
	 */

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

static int
pfile(const char *file, struct curparse *curp)
{
	const char	*savefile;
	int		 fd, savefd;

	if (-1 == (fd = open(file, O_RDONLY, 0))) {
		perror(file);
		file_status = MANDOCLEVEL_SYSERR;
		return(0);
	}

	savefile = curp->file;
	savefd = curp->fd;

	curp->file = file;
	curp->fd = fd;

	pdesc(curp);

	curp->file = savefile;
	curp->fd = savefd;

	if (-1 == close(fd))
		perror(file);

	return(MANDOCLEVEL_FATAL > file_status ? 1 : 0);
}


static void
resize_buf(struct buf *buf, size_t initial)
{

	buf->sz = buf->sz > initial/2 ? 2 * buf->sz : initial;
	buf->buf = realloc(buf->buf, buf->sz);
	if (NULL == buf->buf) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
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

	/*
	 * Called once per file with an opened file descriptor.  All
	 * pre-file-parse operations (whether stdin or a file) should go
	 * here.
	 *
	 * This calls down into the nested parser, which drills down and
	 * fully parses a file and all its dependences (i.e., `so').  It
	 * then runs the cleanup validators and pushes to output.
	 */

	/* Zero the parse type. */

	curp->mdoc = NULL;
	curp->man = NULL;
	file_status = MANDOCLEVEL_OK;

	/* Make sure the mandotory roff parser is initialised. */

	if (NULL == curp->roff) {
		curp->roff = roff_alloc(&curp->regs, curp, mmsg);
		assert(curp->roff);
	}

	/* Fully parse the file. */

	pdesc(curp);

	if (MANDOCLEVEL_FATAL <= file_status)
		goto cleanup;

	/* NOTE a parser may not have been assigned, yet. */

	if ( ! (curp->man || curp->mdoc)) {
		fprintf(stderr, "%s: Not a manual\n", curp->file);
		file_status = MANDOCLEVEL_FATAL;
		goto cleanup;
	}

	/* Clean up the parse routine ASTs. */

	if (curp->mdoc && ! mdoc_endparse(curp->mdoc)) {
		assert(MANDOCLEVEL_FATAL <= file_status);
		goto cleanup;
	}

	if (curp->man && ! man_endparse(curp->man)) {
		assert(MANDOCLEVEL_FATAL <= file_status);
		goto cleanup;
	}

	assert(curp->roff);
	roff_endparse(curp->roff);

	/*
	 * With -Wstop and warnings or errors of at least
	 * the requested level, do not produce output.
	 */

	if (MANDOCLEVEL_OK != file_status && curp->wstop)
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

	if (curp->man && curp->outman)
		(*curp->outman)(curp->outdata, curp->man);
	if (curp->mdoc && curp->outmdoc)
		(*curp->outmdoc)(curp->outdata, curp->mdoc);

 cleanup:

	memset(&curp->regs, 0, sizeof(struct regset));

	/* Reset the current-parse compilers. */

	if (curp->mdoc)
		mdoc_reset(curp->mdoc);
	if (curp->man)
		man_reset(curp->man);

	assert(curp->roff);
	roff_reset(curp->roff);

	if (exit_status < file_status)
		exit_status = file_status;

	return;
}

static void
pdesc(struct curparse *curp)
{
	struct buf	 blk;
	int		 with_mmap;

	/*
	 * Run for each opened file; may be called more than once for
	 * each full parse sequence if the opened file is nested (i.e.,
	 * from `so').  Simply sucks in the whole file and moves into
	 * the parse phase for the file.
	 */

	if ( ! read_whole_file(curp, &blk, &with_mmap)) {
		file_status = MANDOCLEVEL_SYSERR;
		return;
	}

	/* Line number is per-file. */

	curp->line = 1;

	parsebuf(curp, blk, 1);

	if (with_mmap)
		munmap(blk.buf, blk.sz);
	else
		free(blk.buf);
}

static void
parsebuf(struct curparse *curp, struct buf blk, int start)
{
	struct buf	 ln;
	enum rofferr	 rr;
	int		 i, of, rc;
	int		 pos; /* byte number in the ln buffer */
	int		 lnn; /* line number in the real file */
	unsigned char	 c;

	/*
	 * Main parse routine for an opened file.  This is called for
	 * each opened file and simply loops around the full input file,
	 * possibly nesting (i.e., with `so').
	 */

	memset(&ln, 0, sizeof(struct buf));

	lnn = curp->line; 
	pos = 0; 

	for (i = 0; i < (int)blk.sz; ) {
		if (0 == pos && '\0' == blk.buf[i])
			break;

		if (start) {
			curp->line = lnn;
			curp->reparse_count = 0;
		}

		while (i < (int)blk.sz && (start || '\0' != blk.buf[i])) {
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

			if ( ! (isascii(c) && 
					(isgraph(c) || isblank(c)))) {
				mmsg(MANDOCERR_BADCHAR, curp, 
				    curp->line, pos, "ignoring byte");
				i++;
				continue;
			}

			/* Trailing backslash = a plain char. */

			if ('\\' != blk.buf[i] || i + 1 == (int)blk.sz) {
				if (pos >= (int)ln.sz)
					resize_buf(&ln, 256);
				ln.buf[pos++] = blk.buf[i++];
				continue;
			}

			/* Found escape & at least one other char. */

			if ('\n' == blk.buf[i + 1]) {
				i += 2;
				/* Escaped newlines are skipped over */
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

			/* Some other escape sequence, copy & cont. */

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

rerun:
		rr = roff_parseln
			(curp->roff, curp->line, 
			 &ln.buf, &ln.sz, of, &of);

		switch (rr) {
		case (ROFF_REPARSE):
			if (REPARSE_LIMIT >= ++curp->reparse_count)
				parsebuf(curp, ln, 0);
			else
				mmsg(MANDOCERR_ROFFLOOP, curp, 
				    curp->line, pos, NULL);
			pos = 0;
			continue;
		case (ROFF_APPEND):
			pos = strlen(ln.buf);
			continue;
		case (ROFF_RERUN):
			goto rerun;
		case (ROFF_IGN):
			pos = 0;
			continue;
		case (ROFF_ERR):
			assert(MANDOCLEVEL_FATAL <= file_status);
			break;
		case (ROFF_SO):
			if (pfile(ln.buf + of, curp)) {
				pos = 0;
				continue;
			} else
				break;
		default:
			break;
		}

		/*
		 * If input parsers have not been allocated, do so now.
		 * We keep these instanced betwen parsers, but set them
		 * locally per parse routine since we can use different
		 * parsers with each one.
		 */

		if ( ! (curp->man || curp->mdoc))
			pset(ln.buf + of, pos - of, curp);

		/* 
		 * Lastly, push down into the parsers themselves.  One
		 * of these will have already been set in the pset()
		 * routine.
		 * If libroff returns ROFF_TBL, then add it to the
		 * currently open parse.  Since we only get here if
		 * there does exist data (see tbl_data.c), we're
		 * guaranteed that something's been allocated.
		 */

		if (ROFF_TBL == rr) {
			assert(curp->man || curp->mdoc);
			if (curp->man)
				man_addspan(curp->man, roff_span(curp->roff));
			else
				mdoc_addspan(curp->mdoc, roff_span(curp->roff));

		} else if (curp->man || curp->mdoc) {
			rc = curp->man ?
				man_parseln(curp->man, 
					curp->line, ln.buf, of) :
				mdoc_parseln(curp->mdoc, 
					curp->line, ln.buf, of);

			if ( ! rc) {
				assert(MANDOCLEVEL_FATAL <= file_status);
				break;
			}
		}

		/* Temporary buffers typically are not full. */

		if (0 == start && '\0' == blk.buf[i])
			break;

		/* Start the next input line. */

		pos = 0;
	}

	free(ln.buf);
}

static void
pset(const char *buf, int pos, struct curparse *curp)
{
	int		 i;

	/*
	 * Try to intuit which kind of manual parser should be used.  If
	 * passed in by command-line (-man, -mdoc), then use that
	 * explicitly.  If passed as -mandoc, then try to guess from the
	 * line: either skip dot-lines, use -mdoc when finding `.Dt', or
	 * default to -man, which is more lenient.
	 *
	 * Separate out pmdoc/pman from mdoc/man: the first persists
	 * through all parsers, while the latter is used per-parse.
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
		if (NULL == curp->pmdoc) 
			curp->pmdoc = mdoc_alloc
				(&curp->regs, curp, mmsg);
		assert(curp->pmdoc);
		curp->mdoc = curp->pmdoc;
		return;
	case (INTT_MAN):
		if (NULL == curp->pman) 
			curp->pman = man_alloc
				(&curp->regs, curp, mmsg);
		assert(curp->pman);
		curp->man = curp->pman;
		return;
	default:
		break;
	}

	if (pos >= 3 && 0 == memcmp(buf, ".Dd", 3))  {
		if (NULL == curp->pmdoc) 
			curp->pmdoc = mdoc_alloc
				(&curp->regs, curp, mmsg);
		assert(curp->pmdoc);
		curp->mdoc = curp->pmdoc;
		return;
	} 

	if (NULL == curp->pman) 
		curp->pman = man_alloc(&curp->regs, curp, mmsg);
	assert(curp->pman);
	curp->man = curp->pman;
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
		/* LINTED */
		level--;

	cp = (struct curparse *)arg;
	if (level < cp->wlevel)
		return(1);

	fprintf(stderr, "%s:%d:%d: %s: %s",
	    cp->file, ln, col + 1, mandoclevels[level], mandocerrs[t]);
	if (msg)
		fprintf(stderr, ": %s", msg);
	fputc('\n', stderr);

	if (file_status < level)
		file_status = level;
	
	return(level < MANDOCLEVEL_FATAL);
}
