/*	$OpenBSD: bzsc.c,v 1.8 2001/11/06 01:47:02 art Exp $	*/

/*	$NetBSD: bzsc.c,v 1.14 1996/12/23 09:09:53 veego Exp $	*/

/*
 * Copyright (c) 1995 Daniel Widenfalk
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	@(#)dma.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <uvm/uvm_extern.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/sfasreg.h>
#include <amiga/dev/sfasvar.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/bzscreg.h>
#include <amiga/dev/bzscvar.h>

void bzscattach __P((struct device *, struct device *, void *));
int  bzscmatch  __P((struct device *, void *, void *));

struct scsi_adapter bzsc_scsiswitch = {
	sfas_scsicmd,
	sfas_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device bzsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

struct cfattach bzsc_ca = {
	sizeof(struct bzsc_softc), bzscmatch, bzscattach
};

struct cfdriver bzsc_cd = {
	NULL, "bzsc", DV_DULL, NULL, 0
};

int bzsc_intr		__P((void *));
void bzsc_set_dma_adr	__P((struct sfas_softc *sc, vm_offset_t ptr, int mode));       
void bzsc_set_dma_tc	__P((struct sfas_softc *sc, unsigned int len));
int bzsc_setup_dma	__P((struct sfas_softc *sc, vm_offset_t ptr, int len,
			     int mode));
int bzsc_build_dma_chain __P((struct sfas_softc *sc,
				struct sfas_dma_chain *chain, void *p, int l));
int bzsc_need_bump	__P((struct sfas_softc *sc, vm_offset_t ptr, int len));
void bzsc_led_dummy	__P((struct sfas_softc *sc, int mode));

/*
 * if we are an Advanced Systems & Software FastlaneZ3
 */
int
bzscmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct zbus_args *zap;
	vu_char *ta;

	if (!is_a1200())
		return(0);

	zap = auxp;
	if (zap->manid != 0x2140 || zap->prodid != 11)
		return(0);

	ta = (vu_char *)(((char *)zap->va)+0x10010);
	if (badbaddr((caddr_t)ta))
		return(0);

	*ta = 0;
	*ta = 1;
	DELAY(5);
	if (*ta != 1)
		return(0);

	return(1);
}

void
bzscattach(pdp, dp, auxp)
	struct device *pdp;
	struct device *dp;
	void *auxp;
{
	struct bzsc_softc *sc;
	struct zbus_args  *zap;
	bzsc_regmap_p	   rp;
	vu_char		  *fas;

	zap = auxp;
	fas = (vu_char *)(((char *)zap->va)+0x10000);

	sc = (struct bzsc_softc *)dp;
	rp = &sc->sc_regmap;

	rp->FAS216.sfas_tc_low   = &fas[0x00];
	rp->FAS216.sfas_tc_mid   = &fas[0x02];
	rp->FAS216.sfas_fifo     = &fas[0x04];
	rp->FAS216.sfas_command  = &fas[0x06];
	rp->FAS216.sfas_dest_id  = &fas[0x08];
	rp->FAS216.sfas_timeout  = &fas[0x0A];
	rp->FAS216.sfas_syncper  = &fas[0x0C];
	rp->FAS216.sfas_syncoff  = &fas[0x0E];
	rp->FAS216.sfas_config1  = &fas[0x10];
	rp->FAS216.sfas_clkconv  = &fas[0x12];
	rp->FAS216.sfas_test     = &fas[0x14];
	rp->FAS216.sfas_config2  = &fas[0x16];
	rp->FAS216.sfas_config3  = &fas[0x18];
	rp->FAS216.sfas_tc_high  = &fas[0x1C];
	rp->FAS216.sfas_fifo_bot = &fas[0x1E];
	rp->cclkaddr		 = &fas[0x21];
	rp->epowaddr		 = &fas[0x31];

	sc->sc_softc.sc_fas  = (sfas_regmap_p)rp;
	sc->sc_softc.sc_spec = 0;

	sc->sc_softc.sc_led  = bzsc_led_dummy;

	sc->sc_softc.sc_setup_dma	= bzsc_setup_dma;
	sc->sc_softc.sc_build_dma_chain = bzsc_build_dma_chain;
	sc->sc_softc.sc_need_bump	= bzsc_need_bump;

	sc->sc_softc.sc_clock_freq   = 40; /* BlizzardII 1230 runs at 40MHz? */
	sc->sc_softc.sc_timeout      = 250; /* Set default timeout to 250ms */
	sc->sc_softc.sc_config_flags = 0;
	sc->sc_softc.sc_host_id      = 7;

	sc->sc_softc.sc_bump_sz = NBPG;
	sc->sc_softc.sc_bump_pa = 0x0;

	sfasinitialize((struct sfas_softc *)sc);

	sc->sc_softc.sc_link.adapter_softc = sc;
	sc->sc_softc.sc_link.adapter_target = sc->sc_softc.sc_host_id;
	sc->sc_softc.sc_link.adapter = &bzsc_scsiswitch;
	sc->sc_softc.sc_link.device = &bzsc_scsidev;
	sc->sc_softc.sc_link.openings = 1;

	printf("\n");

	sc->sc_softc.sc_isr.isr_intr = bzsc_intr;
	sc->sc_softc.sc_isr.isr_arg = &sc->sc_softc;
	sc->sc_softc.sc_isr.isr_ipl = 2;
	add_isr(&sc->sc_softc.sc_isr);

	/* attach all scsi units on us */
	config_found(dp, &sc->sc_softc.sc_link, scsiprint);
}

int
bzsc_intr(arg)
	void *arg;
{
	struct sfas_softc *dev = arg;
	bzsc_regmap_p	rp;
	int		quickints;

	rp = (bzsc_regmap_p)dev->sc_fas;

	if (!(*rp->FAS216.sfas_status & SFAS_STAT_INTERRUPT_PENDING))
		return(0);

	quickints = 16;
	do {
		dev->sc_status = *rp->FAS216.sfas_status;
		dev->sc_interrupt = *rp->FAS216.sfas_interrupt;
		
		if (dev->sc_interrupt & SFAS_INT_RESELECTED) {
			dev->sc_resel[0] = *rp->FAS216.sfas_fifo;
			dev->sc_resel[1] = *rp->FAS216.sfas_fifo;
		}
		sfasintr(dev);
	} while((*rp->FAS216.sfas_status & SFAS_STAT_INTERRUPT_PENDING) &&
		--quickints);

	return(1);
}

/* --------- */
void
bzsc_set_dma_adr(sc, ptr, mode)
	struct sfas_softc *sc;
	vm_offset_t ptr;
	int mode;
{
	bzsc_regmap_p	rp;
	unsigned long	p;

	rp = (bzsc_regmap_p)sc->sc_fas;

	p = ((unsigned long)ptr)>>1;
	
	if (mode == SFAS_DMA_WRITE)
		p |= BZSC_DMA_WRITE;
	else
		p |= BZSC_DMA_READ;

	*rp->epowaddr = (u_char)(p>>24) & 0xFF;
	*rp->cclkaddr = (u_char)(p>>16) & 0xFF;
	*rp->cclkaddr = (u_char)(p>> 8) & 0xFF;
	*rp->cclkaddr = (u_char)(p    ) & 0xFF;
}

/* Set DMA transfer counter */
void
bzsc_set_dma_tc(sc, len)
	struct sfas_softc *sc;
	unsigned int len;
{
	*sc->sc_fas->sfas_tc_low  = len; len >>= 8;
	*sc->sc_fas->sfas_tc_mid  = len; len >>= 8;
	*sc->sc_fas->sfas_tc_high = len;
}

/* Initialize DMA for transfer */
int
bzsc_setup_dma(sc, ptr, len, mode)
	struct sfas_softc *sc;
	vm_offset_t ptr;
	int len;
	int mode;
{
	int	retval;

	retval = 0;

	switch(mode) {
	case SFAS_DMA_READ:
	case SFAS_DMA_WRITE:
		bzsc_set_dma_adr(sc, ptr, mode);
		bzsc_set_dma_tc(sc, len);
		break;
	case SFAS_DMA_CLEAR:
	default:
		retval = (*sc->sc_fas->sfas_tc_high << 16) |
			 (*sc->sc_fas->sfas_tc_mid  <<  8) |
			  *sc->sc_fas->sfas_tc_low;
      
		bzsc_set_dma_tc(sc, 0);
		break;
	}

	return(retval);
}

/* Check if address and len is ok for DMA transfer */
int
bzsc_need_bump(sc, ptr, len)
	struct sfas_softc *sc;
	vm_offset_t ptr;
	int len;
{
	int	p;

	p = (int)ptr & 0x03;

	if (p) {
		p = 4-p;

		if (len < 256)
			p = len;
	}

	return(p);
}

/* Interrupt driven routines */
int
bzsc_build_dma_chain(sc, chain, p, l)
	struct sfas_softc *sc;
	struct sfas_dma_chain *chain;
	void *p;
	int l;
{
	int	n;

	if (!l)
		return(0);

#define set_link(n, p, l, f)\
do { chain[n].ptr = (p); chain[n].len = (l); chain[n++].flg = (f); } while(0)

	n = 0;

	if (l < 512)
		set_link(n, (vm_offset_t)p, l, SFAS_CHAIN_BUMP);
	else if (
#if defined(M68040) || defined(M68060)
		 ((mmutype == MMU_68040) && ((vm_offset_t)p >= 0xFFFC0000)) &&
#endif
		 ((vm_offset_t)p >= 0xFF000000)) {
		int	len;

		while(l) {
			len = ((l > sc->sc_bump_sz) ? sc->sc_bump_sz : l);

			set_link(n, (vm_offset_t)p, len, SFAS_CHAIN_BUMP);

			p += len;
			l -= len;
		}
	} else  {
		char		*ptr;
		vm_offset_t	 pa, lastpa;
		int		 len,  prelen,  max_t;

		ptr = p;
		len = l;

		pa = kvtop(ptr);
		prelen = ((int)ptr & 0x03);

		if (prelen) {
			prelen = 4-prelen;
			set_link(n, (vm_offset_t)ptr, prelen, SFAS_CHAIN_BUMP);
			ptr += prelen;
			len -= prelen;
		}

		lastpa = 0;
		while(len > 3) {
			pa = kvtop(ptr);
			max_t = NBPG - (pa & PGOFSET);
			if (max_t > len)
				max_t = len;
	  
			max_t &= ~3;
	  
			if (lastpa == pa)
				sc->sc_chain[n-1].len += max_t;
			else
				set_link(n, pa, max_t, SFAS_CHAIN_DMA);
	  
			lastpa = pa+max_t;
	  
			ptr += max_t;
			len -= max_t;
		}
      
		if (len)
			set_link(n, (vm_offset_t)ptr, len, SFAS_CHAIN_BUMP);
	}

	return(n);
}

/* Turn on led */
void bzsc_led_dummy(sc, mode)
	struct sfas_softc *sc;
	int mode;
{
}
