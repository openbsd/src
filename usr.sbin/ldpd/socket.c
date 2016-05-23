/*	$OpenBSD: socket.c,v 1.5 2016/05/23 18:43:28 renato Exp $ */

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

extern struct ldpd_conf		*ldpd_conf;
extern struct ldpd_sysdep	 sysdep;

int
ldp_create_socket(enum socket_type type)
{
	int			 fd, domain, proto;
	struct sockaddr_in	 local_sa;
	int			 opt;

	/* create socket */
	switch (type) {
	case LDP_SOCKET_DISC:
	case LDP_SOCKET_EDISC:
		domain = SOCK_DGRAM;
		proto = IPPROTO_UDP;
		break;
	case LDP_SOCKET_SESSION:
		domain = SOCK_STREAM;
		proto = IPPROTO_TCP;
		break;
	default:
		fatalx("ldp_create_socket: unknown socket type");
	}
	fd = socket(AF_INET, domain | SOCK_NONBLOCK | SOCK_CLOEXEC, proto);
	if (fd == -1) {
		log_warn("%s: error creating socket", __func__);
		return (-1);
	}

	/* bind to a local address/port */
	memset(&local_sa, 0, sizeof(local_sa));
	local_sa.sin_family = AF_INET;
	local_sa.sin_len = sizeof(struct sockaddr_in);
	local_sa.sin_port = htons(LDP_PORT);
	switch (type) {
	case LDP_SOCKET_DISC:
		/* listen on all addresses */
		break;
	case LDP_SOCKET_EDISC:
	case LDP_SOCKET_SESSION:
		local_sa.sin_addr = ldpd_conf->trans_addr;
		if (sock_set_bindany(fd, 1) == -1) {
			close(fd);
			return (-1);
		}
		break;
	}
	if (sock_set_reuse(fd, 1) == -1) {
		close(fd);
		return (-1);
	}
	if (bind(fd, (struct sockaddr *)&local_sa, local_sa.sin_len) == -1) {
		log_warn("%s: error binding socket", __func__);
		close(fd);
		return (-1);
	}

	/* set options */
	if (sock_set_ipv4_tos(fd, IPTOS_PREC_INTERNETCONTROL) == -1) {
		close(fd);
		return (-1);
	}
	if (type == LDP_SOCKET_DISC) {
		if (sock_set_ipv4_mcast_ttl(fd,
		    IP_DEFAULT_MULTICAST_TTL) == -1) {
			close(fd);
			return (-1);
		}
		if (sock_set_ipv4_mcast_loop(fd) == -1) {
			close(fd);
			return (-1);
		}
	}
	if (type == LDP_SOCKET_DISC || type == LDP_SOCKET_EDISC) {
		if (sock_set_ipv4_recvif(fd, 1) == -1) {
			close(fd);
			return (-1);
		}
	}
	switch (type) {
	case LDP_SOCKET_DISC:
	case LDP_SOCKET_EDISC:
		sock_set_recvbuf(fd);
		break;
	case LDP_SOCKET_SESSION:
		if (listen(fd, LDP_BACKLOG) == -1)
			log_warn("%s: error listening on socket", __func__);

		opt = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_MD5SIG, &opt,
		    sizeof(opt)) == -1) {
			if (errno == ENOPROTOOPT) {	/* system w/o md5sig */
				log_warnx("md5sig not available, disabling");
				sysdep.no_md5sig = 1;
			} else {
				close(fd);
				return (-1);
			}
		}
		break;
	}

	return (fd);
}

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
sock_set_bindany(int fd, int enable)
{
	if (setsockopt(fd, SOL_SOCKET, SO_BINDANY, &enable,
	    sizeof(int)) < 0) {
		log_warn("%s: error setting SO_BINDANY", __func__);
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
