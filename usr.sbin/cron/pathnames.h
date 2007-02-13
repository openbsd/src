/*	$OpenBSD: pathnames.h,v 1.12 2007/02/13 18:39:34 mglocker Exp $	*/

/* Copyright 1993,1994 by Paul Vixie
 * All rights reserved
 */

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

#ifndef _PATHNAMES_H_
#define _PATHNAMES_H_

#if (defined(BSD)) && (BSD >= 199103) || defined(__linux) || defined(AIX)
# include <paths.h>
#endif /*BSD*/

#ifndef CRONDIR
			/* CRONDIR is where cron(8) and crontab(1) both chdir
			 * to; SPOOL_DIR, CRON_ALLOW, CRON_DENY, and LOG_FILE
			 * are all relative to this directory.
			 */
#define CRONDIR		"/var/cron"
#endif

			/* SPOOLDIR is where the crontabs live.
			 * This directory will have its modtime updated
			 * whenever crontab(1) changes a crontab; this is
			 * the signal for cron(8) to look at each individual
			 * crontab file and reload those whose modtimes are
			 * newer than they were last time around (or which
			 * didn't exist last time around...)
			 */
#define SPOOL_DIR	"tabs"

			/* ATDIR is where the at jobs live (relative to CRONDIR)
			 * This directory will have its modtime updated
			 * whenever at(1) changes a crontab; this is
			 * the signal for cron(8) to look for changes in the
			 * jobs directory (new, changed or jobs).
			 */
#define AT_DIR		"atjobs"

			/* CRONSOCK is the name of the socket used by at and
			 * crontab to poke cron to re-read the at and cron
			 * spool files while cron is asleep.
			 * It lives in the spool directory.
			 */
#define	CRONSOCK	".sock"

			/* cron allow/deny file.  At least cron.deny must
			 * exist for ordinary users to run crontab.
			 */
#define	CRON_ALLOW	"cron.allow"
#define	CRON_DENY	"cron.deny"

			/* at allow/deny file.  At least at.deny must
			 * exist for ordinary users to run at.
			 */
#define	AT_ALLOW	"at.allow"
#define	AT_DENY		"at.deny"

			/* undefining this turns off logging to a file.  If
			 * neither LOG_FILE or SYSLOG is defined, we don't log.
			 * If both are defined, we log both ways.  Note that if
			 * LOG_CRON is defined by <syslog.h>, LOG_FILE will not
			 * be used.
			 */
#define LOG_FILE	"log"

			/* where should the daemon stick its PID?
			 * PIDDIR must end in '/'.
			 */
#ifdef _PATH_VARRUN
# define PIDDIR	_PATH_VARRUN
#else
# define PIDDIR "/etc/"
#endif
#define PIDFILE		"cron.pid"
#define _PATH_CRON_PID	PIDDIR PIDFILE

			/* 4.3BSD-style crontab */
#define SYSCRONTAB	"/etc/crontab"

			/* what editor to use if no EDITOR or VISUAL
			 * environment variable specified.
			 */
#if defined(_PATH_VI)
# define EDITOR _PATH_VI
#else
# define EDITOR "/usr/ucb/vi"
#endif

#ifndef _PATH_SENDMAIL
# define _PATH_SENDMAIL "/usr/lib/sendmail"
#endif

#ifndef _PATH_BSHELL
# define _PATH_BSHELL "/bin/sh"
#endif

#ifndef _PATH_DEFPATH
# define _PATH_DEFPATH "/usr/bin:/bin"
#endif

#ifndef _PATH_TMP
# define _PATH_TMP "/tmp/"
#endif

#ifndef _PATH_DEVNULL
# define _PATH_DEVNULL "/dev/null"
#endif

#endif /* _PATHNAMES_H_ */
