/*	$OpenBSD: parser.h,v 1.22 2010/05/03 13:11:41 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include "bgpd.h"

enum actions {
	NONE,
	SHOW,
	SHOW_SUMMARY,
	SHOW_SUMMARY_TERSE,
	SHOW_NEIGHBOR,
	SHOW_NEIGHBOR_TIMERS,
	SHOW_NEIGHBOR_TERSE,
	SHOW_FIB,
	SHOW_FIB_TABLES,
	SHOW_RIB,
	SHOW_RIB_MEM,
	SHOW_NEXTHOP,
	SHOW_INTERFACE,
	RELOAD,
	FIB,
	FIB_COUPLE,
	FIB_DECOUPLE,
	LOG_VERBOSE,
	LOG_BRIEF,
	NEIGHBOR,
	NEIGHBOR_UP,
	NEIGHBOR_DOWN,
	NEIGHBOR_CLEAR,
	NEIGHBOR_RREFRESH,
	NETWORK_ADD,
	NETWORK_REMOVE,
	NETWORK_FLUSH,
	NETWORK_SHOW,
	IRRFILTER
};

struct parse_result {
	struct bgpd_addr	 addr;
	struct bgpd_addr	 peeraddr;
	struct filter_as	 as;
	struct filter_set_head	 set;
	struct filter_community  community;
	char			 peerdesc[PEER_DESCR_LEN];
	char			 rib[PEER_DESCR_LEN];
	char			*irr_outdir;
	int			 flags;
	u_int			 rtableid;
	enum actions		 action;
	u_int8_t		 prefixlen;
	u_int8_t		 aid;
};

__dead void		 usage(void);
struct parse_result	*parse(int, char *[]);
