/*	$OpenBSD: pxa2x0_dmac.h,v 1.2 2006/04/04 11:37:05 pascoe Exp $	*/

/*
 * Copyright (c) 2005 Christopher Pascoe <pascoe@openbsd.org>
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

#ifndef _PXA2X0_DMAC_H
#define _PXA2X0_DMAC_H

int pxa2x0_dma_to_fifo(int periph, int chan, bus_addr_t fifo_addr, int width,
    int burstsize, bus_addr_t src_addr, int length, void (*intr)(void *),
    void *intrarg);

int pxa2x0_dma_from_fifo(int periph, int chan, bus_addr_t fifo_addr, int width,
    int burstsize, bus_addr_t trg_addr, int length, void (*intr)(void *),
    void *intrarg);

#endif /* _PXA2X0_DMAC_H */
