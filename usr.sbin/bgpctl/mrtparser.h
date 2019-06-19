/*	$OpenBSD: mrtparser.h,v 1.3 2019/02/25 11:51:58 claudio Exp $ */
/*
 * Copyright (c) 2011 Claudio Jeker <claudio@openbsd.org>
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

#include "bgpd.h"

/* data structures for the MSG_TABLE_DUMP_V2 format */
struct mrt_peer_entry {
	struct bgpd_addr	addr;
	u_int32_t		bgp_id;
	u_int32_t		asnum;
};

struct mrt_peer {
	char			*view;
	struct mrt_peer_entry	*peers;
	u_int32_t		 bgp_id;
	u_int16_t		 npeers;
};

struct mrt_attr {
	void	*attr;
	size_t	 attr_len;
};

struct mrt_rib_entry {
	void		*aspath;
	struct mrt_attr	*attrs;
	struct bgpd_addr nexthop;
	time_t		 originated;
	u_int32_t	 local_pref;
	u_int32_t	 med;
	u_int16_t	 peer_idx;
	u_int16_t	 aspath_len;
	u_int16_t	 nattrs;
	u_int8_t	 origin;
};

struct mrt_rib {
	struct mrt_rib_entry	*entries;
	struct bgpd_addr	 prefix;
	u_int32_t		 seqnum;
	u_int16_t		 nentries;
	u_int8_t		 prefixlen;
};

/* data structures for the BGP4MP MESSAGE and STATE types */
struct mrt_bgp_state {
	struct timespec		time;
	struct bgpd_addr	src;
	struct bgpd_addr	dst;
	u_int32_t		src_as;
	u_int32_t		dst_as;
	u_int16_t		old_state;
	u_int16_t		new_state;
};

struct mrt_bgp_msg {
	struct timespec	 time;
	struct bgpd_addr src;
	struct bgpd_addr dst;
	u_int32_t	 src_as;
	u_int32_t	 dst_as;
	u_int16_t	 msg_len;
	void		*msg;
};

#define MRT_ATTR_ORIGIN		1
#define MRT_ATTR_ASPATH		2
#define MRT_ATTR_NEXTHOP	3
#define MRT_ATTR_MED		4
#define MRT_ATTR_LOCALPREF	5
#define MRT_ATTR_MP_REACH_NLRI	14
#define MRT_ATTR_AS4PATH	17
#define MRT_ATTR_EXTLEN		0x10

#define MRT_PREFIX_LEN(x)	((((u_int)x) + 7) / 8)

struct mrt_parser {
	void	(*dump)(struct mrt_rib *, struct mrt_peer *, void *);
	void	(*state)(struct mrt_bgp_state *, void *);
	void	(*message)(struct mrt_bgp_msg *, void *);
	void	*arg;
};

void	mrt_parse(int, struct mrt_parser *, int);
