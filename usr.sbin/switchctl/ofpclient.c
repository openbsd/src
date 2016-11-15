/*	$OpenBSD: ofpclient.c,v 1.2 2016/11/15 09:05:14 reyk Exp $	*/

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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
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

void
ofpclient(struct parse_result *res, struct passwd *pw)
{
	struct switch_connection con;
	struct switchd		 sc;
	struct ofp_header	 oh;
	int			 s;

	memset(&sc, 0, sizeof(sc));
	sc.sc_tap = -1;

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
	default:
		fatalx("connect type not supported");
	}

	/* Drop privileges */
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	log_verbose(0);

	oh.oh_version = OFP_V_1_3;
	oh.oh_type = OFP_T_HELLO;
	oh.oh_length = 0;
	oh.oh_xid = 0;
	ofp13_hello(&sc, &con, &oh, NULL);

	log_verbose(res->quiet ? 0 : 2);

	switch (res->action) {
	case DUMP_DESC:
		ofp13_desc(&sc, &con);
		break;
	case DUMP_FEATURES:
		ofp13_featuresrequest(&sc, &con);
		break;
	case DUMP_FLOWS:
		ofp13_flow_stats(&sc, &con, OFP_PORT_ANY, OFP_GROUP_ANY,
		    OFP_TABLE_ID_ALL);
		break;
	case DUMP_TABLES:
		ofp13_table_features(&sc, &con, 0);
		break;
	default:
		break;
	}	
}

/*
 * stubs for ofp*.c
 */

void
ofrelay_write(struct switch_connection *con, struct ibuf *buf)
{
	struct msgbuf		msgbuf;
	uint8_t			rbuf[0xffff];
	ssize_t			rlen;
	struct ofp_header	*oh;
	struct ibuf		*ibuf;
	struct pollfd		 pfd[1];
	int			 nfds;

	msgbuf_init(&msgbuf);
	msgbuf.fd = con->con_fd;

	ibuf_close(&msgbuf, buf);
	ibuf_write(&msgbuf);

	/* Wait for response */
	pfd[0].fd = con->con_fd;
	pfd[0].events = POLLIN;
	nfds = poll(pfd, 1, 3 * 1000);
	if (nfds == -1 || (pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL)))
		fatal("poll error");
	if (nfds == 0)
		fatal("time out");

	if ((rlen = read(con->con_fd, rbuf, sizeof(rbuf))) == -1)
		fatal("read");
	if (rlen == 0)
		fatal("connection closed");

	if ((ibuf = ibuf_new(rbuf, rlen)) == NULL)
		fatal("ibuf_new");

	if ((oh = ibuf_seek(ibuf, 0, sizeof(*oh))) == NULL)
		fatal("short header");

	if (ofp13_validate(con->con_sc,
	    &con->con_peer, &con->con_local, oh, ibuf) != 0)
		fatal("ofp13_validate");

	ibuf_free(ibuf);
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
