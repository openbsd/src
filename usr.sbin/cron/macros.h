/*	$OpenBSD: macros.h,v 1.3 2002/07/15 19:13:29 millert Exp $	*/

/*
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

	/* these are really immutable, and are
	 *   defined for symbolic convenience only
	 * TRUE, FALSE, and ERR must be distinct
	 * ERR must be < OK.
	 */
#define TRUE		1
#define FALSE		0
	/* system calls return this on success */
#define OK		0
	/*   or this on error */
#define ERR		(-1)

	/* turn this on to get '-x' code */
#ifndef DEBUGGING
#define DEBUGGING	FALSE
#endif

#define	INIT_PID	1	/* parent of orphans */
#define READ_PIPE	0	/* which end of a pipe pair do you read? */
#define WRITE_PIPE	1	/*   or write to? */
#define STDIN		0	/* what is stdin's file descriptor? */
#define STDOUT		1	/*   stdout's? */
#define STDERR		2	/*   stderr's? */
#define ERROR_EXIT	1	/* exit() with this will scare the shell */
#define	OK_EXIT		0	/* exit() with this is considered 'normal' */
#define	MAX_FNAME	100	/* max length of internally generated fn */
#define	MAX_COMMAND	1000	/* max length of internally generated cmd */
#define	MAX_ENVSTR	1000	/* max length of envvar=value\0 strings */
#define	MAX_TEMPSTR	100	/* obvious */
#define	MAX_UNAME	(_PW_NAME_LEN+1)	/* max length of username, should be overkill */
#define	ROOT_UID	0	/* don't change this, it really must be root */
#define	ROOT_USER	"root"	/* ditto */

				/* NOTE: these correspond to DebugFlagNames,
				 *	defined below.
				 */
#define	DEXT		0x0001	/* extend flag for other debug masks */
#define	DSCH		0x0002	/* scheduling debug mask */
#define	DPROC		0x0004	/* process control debug mask */
#define	DPARS		0x0008	/* parsing debug mask */
#define	DLOAD		0x0010	/* database loading debug mask */
#define	DMISC		0x0020	/* misc debug mask */
#define	DTEST		0x0040	/* test mode: don't execute any commands */

#define	PPC_NULL	((const char **)NULL)

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define	Skip_Blanks(c, f) \
			while (c == '\t' || c == ' ') \
				c = get_char(f);

#define	Skip_Nonblanks(c, f) \
			while (c!='\t' && c!=' ' && c!='\n' && c != EOF) \
				c = get_char(f);

#define	Skip_Line(c, f) \
			do {c = get_char(f);} while (c != '\n' && c != EOF);

#if DEBUGGING
# define Debug(mask, message) \
			if ( (DebugFlags & (mask) ) == (mask) ) \
				printf message;
#else /* !DEBUGGING */
# define Debug(mask, message) \
			;
#endif /* DEBUGGING */

#define	MkLower(ch)	(isupper(ch) ? tolower(ch) : ch)
#define	MkUpper(ch)	(islower(ch) ? toupper(ch) : ch)
#define	Set_LineNum(ln)	{Debug(DPARS|DEXT,("linenum=%d\n",ln)); \
			 LineNumber = ln; \
			}

/* Data values used on cron socket */
#define RELOAD_CRON	0x2
#define RELOAD_AT	0x4

#ifdef HAVE_TM_GMTOFF
#define get_gmtoff(c, t)        (t->tm_gmtoff)
#endif

#define SECONDS_PER_MINUTE	60

#define	FIRST_MINUTE	0
#define	LAST_MINUTE	59
#define	MINUTE_COUNT	(LAST_MINUTE - FIRST_MINUTE + 1)

#define	FIRST_HOUR	0
#define	LAST_HOUR	23
#define	HOUR_COUNT	(LAST_HOUR - FIRST_HOUR + 1)

#define	FIRST_DOM	1
#define	LAST_DOM	31
#define	DOM_COUNT	(LAST_DOM - FIRST_DOM + 1)

#define	FIRST_MONTH	1
#define	LAST_MONTH	12
#define	MONTH_COUNT	(LAST_MONTH - FIRST_MONTH + 1)

/* note on DOW: 0 and 7 are both Sunday, for compatibility reasons. */
#define	FIRST_DOW	0
#define	LAST_DOW	7
#define	DOW_COUNT	(LAST_DOW - FIRST_DOW + 1)

			/* each user's crontab will be held as a list of
			 * the following structure.
			 *
			 * These are the cron commands.
			 */
