/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Standard include file for "less".
 */

#include "defines.h"

#include <sys/types.h>

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wctype.h>

/*
 * Simple lowercase test which can be used during option processing
 * (before options are parsed which might tell us what charset to use).
 */

#undef IS_SPACE
#undef IS_DIGIT

#define	IS_SPACE(c)	isspace((unsigned char)(c))
#define	IS_DIGIT(c)	isdigit((unsigned char)(c))

#ifndef TRUE
#define	TRUE		1
#endif
#ifndef FALSE
#define	FALSE		0
#endif

#define	OPT_OFF		0
#define	OPT_ON		1
#define	OPT_ONPLUS	2

/*
 * Special types and constants.
 */
typedef unsigned long LWCHAR;
#define	MIN_LINENUM_WIDTH  7	/* Min printing width of a line number */
#define	MAX_UTF_CHAR_LEN   6	/* Max bytes in one UTF-8 char */

#define	SHELL_META_QUEST 1

/*
 * An IFILE represents an input file.
 */
#define	IFILE		void *

/*
 * The structure used to represent a "screen position".
 * This consists of a file position, and a screen line number.
 * The meaning is that the line starting at the given file
 * position is displayed on the ln-th line of the screen.
 * (Screen lines before ln are empty.)
 */
struct scrpos {
	off_t pos;
	int ln;
};

typedef union parg {
	char *p_string;
	int p_int;
	off_t p_linenum;
} PARG;

struct textlist {
	char *string;
	char *endstring;
};

#define	EOI		(-1)

#define	READ_INTR	(-2)

/* A fraction is represented by an int n; the fraction is n/NUM_FRAC_DENOM */
#define	NUM_FRAC_DENOM			1000000
#define	NUM_LOG_FRAC_DENOM		6

/* How quiet should we be? */
#define	NOT_QUIET	0	/* Ring bell at eof and for errors */
#define	LITTLE_QUIET	1	/* Ring bell only for errors */
#define	VERY_QUIET	2	/* Never ring bell */

/* How should we prompt? */
#define	PR_SHORT	0	/* Prompt with colon */
#define	PR_MEDIUM	1	/* Prompt with message */
#define	PR_LONG		2	/* Prompt with longer message */

/* How should we handle backspaces? */
#define	BS_SPECIAL	0	/* Do special things for underlining and bold */
#define	BS_NORMAL	1	/* \b treated as normal char; actually output */
#define	BS_CONTROL	2	/* \b treated as control char; prints as ^H */

/* How should we search? */
#define	SRCH_FORW	(1 << 0)  /* Search forward from current position */
#define	SRCH_BACK	(1 << 1)  /* Search backward from current position */
#define	SRCH_NO_MOVE	(1 << 2)  /* Highlight, but don't move */
#define	SRCH_FIND_ALL	(1 << 4)  /* Find and highlight all matches */
#define	SRCH_NO_MATCH	(1 << 8)  /* Search for non-matching lines */
#define	SRCH_PAST_EOF	(1 << 9)  /* Search past end-of-file, into next file */
#define	SRCH_FIRST_FILE	(1 << 10) /* Search starting at the first file */
#define	SRCH_NO_REGEX	(1 << 12) /* Don't use regular expressions */
#define	SRCH_FILTER	(1 << 13) /* Search is for '&' (filter) command */
#define	SRCH_AFTER_TARGET (1 << 14) /* Start search after the target line */

#define	SRCH_REVERSE(t)	(((t) & SRCH_FORW) ? \
				(((t) & ~SRCH_FORW) | SRCH_BACK) : \
				(((t) & ~SRCH_BACK) | SRCH_FORW))

/* */
#define	NO_MCA		0
#define	MCA_DONE	1
#define	MCA_MORE	2

#define	CC_OK		0	/* Char was accepted & processed */
#define	CC_QUIT		1	/* Char was a request to abort current cmd */
#define	CC_ERROR	2	/* Char could not be accepted due to error */
#define	CC_PASS		3	/* Char was rejected (internal) */

#define	CF_QUIT_ON_ERASE 0001   /* Abort cmd if its entirely erased */

/* Special char bit-flags used to tell put_line() to do something special */
#define	AT_NORMAL	(0)
#define	AT_UNDERLINE	(1 << 0)
#define	AT_BOLD		(1 << 1)
#define	AT_BLINK	(1 << 2)
#define	AT_STANDOUT	(1 << 3)
#define	AT_ANSI		(1 << 4)  /* Content-supplied "ANSI" escape sequence */
#define	AT_BINARY	(1 << 5)  /* LESS*BINFMT representation */
#define	AT_HILITE	(1 << 6)  /* Internal highlights (e.g., for search) */
#define	AT_INDET	(1 << 7)  /* Indeterminate: either bold or underline */

#define	CONTROL(c)	((c)&037)
#define	ESC		CONTROL('[')

extern int any_sigs(void);
extern int abort_sigs(void);

#define	QUIT_OK		0
#define	QUIT_ERROR	1
#define	QUIT_INTERRUPT	2
#define	QUIT_SAVED_STATUS (-1)

#define	FOLLOW_DESC	0
#define	FOLLOW_NAME	1

/* filestate flags */
#define	CH_CANSEEK	001
#define	CH_KEEPOPEN	002
#define	CH_POPENED	004
#define	CH_HELPFILE	010
#define	CH_NODATA	020	/* Special case for zero length files */


#define	ch_zero()	(0)

#define	FAKE_EMPTYFILE	"@/\\less/\\empty/\\file/\\@"

/* Flags for cvt_text */
#define	CVT_TO_LC	01	/* Convert upper-case to lower-case */
#define	CVT_BS		02	/* Do backspace processing */
#define	CVT_CRLF	04	/* Remove CR after LF */
#define	CVT_ANSI	010	/* Remove ANSI escape sequences */

#include "funcs.h"

/* Functions not included in funcs.h */
void postoa(off_t, char *, size_t);
void inttoa(int, char *, size_t);
