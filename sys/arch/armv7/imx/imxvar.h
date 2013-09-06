/* $OpenBSD: imxvar.h,v 1.1 2013/09/06 20:45:54 patrick Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@drahn.com>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

/* Physical memory range for on-chip devices. */
struct imx_mem {
	u_int32_t addr;			/* physical start address */
	u_int32_t size;			/* size of range in bytes */
};

#define IMX_DEV_NMEM 6		       /* number of memory ranges */
#define IMX_DEV_NIRQ 4		       /* number of IRQs per device */

/* Descriptor for all on-chip devices. */
struct imx_dev {
	char *name;			/* driver name or made up name */
	int unit;			/* driver instance number or -1 */
	struct imx_mem mem[IMX_DEV_NMEM]; /* memory ranges */
	int irq[IMX_DEV_NIRQ];		    /* IRQ number(s) */
};

/* Passed as third arg to attach functions. */
struct imx_attach_args {
	struct imx_dev *ia_dev;
	bus_space_tag_t	ia_iot;
	bus_dma_tag_t ia_dmat;
};

void imx_set_devs(struct imx_dev *);
struct imx_dev *imx_find_dev(const char *, int);

void imx6_init(void);

/* XXX */
void *avic_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name);

/* board identification - from uboot */
#define BOARD_ID_IMX6_PHYFLEX 3529
#define BOARD_ID_IMX6_SABRELITE 3769
extern uint32_t board_id;
