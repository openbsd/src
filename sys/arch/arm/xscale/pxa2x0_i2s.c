/*	$OpenBSD: pxa2x0_i2s.c,v 1.7 2006/04/04 11:45:40 pascoe Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>
#include <arm/xscale/pxa2x0_i2s.h>
#include <arm/xscale/pxa2x0_dmac.h>

struct pxa2x0_i2s_dma {
	struct pxa2x0_i2s_dma *next;
	caddr_t addr;
	size_t size;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
};

void
pxa2x0_i2s_init(struct pxa2x0_i2s_softc *sc)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SACR0, SACR0_RST);
	delay(100);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SACR0,
	    SACR0_BCKD | SACR0_SET_TFTH(7) | SACR0_SET_RFTH(7));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SACR1, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SADR, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SADIV, sc->sc_sadiv);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SACR0,
		SACR0_BCKD | SACR0_SET_TFTH(7) | SACR0_SET_RFTH(7) | SACR0_ENB);
}

int
pxa2x0_i2s_attach_sub(struct pxa2x0_i2s_softc *sc)
{
	if (bus_space_map(sc->sc_iot, PXA2X0_I2S_BASE, PXA2X0_I2S_SIZE, 0,
	    &sc->sc_ioh)) {
		sc->sc_size = 0;
		return 1;
	}
	sc->sc_sadiv = SADIV_3_058MHz;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, sc->sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	pxa2x0_gpio_set_function(28, GPIO_ALT_FN_1_OUT);  /* I2S_BITCLK */
	pxa2x0_gpio_set_function(113, GPIO_ALT_FN_1_OUT); /* I2S_SYSCLK */
	pxa2x0_gpio_set_function(31, GPIO_ALT_FN_1_OUT);  /* I2S_SYNC */
	pxa2x0_gpio_set_function(30, GPIO_ALT_FN_1_OUT);  /* I2S_SDATA_OUT */
	pxa2x0_gpio_set_function(29, GPIO_ALT_FN_2_IN);   /* I2S_SDATA_IN */

	pxa2x0_i2s_init(sc);

	return 0;
}

void pxa2x0_i2s_open(struct pxa2x0_i2s_softc *sc)
{
	sc->sc_open++;
	pxa2x0_clkman_config(CKEN_I2S, 1);
}

void pxa2x0_i2s_close(struct pxa2x0_i2s_softc *sc)
{
	pxa2x0_clkman_config(CKEN_I2S, 0);
	sc->sc_open--;
}

int
pxa2x0_i2s_detach_sub(struct pxa2x0_i2s_softc *sc)
{
	if (sc->sc_size > 0) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}
	pxa2x0_clkman_config(CKEN_I2S, 0);

	return (0);
}

void pxa2x0_i2s_write(struct pxa2x0_i2s_softc *sc, u_int32_t data)
{
	if (! sc->sc_open)
		return;

	/* Clear intr and underrun bit if set. */
	if (bus_space_read_4(sc->sc_iot, sc->sc_ioh, I2S_SASR0) & SASR0_TUR)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SAICR, SAICR_TUR);

	/* Wait for transmit fifo to have space. */
	while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, I2S_SASR0) & SASR0_TNF)
	     == 0)
		;	/* nothing */

	/* Queue data */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SADR, data);
}

void
pxa2x0_i2s_setspeed(struct pxa2x0_i2s_softc *sc, u_long *argp)
{
	/*
	 * The available speeds are in the following table.
	 * Keep the speeds in increasing order.
	 */
	typedef struct {
		int speed;
		int div;
	} speed_struct;
	u_long arg = *argp;

	static speed_struct speed_table[] = {
		{8000,	SADIV_513_25kHz},
		{11025,	SADIV_702_75kHz},
		{16000,	SADIV_1_026MHz},
		{22050,	SADIV_1_405MHz},
		{44100,	SADIV_2_836MHz},
		{48000,	SADIV_3_058MHz},
	};

	int i, n, selected = -1;

	n = sizeof(speed_table) / sizeof(speed_struct);

	if (arg < speed_table[0].speed)
		selected = 0;
	if (arg > speed_table[n - 1].speed)
		selected = n - 1;

	for (i = 1; selected == -1 && i < n; i++) {
		if (speed_table[i].speed == arg)
			selected = i;
		else if (speed_table[i].speed > arg) {
			int diff1, diff2;

			diff1 = arg - speed_table[i - 1].speed;
			diff2 = speed_table[i].speed - arg;
			if (diff1 < diff2)
				selected = i - 1;
			else
				selected = i;
		}
	}

	if (selected == -1)
		selected = 0;

	*argp = speed_table[selected].speed;

	sc->sc_sadiv = speed_table[selected].div;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2S_SADIV, sc->sc_sadiv);
}

void *
pxa2x0_i2s_allocm(void *hdl, int direction, size_t size, int type, int flags)
{
	struct device *sc_dev = hdl;
	struct pxa2x0_i2s_softc *sc =
	    (struct pxa2x0_i2s_softc *)((struct device *)hdl + 1);
	struct pxa2x0_i2s_dma *p;
	int error;
	int rseg;

	p = malloc(sizeof(*p), type, flags);
	if (!p)
		return 0;

	p->size = size;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, NBPG, 0, &p->seg, 1,
	    &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate dma, error = %d\n",
		    sc_dev->dv_xname, error);
		goto fail_alloc;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &p->seg, rseg, size, &p->addr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map dma, error = %d\n",
		    sc_dev->dv_xname, error);
		goto fail_map;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &p->map)) != 0) {
		printf("%s: unable to create dma map, error = %d\n",
		    sc_dev->dv_xname, error);
		goto fail_create;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, size, NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load dma map, error = %d\n",
		    sc_dev->dv_xname, error);
		goto fail_load;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return p->addr;

fail_load:
	bus_dmamap_destroy(sc->sc_dmat, p->map);
fail_create:
	bus_dmamem_unmap(sc->sc_dmat, p->addr, size);
fail_map:
	bus_dmamem_free(sc->sc_dmat, &p->seg, 1);
fail_alloc:
	free(p, type);
	return 0;
}

void
pxa2x0_i2s_freem(void *hdl, void *ptr, int type)
{
	struct pxa2x0_i2s_softc *sc =
	    (struct pxa2x0_i2s_softc *)((struct device *)hdl + 1);
	struct pxa2x0_i2s_dma **pp, *p;

	for (pp = &(sc->sc_dmas); (p = *pp) != NULL; pp = &p->next)
		if (p->addr == ptr) {
			bus_dmamap_unload(sc->sc_dmat, p->map);
			bus_dmamap_destroy(sc->sc_dmat, p->map);
			bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
			bus_dmamem_free(sc->sc_dmat, &p->seg, 1);

			*pp = p->next;
			free(p, type);
			return;
		}

	panic("pxa2x0_i2s_freem: trying to free unallocated memory");
}

paddr_t
pxa2x0_i2s_mappage(void *hdl, void *mem, off_t off, int prot)
{
	struct pxa2x0_i2s_softc *sc =
	    (struct pxa2x0_i2s_softc *)((struct device *)hdl + 1);
	struct pxa2x0_i2s_dma *p;

	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next)
		;
	if (!p)
		return -1;

	if (off > p->size)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmat, &p->seg, 1, off, prot,
	    BUS_DMA_WAITOK);
}

int
pxa2x0_i2s_round_blocksize(void *hdl, int bs)
{
	/* Enforce individual DMA block size limit */
	if (bs > DCMD_LENGTH_MASK)
		return (DCMD_LENGTH_MASK & ~0x03);

	return (bs + 0x03) & ~0x03;	/* 32-bit multiples */
}

size_t
pxa2x0_i2s_round_buffersize(void *hdl, int direction, size_t bufsize)
{
	return bufsize;
}

int
pxa2x0_i2s_start_output(struct pxa2x0_i2s_softc *sc, void *block, int bsize,
    void (*intr)(void *), void *intrarg)
{
	struct pxa2x0_i2s_dma *p;
	int offset;

	/* Find mapping which contains block completely */
	for (p = sc->sc_dmas; p && (((caddr_t)block < p->addr) ||
	    ((caddr_t)block + bsize > p->addr + p->size)); p = p->next)
		;	/* Nothing */

	if (!p) {
		printf("pxa2x0_i2s_start_output: request with bad start "
		    "address: %p, size: %d)\n", block, bsize);
		return ENXIO;
	}

	/* Offset into block to use in mapped block */
	offset = (caddr_t)block - p->addr;

	/* Start DMA */
	pxa2x0_dma_to_fifo(3, 1, 0x40400080, 4, 32,
	    p->map->dm_segs[0].ds_addr + offset, bsize, intr, intrarg);

	return 0;
}

int
pxa2x0_i2s_start_input(struct pxa2x0_i2s_softc *sc, void *block, int bsize,
    void (*intr)(void *), void *intrarg)
{
	struct pxa2x0_i2s_dma *p;
	int offset;

	/* Find mapping which contains block completely */
	for (p = sc->sc_dmas; p && (((caddr_t)block < p->addr) ||
	    ((caddr_t)block + bsize > p->addr + p->size)); p = p->next)
		;	/* Nothing */

	if (!p) {
		printf("pxa2x0_i2s_start_input: request with bad start "
		    "address: %p, size: %d)\n", block, bsize);
		return ENXIO;
	}

	/* Offset into block to use in mapped block */
	offset = (caddr_t)block - p->addr;

	/* Start DMA */
	pxa2x0_dma_from_fifo(2, 2, 0x40400080, 4, 32,
	    p->map->dm_segs[0].ds_addr + offset, bsize, intr, intrarg);

	return 0;
}
