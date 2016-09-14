/*	$OpenBSD: ofp.c,v 1.7 2016/09/14 13:46:51 rzalamena Exp $	*/

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <event.h>

#include "ofp.h"
#include "ofp10.h"
#include "switchd.h"
#include "ofp_map.h"

int	 ofp_dispatch_control(int, struct privsep_proc *, struct imsg *);
int	 ofp_dispatch_parent(int, struct privsep_proc *, struct imsg *);
void	 ofp_run(struct privsep *, struct privsep_proc *, void *);
int	 ofp_add_device(struct switchd *, int, const char *);

static unsigned int	 id = 0;

static struct privsep_proc procs[] = {
	{ "control",	PROC_CONTROL,	ofp_dispatch_control },
	{ "parent",	PROC_PARENT,	ofp_dispatch_parent },
	{ "ofcconn",	PROC_OFCCONN,	NULL }
};

static TAILQ_HEAD(, switch_connection) conn_head =
    TAILQ_HEAD_INITIALIZER(conn_head);

void
ofp(struct privsep *ps, struct privsep_proc *p)
{
	struct switchd		*sc = ps->ps_env;
	struct switch_server	*srv = &sc->sc_server;

	if ((sc->sc_tap = switchd_tap()) == -1)
		fatal("tap");

	log_info("listen on %s", print_host(&srv->srv_addr, NULL, 0));

	if ((srv->srv_fd = switchd_listen((struct sockaddr *)
	    &srv->srv_addr)) == -1)
		fatal("listen");

	proc_run(ps, p, procs, nitems(procs), ofp_run, NULL);
}

void
ofp_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct switchd		*sc = ps->ps_env;
	struct switch_server	*srv = &sc->sc_server;

	/*
	 * pledge in the ofp process:
 	 * stdio - for malloc and basic I/O including events.
	 * inet - for handling tcp connections with OpenFlow peers.
	 * recvfd - for receiving new sockets on reload.
	 */
	if (pledge("stdio inet recvfd", NULL) == -1)
		fatal("pledge");

	event_set(&srv->srv_ev, srv->srv_fd, EV_READ, ofp_accept, srv);
	event_add(&srv->srv_ev, NULL);
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
	struct switchd			*sc = p->p_ps->ps_env;
	struct switch_device		*sdv;
	struct switch_connection	*c;

	switch (imsg->hdr.type) {
	case IMSG_CTL_DEVICE_CONNECT:
	case IMSG_CTL_DEVICE_DISCONNECT:
		if (IMSG_DATA_SIZE(imsg) < sizeof(*sdv)) {
			log_warnx("%s: IMSG_CTL_DEVICE_CONNECT: "
			    "message size is wrong", __func__);
			return (0);
		}
		sdv = imsg->data;
		if (imsg->hdr.type == IMSG_CTL_DEVICE_CONNECT)
			ofp_add_device(sc, imsg->fd, sdv->sdv_device);
		else {
			TAILQ_FOREACH(c, &conn_head, con_next) {
				if (c->con_peer.ss_family == AF_UNIX &&
				    strcmp(sdv->sdv_device,
				    ((struct sockaddr_un *)&c->con_peer)
				    ->sun_path) == 0)
					break;
			}
			if (c)
				ofp_close(c);
		}
		return (0);
	default:

		break;
	}

	return (-1);
}

void
ofp_close(struct switch_connection *con)
{
	log_info("%s: connection %u closed", __func__, con->con_id);
	switch_remove(con->con_sc, con->con_switch);
	close(con->con_fd);
	TAILQ_REMOVE(&conn_head, con, con_next);
}

int
ofp_validate_header(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, uint8_t version)
{
	struct constmap	*tmap;

	/* For debug, don't verify the header if the version is unset */
	if (version != OFP_V_0 &&
	    (oh->oh_version != version ||
	    oh->oh_type >= OFP_T_TYPE_MAX))
		return (-1);

	switch (version) {
	case OFP_V_1_0:
	case OFP_V_1_1:
		tmap = ofp10_t_map;
		break;
	case OFP_V_1_3:
	default:
		tmap = ofp_t_map;
		break;
	}

	log_debug("%s > %s: version %s type %s length %u xid %u",
	    print_host(src, NULL, 0),
	    print_host(dst, NULL, 0),
	    print_map(oh->oh_version, ofp_v_map),
	    print_map(oh->oh_type, tmap),
	    ntohs(oh->oh_length), ntohl(oh->oh_xid));

	return (0);
}

void
ofp_read(int fd, short event, void *arg)
{
	uint8_t			 buf[SWITCHD_READ_BUFFER];
	struct switch_connection *con = arg;
	struct switch_control	*sw;
	struct switchd		*sc = con->con_sc;
	struct ofp_header	*oh;
	ssize_t			 len;
	struct ibuf		*ibuf = NULL;

	event_add(&con->con_ev, NULL);
	if ((event & EV_TIMEOUT))
		goto fail;

	if ((len = read(fd, buf, sizeof(buf))) == -1)
		goto fail;
	if (len == 0)
		return;

	if ((ibuf = ibuf_new(buf, len)) == NULL)
		goto fail;

	sw = con->con_switch;
	log_debug("%s: connection %d: %ld bytes from switch %u", __func__,
	    con->con_id, len, sw == NULL ? 0 : sw->sw_id);

	if ((oh = ibuf_seek(ibuf, 0, sizeof(*oh))) == NULL) {
		log_debug("short header");
		goto fail;
	}

	switch (oh->oh_version) {
	case OFP_V_1_0:
		if (ofp10_input(sc, con, oh, ibuf) != 0)
			goto fail;
		break;
	case OFP_V_1_3:
		if (ofp13_input(sc, con, oh, ibuf) != 0)
			goto fail;
		break;
	case OFP_V_1_1:
	case OFP_V_1_2:
		/* FALLTHROUGH */
	default:
		(void)ofp10_validate(sc,
		    &con->con_peer, &con->con_local, oh, ibuf);
		ofp10_hello(sc, con, oh, ibuf);
		goto fail;
	}

	return;

 fail:
	ibuf_release(ibuf);
	ofp_close(con);
}

int
ofp_send(struct switch_connection *con, struct ofp_header *oh,
    struct ibuf *obuf)
{
	struct iovec		 iov[2];
	int			 cnt = 0;
	void			*data;
	ssize_t			 len;

	if (oh != NULL) {
		iov[cnt].iov_base = oh;
		iov[cnt++].iov_len = sizeof(*oh);
	}

	if (ibuf_length(obuf)) {
		if (oh != NULL && (ibuf_seek(obuf, 0, sizeof(*oh)) == NULL))
			return (-1);
		len = ibuf_dataleft(obuf);
		if (len < 0) {
			return (-1);
		} else if (len > 0 &&
		    (data = ibuf_getdata(obuf, len)) != NULL) {
			iov[cnt].iov_base = data;
			iov[cnt++].iov_len = len;
		}
	}

	if (cnt == 0)
		return (-1);

	/* XXX */
	if (writev(con->con_fd, iov, cnt) == -1)
		return (-1);

	return (0);
}

int
ofp_add_device(struct switchd *sc, int fd, const char *name)
{
	struct switch_connection	*con = NULL;
	struct sockaddr_un		*sun;
	struct switch_control		*sw;

	if ((con = calloc(1, sizeof(*con))) == NULL) {
		log_warn("calloc");
		goto fail;
	}
	con->con_fd = fd;
	con->con_sc = sc;
	con->con_id = ++id;
	sun = (struct sockaddr_un *)&con->con_peer;
	sun->sun_family = AF_LOCAL;
	strlcpy(sun->sun_path, name, sizeof(sun->sun_path));

	/* Get associated switch, if it exists */
	sw = switch_get(con);

	log_info("%s: new device %u (%s) from switch %u",
	    __func__, con->con_id, name, sw == NULL ? 0 : sw->sw_id);

	bzero(&con->con_ev, sizeof(con->con_ev));
	event_set(&con->con_ev, con->con_fd, EV_READ, ofp_read, con);
	event_add(&con->con_ev, NULL);

	TAILQ_INSERT_TAIL(&conn_head, con, con_next);

	return (0);
fail:
	if (fd != -1)
		close(fd);
	free(con);

	return (-1);
}

void
ofp_accept(int fd, short event, void *arg)
{
	struct switch_server	*server = arg;
	struct switch_connection *con = NULL;
	struct switchd		*sc = server->srv_sc;
	struct switch_control	*sw;
	struct sockaddr_storage	 ss;
	socklen_t		 slen;
	int			 s;

	event_add(&server->srv_ev, NULL);
	if ((event & EV_TIMEOUT))
		return;

	/* XXX accept_reserve() */
	slen = sizeof(ss);
	if ((s = accept(fd, (struct sockaddr *)&ss, &slen)) == -1) {
		log_warn("accept");
		goto fail;
	}

	if ((con = calloc(1, sizeof(*con))) == NULL) {
		log_warn("calloc");
		goto fail;
	}

	slen = sizeof(con->con_local);
	if (getsockname(s, (struct sockaddr *)&con->con_local, &slen) == -1) {
		log_warn("getsockname");
		goto fail;
	}

	con->con_fd = s;
	con->con_sc = sc;
	con->con_id = ++id;
	con->con_port = htons(socket_getport(&ss));
	memcpy(&con->con_peer, &ss, sizeof(ss));

	/* Get associated switch, if it exists */
	sw = switch_get(con);

	log_info("%s: new connection %u from switch %u",
	    __func__, con->con_id, sw == NULL ? 0 : sw->sw_id);

	bzero(&con->con_ev, sizeof(con->con_ev));
	event_set(&con->con_ev, con->con_fd, EV_READ, ofp_read, con);
	event_add(&con->con_ev, NULL);

	TAILQ_INSERT_TAIL(&conn_head, con, con_next);

	return;
 fail:
	if (s != -1)
		close(s);
	free(con);
}
