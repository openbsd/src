/*	$OpenBSD: tccvar.h,v 1.1 2012/09/29 21:46:02 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#define	tcc_read(o) \
	*(volatile uint64_t *)PHYS_TO_XKPHYS(TCC_BASE + (o), CCA_NC)
#define	tcc_write(o,v) \
	*(volatile uint64_t *)PHYS_TO_XKPHYS(TCC_BASE + (o), CCA_NC) = (v)

static __inline__ void
tcc_prefetch_disable(void)
{
	tcc_write(TCC_PREFETCH_CTRL, TCC_PREFETCH_INVALIDATE);
}

static __inline__ void
tcc_prefetch_enable(void)
{
	tcc_write(TCC_PREFETCH_CTRL, TCC_PREFETCH_ENABLE);
}

static __inline__ void
tcc_prefetch_invalidate(void)
{
	tcc_write(TCC_PREFETCH_CTRL,
	    tcc_read(TCC_PREFETCH_CTRL) | TCC_PREFETCH_INVALIDATE);
}

void tcc_bus_reset(void);
