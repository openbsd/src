/*	$OpenBSD: privsep.h,v 1.56 2017/08/31 17:01:48 krw Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE, ABUSE OR PERFORMANCE OF THIS SOFTWARE.
 */

enum imsg_code {
	IMSG_NONE,
	IMSG_HUP,
	IMSG_DELETE_ADDRESS,
	IMSG_SET_ADDRESS,
	IMSG_FLUSH_ROUTES,
	IMSG_ADD_ROUTE,
	IMSG_SET_MTU,
	IMSG_SET_RESOLV_CONF,
	IMSG_WRITE_RESOLV_CONF
};

struct imsg_delete_address {
	struct	in_addr addr;
};

struct imsg_set_address {
	struct	in_addr	addr;
	struct	in_addr mask;
};

struct imsg_flush_routes {
	uint8_t		rtstatic[RTSTATIC_LEN];
	unsigned int	rtstatic_len;
};

struct imsg_add_route {
	struct in_addr	dest;
	struct in_addr	netmask;
	struct in_addr	gateway;
	int		flags;
};

struct imsg_set_mtu {
	int	mtu;
};

void	dispatch_imsg(char *, int, int, int, struct imsgbuf *);
int	default_route_index(int, int);

void	priv_add_route(char *, int, int, struct imsg_add_route *);
void	priv_flush_routes(int, int, int, struct imsg_flush_routes *);

void	priv_write_resolv_conf(char *);

void	priv_delete_address(char *, int, struct imsg_delete_address *);
void	priv_set_address(char *, int, struct imsg_set_address *);

void	priv_set_mtu(char *, int, struct imsg_set_mtu *);
