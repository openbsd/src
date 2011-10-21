/* $OpenBSD: ahb.h,v 1.2 2011/10/21 22:55:01 drahn Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@drahn.com>
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

struct ahb_attach_args {
	int	aa_addr;
	int	aa_size;
	int	aa_intr;
	bus_space_tag_t	aa_iot;
	bus_dma_tag_t aa_dmat;
	char	*aa_name;
};

/* XXX */
void *avic_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name);

/* board identification - from uboot */
#define BOARD_ID_OMAP3_BEAGLE 1546
#define BOARD_ID_OMAP4_PANDA 2791
extern uint32_t board_id;
