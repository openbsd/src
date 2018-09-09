/*	$OpenBSD: ofp10.c,v 1.20 2018/09/09 14:21:32 akoshibe Exp $	*/

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
#include <net/ofp.h>

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

#include "ofp10.h"
#include "switchd.h"
#include "ofp_map.h"


int	 ofp10_packet_match(struct packet *, struct ofp10_match *, unsigned int);

int	 ofp10_features_reply(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_validate_features_reply(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_echo_request(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_validate_error(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_error(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_validate_packet_in(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_packet_in(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp10_validate_packet_out(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);

struct ofp_callback ofp10_callbacks[] = {
	{ OFP10_T_HELLO,		ofp10_hello, ofp_validate_hello },
	{ OFP10_T_ERROR,		NULL, ofp10_validate_error },
	{ OFP10_T_ECHO_REQUEST,		ofp10_echo_request, NULL },
	{ OFP10_T_ECHO_REPLY,		NULL, NULL },
	{ OFP10_T_EXPERIMENTER,		NULL, NULL },
	{ OFP10_T_FEATURES_REQUEST,	NULL, NULL },
	{ OFP10_T_FEATURES_REPLY,	ofp10_features_reply,
					ofp10_validate_features_reply },
	{ OFP10_T_GET_CONFIG_REQUEST,	NULL, NULL },
	{ OFP10_T_GET_CONFIG_REPLY,	NULL, NULL },
	{ OFP10_T_SET_CONFIG,		NULL, NULL },
	{ OFP10_T_PACKET_IN,		ofp10_packet_in, ofp10_validate_packet_in },
	{ OFP10_T_FLOW_REMOVED,		NULL, NULL },
	{ OFP10_T_PORT_STATUS,		NULL, NULL },
	{ OFP10_T_PACKET_OUT,		NULL, ofp10_validate_packet_out },
	{ OFP10_T_FLOW_MOD,		NULL, NULL },
	{ OFP10_T_PORT_MOD,		NULL, NULL },
	{ OFP10_T_STATS_REQUEST,	NULL, NULL },
	{ OFP10_T_STATS_REPLY,		NULL, NULL },
	{ OFP10_T_BARRIER_REQUEST,	NULL, NULL },
	{ OFP10_T_BARRIER_REPLY,	NULL, NULL },
	{ OFP10_T_QUEUE_GET_CONFIG_REQUEST, NULL, NULL },
	{ OFP10_T_QUEUE_GET_CONFIG_REPLY, NULL, NULL }
};

int
ofp10_validate(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	uint8_t	type;

	if (ofp_validate_header(sc, src, dst, oh, OFP_V_1_0) != 0) {
		log_debug("\tinvalid header");
		return (-1);
	}
	if (ibuf == NULL) {
		/* The response packet buffer is optional */
		return (0);
	}
	type = oh->oh_type;
	if (ofp10_callbacks[type].validate != NULL &&
	    ofp10_callbacks[type].validate(sc, src, dst, oh, ibuf) != 0) {
		log_debug("\tinvalid packet");
		return (-1);
	}
	return (0);
}

int
ofp10_validate_packet_in(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp10_packet_in	*pin;
	uint8_t			*p;
	size_t			 len, plen;
	off_t			 off;

	off = 0;
	if ((pin = ibuf_seek(ibuf, off, sizeof(*pin))) == NULL)
		return (-1);
	log_debug("\tbuffer %d port %s "
	    "length %u reason %u",
	    ntohl(pin->pin_buffer_id),
	    print_map(ntohs(pin->pin_port), ofp10_port_map),
	    ntohs(pin->pin_total_len),
	    pin->pin_reason);
	off += sizeof(*pin);

	len = ntohs(pin->pin_total_len);
	plen = ibuf_length(ibuf) - off;

	if (plen < len) {
		log_debug("\ttruncated packet %zu < %zu", plen, len);

		/* Buffered packets can be truncated */
		if (pin->pin_buffer_id != OFP_PKTOUT_NO_BUFFER)
			len = plen;
		else
			return (-1);
	}
	if ((p = ibuf_seek(ibuf, off, len)) == NULL)
		return (-1);
	if (sc->sc_tap != -1)
		(void)write(sc->sc_tap, p, len);

	return (0);
}

int
ofp10_validate_packet_out(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp10_packet_out		*pout;
	size_t				 len;
	off_t				 off;
	struct ofp_action_header	*ah;
	struct ofp10_action_output	*ao;

	off = 0;
	if ((pout = ibuf_seek(ibuf, off, sizeof(*pout))) == NULL) {
		log_debug("%s: seek failed: length %zd",
		    __func__, ibuf_length(ibuf));
		return (-1);
	}
	log_debug("\tbuffer %d port %s "
	    "actions length %u",
	    ntohl(pout->pout_buffer_id),
	    print_map(ntohs(pout->pout_port), ofp10_port_map),
	    ntohs(pout->pout_actions_len));
	len = ntohs(pout->pout_actions_len);

	off += sizeof(*pout);
	while ((ah = ibuf_seek(ibuf, off, len)) != NULL &&
	    ntohs(ah->ah_len) >= (uint16_t)sizeof(*ah)) {
		switch (ntohs(ah->ah_type)) {
		case OFP10_ACTION_OUTPUT:
			ao = (struct ofp10_action_output *)ah;
			log_debug("\t\taction type %s length %d "
			    "port %s max length %d",
			    print_map(ntohs(ao->ao_type), ofp10_action_map),
			    ntohs(ao->ao_len),
			    print_map(ntohs(ao->ao_port), ofp10_port_map),
			    ntohs(ao->ao_max_len));
			break;
		default:
			log_debug("\t\taction type %s length %d",
			    print_map(ntohs(ah->ah_type), ofp10_action_map),
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
ofp10_validate_error(struct switchd *sc,
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
	case OFP10_ERRTYPE_FLOW_MOD_FAILED:
		code = print_map(ntohs(err->err_code), ofp10_errflowmod_map);
		break;
	default:
		code = NULL;
		break;
	}

	log_debug("\terror type %s code %u%s%s",
	    print_map(ntohs(err->err_type), ofp10_errtype_map),
	    ntohs(err->err_code),
	    code == NULL ? "" : ": ",
	    code == NULL ? "" : code);

	return (0);
}

int
ofp10_input(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	if (ofp10_validate(sc, &con->con_peer, &con->con_local, oh, ibuf) != 0)
		return (-1);

	if (ofp10_callbacks[oh->oh_type].cb == NULL) {
		log_debug("message not supported: %s",
		    print_map(oh->oh_type, ofp10_t_map));
		return (-1);
	}
	if (ofp10_callbacks[oh->oh_type].cb(sc, con, oh, ibuf) != 0)
		return (-1);

	return (0);
}

int
ofp10_hello(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	if (switch_add(con) == NULL) {
		log_debug("%s: failed to add switch", __func__);
		return (-1);
	}

	if (ofp_recv_hello(sc, con, oh, ibuf) == -1)
		return (-1);

	return (ofp_nextstate(sc, con, OFP_STATE_FEATURE_WAIT));
}

int
ofp10_features_reply(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	return (ofp_nextstate(sc, con, OFP_STATE_ESTABLISHED));
}

int
ofp10_validate_features_reply(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_switch_features	*swf;
	struct ofp10_phy_port		*swp;
	off_t				 poff;
	int				 portslen;
	char				*mac;

	if ((swf = ibuf_seek(ibuf, 0, sizeof(*swf))) == NULL)
		return (-1);

	log_debug("\tdatapath_id %#016llx nbuffers %u ntables %d "
	    "capabilities %#08x actions %#08x",
	    be64toh(swf->swf_datapath_id), ntohl(swf->swf_nbuffers),
	    swf->swf_ntables, ntohl(swf->swf_capabilities),
	    ntohl(swf->swf_actions));

	poff = sizeof(*swf);
	portslen = ntohs(oh->oh_length) - sizeof(*swf);
	if (portslen <= 0)
		return (0);

	while (portslen > 0) {
		if ((swp = ibuf_seek(ibuf, poff, sizeof(*swp))) == NULL)
			return (-1);

		mac = ether_ntoa((void *)swp->swp_macaddr);
		log_debug("no %s macaddr %s name %s config %#08x state %#08x "
		    "cur %#08x advertised %#08x supported %#08x peer %#08x",
		    print_map(ntohs(swp->swp_number), ofp10_port_map), mac,
		    swp->swp_name, swp->swp_config, swp->swp_state,
		    swp->swp_cur, swp->swp_advertised, swp->swp_supported,
		    swp->swp_peer);

		portslen -= sizeof(*swp);
		poff += sizeof(*swp);
	}

	return (0);
}

int
ofp10_echo_request(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	/* Echo reply */
	oh->oh_type = OFP10_T_ECHO_REPLY;
	if (ofp10_validate(sc, &con->con_local, &con->con_peer, oh, NULL) != 0)
		return (-1);
	ofp_output(con, oh, NULL);

	return (0);
}

int
ofp10_packet_match(struct packet *pkt, struct ofp10_match *m, uint32_t flags)
{
	struct ether_header	*eh = pkt->pkt_eh;

	bzero(m, sizeof(*m));
	m->m_wildcards = htonl(~flags);

	if ((flags & (OFP10_WILDCARD_DL_SRC|OFP10_WILDCARD_DL_DST)) &&
	    (eh == NULL))
		return (-1);

	if (flags & OFP10_WILDCARD_DL_SRC)
		memcpy(m->m_dl_src, eh->ether_shost, ETHER_ADDR_LEN);
	if (flags & OFP10_WILDCARD_DL_DST)
		memcpy(m->m_dl_dst, eh->ether_dhost, ETHER_ADDR_LEN);

	return (0);
}

int
ofp10_packet_in(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *ih, struct ibuf *ibuf)
{
	struct ofp10_packet_in		*pin;
	struct ofp10_packet_out		*pout;
	struct ofp10_action_output	*ao;
	struct ofp10_flow_mod		*fm;
	struct ofp_header		*oh;
	struct packet			 pkt;
	struct ibuf			*obuf = NULL;
	int				 ret = -1;
	size_t				 len;
	uint32_t			 srcport, dstport;
	int				 addflow = 0;
	int				 addpacket = 0;

	if ((pin = ibuf_getdata(ibuf, sizeof(*pin))) == NULL)
		return (-1);

	bzero(&pkt, sizeof(pkt));
	len = ntohs(pin->pin_total_len);
	srcport = ntohs(pin->pin_port);

	if (packet_input(sc, con->con_switch,
	    srcport, &dstport, ibuf, len, &pkt) == -1 ||
	    (dstport > OFP10_PORT_MAX &&
	    dstport != OFP10_PORT_LOCAL &&
	    dstport != OFP10_PORT_CONTROLLER)) {
		/* fallback to flooding */
		dstport = OFP10_PORT_FLOOD;
	} else if (srcport == dstport) {
		/*
		 * silently drop looping packet
		 * (don't use OFP10_PORT_INPUT here)
		 */
		ret = 0;
		goto done;
	} else {
		addflow = 1;
	}

	if ((obuf = ibuf_static()) == NULL)
		goto done;

 again:
	if (addflow) {
		if ((fm = ibuf_advance(obuf, sizeof(*fm))) == NULL)
			goto done;

		ofp10_packet_match(&pkt, &fm->fm_match, OFP10_WILDCARD_DL_DST);

		oh = &fm->fm_oh;
		fm->fm_cookie = 0; /* XXX should we set a cookie? */
		fm->fm_command = htons(OFP_FLOWCMD_ADD);
		fm->fm_idle_timeout = htons(sc->sc_cache_timeout);
		fm->fm_hard_timeout = 0; /* permanent */
		fm->fm_priority = 0;
		fm->fm_buffer_id = pin->pin_buffer_id;
		fm->fm_flags = htons(OFP_FLOWFLAG_SEND_FLOW_REMOVED);
		if (pin->pin_buffer_id == htonl(OFP_PKTOUT_NO_BUFFER))
			addpacket = 1;
	} else {
		if ((pout = ibuf_advance(obuf, sizeof(*pout))) == NULL)
			goto done;

		oh = &pout->pout_oh;
		pout->pout_buffer_id = pin->pin_buffer_id;
		pout->pout_port = pin->pin_port;
		pout->pout_actions_len = htons(sizeof(*ao));

		if (pin->pin_buffer_id == htonl(OFP_PKTOUT_NO_BUFFER))
			addpacket = 1;
	}

	if ((ao = ibuf_advance(obuf, sizeof(*ao))) == NULL)
		goto done;
	ao->ao_type = htons(OFP_ACTION_OUTPUT);
	ao->ao_len =  htons(sizeof(*ao));
	ao->ao_port = htons((uint16_t)dstport);
	ao->ao_max_len = 0;

	/* Add optional packet payload to packet-out. */
	if (addflow == 0 && addpacket &&
	    imsg_add(obuf, pkt.pkt_buf, pkt.pkt_len) == -1)
		goto done;

	/* Set output header */
	memcpy(oh, ih, sizeof(*oh));
	oh->oh_length = htons(ibuf_length(obuf));
	oh->oh_type = addflow ? OFP10_T_FLOW_MOD : OFP10_T_PACKET_OUT;
	oh->oh_xid = htonl(con->con_xidnxt++);

	if (ofp10_validate(sc, &con->con_local, &con->con_peer, oh, obuf) != 0)
		goto done;

	ofp_output(con, NULL, obuf);

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
}
