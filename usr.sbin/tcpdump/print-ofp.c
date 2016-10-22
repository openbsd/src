/*	$$	*/

/*
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

#include <net/ofp.h>

#include <endian.h>
#include <stddef.h>
#include <stdio.h>

#include "interface.h"

/* Size of action header without the padding. */
#define AH_UNPADDED	(offsetof(struct ofp_action_header, ah_pad))

void	 ofp_print_hello(const u_char *, u_int, u_int);
void	 ofp_print_featuresreply(const u_char *, u_int);
void	 ofp_print_setconfig(const u_char *, u_int);
void	 ofp_print_packetin(const u_char *, u_int);
void	 ofp_print_packetout(const u_char *, u_int);
void	 ofp_print_flowremoved(const u_char *, u_int);
void	 ofp_print_flowmod(const u_char *, u_int);

void	 oxm_print_halfword(const u_char *, u_int, int, int);
void	 oxm_print_word(const u_char *, u_int, int, int);
void	 oxm_print_quad(const u_char *, u_int, int, int);
void	 oxm_print_ether(const u_char *, u_int, int);
void	 ofp_print_oxm(struct ofp_ox_match *, const u_char *, u_int);

void	 action_print_output(const u_char *, u_int);
void	 action_print_group(const u_char *, u_int);
void	 action_print_setmplsttl(const u_char *, u_int);
void	 action_print_setnwttl(const u_char *, u_int);
void	 action_print_push(const u_char *, u_int);
void	 action_print_popmpls(const u_char *, u_int);
void	 action_print_setfield(const u_char *, u_int);
void	 ofp_print_action(struct ofp_action_header *, const u_char *,
	    u_int);

void	 instruction_print_gototable(const char *, u_int);
void	 instruction_print_meta(const char *, u_int);
void	 instruction_print_actions(const char *, u_int);
void	 instruction_print_meter(const char *, u_int);
void	 instruction_print_experimenter(const char *, u_int);
void	 ofp_print_instruction(struct ofp_instruction *, const char *, u_int);

void
ofp_print_hello(const u_char *bp, u_int length, u_int ohlen)
{
	struct ofp_hello_element_header		*he;
	int					 hetype, helen, ver, i;
	int					 vmajor, vminor;
	uint32_t				 bmp;

	/* Skip the OFP header. */
	bp += sizeof(struct ofp_header);
	length -= sizeof(struct ofp_header);

	printf(" HELLO");

	/* Check for header truncation. */
	if (ohlen > sizeof(struct ofp_header) &&
	    length < sizeof(*he)) {
		printf(" [truncated]");
		return;
	}

 next_header:
	/* Check for hello element headers. */
	if (length < sizeof(*he))
		return;

	he = (struct ofp_hello_element_header *)bp;
	hetype = ntohs(he->he_type);
	helen = ntohs(he->he_length);

	bp += sizeof(*he);
	length -= sizeof(*he);
	helen -= sizeof(*he);

	switch (hetype) {
	case OPF_HELLO_T_VERSION_BITMAP:
		printf(" version bitmap <");
		if (helen < sizeof(bmp)) {
			printf("invalid header>");
			break;
		}

 next_bitmap:
		if (length < sizeof(bmp)) {
			printf("truncated>");
			break;
		}

		bmp = ntohl(*(uint32_t *)bp);
		for (i = 0, ver = 9; i < 32; i++, ver++) {
			if ((bmp & (1 << i)) == 0)
				continue;

			vmajor = (ver / 10);
			vminor = ver % 10;
			printf("v%d.%d ", vmajor, vminor);
		}
		helen -= min(sizeof(bmp), helen);
		length -= sizeof(bmp);
		bp += sizeof(bmp);
		if (helen)
			goto next_bitmap;

		printf("\b>");
		break;

	default:
		printf(" element header[type %d length %d]", hetype, helen);
		break;
	}

	length -= min(helen, length);
	bp += helen;
	if (length)
		goto next_header;
}

void
ofp_print_error(const u_char *bp, u_int length)
{
	struct ofp_error			*err;

	printf(" ERROR");
	if (length < sizeof(*err)) {
		printf(" [truncated]");
		return;
	}

	err = (struct ofp_error *)bp;
	printf(" <type %d code %d>", ntohs(err->err_type),
	    ntohs(err->err_code));

	length -= min(sizeof(*err), length);
	bp += sizeof(*err);
	/* If there are still bytes left, print the optional error data. */
	if (length) {
		printf(" error data:");
		ofp_print(bp);
	}
}

void
ofp_print_featuresreply(const u_char *bp, u_int length)
{
	struct ofp_switch_features		*swf;

	printf(" FEATURES REPLY");
	if (length < sizeof(*swf)) {
		printf(" [trucanted]");
		return;
	}

	swf = (struct ofp_switch_features *)bp;
	printf(" <datapath_id %#016llx nbuffers %u ntables %d aux_id %d "
	    "capabilities %#08x actions %#08x>",
	    be64toh(swf->swf_datapath_id), ntohl(swf->swf_nbuffers),
	    swf->swf_ntables, swf->swf_aux_id,
	    ntohl(swf->swf_capabilities), ntohl(swf->swf_actions));
}

void
ofp_print_setconfig(const u_char *bp, u_int length)
{
	struct ofp_switch_config		*cfg;

	printf(" SET CONFIG");
	if (length < sizeof(*cfg)) {
		printf(" [truncated]");
		return;
	}

	cfg = (struct ofp_switch_config *)bp;
	printf(" <flags %#04x miss_send_len %d>",
	    ntohs(cfg->cfg_flags), ntohs(cfg->cfg_miss_send_len));
}

void
ofp_print_packetin(const u_char *bp, u_int length)
{
	struct ofp_packet_in		*pin;
	struct ofp_ox_match		*oxm;
	int				 omtype, omlen;
	int				 haspacket = 0;
	const u_char			*pktptr;

	printf(" PACKET-IN");
	if (length < sizeof(*pin)) {
		printf(" [truncated]");
		return;
	}

	pin = (struct ofp_packet_in *)bp;
	omtype = ntohs(pin->pin_match.om_type);
	omlen = ntohs(pin->pin_match.om_length);
	printf(" <buffer_id %u total_len %d reason %d table_id %d "
	    "cookie %#016llx match type %d length %d>",
	    ntohl(pin->pin_buffer_id), ntohs(pin->pin_total_len),
	    pin->pin_reason, pin->pin_table_id, be64toh(pin->pin_cookie),
	    omtype, omlen);

	if (pin->pin_buffer_id == OFP_PKTOUT_NO_BUFFER)
		haspacket = 1;

	/* We only support OXM. */
	if (omtype != OFP_MATCH_OXM)
		return;

	bp += sizeof(*pin);
	length -= sizeof(*pin);

	/* Get packet start address. */
	pktptr = (bp - sizeof(pin->pin_match)) +
	    OFP_ALIGN(omlen) + ETHER_ALIGN;

	/* Don't count the header for the OXM fields. */
	omlen -= min(sizeof(pin->pin_match), omlen);
	if (omlen == 0)
		goto print_packet;

 parse_next_oxm:
	if (length < sizeof(*oxm)) {
		printf(" [truncated]");
		return;
	}

	oxm = (struct ofp_ox_match *)bp;
	bp += sizeof(*oxm);
	length -= sizeof(*oxm);
	if (length < oxm->oxm_length) {
		printf(" [truncated]");
		return;
	}

	ofp_print_oxm(oxm, bp, length);

	bp += oxm->oxm_length;
	length -= oxm->oxm_length;
	omlen -= min(sizeof(*oxm) + oxm->oxm_length, omlen);
	if (omlen)
		goto parse_next_oxm;

 print_packet:
	if (haspacket == 0)
		return;

	/*
	 * Recalculate length:
	 * pktptr skipped the omlen + padding and the ETHER_ALIGN, so
	 * instead of keeping track of that we just recalculate length
	 * using the encapsulated packet begin and snapend.
	 */
	length = max(snapend - pktptr, 0);
	if (length < ETHER_ADDR_LEN) {
		printf(" [|ether]");
		return;
	}

	printf(" ");
	ether_tryprint(pktptr, length, 0);
}

void
ofp_print_flowremoved(const u_char *bp, u_int length)
{
	struct ofp_flow_removed			*fr;
	struct ofp_ox_match			*oxm;
	int					 omtype, omlen;

	printf(" FLOW REMOVED");
	if (length < sizeof(*fr)) {
		printf(" [truncated]");
		return;
	}

	fr = (struct ofp_flow_removed *)bp;
	omtype = ntohs(fr->fr_match.om_type);
	omlen = ntohs(fr->fr_match.om_length);
	printf(" <cookie %#016llx priority %d reason %d table_id %d "
	    "duration sec %u nsec %u timeout idle %d hard %d "
	    "packet count %llu byte count %llu match type %d length %d>",
	    be64toh(fr->fr_cookie), ntohs(fr->fr_priority), fr->fr_reason,
	    fr->fr_table_id, ntohl(fr->fr_duration_sec),
	    ntohl(fr->fr_duration_nsec), ntohs(fr->fr_idle_timeout),
	    ntohs(fr->fr_hard_timeout), be64toh(fr->fr_packet_count),
	    be64toh(fr->fr_byte_count), omtype, omlen);

	/* We only support OXM. */
	if (omtype != OFP_MATCH_OXM)
		return;

	omlen -= min(sizeof(fr->fr_match), omlen);
	if (omlen == 0)
		return;

	bp += sizeof(*fr);
	length -= sizeof(*fr);

 parse_next_oxm:
	if (length < sizeof(*oxm)) {
		printf(" [truncated]");
		return;
	}

	oxm = (struct ofp_ox_match *)bp;
	bp += sizeof(*oxm);
	length -= sizeof(*oxm);
	if (length < oxm->oxm_length) {
		printf(" [truncated]");
		return;
	}

	ofp_print_oxm(oxm, bp, length);

	bp += oxm->oxm_length;
	length -= oxm->oxm_length;
	omlen -= min(sizeof(*oxm) + oxm->oxm_length, omlen);
	if (omlen)
		goto parse_next_oxm;
}

void
ofp_print_packetout(const u_char *bp, u_int length)
{
	struct ofp_packet_out			*pout;
	struct ofp_action_header		*ah;
	const u_char				*pktptr;
	int					 actionslen, haspacket = 0;
	int					 ahlen;

	printf(" PACKET-OUT");
	if (length < sizeof(*pout)) {
		printf(" [truncated]");
		return;
	}

	pout = (struct ofp_packet_out *)bp;
	actionslen = ntohs(pout->pout_actions_len);
	printf(" <buffer_id %u in_port %u actions_len %d>",
	    ntohl(pout->pout_buffer_id), ntohl(pout->pout_in_port),
	    actionslen);

	if (pout->pout_buffer_id == OFP_PKTOUT_NO_BUFFER)
		haspacket = 1;

	bp += sizeof(*pout);
	length -= sizeof(*pout);
	pktptr = bp + actionslen;

	/* No actions or unpadded header. */
	if (actionslen < sizeof(*ah))
		goto print_packet;

 parse_next_action:
	if (length < sizeof(*ah)) {
		printf(" [truncated]");
		return;
	}

	ah = (struct ofp_action_header *)bp;
	bp += AH_UNPADDED;
	length -= AH_UNPADDED;
	actionslen -= AH_UNPADDED;
	ahlen = ntohs(ah->ah_len) - AH_UNPADDED;
	if (length < ahlen) {
		printf(" [truncated]");
		return;
	}

	ofp_print_action(ah, bp, length);

	bp += ahlen;
	length -= ahlen;
	actionslen -= min(ahlen, actionslen);
	if (actionslen)
		goto parse_next_action;

 print_packet:
	if (haspacket == 0)
		return;

	/* Recalculate length using packet boundaries. */
	length = max(snapend - pktptr, 0);
	if (length < ETHER_ADDR_LEN) {
		printf(" [|ether]");
		return;
	}

	printf(" ");
	ether_tryprint(pktptr, length, 0);
}

void
ofp_print_flowmod(const u_char *bp, u_int length)
{
	struct ofp_flow_mod			*fm;
	struct ofp_ox_match			*oxm;
	struct ofp_instruction			*i;
	int					 omtype, omlen, ilen;
	int					 instructionslen, padsize;

	printf(" FLOW-MOD");
	if (length < sizeof(*fm)) {
		printf(" [truncated]");
		return;
	}

	fm = (struct ofp_flow_mod *)bp;
	omtype = ntohs(fm->fm_match.om_type);
	omlen = ntohs(fm->fm_match.om_length);
	printf(" <cookie %llu cookie_mask %#016llx table_id %d command %d "
	    "timeout idle %d hard %d priority %d buffer_id %u out_port %u "
	    "out_group %u flags %#04x match type %d length %d>",
	    be64toh(fm->fm_cookie), be64toh(fm->fm_cookie_mask),
	    fm->fm_table_id, fm->fm_command, ntohs(fm->fm_idle_timeout),
	    ntohs(fm->fm_hard_timeout), fm->fm_priority,
	    ntohl(fm->fm_buffer_id), ntohl(fm->fm_out_port),
	    ntohl(fm->fm_out_group), ntohs(fm->fm_flags), omtype, omlen);

	bp += sizeof(*fm);
	length -= sizeof(*fm);

	padsize = OFP_ALIGN(omlen) - omlen;
	omlen -= min(sizeof(fm->fm_match), omlen);
	instructionslen = length - (omlen + padsize);
	if (omtype != OFP_MATCH_OXM || omlen == 0) {
		if (instructionslen <= 0)
			return;

		/* Skip padding if any. */
		if (padsize) {
			bp += padsize;
			length -= padsize;
		}
		goto parse_next_instruction;
	}

 parse_next_oxm:
	if (length < sizeof(*oxm)) {
		printf(" [truncated]");
		return;
	}

	oxm = (struct ofp_ox_match *)bp;
	bp += sizeof(*oxm);
	length -= sizeof(*oxm);
	if (length < oxm->oxm_length) {
		printf(" [truncated]");
		return;
	}

	ofp_print_oxm(oxm, bp, length);

	bp += oxm->oxm_length;
	length -= oxm->oxm_length;
	omlen -= min(sizeof(*oxm) + oxm->oxm_length, omlen);
	if (omlen)
		goto parse_next_oxm;

	/* Skip padding if any. */
	if (padsize) {
		bp += padsize;
		length -= padsize;
	}

parse_next_instruction:
	if (length < sizeof(*i)) {
		printf(" [truncated]");
		return;
	}

	i = (struct ofp_instruction *)bp;
	bp += sizeof(*i);
	length -= sizeof(*i);
	instructionslen -= sizeof(*i);
	ilen = ntohs(i->i_len) - sizeof(*i);
	if (length < ilen) {
		printf(" [truncated]");
		return;
	}

	ofp_print_instruction(i, bp, length);

	bp += ilen;
	length -= ilen;
	instructionslen -= ilen;
	if (instructionslen > 0)
		goto parse_next_instruction;
}

void
ofp_print(const u_char *bp)
{
	struct ofp_header			*oh;
	unsigned int				 ohlen;
	u_int					 length;

	length = snapend - bp;
	if (length < sizeof(*oh)) {
		printf(" OpenFlow(truncated)");
		return;
	}

	oh = (struct ofp_header *)bp;
	ohlen = ntohs(oh->oh_length);
	printf(" OpenFlow(ver %#02x type %d len %d xid %u)",
	    oh->oh_version, oh->oh_type, ohlen, ntohl(oh->oh_xid));

	switch (oh->oh_version) {
	case OFP_V_1_3:
		break;

	default:
		return;
	}

	switch (oh->oh_type) {
	case OFP_T_HELLO:
		ofp_print_hello(bp, length, ohlen);
		break;
	case OFP_T_ERROR:
		ofp_print_error(bp, length);
		break;
	case OFP_T_ECHO_REQUEST:
		printf(" ECHO REQUEST");
		break;
	case OFP_T_ECHO_REPLY:
		printf(" ECHO REPLY");
		break;
	case OFP_T_FEATURES_REQUEST:
		printf(" FEATURES REQUEST");
		break;
	case OFP_T_FEATURES_REPLY:
		ofp_print_featuresreply(bp, length);
		break;
	case OFP_T_SET_CONFIG:
		ofp_print_setconfig(bp, length);
		break;
	case OFP_T_PACKET_IN:
		ofp_print_packetin(bp, length);
		break;
	case OFP_T_FLOW_REMOVED:
		ofp_print_flowremoved(bp, length);
		break;
	case OFP_T_PACKET_OUT:
		ofp_print_packetout(bp, length);
		break;
	case OFP_T_FLOW_MOD:
		ofp_print_flowmod(bp, length);
		break;
	}
}

void
oxm_print_byte(const u_char *bp, u_int length, int hasmask, int hex)
{
	uint8_t		*b;

	if (length < sizeof(*b)) {
		printf("[truncated]");
		return;
	}

	b = (uint8_t *)bp;
	if (hex)
		printf("%#02x", ntohs(*b));
	else
		printf("%u", ntohs(*b));

	if (hasmask) {
		bp += sizeof(*b);
		length -= sizeof(*b);
		printf(" mask ");
		oxm_print_word(bp, length, 0, 1);
	}
}

void
oxm_print_halfword(const u_char *bp, u_int length, int hasmask, int hex)
{
	uint16_t	*h;

	if (length < sizeof(*h)) {
		printf("[truncated]");
		return;
	}

	h = (uint16_t *)bp;
	if (hex)
		printf("%#04x", ntohs(*h));
	else
		printf("%u", ntohs(*h));

	if (hasmask) {
		bp += sizeof(*h);
		length -= sizeof(*h);
		printf(" mask ");
		oxm_print_word(bp, length, 0, 1);
	}
}

void
oxm_print_word(const u_char *bp, u_int length, int hasmask, int hex)
{
	uint32_t	*w;

	if (length < sizeof(*w)) {
		printf("[truncated]");
		return;
	}

	w = (uint32_t *)bp;
	if (hex)
		printf("%#08x", ntohl(*w));
	else
		printf("%u", ntohl(*w));

	if (hasmask) {
		bp += sizeof(*w);
		length -= sizeof(*w);
		printf(" mask ");
		oxm_print_word(bp, length, 0, 1);
	}
}

void
oxm_print_quad(const u_char *bp, u_int length, int hasmask, int hex)
{
	uint64_t	*q;

	if (length < sizeof(*q)) {
		printf("[truncated]");
		return;
	}

	q = (uint64_t *)bp;
	if (hex)
		printf("%#016llx", be64toh(*q));
	else
		printf("%llu", be64toh(*q));

	if (hasmask) {
		bp += sizeof(*q);
		length -= sizeof(*q);
		printf(" mask ");
		oxm_print_quad(bp, length, 0, 1);
	}
}

void
oxm_print_ether(const u_char *bp, u_int length, int hasmask)
{
	uint8_t		*ptr;
	int		 i;
	char		 hex[8];

	if (length < ETHER_HDR_LEN) {
		printf("[truncated]");
		return;
	}

	ptr = (uint8_t *)bp;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			printf(":");

		snprintf(hex, sizeof(hex), "%02x", ptr[i]);
		printf("%s", hex);
	}

	if (hasmask) {
		bp += ETHER_ADDR_LEN;
		length -= ETHER_ADDR_LEN;
		printf(" mask ");
		oxm_print_ether(bp, length, 0);
	}
}

void
oxm_print_data(const u_char *bp, u_int length, int hasmask, size_t datalen)
{
	uint8_t		*ptr;
	int		 i;
	char		 hex[8];

	if (length < datalen) {
		printf("[truncated]");
		return;
	}

	ptr = (uint8_t *)bp;
	for (i = 0; i < datalen; i++) {
		snprintf(hex, sizeof(hex), "%02x", ptr[i]);
		printf("%s", hex);
	}

	if (hasmask) {
		bp += datalen;
		length -= datalen;
		printf(" mask ");
		oxm_print_data(bp, length, 0, datalen);
	}
}

void
ofp_print_oxm(struct ofp_ox_match *oxm, const u_char *bp, u_int length)
{
	int				 class, field, mask, len;
	uint16_t			*vlan;

	class = ntohs(oxm->oxm_class);
	field = OFP_OXM_GET_FIELD(oxm);
	mask = OFP_OXM_GET_HASMASK(oxm);
	len = oxm->oxm_length;
	printf(" OXM <class %d field %d hasmask %d length %d",
	    class, field, mask, len);

	switch (class) {
	case OFP_OXM_C_OPENFLOW_BASIC:
		break;

	case OFP_OXM_C_NXM_0:
	case OFP_OXM_C_NXM_1:
	case OFP_OXM_C_OPENFLOW_EXPERIMENTER:
	default:
		printf(">");
		return;
	}

	printf(" value ");

	switch (field) {
	case OFP_XM_T_IN_PORT:
	case OFP_XM_T_IN_PHY_PORT:
	case OFP_XM_T_MPLS_LABEL:
		oxm_print_word(bp, length, mask, 0);
		break;

	case OFP_XM_T_META:
	case OFP_XM_T_TUNNEL_ID:
		oxm_print_quad(bp, length, mask, 1);
		break;

	case OFP_XM_T_ETH_DST:
	case OFP_XM_T_ETH_SRC:
	case OFP_XM_T_ARP_SHA:
	case OFP_XM_T_ARP_THA:
	case OFP_XM_T_IPV6_ND_SLL:
	case OFP_XM_T_IPV6_ND_TLL:
		oxm_print_ether(bp, length, mask);
		break;

	case OFP_XM_T_ETH_TYPE:
		oxm_print_halfword(bp, length, mask, 1);
		break;

	case OFP_XM_T_VLAN_VID:
		/*
		 * VLAN has an exception: it uses the higher bits to signal
		 * the presence of the VLAN.
		 */
		if (length < sizeof(*vlan)) {
			printf("[truncated]");
			break;
		}

		vlan = (uint16_t *)bp;
		if (ntohs(*vlan) & OFP_XM_VID_PRESENT)
			printf("(VLAN %d) ",
			    ntohs(*vlan) & (~OFP_XM_VID_PRESENT));
		else
			printf("(no VLAN) ");
		/* FALLTHROUGH */
	case OFP_XM_T_TCP_SRC:
	case OFP_XM_T_TCP_DST:
	case OFP_XM_T_UDP_SRC:
	case OFP_XM_T_UDP_DST:
	case OFP_XM_T_SCTP_SRC:
	case OFP_XM_T_SCTP_DST:
	case OFP_XM_T_ARP_OP:
	case OFP_XM_T_IPV6_EXTHDR:
		oxm_print_halfword(bp, length, mask, 0);
		break;

	case OFP_XM_T_VLAN_PCP:
	case OFP_XM_T_IP_DSCP:
	case OFP_XM_T_IP_ECN:
	case OFP_XM_T_MPLS_TC:
	case OFP_XM_T_MPLS_BOS:
		oxm_print_byte(bp, length, mask, 1);
		break;

	case OFP_XM_T_IPV4_SRC:
	case OFP_XM_T_IPV4_DST:
	case OFP_XM_T_ARP_SPA:
	case OFP_XM_T_ARP_TPA:
	case OFP_XM_T_IPV6_FLABEL:
		oxm_print_word(bp, length, mask, 1);
		break;

	case OFP_XM_T_IP_PROTO:
	case OFP_XM_T_ICMPV4_TYPE:
	case OFP_XM_T_ICMPV4_CODE:
	case OFP_XM_T_ICMPV6_TYPE:
	case OFP_XM_T_ICMPV6_CODE:
		oxm_print_byte(bp, length, mask, 0);
		break;

	case OFP_XM_T_IPV6_SRC:
	case OFP_XM_T_IPV6_DST:
	case OFP_XM_T_IPV6_ND_TARGET:
		oxm_print_data(bp, length, mask, sizeof(struct in6_addr));
		break;

	case OFP_XM_T_PBB_ISID:
		oxm_print_data(bp, length, mask, 3);
		break;

	default:
		printf("unknown");
		break;
	}

	printf(">");
}

void
action_print_output(const u_char *bp, u_int length)
{
	struct ofp_action_output		*ao;

	if (length < (sizeof(*ao) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	ao = (struct ofp_action_output *)(bp - AH_UNPADDED);
	printf(" port %u max_len %d",
	    ntohl(ao->ao_port), ntohs(ao->ao_max_len));
}

void
action_print_group(const u_char *bp, u_int length)
{
	struct ofp_action_group		*ag;

	if (length < (sizeof(*ag) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	ag = (struct ofp_action_group *)(bp - AH_UNPADDED);
	printf(" group_id %u", ntohl(ag->ag_group_id));
}

void
action_print_setmplsttl(const u_char *bp, u_int length)
{
	struct ofp_action_mpls_ttl	*amt;

	if (length < (sizeof(*amt) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	amt = (struct ofp_action_mpls_ttl *)(bp - AH_UNPADDED);
	printf(" ttl %d", amt->amt_ttl);
}

void
action_print_setnwttl(const u_char *bp, u_int length)
{
	struct ofp_action_nw_ttl	*ant;

	if (length < (sizeof(*ant) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	ant = (struct ofp_action_nw_ttl *)(bp - AH_UNPADDED);
	printf(" ttl %d", ant->ant_ttl);
}

void
action_print_push(const u_char *bp, u_int length)
{
	struct ofp_action_push		*ap;

	if (length < (sizeof(*ap) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	ap = (struct ofp_action_push *)(bp - AH_UNPADDED);
	printf(" ethertype %#04x", ntohs(ap->ap_ethertype));
}

void
action_print_popmpls(const u_char *bp, u_int length)
{
	struct ofp_action_pop_mpls	*apm;

	if (length < (sizeof(*apm) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	apm = (struct ofp_action_pop_mpls *)(bp - AH_UNPADDED);
	printf(" ethertype %#04x", ntohs(apm->apm_ethertype));
}

void
action_print_setfield(const u_char *bp, u_int length)
{
	struct ofp_action_set_field	*asf;
	struct ofp_ox_match		*oxm;
	int				 omlen;

	if (length < (sizeof(*asf) - AH_UNPADDED)) {
		printf(" [truncated]");
		return;
	}

	asf = (struct ofp_action_set_field *)(bp - AH_UNPADDED);
	omlen = ntohs(asf->asf_len) - AH_UNPADDED;
	if (omlen == 0)
		return;

 parse_next_oxm:
	if (length < sizeof(*oxm)) {
		printf(" [truncated]");
		return;
	}

	oxm = (struct ofp_ox_match *)bp;
	bp += sizeof(*oxm);
	length -= sizeof(*oxm);
	if (length < oxm->oxm_length) {
		printf(" [truncated]");
		return;
	}

	ofp_print_oxm(oxm, bp, length);

	bp += oxm->oxm_length;
	length -= oxm->oxm_length;
	omlen -= min(sizeof(*oxm) + oxm->oxm_length, omlen);
	if (omlen)
		goto parse_next_oxm;
}

void
ofp_print_action(struct ofp_action_header *ah, const u_char *bp, u_int length)
{
	int			ahtype;

	ahtype = ntohs(ah->ah_type);
	printf(" ACTION <type %d length %d", ahtype, ntohs(ah->ah_len));

	switch (ahtype) {
	case OFP_ACTION_OUTPUT:
		action_print_output(bp, length);
		break;

	case OFP_ACTION_GROUP:
		action_print_group(bp, length);
		break;

	case OFP_ACTION_SET_QUEUE:
		/* TODO missing struct in ofp.h header. */
		break;

	case OFP_ACTION_SET_MPLS_TTL:
		action_print_setmplsttl(bp, length);
		break;

	case OFP_ACTION_SET_NW_TTL:
		action_print_setnwttl(bp, length);
		break;

	case OFP_ACTION_PUSH_VLAN:
	case OFP_ACTION_PUSH_MPLS:
	case OFP_ACTION_PUSH_PBB:
		action_print_push(bp, length);
		break;

	case OFP_ACTION_POP_MPLS:
		action_print_popmpls(bp, length);
		break;

	case OFP_ACTION_SET_FIELD:
		break;

	case OFP_ACTION_COPY_TTL_OUT:
	case OFP_ACTION_COPY_TTL_IN:
	case OFP_ACTION_DEC_NW_TTL:
	case OFP_ACTION_DEC_MPLS_TTL:
	case OFP_ACTION_POP_VLAN:
	case OFP_ACTION_POP_PBB:
	case OFP_ACTION_EXPERIMENTER:
	default:
		/* Generic header, nothing to show here. */
		break;
	}

	printf(">");
}

void
instruction_print_gototable(const char *bp, u_int length)
{
	struct ofp_instruction_goto_table	*igt;

	if (length < (sizeof(*igt) - sizeof(struct ofp_instruction))) {
		printf(" [truncated]");
		return;
	}

	igt = (struct ofp_instruction_goto_table *)
	    (bp - sizeof(struct ofp_instruction));
	printf(" table_id %d", igt->igt_table_id);
}

void
instruction_print_meta(const char *bp, u_int length)
{
	struct ofp_instruction_write_metadata	*iwm;

	if (length < (sizeof(*iwm) - sizeof(struct ofp_instruction))) {
		printf(" [truncated]");
		return;
	}

	iwm = (struct ofp_instruction_write_metadata *)
	    (bp - sizeof(struct ofp_instruction));
	printf(" metadata %llu metadata_mask %llu",
	    be64toh(iwm->iwm_metadata), be64toh(iwm->iwm_metadata_mask));
}

void
instruction_print_actions(const char *bp, u_int length)
{
	struct ofp_instruction_actions		*ia;
	struct ofp_action_header		*ah;
	int					 actionslen;
	unsigned int				 ahlen;

	if (length < (sizeof(*ia) - sizeof(struct ofp_instruction))) {
		printf(" [truncated]");
		return;
	}

	ia = (struct ofp_instruction_actions *)
	    (bp - sizeof(struct ofp_instruction));

	actionslen = ntohs(ia->ia_len) - sizeof(*ia);
	if (actionslen <= 0)
		return;

	bp += sizeof(*ia) - sizeof(struct ofp_instruction);
	length -= sizeof(*ia) - sizeof(struct ofp_instruction);

parse_next_action:
	if (length < sizeof(*ah)) {
		printf(" [truncated]");
		return;
	}

	ah = (struct ofp_action_header *)bp;
	bp += AH_UNPADDED;
	length -= AH_UNPADDED;
	actionslen -= AH_UNPADDED;
	ahlen = ntohs(ah->ah_len) - AH_UNPADDED;
	if (length < ahlen) {
		printf(" [truncated]");
		return;
	}

	ofp_print_action(ah, bp, length);

	bp += ahlen;
	length -= ahlen;
	actionslen -= min(ahlen, actionslen);
	if (actionslen)
		goto parse_next_action;
}

void
instruction_print_meter(const char *bp, u_int length)
{
	struct ofp_instruction_meter		*im;

	if (length < (sizeof(*im) - sizeof(struct ofp_instruction))) {
		printf(" [truncated]");
		return;
	}

	im = (struct ofp_instruction_meter *)
	    (bp - sizeof(struct ofp_instruction));
	printf(" meter_id %u", ntohl(im->im_meter_id));
}

void
instruction_print_experimenter(const char *bp, u_int length)
{
	struct ofp_instruction_experimenter		*ie;

	if (length < (sizeof(*ie) - sizeof(struct ofp_instruction))) {
		printf(" [truncated]");
		return;
	}

	ie = (struct ofp_instruction_experimenter *)
	    (bp - sizeof(struct ofp_instruction));
	printf(" experimenter %u", ntohl(ie->ie_experimenter));
}

void
ofp_print_instruction(struct ofp_instruction *i, const char *bp, u_int length)
{
	int			itype;

	itype = ntohs(i->i_type);
	printf(" INSTRUCTION <type %d length %d", itype, ntohs(i->i_len));

	switch (itype) {
	case OFP_INSTRUCTION_T_GOTO_TABLE:
		instruction_print_gototable(bp, length);
		break;
	case OFP_INSTRUCTION_T_WRITE_META:
		instruction_print_meta(bp, length);
		break;
	case OFP_INSTRUCTION_T_WRITE_ACTIONS:
	case OFP_INSTRUCTION_T_APPLY_ACTIONS:
	case OFP_INSTRUCTION_T_CLEAR_ACTIONS:
		instruction_print_actions(bp, length);
		break;
	case OFP_INSTRUCTION_T_METER:
		instruction_print_meter(bp, length);
		break;
	case OFP_INSTRUCTION_T_EXPERIMENTER:
		instruction_print_meter(bp, length);
		break;
	}

	printf(">");
}
