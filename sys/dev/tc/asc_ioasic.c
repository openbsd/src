/*	$OpenBSD: asc_ioasic.c,v 1.4 1998/05/18 00:25:08 millert Exp $	*/
/*	$NetBSD: asc_ioasic.c,v 1.12 1998/01/12 09:51:30 thorpej Exp $	*/

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
#define USE_CACHED_BUFFER 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>
#include <machine/autoconf.h>

#include <pmax/dev/device.h>		/* XXX */
#include <pmax/dev/scsi.h>		/* XXX */

#include <pmax/dev/ascreg.h>		/* XXX */
#include <dev/tc/ascvar.h>

#include <machine/cpu.h>
#include <machine/bus.h>		/* bus, cache consistency, etc  */

/*XXX*/
#include <pmax/pmax/asic.h>		/* XXX ioasic register defs? */
#include <pmax/pmax/kmin.h>	/* XXX ioasic register defs? */
#include <pmax/pmax/pmaxtype.h>
extern int pmax_boardtype;

extern vm_offset_t kvtophys __P((vm_offset_t));

/*
 * Autoconfiguration data for config.
 */
int asc_ioasic_match __P((struct device *, void *, void *));
void asc_ioasic_attach __P((struct device *, struct device *, void *));

struct cfattach asc_ioasic_ca = {
	sizeof(struct asc_softc), asc_ioasic_match, asc_ioasic_attach
};

#ifdef notdef
extern struct cfdriver asc_cd;
#endif

/*
 * DMA callback declarations
 */

#ifdef ASC_IOASIC_BOUNCE
extern u_long asc_iomem;
#endif
static int
asic_dma_start __P((asc_softc_t asc, State *state, caddr_t cp, int flag,
    int len, int off));

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
#ifdef ASC_IOASIC_BOUNCE
	u_char *buff;
	int i;
#endif

	void *ascaddr;
	int unit;

	ascaddr = (void*)MIPS_PHYS_TO_KSEG1(d->iada_addr);
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

#ifdef ASC_IOASIC_BOUNCE
#if USE_CACHED_BUFFER
	/* XXX Use cached address for DMA buffer to increase raw read speed */
	buff = (u_char *)MIPS_PHYS_TO_KSEG0(asc_iomem);
#else
	buff = (u_char *)MIPS_PHYS_TO_KSEG1(asc_iomem);
#endif	/* USE_CACHED_BUFFER */
	/*
	 * Statically partition the DMA buffer between targets.
	 * This way we will eventually be able to attach/detach
	 * drives on-fly.  And 18k/target is plenty for normal use.
	 */

	/*
	 * Give each target its own DMA buffer region.
	 * We may want to try ping ponging buffers later.
	 */
	for (i = 0; i < ASC_NCMD; i++)
		asc->st[i].dmaBufAddr = buff + 8192 * i;

#endif	/* ASC_IOASIC_BOUNCE */
	*((volatile int *)IOASIC_REG_SCSI_DMAPTR(ioasic_base)) = -1;
	*((volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base)) = -1;
	*((volatile int *)IOASIC_REG_SCSI_SCR(ioasic_base)) = 0;
	asc->dma_start = asic_dma_start;
	asc->dma_end = asic_dma_end;

	/* digital meters show IOASIC 53c94s are clocked at approx 25MHz */
	ascattach(asc, ASC_SPEED_25_MHZ);

	/* tie pseudo-slot to device */

	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_BIO,
			      asc_intr, asc);
}


/*
 * DMA handling routines. For a turbochannel device, just set the dmar.
 * For the I/O ASIC, handle the actual DMA interface.
 */
static int
asic_dma_start(asc, state, cp, flag, len, off)
	asc_softc_t asc;
	State *state;
	caddr_t cp;
	int flag;
	int len;
	int off;
{
	register volatile u_int *ssr = (volatile u_int *)
		IOASIC_REG_CSR(ioasic_base);
	u_int phys, nphys;

	/* stop DMA engine first */
	*ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	*((volatile int *)IOASIC_REG_SCSI_SCR(ioasic_base)) = 0;

#ifndef ASC_IOASIC_BOUNCE
	/* restrict len to the maximum the IOASIC can transfer */
	if (len > ((caddr_t)mips_trunc_page(cp + NBPG * 2) - cp))
		len = (caddr_t)mips_trunc_page(cp + NBPG * 2) - cp;

	/* If R4K, writeback and invalidate  the buffer */
	if (CPUISMIPS3)
		mips3_HitFlushDCache((vm_offset_t)cp, len);

	/* Get physical address of buffer start, no next phys addr */
	phys = (u_int)kvtophys((vm_offset_t)cp);
	nphys = -1;

	/* Compute 2nd DMA pointer only if next page is part of this I/O */
	if ((NBPG - (phys & (NBPG - 1))) < len) {
		cp = (caddr_t)mips_trunc_page(cp + NBPG);
		nphys = (u_int)kvtophys((vm_offset_t)cp);
	}

	/* If not R4K, need to invalidate cache lines for both physical segments */
	if (!CPUISMIPS3 && flag == ASCDMA_READ) {
		MachFlushDCache(MIPS_PHYS_TO_KSEG0(phys),
		    nphys == 0xffffffff ?  len : NBPG - (phys & (NBPG - 1)));
		if (nphys != 0xffffffff)
			MachFlushDCache(MIPS_PHYS_TO_KSEG0(nphys),
			    NBPG);	/* XXX */
	}
#else	/* ASC_IOASIC_BOUNCE */
	/* restrict len to the maximum the IOASIC can transfer */
	if (len > ((caddr_t)mips_trunc_page(state->dmaBufAddr + off + NBPG * 2) - (caddr_t)(state->dmaBufAddr + off)))
		len = (caddr_t)mips_trunc_page(state->dmaBufAddr + off + NBPG * 2) - (caddr_t)(state->dmaBufAddr + off);

	if (flag == ASCDMA_WRITE)
		bcopy(cp, state->dmaBufAddr + off, len);
	cp = state->dmaBufAddr + off;
#if USE_CACHED_BUFFER
#ifdef MIPS3
	/* If R4K, need to writeback the bounce buffer */
	if (CPUISMIPS3)
		mips3_HitFlushDCache((vm_offset_t)cp, len);
#endif /* MIPS3 */
	phys = MIPS_KSEG0_TO_PHYS(cp);
	cp = (caddr_t)mips_trunc_page(cp + NBPG);
	nphys = MIPS_KSEG0_TO_PHYS(cp);
#else
	phys = MIPS_KSEG1_TO_PHYS(cp);
	cp = (caddr_t)mips_trunc_page(cp + NBPG);
	nphys = MIPS_KSEG1_TO_PHYS(cp);
#endif	/* USE_CACHED_BUFFER */
#endif	/* ASC_IOASIC_BOUNCE */

#ifdef notyet
	asc->dma_next = cp;
	asc->dma_xfer = state->dmalen - (nphys - phys);
#endif

	*(volatile int *)IOASIC_REG_SCSI_DMAPTR(ioasic_base) =
		IOASIC_DMA_ADDR(phys);
	*(volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base) =
		IOASIC_DMA_ADDR(nphys);
	if (flag == ASCDMA_READ)
		*ssr |= IOASIC_CSR_SCSI_DIR | IOASIC_CSR_DMAEN_SCSI;
	else
		*ssr = (*ssr & ~IOASIC_CSR_SCSI_DIR) | IOASIC_CSR_DMAEN_SCSI;
	wbflush();
	return (len);
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
#if USE_CACHED_BUFFER	/* XXX - Should uncached address always be used? */
	to = (u_short *)MIPS_PHYS_TO_KSEG0(*dmap >> 3);
#else
	to = (u_short *)MIPS_PHYS_TO_KSEG1(*dmap >> 3);
#endif
	*dmap = -1;
	*((volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base)) = -1;
	wbflush();

	if (flag == ASCDMA_READ) {
#if !defined(ASC_IOASIC_BOUNCE) && USE_CACHED_BUFFER
		/* Invalidate cache for the buffer */
#ifdef MIPS3
		if (CPUISMIPS3)
			MachFlushDCache(MIPS_KSEG1_TO_PHYS(state->dmaBufAddr),
			   state->dmalen);
		else
#endif /* MIPS3 */
			MachFlushDCache(MIPS_PHYS_TO_KSEG0(
			    MIPS_KSEG1_TO_PHYS(state->dmaBufAddr)),
			    state->dmalen);
#endif	/* USE_CACHED_BUFFER */
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
#ifdef ASC_IOASIC_BOUNCE
		bcopy(state->dmaBufAddr, state->buf, state->dmalen);
#endif
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
		next_phys = MIPS_KSEG0_TO_PHYS(asc->dma_next);
	}
	*(volatile int *)IOASIC_REG_SCSI_DMANPTR(ioasic_base) =
		IOASIC_DMA_ADDR(next_phys);
	wbflush();
}
#endif /*notdef*/
