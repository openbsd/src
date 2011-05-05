/*	$OpenBSD: imsg_util.c,v 1.2 2011/05/05 12:17:10 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <event.h>

#include "iked.h"
#include "ikev2.h"

void
imsg_event_add(struct imsgev *iev)
{
	if (iev->handler == NULL) {
		imsg_flush(&iev->ibuf);
		return;
	}

	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, u_int16_t type, u_int32_t peerid,
    pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) == -1)
		return (ret);
	imsg_event_add(iev);
	return (ret);
}

int
imsg_composev_event(struct imsgev *iev, u_int16_t type, u_int32_t peerid,
    pid_t pid, int fd, const struct iovec *iov, int iovcnt)
{
	int	ret;

	if ((ret = imsg_composev(&iev->ibuf, type, peerid,
	    pid, fd, iov, iovcnt)) == -1)
		return (ret);
	imsg_event_add(iev);
	return (ret);
}

int
imsg_compose_proc(struct iked *env, enum privsep_procid id,
    u_int16_t type, int fd, void *data, u_int16_t datalen)
{
	return (imsg_compose_event(&env->sc_ievs[id],
	    type, -1, 0, fd, data, datalen));
}

int
imsg_composev_proc(struct iked *env, enum privsep_procid id,
    u_int16_t type, int fd, const struct iovec *iov, int iovcnt)
{
	return (imsg_composev_event(&env->sc_ievs[id],
	    type, -1, 0, fd, iov, iovcnt));
}

int
imsg_forward_proc(struct iked *env, struct imsg *imsg,
    enum privsep_procid id)
{
	return (imsg_compose_proc(env, id, imsg->hdr.type,
	    imsg->fd, imsg->data, IMSG_DATA_SIZE(imsg)));
}

void
imsg_flush_proc(struct iked *env, enum privsep_procid id)
{
	imsg_flush(&env->sc_ievs[id].ibuf);
}

/*
 * Extending the imsg buffer API for internal use
 */

int
ibuf_cat(struct ibuf *dst, struct ibuf *src)
{
	return (ibuf_add(dst, src->buf, ibuf_size(src)));
}

void
ibuf_zero(struct ibuf *buf)
{
	memset(buf->buf, 0, buf->wpos);
}

struct ibuf *
ibuf_new(void *data, size_t len)
{
	struct ibuf	*buf;

	if ((buf = ibuf_dynamic(len,
	    IKED_MSGBUF_MAX)) == NULL)
		return (NULL);

	ibuf_zero(buf);

	if (data == NULL && len) {
		if (ibuf_advance(buf, len) == NULL) {
			ibuf_free(buf);
			return (NULL);
		}
	} else {
		if (ibuf_add(buf, data, len) != 0) {
			ibuf_free(buf);
			return (NULL);
		}
	}

	return (buf);
}

struct ibuf *
ibuf_static(void)
{
	struct ibuf	*buf;

	if ((buf = ibuf_open(IKED_MSGBUF_MAX)) == NULL)
		return (NULL);

	ibuf_zero(buf);

	return (buf);
}

void *
ibuf_advance(struct ibuf *buf, size_t len)
{
	void	*ptr;

	if ((ptr = ibuf_reserve(buf, len)) != NULL)
		memset(ptr, 0, len);

	return (ptr);
}

void
ibuf_release(struct ibuf *buf)
{
	if (buf == NULL)
		return;
	if (buf->buf != NULL)
		free(buf->buf);
	free(buf);
}

size_t
ibuf_length(struct ibuf *buf)
{
	if (buf == NULL || buf->buf == NULL)
		return (0);
	return (ibuf_size(buf));
}

u_int8_t *
ibuf_data(struct ibuf *buf)
{
	return (ibuf_seek(buf, 0, 0));
}

void *
ibuf_getdata(struct ibuf *buf, size_t len)
{
	void	*data;

	if ((data = ibuf_seek(buf, buf->rpos, len)) == NULL)
		return (NULL);
	buf->rpos += len;

	return (data);
}

struct ibuf *
ibuf_get(struct ibuf *buf, size_t len)
{
	void		*data;

	if ((data = ibuf_getdata(buf, len)) == NULL)
		return (NULL);

	return (ibuf_new(data, len));
}

struct ibuf *
ibuf_dup(struct ibuf *buf)
{
	if (buf == NULL)
		return (NULL);
	return (ibuf_new(ibuf_data(buf), ibuf_size(buf)));
}

struct ibuf *
ibuf_random(size_t len)
{
	struct ibuf	*buf;
	void		*ptr;

	if ((buf = ibuf_open(len)) == NULL)
		return (NULL);
	if ((ptr = ibuf_reserve(buf, len)) == NULL) {
		ibuf_free(buf);
		return (NULL);
	}
	arc4random_buf(ptr, len);
	return (buf);
}

int
ibuf_setsize(struct ibuf *buf, size_t len)
{
	if (len > buf->size)
		return (-1);
	buf->wpos = len;
	return (0);
}

int
ibuf_prepend(struct ibuf *buf, void *data, size_t len)
{
	struct ibuf	*new;

	/* Swap buffers (we could also use memmove here) */
	if ((new = ibuf_new(data, len)) == NULL)
		return (-1);
	if (ibuf_cat(new, buf) == -1) {
		ibuf_release(new);
		return (-1);
	}
	free(buf->buf);
	memcpy(buf, new, sizeof(*buf));
	free(new);

	return (0);
}
