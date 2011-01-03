/*	$Id: mandoc.h,v 1.26 2011/01/03 23:39:27 schwarze Exp $ */
/*
 * Copyright (c) 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef MANDOC_H
#define MANDOC_H

#define ASCII_NBRSP	 31  /* non-breaking space */
#define	ASCII_HYPH	 30  /* breakable hyphen */

/*
 * Status level.  This refers to both internal status (i.e., whilst
 * running, when warnings/errors are reported) and an indicator of a
 * threshold of when to halt (when said internal state exceeds the
 * threshold).
 */
enum	mandoclevel {
	MANDOCLEVEL_OK = 0,
	MANDOCLEVEL_RESERVED,
	MANDOCLEVEL_WARNING, /* warnings: syntax, whitespace, etc. */
	MANDOCLEVEL_ERROR, /* input has been thrown away */
	MANDOCLEVEL_FATAL, /* input is borked */
	MANDOCLEVEL_BADARG, /* bad argument in invocation */
	MANDOCLEVEL_SYSERR, /* system error */
	MANDOCLEVEL_MAX
};

/*
 * All possible things that can go wrong within a parse, be it libroff,
 * libmdoc, or libman.
 */
enum	mandocerr {
	MANDOCERR_OK,

	MANDOCERR_WARNING, /* ===== start of warnings ===== */

	/* related to the prologue */
	MANDOCERR_NOTITLE, /* no title in document */
	MANDOCERR_UPPERCASE, /* document title should be all caps */
	MANDOCERR_BADMSEC, /* unknown manual section */
	MANDOCERR_BADDATE, /* cannot parse date argument */
	MANDOCERR_PROLOGOOO, /* prologue macros out of order */
	MANDOCERR_PROLOGREP, /* duplicate prologue macro */
	MANDOCERR_BADPROLOG, /* macro not allowed in prologue */
	MANDOCERR_BADBODY, /* macro not allowed in body */

	/* related to document structure */
	MANDOCERR_SO, /* .so is fragile, better use ln(1) */
	MANDOCERR_NAMESECFIRST, /* NAME section must come first */
	MANDOCERR_BADNAMESEC, /* bad NAME section contents */
	MANDOCERR_NONAME, /* manual name not yet set */
	MANDOCERR_SECOOO, /* sections out of conventional order */
	MANDOCERR_SECREP, /* duplicate section name */
	MANDOCERR_SECMSEC, /* section not in conventional manual section */

	/* related to macros and nesting */
	MANDOCERR_MACROOBS, /* skipping obsolete macro */
	MANDOCERR_IGNPAR, /* skipping paragraph macro */
	MANDOCERR_SCOPENEST, /* blocks badly nested */
	MANDOCERR_CHILD, /* child violates parent syntax */
	MANDOCERR_NESTEDDISP, /* nested displays are not portable */
	MANDOCERR_SCOPEREP, /* already in literal mode */

	/* related to missing macro arguments */
	MANDOCERR_MACROEMPTY, /* skipping empty macro */
	MANDOCERR_ARGCWARN, /* argument count wrong */
	MANDOCERR_DISPTYPE, /* missing display type */
	MANDOCERR_LISTFIRST, /* list type must come first */
	MANDOCERR_NOWIDTHARG, /* tag lists require a width argument */
	MANDOCERR_FONTTYPE, /* missing font type */

	/* related to bad macro arguments */
	MANDOCERR_IGNARGV, /* skipping argument */
	MANDOCERR_ARGVREP, /* duplicate argument */
	MANDOCERR_DISPREP, /* duplicate display type */
	MANDOCERR_LISTREP, /* duplicate list type */
	MANDOCERR_BADATT, /* unknown AT&T UNIX version */
	MANDOCERR_BADBOOL, /* bad Boolean value */
	MANDOCERR_BADFONT, /* unknown font */
	MANDOCERR_BADSTANDARD, /* unknown standard specifier */
	MANDOCERR_BADWIDTH, /* bad width argument */

	/* related to plain text */
	MANDOCERR_NOBLANKLN, /* blank line in non-literal context */
	MANDOCERR_BADTAB, /* tab in non-literal context */
	MANDOCERR_EOLNSPACE, /* end of line whitespace */
	MANDOCERR_BADCOMMENT, /* bad comment style */
	MANDOCERR_BADESCAPE, /* unknown escape sequence */
	MANDOCERR_BADQUOTE, /* unterminated quoted string */

	MANDOCERR_ERROR, /* ===== start of errors ===== */

	MANDOCERR_ROFFLOOP, /* input stack limit exceeded, infinite loop? */
	MANDOCERR_BADCHAR, /* skipping bad character */
	MANDOCERR_NOTEXT, /* skipping text before the first section header */
	MANDOCERR_MACRO, /* skipping unknown macro */
	MANDOCERR_REQUEST, /* NOT IMPLEMENTED: skipping request */
	MANDOCERR_LINESCOPE, /* line scope broken */
	MANDOCERR_ARGCOUNT, /* argument count wrong */
	MANDOCERR_NOSCOPE, /* skipping end of block that is not open */
	MANDOCERR_SCOPEBROKEN, /* missing end of block */
	MANDOCERR_SCOPEEXIT, /* scope open on exit */
	MANDOCERR_UNAME, /* uname(3) system call failed */
	/* FIXME: merge following with MANDOCERR_ARGCOUNT */
	MANDOCERR_NOARGS, /* macro requires line argument(s) */
	MANDOCERR_NOBODY, /* macro requires body argument(s) */
	MANDOCERR_NOARGV, /* macro requires argument(s) */
	MANDOCERR_LISTTYPE, /* missing list type */
	MANDOCERR_ARGSLOST, /* line argument(s) will be lost */
	MANDOCERR_BODYLOST, /* body argument(s) will be lost */
	MANDOCERR_TBL, /* tbl(1) error */

	MANDOCERR_FATAL, /* ===== start of fatal errors ===== */

	MANDOCERR_COLUMNS, /* column syntax is inconsistent */
	MANDOCERR_BADDISP, /* NOT IMPLEMENTED: .Bd -file */
	MANDOCERR_SYNTLINESCOPE, /* line scope broken, syntax violated */
	MANDOCERR_SYNTARGVCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SYNTCHILD, /* child violates parent syntax */
	MANDOCERR_SYNTARGCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SOPATH, /* NOT IMPLEMENTED: .so with absolute path or ".." */
	MANDOCERR_NODOCBODY, /* no document body */
	MANDOCERR_NODOCPROLOG, /* no document prologue */
	MANDOCERR_MEM, /* static buffer exhausted */
	MANDOCERR_MAX
};

/*
 * Available registers (set in libroff, accessed elsewhere).
 */
enum	regs {
	REG_nS = 0,
	REG__MAX
};

/*
 * A register (struct reg) can consist of many types: this consists of
 * normalised types from the original string form.
 */
union	regval {
	unsigned  u; /* unsigned integer */
};

/*
 * A single register entity.  If "set" is zero, the value of the
 * register should be the default one, which is per-register.  It's
 * assumed that callers know which type in "v" corresponds to which
 * register value.
 */
struct	reg {
	int		  set; /* whether set or not */
	union regval	  v; /* parsed data */
};

/*
 * The primary interface to setting register values is in libroff,
 * although libmdoc and libman from time to time will manipulate
 * registers (such as `.Sh SYNOPSIS' enabling REG_nS).
 */
struct	regset {
	struct reg	  regs[REG__MAX];
};

__BEGIN_DECLS

/*
 * Callback function for warnings, errors, and fatal errors as they
 * occur in the compilers libroff, libmdoc, and libman.
 */
typedef	int		(*mandocmsg)(enum mandocerr, void *,
				int, int, const char *);

__END_DECLS

#endif /*!MANDOC_H*/
