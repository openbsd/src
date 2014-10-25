/*	$OpenBSD: log.c,v 1.24 2014/10/25 03:23:49 lteo Exp $	*/

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
#include <arpa/inet.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <event.h>
#include <netdb.h>
#include <ctype.h>

#include <openssl/ssl.h>

#include "relayd.h"

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
host_error(enum host_error he)
{
	switch (he) {
	case HCE_NONE:
		return ("none");
		break;
	case HCE_ABORT:
		return ("aborted");
		break;
	case HCE_INTERVAL_TIMEOUT:
		return ("interval timeout");
		break;
	case HCE_ICMP_OK:
		return ("icmp ok");
		break;
	case HCE_ICMP_READ_TIMEOUT:
		return ("icmp read timeout");
		break;
	case HCE_ICMP_WRITE_TIMEOUT:
		return ("icmp write timeout");
		break;
	case HCE_TCP_SOCKET_ERROR:
		return ("tcp socket error");
		break;
	case HCE_TCP_SOCKET_LIMIT:
		return ("tcp socket limit");
		break;
	case HCE_TCP_SOCKET_OPTION:
		return ("tcp socket option");
		break;
	case HCE_TCP_CONNECT_FAIL:
		return ("tcp connect failed");
		break;
	case HCE_TCP_CONNECT_TIMEOUT:
		return ("tcp connect timeout");
		break;
	case HCE_TCP_CONNECT_OK:
		return ("tcp connect ok");
		break;
	case HCE_TCP_WRITE_TIMEOUT:
		return ("tcp write timeout");
		break;
	case HCE_TCP_WRITE_FAIL:
		return ("tcp write failed");
		break;
	case HCE_TCP_READ_TIMEOUT:
		return ("tcp read timeout");
		break;
	case HCE_TCP_READ_FAIL:
		return ("tcp read failed");
		break;
	case HCE_SCRIPT_OK:
		return ("script ok");
		break;
	case HCE_SCRIPT_FAIL:
		return ("script failed");
		break;
	case HCE_SSL_CONNECT_OK:
		return ("ssl connect ok");
		break;
	case HCE_SSL_CONNECT_FAIL:
		return ("ssl connect failed");
		break;
	case HCE_SSL_CONNECT_TIMEOUT:
		return ("ssl connect timeout");
		break;
	case HCE_SSL_CONNECT_ERROR:
		return ("ssl connect error");
		break;
	case HCE_SSL_READ_TIMEOUT:
		return ("ssl read timeout");
		break;
	case HCE_SSL_WRITE_TIMEOUT:
		return ("ssl write timeout");
		break;
	case HCE_SSL_READ_ERROR:
		return ("ssl read error");
		break;
	case HCE_SSL_WRITE_ERROR:
		return ("ssl write error");
		break;
	case HCE_SEND_EXPECT_FAIL:
		return ("send/expect failed");
		break;
	case HCE_SEND_EXPECT_OK:
		return ("send/expect ok");
		break;
	case HCE_HTTP_CODE_ERROR:
		return ("http code malformed");
		break;
	case HCE_HTTP_CODE_FAIL:
		return ("http code mismatch");
		break;
	case HCE_HTTP_CODE_OK:
		return ("http code ok");
		break;
	case HCE_HTTP_DIGEST_ERROR:
		return ("http digest malformed");
		break;
	case HCE_HTTP_DIGEST_FAIL:
		return ("http digest mismatch");
		break;
	case HCE_HTTP_DIGEST_OK:
		return ("http digest ok");
		break;
	}
	/* NOTREACHED */
	return ("invalid");
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
	bzero(p, BUFSIZ);

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
