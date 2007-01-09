/*	$OpenBSD: check_tcp.c,v 1.8 2007/01/09 00:45:32 deraadt Exp $	*/

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
#include <stdlib.h>
#include <errno.h>

#include "hoststated.h"

void	tcp_write(int, short, void *);
void	tcp_host_up(int s, struct ctl_tcp_event *);

void
check_tcp(struct ctl_tcp_event *cte)
{
	int			 s;
	int			 type;
	socklen_t		 len;
	struct timeval		 tv;
	struct linger		 lng;

	switch (cte->host->ss.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&cte->host->ss)->sin_port =
			cte->table->port;
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&cte->host->ss)->sin6_port =
			cte->table->port;
		break;
	}

	len = ((struct sockaddr *)&cte->host->ss)->sa_len;

	if ((s = socket(cte->host->ss.ss_family, SOCK_STREAM, 0)) == -1)
		goto bad;

	bzero(&lng, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) == -1)
		goto bad;

	type = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &type, sizeof(type)) == -1)
		goto bad;

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;

	if (connect(s, (struct sockaddr *)&cte->host->ss, len) == -1) {
		if (errno != EINPROGRESS)
			goto bad;
	} else {
		cte->host->up = HOST_UP;
		tcp_host_up(s, cte);
		return;
	}
	bcopy(&cte->table->timeout, &tv, sizeof(tv));
	event_once(s, EV_TIMEOUT|EV_WRITE, tcp_write, cte, &tv);
	return;
bad:
	close(s);
	cte->host->up = HOST_DOWN;
	hce_notify_done(cte->host, "check_tcp: cannot connect");
}

void
tcp_write(int s, short event, void *arg)
{
	struct ctl_tcp_event	*cte = arg;
	int			 err;
	socklen_t		 len;

	if (event == EV_TIMEOUT) {
		log_debug("tcp_write: connect timed out");
		cte->host->up = HOST_DOWN;
	} else {
		len = sizeof(err);
		if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len))
			fatal("tcp_write: getsockopt");
		if (err)
			cte->host->up = HOST_DOWN;
		else
			cte->host->up = HOST_UP;
	}
	if (cte->host->up == HOST_UP)
		tcp_host_up(s, cte);
	else {
		close(s);
		hce_notify_done(cte->host, "connect failed");
	}
}

void
tcp_host_up(int s, struct ctl_tcp_event *cte)
{
	cte->s = s;

	switch (cte->table->check) {
	case CHECK_TCP:
		close(s);
		hce_notify_done(cte->host, "tcp_write: success");
		break;
	case CHECK_HTTP_CODE:
	case CHECK_HTTP_DIGEST:
		send_http_request(cte);
		break;
	case CHECK_SEND_EXPECT:
		start_send_expect(cte);
		break;
	default:
		fatalx("tcp_write: unhandled check type");
	}
}
