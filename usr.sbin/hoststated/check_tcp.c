/*	$OpenBSD: check_tcp.c,v 1.11 2007/01/20 16:32:10 pyr Exp $	*/

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
void	tcp_host_up(int, struct ctl_tcp_event *);
void	tcp_send_req(int, short, void *);
void	tcp_read_buf(int, short, void *);

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

	bcopy(&cte->table->timeout, &tv, sizeof(tv));
	if (connect(s, (struct sockaddr *)&cte->host->ss, len) == -1) {
		if (errno != EINPROGRESS)
			goto bad;
	} else
		cte->host->up = HOST_UP;
	event_set(&cte->ev, s, EV_TIMEOUT|EV_WRITE, tcp_write, cte);
	event_add(&cte->ev, &tv);
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
		if (err != 0)
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
		hce_notify_done(cte->host, "tcp_host_up: success");
		return;
	case CHECK_HTTP_CODE:
		cte->validate_read = NULL;
		cte->validate_close = check_http_code;
		break;
	case CHECK_HTTP_DIGEST:
		cte->validate_read = NULL;;
		cte->validate_close = check_http_digest;
		break;
	case CHECK_SEND_EXPECT:
		cte->validate_read = check_send_expect;
		cte->validate_close = check_send_expect;
		break;
	}

	if (cte->table->sendbuf != NULL) {
		cte->req = cte->table->sendbuf;
		event_again(&cte->ev, s, EV_TIMEOUT|EV_WRITE, tcp_send_req,
		    &cte->tv_start, &cte->table->timeout, cte);
		return;
	}

	log_debug("tcp_host_up: nothing to write");
	if ((cte->buf = buf_dynamic(SMALL_READ_BUF_SIZE, UINT_MAX)) == NULL)
		fatalx("tcp_host_up: cannot create dynamic buffer");
	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, tcp_read_buf,
	    &cte->tv_start, &cte->table->timeout, cte);
}

void
tcp_send_req(int s, short event, void *arg)
{
	struct ctl_tcp_event	*cte = arg;
	int		 	 bs;
	int		 	 len;

	if (event == EV_TIMEOUT) {
		cte->host->up = HOST_DOWN;
		hce_notify_done(cte->host, "tcp_send_req: timeout");
		return;
	}
	len = strlen(cte->req);
	do {
		bs = write(s, cte->req, len);
		if (bs == -1) {
			if (errno == EAGAIN || errno == EINTR)
				goto retry;
			log_warnx("tcp_send_req: cannot send request");
			cte->host->up = HOST_DOWN;
			hce_notify_done(cte->host, "tcp_send_req: write");
			return;
		}
		cte->req += bs;
		len -= bs;
	} while (len > 0);

	log_debug("tcp_send_req: write done");
	if ((cte->buf = buf_dynamic(SMALL_READ_BUF_SIZE, UINT_MAX)) == NULL)
		fatalx("tcp_send_req: cannot create dynamic buffer");
	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, tcp_read_buf,
	    &cte->tv_start, &cte->table->timeout, cte);
	return;

 retry:
	event_again(&cte->ev, s, EV_TIMEOUT|EV_WRITE, tcp_send_req,
	    &cte->tv_start, &cte->table->timeout, cte);
}

void
tcp_read_buf(int s, short event, void *arg)
{
	ssize_t			 br;
	char			 rbuf[SMALL_READ_BUF_SIZE];
	struct ctl_tcp_event	*cte = arg;

        if (event == EV_TIMEOUT) {
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "tcp_read_buf: timeout");
		return;
	}

	log_debug("reading");
	bzero(rbuf, sizeof(rbuf));
	br = read(s, rbuf, sizeof(rbuf) - 1);
	if (br == -1) {
		if (errno == EAGAIN || errno == EINTR)
			goto retry;
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "tcp_read_buf: read failed");
		return;
	} else if (br == 0) {
		cte->host->up = HOST_DOWN;
		(void)cte->validate_close(cte);
		close(cte->s);
		buf_free(cte->buf);
		if (cte->host->up == HOST_UP)
			hce_notify_done(cte->host, "check succeeded");
		else
			hce_notify_done(cte->host, "check failed");
		return;
	}
	buf_add(cte->buf, rbuf, br);
	if (cte->validate_read != NULL) {
		log_debug("calling check");
		if (cte->validate_read(cte) != 0)
			goto retry;

		close(cte->s);
		buf_free(cte->buf);
		if (cte->host->up == HOST_UP)
			hce_notify_done(cte->host, "check succeeded");
		else
			hce_notify_done(cte->host, "check failed");
		return;
	}
retry:
	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, tcp_read_buf,
	    &cte->tv_start, &cte->table->timeout, cte);

}
