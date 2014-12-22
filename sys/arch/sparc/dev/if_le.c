/*	$OpenBSD: if_le.c,v 1.36 2014/12/22 02:26:54 tedu Exp $	*/
/*	$NetBSD: if_le.c,v 1.50 1997/09/09 20:54:48 pk Exp $	*/

/*-
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
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
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/lebuffervar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <sparc/dev/if_lereg.h>
#include <sparc/dev/if_levar.h>

#ifdef solbourne
#include <sparc/sparc/asm.h>
#include <machine/idt.h>
#include <machine/kap.h>
#endif

int	lematch(struct device *, void *, void *);
void	leattach(struct device *, struct device *, void *);

/*
 * ifmedia interfaces
 */
int	lemediachange(struct lance_softc *);
void	lemediastatus(struct lance_softc *, struct ifmediareq *);

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
static int lebufmedia[] = {
	IFM_ETHER | IFM_10_T
};
#endif
#if defined(SUN4M)
static int ledmamedia[] = {
	IFM_ETHER | IFM_10_T,
	IFM_ETHER | IFM_10_5,
	IFM_ETHER | IFM_AUTO
};
#endif
static int lebaremedia[] = {
	IFM_ETHER | IFM_10_5
};

#if defined(SUN4M)
/*
 * media change methods (only for sun4m)
 */
void	lesetutp(struct lance_softc *);
void	lesetaui(struct lance_softc *);
#endif /* SUN4M */

#if defined(SUN4M)	/* XXX */
int	myleintr(void *);
int	ledmaintr(struct dma_softc *);

int
myleintr(void *arg)
{
	struct le_softc *lesc = arg;
	static int dodrain=0;

	if (lesc->sc_dma->sc_regs->csr & D_ERR_PEND) {
		dodrain = 1;
		return ledmaintr(lesc->sc_dma);
	}

	if (dodrain) {	/* XXX - is this necessary with D_DSBL_WRINVAL on? */
		int i = 10;
		while (i-- > 0 && (lesc->sc_dma->sc_regs->csr & D_DRAINING))
			delay(1);
	}

	return (am7990_intr(arg));
}
#endif

struct cfattach le_ca = {
	sizeof(struct le_softc), lematch, leattach
};

void	lewrcsr(struct lance_softc *, uint16_t, uint16_t);
uint16_t lerdcsr(struct lance_softc *, uint16_t);
void	lehwreset(struct lance_softc *);
void	lehwinit(struct lance_softc *);
#if defined(SUN4M)
void	lenocarrier(struct lance_softc *);
#endif
#if defined(solbourne)
void	kap_copytobuf(struct lance_softc *, void *, int, int);
void	kap_copyfrombuf(struct lance_softc *, void *, int, int);
#endif

void
lewrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
#if defined(SUN4M)
	volatile uint16_t discard;
#endif

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
#if defined(SUN4M)
	/* 
	 * We need to flush the SBus->MBus write buffers. This can most
	 * easily be accomplished by reading back the register that we
	 * just wrote (thanks to Chris Torek for this solution).
	 */	   
	if (CPU_ISSUN4M)
		discard = ler1->ler1_rdp;
#endif
}

uint16_t
lerdcsr(struct lance_softc *sc, uint16_t port)
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
	uint16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

#if defined(SUN4M)
void
lesetutp(struct lance_softc *sc)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	u_int32_t csr;
	int tries = 5;

	while (--tries) {
		csr = lesc->sc_dma->sc_regs->csr;
		csr |= E_TP_AUI;
		lesc->sc_dma->sc_regs->csr = csr;
		delay(20000);	/* must not touch le for 20ms */
		if (lesc->sc_dma->sc_regs->csr & E_TP_AUI)
			return;
	}
}

void
lesetaui(struct lance_softc *sc)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	u_int32_t csr;
	int tries = 5;

	while (--tries) {
		csr = lesc->sc_dma->sc_regs->csr;
		csr &= ~E_TP_AUI;
		lesc->sc_dma->sc_regs->csr = csr;
		delay(20000);	/* must not touch le for 20ms */
		if ((lesc->sc_dma->sc_regs->csr & E_TP_AUI) == 0)
			return;
	}
}
#endif

int
lemediachange(struct lance_softc *sc)
{
	struct ifmedia *ifm = &sc->sc_ifmedia;
#if defined(SUN4M)
	struct le_softc *lesc = (struct le_softc *)sc;
#endif

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	/*
	 * Switch to the selected media.  If autoselect is
	 * set, we don't really have to do anything.  We'll
	 * switch to the other media when we detect loss of
	 * carrier.
	 */
	switch (IFM_SUBTYPE(ifm->ifm_media)) {
#if defined(SUN4M)
	case IFM_10_T:
		if (CPU_ISSUN4M && lesc->sc_dma)
			lesetutp(sc);
		else
			return (EOPNOTSUPP);
		break;

	case IFM_AUTO:
		if (CPU_ISSUN4M && lesc->sc_dma)
			return (0);
		else
			return (EOPNOTSUPP);
		break;
#endif

	case IFM_10_5:
#if defined(SUN4M)
		if (CPU_ISSUN4M && lesc->sc_dma)
			lesetaui(sc);
#else
		return (0);
#endif
		break;


	default:
		return (EINVAL);
	}

	return (0);
}

void
lemediastatus(struct lance_softc *sc, struct ifmediareq *ifmr)
{
#if defined(SUN4M)
	struct le_softc *lesc = (struct le_softc *)sc;

	if (lesc->sc_dma == NULL) {
		if (lesc->sc_lebufchild)
			ifmr->ifm_active = IFM_ETHER | IFM_10_T;
		else
			ifmr->ifm_active = IFM_ETHER | IFM_10_5;
		return;
	}

	if (CPU_ISSUN4M) {
		/*
		 * Notify the world which media we're currently using.
		 */
		if (lesc->sc_dma->sc_regs->csr & E_TP_AUI)
			ifmr->ifm_active = IFM_ETHER | IFM_10_T;
		else
			ifmr->ifm_active = IFM_ETHER | IFM_10_5;
	} else
		ifmr->ifm_active = IFM_ETHER | IFM_10_5;
#else
	ifmr->ifm_active = IFM_ETHER | IFM_10_5;
#endif
}

void
lehwreset(struct lance_softc *sc)
{
#if defined(SUN4M) 
	struct le_softc *lesc = (struct le_softc *)sc;

	/*
	 * Reset DMA channel.
	 */
	if (CPU_ISSUN4M && lesc->sc_dma) {
		u_int32_t aui;

		aui = lesc->sc_dma->sc_regs->csr & E_TP_AUI;
		DMA_RESET(lesc->sc_dma);
		lesc->sc_dma->sc_regs->en_bar = lesc->sc_laddr & 0xff000000;
		DMA_ENINTR(lesc->sc_dma);
#define D_DSBL_WRINVAL D_DSBL_SCSI_DRN	/* XXX: fix dmareg.h */
		/* Disable E-cache invalidates on chip writes */
		lesc->sc_dma->sc_regs->csr |= D_DSBL_WRINVAL | aui;
		delay(20000);	/* must not touch le for 20ms */
	}
#endif
}

void
lehwinit(struct lance_softc *sc)
{
#if defined(SUN4M) 
	struct le_softc *lesc = (struct le_softc *)sc;

	if (CPU_ISSUN4M && lesc->sc_dma) {
		switch (IFM_SUBTYPE(sc->sc_ifmedia.ifm_cur->ifm_media)) {
		case IFM_10_T:
			lesetutp(sc);
			break;

		case IFM_10_5:
			lesetaui(sc);
			break;

		case IFM_AUTO:
			lesetutp(sc);
			break;

		default:	/* XXX shouldn't happen */
			lesetutp(sc);
			break;
		}
	}
#endif
}

#if defined(SUN4M)
void
lenocarrier(struct lance_softc *sc)
{
	struct le_softc *lesc = (struct le_softc *)sc;

	if (lesc->sc_dma) {
		/* 
		 * Check if the user has requested a certain cable type, and
		 * if so, honor that request.
		 */
		if (lesc->sc_dma->sc_regs->csr & E_TP_AUI) {
			switch (IFM_SUBTYPE(sc->sc_ifmedia.ifm_media)) {
			case IFM_10_5:
			case IFM_AUTO:
				printf("%s: lost carrier on UTP port"
				    ", switching to AUI port\n",
				    sc->sc_dev.dv_xname);
				lesetaui(sc);
			}
		} else {
			switch (IFM_SUBTYPE(sc->sc_ifmedia.ifm_media)) {
			case IFM_10_T:
			case IFM_AUTO:
				printf("%s: lost carrier on AUI port"
				    ", switching to UTP port\n",
				    sc->sc_dev.dv_xname);
				lesetutp(sc);
			}
		}
	}
}
#endif

int
lematch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
#if defined(solbourne)
	if (CPU_ISKAP) {
		return (ca->ca_bustype == BUS_OBIO);
	}
#endif
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS) {
		if (!sbus_testdma((struct sbus_softc *)parent, ca))
			return (0);
		return (1);
	}
#endif

	return (probeget(ra->ra_vaddr, 2) != -1);
}

void
leattach(struct device *parent, struct device *self, void *aux)
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	struct confargs *ca = aux;
	int pri;
	struct bootpath *bp;
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	int sbuschild = strcmp(parent->dv_cfdata->cf_driver->cd_name, "sbus") == 0;
	int lebufchild = strcmp(parent->dv_cfdata->cf_driver->cd_name, "lebuffer") == 0;
	int dmachild = strcmp(parent->dv_cfdata->cf_driver->cd_name, "ledma") == 0;
	struct lebuf_softc *lebuf;
#endif

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}
	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", pri);

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	lesc->sc_lebufchild = lebufchild;
#endif

	lesc->sc_r1 = (struct lereg1 *)
		mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct lereg1));

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	lebuf = NULL;
	if (lebufchild) {
		lebuf = (struct lebuf_softc *)parent;
	} else if (sbuschild) {
		extern struct cfdriver lebuffer_cd;
		struct lebuf_softc *lebufsc;
		int i;

		for (i = 0; i < lebuffer_cd.cd_ndevs; i++) {
			lebufsc = (struct lebuf_softc *)lebuffer_cd.cd_devs[i];
			if (lebufsc == NULL || lebufsc->attached != 0)
				continue;

			lebuf = lebufsc;
			break;
		}
	}
	if (lebuf != NULL) {
		sc->sc_mem = lebuf->sc_buffer;
		sc->sc_memsize = lebuf->sc_bufsiz;
		sc->sc_addr = 0; /* Lance view is offset by buffer location */
		lebuf->attached = 1;

		/* That old black magic... */
		sc->sc_conf3 = getpropint(ca->ca_ra.ra_node,
			 	"busmaster-regval",
				LE_C3_BSWP | LE_C3_ACON | LE_C3_BCON);
	} else
#endif
	{
		u_long laddr;

#if defined(solbourne)
		if (CPU_ISKAP && ca->ca_bustype == BUS_OBIO) {
			/*
			 * Use the fixed buffer allocated in pmap_bootstrap().
			 * for now, until I get the iCU translation to work...
			 */
			extern vaddr_t lance_va;

			laddr = PTW1_TO_PHYS(lance_va);
			sc->sc_mem = (void *)PHYS_TO_PTW2(laddr);

			/* disable ICU translations for ethernet */
			sta(ICU_TER, ASI_PHYS_IO,
			    lda(ICU_TER, ASI_PHYS_IO) & ~TER_ETHERNET);

			/* stash the high 15 bits of the physical address */
			sta(SE_BASE + 0x18, ASI_PHYS_IO,
			    laddr & 0xfffe0000);
		} /* else */
#endif	/* solbourne */
#if defined(SUN4) || defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
		if (sbuschild && CPU_ISSUN4DOR4M)
			laddr = (u_long)dvma_malloc_space(MEMSIZE,
			     &sc->sc_mem, M_NOWAIT, M_SPACE_D24);
		else
#endif
			laddr = (u_long)dvma_malloc(MEMSIZE,
			     &sc->sc_mem, M_NOWAIT);
#endif	/* SUN4 || SUN4C || SUN4D || SUN4E || SUN4M */
#if defined(SUN4D) || defined (SUN4M)
		if ((laddr & 0xffffff) >= (laddr & 0xffffff) + MEMSIZE)
			panic("if_le: Lance buffer crosses 16MB boundary");
#endif
#if defined(solbourne)
		if (CPU_ISKAP && ca->ca_bustype == BUS_OBIO)
			sc->sc_addr = laddr & 0x01ffff;
		else
#endif
			sc->sc_addr = laddr & 0xffffff;
		sc->sc_memsize = MEMSIZE;
#if defined(solbourne)
		if (CPU_ISKAP && ca->ca_bustype == BUS_OBIO)
			sc->sc_conf3 = LE_C3_BSWP;
		else
#endif
			sc->sc_conf3 = LE_C3_BSWP | LE_C3_ACON | LE_C3_BCON;
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
		if (dmachild) {
			lesc->sc_dma = (struct dma_softc *)parent;
			lesc->sc_dma->sc_le = lesc;
			lesc->sc_laddr = laddr;
		}
#endif
	}

	bp = ca->ca_ra.ra_bp;
	switch (ca->ca_bustype) {
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
#define SAME_LANCE(bp, ca) \
	((bp->val[0] == ca->ca_slot && bp->val[1] == ca->ca_offset) || \
	 (bp->val[0] == -1 && bp->val[1] == sc->sc_dev.dv_unit))

	case BUS_SBUS:
		if (bp != NULL && strcmp(bp->name, le_cd.cd_name) == 0 &&
		    SAME_LANCE(bp, ca))
			bp->dev = &sc->sc_dev;
		break;
#endif /* SUN4C || SUN4D || SUN4E || SUN4M */

	default:
		if (bp != NULL && strcmp(bp->name, le_cd.cd_name) == 0 &&
		    sc->sc_dev.dv_unit == bp->val[1])
			bp->dev = &sc->sc_dev;
		break;
	}

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = lerdcsr;
	sc->sc_wrcsr = lewrcsr;
	sc->sc_hwinit = lehwinit;
#if defined(SUN4M)
	if (CPU_ISSUN4M)
		sc->sc_nocarrier = lenocarrier;
#endif
	sc->sc_hwreset = lehwreset;

	sc->sc_mediachange = lemediachange;
	sc->sc_mediastatus = lemediastatus;
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (lebufchild) {
		sc->sc_supmedia = lebufmedia;
		sc->sc_nsupmedia = nitems(lebufmedia);
		sc->sc_defaultmedia = sc->sc_supmedia[sc->sc_nsupmedia - 1];
	} else
#endif
#if defined(SUN4M)
	if (CPU_ISSUN4M && lesc->sc_dma) {
		sc->sc_supmedia = ledmamedia;
		sc->sc_nsupmedia = nitems(ledmamedia);
		sc->sc_defaultmedia = lesc->sc_dma->sc_defaultmedia;
	} else
#endif
	{
		sc->sc_supmedia = lebaremedia;
		sc->sc_nsupmedia = nitems(lebaremedia);
		sc->sc_defaultmedia = sc->sc_supmedia[sc->sc_nsupmedia - 1];
	}

	am7990_config(&lesc->sc_am7990);

#if defined(solbourne)
	if (CPU_ISKAP && ca->ca_bustype == BUS_OBIO) {
		sc->sc_copytodesc = kap_copytobuf;
		sc->sc_copyfromdesc = kap_copyfrombuf;

		sc->sc_initaddr = 1 << 23 | (sc->sc_initaddr & 0x01ffff);
		sc->sc_rmdaddr = 1 << 23 | (sc->sc_rmdaddr & 0x01ffff);
		sc->sc_tmdaddr = 1 << 23 | (sc->sc_tmdaddr & 0x01ffff);
	}
#endif

	lesc->sc_ih.ih_fun = am7990_intr;
#if defined(SUN4M) /*XXX*/
	if (CPU_ISSUN4M && lesc->sc_dma)
		lesc->sc_ih.ih_fun = myleintr;
#endif
	lesc->sc_ih.ih_arg = sc;
	intr_establish(pri, &lesc->sc_ih, IPL_NET, self->dv_xname);

	/* now initialize DMA */
	lehwreset(sc);
}

#if defined(solbourne)
void
kap_copytobuf(struct lance_softc *sc, void *to, int boff, int len)
{
	return (lance_copytobuf_contig(sc, to, boff & ~(1 << 23), len));
}
void
kap_copyfrombuf(struct lance_softc *sc, void *from, int boff, int len)
{
	return (lance_copyfrombuf_contig(sc, from, boff & ~(1 << 23), len));
}
#endif
