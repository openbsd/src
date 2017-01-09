/*	$OpenBSD: util.c,v 1.6 2017/01/09 14:49:22 reyk Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/un.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <event.h>

#include "switchd.h"

void
socket_set_blockmode(int fd, enum blockmodes bm)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	if (bm == BM_NONBLOCK)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}

int
accept4_reserve(int sockfd, struct sockaddr *addr, socklen_t *addrlen,
    int flags, int reserve, volatile int *counter)
{
	int	 fd;

	if (getdtablecount() + reserve + *counter >= getdtablesize()) {
		errno = EMFILE;
		return (-1);
	}

	if ((fd = accept4(sockfd, addr, addrlen, flags)) != -1) {
		(*counter)++;
		DPRINTF("%s: inflight incremented, now %d",__func__, *counter);
	}

	return (fd);
}

in_port_t
socket_getport(struct sockaddr_storage *ss)
{
	switch (ss->ss_family) {
	case AF_INET:
		return (ntohs(((struct sockaddr_in *)ss)->sin_port));
	case AF_INET6:
		return (ntohs(((struct sockaddr_in6 *)ss)->sin6_port));
	default:
		return (0);
	}

	/* NOTREACHED */
	return (0);
}

int
socket_setport(struct sockaddr_storage *ss, in_port_t port)
{
	switch (ss->ss_family) {
	case AF_INET:
		((struct sockaddr_in *)ss)->sin_port = ntohs(port);
		return (0);
	case AF_INET6:
		((struct sockaddr_in6 *)ss)->sin6_port = ntohs(port);
		return (0);
	default:
		return (-1);
	}

	/* NOTREACHED */
	return (-1);
}

int
sockaddr_cmp(struct sockaddr *a, struct sockaddr *b, int prefixlen)
{
	struct sockaddr_in	*a4, *b4;
	struct sockaddr_in6	*a6, *b6;
	struct sockaddr_un	*au, *bu;
	uint32_t		 av[4], bv[4], mv[4];

	if (a->sa_family == AF_UNSPEC || b->sa_family == AF_UNSPEC)
		return (0);
	else if (a->sa_family > b->sa_family)
		return (1);
	else if (a->sa_family < b->sa_family)
		return (-1);

	if (prefixlen == -1)
		memset(&mv, 0xff, sizeof(mv));

	switch (a->sa_family) {
	case AF_INET:
		a4 = (struct sockaddr_in *)a;
		b4 = (struct sockaddr_in *)b;

		av[0] = a4->sin_addr.s_addr;
		bv[0] = b4->sin_addr.s_addr;
		if (prefixlen != -1)
			mv[0] = prefixlen2mask(prefixlen);

		if ((av[0] & mv[0]) > (bv[0] & mv[0]))
			return (1);
		if ((av[0] & mv[0]) < (bv[0] & mv[0]))
			return (-1);
		break;
	case AF_INET6:
		a6 = (struct sockaddr_in6 *)a;
		b6 = (struct sockaddr_in6 *)b;

		memcpy(&av, &a6->sin6_addr.s6_addr, 16);
		memcpy(&bv, &b6->sin6_addr.s6_addr, 16);
		if (prefixlen != -1)
			prefixlen2mask6(prefixlen, mv);

		if ((av[3] & mv[3]) > (bv[3] & mv[3]))
			return (1);
		if ((av[3] & mv[3]) < (bv[3] & mv[3]))
			return (-1);
		if ((av[2] & mv[2]) > (bv[2] & mv[2]))
			return (1);
		if ((av[2] & mv[2]) < (bv[2] & mv[2]))
			return (-1);
		if ((av[1] & mv[1]) > (bv[1] & mv[1]))
			return (1);
		if ((av[1] & mv[1]) < (bv[1] & mv[1]))
			return (-1);
		if ((av[0] & mv[0]) > (bv[0] & mv[0]))
			return (1);
		if ((av[0] & mv[0]) < (bv[0] & mv[0]))
			return (-1);
		break;
	case AF_UNIX:
		au = (struct sockaddr_un *)a;
		bu = (struct sockaddr_un *)b;
		return (strcmp(au->sun_path, bu->sun_path));
	}

	return (0);
}

uint32_t
prefixlen2mask(uint8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	if (prefixlen > 32)
		prefixlen = 32;

	return (htonl(0xffffffff << (32 - prefixlen)));
}

struct in6_addr *
prefixlen2mask6(uint8_t prefixlen, uint32_t *mask)
{
	static struct in6_addr  s6;
	int			i;

	if (prefixlen > 128)
		prefixlen = 128;

	bzero(&s6, sizeof(s6));
	for (i = 0; i < prefixlen / 8; i++)
		s6.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		s6.s6_addr[prefixlen / 8] = 0xff00 >> i;

	memcpy(mask, &s6, sizeof(s6));

	return (&s6);
}

const char *
print_ether(const uint8_t *ea)
{
	static char	 sbuf[SWITCHD_CYCLE_BUFFERS]
			    [ETHER_ADDR_LEN * 2 + 5 + 1];
	static int	 idx = 0;
	size_t		 len;
	char		*buf;

	buf = sbuf[idx];
	len = sizeof(sbuf[idx]);
	if (++idx >= SWITCHD_CYCLE_BUFFERS)
		idx = 0;

	snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
	    ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);

	return (buf);
}

const char *
print_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	static char	sbuf[SWITCHD_CYCLE_BUFFERS][NI_MAXHOST + 7];
	struct sockaddr_un *un;
	static int	idx = 0;
	char		pbuf[7];
	in_port_t	port;

	if (buf == NULL) {
		buf = sbuf[idx];
		len = sizeof(sbuf[idx]);
		if (++idx >= SWITCHD_CYCLE_BUFFERS)
			idx = 0;
	}

	if (ss->ss_family == AF_UNSPEC) {
		strlcpy(buf, "any", len);
		return (buf);
	} else if (ss->ss_family == AF_UNIX) {
		un = (struct sockaddr_un *)ss;
		strlcpy(buf, un->sun_path, len);
		return (buf);
	}

	if (getnameinfo((struct sockaddr *)ss, ss->ss_len,
	    buf, len, NULL, 0, NI_NUMERICHOST) != 0) {
		buf[0] = '\0';
		return (NULL);
	}

	if ((port = socket_getport(ss)) != 0) {
		snprintf(pbuf, sizeof(pbuf), ":%d", port);
		(void)strlcat(buf, pbuf, len);
	}

	return (buf);
}

const char *
print_map(unsigned int type, struct constmap *map)
{
	unsigned int		 i;
	static char		 buf[SWITCHD_CYCLE_BUFFERS][32];
	static int		 idx = 0;
	const char		*name = NULL;

	if (idx >= SWITCHD_CYCLE_BUFFERS)
		idx = 0;
	bzero(buf[idx], sizeof(buf[idx]));

	for (i = 0; map[i].cm_name != NULL; i++) {
		if (map[i].cm_type == type) {
			name = map[i].cm_name;
			break;
		}
	}

	if (name == NULL)
		snprintf(buf[idx], sizeof(buf[idx]), "<%u>", type);
	else
		strlcpy(buf[idx], name, sizeof(buf[idx]));

	return (buf[idx++]);
}

void
getmonotime(struct timeval *tv)
{
	struct timespec	 ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		fatal("clock_gettime");

	TIMESPEC_TO_TIMEVAL(tv, &ts);
}

void
print_debug(const char *emsg, ...)
{
	va_list	 ap;

	if (log_getverbose() > 2) {
		va_start(ap, emsg);
		vfprintf(stderr, emsg, ap);
		va_end(ap);
	}
}

void
print_verbose(const char *emsg, ...)
{
	va_list	 ap;

	if (log_getverbose()) {
		va_start(ap, emsg);
		vfprintf(stderr, emsg, ap);
		va_end(ap);
	}
}

void
print_hex(uint8_t *buf, off_t offset, size_t length)
{
	unsigned int	 i;

	if (log_getverbose() < 3 || !length)
		return;

	for (i = 0; i < length; i++) {
		if (i && (i % 4) == 0) {
			if ((i % 32) == 0)
				print_debug("\n");
			else
				print_debug(" ");
		}
		print_debug("%02x", buf[offset + i]);
	}
	print_debug("\n");
}

int
parsehostport(const char *str, struct sockaddr *sa, socklen_t salen)
{
	char		 buf[NI_MAXHOST + NI_MAXSERV + 8], *servp, *nodep;
	struct addrinfo	 hints, *ai;

	if (strlcpy(buf, str, sizeof(buf)) >= sizeof(buf))
		return (-1);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (buf[0] == '[' &&
	    (servp = strchr(buf, ']')) != NULL &&
	    (*(servp + 1) == '\0' || *(servp + 1) == ':')) {
		hints.ai_family = AF_INET6;
		hints.ai_flags = AI_NUMERICHOST;
		nodep = buf + 1;
		*servp++ = '\0';
	} else {
		nodep = buf;
		servp = strrchr(nodep, ':');
	}
	if (servp != NULL) {
		*servp = '\0';
		servp++;
	} else
		servp = NULL;

	if (getaddrinfo(nodep, servp, &hints, &ai) != 0)
		return (-1);

	if (salen < ai->ai_addrlen) {
		freeaddrinfo(ai);
		return (-1);
	}
	memset(sa, 0, salen);
	memcpy(sa, ai->ai_addr, ai->ai_addrlen);

	return (0);
}
