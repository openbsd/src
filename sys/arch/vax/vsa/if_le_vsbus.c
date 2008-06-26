/*	$OpenBSD: if_le_vsbus.c,v 1.11 2008/06/26 05:42:14 ray Exp $	*/
/*	$NetBSD: if_le_vsbus.c,v 1.10 2000/06/29 07:14:18 mrg Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#endif
#include <netinet/if_ether.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/bus.h>
#include <machine/vsbus.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

struct le_softc {
	struct	am7990_softc sc_am7990; /* Must be first */
	struct	evcount sc_intrcnt;
	bus_dmamap_t sc_dm;
	volatile u_short *sc_rap;
	volatile u_short *sc_rdp;
};

static	int	le_vsbus_match(struct device *, struct cfdata *, void *);
static	void	le_vsbus_attach(struct device *, struct device *, void *);
static	void	lewrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
static	u_int16_t lerdcsr(struct am7990_softc *, u_int16_t);

struct cfattach le_vsbus_ca = {
	sizeof(struct le_softc), (cfmatch_t)le_vsbus_match, le_vsbus_attach
};

static void
lewrcsr(ls, port, val)
	struct am7990_softc *ls;
	u_int16_t port, val;
{
	struct le_softc *sc = (void *)ls;

	*sc->sc_rap = port;
	*sc->sc_rdp = val;
}

static u_int16_t
lerdcsr(ls, port)
	struct am7990_softc *ls;
	u_int16_t port;
{
	struct le_softc *sc = (void *)ls;

	*sc->sc_rap = port;
	return *sc->sc_rdp;
}

static int
le_vsbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	volatile short *rdp, *rap;
	struct leinit initblock;
	bus_dmamap_t map;
	int i;
	int rv = 0;
	int error;

	if (vax_boardtype == VAX_BTYP_49 || vax_boardtype == VAX_BTYP_1303)
		return 0;

	error = bus_dmamap_create(va->va_dmat, sizeof(initblock), 1,
	    sizeof(initblock), 0, BUS_DMA_NOWAIT, &map);
	if (error) {
		return 0;
	}

	error = bus_dmamap_load(va->va_dmat, map, &initblock, 
	    sizeof(initblock), NULL, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		bus_dmamap_destroy(va->va_dmat, map);
		return 0;
	}

	memset(&initblock, 0, sizeof(initblock));

	rdp = (short *)va->va_addr;
	rap = rdp + 2;

	/* Make sure the chip is stopped. */
	*rap = LE_CSR0;
	*rdp = LE_C0_STOP;
	DELAY(100);
	*rap = LE_CSR1;
	*rdp = map->dm_segs->ds_addr & 0xffff;
	*rap = LE_CSR2;
	*rdp = (map->dm_segs->ds_addr >> 16) & 0xffff;
	*rap = LE_CSR0;
	*rdp = LE_C0_INIT|LE_C0_INEA;

	/* Wait for initialization to finish. */
	for (i = 100; i >= 0; i--) {
		DELAY(1000);
		/* Should have interrupted by now */
		if (*rdp & LE_C0_IDON)
			rv = 1;
	} 
	*rap = LE_CSR0;
	*rdp = LE_C0_STOP;

	bus_dmamap_unload(va->va_dmat, map);
	bus_dmamap_destroy(va->va_dmat, map);
	return rv;
}

static void
le_vsbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	struct le_softc *sc = (void *)self;
	bus_dma_segment_t seg;
	int *lance_addr;
	int i, err, rseg;
	static int cvec;

	sc->sc_rdp = (short *)vax_map_physmem(NI_BASE, 1);
	sc->sc_rap = sc->sc_rdp + 2;

	/*
	 * MD functions.
	 */
	sc->sc_am7990.sc_rdcsr = lerdcsr;
	sc->sc_am7990.sc_wrcsr = lewrcsr;
	sc->sc_am7990.sc_nocarrier = NULL;

	scb_vecalloc(va->va_cvec, (void (*)(void *)) am7990_intr, sc,
	    SCB_ISTACK, &sc->sc_intrcnt);
	cvec = va->va_cvec;
	evcount_attach(&sc->sc_intrcnt, self->dv_xname,
	    (void *)&cvec, &evcount_intr);

        /*
         * Allocate a (DMA-safe) block for all descriptors and buffers.
         */

#define ALLOCSIZ (64 * 1024)
        err = bus_dmamem_alloc(va->va_dmat, ALLOCSIZ, NBPG, 0, 
            &seg, 1, &rseg, BUS_DMA_NOWAIT);
        if (err) {
                printf(": unable to alloc buffer block: err %d\n", err);
                return;
        }
        err = bus_dmamem_map(va->va_dmat, &seg, rseg, ALLOCSIZ, 
            (caddr_t *)&sc->sc_am7990.sc_mem,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
        if (err) {
                printf(": unable to map buffer block: err %d\n", err);
                bus_dmamem_free(va->va_dmat, &seg, rseg);
                return;
        }
	err = bus_dmamap_create(va->va_dmat, ALLOCSIZ, rseg, ALLOCSIZ, 
	    0, BUS_DMA_NOWAIT, &sc->sc_dm);
        if (err) {
                printf(": unable to create dma map: err %d\n", err);
                bus_dmamem_free(va->va_dmat, &seg, rseg);
                return;
        }
	err = bus_dmamap_load(va->va_dmat, sc->sc_dm, sc->sc_am7990.sc_mem,
	    ALLOCSIZ, NULL, BUS_DMA_NOWAIT);
        if (err) {
                printf(": unable to load dma map: err %d\n", err);
                bus_dmamap_destroy(va->va_dmat, sc->sc_dm);
                bus_dmamem_free(va->va_dmat, &seg, rseg);
                return;
        }
	printf(" buf 0x%lx-0x%lx", sc->sc_dm->dm_segs->ds_addr,
	    sc->sc_dm->dm_segs->ds_addr + sc->sc_dm->dm_segs->ds_len - 1);
	sc->sc_am7990.sc_addr = sc->sc_dm->dm_segs->ds_addr & 0xffffff;
	sc->sc_am7990.sc_memsize = sc->sc_dm->dm_segs->ds_len;

	sc->sc_am7990.sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_am7990.sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_am7990.sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_am7990.sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_am7990.sc_zerobuf = am7990_zerobuf_contig;

#ifdef LEDEBUG
	sc->sc_am7990.sc_debug = 1;
#endif
	/*
	 * Get the ethernet address out of rom
	 */
	lance_addr = (int *)vax_map_physmem(NI_ADDR, 1);
	for (i = 0; i < 6; i++)
		sc->sc_am7990.sc_arpcom.ac_enaddr[i] = (u_char)lance_addr[i];
	vax_unmap_physmem((vaddr_t)lance_addr, 1);

	bcopy(self->dv_xname, sc->sc_am7990.sc_arpcom.ac_if.if_xname,
	    IFNAMSIZ);
	/* Prettier printout */
	printf("\n%s", self->dv_xname);

	am7990_config(&sc->sc_am7990);
}
