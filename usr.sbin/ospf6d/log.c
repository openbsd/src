/*	$OpenBSD: log.c,v 1.1 2007/10/08 10:44:50 norby Exp $ */

/*
 * Copyright (c) 2006 Claudio Jeker <claudio@openbsd.org>
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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "ospf6d.h"
#include "log.h"

static const char * const procnames[] = {
	"parent",
	"ospfe",
	"rde"
};

int	debug;

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
		logit(LOG_CRIT, "fatal in %s: %s", procnames[ospfd_process],
		    strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal in %s: %s: %s",
			    procnames[ospfd_process], emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal in %s: %s",
			    procnames[ospfd_process], emsg);

	if (ospfd_process == PROC_MAIN)
		exit(1);
	else				/* parent copes via SIGCHLD */
		_exit(1);
}

void
fatalx(const char *emsg)
{
	errno = 0;
	fatal(emsg);
}

const char *
log_in6addr(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;
	u_int16_t		tmp16;

	bzero(&sa_in6, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	memcpy(&sa_in6.sin6_addr, addr, sizeof(sa_in6.sin6_addr));

	/* XXX thanks, KAME, for this ugliness... adopted from route/show.c */
	if (IN6_IS_ADDR_LINKLOCAL(&sa_in6.sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&sa_in6.sin6_addr)) {
		memcpy(&tmp16, &sa_in6.sin6_addr.s6_addr[2], sizeof(tmp16));
		sa_in6.sin6_scope_id = ntohs(tmp16);
		sa_in6.sin6_addr.s6_addr[2] = 0;
		sa_in6.sin6_addr.s6_addr[3] = 0;
	}

	return (log_sockaddr((struct sockaddr *)&sa_in6));
}

const char *
log_sockaddr(struct sockaddr *sa)
{
	static char	buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}

/* names */
const char *
nbr_state_name(int state)
{
	switch (state) {
	case NBR_STA_DOWN:
		return ("DOWN");
	case NBR_STA_ATTEMPT:
		return ("ATTMP");
	case NBR_STA_INIT:
		return ("INIT");
	case NBR_STA_2_WAY:
		return ("2-WAY");
	case NBR_STA_XSTRT:
		return ("EXSTA");
	case NBR_STA_SNAP:
		return ("SNAP");
	case NBR_STA_XCHNG:
		return ("EXCHG");
	case NBR_STA_LOAD:
		return ("LOAD");
	case NBR_STA_FULL:
		return ("FULL");
	default:
		return ("UNKNW");
	}
}

const char *
if_state_name(int state)
{
	switch (state) {
	case IF_STA_DOWN:
		return ("DOWN");
	case IF_STA_LOOPBACK:
		return ("LOOP");
	case IF_STA_WAITING:
		return ("WAIT");
	case IF_STA_POINTTOPOINT:
		return ("P2P");
	case IF_STA_DROTHER:
		return ("OTHER");
	case IF_STA_BACKUP:
		return ("BCKUP");
	case IF_STA_DR:
		return ("DR");
	default:
		return ("UNKNW");
	}
}

const char *
if_type_name(enum iface_type type)
{
	switch (type) {
	case IF_TYPE_POINTOPOINT:
		return ("POINTOPOINT");
	case IF_TYPE_BROADCAST:
		return ("BROADCAST");
	case IF_TYPE_NBMA:
		return ("NBMA");
	case IF_TYPE_POINTOMULTIPOINT:
		return ("POINTOMULTIPOINT");
	case IF_TYPE_VIRTUALLINK:
		return ("VIRTUALLINK");
	}
	/* NOTREACHED */
	return ("UNKNOWN");
}

const char *
dst_type_name(enum dst_type type)
{
	switch (type) {
	case DT_NET:
		return ("Network");
	case DT_RTR:
		return ("Router");
	}
	/* NOTREACHED */
	return ("unknown");
}

const char *
path_type_name(enum path_type type)
{
	switch (type) {
	case PT_INTRA_AREA:
		return ("Intra-Area");
	case PT_INTER_AREA:
		return ("Inter-Area");
	case PT_TYPE1_EXT:
		return ("Type 1 ext");
	case PT_TYPE2_EXT:
		return ("Type 2 ext");
	}
	/* NOTREACHED */
	return ("unknown");
}
