/*	$OpenBSD: read.c,v 1.171 2018/08/24 22:56:37 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2018 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010, 2012 Joerg Sonnenberger <joerg@netbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
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
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "man.h"
#include "libmandoc.h"

#define	REPARSE_LIMIT	1000

struct	mparse {
	struct roff	 *roff; /* roff parser (!NULL) */
	struct roff_man	 *man; /* man parser */
	char		 *sodest; /* filename pointed to by .so */
	const char	 *file; /* filename of current input file */
	struct buf	 *primary; /* buffer currently being parsed */
	struct buf	 *secondary; /* copy of top level input */
	struct buf	 *loop; /* open .while request line */
	const char	 *os_s; /* default operating system */
	mandocmsg	  mmsg; /* warning/error message handler */
	enum mandoclevel  file_status; /* status of current parse */
	enum mandocerr	  mmin; /* ignore messages below this */
	int		  options; /* parser options */
	int		  gzip; /* current input file is gzipped */
	int		  filenc; /* encoding of the current file */
	int		  reparse_count; /* finite interp. stack */
	int		  line; /* line number in the file */
};

static	void	  choose_parser(struct mparse *);
static	void	  free_buf_list(struct buf *);
static	void	  resize_buf(struct buf *, size_t);
static	int	  mparse_buf_r(struct mparse *, struct buf, size_t, int);
static	int	  read_whole_file(struct mparse *, const char *, int,
				struct buf *, int *);
static	void	  mparse_end(struct mparse *);
static	void	  mparse_parse_buffer(struct mparse *, struct buf,
			const char *);

static	const enum mandocerr	mandoclimits[MANDOCLEVEL_MAX] = {
	MANDOCERR_OK,
	MANDOCERR_OK,
	MANDOCERR_WARNING,
	MANDOCERR_ERROR,
	MANDOCERR_UNSUPP,
	MANDOCERR_MAX,
	MANDOCERR_MAX
};

static	const char * const	mandocerrs[MANDOCERR_MAX] = {
	"ok",

	"base system convention",

	"Mdocdate found",
	"Mdocdate missing",
	"unknown architecture",
	"operating system explicitly specified",
	"RCS id missing",
	"referenced manual not found",

	"generic style suggestion",

	"legacy man(7) date format",
	"normalizing date format to",
	"lower case character in document title",
	"duplicate RCS id",
	"possible typo in section name",
	"unterminated quoted argument",
	"useless macro",
	"consider using OS macro",
	"errnos out of order",
	"duplicate errno",
	"trailing delimiter",
	"no blank before trailing delimiter",
	"fill mode already enabled, skipping",
	"fill mode already disabled, skipping",
	"verbatim \"--\", maybe consider using \\(em",
	"function name without markup",
	"whitespace at end of input line",
	"bad comment style",

	"generic warning",

	/* related to the prologue */
	"missing manual title, using UNTITLED",
	"missing manual title, using \"\"",
	"missing manual section, using \"\"",
	"unknown manual section",
	"missing date, using today's date",
	"cannot parse date, using it verbatim",
	"date in the future, using it anyway",
	"missing Os macro, using \"\"",
	"late prologue macro",
	"prologue macros out of order",

	/* related to document structure */
	".so is fragile, better use ln(1)",
	"no document body",
	"content before first section header",
	"first section is not \"NAME\"",
	"NAME section without Nm before Nd",
	"NAME section without description",
	"description not at the end of NAME",
	"bad NAME section content",
	"missing comma before name",
	"missing description line, using \"\"",
	"description line outside NAME section",
	"sections out of conventional order",
	"duplicate section title",
	"unexpected section",
	"cross reference to self",
	"unusual Xr order",
	"unusual Xr punctuation",
	"AUTHORS section without An macro",

	/* related to macros and nesting */
	"obsolete macro",
	"macro neither callable nor escaped",
	"skipping paragraph macro",
	"moving paragraph macro out of list",
	"skipping no-space macro",
	"blocks badly nested",
	"nested displays are not portable",
	"moving content out of list",
	"first macro on line",
	"line scope broken",
	"skipping blank line in line scope",

	/* related to missing macro arguments */
	"skipping empty request",
	"conditional request controls empty scope",
	"skipping empty macro",
	"empty block",
	"empty argument, using 0n",
	"missing display type, using -ragged",
	"list type is not the first argument",
	"missing -width in -tag list, using 6n",
	"missing utility name, using \"\"",
	"missing function name, using \"\"",
	"empty head in list item",
	"empty list item",
	"missing argument, using next line",
	"missing font type, using \\fR",
	"unknown font type, using \\fR",
	"nothing follows prefix",
	"empty reference block",
	"missing section argument",
	"missing -std argument, adding it",
	"missing option string, using \"\"",
	"missing resource identifier, using \"\"",
	"missing eqn box, using \"\"",

	/* related to bad macro arguments */
	"duplicate argument",
	"skipping duplicate argument",
	"skipping duplicate display type",
	"skipping duplicate list type",
	"skipping -width argument",
	"wrong number of cells",
	"unknown AT&T UNIX version",
	"comma in function argument",
	"parenthesis in function name",
	"unknown library name",
	"invalid content in Rs block",
	"invalid Boolean argument",
	"unknown font, skipping request",
	"odd number of characters in request",

	/* related to plain text */
	"blank line in fill mode, using .sp",
	"tab in filled text",
	"new sentence, new line",
	"invalid escape sequence",
	"undefined string, using \"\"",

	/* related to tables */
	"tbl line starts with span",
	"tbl column starts with span",
	"skipping vertical bar in tbl layout",

	"generic error",

	/* related to tables */
	"non-alphabetic character in tbl options",
	"skipping unknown tbl option",
	"missing tbl option argument",
	"wrong tbl option argument size",
	"empty tbl layout",
	"invalid character in tbl layout",
	"unmatched parenthesis in tbl layout",
	"tbl without any data cells",
	"ignoring data in spanned tbl cell",
	"ignoring extra tbl data cells",
	"data block open at end of tbl",

	/* related to document structure and macros */
	NULL,
	"duplicate prologue macro",
	"skipping late title macro",
	"input stack limit exceeded, infinite loop?",
	"skipping bad character",
	"skipping unknown macro",
	"ignoring request outside macro",
	"skipping insecure request",
	"skipping item outside list",
	"skipping column outside column list",
	"skipping end of block that is not open",
	"fewer RS blocks open, skipping",
	"inserting missing end of block",
	"appending missing end of block",

	/* related to request and macro arguments */
	"escaped character not allowed in a name",
	"using macro argument outside macro",
	"argument number is not numeric",
	"NOT IMPLEMENTED: Bd -file",
	"skipping display without arguments",
	"missing list type, using -item",
	"argument is not numeric, using 1",
	"missing manual name, using \"\"",
	"uname(3) system call failed, using UNKNOWN",
	"unknown standard specifier",
	"skipping request without numeric argument",
	"excessive shift",
	"NOT IMPLEMENTED: .so with absolute path or \"..\"",
	".so request failed",
	"skipping all arguments",
	"skipping excess arguments",
	"divide by zero",

	"unsupported feature",
	"input too large",
	"unsupported control character",
	"unsupported roff request",
	"nested .while loops",
	"end of scope with open .while loop",
	"end of .while loop in inner scope",
	"cannot continue this .while loop",
	"eqn delim option in tbl",
	"unsupported tbl layout modifier",
	"ignoring macro in table",
};

static	const char * const	mandoclevels[MANDOCLEVEL_MAX] = {
	"SUCCESS",
	"STYLE",
	"WARNING",
	"ERROR",
	"UNSUPP",
	"BADARG",
	"SYSERR"
};


static void
resize_buf(struct buf *buf, size_t initial)
{

	buf->sz = buf->sz > initial/2 ? 2 * buf->sz : initial;
	buf->buf = mandoc_realloc(buf->buf, buf->sz);
}

static void
free_buf_list(struct buf *buf)
{
	struct buf *tmp;

	while (buf != NULL) {
		tmp = buf;
		buf = tmp->next;
		free(tmp->buf);
		free(tmp);
	}
}

static void
choose_parser(struct mparse *curp)
{
	char		*cp, *ep;
	int		 format;

	/*
	 * If neither command line arguments -mdoc or -man select
	 * a parser nor the roff parser found a .Dd or .TH macro
	 * yet, look ahead in the main input buffer.
	 */

	if ((format = roff_getformat(curp->roff)) == 0) {
		cp = curp->primary->buf;
		ep = cp + curp->primary->sz;
		while (cp < ep) {
			if (*cp == '.' || *cp == '\'') {
				cp++;
				if (cp[0] == 'D' && cp[1] == 'd') {
					format = MPARSE_MDOC;
					break;
				}
				if (cp[0] == 'T' && cp[1] == 'H') {
					format = MPARSE_MAN;
					break;
				}
			}
			cp = memchr(cp, '\n', ep - cp);
			if (cp == NULL)
				break;
			cp++;
		}
	}

	if (format == MPARSE_MDOC) {
		curp->man->macroset = MACROSET_MDOC;
		if (curp->man->mdocmac == NULL)
			curp->man->mdocmac = roffhash_alloc(MDOC_Dd, MDOC_MAX);
	} else {
		curp->man->macroset = MACROSET_MAN;
		if (curp->man->manmac == NULL)
			curp->man->manmac = roffhash_alloc(MAN_TH, MAN_MAX);
	}
	curp->man->first->tok = TOKEN_NONE;
}

/*
 * Main parse routine for a buffer.
 * It assumes encoding and line numbering are already set up.
 * It can recurse directly (for invocations of user-defined
 * macros, inline equations, and input line traps)
 * and indirectly (for .so file inclusion).
 */
static int
mparse_buf_r(struct mparse *curp, struct buf blk, size_t i, int start)
{
	struct buf	 ln;
	struct buf	*firstln, *lastln, *thisln, *loop;
	const char	*save_file;
	char		*cp;
	size_t		 pos; /* byte number in the ln buffer */
	int		 line_result, result;
	int		 of;
	int		 lnn; /* line number in the real file */
	int		 fd;
	int		 inloop; /* Saw .while on this level. */
	unsigned char	 c;

	ln.sz = 256;
	ln.buf = mandoc_malloc(ln.sz);
	ln.next = NULL;
	firstln = loop = NULL;
	lnn = curp->line;
	pos = 0;
	inloop = 0;
	result = ROFF_CONT;

	while (i < blk.sz && (blk.buf[i] != '\0' || pos != 0)) {
		if (start) {
			curp->line = lnn;
			curp->reparse_count = 0;

			if (lnn < 3 &&
			    curp->filenc & MPARSE_UTF8 &&
			    curp->filenc & MPARSE_LATIN1)
				curp->filenc = preconv_cue(&blk, i);
		}

		while (i < blk.sz && (start || blk.buf[i] != '\0')) {

			/*
			 * When finding an unescaped newline character,
			 * leave the character loop to process the line.
			 * Skip a preceding carriage return, if any.
			 */

			if ('\r' == blk.buf[i] && i + 1 < blk.sz &&
			    '\n' == blk.buf[i + 1])
				++i;
			if ('\n' == blk.buf[i]) {
				++i;
				++lnn;
				break;
			}

			/*
			 * Make sure we have space for the worst
			 * case of 12 bytes: "\\[u10ffff]\n\0"
			 */

			if (pos + 12 > ln.sz)
				resize_buf(&ln, 256);

			/*
			 * Encode 8-bit input.
			 */

			c = blk.buf[i];
			if (c & 0x80) {
				if ( ! (curp->filenc && preconv_encode(
				    &blk, &i, &ln, &pos, &curp->filenc))) {
					mandoc_vmsg(MANDOCERR_CHAR_BAD, curp,
					    curp->line, pos, "0x%x", c);
					ln.buf[pos++] = '?';
					i++;
				}
				continue;
			}

			/*
			 * Exclude control characters.
			 */

			if (c == 0x7f || (c < 0x20 && c != 0x09)) {
				mandoc_vmsg(c == 0x00 || c == 0x04 ||
				    c > 0x0a ? MANDOCERR_CHAR_BAD :
				    MANDOCERR_CHAR_UNSUPP,
				    curp, curp->line, pos, "0x%x", c);
				i++;
				if (c != '\r')
					ln.buf[pos++] = '?';
				continue;
			}

			ln.buf[pos++] = blk.buf[i++];
		}
		ln.buf[pos] = '\0';

		/*
		 * Maintain a lookaside buffer of all lines.
		 * parsed from this input source.
		 */

		thisln = mandoc_malloc(sizeof(*thisln));
		thisln->buf = mandoc_strdup(ln.buf);
		thisln->sz = strlen(ln.buf) + 1;
		thisln->next = NULL;
		if (firstln == NULL) {
			firstln = lastln = thisln;
			if (curp->secondary == NULL)
				curp->secondary = firstln;
		} else {
			lastln->next = thisln;
			lastln = thisln;
		}

		/* XXX Ugly hack to mark the end of the input. */

		if (i == blk.sz || blk.buf[i] == '\0') {
			ln.buf[pos++] = '\n';
			ln.buf[pos] = '\0';
		}

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
		line_result = roff_parseln(curp->roff, curp->line, &ln, &of);

		/* Process options. */

		if (line_result & ROFF_APPEND)
			assert(line_result == (ROFF_IGN | ROFF_APPEND));

		if (line_result & ROFF_USERCALL)
			assert((line_result & ROFF_MASK) == ROFF_REPARSE);

		if (line_result & ROFF_USERRET) {
			assert(line_result == (ROFF_IGN | ROFF_USERRET));
			if (start == 0) {
				/* Return from the current macro. */
				result = ROFF_USERRET;
				goto out;
			}
		}

		switch (line_result & ROFF_LOOPMASK) {
		case ROFF_IGN:
			break;
		case ROFF_WHILE:
			if (curp->loop != NULL) {
				if (loop == curp->loop)
					break;
				mandoc_msg(MANDOCERR_WHILE_NEST,
				    curp, curp->line, pos, NULL);
			}
			curp->loop = thisln;
			loop = NULL;
			inloop = 1;
			break;
		case ROFF_LOOPCONT:
		case ROFF_LOOPEXIT:
			if (curp->loop == NULL) {
				mandoc_msg(MANDOCERR_WHILE_FAIL,
				    curp, curp->line, pos, NULL);
				break;
			}
			if (inloop == 0) {
				mandoc_msg(MANDOCERR_WHILE_INTO,
				    curp, curp->line, pos, NULL);
				curp->loop = loop = NULL;
				break;
			}
			if (line_result & ROFF_LOOPCONT)
				loop = curp->loop;
			else {
				curp->loop = loop = NULL;
				inloop = 0;
			}
			break;
		default:
			abort();
		}

		/* Process the main instruction from the roff parser. */

		switch (line_result & ROFF_MASK) {
		case ROFF_IGN:
			break;
		case ROFF_CONT:
			if (curp->man->macroset == MACROSET_NONE)
				choose_parser(curp);
			if ((curp->man->macroset == MACROSET_MDOC ?
			     mdoc_parseln(curp->man, curp->line, ln.buf, of) :
			     man_parseln(curp->man, curp->line, ln.buf, of)
			    ) == 2)
				goto out;
			break;
		case ROFF_RERUN:
			goto rerun;
		case ROFF_REPARSE:
			if (++curp->reparse_count > REPARSE_LIMIT) {
				/* Abort and return to the top level. */
				result = ROFF_IGN;
				mandoc_msg(MANDOCERR_ROFFLOOP, curp,
				    curp->line, pos, NULL);
				goto out;
			}
			result = mparse_buf_r(curp, ln, of, 0);
			if (line_result & ROFF_USERCALL) {
				roff_userret(curp->roff);
				/* Continue normally. */
				if (result & ROFF_USERRET)
					result = ROFF_CONT;
			}
			if (start == 0 && result != ROFF_CONT)
				goto out;
			break;
		case ROFF_SO:
			if ( ! (curp->options & MPARSE_SO) &&
			    (i >= blk.sz || blk.buf[i] == '\0')) {
				curp->sodest = mandoc_strdup(ln.buf + of);
				goto out;
			}
			save_file = curp->file;
			if ((fd = mparse_open(curp, ln.buf + of)) != -1) {
				mparse_readfd(curp, fd, ln.buf + of);
				close(fd);
				curp->file = save_file;
			} else {
				curp->file = save_file;
				mandoc_vmsg(MANDOCERR_SO_FAIL,
				    curp, curp->line, pos,
				    ".so %s", ln.buf + of);
				ln.sz = mandoc_asprintf(&cp,
				    ".sp\nSee the file %s.\n.sp",
				    ln.buf + of);
				free(ln.buf);
				ln.buf = cp;
				of = 0;
				mparse_buf_r(curp, ln, of, 0);
			}
			break;
		default:
			abort();
		}

		/* Start the next input line. */

		if (loop != NULL &&
		    (line_result & ROFF_LOOPMASK) == ROFF_IGN)
			loop = loop->next;

		if (loop != NULL) {
			if ((line_result & ROFF_APPEND) == 0)
				*ln.buf = '\0';
			if (ln.sz < loop->sz)
				resize_buf(&ln, loop->sz);
			(void)strlcat(ln.buf, loop->buf, ln.sz);
			of = 0;
			goto rerun;
		}

		pos = (line_result & ROFF_APPEND) ? strlen(ln.buf) : 0;
	}
out:
	if (inloop) {
		if (result != ROFF_USERRET)
			mandoc_msg(MANDOCERR_WHILE_OUTOF, curp,
			    curp->line, pos, NULL);
		curp->loop = NULL;
	}
	free(ln.buf);
	if (firstln != curp->secondary)
		free_buf_list(firstln);
	return result;
}

static int
read_whole_file(struct mparse *curp, const char *file, int fd,
		struct buf *fb, int *with_mmap)
{
	struct stat	 st;
	gzFile		 gz;
	size_t		 off;
	ssize_t		 ssz;
	int		 gzerrnum, retval;

	if (fstat(fd, &st) == -1) {
		mandoc_vmsg(MANDOCERR_FILE, curp, 0, 0,
		    "fstat: %s", strerror(errno));
		return 0;
	}

	/*
	 * If we're a regular file, try just reading in the whole entry
	 * via mmap().  This is faster than reading it into blocks, and
	 * since each file is only a few bytes to begin with, I'm not
	 * concerned that this is going to tank any machines.
	 */

	if (curp->gzip == 0 && S_ISREG(st.st_mode)) {
		if (st.st_size > 0x7fffffff) {
			mandoc_msg(MANDOCERR_TOOLARGE, curp, 0, 0, NULL);
			return 0;
		}
		*with_mmap = 1;
		fb->sz = (size_t)st.st_size;
		fb->buf = mmap(NULL, fb->sz, PROT_READ, MAP_SHARED, fd, 0);
		if (fb->buf != MAP_FAILED)
			return 1;
	}

	if (curp->gzip) {
		/*
		 * Duplicating the file descriptor is required
		 * because we will have to call gzclose(3)
		 * to free memory used internally by zlib,
		 * but that will also close the file descriptor,
		 * which this function must not do.
		 */
		if ((fd = dup(fd)) == -1) {
			mandoc_vmsg(MANDOCERR_FILE, curp, 0, 0,
			    "dup: %s", strerror(errno));
			return 0;
		}
		if ((gz = gzdopen(fd, "rb")) == NULL) {
			mandoc_vmsg(MANDOCERR_FILE, curp, 0, 0,
			    "gzdopen: %s", strerror(errno));
			close(fd);
			return 0;
		}
	} else
		gz = NULL;

	/*
	 * If this isn't a regular file (like, say, stdin), then we must
	 * go the old way and just read things in bit by bit.
	 */

	*with_mmap = 0;
	off = 0;
	retval = 0;
	fb->sz = 0;
	fb->buf = NULL;
	for (;;) {
		if (off == fb->sz) {
			if (fb->sz == (1U << 31)) {
				mandoc_msg(MANDOCERR_TOOLARGE, curp,
				    0, 0, NULL);
				break;
			}
			resize_buf(fb, 65536);
		}
		ssz = curp->gzip ?
		    gzread(gz, fb->buf + (int)off, fb->sz - off) :
		    read(fd, fb->buf + (int)off, fb->sz - off);
		if (ssz == 0) {
			fb->sz = off;
			retval = 1;
			break;
		}
		if (ssz == -1) {
			if (curp->gzip)
				(void)gzerror(gz, &gzerrnum);
			mandoc_vmsg(MANDOCERR_FILE, curp, 0, 0, "read: %s",
			    curp->gzip && gzerrnum != Z_ERRNO ?
			    zError(gzerrnum) : strerror(errno));
			break;
		}
		off += (size_t)ssz;
	}

	if (curp->gzip && (gzerrnum = gzclose(gz)) != Z_OK)
		mandoc_vmsg(MANDOCERR_FILE, curp, 0, 0, "gzclose: %s",
		    gzerrnum == Z_ERRNO ? strerror(errno) :
		    zError(gzerrnum));
	if (retval == 0) {
		free(fb->buf);
		fb->buf = NULL;
	}
	return retval;
}

static void
mparse_end(struct mparse *curp)
{
	if (curp->man->macroset == MACROSET_NONE)
		curp->man->macroset = MACROSET_MAN;
	if (curp->man->macroset == MACROSET_MDOC)
		mdoc_endparse(curp->man);
	else
		man_endparse(curp->man);
	roff_endparse(curp->roff);
}

static void
mparse_parse_buffer(struct mparse *curp, struct buf blk, const char *file)
{
	struct buf	*svprimary;
	const char	*svfile;
	size_t		 offset;
	static int	 recursion_depth;

	if (64 < recursion_depth) {
		mandoc_msg(MANDOCERR_ROFFLOOP, curp, curp->line, 0, NULL);
		return;
	}

	/* Line number is per-file. */
	svfile = curp->file;
	curp->file = file;
	svprimary = curp->primary;
	curp->primary = &blk;
	curp->line = 1;
	recursion_depth++;

	/* Skip an UTF-8 byte order mark. */
	if (curp->filenc & MPARSE_UTF8 && blk.sz > 2 &&
	    (unsigned char)blk.buf[0] == 0xef &&
	    (unsigned char)blk.buf[1] == 0xbb &&
	    (unsigned char)blk.buf[2] == 0xbf) {
		offset = 3;
		curp->filenc &= ~MPARSE_LATIN1;
	} else
		offset = 0;

	mparse_buf_r(curp, blk, offset, 1);

	if (--recursion_depth == 0)
		mparse_end(curp);

	curp->primary = svprimary;
	curp->file = svfile;
}

/*
 * Read the whole file into memory and call the parsers.
 * Called recursively when an .so request is encountered.
 */
enum mandoclevel
mparse_readfd(struct mparse *curp, int fd, const char *file)
{
	struct buf	 blk;
	int		 with_mmap;
	int		 save_filenc;

	if (read_whole_file(curp, file, fd, &blk, &with_mmap)) {
		save_filenc = curp->filenc;
		curp->filenc = curp->options &
		    (MPARSE_UTF8 | MPARSE_LATIN1);
		mparse_parse_buffer(curp, blk, file);
		curp->filenc = save_filenc;
		if (with_mmap)
			munmap(blk.buf, blk.sz);
		else
			free(blk.buf);
	}
	return curp->file_status;
}

int
mparse_open(struct mparse *curp, const char *file)
{
	char		 *cp;
	int		  fd;

	curp->file = file;
	cp = strrchr(file, '.');
	curp->gzip = (cp != NULL && ! strcmp(cp + 1, "gz"));

	/* First try to use the filename as it is. */

	if ((fd = open(file, O_RDONLY)) != -1)
		return fd;

	/*
	 * If that doesn't work and the filename doesn't
	 * already  end in .gz, try appending .gz.
	 */

	if ( ! curp->gzip) {
		mandoc_asprintf(&cp, "%s.gz", file);
		fd = open(cp, O_RDONLY);
		free(cp);
		if (fd != -1) {
			curp->gzip = 1;
			return fd;
		}
	}

	/* Neither worked, give up. */

	mandoc_msg(MANDOCERR_FILE, curp, 0, 0, strerror(errno));
	return -1;
}

struct mparse *
mparse_alloc(int options, enum mandocerr mmin, mandocmsg mmsg,
    enum mandoc_os os_e, const char *os_s)
{
	struct mparse	*curp;

	curp = mandoc_calloc(1, sizeof(struct mparse));

	curp->options = options;
	curp->mmin = mmin;
	curp->mmsg = mmsg;
	curp->os_s = os_s;

	curp->roff = roff_alloc(curp, options);
	curp->man = roff_man_alloc(curp->roff, curp, curp->os_s,
		curp->options & MPARSE_QUICK ? 1 : 0);
	if (curp->options & MPARSE_MDOC) {
		curp->man->macroset = MACROSET_MDOC;
		if (curp->man->mdocmac == NULL)
			curp->man->mdocmac = roffhash_alloc(MDOC_Dd, MDOC_MAX);
	} else if (curp->options & MPARSE_MAN) {
		curp->man->macroset = MACROSET_MAN;
		if (curp->man->manmac == NULL)
			curp->man->manmac = roffhash_alloc(MAN_TH, MAN_MAX);
	}
	curp->man->first->tok = TOKEN_NONE;
	curp->man->meta.os_e = os_e;
	return curp;
}

void
mparse_reset(struct mparse *curp)
{
	roff_reset(curp->roff);
	roff_man_reset(curp->man);
	free_buf_list(curp->secondary);
	curp->secondary = NULL;

	free(curp->sodest);
	curp->sodest = NULL;

	curp->file_status = MANDOCLEVEL_OK;
	curp->gzip = 0;
}

void
mparse_free(struct mparse *curp)
{
	roffhash_free(curp->man->mdocmac);
	roffhash_free(curp->man->manmac);
	roff_man_free(curp->man);
	roff_free(curp->roff);
	free_buf_list(curp->secondary);
	free(curp->sodest);
	free(curp);
}

void
mparse_result(struct mparse *curp, struct roff_man **man,
	char **sodest)
{

	if (sodest && NULL != (*sodest = curp->sodest)) {
		*man = NULL;
		return;
	}
	if (man)
		*man = curp->man;
}

void
mparse_updaterc(struct mparse *curp, enum mandoclevel *rc)
{
	if (curp->file_status > *rc)
		*rc = curp->file_status;
}

void
mandoc_vmsg(enum mandocerr t, struct mparse *m,
		int ln, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	mandoc_msg(t, m, ln, pos, buf);
}

void
mandoc_msg(enum mandocerr er, struct mparse *m,
		int ln, int col, const char *msg)
{
	enum mandoclevel level;

	if (er < m->mmin && er != MANDOCERR_FILE)
		return;

	level = MANDOCLEVEL_UNSUPP;
	while (er < mandoclimits[level])
		level--;

	if (m->mmsg)
		(*m->mmsg)(er, level, m->file, ln, col, msg);

	if (m->file_status < level)
		m->file_status = level;
}

const char *
mparse_strerror(enum mandocerr er)
{

	return mandocerrs[er];
}

const char *
mparse_strlevel(enum mandoclevel lvl)
{
	return mandoclevels[lvl];
}

void
mparse_copy(const struct mparse *p)
{
	struct buf	*buf;

	for (buf = p->secondary; buf != NULL; buf = buf->next)
		puts(buf->buf);
}
