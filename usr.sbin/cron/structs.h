/*	$OpenBSD: structs.h,v 1.5 2004/06/17 22:11:55 millert Exp $	*/

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

typedef	struct _entry {
	struct _entry	*next;
	struct passwd	*pwd;
	char		**envp;
	char		*cmd;
	bitstr_t	bit_decl(minute, MINUTE_COUNT);
	bitstr_t	bit_decl(hour,   HOUR_COUNT);
	bitstr_t	bit_decl(dom,    DOM_COUNT);
	bitstr_t	bit_decl(month,  MONTH_COUNT);
	bitstr_t	bit_decl(dow,    DOW_COUNT);
	int		flags;
#define	MIN_STAR	0x01
#define	HR_STAR		0x02
#define	DOM_STAR	0x04
#define	DOW_STAR	0x08
#define	WHEN_REBOOT	0x10
#define	DONT_LOG	0x20
} entry;

			/* the crontab database will be a list of the
			 * following structure, one element per user
			 * plus one for the system.
			 *
			 * These are the crontabs.
			 */

typedef	struct _user {
	struct _user	*next, *prev;	/* links */
	char		*name;
	time_t		mtime;		/* last modtime of crontab */
	entry		*crontab;	/* this person's crontab */
} user;

typedef	struct _cron_db {
	user		*head, *tail;	/* links */
	time_t		mtime;		/* last modtime on spooldir */
} cron_db;

typedef struct _atjob {
	struct _atjob	*next, *prev;	/* links */
	uid_t		uid;		/* uid of the job */
	gid_t		gid;		/* gid of the job */
	int		queue;		/* name of the at queue */
	time_t		run_time;	/* time to run at job */
} atjob;

typedef struct _at_db {
	atjob		*head, *tail;	/* links */
	time_t		mtime;		/* last modtime on spooldir */
} at_db;
				/* in the C tradition, we only create
				 * variables for the main program, just
				 * extern them elsewhere.
				 */
