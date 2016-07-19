/*	$OpenBSD: ofp13.c,v 1.1 2016/07/19 16:54:26 reyk Exp $	*/

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

/* XXX not implemented, this is just a stub */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <imsg.h>
#include <event.h>

#include "ofp.h"
#include "ofp10.h"
#include "switchd.h"

void
ofp13_debug(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_packet_in	 pin;
#if 0
	uint8_t			*p;
#endif
	uint8_t			*buf;
	size_t			 len;

	len = ibuf_length(ibuf);
	buf = ibuf_data(ibuf);

	ofp10_debug_header(sc, src, dst, oh);

	if (oh->oh_version != OFP_V_1_3)
		return;

	switch (oh->oh_type) {
	case OFP_T_PACKET_IN:
		if (len < sizeof(pin))
			goto fail;
		memcpy(&pin, buf, sizeof(pin));
#if 0
		log_debug("\tbuffer %d port 0x%08x "
		    "phy port 0x%08x length %u "
		    "reason %u table id %u",
		    ntohl(pin13.pin_buffer_id),
		    ntohl(pin13.pin_port),
		    ntohl(pin13.pin_phy_port),
		    ntohs(pin13.pin_total_len),
		    pin13.pin_reason,
		    pin13.pin_table_id);
		if ((len - sizeof(pin)) < ntohs(pin.pin_total_len))
			goto fail;
		if (sc->sc_tap != -1) {
			p = (uint8_t *)&buf[sizeof(pin)];
			(void)write(sc->sc_tap, p,
			    ntohs(pin.pin_total_len));
		}
#endif
		break;
	}
	return;

 fail:
	log_debug("\tinvalid packet\n");
}

int
ofp13_input(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	uint8_t		*buf;
	ssize_t		 len;

	len = ibuf_length(ibuf);
	buf = ibuf_data(ibuf);

	ofp13_debug(sc, &con->con_peer, &con->con_local, oh, ibuf);

	switch (oh->oh_type) {
	case OFP_T_HELLO:
		/* Echo back the received Hello packet */
		ofp_send(con, oh, NULL);
		break;
	case OFP_T_ECHO_REQUEST:
		/* Echo reply */
		oh->oh_type = OFP_T_ECHO_REPLY;
		ofp_send(con, oh, NULL);
		break;
	default:
		/* not implemented */
		break;
	}

	return (0);
}
