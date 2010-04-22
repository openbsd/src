/*	$OpenBSD: mrt.h,v 1.25 2010/04/22 08:18:00 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org>
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
 * MRT binary packet format
 * For more info see:
 * draft-ietf-grow-mrt-04.txt, "MRT routing information export format"
 * http://www.quagga.net/docs/docs-multi/Packet-Binary-Dump-Format.html
 */

/*
 * MRT header:
 * +--------+--------+--------+--------+
 * |             timestamp             |
 * +--------+--------+--------+--------+
 * |      type       |     subtype     |
 * +--------+--------+--------+--------+
 * |               length              | length of packet excluding this header
 * +--------+--------+--------+--------+
 *
 * ET types include an additional 32bit microsecond field comming after the
 * length field.
 */
#define MRT_HEADER_SIZE		12

enum MRT_MSG_TYPES {
	MSG_NULL,		/*  0 empty msg (deprecated) */
	MSG_START,		/*  1 sender is starting up */
	MSG_DIE,		/*  2 receiver should shut down (deprecated) */
	MSG_I_AM_DEAD,		/*  3 sender is shutting down */
	MSG_PEER_DOWN,		/*  4 sender's peer is down (deprecated) */
	MSG_PROTOCOL_BGP,	/*  5 msg is a BGP packet (deprecated) */
	MSG_PROTOCOL_RIP,	/*  6 msg is a RIP packet */
	MSG_PROTOCOL_IDRP,	/*  7 msg is an IDRP packet (deprecated) */
	MSG_PROTOCOL_RIPNG,	/*  8 msg is a RIPNG packet */
	MSG_PROTOCOL_BGP4PLUS,	/*  9 msg is a BGP4+ packet (deprecated) */
	MSG_PROTOCOL_BGP4PLUS1,	/* 10 msg is a BGP4+ (draft 01) (deprecated) */
	MSG_PROTOCOL_OSPF,	/* 11 msg is an OSPF packet */
	MSG_TABLE_DUMP,		/* 12 routing table dump */
	MSG_TABLE_DUMP_V2,	/* 13 routing table dump */
	MSG_PROTOCOL_BGP4MP=16,	/* 16 zebras own packet format */
	MSG_PROTOCOL_BGP4MP_ET=17,
	MSG_PROTOCOL_ISIS=32,	/* 32 msg is a ISIS package */
	MSG_PROTOCOL_ISIS_ET=33,
	MSG_PROTOCOL_OSPFV3=48,	/* 48 msg is a OSPFv3 package */
	MSG_PROTOCOL_OSPFV3_ET=49
};

/*
 * Main zebra dump format is in MSG_PROTOCOL_BGP4MP exceptions are table dumps
 * that are normaly saved as MSG_TABLE_DUMP.
 * In most cases this is the format to choose to dump updates et al.
 */
enum MRT_BGP4MP_TYPES {
	BGP4MP_STATE_CHANGE,	/* state change */
	BGP4MP_MESSAGE,		/* bgp message */
	BGP4MP_ENTRY,		/* table dumps (deprecated) */
	BGP4MP_SNAPSHOT,	/* file name for dump (deprecated) */
	BGP4MP_MESSAGE_AS4,	/* same as BGP4MP_MESSAGE with 4byte AS */
	BGP4MP_STATE_CHANGE_AS4,
	BGP4MP_MESSAGE_LOCAL,	  /* same as BGP4MP_MESSAGE but for self */
	BGP4MP_MESSAGE_AS4_LOCAL  /* originated updates. Not implemented */
};

/* size of the BGP4MP headers without payload */
#define MRT_BGP4MP_IPv4_HEADER_SIZE	16
#define MRT_BGP4MP_IPv6_HEADER_SIZE	40
/* 4-byte AS variants of the previous */
#define MRT_BGP4MP_AS4_IPv4_HEADER_SIZE	20
#define MRT_BGP4MP_AS4_IPv6_HEADER_SIZE	44

/* If the type is PROTOCOL_BGP4MP and the subtype is either BGP4MP_STATE_CHANGE
 * or BGP4MP_MESSAGE the message consists of a common header plus the payload.
 * Header format:
 *
 * +--------+--------+--------+--------+
 * |    source_as    |     dest_as     |
 * +--------+--------+--------+--------+
 * |    if_index     |       afi       |
 * +--------+--------+--------+--------+
 * |             source_ip             |
 * +--------+--------+--------+--------+
 * |              dest_ip              |
 * +--------+--------+--------+--------+
 * |      message specific payload ...
 * +--------+--------+--------+--------+
 *
 * The source_ip and dest_ip are dependant of the afi type. For IPv6 source_ip
 * and dest_ip are both 16 bytes long.
 *
 * Payload of a BGP4MP_STATE_CHANGE packet:
 *
 * +--------+--------+--------+--------+
 * |    old_state    |    new_state    |
 * +--------+--------+--------+--------+
 *
 * The state values are the same as in RFC 1771.
 *
 * The payload of a BGP4MP_MESSAGE is the full bgp message with header.
 */

/*
 * size of the BGP4MP entries without variable stuff.
 * All until nexthop plus attr_len, not included plen, prefix and bgp attrs.
 */
#define MRT_BGP4MP_IPv4_ENTRY_SIZE	18
#define MRT_BGP4MP_IPv6_ENTRY_SIZE	30
#define MRT_BGP4MP_MAX_PREFIXLEN	17
/*
 * The "new" table dump format consists of messages of type PROTOCOL_BGP4MP
 * and subtype BGP4MP_ENTRY.
 *
 * +--------+--------+--------+--------+
 * |      view       |     status      |
 * +--------+--------+--------+--------+
 * |            originated             |
 * +--------+--------+--------+--------+
 * |       afi       |  safi  | nhlen  |
 * +--------+--------+--------+--------+
 * |              nexthop              |
 * +--------+--------+--------+--------+
 * |  plen  |  prefix variable  ...    |
 * +--------+--------+--------+--------+
 * |    attr_len     | bgp attributes
 * +--------+--------+--------+--------+
 *  bgp attributes, attr_len bytes long
 * +--------+--------+--------+--------+
 *   ...                      |
 * +--------+--------+--------+
 *
 * View is normaly 0 and originated the time of last change.
 * The status seems to be 1 by default but probably something to indicate
 * the status of a prefix would be more useful.
 * The format of the nexthop address is defined via the afi value. For IPv6
 * the nexthop field is 16 bytes long.
 * The prefix field is as in the bgp update message variable length. It is
 * plen bits long but rounded to the next octet.
 */

/*
 * Format for routing table dumps in "old" mrt format.
 * Type MSG_TABLE_DUMP and subtype is AFI_IPv4 (1) for IPv4 and AFI_IPv6 (2)
 * for IPv6. In the IPv6 case prefix and peer_ip are both 16 bytes long.
 *
 * +--------+--------+--------+--------+
 * |      view       |      seqnum     |
 * +--------+--------+--------+--------+
 * |               prefix              |
 * +--------+--------+--------+--------+
 * |  plen  | status | originated time
 * +--------+--------+--------+--------+
 *   originated time |     peer_ip
 * +--------+--------+--------+--------+
 *       peer_ip     |     peer_as     |
 * +--------+--------+--------+--------+
 * |    attr_len     | bgp attributes
 * +--------+--------+--------+--------+
 *  bgp attributes, attr_len bytes long
 * +--------+--------+--------+--------+
 *   ...                      |
 * +--------+--------+--------+
 *
 *
 * View is normaly 0 and seqnum just a simple counter for this dump.
 * The status field is unused and should be set to 1.
 */

/* size of the dump header until attr_len */
#define MRT_DUMP_HEADER_SIZE	22

/*
 * OLD MRT message headers. These structs are here for completion but
 * will not be used to generate dumps. It seems that nobody uses those.
 *
 * Only for bgp messages (type 5, 9 and 10)
 * Nota bene for bgp dumps MSG_PROTOCOL_BGP4MP should be used.
 */
enum MRT_BGP_TYPES {
	MSG_BGP_NULL,
	MSG_BGP_UPDATE,		/* raw update packet (contains both withdraws
				   and announcements) */
	MSG_BGP_PREF_UPDATE,	/* tlv preferences followed by raw update */
	MSG_BGP_STATE_CHANGE,	/* state change */
	MSG_BGP_SYNC,		/* file name for a table dump */
	MSG_BGP_OPEN,		/* BGP open messages */
	MSG_BGP_NOTIFY,		/* BGP notify messages */
	MSG_BGP_KEEPALIVE	/* BGP keepalives */
};

/* if type MSG_PROTOCOL_BGP and subtype MSG_BGP_UPDATE, MSG_BGP_OPEN,
 * MSG_BGP_NOTIFY or MSG_BGP_KEEPALIVE
 *
 * +--------+--------+--------+--------+
 * |    source_as    |    source_ip
 * +--------+--------+--------+--------+
 *      source_ip    |    dest_as      |
 * +--------+--------+--------+--------+
 * |               dest_ip             |
 * +--------+--------+--------+--------+
 * | bgp update packet with header et
 * +--------+--------+--------+--------+
 *   al. (variable length) ...
 * +--------+--------+--------+--------+
 *
 * For IPv6 the type is MSG_PROTOCOL_BGP4PLUS and the subtype remains
 * MSG_BGP_UPDATE. The source_ip and dest_ip are again extended to 16 bytes.
 */

/*
 * For subtype MSG_BGP_STATECHANGE (for all BGP types or just for the
 * MSG_PROTOCOL_BGP4PLUS case? Unclear.)
 *
 * +--------+--------+--------+--------+
 * |    source_as    |    source_ip
 * +--------+--------+--------+--------+
 *      source_ip    |    old_state    |
 * +--------+--------+--------+--------+
 * |    new_state    |
 * +--------+--------+
 *
 * State are defined in RFC 1771.
 */

/*
 * if type MSG_PROTOCOL_BGP and subtype MSG_BGP_SYNC OR
 * if type MSG_PROTOCOL_BGP4MP and subtype BGP4MP_SNAPSHOT
 * *DEPRECATED*
 *
 * +--------+--------+--------+--------+
 * |      view       |    filename
 * +--------+--------+--------+--------+
 *    filename variable length zero
 * +--------+--------+--------+--------+
 *    terminated ... |   0    |
 * +--------+--------+--------+
 */

#define	MRT_FILE_LEN	512
enum mrt_type {
	MRT_NONE,
	MRT_TABLE_DUMP,
	MRT_TABLE_DUMP_MP,
	MRT_ALL_IN,
	MRT_ALL_OUT,
	MRT_UPDATE_IN,
	MRT_UPDATE_OUT
};

enum mrt_state {
	MRT_STATE_RUNNING,
	MRT_STATE_OPEN,
	MRT_STATE_REOPEN,
	MRT_STATE_REMOVE
};

struct mrt {
	char			rib[PEER_DESCR_LEN];
	struct msgbuf		wbuf;
	LIST_ENTRY(mrt)		entry;
	u_int32_t		peer_id;
	u_int32_t		group_id;
	enum mrt_type		type;
	enum mrt_state		state;
	u_int16_t		seqnum;
};

struct mrt_config {
	struct mrt		conf;
	char			name[MRT_FILE_LEN];	/* base file name */
	char			file[MRT_FILE_LEN];	/* actual file name */
	time_t			ReopenTimer;
	time_t			ReopenTimerInterval;
};

#define	MRT2MC(x)	((struct mrt_config *)(x))
#define	MRT_MAX_TIMEOUT	7200

struct peer;
struct prefix;
struct rib_entry;

/* prototypes */
void		 mrt_dump_bgp_msg(struct mrt *, void *, u_int16_t,
		     struct peer *);
void		 mrt_dump_state(struct mrt *, u_int16_t, u_int16_t,
		     struct peer *);
void		 mrt_clear_seq(void);
void		 mrt_dump_upcall(struct rib_entry *, void *);
void		 mrt_done(void *);
void		 mrt_write(struct mrt *);
void		 mrt_clean(struct mrt *);
void		 mrt_init(struct imsgbuf *, struct imsgbuf *);
int		 mrt_timeout(struct mrt_head *);
void		 mrt_reconfigure(struct mrt_head *);
void		 mrt_handler(struct mrt_head *);
struct mrt	*mrt_get(struct mrt_head *, struct mrt *);
int		 mrt_mergeconfig(struct mrt_head *, struct mrt_head *);

#endif
