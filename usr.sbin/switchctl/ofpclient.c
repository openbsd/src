/*	$OpenBSD: ofpclient.c,v 1.7 2018/10/21 21:10:24 akoshibe Exp $	*/

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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/un.h>

#include <net/if.h>
#include <net/ofp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <poll.h>
#include <err.h>
#include <pwd.h>
#include <vis.h>

#include "switchd.h"
#include "parser.h"

void	 ofpclient_read(struct switch_connection *, int);
int	 flowmod(struct switchd *, struct switch_connection *,
	    struct parse_result *);
int	 flowmod_test(struct switchd *, struct switch_connection *);

void
ofpclient(struct parse_result *res, struct passwd *pw)
{
	struct switch_connection con;
	struct switchd		 sc;
	int			 s, timeout;
	struct sockaddr_un	*un;

	memset(&sc, 0, sizeof(sc));
	sc.sc_tap = -1;

	/* If no uri has been specified, try to connect to localhost */
	if (res->uri.swa_addr.ss_family == AF_UNSPEC) {	
		res->uri.swa_type = SWITCH_CONN_TCP;
		if (parsehostport("127.0.0.1",
		    (struct sockaddr *)&res->uri.swa_addr,
		    sizeof(res->uri.swa_addr)) != 0)
			fatal("could not parse address");
	}

	memset(&con, 0, sizeof(con));
	memcpy(&con.con_peer, &res->uri.swa_addr, sizeof(res->uri.swa_addr));
	con.con_sc = &sc;

	/*
	 * Connect and send the request
	 */
	switch (res->uri.swa_type) {
	case SWITCH_CONN_TCP:
		if ((s = socket(res->uri.swa_addr.ss_family, SOCK_STREAM,
		    IPPROTO_TCP)) == -1)
			fatal("socket");

		/* Use the default port if no port has been specified */
		if (socket_getport(&con.con_peer) == 0)
			(void)socket_setport(&con.con_peer, SWITCHD_CTLR_PORT);

		if (connect(s, (struct sockaddr *)&con.con_peer,
		    con.con_peer.ss_len) == -1)
			fatal("connect");

		con.con_fd = s;
		break;
	case SWITCH_CONN_LOCAL:
		un = (struct sockaddr_un *)&res->uri.swa_addr;

		if (strncmp(un->sun_path, "/dev/switch",
		    strlen("/dev/switch")) != 0)
			fatalx("device path not supported");

		if ((s = open(un->sun_path, O_RDWR | O_NONBLOCK)) == -1)
			fatalx("failed to open %s", un->sun_path);
		con.con_fd = s;
		break;
	default:
		fatalx("connect type not supported");
	}

	/* Drop privileges */
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	/* Set a default read timeout */
	timeout = 3 * 1000;

	log_setverbose(res->verbose);

	ofp_send_hello(&sc, &con, OFP_V_1_3);
	ofpclient_read(&con, timeout);

	log_setverbose(res->quiet ? res->verbose : 2);

	switch (res->action) {
	case DUMP_DESC:
		ofp13_desc(&sc, &con);
		break;
	case DUMP_FEATURES:
		ofp_send_featuresrequest(&sc, &con);
		break;
	case DUMP_FLOWS:
		ofp13_flow_stats(&sc, &con, OFP_PORT_ANY, OFP_GROUP_ID_ANY,
		    res->table);
		break;
	case DUMP_TABLES:
		ofp13_table_features(&sc, &con, res->table);
		break;
	case FLOW_ADD:
	case FLOW_DELETE:
	case FLOW_MODIFY:
		timeout = 0;
		flowmod(&sc, &con, res);
		break;
	default:
		fatalx("unsupported action");
	}

	/* XXX */
	ofpclient_read(&con, timeout);
}

int
flowmod(struct switchd *sc, struct switch_connection *con,
    struct parse_result *res)
{
	struct ofp_header	*oh;
	struct ofp_flow_mod	*fm;

	if (oflowmod_iclose(&res->fctx) == -1)
		goto err;
	if (oflowmod_close(&res->fctx) == -1)
		goto err;

	fm = res->fctx.ctx_fm;
	fm->fm_table_id = res->table;
	oh = &fm->fm_oh;

	if (ofp_validate(sc, &con->con_local, &con->con_peer,
	    oh, res->fbuf, oh->oh_version) != 0)
		goto err;

	ofrelay_write(con, res->fbuf);

	return (0);

 err:
	(void)oflowmod_err(&res->fctx, __func__, __LINE__);
	log_warnx("invalid flow");
	return (-1);
}

void
ofpclient_read(struct switch_connection *con, int timeout)
{
	uint8_t			rbuf[0xffff];
	ssize_t			rlen;
	struct ofp_header	*oh;
	struct ibuf		*ibuf;
	struct pollfd		 pfd[1];
	int			 nfds;

	/* Wait for response */
	pfd[0].fd = con->con_fd;
	pfd[0].events = POLLIN;
	nfds = poll(pfd, 1, timeout);
	if (nfds == -1 || (pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL)))
		fatal("poll error");
	if (nfds == 0) {
		if (timeout)
			fatal("time out");
		return;
	}

	if ((rlen = read(con->con_fd, rbuf, sizeof(rbuf))) == -1)
		fatal("read");
	if (rlen == 0)
		fatal("connection closed");

	if ((ibuf = ibuf_new(rbuf, rlen)) == NULL)
		fatal("ibuf_new");

	if ((oh = ibuf_seek(ibuf, 0, sizeof(*oh))) == NULL)
		fatal("short header");

	if (ofp_validate(con->con_sc,
	    &con->con_peer, &con->con_local, oh, ibuf, oh->oh_version) != 0)
		fatal("ofp_validate");

	if (con->con_state == OFP_STATE_CLOSED) {
		con->con_version = oh->oh_version;
		ofp_recv_hello(con->con_sc, con, oh, ibuf);
		con->con_state = OFP_STATE_ESTABLISHED;
	}

	ibuf_free(ibuf);
}

/*
 * stubs for ofp*.c
 */

void
ofrelay_write(struct switch_connection *con, struct ibuf *buf)
{
	struct msgbuf		msgbuf;

	msgbuf_init(&msgbuf);
	msgbuf.fd = con->con_fd;

	ibuf_close(&msgbuf, buf);
	ibuf_write(&msgbuf);
}

struct switch_control *
switch_add(struct switch_connection *con)
{
	static struct switch_control sw;
	con->con_switch = &sw;
	return (&sw);
}

struct macaddr *
switch_learn(struct switchd *sc, struct switch_control *sw,
    uint8_t *ea, uint32_t port)
{
	return (NULL);
}

struct macaddr *
switch_cached(struct switch_control *sw, uint8_t *ea)
{
	return (NULL);
}

int
ofp_nextstate(struct switchd *sc, struct switch_connection *con,
    enum ofp_state state)
{
	int		rv = 0;

	switch (con->con_state) {
	case OFP_STATE_CLOSED:
		if (state != OFP_STATE_HELLO_WAIT)
			return (-1);
		break;

	case OFP_STATE_HELLO_WAIT:
		if (state != OFP_STATE_FEATURE_WAIT)
			return (-1);

		rv = ofp_send_featuresrequest(sc, con);
		break;

	case OFP_STATE_FEATURE_WAIT:
		if (state != OFP_STATE_ESTABLISHED)
			return (-1);
		break;

	case OFP_STATE_ESTABLISHED:
		if (state != OFP_STATE_CLOSED)
			return (-1);
		break;

	default:
		return (-1);
	}

	/* Set the next state. */
	con->con_state = state;

	return (rv);
}
