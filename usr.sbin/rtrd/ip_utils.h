/*	$OpenBSD: ip_utils.h,v 1.1 2026/09/16 16:11:46 job Exp $ */
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

extern struct in_addr cidrmask4[];
extern struct in6_addr cidrmask6[];

extern void init_masks(void);
extern struct in_addr cidr_to_netmask4(uint8_t);
extern struct in6_addr cidr_to_netmask6(uint8_t);
extern int is_supernet_of_subnet4(
    struct in_addr, uint8_t,
    struct in_addr, uint8_t);
extern int is_supernet_of_subnet6(
    struct in6_addr, uint8_t,
    struct in6_addr, uint8_t);

extern struct in_addr address_to_network4(struct in_addr, uint8_t);
extern struct in6_addr address_to_network6(struct in6_addr, uint8_t);
