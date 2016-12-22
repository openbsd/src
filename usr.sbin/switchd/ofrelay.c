/*	$OpenBSD: ofrelay.c,v 1.10 2016/12/22 15:31:43 rzalamena Exp $	*/

/*
 * Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <event.h>

#include "switchd.h"

void	 ofrelay_close(struct switch_connection *);
void	 ofrelay_event(int, short, void *);
int	 ofrelay_input(int, short, void *);
int	 ofrelay_output(int, short, void *);
void	 ofrelay_accept(int, short, void *);
void	 ofrelay_inflight_dec(struct switch_connection *, const char *);

void	*ofrelay_input_open(struct switch_connection *,
	    struct ibuf *, ssize_t *);
ssize_t	 ofrelay_input_close(struct switch_connection *,
	    struct ibuf *, ssize_t);
int	 ofrelay_input_done(struct switch_connection *, struct ibuf *);
int	 ofrelay_bufget(struct switch_connection *, struct ibuf *);
void	 ofrelay_bufput(struct switch_connection *, struct ibuf *);

volatile int	 ofrelay_sessions;
volatile int	 ofrelay_inflight;
static uint32_t	 ofrelay_conid;

void
ofrelay(struct privsep *ps, struct privsep_proc *p)
{
	struct switchd		*sc = ps->ps_env;
	struct switch_server	*srv = &sc->sc_server;

	log_info("listen on %s", print_host(&srv->srv_addr, NULL, 0));

	if ((srv->srv_fd = switchd_listen((struct sockaddr *)
	    &srv->srv_addr)) == -1)
		fatal("listen");
}

void
ofrelay_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct switchd		*sc = ps->ps_env;
	struct switch_server	*srv = &sc->sc_server;

	TAILQ_INIT(&sc->sc_conns);
	TAILQ_INIT(&sc->sc_clients);

	srv->srv_sc = sc;
	event_set(&srv->srv_ev, srv->srv_fd, EV_READ, ofrelay_accept, srv);
	event_add(&srv->srv_ev, NULL);
	evtimer_set(&srv->srv_evt, ofrelay_accept, srv);
}

void
ofrelay_close(struct switch_connection *con)
{
	struct switchd		*sc = con->con_sc;
	struct switch_server	*srv = con->con_srv;

	log_info("%s: connection %u.%u closed", __func__,
	    con->con_id, con->con_instance);

	if (event_initialized(&con->con_ev))
		event_del(&con->con_ev);

	TAILQ_REMOVE(&sc->sc_conns, con, con_entry);
	ofrelay_sessions--;

	switch_freetables(con);
	ofp_multipart_clear(con);
	switch_remove(con->con_sc, con->con_switch);
	msgbuf_clear(&con->con_wbuf);
	ibuf_release(con->con_rbuf);
	close(con->con_fd);

	ofrelay_inflight_dec(con, __func__);

	/* Some file descriptors are available again. */
	if (evtimer_pending(&srv->srv_evt, NULL)) {
		DPRINTF("%s: accepting again", __func__);
		evtimer_del(&srv->srv_evt);
		event_add(&srv->srv_ev, NULL);
	}

	free(con);
}

void
ofrelay_event(int fd, short event, void *arg)
{
	struct switch_connection	*con = arg;
	const char			*error = NULL;

	event_add(&con->con_ev, NULL);
	if (event & EV_TIMEOUT) {
		error = "timeout";
		goto fail;
	}
	if (event & EV_WRITE) {
		if (ofrelay_output(fd, event, arg) == -1) {
			error = "write";
			goto fail;
		}
	}
	if (event & EV_READ) {
		if (ofrelay_input(fd, event, arg) == -1) {
			error = "input";
			goto fail;
		}
	}

 fail:
	if (error != NULL) {
		DPRINTF("%s: %s error", __func__, error);
		ofrelay_close(con);
	}
}

int
ofrelay_input(int fd, short event, void *arg)
{
	struct switch_connection	*con = arg;
	struct ibuf		*ibuf = con->con_rbuf;
	ssize_t			 len, rlen;
	size_t			 hlen;
	void			*buf;
	struct ofp_header	*oh = NULL;

	if ((buf = ofrelay_input_open(con, ibuf, &len)) == NULL) {
		log_warn("%s: fd %d, failed to get buffer", __func__, fd);
		return (-1);
	}

	/* If we got a new buffer, read the complete openflow header first */
	if ((hlen = ibuf_length(ibuf)) < sizeof(struct ofp_header)) {
		oh = (struct ofp_header *)ibuf_data(ibuf);
		len = sizeof(*oh) - hlen;
	}

	if ((rlen = read(fd, buf, len)) == -1) {
		if (errno == EINTR || errno == EAGAIN)
			return (0);
		log_warn("%s: fd %d, failed to read", __func__, fd);
		return (-1);
	}

	print_hex(buf, 0, rlen);

	DPRINTF("%s: connection %u.%u read %zd bytes", __func__,
	    con->con_id, con->con_instance, rlen);

	if ((len = ofrelay_input_close(con, ibuf, rlen)) == -1)
		return (-1);
	else if (rlen == 0) {
		/* connection closed */
		ofrelay_input_done(con, ibuf);
		return (-1);
	}

	/* After we verified the openflow header, set the size accordingly */
	if (oh != NULL && (hlen + rlen) == sizeof(*oh)) {
		switch (oh->oh_version) {
		case OFP_V_1_0:
		case OFP_V_1_1:
		case OFP_V_1_2:
		case OFP_V_1_3:
			break;
		default:
			DPRINTF("%s: fd %d, openflow version 0x%02x length %d"
			    " not supported", __func__, fd, oh->oh_version,
			    ntohs(oh->oh_length));
			return (-1);
		}

		len = ntohs(oh->oh_length);
		if (len == sizeof(*oh)) {
			len = 0;
		} else if (len > (ssize_t)ibuf->size) {
			log_debug("%s: buffer too big: %zu > %zd", __func__,
			    len, ibuf->size);
			return (-1);
		} else
			ibuf_setmax(ibuf, len);
	}

	if (len > 0)
		return (len);

	return (ofrelay_input_done(con, ibuf));
}

void
ofrelay_write(struct switch_connection *con, struct ibuf *buf)
{
	ibuf_close(&con->con_wbuf, buf);

	event_del(&con->con_ev);
	event_set(&con->con_ev, con->con_fd, EV_READ|EV_WRITE,
	    ofrelay_event, con);
	event_add(&con->con_ev, NULL);
}

void *
ofrelay_input_open(struct switch_connection *con,
    struct ibuf *buf, ssize_t *len)
{
	ssize_t	 left;

	left = buf->max - buf->wpos;
	if (left == 0) {
		ofrelay_bufget(con, buf);
		left = buf->max;
	}
	if (len)
		*len = left;

	return (buf->buf + buf->wpos);
}

ssize_t
ofrelay_input_close(struct switch_connection *con,
    struct ibuf *buf, ssize_t len)
{
	if (len <= 0)
		return (0);
	if (buf->wpos + len > buf->max)
		return (-1);
	buf->wpos += len;

	return (buf->max - buf->wpos);
}

int
ofrelay_input_done(struct switch_connection *con, struct ibuf *buf)
{
	struct switch_control	*sw;

	if (buf->wpos == 0) {
		ofrelay_bufput(con, buf);
		return (0);
	}

	sw = con->con_switch;
	log_debug("%s: connection %u.%u: %ld bytes from switch %u", __func__,
	    con->con_id, con->con_instance, buf->wpos,
	    sw == NULL ? 0 : sw->sw_id);

	print_hex(buf->buf, 0, buf->wpos);

	if (ofp_input(con, buf) == -1)
		return (-1);

	/* Update read buffer */
	if (ofrelay_bufget(con, buf) == -1)
		return (-1);

	return (0);
}

int
ofrelay_bufget(struct switch_connection *con, struct ibuf *buf)
{
	ibuf_reset(buf);

	/* Should be a static buffer with maximum size */
	if (ibuf_setmax(buf, SWITCHD_MSGBUF_MAX) == -1)
		fatalx("%s: invalid buffer", __func__);

	return (0);
}

void
ofrelay_bufput(struct switch_connection *con, struct ibuf *buf)
{
	/* Just reset the buffer */
	ofrelay_bufget(con, buf);
}

int
ofrelay_output(int fd, short event, void *arg)
{
	struct switch_connection	*con = arg;
	struct msgbuf			*wbuf = &con->con_wbuf;

	if (!wbuf->queued) {
		event_del(&con->con_ev);
		event_set(&con->con_ev, con->con_fd, EV_READ,
		    ofrelay_event, con);
		event_add(&con->con_ev, NULL);
		return (0);
	}

	if (ibuf_write(wbuf) <= 0 && errno != EAGAIN)
		return (-1);

	return (0);
}

void
ofrelay_accept(int fd, short event, void *arg)
{
	struct switch_server	*srv = arg;
	struct sockaddr_storage	 ss;
	int			 s;
	socklen_t		 slen;

	event_add(&srv->srv_ev, NULL);
	if (event & EV_TIMEOUT)
		return;

	slen = sizeof(ss);
	if ((s = accept4_reserve(fd, (struct sockaddr *)&ss, &slen,
	    SOCK_NONBLOCK|SOCK_CLOEXEC, SWITCHD_FD_RESERVE,
	    &ofrelay_inflight)) == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			struct timeval evtpause = { 1, 0 };

			event_del(&srv->srv_ev);
			evtimer_add(&srv->srv_evt, &evtpause);
			log_warn("%s: deferring connections", __func__);
		} else
			log_warn("%s accept", __func__);
		return;
	}
	ss.ss_len = slen;

	(void)ofrelay_attach(srv, s, (struct sockaddr *)&ss);
}

void
ofrelay_inflight_dec(struct switch_connection *con, const char *why)
{
	if (con != NULL) {
		/* the flight already left inflight mode. */
		if (con->con_inflight == 0)
			return;
		con->con_inflight = 0;
	}

	/* the file was never opened, thus this was an inflight client. */
	ofrelay_inflight--;
	DPRINTF("%s: inflight decremented, now %d, %s",
	    __func__, ofrelay_inflight, why);
}

int
ofrelay_attach(struct switch_server *srv, int s, struct sockaddr *sa)
{
	struct switchd			*sc = srv->srv_sc;
	struct privsep			*ps = &sc->sc_ps;
	struct switch_connection	*con = NULL;
	socklen_t			 slen;
	int				 ret = -1;

	if (ofrelay_sessions >= SWITCHD_MAX_SESSIONS) {
		log_warn("too many sessions");
		goto done;
	}

	if ((con = calloc(1, sizeof(*con))) == NULL) {
		log_warn("calloc");
		goto done;
	}

	con->con_fd = s;
	con->con_inflight = 1;
	con->con_sc = sc;
	con->con_id = ++ofrelay_conid;
	con->con_instance = ps->ps_instance + 1;
	con->con_srv = srv;
	con->con_state = OFP_STATE_CLOSED;
	SLIST_INIT(&con->con_mmlist);
	TAILQ_INIT(&con->con_stlist);

	memcpy(&con->con_peer, sa, sa->sa_len);
	con->con_port = htons(socket_getport(&con->con_peer));

	if (getsockname(s, (struct sockaddr *)&con->con_local, &slen) == -1) {
		/* Set local sockaddr to AF_UNSPEC */
		memset(&con->con_local, 0, sizeof(con->con_local));
	}

	log_info("%s: new connection %u.%u",
	    __func__, con->con_id, con->con_instance);

	ofrelay_sessions++;
	TAILQ_INSERT_TAIL(&sc->sc_conns, con, con_entry);

	if ((con->con_rbuf = ibuf_static()) == NULL ||
	    ofrelay_bufget(con, con->con_rbuf) == -1) {
		log_warn("ibuf");
		goto done;
	}
	msgbuf_init(&con->con_wbuf);
	con->con_wbuf.fd = s;

	memset(&con->con_ev, 0, sizeof(con->con_ev));
	event_set(&con->con_ev, con->con_fd, EV_READ, ofrelay_event, con);
	event_add(&con->con_ev, NULL);

	ret = ofp_open(ps, con);

 done:
	if (con == NULL)
		close(s);
	ofrelay_inflight_dec(con, __func__);
	if (ret != 0)
		ofrelay_close(con);

	return (ret);
}
