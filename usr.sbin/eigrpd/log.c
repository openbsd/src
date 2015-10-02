/*	$OpenBSD: log.c,v 1.1 2015/10/02 04:26:47 renato Exp $ */

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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "eigrpd.h"
#include "rde.h"
#include "log.h"

static const char * const procnames[] = {
	"parent",
	"eigrpe",
	"rde"
};

int	debug;
int	verbose;

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

	if (verbose) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
fatal(const char *emsg)
{
	if (emsg == NULL)
		logit(LOG_CRIT, "fatal in %s: %s", procnames[eigrpd_process],
		    strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal in %s: %s: %s",
			    procnames[eigrpd_process], emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal in %s: %s",
			    procnames[eigrpd_process], emsg);

	if (eigrpd_process == PROC_MAIN)
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

#define NUM_LOGS	4
const char *
log_sockaddr(void *vp)
{
	static char	 buf[NUM_LOGS][NI_MAXHOST];
	static int	 round = 0;
	struct sockaddr	*sa = vp;

	round = (round + 1) % NUM_LOGS;

	if (getnameinfo(sa, sa->sa_len, buf[round], NI_MAXHOST, NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf[round]);
}

const char *
log_in6addr(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;

	memset(&sa_in6, 0, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	memcpy(&sa_in6.sin6_addr, addr, sizeof(sa_in6.sin6_addr));

	recoverscope(&sa_in6);

	return (log_sockaddr(&sa_in6));
}

const char *
log_in6addr_scope(const struct in6_addr *addr, unsigned int ifindex)
{
	struct sockaddr_in6	sa_in6;

	memset(&sa_in6, 0, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	memcpy(&sa_in6.sin6_addr, addr, sizeof(sa_in6.sin6_addr));

	addscope(&sa_in6, ifindex);

	return (log_sockaddr(&sa_in6));
}

const char *
log_addr(int af, union eigrpd_addr *addr)
{
	static char	 buf[NUM_LOGS][INET6_ADDRSTRLEN];
	static int	 round = 0;

	switch (af) {
	case AF_INET:
		round = (round + 1) % NUM_LOGS;
		if (inet_ntop(AF_INET, &addr->v4, buf[round],
		    sizeof(buf[round])) == NULL)
			return ("???");
		return (buf[round]);
	case AF_INET6:
		return (log_in6addr(&addr->v6));
	default:
		break;
	}

	return ("???");
}

const char *
log_prefix(struct rt_node *rn)
{
	static char	buf[64];

	if (snprintf(buf, sizeof(buf), "%s/%u", log_addr(rn->eigrp->af,
	    &rn->prefix), rn->prefixlen) == -1)
		return ("???");

	return (buf);
}

const char *
opcode_name(uint8_t opcode)
{
	switch (opcode) {
	case EIGRP_OPC_UPDATE:
		return ("UPDATE");
	case EIGRP_OPC_REQUEST:
		return ("REQUEST");
	case EIGRP_OPC_QUERY:
		return ("QUERY");
	case EIGRP_OPC_REPLY:
		return ("REPLY");
	case EIGRP_OPC_HELLO:
		return ("HELLO");
	case EIGRP_OPC_PROBE:
		return ("PROBE");
	case EIGRP_OPC_SIAQUERY:
		return ("SIAQUERY");
	case EIGRP_OPC_SIAREPLY:
		return ("SIAREPLY");
	default:
		return ("UNKNOWN");
	}
}

const char *
af_name(int af)
{
	switch (af) {
	case AF_INET:
		return ("ipv4");
	case AF_INET6:
		return ("ipv6");
	default:
		return ("UNKNOWN");
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
	}
	/* NOTREACHED */
	return ("UNKNOWN");
}

const char *
dual_state_name(int state)
{
	switch (state) {
	case DUAL_STA_PASSIVE:
		return ("PASSIVE");
	case DUAL_STA_ACTIVE0:
		return ("ACTIVE(Oij=0)");
	case DUAL_STA_ACTIVE1:
		return ("ACTIVE(Oij=1)");
	case DUAL_STA_ACTIVE2:
		return ("ACTIVE(Oij=2)");
	case DUAL_STA_ACTIVE3:
		return ("ACTIVE(Oij=3)");
	default:
		return ("UNKNOWN");
	}
}

const char *
ext_proto_name(int proto)
{
	switch (proto) {
	case EIGRP_EXT_PROTO_IGRP:
		return ("IGRP");
	case EIGRP_EXT_PROTO_EIGRP:
		return ("EIGRP");
	case EIGRP_EXT_PROTO_STATIC:
		return ("Static");
	case EIGRP_EXT_PROTO_RIP:
		return ("RIP");
	case EIGRP_EXT_PROTO_HELLO:
		return ("HELLO");
	case EIGRP_EXT_PROTO_OSPF:
		return ("OSPF");
	case EIGRP_EXT_PROTO_ISIS:
		return ("ISIS");
	case EIGRP_EXT_PROTO_EGP:
		return ("EGP");
	case EIGRP_EXT_PROTO_BGP:
		return ("BGP");
	case EIGRP_EXT_PROTO_IDRP:
		return ("IDRP");
	case EIGRP_EXT_PROTO_CONN:
		return ("Connected");
	default:
		return ("UNKNOWN");
	}
}
