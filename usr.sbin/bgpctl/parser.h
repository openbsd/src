/*	$OpenBSD: parser.h,v 1.4 2004/05/21 11:52:32 claudio Exp $ */

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
	SHOW_NEIGHBOR,
	SHOW_NEIGHBOR_TIMERS,
	SHOW_FIB,
	SHOW_RIB,
	SHOW_NEXTHOP,
	SHOW_INTERFACE,
	RELOAD,
	FIB,
	FIB_COUPLE,
	FIB_DECOUPLE,
	NEIGHBOR,
	NEIGHBOR_UP,
	NEIGHBOR_DOWN,
	NETWORK_ADD,
	NETWORK_REMOVE,
	NETWORK_FLUSH,
	NETWORK_SHOW
};

struct parse_result {
	enum actions		action;
	int			flags;
	struct bgpd_addr	addr;
	u_int8_t		prefixlen;
	struct as_filter	as;
};

struct parse_result	*parse(int, char *[]);
