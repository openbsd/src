/*	$OpenBSD: log.c,v 1.4 2014/12/21 00:54:49 guenther Exp $	*/

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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <event.h>
#include <netdb.h>
#include <ctype.h>

#include "httpd.h"

int	 debug;
int	 verbose;

void	 vlog(int, const char *, va_list)
	    __attribute__((__format__ (printf, 2, 0)));
void	 logit(int, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)));

void
log_init(int n_debug)
{
	extern char	*__progname;

	debug = n_debug;
	verbose = n_debug;

	if (!debug)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

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

	if (debug) {
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

	if (verbose > 1) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
fatal(const char *emsg)
{
	if (emsg == NULL)
		logit(LOG_CRIT, "fatal: %s", strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal: %s: %s",
			    emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal: %s", emsg);

	exit(1);
}

void
fatalx(const char *emsg)
{
	errno = 0;
	fatal(emsg);
}

const char *
print_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	if (getnameinfo((struct sockaddr *)ss, ss->ss_len,
	    buf, len, NULL, 0, NI_NUMERICHOST) != 0) {
		buf[0] = '\0';
		return (NULL);
	}
	return (buf);
}

const char *
print_time(struct timeval *a, struct timeval *b, char *buf, size_t len)
{
	struct timeval		tv;
	u_long			h, sec, min;

	timerclear(&tv);
	timersub(a, b, &tv);
	sec = tv.tv_sec % 60;
	min = tv.tv_sec / 60 % 60;
	h = tv.tv_sec / 60 / 60;

	snprintf(buf, len, "%.2lu:%.2lu:%.2lu", h, min, sec);
	return (buf);
}

const char *
printb_flags(const u_int32_t v, const char *bits)
{
	static char	 buf[2][BUFSIZ];
	static int	 idx = 0;
	int		 i, any = 0;
	char		 c, *p, *r;

	p = r = buf[++idx % 2];
	memset(p, 0, BUFSIZ);

	if (bits) {
		bits++;
		while ((i = *bits++)) {
			if (v & (1 << (i - 1))) {
				if (any) {
					*p++ = ',';
					*p++ = ' ';
				}
				any = 1;
				for (; (c = *bits) > 32; bits++) {
					if (c == '_')
						*p++ = ' ';
					else
						*p++ = tolower((u_char)c);
				}
			} else
				for (; *bits > 32; bits++)
					;
		}
	}

	return (r);
}

void
getmonotime(struct timeval *tv)
{
	struct timespec	 ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		fatal("clock_gettime");

	TIMESPEC_TO_TIMEVAL(tv, &ts);
}
