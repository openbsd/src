/*	$OpenBSD: uba_cmi.c,v 1.3 2003/06/02 23:27:58 millert Exp $	*/
/*	$NetBSD: uba_cmi.c,v 1.2 1999/08/14 11:31:48 ragge Exp $	   */
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

#include <arch/vax/qbus/ubavar.h>

#include <arch/vax/uba/uba_common.h>

/* Some CMI-specific defines */
#define	UBASIZE		((UBAPAGES + UBAIOPAGES) * VAX_NBPG)
#define UMEM750(i)     	(0xfc0000 - (i) * UBASIZE)
#define	UIOPAGE(x)	(UMEM750(x) + (UBAPAGES * VAX_NBPG))

/*
 * The DW780 and DW750 are quite similar to their function from
 * a programmers point of view. Differencies are number of BDP's
 * and bus status/command registers, the latter are (partly) IPR's
 * on 750.
 */
static	int	dw750_match(struct device *, struct cfdata *, void *);
static	void	dw750_attach(struct device *, struct device *, void *);
static	void	dw750_init(struct uba_softc*);
#ifdef notyet
static	void	dw750_purge(struct uba_softc *, int);
#endif

struct	cfattach uba_cmi_ca = {
	sizeof(struct uba_vsoftc), dw750_match, dw750_attach
};

extern	struct vax_bus_space vax_mem_bus_space;

int
dw750_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;

	if ((cf->cf_loc[0] != sa->nexnum) && (cf->cf_loc[0] > -1 ))
		return 0;
	/*
	 * The uba type is actually only telling where the uba
	 * space is in nexus space.
	 */
	if ((sa->type & ~3) != NEX_UBA0)
		return 0;

	return 1;
}

void
dw750_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_vsoftc *sc = (void *)self;
	struct sbi_attach_args *sa = aux;
	struct  uba_regs *uh_uba = (void *)sa->nexaddr;
	int ubaddr = sa->nexinfo & 1;

	printf(": DW750\n");
	/*
	 * Fill in bus specific data.
	 */
	sc->uv_sc.uh_ubainit = dw750_init;
#ifdef notyet
	sc->uv_sc.uh_ubapurge = dw750_purge;
#endif
	sc->uv_sc.uh_iot = &vax_mem_bus_space;
	sc->uv_sc.uh_dmat = &sc->uv_dmat;

	/*
	 * Fill in variables used by the sgmap system.
	 */
	sc->uv_size = UBASIZE;		/* Size in bytes of Qbus space */
	sc->uv_addr = (paddr_t)uh_uba->uba_map;	/* Map regs physical address */

	uba_dma_init(sc);
	uba_attach(&sc->uv_sc, UIOPAGE(ubaddr));
}

void
dw750_init(sc)
	struct uba_softc *sc;
{
	mtpr(0, PR_IUR);
	DELAY(500000);
}

#ifdef notyet
void
dw750_purge(sc, bdp)
	struct uba_softc *sc;
	int bdp;
{
	sc->uh_uba->uba_dpr[bdp] |= UBADPR_PURGE | UBADPR_NXM | UBADPR_UCE;
}
#endif
