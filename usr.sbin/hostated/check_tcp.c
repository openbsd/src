/*	$OpenBSD: check_tcp.c,v 1.2 2006/12/16 12:42:14 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <net/if.h>
#include <limits.h>
#include <event.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "hostated.h"

int
check_tcp(struct host *host, struct table *table)
{
	int	sock;

	if ((sock = tcp_connect(host, table)) <= 0)
		return (sock);
	close(sock);
	return (HOST_UP);
}

int
tcp_connect(struct host *host, struct table *table)
{
	int		s;
	socklen_t	len;
	struct timeval	tv;
	struct sockaddr	sa;
	fd_set		fdset;

	switch (host->ss.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&host->ss)->sin_port =
			htons(table->port);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&host->ss)->sin6_port =
			htons(table->port);
		break;
	}

	len = ((struct sockaddr *)&host->ss)->sa_len;

	if ((s = socket(host->ss.ss_family, SOCK_STREAM, 0)) == -1)
		fatal("check_tcp: cannot create socket");

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		fatal("check_tcp: cannot set non blocking socket");

	if (connect(s, (struct sockaddr *)&host->ss, len) == -1) {
		if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
			close(s);
			return (HOST_DOWN);
		}
	} else
		return (s);

	tv.tv_sec = table->timeout / 1000;
	tv.tv_usec = table->timeout % 1000;
	FD_ZERO(&fdset);
	FD_SET(s, &fdset);

	/* XXX This needs to be rewritten */
	switch (select(s + 1, NULL, &fdset, NULL, &tv)) {
	case -1:
		if (errno != EINTR)
			fatal("check_tcp: select");
		else
			return (HOST_UNKNOWN);
	case 0:
		close(s);
		return (HOST_DOWN);
	default:
		if (getpeername(s, &sa, &len) == -1) {
			if (errno == ENOTCONN) {
				close(s);
				return (HOST_DOWN);
			} else {
				log_debug("check_tcp: unknown peername");
				close(s);
				return (HOST_UNKNOWN);
			}
		} else
			return (s);
	}
	return (HOST_UNKNOWN);
}
