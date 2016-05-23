/*	$OpenBSD: socket.c,v 1.2 2016/05/23 17:43:42 renato Exp $ */

/*
 * Copyright (c) 2016 Renato Westphal <renato@openbsd.org>
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
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
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ldpd.h"
#include "ldpe.h"
#include "log.h"

extern struct ldpd_sysdep	 sysdep;

void
sock_set_recvbuf(int fd)
{
	int	bsize;

	bsize = 65535;
	while (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bsize,
	    sizeof(bsize)) == -1)
		bsize /= 2;
}

int
sock_set_reuse(int fd, int enable)
{
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,
	    sizeof(int)) < 0) {
		log_warn("%s: error setting SO_REUSEADDR", __func__);
		return (-1);
	}

	return (0);
}

int
sock_set_ipv4_mcast_ttl(int fd, uint8_t ttl)
{
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
	    (char *)&ttl, sizeof(ttl)) < 0) {
		log_warn("%s: error setting IP_MULTICAST_TTL to %d",
		    __func__, ttl);
		return (-1);
	}

	return (0);
}

int
sock_set_ipv4_tos(int fd, int tos)
{
	if (setsockopt(fd, IPPROTO_IP, IP_TOS, (int *)&tos, sizeof(tos)) < 0) {
		log_warn("%s: error setting IP_TOS to 0x%x", __func__, tos);
		return (-1);
	}

	return (0);
}

int
sock_set_ipv4_recvif(int fd, int enable)
{
	if (setsockopt(fd, IPPROTO_IP, IP_RECVIF, &enable,
	    sizeof(enable)) < 0) {
		log_warn("%s: error setting IP_RECVIF", __func__);
		return (-1);
	}
	return (0);
}

int
sock_set_ipv4_mcast(struct iface *iface)
{
	struct if_addr		*if_addr;

	if_addr = LIST_FIRST(&iface->addr_list);
	if (!if_addr)
		return (0);

	if (setsockopt(global.ldp_disc_socket, IPPROTO_IP, IP_MULTICAST_IF,
	    &if_addr->addr.s_addr, sizeof(if_addr->addr.s_addr)) < 0) {
		log_warn("%s: error setting IP_MULTICAST_IF, interface %s",
		    __func__, iface->name);
		return (-1);
	}

	return (0);
}

int
sock_set_ipv4_mcast_loop(int fd)
{
	uint8_t	loop = 0;

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
	    (char *)&loop, sizeof(loop)) < 0) {
		log_warn("%s: error setting IP_MULTICAST_LOOP", __func__);
		return (-1);
	}

	return (0);
}
