/*	$OpenBSD: sunxivar.h,v 1.2 2013/10/23 18:01:52 jasper Exp $	*/
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

#include <machine/bus.h>


#define SXIREAD1(sc, reg)						\
	(bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define SXIWRITE1(sc, reg, val)						\
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define SXISET1(sc, reg, bits)						\
	SXIWRITE1((sc), (reg), SXIREAD1((sc), (reg)) | (bits))
#define SXICLR1(sc, reg, bits)						\
	SXIWRITE1((sc), (reg), SXIREAD1((sc), (reg)) & ~(bits))
#define	SXICMS1(sc, reg, mask, bits)					\
	SXIWRITE1((sc), (reg), (SXIREAD1((sc), (reg)) & ~(mask)) | (bits))

#define SXIREAD4(sc, reg)						\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define SXIWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define SXISET4(sc, reg, bits)						\
	SXIWRITE4((sc), (reg), SXIREAD4((sc), (reg)) | (bits))
#define SXICLR4(sc, reg, bits)						\
	SXIWRITE4((sc), (reg), SXIREAD4((sc), (reg)) & ~(bits))
#define	SXICMS4(sc, reg, mask, bits)					\
	SXIWRITE4((sc), (reg), (SXIREAD4((sc), (reg)) & ~(mask)) | (bits))


/* Physical memory range for on-chip devices. */
struct sxi_mem {
	bus_addr_t addr;		/* physical start address */
	bus_size_t size;		/* size of range in bytes */
};

#define SXI_DEV_NMEM 4			/* max number of memory ranges */
#define SXI_DEV_NIRQ 4			/* max number of IRQs per device */

/* Descriptor for all on-chip devices. */
struct sxi_dev {
	char *name;			/* driver name or made up name */
	int unit;			/* driver instance number or -1 */
	struct sxi_mem mem[SXI_DEV_NMEM]; /* memory ranges */
	int irq[SXI_DEV_NIRQ];		/* IRQ number(s) */
};

/* Passed as third arg to attach functions. */
struct sxi_attach_args {
	struct sxi_dev *sxi_dev;
	bus_space_tag_t	sxi_iot;
	bus_dma_tag_t sxi_dmat;
};

void sxi_set_devs(struct sxi_dev *);
struct sxi_dev *sxi_find_dev(const char *, int);

/* board identification - from uboot */
#define BOARD_ID_SUN4I_A10 4104
#define BOARD_ID_SUN7I_A20 4283
extern uint32_t board_id;
