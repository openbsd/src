/* $OpenBSD: omapvar.h,v 1.2 2013/10/10 19:40:02 syl Exp $ */
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

/* Physical memory range for on-chip devices. */
struct omap_mem {
	u_int32_t addr;			/* physical start address */
	u_int32_t size;			/* size of range in bytes */
};

#define OMAP_DEV_NMEM 4		       /* number of memory ranges */
#define OMAP_DEV_NIRQ 4		       /* number of IRQs per device */
#define OMAP_DEV_NDMA 4		       /* number of DMA channels per device */

/* Descriptor for all on-chip devices. */
struct omap_dev {
	char *name;			/* driver name or made up name */
	int unit;			/* driver instance number or -1 */
	struct omap_mem mem[OMAP_DEV_NMEM]; /* memory ranges */
	int irq[OMAP_DEV_NIRQ];		    /* IRQ number(s) */
	int dma[OMAP_DEV_NDMA];		/* DMA chan number(s) */
};

/* Passed as third arg to attach functions. */
struct omap_attach_args {
	struct omap_dev *oa_dev;
	bus_space_tag_t	oa_iot;
	bus_dma_tag_t oa_dmat;
};

void omap_set_devs(struct omap_dev *);
struct omap_dev *omap_find_dev(const char *, int);

void omap3_init(void);
void omap4_init(void);
void am335x_init(void);

/* XXX */
void *avic_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name);

/* board identification - from uboot */
#define BOARD_ID_AM335X_BEAGLEBONE 3589
#define BOARD_ID_OMAP3_BEAGLE 1546
#define BOARD_ID_OMAP3_OVERO 1798
#define BOARD_ID_OMAP4_PANDA 2791
extern uint32_t board_id;
