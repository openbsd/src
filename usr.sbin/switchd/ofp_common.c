/*	$OpenBSD: ofp_common.c,v 1.11 2019/11/21 06:22:57 akoshibe Exp $	*/

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

int		ofp_setversion(struct switch_connection *, int);

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

int
ofp_validate(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf, uint8_t version)
{
	switch (version) {
	case OFP_V_1_0:
		return (ofp10_validate(sc, src, dst, oh, ibuf));
	case OFP_V_1_3:
		return (ofp13_validate(sc, src, dst, oh, ibuf));
	default:
		return (-1);
	}

	/* NOTREACHED */
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
ofp_send_hello(struct switchd *sc, struct switch_connection *con, int version)
{
	struct ofp_hello_element_header	*he;
	struct ofp_header		*oh;
	struct ibuf			*ibuf;
	size_t				 hstart, hend;
	uint32_t			*bmp;
	int				 rv = -1;

	if ((ibuf = ibuf_static()) == NULL ||
	    (oh = ibuf_advance(ibuf, sizeof(*oh))) == NULL ||
	    (he = ibuf_advance(ibuf, sizeof(*he))) == NULL)
		goto done;

	/* Write down all versions we support. */
	hstart = ibuf->wpos;
	if ((bmp = ibuf_advance(ibuf, sizeof(*bmp))) == NULL)
		goto done;

	*bmp = htonl((1 << OFP_V_1_0) | (1 << OFP_V_1_3));
	hend = ibuf->wpos;

	/* Fill the headers. */
	oh->oh_version = version;
	oh->oh_type = OFP_T_HELLO;
	oh->oh_length = htons(ibuf_length(ibuf));
	oh->oh_xid = htonl(con->con_xidnxt++);
	he->he_type = htons(OFP_HELLO_T_VERSION_BITMAP);
	he->he_length = htons(sizeof(*he) + (hend - hstart));

	if (ofp_validate(sc, &con->con_local, &con->con_peer, oh, ibuf,
	    version) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

int
ofp_validate_hello(struct switchd *sc,
    struct sockaddr_storage *src, struct sockaddr_storage *dst,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_hello_element_header	*he;
	uint32_t			*bmp;
	off_t				 poff;
	int				 helen, i, ver;

	/* No extra element headers. */
	if (ntohs(oh->oh_length) == sizeof(*oh))
		return (0);

	/* Test for supported element headers. */
	if ((he = ibuf_seek(ibuf, sizeof(*oh), sizeof(*he))) == NULL)
		return (-1);
	if (he->he_type != htons(OFP_HELLO_T_VERSION_BITMAP))
		return (-1);

	log_debug("\tversion bitmap:");

	/* Validate header sizes. */
	helen = ntohs(he->he_length);
	if (helen < (int)sizeof(*he))
		return (-1);
	else if (helen == sizeof(*he))
		return (0);

	helen -= sizeof(*he);
	/* Invalid bitmap size. */
	if ((helen % sizeof(*bmp)) != 0)
		return (-1);

	ver = 0;
	poff = sizeof(*oh) + sizeof(*he);
	while (helen > 0) {
		if ((bmp = ibuf_seek(ibuf, poff, sizeof(*bmp))) == NULL)
			return (-1);

		for (i = 0; i < 32; i++, ver++) {
			if ((ntohl(*bmp) & (1 << i)) == 0)
				continue;

			log_debug("\t\tversion %s",
			    print_map(ver, ofp_v_map));
		}

		helen -= sizeof(*bmp);
		poff += sizeof(*bmp);
	}

	return (0);
}

int
ofp_setversion(struct switch_connection *con, int version)
{
	switch (version) {
	case OFP_V_1_0:
	case OFP_V_1_3:
		con->con_version = version;
		return (0);

	default:
		return (-1);
	}
}

int
ofp_recv_hello(struct switchd *sc, struct switch_connection *con,
    struct ofp_header *oh, struct ibuf *ibuf)
{
	struct ofp_hello_element_header	*he;
	uint32_t			*bmp;
	off_t				 poff;
	int				 helen, i, ver;

	/* No extra element headers, just use the header version. */
	if (ntohs(oh->oh_length) == sizeof(*oh))
		return (ofp_setversion(con, oh->oh_version));

	/* Read the element header. */
	if ((he = ibuf_seek(ibuf, sizeof(*oh), sizeof(*he))) == NULL)
		return (-1);

	/* We don't support anything else than the version bitmap. */
	if (he->he_type != htons(OFP_HELLO_T_VERSION_BITMAP))
		return (-1);

	/* Validate header sizes. */
	helen = ntohs(he->he_length);
	if (helen < (int)sizeof(*he))
		return (-1);
	else if (helen == sizeof(*he))
		return (ofp_setversion(con, oh->oh_version));

	helen -= sizeof(*he);
	/* Invalid bitmap size. */
	if ((helen % sizeof(*bmp)) != 0)
		return (-1);

	ver = 0;
	poff = sizeof(*oh) + sizeof(*he);

	/* Loop through the bitmaps and choose the higher version. */
	while (helen > 0) {
		if ((bmp = ibuf_seek(ibuf, poff, sizeof(*bmp))) == NULL)
			return (-1);

		for (i = 0; i < 32; i++, ver++) {
			if ((ntohl(*bmp) & (1 << i)) == 0)
				continue;

			ofp_setversion(con, ver);
		}

		helen -= sizeof(*bmp);
		poff += sizeof(*bmp);
	}

	/* Check if we have set any version, otherwise fallback. */
	if (con->con_version == OFP_V_0)
		return (ofp_setversion(con, oh->oh_version));

	return (0);
}

int
ofp_send_featuresrequest(struct switchd *sc, struct switch_connection *con)
{
	struct ofp_header	*oh;
	struct ibuf		*ibuf;
	int			 rv = -1;

	if ((ibuf = ibuf_static()) == NULL ||
	    (oh = ibuf_advance(ibuf, sizeof(*oh))) == NULL)
		return (-1);

	oh->oh_version = con->con_version;
	oh->oh_type = OFP_T_FEATURES_REQUEST;
	oh->oh_length = htons(ibuf_length(ibuf));
	oh->oh_xid = htonl(con->con_xidnxt++);
	if (ofp_validate(sc, &con->con_local, &con->con_peer, oh, ibuf,
	    con->con_version) != 0)
		goto done;

	rv = ofp_output(con, NULL, ibuf);

 done:
	ibuf_free(ibuf);
	return (rv);
}

/* Appends an action with just the generic header. */
int
action_new(struct ibuf *ibuf, uint16_t type)
{
	struct ofp_action_header	*ah;

	if ((ah = ibuf_advance(ibuf, sizeof(*ah))) == NULL)
		return (-1);

	ah->ah_type = htons(type);
	ah->ah_len = htons(sizeof(*ah));
	return (0);
}

int
action_group(struct ibuf *ibuf, uint32_t group)
{
	struct ofp_action_group		*ag;

	if ((ag = ibuf_advance(ibuf, sizeof(*ag))) == NULL)
		return (-1);

	ag->ag_type = htons(OFP_ACTION_GROUP);
	ag->ag_len = sizeof(*ag);
	ag->ag_group_id = htonl(group);
	return (0);
}

int
action_output(struct ibuf *ibuf, uint32_t port, uint16_t maxlen)
{
	struct ofp_action_output	*ao;

	if ((ao = ibuf_advance(ibuf, sizeof(*ao))) == NULL)
		return (-1);

	ao->ao_type = htons(OFP_ACTION_OUTPUT);
	ao->ao_len = htons(sizeof(*ao));
	ao->ao_port = htonl(port);
	ao->ao_max_len = htons(maxlen);
	return (0);
}

/*
 * This action pushes VLAN/MPLS/PBB tags into the outermost part of the
 * packet. When the type is X ethertype must be Y:
 * - OFP_ACTION_PUSH_VLAN: ETHERTYPE_VLAN or ETHERTYPE_QINQ.
 * - OFP_ACTION_PUSH_MPLS: ETHERTYPE_MPLS or ETHERTYPE_MPLSCAST.
 * - OFP_ACTION_PUSH_PBB: ETHERTYPE_??? (0x88E7).
 */
int
action_push(struct ibuf *ibuf, uint16_t type, uint16_t ethertype)
{
	struct ofp_action_push		*ap;

	if ((ap = ibuf_advance(ibuf, sizeof(*ap))) == NULL)
		return (-1);

	ap->ap_type = htons(type);
	ap->ap_len = htons(sizeof(*ap));
	ap->ap_ethertype = htons(ethertype);
	return (0);
}

/* 
 * This action only pops the outermost VLAN tag and only one at a time,
 * you can only pop multiple VLANs with an action list that is only
 * availiable for OFP_INSTRUCTION_T_APPLY_ACTIONS.
 */
int
action_pop_vlan(struct ibuf *ibuf)
{
	return (action_new(ibuf, OFP_ACTION_POP_VLAN));
}

/*
 * Use this with caution since this will pop MPLS shim regardless of the
 * BoS bit state.
 */
int
action_pop_mpls(struct ibuf *ibuf, uint16_t ethertype)
{
	struct ofp_action_pop_mpls	*apm;

	if ((apm = ibuf_advance(ibuf, sizeof(*apm))) == NULL)
		return (-1);

	apm->apm_type = htons(OFP_ACTION_POP_MPLS);
	apm->apm_len = htons(sizeof(*apm));
	apm->apm_ethertype = htons(ethertype);
	return (0);
}

int
action_copyttlout(struct ibuf *ibuf)
{
	return (action_new(ibuf, OFP_ACTION_COPY_TTL_OUT));
}

int
action_copyttlin(struct ibuf *ibuf)
{
	return (action_new(ibuf, OFP_ACTION_COPY_TTL_IN));
}

int
action_decnwttl(struct ibuf *ibuf)
{
	return (action_new(ibuf, OFP_ACTION_DEC_NW_TTL));
}

/*
 * This function should be used with the oxm_*() family.
 *
 * After filling the action_setfield() with oxms you have to set the
 * asf_len with htons(size_of_oxms).
 */
struct ofp_action_set_field *
action_setfield(struct ibuf *ibuf)
{
	struct ofp_action_set_field	*asf;

	if ((asf = ibuf_advance(ibuf, sizeof(*asf))) == NULL)
		return (NULL);

	asf->asf_type = htons(OFP_ACTION_SET_FIELD);
	return (asf);
}

struct ofp_ox_match *
oxm_get(struct ibuf *ibuf, uint16_t field, int hasmask, uint8_t len)
{
	struct ofp_ox_match	*oxm;
	size_t			 oxmlen;

	/*
	 * When the mask is used we must always reserve double the space,
	 * because the mask field is the same size of the value.
	 */
	if (hasmask)
		len = len * 2;

	oxmlen = sizeof(*oxm) + len;
	if ((oxm = ibuf_advance(ibuf, oxmlen)) == NULL)
		return (NULL);

	oxm->oxm_class = htons(OFP_OXM_C_OPENFLOW_BASIC);
	oxm->oxm_length = len;
	OFP_OXM_SET_FIELD(oxm, field);
	if (hasmask)
		OFP_OXM_SET_HASMASK(oxm);

	return (oxm);
}

/*
 * OpenFlow port where the packet where received.
 * May be a physical port, a logical port or the reserved port OFPP_LOCAL.
 */
int
oxm_inport(struct ibuf *ibuf, uint32_t in_port)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IN_PORT, 0,
	    sizeof(in_port))) == NULL)
		return (-1);

	in_port = htonl(in_port);
	memcpy(oxm->oxm_value, &in_port, sizeof(in_port));
	return (0);
}

/*
 * Physical port on which the packet was received.
 * Requires: oxm_inport.
 */
int
oxm_inphyport(struct ibuf *ibuf, uint32_t in_phy_port)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IN_PHY_PORT, 0,
	    sizeof(in_phy_port))) == NULL)
		return (-1);

	in_phy_port = htonl(in_phy_port);
	memcpy(oxm->oxm_value, &in_phy_port, sizeof(in_phy_port));
	return (0);
}

/* Table metadata. */
int
oxm_metadata(struct ibuf *ibuf, int hasmask, uint64_t metadata, uint64_t mask)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_META, hasmask,
	    sizeof(metadata))) == NULL)
		return (-1);

	metadata = htobe64(metadata);
	memcpy(oxm->oxm_value, &metadata, sizeof(metadata));
	if (hasmask) {
		mask = htobe64(mask);
		memcpy(oxm->oxm_value + sizeof(metadata), &mask, sizeof(mask));
	}

	return (0);
}

int
oxm_etheraddr(struct ibuf *ibuf, int issrc, uint8_t *addr, uint8_t *mask)
{
	struct ofp_ox_match	*oxm;
	int			 type;
	int			 hasmask = (mask != NULL);

	type = issrc ? OFP_XM_T_ETH_SRC : OFP_XM_T_ETH_DST;
	if ((oxm = oxm_get(ibuf, type, hasmask, ETHER_ADDR_LEN)) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, addr, ETHER_ADDR_LEN);
	if (hasmask)
		memcpy(oxm->oxm_value + ETHER_ADDR_LEN, mask, ETHER_ADDR_LEN);

	return (0);
}

int
oxm_ethertype(struct ibuf *ibuf, uint16_t type)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_ETH_TYPE, 0, sizeof(type))) == NULL)
		return (-1);

	type = htons(type);
	memcpy(oxm->oxm_value, &type, sizeof(type));
	return (0);
}

int
oxm_vlanvid(struct ibuf *ibuf, int hasmask, uint16_t vid, uint16_t mask)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_VLAN_VID, hasmask,
	    sizeof(vid))) == NULL)
		return (-1);

	/* VID uses only the 13 least significant bits. */
	vid &= 0x1fff;
	vid = htons(vid);
	memcpy(oxm->oxm_value, &vid, sizeof(vid));
	if (hasmask) {
		mask &= 0x1fff;
		mask = htons(mask);
		memcpy(oxm->oxm_value + sizeof(vid), &mask, sizeof(mask));
	}

	return (0);
}

/*
 * 802.1Q Prio from the outermost tag.
 *
 * Requires: oxm_vlanvid.
 */
int
oxm_vlanpcp(struct ibuf *ibuf, uint8_t pcp)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_VLAN_PCP, 0, sizeof(pcp))) == NULL)
		return (-1);

	/* PCP only uses the lower 3 bits. */
	pcp &= 0x07;
	memcpy(oxm->oxm_value, &pcp, sizeof(pcp));
	return (0);
}

/*
 * The Diff Serv Code Point (DSCP) bits avaliable in IPv4 ToS field or
 * IPv6 Traffic Class field.
 *
 * Requires: oxm_ethertype(ETHERTYPE_IP) or oxm_ethertype(ETHERTYPE_IPV6).
 */
int
oxm_ipdscp(struct ibuf *ibuf, uint8_t dscp)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IP_DSCP, 0, sizeof(dscp))) == NULL)
		return (-1);

	/* Only the 6 lower bits have meaning. */
	dscp &= 0x3F;
	memcpy(oxm->oxm_value, &dscp, sizeof(dscp));
	return (0);
}

/*
 * The ECN (Explicit Congestion Notification) bits of IP headers.
 *
 * Requires: oxm_ethertype(ETHERTYPE_IP) or oxm_ethertype(ETHERTYPE_IPV6).
 */
int
oxm_ipecn(struct ibuf *ibuf, uint8_t ecn)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IP_ECN, 0, sizeof(ecn))) == NULL)
		return (-1);

	/* Only the 2 most significant bits have meaning. */
	ecn &= 0x03;
	memcpy(oxm->oxm_value, &ecn, sizeof(ecn));
	return (0);
}

/*
 * The IP protocol byte.
 *
 * Requires: oxm_ethertype(ETHERTYPE_IP) or oxm_ethertype(ETHERTYPE_IPV6).
 */
int
oxm_ipproto(struct ibuf *ibuf, uint8_t proto)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IP_PROTO, 0, sizeof(proto))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, &proto, sizeof(proto));
	return (0);
}

/*
 * The IPv4 address source/destination.
 *
 * Requires: oxm_ethertype(ETHERTYPE_IP).
 */
int
oxm_ipaddr(struct ibuf *ibuf, int issrc, int hasmask, uint32_t addr,
    uint32_t mask)
{
	struct ofp_ox_match	*oxm;
	int			 type;

	type = issrc ? OFP_XM_T_IPV4_SRC : OFP_XM_T_IPV4_DST;
	if ((oxm = oxm_get(ibuf, type, hasmask, sizeof(addr))) == NULL)
		return (-1);

	addr = htonl(addr);
	memcpy(oxm->oxm_value, &addr, sizeof(addr));
	if (hasmask) {
		mask = htonl(mask);
		memcpy(oxm->oxm_value + sizeof(addr), &mask, sizeof(mask));
	}

	return (0);
}

/*
 * The TCP source/destination port.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IP) or oxm_ethertype(ETHERTYPE_IPV6)
 * and oxm_ipproto(IPPROTO_TCP).
 */
int
oxm_tcpport(struct ibuf *ibuf, int issrc, uint16_t port)
{
	struct ofp_ox_match	*oxm;
	int			 type;

	type = issrc ? OFP_XM_T_TCP_SRC : OFP_XM_T_TCP_DST;
	if ((oxm = oxm_get(ibuf, type, 0, sizeof(port))) == NULL)
		return (-1);

	port = htons(port);
	memcpy(oxm->oxm_value, &port, sizeof(port));
	return (0);
}

/*
 * The UDP source/destination port.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IP) or oxm_ethertype(ETHERTYPE_IPV6)
 * and oxm_ipproto(IPPROTO_UDP).
 */
int
oxm_udpport(struct ibuf *ibuf, int issrc, uint16_t port)
{
	struct ofp_ox_match	*oxm;
	int			 type;

	type = issrc ? OFP_XM_T_UDP_SRC : OFP_XM_T_UDP_DST;
	if ((oxm = oxm_get(ibuf, type, 0, sizeof(port))) == NULL)
		return (-1);

	port = htons(port);
	memcpy(oxm->oxm_value, &port, sizeof(port));
	return (0);
}

/*
 * The SCTP source/destination port.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IP) or oxm_ethertype(ETHERTYPE_IPV6)
 * and oxm_ipproto(IPPROTO_??? -- 132).
 */
int
oxm_sctpport(struct ibuf *ibuf, int issrc, uint16_t port)
{
	struct ofp_ox_match	*oxm;
	int			 type;

	type = issrc ? OFP_XM_T_SCTP_SRC : OFP_XM_T_SCTP_DST;
	if ((oxm = oxm_get(ibuf, type, 0, sizeof(port))) == NULL)
		return (-1);

	port = htons(port);
	memcpy(oxm->oxm_value, &port, sizeof(port));
	return (0);
}

/*
 * The ICMPv4 type in the ICMP header.
 *
 * Requires: oxm_ethertype(ETHERTYPE_IP) and oxm_ipproto(IPPROTO_ICMP).
 */
int
oxm_icmpv4type(struct ibuf *ibuf, uint8_t type)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_ICMPV4_TYPE, 0,
	    sizeof(type))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, &type, sizeof(type));
	return (0);
}

/*
 * The ICMPv4 code in the ICMP header.
 *
 * Requires: oxm_ethertype(ETHERTYPE_IP) and oxm_ipproto(IPPROTO_ICMP).
 */
int
oxm_icmpv4code(struct ibuf *ibuf, uint8_t code)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_ICMPV4_CODE, 0,
	    sizeof(code))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, &code, sizeof(code));
	return (0);
}

/*
 * ARP opcode.
 *
 * Requires: oxm_ethertype(ETHERTYPE_ARP).
 */
int
oxm_arpop(struct ibuf *ibuf, uint16_t op)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_ARP_OP, 0, sizeof(op))) == NULL)
		return (-1);

	op = htons(op);
	memcpy(oxm->oxm_value, &op, sizeof(op));
	return (0);
}

/*
 * ARP source/target protocol address.
 *
 * Requires: oxm_ethertype(ETHERTYPE_ARP).
 */
int
oxm_arpaddr(struct ibuf *ibuf, int issrc, int hasmask, uint32_t addr,
    uint32_t mask)
{
	struct ofp_ox_match	*oxm;
	int			 type;

	type = issrc ? OFP_XM_T_ARP_SPA : OFP_XM_T_ARP_TPA;
	if ((oxm = oxm_get(ibuf, type, hasmask, sizeof(addr))) == NULL)
		return (-1);

	addr = htonl(addr);
	memcpy(oxm->oxm_value, &addr, sizeof(addr));
	if (hasmask) {
		mask = htonl(mask);
		memcpy(oxm->oxm_value + sizeof(addr), &mask, sizeof(mask));
	}

	return (0);
}

/*
 * ARP source/target hardware address.
 *
 * Requires: oxm_ethertype(ETHERTYPE_ARP).
 */
int
oxm_arphaddr(struct ibuf *ibuf, int issrc, uint8_t *addr, uint8_t *mask)
{
	struct ofp_ox_match	*oxm;
	int			 type;
	int			 hasmask = (mask != NULL);

	type = issrc ? OFP_XM_T_ARP_SHA : OFP_XM_T_ARP_THA;
	if ((oxm = oxm_get(ibuf, type, hasmask, ETHER_ADDR_LEN)) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, addr, ETHER_ADDR_LEN);
	if (hasmask)
		memcpy(oxm->oxm_value + ETHER_ADDR_LEN, mask, ETHER_ADDR_LEN);

	return (0);
}

/*
 * The source or destination of the IPv6 address.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6).
 */
int
oxm_ipv6addr(struct ibuf *ibuf, int issrc, struct in6_addr *addr,
    struct in6_addr *mask)
{
	struct ofp_ox_match	*oxm;
	int			 type;
	int			 hasmask = (mask != NULL);

	type = issrc ? OFP_XM_T_IPV6_SRC : OFP_XM_T_IPV6_DST;
	if ((oxm = oxm_get(ibuf, type, hasmask, sizeof(*addr))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, addr, sizeof(*addr));
	if (hasmask)
		memcpy(oxm->oxm_value + sizeof(*addr), mask, sizeof(*mask));

	return (0);
}

/*
 * The IPv6 flow label field.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6).
 */
int
oxm_ipv6flowlabel(struct ibuf *ibuf, int hasmask, uint32_t flowlabel,
    uint32_t mask)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IPV6_FLABEL, hasmask,
	    sizeof(flowlabel))) == NULL)
		return (-1);

	/*
         * 12 most significants bits forced to 0 and only the 20 lowers
         * bits have meaning.
	 */
	flowlabel &= 0x000FFFFFU;
	flowlabel = htonl(flowlabel);
	memcpy(oxm->oxm_value, &flowlabel, sizeof(flowlabel));
	if (hasmask) {
		mask &= 0x000FFFFFU;
		mask = htonl(mask);
		memcpy(oxm->oxm_value + sizeof(flowlabel), &mask, sizeof(mask));
	}

	return (0);
}

/*
 * The ICMPv6 type in ICMP header.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6) and oxm_ipproto(IPPROTO_ICMPV6).
 */
int
oxm_icmpv6type(struct ibuf *ibuf, uint8_t type)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_ICMPV6_TYPE, 0,
	    sizeof(type))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, &type, sizeof(type));
	return (0);
}

/*
 * The ICMPv6 code in ICMP header.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6) and oxm_ipproto(IPPROTO_ICMPV6).
 */
int
oxm_icmpv6code(struct ibuf *ibuf, uint8_t code)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_ICMPV6_CODE, 0,
	    sizeof(code))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, &code, sizeof(code));
	return (0);
}

/*
 * The target address in neighbour discovery message.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6), oxm_ipproto(IPPROTO_ICMPV6)
 * and oxm_icmpv6type(ND_NEIGHBOR_SOLICIT) or
 * oxm_icmpv6type(ND_NEIGHBOR_ADVERT).
 */
int
oxm_ipv6ndtarget(struct ibuf *ibuf, struct in6_addr *addr)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IPV6_ND_TARGET, 0,
	    sizeof(*addr))) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, addr, sizeof(*addr));
	return (0);
}

/*
 * The source link-layer address in an IPv6 Neighbour discovery.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6), oxm_ipproto(IPPROTO_ICMPV6)
 * and oxm_icmpv6type(ND_NEIGHBOR_SOLICIT).
 */
int
oxm_ipv6ndlinkaddr(struct ibuf *ibuf, int issrc, uint8_t *addr)
{
	struct ofp_ox_match	*oxm;
	int			 type;

	type = issrc ? OFP_XM_T_IPV6_ND_SLL : OFP_XM_T_IPV6_ND_TLL;
	if ((oxm = oxm_get(ibuf, type, 0, ETHER_ADDR_LEN)) == NULL)
		return (-1);

	memcpy(oxm->oxm_value, addr, ETHER_ADDR_LEN);
	return (0);
}

/*
 * The label in the MPLS shim.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_MPLS) or
 * oxm_ethertype(ETHERTYPE_MPLS_MCAST).
 */
int
oxm_mplslabel(struct ibuf *ibuf, uint32_t label)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_MPLS_LABEL, 0,
	    sizeof(label))) == NULL)
		return (-1);

	label &= MPLS_LABEL_MASK;
	label = htonl(label);
	memcpy(oxm->oxm_value, &label, sizeof(label));
	return (0);
}

/*
 * The TC in the first MPLS shim.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_MPLS) or
 * oxm_ethertype(ETHERTYPE_MPLS_MCAST).
 */
int
oxm_mplstc(struct ibuf *ibuf, uint8_t tc)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_MPLS_TC, 0, sizeof(tc))) == NULL)
		return (-1);

	tc &= 0x07;
	memcpy(oxm->oxm_value, &tc, sizeof(tc));
	return (0);
}

/*
 * The BoS bit in the first MPLS shim.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_MPLS) or
 * oxm_ethertype(ETHERTYPE_MPLS_MCAST).
 */
int
oxm_mplsbos(struct ibuf *ibuf, uint8_t bos)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_MPLS_BOS, 0, sizeof(bos))) == NULL)
		return (-1);

	bos &= 0x01;
	memcpy(oxm->oxm_value, &bos, sizeof(bos));
	return (0);
}

/*
 * Comment shamelessly taken from OpenFlow 1.3.5 specification.
 *
 * Metadata associated with a logical port.
 *
 * If the logical port performs encapsulation and decapsulation, this
 * is the demultiplexing field from the encapsulation header.
 * For example, for a packet received via GRE tunnel including a (32-bit) key,
 * the key is stored in the low 32-bits and the high bits are zeroed.
 * For a MPLS logical port, the low 20 bits represent the MPLS Label.
 * For a VxLAN logical port, the low 24 bits represent the VNI.
 * If the packet is not received through a logical port, the value is 0.
 */
int
oxm_tunnelid(struct ibuf *ibuf, int hasmask, uint64_t id, uint64_t mask)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_TUNNEL_ID, hasmask,
	    sizeof(id))) == NULL)
		return (-1);

	id = htobe64(id);
	memcpy(oxm->oxm_value, &id, sizeof(id));
	if (hasmask) {
		mask = htobe64(mask);
		memcpy(oxm->oxm_value + sizeof(id), &mask, sizeof(mask));
	}
	return (0);
}

/*
 * The IPv6 extension header.
 *
 * Tip: use the OFP_XM_IPV6_EXTHDR_* macros.
 *
 * Requirements: oxm_ethertype(ETHERTYPE_IPV6).
 */
int
oxm_ipv6exthdr(struct ibuf *ibuf, int hasmask, uint16_t exthdr, uint16_t mask)
{
	struct ofp_ox_match	*oxm;

	if ((oxm = oxm_get(ibuf, OFP_XM_T_IPV6_EXTHDR, hasmask,
	    sizeof(exthdr))) == NULL)
		return (-1);

	/* Only the lower 9 bits have meaning. */
	exthdr &= 0x01FF;
	exthdr = htons(exthdr);
	memcpy(oxm->oxm_value, &exthdr, sizeof(exthdr));
	if (hasmask) {
		mask &= 0x01FF;
		mask = htons(mask);
		memcpy(oxm->oxm_value + sizeof(exthdr), &mask, sizeof(mask));
	}
	return (0);
}

/*
 * Appends a new instruction with hlen size.
 *
 * Remember to set the instruction length (i->i_len) if it has more data,
 * like ofp_instruction_actions, ofp_instruction_goto_table etc...
 */
struct ofp_instruction *
ofp_instruction(struct ibuf *ibuf, uint16_t type, uint16_t hlen)
{
	struct ofp_instruction	*oi;

	if ((oi = ibuf_advance(ibuf, hlen)) == NULL)
		return (NULL);

	oi->i_type = htons(type);
	oi->i_len = htons(hlen);
	return (oi);
}

struct multipart_message *
ofp_multipart_lookup(struct switch_connection *con, uint32_t xid)
{
	struct multipart_message	*mm;

	SLIST_FOREACH(mm, &con->con_mmlist, mm_entry) {
		if (mm->mm_xid != xid)
			continue;

		return (mm);
	}

	return (NULL);
}

int
ofp_multipart_add(struct switch_connection *con, uint32_t xid, uint8_t type)
{
	struct multipart_message	*mm;

	if ((mm = ofp_multipart_lookup(con, xid)) != NULL) {
		/*
		 * A multipart reply has the same xid and type, otherwise
		 * something went wrong.
		 */
		if (mm->mm_type != type)
			return (-1);

		return (0);
	}

	if ((mm = calloc(1, sizeof(*mm))) == NULL)
		return (-1);

	mm->mm_xid = xid;
	mm->mm_type = type;
	SLIST_INSERT_HEAD(&con->con_mmlist, mm, mm_entry);
	return (0);
}

void
ofp_multipart_del(struct switch_connection *con, uint32_t xid)
{
	struct multipart_message	*mm;

	SLIST_FOREACH(mm, &con->con_mmlist, mm_entry)
		if (mm->mm_xid == xid)
			break;

	if (mm == NULL)
		return;

	ofp_multipart_free(con, mm);
}

void
ofp_multipart_free(struct switch_connection *con,
    struct multipart_message *mm)
{
	SLIST_REMOVE(&con->con_mmlist, mm, multipart_message, mm_entry);
	free(mm);
}

void
ofp_multipart_clear(struct switch_connection *con)
{
	struct multipart_message	*mm;

	while (!SLIST_EMPTY(&con->con_mmlist)) {
		mm = SLIST_FIRST(&con->con_mmlist);
		ofp_multipart_free(con, mm);
	}
}

struct switch_table *
switch_tablelookup(struct switch_connection *con, int table)
{
	struct switch_table		*st;

	TAILQ_FOREACH(st, &con->con_stlist, st_entry) {
		if (st->st_table == table)
			return (st);
	}

	return (NULL);
}

struct switch_table *
switch_newtable(struct switch_connection *con, int table)
{
	struct switch_table		*st;

	if ((st = calloc(1, sizeof(*st))) == NULL)
		return (NULL);

	st->st_table = table;
	TAILQ_INSERT_TAIL(&con->con_stlist, st, st_entry);

	return (st);
}

void
switch_deltable(struct switch_connection *con, struct switch_table *st)
{
	TAILQ_REMOVE(&con->con_stlist, st, st_entry);
	free(st);
}

void
switch_freetables(struct switch_connection *con)
{
	struct switch_table		*st;

	while (!TAILQ_EMPTY(&con->con_stlist)) {
		st = TAILQ_FIRST(&con->con_stlist);
		switch_deltable(con, st);
	}
}

int
oflowmod_state(struct oflowmod_ctx *ctx, unsigned int old, unsigned int new)
{
	if (ctx->ctx_state != old)
		return (-1);
	ctx->ctx_state = new;
	return (0);
}

int
oflowmod_err(struct oflowmod_ctx *ctx, const char *func, int line)
{
	log_debug("%s: function %s line %d state %d",
	    __func__, func, line, ctx->ctx_state);

	if (ctx->ctx_state >= OFMCTX_ERR)
		return (-1);
	if (ctx->ctx_flags & OFMCTX_IBUF)
		ibuf_release(ctx->ctx_ibuf);
	ctx->ctx_state = OFMCTX_ERR;
	return (-1);
}

struct ibuf *
oflowmod_open(struct oflowmod_ctx *ctx, struct switch_connection *con,
    struct ibuf *ibuf, uint8_t version)
{
	struct ofp_flow_mod	*fm;
	struct switch_connection conb;

	switch (version) {
	case OFP_V_0:
	case OFP_V_1_3:
		version = OFP_V_1_3;
		break;
	default:
		log_warnx("%s: unsupported version 0x%02x", __func__, version);
		return (NULL);
	}

	memset(ctx, 0, sizeof(*ctx));

	if (oflowmod_state(ctx, OFMCTX_INIT, OFMCTX_OPEN) == -1)
		goto err;

	if (ibuf == NULL) {
		ctx->ctx_flags |= OFMCTX_IBUF;
		if ((ibuf = ibuf_static()) == NULL)
			goto err;
	}

	ctx->ctx_ibuf = ibuf;
	ctx->ctx_start = ibuf->wpos;

	/*
	 * The connection is not strictly required and might not be
	 * available in other places;  just default to an xid 0.
	 */
	if (con == NULL) {
		con = &conb;
		memset(con, 0, sizeof(*con));
	}

	/* uses defaults, can be changed by accessing fm later */
	if ((fm = ofp13_flowmod(con, ibuf, OFP_FLOWCMD_ADD)) == NULL)
		goto err;

	ctx->ctx_fm = fm;

	return (ctx->ctx_ibuf);

 err:
	(void)oflowmod_err(ctx, __func__, __LINE__);
	return (NULL);
}

int
oflowmod_mopen(struct oflowmod_ctx *ctx)
{
	if (oflowmod_state(ctx, OFMCTX_OPEN, OFMCTX_MOPEN) == -1)
		return (oflowmod_err(ctx, __func__, __LINE__));

	ctx->ctx_ostart = ctx->ctx_start +
	    offsetof(struct ofp_flow_mod, fm_match);

	return (0);
}

int
oflowmod_mclose(struct oflowmod_ctx *ctx)
{
	struct ibuf		*ibuf = ctx->ctx_ibuf;
	struct ofp_flow_mod	*fm = ctx->ctx_fm;
	size_t			 omlen, padding;

	if (oflowmod_state(ctx, OFMCTX_MOPEN, OFMCTX_MCLOSE) == -1)
		return (oflowmod_err(ctx, __func__, __LINE__));

	ctx->ctx_oend = ibuf->wpos;
	omlen = ctx->ctx_oend - ctx->ctx_ostart;

	/* Update match length */
	fm->fm_match.om_length = htons(omlen);

	padding = OFP_ALIGN(omlen) - omlen;
	if (padding) {
		ctx->ctx_oend += padding;
		if (ibuf_advance(ibuf, padding) == NULL)
			return (oflowmod_err(ctx, __func__, __LINE__));
	}

	return (0);
}

int
oflowmod_iopen(struct oflowmod_ctx *ctx)
{
	struct ibuf		*ibuf = ctx->ctx_ibuf;

	if (ctx->ctx_state < OFMCTX_MOPEN &&
	    (oflowmod_mopen(ctx) == -1))
		return (oflowmod_err(ctx, __func__, __LINE__));
	if (ctx->ctx_state < OFMCTX_MCLOSE &&
	    (oflowmod_mclose(ctx) == -1))
		return (oflowmod_err(ctx, __func__, __LINE__));

	if (oflowmod_state(ctx, OFMCTX_MCLOSE, OFMCTX_IOPEN) == -1)
		return (oflowmod_err(ctx, __func__, __LINE__));

	ctx->ctx_istart = ibuf->wpos;

	return (0);
}

int
oflowmod_instruction(struct oflowmod_ctx *ctx, unsigned int type)
{
	struct ibuf		*ibuf = ctx->ctx_ibuf;
	struct ofp_instruction	*oi;
	size_t			 len;

	if (ctx->ctx_state < OFMCTX_IOPEN &&
	    (oflowmod_iopen(ctx) == -1))
		return (oflowmod_err(ctx, __func__, __LINE__));

	if (oflowmod_state(ctx, OFMCTX_IOPEN, OFMCTX_IOPEN) == -1)
		return (oflowmod_err(ctx, __func__, __LINE__));

	if (ctx->ctx_oi != NULL && oflowmod_instructionclose(ctx) == -1)
		return (oflowmod_err(ctx, __func__, __LINE__));

	ctx->ctx_oioff = ibuf->wpos;

	switch (type) {
	case OFP_INSTRUCTION_T_GOTO_TABLE:
		len = sizeof(struct ofp_instruction_goto_table);
		break;
	case OFP_INSTRUCTION_T_WRITE_META:
		len = sizeof(struct ofp_instruction_write_metadata);
		break;
	case OFP_INSTRUCTION_T_WRITE_ACTIONS:
	case OFP_INSTRUCTION_T_APPLY_ACTIONS:
	case OFP_INSTRUCTION_T_CLEAR_ACTIONS:
		len = sizeof(struct ofp_instruction_actions);
		break;
	case OFP_INSTRUCTION_T_METER:
		len = sizeof(struct ofp_instruction_meter);
		break;
	case OFP_INSTRUCTION_T_EXPERIMENTER:
		len = sizeof(struct ofp_instruction_experimenter);
		break;
	default:
		return (oflowmod_err(ctx, __func__, __LINE__));
	}

	if ((oi = ofp_instruction(ibuf, type, len)) == NULL)
		return (oflowmod_err(ctx, __func__, __LINE__));

	ctx->ctx_oi = oi;

	return (0);
}

int
oflowmod_instructionclose(struct oflowmod_ctx *ctx)
{
	struct ibuf		*ibuf = ctx->ctx_ibuf;
	struct ofp_instruction	*oi = ctx->ctx_oi;
	size_t			 oilen;

	if (ctx->ctx_state < OFMCTX_IOPEN || oi == NULL)
		return (oflowmod_err(ctx, __func__, __LINE__));

	oilen = ibuf->wpos - ctx->ctx_oioff;

	if (oilen > UINT16_MAX)
		return (oflowmod_err(ctx, __func__, __LINE__));

	oi->i_len = htons(oilen);
	ctx->ctx_oi = NULL;

	return (0);
}

int
oflowmod_iclose(struct oflowmod_ctx *ctx)
{
	struct ibuf		*ibuf = ctx->ctx_ibuf;

	if (oflowmod_state(ctx, OFMCTX_IOPEN, OFMCTX_ICLOSE) == -1)
		return (oflowmod_err(ctx, __func__, __LINE__));

	if (ctx->ctx_oi != NULL && oflowmod_instructionclose(ctx) == -1)
		return (-1);

	ctx->ctx_iend = ibuf->wpos;

	return (0);
}

int
oflowmod_close(struct oflowmod_ctx *ctx)
{
	struct ofp_flow_mod	*fm = ctx->ctx_fm;
	struct ibuf		*ibuf = ctx->ctx_ibuf;
	size_t			 len;

	/* No matches, calculate default */
	if (ctx->ctx_state < OFMCTX_MOPEN &&
	    (oflowmod_mopen(ctx) == -1 ||
	    oflowmod_mclose(ctx) == -1))
		goto err;

	/* No instructions, calculate default */
	if (ctx->ctx_state < OFMCTX_IOPEN &&
	    (oflowmod_iopen(ctx) == -1 ||
	    oflowmod_iclose(ctx) == -1))
		goto err;

	if (oflowmod_state(ctx, OFMCTX_ICLOSE, OFMCTX_CLOSE) == -1)
		goto err;

	/* Update length */
	len = ibuf->wpos - ctx->ctx_start;
	fm->fm_oh.oh_length = htons(len);

	return (0);

 err:
	return (oflowmod_err(ctx, __func__, __LINE__));

}
