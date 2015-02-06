/*	$OpenBSD: privsep.h,v 1.25 2015/02/06 06:47:29 krw Exp $ */

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

#include <arpa/inet.h>

#include <imsg.h>

enum imsg_code {
	IMSG_NONE,
	IMSG_DELETE_ADDRESS,
	IMSG_ADD_ADDRESS,
	IMSG_FLUSH_ROUTES,
	IMSG_ADD_ROUTE,
	IMSG_HUP,
	IMSG_WRITE_FILE
};

struct imsg_delete_address {
	struct	in_addr addr;
};

struct imsg_add_address {
	struct	in_addr	addr;
	struct	in_addr mask;
};

struct imsg_flush_routes {
	int	zapzombies;
};

struct imsg_add_route {
	struct in_addr	dest;
	struct in_addr	netmask;
	struct in_addr	gateway;
	int		addrs;
	int		flags;
};

struct imsg_hup {
	struct	in_addr addr;
};

struct imsg_write_file {
	char	path[PATH_MAX];
	int	flags;
	mode_t	mode;
	size_t	len;
	uid_t	uid;
	gid_t	gid;
};

void	dispatch_imsg(struct imsgbuf *);
void	priv_delete_address(struct imsg_delete_address *);
void	priv_add_address(struct imsg_add_address *);
void	priv_flush_routes(struct imsg_flush_routes *);
void	priv_add_route(struct imsg_add_route *);
void	priv_cleanup(struct imsg_hup *);
void	priv_write_file(struct imsg_write_file *);
