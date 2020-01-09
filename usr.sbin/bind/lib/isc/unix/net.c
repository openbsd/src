/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: net.c,v 1.10 2020/01/09 13:45:33 florian Exp $ */

#include <config.h>

#include <sys/types.h>

#if defined(HAVE_SYS_SYSCTL_H)
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif
#include <sys/sysctl.h>
#endif
#include <sys/uio.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <isc/log.h>
#include <isc/msgs.h>
#include <isc/net.h>
#include <isc/netdb.h>
#include <isc/once.h>
#include <isc/strerror.h>
#include <isc/string.h>
#include <isc/util.h>

#ifndef ISC_SOCKADDR_LEN_T
#define ISC_SOCKADDR_LEN_T unsigned int
#endif

/*%
 * Definitions about UDP port range specification.  This is a total mess of
 * portability variants: some use sysctl (but the sysctl names vary), some use
 * system-specific interfaces, some have the same interface for IPv4 and IPv6,
 * some separate them, etc...
 */

/*%
 * The last resort defaults: use all non well known port space
 */
#ifndef ISC_NET_PORTRANGELOW
#define ISC_NET_PORTRANGELOW 1024
#endif	/* ISC_NET_PORTRANGELOW */
#ifndef ISC_NET_PORTRANGEHIGH
#define ISC_NET_PORTRANGEHIGH 65535
#endif	/* ISC_NET_PORTRANGEHIGH */

#ifdef HAVE_SYSCTLBYNAME

/*%
 * sysctl variants
 */
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__DragonFly__)
#define USE_SYSCTL_PORTRANGE
#define SYSCTL_V4PORTRANGE_LOW	"net.inet.ip.portrange.hifirst"
#define SYSCTL_V4PORTRANGE_HIGH	"net.inet.ip.portrange.hilast"
#define SYSCTL_V6PORTRANGE_LOW	"net.inet.ip.portrange.hifirst"
#define SYSCTL_V6PORTRANGE_HIGH	"net.inet.ip.portrange.hilast"
#endif

#ifdef __NetBSD__
#define USE_SYSCTL_PORTRANGE
#define SYSCTL_V4PORTRANGE_LOW	"net.inet.ip.anonportmin"
#define SYSCTL_V4PORTRANGE_HIGH	"net.inet.ip.anonportmax"
#define SYSCTL_V6PORTRANGE_LOW	"net.inet6.ip6.anonportmin"
#define SYSCTL_V6PORTRANGE_HIGH	"net.inet6.ip6.anonportmax"
#endif

#else /* !HAVE_SYSCTLBYNAME */

#ifdef __OpenBSD__
#define USE_SYSCTL_PORTRANGE
#define SYSCTL_V4PORTRANGE_LOW	{ CTL_NET, PF_INET, IPPROTO_IP, \
				  IPCTL_IPPORT_HIFIRSTAUTO }
#define SYSCTL_V4PORTRANGE_HIGH	{ CTL_NET, PF_INET, IPPROTO_IP, \
				  IPCTL_IPPORT_HILASTAUTO }
/* Same for IPv6 */
#define SYSCTL_V6PORTRANGE_LOW	SYSCTL_V4PORTRANGE_LOW
#define SYSCTL_V6PORTRANGE_HIGH	SYSCTL_V4PORTRANGE_HIGH
#endif

#endif /* HAVE_SYSCTLBYNAME */

# if defined(WANT_IPV6)
static isc_once_t 	once_ipv6only = ISC_ONCE_INIT;

static isc_once_t 	once_ipv6pktinfo = ISC_ONCE_INIT;
# endif /* WANT_IPV6 */

#ifndef ISC_CMSG_IP_TOS
#ifdef __APPLE__
#define ISC_CMSG_IP_TOS 0	/* As of 10.8.2. */
#else /* ! __APPLE__ */
#define ISC_CMSG_IP_TOS 1
#endif /* ! __APPLE__ */
#endif /* ! ISC_CMSG_IP_TOS */

static isc_once_t 	once = ISC_ONCE_INIT;

static isc_result_t	ipv4_result = ISC_R_NOTFOUND;
static isc_result_t	ipv6_result = ISC_R_NOTFOUND;
static isc_result_t	unix_result = ISC_R_NOTFOUND;
static isc_result_t	ipv6only_result = ISC_R_NOTFOUND;
static isc_result_t	ipv6pktinfo_result = ISC_R_NOTFOUND;
static unsigned int	dscp_result =
		ISC_NET_DSCPSETV4 | ISC_NET_DSCPRECVV4 | ISC_NET_DSCPPKTV4 |
		ISC_NET_DSCPSETV6 | ISC_NET_DSCPRECVV6 | ISC_NET_DSCPPKTV6;

static isc_result_t
try_proto(int domain) {
	int s;
	isc_result_t result = ISC_R_SUCCESS;
	char strbuf[ISC_STRERRORSIZE];

	s = socket(domain, SOCK_STREAM | SOCK_DNS, 0);
	if (s == -1) {
		switch (errno) {
#ifdef EAFNOSUPPORT
		case EAFNOSUPPORT:
#endif
#ifdef EPFNOSUPPORT
		case EPFNOSUPPORT:
#endif
#ifdef EPROTONOSUPPORT
		case EPROTONOSUPPORT:
#endif
#ifdef EINVAL
		case EINVAL:
#endif
			return (ISC_R_NOTFOUND);
		default:
			isc__strerror(errno, strbuf, sizeof(strbuf));
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "socket() %s: %s",
					 isc_msgcat_get(isc_msgcat,
							ISC_MSGSET_GENERAL,
							ISC_MSG_FAILED,
							"failed"),
					 strbuf);
			return (ISC_R_UNEXPECTED);
		}
	}

#ifdef WANT_IPV6
	if (domain == PF_INET6) {
		struct sockaddr_in6 sin6;
		unsigned int len;

		/*
		 * Check to see if IPv6 is broken, as is common on Linux.
		 */
		len = sizeof(sin6);
		if (getsockname(s, (struct sockaddr *)&sin6, (void *)&len) < 0)
		{
			isc_log_write(isc_lctx, ISC_LOGCATEGORY_GENERAL,
				      ISC_LOGMODULE_SOCKET, ISC_LOG_ERROR,
				      "retrieving the address of an IPv6 "
				      "socket from the kernel failed.");
			isc_log_write(isc_lctx, ISC_LOGCATEGORY_GENERAL,
				      ISC_LOGMODULE_SOCKET, ISC_LOG_ERROR,
				      "IPv6 is not supported.");
			result = ISC_R_NOTFOUND;
		} else {
			if (len == sizeof(struct sockaddr_in6))
				result = ISC_R_SUCCESS;
			else {
				isc_log_write(isc_lctx,
					      ISC_LOGCATEGORY_GENERAL,
					      ISC_LOGMODULE_SOCKET,
					      ISC_LOG_ERROR,
					      "IPv6 structures in kernel and "
					      "user space do not match.");
				isc_log_write(isc_lctx,
					      ISC_LOGCATEGORY_GENERAL,
					      ISC_LOGMODULE_SOCKET,
					      ISC_LOG_ERROR,
					      "IPv6 is not supported.");
				result = ISC_R_NOTFOUND;
			}
		}
	}
#endif

	(void)close(s);

	return (result);
}

static void
initialize_action(void) {
	ipv4_result = try_proto(PF_INET);
#ifdef WANT_IPV6
	ipv6_result = try_proto(PF_INET6);
#endif
}

static void
initialize(void) {
	RUNTIME_CHECK(isc_once_do(&once, initialize_action) == ISC_R_SUCCESS);
}

isc_result_t
isc_net_probeipv4(void) {
	initialize();
	return (ipv4_result);
}

isc_result_t
isc_net_probeipv6(void) {
	initialize();
	return (ipv6_result);
}

isc_result_t
isc_net_probeunix(void) {
	initialize();
	return (unix_result);
}

#ifdef WANT_IPV6
static void
try_ipv6only(void) {
#ifdef IPV6_V6ONLY
	int s, on;
	char strbuf[ISC_STRERRORSIZE];
#endif
	isc_result_t result;

	result = isc_net_probeipv6();
	if (result != ISC_R_SUCCESS) {
		ipv6only_result = result;
		return;
	}

#ifndef IPV6_V6ONLY
	ipv6only_result = ISC_R_NOTFOUND;
	return;
#else
	/* check for TCP sockets */
	s = socket(PF_INET6, SOCK_STREAM | SOCK_DNS, 0);
	if (s == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "socket() %s: %s",
				 isc_msgcat_get(isc_msgcat,
						ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED,
						"failed"),
				 strbuf);
		ipv6only_result = ISC_R_UNEXPECTED;
		return;
	}

	on = 1;
	if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
		ipv6only_result = ISC_R_NOTFOUND;
		goto close;
	}

	close(s);

	/* check for UDP sockets */
	s = socket(PF_INET6, SOCK_DGRAM | SOCK_DNS, 0);
	if (s == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "socket() %s: %s",
				 isc_msgcat_get(isc_msgcat,
						ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED,
						"failed"),
				 strbuf);
		ipv6only_result = ISC_R_UNEXPECTED;
		return;
	}

	on = 1;
	if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
		ipv6only_result = ISC_R_NOTFOUND;
		goto close;
	}

	ipv6only_result = ISC_R_SUCCESS;

close:
	close(s);
	return;
#endif /* IPV6_V6ONLY */
}

static void
initialize_ipv6only(void) {
	RUNTIME_CHECK(isc_once_do(&once_ipv6only,
				  try_ipv6only) == ISC_R_SUCCESS);
}
#endif /* WANT_IPV6 */

#ifdef WANT_IPV6
static void
try_ipv6pktinfo(void) {
	int s, on;
	char strbuf[ISC_STRERRORSIZE];
	isc_result_t result;
	int optname;

	result = isc_net_probeipv6();
	if (result != ISC_R_SUCCESS) {
		ipv6pktinfo_result = result;
		return;
	}

	/* we only use this for UDP sockets */
	s = socket(PF_INET6, SOCK_DGRAM | SOCK_DNS, IPPROTO_UDP);
	if (s == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "socket() %s: %s",
				 isc_msgcat_get(isc_msgcat,
						ISC_MSGSET_GENERAL,
						ISC_MSG_FAILED,
						"failed"),
				 strbuf);
		ipv6pktinfo_result = ISC_R_UNEXPECTED;
		return;
	}

#ifdef IPV6_RECVPKTINFO
	optname = IPV6_RECVPKTINFO;
#else
	optname = IPV6_PKTINFO;
#endif
	on = 1;
	if (setsockopt(s, IPPROTO_IPV6, optname, &on, sizeof(on)) < 0) {
		ipv6pktinfo_result = ISC_R_NOTFOUND;
		goto close;
	}

	ipv6pktinfo_result = ISC_R_SUCCESS;

close:
	close(s);
	return;
}

static void
initialize_ipv6pktinfo(void) {
	RUNTIME_CHECK(isc_once_do(&once_ipv6pktinfo,
				  try_ipv6pktinfo) == ISC_R_SUCCESS);
}
#endif /* WANT_IPV6 */

isc_result_t
isc_net_probe_ipv6only(void) {
#ifdef WANT_IPV6
	initialize_ipv6only();
#else
	ipv6only_result = ISC_R_NOTFOUND;
#endif
	return (ipv6only_result);
}

isc_result_t
isc_net_probe_ipv6pktinfo(void) {
#ifdef WANT_IPV6
	initialize_ipv6pktinfo();
#else
	ipv6pktinfo_result = ISC_R_NOTFOUND;
#endif
	return (ipv6pktinfo_result);
}

unsigned int
isc_net_probedscp(void) {
	return (dscp_result);
}

void
isc_net_disableipv4(void) {
	initialize();
	if (ipv4_result == ISC_R_SUCCESS)
		ipv4_result = ISC_R_DISABLED;
}

void
isc_net_disableipv6(void) {
	initialize();
	if (ipv6_result == ISC_R_SUCCESS)
		ipv6_result = ISC_R_DISABLED;
}

void
isc_net_enableipv4(void) {
	initialize();
	if (ipv4_result == ISC_R_DISABLED)
		ipv4_result = ISC_R_SUCCESS;
}

void
isc_net_enableipv6(void) {
	initialize();
	if (ipv6_result == ISC_R_DISABLED)
		ipv6_result = ISC_R_SUCCESS;
}
