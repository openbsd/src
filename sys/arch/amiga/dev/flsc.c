/*	$OpenBSD: flsc.c,v 1.9 2001/09/11 20:05:20 miod Exp $	*/
/*	$NetBSD: flsc.c,v 1.14 1996/12/23 09:10:00 veego Exp $	*/

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
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <machine/pmap.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/sfasreg.h>
#include <amiga/dev/sfasvar.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/flscreg.h>
#include <amiga/dev/flscvar.h>

void flscattach __P((struct device *, struct device *, void *));
int  flscmatch  __P((struct device *, void *, void *));

struct scsi_adapter flsc_scsiswitch = {
	sfas_scsicmd,
	sfas_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device flsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

struct cfattach flsc_ca = {
	sizeof(struct flsc_softc), flscmatch, flscattach
};

struct cfdriver flsc_cd = {
	NULL, "flsc", DV_DULL, NULL, 0
};

int flsc_intr		 __P((void *));
void flsc_set_dma_adr	 __P((struct sfas_softc *sc, vm_offset_t ptr));
void flsc_set_dma_tc	 __P((struct sfas_softc *sc, unsigned int len));
void flsc_set_dma_mode	 __P((struct sfas_softc *sc, int mode));
int flsc_setup_dma	 __P((struct sfas_softc *sc, vm_offset_t ptr, int len,
			      int mode));
int flsc_build_dma_chain __P((struct sfas_softc *sc,
			      struct sfas_dma_chain *chain, void *p, int l));
int flsc_need_bump	 __P((struct sfas_softc *sc, vm_offset_t ptr, int len));
void flsc_led		 __P((struct sfas_softc *sc, int mode));

/*
 * if we are an Advanced Systems & Software FastlaneZ3
 */
int
flscmatch(pdp, match, auxp)
	struct device	*pdp;
	void		*match, *auxp;
{
	struct zbus_args *zap;

	if (!is_a4000() && !is_a3000())
		return(0);

	zap = auxp;
	if (zap->manid == 0x2140 && zap->prodid == 11
	    && iszthreepa(zap->pa))
		return(1);

	return(0);
}

void
flscattach(pdp, dp, auxp)
	struct device	*pdp;
	struct device	*dp;
	void		*auxp;
{
	struct flsc_softc *sc;
	struct zbus_args  *zap;
	flsc_regmap_p	   rp;
	vu_char		  *fas;

	zap = auxp;
	fas = &((vu_char *)zap->va)[0x1000001];

	sc = (struct flsc_softc *)dp;
	rp = &sc->sc_regmap;

	rp->FAS216.sfas_tc_low	= &fas[0x00];
	rp->FAS216.sfas_tc_mid	= &fas[0x04];
	rp->FAS216.sfas_fifo	= &fas[0x08];
	rp->FAS216.sfas_command	= &fas[0x0C];
	rp->FAS216.sfas_dest_id	= &fas[0x10];
	rp->FAS216.sfas_timeout	= &fas[0x14];
	rp->FAS216.sfas_syncper	= &fas[0x18];
	rp->FAS216.sfas_syncoff	= &fas[0x1C];
	rp->FAS216.sfas_config1	= &fas[0x20];
	rp->FAS216.sfas_clkconv	= &fas[0x24];
	rp->FAS216.sfas_test	= &fas[0x28];
	rp->FAS216.sfas_config2	= &fas[0x2C];
	rp->FAS216.sfas_config3	= &fas[0x30];
	rp->FAS216.sfas_tc_high	= &fas[0x38];
	rp->FAS216.sfas_fifo_bot = &fas[0x3C];
	rp->hardbits		= &fas[0x40];
	rp->clear		= &fas[0x80];
	rp->dmabase		= zap->va;

	sc->sc_softc.sc_fas	= (sfas_regmap_p)rp;
	sc->sc_softc.sc_spec	= &sc->sc_specific;

	sc->sc_softc.sc_led	= flsc_led;

	sc->sc_softc.sc_setup_dma	= flsc_setup_dma;
	sc->sc_softc.sc_build_dma_chain = flsc_build_dma_chain;
	sc->sc_softc.sc_need_bump	= flsc_need_bump;

	sc->sc_softc.sc_clock_freq   = 40;   /* FastlaneZ3 runs at 40MHz */
	sc->sc_softc.sc_timeout      = 250;  /* Set default timeout to 250ms */
	sc->sc_softc.sc_config_flags = 0;    /* No config flags yet */
	sc->sc_softc.sc_host_id      = 7;    /* Should check the jumpers */

	sc->sc_specific.portbits = 0xA0 | FLSC_PB_EDI | FLSC_PB_ESI;
	sc->sc_specific.hardbits = *rp->hardbits;

	sc->sc_softc.sc_bump_sz = NBPG;
	sc->sc_softc.sc_bump_pa = 0x0;

	sfasinitialize((struct sfas_softc *)sc);

	sc->sc_softc.sc_link.adapter_softc  = sc;
	sc->sc_softc.sc_link.adapter_target = sc->sc_softc.sc_host_id;
	sc->sc_softc.sc_link.adapter	    = &flsc_scsiswitch;
	sc->sc_softc.sc_link.device	    = &flsc_scsidev;
	sc->sc_softc.sc_link.openings	    = 1;

	sc->sc_softc.sc_isr.isr_intr = flsc_intr;
	sc->sc_softc.sc_isr.isr_arg  = &sc->sc_softc;
	sc->sc_softc.sc_isr.isr_ipl  = 2;
	add_isr(&sc->sc_softc.sc_isr);

/* We don't want interrupt until we're initialized! */
	*rp->hardbits = sc->sc_specific.portbits;

	printf("\n");

/* attach all scsi units on us */
	config_found(dp, &sc->sc_softc.sc_link, scsiprint);
}

int
flsc_intr(arg)
	void *arg;
{
	struct sfas_softc *dev = arg;
	flsc_regmap_p	      rp;
	struct flsc_specific *flspec;
	int		      quickints;
	u_char		      hb;

	flspec = dev->sc_spec;
	rp = (flsc_regmap_p)dev->sc_fas;
	hb = *rp->hardbits;

	if (hb & FLSC_HB_IACT)
		return(0);

	flspec->hardbits = hb;
	if ((hb & FLSC_HB_CREQ) &&
	    !(hb & FLSC_HB_MINT) &&
	    (*rp->FAS216.sfas_status & SFAS_STAT_INTERRUPT_PENDING)) {
		quickints = 16;
		do {
			dev->sc_status = *rp->FAS216.sfas_status;
			dev->sc_interrupt = *rp->FAS216.sfas_interrupt;
	  
			if (dev->sc_interrupt & SFAS_INT_RESELECTED) {
				dev->sc_resel[0] = *rp->FAS216.sfas_fifo;
				dev->sc_resel[1] = *rp->FAS216.sfas_fifo;
			}
			sfasintr(dev);

		} while((*rp->FAS216.sfas_status & SFAS_STAT_INTERRUPT_PENDING)
			&& --quickints);
	}

	/* Reset fastlane interrupt bits */
	*rp->hardbits = flspec->portbits & ~FLSC_PB_INT_BITS;
	*rp->hardbits = flspec->portbits;

	return(1);
}

/* Load transfer adress into dma register */
void
flsc_set_dma_adr(sc, ptr)
	struct sfas_softc *sc;
	vm_offset_t	  ptr;
{
	flsc_regmap_p	rp;
	unsigned int   *p;
	unsigned int	d;

	rp = (flsc_regmap_p)sc->sc_fas;

	d = (unsigned int)ptr;
	p = (unsigned int *)((d & 0xFFFFFF) + (int)rp->dmabase);

	*rp->clear=0;
	*p = d;
}

/* Set DMA transfer counter */
void
flsc_set_dma_tc(sc, len)
	struct sfas_softc *sc;
	unsigned int	  len;
{
	*sc->sc_fas->sfas_tc_low  = len; len >>= 8;
	*sc->sc_fas->sfas_tc_mid  = len; len >>= 8;
	*sc->sc_fas->sfas_tc_high = len;
}

/* Set DMA mode */
void
flsc_set_dma_mode(sc, mode)
	struct sfas_softc *sc;
	int		  mode;
{
	struct flsc_specific *spec;

	spec = sc->sc_spec;

	spec->portbits = (spec->portbits & ~FLSC_PB_DMA_BITS) | mode;
	*((flsc_regmap_p)sc->sc_fas)->hardbits = spec->portbits;
}

/* Initialize DMA for transfer */
int
flsc_setup_dma(sc, ptr, len, mode)
	struct sfas_softc *sc;
	vm_offset_t	  ptr;
	int		  len;
	int		  mode;
{
	int	retval;

	retval = 0;

	switch(mode) {
	case SFAS_DMA_READ:
	case SFAS_DMA_WRITE:
		flsc_set_dma_adr(sc, ptr);
		if (mode == SFAS_DMA_READ)
		  flsc_set_dma_mode(sc,FLSC_PB_ENABLE_DMA | FLSC_PB_DMA_READ);
		else
		  flsc_set_dma_mode(sc,FLSC_PB_ENABLE_DMA | FLSC_PB_DMA_WRITE);

		flsc_set_dma_tc(sc, len);
		break;

	case SFAS_DMA_CLEAR:
	default:
		flsc_set_dma_mode(sc, FLSC_PB_DISABLE_DMA);
		flsc_set_dma_adr(sc, 0);

		retval = (*sc->sc_fas->sfas_tc_high << 16) |
			 (*sc->sc_fas->sfas_tc_mid  <<  8) |
			  *sc->sc_fas->sfas_tc_low;

		flsc_set_dma_tc(sc, 0);
		break;
	}

	return(retval);
}

/* Check if address and len is ok for DMA transfer */
int
flsc_need_bump(sc, ptr, len)
	struct sfas_softc *sc;
	vm_offset_t	  ptr;
	int		  len;
{
	int	p;

	if (((int)ptr & 0x03) || (len & 0x03)) {
		if (len < 256) 
			p = len;
		else
			p = 256;
	} else 
		p = 0;

	return(p);
}

/* Interrupt driven routines */
int
flsc_build_dma_chain(sc, chain, p, l)
	struct sfas_softc	*sc;
	struct sfas_dma_chain	*chain;
	void			*p;
	int			 l;
{
	vm_offset_t  pa, lastpa;
	char	    *ptr;
	int	     len, prelen, max_t, n;

	if (l == 0)
		return(0);

#define set_link(n, p, l, f)\
do { chain[n].ptr = (p); chain[n].len = (l); chain[n++].flg = (f); } while(0)

	n = 0;

	if (l < 512)
		set_link(n, (vm_offset_t)p, l, SFAS_CHAIN_BUMP);
	else if ((p >= (void *)0xFF000000)
#if defined(M68040) || defined(M68060)
		 && ((mmutype == MMU_68040) && (p >= (void *)0xFFFC0000))
#endif
		 ) {
		while(l != 0) {
			len = ((l > sc->sc_bump_sz) ? sc->sc_bump_sz : l);
	  
			set_link(n, (vm_offset_t)p, len, SFAS_CHAIN_BUMP);
	  
			p += len;
			l -= len;
		}
	} else {
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

/* Turn on/off led */
void
flsc_led(sc, mode)
	struct sfas_softc *sc;
	int		  mode;
{
	struct flsc_specific   *spec;
	flsc_regmap_p		rp;

	spec = sc->sc_spec;
	rp = (flsc_regmap_p)sc->sc_fas;

	if (mode) {
		sc->sc_led_status++;

		spec->portbits |= FLSC_PB_LED;
		*rp->hardbits = spec->portbits;
	} else {
		if (sc->sc_led_status)
			sc->sc_led_status--;

		if (!sc->sc_led_status) {
			spec->portbits &= ~FLSC_PB_LED;
			*rp->hardbits = spec->portbits;
		}
	}
}
