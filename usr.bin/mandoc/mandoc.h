/*	$Id: mandoc.h,v 1.11 2010/07/25 18:05:54 schwarze Exp $ */
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

/*
 * This contains declarations that are available system-wide.
 */

#define ASCII_NBRSP	 31  /* non-breaking space */
#define	ASCII_HYPH	 30  /* breakable hyphen */

__BEGIN_DECLS

enum	mandocerr {
	MANDOCERR_OK,

	MANDOCERR_WARNING, /* ===== end of warnings ===== */

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
	MANDOCERR_SECMSEC, /* section not in conventional manual section */
	MANDOCERR_EOLNSPACE, /* end of line whitespace */
	MANDOCERR_SCOPENEST, /* blocks badly nested */
	MANDOCERR_SCOPEEXIT, /* scope open on exit */

	MANDOCERR_ERROR, /* ===== end of errors ===== */

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
	MANDOCERR_SCOPEREP, /* scope already open */
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

	MANDOCERR_FATAL, /* ===== end of fatal errors ===== */

	MANDOCERR_COLUMNS, /* column syntax is inconsistent */
	/* FIXME: this should be a MANDOCERR_ERROR */
	MANDOCERR_NESTEDDISP, /* displays may not be nested */
	MANDOCERR_BADDISP, /* unsupported display type */
	MANDOCERR_SCOPEFATAL, /* blocks badly nested */
	MANDOCERR_SYNTNOSCOPE, /* no scope to rewind: syntax violated */
	MANDOCERR_SYNTSCOPE, /* scope broken, syntax violated */
	MANDOCERR_SYNTLINESCOPE, /* line scope broken, syntax violated */
	MANDOCERR_SYNTARGVCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_SYNTCHILD, /* child violates parent syntax */
	MANDOCERR_SYNTARGCOUNT, /* argument count wrong, violates syntax */
	MANDOCERR_NODOCBODY, /* no document body */
	MANDOCERR_NODOCPROLOG, /* no document prologue */
	MANDOCERR_UTSNAME, /* utsname system call failed */
	MANDOCERR_MEM, /* memory exhausted */

	MANDOCERR_MAX
};

enum	regs {
	REG_nS = 0,	/* register: nS */
	REG__MAX
};

/*
 * A single register entity.  If "set" is zero, the value of the
 * register should be the default one, which is per-register.  It's
 * assumed that callers know which type in "v" corresponds to which
 * register value.
 */
struct	reg {
	int		  set; /* whether set or not */
	union {
		unsigned  u; /* unsigned integer */
	} v;
};

/*
 * The primary interface to setting register values is in libroff,
 * although libmdoc and libman from time to time will manipulate
 * registers (such as `.Sh SYNOPSIS' enabling REG_nS).
 */
struct	regset {
	struct reg	  regs[REG__MAX];
};

/*
 * Callback function for warnings, errors, and fatal errors as they
 * occur in the compilers libroff, libmdoc, and libman.
 */
typedef	int		(*mandocmsg)(enum mandocerr, void *,
				int, int, const char *);

__END_DECLS

#endif /*!MANDOC_H*/
