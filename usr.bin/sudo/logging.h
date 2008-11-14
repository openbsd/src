/*
 * Copyright (c) 1999-2005, 2008
 *	Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * $Sudo: logging.h,v 1.13 2008/11/09 14:13:12 millert Exp $
 */

#ifndef _LOGGING_H
#define _LOGGING_H

#include <syslog.h>
#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

/* Logging types */
#define SLOG_SYSLOG		0x01
#define SLOG_FILE		0x02
#define SLOG_BOTH		0x03

/* Flags for log_error() */
#define MSG_ONLY		0x01
#define USE_ERRNO		0x02
#define NO_MAIL			0x04
#define NO_EXIT			0x08
#define NO_STDERR		0x10

/*
 * Maximum number of characters to log per entry.  The syslogger
 * will log this much, after that, it truncates the log line.
 * We need this here to make sure that we continue with another
 * syslog(3) call if the internal buffer is more than 1023 characters.
 */
#ifndef MAXSYSLOGLEN
# define MAXSYSLOGLEN		960
#endif

void log_allowed		__P((int));
void log_denial			__P((int, int));
void log_error			__P((int flags, const char *fmt, ...))
				    __printflike(2, 3);
RETSIGTYPE reapchild		__P((int));

#endif /* _LOGGING_H */
