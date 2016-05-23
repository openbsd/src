/*	$OpenBSD: util.c,v 1.3 2016/05/23 18:55:21 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
 * Copyright (c) 2012 Alexander Bluhm <bluhm@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
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
#include <string.h>

#include "ldpd.h"
#include "log.h"

uint8_t
mask2prefixlen(in_addr_t ina)
{
	if (ina == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina)));
}

in_addr_t
prefixlen2mask(uint8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (htonl(0xffffffff << (32 - prefixlen)));
}

int
bad_ip_addr(struct in_addr addr)
{
	uint32_t	 a = ntohl(addr.s_addr);

	if (((a >> IN_CLASSA_NSHIFT) == 0) ||
	    ((a >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) ||
	    IN_MULTICAST(a) || IN_BADCLASS(a))
		return (1);

	return (0);
}
