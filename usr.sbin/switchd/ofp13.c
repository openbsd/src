/*	$OpenBSD: ofp13.c,v 1.2 2016/07/20 14:15:08 reyk Exp $	*/

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
#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <imsg.h>
#include <event.h>

#include "ofp.h"
#include "switchd.h"
#include "ofp_map.h"

int	 ofp13_hello(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_echo_request(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_packet_in(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_error(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);

int	 ofp13_packet_match(struct packet *, struct ofp_match *, unsigned int);

void	 ofp13_debug(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
void	 ofp13_debug_header(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *);
int	 ofp13_debug_packet_in(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_debug_packet_out(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_debug_error(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);

struct ofp_callback ofp13_callbacks[] = {
	{ OFP_T_HELLO,			ofp13_hello, NULL },
	{ OFP_T_ERROR,			NULL, ofp13_debug_error },
	{ OFP_T_ECHO_REQUEST,		ofp13_echo_request, NULL },
	{ OFP_T_ECHO_REPLY,		NULL, NULL },
	{ OFP_T_EXPERIMENTER,		NULL, NULL },
	{ OFP_T_FEATURES_REQUEST,	NULL, NULL },
	{ OFP_T_FEATURES_REPLY,		NULL, NULL },
	{ OFP_T_GET_CONFIG_REQUEST,	NULL, NULL },
	{ OFP_T_GET_CONFIG_REPLY,	NULL, NULL },
	{ OFP_T_SET_CONFIG,		NULL, NULL },
	{ OFP_T_PACKET_IN,		ofp13_packet_in, ofp13_debug_packet_in },
	{ OFP_T_FLOW_REMOVED,		NULL, NULL },
	{ OFP_T_PORT_STATUS,		NULL, NULL },
	{ OFP_T_PACKET_OUT,		NULL, ofp13_debug_packet_out },
	{ OFP_T_FLOW_MOD,		NULL, NULL },
	{ OFP_T_GROUP_MOD,		NULL, NULL },
	{ OFP_T_PORT_MOD,		NULL, NULL },
	{ OFP_T_TABLE_MOD,		NULL, NULL },
	{ OFP_T_MULTIPART_REQUEST,	NULL, NULL },
	{ OFP_T_MULTIPART_REPLY,	NULL, NULL },
	{ OFP_T_BARRIER_REQUEST,	NULL, NULL },
	{ OFP_T_BARRIER_REPLY,		NULL, NULL },
	{ OFP_T_QUEUE_GET_CONFIG_REQUEST, NULL, NULL },
	{ OFP_T_QUEUE_GET_CONFIG_REPLY,	NULL, NULL },
	{ OFP_T_ROLE_REQUEST,		NULL, NULL },
	{ OFP_T_ROLE_REPLY,		NULL, NULL },
	{ OFP_T_GET_ASYNC_REQUEST,	NULL, NULL },
	{ OFP_T_GET_ASYNC_REPLY,	NULL, NULL },
	{ OFP_T_SET_ASYNC,		NULL, NULL },
	{ OFP_T_METER_MOD,		NULL, NULL },
};

void
ofp13_debug_header(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh)
{
	log_debug("%s > %s: version %s type %s length %u xid %u",
	    print_host(src, NULL, 0),
	    print_host(dst, NULL, 0),
	    print_map(oh->oh_version, ofp_v_map),
	    print_map(oh->oh_type, ofp_t_map),
	    ntohs(oh->oh_length), ntohl(oh->oh_xid));
}

void
ofp13_debug(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	ofp13_debug_header(sc, src, dst, oh);

	if (ibuf == NULL ||
	    oh->oh_version != OFP_V_1_3 ||
	    oh->oh_type >= OFP_T_TYPE_MAX ||
	    ofp13_callbacks[oh->oh_type].debug == NULL)
		return;
	if (ofp13_callbacks[oh->oh_type].debug(sc, src, dst, oh, ibuf) != 0)
		goto fail;

	return;
 fail:
	log_debug("\tinvalid packet");
}

int
ofp13_debug_packet_in(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_packet_in	*pin;
	uint8_t			*p;
	size_t			 len;
	off_t			 off;

	off = 0;
	if ((pin = ibuf_seek(ibuf, off, sizeof(*pin))) == NULL)
		return (-1);
	log_debug("\tbuffer %d length %u reason %s table %u cookie 0x%016llx",
	    ntohl(pin->pin_buffer_id),
	    ntohs(pin->pin_total_len),
	    print_map(ntohs(pin->pin_reason), ofp_pktin_map),
	    pin->pin_table_id,
	    be64toh(pin->pin_cookie));
	len = ntohs(pin->pin_total_len);
	off += sizeof(*pin);
	if ((p = ibuf_seek(ibuf, off, len)) == NULL)
		return (-1);
	if (sc->sc_tap != -1)
		(void)write(sc->sc_tap, p, len);
	return (0);
}

int
ofp13_debug_packet_out(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_packet_out		*pout;
	size_t				 len;
	off_t				 off;
	struct ofp_action_header	*ah;
	struct ofp_action_output	*ao;

	off = 0;
	if ((pout = ibuf_seek(ibuf, off, sizeof(*pout))) == NULL) {
		log_debug("%s: seek failed: length %zd",
		    __func__, ibuf_length(ibuf));
		return (-1);
	}

	log_debug("\tbuffer %d port %s "
	    "actions length %u",
	    ntohl(pout->pout_buffer_id),
	    print_map(ntohl(pout->pout_in_port), ofp_port_map),
	    ntohl(pout->pout_actions_len));
	len = ntohl(pout->pout_actions_len);

	off += sizeof(*pout);
	while ((ah = ibuf_seek(ibuf, off, len)) != NULL &&
	    ntohs(ah->ah_len) >= (uint16_t)sizeof(*ah)) {
		switch (ntohs(ah->ah_type)) {
		case OFP_ACTION_OUTPUT:
			ao = (struct ofp_action_output *)ah;
			log_debug("\t\taction type %s length %d "
			    "port %s max length %d",
			    print_map(ntohs(ao->ao_type), ofp_action_map),
			    ntohs(ao->ao_len),
			    print_map(ntohs(ao->ao_port), ofp_port_map),
			    ntohs(ao->ao_max_len));
			break;
		default:
			log_debug("\t\taction type %s length %d",
			    print_map(ntohs(ah->ah_type), ofp_action_map),
			    ntohs(ah->ah_len));
			break;
		}
		if (pout->pout_buffer_id == (uint32_t)-1)
			break;
		off += ntohs(ah->ah_len);
	}

	return (0);
}

int
ofp13_debug_error(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_error		*err;
	off_t				 off;
	const char			*code;

	off = 0;
	if ((err = ibuf_seek(ibuf, off, sizeof(*err))) == NULL) {
		log_debug("%s: seek failed: length %zd",
		    __func__, ibuf_length(ibuf));
		return (-1);
	}

	switch (ntohs(err->err_type)) {
	case OFP_ERRTYPE_FLOW_MOD_FAILED:
		code = print_map(ntohs(err->err_code), ofp_errflowmod_map);
		break;
	default:
		code = NULL;
		break;
	}

	log_debug("\terror type %s code %u%s%s",
	    print_map(ntohs(err->err_type), ofp_errtype_map),
	    ntohs(err->err_code),
	    code == NULL ? "" : ": ",
	    code == NULL ? "" : code);

	return (0);
}

int
ofp13_input(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	ofp13_debug(sc, &con->con_peer, &con->con_local, oh, ibuf);

	if (oh->oh_version != OFP_V_1_3 ||
	    oh->oh_type >= OFP_T_TYPE_MAX) {
		log_debug("unsupported packet");
		return (-1);
	}

	if (ofp13_callbacks[oh->oh_type].cb == NULL) {
		log_debug("message not supported: %s",
		    print_map(oh->oh_type, ofp_t_map));
		return (-1);
	}
	if (ofp13_callbacks[oh->oh_type].cb(sc, con, oh, ibuf) != 0)
		return (-1);

	return (0);
}

int
ofp13_hello(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	if (oh->oh_version == OFP_V_1_3 &&
	    switch_add(con) == NULL) {
		log_debug("%s: failed to add switch", __func__);
		ofp_close(con);
		return (-1);
	}

	/* Echo back the received Hello packet */
	oh->oh_version = OFP_V_1_3;
	oh->oh_length = htons(sizeof(*oh));
	oh->oh_xid = htonl(con->con_xidnxt++);
	ofp_send(con, oh, NULL);
	ofp13_debug(sc, &con->con_local, &con->con_peer, oh, NULL);

	return (0);
}

int
ofp13_echo_request(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	/* Echo reply */
	oh->oh_type = OFP_T_ECHO_REPLY;
	ofp13_debug(sc, &con->con_local, &con->con_peer, oh, NULL);
	ofp_send(con, oh, NULL);

	return (0);
}

int
ofp13_packet_match(struct packet *pkt, struct ofp_match *m, uint32_t flags)
{
#if 0
	struct ether_header	*eh = pkt->pkt_eh;

	bzero(m, sizeof(*m));
	m->m_wildcards = htonl(~flags);

	if ((flags & (OFP_WILDCARD_DL_SRC|OFP_WILDCARD_DL_DST)) && (eh == NULL))
		return (-1);

	if (flags & OFP_WILDCARD_DL_SRC)
		memcpy(m->m_dl_src, eh->ether_shost, ETHER_ADDR_LEN);
	if (flags & OFP_WILDCARD_DL_DST)
		memcpy(m->m_dl_dst, eh->ether_dhost, ETHER_ADDR_LEN);
#endif
	return (0);
}

int
ofp13_packet_in(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *ih, struct ibuf *ibuf)
{
#if 0
	struct ofp_packet_in		*pin;
	struct ofp_packet_out		*pout;
	struct ofp_action_output	*ao;
	struct ofp_flow_mod		*fm;
	struct ofp_header		*oh;
	struct packet			 pkt;
	struct ibuf			*obuf = NULL;
	int				 ret = -1;
	size_t				 len;
	long				 srcport, dstport;
	int				 addflow = 0;
	int				 addpacket = 0;

	if ((pin = ibuf_getdata(ibuf, sizeof(*pin))) == NULL)
		return (-1);

	bzero(&pkt, sizeof(pkt));
	len = ntohs(pin->pin_total_len);
	srcport = ntohs(pin->pin_port);

	if ((dstport = packet_input(sc, con->con_switch,
	    srcport, ibuf, len, &pkt)) == -1 ||
	    dstport > OFP_PORT_MAX) {
		/* fallback to flooding */
		dstport = OFP_PORT_FLOOD;
	} else if (srcport == dstport) {
		/*
		 * silently drop looping packet
		 * (don't use OFP_PORT_INPUT here)
		 */
		dstport = OFP_PORT_ANY;
	}

	if (dstport <= OFP_PORT_MAX)
		addflow = 1;

	if ((obuf = ibuf_static()) == NULL)
		goto done;

 again:
	if (addflow) {
		if ((fm = ibuf_advance(obuf, sizeof(*fm))) == NULL)
			goto done;

		ofp13_packet_match(&pkt, &fm->fm_match, OFP_WILDCARD_DL_DST);

		oh = &fm->fm_oh;
		fm->fm_cookie = 0; /* XXX should we set a cookie? */
		fm->fm_command = htons(OFP_FLOWCMD_ADD);
		fm->fm_idle_timeout = htons(sc->sc_cache_timeout);
		fm->fm_hard_timeout = 0; /* permanent */
		fm->fm_priority = 0;
		fm->fm_buffer_id = pin->pin_buffer_id;
		fm->fm_flags = htons(OFP_FLOWFLAG_SEND_FLOW_REMOVED);
		if (pin->pin_buffer_id == (uint32_t)-1)
			addpacket = 1;
	} else {
		if ((pout = ibuf_advance(obuf, sizeof(*pout))) == NULL)
			goto done;

		oh = &pout->pout_oh;
		pout->pout_buffer_id = pin->pin_buffer_id;
		pout->pout_port = pin->pin_port;
		pout->pout_actions_len = htons(sizeof(*ao));

		if (pin->pin_buffer_id == (uint32_t)-1)
			addpacket = 1;
	}

	if ((ao = ibuf_advance(obuf, sizeof(*ao))) == NULL)
		goto done;
	ao->ao_type = htons(OFP_ACTION_OUTPUT);
	ao->ao_len =  htons(sizeof(*ao));
	ao->ao_port = htons((uint16_t)dstport);
	ao->ao_max_len = 0;

	/* Add optional packet payload */
	if (addpacket &&
	    imsg_add(obuf, pkt.pkt_buf, pkt.pkt_len) == -1)
		goto done;

	/* Set output header */
	memcpy(oh, ih, sizeof(*oh));
	oh->oh_length = htons(ibuf_length(obuf));
	oh->oh_type = addflow ? OFP_T_FLOW_MOD : OFP_T_PACKET_OUT;
	oh->oh_xid = htonl(con->con_xidnxt++);

	ofp13_debug(sc, &con->con_local, &con->con_peer, oh, obuf);

	ofp_send(con, NULL, obuf);

	if (addflow && addpacket) {
		/* loop to output the packet again */
		addflow = 0;
		if ((obuf = ibuf_static()) == NULL)
			goto done;
		goto again;
	}

	ret = 0;
 done:
	ibuf_release(obuf);
	return (ret);
#else
	return (0);
#endif
}
