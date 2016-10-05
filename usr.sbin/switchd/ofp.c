/*	$OpenBSD: ofp.c,v 1.13 2016/10/05 16:40:55 reyk Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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
#include <net/ofp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <event.h>

#include "ofp10.h"
#include "switchd.h"
#include "ofp_map.h"

int	 ofp_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 ofp_dispatch_control(int, struct privsep_proc *, struct imsg *);
void	 ofp_run(struct privsep *, struct privsep_proc *, void *);
int	 ofp_add_device(struct switchd *, int, const char *);

static struct privsep_proc procs[] = {
	{ "control",	PROC_CONTROL,	ofp_dispatch_control },
	{ "parent",	PROC_PARENT,	ofp_dispatch_parent }
};

void
ofp(struct privsep *ps, struct privsep_proc *p)
{
	ofrelay(ps, p);
	proc_run(ps, p, procs, nitems(procs), ofp_run, NULL);
}

void
ofp_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct switchd	*sc = ps->ps_env;

	/*
	 * pledge in the ofp process:
 	 * stdio - for malloc and basic I/O including events.
	 * inet - for handling tcp connections with OpenFlow peers.
	 * recvfd - for receiving new sockets on reload.
	 */
	if (pledge("stdio inet recvfd", NULL) == -1)
		fatal("pledge");

	TAILQ_INIT(&sc->sc_conns);
	sc->sc_tap = -1;

	ofrelay_run(ps, p, NULL);
}

int
ofp_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_SUM:
		return (switch_dispatch_control(fd, p, imsg));
	default:
		break;
	}

	return (-1);
}

int
ofp_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep			*ps = p->p_ps;
	struct switchd			*sc = ps->ps_env;
	struct sockaddr_un		 un;
	struct switch_device		*sdv;
	struct switch_connection	*con;

	switch (imsg->hdr.type) {
	case IMSG_TAPFD:
		if (sc->sc_tap != -1)
			close(sc->sc_tap);
		sc->sc_tap = imsg->fd;
		return (0);
	case IMSG_CTL_DEVICE_CONNECT:
	case IMSG_CTL_DEVICE_DISCONNECT:
		IMSG_SIZE_CHECK(imsg, sdv);
		sdv = imsg->data;

		if (strlcpy(un.sun_path, sdv->sdv_device,
		    sizeof(un.sun_path)) >= sizeof(un.sun_path)) {
			log_warnx("invalid device: %s", sdv->sdv_device);
			return (0);
		}
		un.sun_family = AF_UNIX;
		un.sun_len = sizeof(un);

		if (imsg->hdr.type == IMSG_CTL_DEVICE_CONNECT)
			ofrelay_attach(&sc->sc_server,
			    imsg->fd, (struct sockaddr *)&un);
		else if ((con =
		    switchd_connbyaddr(sc, (struct sockaddr *)&un)) != NULL)
			ofp_close(con);
		return (0);
	default:
		break;
	}

	return (-1);
}

int
ofp_input(struct switch_connection *con, struct ibuf *ibuf)
{
	struct switchd		*sc = con->con_sc;
	struct ofp_header	*oh;

	if ((oh = ibuf_seek(ibuf, 0, sizeof(*oh))) == NULL) {
		log_debug("short header");
		return (-1);
	}

	switch (oh->oh_version) {
	case OFP_V_1_0:
		if (ofp10_input(sc, con, oh, ibuf) != 0)
			return (-1);
		break;
	case OFP_V_1_3:
		if (ofp13_input(sc, con, oh, ibuf) != 0)
			return (-1);
		break;
	case OFP_V_1_1:
	case OFP_V_1_2:
		/* FALLTHROUGH */
	default:
		(void)ofp10_validate(sc,
		    &con->con_peer, &con->con_local, oh, ibuf);
		ofp10_hello(sc, con, oh, ibuf);
		return (-1);
	}

	return (0);
}

int
ofp_output(struct switch_connection *con, struct ofp_header *oh,
    struct ibuf *obuf)
{
	struct ibuf	*buf;

	if ((buf = ibuf_static()) == NULL)
		return (-1);
	if ((oh != NULL) &&
	    (ibuf_add(buf, oh, sizeof(*oh)) == -1)) {
		ibuf_release(buf);
		return (-1);
	}
	if ((obuf != NULL) &&
	    (ibuf_cat(buf, obuf) == -1)) {
		ibuf_release(buf);
		return (-1);
	}

	ofrelay_write(con, buf);

	return (0);
}

int
ofp_open(struct privsep *ps, struct switch_connection *con)
{
	struct switch_control	*sw;

	/* Get associated switch, if it exists */
	sw = switch_get(con);

	log_info("%s: new connection %u.%u from switch %u",
	    __func__, con->con_id, con->con_instance,
	    sw == NULL ? 0 : sw->sw_id);

	return (0);
}

void
ofp_close(struct switch_connection *con)
{
	ofrelay_close(con);
}
