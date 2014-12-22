/*	$OpenBSD: if_le_fwio.c,v 1.4 2014/12/22 02:26:54 tedu Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
#include <sys/socket.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>
#include <vax/mbus/fwioreg.h>
#include <vax/mbus/fwiovar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

struct le_fwio_softc {
	struct am7990_softc sc_am7990;
	volatile uint16_t *sc_rap;
	volatile uint16_t *sc_rdp;
};

int	le_fwio_match(struct device *, void *, void *);
void	le_fwio_attach(struct device *, struct device *, void *);

struct cfattach le_fwio_ca = {
	sizeof(struct le_fwio_softc), le_fwio_match, le_fwio_attach
};

int	le_fwio_intr(void *);
uint16_t le_fwio_rdcsr(struct lance_softc *, uint16_t);
void	le_fwio_wrcsr(struct lance_softc *, uint16_t, uint16_t);
void	le_fwio_wrcsr_interrupt(struct lance_softc *, uint16_t, uint16_t);

int
le_fwio_match(struct device *parent, void *vcf, void *aux)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;

	return strcmp(faa->faa_dev, le_cd.cd_name) == 0 ? 1 : 0;
}

void
le_fwio_attach(struct device *parent, struct device *self, void *aux)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;
	struct le_fwio_softc *lsc = (struct le_fwio_softc *)self;
	struct lance_softc *sc = &lsc->sc_am7990.lsc;
	unsigned int vec;
	uint32_t *esar;
	int i;

	vec = faa->faa_vecbase + FBIC_DEVIRQ1 * 4;
	printf(" vec %d", vec);

	/*
	 * Map registers.
	 */

	lsc->sc_rdp = (volatile uint16_t *)
	    vax_map_physmem(faa->faa_base + FWIO_LANCE_REG_OFFSET, 1);
	lsc->sc_rap = lsc->sc_rdp + 2;

	/*
	 * Register access functions.
	 */

	sc->sc_rdcsr = le_fwio_rdcsr;
	sc->sc_wrcsr = le_fwio_wrcsr;

	/*
	 * Map buffers.
	 */

	sc->sc_mem = (void *)uvm_km_valloc(kernel_map, FWIO_LANCE_BUF_SIZE);
	if (sc->sc_mem == NULL) {
		vax_unmap_physmem(faa->faa_base + FWIO_LANCE_REG_OFFSET, 1);
		printf(": can't map buffers\n");
		return;
	}

	ioaccess((vaddr_t)sc->sc_mem, faa->faa_base +
	    FWIO_LANCE_BUF_OFFSET, FWIO_LANCE_BUF_SIZE >> VAX_PGSHIFT);

	sc->sc_addr = FWIO_LANCE_BUF_OFFSET;
	sc->sc_memsize = FWIO_LANCE_BUF_SIZE;
	sc->sc_conf3 = 0;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	/*
	 * Get the Ethernet address from the Station Address ROM.
	 */

	esar = (uint32_t *)vax_map_physmem(faa->faa_base + FWIO_ESAR_OFFSET, 1);
	for (i = 0; i < 6; i++)
		sc->sc_arpcom.ac_enaddr[i] =
		    (esar[i] & FWIO_ESAR_MASK) >> FWIO_ESAR_SHIFT;
	vax_unmap_physmem((vaddr_t)esar, 1);

	/*
	 * Register interrupt handler.
	 */

	if (mbus_intr_establish(vec, IPL_NET, le_fwio_intr, sc,
	    self->dv_xname) != 0) {
		vax_unmap_physmem(faa->faa_base + FWIO_LANCE_REG_OFFSET, 1);
		uvm_km_free(kernel_map, (vaddr_t)sc->sc_mem,
		    FWIO_LANCE_BUF_SIZE);
		printf(": can't establish interrupt\n");
		return;
	}

	/*
	 * Complete attachment.
	 */

	am7990_config(&lsc->sc_am7990);
}

int
le_fwio_intr(void *v)
{
	struct le_fwio_softc *lsc = (struct le_fwio_softc *)v;
	struct lance_softc *sc = &lsc->sc_am7990.lsc;
	int rc;

	/*
	 * FBIC expects edge interrupts, while the LANCE does level
	 * interrupts. To avoid missing interrupts while servicing,
	 * we disable further device interrupts while servicing.
	 *
	 * However, am7990_intr() will flip the interrupt enable bit
	 * itself; we override wrcsr with a specific version during
	 * servicing, so as not to reenable interrupts accidentally...
	 */
	sc->sc_wrcsr = le_fwio_wrcsr_interrupt;

	rc = am7990_intr(v);

	sc->sc_wrcsr = le_fwio_wrcsr;
	/*
	 * ...but we should not forget to reenable interrupts at this point!
	 */
	le_fwio_wrcsr(sc, LE_CSR0, LE_C0_INEA | le_fwio_rdcsr(sc, LE_CSR0));

	return rc;
}

uint16_t
le_fwio_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_fwio_softc *lsc = (struct le_fwio_softc *)sc;

	*lsc->sc_rap = port;
	return *lsc->sc_rdp;
}

void
le_fwio_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_fwio_softc *lsc = (struct le_fwio_softc *)sc;

	*lsc->sc_rap = port;
	*lsc->sc_rdp = val;
}

void
le_fwio_wrcsr_interrupt(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	if (port == LE_CSR0)
		val &= ~LE_C0_INEA;

	le_fwio_wrcsr(sc, port, val);
}
