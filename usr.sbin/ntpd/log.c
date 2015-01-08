/*	$OpenBSD: log.c,v 1.10 2015/01/08 00:30:08 bcook Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/socket.h>

#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "log.h"

#define	TRACE_DEBUG	0x1

static int	 foreground;
static int	 verbose;

void	 vlog(int, const char *, va_list);
void	 logit(int, const char *, ...)
    __attribute__((format (printf, 2, 3)));


void
log_init(int n_foreground)
{
	extern char	*__progname;

	foreground = n_foreground;
	if (! foreground)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_MAIL);

	tzset();
}

void
log_verbose(int v)
{
	verbose = v;
}

void
logit(int pri, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vlog(pri, fmt, ap);
	va_end(ap);
}

void
vlog(int pri, const char *fmt, va_list ap)
{
	char	*nfmt;

	if (foreground) {
		/* best effort in out of mem situations */
		if (asprintf(&nfmt, "%s\n", fmt) == -1) {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		} else {
			vfprintf(stderr, nfmt, ap);
			free(nfmt);
		}
		fflush(stderr);
	} else
		vsyslog(pri, fmt, ap);
}


void
log_warn(const char *emsg, ...)
{
	char	*nfmt;
	va_list	 ap;

	/* best effort to even work in out of memory situations */
	if (emsg == NULL)
		logit(LOG_CRIT, "%s", strerror(errno));
	else {
		va_start(ap, emsg);

		if (asprintf(&nfmt, "%s: %s", emsg, strerror(errno)) == -1) {
			/* we tried it... */
			vlog(LOG_CRIT, emsg, ap);
			logit(LOG_CRIT, "%s", strerror(errno));
		} else {
			vlog(LOG_CRIT, nfmt, ap);
			free(nfmt);
		}
		va_end(ap);
	}
}

void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_CRIT, emsg, ap);
	va_end(ap);
}

void
log_info(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_INFO, emsg, ap);
	va_end(ap);
}

void
log_debug(const char *emsg, ...)
{
	va_list	 ap;

	if (verbose & TRACE_DEBUG) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
log_trace(int mask, const char *emsg, ...)
{
	va_list	 ap;

	if (verbose & mask) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

static void
fatal_arg(const char *emsg, va_list ap)
{
#define	FATALBUFSIZE	1024
	static char	ebuffer[FATALBUFSIZE];

	if (emsg == NULL)
		(void)strlcpy(ebuffer, strerror(errno), sizeof ebuffer);
	else {
		if (errno) {
			(void)vsnprintf(ebuffer, sizeof ebuffer, emsg, ap);
			(void)strlcat(ebuffer, ": ", sizeof ebuffer);
			(void)strlcat(ebuffer, strerror(errno), sizeof ebuffer);
		}
		else
			(void)vsnprintf(ebuffer, sizeof ebuffer, emsg, ap);
	}
	logit(LOG_CRIT, "fatal: %s", ebuffer);
}

void
fatal(const char *emsg, ...)
{
	va_list	ap;

	va_start(ap, emsg);
	fatal_arg(emsg, ap);
	va_end(ap);
	exit(1);
}

void
fatalx(const char *emsg, ...)
{
	va_list	ap;

	errno = 0;
	va_start(ap, emsg);
	fatal_arg(emsg, ap);
	va_end(ap);
	exit(1);
}

const char *
log_sockaddr(struct sockaddr *sa)
{
	static char	buf[NI_MAXHOST];

	if (getnameinfo(sa, SA_LEN(sa), buf, sizeof(buf), NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}
