/*	$NetBSD: asc_ioasic.c,v 1.3 1996/10/13 01:38:36 christos Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>
#include <machine/autoconf.h>

#include <pmax/dev/device.h>	/* XXX */
#include <pmax/dev/scsi.h>	/* XXX */

#include <pmax/dev/ascreg.h>	/* XXX */
#include <dev/tc/ascvar.h>

#include <machine/locore.h> /* XXX XXX bus.h needs cache-consistency*/

/*XXX*/
#include <pmax/pmax/asic.h>		/* XXX ioasic register defs? */
#include <pmax/pmax/kmin.h>	/* XXX ioasic register defs? */
#include <pmax/pmax/pmaxtype.h>
extern int pmax_boardtype;


/*
 * Autoconfiguration data for config.
 */
int asc_ioasic_match __P((struct device *, void *, void *));
void asc_ioasic_attach __P((struct device *, struct device *, void *));

struct cfattach asc_ioasic_ca = {
	sizeof(struct asc_softc), asc_ioasic_match, asc_ioasic_attach
};

/*
 * DMA callback declarations
 */

extern u_long asc_iomem;
static void
asic_dma_start __P((asc_softc_t asc, State *state, caddr_t cp, int flag));

static void
asic_dma_end __P((asc_softc_t asc, State *state, int flag));

int
asc_ioasic_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;
	void *ascaddr;

	if (strncmp(d->iada_modname, "asc", TC_ROM_LLEN) &&
	    strncmp(d->iada_modname, "PMAZ-AA ", TC_ROM_LLEN))
		return (0);

	/* probe for chip */
	ascaddr = (void*)d->iada_addr;
	if (tc_badaddr(ascaddr + ASC_OFFSET_53C94))
		return (0);

	return (1);
}

void
asc_ioasic_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct ioasicdev_attach_args *d = aux;
	register asc_softc_t asc = (asc_softc_t) self;
	int bufsiz, speed;

	void *ascaddr;
	int unit;

	ascaddr = (void*)MACH_PHYS_TO_UNCACHED(d->iada_addr);
	unit = asc->sc_dev.dv_unit;
	
	/*
	 * Initialize hw descriptor, cache some pointers
	 */
	asc->regs = (asc_regmap_t *)(ascaddr + ASC_OFFSET_53C94);

	/*
	 * Set up machine dependencies.
	 * (1) how to do dma
	 * (2) timing based on turbochannel frequency
	 */

	asc->buff = (u_char *)MACH_PHYS_TO_UNCACHED(asc_iomem);
	bufsiz = 8192;
	*((volatile int *)IOASIC_REG_SCSI_DMAPTR(ioasic_base)) = -1;
	*((volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base)) = -1;
	*((volatile int *)IOASIC_REG_SCSI_SCR(ioasic_base)) = 0;
	asc->dma_start = asic_dma_start;
	asc->dma_end = asic_dma_end;

	/*
	 * Now for timing. The 3max has a 25Mhz tb whereas the 3min and
	 * maxine are 12.5Mhz.
	 */

	/*printf(" (bus speed: %d) ", t->ta_busspeed);*/
	/* XXX don't these run at 25MHz on any ioasic??*/
	switch (pmax_boardtype) {
	case DS_3MAX:
	case DS_3MAXPLUS:
		speed = ASC_SPEED_25_MHZ;
		break;
	case DS_3MIN:
	case DS_MAXINE:
	default:
		speed = ASC_SPEED_12_5_MHZ;
		break;
	};

	ascattach(asc, bufsiz, speed);

	/* tie pseudo-slot to device */

	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_BIO,
			      asc_intr, asc);
}


/*
 * DMA handling routines. For a turbochannel device, just set the dmar.
 * For the I/O ASIC, handle the actual DMA interface.
 */
static void
asic_dma_start(asc, state, cp, flag)
	asc_softc_t asc;
	State *state;
	caddr_t cp;
	int flag;
{
	register volatile u_int *ssr = (volatile u_int *)
		IOASIC_REG_CSR(ioasic_base);
	u_int phys, nphys;

	/* stop DMA engine first */
	*ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	*((volatile int *)IOASIC_REG_SCSI_SCR(ioasic_base)) = 0;

	phys = MACH_CACHED_TO_PHYS(cp);
	cp = (caddr_t)mips_trunc_page(cp + NBPG);
	nphys = MACH_CACHED_TO_PHYS(cp);

	asc->dma_next = cp;
	asc->dma_xfer = state->dmalen - (nphys - phys);

	*(volatile int *)IOASIC_REG_SCSI_DMAPTR(ioasic_base) =
		IOASIC_DMA_ADDR(phys);
	*(volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base) =
		IOASIC_DMA_ADDR(nphys);
	if (flag == ASCDMA_READ)
		*ssr |= IOASIC_CSR_SCSI_DIR | IOASIC_CSR_DMAEN_SCSI;
	else
		*ssr = (*ssr & ~IOASIC_CSR_SCSI_DIR) | IOASIC_CSR_DMAEN_SCSI;
	wbflush();
}

static void
asic_dma_end(asc, state, flag)
	asc_softc_t asc;
	State *state;
	int flag;
{
	register volatile u_int *ssr = (volatile u_int *)
		IOASIC_REG_CSR(ioasic_base);
	register volatile u_int *dmap = (volatile u_int *)
		IOASIC_REG_SCSI_DMAPTR(ioasic_base);
	register u_short *to;
	register int w;
	int nb;

	*ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	to = (u_short *)MACH_PHYS_TO_CACHED(*dmap >> 3);
	*dmap = -1;
	*((volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base)) = -1;
	wbflush();

	if (flag == ASCDMA_READ) {
		MachFlushDCache(MACH_PHYS_TO_CACHED(
		    MACH_UNCACHED_TO_PHYS(state->dmaBufAddr)), state->dmalen);
		if ( (nb = *((int *)IOASIC_REG_SCSI_SCR(ioasic_base))) != 0) {
			/* pick up last upto6 bytes, sigh. */
	
			/* Last byte really xferred is.. */
			w = *(int *)IOASIC_REG_SCSI_SDR0(ioasic_base);
			*to++ = w;
			if (--nb > 0) {
				w >>= 16;
				*to++ = w;
			}
			if (--nb > 0) {
				w = *(int *)IOASIC_REG_SCSI_SDR1(ioasic_base);
				*to++ = w;
			}
		}
	}
}

#ifdef notdef
/*
 * Called by asic_intr() for scsi dma pointer update interrupts.
 */
void
asc_dma_intr()
{
	asc_softc_t asc =  &asc_cd.cd_devs[0]; /*XXX*/
	u_int next_phys;

	asc->dma_xfer -= NBPG;
	if (asc->dma_xfer <= -NBPG) {
		volatile u_int *ssr = (volatile u_int *)
			IOASIC_REG_CSR(ioasic_base);
		*ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	} else {
		asc->dma_next += NBPG;
		next_phys = MACH_CACHED_TO_PHYS(asc->dma_next);
	}
	*(volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base) =
		IOASIC_DMA_ADDR(next_phys);
	wbflush();
}
#endif /*notdef*/
