/*	$OpenBSD: mrt.h,v 1.4 2003/12/21 23:26:37 henning Exp $ */

/*
 * Copyright (c) 2003 Claudio Jeker <cjeker@diehard.n-r-g.com>
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
#ifndef __MRT_H__
#define __MRT_H__

#include "bgpd.h"

/*
 * MRT binary packet format as used by zebra.
 * For more info see:
 * http://www.mrtd.net/mrt_doc/html/mrtprogrammer.html#_Toc412283890
 * or
 * http://www.mrtd.net/mrt_doc/PDF/mrtprogrammer.pdf (Chapter 12)
 * and
 * http://www.quagga.net/docs/docs-multi/Packet-Binary-Dump-Format.html
 */

struct mrt_header {
	u_int32_t	timestamp;
	u_int16_t	type;
	u_int16_t	subtype;
	u_int32_t	length;		/* length of packet including header */
};

enum MRT_MSG_TYPES {
	MSG_NULL,
	MSG_START,		/*  1 sender is starting up */
	MSG_DIE,		/*  2 receiver should shut down */
	MSG_I_AM_DEAD,		/*  3 sender is shutting down */
	MSG_PEER_DOWN,		/*  4 sender's peer is down */
	MSG_PROTOCOL_BGP,	/*  5 msg is a BGP packet */
	MSG_PROTOCOL_RIP,	/*  6 msg is a RIP packet */
	MSG_PROTOCOL_IDRP,	/*  7 msg is an IDRP packet */
	MSG_PROTOCOL_RIPNG,	/*  8 msg is a RIPNG packet */
	MSG_PROTOCOL_BGP4PLUS,	/*  9 msg is a BGP4+ packet */
	MSG_PROTOCOL_BGP4PLUS1,	/* 10 msg is a BGP4+ (draft 01) packet */
	MSG_PROTOCOL_OSPF,	/* 11 msg is an OSPF packet */
	MSG_TABLE_DUMP,		/* 12 routing table dump */
	MSG_PROTOCOL_BGP4MP=16,	/* 16 zebras own packet format */
};

#define MRT_HEADER_SIZE		sizeof(struct mrt_header)
#define MRT_DUMP_HEADER_SIZE	22	/* sizeof(struct mrt_dump_v4_header) */
#define MRT_BGP4MP_HEADER_SIZE	\
	sizeof(struct mrt_bgp4mp_header) + \
	sizeof(struct mrt_bgp4mp_IPv4)
/*
 * format for routing table dumps in mrt format.
 * Type MSG_TABLE_DUMP and subtype is address family (IPv4)
 */
struct mrt_dump_v4_header {
	u_int16_t	view;	/* normally 0 */
	u_int16_t	seqnum;	/* simple counter for this dump */
	u_int32_t	prefix;
	u_int8_t	prefixlen;
	u_int8_t	status;	/* default seems to be 1 */
	u_int32_t	originated; /* seems to be time of last update */
	u_int32_t	peer_ip;
	u_int16_t	peer_as;
	u_int16_t	attr_len;
	/* bgp attributes attr_len bytes long */
};

/*
 * format for routing table dumps in mrt format.
 * Type MSG_TABLE_DUMP and subtype is address family (IPv6)
 */
struct mrt_dump_v6_header {
	u_int16_t	view;	/* normally 0 */
	u_int16_t	seqnum;	/* simple counter for this dump */
	u_int32_t	prefix[4];
	u_int8_t	prefixlen;
	u_int8_t	status;	/* default seems to be 1 */
	u_int32_t	originated; /* seems to be time of last update */
	u_int32_t	peer_ip[4];
	u_int16_t	peer_as;
	u_int16_t	attr_len;
	/* bgp attributes attr_len bytes long */
};



/*
 * Main zebra dump format is in MSG_PROTOCOL_BGP4MP exptions are table dumps
 * that are normaly saved as MSG_TABLE_DUMP.
 * In most cases this is the format to choose to dump updates et al.
 */
enum MRT_BGP4MP_TYPES {
	BGP4MP_STATE_CHANGE=0,	/* state change */
	BGP4MP_MESSAGE=1,	/* bgp message */
	BGP4MP_ENTRY=2,		/* table dumps */
	BGP4MP_SNAPSHOT=3
};

/* if type PROTOCOL_BGP4MP and
 * subtype BGP4MP_STATE_CHANGE or BGP4MP_MESSAGE  */
struct mrt_bgp4mp_header {
	u_int16_t	source_as;
	u_int16_t	dest_as;
	u_int16_t	if_index;
	u_int16_t	afi;
	/*
	 * Next comes either a struct mrt_bgp4mp_IPv4 or a
	 * struct mrt_bgp4mp_IPv6 dependant on afi type.
	 *
	 * Last but not least the payload.
	 */
};

/* if afi = 4 */
struct mrt_bgp4mp_IPv4 {
	u_int32_t	source_ip;
	u_int32_t	dest_ip;
};

/* if afi = 6 */
struct mrt_bgp4mp_IPv6 {
	u_int32_t	source_ip[4];
	u_int32_t	dest_ip[4];
};

/*
 * payload of a BGP4MP_STATE_CHANGE packet
 */
struct mrt_bgp4mp_state_data {
	u_int16_t	old_state;	/* as in RFC 1771 */
	u_int16_t	new_state;
};

/*
 * The payload of a BGP4MP_MESSAGE is the full bgp message with header.
 */

/* if type PROTOCOL_BGP4MP, subtype BGP4MP_ENTRY */
struct mrt_bgp4mp_entry_header {
	u_int16_t	view;
	u_int16_t	status;
	u_int32_t	originated;	/* time last change */
	u_int16_t	afi;
	u_int8_t	safi;
	u_int8_t	nexthop_len;
	/*
	 * u_int32_t nexthop    for IPv4 or
	 * u_int32_t nexthop[4] for IPv6
	 * u_int8_t  prefixlen
	 * variable prefix (prefixlen bits long rounded to the next octet)
	 * u_int16_t attrlen
	 * variable lenght bgp attributes attrlen bytes long
	 */
};

/*
 * What is this for?
 * if type MSG_PROTOCOL_BGP and subtype MSG_BGP_SYNC OR
 * if type MSG_PROTOCOL_BGP4MP and subtype BGP4MP_SNAPSHOT
 */
struct mrt_bgp_sync_header {
	u_int16_t	view;
	char		filename[2]; /* variable */
};

/*
 * OLD MRT message headers. These structs are here for completition but
 * will not be used to generate dumps. It seems that nobody uses those.
 */

/*
 * Only for bgp messages (type 5, 9 and 10)
 * Nota bene for bgp dumps MSG_PROTOCOL_BGP4MP should be used.
 */
enum MRT_BGP_TYPES {
	MSG_BGP_NULL,
	MSG_BGP_UPDATE,		/* raw update packet (contains both withdraws
				   and announcements) */
	MSG_BGP_PREF_UPDATE,	/* tlv preferences followed by raw update */
	MSG_BGP_STATE_CHANGE,	/* state change */
	MSG_BGP_SYNC
};

/* if type MSG_PROTOCOL_BGP and subtype MSG_BGP_UPDATE */
struct mrt_bgp_update_header {
	u_int16_t	source_as;
	u_int32_t	source_ip;
	u_int16_t	dest_as;
	u_int32_t	dest_ip;
	/* bgp update packet */
};

/* if type MSG_PROTOCOL_BGP4PLUS and subtype MSG_BGP_UPDATE */
struct mrt_bgp_update_plus_header {
	u_int16_t	source_as;
	u_int32_t	source_ip[4];
	u_int16_t	dest_as;
	u_int32_t	dest_ip[4];
	/* bgp update packet */
};

/* for subtype MSG_BGP_STATECHANGE (for all BGP types ???) */
struct mrt_bgp_state_header {
	u_int16_t	source_as;
	u_int32_t	source_ip[4];
	u_int16_t	old_state;	/* as in RFC 1771 ??? */
	u_int16_t	new_state;
};

/* pseudo predeclarations */
struct prefix;
struct pt_entry;
struct mrt {
	struct msgbuf	*msgbuf;
	u_int32_t	 id;
};

/* prototypes */
int	mrt_dump_bgp_msg(struct mrt *, void *, u_int16_t, int,
    struct peer_config *, struct bgpd_config *);
void	mrt_clear_seq(void);
void	mrt_dump_upcall(struct pt_entry *, void *);
int	mrt_state(struct mrtdump_config *, enum imsg_type, struct imsgbuf *);
int	mrt_alrm(struct mrt_config *, struct imsgbuf *);
int	mrt_usr1(struct mrt_config *, struct imsgbuf *);

#endif
