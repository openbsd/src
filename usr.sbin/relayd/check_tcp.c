/*	$OpenBSD: check_tcp.c,v 1.45 2015/01/16 15:06:40 deraadt Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <limits.h>
#include <event.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fnmatch.h>
#include <sha1.h>

#include <openssl/ssl.h>

#include "relayd.h"

void	tcp_write(int, short, void *);
void	tcp_host_up(struct ctl_tcp_event *);
void	tcp_close(struct ctl_tcp_event *, int);
void	tcp_send_req(int, short, void *);
void	tcp_read_buf(int, short, void *);

int	check_http_code(struct ctl_tcp_event *);
int	check_http_digest(struct ctl_tcp_event *);
int	check_send_expect(struct ctl_tcp_event *);

void
check_tcp(struct ctl_tcp_event *cte)
{
	int			 s;
	socklen_t		 len;
	struct timeval		 tv;
	struct linger		 lng;
	int			 he = HCE_TCP_SOCKET_OPTION;

	switch (cte->host->conf.ss.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&cte->host->conf.ss)->sin_port =
			cte->table->conf.port;
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&cte->host->conf.ss)->sin6_port =
			cte->table->conf.port;
		break;
	}

	len = ((struct sockaddr *)&cte->host->conf.ss)->sa_len;

	if ((s = socket(cte->host->conf.ss.ss_family, SOCK_STREAM, 0)) == -1) {
		if (errno == EMFILE || errno == ENFILE)
			he = HCE_TCP_SOCKET_LIMIT;
		else
			he = HCE_TCP_SOCKET_ERROR;
		goto bad;
	}

	cte->s = s;

	bzero(&lng, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) == -1)
		goto bad;

	if (cte->host->conf.ttl > 0) {
		if (setsockopt(s, IPPROTO_IP, IP_TTL,
		    &cte->host->conf.ttl, sizeof(int)) == -1)
			goto bad;
	}

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;

	bcopy(&cte->table->conf.timeout, &tv, sizeof(tv));
	if (connect(s, (struct sockaddr *)&cte->host->conf.ss, len) == -1) {
		if (errno != EINPROGRESS) {
			he = HCE_TCP_CONNECT_FAIL;
			goto bad;
		}
	}

	cte->buf = NULL;
	cte->host->up = HOST_UP;
	event_del(&cte->ev);
	event_set(&cte->ev, s, EV_TIMEOUT|EV_WRITE, tcp_write, cte);
	event_add(&cte->ev, &tv);
	return;

bad:
	tcp_close(cte, HOST_DOWN);
	hce_notify_done(cte->host, he);
}

void
tcp_write(int s, short event, void *arg)
{
	struct ctl_tcp_event	*cte = arg;
	int			 err;
	socklen_t		 len;

	if (event == EV_TIMEOUT) {
		tcp_close(cte, HOST_DOWN);
		hce_notify_done(cte->host, HCE_TCP_CONNECT_TIMEOUT);
		return;
	}

	len = sizeof(err);
	if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len))
		fatal("tcp_write: getsockopt");
	if (err != 0) {
		tcp_close(cte, HOST_DOWN);
		hce_notify_done(cte->host, HCE_TCP_CONNECT_FAIL);
		return;
	}

	cte->host->up = HOST_UP;
	tcp_host_up(cte);
}

void
tcp_close(struct ctl_tcp_event *cte, int status)
{
	close(cte->s);
	cte->s = -1;
	if (status != 0)
		cte->host->up = status;
	if (cte->buf) {
		ibuf_free(cte->buf);
		cte->buf = NULL;
	}
}

void
tcp_host_up(struct ctl_tcp_event *cte)
{
	switch (cte->table->conf.check) {
	case CHECK_TCP:
		if (cte->table->conf.flags & F_TLS)
			break;
		tcp_close(cte, 0);
		hce_notify_done(cte->host, HCE_TCP_CONNECT_OK);
		return;
	case CHECK_HTTP_CODE:
		cte->validate_read = NULL;
		cte->validate_close = check_http_code;
		break;
	case CHECK_HTTP_DIGEST:
		cte->validate_read = NULL;
		cte->validate_close = check_http_digest;
		break;
	case CHECK_SEND_EXPECT:
		cte->validate_read = check_send_expect;
		cte->validate_close = check_send_expect;
		break;
	}

	if (cte->table->conf.flags & F_TLS) {
		ssl_transaction(cte);
		return;
	}

	if (cte->table->sendbuf != NULL) {
		cte->req = cte->table->sendbuf;
		event_again(&cte->ev, cte->s, EV_TIMEOUT|EV_WRITE, tcp_send_req,
		    &cte->tv_start, &cte->table->conf.timeout, cte);
		return;
	}

	if ((cte->buf = ibuf_dynamic(SMALL_READ_BUF_SIZE, UINT_MAX)) == NULL)
		fatalx("tcp_host_up: cannot create dynamic buffer");
	event_again(&cte->ev, cte->s, EV_TIMEOUT|EV_READ, tcp_read_buf,
	    &cte->tv_start, &cte->table->conf.timeout, cte);
}

void
tcp_send_req(int s, short event, void *arg)
{
	struct ctl_tcp_event	*cte = arg;
	int			 bs;
	int			 len;

	if (event == EV_TIMEOUT) {
		tcp_close(cte, HOST_DOWN);
		hce_notify_done(cte->host, HCE_TCP_WRITE_TIMEOUT);
		return;
	}
	len = strlen(cte->req);
	do {
		bs = write(s, cte->req, len);
		if (bs == -1) {
			if (errno == EAGAIN || errno == EINTR)
				goto retry;
			log_warn("%s: cannot send request", __func__);
			tcp_close(cte, HOST_DOWN);
			hce_notify_done(cte->host, HCE_TCP_WRITE_FAIL);
			return;
		}
		cte->req += bs;
		len -= bs;
	} while (len > 0);

	if ((cte->buf = ibuf_dynamic(SMALL_READ_BUF_SIZE, UINT_MAX)) == NULL)
		fatalx("tcp_send_req: cannot create dynamic buffer");
	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, tcp_read_buf,
	    &cte->tv_start, &cte->table->conf.timeout, cte);
	return;

 retry:
	event_again(&cte->ev, s, EV_TIMEOUT|EV_WRITE, tcp_send_req,
	    &cte->tv_start, &cte->table->conf.timeout, cte);
}

void
tcp_read_buf(int s, short event, void *arg)
{
	ssize_t			 br;
	char			 rbuf[SMALL_READ_BUF_SIZE];
	struct ctl_tcp_event	*cte = arg;

	if (event == EV_TIMEOUT) {
		tcp_close(cte, HOST_DOWN);
		hce_notify_done(cte->host, HCE_TCP_READ_TIMEOUT);
		return;
	}

	bzero(rbuf, sizeof(rbuf));
	br = read(s, rbuf, sizeof(rbuf) - 1);
	switch (br) {
	case -1:
		if (errno == EAGAIN || errno == EINTR)
			goto retry;
		tcp_close(cte, HOST_DOWN);
		hce_notify_done(cte->host, HCE_TCP_READ_FAIL);
		return;
	case 0:
		cte->host->up = HOST_DOWN;
		(void)cte->validate_close(cte);
		tcp_close(cte, 0);
		hce_notify_done(cte->host, cte->host->he);
		return;
	default:
		if (ibuf_add(cte->buf, rbuf, br) == -1)
			fatal("tcp_read_buf: buf_add error");
		if (cte->validate_read != NULL) {
			if (cte->validate_read(cte) != 0)
				goto retry;
			tcp_close(cte, 0);
			hce_notify_done(cte->host, cte->host->he);
			return;
		}
		break; /* retry */
	}
retry:
	event_again(&cte->ev, s, EV_TIMEOUT|EV_READ, tcp_read_buf,
	    &cte->tv_start, &cte->table->conf.timeout, cte);
}

int
check_send_expect(struct ctl_tcp_event *cte)
{
	u_char	*b;

	/*
	 * ensure string is nul-terminated.
	 */
	b = ibuf_reserve(cte->buf, 1);
	if (b == NULL)
		fatal("out of memory");
	*b = '\0';
	if (fnmatch(cte->table->conf.exbuf, cte->buf->buf, 0) == 0) {
		cte->host->he = HCE_SEND_EXPECT_OK;
		cte->host->up = HOST_UP;
		return (0);
	}
	cte->host->he = HCE_SEND_EXPECT_FAIL;
	cte->host->up = HOST_UNKNOWN;

	/*
	 * go back to original position.
	 */
	cte->buf->wpos--;
	return (1);
}

int
check_http_code(struct ctl_tcp_event *cte)
{
	char		*head;
	char		 scode[4];
	const char	*estr;
	u_char		*b;
	int		 code;
	struct host	*host;

	/*
	 * ensure string is nul-terminated.
	 */
	b = ibuf_reserve(cte->buf, 1);
	if (b == NULL)
		fatal("out of memory");
	*b = '\0';

	head = cte->buf->buf;
	host = cte->host;
	host->he = HCE_HTTP_CODE_ERROR;

	if (strncmp(head, "HTTP/1.1 ", strlen("HTTP/1.1 ")) &&
	    strncmp(head, "HTTP/1.0 ", strlen("HTTP/1.0 "))) {
		log_debug("%s: %s failed (cannot parse HTTP version)",
		    __func__, host->conf.name);
		host->up = HOST_DOWN;
		return (1);
	}
	head += strlen("HTTP/1.1 ");
	if (strlen(head) < 5) /* code + \r\n */ {
		host->up = HOST_DOWN;
		return (1);
	}
	(void)strlcpy(scode, head, sizeof(scode));
	code = strtonum(scode, 100, 999, &estr);
	if (estr != NULL) {
		log_debug("%s: %s failed (cannot parse HTTP code)",
		    __func__, host->conf.name);
		host->up = HOST_DOWN;
		return (1);
	}
	if (code != cte->table->conf.retcode) {
		log_debug("%s: %s failed (invalid HTTP code returned)",
		    __func__, host->conf.name);
		host->he = HCE_HTTP_CODE_FAIL;
		host->up = HOST_DOWN;
	} else {
		host->he = HCE_HTTP_CODE_OK;
		host->up = HOST_UP;
	}
	return (!(host->up == HOST_UP));
}

int
check_http_digest(struct ctl_tcp_event *cte)
{
	char		*head;
	u_char		*b;
	char		 digest[SHA1_DIGEST_STRING_LENGTH];
	struct host	*host;

	/*
	 * ensure string is nul-terminated.
	 */
	b = ibuf_reserve(cte->buf, 1);
	if (b == NULL)
		fatal("out of memory");
	*b = '\0';

	head = cte->buf->buf;
	host = cte->host;
	host->he = HCE_HTTP_DIGEST_ERROR;

	if ((head = strstr(head, "\r\n\r\n")) == NULL) {
		log_debug("%s: %s failed (no end of headers)",
		    __func__, host->conf.name);
		host->up = HOST_DOWN;
		return (1);
	}
	head += strlen("\r\n\r\n");

	digeststr(cte->table->conf.digest_type, head, strlen(head), digest);

	if (strcmp(cte->table->conf.digest, digest)) {
		log_warnx("%s: %s failed (wrong digest)",
		    __func__, host->conf.name);
		host->he = HCE_HTTP_DIGEST_FAIL;
		host->up = HOST_DOWN;
	} else {
		host->he = HCE_HTTP_DIGEST_OK;
		host->up = HOST_UP;
	}
	return (!(host->up == HOST_UP));
}
