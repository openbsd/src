/*	$OpenBSD: allwinnervar.h,v 1.1 2013/10/22 13:22:19 jasper Exp $	*/
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


#define AWREAD1(sc, reg)						\
	(bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define AWWRITE1(sc, reg, val)						\
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define AWSET1(sc, reg, bits)						\
	AWWRITE1((sc), (reg), AWREAD1((sc), (reg)) | (bits))
#define AWCLR1(sc, reg, bits)						\
	AWWRITE1((sc), (reg), AWREAD1((sc), (reg)) & ~(bits))
#define	AWCMS1(sc, reg, mask, bits)					\
	AWWRITE1((sc), (reg), (AWREAD1((sc), (reg)) & ~(mask)) | (bits))

#define AWREAD4(sc, reg)						\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define AWWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define AWSET4(sc, reg, bits)						\
	AWWRITE4((sc), (reg), AWREAD4((sc), (reg)) | (bits))
#define AWCLR4(sc, reg, bits)						\
	AWWRITE4((sc), (reg), AWREAD4((sc), (reg)) & ~(bits))
#define	AWCMS4(sc, reg, mask, bits)					\
	AWWRITE4((sc), (reg), (AWREAD4((sc), (reg)) & ~(mask)) | (bits))


/* Physical memory range for on-chip devices. */
struct aw_mem {
	bus_addr_t addr;		/* physical start address */
	bus_size_t size;		/* size of range in bytes */
};

#define AW_DEV_NMEM 4			/* max number of memory ranges */
#define AW_DEV_NIRQ 4			/* max number of IRQs per device */

/* Descriptor for all on-chip devices. */
struct aw_dev {
	char *name;			/* driver name or made up name */
	int unit;			/* driver instance number or -1 */
	struct aw_mem mem[AW_DEV_NMEM]; /* memory ranges */
	int irq[AW_DEV_NIRQ];		/* IRQ number(s) */
};

/* Passed as third arg to attach functions. */
struct aw_attach_args {
	struct aw_dev *aw_dev;
	bus_space_tag_t	aw_iot;
	bus_dma_tag_t aw_dmat;
};

void aw_set_devs(struct aw_dev *);
struct aw_dev *aw_find_dev(const char *, int);

/* board identification - from uboot */
#define BOARD_ID_A10_CUBIE 4104
#define BOARD_ID_A20_CUBIE 4283
extern uint32_t board_id;
