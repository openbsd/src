/*	$OpenBSD: bztzsc.c,v 1.5 2001/11/30 22:08:16 miod Exp $	*/
/*	$NetBSD: bztzsc.c,v 1.2 1996/12/23 09:09:54 veego Exp $	*/

/*
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *	This product contains software written by Ignatios Souvatzis for
 *	the NetBSD project.
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
#include <amiga/dev/bztzscreg.h>
#include <amiga/dev/bztzscvar.h>

void bztzscattach __P((struct device *, struct device *, void *));
int  bztzscmatch  __P((struct device *, void *, void *));

struct scsi_adapter bztzsc_scsiswitch = {
	sfas_scsicmd,
	sfas_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device bztzsc_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start functio */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

struct cfattach bztzsc_ca = {
	sizeof(struct bztzsc_softc), bztzscmatch, bztzscattach
};

struct cfdriver bztzsc_cd = {
	NULL, "bztzsc", DV_DULL, NULL, 0
};

int bztzsc_intr		 __P((void *));
void bztzsc_set_dma_tc	 __P((struct sfas_softc *sc, unsigned int len));
int bztzsc_setup_dma	 __P((struct sfas_softc *sc, vm_offset_t ptr, int len,
			      int mode));
int bztzsc_build_dma_chain __P((struct sfas_softc *sc,
			      struct sfas_dma_chain *chain, void *p, int l));
int bztzsc_need_bump	 __P((struct sfas_softc *sc, vm_offset_t ptr, int len));
void bztzsc_led		 __P((struct sfas_softc *sc, int mode));

/*
 * If we are an Phase 5 Devices Blizzard-2060 SCSI option:
 */
int
bztzscmatch(pdp, cfp, auxp)
	struct device	*pdp;
	void		*cfp;
	void		*auxp;
{
	struct zbus_args *zap;
	volatile u_int8_t *ta;

	zap = auxp;

	if (zap->manid != 0x2140)	/* Phase V ? */
		return(0);

	
	if (zap->prodid != 24)		/* is it B2060? */
		return 0;

       	ta = (vu_char *)(((char *)zap->va) + 0x1ff00 + 0x20);

	if (badbaddr((caddr_t)ta))
		return(0);
				
	*ta = 0;
	*ta = 1;
	DELAY(5);
	if (*ta != 1)
		return(0);

	return(1);
}

u_int32_t bztzsc_flags = 0;

void
bztzscattach(pdp, dp, auxp)
	struct device	*pdp;
	struct device	*dp;
	void		*auxp;
{
	struct bztzsc_softc *sc;
	struct zbus_args  *zap;
	bztzsc_regmap_p	   rp;
	vu_char		  *fas;

	zap = auxp;

	fas = &((vu_char *)zap->va)[0x1ff00];

	sc = (struct bztzsc_softc *)dp;
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

	rp->hardbits		= &fas[0xe0];
	rp->addrport		= &fas[0xf0];

	sc->sc_softc.sc_fas	= (sfas_regmap_p)rp;

	sc->sc_softc.sc_led	= bztzsc_led;

	sc->sc_softc.sc_setup_dma	= bztzsc_setup_dma;
	sc->sc_softc.sc_build_dma_chain = bztzsc_build_dma_chain;
	sc->sc_softc.sc_need_bump	= bztzsc_need_bump;

	sc->sc_softc.sc_clock_freq   = 40;   /* Phase5 SCSI all run at 40MHz */
	sc->sc_softc.sc_timeout      = 250;  /* Set default timeout to 250ms */

	sc->sc_softc.sc_config_flags = bztzsc_flags;	/* for the moment */

	sc->sc_softc.sc_host_id      = 7;    /* Should check the jumpers */

	sc->sc_softc.sc_bump_sz = NBPG;	/* XXX should be the VM pagesize */
	sc->sc_softc.sc_bump_pa = 0x0;

	sfasinitialize((struct sfas_softc *)sc);

	sc->sc_softc.sc_link.adapter_softc  = sc;
	sc->sc_softc.sc_link.adapter_target = sc->sc_softc.sc_host_id;
	sc->sc_softc.sc_link.adapter	    = &bztzsc_scsiswitch;
	sc->sc_softc.sc_link.device	    = &bztzsc_scsidev;
	sc->sc_softc.sc_link.openings	    = 1;

	sc->sc_softc.sc_isr.isr_intr = bztzsc_intr;
	sc->sc_softc.sc_isr.isr_arg  = &sc->sc_softc;
	sc->sc_softc.sc_isr.isr_ipl  = 2;
	add_isr(&sc->sc_softc.sc_isr);

/* We don't want interrupt until we're initialized! */

	printf("\n");

/* attach all scsi units on us */
	config_found(dp, &sc->sc_softc.sc_link, scsiprint);
}

int
bztzsc_intr(arg)
	void *arg;
{
	struct sfas_softc *dev = arg;
	bztzsc_regmap_p	      rp;
	int		      quickints;

	rp = (bztzsc_regmap_p)dev->sc_fas;

	if (*rp->FAS216.sfas_status & SFAS_STAT_INTERRUPT_PENDING) {
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

		return(1);
	} 
	return(0);
}

/* Set DMA transfer counter */
void
bztzsc_set_dma_tc(sc, len)
	struct sfas_softc *sc;
	unsigned int	  len;
{
	*sc->sc_fas->sfas_tc_low  = len; len >>= 8;
	*sc->sc_fas->sfas_tc_mid  = len; len >>= 8;
	*sc->sc_fas->sfas_tc_high = len;
}

/* Initialize DMA for transfer */
int
bztzsc_setup_dma(sc, ptr, len, mode)
	struct sfas_softc *sc;
	vm_offset_t	  ptr;
	int		  len;
	int		  mode;
{
	int		retval;
	u_int32_t	d;
	bztzsc_regmap_p	   rp;

	retval = 0;

	switch(mode) {

	case SFAS_DMA_READ:
	case SFAS_DMA_WRITE:

		rp = (bztzsc_regmap_p)sc->sc_fas;

		d = (u_int32_t)ptr;
		d >>= 1;

		if (mode == SFAS_DMA_WRITE)
			d |= (1L << 31);

		rp->addrport[12] = (u_int8_t)d;
		__asm __volatile("nop");

		d >>= 8;
		rp->addrport[8] = (u_int8_t)d;
		__asm __volatile("nop");

		d >>= 8;
		rp->addrport[4] = (u_int8_t)d;
		__asm __volatile("nop");

		d >>= 8;
		rp->addrport[0] = (u_int8_t)d;
		__asm __volatile("nop");

		bztzsc_set_dma_tc(sc, len);
		break;

	case SFAS_DMA_CLEAR:
	default:
		retval = (*sc->sc_fas->sfas_tc_high << 16) |
			 (*sc->sc_fas->sfas_tc_mid  <<  8) |
			  *sc->sc_fas->sfas_tc_low;

		bztzsc_set_dma_tc(sc, 0);
		break;
	}

	return(retval);
}

/* Check if address and len is ok for DMA transfer */
int
bztzsc_need_bump(sc, ptr, len)
	struct sfas_softc *sc;
	vm_offset_t	  ptr;
	int		  len;
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
/* XXX some of this is voodoo might be remnants intended for the Fastlane. */
int
bztzsc_build_dma_chain(sc, chain, p, l)
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
		 && ((mmutype <= MMU_68040) && (p >= (void *)0xFFFC0000))
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

/* real one for 2060 */
void
bztzsc_led(sc, mode)
	struct sfas_softc *sc;
	int		  mode;
{
	bztzsc_regmap_p		rp;

	rp = (bztzsc_regmap_p)sc->sc_fas;

	if (mode)
		*rp->hardbits = 0x00;	/* Led on, Int on */
	else
		*rp->hardbits = 0x02;	/* Led off, Int on */
}
