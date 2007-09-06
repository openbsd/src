/*	$OpenBSD: log.c,v 1.6 2007/09/06 19:55:45 reyk Exp $	*/

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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>

#include <arpa/inet.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <event.h>

#include <openssl/ssl.h>

#include "hoststated.h"

int	 debug;

void	 vlog(int, const char *, va_list);
void	 logit(int, const char *, ...);

void
log_init(int n_debug)
{
	extern char	*__progname;

	debug = n_debug;

	if (!debug)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	tzset();
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

	if (debug) {
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
host_status(enum host_status status)
{
	switch (status) {
	case HOST_DOWN:
		return ("down");
	case HOST_UNKNOWN:
		return ("unknown");
	case HOST_UP:
		return ("up");
	};
	/* NOTREACHED */
	return ("invalid");
}

const char *
table_check(enum table_check check)
{
	switch (check) {
	case CHECK_NOCHECK:
		return ("none");
	case CHECK_ICMP:
		return ("icmp");
	case CHECK_TCP:
		return ("tcp");
	case CHECK_HTTP_CODE:
		return ("http code");
	case CHECK_HTTP_DIGEST:
		return ("http digest");
	case CHECK_SEND_EXPECT:
		return ("send expect");
	case CHECK_SCRIPT:
		return ("script");
	};
	/* NOTREACHED */
	return ("invalid");
}

const char *
print_availability(u_long cnt, u_long up)
{
	static char buf[BUFSIZ];

	if (cnt == 0)
		return ("");
	bzero(buf, sizeof(buf));
	snprintf(buf, sizeof(buf), "%.2f%%", (double)up / cnt * 100);
	return (buf);
}

const char *
print_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	int af = ss->ss_family;
	void *ptr;

	bzero(buf, len);
	if (af == AF_INET)
		ptr = &((struct sockaddr_in *)ss)->sin_addr;
	else
		ptr = &((struct sockaddr_in6 *)ss)->sin6_addr;
	return (inet_ntop(af, ptr, buf, len));
}
