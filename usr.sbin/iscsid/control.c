/*	$OpenBSD: control.c,v 1.2 2010/09/24 10:32:57 claudio Exp $ */

/*
 * Copyright (c) 2010 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "log.h"

struct control {
	TAILQ_ENTRY(control)	entry;
	struct event		ev;
	struct pduq		channel;
	int			fd;
} *control_state;

TAILQ_HEAD(, control)	controls;

#define	CONTROL_BACKLOG	5

void	control_accept(int, short, void *);
void	control_close(struct control *);
void	control_dispatch(int, short, void *);
struct pdu *control_getpdu(char *, size_t);

int
control_init(char *path)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask;

	if ((control_state = calloc(1, sizeof(*control_state))) == NULL) {
		log_warn("control_init: calloc");
		return (-1);
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("control_init: socket");
		return (-1);
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

	if (unlink(path) == -1)
		if (errno != ENOENT) {
			log_warn("control_init: unlink %s", path);
			close(fd);
			return (-1);
		}

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("control_init: bind: %s", path);
		close(fd);
		umask(old_umask);
		return (-1);
	}
	umask(old_umask);

	if (chmod(path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) == -1) {
		log_warn("control_init: chmod");
		close(fd);
		(void)unlink(path);
		return (-1);
	}

	socket_setblockmode(fd, 1);
	control_state->fd = fd;
	TAILQ_INIT(&control_state->channel);
	TAILQ_INIT(&controls);

	return (0);
}

void
control_cleanup(char *path)
{
	struct control	*c;

	if (path)
		unlink(path);

	while ((c = TAILQ_FIRST(&controls)) != NULL) {
		TAILQ_REMOVE(&controls, c, entry);
		control_close(c);
	}
	control_close(control_state);
}

int
control_listen(void)
{
	if (listen(control_state->fd, CONTROL_BACKLOG) == -1) {
		log_warn("control_listen: listen");
		return (-1);
	}

	event_set(&control_state->ev, control_state->fd, EV_READ | EV_PERSIST,
	    control_accept, NULL);
	event_add(&control_state->ev, NULL);

	return (0);
}

/* ARGSUSED */
void
control_accept(int listenfd, short event, void *bula)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct control		*c;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno != EWOULDBLOCK && errno != EINTR)
			log_warn("control_accept");
		return;
	}

	if ((c = malloc(sizeof(struct control))) == NULL) {
		log_warn("control_accept");
		close(connfd);
		return;
	}

	TAILQ_INIT(&c->channel);
	c->fd = connfd;
	event_set(&c->ev, connfd, EV_READ, control_dispatch, c);
	event_add(&c->ev, NULL);
}

void
control_close(struct control *c)
{
	close(c->fd);
	event_del(&c->ev);
	pdu_free_queue(&c->channel);
	free(c);
}

static char	cbuf[CONTROL_READ_SIZE];

/* ARGSUSED */
void
control_dispatch(int fd, short event, void *bula)
{
	struct iovec iov[PDU_MAXIOV];
	struct msghdr msg;
	struct control *c = bula;
	struct pdu *pdu;
	ssize_t	 n;
	unsigned int niov = 0;
	short flags = EV_READ;

	if (event & EV_TIMEOUT) {
		log_debug("control connection (fd %d) timed out.", fd);
		control_close(c);
		return;
	}
	if (event & EV_READ) {
		if ((n = recv(fd, cbuf, sizeof(cbuf), 0)) == -1 &&
		    !(errno == EAGAIN || errno == EINTR)) {
			control_close(c);
			return;
		}
		if (n == 0) {
			control_close(c);
			return;
		}
		pdu = control_getpdu(cbuf, n);
		if (!pdu) {
			log_debug("control connection (fd %d) bad msg.", fd);
			control_close(c);
			return;
		}
		iscsid_ctrl_dispatch(c, pdu);
	}
	if (event & EV_WRITE) {
		if ((pdu = TAILQ_FIRST(&c->channel)) != NULL) {
			TAILQ_REMOVE(&c->channel, pdu, entry);

			for (niov = 0; niov < PDU_MAXIOV; niov++) {
				iov[niov].iov_base = pdu->iov[niov].iov_base;
				iov[niov].iov_len = pdu->iov[niov].iov_len;
			}
			bzero(&msg, sizeof(msg));
			msg.msg_iov = iov;
			msg.msg_iovlen = niov;
			if (sendmsg(fd, &msg, 0) == -1 &&
			    !(errno == EAGAIN || errno == ENOBUFS)) {
				control_close(c);
				return;
			}
		}
	}
	if (!TAILQ_EMPTY(&c->channel))
		flags |= EV_WRITE;

	event_del(&c->ev);
	event_set(&c->ev, fd, flags, control_dispatch, c);
	event_add(&c->ev, NULL);
}

struct pdu *
control_getpdu(char *buf, size_t len)
{
	struct pdu *p;
	struct ctrlmsghdr *cmh;
	void *data;
	size_t n;
	int i;

	if (len < sizeof(*cmh))
		return NULL;

	if (!(p = pdu_new()))
		return NULL;

	n = sizeof(*cmh);
	cmh = pdu_alloc(n);
	bcopy(buf, cmh, n);
	buf += n;
	len -= n;

	if (pdu_addbuf(p, cmh, n, 0)) {
		free(cmh);
fail:
		pdu_free(p);
		return NULL;
	}

	for (i = 0; i < 3; i++) {
		n = cmh->len[i];
		if (n == 0)
			continue;
		if (PDU_LEN(n) > len)
			goto fail;
		if (!(data = pdu_alloc(n)))
			goto fail;
		bcopy(buf, data, n);
		if (pdu_addbuf(p, data, n, i + 1)) {
			free(data);
			goto fail;
		}
		buf += PDU_LEN(n);
		len -= PDU_LEN(n);
	}

	return p;
}

int
control_queue(void *ch, struct pdu *pdu)
{
	struct control *c = ch;

	TAILQ_INSERT_TAIL(&c->channel, pdu, entry);

	event_del(&c->ev);
	event_set(&c->ev, c->fd, EV_READ|EV_WRITE, control_dispatch, c);
	event_add(&c->ev, NULL);

	return 0;
}

int
control_compose(void *ch, u_int16_t type, void *buf, size_t len)
{
	struct pdu *pdu;
	struct ctrlmsghdr *cmh;
	void *ptr;

	if (PDU_LEN(len) > CONTROL_READ_SIZE - PDU_LEN(sizeof(*cmh)))
		return -1;
	if ((pdu = pdu_new()) == NULL)
		return -1;
	if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
		goto fail;
	bzero(cmh, sizeof(*cmh));
	cmh->type = type;
	cmh->len[0] = len;
	pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
	if (len > 0) {
		if ((ptr = pdu_alloc(len)) == NULL)
			goto fail;
		bcopy(buf, ptr, len);
		pdu_addbuf(pdu, ptr, len, 1);
	}

	return control_queue(ch, pdu);
fail:
	pdu_free(pdu);
	return -1;
}
