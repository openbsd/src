/*	$Id: mandoc.h,v 1.18 2010/10/26 22:28:57 schwarze Exp $ */
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
	MANDOCERR_UPPERCASE, /* text should be uppercase */
	MANDOCERR_SECOOO, /* sections out of conventional order */
	MANDOCERR_SECREP, /* section name repeats */
	MANDOCERR_PROLOGOOO, /* out of order prologue */
	MANDOCERR_PROLOGREP, /* repeated prologue entry */
	MANDOCERR_LISTFIRST, /* list type must come first */
	MANDOCERR_BADSTANDARD, /* bad standard */
	MANDOCERR_BADLIB, /* bad library */
	MANDOCERR_BADTAB, /* tab in non-literal context */
	MANDOCERR_BADESCAPE, /* bad escape sequence */
	MANDOCERR_BADQUOTE, /* unterminated quoted string */
	MANDOCERR_NOWIDTHARG, /* argument requires the width argument */
	/* FIXME: merge with MANDOCERR_IGNARGV. */
	MANDOCERR_WIDTHARG, /* superfluous width argument */
	MANDOCERR_IGNARGV, /* ignoring argument */
	MANDOCERR_BADDATE, /* bad date argument */
	MANDOCERR_BADWIDTH, /* bad width argument */
	MANDOCERR_BADMSEC, /* unknown manual section */
	MANDOCERR_NESTEDDISP, /* nested displays are not portable */
	MANDOCERR_SECMSEC, /* section not in conventional manual section */
	MANDOCERR_EOLNSPACE, /* end of line whitespace */
	MANDOCERR_SCOPENEST, /* blocks badly nested */

	MANDOCERR_ERROR, /* ===== start of errors ===== */
	MANDOCERR_NAMESECFIRST, /* NAME section must come first */
	MANDOCERR_BADBOOL, /* bad Boolean value */
	MANDOCERR_CHILD, /* child violates parent syntax */
	MANDOCERR_BADATT, /* bad AT&T symbol */
	MANDOCERR_LISTREP, /* list type repeated */
	MANDOCERR_DISPREP, /* display type repeated */
	MANDOCERR_ARGVREP, /* argument repeated */
	MANDOCERR_NONAME, /* manual name not yet set */
	MANDOCERR_MACROOBS, /* obsolete macro ignored */
	MANDOCERR_MACROEMPTY, /* empty macro ignored */
	MANDOCERR_BADBODY, /* macro not allowed in body */
	MANDOCERR_BADPROLOG, /* macro not allowed in prologue */
	MANDOCERR_BADCHAR, /* bad character */
	MANDOCERR_BADNAMESEC, /* bad NAME section contents */
	MANDOCERR_NOBLANKLN, /* no blank lines */
	MANDOCERR_NOTEXT, /* no text in this context */
	MANDOCERR_BADCOMMENT, /* bad comment style */
	MANDOCERR_MACRO, /* unknown macro will be lost */
	MANDOCERR_LINESCOPE, /* line scope broken */
	MANDOCERR_ARGCOUNT, /* argument count wrong */
	MANDOCERR_NOSCOPE, /* no such block is open */
	MANDOCERR_SCOPEBROKEN, /* missing end of block */
	MANDOCERR_SCOPEREP, /* scope already open */
	MANDOCERR_SCOPEEXIT, /* scope open on exit */
	MANDOCERR_UNAME, /* uname(3) system call failed */
	/* FIXME: merge following with MANDOCERR_ARGCOUNT */
	MANDOCERR_NOARGS, /* macro requires line argument(s) */
	MANDOCERR_NOBODY, /* macro requires body argument(s) */
	MANDOCERR_NOARGV, /* macro requires argument(s) */
	MANDOCERR_NOTITLE, /* no title in document */
	MANDOCERR_LISTTYPE, /* missing list type */
	MANDOCERR_DISPTYPE, /* missing display type */
	MANDOCERR_FONTTYPE, /* missing font type */
	MANDOCERR_ARGSLOST, /* line argument(s) will be lost */
	MANDOCERR_BODYLOST, /* body argument(s) will be lost */
	MANDOCERR_IGNPAR, /* paragraph macro ignored */
	MANDOCERR_TBL, /* tbl(1) error */

	MANDOCERR_FATAL, /* ===== start of fatal errors ===== */
	MANDOCERR_COLUMNS, /* column syntax is inconsistent */
	MANDOCERR_BADDISP, /* unsupported display type */
	MANDOCERR_SYNTLINESCOPE, /* line scope broken, syntax violated */
	MANDOCERR_SYNTARGVCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SYNTCHILD, /* child violates parent syntax */
	MANDOCERR_SYNTARGCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SOPATH, /* invalid path in include directive */
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
