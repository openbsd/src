/*	$OpenBSD: if_le_ledma.c,v 1.7 2003/02/17 01:29:21 henric Exp $	*/
/*	$NetBSD: if_le_ledma.c,v 1.14 2001/05/30 11:46:35 mrg Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center; Paul Kranenburg.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/ic/lsi64854reg.h>
#include <dev/ic/lsi64854var.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

/*
 * LANCE registers.
 */
#define LEREG1_RDP	0	/* Register Data port */
#define LEREG1_RAP	2	/* Register Address port */

struct	le_softc {
	struct	am7990_softc	sc_am7990;	/* glue to MI code */
	struct	sbusdev		sc_sd;		/* sbus device */
	bus_space_tag_t		sc_bustag;
	bus_dmamap_t		sc_dmamap;
	bus_space_handle_t	sc_reg;		/* LANCE registers */
	struct	lsi64854_softc	*sc_dma;	/* pointer to my dma */
	u_int			sc_laddr;	/* LANCE DMA address */
};

#define MEMSIZE		(16*1024)	/* LANCE memory size */
#define LEDMA_BOUNDARY	(16*1024*1024)	/* must not cross 16MB boundary */

int	lematch_ledma(struct device *, void *, void *);
void	leattach_ledma(struct device *, struct device *, void *);

/*
 * Media types supported by the Sun4m.
 */

void	lesetutp(struct am7990_softc *);
void	lesetaui(struct am7990_softc *);

int	lemediachange(struct ifnet *);
void	lemediastatus(struct ifnet *, struct ifmediareq *);

struct cfattach le_ledma_ca = {
	sizeof(struct le_softc), lematch_ledma, leattach_ledma
};

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

#ifdef DDB
#define	integrate
#define hide
#else
#define	integrate	static __inline
#define hide		static
#endif

static void lewrcsr(struct am7990_softc *, u_int16_t, u_int16_t);
static u_int16_t lerdcsr(struct am7990_softc *, u_int16_t);
hide void lehwreset(struct am7990_softc *);
hide void lehwinit(struct am7990_softc *);
hide void lenocarrier(struct am7990_softc *);

static void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct le_softc *lesc = (struct le_softc *)sc;

	bus_space_write_2(lesc->sc_bustag, lesc->sc_reg, LEREG1_RAP, port);
	bus_space_write_2(lesc->sc_bustag, lesc->sc_reg, LEREG1_RDP, val);

#if defined(SUN4M)
	/*
	 * We need to flush the Sbus->Mbus write buffers. This can most
	 * easily be accomplished by reading back the register that we
	 * just wrote (thanks to Chris Torek for this solution).
	 */
	if (CPU_ISSUN4M) {
		volatile u_int16_t discard;
		discard = bus_space_read_2(lesc->sc_bustag, lesc->sc_reg,
					   LEREG1_RDP);
	}
#endif
}

static u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct le_softc *lesc = (struct le_softc *)sc;

	bus_space_write_2(lesc->sc_bustag, lesc->sc_reg, LEREG1_RAP, port);
	return (bus_space_read_2(lesc->sc_bustag, lesc->sc_reg, LEREG1_RDP));
}

void
lesetutp(sc)
	struct am7990_softc *sc;
{
	struct lsi64854_softc *dma = ((struct le_softc *)sc)->sc_dma;
	u_int32_t csr;

	csr = L64854_GCSR(dma);
	csr |= E_TP_AUI;
	L64854_SCSR(dma, csr);
	delay(20000);	/* must not touch le for 20ms */
}

void
lesetaui(sc)
	struct am7990_softc *sc;
{
	struct lsi64854_softc *dma = ((struct le_softc *)sc)->sc_dma;
	u_int32_t csr;

	csr = L64854_GCSR(dma);
	csr &= ~E_TP_AUI;
	L64854_SCSR(dma, csr);
	delay(20000);	/* must not touch le for 20ms */
}

int
lemediachange(ifp)
	struct ifnet *ifp;
{
	struct am7990_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	/*
	 * Switch to the selected media.  If autoselect is
	 * set, we don't really have to do anything.  We'll
	 * switch to the other media when we detect loss of
	 * carrier.
	 */
	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_10_T:
		lesetutp(sc);
		break;

	case IFM_10_5:
		lesetaui(sc);
		break;

	case IFM_AUTO:
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

void
lemediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct am7990_softc *sc = ifp->if_softc;
	struct lsi64854_softc *dma = ((struct le_softc *)sc)->sc_dma;

	/*
	 * Notify the world which media we're currently using.
	 */
	if (L64854_GCSR(dma) & E_TP_AUI)
		ifmr->ifm_active = IFM_ETHER|IFM_10_T;
	else
		ifmr->ifm_active = IFM_ETHER|IFM_10_5;
}

hide void
lehwreset(sc)
	struct am7990_softc *sc;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	struct lsi64854_softc *dma = lesc->sc_dma;
	u_int32_t csr;
	u_int aui_bit;

	/*
	 * Reset DMA channel.
	 */
	csr = L64854_GCSR(dma);
	aui_bit = csr & E_TP_AUI;
	DMA_RESET(dma);

	/* Write bits 24-31 of Lance address */
	bus_space_write_4(dma->sc_bustag, dma->sc_regs, L64854_REG_ENBAR,
			  lesc->sc_laddr & 0xff000000);

	DMA_ENINTR(dma);

	/*
	 * Disable E-cache invalidates on chip writes.
	 * Retain previous cable selection bit.
	 */
	csr = L64854_GCSR(dma);
	csr |= (E_DSBL_WR_INVAL | aui_bit);
	L64854_SCSR(dma, csr);
	delay(20000);	/* must not touch le for 20ms */
}

hide void
lehwinit(sc)
	struct am7990_softc *sc;
{

	/*
	 * Make sure we're using the currently-enabled media type.
	 * XXX Actually, this is probably unnecessary, now.
	 */
	switch (IFM_SUBTYPE(sc->sc_ifmedia.ifm_cur->ifm_media)) {
	case IFM_10_T:
		lesetutp(sc);
		break;

	case IFM_10_5:
		lesetaui(sc);
		break;
	}
}

hide void
lenocarrier(sc)
	struct am7990_softc *sc;
{
	struct le_softc *lesc = (struct le_softc *)sc;

	/*
	 * Check if the user has requested a certain cable type, and
	 * if so, honor that request.
	 */
	printf("%s: lost carrier on ", sc->sc_dev.dv_xname);

	if (L64854_GCSR(lesc->sc_dma) & E_TP_AUI) {
		printf("UTP port");
		switch (IFM_SUBTYPE(sc->sc_ifmedia.ifm_media)) {
		case IFM_10_5:
		case IFM_AUTO:
			printf(", switching to AUI port");
			lesetaui(sc);
		}
	} else {
		printf("AUI port");
		switch (IFM_SUBTYPE(sc->sc_ifmedia.ifm_media)) {
		case IFM_10_T:
		case IFM_AUTO:
			printf(", switching to UTP port");
			lesetutp(sc);
		}
	}
	printf("\n");
}

int
lematch_ledma(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct cfdata *cf = vcf;
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0);
}


void
leattach_ledma(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct le_softc *lesc = (struct le_softc *)self;
	struct lsi64854_softc *lsi = (struct lsi64854_softc *)parent;
	struct am7990_softc *sc = &lesc->sc_am7990;
	bus_dma_tag_t dmatag = sa->sa_dmatag;
	bus_dma_segment_t seg;
	int rseg, error;
	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);

	lesc->sc_bustag = sa->sa_bustag;

	/* Establish link to `ledma' device */
	lesc->sc_dma = lsi;
	lesc->sc_dma->sc_client = lesc;

	/* Map device registers */
	if (sbus_bus_map(sa->sa_bustag,
			   sa->sa_slot,
			   sa->sa_offset,
			   sa->sa_size,
			   BUS_SPACE_MAP_LINEAR,
			   0, &lesc->sc_reg) != 0) {
		printf("%s @ ledma: cannot map registers\n", self->dv_xname);
		return;
	}

	/* Allocate buffer memory */
	sc->sc_memsize = MEMSIZE;

	/* Get a DMA handle */
	if ((error = bus_dmamap_create(dmatag, MEMSIZE, 1, MEMSIZE,
					LEDMA_BOUNDARY, BUS_DMA_NOWAIT,
					&lesc->sc_dmamap)) != 0) {
		printf("%s: DMA map create error %d\n", self->dv_xname, error);
		return;
	}

	/* Allocate DMA buffer */
	if ((error = bus_dmamem_alloc(dmatag, MEMSIZE, 0, LEDMA_BOUNDARY,
				 &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s @ ledma: DMA buffer alloc error %d\n",
			self->dv_xname, error);
		return;
	}

	/* Map DMA buffer into kernel space */
	if ((error = bus_dmamem_map(dmatag, &seg, rseg, MEMSIZE,
			       (caddr_t *)&sc->sc_mem,
			       BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s @ ledma: DMA buffer map error %d\n",
			self->dv_xname, error);
		bus_dmamem_free(dmatag, &seg, rseg);
		return;
	}

	/* Load DMA buffer */
	if ((error = bus_dmamap_load(dmatag, lesc->sc_dmamap, sc->sc_mem,
			MEMSIZE, NULL, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: DMA buffer map load error %d\n",
			self->dv_xname, error);
		bus_dmamem_free(dmatag, &seg, rseg);
		bus_dmamem_unmap(dmatag, sc->sc_mem, MEMSIZE);
		return;
	}

	lesc->sc_laddr = lesc->sc_dmamap->dm_segs[0].ds_addr;
	sc->sc_addr = lesc->sc_laddr & 0xffffff;
	sc->sc_conf3 = LE_C3_BSWP | LE_C3_ACON | LE_C3_BCON;


	/* Assume SBus is grandparent */
	lesc->sc_sd.sd_reset = (void *)am7990_reset;
	sbus_establish(&lesc->sc_sd, parent);

	ifmedia_init(&sc->sc_ifmedia, 0, lemediachange, lemediastatus);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_10_T, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_10_5, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_ifmedia, IFM_ETHER | IFM_AUTO);
	sc->sc_hasifmedia = 1;

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = lerdcsr;
	sc->sc_wrcsr = lewrcsr;
	sc->sc_hwinit = lehwinit;
	sc->sc_nocarrier = lenocarrier;
	sc->sc_hwreset = lehwreset;

	/* Establish interrupt handler */
	if (sa->sa_nintr != 0)
		(void)bus_intr_establish(sa->sa_bustag, sa->sa_pri, IPL_NET, 0,
					 am7990_intr, sc);

	am7990_config(&lesc->sc_am7990);

	/* now initialize DMA */
	lehwreset(sc);
}
