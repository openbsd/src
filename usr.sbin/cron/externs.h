/*	$OpenBSD: externs.h,v 1.8 2002/07/17 22:10:56 millert Exp $	*/

/* Copyright 1993,1994 by Paul Vixie
 * All rights reserved
 */

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

/* reorder these #include's at your peril */

#include <sys/param.h>
#include <sys/types.h>
#if !defined(AIX) && !defined(UNICOS)
#include <sys/time.h>
#endif
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <ctype.h>
#ifndef isascii
#define isascii(c)      ((unsigned)(c)<=0177)
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#if defined(SYSLOG)
# include <syslog.h>
#endif

#if defined(LOGIN_CAP)
#include <login_cap.h>
#endif /*LOGIN_CAP*/

#if defined(BSD_AUTH)
#include <bsd_auth.h>
#endif /*BSD_AUTH*/

#define DIR_T	struct dirent
#define WAIT_T	int
#define SIG_T	sig_t
#define TIME_T	time_t
#define PID_T	pid_t

#ifndef TZNAME_ALREADY_DEFINED
extern char *tzname[2];
#endif
#define TZONE(tm) tzname[(tm).tm_isdst]

#if (BSD >= 198606)
# define HAVE_FCHOWN
# define HAVE_FCHMOD
#endif

#if (BSD >= 199103)
# define HAVE_SAVED_GIDS
#endif

#define MY_UID(pw) getuid()
#define MY_GID(pw) getgid()

/* getopt() isn't part of POSIX.  some systems define it in <stdlib.h> anyway.
 * of those that do, some complain that our definition is different and some
 * do not.  to add to the misery and confusion, some systems define getopt()
 * in ways that we cannot predict or comprehend, yet do not define the adjunct
 * external variables needed for the interface.
 */
#if (!defined(BSD) || (BSD < 198911))
int	getopt(int, char * const *, const char *);
#endif

#if (!defined(BSD) || (BSD < 199103))
extern	char *optarg;
extern	int optind, opterr, optopt;
#endif

/* digital unix needs this but does not give us a way to identify it.
 */
extern	int		flock(int, int);

/* not all systems who provide flock() provide these definitions.
 */
#ifndef LOCK_SH
# define LOCK_SH 1
#endif
#ifndef LOCK_EX
# define LOCK_EX 2
#endif
#ifndef LOCK_NB
# define LOCK_NB 4
#endif
#ifndef LOCK_UN
# define LOCK_UN 8
#endif

#ifndef WCOREDUMP
# define WCOREDUMP(st)          (((st) & 0200) != 0)
#endif
