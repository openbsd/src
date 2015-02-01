/*	$OpenBSD: uba_ibus.c,v 1.4 2015/02/01 15:27:12 miod Exp $	*/
/*	$NetBSD: uba_ibus.c,v 1.1 1999/08/07 10:36:47 ragge Exp $	   */
/*
 * Copyright (c) 1996 Jonathan Stone.
 * Copyright (c) 1994, 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)uba.c	7.10 (Berkeley) 12/16/90
 *	@(#)autoconf.c	7.20 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#define	_VAX_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/cpu.h>
#include <machine/sgmap.h>

#include <arch/vax/uba/ubareg.h>
#include <arch/vax/qbus/ubavar.h>
#include <arch/vax/uba/uba_common.h>

/*
 * The Q22 bus is the main IO bus on MicroVAX II/MicroVAX III systems.
 * It has an address space of 4MB (22 address bits), therefore the name,
 * and is hardware compatible with all 16 and 18 bits Q-bus devices.
 */
static	int	qba_match(struct device *, struct cfdata *, void *);
static	void	qba_attach(struct device *, struct device *, void *);
static	void	qba_beforescan(struct uba_softc*);
static	void	qba_init(struct uba_softc*);

struct	cfattach uba_ibus_ca = {
	sizeof(struct uba_vsoftc), (cfmatch_t)qba_match, qba_attach
};

extern	struct vax_bus_space vax_mem_bus_space;

int
qba_match(parent, vcf, aux)
	struct device *parent;
	struct cfdata *vcf;
	void *aux;
{
	struct	bp_conf *bp = aux;

	if (strcmp(bp->type, "uba"))
		return 0;

	return 1;
}

void
qba_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_vsoftc *sc = (void *)self;

	printf(": Q22\n");
	/*
	 * Fill in bus specific data.
	 */
	sc->uv_sc.uh_beforescan = qba_beforescan;
	sc->uv_sc.uh_ubainit = qba_init;
	sc->uv_sc.uh_iot = &vax_mem_bus_space;
	sc->uv_sc.uh_dmat = &sc->uv_dmat;

	/*
	 * Fill in variables used by the sgmap system.
	 */
	sc->uv_size = QBASIZE;	/* Size in bytes of Qbus space */
	sc->uv_addr = QBAMAP;	/* Physical address of map registers */

	uba_dma_init(sc);
	uba_attach(&sc->uv_sc, QIOPAGE);
}

/*
 * Called when the QBA is set up; to enable DMA access from
 * QBA devices to main memory.
 */
void
qba_beforescan(sc)
	struct uba_softc *sc;
{
	bus_space_write_2(sc->uh_tag, sc->uh_ioh, QIPCR, Q_LMEAE);
}

void
qba_init(sc)
	struct uba_softc *sc;
{
	mtpr(0, PR_IUR);
	DELAY(500000);
	qba_beforescan(sc);
}
