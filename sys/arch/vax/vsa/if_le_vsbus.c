/*	$OpenBSD: if_le_vsbus.c,v 1.1 2000/04/27 02:34:50 bjc Exp $	*/
/*	$NetBSD: if_le_vsbus.c,v 1.2 1999/08/27 20:05:08 ragge Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	  This product includes software developed by the NetBSD
 *	  Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#endif
#include <netinet/if_ether.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/bus.h>
#include <machine/scb.h>
#include <machine/rpb.h>
#include <machine/vsbus.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>
#include <dev/tc/if_levar.h>

int	le_vsbus_match __P((struct device *, struct cfdata *, void *));
void	le_vsbus_attach __P((struct device *, struct device *, void *));
static	void	lewrcsr __P((struct am7990_softc *, u_int16_t, u_int16_t));
static	u_int16_t lerdcsr __P((struct am7990_softc *, u_int16_t));

struct cfattach le_vsbus_ca = {
	sizeof(struct le_softc), (cfmatch_t)le_vsbus_match, le_vsbus_attach
};

void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	return ler1->ler1_rdp;
}

int
le_vsbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct  vsbus_attach_args *va = aux;
	volatile short *rdp, *rap;

	if (vax_boardtype == VAX_BTYP_49)
		return 0;
	rdp = (short *)va->va_addr;
	rap = rdp + 2;

	/* Make sure the chip is stopped. */
	*rap = LE_CSR0;
	*rdp = LE_C0_STOP;
	DELAY(100);
	*rdp = LE_C0_INIT|LE_C0_INEA;

	/* Wait for initialization to finish. */
	DELAY(100000);
	va->va_ivec = vsbus_intr;		/* we do our own scb stuff */

	/* Should have interrupted by now */
	if (*rdp & LE_C0_IDON)
		return 1;
	return 0;
}

void
le_vsbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	struct le_softc *sc = (void *)self;
	struct vsbus_softc *vsc = (struct vsbus_softc *)parent;
	bus_dma_segment_t seg;
	int *lance_addr;
	int i, err, rseg;
	struct lereg1 *ler1; 

	ler1 = sc->sc_r1 = (void *)vax_map_physmem(NI_BASE, 1);

	/* Prettier printout */
	printf("\n%s", self->dv_xname);

	/*
	 * MD functions.
	 */
	sc->sc_am7990.sc_rdcsr = lerdcsr;
	sc->sc_am7990.sc_wrcsr = lewrcsr;
	sc->sc_am7990.sc_nocarrier = NULL;

	scb_vecalloc(va->va_cvec, (void (*)(void *)) am7990_intr, sc, SCB_ISTACK);
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
	sc->sc_am7990.sc_addr =
	    (paddr_t)sc->sc_am7990.sc_mem & 0xffffff;
	sc->sc_am7990.sc_memsize = ALLOCSIZ;

	sc->sc_am7990.sc_conf3 = 0;
	sc->sc_am7990.sc_hwinit = NULL;
	sc->sc_am7990.sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_am7990.sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_am7990.sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_am7990.sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_am7990.sc_zerobuf = am7990_zerobuf_contig;

	/*
	 * Get the ethernet address out of rom
	 */
	lance_addr = (int *)vax_map_physmem(NI_ADDR, 1);
	for (i = 0; i < 6; i++)
		sc->sc_am7990.sc_arpcom.ac_enaddr[i] = (u_char)lance_addr[i];
	vax_unmap_physmem((vaddr_t)lance_addr, 1);

	bcopy(self->dv_xname, sc->sc_am7990.sc_arpcom.ac_if.if_xname,
	    IFNAMSIZ);
	am7990_config(&sc->sc_am7990);

	/*
	 * Register this device as boot device if we booted from it.
	 * This will fail if there are more than one le in a machine,
	 * fortunately there may be only one.
	 */
	if (B_TYPE(bootdev) == BDEV_LE)
		booted_from = self;
	
	vsc->sc_mask |= 1 << (va->va_maskno-1);
}
