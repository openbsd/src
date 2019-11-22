/*	$OpenBSD: privsep.h,v 1.61 2019/11/22 22:45:52 krw Exp $ */

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
	IMSG_REVOKE,
	IMSG_WRITE_RESOLV_CONF,
	IMSG_PROPOSE,
	IMSG_TELL_UNWIND
};

#define	RTLEN	128

struct proposal {
	uint8_t		rtstatic[RTLEN];
	uint8_t		rtsearch[RTLEN];
	uint8_t		rtdns[RTLEN];
	struct in_addr	ifa;
	struct in_addr	netmask;
	unsigned int	rtstatic_len;
	unsigned int	rtsearch_len;
	unsigned int	rtdns_len;
	int		mtu;
	int		addrs;
	int		inits;
};

struct unwind_info {
	in_addr_t	ns[MAXNS];
	unsigned int	count;
};

struct imsg_propose {
	struct proposal		proposal;
};

struct imsg_revoke {
	struct proposal		proposal;
};

struct imsg_tell_unwind {
	struct unwind_info	unwind_info;
};

void	dispatch_imsg(char *, int, int, int, struct imsgbuf *);

void	priv_write_resolv_conf(int, int, int, char *, int *);
void	priv_propose(char *, int, struct imsg_propose *, char **, int, int, int);

void	priv_revoke_proposal(char *, int, struct imsg_revoke *, char **);

void	priv_tell_unwind(int, int, int, struct imsg_tell_unwind *);
