/*	$OpenBSD: check_send_expect.c,v 1.2 2007/01/08 20:46:18 reyk Exp $ */
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

#include "hostated.h"

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
		cte->host->up = HOST_DOWN;

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
	struct timeval		 tv;
	struct timeval		 tv_now;
	struct ctl_tcp_event	*cte = arg;

	if (event == EV_TIMEOUT) {
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: timeout");
		return;
	}
	br = read(s, rbuf, sizeof(rbuf));
	log_debug("se_read: %d bytes read", br);
	if (br == 0) {
		cte->host->up = HOST_DOWN;
		se_validate(cte);
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: connection closed");
	} else if (br == -1) {
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "se_read: read failed");
	} else {
		buf_add(cte->buf, rbuf, br);
		bcopy(&cte->table->timeout, &tv, sizeof(tv));
		if (gettimeofday(&tv_now, NULL))
			fatal("se_read: gettimeofday");
		timersub(&tv_now, &cte->tv_start, &tv_now);
		timersub(&tv, &tv_now, &tv);
		se_validate(cte);
		if (cte->host->up == HOST_UP) {
			buf_free(cte->buf);
			hce_notify_done(cte->host, NULL);
		} else
			event_once(s, EV_READ|EV_TIMEOUT, se_read, cte, &tv);
	}
}

void
start_send_expect(struct ctl_tcp_event *cte)
{
	int		 bs;
	int		 pos;
	int		 len;
	char		*req;
	struct timeval	 tv;
	struct timeval	 tv_now;

	req = cte->table->sendbuf;
	pos = 0;
	len = strlen(req);
	if (len) {
		do {
			bs = write(cte->s, req + pos, len);
			if (bs <= 0) {
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

	log_debug("start_send_expect: reading");

	bcopy(&cte->table->timeout, &tv, sizeof(tv));
	if (gettimeofday(&tv_now, NULL))
		fatal("start_send_expect: gettimeofday");
	timersub(&tv_now, &cte->tv_start, &tv_now);
	timersub(&tv, &tv_now, &tv);
	event_once(cte->s, EV_READ|EV_TIMEOUT, se_read, cte, &tv);
}
