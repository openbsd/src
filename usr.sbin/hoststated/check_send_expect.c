/*	$OpenBSD: check_send_expect.c,v 1.4 2007/01/11 18:05:08 reyk Exp $ */
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

#include <net/if.h>
#include <limits.h>
#include <event.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <errno.h>

#include "hoststated.h"

void	se_validate(struct ctl_tcp_event *);
void	se_read(int, short, void *);

void
se_validate(struct ctl_tcp_event *cte)
{
	u_char	*b;

	/*
	 * ensure string is nul-terminated.
	 */
	b = buf_reserve(cte->buf, 1);
	if (b == NULL)
		fatal("out of memory");
	*b = '\0';
	if (fnmatch(cte->table->exbuf, cte->buf->buf, 0) == 0)
		cte->host->up = HOST_UP;
	else
		cte->host->up = HOST_UNKNOWN;

	/*
	 * go back to original position.
	 */
	cte->buf->wpos--;
}

void
se_read(int s, short event, void *arg)
{
	ssize_t			 br;
	char			 rbuf[SMALL_READ_BUF_SIZE];
	struct ctl_tcp_event	*cte = arg;

	if (event == EV_TIMEOUT) {
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: timeout");
		return;
	}

	br = read(s, rbuf, sizeof(rbuf));
	if (br == -1) {
		if (errno == EAGAIN || errno == EINTR)
			goto retry;
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: read failed");
		return;
	} else if (br == 0) {
		cte->host->up = HOST_DOWN;
		se_validate(cte);
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: connection closed");
		return;
	}

	buf_add(cte->buf, rbuf, br);
	se_validate(cte);
	if (cte->host->up == HOST_UP) {
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: done");
		return;
	}

 retry:
	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, se_read,
	    &cte->tv_start, &cte->table->timeout, cte);
}

void
start_send_expect(int s, short event, void *arg)
{
	struct ctl_tcp_event	*cte = (struct ctl_tcp_event *)arg;
	int			 bs;
	int			 pos;
	int			 len;
	char			*req;

	req = cte->table->sendbuf;
	pos = 0;
	len = strlen(req);
	if (len) {
		do {
			bs = write(cte->s, req + pos, len);
			if (bs == -1) {
				if (errno == EAGAIN || errno == EINTR)
					goto retry;
				log_warnx("send_se_data: cannot send");
				cte->host->up = HOST_DOWN;
				hce_notify_done(cte->host,
				    "start_send_expect: write");
				return;
			}
			pos += bs;
			len -= bs;
		} while (len > 0);
	}

	if ((cte->buf = buf_dynamic(SMALL_READ_BUF_SIZE, UINT_MAX)) == NULL)
		fatalx("send_se_data: cannot create dynamic buffer");

	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, se_read,
	    &cte->tv_start, &cte->table->timeout, cte);
	return;

 retry:
	event_again(&cte->ev, s, EV_TIMEOUT|EV_WRITE, start_send_expect,
	    &cte->tv_start, &cte->table->timeout, cte);
}
