/*
 * Copyright (c) 2010 Owain G. Ainsworth <oga@openbsd.org>
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
#include <sys/param.h>
#include <sys/systm.h>

#include <net/netisr.h>

#include <machine/intr.h>

#include "ether.h"
#include "ppp.h"
#include "bridge.h"
#include "pppoe.h"
#include "pfsync.h"

void	 netintr(void *);

int	 netisr;
void	*netisr_intr;

void
netintr(void *unused) /* ARGSUSED */
{
	int n, t = 0;

	while ((n = netisr) != 0) {
		atomic_clearbits_int(&netisr, n);

#if NETHER > 0
		if (n & (1 << NETISR_ARP))
			arpintr();
#endif
		if (n & (1 << NETISR_IP))
			ipintr();
#ifdef INET6
		if (n & (1 << NETISR_IPV6))
			ip6intr();
#endif
#if NPPP > 0
		if (n & (1 << NETISR_PPP))
			pppintr();
#endif
#if NBRIDGE > 0
		if (n & (1 << NETISR_BRIDGE))
			bridgeintr();
#endif
#if NPPPOE > 0
		if (n & (1 << NETISR_PPPOE))
			pppoeintr();
#endif
		t |= n;
	}

#if NPFSYNC > 0
	if (t & (1 << NETISR_PFSYNC))
		pfsyncintr();
#endif
	if (t & (1 << NETISR_TX))
		nettxintr();
}

void
netisr_init(void)
{
	netisr_intr = softintr_establish(IPL_SOFTNET, netintr, NULL);
	if (netisr_intr == NULL)
		panic("can't establish softnet handler");
}
