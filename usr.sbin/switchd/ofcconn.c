/*	$OpenBSD: ofcconn.c,v 1.13 2019/06/28 13:32:51 deraadt Exp $	*/

/*
 * Copyright (c) 2016 YASUOKA Masahiko <yasuoka@openbsd.org>
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
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/ofp.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <imsg.h>

#include "ofp10.h"
#include "types.h"
#include "switchd.h"

int	 ofcconn_dispatch_parent(int, struct privsep_proc *, struct imsg *);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	ofcconn_dispatch_parent },
	{ "control",	PROC_CONTROL,	NULL },
};

struct ofcconn;

/* OpenFlow Switch */
struct ofsw {
	int			 os_fd;
	char			*os_name;
	int			 os_write_ready;
	TAILQ_HEAD(,ofcconn)	 os_ofcconns;
	struct event		 os_evio;
	TAILQ_ENTRY(ofsw)	 os_next;
};
TAILQ_HEAD(, ofsw)	 ofsw_list = TAILQ_HEAD_INITIALIZER(ofsw_list);

/* OpenFlow Channel Connection */
struct ofcconn {
	struct ofsw		*oc_sw;
	char			*oc_name;
	struct sockaddr_storage	 oc_peer;
	int			 oc_sock;
	int			 oc_write_ready;
	int			 oc_connected;
	int			 oc_conn_fails;
	struct ibuf		*oc_buf;
	TAILQ_ENTRY(ofcconn)	 oc_next;
	struct event		 oc_evsock;
	struct event		 oc_evtimer;
};

struct ofsw	*ofsw_create(const char *, int);
void		 ofsw_close(struct ofsw *);
void		 ofsw_free(struct ofsw *);
void		 ofsw_on_io(int, short, void *);
int		 ofsw_write(struct ofsw *, struct ofcconn *);
int		 ofsw_ofc_write_ready(struct ofsw *);
void		 ofsw_reset_event_handlers(struct ofsw *);
int		 ofsw_new_ofcconn(struct ofsw *, struct switch_address *);
int		 ofcconn_connect(struct ofcconn *);
void		 ofcconn_on_sockio(int, short, void *);
void		 ofcconn_connect_again(struct ofcconn *);
void		 ofcconn_on_timer(int, short, void *);
void		 ofcconn_reset_event_handlers(struct ofcconn *);
void		 ofcconn_io_fail(struct ofcconn *);
void		 ofcconn_close(struct ofcconn *);
void		 ofcconn_free(struct ofcconn *);
void		 ofcconn_shutdown_all(void);
int		 ofcconn_send_hello(struct ofcconn *);
void		 ofcconn_run(struct privsep *, struct privsep_proc *, void *);

void
ofcconn(struct privsep *ps, struct privsep_proc *p)
{
	p->p_shutdown = ofcconn_shutdown;
	proc_run(ps, p, procs, nitems(procs), ofcconn_run, NULL);
}

void
ofcconn_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	/*
	 * pledge in the ofcconn process:
 	 * stdio - for malloc and basic I/O including events.
	 * inet - for socket operations and OpenFlow connections.
	 * recvfd - for receiving new sockets on reload.
	 */
	if (pledge("stdio inet recvfd", NULL) == -1)
		fatal("pledge");
}

void
ofcconn_shutdown(void)
{
	struct ofsw	*e, *t;

	TAILQ_FOREACH_SAFE(e, &ofsw_list, os_next, t) {
		ofsw_close(e);
		ofsw_free(e);
	}
}

int
ofcconn_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct ofsw			*os;
	struct switch_client		 swc;
	struct sockaddr_un		*un;

	switch (imsg->hdr.type) {
	case IMSG_CTL_CONNECT:
		if (IMSG_DATA_SIZE(imsg) < sizeof(swc)) {
			log_warnx("%s: IMSG_CTL_CONNECT: "
			    "invalid message size", __func__);
			return (0);
		}
		memcpy(&swc, imsg->data, sizeof(swc));
		un = (struct sockaddr_un *)&swc.swc_addr.swa_addr;

		if ((os = ofsw_create(un->sun_path, imsg->fd)) != NULL)
			ofsw_new_ofcconn(os, &swc.swc_target);
		return (0);
	case IMSG_CTL_DISCONNECT:
		if (IMSG_DATA_SIZE(imsg) < sizeof(swc)) {
			log_warnx("%s: IMSG_CTL_DEVICE_DISCONNECT: "
			    "invalid message size", __func__);
			return (0);
		}
		memcpy(&swc, imsg->data, sizeof(swc));
		un = (struct sockaddr_un *)&swc.swc_addr.swa_addr;

		TAILQ_FOREACH(os, &ofsw_list, os_next) {
			if (!strcmp(os->os_name, un->sun_path))
				break;
		}
		if (os) {
			log_warnx("%s: closed by request", os->os_name);
			ofsw_close(os);
			ofsw_free(os);
		}
		return (0);
	default:
		break;
	}

	return (-1);
}

struct ofsw *
ofsw_create(const char *name, int fd)
{
	struct ofsw	*os = NULL;

	if ((os = calloc(1, sizeof(struct ofsw))) == NULL) {
		log_warn("%s: calloc failed", __func__);
		goto fail;
	}
	if ((os->os_name = strdup(name)) == NULL) {
		log_warn("%s: strdup failed", __func__);
		goto fail;
	}
	os->os_fd = fd;
	TAILQ_INIT(&os->os_ofcconns);
	TAILQ_INSERT_TAIL(&ofsw_list, os, os_next);

	event_set(&os->os_evio, os->os_fd, EV_READ|EV_WRITE, ofsw_on_io, os);
	event_add(&os->os_evio, NULL);

	return (os);

 fail:
	if (os != NULL)
		free(os->os_name);
	free(os);

	return (NULL);
}

void
ofsw_close(struct ofsw *os)
{
	struct ofcconn	*oc, *oct;

	if (os->os_fd >= 0) {
		close(os->os_fd);
		event_del(&os->os_evio);
		os->os_fd = -1;
	}
	TAILQ_FOREACH_SAFE(oc, &os->os_ofcconns, oc_next, oct) {
		ofcconn_close(oc);
		ofcconn_free(oc);
	}
}

void
ofsw_free(struct ofsw *os)
{
	if (os == NULL)
		return;

	TAILQ_REMOVE(&ofsw_list, os, os_next);
	free(os->os_name);
	free(os);
}

void
ofsw_on_io(int fd, short evmask, void *ctx)
{
	struct ofsw		*os = ctx;
	struct ofcconn		*oc, *oct;
	static char		 msg[65536];/* max size of OpenFlow message */
	ssize_t			 msgsz, sz;
	struct ofp_header	*hdr;

	if (evmask & EV_WRITE || os->os_write_ready) {
		os->os_write_ready = 1;
		if (ofsw_write(os, NULL) == -1)
			return;
	}

	if ((evmask & EV_READ) && ofsw_ofc_write_ready(os)) {
		if ((msgsz = read(os->os_fd, msg, sizeof(msg))) <= 0) {
			if (msgsz == -1)
				log_warn("%s: %s read", __func__, os->os_name);
			else
				log_warnx("%s: %s closed", __func__,
				    os->os_name);
			ofsw_close(os);
			ofsw_free(os);
			return;
		}
		hdr = (struct ofp_header *)msg;
		if (hdr->oh_type != OFP_T_HELLO) {
			TAILQ_FOREACH_SAFE(oc, &os->os_ofcconns, oc_next, oct) {
				if ((sz = write(oc->oc_sock, msg, msgsz))
				    != msgsz) {
					log_warn("%s: sending a message to "
					    "%s failed", os->os_name,
					    oc->oc_name);
					ofcconn_io_fail(oc);
					continue;
				}
				oc->oc_write_ready = 0;
				ofcconn_reset_event_handlers(oc);
			}
		}
	}
	ofsw_reset_event_handlers(os);

	return;
}

int
ofsw_write(struct ofsw *os, struct ofcconn *oc0)
{
	struct ofcconn		*oc = oc0;
	struct ofp_header	*hdr;
	u_char			*msg;
	ssize_t			 sz, msglen;
	int			 remain = 0;
	unsigned char		 buf[65536];

	if (!os->os_write_ready)
		return (0);

 again:
	if (oc != NULL) {
		hdr = ibuf_seek(oc->oc_buf, 0, sizeof(*hdr));
		if (hdr == NULL)
			return (0);
		msglen = ntohs(hdr->oh_length);
		msg = ibuf_seek(oc->oc_buf, 0, msglen);
		if (msg == NULL)
			return (0);
	} else {
		TAILQ_FOREACH(oc, &os->os_ofcconns, oc_next) {
			hdr = ibuf_seek(oc->oc_buf, 0, sizeof(*hdr));
			if (hdr == NULL)
				continue;
			msglen = ntohs(hdr->oh_length);
			msg = ibuf_seek(oc->oc_buf, 0, msglen);
			if (msg != NULL)
				break;
		}
		if (oc == NULL)
			return (0);	/* no message to write yet */
	}
	if (hdr->oh_type != OFP_T_HELLO) {
		if ((sz = write(os->os_fd, msg, msglen)) != msglen) {
			if (sz == -1)
				log_warn("%s: %s write failed", __func__,
				    os->os_name);
			else
				log_warn("%s: %s write partially", __func__,
				    os->os_name);
			ofsw_close(os);
			ofsw_free(os);
			return (-1);
		}
		os->os_write_ready = 0;
	}

	/* XXX preserve the remaining part */
	if ((remain = oc->oc_buf->wpos - msglen) > 0)
		memcpy(buf, (caddr_t)msg + msglen, remain);
	ibuf_reset(oc->oc_buf);

	/* XXX put the remaining part again */
	if (remain > 0)
		ibuf_add(oc->oc_buf, buf, remain);

	if (os->os_write_ready) {
		oc = NULL;
		goto again;
	}

	return (0);
}

int
ofsw_ofc_write_ready(struct ofsw *os)
{
	struct ofcconn	*oc;
	int		 write_ready = 0;

	TAILQ_FOREACH(oc, &os->os_ofcconns, oc_next) {
		if (oc->oc_write_ready)
			write_ready = 1;
		else
			break;
	}
	if (oc != NULL)
		return (0);

	return (write_ready);
}

void
ofsw_reset_event_handlers(struct ofsw *os)
{
	short	evmask = 0, oevmask;

	oevmask = event_pending(&os->os_evio, EV_READ|EV_WRITE, NULL);

	if (ofsw_ofc_write_ready(os))
		evmask |= EV_READ;
	if (!os->os_write_ready)
		evmask |= EV_WRITE;

	if (oevmask != evmask) {
		if (oevmask)
			event_del(&os->os_evio);
		event_set(&os->os_evio, os->os_fd, evmask, ofsw_on_io, os);
		event_add(&os->os_evio, NULL);
	}
}

int
ofsw_new_ofcconn(struct ofsw *os, struct switch_address *swa)
{
	struct ofcconn	*oc = NULL;
	char		 buf[128];

	if ((oc = calloc(1, sizeof(struct ofcconn))) == NULL) {
		log_warn("%s: calloc failed", __func__);
		goto fail;
	}

	if (asprintf(&oc->oc_name, "tcp:%s",
	    print_host(&swa->swa_addr, buf, sizeof(buf))) == -1) {
		log_warn("%s: strdup failed", __func__);
		goto fail;
	}
	if ((oc->oc_buf = ibuf_new(NULL, 0)) == NULL) {
		log_warn("%s: failed to get new ibuf", __func__);
		goto fail;
	}
	oc->oc_sw = os;
	oc->oc_sock = -1;
	memcpy(&oc->oc_peer, &swa->swa_addr, sizeof(oc->oc_peer));

	if (ntohs(((struct sockaddr_in *)&oc->oc_peer)->sin_port) == 0)
		((struct sockaddr_in *)&oc->oc_peer)->sin_port =
		    htons(SWITCHD_CTLR_PORT);

	evtimer_set(&oc->oc_evtimer, ofcconn_on_timer, oc);
	TAILQ_INSERT_TAIL(&os->os_ofcconns, oc, oc_next);

	return (ofcconn_connect(oc));

 fail:
	if (oc != NULL) {
		free(oc->oc_name);
		ibuf_release(oc->oc_buf);
	}
	free(oc);

	return (-1);
}

int
ofcconn_connect(struct ofcconn *oc)
{
	int		 sock = -1;
	struct timeval	 tv;

	if ((sock = socket(oc->oc_peer.ss_family, SOCK_STREAM | SOCK_NONBLOCK,
	    IPPROTO_TCP)) == -1) {
		log_warn("%s: failed to open socket for channel with %s",
		    oc->oc_sw->os_name, oc->oc_name);
		goto fail;
	}

	if (connect(sock, (struct sockaddr *)&oc->oc_peer,
	    oc->oc_peer.ss_len) == -1) {
		if (errno != EINPROGRESS) {
			log_warn("%s: failed to connect channel to %s",
			    oc->oc_sw->os_name, oc->oc_name);
			goto fail;
		}
	}

	oc->oc_sock = sock;
	event_set(&oc->oc_evsock, oc->oc_sock, EV_READ|EV_WRITE,
	    ofcconn_on_sockio, oc);
	event_add(&oc->oc_evsock, NULL);

	tv.tv_sec = SWITCHD_CONNECT_TIMEOUT;
	tv.tv_usec = 0;
	event_add(&oc->oc_evtimer, &tv);

	return (0);

 fail:
	if (sock >= 0)
		close(sock);

	oc->oc_conn_fails++;
	ofcconn_connect_again(oc);

	return (-1);
}

void
ofcconn_on_sockio(int fd, short evmask, void *ctx)
{
	struct ofcconn	*oc = ctx;
	ssize_t		 sz;
	size_t		 wpos;
	int		 err;
	socklen_t	 optlen;

	if (evmask & EV_WRITE) {
		if (oc->oc_connected == 0) {
			optlen = sizeof(err);
			getsockopt(oc->oc_sock, SOL_SOCKET, SO_ERROR, &err,
			    &optlen);
			if (err != 0) {
				log_warnx("%s: connection error with %s: %s",
				    oc->oc_sw->os_name, oc->oc_name,
				    strerror(err));
				oc->oc_conn_fails++;
				ofcconn_close(oc);
				ofcconn_connect_again(oc);
				return;
			}
			log_info("%s: OpenFlow channel to %s connected",
			    oc->oc_sw->os_name, oc->oc_name);

			event_del(&oc->oc_evtimer);
			oc->oc_connected = 1;
			oc->oc_conn_fails = 0;
			if (ofcconn_send_hello(oc) != 0)
				return;
		} else
			oc->oc_write_ready = 1;
	}

	if ((evmask & EV_READ) && ibuf_left(oc->oc_buf) > 0) {
		wpos = ibuf_length(oc->oc_buf);

		/* XXX temporally fix not to access unallocated area */
		if (wpos + ibuf_left(oc->oc_buf) > oc->oc_buf->size) {
			ibuf_reserve(oc->oc_buf, ibuf_left(oc->oc_buf));
			ibuf_setsize(oc->oc_buf, wpos);
		}

		if ((sz = read(oc->oc_sock, ibuf_data(oc->oc_buf) + wpos,
		    ibuf_left(oc->oc_buf))) <= 0) {
			if (sz == 0)
				log_warnx("%s: %s: connection closed by peer",
				    oc->oc_sw->os_name, oc->oc_name);
			else
				log_warn("%s: %s: connection read error",
				    oc->oc_sw->os_name, oc->oc_name);
			goto fail;
		}
		if (ibuf_setsize(oc->oc_buf, wpos + sz) == -1)
			goto fail;

		if (ofsw_write(oc->oc_sw, oc) == -1)
			return;	/* oc is already freeed */
	}
	ofcconn_reset_event_handlers(oc);
	ofsw_reset_event_handlers(oc->oc_sw);

	return;

 fail:
	ofcconn_close(oc);
	ofcconn_connect_again(oc);
}

void
ofcconn_connect_again(struct ofcconn *oc)
{
	struct timeval	 tv;
	const int	 ofcconn_backoffs[] = { 1, 2, 4, 8, 16 };

	tv.tv_sec = (oc->oc_conn_fails < (int)nitems(ofcconn_backoffs))
	    ? ofcconn_backoffs[oc->oc_conn_fails]
	    : ofcconn_backoffs[nitems(ofcconn_backoffs) - 1];
	tv.tv_usec = 0;
	event_add(&oc->oc_evtimer, &tv);
}

void
ofcconn_on_timer(int fd, short evmask, void *ctx)
{
	struct ofcconn	*oc = ctx;

	if (oc->oc_sock < 0)
		ofcconn_connect(oc);
	else if (!oc->oc_connected) {
		log_warnx("%s: timeout connecting channel to %s",
		    oc->oc_sw->os_name, oc->oc_name);
		ofcconn_close(oc);
		oc->oc_conn_fails++;
		ofcconn_connect_again(oc);
	}
}

void
ofcconn_reset_event_handlers(struct ofcconn *oc)
{
	short	evmask = 0, oevmask;

	oevmask = event_pending(&oc->oc_evsock, EV_READ|EV_WRITE, NULL);

	if (ibuf_left(oc->oc_buf) > 0)
		evmask |= EV_READ;
	if (!oc->oc_write_ready)
		evmask |= EV_WRITE;

	if (oevmask != evmask) {
		if (oevmask)
			event_del(&oc->oc_evsock);
		if (evmask) {
			event_set(&oc->oc_evsock, oc->oc_sock, evmask,
			    ofcconn_on_sockio, oc);
			event_add(&oc->oc_evsock, NULL);
		}
	}
}

void
ofcconn_io_fail(struct ofcconn *oc)
{
	ofcconn_close(oc);
	ofcconn_connect_again(oc);
}

void
ofcconn_close(struct ofcconn *oc)
{
	if (oc->oc_sock >= 0) {
		event_del(&oc->oc_evsock);
		close(oc->oc_sock);
		oc->oc_sock = -1;
		oc->oc_write_ready = 0;
	}
	event_del(&oc->oc_evtimer);
	oc->oc_connected = 0;
}

void
ofcconn_free(struct ofcconn *oc)
{
	if (oc == NULL)
		return;
	TAILQ_REMOVE(&oc->oc_sw->os_ofcconns, oc, oc_next);
	ibuf_release(oc->oc_buf);
	free(oc->oc_name);
	free(oc);
}

int
ofcconn_send_hello(struct ofcconn *oc)
{
	struct ofp_header	 hdr;
	ssize_t			 sz;

	hdr.oh_version = OFP_V_1_3;
	hdr.oh_type = OFP_T_HELLO;
	hdr.oh_length = htons(sizeof(hdr));
	hdr.oh_xid = htonl(0xffffffffUL);

	sz = sizeof(hdr);
	if (write(oc->oc_sock, &hdr, sz) != sz) {
		log_warn("%s: %s: %s; write", __func__, oc->oc_sw->os_name,
		    oc->oc_name);
		ofcconn_close(oc);
		ofcconn_connect_again(oc);
		return (-1);
	}

	return (0);
}
