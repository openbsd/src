/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Standard include file for "less".
 */

/*
 * Include the file of compile-time options.
 * The <> make cc search for it in -I., not srcdir.
 */
#include <defines.h>

#ifdef _SEQUENT_
/*
 * Kludge for Sequent Dynix systems that have sigsetmask, but
 * it's not compatible with the way less calls it.
 * {{ Do other systems need this? }}
 */
#undef HAVE_SIGSETMASK
#endif

/*
 * Language details.
 */
#if HAVE_VOID
#define	VOID_POINTER	void *
#else
#define	VOID_POINTER	char *
#define	void  int
#endif

#define	public		/* PUBLIC FUNCTION */

/* Library function declarations */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#endif

#if !STDC_HEADERS
char *getenv();
off_t lseek();
VOID_POINTER calloc();
void free();
#endif

#if !HAVE_UPPER_LOWER
#define	isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define	islower(c)	((c) >= 'a' && (c) <= 'z')
#define	toupper(c)	((c) - 'a' + 'A')
#define	tolower(c)	((c) - 'A' + 'a')
#endif

#ifndef NULL
#define	NULL	0
#endif

#ifndef TRUE
#define	TRUE		1
#endif
#ifndef FALSE
#define	FALSE		0
#endif

#define	OPT_OFF		0
#define	OPT_ON		1
#define	OPT_ONPLUS	2

#ifndef HAVE_MEMCPY
#ifndef memcpy
#define	memcpy(to,from,len)	bcopy((from),(to),(len))
#endif
#endif

#define	BAD_LSEEK	((off_t)-1)

/*
 * Special types and constants.
 */
typedef off_t		POSITION;
/*
 * {{ Warning: if POSITION is changed to other than "long",
 *    you may have to change some of the printfs which use "%ld"
 *    to print a variable of type POSITION. }}
 */

#define	NULL_POSITION	((POSITION)(-1))

/*
 * Flags for open()
 */
#if MSOFTC || OS2
#define	OPEN_READ	(O_RDONLY|O_BINARY)
#else
#define	OPEN_READ	(0)
#endif
#if MSOFTC || OS2
#define	OPEN_APPEND	(O_APPEND|O_WRONLY)
#else
#define	OPEN_APPEND	(1)
#endif

#if MSOFTC || OS2
#define	OPEN_TTYIN()	open("CON", O_BINARY|O_RDONLY)
#else
#define	OPEN_TTYIN()	open("/dev/tty", 0)
#endif

/*
 * An IFILE represents an input file.
 */
#define	IFILE		VOID_POINTER
#define	NULL_IFILE	((IFILE)NULL)

/*
 * The structure used to represent a "screen position".
 * This consists of a file position, and a screen line number.
 * The meaning is that the line starting at the given file
 * position is displayed on the ln-th line of the screen.
 * (Screen lines before ln are empty.)
 */
struct scrpos
{
	POSITION pos;
	int ln;
};

typedef union parg
{
	char *p_string;
	int p_int;
} PARG;

#define	NULL_PARG	((PARG *)NULL)

struct textlist
{
	char *string;
	char *endstring;
};

#define	EOI		(-1)

#define	READ_INTR	(-2)

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
#define	SRCH_FORW	0001	/* Search forward from current position */
#define	SRCH_BACK	0002	/* Search backward from current position */
#define	SRCH_FIND_ALL	0010	/* Find and highlight all matches */
#define	SRCH_NOMATCH	0100	/* Search for non-matching lines */
#define	SRCH_PAST_EOF	0200	/* Search past end-of-file, into next file */
#define	SRCH_FIRST_FILE	0400	/* Search starting at the first file */

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

/* Special chars used to tell put_line() to do something special */
#define	AT_NORMAL	(0)
#define	AT_UNDERLINE	(1)
#define	AT_BOLD		(2)
#define	AT_BLINK	(3)
#define	AT_INVIS	(4)
#define	AT_STANDOUT	(5)

#define	CONTROL(c)	((c)&037)
#define	ESC		CONTROL('[')

#define	SIGNAL(sig,func)	signal(sig,func)

#define	S_INTERRUPT	01
#define	S_STOP		02
#define S_WINCH		04
#define	ABORT_SIGS()	(sigs & (S_INTERRUPT|S_STOP))

#define	QUIT_OK		0
#define	QUIT_ERROR	1
#define	QUIT_SAVED_STATUS (-1)

/* filestate flags */
#define	CH_CANSEEK	001
#define	CH_KEEPOPEN	002
#define	CH_POPENED	004

#define	ch_zero()	((POSITION)0)

#include "funcs.h"
