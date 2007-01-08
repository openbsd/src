/*	$OpenBSD: check_http.c,v 1.5 2007/01/08 13:37:26 reyk Exp $	*/
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
#include <sha1.h>
#include <limits.h>
#include <event.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>

#include "hostated.h"

void	check_http_code(struct ctl_tcp_event *);
void	check_http_digest(struct ctl_tcp_event *);
void	http_read(int, short, void *);

void
check_http_code(struct ctl_tcp_event *cte)
{
	char		*head;
	char		 scode[4];
	const char	*estr;
	int		 code;

	head = cte->buf->buf;
	if (strncmp(head, "HTTP/1.1 ", strlen("HTTP/1.1 ")) &&
	    strncmp(head, "HTTP/1.0 ", strlen("HTTP/1.0 "))) {
		log_debug("check_http_code: cannot parse HTTP version");
		cte->host->up = HOST_DOWN;
		return;
	}
	head += strlen("HTTP/1.1 ");
	if (strlen(head) < 5) /* code + \r\n */ {
		cte->host->up = HOST_DOWN;
		return;
	}
	strlcpy(scode, head, sizeof(scode));
	code = strtonum(scode, 100, 999, &estr);
	if (estr != NULL) {
		log_debug("check_http_code: cannot parse HTTP code");
		cte->host->up = HOST_DOWN;
		return;
	}
	if (code != cte->table->retcode) {
		log_debug("check_http_code: invalid HTTP code returned");
		cte->host->up = HOST_DOWN;
	} else
		cte->host->up = HOST_UP;
}

void
check_http_digest(struct ctl_tcp_event *cte)
{
	char	*head;
	char	 digest[(SHA1_DIGEST_LENGTH*2)+1];

	head = cte->buf->buf;
	if ((head = strstr(head, "\r\n\r\n")) == NULL) {
		log_debug("check_http_digest: host %u no end of headers",
		    cte->host->id);
		cte->host->up = HOST_DOWN;
		return;
	}
	head += strlen("\r\n\r\n");
	SHA1Data(head, strlen(head), digest);

	if (strcmp(cte->table->digest, digest)) {
		log_warnx("check_http_digest: wrong digest for host %u",
		    cte->host->id);
		cte->host->up = HOST_DOWN;
	} else
		cte->host->up = HOST_UP;
}

void
http_read(int s, short event, void *arg)
{
	ssize_t			 br;
	char			 rbuf[SMALL_READ_BUF_SIZE];
	struct timeval		 tv;
	struct timeval		 tv_now;
	struct ctl_tcp_event	*cte = arg;

	if (event == EV_TIMEOUT) {
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "http_read: timeout");
		return;
	}
	br = read(s, rbuf, sizeof(rbuf));
	if (br == 0) {
		cte->host->up = HOST_DOWN;
		switch (cte->table->check) {
		case CHECK_HTTP_CODE:
			check_http_code(cte);
			break;
		case CHECK_HTTP_DIGEST:
			check_http_digest(cte);
			break;
		default:
			fatalx("http_read: unhandled check type");
		}
		buf_free(cte->buf);
		hce_notify_done(cte->host, "http_read: connection closed");
	} else if (br == -1) {
		cte->host->up = HOST_DOWN;
		buf_free(cte->buf);
		hce_notify_done(cte->host, "http_read: read failed");
	} else {
		buf_add(cte->buf, rbuf, br);
		bcopy(&cte->table->timeout, &tv, sizeof(tv));
		if (gettimeofday(&tv_now, NULL))
			fatal("send_http_request: gettimeofday");
		timersub(&tv_now, &cte->tv_start, &tv_now);
		timersub(&tv, &tv_now, &tv);
		event_once(s, EV_READ|EV_TIMEOUT, http_read, cte, &tv);
	}
}

void
send_http_request(struct ctl_tcp_event *cte)
{
	int		 bs;
	int		 pos;
	int		 len;
	char		*req;
	struct timeval	 tv;
	struct timeval	 tv_now;

	switch (cte->table->check) {
	case CHECK_HTTP_CODE:
		asprintf(&req, "HEAD %s HTTP/1.0\r\n\r\n",
		    cte->table->path);
		break;
	case CHECK_HTTP_DIGEST:
		asprintf(&req, "GET %s HTTP/1.0\r\n\r\n",
		    cte->table->path);
		break;
	default:
		fatalx("send_http_request: unhandled check type");
	}
	if (req == NULL)
		fatal("out of memory");
	pos = 0;
	len = strlen(req);
	/*
	 * write all at once for now.
	 */
	do {
		bs = write(cte->s, req + pos, len);
		if (bs <= 0) {
			log_warnx("send_http_request: cannot send request");
			cte->host->up = HOST_DOWN;
			hce_notify_done(cte->host, "send_http_request: write");
			free(req);
			return;
		}
		pos += bs;
		len -= bs;
	} while (len > 0);
	free(req);
	if ((cte->buf = buf_dynamic(SMALL_READ_BUF_SIZE, UINT_MAX)) == NULL)
		fatalx("send_http_request: cannot create dynamic buffer");

	bcopy(&cte->table->timeout, &tv, sizeof(tv));
	if (gettimeofday(&tv_now, NULL))
		fatal("send_http_request: gettimeofday");
	timersub(&tv_now, &cte->tv_start, &tv_now);
	timersub(&tv, &tv_now, &tv);
	event_once(cte->s, EV_READ|EV_TIMEOUT, http_read, cte, &tv);
}
