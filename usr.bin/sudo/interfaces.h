/*
 * Copyright (c) 1996, 1998-2004 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 *
 * $Sudo: interfaces.h,v 1.8.2.3 2007/10/24 16:43:27 millert Exp $
 */

#ifndef _SUDO_INTERFACES_H
#define _SUDO_INTERFACES_H

/*
 * IP address and netmask pairs for checking against local interfaces.
 */
struct interface {
    int family;	/* AF_INET or AF_INET6 */
    union {
	struct in_addr ip4;
#ifdef HAVE_IN6_ADDR
	struct in6_addr ip6;
#endif
    } addr;
    union {
	struct in_addr ip4;
#ifdef HAVE_IN6_ADDR
	struct in6_addr ip6;
#endif
    } netmask;
};

/*
 * Prototypes for external functions.
 */
void load_interfaces	__P((void));
void dump_interfaces	__P((void));

/*
 * Definitions for external variables.
 */
#ifndef _SUDO_MAIN
extern struct interface *interfaces;
extern int num_interfaces;
#endif

#endif /* _SUDO_INTERFACES_H */
