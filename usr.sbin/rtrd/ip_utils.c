/*	$OpenBSD: ip_utils.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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
#include <sys/time.h>
#include <sys/tree.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"

#define IP4_BITS	(sizeof(struct in_addr) * 8)
#define IP4_BYTES	(sizeof(struct in_addr))
#define IP6_BITS	(sizeof(struct in6_addr) * 8)
#define IP6_BYTES	(sizeof(struct in6_addr))

struct in_addr cidrmask4[IP4_BITS + 1];
struct in6_addr cidrmask6[IP6_BITS + 1];

void
init_masks(void)
{
	uint8_t i;

	for (i = 0; i <= IP4_BITS; i++)
		cidrmask4[i] = cidr_to_netmask4(i);

	for (i = 0; i <= IP6_BITS; i++)
		cidrmask6[i] = cidr_to_netmask6(i);
}

struct in_addr
cidr_to_netmask4(uint8_t cidr)
{
	struct in_addr r;

	if (cidr > IP4_BITS)
		cidr = IP4_BITS;

	if (cidr > 0) {
		r.s_addr = -1;
		r.s_addr <<= (IP4_BITS - cidr);
		r.s_addr = htobe32(r.s_addr);
	} else {
		r.s_addr = 0;
	}
	return r;
}

struct in6_addr
cidr_to_netmask6(uint8_t cidr)
{
	struct in6_addr r;
	uint8_t boundry_byte, boundry_bits, i;

	if (cidr > IP6_BITS)
		cidr = IP6_BITS;
	boundry_byte = cidr / 8;
	boundry_bits = (IP6_BITS - cidr) % 8;

	for (i = 0; i < IP6_BYTES; i++) {
		if (i < boundry_byte)
			r.s6_addr[i] = -1;
		if (i == boundry_byte) {
			if (boundry_bits > 0) {
				r.s6_addr[i] = -1;
				r.s6_addr[i] <<= boundry_bits;
			} else {
				r.s6_addr[i] = 0;
			}
		}
		if (i > boundry_byte)
			r.s6_addr[i] = 0;
	}
	return r;
}

int
is_supernet_of_subnet4(struct in_addr super_net, uint8_t super_cidr,
    struct in_addr sub_net, uint8_t sub_cidr)
{
	struct in_addr super, sub, supermask;

	if (super_cidr > IP4_BITS)
		super_cidr = IP4_BITS;
	if (sub_cidr > IP4_BITS)
		sub_cidr = IP4_BITS;
	if (sub_cidr < super_cidr)
		return 0;

	if ((super_net.s_addr == sub_net.s_addr) &&
	  (super_cidr == sub_cidr))
		return 0;

	supermask = cidrmask4[super_cidr];

	super.s_addr = super_net.s_addr & supermask.s_addr;
	sub.s_addr = sub_net.s_addr & supermask.s_addr;
	return (super.s_addr == sub.s_addr);
}

int
is_supernet_of_subnet6(struct in6_addr super_net, uint8_t super_cidr,
    struct in6_addr sub_net, uint8_t sub_cidr)
{
	struct in6_addr super, sub, supermask;
	uint8_t i;

	if (super_cidr > IP6_BITS)
		super_cidr = IP6_BITS;
	if (sub_cidr > IP6_BITS)
		sub_cidr = IP6_BITS;
	if (sub_cidr < super_cidr)
		return 0;

	if ((memcmp(&super_net, &sub_net, sizeof(struct in6_addr)) == 0) &&
	    (super_cidr == sub_cidr))
		return 0;

	supermask = cidrmask6[super_cidr];

	for (i = 0; i < IP6_BYTES; i++) {
		super.s6_addr[i] = super_net.s6_addr[i] & supermask.s6_addr[i];
		sub.s6_addr[i] = sub_net.s6_addr[i] & supermask.s6_addr[i];
		if (super.s6_addr[i] != sub.s6_addr[i])
			return 0;
	}
	return 1;
}

struct in_addr
address_to_network4(struct in_addr address, uint8_t cidr)
{
	struct in_addr network, netmask;

	if (cidr > IP4_BITS)
		cidr = IP4_BITS;

	netmask = cidrmask4[cidr];
	network.s_addr = address.s_addr & netmask.s_addr;
	return network;
}

struct in6_addr
address_to_network6(struct in6_addr address, uint8_t cidr)
{
	struct in6_addr network, netmask;
	uint8_t i;

	if (cidr > IP6_BITS)
		cidr = IP6_BITS;

	netmask = cidrmask6[cidr];
	for (i = 0; i < IP6_BYTES; i++)
		network.s6_addr[i] = address.s6_addr[i] & netmask.s6_addr[i];
	return network;
}
