/*	$OpenBSD: ofp13.c,v 1.34 2016/11/18 20:20:19 reyk Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2016 Rafael Zalamena <rzalamena@openbsd.org>
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
#include <netmpls/mpls.h>

#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <imsg.h>
#include <event.h>

#include "switchd.h"
#include "ofp_map.h"

int	 ofp13_echo_request(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_validate_features_reply(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_features_reply(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_validate_error(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_validate_action(struct switchd *, struct ofp_header *,
	    struct ibuf *, off_t *, struct ofp_action_header *);
int	 ofp13_validate_instruction(struct switchd *, struct ofp_header *,
	    struct ibuf *, off_t *, struct ofp_instruction *);
int	 ofp13_validate_flow_mod(struct switchd *, struct sockaddr_storage *,
	    struct sockaddr_storage *, struct ofp_header *, struct ibuf *);
int	 ofp13_validate_oxm_basic(struct ibuf *, off_t, int, uint8_t);
int	 ofp13_validate_oxm(struct switchd *, struct ofp_ox_match *,
	    struct ofp_header *, struct ibuf *, off_t);
int	 ofp13_validate_packet_in(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_packet_match(struct ibuf *, struct packet *, struct ofp_match *);
int	 ofp13_packet_in(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_flow_removed(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_parse_instruction(struct ibuf *, struct ofp_instruction *);
int	 ofp13_parse_action(struct ibuf *, struct ofp_action_header *);
int	 ofp13_parse_oxm(struct ibuf *, struct ofp_ox_match *);
int	 ofp13_parse_tableproperties(struct ibuf *, struct ofp_table_features *);
int	 ofp13_multipart_reply(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_multipart_reply_validate(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_validate_packet_out(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);

struct ofp_multipart *
	    ofp13_multipart_request(struct switch_connection *, struct ibuf *,
	    uint16_t, uint16_t);
int	 ofp13_multipart_request_validate(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);

int	 ofp13_error(struct switchd *, struct switch_connection *,
	    struct ofp_header *, struct ibuf *, uint16_t, uint16_t);

struct ofp_group_mod *
	    ofp13_group(struct switch_connection *, struct ibuf *,
	    uint32_t, uint16_t, uint8_t);
struct ofp_bucket *
	    ofp13_bucket(struct ibuf *, uint16_t, uint32_t, uint32_t);

int	 ofp13_setconfig_validate(struct switchd *,
	    struct sockaddr_storage *, struct sockaddr_storage *,
	    struct ofp_header *, struct ibuf *);
int	 ofp13_setconfig(struct switchd *, struct switch_connection *,
	    uint16_t, uint16_t);
int	 ofp13_tablemiss_sendctrl(struct switchd *, struct switch_connection *,
	    uint8_t);

struct ofp_callback ofp13_callbacks[] = {
	{ OFP_T_HELLO,			ofp13_hello, NULL },
	{ OFP_T_ERROR,			NULL, ofp13_validate_error },
	{ OFP_T_ECHO_REQUEST,		ofp13_echo_request, NULL },
	{ OFP_T_ECHO_REPLY,		NULL, NULL },
	{ OFP_T_EXPERIMENTER,		NULL, NULL },
	{ OFP_T_FEATURES_REQUEST,	NULL, NULL },
	{ OFP_T_FEATURES_REPLY,		ofp13_features_reply,
					ofp13_validate_features_reply },
	{ OFP_T_GET_CONFIG_REQUEST,	NULL, NULL },
	{ OFP_T_GET_CONFIG_REPLY,	NULL, NULL },
	{ OFP_T_SET_CONFIG,		NULL, ofp13_setconfig_validate },
	{ OFP_T_PACKET_IN,		ofp13_packet_in,
					ofp13_validate_packet_in },
	{ OFP_T_FLOW_REMOVED,		ofp13_flow_removed, NULL },
	{ OFP_T_PORT_STATUS,		NULL, NULL },
	{ OFP_T_PACKET_OUT,		NULL, ofp13_validate_packet_out },
	{ OFP_T_FLOW_MOD,		NULL, ofp13_validate_flow_mod },
	{ OFP_T_GROUP_MOD,		NULL, NULL },
	{ OFP_T_PORT_MOD,		NULL, NULL },
	{ OFP_T_TABLE_MOD,		NULL, NULL },
	{ OFP_T_MULTIPART_REQUEST,	NULL,
					ofp13_multipart_request_validate },
	{ OFP_T_MULTIPART_REPLY,	ofp13_multipart_reply,
					ofp13_multipart_reply_validate },
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

int
ofp13_validate(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	uint8_t	type;

	if (ofp_validate_header(sc, src, dst, oh, OFP_V_1_3) != 0) {
		log_debug("\tinvalid header");
		return (-1);
	}
	if (ibuf == NULL) {
		/* The response packet buffer is optional */
		return (0);
	}
	type = oh->oh_type;
	if (ofp13_callbacks[type].validate != NULL &&
	    ofp13_callbacks[type].validate(sc, src, dst, oh, ibuf) != 0) {
		log_debug("\tinvalid packet");
		return (-1);
	}
	return (0);
}

int
ofp13_validate_oxm_basic(struct ibuf *ibuf, off_t off, int hasmask,
    uint8_t type)
{
	uint8_t		*ui8;
	uint16_t	*ui16;
	uint32_t	*ui32;
	uint64_t	*ui64;
	int		 i, len;
	char		 hex[8], buf[64], maskbuf[64];

	switch (type) {
	case OFP_XM_T_IN_PORT:
	case OFP_XM_T_IN_PHY_PORT:
	case OFP_XM_T_MPLS_LABEL:
		if ((ui32 = ibuf_seek(ibuf, off, sizeof(*ui32))) == NULL)
			return (-1);

		log_debug("\t\t%u", ntohl(*ui32));
		break;

	case OFP_XM_T_META:
	case OFP_XM_T_TUNNEL_ID:
		len = sizeof(*ui64);
		if (hasmask)
			len *= 2;

		if ((ui64 = ibuf_seek(ibuf, off, len)) == NULL)
			return (-1);

		if (hasmask)
			log_debug("\t\t%llu mask %#16llx",
			    be64toh(*ui64), be64toh(*(ui64 + 1)));
		else
			log_debug("\t\t%llu", be64toh(*ui64));
		break;

	case OFP_XM_T_ETH_DST:
	case OFP_XM_T_ETH_SRC:
	case OFP_XM_T_ARP_SHA:
	case OFP_XM_T_ARP_THA:
	case OFP_XM_T_IPV6_ND_SLL:
	case OFP_XM_T_IPV6_ND_TLL:
		len = ETHER_ADDR_LEN;
		if (hasmask)
			len *= 2;

		if ((ui8 = ibuf_seek(ibuf, off, len)) == NULL)
			return (-1);

		buf[0] = 0;
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			snprintf(hex, sizeof(hex), "%02x", *(ui8 + i));
			strlcat(buf, hex, sizeof(buf));
		}

		if (hasmask) {
			maskbuf[0] = 0;
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				snprintf(hex, sizeof(hex), "%02x", *(ui8 +
				    (i + ETHER_ADDR_LEN)));
				strlcat(maskbuf, hex, sizeof(maskbuf));
			}
			log_debug("\t\t%s mask %s", buf, maskbuf);
		} else
			log_debug("\t\t%s", buf);
		break;

	case OFP_XM_T_ETH_TYPE:
		if (hasmask)
			return (-1);
		len = sizeof(*ui16);
		if ((ui16 = ibuf_seek(ibuf, off, len)) == NULL)
			return (-1);
		log_debug("\t\t0x%04x", ntohs(*ui16));
		break;

	case OFP_XM_T_ARP_OP:
	case OFP_XM_T_VLAN_VID:
	case OFP_XM_T_IP_PROTO:
	case OFP_XM_T_TCP_SRC:
	case OFP_XM_T_TCP_DST:
	case OFP_XM_T_UDP_SRC:
	case OFP_XM_T_UDP_DST:
	case OFP_XM_T_SCTP_SRC:
	case OFP_XM_T_SCTP_DST:
	case OFP_XM_T_IPV6_EXTHDR:
		len = sizeof(*ui16);
		if (hasmask)
			len *= 2;

		if ((ui16 = ibuf_seek(ibuf, off, len)) == NULL)
			return (-1);

		if (type == OFP_XM_T_VLAN_VID) {
			/* Remove the VID present bit to display. */
			if (hasmask)
				log_debug("\t\t%d mask %#04x",
				    ntohs(*ui16) & ~OFP_XM_VID_PRESENT,
				    ntohs(*(ui16 + 1)));
			else
				log_debug("\t\t%d",
				    ntohs(*ui16) & ~OFP_XM_VID_PRESENT);
			break;
		}

		if (hasmask)
			log_debug("\t\t%d mask %#04x",
			    ntohs(*ui16), ntohs(*(ui16 + 1)));
		else
			log_debug("\t\t%d", ntohs(*ui16));
		break;

	case OFP_XM_T_IP_DSCP:
	case OFP_XM_T_IP_ECN:
	case OFP_XM_T_ICMPV4_TYPE:
	case OFP_XM_T_ICMPV4_CODE:
	case OFP_XM_T_ICMPV6_TYPE:
	case OFP_XM_T_ICMPV6_CODE:
	case OFP_XM_T_MPLS_TC:
	case OFP_XM_T_MPLS_BOS:
		if ((ui8 = ibuf_seek(ibuf, off, sizeof(*ui8))) == NULL)
			return (-1);

		log_debug("\t\t%#02x", *ui8);
		break;

	case OFP_XM_T_IPV4_SRC:
	case OFP_XM_T_IPV4_DST:
	case OFP_XM_T_ARP_SPA:
	case OFP_XM_T_ARP_TPA:
	case OFP_XM_T_IPV6_FLABEL:
		len = sizeof(*ui32);
		if (hasmask)
			len *= 2;

		if ((ui32 = ibuf_seek(ibuf, off, len)) == NULL)
			return (-1);

		if (hasmask)
			log_debug("\t\t%#08x mask %#08x",
			    ntohl(*ui32), ntohl(*(ui32 + 1)));
		else
			log_debug("\t\t%#08x", ntohl(*ui32));
		break;

	case OFP_XM_T_IPV6_SRC:
	case OFP_XM_T_IPV6_DST:
	case OFP_XM_T_IPV6_ND_TARGET:
		len = sizeof(struct in6_addr);
		if (hasmask)
			len *= 2;

		if ((ui8 = ibuf_seek(ibuf, off, len)) == NULL)
			return (-1);

		buf[0] = 0;
		for (i = 0; i < (int)sizeof(struct in6_addr); i++) {
			snprintf(hex, sizeof(hex), "%02x", *(ui8 + i));
			strlcat(buf, hex, sizeof(buf));
		}

		if (hasmask) {
			maskbuf[0] = 0;
			for (i = 0; i < (int)sizeof(struct in6_addr); i++) {
				snprintf(hex, sizeof(hex), "%02x", *(ui8 +
				    (i + sizeof(struct in6_addr))));
				strlcat(maskbuf, hex, sizeof(maskbuf));
			}
			log_debug("\t\t%s mask %s", buf, maskbuf);
		} else
			log_debug("\t\t%s", buf);
		break;

	case OFP_XM_T_PBB_ISID:
		/* TODO teach me how to read 24 bits and convert to be. */
		break;

	default:
		log_debug("\t\tUnknown type");
		return (-1);
	}

	return (0);
}

int
ofp13_validate_oxm(struct switchd *sc, struct ofp_ox_match *oxm,
    struct ofp_header *oh, struct ibuf *ibuf, off_t off)
{
	uint16_t	 class;
	uint8_t		 type;
	int		 hasmask;

	/* match element is always followed by data */
	if (oxm->oxm_length == 0)
		return (0);

	type = OFP_OXM_GET_FIELD(oxm);
	hasmask = OFP_OXM_GET_HASMASK(oxm);
	class = ntohs(oxm->oxm_class);
	off += sizeof(*oxm);

	log_debug("\tox match class %s type %s hasmask %s length %u",
	    print_map(class, ofp_oxm_c_map),
	    print_map(type, ofp_xm_t_map),
	    hasmask ? "yes" : "no",
	    oxm->oxm_length);

	switch (class) {
	case OFP_OXM_C_NXM_0:
	case OFP_OXM_C_NXM_1:
		/* TODO teach me how to read NXM_*. */
		break;

	case OFP_OXM_C_OPENFLOW_BASIC:
		return (ofp13_validate_oxm_basic(ibuf, off, hasmask, type));

	case OFP_OXM_C_OPENFLOW_EXPERIMENTER:
		/* Implementation dependent: there is nothing to do here. */
		break;

	default:
		return (-1);
	}

	return (0);
}

int
ofp13_validate_packet_in(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_packet_in	*pin;
	struct ofp_match	*om;
	struct ofp_ox_match	*oxm;
	uint8_t			*p;
	ssize_t			 len, mlen, plen;
	off_t			 moff, off;

	off = 0;
	if ((pin = ibuf_seek(ibuf, off, sizeof(*pin))) == NULL)
		return (-1);
	log_debug("\tbuffer %s length %u reason %s table %s cookie 0x%#016llx",
	    print_map(ntohl(pin->pin_buffer_id), ofp_pktout_map),
	    ntohs(pin->pin_total_len),
	    print_map(ntohs(pin->pin_reason), ofp_pktin_map),
	    print_map(pin->pin_table_id, ofp_table_id_map),
	    be64toh(pin->pin_cookie));
	off += offsetof(struct ofp_packet_in, pin_match);

	om = &pin->pin_match;
	mlen = ntohs(om->om_length);
	log_debug("\tmatch type %s length %zu (padded to %zu)",
	    print_map(ntohs(om->om_type), ofp_match_map),
	    mlen, OFP_ALIGN(mlen) + ETHER_ALIGN);
	mlen -= sizeof(*om);

	/* current match offset, aligned offset after all matches */
	moff = off + sizeof(*om);
	off += OFP_ALIGN(mlen) + ETHER_ALIGN;

	switch (htons(om->om_type)) {
	case OFP_MATCH_OXM:
		do {
			if ((oxm = ibuf_seek(ibuf, moff, sizeof(*oxm))) == NULL)
				return (-1);
			if (ofp13_validate_oxm(sc, oxm, oh, ibuf, moff) == -1)
				return (-1);
			moff += sizeof(*oxm) + oxm->oxm_length;
			mlen -= sizeof(*oxm) + oxm->oxm_length;
		} while (mlen > 0 && oxm->oxm_length);
		break;
	case OFP_MATCH_STANDARD:
		/* deprecated */
		break;
	}

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
ofp13_validate_packet_out(struct switchd *sc,
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

	log_debug("\tbuffer %s port %s actions length %u",
	    print_map(ntohl(pout->pout_buffer_id), ofp_pktout_map),
	    print_map(ntohl(pout->pout_in_port), ofp_port_map),
	    ntohs(pout->pout_actions_len));
	len = ntohl(pout->pout_actions_len);

	off += sizeof(*pout);
	while ((ah = ibuf_seek(ibuf, off, len)) != NULL &&
	    ntohs(ah->ah_len) >= (uint16_t)sizeof(*ah)) {
		switch (ntohs(ah->ah_type)) {
		case OFP_ACTION_OUTPUT:
			ao = (struct ofp_action_output *)ah;
			log_debug("\t\taction type %s length %d "
			    "port %s max length %s",
			    print_map(ntohs(ao->ao_type), ofp_action_map),
			    ntohs(ao->ao_len),
			    print_map(ntohs(ao->ao_port), ofp_port_map),
			    print_map(ntohs(ao->ao_max_len),
			    ofp_controller_maxlen_map));
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
ofp13_validate_error(struct switchd *sc,
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
	case OFP_ERRTYPE_BAD_MATCH:
		code = print_map(ntohs(err->err_code), ofp_errmatch_map);
		break;
	case OFP_ERRTYPE_BAD_INSTRUCTION:
		code = print_map(ntohs(err->err_code), ofp_errinst_map);
		break;
	case OFP_ERRTYPE_BAD_REQUEST:
		code = print_map(ntohs(err->err_code), ofp_errreq_map);
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
	if (ofp13_validate(sc, &con->con_peer, &con->con_local, oh, ibuf) != 0)
		return (-1);

	if (ofp13_callbacks[oh->oh_type].cb == NULL) {
		log_debug("%s: message not supported: %s", __func__,
		    print_map(oh->oh_type, ofp_t_map));
		return (-1);
	}
	if (ofp13_callbacks[oh->oh_type].cb(sc, con, oh, ibuf) != 0) {
		log_debug("%s: message parsing failed: %s", __func__,
		    print_map(oh->oh_type, ofp_t_map));
		return (-1);
	}

	return (0);
}

int
ofp13_hello(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	if (switch_add(con) == NULL) {
		log_debug("%s: failed to add switch", __func__);
		return (-1);
	}

	/* Echo back the received Hello packet */
	oh->oh_version = OFP_V_1_3;
	oh->oh_length = htons(sizeof(*oh));
	oh->oh_xid = htonl(con->con_xidnxt++);
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, NULL) != 0)
		return (-1);
	ofp_output(con, oh, NULL);

	/* Ask for switch features so we can get more information. */
	if (ofp13_featuresrequest(sc, con) == -1)
		return (-1);

	return (0);
}

int
ofp13_echo_request(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	/* Echo reply */
	oh->oh_type = OFP_T_ECHO_REPLY;
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, NULL) != 0)
		return (-1);
	ofp_output(con, oh, NULL);

	return (0);
}

int
ofp13_validate_features_reply(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_switch_features	*swf;

	if ((swf = ibuf_seek(ibuf, 0, sizeof(*swf))) == NULL)
		return (-1);

	log_debug("\tdatapath_id %#016llx nbuffers %u ntables %d aux_id %d "
	    "capabilities %#08x",
	    be64toh(swf->swf_datapath_id), ntohl(swf->swf_nbuffers),
	    swf->swf_ntables, swf->swf_aux_id, ntohl(swf->swf_capabilities));
	return (0);
}

int
ofp13_features_reply(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
#if 0
	/* Let's not ask this while we don't use it. */
	ofp13_flow_stats(sc, con, OFP_PORT_ANY, OFP_GROUP_ID_ANY,
	    OFP_TABLE_ID_ALL);
	ofp13_table_features(sc, con, 0);
	ofp13_desc(sc, con);
#endif
	ofp13_setconfig(sc, con, OFP_CONFIG_FRAG_NORMAL,
	    OFP_CONTROLLER_MAXLEN_NO_BUFFER);

	/* Use table '0' for switch(4) and '100' for HP 3800. */
	ofp13_tablemiss_sendctrl(sc, con, 0);

	return (0);
}

int
ofp13_validate_action(struct switchd *sc, struct ofp_header *oh,
    struct ibuf *ibuf, off_t *off, struct ofp_action_header *ah)
{
	struct ofp_action_output	*ao;
	struct ofp_action_mpls_ttl	*amt;
	struct ofp_action_push		*ap;
	struct ofp_action_pop_mpls	*apm;
	struct ofp_action_group		*ag;
	struct ofp_action_nw_ttl	*ant;
	struct ofp_action_set_field	*asf;
	struct ofp_action_set_queue	*asq;
	struct ofp_ox_match		*oxm;
	size_t				 len;
	int				 type;
	off_t				 moff;

	type = ntohs(ah->ah_type);
	len = ntohs(ah->ah_len);

	switch (type) {
	case OFP_ACTION_OUTPUT:
		if (len != sizeof(*ao))
			return (-1);
		if ((ao = ibuf_seek(ibuf, *off, sizeof(*ao))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu port %s max_len %s",
		    print_map(type, ofp_action_map), len,
		    print_map(ntohl(ao->ao_port), ofp_port_map),
		    print_map(ntohs(ao->ao_max_len),
		    ofp_controller_maxlen_map));
		break;
	case OFP_ACTION_SET_MPLS_TTL:
		if (len != sizeof(*amt))
			return (-1);
		if ((amt = ibuf_seek(ibuf, *off, sizeof(*amt))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu ttl %d",
		    print_map(type, ofp_action_map), len, amt->amt_ttl);
		break;
	case OFP_ACTION_PUSH_VLAN:
	case OFP_ACTION_PUSH_MPLS:
	case OFP_ACTION_PUSH_PBB:
		if (len != sizeof(*ap))
			return (-1);
		if ((ap = ibuf_seek(ibuf, *off, sizeof(*ap))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu ethertype %#04x",
		    print_map(type, ofp_action_map), len,
		    ntohs(ap->ap_ethertype));
		break;
	case OFP_ACTION_POP_MPLS:
		if (len != sizeof(*apm))
			return (-1);
		if ((apm = ibuf_seek(ibuf, *off, sizeof(*apm))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu ethertype %#04x",
		    print_map(type, ofp_action_map), len,
		    ntohs(apm->apm_ethertype));
		break;
	case OFP_ACTION_SET_QUEUE:
		if (len != sizeof(*asq))
			return (-1);
		if ((asq = ibuf_seek(ibuf, *off, sizeof(*asq))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu queue_id %u",
		    print_map(type, ofp_action_map), len,
		    ntohl(asq->asq_queue_id));
		break;
	case OFP_ACTION_GROUP:
		if (len != sizeof(*ag))
			return (-1);
		if ((ag = ibuf_seek(ibuf, *off, sizeof(*ag))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu group_id %s",
		    print_map(type, ofp_action_map), len,
		    print_map(ntohl(ag->ag_group_id), ofp_group_id_map));
		break;
	case OFP_ACTION_SET_NW_TTL:
		if (len != sizeof(*ant))
			return (-1);
		if ((ant = ibuf_seek(ibuf, *off, sizeof(*ant))) == NULL)
			return (-1);

		*off += len;
		log_debug("\t\taction %s len %lu ttl %d",
		    print_map(type, ofp_action_map), len, ant->ant_ttl);
		break;
	case OFP_ACTION_SET_FIELD:
		if (len < sizeof(*asf))
			return (-1);
		if ((asf = ibuf_seek(ibuf, *off, sizeof(*asf))) == NULL)
			return (-1);

		moff = *off + sizeof(*asf) - sizeof(asf->asf_field);
		*off += len;
		log_debug("\t\taction %s len %lu",
		    print_map(type, ofp_action_map), len);

		len -= sizeof(*asf) - sizeof(asf->asf_field);
		while (len) {
			if ((oxm = ibuf_seek(ibuf, moff, sizeof(*oxm)))
			    == NULL)
				return (-1);
			if (ofp13_validate_oxm(sc, oxm, oh, ibuf, moff) == -1)
				return (-1);

			len -= sizeof(*oxm) - oxm->oxm_length;
			moff += sizeof(*oxm) - oxm->oxm_length;
		}
		break;

	default:
		if (len < sizeof(*ah))
			return (-1);

		/* Generic header without information. */
		*off += len;
		log_debug("\t\taction %s len %lu",
		    print_map(type, ofp_action_map), len);
		break;
	}

	return (0);
}

int
ofp13_validate_instruction(struct switchd *sc, struct ofp_header *oh,
    struct ibuf *ibuf, off_t *off, struct ofp_instruction *i)
{
	struct ofp_instruction_actions		*ia;
	struct ofp_instruction_goto_table	*igt;
	struct ofp_instruction_write_metadata	*iwm;
	struct ofp_instruction_meter		*im;
	struct ofp_action_header		*ah;
	int					 type;
	size_t					 len;
	off_t					 oldoff, diff;

	type = ntohs(i->i_type);
	len = ntohs(i->i_len);

	switch (type) {
	case OFP_INSTRUCTION_T_GOTO_TABLE:
		if (len != sizeof(*igt))
			return (-1);
		if ((igt = ibuf_seek(ibuf, *off, sizeof(*igt))) == NULL)
			return (-1);

		*off += len;
		log_debug("\tinstruction %s length %lu table_id %s",
		    print_map(type, ofp_instruction_t_map), len,
		    print_map(igt->igt_table_id, ofp_table_id_map));
		break;
	case OFP_INSTRUCTION_T_WRITE_META:
		if (len != sizeof(*iwm))
			return (-1);
		if ((iwm = ibuf_seek(ibuf, *off, sizeof(*iwm))) == NULL)
			return (-1);

		*off += len;
		log_debug("\tinstruction %s length %lu "
		    "metadata %#016llx mask %#016llx",
		    print_map(type, ofp_instruction_t_map), len,
		    be64toh(iwm->iwm_metadata),
		    be64toh(iwm->iwm_metadata_mask));
		break;
	case OFP_INSTRUCTION_T_METER:
		if (len != sizeof(*im))
			return (-1);
		if ((im = ibuf_seek(ibuf, *off, sizeof(*im))) == NULL)
			return (-1);

		*off += len;
		log_debug("\tinstruction %s length %lu meter_id %d",
		    print_map(type, ofp_instruction_t_map), len,
		    im->im_meter_id);
		break;
	case OFP_INSTRUCTION_T_WRITE_ACTIONS:
	case OFP_INSTRUCTION_T_CLEAR_ACTIONS:
	case OFP_INSTRUCTION_T_APPLY_ACTIONS:
		if (len < sizeof(*ia))
			return (-1);
		if ((ia = ibuf_seek(ibuf, *off, sizeof(*ia))) == NULL)
			return (-1);

		log_debug("\tinstruction %s length %lu",
		    print_map(type, ofp_instruction_t_map), len);

		*off += sizeof(*ia);
		len -= sizeof(*ia);
		while (len) {
			oldoff = *off;
			if ((ah = ibuf_seek(ibuf, *off, sizeof(*ah))) == NULL ||
			    ofp13_validate_action(sc, oh, ibuf, off, ah) == -1)
				return (-1);

			diff = *off - oldoff;
			/* Loop prevention. */
			if (*off < oldoff || diff == 0)
				break;

			len -= diff;
		}
		break;
	default:
		if (len < sizeof(*i))
			return (-1);

		log_debug("\tinstruction %s length %lu",
		    print_map(type, ofp_instruction_t_map), len);
		*off += len;
		break;
	}

	return (0);
}

int
ofp13_validate_flow_mod(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_flow_mod		*fm;
	struct ofp_match		*om;
	struct ofp_instruction		*i;
	struct ofp_ox_match		*oxm;
	off_t				 off, moff, offdiff;
	int				 matchlen, matchtype, left;

	off = 0;
	if ((fm = ibuf_seek(ibuf, off, sizeof(*fm))) == NULL)
		return (-1);

	log_debug("\tcommand %s table %s timeout (idle %d hard %d) "
	    "priority %d buffer_id %s out_port %s out_group %s "
	    "flags %#04x cookie %#016llx mask %#016llx",
	    print_map(fm->fm_command, ofp_flowcmd_map),
	    print_map(fm->fm_table_id, ofp_table_id_map),
	    ntohs(fm->fm_idle_timeout), ntohs(fm->fm_hard_timeout),
	    ntohs(fm->fm_priority),
	    print_map(ntohl(fm->fm_buffer_id), ofp_pktout_map),
	    print_map(ntohl(fm->fm_out_port), ofp_port_map),
	    print_map(ntohl(fm->fm_out_group), ofp_group_id_map),
	    ntohs(fm->fm_flags), be64toh(fm->fm_cookie),
	    be64toh(fm->fm_cookie_mask));

	off += offsetof(struct ofp_flow_mod, fm_match);

	om = &fm->fm_match;
	matchtype = ntohs(om->om_type);
	matchlen = ntohs(om->om_length);

	moff = off + sizeof(*om);
	off += OFP_ALIGN(matchlen);

	matchlen -= sizeof(*om);
	while (matchlen) {
		if ((oxm = ibuf_seek(ibuf, moff, sizeof(*oxm))) == NULL ||
		    ofp13_validate_oxm(sc, oxm, oh, ibuf, moff) == -1)
			return (-1);
		moff += sizeof(*oxm) + oxm->oxm_length;
		matchlen -= sizeof(*oxm) + oxm->oxm_length;
	}

	left = ntohs(oh->oh_length) - off;
	moff = off;
	while (left) {
		if ((i = ibuf_seek(ibuf, moff, sizeof(*i))) == NULL ||
		    ofp13_validate_instruction(sc, oh, ibuf, &moff, i) == -1)
			return (-1);

		offdiff = moff - off;
		/* Loop prevention. */
		if (moff < off || offdiff == 0)
			break;

		left -= offdiff;
		off = moff;
	}

	return (0);
}

int
ofp13_packet_match(struct ibuf *ibuf, struct packet *pkt, struct ofp_match *om)
{
	struct ether_header	*eh = pkt->pkt_eh;
	size_t			 padsize, startpos, endpos, omlen;

	if (eh == NULL)
		return (-1);

	startpos = ibuf->wpos;
	if (oxm_etheraddr(ibuf, 1, eh->ether_shost, NULL) == -1)
		return (-1);
	if (oxm_etheraddr(ibuf, 0, eh->ether_dhost, NULL) == -1)
		return (-1);
	endpos = ibuf->wpos;

	omlen = sizeof(*om) + (endpos - startpos);
	padsize = OFP_ALIGN(omlen) - omlen;

	om->om_type = htons(OFP_MATCH_OXM);
	om->om_length = htons(omlen);
	if (padsize && ibuf_advance(ibuf, padsize) == NULL)
		return (-1);

	return (0);
}

int
ofp13_packet_in(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *ih, struct ibuf *ibuf)
{
	struct ofp_packet_in		*pin;
	struct ofp_packet_out		*pout;
	struct ofp_flow_mod		*fm;
	struct ofp_header		*oh;
	struct ofp_match		*om;
	struct ofp_ox_match		*oxm;
	struct packet			 pkt;
	struct ibuf			*obuf = NULL;
	int				 ret = -1;
	ssize_t				 len, mlen;
	uint32_t			 srcport = 0, dstport;
	int				 addflow = 0, sendbuffer = 0;
	off_t				 off, moff;
	void				*ptr;
	struct ofp_instruction_actions	*ia;

	if ((pin = ibuf_getdata(ibuf, sizeof(*pin))) == NULL)
		return (-1);

	/* We only handle no matches right now. */
	if (pin->pin_reason != OFP_PKTIN_REASON_NO_MATCH)
		return (-1);

	bzero(&pkt, sizeof(pkt));
	len = ntohs(pin->pin_total_len);

	/* very basic way of getting the source port */
	om = &pin->pin_match;
	mlen = ntohs(om->om_length);
	off = (OFP_ALIGN(mlen) + ETHER_ALIGN) - sizeof(pin->pin_match);
	moff = ibuf_dataoffset(ibuf);

	do {
		if ((oxm = ibuf_seek(ibuf, moff, sizeof(*oxm))) == NULL)
			return (-1);

		/* Find IN_PORT */
		switch (ntohs(oxm->oxm_class)) {
		case OFP_OXM_C_OPENFLOW_BASIC:
			switch (OFP_OXM_GET_FIELD(oxm)) {
			case OFP_XM_T_IN_PORT:
				moff += sizeof(*oxm);
				if ((ptr = ibuf_seek(ibuf, moff,
				    sizeof(srcport))) == NULL)
					return (-1);
				srcport = htonl(*(uint32_t *)ptr);
				mlen = 0; /* break loop */
				break;
			default:
				/* ignore unsupported match types */
				break;
			}
		default:
			/* ignore unsupported match classes */
			break;
		}
		moff += sizeof(*oxm) + oxm->oxm_length;
		mlen -= sizeof(*oxm) + oxm->oxm_length;
	} while (mlen > 0 && oxm->oxm_length);

	/* Skip all matches and seek to the packet */
	if (ibuf_getdata(ibuf, off) == NULL)
		return (-1);

	if (packet_input(sc, con->con_switch,
	    srcport, &dstport, ibuf, len, &pkt) == -1 ||
	    (dstport > OFP_PORT_MAX &&
	    dstport != OFP_PORT_LOCAL &&
	    dstport != OFP_PORT_CONTROLLER)) {
		/* fallback to flooding */
		dstport = OFP_PORT_FLOOD;
	} else if (srcport == dstport) {
		/*
		 * silently drop looping packet
		 * (don't use OFP_PORT_INPUT here)
		 */
		dstport = OFP_PORT_ANY;
	} else {
		addflow = 1;
	}

	if ((obuf = ibuf_static()) == NULL)
		goto done;

 again:
	if (addflow) {
		if ((fm = ibuf_advance(obuf, sizeof(*fm))) == NULL)
			goto done;

		oh = &fm->fm_oh;
		fm->fm_cookie = 0; /* XXX should we set a cookie? */
		fm->fm_command = OFP_FLOWCMD_ADD;
		fm->fm_idle_timeout = htons(sc->sc_cache_timeout);
		fm->fm_hard_timeout = 0; /* permanent */
		fm->fm_priority = 0;
		fm->fm_buffer_id = pin->pin_buffer_id;
		fm->fm_flags = htons(OFP_FLOWFLAG_SEND_FLOW_REMOVED);
		if (pin->pin_buffer_id == OFP_PKTOUT_NO_BUFFER)
			sendbuffer = 1;

		/* Write flow matches to create an entry. */
		if (ofp13_packet_match(obuf, &pkt, &fm->fm_match) == -1)
			goto done;

		/*
		 * Write the instruction action header and add the output
		 * action.
		 */
		if ((ia = ibuf_advance(obuf, sizeof(*ia))) == NULL ||
		    action_output(obuf, dstport,
		    OFP_CONTROLLER_MAXLEN_NO_BUFFER) == -1)
			goto done;

		ia->ia_type = htons(OFP_INSTRUCTION_T_APPLY_ACTIONS);
		ia->ia_len = htons(sizeof(*ia) +
		    sizeof(struct ofp_action_output));
	} else {
		if ((pout = ibuf_advance(obuf, sizeof(*pout))) == NULL)
			goto done;

		oh = &pout->pout_oh;
		pout->pout_buffer_id = pin->pin_buffer_id;
		pout->pout_in_port = htonl(srcport);
		pout->pout_actions_len =
		    htons(sizeof(struct ofp_action_output));

		if (action_output(obuf, dstport,
		    OFP_CONTROLLER_MAXLEN_NO_BUFFER) == -1)
			goto done;

		/* Add optional packet payload */
		if (pin->pin_buffer_id == OFP_PKTOUT_NO_BUFFER &&
		    imsg_add(obuf, pkt.pkt_buf, pkt.pkt_len) == -1)
			goto done;
	}

	/* Set output header */
	oh->oh_version = OFP_V_1_3;
	oh->oh_length = htons(ibuf_length(obuf));
	oh->oh_type = addflow ? OFP_T_FLOW_MOD : OFP_T_PACKET_OUT;
	oh->oh_xid = htonl(con->con_xidnxt++);

	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, obuf) != 0)
		goto done;

	ofp_output(con, NULL, obuf);

	if (sendbuffer) {
		ibuf_release(obuf);

		/* loop to output the packet again */
		addflow = sendbuffer = 0;
		if ((obuf = ibuf_static()) == NULL)
			goto done;
		goto again;
	}

	ret = 0;
 done:
	ibuf_release(obuf);
	return (ret);
}

int
ofp13_flow_removed(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *ih, struct ibuf *ibuf)
{
	struct ofp_flow_removed		*fr;

	if ((fr = ibuf_getdata(ibuf, sizeof(*fr))) == NULL)
		return (-1);

	log_debug("cookie %#016llx priority %d reason %s table_id %s "
	    "duration(%u sec, %u nsec) timeout idle %d hard %d "
	    "packet %llu byte %llu",
	    be64toh(fr->fr_cookie), ntohs(fr->fr_priority),
	    print_map(fr->fr_reason, ofp_flowrem_reason_map),
	    print_map(fr->fr_table_id, ofp_table_id_map),
	    ntohl(fr->fr_duration_sec), ntohl(fr->fr_duration_nsec),
	    ntohs(fr->fr_idle_timeout), ntohs(fr->fr_hard_timeout),
	    be64toh(fr->fr_packet_count), be64toh(fr->fr_byte_count));

	return (0);
}

int
ofp13_parse_instruction(struct ibuf *ibuf, struct ofp_instruction *i)
{
	int			 type;
	int			 len;

	type = ntohs(i->i_type);
	len = ntohs(i->i_len);

	log_debug("\t\t%s", print_map(type, ofp_instruction_t_map));

	return (len);
}

int
ofp13_parse_action(struct ibuf *ibuf, struct ofp_action_header *ah)
{
	int				 len, type;

	len = htons(ah->ah_len);
	type = htons(ah->ah_type);

	log_debug("\t\t%s", print_map(type, ofp_action_map));

	return (len);
}

int
ofp13_parse_oxm(struct ibuf *ibuf, struct ofp_ox_match *oxm)
{
	int			length, type, class, hasmask;

	class = ntohs(oxm->oxm_class);
	type = OFP_OXM_GET_FIELD(oxm);
	hasmask = OFP_OXM_GET_HASMASK(oxm);
	/*
	 * XXX the OpenFlow 1.3.5 specification says this field is only
	 * 4 bytes long, however the experimental type is 8 bytes.
	 */
	length = sizeof(*oxm);

	log_debug("\t\t%s hasmask %s type %s",
	    print_map(class, ofp_oxm_c_map), hasmask ? "yes" : "no",
	    print_map(type, ofp_xm_t_map));

	if (class == OFP_OXM_C_OPENFLOW_EXPERIMENTER) {
		/* Get the last bytes. */
		if (ibuf_getdata(ibuf, 4) == NULL)
			return (-1);

		return (8);
	}

	return (length);
}

int
ofp13_parse_tableproperties(struct ibuf *ibuf, struct ofp_table_features *tf)
{
	struct ofp_table_feature_property	*tp;
	struct ofp_instruction			*i;
	struct ofp_action_header		*ah;
	struct ofp_ox_match			*oxm;
	uint8_t					*next_table;
	int					 remaining, type, length;
	int					 totallen, padsize, rv;

	log_debug("Table %s (%d) max_entries %u config %u "
	    "metadata match %#016llx write %#016llx",
	    tf->tf_name, tf->tf_tableid, ntohl(tf->tf_max_entries),
	    ntohl(tf->tf_config), be64toh(tf->tf_metadata_match),
	    be64toh(tf->tf_metadata_write));
	totallen = htons(tf->tf_length);
	remaining = totallen - sizeof(*tf);

 next_table_property:
	if ((tp = ibuf_getdata(ibuf, sizeof(*tp))) == NULL)
		return (-1);

	type = ntohs(tp->tp_type);
	length = ntohs(tp->tp_length);

	/* Calculate the padding. */
	padsize = OFP_ALIGN(length) - length;
	remaining -= OFP_ALIGN(length);
	length -= sizeof(*tp);

	log_debug("\t%s:", print_map(type, ofp_table_featprop_map));
	if (length == 0)
		log_debug("\t\tNONE");

	switch (type) {
	case OFP_TABLE_FEATPROP_INSTRUCTION:
	case OFP_TABLE_FEATPROP_INSTRUCTION_MISS:
		while (length) {
			if ((i = ibuf_getdata(ibuf, sizeof(*i))) == NULL)
				return (-1);
			if ((rv = ofp13_parse_instruction(ibuf, i)) == -1)
				return (-1);
			length -= rv;
		}
		break;

	case OFP_TABLE_FEATPROP_NEXT_TABLES:
	case OFP_TABLE_FEATPROP_NEXT_TABLES_MISS:
		while (length) {
			if ((next_table = ibuf_getdata(ibuf,
			    sizeof(*next_table))) == NULL)
				return (-1);

			log_debug("\t\t%d", *next_table);
			length -= sizeof(*next_table);
		}
		break;

	case OFP_TABLE_FEATPROP_WRITE_ACTIONS:
	case OFP_TABLE_FEATPROP_WRITE_ACTIONS_MISS:
	case OFP_TABLE_FEATPROP_APPLY_ACTIONS:
	case OFP_TABLE_FEATPROP_APPLY_ACTIONS_MISS:
		while (length) {
			/*
			 * XXX the OpenFlow 1.3.5 specs says that we only
			 * get 4 bytes here instead of the full OXM.
			 */
			if ((ah = ibuf_getdata(ibuf, 4)) == NULL)
				return (-1);
			if ((rv = ofp13_parse_action(ibuf, ah)) == -1)
				return (-1);
			length -= rv;
		}
		break;

	case OFP_TABLE_FEATPROP_MATCH:
	case OFP_TABLE_FEATPROP_WILDCARDS:
	case OFP_TABLE_FEATPROP_WRITE_SETFIELD:
	case OFP_TABLE_FEATPROP_WRITE_SETFIELD_MISS:
	case OFP_TABLE_FEATPROP_APPLY_SETFIELD:
	case OFP_TABLE_FEATPROP_APPLY_SETFIELD_MISS:
		while (length) {
			if ((oxm = ibuf_getdata(ibuf, sizeof(*oxm))) == NULL)
				return (-1);
			if ((rv = ofp13_parse_oxm(ibuf, oxm)) == -1)
				return (-1);
			length -= rv;
		}
		break;

	default:
		log_debug("%s: unsupported property type: %d", __func__, type);

		/* Skip this field and try to continue otherwise fail. */
		if (ibuf_getdata(ibuf, length) == NULL)
			return (-1);

		break;
	}

	/* Skip the padding and read the next property if any. */
	if (padsize && ibuf_getdata(ibuf, padsize) == NULL)
		return (-1);
	if (remaining)
		goto next_table_property;

	return (totallen);
}

int
ofp13_multipart_reply(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_multipart		*mp;
	struct ofp_table_features	*tf;
	int				 readlen, type, flags;
	int				 remaining;

	if ((mp = ibuf_getdata(ibuf, sizeof(*mp))) == NULL)
		return (-1);

	type = ntohs(mp->mp_type);
	flags = ntohs(mp->mp_flags);
	remaining = ntohs(oh->oh_length) - sizeof(*mp);

	if (flags & OFP_MP_FLAG_REPLY_MORE) {
		if (ofp_multipart_add(con, oh->oh_xid, type) == -1) {
			ofp13_error(sc, con, oh, ibuf,
			    OFP_ERRTYPE_BAD_REQUEST,
			    OFP_ERRREQ_MULTIPART_OVERFLOW);
			ofp_multipart_del(con, oh->oh_xid);
			return (0);
		}

		/*
		 * We don't need to concatenate with other messages,
		 * because the specification says that switches don't
		 * break objects. We should only need this if our parser
		 * requires the whole data before hand, but then we have
		 * better ways to achieve the same thing.
		 */
	} else
		ofp_multipart_del(con, oh->oh_xid);

	switch (type) {
	case OFP_MP_T_TABLE_FEATURES:
 read_next_table:
		if ((tf = ibuf_getdata(ibuf, sizeof(*tf))) == NULL)
			return (-1);
		if ((readlen = ofp13_parse_tableproperties(ibuf, tf)) == -1)
			return (-1);

		remaining -= readlen;
		if (remaining)
			goto read_next_table;
		break;
	}

	return (0);
}

int
ofp13_multipart_reply_validate(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_multipart		*mp;
	struct ofp_table_features	*tf;
	struct ofp_flow_stats		*fs;
	struct ofp_desc			*d;
	struct ofp_match		*om;
	struct ofp_ox_match		*oxm;
	int				 mptype, mpflags, hlen;
	int				 remaining, matchlen, matchtype;
	off_t				 off, moff;

	remaining = ntohs(oh->oh_length);

	off = 0;
	if ((mp = ibuf_seek(ibuf, off, sizeof(*mp))) == NULL)
		return (-1);

	mptype = ntohs(mp->mp_type);
	mpflags = ntohs(mp->mp_flags);
	log_debug("\ttype %s flags %#04x",
	    print_map(mptype, ofp_mp_t_map), mpflags);

	off += sizeof(*mp);
	remaining -= sizeof(*mp);
	if (remaining == 0) {
		log_debug("\tempty reply");
		return (0);
	}

	switch (mptype) {
	case OFP_MP_T_DESC:
		if ((d = ibuf_seek(ibuf, off, sizeof(*d))) == NULL)
			return (-1);

		off += sizeof(*d);
		remaining -= sizeof(*d);
		log_debug("\tmfr_desc \"%s\" hw_desc \"%s\" sw_desc \"%s\" "
		    "serial_num \"%s\" dp_desc \"%s\"",
		    d->d_mfr_desc, d->d_hw_desc, d->d_sw_desc,
		    d->d_serial_num, d->d_dp_desc);
		break;

	case OFP_MP_T_FLOW:
 read_next_flow:
		if ((fs = ibuf_seek(ibuf, off, sizeof(*fs))) == NULL)
			return (-1);

		hlen = ntohs(fs->fs_length);
		remaining -= hlen;
		moff = off + sizeof(*fs);
		off += hlen;

		log_debug("\tflow length %d table_id %s duration sec %u "
		    "nsec %u prio %d timeout idle %d hard %d flags %#04x "
		    "cookie %#016llx packet count %llu byte count %llu",
		    hlen, print_map(fs->fs_table_id, ofp_table_id_map),
		    ntohl(fs->fs_duration_sec), ntohl(fs->fs_duration_nsec),
		    ntohs(fs->fs_priority), ntohs(fs->fs_idle_timeout),
		    ntohs(fs->fs_hard_timeout), ntohs(fs->fs_flags),
		    be64toh(fs->fs_cookie), be64toh(fs->fs_packet_count),
		    be64toh(fs->fs_byte_count));

		om = &fs->fs_match;
		matchtype = ntohs(om->om_type);
		matchlen = ntohs(om->om_length) - sizeof(*om);

		/* We don't know how to parse anything else yet. */
		if (matchtype != OFP_MATCH_OXM)
			break;

		while (matchlen) {
			if ((oxm = ibuf_seek(ibuf, moff, sizeof(*oxm))) == NULL)
				return (-1);
			if (ofp13_validate_oxm(sc, oxm, oh, ibuf, moff) == -1)
				return (-1);
			moff += sizeof(*oxm) + oxm->oxm_length;
			matchlen -= sizeof(*oxm) + oxm->oxm_length;
		}
		if (remaining)
			goto read_next_flow;
		break;

	case OFP_MP_T_AGGREGATE:
	case OFP_MP_T_TABLE:
	case OFP_MP_T_PORT_STATS:
	case OFP_MP_T_QUEUE:
	case OFP_MP_T_GROUP:
	case OFP_MP_T_GROUP_DESC:
	case OFP_MP_T_GROUP_FEATURES:
	case OFP_MP_T_METER:
	case OFP_MP_T_METER_CONFIG:
	case OFP_MP_T_METER_FEATURES:
		break;

	case OFP_MP_T_TABLE_FEATURES:
 read_next_table:
		if ((tf = ibuf_seek(ibuf, off, sizeof(*tf))) == NULL)
			return (-1);

		hlen = ntohs(tf->tf_length);
		log_debug("\ttable features length %d tableid %s name \"%s\" "
		    "config %u max_entries %u "
		    "metadata match %#016llx write %#016llx",
		    hlen, print_map(tf->tf_tableid, ofp_table_id_map),
		    tf->tf_name,ntohl(tf->tf_config),
		    ntohl(tf->tf_max_entries),
		    be64toh(tf->tf_metadata_match),
		    be64toh(tf->tf_metadata_write));
		remaining -= hlen;
		off += hlen;
		if (remaining)
			goto read_next_table;
		break;

	case OFP_MP_T_PORT_DESC:
	case OFP_MP_T_EXPERIMENTER:
		break;

	default:
		return (-1);
	}

	return (0);
}

/* Don't forget to update mp->mp_oh.oh_length */
struct ofp_multipart *
ofp13_multipart_request(struct switch_connection *con, struct ibuf *ibuf,
    uint16_t type, uint16_t flags)
{
	struct ofp_multipart		*mp;
	struct ofp_header		*oh;

	if ((mp = ibuf_advance(ibuf, sizeof(*mp))) == NULL)
		return (NULL);

	oh = &mp->mp_oh;
	oh->oh_version = OFP_V_1_3;
	oh->oh_type = OFP_T_MULTIPART_REQUEST;
	oh->oh_xid = htonl(con->con_xidnxt++);
	mp->mp_type = htons(type);
	mp->mp_flags = htons(flags);
	return (mp);
}

int
ofp13_multipart_request_validate(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_multipart		*mp;
	struct ofp_match		*om;
	struct ofp_flow_stats_request	*fsr;
	struct ofp_ox_match		*oxm;
	off_t				 off, moff;
	int				 type, flags, totallen;
	int				 matchlen, matchtype;

	off = 0;
	if ((mp = ibuf_seek(ibuf, off, sizeof(*mp))) == NULL)
		return (-1);

	type = ntohs(mp->mp_type);
	flags = ntohs(mp->mp_flags);
	log_debug("\ttype %s flags %#04x",
	    print_map(type, ofp_mp_t_map), flags);

	totallen = ntohs(oh->oh_length);
	off += sizeof(*mp);

	switch (type) {
	case OFP_MP_T_DESC:
		/* This type doesn't use a payload. */
		if (totallen != sizeof(*mp))
			return (-1);
		break;

	case OFP_MP_T_FLOW:
		if ((fsr = ibuf_seek(ibuf, off, sizeof(*fsr))) == NULL)
			return (-1);

		om = &fsr->fsr_match;
		matchtype = ntohs(om->om_type);
		matchlen = ntohs(om->om_length);
		log_debug("\ttable_id %s out_port %s out_group %s "
		    "cookie %#016llx mask %#016llx match type %s length %d "
		    "(padded to %d)",
		    print_map(fsr->fsr_table_id, ofp_table_id_map),
		    print_map(ntohl(fsr->fsr_out_port), ofp_port_map),
		    print_map(ntohl(fsr->fsr_out_group), ofp_group_id_map),
		    be64toh(fsr->fsr_cookie), be64toh(fsr->fsr_cookie_mask),
		    print_map(matchtype, ofp_match_map), matchlen,
		    OFP_ALIGN(matchlen));

		/* Get the first OXM byte offset. */
		moff = off + sizeof(*fsr);

		/* Don't count the header bytes. */
		matchlen -= sizeof(*om);

		/* We don't know how to parse anything else yet. */
		if (matchtype != OFP_MATCH_OXM)
			break;

		while (matchlen) {
			if ((oxm = ibuf_seek(ibuf, moff, sizeof(*oxm))) == NULL)
				return (-1);
			if (ofp13_validate_oxm(sc, oxm, oh, ibuf, moff) == -1)
				return (-1);
			moff += sizeof(*oxm) + oxm->oxm_length;
			matchlen -= sizeof(*oxm) + oxm->oxm_length;
		}
		break;

	case OFP_MP_T_AGGREGATE:
	case OFP_MP_T_TABLE:
	case OFP_MP_T_PORT_STATS:
	case OFP_MP_T_QUEUE:
	case OFP_MP_T_GROUP:
	case OFP_MP_T_GROUP_DESC:
	case OFP_MP_T_GROUP_FEATURES:
	case OFP_MP_T_METER:
	case OFP_MP_T_METER_CONFIG:
	case OFP_MP_T_METER_FEATURES:
		break;

	case OFP_MP_T_TABLE_FEATURES:
		if (totallen == sizeof(*mp)) {
			log_debug("\tempty table properties request");
			break;
		}
		break;

	case OFP_MP_T_PORT_DESC:
	case OFP_MP_T_EXPERIMENTER:
		break;

	default:
		return (-1);
	}

	return (0);
}

int
ofp13_desc(struct switchd *sc, struct switch_connection *con)
{
	struct ofp_header		*oh;
	struct ofp_multipart		*mp;
	struct ibuf			*ibuf;
	int				 rv = -1;

	if ((ibuf = ibuf_static()) == NULL)
		return (-1);

	if ((mp = ofp13_multipart_request(con, ibuf, OFP_MP_T_DESC, 0)) == NULL)
		goto done;

	oh = &mp->mp_oh;
	oh->oh_length = htons(sizeof(*mp));
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, ibuf) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

int
ofp13_flow_stats(struct switchd *sc, struct switch_connection *con,
    uint32_t out_port, uint32_t out_group, uint8_t table_id)
{
	struct ofp_header		*oh;
	struct ofp_multipart		*mp;
	struct ofp_flow_stats_request	*fsr;
	struct ofp_match		*om;
	struct ibuf			*ibuf;
	int				 padsize, rv = -1;

	if ((ibuf = ibuf_static()) == NULL)
		return (-1);

	if ((mp = ofp13_multipart_request(con, ibuf, OFP_MP_T_FLOW, 0)) == NULL)
		goto done;
	if ((fsr = ibuf_advance(ibuf, sizeof(*fsr))) == NULL)
		goto done;

	oh = &mp->mp_oh;
	fsr->fsr_table_id = table_id;
	fsr->fsr_out_port = htonl(out_port);
	fsr->fsr_out_group = htonl(out_group);

	om = &fsr->fsr_match;
	om->om_type = htons(OFP_MATCH_OXM);
	om->om_length = htons(sizeof(*om));
	padsize = OFP_ALIGN(sizeof(*om)) - sizeof(*om);
	if (padsize && ibuf_advance(ibuf, padsize) == NULL)
		goto done;

	oh->oh_length = htons(ibuf_length(ibuf));
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, ibuf) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

int
ofp13_table_features(struct switchd *sc, struct switch_connection *con,
    uint8_t tableid)
{
	struct ofp_header		*oh;
	struct ofp_multipart		*mp;
	struct ibuf			*ibuf;
	int				 rv = -1;

	if ((ibuf = ibuf_static()) == NULL)
		return (-1);

	if ((mp = ofp13_multipart_request(con, ibuf,
	    OFP_MP_T_TABLE_FEATURES, 0)) == NULL)
		goto done;

	oh = &mp->mp_oh;
	oh->oh_length = htons(sizeof(*mp));
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, ibuf) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

int
ofp13_error(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf, uint16_t type, uint16_t code)
{
	struct ibuf		*obuf;
	struct ofp_error	*err;
	struct ofp_header	*header;
	int			 dlen, rv = -1;

	if ((obuf = ibuf_static()) == NULL ||
	    (err = ibuf_advance(obuf, sizeof(*err))) == NULL)
		goto done;

	header = &err->err_oh;
	err->err_type = htons(type);
	err->err_code = htons(code);

	/* Copy the first message bytes for the error payload. */
	dlen = ibuf_size(ibuf);
	if (dlen > OFP_ERRDATA_MAX)
		dlen = OFP_ERRDATA_MAX;
	if (ibuf_add(obuf, ibuf_seek(ibuf, 0, dlen), dlen) == -1)
		goto done;

	header->oh_version = OFP_V_1_3;
	header->oh_type = OFP_T_ERROR;
	header->oh_length = htons(ibuf_length(obuf));
	header->oh_xid = oh->oh_xid;
	if (ofp13_validate(sc, &con->con_peer, &con->con_local, header,
	    obuf) == -1)
		goto done;

	rv = ofp_output(con, NULL, obuf);

 done:
	ibuf_free(obuf);
	return (rv);
}

/*
 * The valid commands for groups are:
 * OFP_GROUPCMD_{ADD,MODIFY,DELETE}
 *
 * The valid type for groups are:
 * OFP_GROUP_T_{ALL,SELECT,INDIRECT,FAST_FAILOVER}
 *
 * You have to update the gm->gm_oh.oh_length = htons(ibuf_length(ibuf));
 */
struct ofp_group_mod *
ofp13_group(struct switch_connection *con, struct ibuf *ibuf,
    uint32_t gid, uint16_t cmd, uint8_t type)
{
	struct ofp_group_mod		*gm;
	struct ofp_header		*oh;

	if ((gm = ibuf_advance(ibuf, sizeof(*gm))) == NULL)
		return (NULL);

	oh = &gm->gm_oh;
	oh->oh_version = OFP_V_1_3;
	oh->oh_type = OFP_T_GROUP_MOD;
	oh->oh_xid = htonl(con->con_xidnxt++);
	gm->gm_command = htons(cmd);
	gm->gm_type = type;
	gm->gm_group_id = htonl(gid);
	return (gm);
}

/* Remember to update b->b_len. */
struct ofp_bucket *
ofp13_bucket(struct ibuf *ibuf, uint16_t weight, uint32_t watchport,
    uint32_t watchgroup)
{
	struct ofp_bucket		*b;

	if ((b = ibuf_advance(ibuf, sizeof(*b))) == NULL)
		return (NULL);

	b->b_weight = htons(weight);
	b->b_watch_port = htonl(watchport);
	b->b_watch_group = htonl(watchgroup);
	return (b);
}

int
ofp13_setconfig_validate(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_switch_config	*cfg;

	if ((cfg = ibuf_seek(ibuf, 0, sizeof(*cfg))) == NULL)
		return (-1);

	log_debug("\tflags %#04x miss_send_len %s",
	    ntohs(cfg->cfg_flags), print_map(ntohs(cfg->cfg_miss_send_len),
	    ofp_controller_maxlen_map));
	return (0);
}

int
ofp13_setconfig(struct switchd *sc, struct switch_connection *con,
     uint16_t flags, uint16_t misslen)
{
	struct ibuf			*ibuf;
	struct ofp_switch_config	*cfg;
	struct ofp_header		*oh;
	int				 rv = -1;

	if ((ibuf = ibuf_static()) == NULL ||
	    (cfg = ibuf_advance(ibuf, sizeof(*cfg))) == NULL)
		goto done;

	cfg->cfg_flags = htons(flags);
	cfg->cfg_miss_send_len = htons(misslen);

	oh = &cfg->cfg_oh;
	oh->oh_version = OFP_V_1_3;
	oh->oh_type = OFP_T_SET_CONFIG;
	oh->oh_length = htons(ibuf_length(ibuf));
	oh->oh_xid = htonl(con->con_xidnxt++);
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, ibuf) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

int
ofp13_featuresrequest(struct switchd *sc, struct switch_connection *con)
{
	struct ibuf			*ibuf;
	struct ofp_header		*oh;
	int				 rv = -1;

	if ((ibuf = ibuf_static()) == NULL ||
	    (oh = ibuf_advance(ibuf, sizeof(*oh))) == NULL)
		goto done;

	oh->oh_version = OFP_V_1_3;
	oh->oh_type = OFP_T_FEATURES_REQUEST;
	oh->oh_length = htons(ibuf_length(ibuf));
	oh->oh_xid = htonl(con->con_xidnxt++);
	if (ofp13_validate(sc, &con->con_local, &con->con_peer, oh, ibuf) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

/*
 * Flow modification message.
 *
 * After the flow-mod header we have N OXM filters to match packets, when
 * you finish adding them you must update match header:
 * fm_match.om_length = sizeof(fm_match) + OXM length.
 *
 * Then you must add flow instructions and update the OFP header length:
 * fm_oh.oh_length =
 *     sizeof(*fm) + (fm_match.om_len - sizeof(fm_match)) + instructionslen.
 * or
 * fm_oh.oh_length = ibuf_length(ibuf).
 *
 * Note on match payload:
 * After adding all matches and before starting to insert instructions you
 * must add the mandatory padding to fm_match. You can calculate the padding
 * size with this formula:
 * padsize = OFP_ALIGN(fm_match.om_length) - fm_match.om_length;
 *
 * Note on Table-miss:
 * To make a table miss you need to set priority 0 and don't add any
 * matches, just instructions.
 */
struct ofp_flow_mod *
ofp13_flowmod(struct switch_connection *con, struct ibuf *ibuf,
    uint8_t cmd, uint8_t table, uint16_t idleto, uint16_t hardto,
    uint16_t prio)
{
	struct ofp_flow_mod		*fm;

	if ((fm = ibuf_advance(ibuf, sizeof(*fm))) == NULL)
		return (NULL);

	fm->fm_oh.oh_version = OFP_V_1_3;
	fm->fm_oh.oh_type = OFP_T_FLOW_MOD;
	fm->fm_oh.oh_length = htons(sizeof(*fm));
	fm->fm_oh.oh_xid = htonl(con->con_xidnxt++);

	fm->fm_command = cmd;
	fm->fm_buffer_id = htonl(OFP_PKTOUT_NO_BUFFER);
	fm->fm_flags = htons(OFP_FLOWFLAG_SEND_FLOW_REMOVED);

	fm->fm_match.om_type = htons(OFP_MATCH_OXM);
	fm->fm_match.om_length = htons(sizeof(fm->fm_match));

	return (fm);
}

int
ofp13_tablemiss_sendctrl(struct switchd *sc, struct switch_connection *con,
    uint8_t table)
{
	struct oflowmod_ctx	 ctx;
	struct ibuf		*ibuf;
	int			 ret;

	if ((ibuf = oflowmod_open(&ctx, con, NULL, OFP_V_1_3)) == NULL)
		goto err;

	if (oflowmod_iopen(&ctx) == -1)
		goto err;

	/* Update header */
	ctx.ctx_fm->fm_table_id = table;

	if (oflowmod_instruction(&ctx,
	    OFP_INSTRUCTION_T_APPLY_ACTIONS) == -1)
		goto err;
	if (action_output(ibuf, OFP_PORT_CONTROLLER,
	    OFP_CONTROLLER_MAXLEN_MAX) == -1)
		goto err;

	if (oflowmod_iclose(&ctx) == -1)
		goto err;
	if (oflowmod_close(&ctx) == -1)
		goto err;

	if (ofp13_validate(sc, &con->con_local, &con->con_peer,
	    &ctx.ctx_fm->fm_oh, ibuf) != 0)
		goto err;

	ret = ofp_output(con, NULL, ibuf);
	ibuf_release(ibuf);

	return (ret);

 err:
	(void)oflowmod_err(&ctx, __func__, __LINE__);
	return (-1);
}
