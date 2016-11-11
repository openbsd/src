/*	$OpenBSD: ofp_common.c,v 1.3 2016/11/11 22:07:40 reyk Exp $	*/

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

int
ofp_multipart_add(struct switch_connection *con, uint32_t xid, uint8_t type)
{
	struct multipart_message	*mm;

	/* A multipart reply have the same xid and type in all parts. */
	SLIST_FOREACH(mm, &con->con_mmlist, mm_entry) {
		if (mm->mm_xid != xid)
			continue;
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
