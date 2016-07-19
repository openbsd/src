/*	$OpenBSD: ofcconn.c,v 1.3 2016/07/19 18:04:04 reyk Exp $	*/

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

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <imsg.h>

#include "ofp.h"
#include "ofp10.h"
#include "types.h"
#include "switchd.h"

int	 ofcconn_dispatch_parent(int, struct privsep_proc *, struct imsg *);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	ofcconn_dispatch_parent },
	{ "control",	PROC_CONTROL,	NULL },
	{ "ofp",	PROC_OFP,	NULL }
};

/* OpenFlow Channel Connection */
struct ofcconn {
	char			*oc_device;
	struct sockaddr_storage	 oc_peer;
	int			 oc_sock;
	int			 oc_devf;
	int			 oc_sock_write_ready;
	int			 oc_devf_write_ready;
	int			 oc_connected;
	int			 oc_conn_fails;
	struct ibuf		*oc_buf;
	TAILQ_ENTRY(ofcconn)	 oc_next;
	struct event		 oc_evsock;
	struct event		 oc_evdevf;
	struct event		 oc_evtimer;
};

TAILQ_HEAD(, ofcconn)	 ofcconn_list = TAILQ_HEAD_INITIALIZER(ofcconn_list);

struct ofcconn	*ofcconn_create(const char *, struct switch_controller *, int);
int		 ofcconn_connect(struct ofcconn *);
void		 ofcconn_on_sockio(int, short, void *);
void		 ofcconn_on_devfio(int, short, void *);
int		 ofcconn_write(struct ofcconn *);
void		 ofcconn_connect_again(struct ofcconn *);
void		 ofcconn_on_timer(int, short, void *);
void		 ofcconn_reset_evsock(struct ofcconn *);
void		 ofcconn_reset_evdevf(struct ofcconn *);
void		 ofcconn_close(struct ofcconn *);
void		 ofcconn_free(struct ofcconn *);
void		 ofcconn_shutdown_all(void);
int		 ofcconn_send_hello(struct ofcconn *);

pid_t
ofcconn_proc_init(struct privsep *ps, struct privsep_proc *p)
{
	p->p_shutdown = ofcconn_proc_shutdown;
	return (proc_run(ps, p, procs, nitems(procs), NULL, NULL));
}

void
ofcconn_proc_shutdown(void)
{
	struct ofcconn	*e, *t;

	TAILQ_FOREACH_SAFE(e, &ofcconn_list, oc_next, t) {
		ofcconn_close(e);
		ofcconn_free(e);
	}
}

int
ofcconn_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct ofcconn			*conn;
	struct switch_device		*sdv;
	struct switch_controller	*swc;

	switch (imsg->hdr.type) {
	case IMSG_CTL_DEVICE_CONNECT:
		if (IMSG_DATA_SIZE(imsg) < sizeof(*sdv)) {
			log_warnx("%s: IMSG_CTL_DEVICE_CONNECT: "
			    "invalid message size", __func__);
			return (0);
		}
		sdv = imsg->data;
		swc = &sdv->sdv_swc;
		if ((conn = ofcconn_create(sdv->sdv_device, swc,
		    imsg->fd)) != NULL)
			ofcconn_connect(conn);
		return (0);
	case IMSG_CTL_DEVICE_DISCONNECT:
		if (IMSG_DATA_SIZE(imsg) < sizeof(*sdv)) {
			log_warnx("%s: IMSG_CTL_DEVICE_DISCONNECT: "
			    "invalid message size", __func__);
			return (0);
		}
		sdv = imsg->data;
		TAILQ_FOREACH(conn, &ofcconn_list, oc_next) {
			if (!strcmp(conn->oc_device, sdv->sdv_device))
				break;
		}
		if (conn) {
			log_warnx("%s: closed by request",
			    conn->oc_device);
			ofcconn_close(conn);
			ofcconn_free(conn);
		}
		return (0);
	default:
		break;
	}

	return (-1);
}

struct ofcconn *
ofcconn_create(const char *name, struct switch_controller *swc, int fd)
{
	struct ofcconn	*oc = NULL;

	if ((oc = calloc(1, sizeof(struct ofcconn))) == NULL) {
		log_warn("%s: calloc failed", __func__);
		goto fail;
	}
	if ((oc->oc_device = strdup(name)) == NULL) {
		log_warn("%s: calloc failed", __func__);
		goto fail;
	}
	if ((oc->oc_buf = ibuf_new(NULL, 0)) == NULL) {
		log_warn("%s: failed to get new ibuf", __func__);
		goto fail;
	}

	oc->oc_sock = -1;
	oc->oc_devf = fd;
	TAILQ_INSERT_TAIL(&ofcconn_list, oc, oc_next);

	memcpy(&oc->oc_peer, &swc->swc_addr, sizeof(oc->oc_peer));

	if (ntohs(((struct sockaddr_in *)&oc->oc_peer)->sin_port) == 0)
		((struct sockaddr_in *)&oc->oc_peer)->sin_port =
		    htons(SWITCHD_CTLR_PORT);

	evtimer_set(&oc->oc_evtimer, ofcconn_on_timer, oc);

	return (oc);

 fail:
	if (oc != NULL) {
		free(oc->oc_device);
		ibuf_release(oc->oc_buf);
	}
	free(oc);

	return (NULL);
}

int
ofcconn_connect(struct ofcconn *oc)
{
	int		 sock = -1;
	char		 buf[256];
	struct timeval	 tv;

	if ((sock = socket(oc->oc_peer.ss_family, SOCK_STREAM | SOCK_NONBLOCK,
	    IPPROTO_TCP)) == -1) {
		log_warn("%s: failed to open socket for channel with %s",
		    oc->oc_device,
		    print_host(&oc->oc_peer, buf, sizeof(buf)));
		goto fail;
	}

	if (connect(sock, (struct sockaddr *)&oc->oc_peer,
	    oc->oc_peer.ss_len) == -1) {
		if (errno != EINPROGRESS) {
			log_warn("%s: failed to connect channel to %s",
			    oc->oc_device,
			    print_host(&oc->oc_peer, buf, sizeof(buf)));
			goto fail;
		}
	}

	oc->oc_sock = sock;
	event_set(&oc->oc_evsock, oc->oc_sock, EV_READ|EV_WRITE,
	    ofcconn_on_sockio, oc);
	event_set(&oc->oc_evdevf, oc->oc_devf, EV_READ|EV_WRITE,
	    ofcconn_on_devfio, oc);
	event_add(&oc->oc_evdevf, NULL);
	event_add(&oc->oc_evsock, NULL);

	tv.tv_sec = SWITCHD_OFCCONN_TIMEOUT;
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
	struct ofcconn		*oc = ctx;
	ssize_t			 sz;
	size_t			 wpos;
	char			 buf[256];
	int			 err;
	socklen_t		 optlen;

	if (evmask & EV_WRITE) {
		if (oc->oc_connected == 0) {
			optlen = sizeof(err);
			getsockopt(oc->oc_sock, SOL_SOCKET, SO_ERROR, &err,
			    &optlen);
			if (err != 0) {
				log_warnx("%s: connection error with %s: %s ",
				    oc->oc_device,
				    print_host(&oc->oc_peer, buf, sizeof(buf)),
				    strerror(err));
				oc->oc_conn_fails++;
				ofcconn_close(oc);
				ofcconn_connect_again(oc);
				return;
			}
			log_info("%s: OpenFlow channel to %s connected",
			    oc->oc_device,
			    print_host(&oc->oc_peer, buf, sizeof(buf)));
			event_del(&oc->oc_evtimer);
			oc->oc_connected = 1;
			oc->oc_conn_fails = 0;
			if (ofcconn_send_hello(oc) != 0)
				return;
		} else {
			oc->oc_sock_write_ready = 1;
			/* schedule an event to reset the event handlers */
			event_active(&oc->oc_evdevf, 0, 1);
		}
	}

	if (evmask & EV_READ && ibuf_left(oc->oc_buf) > 0) {
		wpos = ibuf_length(oc->oc_buf);

		/* XXX temporally fix not to access unallocated area */
		if (wpos + ibuf_left(oc->oc_buf) > oc->oc_buf->size) {
			ibuf_reserve(oc->oc_buf, ibuf_left(oc->oc_buf));
			ibuf_setsize(oc->oc_buf, wpos);
		}

		if ((sz = read(oc->oc_sock,
		    ibuf_data(oc->oc_buf) + wpos,
		    ibuf_left(oc->oc_buf))) <= 0) {
			if (sz == 0)
				log_warnx("%s: connection closed by peer",
				    oc->oc_device);
			else
				log_warn("%s: connection read error",
				    oc->oc_device);
			goto fail;
		}
		if (ibuf_setsize(oc->oc_buf, wpos + sz) == -1)
			goto fail;
		if (oc->oc_devf_write_ready) {
			if (ofcconn_write(oc) == -1)
				goto fail;
			event_active(&oc->oc_evdevf, 0, 1);
		}
	}
	ofcconn_reset_evsock(oc);

	return;

 fail:
	ofcconn_close(oc);
	ofcconn_connect_again(oc);
}

void
ofcconn_on_devfio(int fd, short evmask, void *ctx)
{
	struct ofcconn		*oc = ctx;
	static char		 buf[65536];/* max size of OpenFlow message */
	size_t			 sz, sz2;
	struct ofp_header	*hdr;

	if (evmask & EV_WRITE) {
		oc->oc_devf_write_ready = 1;
		if (ofcconn_write(oc) == -1)
			goto fail;
	}

	if (evmask & EV_READ && oc->oc_sock_write_ready) {
		if ((sz = read(oc->oc_devf, buf, sizeof(buf))) <= 0) {
			if (sz < 0)
				log_warn("%s: %s read", __func__,
				    oc->oc_device);
			goto fail;
		}
		hdr = (struct ofp_header *)buf;
		if (hdr->oh_type != OFP_T_HELLO) {
			/* forward packet */
			if ((sz2 = write(oc->oc_sock, buf, sz)) != sz) {
				log_warn("%s: %s write", __func__,
				    oc->oc_device);
				goto fail;
			}
			oc->oc_sock_write_ready = 0;
			/* schedule an event to reset the event handlers */
			event_active(&oc->oc_evsock, 0, 1);
		}
	}
	ofcconn_reset_evdevf(oc);

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
	char		 buf[256];

	if (oc->oc_sock < 0)
		ofcconn_connect(oc);
	else if (!oc->oc_connected) {
		log_warnx("%s: timeout connecting channel to %s",
		    oc->oc_device,
		    print_host(&oc->oc_peer, buf, sizeof(buf)));
		ofcconn_close(oc);
		oc->oc_conn_fails++;
		ofcconn_connect_again(oc);
	}
}

void
ofcconn_reset_evsock(struct ofcconn *oc)
{
	short	evmask = 0, oevmask;

	oevmask = event_pending(&oc->oc_evsock, EV_READ|EV_WRITE, NULL);

	if (ibuf_left(oc->oc_buf) > 0)
		evmask |= EV_READ;
	if (!oc->oc_sock_write_ready)
		evmask |= EV_WRITE;

	if (oevmask != evmask) {
		if (oevmask)
			event_del(&oc->oc_evsock);
		event_set(&oc->oc_evsock, oc->oc_sock, evmask,
		    ofcconn_on_sockio, oc);
		event_add(&oc->oc_evsock, NULL);
	}
}

void
ofcconn_reset_evdevf(struct ofcconn *oc)
{
	short	evmask = 0, oevmask;

	oevmask = event_pending(&oc->oc_evdevf, EV_READ|EV_WRITE, NULL);

	if (oc->oc_sock_write_ready)
		evmask |= EV_READ;
	if (!oc->oc_devf_write_ready)
		evmask |= EV_WRITE;

	if (oevmask != evmask) {
		if (oevmask)
			event_del(&oc->oc_evdevf);
		event_set(&oc->oc_evdevf, oc->oc_devf, evmask,
		    ofcconn_on_devfio, oc);
		event_add(&oc->oc_evdevf, NULL);
	}
}

int
ofcconn_write(struct ofcconn *oc)
{
	struct ofp_header	*hdr;
	size_t			 sz, pktlen;
	void			*pkt;
	/* XXX */
	unsigned char		 buf[65536];
	int			 remain = 0;

	/* Try to write if the OFP header has arrived */
	if (!oc->oc_devf_write_ready ||
	    (hdr = ibuf_seek(oc->oc_buf, 0, sizeof(*hdr))) == NULL)
		return (0);

	/* Check the length in the OFP header */
	pktlen = ntohs(hdr->oh_length);

	if ((pkt = ibuf_seek(oc->oc_buf, 0, pktlen)) != NULL) {
		hdr = pkt;
		if (hdr->oh_type != OFP_T_HELLO) {
			/* forward packet; has entire packet already */
			if ((sz = write(oc->oc_devf, pkt, pktlen)) != pktlen) {
				log_debug("%s: %s size %d pktlen %d",
				    __func__,
				    oc->oc_device, (int)sz, (int)pktlen);
				return (-1);
			}
		}
		/* XXX preserve the remaining part */
		if ((remain = oc->oc_buf->wpos - pktlen) > 0)
			memmove(buf, (caddr_t)pkt + pktlen, remain);
		ibuf_reset(oc->oc_buf);
		oc->oc_devf_write_ready = 0;
	}
	/* XXX put the remaining part again */
	if (remain > 0)
		ibuf_add(oc->oc_buf, buf, remain);

	return (0);
}

void
ofcconn_close(struct ofcconn *oc)
{
	if (oc->oc_sock >= 0) {
		event_del(&oc->oc_evsock);
		close(oc->oc_sock);
		oc->oc_sock = -1;
		oc->oc_sock_write_ready = 0;
	}
	event_del(&oc->oc_evdevf);
	event_del(&oc->oc_evtimer);
	oc->oc_connected = 0;
}

void
ofcconn_free(struct ofcconn *oc)
{
	if (oc == NULL)
		return;
	close(oc->oc_devf);
	TAILQ_REMOVE(&ofcconn_list, oc, oc_next);
	ibuf_release(oc->oc_buf);
	free(oc->oc_device);
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

	if ((sz = write(oc->oc_sock, &hdr, sizeof(hdr))) != sz) {
		log_warn("%s: %s write", __func__, oc->oc_device);
		ofcconn_close(oc);
		ofcconn_connect_again(oc);
		return (-1);
	}

	return (0);
}
